#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace outlaws {
#pragma pack(push, 1)
struct EnvDescriptor {
    std::uint32_t valueOrFlags;       // +0x00 (purpose still under validation)
    std::uint16_t id;                 // +0x04
    std::uint16_t pad06;
    const char* name;                 // +0x08
    std::uint64_t initialValueBits;   // +0x10
    std::uint32_t reserved18;         // +0x18
    std::uint8_t reserved1C;          // +0x1C
    std::uint8_t pad1D[3];
    std::uintptr_t typeInfo;          // +0x20
    std::uint8_t hasMin;              // +0x28
    std::uint8_t pad29[3];
    float minValue;                   // +0x2C
    std::uint8_t hasMax;              // +0x30
    std::uint8_t pad31[3];
    float maxValue;                   // +0x34
    std::uint8_t hasExtra;            // +0x38
    std::uint8_t pad39[7];
};
#pragma pack(pop)
static_assert(sizeof(EnvDescriptor) == 0x40);
static_assert(offsetof(EnvDescriptor, id) == 0x04);
static_assert(offsetof(EnvDescriptor, name) == 0x08);
static_assert(offsetof(EnvDescriptor, typeInfo) == 0x20);

struct KnownEnvDescriptor {
    std::uint16_t id;
    std::ptrdiff_t descriptorOffset;
    std::string_view name;
};

class EnvironmentRegistry {
public:
    static constexpr std::uintptr_t ConstructorRva = 0x178A240;
    static constexpr std::size_t DescriptorStride = 0x40;

    static constexpr KnownEnvDescriptor ExposureTarget{0x52, 0x1480, "Env_ExposureTarget2"};
    static constexpr KnownEnvDescriptor GameplayRain{0xD0, 0x3400, "Env_GameplayRainAmount"};
    static constexpr KnownEnvDescriptor GameplayFog{0xD1, 0x3440, "Env_GameplayFogAmount"};

    void setDescriptorOwner(std::uintptr_t owner) noexcept { owner_ = owner; }
    [[nodiscard]] std::uintptr_t descriptorOwner() const noexcept { return owner_; }
    [[nodiscard]] const EnvDescriptor* descriptor(const KnownEnvDescriptor& known) const noexcept;
    [[nodiscard]] bool validate(const KnownEnvDescriptor& known) const noexcept;

private:
    std::uintptr_t owner_{};
};
}
