#include "EnvironmentSystem.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace outlaws {
namespace {
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

void* EnvironmentSystem::lookupRuntimeValue(const char* name) const noexcept {
    if (!moduleBase_ || !name || !*name) return nullptr;
    using RuntimeLookupFn = void*(*)(const char*);
    auto lookup = reinterpret_cast<RuntimeLookupFn>(moduleBase_ + RuntimeLookupRva);
    return lookup(name);
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

bool EnvironmentSystem::logRain() const noexcept {
    const auto* record = static_cast<const std::uint8_t*>(
        lookupRuntimeValue("Env_GameplayRainAmount"));
    if (!record) {
        appendLog("WEATHER Rain Env_GameplayRainAmount = <not found>");
        return false;
    }

    const auto type = *reinterpret_cast<const std::uint32_t*>(record + 0x00);
    const float value = *reinterpret_cast<const float*>(record + 0x10);

    char line[224]{};
    std::snprintf(line, sizeof(line),
        "WEATHER Rain runtime type=%u value=%g record=0x%llX",
        type,
        value,
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(record)));
    appendLog(line);
    return true;
}

bool EnvironmentSystem::logRainMetadata() const noexcept {
    if (!moduleBase_) return false;

    const auto descriptor = moduleBase_ + EnvRegistryOwnerRva
        + static_cast<std::uintptr_t>(GameplayRainId) * 0x40u;

    const auto defaultBits = *reinterpret_cast<const std::uint32_t*>(descriptor + 0x00);
    float defaultValue{};
    std::memcpy(&defaultValue, &defaultBits, sizeof(defaultValue));

    const bool hasMin = *reinterpret_cast<const std::uint8_t*>(descriptor + 0x18) != 0;
    const float minValue = *reinterpret_cast<const float*>(descriptor + 0x1C);
    const bool hasMax = *reinterpret_cast<const std::uint8_t*>(descriptor + 0x20) != 0;
    const float maxValue = *reinterpret_cast<const float*>(descriptor + 0x24);
    const auto id = *reinterpret_cast<const std::uint16_t*>(descriptor + 0x34);
    const auto name = *reinterpret_cast<const char* const*>(descriptor + 0x38);

    char line[320]{};
    std::snprintf(line, sizeof(line),
        "WEATHER Rain descriptor id=0x%X name=%s default=%g min=%s%g max=%s%g",
        static_cast<unsigned>(id),
        name ? name : "<null>",
        defaultValue,
        hasMin ? "" : "<none>",
        minValue,
        hasMax ? "" : "<none>",
        maxValue);
    appendLog(line);
    return true;
}

bool EnvironmentSystem::setRain(float value) noexcept {
    auto* record = static_cast<std::uint8_t*>(
        lookupRuntimeValue("Env_GameplayRainAmount"));
    if (!record) {
        appendLog("WEATHER Rain DIRECT = <not found>");
        return false;
    }

    auto* liveValue = reinterpret_cast<float*>(record + 0x10);
    const float before = *liveValue;
    *liveValue = value;
    const float immediate = *liveValue;

    char line[256]{};
    std::snprintf(line, sizeof(line),
        "WEATHER Rain DIRECT record=0x%llX value %g -> %g immediate=%g",
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(record)),
        before, value, immediate);
    appendLog(line);
    return immediate == value;
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
