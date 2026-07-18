#include "game/board.hpp"
#include "game/piece.hpp"
#include "game/randomizer.hpp"
#include "game/rules.hpp"
#include "game/single_player.hpp"

#include <array>
#include <cstdio>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (condition)
        return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

tetris::StartupRandom startup_random() {
    tetris::StartupRandom random;
    random.pieces = {{{2, 3, 4}, {5, 6, 7}, {8, 9, 10}}};
    for (std::size_t index = 0; index < random.garbage.size(); ++index) {
        random.garbage[index].occupancy = 1;
        random.garbage[index].appearance = static_cast<std::uint8_t>(index);
    }
    return random;
}

void test_board() {
    tetris::Board board;
    expect(board.occupied({-1, 4}), "left wall is solid");
    expect(board.occupied({10, 4}), "right wall is solid");
    expect(board.occupied({4, 18}), "floor is solid");
    expect(!board.occupied({4, -2}), "space above the board remains available");

    for (int column = 0; column < tetris::board_width; ++column)
        board.set({column, 17}, tetris::Block::l);
    board.set({3, 16}, tetris::Block::j);
    const auto rows = board.full_rows();
    expect(rows.size() == 1 && rows.front() == 17, "full rows are discovered semantically");
    board.remove_rows(rows);
    expect(board.at({3, 17}) == tetris::Block::j, "row collapse moves surviving cells downward");

    board.add_garbage(2, 4, tetris::Block::garbage_3);
    expect(board.at({4, 17}) == tetris::Block::empty &&
           board.at({5, 17}) == tetris::Block::garbage_3,
           "garbage has one explicit hole");
}

void test_piece_geometry() {
    using namespace tetris;
    const FallingPiece horizontal_i{.kind = PieceKind::I, .origin = {3, -1}};
    const auto cells = occupied_cells(horizontal_i);
    expect(cells == std::array<Cell, 4>{{{3, 1}, {4, 1}, {5, 1}, {6, 1}}},
           "I spawn geometry matches the original playfield coordinates");
    expect(clockwise(Rotation::spawn) == Rotation::left,
           "clockwise rotation follows the original orientation ordering");
    expect(counterclockwise(Rotation::spawn) == Rotation::right,
           "counterclockwise rotation follows the original orientation ordering");
}

void test_rules_and_randomizer() {
    using namespace tetris;
    expect(frames_per_drop(0, false) == 52 && frames_per_drop(9, false) == 10,
           "gravity table retains original low-level timings");
    expect(frames_per_drop(0, true) == 9, "heart mode applies the ten-level speed offset");
    expect(line_clear_score(4, 9) == 12'000, "Tetris scoring includes level multiplier");
    for (unsigned int value = 0; value <= 255; ++value) {
        std::uint8_t counter = static_cast<std::uint8_t>(value);
        unsigned int assembly_piece = 0;
        while (true) {
            --counter;
            if (counter == 0)
                break;
            assembly_piece = (assembly_piece + 1U) % 7U;
        }
        expect(piece_from_divider(static_cast<std::uint8_t>(value)) ==
                   static_cast<PieceKind>(assembly_piece),
               "closed-form divider mapping matches the assembly loop");
    }

    const PieceQueue accepted = advance_piece_queue({PieceKind::L}, {PieceKind::L}, {1, 2, 3});
    expect(accepted.attempts == 2 && accepted.hidden.kind == PieceKind::J,
           "history rejection accepts the second distinct candidate");
    const PieceQueue forced = advance_piece_queue({PieceKind::J}, {PieceKind::J}, {1, 2, 3});
    expect(forced.attempts == 3 && forced.hidden.kind == PieceKind::I,
           "history rejection is capped at three samples");
    const PieceQueue oriented = advance_piece_queue(
        {PieceKind::J, Rotation::left}, {PieceKind::I, Rotation::reverse}, {2, 0, 0});
    expect(oriented.active == PieceSpec{PieceKind::J, Rotation::left} &&
               oriented.preview == PieceSpec{PieceKind::I, Rotation::reverse},
           "queued pieces preserve their semantic orientation");
}

void test_every_fixed_piece_orientation() {
    using namespace tetris;
    for (int kind = 0; kind < 7; ++kind) {
        for (int rotation = 0; rotation < 4; ++rotation) {
            const PieceSpec expected{static_cast<PieceKind>(kind),
                                     static_cast<Rotation>(rotation)};
            const std::array fixed = {PieceSpec{}, expected, PieceSpec{}};
            SinglePlayer game;
            game.start({}, startup_random(), fixed);
            expect(game.piece().kind == expected.kind &&
                       game.piece().rotation == expected.rotation &&
                       game.piece().origin == Cell{3, -1},
                   "all 28 fixed piece handoffs preserve orientation and spawn origin");
        }
    }
}

void tick_until_falling(tetris::SinglePlayer& game) {
    int guard = 0;
    while (game.state() != tetris::PlayState::falling &&
           game.state() != tetris::PlayState::game_over && guard < 200) {
        game.tick({});
        ++guard;
    }
    expect(guard < 200, "line resolution terminates");
}

void test_single_player_clear() {
    using namespace tetris;
    SinglePlayer game;
    const std::array fixed = {PieceSpec{PieceKind::I}, PieceSpec{PieceKind::O},
                              PieceSpec{PieceKind::T}};
    game.start({}, startup_random(), fixed);
    for (int column = 0; column < board_width; ++column) {
        if (column < 3 || column > 6)
            game.edit_board().set({column, 17}, Block::j);
    }
    game.place_piece_for_test({.kind = PieceKind::I, .origin = {3, 15}});
    game.tick({.buttons = {.down = true}, .random = {}});
    game.tick({});
    expect(game.state() == PlayState::resolving, "a complete row enters resolution delay");
    expect(game.lines() == 1, "Type A counts cleared rows upward");
    tick_until_falling(game);
    expect(game.score() == 40, "single at level zero awards forty points");
    expect(game.board().at({0, 17}) == Block::empty, "cleared row is removed");
}

void test_type_b_start() {
    using namespace tetris;
    SinglePlayer game;
    game.start({.type = GameType::type_b, .starting_level = 4, .type_b_height = 2},
               startup_random());
    expect(game.lines() == 25, "Type B begins with twenty-five remaining lines");
    expect(game.board().at({0, 14}) != Block::empty, "Type B height two creates four garbage rows");
    expect(game.board().at({0, 13}) == Block::empty, "garbage height does not spill upward");
}

} // namespace

int main() {
    test_board();
    test_piece_geometry();
    test_rules_and_randomizer();
    test_every_fixed_piece_orientation();
    test_single_player_clear();
    test_type_b_start();
    if (failures != 0)
        return 1;
    std::puts("modern Native Tetris game tests passed");
    return 0;
}
