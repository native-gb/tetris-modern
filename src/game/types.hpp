#pragma once

#include <cstdint>

namespace tetris {

constexpr int board_width = 10;
constexpr int board_height = 18;
using Block = std::uint8_t;

struct Cell {
    int x{};
    int y{};

    bool operator==(const Cell&) const = default;
};

enum class PieceKind : std::uint8_t {
    L,
    J,
    I,
    O,
    S,
    Z,
    T,
};

enum class Rotation : std::uint8_t {
    spawn,
    right,
    reverse,
    left,
};

enum class GameType {
    type_a,
    type_b,
    versus,
};

} // namespace tetris
