#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace outlaws {
#pragma pack(push, 1)
struct EnvDescriptor {
    std::uint64_t initialValueBits;   // +0x00, scalar/default payload observed in constructor
    std::uint32_t reserved08;         // +0x08
    std::uint8_t reserved0C;          // +0x0C
    std::uint8_t pad0D[3];
    std::uintptr_t typeInfo;          // +0x10
    std::uint8_t hasMin;              // +0x18
    std::uint8_t pad19[3];
    float minValue;                   // +0x1C
    std::uint8_t hasMax;              // +0x20
    std::uint8_t pad21[3];
    float maxValue;                   // +0x24
    std::uint8_t hasExtra;            // +0x28
    std::uint8_t pad29[7];
    float extraValue;                 // +0x30
    std::uint16_t id;                 // +0x34
    std::uint16_t pad36;
    const char* name;                 // +0x38
};
#pragma pack(pop)
static_assert(sizeof(EnvDescriptor) == 0x40);
static_assert(offsetof(EnvDescriptor, id) == 0x34);
static_assert(offsetof(EnvDescriptor, name) == 0x38);

struct KnownEnvDescriptor {
    std::uint16_t id;
    std::ptrdiff_t descriptorOffset;
    std::uintptr_t nameRva;
    std::string_view name;
};

class EnvironmentRegistry {
public:
    static constexpr std::uintptr_t ConstructorRva = 0x178A240;
    static constexpr std::size_t DescriptorStride = 0x40;

    static constexpr KnownEnvDescriptor ExposureTarget{0x52, 0x1450, 0x5C98F88, "Env_ExposureTarget2"};
    static constexpr KnownEnvDescriptor GameplayRain{0xD0, 0x33D0, 0x5C99DC0, "Env_GameplayRainAmount"};
    static constexpr KnownEnvDescriptor GameplayFog{0xD1, 0x3410, 0x5C99DD8, "Env_GameplayFogAmount"};

    bool locate(std::uintptr_t moduleBase) noexcept;
    void setDescriptorOwner(std::uintptr_t owner) noexcept { owner_ = owner; }
    [[nodiscard]] std::uintptr_t descriptorOwner() const noexcept { return owner_; }
    [[nodiscard]] const EnvDescriptor* descriptor(const KnownEnvDescriptor& known) const noexcept;
    [[nodiscard]] bool validate(const KnownEnvDescriptor& known) const noexcept;

private:
    std::uintptr_t owner_{};
};
}
