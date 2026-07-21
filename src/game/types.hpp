#pragma once

#include <cstdint>

namespace tetris {

constexpr int board_width = 10;
constexpr int board_height = 18;

enum class Block : std::uint8_t {
    empty,
    l,
    j,
    o,
    s,
    z,
    t,
    i_horizontal_first,
    i_horizontal_middle,
    i_horizontal_last,
    i_vertical_first,
    i_vertical_middle,
    i_vertical_last,
    garbage_0,
    garbage_1,
    garbage_2,
    garbage_3,
    garbage_4,
    garbage_5,
    garbage_6,
    garbage_7,
};

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
