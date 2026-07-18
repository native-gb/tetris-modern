#include "game/rules.hpp"

#include <algorithm>
#include <array>
#include <cassert>

namespace tetris {

int frames_per_drop(const GameplayData& data, int level, bool heart_mode) {
    const int index = std::clamp(level + (heart_mode ? 10 : 0), 0, 20);
    return data.gravity_frames[static_cast<std::size_t>(index)];
}

std::uint32_t line_clear_score(int rows, int level) {
    constexpr std::array<std::uint32_t, 5> base = {0, 40, 100, 300, 1'200};
    assert(rows >= 0 && rows <= 4);
    return base[static_cast<std::size_t>(rows)] * static_cast<std::uint32_t>(level + 1);
}

} // namespace tetris
