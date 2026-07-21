#include "game/multiplayer.hpp"
#include "game/state.hpp"

#include <algorithm>
#include <cassert>

namespace tetris {
namespace {

bool has_event(const SinglePlayer& game, GameEvent type) {
    return std::ranges::any_of(game.events, [type](Event event) { return event.type == type; });
}

int cleared_lines(const SinglePlayer& game) {
    for (const Event event : game.events) {
        if (event.type == GameEvent::cleared_lines)
            return event.value;
    }
    return 0;
}

int attack_for(int lines) {
    if (lines == 2)
        return 1;
    if (lines == 3)
        return 2;
    if (lines == 4)
        return 4;
    return 0;
}

bool terminal(const SinglePlayer& game) {
    return game.state == PlayState::complete || game.state == PlayState::game_over;
}

bool confirm(const Buttons& pressed) {
    return pressed.confirm || pressed.start;
}

void move_height_cursor(int& value, const Buttons& pressed) {
    constexpr int columns = 3;
    constexpr int maximum = 5;
    if (pressed.left)
        value = std::max(value - 1, 0);
    if (pressed.right)
        value = std::min(value + 1, maximum);
    if (pressed.up && value >= columns)
        value -= columns;
    if (pressed.down && value + columns <= maximum)
        value += columns;
}

} // namespace

std::array<PieceSpec, versus_piece_count>
make_versus_piece_sequence(std::span<const RandomSamples> samples) {
    assert(samples.size() >= static_cast<std::size_t>(versus_piece_count + 3));
    std::array<PieceSpec, versus_piece_count> pieces{};
    PieceSpec preview{};
    PieceSpec hidden{};

    // Discard the same three queue warm-up results as the original game.
    for (std::size_t index = 0; index < static_cast<std::size_t>(versus_piece_count + 3); ++index) {
        const PieceQueue result = advance_piece_queue(preview, hidden, samples[index]);
        preview = result.preview;
        hidden = result.hidden;
        if (index >= 3)
            pieces[index - 3] = preview;
    }
    return pieces;
}

void VersusMatch::start(const GameplayData& source, VersusSetup setup, const VersusRandom& random) {
    data = source;
    wins = {};
    begin_round(setup, random);
}

void VersusMatch::next_round(VersusSetup setup, const VersusRandom& random) {
    assert(state == MatchState::round_over);
    begin_round(setup, random);
}

void VersusMatch::begin_round(VersusSetup setup, const VersusRandom& random) {
    // Start both boards with shared garbage and the same fixed piece sequence.
    const std::array heights = {setup.player_one_height, setup.player_two_height};
    StartupRandom startup;
    startup.garbage = random.garbage;
    for (int index = 0; index < 2; ++index) {
        VersusPlayer& player = players[static_cast<std::size_t>(index)];
        player.game.start(data,
                          {.type = GameType::versus,
                           .starting_level = 1,
                           .type_b_height = heights[static_cast<std::size_t>(index)]},
                          startup, random.pieces, {}, options);
        player.game.line_clear_speed = clear_speed;
        player.pending = 0;
        player.queued = 0;
    }

    // Derive the shared garbage hole from the final generated piece.
    const PieceSpec last = random.pieces.back();
    const int piece_code = static_cast<int>(last.kind) * 4 + static_cast<int>(last.rotation);
    const int decrements = piece_code == 0 ? 256 : piece_code;
    garbage_hole = (decrements - 1) % board_width;
    state = MatchState::playing;
    winner = RoundWinner::none;
    paused = false;
    previous_start = false;
}

void VersusMatch::step(StepInput player_one, StepInput player_two) {
    if (state != MatchState::playing)
        return;
    // Player one owns the shared pause toggle.
    const bool start_pressed = player_one.buttons.start && !previous_start;
    previous_start = player_one.buttons.start;
    player_one.buttons.start = false;
    player_two.buttons.start = false;
    if (start_pressed) {
        paused = !paused;
        players[0].game.set_paused(paused);
        players[1].game.set_paused(paused);
    }
    players[0].game.step(player_one);
    players[1].game.step(player_two);

    // Apply pending garbage at spawn, then queue attacks from new clears.
    if (has_event(players[0].game, GameEvent::spawned))
        apply_pending(players[0]);
    if (has_event(players[1].game, GameEvent::spawned))
        apply_pending(players[1]);
    queue(players[1], attack_for(cleared_lines(players[0].game)));
    queue(players[0], attack_for(cleared_lines(players[1].game)));

    if (!terminal(players[0].game) && !terminal(players[1].game))
        return;

    // Resolve simultaneous completion/loss and single-player wins.
    const bool one_complete = players[0].game.state == PlayState::complete;
    const bool two_complete = players[1].game.state == PlayState::complete;
    const bool one_lost = players[0].game.state == PlayState::game_over;
    const bool two_lost = players[1].game.state == PlayState::game_over;
    if ((one_complete && two_complete) || (one_lost && two_lost))
        finish(RoundWinner::draw);
    else if (one_complete || two_lost)
        finish(RoundWinner::player_one);
    else
        finish(RoundWinner::player_two);
}

void VersusMatch::apply_pending(VersusPlayer& player) {
    if (player.pending == 0)
        return;
    player.game.add_garbage(player.pending, garbage_hole);
    player.pending = player.queued;
    player.queued = 0;
}

void VersusMatch::queue(VersusPlayer& player, int rows) {
    if (rows == 0)
        return;
    if (player.pending == 0)
        player.pending = rows;
    else
        player.queued = rows;
}

void VersusMatch::finish(RoundWinner result) {
    // Record the round result and promote the match when either side reaches four
    // wins.
    winner = result;
    if (result == RoundWinner::player_one)
        ++wins[0];
    if (result == RoundWinner::player_two)
        ++wins[1];
    if (wins[0] >= wins_for_match || wins[1] >= wins_for_match) {
        state = MatchState::match_over;
        if (wins[0] >= wins_for_match)
            wins[0] = winner_display_wins;
        if (wins[1] >= wins_for_match)
            wins[1] = winner_display_wins;
    } else {
        state = MatchState::round_over;
    }
}

void VersusMatch::set_line_clear_speed(LineClearSpeed speed) {
    clear_speed = speed;
    players[0].game.line_clear_speed = speed;
    players[1].game.line_clear_speed = speed;
}
void VersusMatch::set_gameplay_options(GameplayOptions gameplay) {
    options = gameplay;
    players[0].game.options = gameplay;
    players[1].game.options = gameplay;
}
void VersusMatch::clear_events() {
    players[0].game.events.clear();
    players[1].game.events.clear();
}
int VersusMatch::stack_height(int player_index) const {
    const Board& board = players[static_cast<std::size_t>(player_index)].game.board;
    for (int row = 0; row < board_height; ++row) {
        for (int column = 0; column < board_width; ++column) {
            if (board.at({column, row}) != Block::empty)
                return board_height - row;
        }
    }
    return 0;
}

void step_versus(GameState& state, const GameInput& input, const Buttons& pressed,
                 const Buttons& player_two_pressed) {
    // Height selection starts a new match or the next round.
    if (state.screen == Screen::versus_height) {
        move_height_cursor(state.versus_heights[0], pressed);
        move_height_cursor(state.versus_heights[1], player_two_pressed);
        if (pressed.back) {
            state.screen = Screen::game_type;
            state.new_match = true;
        } else if (confirm(pressed)) {
            const VersusSetup setup{state.versus_heights[0], state.versus_heights[1]};
            if (state.new_match) {
                state.versus.start(state.resources.gameplay, setup, input.versus);
                state.new_match = false;
            } else {
                state.versus.next_round(setup, input.versus);
            }
            state.versus.set_line_clear_speed(state.line_clear_speed);
            state.screen = Screen::versus_gameplay;
            state.result_timer = 0;
            state.result_step = 0;
            state.result_elapsed = 0;
        }
        return;
    }

    // Active play delays briefly before revealing its result screen.
    if (state.screen == Screen::versus_gameplay) {
        if (state.result_timer > 0) {
            --state.result_timer;
            ++state.result_elapsed;
            if (state.result_timer == 0) {
                state.screen = state.versus.state == MatchState::match_over
                                   ? Screen::versus_match_result
                                   : Screen::versus_round_result;
                state.result_timer = 25;
                state.result_step = 0;
                state.result_elapsed = 0;
            }
            return;
        }

        state.versus.step({input.player_one, input.random}, {input.player_two, input.random});
        if (state.versus.state != MatchState::playing)
            state.result_timer = 40;
        return;
    }

    // Start leaves the round result and returns to height selection.
    if (state.screen == Screen::versus_round_result && pressed.start) {
        state.screen = Screen::versus_height;
        return;
    }

    // Match-result animation eventually starts a fresh match.
    ++state.result_elapsed;
    if (--state.result_timer > 0)
        return;
    state.result_timer = 25;
    ++state.result_step;
    if (state.screen == Screen::versus_match_result && state.result_step == 29) {
        state.new_match = true;
        state.screen = Screen::versus_height;
    }
}

} // namespace tetris
