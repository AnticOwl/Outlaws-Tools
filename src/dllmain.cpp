#include <Windows.h>
#include "ToolRuntime.h"

namespace {
bool pressed(int vk) {
    return (GetAsyncKeyState(vk) & 1) != 0;
}

DWORD WINAPI OutlawsTools_Bootstrap(LPVOID) {
    auto& runtime = outlaws::ToolRuntime::instance();
    if (!runtime.start()) return 0;

    runtime.environment.logRain();
    runtime.environment.logRainMetadata();

    while (runtime.running()) {
        if (pressed(VK_HOME)) {
            runtime.environment.logRain();
            runtime.environment.logRainMetadata();
        }
        if (pressed(VK_NEXT))  runtime.environment.setRain(0.0f); // PageDown
        if (pressed(VK_PRIOR)) runtime.environment.setRain(1.0f); // PageUp
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
