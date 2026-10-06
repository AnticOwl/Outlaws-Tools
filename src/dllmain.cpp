#include <Windows.h>
#include <cstdint>
#include "ToolRuntime.h"

namespace {
bool pressed(int vk) {
    return (GetAsyncKeyState(vk) & 1) != 0;
}

bool ctrlDown() {
    return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
}

DWORD WINAPI OutlawsTools_Bootstrap(LPVOID) {
    auto& runtime = outlaws::ToolRuntime::instance();
    if (!runtime.start()) return 0;

    while (runtime.running()) {
        const bool ctrl = ctrlDown();

        // Remote-friendly Post diagnostics: no function-key modifiers required.
        if (pressed(VK_HOME))     runtime.post.setCameraLensOptics(true);
        if (pressed(VK_END))      runtime.post.setCameraLensOptics(false);

        if (pressed(VK_INSERT))   runtime.post.setGlareEnabled(true);
        if (pressed(VK_DELETE))   runtime.post.setGlareEnabled(false);

        if (pressed(VK_PRIOR))    runtime.post.setLensGlare(true);   // PageUp
        if (pressed(VK_NEXT))     runtime.post.setLensGlare(false);  // PageDown

        if (pressed(VK_PAUSE))    runtime.post.setLensVeilingGlare(true);
        if (pressed(VK_SCROLL))   runtime.post.setLensVeilingGlare(false);

        if (pressed(VK_ADD))      runtime.post.setGlare(10.0f);
        if (pressed(VK_SUBTRACT)) runtime.post.setGlare(0.0f);

        if (!ctrl) {
            if (pressed(VK_F1))  runtime.post.setExposure(0.0f);
            if (pressed(VK_F2))  runtime.post.setExposure(10.0f);

            if (pressed(VK_F3))  runtime.post.setBloom(0.0f);
            if (pressed(VK_F4))  runtime.post.setBloom(10.0f);

            if (pressed(VK_F5))  runtime.post.setGlare(0.0f);
            if (pressed(VK_F6))  runtime.post.setGlare(10.0f);

            if (pressed(VK_F7))  runtime.post.setFilmGrainAmount(0.0f);
            if (pressed(VK_F8))  runtime.post.setFilmGrainAmount(10.0f);

            if (pressed(VK_F9))  runtime.post.setLensFlare(false);
            if (pressed(VK_F10)) runtime.post.setLensFlare(true);

            if (pressed(VK_F11)) runtime.post.setDepthOfField(false);
            if (pressed(VK_F12)) runtime.post.setDepthOfField(true);
        } else {
            if (pressed(VK_F1))  runtime.post.setLensGlare(false);
            if (pressed(VK_F2))  runtime.post.setLensGlare(true);

            if (pressed(VK_F3))  runtime.post.setLensVeilingGlare(false);
            if (pressed(VK_F4))  runtime.post.setLensVeilingGlare(true);

            if (pressed(VK_F5))  runtime.post.setGlareEnabled(false);
            if (pressed(VK_F6))  runtime.post.setGlareEnabled(true);

            if (pressed(VK_F7))  runtime.post.setFilmGrain(false);
            if (pressed(VK_F8))  runtime.post.setFilmGrain(true);

            if (pressed(VK_F9))  runtime.post.setCameraLensOptics(false);
            if (pressed(VK_F10)) runtime.post.setCameraLensOptics(true);
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
