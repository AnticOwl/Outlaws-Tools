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

    // Confirmed Snowdrop schema/type-description allocation wrappers.
    // IMPORTANT: these allocate reflection/schema objects and register property IDs;
    // they are NOT world-light spawners and must never be used by spawnPoint/spawnSpot.
    static constexpr std::uintptr_t PointSchemaFactoryRva = 0x1D4E090;
    static constexpr std::uintptr_t PointSchemaConstructorRva = 0x194E220;
    static constexpr std::size_t PointSchemaObjectSize = 0x38;

    static constexpr std::uintptr_t SpotSchemaFactoryRva = 0x1D4E4C0;
    static constexpr std::uintptr_t SpotSchemaConstructorRva = 0x1950560;
    static constexpr std::size_t SpotSchemaObjectSize = 0x40;

    static constexpr std::uintptr_t TubeSchemaFactoryRva = 0x1D4E5E0;
    static constexpr std::uintptr_t TubeSchemaConstructorRva = 0x1951360;
    static constexpr std::size_t TubeSchemaObjectSize = 0x40;

    // Prefab node registration path. All four light prefab nodes are registered through
    // the common Snowdrop dispatcher at 0x0BA6660. Point/Spot are then post-processed
    // through 0x0C79C20 during node registration.
    static constexpr std::uintptr_t PrefabNodeDispatcherRva = 0x0BA6660;
    static constexpr std::uintptr_t PrefabNodeFinalizeRva   = 0x0C79C20;
    static constexpr std::uintptr_t PointPrefabRegistrationRva = 0x1DAE4EC;
    static constexpr std::uintptr_t SpotPrefabRegistrationRva  = 0x1DAE536;
    static constexpr std::uintptr_t TubePrefabRegistrationRva  = 0x1DAE580;
    static constexpr std::uintptr_t AreaPrefabRegistrationRva  = 0x1DAE5CA;

    // Confirmed prefab-light node execution handlers. These consume Snowdrop node
    // execution parameters (including the parameter containers at +0x830/+0x840),
    // so they are NOT safe to call without constructing a valid execution context.
    static constexpr std::uintptr_t PointNodeExecuteRva = 0x19A50D0;
    static constexpr std::uintptr_t SpotNodeExecuteRva  = 0x19ACE20;
    static constexpr std::uintptr_t TubeNodeExecuteRva  = 0x19AF2E0;

    // Shared renderer creation/registration path. Spot reaches the generic create path
    // from this exact callsite and then stores the returned 32-bit renderer handle.
    static constexpr std::uintptr_t SpotRendererCreateCallsiteRva = 0x19ADCAF;
    static constexpr std::uintptr_t GenericRendererCreateRva = 0x1B70C20;
    static constexpr std::uintptr_t ExistingManagerCreateRva = 0x1B70650;

    // The Tube execution path contains an explicit renderer registration state bit and
    // calls this add/remove pair. These are recorded for reverse-engineering only until
    // their exact renderer-object contract is proven for Point/Spot as well.
    static constexpr std::uintptr_t TubeRendererRegisterRva   = 0x1C57B70;
    static constexpr std::uintptr_t TubeRendererUnregisterRva = 0x1CB8FF0;

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
