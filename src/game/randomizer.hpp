#pragma once

#include "game/types.hpp"

#include <array>
#include <cstdint>

namespace tetris {

using RandomSamples = std::array<std::uint8_t, 3>;

struct PieceQueue {
    PieceKind active{PieceKind::L};
    PieceKind preview{PieceKind::L};
    PieceKind hidden{PieceKind::L};
    int attempts{};
};

PieceKind piece_from_divider(std::uint8_t divider);
PieceQueue advance_piece_queue(PieceKind preview, PieceKind hidden,
                               const RandomSamples& samples);

} // namespace tetris
