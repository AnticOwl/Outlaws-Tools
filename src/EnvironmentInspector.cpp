#include "EnvironmentInspector.h"
#include "EnvironmentSystem.h"

#include <cstdio>
#include <cwchar>
#include <iterator>
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

const wchar_t* onOff(bool value) {
    return value ? L"ON" : L"OFF";
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
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = CreateSolidBrush(RGB(19, 20, 24));
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"Outlaws Tools - Environment Monitor",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        80, 80, 720, 830,
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
        {kBtnPause, 374, 120, L"Pause TOD"},
        {kBtnResume, 502, 140, L"Resume TOD"},
    };

    for (const auto& b : buttons) {
        HWND h = CreateWindowW(L"BUTTON", b.text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               b.x, 706, b.w, 32, hwnd_,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(b.id)),
                               instance, nullptr);
        if (h && font_) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }

    SetTimer(hwnd_, kTimerId, 250, nullptr);
    sample();
    appendLog("Environment Monitor started");

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
            if (!self->running_.load()) {
                DestroyWindow(hwnd);
            } else {
                self->visible_ = false;
                ShowWindow(hwnd, SW_HIDE);
            }
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
    todAvailable_ = environment_->read(state);
    if (todAvailable_) {
        timeOfDay_ = state.timeOfDay;
        timePaused_ = state.timePaused;
    }

    WeatherSceneState weather{};
    weatherAvailable_ = environment_->readWeatherScene(weather);
    if (weatherAvailable_) {
        weatherManager_ = weather.manager;
        weatherPreset_ = weather.preset;
        activePresetIndex_ = weather.activePresetIndex;

        gameplayRainField_ = weather.gameplayRainField;
        graphicsRainField_ = weather.graphicsRainField;
        temperatureField_ = weather.temperatureField;
        viewDistanceField_ = weather.viewDistanceField;
        outdoorFogField_ = weather.outdoorFogField;
        cloudCoverageField_ = weather.cloudCoverageField;
        windDirectionField_ = weather.windDirectionField;
        windStrengthField_ = weather.windStrengthField;
        hasSnow_ = weather.hasSnow;
        hasFog_ = weather.hasFog;
    }

    if (!firstSample_) {
        const bool weatherChanged =
            lastWeatherManager_ != weatherManager_ ||
            lastWeatherPreset_ != weatherPreset_ ||
            lastActivePresetIndex_ != activePresetIndex_ ||
            lastGameplayRainField_ != gameplayRainField_ ||
            lastGraphicsRainField_ != graphicsRainField_ ||
            lastTemperatureField_ != temperatureField_ ||
            lastViewDistanceField_ != viewDistanceField_ ||
            lastOutdoorFogField_ != outdoorFogField_ ||
            lastCloudCoverageField_ != cloudCoverageField_ ||
            lastWindDirectionField_ != windDirectionField_ ||
            lastWindStrengthField_ != windStrengthField_ ||
            lastHasSnow_ != hasSnow_ ||
            lastHasFog_ != hasFog_;

        if (weatherChanged || (todAvailable_ && lastPaused_ != timePaused_)) {
            logSceneChange();
        }
    }

    lastWeatherManager_ = weatherManager_;
    lastWeatherPreset_ = weatherPreset_;
    lastActivePresetIndex_ = activePresetIndex_;
    lastGameplayRainField_ = gameplayRainField_;
    lastGraphicsRainField_ = graphicsRainField_;
    lastTemperatureField_ = temperatureField_;
    lastViewDistanceField_ = viewDistanceField_;
    lastOutdoorFogField_ = outdoorFogField_;
    lastCloudCoverageField_ = cloudCoverageField_;
    lastWindDirectionField_ = windDirectionField_;
    lastWindStrengthField_ = windStrengthField_;
    lastHasSnow_ = hasSnow_;
    lastHasFog_ = hasFog_;
    lastPaused_ = timePaused_;
    firstSample_ = false;
}

void EnvironmentInspector::logSceneChange() noexcept {
    char line[2048]{};
    std::snprintf(line, sizeof(line),
        "=== ENVIRONMENT CHANGE ===\n"
        "WeatherManager: 0x%llX -> 0x%llX\n"
        "Preset: 0x%llX -> 0x%llX\n"
        "Preset Index: %d -> %d\n"
        "GameplayRain Field: %s -> %s\n"
        "GraphicsRain Field: %s -> %s\n"
        "Temperature Field: %s -> %s\n"
        "ViewDistance Field: %s -> %s\n"
        "OutdoorFog Field: %s -> %s\n"
        "CloudCoverage Field: %s -> %s\n"
        "WindDirection Field: %s -> %s\n"
        "WindStrength Field: %s -> %s\n"
        "Snow: %s -> %s\n"
        "Fog: %s -> %s\n"
        "TOD Paused: %s -> %s",
        static_cast<unsigned long long>(lastWeatherManager_),
        static_cast<unsigned long long>(weatherManager_),
        static_cast<unsigned long long>(lastWeatherPreset_),
        static_cast<unsigned long long>(weatherPreset_),
        lastActivePresetIndex_, activePresetIndex_,
        lastGameplayRainField_ ? "ON" : "OFF", gameplayRainField_ ? "ON" : "OFF",
        lastGraphicsRainField_ ? "ON" : "OFF", graphicsRainField_ ? "ON" : "OFF",
        lastTemperatureField_ ? "ON" : "OFF", temperatureField_ ? "ON" : "OFF",
        lastViewDistanceField_ ? "ON" : "OFF", viewDistanceField_ ? "ON" : "OFF",
        lastOutdoorFogField_ ? "ON" : "OFF", outdoorFogField_ ? "ON" : "OFF",
        lastCloudCoverageField_ ? "ON" : "OFF", cloudCoverageField_ ? "ON" : "OFF",
        lastWindDirectionField_ ? "ON" : "OFF", windDirectionField_ ? "ON" : "OFF",
        lastWindStrengthField_ ? "ON" : "OFF", windStrengthField_ ? "ON" : "OFF",
        lastHasSnow_ ? "ON" : "OFF", hasSnow_ ? "ON" : "OFF",
        lastHasFog_ ? "ON" : "OFF", hasFog_ ? "ON" : "OFF",
        lastPaused_ ? "ON" : "OFF", timePaused_ ? "ON" : "OFF");
    appendLog(line);
}

void EnvironmentInspector::paint(HDC dc, const RECT& client) noexcept {
    HBRUSH bg = CreateSolidBrush(RGB(19, 20, 24));
    FillRect(dc, &client, bg);
    DeleteObject(bg);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(238, 238, 244));

    RECT title{24, 18, client.right - 24, 52};
    SelectObject(dc, fontBold_);
    DrawTextW(dc, L"Environment Monitor", -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT sub{24, 48, client.right - 24, 76};
    SelectObject(dc, font_);
    SetTextColor(dc, RGB(145, 148, 162));
    DrawTextW(dc, L"Active-scene WeatherPreset observer — F10 toggles this window",
              -1, &sub, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int y = 88;

    drawSection(dc, y, 24, client.right - 24, L"ACTIVE SCENE / PRESET");

    wchar_t managerText[64]{};
    wchar_t presetText[64]{};
    wchar_t indexText[64]{};
    if (weatherAvailable_) {
        std::swprintf(managerText, std::size(managerText), L"0x%llX",
                      static_cast<unsigned long long>(weatherManager_));
        if (weatherPreset_) {
            std::swprintf(presetText, std::size(presetText), L"0x%llX",
                          static_cast<unsigned long long>(weatherPreset_));
        } else {
            wcscpy_s(presetText, L"NONE / TRANSITION");
        }
        std::swprintf(indexText, std::size(indexText), L"%d", activePresetIndex_);
    } else {
        wcscpy_s(managerText, L"UNAVAILABLE");
        wcscpy_s(presetText, L"UNAVAILABLE");
        wcscpy_s(indexText, L"UNAVAILABLE");
    }

    drawRow(dc, y, 24, client.right - 24, L"Weather Manager", managerText,
            weatherAvailable_ ? RGB(112,190,255) : RGB(145,145,155));
    drawRow(dc, y, 24, client.right - 24, L"Active Weather Preset", presetText,
            weatherPreset_ ? RGB(112,190,255) : RGB(245,190,90));
    drawRow(dc, y, 24, client.right - 24, L"Preset / Transition Index", indexText,
            weatherAvailable_ ? RGB(190,190,205) : RGB(145,145,155));

    y += 10;
    drawSection(dc, y, 24, client.right - 24, L"WEATHER PRESET CONTRIBUTIONS");
    drawRow(dc, y, 24, client.right - 24, L"Gameplay Rain", onOff(gameplayRainField_),
            statusColor(weatherAvailable_, gameplayRainField_));
    drawRow(dc, y, 24, client.right - 24, L"Graphics Rain", onOff(graphicsRainField_),
            statusColor(weatherAvailable_, graphicsRainField_));
    drawRow(dc, y, 24, client.right - 24, L"Outdoor Fog", onOff(outdoorFogField_),
            statusColor(weatherAvailable_, outdoorFogField_));
    drawRow(dc, y, 24, client.right - 24, L"Cloud Coverage", onOff(cloudCoverageField_),
            statusColor(weatherAvailable_, cloudCoverageField_));
    drawRow(dc, y, 24, client.right - 24, L"Wind Direction", onOff(windDirectionField_),
            statusColor(weatherAvailable_, windDirectionField_));
    drawRow(dc, y, 24, client.right - 24, L"Wind Strength", onOff(windStrengthField_),
            statusColor(weatherAvailable_, windStrengthField_));
    drawRow(dc, y, 24, client.right - 24, L"Temperature", onOff(temperatureField_),
            statusColor(weatherAvailable_, temperatureField_));
    drawRow(dc, y, 24, client.right - 24, L"View Distance", onOff(viewDistanceField_),
            statusColor(weatherAvailable_, viewDistanceField_));

    y += 10;
    drawSection(dc, y, 24, client.right - 24, L"WEATHER FLAGS");
    drawRow(dc, y, 24, client.right - 24, L"Snow", onOff(hasSnow_),
            statusColor(weatherAvailable_, hasSnow_));
    drawRow(dc, y, 24, client.right - 24, L"Fog", onOff(hasFog_),
            statusColor(weatherAvailable_, hasFog_));
    drawRow(dc, y, 24, client.right - 24, L"Indoor / Outdoor", L"PENDING NATIVE PROBE",
            RGB(245,190,90));
    drawRow(dc, y, 24, client.right - 24, L"Weather Mask Wetness", L"PENDING NATIVE PROBE",
            RGB(245,190,90));
    drawRow(dc, y, 24, client.right - 24, L"Sandstorm Tag", L"PENDING NATIVE PROBE",
            RGB(245,190,90));

    y += 10;
    drawSection(dc, y, 24, client.right - 24, L"TIME OF DAY");

    wchar_t timeText[64]{};
    if (todAvailable_) {
        int totalSeconds = static_cast<int>(timeOfDay_ * 3600.0f);
        if (totalSeconds < 0) totalSeconds = 0;
        const int hh = (totalSeconds / 3600) % 24;
        const int mm = (totalSeconds / 60) % 60;
        const int ss = totalSeconds % 60;
        std::swprintf(timeText, std::size(timeText), L"%02d:%02d:%02d", hh, mm, ss);
    } else {
        wcscpy_s(timeText, L"UNAVAILABLE");
    }

    drawRow(dc, y, 24, client.right - 24, L"Current Time", timeText,
            todAvailable_ ? RGB(112,190,255) : RGB(145,145,155));
    drawRow(dc, y, 24, client.right - 24, L"Paused",
            todAvailable_ ? onOff(timePaused_) : L"UNAVAILABLE",
            statusColor(todAvailable_, timePaused_));

    y += 10;
    drawSection(dc, y, 24, client.right - 24, L"STATUS");
    drawRow(dc, y, 24, client.right - 24, L"Scene-change logger", L"ACTIVE",
            RGB(88,210,130));
    drawRow(dc, y, 24, client.right - 24, L"Unsafe descriptor writes", L"DISABLED",
            RGB(88,210,130));

    RECT foot{24, client.bottom - 58, client.right - 24, client.bottom - 16};
    SelectObject(dc, font_);
    SetTextColor(dc, RGB(120,123,136));
    DrawTextW(dc,
              L"Only verified scene data is displayed as live. Pending probes are explicitly marked.",
              -1, &foot, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void EnvironmentInspector::drawSection(HDC dc, int& y, int left, int right, const wchar_t* title) noexcept {
    RECT r{left, y, right, y + 28};
    HBRUSH brush = CreateSolidBrush(RGB(29, 31, 38));
    FillRect(dc, &r, brush);
    DeleteObject(brush);

    SelectObject(dc, fontBold_);
    SetTextColor(dc, RGB(214, 216, 226));
    RECT t{left + 10, y, right - 10, y + 28};
    DrawTextW(dc, title, -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 30;
}

void EnvironmentInspector::drawRow(HDC dc, int& y, int left, int right,
                                   const wchar_t* label, const wchar_t* value,
                                   COLORREF valueColor) noexcept {
    RECT r{left, y, right, y + 25};
    HBRUSH brush = CreateSolidBrush((y / 25) % 2 ? RGB(23,24,29) : RGB(25,26,31));
    FillRect(dc, &r, brush);
    DeleteObject(brush);

    SelectObject(dc, font_);
    SetTextColor(dc, RGB(177,180,192));
    RECT l{left + 10, y, left + 330, y + 25};
    DrawTextW(dc, label, -1, &l, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(dc, valueColor);
    RECT v{left + 340, y, right - 10, y + 25};
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
