#pragma once

#include "game/board.hpp"
#include "game/input.hpp"
#include "game/piece.hpp"
#include "game/randomizer.hpp"

#include <cstdint>
#include <array>
#include <span>
#include <vector>

namespace tetris {

enum class PlayState {
    falling,
    checking_lines,
    flashing_lines,
    waiting_to_collapse,
    wiping_board,
    game_over,
    complete,
};

enum class LineClearSpeed {
    original,
    fast,
    instant,
};

enum class GameEvent {
    spawned,
    moved,
    rotated,
    landed,
    cleared_lines,
    score_changed,
    level_changed,
    paused,
    preview_changed,
    garbage_applied,
    game_over,
    complete,
};

struct Event {
    GameEvent type{};
    int value{};
};

struct GameRules {
    GameType type{GameType::type_a};
    int starting_level{};
    int type_b_height{};
    bool heart_mode{};
};

struct LineCounts {
    std::uint32_t singles{};
    std::uint32_t doubles{};
    std::uint32_t triples{};
    std::uint32_t tetrises{};
};

struct TickInput {
    Buttons buttons;
    RandomSamples random;
};

struct GarbageRandom {
    std::uint8_t occupancy{};
    std::uint8_t appearance{};
};

struct StartupRandom {
    std::array<RandomSamples, 3> pieces{};
    std::array<GarbageRandom, 100> garbage{};
};

class SinglePlayer {
public:
    void start(GameRules rules, const StartupRandom& random,
               std::span<const PieceKind> fixed_pieces = {});
    void tick(const TickInput& input);

    void set_line_clear_speed(LineClearSpeed speed);
    void set_paused(bool paused);
    void add_garbage(int rows, int hole);

    const Board& board() const;
    Board& edit_board();
    const FallingPiece& piece() const;
    PieceKind preview() const;
    PlayState state() const;
    GameRules rules() const;
    int level() const;
    int lines() const;
    std::uint32_t score() const;
    std::uint32_t soft_drop_points() const;
    const LineCounts& line_counts() const;
    bool paused() const;
    bool preview_visible() const;
    std::span<const int> clearing_rows() const;
    std::span<const Event> events() const;
    int animation_step() const;
    std::uint64_t tick_count() const;

    void place_piece_for_test(FallingPiece piece);
    void set_state_for_test(PlayState state);

private:
    void rotate(const Buttons& pressed);
    void move_horizontally(const Buttons& held, const Buttons& pressed);
    void fall(const Buttons& held, const Buttons& pressed);
    bool try_move(Cell distance);
    bool collides(const FallingPiece& piece) const;
    void land();
    void check_lines();
    void advance_animation(const RandomSamples& random);
    void collapse_lines();
    void spawn(const RandomSamples& random);
    void lose();
    void emit(GameEvent event, int value = 0);

    GameRules rules_{};
    Board board_{};
    FallingPiece piece_{};
    PieceKind preview_{PieceKind::L};
    PieceKind hidden_{PieceKind::L};
    PlayState state_{PlayState::game_over};
    Buttons previous_buttons_{};
    bool paused_{};
    bool preview_visible_{true};
    bool require_fresh_down_press_{};
    int level_{};
    int lines_{};
    std::uint32_t score_{};
    std::uint32_t soft_drop_points_{};
    LineCounts line_counts_{};
    int gravity_frames_{52};
    int fall_timer_{52};
    int horizontal_repeat_timer_{23};
    int soft_drop_timer_{};
    int soft_drop_steps_{};
    int animation_timer_{};
    int animation_step_{};
    std::uint64_t tick_count_{};
    int locks_at_spawn_{};
    bool score_clear_when_wiping_{};
    LineClearSpeed line_clear_speed_{LineClearSpeed::original};
    std::vector<int> clearing_rows_{};
    std::vector<PieceKind> fixed_pieces_{};
    std::size_t next_fixed_piece_{};
    std::vector<Event> events_{};
};

} // namespace tetris
