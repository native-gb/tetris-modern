#include "game/multiplayer.hpp"

#include <algorithm>
#include <cassert>

namespace tetris {
namespace {

bool has_event(const SinglePlayer& game, GameEvent type) {
    return std::ranges::any_of(game.events(), [type](Event event) { return event.type == type; });
}

int cleared_lines(const SinglePlayer& game) {
    for (const Event event : game.events()) {
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
    return game.state() == PlayState::complete || game.state() == PlayState::game_over;
}

} // namespace

std::array<PieceSpec, versus_piece_count> make_versus_piece_sequence(
    std::span<const RandomSamples> samples) {
    assert(samples.size() >= static_cast<std::size_t>(versus_piece_count + 3));
    std::array<PieceSpec, versus_piece_count> pieces{};
    PieceSpec preview{};
    PieceSpec hidden{};
    for (std::size_t index = 0; index < static_cast<std::size_t>(versus_piece_count + 3); ++index) {
        const PieceQueue result = advance_piece_queue(preview, hidden, samples[index]);
        preview = result.preview;
        hidden = result.hidden;
        if (index >= 3)
            pieces[index - 3] = preview;
    }
    return pieces;
}

void VersusMatch::start(const GameplayData& data, VersusSetup setup,
                        const VersusRandom& random) {
    data_ = data;
    wins_ = {};
    begin_round(setup, random);
}

void VersusMatch::next_round(VersusSetup setup, const VersusRandom& random) {
    assert(state_ == MatchState::round_over);
    begin_round(setup, random);
}

void VersusMatch::begin_round(VersusSetup setup, const VersusRandom& random) {
    const std::array heights = {setup.player_one_height, setup.player_two_height};
    StartupRandom startup;
    startup.garbage = random.garbage;
    for (int index = 0; index < 2; ++index) {
        Player& player = players_[static_cast<std::size_t>(index)];
        player.game.start(data_, {.type = GameType::versus, .starting_level = 1,
                                  .type_b_height = heights[static_cast<std::size_t>(index)]},
                          startup, random.pieces);
        player.game.set_line_clear_speed(clear_speed_);
        player.pending = 0;
        player.queued = 0;
    }
    const PieceSpec last = random.pieces.back();
    const int piece_code = static_cast<int>(last.kind) * 4 + static_cast<int>(last.rotation);
    const int decrements = piece_code == 0 ? 256 : piece_code;
    garbage_hole_ = (decrements - 1) % board_width;
    state_ = MatchState::playing;
    winner_ = RoundWinner::none;
    paused_ = false;
    previous_start_ = false;
}

void VersusMatch::tick(TickInput player_one, TickInput player_two) {
    if (state_ != MatchState::playing)
        return;
    const bool start_pressed = player_one.buttons.start && !previous_start_;
    previous_start_ = player_one.buttons.start;
    player_one.buttons.start = false;
    player_two.buttons.start = false;
    if (start_pressed) {
        paused_ = !paused_;
        players_[0].game.set_paused(paused_);
        players_[1].game.set_paused(paused_);
    }
    players_[0].game.tick(player_one);
    players_[1].game.tick(player_two);

    if (has_event(players_[0].game, GameEvent::spawned))
        apply_pending(players_[0]);
    if (has_event(players_[1].game, GameEvent::spawned))
        apply_pending(players_[1]);
    queue(players_[1], attack_for(cleared_lines(players_[0].game)));
    queue(players_[0], attack_for(cleared_lines(players_[1].game)));

    if (!terminal(players_[0].game) && !terminal(players_[1].game))
        return;
    const bool one_complete = players_[0].game.state() == PlayState::complete;
    const bool two_complete = players_[1].game.state() == PlayState::complete;
    const bool one_lost = players_[0].game.state() == PlayState::game_over;
    const bool two_lost = players_[1].game.state() == PlayState::game_over;
    if ((one_complete && two_complete) || (one_lost && two_lost))
        finish(RoundWinner::draw);
    else if (one_complete || two_lost)
        finish(RoundWinner::player_one);
    else
        finish(RoundWinner::player_two);
}

void VersusMatch::apply_pending(Player& player) {
    if (player.pending == 0)
        return;
    player.game.add_garbage(player.pending, garbage_hole_);
    player.pending = player.queued;
    player.queued = 0;
}

void VersusMatch::queue(Player& player, int rows) {
    if (rows == 0)
        return;
    if (player.pending == 0)
        player.pending = rows;
    else
        player.queued = rows;
}

void VersusMatch::finish(RoundWinner winner) {
    winner_ = winner;
    if (winner == RoundWinner::player_one)
        ++wins_[0];
    if (winner == RoundWinner::player_two)
        ++wins_[1];
    if (wins_[0] >= wins_for_match || wins_[1] >= wins_for_match) {
        state_ = MatchState::match_over;
        if (wins_[0] >= wins_for_match)
            wins_[0] = winner_display_wins;
        if (wins_[1] >= wins_for_match)
            wins_[1] = winner_display_wins;
    } else {
        state_ = MatchState::round_over;
    }
}

void VersusMatch::set_line_clear_speed(LineClearSpeed speed) {
    clear_speed_ = speed;
    players_[0].game.set_line_clear_speed(speed);
    players_[1].game.set_line_clear_speed(speed);
}
MatchState VersusMatch::state() const { return state_; }
RoundWinner VersusMatch::winner() const { return winner_; }
const SinglePlayer& VersusMatch::player(int index) const {
    return players_[static_cast<std::size_t>(index)].game;
}
SinglePlayer& VersusMatch::edit_player(int index) {
    return players_[static_cast<std::size_t>(index)].game;
}
int VersusMatch::wins(int player) const { return wins_[static_cast<std::size_t>(player)]; }
int VersusMatch::pending_garbage(int player) const {
    return players_[static_cast<std::size_t>(player)].pending;
}
int VersusMatch::garbage_hole() const { return garbage_hole_; }
bool VersusMatch::paused() const { return paused_; }
void VersusMatch::clear_events() {
    players_[0].game.clear_events();
    players_[1].game.clear_events();
}
int VersusMatch::stack_height(int player) const {
    const Board& board = players_[static_cast<std::size_t>(player)].game.board();
    for (int row = 0; row < board_height; ++row) {
        for (int column = 0; column < board_width; ++column) {
            if (board.at({column, row}) != Block::empty)
                return board_height - row;
        }
    }
    return 0;
}
int VersusMatch::queued_garbage(int player) const {
    return players_[static_cast<std::size_t>(player)].queued;
}

} // namespace tetris
