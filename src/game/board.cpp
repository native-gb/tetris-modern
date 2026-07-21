#include "game/board.hpp"

#include <algorithm>
#include <cassert>

namespace tetris {
namespace {

std::size_t index_of(Cell cell) {
    assert(cell.x >= 0 && cell.x < board_width);
    assert(cell.y >= 0 && cell.y < board_height);
    return static_cast<std::size_t>(cell.y * board_width + cell.x);
}

} // namespace

bool Board::occupied(Cell cell) const {
    // Side and bottom boundaries are solid; cells above the board remain open.
    if (cell.x < 0 || cell.x >= board_width || cell.y >= board_height)
        return true;
    if (cell.y < 0)
        return false;
    return cells[index_of(cell)] != Block::empty;
}

Block Board::at(Cell cell) const {
    return cells[index_of(cell)];
}

void Board::set(Cell cell, Block block) {
    cells[index_of(cell)] = block;
}

void Board::clear() {
    cells.fill(Block::empty);
}

bool Board::row_is_full(int row) const {
    for (int column = 0; column < board_width; ++column) {
        if (at({column, row}) == Block::empty)
            return false;
    }
    return true;
}

std::vector<int> Board::full_rows() const {
    // Preserve top-to-bottom order for deterministic animation and removal.
    std::vector<int> rows;
    for (int row = 0; row < board_height; ++row) {
        if (row_is_full(row))
            rows.push_back(row);
    }
    return rows;
}

void Board::remove_rows(std::span<const int> rows) {
    // Compact every surviving row downward in one bottom-up pass.
    std::array<Block, board_width * board_height> result{};
    int destination = board_height - 1;

    for (int source = board_height - 1; source >= 0; --source) {
        if (std::ranges::find(rows, source) != rows.end())
            continue;
        for (int column = 0; column < board_width; ++column) {
            const auto destination_index =
                static_cast<std::size_t>(destination * board_width + column);
            result[destination_index] = at({column, source});
        }
        --destination;
    }
    cells = result;
}

void Board::add_garbage(int row_count, int hole, Block block) {
    assert(row_count > 0 && row_count < board_height);
    assert(hole >= 0 && hole < board_width);
    assert(block != Block::empty);

    // Shift the surviving board upward by the incoming garbage height.
    std::array<Block, board_width * board_height> result{};
    for (int row = 0; row < board_height - row_count; ++row) {
        for (int column = 0; column < board_width; ++column) {
            const auto destination = static_cast<std::size_t>(row * board_width + column);
            result[destination] = at({column, row + row_count});
        }
    }

    // Fill the exposed bottom rows, leaving one shared hole column.
    for (int row = board_height - row_count; row < board_height; ++row) {
        for (int column = 0; column < board_width; ++column) {
            if (column != hole)
                result[static_cast<std::size_t>(row * board_width + column)] = block;
        }
    }
    cells = result;
}

} // namespace tetris
