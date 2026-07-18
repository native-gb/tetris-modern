#pragma once

#include "game/types.hpp"

#include <array>

namespace tetris {

struct FallingPiece {
    PieceKind kind{PieceKind::L};
    Rotation rotation{Rotation::spawn};
    Cell origin{3, -1};
};

Rotation clockwise(Rotation rotation);
Rotation counterclockwise(Rotation rotation);
std::array<Cell, 4> occupied_cells(const FallingPiece& piece);
Block block_for(PieceKind kind);

} // namespace tetris
