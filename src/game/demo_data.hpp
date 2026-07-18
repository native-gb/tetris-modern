#pragma once

#include "game/input.hpp"
#include "game/piece.hpp"

namespace tetris {

struct DemoRun {
    Buttons held;
    int frames{};
};

struct DemoPiece {
    PieceKind kind{};
    Rotation rotation{};
};

} // namespace tetris
