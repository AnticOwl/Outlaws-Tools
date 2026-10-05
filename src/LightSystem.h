#pragma once
#include <cstdint>
#include <vector>

namespace outlaws {
struct Vec3 { float x{}, y{}, z{}; };
struct Color3 { float r{1.f}, g{1.f}, b{1.f}; };

enum class LightType : std::uint8_t { Unknown, Point, Spot, Area, Directional };

struct LightHandle {
    std::uintptr_t address{};
    LightType type{LightType::Unknown};
    Vec3 position{};
    Vec3 rotation{};
    Color3 color{};
    float intensity{1.f};
    float range{10.f};
    float innerCone{};
    float outerCone{};
    bool enabled{true};
};

class LightSystem {
public:
    bool scan();
    bool refresh();
    LightHandle* selected();
    bool moveSelectedToCamera(float forwardDistance = 2.0f);
    bool cloneSelected();
    bool spawnPoint();
    bool spawnSpot();
    bool removeSelected();

    const std::vector<LightHandle>& lights() const { return m_lights; }
private:
    std::vector<LightHandle> m_lights;
    std::size_t m_selected{};
};
}
