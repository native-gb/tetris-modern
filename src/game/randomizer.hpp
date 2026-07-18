#pragma once

#include "game/piece.hpp"

#include <array>
#include <cstdint>

namespace tetris {

using RandomSamples = std::array<std::uint8_t, 3>;

struct PieceQueue {
    PieceSpec active{};
    PieceSpec preview{};
    PieceSpec hidden{};
    int attempts{};
};

PieceKind piece_from_divider(std::uint8_t divider);
PieceQueue advance_piece_queue(PieceSpec preview, PieceSpec hidden,
                               const RandomSamples& samples);

} // namespace tetris
