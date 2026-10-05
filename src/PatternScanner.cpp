#include "PatternScanner.h"
#include <Windows.h>
#include <charconv>
#include <vector>

namespace outlaws {
namespace {
struct Token { bool wildcard{}; std::uint8_t value{}; };

std::vector<Token> parse(std::string_view pattern) {
    std::vector<Token> out;
    std::size_t i = 0;
    while (i < pattern.size()) {
        while (i < pattern.size() && pattern[i] == ' ') ++i;
        if (i >= pattern.size()) break;
        if (pattern[i] == '?') {
            out.push_back({true, 0});
            while (i < pattern.size() && pattern[i] == '?') ++i;
            continue;
        }
        if (i + 1 >= pattern.size()) return {};
        unsigned value = 0;
        const char* first = pattern.data() + i;
        const char* last = first + 2;
        auto [ptr, ec] = std::from_chars(first, last, value, 16);
        if (ec != std::errc{} || ptr != last) return {};
        out.push_back({false, static_cast<std::uint8_t>(value)});
        i += 2;
    }
    return out;
}
}

PatternScanner::PatternScanner(std::uintptr_t moduleBase, std::size_t imageSize) noexcept
    : moduleBase_(moduleBase), imageSize_(imageSize) {}

std::uintptr_t PatternScanner::find(std::string_view pattern) const noexcept {
    if (!moduleBase_ || !imageSize_) return 0;
    const auto tokens = parse(pattern);
    if (tokens.empty() || tokens.size() > imageSize_) return 0;
    const auto* data = reinterpret_cast<const std::uint8_t*>(moduleBase_);
    const std::size_t last = imageSize_ - tokens.size();
    for (std::size_t i = 0; i <= last; ++i) {
        bool match = true;
        for (std::size_t j = 0; j < tokens.size(); ++j) {
            if (!tokens[j].wildcard && data[i + j] != tokens[j].value) {
                match = false;
                break;
            }
        }
        if (match) return moduleBase_ + i;
    }
    return 0;
}

std::uintptr_t PatternScanner::resolveRip(std::uintptr_t instruction, std::size_t displacementOffset, std::size_t instructionSize) noexcept {
    if (!instruction) return 0;
    const auto displacement = *reinterpret_cast<const std::int32_t*>(instruction + displacementOffset);
    return instruction + instructionSize + displacement;
}
}
