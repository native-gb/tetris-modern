#include "game/multiplayer.hpp"
#include "gameplay_fixture.hpp"

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

tetris::VersusRandom round_random() {
    tetris::VersusRandom random;
    for (std::size_t index = 0; index < random.pieces.size(); ++index)
        random.pieces[index] = {static_cast<tetris::PieceKind>(index % 7)};
    for (std::size_t index = 0; index < random.garbage.size(); ++index)
        random.garbage[index] = {1, static_cast<std::uint8_t>(index)};
    return random;
}

void step(tetris::VersusMatch& match, tetris::Buttons one = {}, tetris::Buttons two = {}) {
    match.step({one, {11, 12, 13}}, {two, {11, 12, 13}});
}

void force_clear(tetris::VersusMatch& match, int player, int rows) {
    using namespace tetris;
    SinglePlayer& game = match.players[static_cast<std::size_t>(player)].game;
    for (int row = board_height - rows; row < board_height; ++row) {
        for (int column = 0; column < board_width; ++column) {
            if (column != 6)
                game.board.set({column, row}, Block::j);
        }
    }
    game.debug_place_piece({.kind = PieceKind::I, .rotation = Rotation::right,
                               .origin = {5, board_height - 4}});
    if (player == 0)
        step(match, {.down = true});
    else
        step(match, {}, {.down = true});
    step(match);
}

void advance_to_piece(tetris::VersusMatch& match, int player) {
    int guard = 0;
    while (match.state == tetris::MatchState::playing &&
           match.players[static_cast<std::size_t>(player)].game.state != tetris::PlayState::falling && guard < 200) {
        step(match);
        ++guard;
    }
    expect(guard < 200, "versus clear reaches the next falling piece");
}

void test_sequence_and_round_setup() {
    using namespace tetris;
    std::array<RandomSamples, versus_piece_count + 3> samples{};
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const auto base = static_cast<std::uint8_t>(index * 3U + 1U);
        samples[index] = {base, static_cast<std::uint8_t>(base + 1U),
                          static_cast<std::uint8_t>(base + 2U)};
    }
    const auto pieces = make_versus_piece_sequence(samples);
    for (const PieceSpec piece : pieces) {
        expect(static_cast<int>(piece.kind) < 7 &&
                   piece.rotation == Rotation::spawn,
               "shared sequence contains valid spawn-orientation pieces");
    }

    VersusRandom random = round_random();
    random.pieces.back() = {PieceKind::T};
    VersusMatch match;
    match.start(tetris::test::gameplay_data(), {1, 3}, random);
    expect(match.state == MatchState::playing, "versus starts in play");
    expect(match.players[0].game.level == 1 && match.players[1].game.level == 1 &&
               match.players[0].game.lines == 30 && match.players[1].game.lines == 30,
           "both players receive original level and line targets");
    expect(match.players[0].game.piece.kind == random.pieces[0].kind &&
               match.players[1].game.piece.kind == random.pieces[0].kind &&
               match.players[0].game.preview == random.pieces[1],
           "both players consume the same piece list");
    expect(match.players[0].game.board.at({0, 15}) == Block::empty &&
               match.players[0].game.board.at({0, 16}) != Block::empty &&
               match.players[1].game.board.at({0, 12}) != Block::empty,
           "each player receives garbage for the independently selected height");
    expect(match.garbage_hole == 3,
           "garbage hole uses the original encoded-piece decrement rule");
}

void test_attacks_and_pause() {
    using namespace tetris;
    VersusMatch match;
    match.start(tetris::test::gameplay_data(), {}, round_random());
    force_clear(match, 0, 4);
    expect(match.players[1].pending == 4, "Tetris queues four rows for the opponent");
    expect(match.players[1].game.board.at({match.garbage_hole, 17}) == Block::empty,
           "queued attack waits for the recipient's next piece");

    SinglePlayer& recipient = match.players[1].game;
    recipient.debug_place_piece({.kind = PieceKind::O, .origin = {3, 15}});
    step(match, {}, {.down = true});
    step(match);
    advance_to_piece(match, 1);
    expect(match.players[1].pending == 0, "pending attack is consumed on spawn");
    for (int row = 14; row < 18; ++row) {
        expect(match.players[1].game.board.at({match.garbage_hole, row}) == Block::empty,
               "attack rows share the fixed round hole");
    }

    VersusMatch controls;
    controls.start(tetris::test::gameplay_data(), {}, round_random());
    force_clear(controls, 0, 1);
    expect(controls.players[1].pending == 0, "single sends no garbage");
    step(controls, {}, {.start = true});
    expect(!controls.paused, "player two cannot pause versus");
    step(controls);
    step(controls, {.start = true});
    expect(controls.paused && controls.players[0].game.paused && controls.players[1].game.paused,
           "player one pauses both boards");
    step(controls);
    step(controls, {.start = true});
    expect(!controls.paused, "second player-one Start resumes both boards");
}

void test_results_and_draws() {
    using namespace tetris;
    const VersusRandom random = round_random();
    VersusMatch match;
    match.start(tetris::test::gameplay_data(), {}, random);
    match.players[0].game.debug_set_state(PlayState::complete);
    step(match);
    expect(match.state == MatchState::round_over &&
               match.winner == RoundWinner::player_one && match.wins[0] == 1,
           "completion awards player one a round");
    for (int win = 1; win < wins_for_match; ++win) {
        match.next_round({}, random);
        match.players[1].game.debug_set_state(PlayState::game_over);
        step(match);
    }
    expect(match.state == MatchState::match_over && match.wins[0] == winner_display_wins,
           "four wins complete the match and use the original display sentinel");

    VersusMatch draw;
    draw.start(tetris::test::gameplay_data(), {}, random);
    draw.players[0].game.debug_set_state(PlayState::complete);
    draw.players[1].game.debug_set_state(PlayState::complete);
    step(draw);
    expect(draw.winner == RoundWinner::draw && draw.wins[0] == 0 && draw.wins[1] == 0,
           "simultaneous terminal states are a no-score draw");
}

} // namespace

int main() {
    test_sequence_and_round_setup();
    test_attacks_and_pause();
    test_results_and_draws();
    if (failures != 0)
        return 1;
    std::puts("modern Native Tetris multiplayer tests passed");
    return 0;
}
