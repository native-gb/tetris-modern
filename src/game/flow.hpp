#pragma once

#include "game/demo.hpp"
#include "game/high_scores.hpp"
#include "game/multiplayer.hpp"

#include <optional>
#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace tetris {

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

struct FlowResources {
    std::span<const DemoRun> type_a_demo;
    std::span<const DemoRun> type_b_demo;
    std::span<const DemoPiece> demo_pieces;
    std::span<const std::uint8_t> type_b_demo_garbage;
};

struct FlowInput {
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

class GameFlow {
public:
    void start(FlowResources resources);
    void tick(const FlowInput& input);
    void set_line_clear_speed(LineClearSpeed speed);

    Screen screen() const;
    GameType selected_type() const;
    Music selected_music() const;
    int selected_level() const;
    int selected_height() const;
    bool heart_mode() const;
    bool demo_is_type_b() const;
    int timer() const;
    EndingStage ending_stage() const;
    int ending_elapsed() const;
    Rocket rocket() const;
    int launch_y() const;
    int exhaust_y() const;
    int congratulations_characters() const;
    const Scoreboard& scoreboard() const;
    const SinglePlayer& game() const;
    SinglePlayer& edit_game();
    const VersusMatch& versus() const;
    VersusMatch& edit_versus();
    int versus_height(int player) const;
    int versus_result_step() const;
    const HighScores& high_scores() const;
    void set_high_scores(HighScores scores);
    const std::string& pending_name() const;
    int name_cursor() const;
    int name_entry_rank() const;
    bool name_character_visible() const;
    int game_over_curtain_rows() const;
    int game_over_panel_rows() const;
    bool launch_smoke_visible() const;
    int exhaust_x() const;
    int exhaust_animation_frame() const;
    int dancer_frame(int dancer) const;
    int dancer_vertical_offset(int dancer) const;
    int versus_result_elapsed() const;
    bool versus_result_prompt_visible() const;
    LineClearSpeed line_clear_speed() const;

    void start_session(GameRules rules, const StartupRandom& random);

private:
    void tick_copyright(const Buttons& pressed);
    void tick_title(const FlowInput& input, const Buttons& pressed);
    void tick_menu(const FlowInput& input, const Buttons& pressed);
    void tick_gameplay(const FlowInput& input);
    void tick_demo(const FlowInput& input, const Buttons& pressed);
    void tick_versus(const FlowInput& input, const Buttons& pressed,
                     const Buttons& player_two_pressed);
    void tick_ending(const Buttons& pressed);
    void tick_name_entry(const Buttons& held, const Buttons& pressed);
    void begin_game(const FlowInput& input);
    void begin_demo(const FlowInput& input);
    void begin_game_over();
    void begin_type_b_ending();
    void begin_scoreboard();
    void begin_name_entry(Screen after);
    void return_to_title(int intervals = 19);

    FlowResources resources_{};
    Screen screen_{Screen::copyright_fixed};
    Buttons previous_one_{};
    Buttons previous_two_{};
    GameType selected_type_{GameType::type_a};
    Music selected_music_{Music::a};
    int type_a_level_{};
    int type_b_level_{};
    int type_b_height_{};
    bool heart_mode_{};
    int timer_{250};
    int title_intervals_{19};
    bool next_demo_type_b_{};
    bool demo_type_b_{};
    DemoPlayback demo_{};
    SinglePlayer game_{};
    VersusMatch versus_{};
    LineClearSpeed line_clear_speed_{LineClearSpeed::original};
    std::array<int, 2> versus_heights_{};
    bool new_match_{true};
    int result_timer_{};
    int result_step_{};
    int result_elapsed_{};
    EndingStage ending_stage_{EndingStage::none};
    int ending_elapsed_{};
    Rocket rocket_{Rocket::small};
    int launch_y_{};
    int exhaust_y_{};
    int congratulations_characters_{};
    Scoreboard scoreboard_{};
    LineCounts scoreboard_remaining_{};
    std::uint32_t soft_drop_remaining_{};
    HighScores high_scores_{};
    std::optional<ScorePosition> pending_score_{};
    std::string pending_name_{};
    int name_cursor_{};
    int name_repeat_timer_{};
    int name_blink_timer_{};
    bool name_character_visible_{true};
    Screen after_name_entry_{Screen::type_a_level};
};

} // namespace tetris
