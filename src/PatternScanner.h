#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace outlaws {
class PatternScanner {
public:
    PatternScanner(std::uintptr_t moduleBase, std::size_t imageSize) noexcept;

    std::uintptr_t find(std::string_view pattern) const noexcept;
    static std::uintptr_t resolveRip(std::uintptr_t instruction, std::size_t displacementOffset, std::size_t instructionSize) noexcept;

private:
    std::uintptr_t moduleBase_{};
    std::size_t imageSize_{};
};
}
