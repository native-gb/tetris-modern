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
        board.set({column, 17}, 1);
    board.set({3, 16}, 2);
    const auto rows = board.full_rows();
    expect(rows.size() == 1 && rows.front() == 17, "full rows are discovered semantically");
    board.remove_rows(rows);
    expect(board.at({3, 17}) == 2, "row collapse moves surviving cells downward");

    board.add_garbage(2, 4, 3);
    expect(board.at({4, 17}) == 0 && board.at({5, 17}) == 3,
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
    expect(piece_from_divider(1) == PieceKind::L, "divider one selects L");
    expect(piece_from_divider(0) == PieceKind::O, "divider zero preserves wraparound behavior");

    const PieceQueue queue = advance_piece_queue(PieceKind::T, PieceKind::T, {1, 2, 3});
    expect(queue.attempts == 3, "history rejection is capped at three samples");
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
    const std::array fixed = {PieceKind::I, PieceKind::O, PieceKind::T};
    game.start({}, startup_random(), fixed);
    for (int column = 0; column < board_width; ++column) {
        if (column < 3 || column > 6)
            game.edit_board().set({column, 17}, 2);
    }
    game.place_piece_for_test({.kind = PieceKind::I, .origin = {3, 15}});
    game.tick({.buttons = {.down = true}, .random = {}});
    game.tick({});
    expect(game.state() == PlayState::flashing_lines, "a complete row enters line animation");
    expect(game.lines() == 1, "Type A counts cleared rows upward");
    tick_until_falling(game);
    expect(game.score() == 40, "single at level zero awards forty points");
    expect(game.board().at({0, 17}) == 0, "cleared row is removed");
}

void test_type_b_start() {
    using namespace tetris;
    SinglePlayer game;
    game.start({.type = GameType::type_b, .starting_level = 4, .type_b_height = 2},
               startup_random());
    expect(game.lines() == 25, "Type B begins with twenty-five remaining lines");
    expect(game.board().at({0, 14}) != 0, "Type B height two creates four garbage rows");
    expect(game.board().at({0, 13}) == 0, "garbage height does not spill upward");
}

} // namespace

int main() {
    test_board();
    test_piece_geometry();
    test_rules_and_randomizer();
    test_single_player_clear();
    test_type_b_start();
    if (failures != 0)
        return 1;
    std::puts("modern Native Tetris game tests passed");
    return 0;
}
