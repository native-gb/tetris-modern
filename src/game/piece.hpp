#pragma once

#include "game/types.hpp"

#include <array>

namespace tetris {

struct PieceSpec {
    PieceKind kind{PieceKind::L};
    Rotation rotation{Rotation::spawn};

    bool operator==(const PieceSpec&) const = default;
};

struct FallingPiece {
    PieceKind kind{PieceKind::L};
    Rotation rotation{Rotation::spawn};
    Cell origin{3, -1};
};

FallingPiece spawn_piece(PieceSpec piece);

Rotation clockwise(Rotation rotation);
Rotation counterclockwise(Rotation rotation);
std::array<Cell, 4> piece_cells(PieceSpec piece);
std::array<Cell, 4> occupied_cells(const FallingPiece& piece);
std::array<Block, 4> blocks_for(const FallingPiece& piece);
Block garbage_block(std::uint8_t sample);

} // namespace tetris
