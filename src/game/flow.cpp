#include "game/flow.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <string_view>
#include <vector>

namespace tetris {
namespace {

constexpr int title_interval_frames = 125;

bool confirm(const Buttons& pressed) {
    return pressed.rotate_right || pressed.start;
}

bool back(const Buttons& pressed) {
    return pressed.rotate_left;
}

void move_grid_cursor(int& value, int columns, int maximum, const Buttons& pressed) {
    if (pressed.left)
        value = std::max(value - 1, 0);
    if (pressed.right)
        value = std::min(value + 1, maximum);
    if (pressed.up && value >= columns)
        value -= columns;
    if (pressed.down && value + columns <= maximum)
        value += columns;
}

} // namespace

void GameFlow::start(FlowResources resources) {
    resources_ = resources;
    screen_ = Screen::copyright_fixed;
    previous_one_ = {};
    previous_two_ = {};
    selected_type_ = GameType::type_a;
    selected_music_ = Music::a;
    type_a_level_ = 0;
    type_b_level_ = 0;
    type_b_height_ = 0;
    heart_mode_ = false;
    timer_ = 250;
    title_intervals_ = 19;
    next_demo_type_b_ = false;
    ending_stage_ = EndingStage::none;
}

void GameFlow::tick(const FlowInput& input) {
    const Buttons pressed = newly_pressed(input.player_one, previous_one_);
    const Buttons pressed_two = newly_pressed(input.player_two, previous_two_);
    previous_one_ = input.player_one;
    previous_two_ = input.player_two;

    const bool reset = input.player_one.start && input.player_one.select &&
                       input.player_one.rotate_left && input.player_one.rotate_right;
    if (reset) {
        start(resources_);
        return;
    }

    switch (screen_) {
    case Screen::copyright_fixed:
    case Screen::copyright_skippable:
        tick_copyright(pressed);
        break;
    case Screen::title:
        tick_title(input, pressed);
        break;
    case Screen::game_type:
    case Screen::music:
    case Screen::type_a_level:
    case Screen::type_b_level:
    case Screen::type_b_height:
        tick_menu(input, pressed);
        break;
    case Screen::versus_height:
    case Screen::versus_gameplay:
    case Screen::versus_round_result:
    case Screen::versus_match_result:
        tick_versus(input, pressed, pressed_two);
        break;
    case Screen::gameplay:
        tick_gameplay(input);
        break;
    case Screen::demo:
        tick_demo(input, pressed);
        break;
    case Screen::game_over:
    case Screen::type_b_celebration:
    case Screen::dancers:
    case Screen::buran:
    case Screen::rocket:
    case Screen::scoreboard:
        tick_ending(pressed);
        break;
    case Screen::name_entry:
        tick_name_entry(input.player_one, pressed);
        break;
    }
}

void GameFlow::tick_copyright(const Buttons& pressed) {
    if (timer_ > 0)
        --timer_;
    if (screen_ == Screen::copyright_skippable && confirm(pressed)) {
        return_to_title();
        return;
    }
    if (timer_ != 0)
        return;
    if (screen_ == Screen::copyright_fixed) {
        screen_ = Screen::copyright_skippable;
        timer_ = 250;
    } else {
        return_to_title();
    }
}

void GameFlow::tick_title(const FlowInput& input, const Buttons& pressed) {
    if (pressed.start) {
        heart_mode_ = input.player_one.down;
        screen_ = Screen::game_type;
        return;
    }
    if (timer_ > 0)
        --timer_;
    if (timer_ != 0)
        return;
    --title_intervals_;
    if (title_intervals_ == 0)
        begin_demo(input);
    else
        timer_ = title_interval_frames;
}

void GameFlow::tick_menu(const FlowInput& input, const Buttons& pressed) {
    if (screen_ == Screen::game_type) {
        int type = static_cast<int>(selected_type_);
        if (pressed.left)
            type = std::max(type - 1, 0);
        if (pressed.right)
            type = std::min(type + 1, 2);
        selected_type_ = static_cast<GameType>(type);
        if (pressed.start) {
            screen_ = selected_type_ == GameType::type_a ? Screen::type_a_level
                    : selected_type_ == GameType::type_b ? Screen::type_b_level
                                                         : Screen::versus_height;
        } else if (pressed.rotate_right) {
            screen_ = Screen::music;
        }
        return;
    }

    if (screen_ == Screen::music) {
        int music = static_cast<int>(selected_music_);
        move_grid_cursor(music, 2, 3, pressed);
        selected_music_ = static_cast<Music>(music);
        if (back(pressed)) {
            screen_ = Screen::game_type;
        } else if (confirm(pressed)) {
            screen_ = selected_type_ == GameType::type_a ? Screen::type_a_level
                    : selected_type_ == GameType::type_b ? Screen::type_b_level
                                                         : Screen::versus_height;
        }
        return;
    }

    if (screen_ == Screen::type_a_level) {
        move_grid_cursor(type_a_level_, 5, 9, pressed);
        if (back(pressed))
            screen_ = Screen::game_type;
        else if (confirm(pressed))
            begin_game(input);
        return;
    }

    if (screen_ == Screen::type_b_level) {
        move_grid_cursor(type_b_level_, 5, 9, pressed);
        if (back(pressed))
            screen_ = Screen::game_type;
        else if (pressed.start)
            begin_game(input);
        else if (pressed.rotate_right)
            screen_ = Screen::type_b_height;
        return;
    }

    move_grid_cursor(type_b_height_, 3, 5, pressed);
    if (back(pressed))
        screen_ = Screen::type_b_level;
    else if (confirm(pressed))
        begin_game(input);
}

void GameFlow::begin_game(const FlowInput& input) {
    const int level = selected_type_ == GameType::type_a ? type_a_level_ : type_b_level_;
    game_.start({.type = selected_type_, .starting_level = level,
                 .type_b_height = selected_type_ == GameType::type_b ? type_b_height_ : 0,
                 .heart_mode = heart_mode_}, input.startup);
    game_.set_line_clear_speed(line_clear_speed_);
    screen_ = Screen::gameplay;
}

void GameFlow::tick_gameplay(const FlowInput& input) {
    game_.tick({input.player_one, input.random});
    if (game_.state() == PlayState::game_over)
        begin_game_over();
    else if (game_.state() == PlayState::complete)
        begin_type_b_ending();
}

void GameFlow::begin_demo(const FlowInput& input) {
    demo_type_b_ = next_demo_type_b_;
    next_demo_type_b_ = !next_demo_type_b_;
    std::vector<PieceKind> pieces;
    const std::size_t begin = demo_type_b_ ? 17U : 0U;
    for (std::size_t index = begin; index < resources_.demo_pieces.size(); ++index)
        pieces.push_back(resources_.demo_pieces[index].kind);
    game_.start({.type = demo_type_b_ ? GameType::type_b : GameType::type_a,
                 .starting_level = 9, .type_b_height = demo_type_b_ ? 2 : 0},
                input.startup, pieces);
    game_.set_line_clear_speed(line_clear_speed_);
    if (demo_type_b_ && resources_.type_b_demo_garbage.size() >= 40) {
        for (int index = 0; index < 40; ++index) {
            const std::uint8_t tile = resources_.type_b_demo_garbage[static_cast<std::size_t>(index)];
            if (tile != 0x2F)
                game_.edit_board().set({index % 10, 14 + index / 10}, tile);
        }
    }
    demo_.start(demo_type_b_ ? resources_.type_b_demo : resources_.type_a_demo);
    screen_ = Screen::demo;
}

void GameFlow::tick_demo(const FlowInput& input, const Buttons& pressed) {
    if (pressed.start) {
        return_to_title(4);
        return;
    }
    game_.tick({demo_.tick(), input.random});
    const std::size_t end_piece = demo_type_b_ ? 12U : 16U;
    if (demo_.run_index() >= end_piece || game_.state() == PlayState::game_over ||
        game_.state() == PlayState::complete)
        return_to_title(4);
}

void GameFlow::tick_versus(const FlowInput& input, const Buttons& pressed,
                           const Buttons& player_two_pressed) {
    if (screen_ == Screen::versus_height) {
        move_grid_cursor(versus_heights_[0], 3, 5, pressed);
        move_grid_cursor(versus_heights_[1], 3, 5, player_two_pressed);
        if (back(pressed)) {
            screen_ = Screen::game_type;
            new_match_ = true;
        } else if (confirm(pressed)) {
            const VersusSetup setup{versus_heights_[0], versus_heights_[1]};
            if (new_match_) {
                versus_.start(setup, input.versus);
                new_match_ = false;
            } else {
                versus_.next_round(setup, input.versus);
            }
            versus_.set_line_clear_speed(line_clear_speed_);
            screen_ = Screen::versus_gameplay;
        }
        return;
    }

    if (screen_ == Screen::versus_gameplay) {
        if (result_timer_ > 0) {
            --result_timer_;
            if (result_timer_ == 0) {
                screen_ = versus_.state() == MatchState::match_over
                              ? Screen::versus_match_result
                              : Screen::versus_round_result;
                result_timer_ = 25;
                result_step_ = 0;
            }
            return;
        }
        versus_.tick({input.player_one, input.random}, {input.player_two, input.random});
        if (versus_.state() != MatchState::playing)
            result_timer_ = 40;
        return;
    }

    if (screen_ == Screen::versus_round_result && pressed.start) {
        screen_ = Screen::versus_height;
        return;
    }
    if (--result_timer_ > 0)
        return;
    result_timer_ = 25;
    ++result_step_;
    if (screen_ == Screen::versus_match_result && result_step_ == 29) {
        new_match_ = true;
        screen_ = Screen::versus_height;
    }
}

void GameFlow::tick_name_entry(const Buttons& held, const Buttons& pressed) {
    if (!pending_score_) {
        screen_ = after_name_entry_;
        return;
    }
    constexpr std::string_view normal = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-* ";
    constexpr std::string_view heart = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-*~ ";
    const std::string_view characters = heart_mode_ ? heart : normal;
    if (pressed.left)
        name_cursor_ = std::max(name_cursor_ - 1, 0);
    if (pressed.right)
        name_cursor_ = std::min(name_cursor_ + 1, 5);
    if (pressed.up || pressed.down || (held.up && name_repeat_timer_ == 0) ||
        (held.down && name_repeat_timer_ == 0)) {
        std::size_t value = characters.find(pending_name_[static_cast<std::size_t>(name_cursor_)]);
        if (value == std::string_view::npos)
            value = 0;
        value = pressed.up || held.up ? (value + characters.size() - 1) % characters.size()
                                     : (value + 1) % characters.size();
        pending_name_[static_cast<std::size_t>(name_cursor_)] = characters[value];
        name_repeat_timer_ = 8;
    }
    if (name_repeat_timer_ > 0)
        --name_repeat_timer_;
    if (++name_blink_timer_ == 8)
        name_blink_timer_ = 0;
    if (pressed.rotate_right && name_cursor_ < 5) {
        ++name_cursor_;
    } else if (pressed.start || (pressed.rotate_right && name_cursor_ == 5)) {
        high_scores_.name(*pending_score_, pending_name_);
        pending_score_.reset();
        screen_ = after_name_entry_;
    }
}

void GameFlow::begin_name_entry(Screen after) {
    after_name_entry_ = after;
    pending_score_ = high_scores_.insert(game_.rules().type, game_.rules().starting_level,
                                         game_.rules().type_b_height, game_.score());
    if (!pending_score_) {
        screen_ = after;
        return;
    }
    pending_name_ = "AAAAAA";
    name_cursor_ = 0;
    name_repeat_timer_ = 0;
    name_blink_timer_ = 0;
    screen_ = Screen::name_entry;
}

void GameFlow::return_to_title(int intervals) {
    screen_ = Screen::title;
    timer_ = title_interval_frames;
    title_intervals_ = intervals;
    previous_one_ = {};
    previous_two_ = {};
}

void GameFlow::set_line_clear_speed(LineClearSpeed speed) {
    line_clear_speed_ = speed;
    game_.set_line_clear_speed(speed);
    versus_.set_line_clear_speed(speed);
}

Screen GameFlow::screen() const { return screen_; }
GameType GameFlow::selected_type() const { return selected_type_; }
Music GameFlow::selected_music() const { return selected_music_; }
int GameFlow::selected_level() const {
    return selected_type_ == GameType::type_a ? type_a_level_ : type_b_level_;
}
int GameFlow::selected_height() const { return type_b_height_; }
bool GameFlow::heart_mode() const { return heart_mode_; }
bool GameFlow::demo_is_type_b() const { return demo_type_b_; }
int GameFlow::timer() const { return timer_; }
EndingStage GameFlow::ending_stage() const { return ending_stage_; }
int GameFlow::ending_elapsed() const { return ending_elapsed_; }
Rocket GameFlow::rocket() const { return rocket_; }
int GameFlow::launch_y() const { return launch_y_; }
int GameFlow::exhaust_y() const { return exhaust_y_; }
int GameFlow::congratulations_characters() const { return congratulations_characters_; }
const Scoreboard& GameFlow::scoreboard() const { return scoreboard_; }
const SinglePlayer& GameFlow::game() const { return game_; }
SinglePlayer& GameFlow::edit_game() { return game_; }
const VersusMatch& GameFlow::versus() const { return versus_; }
VersusMatch& GameFlow::edit_versus() { return versus_; }
int GameFlow::versus_height(int player) const { return versus_heights_[static_cast<std::size_t>(player)]; }
int GameFlow::versus_result_step() const { return result_step_; }
const HighScores& GameFlow::high_scores() const { return high_scores_; }
const std::string& GameFlow::pending_name() const { return pending_name_; }
int GameFlow::name_cursor() const { return name_cursor_; }

void GameFlow::start_game_for_test(GameRules rules, const StartupRandom& random) {
    selected_type_ = rules.type;
    type_a_level_ = rules.starting_level;
    type_b_level_ = rules.starting_level;
    type_b_height_ = rules.type_b_height;
    game_.start(rules, random);
    game_.set_line_clear_speed(line_clear_speed_);
    screen_ = Screen::gameplay;
}

} // namespace tetris
