#include "EnvironmentInspector.h"
#include "EnvironmentSystem.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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

bool floatChanged(float a, float b) noexcept {
    if (!std::isfinite(a) || !std::isfinite(b)) return a != b;
    return std::fabs(a - b) > 0.0001f;
}

void statusText(const char* label, bool value, bool available = true) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine(250.0f);
    if (!available) {
        ImGui::TextDisabled("N/A");
        return;
    }
    const ImVec4 color = value ? ImVec4(0.35f, 0.90f, 0.52f, 1.0f)
                               : ImVec4(0.95f, 0.38f, 0.38f, 1.0f);
    ImGui::TextColored(color, "%s", value ? "ON" : "OFF");
}

void floatRow(const char* label, float value, bool available) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine(250.0f);
    if (available && std::isfinite(value)) {
        ImGui::Text("%.4f", value);
    } else {
        ImGui::TextDisabled("N/A");
    }
}

void pointerRow(const char* label, std::uintptr_t value) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine(250.0f);
    if (value) ImGui::Text("0x%llX", static_cast<unsigned long long>(value));
    else ImGui::TextDisabled("N/A");
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
        WaitForSingleObject(thread_, 3000);
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

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 5.0f;
    style.ChildRounding = 6.0f;
    style.PopupRounding = 5.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding = 5.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);

    ImGui_ImplWin32_Init(self->hwnd_);
    ImGui_ImplDX11_Init(self->device_, self->deviceContext_);

    self->appendLog("Environment Monitor ImGui started");
    self->sample();

    ULONGLONG lastSample = GetTickCount64();

    while (self->running_.load()) {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                self->running_ = false;
                break;
            }
        }
        if (!self->running_.load()) break;

        const ULONGLONG now = GetTickCount64();
        if (now - lastSample >= 250) {
            self->sample();
            lastSample = now;
        }

        if (!self->visible_.load()) {
            Sleep(50);
            continue;
        }

        self->renderFrame();
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    self->destroyWindow();
    return 0;
}

bool EnvironmentInspector::createWindow() noexcept {
    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = &EnvironmentInspector::wndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"OutlawsEnvironmentInspectorImGui";

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    hwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"Outlaws Tools - Environment Inspector",
        WS_OVERLAPPEDWINDOW,
        80, 80, 760, 820,
        nullptr, nullptr, instance, this);

    if (!hwnd_) return false;
    if (!createDeviceD3D(hwnd_)) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    UpdateWindow(hwnd_);
    return true;
}

void EnvironmentInspector::destroyWindow() noexcept {
    cleanupDeviceD3D();
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    UnregisterClassW(L"OutlawsEnvironmentInspectorImGui", GetModuleHandleW(nullptr));
}

bool EnvironmentInspector::createDeviceD3D(HWND hwnd) noexcept {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL featureLevel{};

    const HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        featureLevels,
        static_cast<UINT>(std::size(featureLevels)),
        D3D11_SDK_VERSION,
        &sd,
        &swapChain_,
        &device_,
        &featureLevel,
        &deviceContext_);

    if (FAILED(hr)) return false;
    createRenderTarget();
    return renderTargetView_ != nullptr;
}

void EnvironmentInspector::cleanupDeviceD3D() noexcept {
    cleanupRenderTarget();
    if (swapChain_) { swapChain_->Release(); swapChain_ = nullptr; }
    if (deviceContext_) { deviceContext_->Release(); deviceContext_ = nullptr; }
    if (device_) { device_->Release(); device_ = nullptr; }
}

void EnvironmentInspector::createRenderTarget() noexcept {
    if (!swapChain_ || !device_) return;
    ID3D11Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) && backBuffer) {
        device_->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView_);
        backBuffer->Release();
    }
}

void EnvironmentInspector::cleanupRenderTarget() noexcept {
    if (renderTargetView_) {
        renderTargetView_->Release();
        renderTargetView_ = nullptr;
    }
}

LRESULT CALLBACK EnvironmentInspector::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<EnvironmentInspector*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<EnvironmentInspector*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (self && self->device_ && wParam != SIZE_MINIMIZED) {
            self->cleanupRenderTarget();
            if (self->swapChain_) {
                self->swapChain_->ResizeBuffers(
                    0,
                    static_cast<UINT>(LOWORD(lParam)),
                    static_cast<UINT>(HIWORD(lParam)),
                    DXGI_FORMAT_UNKNOWN,
                    0);
                self->createRenderTarget();
            }
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU) return 0;
        break;

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

void EnvironmentInspector::renderFrame() noexcept {
    if (!deviceContext_ || !swapChain_ || !renderTargetView_) return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    drawUi();

    ImGui::Render();
    const float clearColor[4] = {0.055f, 0.060f, 0.075f, 1.0f};
    deviceContext_->OMSetRenderTargets(1, &renderTargetView_, nullptr);
    deviceContext_->ClearRenderTargetView(renderTargetView_, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    swapChain_->Present(1, 0);
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
    const bool readOk = environment_->readWeatherScene(weather);
    weatherAvailable_ = readOk && weather.available;

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

        gameplayRain_ = weather.gameplayRain;
        graphicsRain_ = weather.graphicsRain;
        temperature_ = weather.temperature;
        viewDistance_ = weather.viewDistance;
        outdoorFog_ = weather.outdoorFog;
        cloudCoverage_ = weather.cloudCoverage;
        windDirection_ = weather.windDirection;
        windStrength_ = weather.windStrength;

        hasSnow_ = weather.hasSnow;
        hasFog_ = weather.hasFog;
    } else {
        weatherManager_ = 0;
        weatherPreset_ = 0;
        activePresetIndex_ = -1;
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
            floatChanged(lastGameplayRain_, gameplayRain_) ||
            floatChanged(lastGraphicsRain_, graphicsRain_) ||
            floatChanged(lastTemperature_, temperature_) ||
            floatChanged(lastViewDistance_, viewDistance_) ||
            floatChanged(lastOutdoorFog_, outdoorFog_) ||
            floatChanged(lastCloudCoverage_, cloudCoverage_) ||
            floatChanged(lastWindDirection_, windDirection_) ||
            floatChanged(lastWindStrength_, windStrength_) ||
            lastHasSnow_ != hasSnow_ ||
            lastHasFog_ != hasFog_;

        if (weatherChanged || (todAvailable_ && lastPaused_ != timePaused_))
            logSceneChange();
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

    lastGameplayRain_ = gameplayRain_;
    lastGraphicsRain_ = graphicsRain_;
    lastTemperature_ = temperature_;
    lastViewDistance_ = viewDistance_;
    lastOutdoorFog_ = outdoorFog_;
    lastCloudCoverage_ = cloudCoverage_;
    lastWindDirection_ = windDirection_;
    lastWindStrength_ = windStrength_;

    lastHasSnow_ = hasSnow_;
    lastHasFog_ = hasFog_;
    lastPaused_ = timePaused_;
    firstSample_ = false;
}

void EnvironmentInspector::logSceneChange() noexcept {
    char line[3072]{};
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
        "GameplayRain: %.4f -> %.4f\n"
        "GraphicsRain: %.4f -> %.4f\n"
        "Temperature: %.4f -> %.4f\n"
        "ViewDistance: %.4f -> %.4f\n"
        "OutdoorFog: %.4f -> %.4f\n"
        "CloudCoverage: %.4f -> %.4f\n"
        "WindDirection: %.4f -> %.4f\n"
        "WindStrength: %.4f -> %.4f\n"
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
        lastGameplayRain_, gameplayRain_,
        lastGraphicsRain_, graphicsRain_,
        lastTemperature_, temperature_,
        lastViewDistance_, viewDistance_,
        lastOutdoorFog_, outdoorFog_,
        lastCloudCoverage_, cloudCoverage_,
        lastWindDirection_, windDirection_,
        lastWindStrength_, windStrength_,
        lastHasSnow_ ? "ON" : "OFF", hasSnow_ ? "ON" : "OFF",
        lastHasFog_ ? "ON" : "OFF", hasFog_ ? "ON" : "OFF",
        lastPaused_ ? "ON" : "OFF", timePaused_ ? "ON" : "OFF");

    appendLog(line);
}

void EnvironmentInspector::drawUi() noexcept {
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("Environment Inspector", nullptr, flags);

    ImGui::TextColored(ImVec4(0.50f, 0.78f, 1.0f, 1.0f), "Outlaws Tools");
    ImGui::SameLine();
    ImGui::TextDisabled(" / Scene-aware Environment Monitor");
    ImGui::Separator();

    if (ImGui::BeginTabBar("EnvironmentTabs")) {
        if (ImGui::BeginTabItem("Environment")) {
            ImGui::Spacing();
            ImGui::TextUnformatted("Active Scene / WeatherPreset");
            ImGui::Separator();
            pointerRow("Weather Manager", weatherManager_);
            pointerRow("Weather Preset", weatherPreset_);
            ImGui::Text("Preset / Transition Index");
            ImGui::SameLine(250.0f);
            if (weatherAvailable_) ImGui::Text("%d", activePresetIndex_);
            else ImGui::TextDisabled("N/A");

            ImGui::Spacing();
            ImGui::TextUnformatted("Preset contributions");
            ImGui::Separator();
            statusText("Gameplay Rain field", gameplayRainField_, weatherAvailable_);
            statusText("Graphics Rain field", graphicsRainField_, weatherAvailable_);
            statusText("Temperature field", temperatureField_, weatherAvailable_);
            statusText("View Distance field", viewDistanceField_, weatherAvailable_);
            statusText("Outdoor Fog field", outdoorFogField_, weatherAvailable_);
            statusText("Cloud Coverage field", cloudCoverageField_, weatherAvailable_);
            statusText("Wind Direction field", windDirectionField_, weatherAvailable_);
            statusText("Wind Strength field", windStrengthField_, weatherAvailable_);

            ImGui::Spacing();
            ImGui::TextUnformatted("Active preset values");
            ImGui::Separator();
            floatRow("Gameplay Rain", gameplayRain_, weatherAvailable_ && gameplayRainField_);
            floatRow("Graphics Rain", graphicsRain_, weatherAvailable_ && graphicsRainField_);
            floatRow("Temperature", temperature_, weatherAvailable_ && temperatureField_);
            floatRow("View Distance", viewDistance_, weatherAvailable_ && viewDistanceField_);
            floatRow("Outdoor Fog", outdoorFog_, weatherAvailable_ && outdoorFogField_);
            floatRow("Cloud Coverage", cloudCoverage_, weatherAvailable_ && cloudCoverageField_);
            floatRow("Wind Direction", windDirection_, weatherAvailable_ && windDirectionField_);
            floatRow("Wind Strength", windStrength_, weatherAvailable_ && windStrengthField_);

            ImGui::Spacing();
            ImGui::TextUnformatted("Weather flags");
            ImGui::Separator();
            statusText("Snow", hasSnow_, weatherAvailable_);
            statusText("Fog", hasFog_, weatherAvailable_);

            ImGui::TextUnformatted("Indoor / Outdoor");
            ImGui::SameLine(250.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.30f, 1.0f), "PENDING NATIVE PROBE");

            ImGui::TextUnformatted("Weather Mask Wetness");
            ImGui::SameLine(250.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.30f, 1.0f), "PENDING NATIVE PROBE");

            ImGui::TextUnformatted("Sandstorm Tag");
            ImGui::SameLine(250.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.30f, 1.0f), "PENDING NATIVE PROBE");

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Time of Day")) {
            ImGui::Spacing();
            if (todAvailable_) {
                int totalSeconds = static_cast<int>(timeOfDay_ * 3600.0f);
                totalSeconds = std::max(0, totalSeconds);
                const int hh = (totalSeconds / 3600) % 24;
                const int mm = (totalSeconds / 60) % 60;
                const int ss = totalSeconds % 60;
                ImGui::Text("Current Time: %02d:%02d:%02d", hh, mm, ss);
                statusText("Paused", timePaused_, true);
            } else {
                ImGui::TextDisabled("TimeOfDaySystem unavailable.");
            }

            ImGui::Spacing();
            if (ImGui::Button("06:00", ImVec2(110, 34)) && environment_)
                environment_->setTimeOfDay(6.0f);
            ImGui::SameLine();
            if (ImGui::Button("12:00", ImVec2(110, 34)) && environment_)
                environment_->setTimeOfDay(12.0f);
            ImGui::SameLine();
            if (ImGui::Button("18:00", ImVec2(110, 34)) && environment_)
                environment_->setTimeOfDay(18.0f);

            if (ImGui::Button("Pause TOD", ImVec2(170, 34)) && environment_)
                environment_->setTimePaused(true);
            ImGui::SameLine();
            if (ImGui::Button("Resume TOD", ImVec2(170, 34)) && environment_)
                environment_->setTimePaused(false);

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Diagnostics")) {
            ImGui::Spacing();
            pointerRow("Module Base", moduleBase_);
            pointerRow("Weather Manager", weatherManager_);
            pointerRow("Weather Preset", weatherPreset_);
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.52f, 1.0f),
                               "Scene-change logger: ACTIVE");
            ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.52f, 1.0f),
                               "Unsafe descriptor writes: DISABLED");
            ImGui::TextWrapped(
                "Sampling runs every 250 ms. The log only emits an ENVIRONMENT CHANGE "
                "block when the observed manager, preset, contribution flags, numeric "
                "preset values, weather flags, or TOD pause state changes.");
            ImGui::Spacing();
            ImGui::TextDisabled(
                "F10 toggles this inspector. Pending probes are deliberately not guessed.");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void EnvironmentInspector::appendLog(const char* text) const noexcept {
    if (!text) return;
    std::ofstream log(logPath(), std::ios::out | std::ios::app);
    if (!log) return;
    log << text << "\n";
    log.flush();
}

}
