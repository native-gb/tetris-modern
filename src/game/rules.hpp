#pragma once

#include "game/types.hpp"

#include <cstdint>
#include <array>

namespace tetris {

const std::array<int, 21>& gravity_table();
int frames_per_drop(int level, bool heart_mode);
std::uint32_t line_clear_score(int rows, int level);

} // namespace tetris
