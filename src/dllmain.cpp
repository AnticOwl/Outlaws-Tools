#include <Windows.h>
#include <cstdint>
#include "ToolRuntime.h"

namespace {
DWORD WINAPI OutlawsTools_Bootstrap(LPVOID) {
    outlaws::ToolRuntime::instance().start();
    return 0;
}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);

        // Start outside the loader-lock path. For normal testing, injection is enough:
        // no manual call to OutlawsTools_Start() is required.
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
    const auto& bindings = outlaws::ToolRuntime::instance().bindings;
    return bindings.environmentRegistryFound ? bindings.environmentRegistry.descriptorOwner() : 0;
}

extern "C" __declspec(dllexport) std::uintptr_t OutlawsTools_GetPointLightType() {
    return outlaws::ToolRuntime::instance().lights.pointTypeDescriptor();
}

extern "C" __declspec(dllexport) std::uintptr_t OutlawsTools_GetSpotLightType() {
    return outlaws::ToolRuntime::instance().lights.spotTypeDescriptor();
}

extern "C" __declspec(dllexport) std::uintptr_t OutlawsTools_GetTubeLightType() {
    return outlaws::ToolRuntime::instance().lights.tubeTypeDescriptor();
}

extern "C" __declspec(dllexport) std::uintptr_t OutlawsTools_GetAreaLightType() {
    return outlaws::ToolRuntime::instance().lights.areaTypeDescriptor();
}
