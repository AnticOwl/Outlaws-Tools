#include "EnvironmentSystem.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <limits>
#include <atomic>
#include <MinHook.h>

namespace outlaws {
namespace {

using NativePrecipExecFn = void(__fastcall*)(void*, void*);
std::atomic<std::uintptr_t> gPrecipitationContext{};
NativePrecipExecFn gOriginalGetEnvironmentPrecipitation{};
NativePrecipExecFn gOriginalGetPrecipitationIntensity{};

void __fastcall hookGetEnvironmentPrecipitation(void* self, void* context) {
    if (context) gPrecipitationContext.store(reinterpret_cast<std::uintptr_t>(context), std::memory_order_relaxed);
    if (gOriginalGetEnvironmentPrecipitation) gOriginalGetEnvironmentPrecipitation(self, context);
}

void __fastcall hookGetPrecipitationIntensity(void* self, void* context) {
    if (context) gPrecipitationContext.store(reinterpret_cast<std::uintptr_t>(context), std::memory_order_relaxed);
    if (gOriginalGetPrecipitationIntensity) gOriginalGetPrecipitationIntensity(self, context);
}

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


std::uintptr_t resolvePrecipitationObjectFromContext(std::uintptr_t context, std::uintptr_t moduleBase) noexcept {
    if (!context || !moduleBase || !readable(context + 0x830, sizeof(std::uintptr_t))) return 0;

    const auto entryOwner = *reinterpret_cast<const std::uintptr_t*>(context + 0x830);
    if (!entryOwner || !readable(entryOwner, 0x10)) return 0;

    const auto entries = *reinterpret_cast<const std::uintptr_t*>(entryOwner + 0x0);
    const auto count = *reinterpret_cast<const std::uint32_t*>(entryOwner + 0x8);
    if (!entries || count == 0 || count > 0x1000) return 0;

    const auto typeA = moduleBase + 0x8A17F88;
    const auto typeB = moduleBase + 0x89E8820;
    const auto typeC = moduleBase + 0x89E6F08;

    auto findEntryObject = [&](std::uintptr_t type) -> std::uintptr_t {
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto e = entries + static_cast<std::uintptr_t>(i) * 0x20u;
            if (!readable(e, 0x20)) break;
            if (*reinterpret_cast<const std::uintptr_t*>(e + 0x18) == type)
                return *reinterpret_cast<const std::uintptr_t*>(e + 0x00);
        }
        return 0;
    };

    if (const auto direct = findEntryObject(typeA)) {
        if (readable(direct + 0x3BC, 4)) return direct;
    }

    if (const auto b = findEntryObject(typeB)) {
        if (readable(b + 0x88, sizeof(std::uintptr_t))) {
            const auto object = *reinterpret_cast<const std::uintptr_t*>(b + 0x80);
            if (object && readable(object + 0x3BC, 4)) return object;
        }
    }

    if (const auto c = findEntryObject(typeC)) {
        if (readable(c, sizeof(std::uintptr_t))) {
            const auto p = *reinterpret_cast<const std::uintptr_t*>(c);
            if (p && readable(p + 0x298, sizeof(std::uintptr_t))) {
                const auto object = *reinterpret_cast<const std::uintptr_t*>(p + 0x290);
                if (object && readable(object + 0x3BC, 4)) return object;
            }
        }
    }

    return 0;
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
    if (moduleBase_) installPrecipitationHooks();
    return moduleBase_ != 0;
}

void EnvironmentSystem::shutdown() noexcept {
    gPrecipitationContext.store(0, std::memory_order_relaxed);
    moduleBase_ = 0;
}


bool EnvironmentSystem::installPrecipitationHooks() noexcept {
    if (!moduleBase_) return false;

    const auto init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        appendLog("precipitation hooks: MinHook initialize failed");
        return false;
    }

    const auto stateTarget = reinterpret_cast<void*>(moduleBase_ + GetEnvironmentPrecipitationExecRva);
    const auto intensityTarget = reinterpret_cast<void*>(moduleBase_ + GetPrecipitationIntensityExecRva);

    const auto a = MH_CreateHook(
        stateTarget,
        reinterpret_cast<void*>(&hookGetEnvironmentPrecipitation),
        reinterpret_cast<void**>(&gOriginalGetEnvironmentPrecipitation));

    const auto b = MH_CreateHook(
        intensityTarget,
        reinterpret_cast<void*>(&hookGetPrecipitationIntensity),
        reinterpret_cast<void**>(&gOriginalGetPrecipitationIntensity));

    const bool aCreated = a == MH_OK || a == MH_ERROR_ALREADY_CREATED;
    const bool bCreated = b == MH_OK || b == MH_ERROR_ALREADY_CREATED;

    if (!aCreated || !bCreated) {
        appendLog("precipitation hooks: create failed");
        return false;
    }

    const auto ea = MH_EnableHook(stateTarget);
    const auto eb = MH_EnableHook(intensityTarget);

    const bool aEnabled = ea == MH_OK || ea == MH_ERROR_ENABLED;
    const bool bEnabled = eb == MH_OK || eb == MH_ERROR_ENABLED;

    appendLog((aEnabled && bEnabled)
        ? "precipitation native hooks enabled"
        : "precipitation native hooks enable failed");

    return aEnabled && bEnabled;
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
    if (!preset || !readable(preset, 0x2C0)) return nullptr;
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
    std::memcpy(out.managerRaw.data(),
                reinterpret_cast<const void*>(managerAddress),
                out.managerRaw.size());
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

    // These 0x48-byte blocks are serialized property objects, not plain floats.
    // Keep the old float members unavailable until their inner value layout is decoded.
    const float unavailable = std::numeric_limits<float>::quiet_NaN();
    out.gameplayRain = unavailable;
    out.graphicsRain = unavailable;
    out.temperature = unavailable;
    out.viewDistance = unavailable;
    out.outdoorFog = unavailable;
    out.cloudCoverage = unavailable;
    out.windDirection = unavailable;
    out.windStrength = unavailable;

    // Raw preset bytes are captured so one zone/planet transition can reveal which
    // offsets actually change, without guessing or writing into game memory.
    std::memcpy(out.presetRaw.data(), reinterpret_cast<const void*>(p), out.presetRaw.size());

    out.hasSnow = *reinterpret_cast<const std::uint8_t*>(p + 0x2B8) != 0;
    out.hasFog  = *reinterpret_cast<const std::uint8_t*>(p + 0x2B9) != 0;

    const auto ctx = gPrecipitationContext.load(std::memory_order_relaxed);
    out.precipitationContext = ctx;
    if (ctx) {
        const auto object = resolvePrecipitationObjectFromContext(ctx, moduleBase_);
        out.precipitationObject = object;
        if (object) {
            out.precipitationStateRaw =
                *reinterpret_cast<const float*>(object + 0x388);
            out.precipitationIntensity =
                *reinterpret_cast<const float*>(object + 0x3B8);

            out.precipitationAvailable =
                std::isfinite(out.precipitationStateRaw) &&
                std::isfinite(out.precipitationIntensity);
        }
    }

    // Fallback live discovery: inspect first-level pointers owned by the same
    // Snowdrop owner that already resolves TOD. This is read-only and lets a
    // rainy scene expose likely precipitation objects even if the script getter
    // node is never executed.
    auto addCandidate = [&](std::uint32_t ownerOffset, std::uintptr_t object) {
        if (!object || out.precipitationCandidateCount >= out.precipitationCandidates.size())
            return;
        if (!readable(object + 0x3BC, 4))
            return;

        const float state = *reinterpret_cast<const float*>(object + 0x388);
        const float intensity = *reinterpret_cast<const float*>(object + 0x3B8);

        if (!std::isfinite(state) || !std::isfinite(intensity))
            return;
        if (state < -0.25f || state > 4.25f)
            return;
        if (intensity < -0.05f || intensity > 2.05f)
            return;

        for (std::uint32_t i = 0; i < out.precipitationCandidateCount; ++i) {
            if (out.precipitationCandidates[i].object == object)
                return;
        }

        auto& c = out.precipitationCandidates[out.precipitationCandidateCount++];
        c.ownerOffset = ownerOffset;
        c.object = object;
        c.stateRaw = state;
        c.intensity = intensity;
    };

    addCandidate(0xFFFFFFFFu, managerAddress);

    for (std::uint32_t off = 0; off < 0x1000; off += 8) {
        const auto address = managerAddress + off;
        if (!readable(address, sizeof(std::uintptr_t)))
            break;

        const auto object = *reinterpret_cast<const std::uintptr_t*>(address);
        if (object < 0x10000)
            continue;

        addCandidate(off, object);
    }

    if (readable(managerAddress + 0x18, sizeof(std::uintptr_t))) {
        const auto object = *reinterpret_cast<const std::uintptr_t*>(managerAddress + 0x18);
        out.manager18Object = object;
        if (object && readable(object, out.manager18Raw.size())) {
            std::memcpy(out.manager18Raw.data(),
                        reinterpret_cast<const void*>(object),
                        out.manager18Raw.size());
            out.manager18RawValid = true;
        }
    }

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
