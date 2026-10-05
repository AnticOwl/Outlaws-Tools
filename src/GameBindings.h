#pragma once
#include <cstdint>
#include <string_view>

namespace outlaws {
struct GameBindings {
    std::uintptr_t moduleBase{};
    bool initialized{};

    bool initialize();
    void shutdown();

    // Reverse-engineering targets discovered in Outlaws.exe.
    // Addresses remain deliberately unset until validated live.
    std::uintptr_t setWeather{};
    std::uintptr_t setFog{};
    std::uintptr_t setCameraExposure{};
    std::uintptr_t setBloom{};
    std::uintptr_t setGlare{};
    std::uintptr_t setColorGrading{};
    std::uintptr_t setDepthOfField{};
    std::uintptr_t setTimeOfDay{};
    std::uintptr_t setTimeOfDayPaused{};
};
}
