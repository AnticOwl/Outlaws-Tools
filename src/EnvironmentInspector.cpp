#include "EnvironmentInspector.h"
#include "EnvironmentSystem.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace outlaws {
namespace {
constexpr UINT_PTR kTimerId = 1;
constexpr int kBtn0600 = 1001;
constexpr int kBtn1200 = 1002;
constexpr int kBtn1800 = 1003;
constexpr int kBtnPause = 1004;
constexpr int kBtnResume = 1005;

std::filesystem::path logPath() {
    wchar_t buffer[MAX_PATH]{};
    const auto len = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (!len || len >= std::size(buffer)) return L"OutlawsTools.log";
    std::filesystem::path p(buffer);
    p.replace_filename(L"OutlawsTools.log");
    return p;
}

COLORREF statusColor(bool known, bool on) {
    if (!known) return RGB(145, 145, 155);
    return on ? RGB(88, 210, 130) : RGB(235, 105, 105);
}
}

bool EnvironmentInspector::start(EnvironmentSystem* environment, std::uintptr_t moduleBase) noexcept {
    if (running_.exchange(true)) return true;
    environment_ = environment;
    moduleBase_ = moduleBase;

    thread_ = CreateThread(nullptr, 0, &EnvironmentInspector::threadProc, this, 0, nullptr);
    if (!thread_) {
        running_ = false;
        return false;
    }
    return true;
}

void EnvironmentInspector::stop() noexcept {
    if (!running_.exchange(false)) return;
    if (hwnd_) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    if (thread_) {
        WaitForSingleObject(thread_, 1500);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
}

void EnvironmentInspector::show() noexcept {
    visible_ = true;
    if (hwnd_) ShowWindow(hwnd_, SW_SHOW);
}

void EnvironmentInspector::hide() noexcept {
    visible_ = false;
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

void EnvironmentInspector::toggle() noexcept {
    if (visible_.load()) hide(); else show();
}

DWORD WINAPI EnvironmentInspector::threadProc(LPVOID param) {
    auto* self = static_cast<EnvironmentInspector*>(param);
    if (!self || !self->createWindow()) {
        if (self) self->running_ = false;
        return 0;
    }

    MSG msg{};
    while (self->running_.load() && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    self->destroyWindow();
    return 0;
}

bool EnvironmentInspector::createWindow() noexcept {
    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &EnvironmentInspector::wndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"OutlawsEnvironmentInspector";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(22, 22, 26));
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"Outlaws Tools — Environment Inspector",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        80, 80, 650, 760,
        nullptr, nullptr, instance, this);

    if (!hwnd_) return false;

    font_ = CreateFontW(
        -17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    fontBold_ = CreateFontW(
        -19, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    const struct ButtonDef { int id; int x; int w; const wchar_t* text; } buttons[] = {
        {kBtn0600, 24, 104, L"06:00"},
        {kBtn1200, 136, 104, L"12:00"},
        {kBtn1800, 248, 104, L"18:00"},
        {kBtnPause, 374, 104, L"Pause TOD"},
        {kBtnResume, 486, 116, L"Resume TOD"},
    };

    for (const auto& b : buttons) {
        HWND h = CreateWindowW(L"BUTTON", b.text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               b.x, 628, b.w, 32, hwnd_,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(b.id)),
                               instance, nullptr);
        if (h && font_) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }

    SetTimer(hwnd_, kTimerId, 250, nullptr);
    sample();
    appendLog("Environment Inspector started");

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    return true;
}

void EnvironmentInspector::destroyWindow() noexcept {
    if (hwnd_) {
        KillTimer(hwnd_, kTimerId);
        hwnd_ = nullptr;
    }
    if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
    }
    if (fontBold_) {
        DeleteObject(fontBold_);
        fontBold_ = nullptr;
    }
}

LRESULT CALLBACK EnvironmentInspector::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    EnvironmentInspector* self = reinterpret_cast<EnvironmentInspector*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<EnvironmentInspector*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    switch (msg) {
    case WM_TIMER:
        if (self && wParam == kTimerId) {
            self->sample();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_COMMAND:
        if (!self || !self->environment_) return 0;
        switch (LOWORD(wParam)) {
        case kBtn0600: self->environment_->setTimeOfDay(6.0f); break;
        case kBtn1200: self->environment_->setTimeOfDay(12.0f); break;
        case kBtn1800: self->environment_->setTimeOfDay(18.0f); break;
        case kBtnPause: self->environment_->setTimePaused(true); break;
        case kBtnResume: self->environment_->setTimePaused(false); break;
        default: break;
        }
        return 0;

    case WM_PAINT:
        if (self) {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            RECT client{};
            GetClientRect(hwnd, &client);
            self->paint(dc, client);
            EndPaint(hwnd, &ps);
            return 0;
        }
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_CLOSE:
        if (self) {
            self->visible_ = false;
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void EnvironmentInspector::sample() noexcept {
    if (!environment_) return;

    EnvironmentState state{};
    const bool ok = environment_->read(state);

    todAvailable_ = ok;
    if (ok) {
        timeOfDay_ = state.timeOfDay;
        timePaused_ = state.timePaused;

        if (!firstSample_ && lastPaused_ != timePaused_) {
            char line[160]{};
            std::snprintf(line, sizeof(line),
                "=== ENVIRONMENT CHANGE ===\nTOD Paused: %s -> %s",
                lastPaused_ ? "ON" : "OFF",
                timePaused_ ? "ON" : "OFF");
            appendLog(line);
        }

        lastPaused_ = timePaused_;
    }

    firstSample_ = false;
}

void EnvironmentInspector::paint(HDC dc, const RECT& client) noexcept {
    HBRUSH bg = CreateSolidBrush(RGB(22, 22, 26));
    FillRect(dc, &client, bg);
    DeleteObject(bg);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(235, 235, 240));

    RECT title{24, 20, client.right - 24, 55};
    SelectObject(dc, fontBold_);
    DrawTextW(dc, L"Environment Inspector", -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT sub{24, 52, client.right - 24, 78};
    SelectObject(dc, font_);
    SetTextColor(dc, RGB(150, 150, 165));
    DrawTextW(dc, L"Scene-aware weather diagnostics — read-only except validated TOD controls", -1,
              &sub, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int y = 92;
    drawSection(dc, y, 24, client.right - 24, L"SCENE / WEATHER STATUS");
    drawRow(dc, y, 24, client.right - 24, L"Scene / Zone", L"UNRESOLVED", RGB(145,145,155));
    drawRow(dc, y, 24, client.right - 24, L"Weather Preset", L"UNRESOLVED", RGB(145,145,155));
    drawRow(dc, y, 24, client.right - 24, L"Indoor", L"UNRESOLVED", RGB(145,145,155));
    drawRow(dc, y, 24, client.right - 24, L"Level Weather Override", L"UNRESOLVED", RGB(145,145,155));

    y += 10;
    drawSection(dc, y, 24, client.right - 24, L"PRECIPITATION");
    drawRow(dc, y, 24, client.right - 24, L"Raining", L"UNRESOLVED", RGB(145,145,155));
    drawRow(dc, y, 24, client.right - 24, L"Precipitation Intensity", L"UNRESOLVED", RGB(145,145,155));
    drawRow(dc, y, 24, client.right - 24, L"Gameplay Rain", L"UNRESOLVED", RGB(145,145,155));
    drawRow(dc, y, 24, client.right - 24, L"Graphics Rain", L"UNRESOLVED", RGB(145,145,155));
    drawRow(dc, y, 24, client.right - 24, L"Sandstorm", L"UNRESOLVED", RGB(145,145,155));
    drawRow(dc, y, 24, client.right - 24, L"Snow", L"UNRESOLVED", RGB(145,145,155));

    y += 10;
    drawSection(dc, y, 24, client.right - 24, L"ATMOSPHERE");
    drawRow(dc, y, 24, client.right - 24, L"Fog", L"UNRESOLVED", RGB(145,145,155));
    drawRow(dc, y, 24, client.right - 24, L"Outdoor Fog", L"UNRESOLVED", RGB(145,145,155));
    drawRow(dc, y, 24, client.right - 24, L"Cloud Cover", L"UNRESOLVED", RGB(145,145,155));
    drawRow(dc, y, 24, client.right - 24, L"Wind Strength", L"UNRESOLVED", RGB(145,145,155));
    drawRow(dc, y, 24, client.right - 24, L"Weather Mask Wetness", L"UNRESOLVED", RGB(145,145,155));

    y += 10;
    drawSection(dc, y, 24, client.right - 24, L"TIME OF DAY");

    wchar_t timeText[64]{};
    if (todAvailable_) {
        const int totalSeconds = static_cast<int>(timeOfDay_ * 3600.0f);
        const int hh = (totalSeconds / 3600) % 24;
        const int mm = (totalSeconds / 60) % 60;
        const int ss = totalSeconds % 60;
        std::swprintf(timeText, std::size(timeText), L"%02d:%02d:%02d", hh, mm, ss);
    } else {
        std::wcscpy(timeText, L"UNAVAILABLE");
    }

    drawRow(dc, y, 24, client.right - 24, L"Current Time",
            timeText, todAvailable_ ? RGB(112, 190, 255) : RGB(145,145,155));
    drawRow(dc, y, 24, client.right - 24, L"Paused",
            todAvailable_ ? (timePaused_ ? L"ON" : L"OFF") : L"UNAVAILABLE",
            statusColor(todAvailable_, timePaused_));

    y += 10;
    drawSection(dc, y, 24, client.right - 24, L"DIAGNOSTICS");

    wchar_t baseText[64]{};
    std::swprintf(baseText, std::size(baseText), L"0x%llX",
                  static_cast<unsigned long long>(moduleBase_));
    drawRow(dc, y, 24, client.right - 24, L"Module Base", baseText, RGB(190,190,205));

    drawRow(dc, y, 24, client.right - 24, L"Weather probes",
            L"PENDING REVERSE", RGB(245, 190, 90));

    RECT foot{24, client.bottom - 58, client.right - 24, client.bottom - 16};
    SelectObject(dc, font_);
    SetTextColor(dc, RGB(125,125,140));
    DrawTextW(dc, L"Only validated data is shown as live. Unresolved fields are never guessed.",
              -1, &foot, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void EnvironmentInspector::drawSection(HDC dc, int& y, int left, int right, const wchar_t* title) noexcept {
    RECT r{left, y, right, y + 28};
    HBRUSH brush = CreateSolidBrush(RGB(31, 31, 38));
    FillRect(dc, &r, brush);
    DeleteObject(brush);

    SelectObject(dc, fontBold_);
    SetTextColor(dc, RGB(210, 210, 220));
    RECT t{left + 10, y, right - 10, y + 28};
    DrawTextW(dc, title, -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 30;
}

void EnvironmentInspector::drawRow(HDC dc, int& y, int left, int right,
                                   const wchar_t* label, const wchar_t* value,
                                   COLORREF valueColor) noexcept {
    RECT r{left, y, right, y + 25};
    HBRUSH brush = CreateSolidBrush((y / 25) % 2 ? RGB(25,25,30) : RGB(27,27,33));
    FillRect(dc, &r, brush);
    DeleteObject(brush);

    SelectObject(dc, font_);
    SetTextColor(dc, RGB(180,180,192));
    RECT l{left + 10, y, left + 310, y + 25};
    DrawTextW(dc, label, -1, &l, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(dc, valueColor);
    RECT v{left + 320, y, right - 10, y + 25};
    DrawTextW(dc, value, -1, &v, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    y += 25;
}

void EnvironmentInspector::appendLog(const char* text) const noexcept {
    if (!text) return;
    std::ofstream log(logPath(), std::ios::out | std::ios::app);
    if (!log) return;
    log << text << "\n";
    log.flush();
}

}
