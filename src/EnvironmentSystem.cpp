#include "EnvironmentSystem.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace outlaws {
namespace {
bool readable(std::uintptr_t address, std::size_t size) noexcept {
    if (!address || !size) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<const void*>(address), &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const DWORD p = mbi.Protect & 0xFF;
    const bool canRead =
        p == PAGE_READONLY || p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
        p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
    if (!canRead) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
    const auto end = begin + mbi.RegionSize;
    return address >= begin && address + size >= address && address + size <= end;
}

std::filesystem::path logPath() {
    wchar_t buffer[MAX_PATH]{};
    const auto len = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (!len || len >= std::size(buffer)) return L"OutlawsTools.log";
    std::filesystem::path p(buffer);
    p.replace_filename(L"OutlawsTools.log");
    return p;
}
}

bool EnvironmentSystem::initialize(std::uintptr_t moduleBase) noexcept {
    moduleBase_ = moduleBase;
    appendLog(moduleBase_ ? "environment initialized" : "environment initialization failed");
    return moduleBase_ != 0;
}

void EnvironmentSystem::shutdown() noexcept {
    moduleBase_ = 0;
}

void EnvironmentSystem::appendLog(const char* text) const noexcept {
    if (!text) return;
    std::ofstream log(logPath(), std::ios::out | std::ios::app);
    if (!log) return;
    log << text << "\n";
    log.flush();
}

void* EnvironmentSystem::resolveWeatherManager() const noexcept {
    if (!moduleBase_) return nullptr;

    const auto rootAddress = moduleBase_ + TodRootRva;
    if (!readable(rootAddress, sizeof(std::uintptr_t))) return nullptr;
    const auto root = *reinterpret_cast<const std::uintptr_t*>(rootAddress);
    if (!root || !readable(root + 0xD08, sizeof(std::uintptr_t))) return nullptr;

    const auto slotAddress = moduleBase_ + TodSlotRva;
    if (!readable(slotAddress, sizeof(std::uint32_t))) return nullptr;
    const auto slot = *reinterpret_cast<const std::uint32_t*>(slotAddress);
    if (slot >= 0x1000) return nullptr;

    const auto table = *reinterpret_cast<const std::uintptr_t*>(root + 0xD08);
    const auto ownerAddress = table + static_cast<std::uintptr_t>(slot) * 8u;
    if (!table || !readable(ownerAddress, sizeof(std::uintptr_t))) return nullptr;

    const auto owner = *reinterpret_cast<const std::uintptr_t*>(ownerAddress);
    if (!owner || !readable(owner, 0x360)) return nullptr;
    return reinterpret_cast<void*>(owner);
}

void* EnvironmentSystem::resolveActiveWeatherPreset(void* manager) const noexcept {
    const auto base = reinterpret_cast<std::uintptr_t>(manager);
    if (!base || !readable(base, 0x360)) return nullptr;

    const auto index = *reinterpret_cast<const std::int32_t*>(base + 0x354);
    std::uintptr_t source{};

    if (index == -1) {
        source = *reinterpret_cast<const std::uintptr_t*>(base + 0xF0);
    } else {
        const auto level = *reinterpret_cast<const std::uintptr_t*>(base + 0x88);
        if (!level || !readable(level + 0x8, sizeof(std::uintptr_t))) return nullptr;
        const auto state = *reinterpret_cast<const std::uintptr_t*>(level + 0x8);
        if (!state || !readable(state + 0x40, sizeof(std::uintptr_t))) return nullptr;
        source = *reinterpret_cast<const std::uintptr_t*>(state + 0x40);
    }

    if (!source || !readable(source + 0x60, sizeof(std::uintptr_t))) return nullptr;
    const auto preset = *reinterpret_cast<const std::uintptr_t*>(source + 0x60);
    if (!preset || !readable(preset, 0x2BA)) return nullptr;
    return reinterpret_cast<void*>(preset);
}

void* EnvironmentSystem::resolveTimeOfDaySystem() const noexcept {
    if (!moduleBase_) return nullptr;

    const auto root = *reinterpret_cast<const std::uintptr_t*>(moduleBase_ + TodRootRva);
    if (!root) return nullptr;

    const auto slot = *reinterpret_cast<const std::uint32_t*>(moduleBase_ + TodSlotRva);
    const auto table = *reinterpret_cast<const std::uintptr_t*>(root + 0xD08);
    if (!table || slot >= 0x1000) return nullptr;

    const auto owner = *reinterpret_cast<const std::uintptr_t*>(table + static_cast<std::uintptr_t>(slot) * 8u);
    if (!owner) return nullptr;

    return *reinterpret_cast<void* const*>(owner + 0x2D0);
}

bool EnvironmentSystem::logTimeOfDay() const noexcept {
    const auto* tod = static_cast<const std::uint8_t*>(resolveTimeOfDaySystem());
    if (!tod) {
        appendLog("TOD native system = <not found>");
        return false;
    }

    const auto ms = *reinterpret_cast<const std::uint32_t*>(tod + 0x18);
    const bool paused = *(tod + 0x22) != 0;
    const auto totalSeconds = ms / 1000u;
    const auto hour = (totalSeconds / 3600u) % 24u;
    const auto minute = (totalSeconds / 60u) % 60u;
    const auto second = totalSeconds % 60u;

    char line[256]{};
    std::snprintf(line, sizeof(line),
        "TOD native system=0x%llX time=%02u:%02u:%02u ms=%u paused=%s",
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(tod)),
        hour, minute, second, ms, paused ? "ON" : "OFF");
    appendLog(line);
    return true;
}

bool EnvironmentSystem::setTimeOfDay(float value) noexcept {
    auto* tod = resolveTimeOfDaySystem();
    if (!tod) {
        appendLog("TOD native SET = <system not found>");
        return false;
    }

    float wrapped = std::fmod(value, 24.0f);
    if (wrapped < 0.0f) wrapped += 24.0f;

    const auto totalSeconds = static_cast<std::uint32_t>(wrapped * 3600.0f + 0.5f);
    const std::uint32_t hour = (totalSeconds / 3600u) % 24u;
    const std::uint32_t minute = (totalSeconds / 60u) % 60u;
    const std::uint32_t second = totalSeconds % 60u;

    const auto* bytes = static_cast<const std::uint8_t*>(tod);
    const auto beforeMs = *reinterpret_cast<const std::uint32_t*>(bytes + 0x18);

    using SetTodFn = void(*)(void*, std::uint32_t, std::uint32_t, std::uint32_t);
    auto setter = reinterpret_cast<SetTodFn>(moduleBase_ + SetTimeOfDayRva);
    setter(tod, hour, minute, second);

    const auto afterMs = *reinterpret_cast<const std::uint32_t*>(
        static_cast<const std::uint8_t*>(tod) + 0x18);

    char line[256]{};
    std::snprintf(line, sizeof(line),
        "TOD native SET system=0x%llX %u -> %u requested=%02u:%02u:%02u",
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(tod)),
        beforeMs, afterMs, hour, minute, second);
    appendLog(line);
    return true;
}

bool EnvironmentSystem::setTimePaused(bool paused) noexcept {
    auto* tod = static_cast<std::uint8_t*>(resolveTimeOfDaySystem());
    if (!tod) {
        appendLog("TOD native PAUSE = <system not found>");
        return false;
    }

    const bool before = *(tod + 0x22) != 0;
    *(tod + 0x22) = paused ? 1u : 0u;
    const bool after = *(tod + 0x22) != 0;

    char line[224]{};
    std::snprintf(line, sizeof(line),
        "TOD native PAUSE system=0x%llX %s -> %s",
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(tod)),
        before ? "ON" : "OFF", after ? "ON" : "OFF");
    appendLog(line);
    return after == paused;
}

bool EnvironmentSystem::readWeatherScene(WeatherSceneState& out) const noexcept {
    out = {};

    auto* manager = resolveWeatherManager();
    if (!manager) return false;

    const auto managerAddress = reinterpret_cast<std::uintptr_t>(manager);
    out.manager = managerAddress;
    out.activePresetIndex = *reinterpret_cast<const std::int32_t*>(managerAddress + 0x354);

    auto* preset = resolveActiveWeatherPreset(manager);
    if (!preset) {
        out.available = true;
        return true;
    }

    const auto p = reinterpret_cast<std::uintptr_t>(preset);
    out.preset = p;
    out.available = true;

    // Presence flags reconstructed from HC_EnvironmentWeatherPresetConstantData
    // deserialization. They tell us whether the active scene preset contributes
    // each field; they are not guessed "current renderer values".
    out.gameplayRainField   = *reinterpret_cast<const std::uint8_t*>(p + 0x080) != 0;
    out.graphicsRainField   = *reinterpret_cast<const std::uint8_t*>(p + 0x0D0) != 0;
    out.temperatureField    = *reinterpret_cast<const std::uint8_t*>(p + 0x120) != 0;
    out.viewDistanceField   = *reinterpret_cast<const std::uint8_t*>(p + 0x170) != 0;
    out.outdoorFogField     = *reinterpret_cast<const std::uint8_t*>(p + 0x1C0) != 0;
    out.cloudCoverageField  = *reinterpret_cast<const std::uint8_t*>(p + 0x210) != 0;
    out.windDirectionField  = *reinterpret_cast<const std::uint8_t*>(p + 0x260) != 0;
    out.windStrengthField   = *reinterpret_cast<const std::uint8_t*>(p + 0x2B0) != 0;

    out.gameplayRain  = *reinterpret_cast<const float*>(p + 0x038);
    out.graphicsRain  = *reinterpret_cast<const float*>(p + 0x088);
    out.temperature   = *reinterpret_cast<const float*>(p + 0x0D8);
    out.viewDistance  = *reinterpret_cast<const float*>(p + 0x128);
    out.outdoorFog    = *reinterpret_cast<const float*>(p + 0x178);
    out.cloudCoverage = *reinterpret_cast<const float*>(p + 0x1C8);
    out.windDirection = *reinterpret_cast<const float*>(p + 0x218);
    out.windStrength  = *reinterpret_cast<const float*>(p + 0x268);

    out.hasSnow = *reinterpret_cast<const std::uint8_t*>(p + 0x2B8) != 0;
    out.hasFog  = *reinterpret_cast<const std::uint8_t*>(p + 0x2B9) != 0;
    return true;
}

bool EnvironmentSystem::read(EnvironmentState& out) const {
    const auto* tod = static_cast<const std::uint8_t*>(resolveTimeOfDaySystem());
    if (!tod) return false;

    const auto ms = *reinterpret_cast<const std::uint32_t*>(tod + 0x18);
    out.timeOfDay = static_cast<float>(ms) / 3600000.0f;
    out.timePaused = *(tod + 0x22) != 0;
    return true;
}

bool EnvironmentSystem::apply(const EnvironmentState& state) {
    const bool a = setTimeOfDay(state.timeOfDay);
    const bool b = setTimePaused(state.timePaused);
    return a && b;
}

}
