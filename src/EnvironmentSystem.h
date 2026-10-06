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

class EnvironmentSystem {
public:
    static constexpr std::uintptr_t TodRootRva      = 0x97D20F8;
    static constexpr std::uintptr_t TodSlotRva      = 0x8A749C8;
    static constexpr std::uintptr_t SetTimeOfDayRva = 0x32F1250;
    static constexpr std::uintptr_t RuntimeLookupRva = 0x17664B0;

    bool initialize(std::uintptr_t moduleBase) noexcept;
    void shutdown() noexcept;

    bool read(EnvironmentState& out) const;
    bool apply(const EnvironmentState& state);

    bool logTimeOfDay() const noexcept;
    bool setTimeOfDay(float value) noexcept;
    bool setTimePaused(bool paused) noexcept;
    bool logRain() const noexcept;
    bool setRain(float value) noexcept;

private:
    void* resolveTimeOfDaySystem() const noexcept;
    void* lookupRuntimeValue(const char* name) const noexcept;
    void appendLog(const char* text) const noexcept;

    std::uintptr_t moduleBase_{};
};

}
