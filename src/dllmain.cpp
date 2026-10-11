#include <Windows.h>
#include <filesystem>
#include <fstream>
#include "ToolRuntime.h"

namespace {

std::filesystem::path bootLogPath() {
    wchar_t buffer[MAX_PATH]{};
    const auto len = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (!len || len >= std::size(buffer)) return L"OutlawsTools.log";
    std::filesystem::path p(buffer);
    p.replace_filename(L"OutlawsTools.log");
    return p;
}

void bootLog(const char* text) {
    std::ofstream log(bootLogPath(), std::ios::out | std::ios::app);
    if (!log) return;
    log << text << "\n";
    log.flush();
}

bool pressed(int vk) {
    return (GetAsyncKeyState(vk) & 1) != 0;
}

DWORD WINAPI OutlawsTools_Bootstrap(LPVOID) {
    bootLog("BOOTSTRAP THREAD ENTERED");
    auto& runtime = outlaws::ToolRuntime::instance();
    if (!runtime.start()) {
        bootLog("BOOTSTRAP runtime.start FAILED");
        return 0;
    }
    bootLog("BOOTSTRAP runtime.start OK");

    while (runtime.running()) {
        if (pressed(VK_HOME))   runtime.environment.logTimeOfDay();
        if (pressed(VK_NEXT))   runtime.environment.setTimeOfDay(6.0f);    // PageDown
        if (pressed(VK_PRIOR))  runtime.environment.setTimeOfDay(18.0f);   // PageUp
        if (pressed(VK_INSERT)) runtime.environment.setTimePaused(true);
        if (pressed(VK_DELETE)) runtime.environment.setTimePaused(false);
        Sleep(50);
    }
    return 0;
}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        if (HANDLE thread = CreateThread(nullptr, 0, OutlawsTools_Bootstrap, nullptr, 0, nullptr)) {
            CloseHandle(thread);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        outlaws::ToolRuntime::instance().stop();
    }
    return TRUE;
}

extern "C" __declspec(dllexport) bool OutlawsTools_Start() {
    return outlaws::ToolRuntime::instance().start();
}

extern "C" __declspec(dllexport) void OutlawsTools_Stop() {
    outlaws::ToolRuntime::instance().stop();
}
