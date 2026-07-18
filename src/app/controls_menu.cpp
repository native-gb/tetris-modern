#include "app/controls_menu.hpp"

#include <gubsy/menu/ids.hpp>

namespace tetris::app {

bool ControlsMenu::open(GubsyRuntime& runtime) {
    if (!gubsy_open_in_game_menu(runtime))
        return false;
    gubsy_clear_menu_stack(runtime);
    if (!gubsy_push_menu_screen(runtime, MenuScreenID::LOBBY_LOCAL_PLAYERS)) {
        gubsy_close_in_game_menu(runtime);
        return false;
    }
    previous_ = {};
    return true;
}

bool ControlsMenu::is_open(const GubsyRuntime& runtime) const {
    return gubsy_in_game_menu_open(runtime);
}

bool ControlsMenu::chord_pressed(const Buttons& buttons) {
    const bool chord_down = buttons.start && buttons.select;
    const bool pressed = chord_down && !chord_was_down_;
    chord_was_down_ = chord_down;
    return pressed;
}

void ControlsMenu::update(GubsyRuntime& runtime, const Buttons& buttons, float elapsed_seconds,
                          int width, int height) {
    if (!is_open(runtime)) {
        previous_ = {};
        return;
    }
    const Buttons pressed = newly_pressed(buttons, previous_);
    previous_ = buttons;
    gubsy_set_menu_input(runtime, MenuInputState{
        .up = pressed.up,
        .down = pressed.down,
        .left = pressed.left,
        .right = pressed.right,
        .select = pressed.rotate_right || pressed.start,
        .back = pressed.rotate_left || pressed.select,
        .page_prev = false,
        .page_next = false,
    });
    gubsy_update_menu(runtime, elapsed_seconds, width, height);
}

void ControlsMenu::draw(GubsyRuntime& runtime, SDL_Renderer* renderer, int width, int height) const {
    if (!is_open(runtime))
        return;
    gubsy_render_menu(runtime, renderer, width, height);
    gubsy_render_alerts(runtime, renderer, width);
}

} // namespace tetris::app
