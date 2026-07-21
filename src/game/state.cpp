#include "game/state.hpp"

#include <algorithm>
#include <array>

namespace tetris {
namespace {

constexpr int rocket_exhaust_x = 84;
constexpr int buran_exhaust_x = 76;

} // namespace

void start_game(GameState& state, GameResources source) {
    // Reset shared resources and screen input history.
    state.resources = source;
    state.screen = Screen::title;
    state.previous_one = {};
    state.previous_two = {};

    // Reset title and menu selections.
    state.selected_type = GameType::type_a;
    state.selected_music = Music::a;
    state.type_a_level = 0;
    state.type_b_level = 0;
    state.type_b_height = 0;
    state.heart_mode = false;
    state.timer = title_interval_steps;
    state.title_intervals = 19;
    state.next_demo_type_b = false;
    state.demo_type_b = false;

    // Reset game-over and victory rendering.
    state.ending_stage = EndingStage::none;
    state.ending_elapsed = 0;
    state.rocket = Rocket::small;
    state.launch_y = 0;
    state.exhaust_y = 0;
    state.congratulations_characters = 0;
    state.scoreboard = {};
    state.scoreboard_remaining = {};
    state.soft_drop_remaining = 0;

    // Reset versus setup and results.
    state.versus_heights = {};
    state.new_match = true;
    state.result_timer = 0;
    state.result_step = 0;
    state.result_elapsed = 0;

    // Reset high-score name entry.
    state.pending_score.reset();
    state.pending_name.clear();
    state.name_cursor = 0;
    state.name_repeat_timer = 0;
    state.name_blink_timer = 0;
    state.name_character_visible = true;
}

void step_game(GameState& state, const GameInput& input) {
    // Clear transient events and derive newly pressed buttons.
    state.single_player.events.clear();
    state.versus.clear_events();
    const Buttons pressed = newly_pressed(input.player_one, state.previous_one);
    const Buttons pressed_two = newly_pressed(input.player_two, state.previous_two);
    state.previous_one = input.player_one;
    state.previous_two = input.player_two;

    // Start + Select + A + B performs the original hard reset chord.
    const bool reset = input.player_one.start && input.player_one.select &&
                       input.player_one.back && input.player_one.confirm;
    if (reset) {
        start_game(state, state.resources);
        return;
    }
    // North and Select double as retry buttons only while a pause/game-over
    // menu owns input. Their normal Hold and Select behavior remains available
    // during active play; a dedicated Restart binding can still retry directly.
    const bool paused_game =
        state.screen == Screen::gameplay && state.single_player.paused;
    const bool retry_menu = paused_game || state.screen == Screen::game_over;
    const bool contextual_retry = retry_menu && (pressed.restart || pressed.hold || pressed.select);
    const bool dedicated_retry = state.screen == Screen::gameplay && !paused_game &&
                                 pressed.restart && !pressed.hold && !pressed.select;
    if (state.gameplay_options.quick_restart && (contextual_retry || dedicated_retry)) {
        begin_game(state, input);
        return;
    }
    const bool leaving_paused_game =
        state.screen == Screen::gameplay && state.single_player.paused && pressed.back;
    const bool leaving_game_over = state.screen == Screen::game_over && pressed.back;
    if (leaving_paused_game || leaving_game_over) {
        return_to_title(state);
        return;
    }

    // Dispatch one step to the owner of the current screen.
    switch (state.screen) {
    case Screen::copyright_fixed:
    case Screen::copyright_skippable:
        step_copyright(state, pressed);
        break;
    case Screen::title:
        step_title(state, input, pressed);
        break;
    case Screen::game_type:
    case Screen::music:
    case Screen::type_a_level:
    case Screen::type_b_level:
    case Screen::type_b_height:
        step_menu(state, input, pressed);
        break;
    case Screen::versus_height:
    case Screen::versus_gameplay:
    case Screen::versus_round_result:
    case Screen::versus_match_result:
        step_versus(state, input, pressed, pressed_two);
        break;
    case Screen::gameplay:
        step_gameplay(state, input);
        break;
    case Screen::demo:
        step_demo(state, input, pressed);
        break;
    case Screen::game_over:
    case Screen::type_b_celebration:
    case Screen::dancers:
    case Screen::buran:
    case Screen::rocket:
    case Screen::scoreboard:
        step_ending(state, pressed);
        break;
    case Screen::name_entry:
        step_name_entry(state, input.player_one, pressed);
        break;
    }
}

void begin_game(GameState& state, const GameInput& input) {
    // Convert menu selections into one explicit session setup.
    const int level =
        state.selected_type == GameType::type_a ? state.type_a_level : state.type_b_level;
    state.single_player.start(
        state.resources.gameplay,
        {.type = state.selected_type,
         .starting_level = level,
         .type_b_height = state.selected_type == GameType::type_b ? state.type_b_height : 0,
         .heart_mode = state.heart_mode},
        input.startup, {}, input.player_one, state.gameplay_options);
    state.single_player.line_clear_speed = state.line_clear_speed;
    state.screen = Screen::gameplay;
}

void step_gameplay(GameState& state, const GameInput& input) {
    state.single_player.step({input.player_one, input.random});
    if (state.single_player.state == PlayState::game_over)
        begin_game_over(state);
    else if (state.single_player.state == PlayState::complete &&
             (state.single_player.rules.type != GameType::type_a ||
              state.single_player.options.challenge == ChallengeMode::marathon))
        begin_type_b_ending(state);
}

void set_line_clear_speed(GameState& state, LineClearSpeed speed) {
    // Keep current and future single-player and versus sessions in sync.
    state.line_clear_speed = speed;
    state.single_player.line_clear_speed = speed;
    state.versus.set_line_clear_speed(speed);
}

void set_gameplay_options(GameState& state, GameplayOptions options) {
    state.gameplay_options = options;
    state.single_player.options = options;
    state.versus.set_gameplay_options(options);
}

int selected_level(const GameState& state) {
    return state.selected_type == GameType::type_a ? state.type_a_level : state.type_b_level;
}
int name_entry_rank(const GameState& state) {
    return state.pending_score ? static_cast<int>(state.pending_score->rank) : -1;
}
int game_over_curtain_rows(const GameState& state) {
    return state.ending_stage == EndingStage::game_over_curtain
               ? std::min(state.ending_elapsed, board_height)
               : 0;
}
int game_over_panel_rows(const GameState& state) {
    if (state.screen != Screen::game_over || state.ending_stage == EndingStage::game_over_curtain)
        return 0;
    return std::min(state.ending_elapsed, board_height);
}
bool launch_smoke_visible(const GameState& state) {
    const bool ignition = state.ending_stage == EndingStage::rocket_ignition ||
                          state.ending_stage == EndingStage::rocket_liftoff_delay ||
                          state.ending_stage == EndingStage::rocket_liftoff ||
                          state.ending_stage == EndingStage::buran_ignition ||
                          state.ending_stage == EndingStage::buran_final_ignition ||
                          state.ending_stage == EndingStage::buran_liftoff;
    return ignition && ((state.ending_elapsed / 10) & 1) == 0;
}
int exhaust_x(const GameState& state) {
    return state.screen == Screen::buran ? buran_exhaust_x : rocket_exhaust_x;
}
int exhaust_animation_frame(const GameState& state) {
    if (state.ending_stage == EndingStage::rocket_rising)
        return ((state.ending_elapsed + 3) / 6) & 1;
    if (state.ending_stage == EndingStage::buran_rising)
        return ((state.ending_elapsed + 5) / 6) & 1;
    return 0;
}
int dancer_frame(const GameState& state, int dancer) {
    constexpr std::array lengths = {28, 15, 30, 50, 32, 24, 38, 29, 40, 43};
    if (dancer < 0 || dancer >= static_cast<int>(lengths.size()))
        return 0;
    return (std::max(state.ending_elapsed - 26, 0) / lengths[static_cast<std::size_t>(dancer)]) & 1;
}
int dancer_vertical_offset(const GameState& state, int dancer) {
    return dancer == 6 && dancer_frame(state, dancer) != 0 ? -10 : 0;
}
bool versus_result_prompt_visible(const GameState& state) {
    if (state.versus.state == MatchState::match_over ||
        (state.screen != Screen::versus_round_result &&
         state.screen != Screen::versus_match_result))
        return false;
    const bool local_victory = state.versus.winner != RoundWinner::player_two;
    return local_victory ? state.result_step > 0 && state.result_step % 2 == 0
                         : state.result_step % 2 == 1;
}

void start_session(GameState& state, GameRules rules, const StartupRandom& random) {
    // Developer tools enter gameplay through the same state fields as the menus.
    state.selected_type = rules.type;
    state.type_a_level = rules.starting_level;
    state.type_b_level = rules.starting_level;
    state.type_b_height = rules.type_b_height;
    state.single_player.start(state.resources.gameplay, rules, random, {}, {},
                              state.gameplay_options);
    state.single_player.line_clear_speed = state.line_clear_speed;
    state.screen = Screen::gameplay;
}

} // namespace tetris
