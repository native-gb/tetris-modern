#include "game/piece.hpp"

#include "game/board.hpp"

namespace tetris {
namespace {

std::size_t index_of(PieceKind kind) {
    return static_cast<std::size_t>(kind);
}

std::size_t index_of(Rotation rotation) {
    return static_cast<std::size_t>(rotation);
}

} // namespace

FallingPiece spawn_piece(PieceSpec piece) {
    return {.kind = piece.kind, .rotation = piece.rotation, .origin = {3, -1}};
}

Rotation clockwise(Rotation rotation) {
    const auto value = static_cast<unsigned int>(rotation);
    return static_cast<Rotation>((value + 3U) % 4U);
}

Rotation counterclockwise(Rotation rotation) {
    const auto value = static_cast<unsigned int>(rotation);
    return static_cast<Rotation>((value + 1U) % 4U);
}

PieceShape occupied_cells(const GameplayData& data, const FallingPiece& piece) {
    auto cells = piece_cells(data, {piece.kind, piece.rotation});
    for (Cell& cell : cells) {
        cell.x += piece.origin.x;
        cell.y += piece.origin.y;
    }
    return cells;
}

PieceShape piece_cells(const GameplayData& data, PieceSpec piece) {
    return data.piece_shapes[index_of(piece.kind) * 4U + index_of(piece.rotation)];
}

std::array<Block, 4> blocks_for(const FallingPiece& piece) {
    switch (piece.kind) {
    case PieceKind::L: return {Block::l, Block::l, Block::l, Block::l};
    case PieceKind::J: return {Block::j, Block::j, Block::j, Block::j};
    case PieceKind::O: return {Block::o, Block::o, Block::o, Block::o};
    case PieceKind::S: return {Block::s, Block::s, Block::s, Block::s};
    case PieceKind::Z: return {Block::z, Block::z, Block::z, Block::z};
    case PieceKind::T: return {Block::t, Block::t, Block::t, Block::t};
    case PieceKind::I: {
        const bool vertical = piece.rotation == Rotation::right || piece.rotation == Rotation::left;
        if (vertical) {
            return {Block::i_vertical_first, Block::i_vertical_middle,
                    Block::i_vertical_middle, Block::i_vertical_last};
        }
        return {Block::i_horizontal_first, Block::i_horizontal_middle,
                Block::i_horizontal_middle, Block::i_horizontal_last};
    }
    }
    return {};
}

Block garbage_block(std::uint8_t sample) {
    return static_cast<Block>(static_cast<unsigned int>(Block::garbage_0) + (sample & 7U));
}

} // namespace tetris
