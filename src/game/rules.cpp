#include "game/rules.hpp"

#include <algorithm>
#include <array>
#include <cassert>

namespace tetris {

const std::array<int, 21>& gravity_table() {
    static constexpr std::array<int, 21> frames = {
        52, 48, 44, 40, 36, 32, 27, 21, 16, 10, 9,
        8, 7, 6, 5, 5, 4, 4, 3, 3, 2,
    };
    return frames;
}

int frames_per_drop(int level, bool heart_mode) {
    const int index = std::clamp(level + (heart_mode ? 10 : 0), 0, 20);
    return gravity_table()[static_cast<std::size_t>(index)];
}

std::uint32_t line_clear_score(int rows, int level) {
    constexpr std::array<std::uint32_t, 5> base = {0, 40, 100, 300, 1'200};
    assert(rows >= 0 && rows <= 4);
    return base[static_cast<std::size_t>(rows)] * static_cast<std::uint32_t>(level + 1);
}

} // namespace tetris
