#pragma once
#include <atomic>
#include <cstdint>

namespace outlaws {

struct PostProcessState {
    float exposure{1.f};
    float bloom{1.f};
    float glare{1.f};
    float saturation{1.f};
    float contrast{1.f};
    float gamma{1.f};
    bool lensFlare{true};
    bool lensGlare{true};
    bool depthOfField{true};
    bool filmGrain{true};
};

class PostProcessSystem {
public:
    static constexpr std::uintptr_t EnvRegistryOwnerRva = 0x9658C70;
    static constexpr std::uintptr_t EnvDescriptorLookupRva = 0x177A410;
    static constexpr std::uintptr_t EnvironmentUpdateRva = 0x17B9CA0;
    static constexpr std::uintptr_t FloatSetterRva = 0x17D9C20;

    static constexpr std::uint16_t ExposureTargetId = 0x52;

    bool initialize(std::uintptr_t moduleBase) noexcept;
    void shutdown() noexcept;

    [[nodiscard]] bool ready() const noexcept { return environmentSystem_.load() != 0; }
    [[nodiscard]] std::uintptr_t environmentSystem() const noexcept { return environmentSystem_.load(); }
    [[nodiscard]] bool originalExposureKnown() const noexcept { return originalExposureKnown_.load(); }
    [[nodiscard]] float originalExposure() const noexcept;

    bool setExposure(float value) noexcept;
    bool restoreExposure() noexcept;

    bool read(PostProcessState& out) const;
    bool apply(const PostProcessState& state);

private:
    using EnvironmentUpdateFn = void(*)(void*);
    using FloatSetterFn = void(*)(void*, std::uint16_t, float, std::uint32_t, std::uint32_t);

    static void environmentUpdateDetour(void* environmentSystem);
    static void floatSetterDetour(void* environmentSystem, std::uint16_t id, float value,
                                  std::uint32_t flags, std::uint32_t extra);

    void onEnvironmentUpdate(void* environmentSystem) noexcept;
    void onFloatSetter(void* environmentSystem, std::uint16_t id, float value,
                       std::uint32_t flags, std::uint32_t extra) noexcept;

    bool installEnvironmentUpdateHook() noexcept;
    void removeEnvironmentUpdateHook() noexcept;
    bool installFloatSetterHook() noexcept;
    void removeFloatSetterHook() noexcept;
    void appendLog(const char* text) const noexcept;

    std::uintptr_t moduleBase_{};

    std::uintptr_t environmentUpdateTarget_{};
    void* environmentUpdateTrampoline_{};
    unsigned char environmentUpdateOriginalBytes_[16]{};

    std::uintptr_t floatSetterTarget_{};
    void* floatSetterTrampoline_{};
    unsigned char floatSetterOriginalBytes_[15]{};

    std::atomic<std::uintptr_t> environmentSystem_{};
    std::atomic<std::uint32_t> lastFlags_{};
    std::atomic<std::uint32_t> lastExtra_{};
    std::atomic<std::uint32_t> originalExposureBits_{};
    std::atomic_bool originalExposureKnown_{};
    std::atomic_bool injectedWrite_{};
};

}
