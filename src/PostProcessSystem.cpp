#include "PostProcessSystem.h"
#include <Windows.h>
#include <bit>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace outlaws {
namespace {
PostProcessSystem* g_postProcess = nullptr;

std::filesystem::path logPath() {
    wchar_t buffer[MAX_PATH]{};
    const auto len = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (!len || len >= std::size(buffer)) return L"OutlawsTools.log";
    std::filesystem::path p(buffer);
    p.replace_filename(L"OutlawsTools.log");
    return p;
}

void writeAbsoluteJump(unsigned char* at, const void* destination) {
    at[0] = 0xFF;
    at[1] = 0x25;
    *reinterpret_cast<std::uint32_t*>(at + 2) = 0;
    *reinterpret_cast<std::uintptr_t*>(at + 6) = reinterpret_cast<std::uintptr_t>(destination);
}

bool patchHook(std::uintptr_t targetAddress,
               const void* detour,
               unsigned char* originalBytes,
               std::size_t stolenLength,
               void*& trampolineOut) {
    auto* target = reinterpret_cast<unsigned char*>(targetAddress);

    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(target, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) return false;

    std::memcpy(originalBytes, target, stolenLength);

    const std::size_t trampolineSize = stolenLength + 14;
    auto* trampoline = static_cast<unsigned char*>(
        VirtualAlloc(nullptr, trampolineSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!trampoline) return false;

    std::memcpy(trampoline, originalBytes, stolenLength);
    writeAbsoluteJump(trampoline + stolenLength, target + stolenLength);

    DWORD oldProtect{};
    if (!VirtualProtect(target, stolenLength, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }

    unsigned char patch[32]{};
    writeAbsoluteJump(patch, detour);
    for (std::size_t i = 14; i < stolenLength; ++i) patch[i] = 0x90;

    std::memcpy(target, patch, stolenLength);
    FlushInstructionCache(GetCurrentProcess(), target, stolenLength);

    DWORD ignored{};
    VirtualProtect(target, stolenLength, oldProtect, &ignored);

    trampolineOut = trampoline;
    return true;
}

void unpatchHook(std::uintptr_t targetAddress,
                 const unsigned char* originalBytes,
                 std::size_t stolenLength,
                 void*& trampoline) {
    if (targetAddress) {
        auto* target = reinterpret_cast<unsigned char*>(targetAddress);
        DWORD oldProtect{};
        if (VirtualProtect(target, stolenLength, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            std::memcpy(target, originalBytes, stolenLength);
            FlushInstructionCache(GetCurrentProcess(), target, stolenLength);
            DWORD ignored{};
            VirtualProtect(target, stolenLength, oldProtect, &ignored);
        }
    }

    if (trampoline) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        trampoline = nullptr;
    }
}
}

bool PostProcessSystem::initialize(std::uintptr_t moduleBase) noexcept {
    moduleBase_ = moduleBase;
    environmentSystem_ = 0;
    lastFlags_ = 0;
    lastExtra_ = 0;

    if (!moduleBase_) return false;
    g_postProcess = this;

    const bool updateHook = installEnvironmentUpdateHook();
    const bool setterHook = installFloatSetterHook();

    if (!updateHook) appendLog("post hook: Environment update hook failed");
    if (!setterHook) appendLog("post hook: float setter hook failed");

    return updateHook && setterHook;
}

void PostProcessSystem::shutdown() noexcept {
    removeFloatSetterHook();
    removeEnvironmentUpdateHook();
    g_postProcess = nullptr;
    moduleBase_ = 0;
    environmentSystem_ = 0;
}

void PostProcessSystem::appendLog(const char* text) const noexcept {
    if (!text) return;
    std::ofstream log(logPath(), std::ios::out | std::ios::app);
    if (!log) return;
    log << text << "\n";
    log.flush();
}

bool PostProcessSystem::installEnvironmentUpdateHook() noexcept {
    environmentUpdateTarget_ = moduleBase_ + EnvironmentUpdateRva;
    if (!patchHook(environmentUpdateTarget_,
                   reinterpret_cast<const void*>(&PostProcessSystem::environmentUpdateDetour),
                   environmentUpdateOriginalBytes_,
                   sizeof(environmentUpdateOriginalBytes_),
                   environmentUpdateTrampoline_)) {
        return false;
    }
    appendLog("post hook: Environment update installed");
    return true;
}

void PostProcessSystem::removeEnvironmentUpdateHook() noexcept {
    unpatchHook(environmentUpdateTarget_,
                environmentUpdateOriginalBytes_,
                sizeof(environmentUpdateOriginalBytes_),
                environmentUpdateTrampoline_);
    environmentUpdateTarget_ = 0;
}

bool PostProcessSystem::installFloatSetterHook() noexcept {
    floatSetterTarget_ = moduleBase_ + FloatSetterRva;
    if (!patchHook(floatSetterTarget_,
                   reinterpret_cast<const void*>(&PostProcessSystem::floatSetterDetour),
                   floatSetterOriginalBytes_,
                   sizeof(floatSetterOriginalBytes_),
                   floatSetterTrampoline_)) {
        return false;
    }
    appendLog("post hook: float setter installed");
    return true;
}

void PostProcessSystem::removeFloatSetterHook() noexcept {
    unpatchHook(floatSetterTarget_,
                floatSetterOriginalBytes_,
                sizeof(floatSetterOriginalBytes_),
                floatSetterTrampoline_);
    floatSetterTarget_ = 0;
}

void PostProcessSystem::environmentUpdateDetour(void* environmentSystem) {
    auto* self = g_postProcess;
    if (!self) return;

    self->onEnvironmentUpdate(environmentSystem);
    self->logPreviousQueueDrain(environmentSystem);

    auto original = reinterpret_cast<EnvironmentUpdateFn>(self->environmentUpdateTrampoline_);
    if (original) original(environmentSystem);

    // A command queued on the previous update has now had one native Snowdrop
    // Environment pass in which to be consumed. Read the active runtime value now.
    self->logRuntimeAfter(environmentSystem);

    self->processPending(environmentSystem);
}

void PostProcessSystem::floatSetterDetour(void* environmentSystem, std::uint16_t id, float value,
                                         std::uint32_t flags, std::uint32_t extra) {
    auto* self = g_postProcess;
    if (!self) return;

    self->onFloatSetter(environmentSystem, id, value, flags, extra);

    auto original = reinterpret_cast<FloatSetterFn>(self->floatSetterTrampoline_);
    if (original) original(environmentSystem, id, value, flags, extra);
}

void PostProcessSystem::onEnvironmentUpdate(void* environmentSystem) noexcept {
    const auto ptr = reinterpret_cast<std::uintptr_t>(environmentSystem);
    if (!ptr) return;

    std::uintptr_t expected = 0;
    if (environmentSystem_.compare_exchange_strong(expected, ptr)) {
        char line[128]{};
        std::snprintf(line, sizeof(line), "post EnvironmentSystem=0x%llX",
            static_cast<unsigned long long>(ptr));
        appendLog(line);
    }
}

void PostProcessSystem::onFloatSetter(void* environmentSystem, std::uint16_t id, float value,
                                      std::uint32_t flags, std::uint32_t extra) noexcept {
    const auto ptr = reinterpret_cast<std::uintptr_t>(environmentSystem);
    if (ptr && !environmentSystem_.load()) environmentSystem_ = ptr;

    lastFlags_ = flags;
    lastExtra_ = extra;

    char line[160]{};
    std::snprintf(line, sizeof(line),
        "post native float id=0x%X value=%g flags=0x%X extra=0x%X",
        static_cast<unsigned>(id), value, flags, extra);
    appendLog(line);
}

bool PostProcessSystem::setFloat(std::uint16_t id, float value, const char* label) noexcept {
    if (!ready()) {
        appendLog("post queue: EnvironmentSystem not captured yet");
        return false;
    }

    logDescriptorDefault(id, false, label);
    logRuntimeBefore(label, false);

    pendingId_ = id;
    pendingValueBits_ = std::bit_cast<std::uint32_t>(value);
    pendingKind_ = 1;

    char line[160]{};
    std::snprintf(line, sizeof(line), "post queued %s -> %g", label ? label : "float", value);
    appendLog(line);
    return true;
}

bool PostProcessSystem::setBool(std::uint16_t id, bool value, const char* label) noexcept {
    if (!ready()) {
        appendLog("post queue: EnvironmentSystem not captured yet");
        return false;
    }

    logDescriptorDefault(id, true, label);
    logRuntimeBefore(label, true);

    pendingId_ = id;
    pendingValueBits_ = value ? 1u : 0u;
    pendingKind_ = 2;

    char line[160]{};
    std::snprintf(line, sizeof(line), "post queued %s -> %s",
        label ? label : "bool", value ? "ON" : "OFF");
    appendLog(line);
    return true;
}

void PostProcessSystem::processPending(void* environmentSystem) noexcept {
    const int kind = pendingKind_.exchange(0);
    if (!kind || !environmentSystem || !moduleBase_) return;

    const auto id = pendingId_.load();
    const auto bits = pendingValueBits_.load();

    std::uint32_t slot = 0;
    const auto before = queueCountForCurrentThread(environmentSystem, &slot);

    if (kind == 1) {
        const float value = std::bit_cast<float>(bits);
        auto setter = reinterpret_cast<FloatSetterFn>(moduleBase_ + FloatSetterRva);
        setter(environmentSystem, id, value, lastFlags_.load(), lastExtra_.load());

        char line[192]{};
        std::snprintf(line, sizeof(line),
            "post native-thread float id=0x%X value=%g slot=%u queueBefore=%u",
            static_cast<unsigned>(id), value, slot, before);
        appendLog(line);
    } else if (kind == 2) {
        const bool value = bits != 0;
        auto setter = reinterpret_cast<BoolSetterFn>(moduleBase_ + BoolSetterRva);
        setter(environmentSystem, id, value, lastFlags_.load(), lastExtra_.load());

        char line[192]{};
        std::snprintf(line, sizeof(line),
            "post native-thread bool id=0x%X value=%s slot=%u queueBefore=%u",
            static_cast<unsigned>(id), value ? "ON" : "OFF", slot, before);
        appendLog(line);
    }

    const auto after = queueCountForCurrentThread(environmentSystem, nullptr);

    queueWatchSlot_ = slot;
    queueWatchBefore_ = before;
    queueWatchAfter_ = after;
    queueWatchId_ = id;
    queueWatchActive_ = true;
    runtimeWatchId_ = id;
    runtimeWatchIsBool_ = (kind == 2);
    runtimeWatchActive_ = true;

    char line[192]{};
    std::snprintf(line, sizeof(line),
        "post queue accepted id=0x%X slot=%u count %u -> %u",
        static_cast<unsigned>(id), slot, before, after);
    appendLog(line);
}

void PostProcessSystem::logDescriptorDefault(std::uint16_t id, bool isBool, const char* label) noexcept {
    if (!moduleBase_ || id >= 0x1E8) return;

    const auto descriptor = moduleBase_ + EnvRegistryOwnerRva
        + static_cast<std::uintptr_t>(id) * 0x40u;

    if (isBool) {
        const bool value = *reinterpret_cast<const std::uint8_t*>(descriptor) != 0;
        char line[192]{};
        std::snprintf(line, sizeof(line),
            "post descriptor default %s id=0x%X = %s",
            label ? label : "bool", static_cast<unsigned>(id), value ? "ON" : "OFF");
        appendLog(line);
    } else {
        const auto bits = *reinterpret_cast<const std::uint32_t*>(descriptor);
        const float value = std::bit_cast<float>(bits);
        char line[192]{};
        std::snprintf(line, sizeof(line),
            "post descriptor default %s id=0x%X = %g",
            label ? label : "float", static_cast<unsigned>(id), value);
        appendLog(line);
    }
}


void* PostProcessSystem::lookupRuntimeValue(const char* envName) const noexcept {
    if (!moduleBase_ || !envName || !*envName) return nullptr;
    using RuntimeLookupFn = void*(*)(const char*);
    auto lookup = reinterpret_cast<RuntimeLookupFn>(moduleBase_ + RuntimeLookupRva);
    return lookup(envName);
}

void PostProcessSystem::logRuntimeBefore(const char* envName, bool isBool) noexcept {
    const auto* record = static_cast<const std::uint8_t*>(lookupRuntimeValue(envName));
    if (!record) {
        char line[192]{};
        std::snprintf(line, sizeof(line), "post runtime BEFORE %s = <not found>",
            envName ? envName : "<null>");
        appendLog(line);
        return;
    }

    const auto type = *reinterpret_cast<const std::uint32_t*>(record + 0x00);
    char line[224]{};
    if (isBool) {
        const bool value = *(record + 0x10) != 0;
        std::snprintf(line, sizeof(line),
            "post runtime BEFORE %s type=%u value=%s record=0x%llX",
            envName, type, value ? "ON" : "OFF",
            static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(record)));
    } else {
        const float value = *reinterpret_cast<const float*>(record + 0x10);
        std::snprintf(line, sizeof(line),
            "post runtime BEFORE %s type=%u value=%g record=0x%llX",
            envName, type, value,
            static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(record)));
    }
    appendLog(line);
}

void PostProcessSystem::logRuntimeAfter(void*) noexcept {
    if (!runtimeWatchActive_.exchange(false)) return;

    const auto id = runtimeWatchId_.load();
    const char* envName = nullptr;
    switch (id) {
    case BloomStrengthId:           envName = "Env_BloomStrength2"; break;
    case GlareStrengthId:           envName = "Env_GlareStrength"; break;
    case ExposureTargetId:          envName = "Env_ExposureTarget2"; break;
    case FilmGrainAmountId:         envName = "Env_FilmGrainAmount"; break;
    case LensFlareEnabledId:        envName = "Env_LensFlareEnabled"; break;
    case DepthOfFieldEnabledId:     envName = "Env_DepthOfFieldEnabled"; break;
    case FilmGrainEnabledId:        envName = "Env_FilmGrainEnabled"; break;
    case LensVeilingGlareEnabledId: envName = "Env_LensVeilingGlareEnabled"; break;
    case LensGlareEnabledId:        envName = "Env_LensGlareEnabled"; break;
    case GlareEnabledId:            envName = "Env_GlareEnabled"; break;
    case CameraLensOpticsEnabledId: envName = "Env_CameraLensOpticsEnabled"; break;
    default: break;
    }

    if (!envName) {
        char line[128]{};
        std::snprintf(line, sizeof(line),
            "post runtime AFTER id=0x%X = <unknown name>",
            static_cast<unsigned>(id));
        appendLog(line);
        return;
    }

    const auto* record = static_cast<const std::uint8_t*>(lookupRuntimeValue(envName));
    if (!record) {
        char line[192]{};
        std::snprintf(line, sizeof(line), "post runtime AFTER %s = <not found>", envName);
        appendLog(line);
        return;
    }

    const auto type = *reinterpret_cast<const std::uint32_t*>(record + 0x00);
    char line[224]{};
    if (runtimeWatchIsBool_.load()) {
        const bool value = *(record + 0x10) != 0;
        std::snprintf(line, sizeof(line),
            "post runtime AFTER %s type=%u value=%s record=0x%llX",
            envName, type, value ? "ON" : "OFF",
            static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(record)));
    } else {
        const float value = *reinterpret_cast<const float*>(record + 0x10);
        std::snprintf(line, sizeof(line),
            "post runtime AFTER %s type=%u value=%g record=0x%llX",
            envName, type, value,
            static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(record)));
    }
    appendLog(line);
}

std::uint32_t PostProcessSystem::queueCountForCurrentThread(
    void* environmentSystem, std::uint32_t* slotOut) noexcept {
    if (!environmentSystem || !moduleBase_) return 0;

    using ThreadSlotFn = int(*)();
    auto threadSlot = reinterpret_cast<ThreadSlotFn>(moduleBase_ + ThreadSlotRva);
    const int slot = threadSlot();
    if (slot < 0 || slot >= 0x1000) return 0;

    if (slotOut) *slotOut = static_cast<std::uint32_t>(slot);
    return queueCountForSlot(environmentSystem, static_cast<std::uint32_t>(slot));
}

std::uint32_t PostProcessSystem::queueCountForSlot(
    void* environmentSystem, std::uint32_t slot) noexcept {
    if (!environmentSystem || slot >= 0x1000) return 0;

    const auto lane = reinterpret_cast<const std::uint8_t*>(environmentSystem)
        + static_cast<std::size_t>(slot) * 0x10u;

    return *reinterpret_cast<const std::uint32_t*>(lane + 0x8);
}

void PostProcessSystem::logPreviousQueueDrain(void* environmentSystem) noexcept {
    if (!queueWatchActive_.exchange(false)) return;

    const auto expectedSlot = queueWatchSlot_.load();
    const auto current = queueCountForSlot(environmentSystem, expectedSlot);

    std::uint32_t currentThreadSlot = 0;
    (void)queueCountForCurrentThread(environmentSystem, &currentThreadSlot);

    char line[224]{};
    std::snprintf(line, sizeof(line),
        "post queue next-frame id=0x%X originalSlot=%u currentThreadSlot=%u count=%u (afterWrite=%u)",
        static_cast<unsigned>(queueWatchId_.load()),
        expectedSlot,
        currentThreadSlot,
        current,
        queueWatchAfter_.load());
    appendLog(line);
}

bool PostProcessSystem::setExposure(float value) noexcept {
    return setFloat(ExposureTargetId, value, "Env_ExposureTarget2");
}

bool PostProcessSystem::setBloom(float value) noexcept {
    return setFloat(BloomStrengthId, value, "Env_BloomStrength2");
}

bool PostProcessSystem::setGlare(float value) noexcept {
    return setFloat(GlareStrengthId, value, "Env_GlareStrength");
}

bool PostProcessSystem::setFilmGrainAmount(float value) noexcept {
    return setFloat(FilmGrainAmountId, value, "Env_FilmGrainAmount");
}

bool PostProcessSystem::setLensFlare(bool enabled) noexcept {
    return setBool(LensFlareEnabledId, enabled, "Env_LensFlareEnabled");
}

bool PostProcessSystem::setDepthOfField(bool enabled) noexcept {
    return setBool(DepthOfFieldEnabledId, enabled, "Env_DepthOfFieldEnabled");
}

bool PostProcessSystem::setFilmGrain(bool enabled) noexcept {
    return setBool(FilmGrainEnabledId, enabled, "Env_FilmGrainEnabled");
}

bool PostProcessSystem::setLensGlare(bool enabled) noexcept {
    return setBool(LensGlareEnabledId, enabled, "Env_LensGlareEnabled");
}

bool PostProcessSystem::setLensVeilingGlare(bool enabled) noexcept {
    return setBool(LensVeilingGlareEnabledId, enabled, "Env_LensVeilingGlareEnabled");
}

bool PostProcessSystem::setGlareEnabled(bool enabled) noexcept {
    return setBool(GlareEnabledId, enabled, "Env_GlareEnabled");
}

bool PostProcessSystem::setCameraLensOptics(bool enabled) noexcept {
    return setBool(CameraLensOpticsEnabledId, enabled, "Env_CameraLensOpticsEnabled");
}

bool PostProcessSystem::applyTestPresetOff() noexcept {
    if (!ready()) {
        appendLog("post preset OFF: EnvironmentSystem not captured yet");
        return false;
    }

    bool ok = true;
    ok &= setExposure(1.0f);
    ok &= setBloom(0.0f);
    ok &= setGlare(0.0f);
    ok &= setFilmGrainAmount(0.0f);
    ok &= setLensFlare(false);
    ok &= setDepthOfField(false);
    ok &= setFilmGrain(false);
    ok &= setLensGlare(false);
    ok &= setLensVeilingGlare(false);
    ok &= setGlareEnabled(false);
    ok &= setCameraLensOptics(false);
    appendLog(ok ? "post preset OFF complete" : "post preset OFF completed with failures");
    return ok;
}

bool PostProcessSystem::applyTestPresetOn() noexcept {
    if (!ready()) {
        appendLog("post preset ON: EnvironmentSystem not captured yet");
        return false;
    }

    bool ok = true;
    ok &= setExposure(1.0f);
    ok &= setBloom(1.0f);
    ok &= setGlare(1.0f);
    ok &= setFilmGrainAmount(1.0f);
    ok &= setLensFlare(true);
    ok &= setDepthOfField(true);
    ok &= setFilmGrain(true);
    ok &= setLensGlare(true);
    ok &= setLensVeilingGlare(true);
    ok &= setGlareEnabled(true);
    ok &= setCameraLensOptics(true);
    appendLog(ok ? "post preset ON complete" : "post preset ON completed with failures");
    return ok;
}
}
