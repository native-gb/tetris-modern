#pragma once

#include "game/gameplay_data.hpp"

#include <cstdint>

namespace tetris {

int frames_per_drop(const GameplayData& data, int level, bool heart_mode);
std::uint32_t line_clear_score(int rows, int level);

} // namespace tetris
