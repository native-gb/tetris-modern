#pragma once

#include "game/board.hpp"
#include "game/input.hpp"
#include "game/options.hpp"
#include "game/piece.hpp"
#include "game/randomizer.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace tetris {

enum class PlayState {
    falling,
    locked,
    resolving,
    clearing,
    collapse_pending,
    wiping,
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

    bool operator==(const GameRules&) const = default;
};

struct LineCounts {
    std::uint32_t singles{};
    std::uint32_t doubles{};
    std::uint32_t triples{};
    std::uint32_t tetrises{};
};

struct PlayStatistics {
    std::uint32_t pieces{};
    std::uint32_t holds{};
    std::uint32_t hard_drop_cells{};
    std::uint32_t t_spins{};
    int combo{-1};
    int maximum_combo{-1};
    bool back_to_back{};
};

struct StepInput {
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

struct SinglePlayer {
    // Rules and board state.
    GameplayData data{};
    GameRules rules{};
    GameplayOptions options{};
    Board board{};
    Board wipe_board{};

    // Current and upcoming pieces.
    FallingPiece piece{};
    PieceSpec preview{};
    PieceSpec hidden{};
    std::optional<PieceSpec> held_piece{};
    std::vector<PieceSpec> upcoming{};

    // Input and top-level play state.
    PlayState state{PlayState::game_over};
    Buttons previous_buttons{};
    Buttons pressed_buttons{};
    bool paused{};
    bool preview_visible{true};
    bool require_fresh_down_press{};
    bool hold_used{};
    bool grounded{};
    bool last_action_rotation{};
    Buttons spawn_buffer{};

    // Score and progress.
    int level{};
    int lines{};
    std::uint32_t score{};
    std::uint32_t soft_drop_points{};
    std::uint32_t pending_score_award{};
    LineCounts line_counts{};
    PlayStatistics statistics{};
    int time_remaining_steps{};

    // Frame timers and animation progress.
    int gravity_frames{52};
    int fall_timer{52};
    int horizontal_repeat_timer{23};
    int delay_timer{};
    int soft_drop_timer{};
    int soft_drop_steps{};
    int line_animation_step{};
    int wipe_step{};
    int lock_timer{};
    int lock_resets{};
    std::uint64_t step_count{};

    // Randomizer diagnostics and compatibility behavior.
    int locks_at_spawn{};
    RandomSamples last_random_samples{};
    int last_random_attempts{};
    std::uint32_t modern_random_state{};
    std::array<PieceKind, 7> random_bag{};
    int random_bag_position{7};
    std::array<PieceKind, 4> random_history{};
    bool score_clear_when_wiping{};
    LineClearSpeed line_clear_speed{LineClearSpeed::original};

    // Variable-length simulation data.
    std::vector<int> clearing_rows{};
    std::vector<PieceSpec> fixed_pieces{};
    std::size_t next_fixed_piece{};
    std::vector<Event> events{};

    // Session lifecycle.
    void start(const GameplayData& source_data, GameRules setup, const StartupRandom& random,
               std::span<const PieceSpec> fixed_sequence = {}, Buttons initial_buttons = {},
               GameplayOptions gameplay = {});
    void step(const StepInput& input);

    void set_paused(bool value);
    void add_garbage(int rows, int hole);

    const Board& visible_board() const;
    int animation_step() const;
    FallingPiece landing_piece() const;
    PieceSpec next_piece(int index) const;

    // Developer controls used by the F1 tools and focused tests.
    void debug_place_piece(FallingPiece value);
    void debug_set_state(PlayState value);
    void debug_set_score(std::uint32_t value);
    void debug_set_results(LineCounts counts, std::uint32_t points);

    // One simulation step.
    void rotate(const Buttons& pressed);
    void move_horizontally(const Buttons& held, const Buttons& pressed);
    void fall(const Buttons& held, const Buttons& pressed);
    void hold(const RandomSamples& random);
    bool try_move(Cell distance);
    bool collides(const FallingPiece& candidate) const;
    void land();
    void detect_lines();
    void advance_timers();
    void advance_animation(const RandomSamples& random);
    void advance_wipe(const RandomSamples& random);
    void collapse_lines();
    void spawn(const RandomSamples& random);
    void fill_modern_queue();
    PieceSpec generate_modern_piece();
    std::uint32_t next_random();
    void reset_lock_delay();
    void begin_lock_delay();
    bool t_spin() const;
    void score_clear(int cleared);
    void update_level();
    void lose();
    void emit(GameEvent event, int value = 0);
};

} // namespace tetris
