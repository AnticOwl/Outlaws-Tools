#pragma once
#include <cstdint>

namespace outlaws {

class EnvironmentSystem;

class EnvironmentInspector {
public:
    bool start(EnvironmentSystem* environment, std::uintptr_t moduleBase) noexcept;
    void stop() noexcept;
    void render() noexcept;

private:
    void sample() noexcept;
    void drawUi() noexcept;
    void appendLog(const char* text) const noexcept;
    void logSceneChange() noexcept;

    EnvironmentSystem* environment_{};
    std::uintptr_t moduleBase_{};
    bool running_{};
    std::uint64_t lastSampleTick_{};

    float timeOfDay_{};
    bool timePaused_{};
    bool todAvailable_{};

    bool weatherAvailable_{};
    std::uintptr_t weatherManager_{};
    std::uintptr_t weatherPreset_{};
    std::int32_t activePresetIndex_{-1};

    bool gameplayRainField_{};
    bool graphicsRainField_{};
    bool temperatureField_{};
    bool viewDistanceField_{};
    bool outdoorFogField_{};
    bool cloudCoverageField_{};
    bool windDirectionField_{};
    bool windStrengthField_{};

    float gameplayRain_{};
    float graphicsRain_{};
    float temperature_{};
    float viewDistance_{};
    float outdoorFog_{};
    float cloudCoverage_{};
    float windDirection_{};
    float windStrength_{};

    bool hasSnow_{};
    bool hasFog_{};

    bool firstSample_{true};

    std::uintptr_t lastWeatherManager_{};
    std::uintptr_t lastWeatherPreset_{};
    std::int32_t lastActivePresetIndex_{-1};
    bool lastGameplayRainField_{};
    bool lastGraphicsRainField_{};
    bool lastTemperatureField_{};
    bool lastViewDistanceField_{};
    bool lastOutdoorFogField_{};
    bool lastCloudCoverageField_{};
    bool lastWindDirectionField_{};
    bool lastWindStrengthField_{};

    float lastGameplayRain_{};
    float lastGraphicsRain_{};
    float lastTemperature_{};
    float lastViewDistance_{};
    float lastOutdoorFog_{};
    float lastCloudCoverage_{};
    float lastWindDirection_{};
    float lastWindStrength_{};

    bool lastHasSnow_{};
    bool lastHasFog_{};
    bool lastPaused_{};
};

}
