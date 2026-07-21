#pragma once

#include "game/single_player.hpp"

#include <array>

namespace tetris {

constexpr int versus_piece_count = 256;
constexpr int wins_for_match = 4;
constexpr int winner_display_wins = 5;

enum class MatchState { playing, round_over, match_over };
enum class RoundWinner { none, player_one, player_two, draw };

struct VersusSetup {
    int player_one_height{};
    int player_two_height{};
};

struct VersusRandom {
    std::array<PieceSpec, versus_piece_count> pieces{};
    std::array<GarbageRandom, 100> garbage{};
};

std::array<PieceSpec, versus_piece_count>
make_versus_piece_sequence(std::span<const RandomSamples> samples);

struct VersusPlayer {
    SinglePlayer game;
    int pending{};
    int queued{};
};

struct VersusMatch {
    std::array<VersusPlayer, 2> players{};
    GameplayData data{};
    MatchState state{MatchState::round_over};
    RoundWinner winner{RoundWinner::none};
    std::array<int, 2> wins{};
    int garbage_hole{};
    bool paused{};
    bool previous_start{};
    LineClearSpeed clear_speed{LineClearSpeed::original};
    GameplayOptions options{};

    void start(const GameplayData& source, VersusSetup setup, const VersusRandom& random);
    void next_round(VersusSetup setup, const VersusRandom& random);
    void step(StepInput player_one, StepInput player_two);
    void set_line_clear_speed(LineClearSpeed speed);
    void set_gameplay_options(GameplayOptions gameplay);

    void clear_events();
    int stack_height(int player_index) const;

    void begin_round(VersusSetup setup, const VersusRandom& random);
    void apply_pending(VersusPlayer& player);
    void queue(VersusPlayer& player, int rows);
    void finish(RoundWinner result);
};

} // namespace tetris
