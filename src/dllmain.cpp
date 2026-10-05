#include <Windows.h>
#include <cstdint>
#include "ToolRuntime.h"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
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
