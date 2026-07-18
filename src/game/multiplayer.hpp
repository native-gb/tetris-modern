#pragma once

#include "game/single_player.hpp"

#include <array>

namespace tetris {

constexpr int versus_piece_count = 256;
constexpr int wins_for_match = 4;

enum class MatchState { playing, round_over, match_over };
enum class RoundWinner { none, player_one, player_two, draw };

struct VersusSetup {
    int player_one_height{};
    int player_two_height{};
};

struct VersusRandom {
    std::array<PieceKind, versus_piece_count> pieces{};
    std::array<GarbageRandom, 100> garbage{};
};

std::array<PieceKind, versus_piece_count> make_versus_piece_sequence(
    std::span<const RandomSamples> samples);

class VersusMatch {
public:
    void start(VersusSetup setup, const VersusRandom& random);
    void next_round(VersusSetup setup, const VersusRandom& random);
    void tick(TickInput player_one, TickInput player_two);
    void set_line_clear_speed(LineClearSpeed speed);

    MatchState state() const;
    RoundWinner winner() const;
    const SinglePlayer& player(int index) const;
    SinglePlayer& edit_player(int index);
    int wins(int player) const;
    int pending_garbage(int player) const;
    int garbage_hole() const;
    bool paused() const;
    void clear_events();
    int stack_height(int player) const;
    int queued_garbage(int player) const;

private:
    struct Player {
        SinglePlayer game;
        int pending{};
        int queued{};
    };

    void begin_round(VersusSetup setup, const VersusRandom& random);
    void apply_pending(Player& player);
    void queue(Player& player, int rows);
    void finish(RoundWinner winner);

    std::array<Player, 2> players_{};
    MatchState state_{MatchState::round_over};
    RoundWinner winner_{RoundWinner::none};
    std::array<int, 2> wins_{};
    int garbage_hole_{};
    bool paused_{};
    bool previous_start_{};
    LineClearSpeed clear_speed_{LineClearSpeed::original};
};

} // namespace tetris
