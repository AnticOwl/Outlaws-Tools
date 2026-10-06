#include <Windows.h>
#include <cstdint>
#include "ToolRuntime.h"

namespace {
DWORD WINAPI OutlawsTools_Bootstrap(LPVOID) {
    auto& runtime = outlaws::ToolRuntime::instance();
    if (!runtime.start()) return 0;

    while (runtime.running()) {
        if (GetAsyncKeyState(VK_F8) & 1) {
            runtime.post.setExposure(1.0f);
        }
        if (GetAsyncKeyState(VK_F9) & 1) {
            runtime.post.restoreExposure();
        }
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

extern "C" __declspec(dllexport) std::uintptr_t OutlawsTools_GetEnvRegistryOwner() {
    return 0;
}

extern "C" __declspec(dllexport) std::uintptr_t OutlawsTools_GetPointLightType() {
    return 0;
}

extern "C" __declspec(dllexport) std::uintptr_t OutlawsTools_GetSpotLightType() {
    return 0;
}

extern "C" __declspec(dllexport) std::uintptr_t OutlawsTools_GetTubeLightType() {
    return 0;
}

extern "C" __declspec(dllexport) std::uintptr_t OutlawsTools_GetAreaLightType() {
    return 0;
}
