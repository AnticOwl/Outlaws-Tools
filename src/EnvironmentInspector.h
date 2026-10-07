#pragma once
#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <thread>

namespace outlaws {

class EnvironmentSystem;

class EnvironmentInspector {
public:
    bool start(EnvironmentSystem* environment, std::uintptr_t moduleBase) noexcept;
    void stop() noexcept;
    void show() noexcept;
    void hide() noexcept;
    void toggle() noexcept;
    [[nodiscard]] bool running() const noexcept { return running_.load(); }

private:
    static DWORD WINAPI threadProc(LPVOID param);
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool createWindow() noexcept;
    void destroyWindow() noexcept;
    void sample() noexcept;
    void paint(HDC dc, const RECT& client) noexcept;
    void appendLog(const char* text) const noexcept;

    void drawSection(HDC dc, int& y, int left, int right, const wchar_t* title) noexcept;
    void drawRow(HDC dc, int& y, int left, int right, const wchar_t* label,
                 const wchar_t* value, COLORREF valueColor) noexcept;

    EnvironmentSystem* environment_{};
    std::uintptr_t moduleBase_{};

    std::atomic_bool running_{};
    std::atomic_bool visible_{true};
    HANDLE thread_{};
    HWND hwnd_{};
    HFONT font_{};
    HFONT fontBold_{};

    float timeOfDay_{};
    bool timePaused_{};
    bool todAvailable_{};
    std::uintptr_t todSystem_{};

    bool firstSample_{true};
    bool lastPaused_{};
};

}
