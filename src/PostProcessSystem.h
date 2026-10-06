#pragma once
#include <atomic>
#include <cstdint>

namespace outlaws {

class PostProcessSystem {
public:
    static constexpr std::uintptr_t EnvironmentUpdateRva = 0x17B9CA0;
    static constexpr std::uintptr_t FloatSetterRva       = 0x17D9C20;
    static constexpr std::uintptr_t BoolSetterRva        = 0x17D9CF0;

    static constexpr std::uint16_t BloomStrengthId            = 0x2C;
    static constexpr std::uint16_t GlareStrengthId            = 0x36;
    static constexpr std::uint16_t ExposureTargetId           = 0x52;
    static constexpr std::uint16_t FilmGrainAmountId          = 0xE9;
    static constexpr std::uint16_t LensFlareEnabledId         = 0x177;
    static constexpr std::uint16_t DepthOfFieldEnabledId      = 0x178;
    static constexpr std::uint16_t FilmGrainEnabledId         = 0x18B;
    static constexpr std::uint16_t LensVeilingGlareEnabledId  = 0x190;
    static constexpr std::uint16_t LensGlareEnabledId         = 0x191;
    static constexpr std::uint16_t GlareEnabledId             = 0x192;
    static constexpr std::uint16_t CameraLensOpticsEnabledId  = 0x197;

    bool initialize(std::uintptr_t moduleBase) noexcept;
    void shutdown() noexcept;

    [[nodiscard]] bool ready() const noexcept { return environmentSystem_.load() != 0; }
    [[nodiscard]] std::uintptr_t environmentSystem() const noexcept { return environmentSystem_.load(); }

    bool setExposure(float value) noexcept;
    bool setBloom(float value) noexcept;
    bool setGlare(float value) noexcept;
    bool setFilmGrainAmount(float value) noexcept;

    bool setLensFlare(bool enabled) noexcept;
    bool setDepthOfField(bool enabled) noexcept;
    bool setFilmGrain(bool enabled) noexcept;
    bool setLensGlare(bool enabled) noexcept;
    bool setLensVeilingGlare(bool enabled) noexcept;
    bool setGlareEnabled(bool enabled) noexcept;
    bool setCameraLensOptics(bool enabled) noexcept;

    bool applyTestPresetOff() noexcept;
    bool applyTestPresetOn() noexcept;

private:
    using EnvironmentUpdateFn = void(*)(void*);
    using FloatSetterFn = void(*)(void*, std::uint16_t, float, std::uint32_t, std::uint32_t);
    using BoolSetterFn  = void(*)(void*, std::uint16_t, bool, std::uint32_t, std::uint32_t);

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

    bool setFloat(std::uint16_t id, float value, const char* label) noexcept;
    bool setBool(std::uint16_t id, bool value, const char* label) noexcept;
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
};

}
