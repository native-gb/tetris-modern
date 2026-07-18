#include "game/flow.hpp"

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
    return pressed.rotate_right || pressed.start;
}

} // namespace

void GameFlow::begin_game_over() {
    screen_ = Screen::game_over;
    ending_stage_ = EndingStage::game_over_curtain;
    ending_elapsed_ = 0;
    timer_ = game_over_curtain_frames;
}

void GameFlow::begin_type_b_ending() {
    screen_ = Screen::type_b_celebration;
    ending_stage_ = EndingStage::type_b_victory_delay;
    ending_elapsed_ = 0;
    timer_ = type_b_delay_frames;
}

void GameFlow::tick_ending(const Buttons& pressed) {
    if (ending_stage_ == EndingStage::game_over_wait) {
        ++ending_elapsed_;
        if (confirm(pressed)) {
            begin_name_entry(selected_type_ == GameType::type_a
                                 ? Screen::type_a_level
                                 : Screen::type_b_level);
        }
        return;
    }
    if (ending_stage_ == EndingStage::scoreboard_wait) {
        ++ending_elapsed_;
        if (confirm(pressed))
            begin_name_entry(Screen::type_b_level);
        return;
    }

    ++ending_elapsed_;
    if (timer_ > 0)
        --timer_;
    if (timer_ != 0)
        return;

    switch (ending_stage_) {
    case EndingStage::game_over_curtain:
        if (game_.rules().type != GameType::type_a || game_.score() < 100'000) {
            ending_stage_ = EndingStage::game_over_wait;
            ending_elapsed_ = 0;
        } else {
            rocket_ = game_.score() >= 200'000 ? Rocket::large
                    : game_.score() >= 150'000 ? Rocket::medium
                                               : Rocket::small;
            ending_stage_ = EndingStage::rocket_bonus_delay;
            timer_ = rocket_bonus_delay_frames;
            ending_elapsed_ = 0;
        }
        break;
    case EndingStage::rocket_bonus_delay:
        screen_ = Screen::rocket;
        ending_stage_ = EndingStage::rocket_initialize;
        launch_y_ = 0x6F;
        exhaust_y_ = 0;
        timer_ = initialize_frames;
        ending_elapsed_ = 0;
        break;
    case EndingStage::rocket_initialize:
        ending_stage_ = EndingStage::rocket_prelaunch;
        timer_ = preflight_frames;
        ending_elapsed_ = 0;
        break;
    case EndingStage::rocket_prelaunch:
        ending_stage_ = EndingStage::rocket_ignition;
        timer_ = rocket_ignition_frames;
        ending_elapsed_ = 0;
        break;
    case EndingStage::rocket_ignition:
        ending_stage_ = EndingStage::rocket_liftoff_delay;
        timer_ = rocket_liftoff_delay_frames;
        ending_elapsed_ = 0;
        break;
    case EndingStage::rocket_liftoff_delay:
        ending_stage_ = EndingStage::rocket_liftoff;
        ending_elapsed_ = 0;
        [[fallthrough]];
    case EndingStage::rocket_liftoff:
        launch_y_ = (launch_y_ - 1) & 0xFF;
        timer_ = movement_frames;
        if (launch_y_ == 0x6A) {
            exhaust_y_ = launch_y_ + 0x10;
            ending_stage_ = EndingStage::rocket_rising;
            ending_elapsed_ = 0;
        }
        break;
    case EndingStage::rocket_rising:
        launch_y_ = (launch_y_ - 1) & 0xFF;
        exhaust_y_ = (exhaust_y_ - 1) & 0xFF;
        timer_ = movement_frames;
        if (launch_y_ == 0xE0) {
            ending_stage_ = EndingStage::rocket_return;
            timer_ = 5;
            ending_elapsed_ = 0;
        }
        break;
    case EndingStage::rocket_return:
        ending_stage_ = EndingStage::none;
        begin_name_entry(Screen::type_a_level);
        break;
    case EndingStage::type_b_victory_delay:
        if (game_.rules().starting_level != 9) {
            begin_scoreboard();
        } else {
            screen_ = Screen::dancers;
            ending_stage_ = EndingStage::dancers;
            const int height = std::clamp(game_.rules().type_b_height, 0, 5);
            timer_ = dancer_frames[static_cast<std::size_t>(height)];
            ending_elapsed_ = 0;
        }
        break;
    case EndingStage::dancers:
        if (game_.rules().type_b_height != 5) {
            begin_scoreboard();
        } else {
            screen_ = Screen::buran;
            ending_stage_ = EndingStage::buran_initialize;
            launch_y_ = 0x5F;
            exhaust_y_ = 0;
            timer_ = initialize_frames;
            ending_elapsed_ = 0;
        }
        break;
    case EndingStage::buran_initialize:
        ending_stage_ = EndingStage::buran_prelaunch;
        timer_ = preflight_frames;
        ending_elapsed_ = 0;
        break;
    case EndingStage::buran_prelaunch:
        ending_stage_ = EndingStage::buran_ignition;
        timer_ = buran_ignition_frames;
        ending_elapsed_ = 0;
        break;
    case EndingStage::buran_ignition:
        ending_stage_ = EndingStage::buran_final_ignition;
        timer_ = buran_ignition_frames;
        ending_elapsed_ = 0;
        break;
    case EndingStage::buran_final_ignition:
        ending_stage_ = EndingStage::buran_liftoff;
        ending_elapsed_ = 0;
        break;
    case EndingStage::buran_liftoff:
        launch_y_ = (launch_y_ - 1) & 0xFF;
        timer_ = movement_frames;
        if (launch_y_ == 0x58) {
            exhaust_y_ = launch_y_ + 0x20;
            ending_stage_ = EndingStage::buran_rising;
            ending_elapsed_ = 0;
        }
        break;
    case EndingStage::buran_rising:
        launch_y_ = (launch_y_ - 1) & 0xFF;
        exhaust_y_ = (exhaust_y_ - 1) & 0xFF;
        timer_ = movement_frames;
        if (launch_y_ == 0xD0) {
            ending_stage_ = EndingStage::congratulations;
            congratulations_characters_ = 0;
            ending_elapsed_ = 0;
        }
        break;
    case EndingStage::congratulations:
        ++congratulations_characters_;
        ending_elapsed_ = 0;
        if (congratulations_characters_ == 16) {
            ending_stage_ = EndingStage::congratulations_wait;
            timer_ = congratulations_wait_frames;
        } else {
            timer_ = congratulations_character_frames;
        }
        break;
    case EndingStage::congratulations_wait:
        ending_stage_ = EndingStage::buran_teardown;
        timer_ = 3;
        ending_elapsed_ = 0;
        break;
    case EndingStage::buran_teardown:
        begin_scoreboard();
        break;
    case EndingStage::scoreboard_delay:
        ending_stage_ = EndingStage::scoreboard_tally;
        scoreboard_.category = 0;
        timer_ = scoreboard_category_frames;
        ending_elapsed_ = 0;
        break;
    case EndingStage::scoreboard_tally: {
        const int category = scoreboard_.category;
        std::uint32_t& remaining = line_count(scoreboard_remaining_, category);
        std::uint32_t& displayed = line_count(scoreboard_.lines, category);
        if (remaining > 0) {
            --remaining;
            ++displayed;
            scoreboard_.score = std::min<std::uint32_t>(
                999'999, scoreboard_.score + line_clear_score(category + 1, game_.level()));
            timer_ = 5;
        } else if (category < 3) {
            ++scoreboard_.category;
            timer_ = scoreboard_category_frames;
        } else if (soft_drop_remaining_ > 0) {
            --soft_drop_remaining_;
            ++scoreboard_.soft_drop;
            scoreboard_.score = std::min<std::uint32_t>(999'999, scoreboard_.score + 1);
            timer_ = 1;
        } else {
            ending_stage_ = EndingStage::scoreboard_wait;
            ending_elapsed_ = 0;
        }
        break;
    }
    case EndingStage::none:
    case EndingStage::game_over_wait:
    case EndingStage::scoreboard_wait:
        break;
    }
}

void GameFlow::begin_scoreboard() {
    screen_ = Screen::scoreboard;
    ending_stage_ = EndingStage::scoreboard_delay;
    ending_elapsed_ = 0;
    timer_ = scoreboard_delay_frames;
    scoreboard_ = {};
    scoreboard_remaining_ = game_.line_counts();
    soft_drop_remaining_ = game_.soft_drop_points();
}

} // namespace tetris
