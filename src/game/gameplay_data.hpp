#pragma once

#include "game/types.hpp"

#include <array>

namespace tetris {

using PieceShape = std::array<Cell, 4>;

struct GameplayData {
    std::array<int, 21> gravity_frames{};
    std::array<PieceShape, 28> piece_shapes{};
};

} // namespace tetris
