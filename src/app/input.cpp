#include "app/input.hpp"

#include <SDL3/SDL.h>
#include <gubsy/input/binds_profile.hpp>
#include <gubsy/runtime.hpp>

#include <algorithm>

namespace tetris::app {
namespace {

constexpr int player_one_profile = 4201;
constexpr int player_two_profile = 4202;

void bind(BindsProfile& profile, GubsyButton button, Action action) {
    bind_button(profile, button, static_cast<int>(action));
}

void bind_gamepad(BindsProfile& profile) {
    bind(profile, GubsyButton::GP_DPAD_LEFT, Action::left);
    bind(profile, GubsyButton::GP_DPAD_RIGHT, Action::right);
    bind(profile, GubsyButton::GP_DPAD_UP, Action::up);
    bind(profile, GubsyButton::GP_DPAD_DOWN, Action::down);
    bind(profile, GubsyButton::GP_X, Action::rotate_left);
    bind(profile, GubsyButton::GP_A, Action::rotate_right);
    bind(profile, GubsyButton::GP_START, Action::start);
    bind(profile, GubsyButton::GP_BACK, Action::select);
    bind(profile, GubsyButton::GP_GUIDE, Action::quit);
    bind(profile, GubsyButton::GP_RIGHT_STICK_BUTTON, Action::quit);
}

bool has_gamepad(const GubsyLobbyPlayer& player) {
    return std::ranges::any_of(player.devices, [](GubsyLobbyDeviceAssignment device) {
        return device.type == InputSourceType::Gamepad;
    });
}

bool assigned(const GubsyLobbyState& lobby, int device_id) {
    for (const GubsyLobbyPlayer& player : lobby.local_players) {
        const bool found = std::ranges::any_of(
            player.devices, [device_id](GubsyLobbyDeviceAssignment device) {
                return device.type == InputSourceType::Gamepad && device.device_id == device_id;
            });
        if (found)
            return true;
    }
    return false;
}

} // namespace

bool action_down(GubsyRuntime& runtime, int player, Action action) {
    return gubsy_lobby_player_action_down(runtime, player, static_cast<int>(action));
}

Buttons read_buttons(GubsyRuntime& runtime, int player) {
    return {
        .left = action_down(runtime, player, Action::left),
        .right = action_down(runtime, player, Action::right),
        .up = action_down(runtime, player, Action::up),
        .down = action_down(runtime, player, Action::down),
        .rotate_left = action_down(runtime, player, Action::rotate_left),
        .rotate_right = action_down(runtime, player, Action::rotate_right),
        .start = action_down(runtime, player, Action::start),
        .select = action_down(runtime, player, Action::select),
    };
}

bool register_controls(GubsyRuntime& runtime) {
    BindsSchema schema;
    (void)schema.add_action(static_cast<int>(Action::left), "Left", "Game Boy");
    (void)schema.add_action(static_cast<int>(Action::right), "Right", "Game Boy");
    (void)schema.add_action(static_cast<int>(Action::up), "Up", "Game Boy");
    (void)schema.add_action(static_cast<int>(Action::down), "Down", "Game Boy");
    (void)schema.add_action(static_cast<int>(Action::rotate_left), "B / Rotate left", "Game Boy");
    (void)schema.add_action(static_cast<int>(Action::rotate_right), "A / Rotate right", "Game Boy");
    (void)schema.add_action(static_cast<int>(Action::start), "Start", "Game Boy");
    (void)schema.add_action(static_cast<int>(Action::select), "Select", "Game Boy");
    (void)schema.add_action(static_cast<int>(Action::quit), "Quit", "Application");
    gubsy_register_binds_schema(runtime, schema);

    BindsProfile one;
    one.id = player_one_profile;
    one.name = "TetrisPlayerOne";
    bind(one, GubsyButton::KB_LEFT, Action::left); bind(one, GubsyButton::KB_A, Action::left);
    bind(one, GubsyButton::KB_RIGHT, Action::right); bind(one, GubsyButton::KB_D, Action::right);
    bind(one, GubsyButton::KB_UP, Action::up); bind(one, GubsyButton::KB_W, Action::up);
    bind(one, GubsyButton::KB_DOWN, Action::down); bind(one, GubsyButton::KB_S, Action::down);
    bind(one, GubsyButton::KB_Z, Action::rotate_left); bind(one, GubsyButton::KB_X, Action::rotate_right);
    bind(one, GubsyButton::KB_ENTER, Action::start); bind(one, GubsyButton::KB_BACKSPACE, Action::select);
    bind(one, GubsyButton::KB_ESCAPE, Action::quit); bind_gamepad(one);

    BindsProfile two;
    two.id = player_two_profile;
    two.name = "TetrisPlayerTwo";
    bind(two, GubsyButton::KB_J, Action::left); bind(two, GubsyButton::KB_L, Action::right);
    bind(two, GubsyButton::KB_I, Action::up); bind(two, GubsyButton::KB_K, Action::down);
    bind(two, GubsyButton::KB_U, Action::rotate_left); bind(two, GubsyButton::KB_O, Action::rotate_right);
    bind(two, GubsyButton::KB_P, Action::start); bind(two, GubsyButton::KB_Y, Action::select);
    bind_gamepad(two);

    if (gubsy_find_binds_profile(runtime, one.id) == nullptr &&
        !gubsy_replace_binds_profile(runtime, one))
        return false;
    if (gubsy_find_binds_profile(runtime, two.id) == nullptr &&
        !gubsy_replace_binds_profile(runtime, two))
        return false;
    const int second = gubsy_add_lobby_local_player(runtime);
    return second == 1 &&
           gubsy_set_lobby_player_binds_profile(runtime, 0, one.id) &&
           gubsy_set_lobby_player_binds_profile(runtime, 1, two.id);
}

bool assign_gamepad(GubsyRuntime& runtime, int device_id) {
    const GubsyLobbyState& lobby = gubsy_get_lobby_state(runtime);
    if (assigned(lobby, device_id))
        return true;
    for (int player = 0; player < static_cast<int>(lobby.local_players.size()); ++player) {
        if (has_gamepad(lobby.local_players[static_cast<std::size_t>(player)]))
            continue;
        gubsy_toggle_lobby_player_device(
            runtime, player,
            GubsyLobbyDeviceAssignment{InputSourceType::Gamepad, device_id});
        return assigned(gubsy_get_lobby_state(runtime), device_id);
    }
    return false;
}

void assign_unclaimed_gamepads(GubsyRuntime& runtime) {
    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    if (gamepads == nullptr)
        return;
    for (int index = 0; index < count; ++index)
        (void)assign_gamepad(runtime, static_cast<int>(gamepads[index]));
    SDL_free(gamepads);
}

} // namespace tetris::app
