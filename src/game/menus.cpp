#include "game/state.hpp"

#include <algorithm>
#include <string_view>

namespace tetris {
namespace {

bool confirm(const Buttons& pressed) {
    return pressed.confirm || pressed.start;
}

bool back(const Buttons& pressed) {
    return pressed.back;
}

bool any_button(const Buttons& buttons) {
    return buttons.left || buttons.right || buttons.up || buttons.down || buttons.back ||
           buttons.confirm || buttons.rotate_counterclockwise || buttons.rotate_clockwise ||
           buttons.hard_drop || buttons.hold || buttons.restart || buttons.start ||
           buttons.select;
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

void step_copyright(GameState& state, const Buttons& pressed) {
    // The copyright page may be dismissed from its first presented frame.
    if (any_button(pressed)) {
        return_to_title(state);
        return;
    }
    if (state.timer > 0)
        --state.timer;
    if (state.timer != 0)
        return;
    if (state.screen == Screen::copyright_fixed) {
        state.screen = Screen::copyright_skippable;
        state.timer = copyright_skippable_steps;
    } else {
        return_to_title(state);
    }
}

void step_title(GameState& state, const GameInput& input, const Buttons& pressed) {
    // Start enters the menus and samples the hidden heart-mode chord.
    if (pressed.start) {
        state.heart_mode = input.player_one.down;
        state.screen = Screen::game_type;
        return;
    }

    // Idle title intervals eventually alternate between the two demos.
    if (state.timer > 0)
        --state.timer;
    if (state.timer != 0)
        return;
    --state.title_intervals;
    if (state.title_intervals == 0)
        begin_demo(state, input);
    else
        state.timer = title_interval_steps;
}

void step_menu(GameState& state, const GameInput& input, const Buttons& pressed) {
    // Choose Type A, Type B, or two-player versus.
    if (state.screen == Screen::game_type) {
        int type = static_cast<int>(state.selected_type);
        if (pressed.left)
            type = std::max(type - 1, 0);
        if (pressed.right)
            type = std::min(type + 1, 2);
        state.selected_type = static_cast<GameType>(type);
        if (pressed.start) {
            state.screen = state.selected_type == GameType::type_a   ? Screen::type_a_level
                           : state.selected_type == GameType::type_b ? Screen::type_b_level
                                                                     : Screen::versus_height;
        } else if (pressed.confirm) {
            state.screen = Screen::music;
        }
        return;
    }

    // Choose one of the three songs or disable music.
    if (state.screen == Screen::music) {
        int music = static_cast<int>(state.selected_music);
        move_grid_cursor(music, 2, 3, pressed);
        state.selected_music = static_cast<Music>(music);
        if (back(pressed)) {
            state.screen = Screen::game_type;
        } else if (confirm(pressed)) {
            state.screen = state.selected_type == GameType::type_a   ? Screen::type_a_level
                           : state.selected_type == GameType::type_b ? Screen::type_b_level
                                                                     : Screen::versus_height;
        }
        return;
    }

    // Type A goes directly from level selection into gameplay.
    if (state.screen == Screen::type_a_level) {
        move_grid_cursor(state.type_a_level, 5, 9, pressed);
        if (back(pressed))
            state.screen = Screen::game_type;
        else if (confirm(pressed))
            begin_game(state, input);
        return;
    }

    // Type B selects level first, then optionally opens height selection.
    if (state.screen == Screen::type_b_level) {
        move_grid_cursor(state.type_b_level, 5, 9, pressed);
        if (back(pressed))
            state.screen = Screen::game_type;
        else if (pressed.start)
            begin_game(state, input);
        else if (pressed.confirm)
            state.screen = Screen::type_b_height;
        return;
    }

    // The remaining menu screen is Type-B height selection.
    move_grid_cursor(state.type_b_height, 3, 5, pressed);
    if (back(pressed))
        state.screen = Screen::type_b_level;
    else if (confirm(pressed))
        begin_game(state, input);
}

void step_name_entry(GameState& state, const Buttons& held, const Buttons& pressed) {
    if (!state.pending_score) {
        state.screen = state.after_name_entry;
        return;
    }

    // Blink the currently selected character.
    if (state.name_blink_timer > 0)
        --state.name_blink_timer;
    if (state.name_blink_timer == 0) {
        state.name_blink_timer = 7;
        state.name_character_visible = !state.name_character_visible;
    }

    // Convert held vertical input into the original repeating character cycle.
    constexpr std::string_view normal = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-* ";
    constexpr std::string_view heart = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-*~ ";
    const std::string_view characters = state.heart_mode ? heart : normal;
    bool cycle_up = pressed.up;
    bool cycle_down = pressed.down;
    if (cycle_up || cycle_down) {
        state.name_repeat_timer = 9;
    } else if (held.up || held.down) {
        if (state.name_repeat_timer > 0)
            --state.name_repeat_timer;
        if (state.name_repeat_timer == 0) {
            state.name_repeat_timer = 9;
            cycle_up = held.up;
            cycle_down = !cycle_up && held.down;
        }
    } else {
        state.name_repeat_timer = 0;
    }

    // Change the character under the cursor.
    if (cycle_up || cycle_down) {
        std::size_t value =
            characters.find(state.pending_name[static_cast<std::size_t>(state.name_cursor)]);
        if (value == std::string_view::npos)
            value = 0;
        value = cycle_up ? (value + 1) % characters.size()
                         : (value + characters.size() - 1) % characters.size();
        state.pending_name[static_cast<std::size_t>(state.name_cursor)] = characters[value];
        state.high_scores.name(*state.pending_score, state.pending_name);
    }

    // Move the cursor or commit the completed name.
    if (pressed.back && state.name_cursor > 0) {
        --state.name_cursor;
    } else if (pressed.confirm && state.name_cursor < 5) {
        ++state.name_cursor;
        if (static_cast<std::size_t>(state.name_cursor) == state.pending_name.size()) {
            state.pending_name.push_back('A');
            state.high_scores.name(*state.pending_score, state.pending_name);
        }
    } else if (pressed.start || (pressed.confirm && state.name_cursor == 5)) {
        state.high_scores.name(*state.pending_score, state.pending_name);
        state.pending_score.reset();
        state.screen = state.after_name_entry;
    }
}

void begin_name_entry(GameState& state, Screen after) {
    // Insert the score first; skip name entry when it did not place.
    state.after_name_entry = after;
    state.pending_score = state.high_scores.insert(
        state.single_player.rules.type, state.single_player.rules.starting_level,
        state.single_player.rules.type_b_height, state.single_player.score);
    if (!state.pending_score) {
        state.screen = after;
        return;
    }

    // Seed the first character and reset editor timing.
    state.pending_name = "A";
    state.name_cursor = 0;
    state.name_repeat_timer = 0;
    state.name_blink_timer = 0;
    state.name_character_visible = true;
    state.high_scores.name(*state.pending_score, state.pending_name);
    state.screen = Screen::name_entry;
}

void return_to_title(GameState& state, int intervals) {
    state.screen = Screen::title;
    state.timer = title_interval_steps;
    state.title_intervals = intervals;
    state.previous_one = {};
    state.previous_two = {};
}

} // namespace tetris
