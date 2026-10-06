#include "PostProcessSystem.h"
#include <Windows.h>
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

    auto original = reinterpret_cast<EnvironmentUpdateFn>(self->environmentUpdateTrampoline_);
    if (original) original(environmentSystem);
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
    const auto env = environmentSystem_.load();
    if (!env || !moduleBase_) {
        appendLog("post write: EnvironmentSystem not captured yet");
        return false;
    }

    auto setter = reinterpret_cast<FloatSetterFn>(moduleBase_ + FloatSetterRva);
    setter(reinterpret_cast<void*>(env), id, value, lastFlags_.load(), lastExtra_.load());

    char line[160]{};
    std::snprintf(line, sizeof(line), "post %s -> %g", label ? label : "float", value);
    appendLog(line);
    return true;
}

bool PostProcessSystem::setBool(std::uint16_t id, bool value, const char* label) noexcept {
    const auto env = environmentSystem_.load();
    if (!env || !moduleBase_) {
        appendLog("post write: EnvironmentSystem not captured yet");
        return false;
    }

    auto setter = reinterpret_cast<BoolSetterFn>(moduleBase_ + BoolSetterRva);
    setter(reinterpret_cast<void*>(env), id, value, lastFlags_.load(), lastExtra_.load());

    char line[160]{};
    std::snprintf(line, sizeof(line), "post %s -> %s",
        label ? label : "bool", value ? "ON" : "OFF");
    appendLog(line);
    return true;
}

bool PostProcessSystem::setExposure(float value) noexcept {
    return setFloat(ExposureTargetId, value, "ExposureTarget2");
}

bool PostProcessSystem::setBloom(float value) noexcept {
    return setFloat(BloomStrengthId, value, "BloomStrength2");
}

bool PostProcessSystem::setGlare(float value) noexcept {
    return setFloat(GlareStrengthId, value, "GlareStrength");
}

bool PostProcessSystem::setFilmGrainAmount(float value) noexcept {
    return setFloat(FilmGrainAmountId, value, "FilmGrainAmount");
}

bool PostProcessSystem::setLensFlare(bool enabled) noexcept {
    return setBool(LensFlareEnabledId, enabled, "LensFlareEnabled");
}

bool PostProcessSystem::setDepthOfField(bool enabled) noexcept {
    return setBool(DepthOfFieldEnabledId, enabled, "DepthOfFieldEnabled");
}

bool PostProcessSystem::setFilmGrain(bool enabled) noexcept {
    return setBool(FilmGrainEnabledId, enabled, "FilmGrainEnabled");
}

bool PostProcessSystem::setLensGlare(bool enabled) noexcept {
    return setBool(LensGlareEnabledId, enabled, "LensGlareEnabled");
}

bool PostProcessSystem::setLensVeilingGlare(bool enabled) noexcept {
    return setBool(LensVeilingGlareEnabledId, enabled, "LensVeilingGlareEnabled");
}

bool PostProcessSystem::setGlareEnabled(bool enabled) noexcept {
    return setBool(GlareEnabledId, enabled, "GlareEnabled");
}

bool PostProcessSystem::setCameraLensOptics(bool enabled) noexcept {
    return setBool(CameraLensOpticsEnabledId, enabled, "CameraLensOpticsEnabled");
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
