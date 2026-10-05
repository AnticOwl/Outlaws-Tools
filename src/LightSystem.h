#pragma once
#include <cstdint>
#include <vector>

namespace outlaws {
struct Vec3 { float x{}, y{}, z{}; };
struct Color3 { float r{1.f}, g{1.f}, b{1.f}; };

enum class LightType : std::uint8_t { Unknown, Point, Spot, Tube, Area, Directional };

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
    static constexpr std::uintptr_t PointTypeGetterRva = 0x19C7C90;
    static constexpr std::uintptr_t SpotTypeGetterRva  = 0x19C7DE0;
    static constexpr std::uintptr_t TubeTypeGetterRva  = 0x19C7E50;
    static constexpr std::uintptr_t BaseTypeGetterRva  = 0x19C7C80;

    static constexpr std::uintptr_t PointTypeGlobalRva = 0x9666900;
    static constexpr std::uintptr_t SpotTypeGlobalRva  = 0x9666A50;
    static constexpr std::uintptr_t TubeTypeGlobalRva  = 0x9666908;
    static constexpr std::uintptr_t AreaTypeGlobalRva  = 0x96BE680;
    static constexpr std::uintptr_t BaseTypeGlobalRva  = 0x96669D0;

    // Confirmed allocation wrappers and constructors from static analysis.
    // These construct the light-class objects but are NOT called by spawnPoint/spawnSpot yet:
    // world/entity registration and ownership still need to be recovered first.
    static constexpr std::uintptr_t PointFactoryRva = 0x1D4E090;
    static constexpr std::uintptr_t PointConstructorRva = 0x194E220;
    static constexpr std::size_t PointObjectSize = 0x38;

    static constexpr std::uintptr_t SpotFactoryRva = 0x1D4E4C0;
    static constexpr std::uintptr_t SpotConstructorRva = 0x1950560;
    static constexpr std::size_t SpotObjectSize = 0x40;

    static constexpr std::uintptr_t TubeFactoryRva = 0x1D4E5E0;
    static constexpr std::uintptr_t TubeConstructorRva = 0x1951360;
    static constexpr std::size_t TubeObjectSize = 0x40;

    void bind(std::uintptr_t moduleBase) noexcept { moduleBase_ = moduleBase; }
    [[nodiscard]] std::uintptr_t pointTypeDescriptor() const noexcept;
    [[nodiscard]] std::uintptr_t spotTypeDescriptor() const noexcept;
    [[nodiscard]] std::uintptr_t tubeTypeDescriptor() const noexcept;
    [[nodiscard]] std::uintptr_t baseTypeDescriptor() const noexcept;
    [[nodiscard]] std::uintptr_t areaTypeDescriptor() const noexcept;

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
    [[nodiscard]] std::uintptr_t readTypeGlobal(std::uintptr_t rva) const noexcept;

    std::uintptr_t moduleBase_{};
    std::vector<LightHandle> m_lights;
    std::size_t m_selected{};
};
}
