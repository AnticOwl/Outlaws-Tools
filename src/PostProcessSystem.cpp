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
    // FF 25 00 00 00 00 ; [absolute 64-bit target]
    at[0] = 0xFF;
    at[1] = 0x25;
    *reinterpret_cast<std::uint32_t*>(at + 2) = 0;
    *reinterpret_cast<std::uintptr_t*>(at + 6) = reinterpret_cast<std::uintptr_t>(destination);
}
}

bool PostProcessSystem::initialize(std::uintptr_t moduleBase) noexcept {
    moduleBase_ = moduleBase;
    environmentSystem_ = 0;
    lastFlags_ = 0;
    lastExtra_ = 0;
    originalExposureKnown_ = false;
    injectedWrite_ = false;

    if (!moduleBase_) return false;
    g_postProcess = this;
    return installFloatSetterHook();
}

void PostProcessSystem::shutdown() noexcept {
    removeFloatSetterHook();
    g_postProcess = nullptr;
    moduleBase_ = 0;
    environmentSystem_ = 0;
    originalExposureKnown_ = false;
}

float PostProcessSystem::originalExposure() const noexcept {
    return std::bit_cast<float>(originalExposureBits_.load());
}

void PostProcessSystem::appendLog(const char* text) const noexcept {
    if (!text) return;
    std::ofstream log(logPath(), std::ios::out | std::ios::app);
    if (!log) return;
    log << text << "\n";
}

bool PostProcessSystem::installFloatSetterHook() noexcept {
    floatSetterTarget_ = moduleBase_ + FloatSetterRva;
    auto* target = reinterpret_cast<unsigned char*>(floatSetterTarget_);

    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(target, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) {
        appendLog("post hook: target not committed");
        return false;
    }

    std::memcpy(originalBytes_, target, sizeof(originalBytes_));

    constexpr std::size_t trampolineSize = 15 + 14;
    auto* trampoline = static_cast<unsigned char*>(
        VirtualAlloc(nullptr, trampolineSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!trampoline) {
        appendLog("post hook: trampoline allocation failed");
        return false;
    }

    std::memcpy(trampoline, originalBytes_, 15);
    writeAbsoluteJump(trampoline + 15, target + 15);
    trampoline_ = trampoline;

    DWORD oldProtect{};
    if (!VirtualProtect(target, 15, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        VirtualFree(trampoline_, 0, MEM_RELEASE);
        trampoline_ = nullptr;
        appendLog("post hook: VirtualProtect failed");
        return false;
    }

    unsigned char patch[15]{};
    writeAbsoluteJump(patch, reinterpret_cast<const void*>(&PostProcessSystem::floatSetterDetour));
    patch[14] = 0x90;
    std::memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD ignored{};
    VirtualProtect(target, 15, oldProtect, &ignored);

    appendLog("post hook: float setter installed");
    return true;
}

void PostProcessSystem::removeFloatSetterHook() noexcept {
    if (floatSetterTarget_) {
        auto* target = reinterpret_cast<unsigned char*>(floatSetterTarget_);
        DWORD oldProtect{};
        if (VirtualProtect(target, 15, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            std::memcpy(target, originalBytes_, sizeof(originalBytes_));
            FlushInstructionCache(GetCurrentProcess(), target, sizeof(originalBytes_));
            DWORD ignored{};
            VirtualProtect(target, 15, oldProtect, &ignored);
        }
    }

    if (trampoline_) {
        VirtualFree(trampoline_, 0, MEM_RELEASE);
        trampoline_ = nullptr;
    }
    floatSetterTarget_ = 0;
}

void PostProcessSystem::floatSetterDetour(void* environmentSystem, std::uint16_t id, float value,
                                         std::uint32_t flags, std::uint32_t extra) {
    auto* self = g_postProcess;
    if (!self) return;

    self->onFloatSetter(environmentSystem, id, value, flags, extra);

    auto original = reinterpret_cast<FloatSetterFn>(self->trampoline_);
    if (original) {
        original(environmentSystem, id, value, flags, extra);
    }
}

void PostProcessSystem::onFloatSetter(void* environmentSystem, std::uint16_t id, float value,
                                      std::uint32_t flags, std::uint32_t extra) noexcept {
    const auto ptr = reinterpret_cast<std::uintptr_t>(environmentSystem);
    if (ptr && !environmentSystem_.load()) {
        environmentSystem_ = ptr;

        char line[128]{};
        std::snprintf(line, sizeof(line), "post EnvironmentSystem=0x%llX",
            static_cast<unsigned long long>(ptr));
        appendLog(line);
    }

    lastFlags_ = flags;
    lastExtra_ = extra;

    if (id == ExposureTargetId && !injectedWrite_.load()) {
        originalExposureBits_ = std::bit_cast<std::uint32_t>(value);
        if (!originalExposureKnown_.exchange(true)) {
            char line[128]{};
            std::snprintf(line, sizeof(line), "post captured ExposureTarget2=%g", value);
            appendLog(line);
        }
    }
}

bool PostProcessSystem::setExposure(float value) noexcept {
    const auto env = environmentSystem_.load();
    auto original = reinterpret_cast<FloatSetterFn>(trampoline_);
    if (!env || !original) {
        appendLog("post F8: EnvironmentSystem not captured yet");
        return false;
    }

    injectedWrite_ = true;
    original(reinterpret_cast<void*>(env), ExposureTargetId, value, lastFlags_.load(), lastExtra_.load());
    injectedWrite_ = false;

    char line[128]{};
    std::snprintf(line, sizeof(line), "post ExposureTarget2 -> %g", value);
    appendLog(line);
    return true;
}

bool PostProcessSystem::restoreExposure() noexcept {
    if (!originalExposureKnown_.load()) {
        appendLog("post F9: original ExposureTarget2 not captured");
        return false;
    }
    return setExposure(originalExposure());
}

bool PostProcessSystem::read(PostProcessState&) const { return false; }
bool PostProcessSystem::apply(const PostProcessState&) { return false; }
}
