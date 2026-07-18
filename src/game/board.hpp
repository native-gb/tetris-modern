#pragma once

#include "game/types.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace tetris {

class Board {
public:
    bool occupied(Cell cell) const;
    Block at(Cell cell) const;
    void set(Cell cell, Block block);
    void clear();

    bool row_is_full(int row) const;
    std::vector<int> full_rows() const;
    void remove_rows(std::span<const int> rows);
    void add_garbage(int row_count, int hole, Block block);

    const std::array<Block, board_width * board_height>& cells() const;

private:
    std::array<Block, board_width * board_height> cells_{};
};

} // namespace tetris
