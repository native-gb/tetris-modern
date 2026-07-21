#pragma once

#include "game/gameplay_data.hpp"

namespace tetris::test {

inline GameplayData gameplay_data() {
    GameplayData data;
    data.gravity_frames = {
        52, 48, 44, 40, 36, 32, 27, 21, 16, 10, 9,
        8, 7, 6, 5, 5, 4, 4, 3, 3, 2,
    };
    data.piece_shapes = {{
        PieceShape{{{0, 2}, {1, 2}, {2, 2}, {0, 3}}},
        PieceShape{{{1, 1}, {1, 2}, {1, 3}, {2, 3}}},
        PieceShape{{{2, 1}, {0, 2}, {1, 2}, {2, 2}}},
        PieceShape{{{0, 1}, {1, 1}, {1, 2}, {1, 3}}},
        PieceShape{{{0, 2}, {1, 2}, {2, 2}, {2, 3}}},
        PieceShape{{{1, 1}, {2, 1}, {1, 2}, {1, 3}}},
        PieceShape{{{0, 1}, {0, 2}, {1, 2}, {2, 2}}},
        PieceShape{{{1, 1}, {1, 2}, {0, 3}, {1, 3}}},
        PieceShape{{{0, 2}, {1, 2}, {2, 2}, {3, 2}}},
        PieceShape{{{1, 0}, {1, 1}, {1, 2}, {1, 3}}},
        PieceShape{{{0, 2}, {1, 2}, {2, 2}, {3, 2}}},
        PieceShape{{{1, 0}, {1, 1}, {1, 2}, {1, 3}}},
        PieceShape{{{1, 2}, {2, 2}, {1, 3}, {2, 3}}},
        PieceShape{{{1, 2}, {2, 2}, {1, 3}, {2, 3}}},
        PieceShape{{{1, 2}, {2, 2}, {1, 3}, {2, 3}}},
        PieceShape{{{1, 2}, {2, 2}, {1, 3}, {2, 3}}},
        PieceShape{{{0, 2}, {1, 2}, {1, 3}, {2, 3}}},
        PieceShape{{{1, 1}, {0, 2}, {1, 2}, {0, 3}}},
        PieceShape{{{0, 2}, {1, 2}, {1, 3}, {2, 3}}},
        PieceShape{{{1, 1}, {0, 2}, {1, 2}, {0, 3}}},
        PieceShape{{{1, 2}, {2, 2}, {0, 3}, {1, 3}}},
        PieceShape{{{0, 1}, {0, 2}, {1, 2}, {1, 3}}},
        PieceShape{{{1, 2}, {2, 2}, {0, 3}, {1, 3}}},
        PieceShape{{{0, 1}, {0, 2}, {1, 2}, {1, 3}}},
        PieceShape{{{0, 2}, {1, 2}, {2, 2}, {1, 3}}},
        PieceShape{{{1, 1}, {1, 2}, {2, 2}, {1, 3}}},
        PieceShape{{{1, 1}, {0, 2}, {1, 2}, {2, 2}}},
        PieceShape{{{1, 1}, {0, 2}, {1, 2}, {1, 3}}},
    }};
    return data;
}

} // namespace tetris::test
