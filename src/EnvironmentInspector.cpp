#include "EnvironmentInspector.h"
#include "EnvironmentSystem.h"

#include <Windows.h>
#include <imgui.h>

#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>

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

void stateRow(const char* label, bool value, bool available) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine(260.0f);
    if (!available) {
        ImGui::TextDisabled("N/A");
        return;
    }

    const ImVec4 c = value ? ImVec4(0.30f, 0.90f, 0.48f, 1.0f)
                           : ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
    ImGui::TextColored(c, "%s", value ? "ON" : "OFF");
}

void ptrRow(const char* label, std::uintptr_t value) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine(260.0f);
    if (value)
        ImGui::Text("0x%llX", static_cast<unsigned long long>(value));
    else
        ImGui::TextDisabled("N/A");
}

template <std::size_t N>
bool byteBlockChanged(const std::array<std::uint8_t, N>& a,
                      const std::array<std::uint8_t, N>& b) {
    return std::memcmp(a.data(), b.data(), a.size()) != 0;
}

template <std::size_t N>
void dumpRawDwords(std::ofstream& log,
                   const char* label,
                   const std::array<std::uint8_t, N>& bytes) {
    log << "-- " << label << " RAW " << std::dec << N << " bytes --\n";
    for (std::size_t off = 0; off + 4 <= N; off += 4) {
        std::uint32_t raw{};
        std::memcpy(&raw, bytes.data() + off, sizeof(raw));
        const float f = std::bit_cast<float>(raw);

        log << "+0x" << std::hex << std::uppercase
            << std::setw(3) << std::setfill('0') << off
            << " raw=0x" << std::setw(8) << raw
            << std::setfill(' ') << std::dec;

        if (std::isfinite(f) && std::fabs(f) < 1000000.0f)
            log << " float=" << f;
        log << "\n";
    }
}

template <std::size_t N>
void dumpRawDiff(std::ofstream& log,
                 const char* label,
                 const std::array<std::uint8_t, N>& before,
                 const std::array<std::uint8_t, N>& after) {
    bool header = false;
    for (std::size_t off = 0; off + 4 <= N; off += 4) {
        std::uint32_t a{}, b{};
        std::memcpy(&a, before.data() + off, sizeof(a));
        std::memcpy(&b, after.data() + off, sizeof(b));
        if (a == b) continue;

        if (!header) {
            log << "-- " << label << " DWORD DIFF --\n";
            header = true;
        }

        const float fa = std::bit_cast<float>(a);
        const float fb = std::bit_cast<float>(b);
        log << "+0x" << std::hex << std::uppercase
            << std::setw(3) << std::setfill('0') << off
            << ": 0x" << std::setw(8) << a
            << " -> 0x" << std::setw(8) << b
            << std::setfill(' ') << std::dec;
        if (std::isfinite(fa) && std::isfinite(fb) &&
            std::fabs(fa) < 1000000.0f && std::fabs(fb) < 1000000.0f)
            log << " float " << fa << " -> " << fb;
        log << "\n";
    }
}

}

bool EnvironmentInspector::start(EnvironmentSystem* environment, std::uintptr_t moduleBase) noexcept {
    environment_ = environment;
    moduleBase_ = moduleBase;
    running_ = environment_ != nullptr && moduleBase_ != 0;
    firstSample_ = true;
    lastSampleTick_ = 0;
    appendLog(running_ ? "Environment Inspector panel initialized"
                       : "Environment Inspector panel initialization failed");
    return running_;
}

void EnvironmentInspector::stop() noexcept {
    running_ = false;
    environment_ = nullptr;
    moduleBase_ = 0;
}

void EnvironmentInspector::render() noexcept {
    if (!running_ || !environment_) return;

    const auto now = GetTickCount64();
    if (lastSampleTick_ == 0 || now - lastSampleTick_ >= 250) {
        sample();
        lastSampleTick_ = now;
    }

    drawUi();
}

void EnvironmentInspector::sample() noexcept {
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

        hasSnow_ = weather.hasSnow;
        hasFog_ = weather.hasFog;

        precipitationAvailable_ = weather.precipitationAvailable;
        precipitationContext_ = weather.precipitationContext;
        precipitationObject_ = weather.precipitationObject;
        precipitationStateRaw_ = weather.precipitationStateRaw;
        precipitationIntensity_ = weather.precipitationIntensity;
        precipitationCandidates_ = weather.precipitationCandidates;
        precipitationCandidateCount_ = weather.precipitationCandidateCount;

        managerBytes_ = weather.managerRaw;
        managerBytesValid_ = weatherManager_ != 0;
        presetBytes_ = weather.presetRaw;
        presetBytesValid_ = weatherPreset_ != 0;
    } else {
        weatherManager_ = 0;
        weatherPreset_ = 0;
        activePresetIndex_ = -1;
        gameplayRainField_ = false;
        graphicsRainField_ = false;
        temperatureField_ = false;
        viewDistanceField_ = false;
        outdoorFogField_ = false;
        cloudCoverageField_ = false;
        windDirectionField_ = false;
        windStrengthField_ = false;
        hasSnow_ = false;
        hasFog_ = false;
        precipitationAvailable_ = false;
        precipitationContext_ = 0;
        precipitationObject_ = 0;
        precipitationStateRaw_ = 0.0f;
        precipitationIntensity_ = 0.0f;
        precipitationCandidateCount_ = 0;
        managerBytesValid_ = false;
        presetBytesValid_ = false;
    }

    if (firstSample_) {
        logFullWeatherSnapshot("INITIAL");
        if (presetBytesValid_)
            logPresetSnapshot("INITIAL");
    }

    if (!firstSample_) {
        bool changed =
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
            lastHasFog_ != hasFog_ ||
            lastPaused_ != timePaused_ ||
            lastPrecipitationAvailable_ != precipitationAvailable_ ||
            lastPrecipitationContext_ != precipitationContext_ ||
            lastPrecipitationObject_ != precipitationObject_ ||
            (precipitationAvailable_ && std::fabs(lastPrecipitationStateRaw_ - precipitationStateRaw_) > 0.0001f) ||
            (precipitationAvailable_ && std::fabs(lastPrecipitationIntensity_ - precipitationIntensity_) > 0.0001f) ||
            precipitationCandidateCount_ != lastPrecipitationCandidateCount_ ||
            (managerBytesValid_ && lastManagerBytesValid_ &&
             byteBlockChanged(lastManagerBytes_, managerBytes_)) ||
            (presetBytesValid_ && lastPresetBytesValid_ &&
             byteBlockChanged(lastPresetBytes_, presetBytes_));

        if (!changed && precipitationCandidateCount_ == lastPrecipitationCandidateCount_) {
            for (std::uint32_t i = 0; i < precipitationCandidateCount_; ++i) {
                const auto& a = precipitationCandidates_[i];
                const auto& b = lastPrecipitationCandidates_[i];
                if (a.ownerOffset != b.ownerOffset || a.object != b.object ||
                    std::fabs(a.stateRaw - b.stateRaw) > 0.0001f ||
                    std::fabs(a.intensity - b.intensity) > 0.0001f) {
                    changed = true;
                    break;
                }
            }
        }

        if (changed)
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
    lastHasSnow_ = hasSnow_;
    lastHasFog_ = hasFog_;
    lastPaused_ = timePaused_;

    lastPrecipitationAvailable_ = precipitationAvailable_;
    lastPrecipitationContext_ = precipitationContext_;
    lastPrecipitationObject_ = precipitationObject_;
    lastPrecipitationStateRaw_ = precipitationStateRaw_;
    lastPrecipitationIntensity_ = precipitationIntensity_;
    lastPrecipitationCandidates_ = precipitationCandidates_;
    lastPrecipitationCandidateCount_ = precipitationCandidateCount_;

    lastManagerBytes_ = managerBytes_;
    lastManagerBytesValid_ = managerBytesValid_;
    lastPresetBytes_ = presetBytes_;
    lastPresetBytesValid_ = presetBytesValid_;
    firstSample_ = false;
}

void EnvironmentInspector::logSceneChange() noexcept {
    std::ofstream log(logPath(), std::ios::out | std::ios::app);
    if (!log) return;

    log << "=== ENVIRONMENT CHANGE ===\n";
    log << std::hex << std::uppercase;
    log << "WeatherManager: 0x" << lastWeatherManager_
        << " -> 0x" << weatherManager_ << "\n";
    log << "Preset: 0x" << lastWeatherPreset_
        << " -> 0x" << weatherPreset_ << "\n";
    log << std::dec;
    log << "Preset Index: " << lastActivePresetIndex_
        << " -> " << activePresetIndex_ << "\n";

    auto boolLine = [&](const char* name, bool before, bool after) {
        if (before != after)
            log << name << ": " << (before ? "ON" : "OFF")
                << " -> " << (after ? "ON" : "OFF") << "\n";
    };

    boolLine("GameplayRain Field", lastGameplayRainField_, gameplayRainField_);
    boolLine("GraphicsRain Field", lastGraphicsRainField_, graphicsRainField_);
    boolLine("Temperature Field", lastTemperatureField_, temperatureField_);
    boolLine("ViewDistance Field", lastViewDistanceField_, viewDistanceField_);
    boolLine("OutdoorFog Field", lastOutdoorFogField_, outdoorFogField_);
    boolLine("CloudCoverage Field", lastCloudCoverageField_, cloudCoverageField_);
    boolLine("WindDirection Field", lastWindDirectionField_, windDirectionField_);
    boolLine("WindStrength Field", lastWindStrengthField_, windStrengthField_);
    boolLine("Preset HasSnow", lastHasSnow_, hasSnow_);
    boolLine("Preset HasFog", lastHasFog_, hasFog_);
    boolLine("TOD Paused", lastPaused_, timePaused_);
    boolLine("Native Precipitation Available", lastPrecipitationAvailable_, precipitationAvailable_);

    if (lastPrecipitationContext_ != precipitationContext_) {
        log << std::hex << std::uppercase
            << "Precipitation Context: 0x" << lastPrecipitationContext_
            << " -> 0x" << precipitationContext_ << "\n"
            << std::dec;
    }

    if (lastPrecipitationObject_ != precipitationObject_) {
        log << std::hex << std::uppercase
            << "Precipitation Object: 0x" << lastPrecipitationObject_
            << " -> 0x" << precipitationObject_ << "\n"
            << std::dec;
    }

    if (precipitationAvailable_) {
        if (!lastPrecipitationAvailable_ ||
            std::fabs(lastPrecipitationStateRaw_ - precipitationStateRaw_) > 0.0001f) {
            log << "Precipitation State Raw: "
                << lastPrecipitationStateRaw_ << " -> "
                << precipitationStateRaw_ << "\n";
        }

        if (!lastPrecipitationAvailable_ ||
            std::fabs(lastPrecipitationIntensity_ - precipitationIntensity_) > 0.0001f) {
            log << "Precipitation Intensity: "
                << lastPrecipitationIntensity_ << " -> "
                << precipitationIntensity_ << "\n";
        }
    }

    if (presetBytesValid_ && lastPresetBytesValid_) {
        struct Block { const char* name; std::size_t begin; std::size_t end; };
        constexpr Block blocks[] = {
            {"GameplayRain",  0x038, 0x080},
            {"GraphicsRain",  0x088, 0x0D0},
            {"Temperature",   0x0D8, 0x120},
            {"ViewDistance",  0x128, 0x170},
            {"OutdoorFog",    0x178, 0x1C0},
            {"CloudCoverage", 0x1C8, 0x210},
            {"WindDirection", 0x218, 0x260},
            {"WindStrength",  0x268, 0x2B0},
        };

        for (const auto& block : blocks) {
            bool header = false;
            for (std::size_t off = block.begin; off + 4 <= block.end; off += 4) {
                std::uint32_t before{};
                std::uint32_t after{};
                std::memcpy(&before, lastPresetBytes_.data() + off, sizeof(before));
                std::memcpy(&after, presetBytes_.data() + off, sizeof(after));
                if (before == after) continue;

                if (!header) {
                    log << "-- " << block.name << " property changes --\n";
                    header = true;
                }

                const float fb = std::bit_cast<float>(before);
                const float fa = std::bit_cast<float>(after);

                log << "+0x" << std::hex << std::uppercase << off
                    << ": 0x" << std::setw(8) << std::setfill('0') << before
                    << " -> 0x" << std::setw(8) << after
                    << std::setfill(' ') << std::dec;

                if (std::isfinite(fb) && std::isfinite(fa) &&
                    std::fabs(fb) < 1000000.0f && std::fabs(fa) < 1000000.0f) {
                    log << "  float " << fb << " -> " << fa;
                }
                log << "\n";
            }
        }
    }

    if (managerBytesValid_ && lastManagerBytesValid_)
        dumpRawDiff(log, "WEATHER MANAGER", lastManagerBytes_, managerBytes_);
    if (presetBytesValid_ && lastPresetBytesValid_)
        dumpRawDiff(log, "WEATHER PRESET", lastPresetBytes_, presetBytes_);

    log << "-- PRECIPITATION CANDIDATES --\n";
    log << "count=" << precipitationCandidateCount_ << "\n";
    for (std::uint32_t i = 0; i < precipitationCandidateCount_; ++i) {
        const auto& c = precipitationCandidates_[i];
        log << "[" << i << "] owner=";
        if (c.ownerOffset == 0xFFFFFFFFu) log << "SELF";
        else log << "+0x" << std::hex << std::uppercase << c.ownerOffset << std::dec;
        log << " object=0x" << std::hex << std::uppercase << c.object << std::dec
            << " state=" << c.stateRaw
            << " intensity=" << c.intensity << "\n";
    }

    log << "=== END ENVIRONMENT CHANGE ===\n";
    log.flush();

    // A complete raw snapshot follows every detected change.  This is noisy by
    // design for the diagnostic build and lets us correlate opaque words with
    // actual clear/rain/fog/snow transitions.
    logFullWeatherSnapshot("CHANGE");
}


void EnvironmentInspector::logFullWeatherSnapshot(const char* reason) noexcept {
    std::ofstream log(logPath(), std::ios::out | std::ios::app);
    if (!log) return;

    log << "=== FULL WEATHER SNAPSHOT";
    if (reason && *reason) log << " [" << reason << "]";
    log << " ===\n";
    log << std::hex << std::uppercase;
    log << "ModuleBase: 0x" << moduleBase_ << "\n";
    log << "WeatherManager: 0x" << weatherManager_ << "\n";
    log << "WeatherPreset: 0x" << weatherPreset_ << "\n";
    log << "PrecipitationContext: 0x" << precipitationContext_ << "\n";
    log << "PrecipitationObject: 0x" << precipitationObject_ << "\n";
    log << std::dec;
    log << "PresetIndex: " << activePresetIndex_ << "\n";
    log << "TOD: " << timeOfDay_ << " paused=" << (timePaused_ ? 1 : 0) << "\n";
    log << "NativePrecipitationAvailable=" << (precipitationAvailable_ ? 1 : 0)
        << " state=" << precipitationStateRaw_
        << " intensity=" << precipitationIntensity_ << "\n";

    if (managerBytesValid_)
        dumpRawDwords(log, "WEATHER MANAGER", managerBytes_);
    if (presetBytesValid_)
        dumpRawDwords(log, "WEATHER PRESET", presetBytes_);

    log << "-- PRECIPITATION CANDIDATES FULL --\n";
    log << "count=" << precipitationCandidateCount_ << "\n";
    for (std::uint32_t i = 0; i < precipitationCandidateCount_; ++i) {
        const auto& c = precipitationCandidates_[i];
        log << "[" << i << "] owner=";
        if (c.ownerOffset == 0xFFFFFFFFu) log << "SELF";
        else log << "+0x" << std::hex << std::uppercase << c.ownerOffset << std::dec;
        log << " object=0x" << std::hex << std::uppercase << c.object << std::dec
            << " state=" << c.stateRaw
            << " intensity=" << c.intensity << "\n";
    }
    log << "=== END FULL WEATHER SNAPSHOT ===\n";
    log.flush();
}


void EnvironmentInspector::logPresetSnapshot(const char* reason) noexcept {
    if (!presetBytesValid_) return;

    std::ofstream log(logPath(), std::ios::out | std::ios::app);
    if (!log) return;

    log << "=== WEATHER PRESET SNAPSHOT";
    if (reason && *reason) log << " [" << reason << "]";
    log << " ===\n";

    log << std::hex << std::uppercase;
    log << "WeatherManager: 0x" << weatherManager_ << "\n";
    log << "Preset: 0x" << weatherPreset_ << "\n";
    log << std::dec;
    log << "Preset Index: " << activePresetIndex_ << "\n";

    struct Probe { const char* label; std::size_t off; };
    constexpr Probe probes[] = {
        {"GameplayRain A", 0x060},
        {"GameplayRain B", 0x064},
        {"GameplayRain C", 0x068},
        {"GameplayRain D", 0x06C},
        {"GraphicsRain A", 0x0B0},
        {"GraphicsRain B", 0x0B4},
        {"GraphicsRain C", 0x0B8},
        {"GraphicsRain D", 0x0BC},
        {"Temperature A", 0x100},
        {"Temperature B", 0x104},
        {"Temperature C", 0x108},
        {"Temperature D", 0x10C},
        {"ViewDistance A", 0x150},
        {"ViewDistance B", 0x154},
        {"ViewDistance C", 0x158},
        {"ViewDistance D", 0x15C},
        {"OutdoorFog A", 0x1A0},
        {"OutdoorFog B", 0x1A4},
        {"OutdoorFog C", 0x1A8},
        {"OutdoorFog D", 0x1AC},
        {"CloudCoverage A", 0x1F0},
        {"CloudCoverage B", 0x1F4},
        {"CloudCoverage C", 0x1F8},
        {"CloudCoverage D", 0x1FC},
        {"WindDirection A", 0x240},
        {"WindDirection B", 0x244},
        {"WindDirection C", 0x248},
        {"WindDirection D", 0x24C},
        {"WindStrength A", 0x290},
        {"WindStrength B", 0x294},
        {"WindStrength C", 0x298},
        {"WindStrength D", 0x29C},
    };

    for (const auto& p : probes) {
        float v{};
        std::uint32_t raw{};
        std::memcpy(&raw, presetBytes_.data() + p.off, sizeof(raw));
        v = std::bit_cast<float>(raw);

        log << p.label << " +0x"
            << std::hex << std::uppercase << p.off
            << " raw=0x" << std::setw(8) << std::setfill('0') << raw
            << std::setfill(' ') << std::dec;

        if (std::isfinite(v) && std::fabs(v) < 1000000.0f)
            log << " float=" << v;

        log << "\n";
    }

    log << "Snow candidate +0x2B8 = " << (hasSnow_ ? 1 : 0) << "\n";
    log << "Fog candidate  +0x2B9 = " << (hasFog_ ? 1 : 0) << "\n";
    log << "=== END WEATHER PRESET SNAPSHOT ===\n";
    log.flush();
}

void EnvironmentInspector::drawUi() noexcept {
    ImGui::SetNextWindowSize(ImVec2(650.0f, 720.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(35.0f, 35.0f), ImGuiCond_FirstUseEver);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;

    if (!ImGui::Begin("Outlaws Tools", nullptr, flags)) {
        ImGui::End();
        return;
    }

    ImGui::TextColored(ImVec4(0.40f, 0.78f, 1.0f, 1.0f),
                       "Outlaws Tools");
    ImGui::SameLine();
    ImGui::TextDisabled("Weather Inspector");
    ImGui::Separator();

    if (ImGui::BeginTabBar("MainTabs")) {
        if (ImGui::BeginTabItem("Weather")) {
            ImGui::Spacing();
            ImGui::TextUnformatted("Active Scene / WeatherPreset");
            ImGui::Separator();
            ptrRow("Weather Manager", weatherManager_);
            ptrRow("Weather Preset", weatherPreset_);

            ImGui::TextUnformatted("Preset / Transition Index");
            ImGui::SameLine(260.0f);
            if (weatherAvailable_)
                ImGui::Text("%d", activePresetIndex_);
            else
                ImGui::TextDisabled("N/A");

            ImGui::Spacing();
            ImGui::TextUnformatted("Live precipitation");
            ImGui::Separator();

            if (precipitationAvailable_) {
                ImGui::TextUnformatted("Native getter context");
                ImGui::SameLine(260.0f);
                ImGui::Text("0x%llX",
                    static_cast<unsigned long long>(precipitationContext_));

                ImGui::TextUnformatted("Environment object");
                ImGui::SameLine(260.0f);
                ImGui::Text("0x%llX",
                    static_cast<unsigned long long>(precipitationObject_));

                ImGui::TextUnformatted("Precipitation State Raw");
                ImGui::SameLine(260.0f);
                ImGui::Text("%.4f", precipitationStateRaw_);

                ImGui::TextUnformatted("Precipitation Intensity");
                ImGui::SameLine(260.0f);
                ImGui::TextColored(
                    precipitationIntensity_ > 0.001f
                        ? ImVec4(0.30f, 0.90f, 0.48f, 1.0f)
                        : ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
                    "%.4f", precipitationIntensity_);
            } else {
                ImGui::TextColored(
                    ImVec4(1.0f, 0.76f, 0.28f, 1.0f),
                    "NATIVE SCRIPT GETTER NOT ACTIVE IN THIS SCENE");
                ImGui::TextWrapped(
                    "The script nodes are not being called continuously here. "
                    "Fallback owner scanning is active below.");
            }

            ImGui::Spacing();
            ImGui::TextUnformatted("Live precipitation candidates");
            ImGui::Separator();

            if (precipitationCandidateCount_ == 0) {
                ImGui::TextDisabled("No plausible +0x388/+0x3B8 candidates found.");
            } else {
                if (ImGui::BeginTable("PrecipCandidates", 4,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Owner +");
                    ImGui::TableSetupColumn("Object");
                    ImGui::TableSetupColumn("State");
                    ImGui::TableSetupColumn("Intensity");
                    ImGui::TableHeadersRow();

                    for (std::uint32_t i = 0; i < precipitationCandidateCount_; ++i) {
                        const auto& c = precipitationCandidates_[i];
                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0);
                        if (c.ownerOffset == 0xFFFFFFFFu)
                            ImGui::TextUnformatted("SELF");
                        else
                            ImGui::Text("+0x%X", c.ownerOffset);

                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("0x%llX", static_cast<unsigned long long>(c.object));

                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%.4f", c.stateRaw);

                        ImGui::TableSetColumnIndex(3);
                        if (c.intensity > 0.001f)
                            ImGui::TextColored(ImVec4(0.30f,0.90f,0.48f,1.0f), "%.4f", c.intensity);
                        else
                            ImGui::Text("%.4f", c.intensity);
                    }

                    ImGui::EndTable();
                }
            }

            ImGui::Spacing();
            ImGui::TextUnformatted("Decoded preset candidates");
            ImGui::Separator();

            auto presetFloat = [&](std::size_t off, float& out) -> bool {
                if (!presetBytesValid_ || off + sizeof(float) > presetBytes_.size())
                    return false;
                std::memcpy(&out, presetBytes_.data() + off, sizeof(float));
                return std::isfinite(out);
            };

            float v{};
            auto candidateRow = [&](const char* label, std::size_t off) {
                ImGui::TextUnformatted(label);
                ImGui::SameLine(260.0f);
                if (presetFloat(off, v))
                    ImGui::Text("+0x%03zX  %.4f", off, v);
                else
                    ImGui::TextDisabled("N/A");
            };

            candidateRow("Graphics Rain A", 0x0B0);
            candidateRow("Graphics Rain B", 0x0B4);
            candidateRow("Graphics Rain C", 0x0B8);
            candidateRow("Temperature A", 0x100);
            candidateRow("Temperature B", 0x104);
            candidateRow("Temperature C", 0x108);
            candidateRow("Temperature D", 0x10C);
            candidateRow("Outdoor Fog A", 0x1A0);
            candidateRow("Outdoor Fog B", 0x1A4);
            candidateRow("Wind Strength A", 0x290);
            candidateRow("Wind Strength B", 0x294);
            candidateRow("Wind Strength C", 0x298);
            candidateRow("Wind Strength D", 0x29C);

            ImGui::TextDisabled(
                "Candidate labels are provisional. Values are read-only and come from "
                "the active WeatherPreset blocks that changed between scenes.");

            ImGui::Spacing();
            ImGui::TextUnformatted("Preset contribution fields");
            ImGui::Separator();
            stateRow("Gameplay Rain field", gameplayRainField_, weatherAvailable_);
            stateRow("Graphics Rain field", graphicsRainField_, weatherAvailable_);
            stateRow("Temperature field", temperatureField_, weatherAvailable_);
            stateRow("View Distance field", viewDistanceField_, weatherAvailable_);
            stateRow("Outdoor Fog field", outdoorFogField_, weatherAvailable_);
            stateRow("Cloud Coverage field", cloudCoverageField_, weatherAvailable_);
            stateRow("Wind Direction field", windDirectionField_, weatherAvailable_);
            stateRow("Wind Strength field", windStrengthField_, weatherAvailable_);

            ImGui::Spacing();
            ImGui::TextUnformatted("Unvalidated preset bytes");
            ImGui::Separator();
            ImGui::Text("Snow candidate (+0x2B8): %u", hasSnow_ ? 1u : 0u);
            ImGui::Text("Fog candidate  (+0x2B9): %u", hasFog_ ? 1u : 0u);
            ImGui::TextColored(
                ImVec4(1.0f, 0.76f, 0.28f, 1.0f),
                "These two bytes are NOT yet validated as live Snow/Fog state.");

            ImGui::Spacing();
            ImGui::TextUnformatted("Live weather decoding");
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.76f, 0.28f, 1.0f),
                               "Numeric property layout: AUTO-DIAGNOSTIC ACTIVE");
            ImGui::TextWrapped(
                "The active preset uses 0x48-byte property objects, not plain floats. "
                "When the scene or planet changes, the logger compares those blocks and "
                "records the exact offsets that changed. No descriptor or preset memory is written.");

            ImGui::Spacing();
            ImGui::TextDisabled(
                "Indoor / Outdoor, WeatherMask and Sandstorm will be added as native probes "
                "after the active preset layout is confirmed.");

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Time of Day")) {
            ImGui::Spacing();

            if (todAvailable_) {
                int seconds = static_cast<int>(timeOfDay_ * 3600.0f);
                if (seconds < 0) seconds = 0;
                const int hh = (seconds / 3600) % 24;
                const int mm = (seconds / 60) % 60;
                const int ss = seconds % 60;
                ImGui::Text("Current Time: %02d:%02d:%02d", hh, mm, ss);
                stateRow("Paused", timePaused_, true);
            } else {
                ImGui::TextDisabled("TimeOfDaySystem unavailable");
            }

            ImGui::Spacing();
            if (ImGui::Button("06:00", ImVec2(110, 34)))
                environment_->setTimeOfDay(6.0f);
            ImGui::SameLine();
            if (ImGui::Button("12:00", ImVec2(110, 34)))
                environment_->setTimeOfDay(12.0f);
            ImGui::SameLine();
            if (ImGui::Button("18:00", ImVec2(110, 34)))
                environment_->setTimeOfDay(18.0f);

            if (ImGui::Button("Pause TOD", ImVec2(170, 34)))
                environment_->setTimePaused(true);
            ImGui::SameLine();
            if (ImGui::Button("Resume TOD", ImVec2(170, 34)))
                environment_->setTimePaused(false);

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Diagnostics")) {
            ImGui::Spacing();
            ptrRow("Module Base", moduleBase_);
            ptrRow("Weather Manager", weatherManager_);
            ptrRow("Weather Preset", weatherPreset_);

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.30f, 0.90f, 0.48f, 1.0f),
                               "In-game DX12 overlay");
            ImGui::TextColored(ImVec4(0.30f, 0.90f, 0.48f, 1.0f),
                               "Full weather logger: ACTIVE");
            ImGui::TextColored(ImVec4(0.30f, 0.90f, 0.48f, 1.0f),
                               "Unsafe weather writes: DISABLED");

            ImGui::Spacing();
            ImGui::TextWrapped(
                "Sampling interval: 250 ms. Diagnostic build logs the full 0x360-byte "
                "WeatherManager, full 0x2C0-byte WeatherPreset, every precipitation candidate, "
                "plus DWORD diffs whenever any tracked value changes. No weather memory is written.");

            ImGui::Spacing();
            ImGui::TextDisabled("F10 toggles the in-game overlay.");

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
