#include "game/state.hpp"

#include "game/rules.hpp"

#include <algorithm>
#include <array>

namespace tetris {
namespace {

constexpr int game_over_curtain_frames = 70;
constexpr int rocket_bonus_delay_frames = 144;
constexpr int initialize_frames = 6;
constexpr int preflight_frames = 186;
constexpr int rocket_ignition_frames = 160;
constexpr int rocket_liftoff_delay_frames = 128;
constexpr int buran_ignition_frames = 255;
constexpr int type_b_delay_frames = 100;
constexpr int scoreboard_delay_frames = 128;
constexpr int scoreboard_category_frames = 34;
constexpr int movement_frames = 10;
constexpr int congratulations_character_frames = 6;
constexpr int congratulations_wait_frames = 255;
constexpr std::array<int, 6> dancer_frames = {241, 241, 241, 241, 385, 769};
constexpr int rocket_start_y = 111;
constexpr int rocket_liftoff_y = 106;
constexpr int rocket_exhaust_y = 122;
constexpr int rocket_exit_y = -32;
constexpr int buran_start_y = 95;
constexpr int buran_liftoff_y = 88;
constexpr int buran_exhaust_y = 120;
constexpr int buran_exit_y = -48;

std::uint32_t& line_count(LineCounts& counts, int category) {
    if (category == 0)
        return counts.singles;
    if (category == 1)
        return counts.doubles;
    if (category == 2)
        return counts.triples;
    return counts.tetrises;
}

bool confirm(const Buttons& pressed) {
    return pressed.confirm || pressed.start;
}

} // namespace

void begin_game_over(GameState& state) {
    state.screen = Screen::game_over;
    state.ending_stage = EndingStage::game_over_curtain;
    state.ending_elapsed = 0;
    state.timer = game_over_curtain_frames;
}

void begin_type_b_ending(GameState& state) {
    state.screen = Screen::type_b_celebration;
    state.ending_stage = EndingStage::type_b_victory_delay;
    state.ending_elapsed = 0;
    state.timer = type_b_delay_frames;
}

void step_ending(GameState& state, const Buttons& pressed) {
    // Final wait screens remain until the player confirms name entry.
    if (state.ending_stage == EndingStage::game_over_wait) {
        ++state.ending_elapsed;
        if (confirm(pressed)) {
            begin_name_entry(state, state.selected_type == GameType::type_a ? Screen::type_a_level
                                                                            : Screen::type_b_level);
        }
        return;
    }
    if (state.ending_stage == EndingStage::scoreboard_wait) {
        ++state.ending_elapsed;
        if (confirm(pressed))
            begin_name_entry(state, Screen::type_b_level);
        return;
    }

    // Every other ending stage advances when its countdown reaches zero.
    ++state.ending_elapsed;
    if (state.timer > 0)
        --state.timer;
    if (state.timer != 0)
        return;

    switch (state.ending_stage) {
    // High Type-A scores replace the ordinary game-over wait with a rocket
    // launch.
    case EndingStage::game_over_curtain:
        if (state.single_player.rules.type != GameType::type_a ||
            state.single_player.score < 100'000) {
            state.ending_stage = EndingStage::game_over_wait;
            state.ending_elapsed = 0;
        } else {
            state.rocket = state.single_player.score >= 200'000   ? Rocket::large
                           : state.single_player.score >= 150'000 ? Rocket::medium
                                                                  : Rocket::small;
            state.ending_stage = EndingStage::rocket_bonus_delay;
            state.timer = rocket_bonus_delay_frames;
            state.ending_elapsed = 0;
        }
        break;

    // Type-A rocket setup and preflight.
    case EndingStage::rocket_bonus_delay:
        state.screen = Screen::rocket;
        state.ending_stage = EndingStage::rocket_initialize;
        state.launch_y = rocket_start_y;
        state.exhaust_y = 0;
        state.timer = initialize_frames;
        state.ending_elapsed = 0;
        break;
    case EndingStage::rocket_initialize:
        state.ending_stage = EndingStage::rocket_prelaunch;
        state.timer = preflight_frames;
        state.ending_elapsed = 0;
        break;
    case EndingStage::rocket_prelaunch:
        state.ending_stage = EndingStage::rocket_ignition;
        state.timer = rocket_ignition_frames;
        state.ending_elapsed = 0;
        break;
    case EndingStage::rocket_ignition:
        state.ending_stage = EndingStage::rocket_liftoff_delay;
        state.timer = rocket_liftoff_delay_frames;
        state.ending_elapsed = 0;
        break;

    // Type-A rocket movement and return to high-score entry.
    case EndingStage::rocket_liftoff_delay:
        state.ending_stage = EndingStage::rocket_liftoff;
        state.ending_elapsed = 0;
        [[fallthrough]];
    case EndingStage::rocket_liftoff:
        --state.launch_y;
        state.timer = movement_frames;
        if (state.launch_y == rocket_liftoff_y) {
            state.exhaust_y = rocket_exhaust_y;
            state.ending_stage = EndingStage::rocket_rising;
            state.ending_elapsed = 0;
        }
        break;
    case EndingStage::rocket_rising:
        --state.launch_y;
        --state.exhaust_y;
        state.timer = movement_frames;
        if (state.launch_y == rocket_exit_y) {
            state.ending_stage = EndingStage::rocket_return;
            state.timer = 5;
            state.ending_elapsed = 0;
        }
        break;
    case EndingStage::rocket_return:
        state.ending_stage = EndingStage::none;
        begin_name_entry(state, Screen::type_a_level);
        break;

    // Type-B victory selects a scoreboard, dancers, or the Buran launch.
    case EndingStage::type_b_victory_delay:
        if (state.single_player.rules.starting_level != 9) {
            begin_scoreboard(state);
        } else {
            state.screen = Screen::dancers;
            state.ending_stage = EndingStage::dancers;
            const int height = std::clamp(state.single_player.rules.type_b_height, 0, 5);
            state.timer = dancer_frames[static_cast<std::size_t>(height)];
            state.ending_elapsed = 0;
        }
        break;
    case EndingStage::dancers:
        if (state.single_player.rules.type_b_height != 5) {
            begin_scoreboard(state);
        } else {
            state.screen = Screen::buran;
            state.ending_stage = EndingStage::buran_initialize;
            state.launch_y = buran_start_y;
            state.exhaust_y = 0;
            state.timer = initialize_frames;
            state.ending_elapsed = 0;
        }
        break;

    // Buran setup and preflight.
    case EndingStage::buran_initialize:
        state.ending_stage = EndingStage::buran_prelaunch;
        state.timer = preflight_frames;
        state.ending_elapsed = 0;
        break;
    case EndingStage::buran_prelaunch:
        state.ending_stage = EndingStage::buran_ignition;
        state.timer = buran_ignition_frames;
        state.ending_elapsed = 0;
        break;
    case EndingStage::buran_ignition:
        state.ending_stage = EndingStage::buran_final_ignition;
        state.timer = buran_ignition_frames;
        state.ending_elapsed = 0;
        break;

    // Buran movement and congratulations text.
    case EndingStage::buran_final_ignition:
        state.ending_stage = EndingStage::buran_liftoff;
        state.ending_elapsed = 0;
        break;
    case EndingStage::buran_liftoff:
        --state.launch_y;
        state.timer = movement_frames;
        if (state.launch_y == buran_liftoff_y) {
            state.exhaust_y = buran_exhaust_y;
            state.ending_stage = EndingStage::buran_rising;
            state.ending_elapsed = 0;
        }
        break;
    case EndingStage::buran_rising:
        --state.launch_y;
        --state.exhaust_y;
        state.timer = movement_frames;
        if (state.launch_y == buran_exit_y) {
            state.ending_stage = EndingStage::congratulations;
            state.congratulations_characters = 0;
            state.ending_elapsed = 0;
        }
        break;
    case EndingStage::congratulations:
        ++state.congratulations_characters;
        state.ending_elapsed = 0;
        if (state.congratulations_characters == 16) {
            state.ending_stage = EndingStage::congratulations_wait;
            state.timer = congratulations_wait_frames;
        } else {
            state.timer = congratulations_character_frames;
        }
        break;
    case EndingStage::congratulations_wait:
        state.ending_stage = EndingStage::buran_teardown;
        state.timer = 3;
        state.ending_elapsed = 0;
        break;

    // Scoreboard delay and incremental tally.
    case EndingStage::buran_teardown:
        begin_scoreboard(state);
        break;
    case EndingStage::scoreboard_delay:
        state.ending_stage = EndingStage::scoreboard_tally;
        state.scoreboard.category = 0;
        state.ending_elapsed = 0;
        break;
    case EndingStage::scoreboard_tally: {
        const int category = state.scoreboard.category;
        if (category < 4) {
            std::uint32_t& remaining = line_count(state.scoreboard_remaining, category);
            std::uint32_t& displayed = line_count(state.scoreboard.lines, category);
            if (remaining > 0) {
                --remaining;
                ++displayed;
                state.scoreboard.score = std::min<std::uint32_t>(
                    999'999,
                    state.scoreboard.score +
                        line_clear_score(category + 1, state.single_player.rules.starting_level));
                state.timer = 5;
                break;
            }
            ++state.scoreboard.category;
            state.timer = scoreboard_category_frames;
        } else if (state.soft_drop_remaining > 0) {
            --state.soft_drop_remaining;
            ++state.scoreboard.soft_drop;
            state.scoreboard.score = std::min<std::uint32_t>(999'999, state.scoreboard.score + 1);
            state.timer = 1;
        } else {
            state.scoreboard.category = 5;
            state.ending_stage = EndingStage::scoreboard_wait;
            state.ending_elapsed = 0;
        }
        break;
    }
    case EndingStage::none:
    case EndingStage::game_over_wait:
    case EndingStage::scoreboard_wait:
        break;
    }
}

void begin_scoreboard(GameState& state) {
    state.screen = Screen::scoreboard;
    state.ending_stage = EndingStage::scoreboard_delay;
    state.ending_elapsed = 0;
    state.timer = scoreboard_delay_frames;
    state.scoreboard = {};
    if (state.single_player.options.modern_scoring) {
        // Modern Type B scores live during play. Preserve that total while the
        // original result screen presents the completed line breakdown.
        state.scoreboard.lines = state.single_player.line_counts;
        state.scoreboard.score = state.single_player.score;
        state.scoreboard.soft_drop = state.single_player.soft_drop_points;
        state.scoreboard_remaining = {};
        state.soft_drop_remaining = 0;
    } else {
        state.scoreboard_remaining = state.single_player.line_counts;
        state.soft_drop_remaining = state.single_player.soft_drop_points;
    }
}

} // namespace tetris
