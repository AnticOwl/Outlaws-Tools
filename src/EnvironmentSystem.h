#pragma once
#include <cstdint>

namespace outlaws {

struct EnvironmentState {
    float timeOfDay{12.f};
    bool timePaused{};
    float rain{};
    float fog{};
    float outdoorFogDensity{};
    float wind{};
    float cloudCover{};
    float snow{};
};

struct WeatherSceneState {
    bool available{};
    std::uintptr_t manager{};
    std::uintptr_t preset{};
    std::int32_t activePresetIndex{-1};

    bool gameplayRainField{};
    bool graphicsRainField{};
    bool temperatureField{};
    bool viewDistanceField{};
    bool outdoorFogField{};
    bool cloudCoverageField{};
    bool windDirectionField{};
    bool windStrengthField{};

    float gameplayRain{};
    float graphicsRain{};
    float temperature{};
    float viewDistance{};
    float outdoorFog{};
    float cloudCoverage{};
    float windDirection{};
    float windStrength{};

    bool hasSnow{};
    bool hasFog{};
};

class EnvironmentSystem {
public:
    static constexpr std::uintptr_t TodRootRva      = 0x97D20F8;
    static constexpr std::uintptr_t TodSlotRva      = 0x8A749C8;
    static constexpr std::uintptr_t SetTimeOfDayRva = 0x32F1250;

    bool initialize(std::uintptr_t moduleBase) noexcept;
    void shutdown() noexcept;

    bool read(EnvironmentState& out) const;
    bool apply(const EnvironmentState& state);

    bool readWeatherScene(WeatherSceneState& out) const noexcept;

    bool logTimeOfDay() const noexcept;
    bool setTimeOfDay(float value) noexcept;
    bool setTimePaused(bool paused) noexcept;

private:
    void* resolveTimeOfDaySystem() const noexcept;
    void* resolveWeatherManager() const noexcept;
    void* resolveActiveWeatherPreset(void* manager) const noexcept;
    void appendLog(const char* text) const noexcept;

    std::uintptr_t moduleBase_{};
};

}
