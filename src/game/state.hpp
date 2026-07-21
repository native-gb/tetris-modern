#pragma once

#include "game/demo.hpp"
#include "game/high_scores.hpp"
#include "game/multiplayer.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace tetris {

inline constexpr int copyright_skippable_steps = 250;
inline constexpr int title_interval_steps = 125;

enum class Screen {
    copyright_fixed,
    copyright_skippable,
    title,
    game_type,
    music,
    type_a_level,
    type_b_level,
    type_b_height,
    versus_height,
    gameplay,
    versus_gameplay,
    versus_round_result,
    versus_match_result,
    demo,
    game_over,
    type_b_celebration,
    dancers,
    buran,
    rocket,
    scoreboard,
    name_entry,
};

enum class Music { a, b, c, off };
enum class Rocket { small, medium, large };

enum class EndingStage {
    none,
    game_over_curtain,
    game_over_wait,
    rocket_bonus_delay,
    rocket_initialize,
    rocket_prelaunch,
    rocket_ignition,
    rocket_liftoff_delay,
    rocket_liftoff,
    rocket_rising,
    rocket_return,
    type_b_victory_delay,
    dancers,
    buran_initialize,
    buran_prelaunch,
    buran_ignition,
    buran_final_ignition,
    buran_liftoff,
    buran_rising,
    congratulations,
    congratulations_wait,
    buran_teardown,
    scoreboard_delay,
    scoreboard_tally,
    scoreboard_wait,
};

struct GameResources {
    GameplayData gameplay;
    std::span<const DemoRun> type_a_demo;
    std::span<const DemoRun> type_b_demo;
    std::span<const DemoPiece> demo_pieces;
    std::span<const std::uint8_t> type_b_demo_garbage;
};

struct GameInput {
    Buttons player_one;
    Buttons player_two;
    RandomSamples random;
    StartupRandom startup;
    VersusRandom versus;
};

struct Scoreboard {
    LineCounts lines;
    std::uint32_t soft_drop{};
    std::uint32_t score{};
    int category{};
};

// Complete deterministic state for the original game. Platform state such as
// the SDL window, renderer, audio device, and developer menus lives elsewhere.
struct GameState {
    // Content used by every new session.
    GameResources resources{};

    // Current screen and edge-triggered input history.
    Screen screen{Screen::title};
    Buttons previous_one{};
    Buttons previous_two{};

    // Title screen and menu selections.
    GameType selected_type{GameType::type_a};
    Music selected_music{Music::a};
    int type_a_level{};
    int type_b_level{};
    int type_b_height{};
    bool heart_mode{};
    int timer{title_interval_steps};
    int title_intervals{19};
    bool next_demo_type_b{};
    bool demo_type_b{};

    // Active simulations.
    DemoPlayback demo{};
    SinglePlayer single_player{};
    VersusMatch versus{};
    LineClearSpeed line_clear_speed{LineClearSpeed::original};
    GameplayOptions gameplay_options{};

    // Two-player setup and result screens.
    std::array<int, 2> versus_heights{};
    bool new_match{true};
    int result_timer{};
    int result_step{};
    int result_elapsed{};

    // Game-over and victory sequences.
    EndingStage ending_stage{EndingStage::none};
    int ending_elapsed{};
    Rocket rocket{Rocket::small};
    int launch_y{};
    int exhaust_y{};
    int congratulations_characters{};

    // Score tally and persistent high-score entry.
    Scoreboard scoreboard{};
    LineCounts scoreboard_remaining{};
    std::uint32_t soft_drop_remaining{};
    HighScores high_scores{};
    std::optional<ScorePosition> pending_score{};
    std::string pending_name{};
    int name_cursor{};
    int name_repeat_timer{};
    int name_blink_timer{};
    bool name_character_visible{true};
    Screen after_name_entry{Screen::type_a_level};
};

// Main state transitions.
void start_game(GameState& state, GameResources resources);
void step_game(GameState& state, const GameInput& input);
void start_session(GameState& state, GameRules rules, const StartupRandom& random);
void set_line_clear_speed(GameState& state, LineClearSpeed speed);
void set_gameplay_options(GameState& state, GameplayOptions options);

// Screen-specific state transitions. These are public so focused tests and
// developer tools can enter the same paths as ordinary play.
void step_copyright(GameState& state, const Buttons& pressed);
void step_title(GameState& state, const GameInput& input, const Buttons& pressed);
void step_menu(GameState& state, const GameInput& input, const Buttons& pressed);
void step_gameplay(GameState& state, const GameInput& input);
void step_demo(GameState& state, const GameInput& input, const Buttons& pressed);
void step_versus(GameState& state, const GameInput& input, const Buttons& pressed,
                 const Buttons& player_two_pressed);
void step_ending(GameState& state, const Buttons& pressed);
void step_name_entry(GameState& state, const Buttons& held, const Buttons& pressed);
void begin_game(GameState& state, const GameInput& input);
void begin_demo(GameState& state, const GameInput& input);
void begin_game_over(GameState& state);
void begin_type_b_ending(GameState& state);
void begin_scoreboard(GameState& state);
void begin_name_entry(GameState& state, Screen after);
void return_to_title(GameState& state, int intervals = 19);

// Values derived from state for rendering and developer tools.
int selected_level(const GameState& state);
int name_entry_rank(const GameState& state);
int game_over_curtain_rows(const GameState& state);
int game_over_panel_rows(const GameState& state);
bool launch_smoke_visible(const GameState& state);
int exhaust_x(const GameState& state);
int exhaust_animation_frame(const GameState& state);
int dancer_frame(const GameState& state, int dancer);
int dancer_vertical_offset(const GameState& state, int dancer);
bool versus_result_prompt_visible(const GameState& state);

} // namespace tetris
