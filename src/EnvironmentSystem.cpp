#include "EnvironmentSystem.h"
#include <Windows.h>
#include <cstdio>
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

bool EnvironmentSystem::logTimeOfDay() const noexcept {
    const auto* record = static_cast<const std::uint8_t*>(lookupRuntimeValue("Environment.TimeOfDay"));
    if (!record) {
        appendLog("TOD runtime Environment.TimeOfDay = <not found>");
        return false;
    }

    const auto type = *reinterpret_cast<const std::uint32_t*>(record + 0x00);
    const float value = *reinterpret_cast<const float*>(record + 0x10);

    char line[224]{};
    std::snprintf(line, sizeof(line),
        "TOD runtime Environment.TimeOfDay type=%u value=%g record=0x%llX",
        type,
        value,
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(record)));
    appendLog(line);
    return true;
}

bool EnvironmentSystem::setTimeOfDay(float value) noexcept {
    auto* record = static_cast<std::uint8_t*>(lookupRuntimeValue("Environment.TimeOfDay"));
    if (!record) {
        appendLog("TOD DIRECT Environment.TimeOfDay = <not found>");
        return false;
    }

    const auto type = *reinterpret_cast<const std::uint32_t*>(record + 0x00);
    auto* liveValue = reinterpret_cast<float*>(record + 0x10);
    const float before = *liveValue;
    *liveValue = value;
    const float immediate = *liveValue;

    char line[256]{};
    std::snprintf(line, sizeof(line),
        "TOD DIRECT Environment.TimeOfDay type=%u record=0x%llX value %g -> %g immediate=%g",
        type,
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(record)),
        before,
        value,
        immediate);
    appendLog(line);
    return immediate == value;
}

bool EnvironmentSystem::read(EnvironmentState& out) const {
    const auto* record = static_cast<const std::uint8_t*>(lookupRuntimeValue("Environment.TimeOfDay"));
    if (!record) return false;
    out.timeOfDay = *reinterpret_cast<const float*>(record + 0x10);
    return true;
}

bool EnvironmentSystem::apply(const EnvironmentState& state) {
    return setTimeOfDay(state.timeOfDay);
}

}
