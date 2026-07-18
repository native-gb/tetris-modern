#include "game/flow.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <string_view>
#include <utility>
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

bool any_button(const Buttons& buttons) {
    return buttons.left || buttons.right || buttons.up || buttons.down ||
           buttons.rotate_left || buttons.rotate_right || buttons.start || buttons.select;
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
    demo_type_b_ = false;
    ending_stage_ = EndingStage::none;
    ending_elapsed_ = 0;
    rocket_ = Rocket::small;
    launch_y_ = 0;
    exhaust_y_ = 0;
    congratulations_characters_ = 0;
    scoreboard_ = {};
    scoreboard_remaining_ = {};
    soft_drop_remaining_ = 0;
    versus_heights_ = {};
    new_match_ = true;
    result_timer_ = 0;
    result_step_ = 0;
    result_elapsed_ = 0;
    pending_score_.reset();
    pending_name_.clear();
    name_cursor_ = 0;
    name_repeat_timer_ = 0;
    name_blink_timer_ = 0;
    name_character_visible_ = true;
}

void GameFlow::tick(const FlowInput& input) {
    game_.clear_events();
    versus_.clear_events();
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
    if (screen_ == Screen::copyright_skippable && any_button(pressed)) {
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
                 .heart_mode = heart_mode_}, input.startup, {}, input.player_one);
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
    std::vector<PieceSpec> pieces;
    const std::size_t begin = demo_type_b_ ? 17U : 0U;
    for (std::size_t index = begin; index < resources_.demo_pieces.size(); ++index)
        pieces.push_back(resources_.demo_pieces[index]);
    game_.start({.type = demo_type_b_ ? GameType::type_b : GameType::type_a,
                 .starting_level = 9, .type_b_height = demo_type_b_ ? 2 : 0},
                input.startup, pieces);
    game_.set_line_clear_speed(line_clear_speed_);
    if (demo_type_b_ && resources_.type_b_demo_garbage.size() >= 40) {
        for (int index = 0; index < 40; ++index) {
            const std::uint8_t tile = resources_.type_b_demo_garbage[static_cast<std::size_t>(index)];
            if (tile != 0x2F)
                game_.edit_board().set({index % 10, 14 + index / 10}, garbage_block(tile));
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
    if (game_.fixed_pieces_consumed() >= end_piece || game_.state() == PlayState::game_over ||
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
            result_timer_ = 0;
            result_step_ = 0;
            result_elapsed_ = 0;
        }
        return;
    }

    if (screen_ == Screen::versus_gameplay) {
        if (result_timer_ > 0) {
            --result_timer_;
            ++result_elapsed_;
            if (result_timer_ == 0) {
                screen_ = versus_.state() == MatchState::match_over
                              ? Screen::versus_match_result
                              : Screen::versus_round_result;
                result_timer_ = 25;
                result_step_ = 0;
                result_elapsed_ = 0;
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
    ++result_elapsed_;
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
    if (name_blink_timer_ > 0)
        --name_blink_timer_;
    if (name_blink_timer_ == 0) {
        name_blink_timer_ = 7;
        name_character_visible_ = !name_character_visible_;
    }
    constexpr std::string_view normal = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-* ";
    constexpr std::string_view heart = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-*~ ";
    const std::string_view characters = heart_mode_ ? heart : normal;
    bool cycle_up = pressed.up;
    bool cycle_down = pressed.down;
    if (cycle_up || cycle_down) {
        name_repeat_timer_ = 9;
    } else if (held.up || held.down) {
        if (name_repeat_timer_ > 0)
            --name_repeat_timer_;
        if (name_repeat_timer_ == 0) {
            name_repeat_timer_ = 9;
            cycle_up = held.up;
            cycle_down = !cycle_up && held.down;
        }
    } else {
        name_repeat_timer_ = 0;
    }
    if (cycle_up || cycle_down) {
        std::size_t value = characters.find(pending_name_[static_cast<std::size_t>(name_cursor_)]);
        if (value == std::string_view::npos)
            value = 0;
        value = cycle_up ? (value + 1) % characters.size()
                         : (value + characters.size() - 1) % characters.size();
        pending_name_[static_cast<std::size_t>(name_cursor_)] = characters[value];
        high_scores_.name(*pending_score_, pending_name_);
    }
    if (pressed.rotate_left && name_cursor_ > 0) {
        --name_cursor_;
    } else if (pressed.rotate_right && name_cursor_ < 5) {
        ++name_cursor_;
        if (static_cast<std::size_t>(name_cursor_) == pending_name_.size()) {
            pending_name_.push_back('A');
            high_scores_.name(*pending_score_, pending_name_);
        }
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
    pending_name_ = "A";
    name_cursor_ = 0;
    name_repeat_timer_ = 0;
    name_blink_timer_ = 0;
    name_character_visible_ = true;
    high_scores_.name(*pending_score_, pending_name_);
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
void GameFlow::set_high_scores(HighScores scores) { high_scores_ = std::move(scores); }
const std::string& GameFlow::pending_name() const { return pending_name_; }
int GameFlow::name_cursor() const { return name_cursor_; }
int GameFlow::name_entry_rank() const {
    return pending_score_ ? static_cast<int>(pending_score_->rank) : -1;
}
bool GameFlow::name_character_visible() const { return name_character_visible_; }
int GameFlow::game_over_curtain_rows() const {
    return ending_stage_ == EndingStage::game_over_curtain
               ? std::min(ending_elapsed_, board_height)
               : 0;
}
int GameFlow::game_over_panel_rows() const {
    if (screen_ != Screen::game_over || ending_stage_ == EndingStage::game_over_curtain)
        return 0;
    return std::min(ending_elapsed_, board_height);
}
bool GameFlow::launch_smoke_visible() const {
    const bool ignition = ending_stage_ == EndingStage::rocket_ignition ||
        ending_stage_ == EndingStage::rocket_liftoff_delay ||
        ending_stage_ == EndingStage::rocket_liftoff ||
        ending_stage_ == EndingStage::buran_ignition ||
        ending_stage_ == EndingStage::buran_final_ignition ||
        ending_stage_ == EndingStage::buran_liftoff;
    return ignition && ((ending_elapsed_ / 10) & 1) == 0;
}
int GameFlow::exhaust_x() const { return screen_ == Screen::buran ? 0x4C : 0x54; }
int GameFlow::exhaust_animation_frame() const {
    if (ending_stage_ == EndingStage::rocket_rising)
        return ((ending_elapsed_ + 3) / 6) & 1;
    if (ending_stage_ == EndingStage::buran_rising)
        return ((ending_elapsed_ + 5) / 6) & 1;
    return 0;
}
int GameFlow::dancer_frame(int dancer) const {
    constexpr std::array lengths = {28, 15, 30, 50, 32, 24, 38, 29, 40, 43};
    if (dancer < 0 || dancer >= static_cast<int>(lengths.size()))
        return 0;
    return (std::max(ending_elapsed_ - 26, 0) /
            lengths[static_cast<std::size_t>(dancer)]) & 1;
}
int GameFlow::dancer_vertical_offset(int dancer) const {
    return dancer == 6 && dancer_frame(dancer) != 0 ? -10 : 0;
}
int GameFlow::versus_result_elapsed() const { return result_elapsed_; }
bool GameFlow::versus_result_prompt_visible() const {
    if (versus_.state() == MatchState::match_over ||
        (screen_ != Screen::versus_round_result && screen_ != Screen::versus_match_result))
        return false;
    const bool local_victory = versus_.winner() != RoundWinner::player_two;
    return local_victory ? result_step_ > 0 && result_step_ % 2 == 0
                         : result_step_ % 2 == 1;
}
LineClearSpeed GameFlow::line_clear_speed() const { return line_clear_speed_; }

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
