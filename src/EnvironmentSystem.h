#pragma once

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
    bool read(EnvironmentState& out) const;
    bool apply(const EnvironmentState& state);
};
}
