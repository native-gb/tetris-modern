#pragma once

#include "game/input.hpp"
#include "game/piece.hpp"

namespace tetris {

struct DemoRun {
    Buttons held;
    int frames{};
};

using DemoPiece = PieceSpec;

} // namespace tetris
