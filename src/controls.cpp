#include "controls.hpp"

#include <SDL3/SDL.h>
#include <gubsy/input/binds_profile.hpp>
#include <gubsy/runtime.hpp>

#include <algorithm>

namespace tetris {
namespace {

constexpr int player_one_profile = 4601;
constexpr int player_two_profile = 4602;
constexpr float trigger_threshold = 0.25F;

void bind(BindsProfile& profile, GubsyButton button, Action action) {
    bind_button(profile, button, static_cast<int>(action));
}

void bind(BindsProfile& profile, Gubsy1DAnalog axis, AnalogAction action) {
    bind_1d_analog(profile, axis, static_cast<int>(action));
}

void bind_gamepad(BindsProfile& profile) {
    // Menu A/B and modern rotations are separate actions. A physical button
    // may own one of each because menus and falling play consume them in
    // different contexts.
    bind(profile, GubsyButton::GP_DPAD_LEFT, Action::left);
    bind(profile, GubsyButton::GP_DPAD_RIGHT, Action::right);
    bind(profile, GubsyButton::GP_DPAD_UP, Action::up);
    bind(profile, GubsyButton::GP_DPAD_UP, Action::hard_drop);
    bind(profile, GubsyButton::GP_DPAD_DOWN, Action::down);
    bind(profile, GubsyButton::GP_A, Action::confirm);
    bind(profile, GubsyButton::GP_A, Action::rotate_counterclockwise);
    bind(profile, GubsyButton::GP_B, Action::back);
    bind(profile, GubsyButton::GP_B, Action::rotate_clockwise);
    bind(profile, GubsyButton::GP_Y, Action::hold);
    bind(profile, GubsyButton::GP_Y, Action::restart);
    bind(profile, GubsyButton::GP_LEFT_SHOULDER, Action::hold);
    bind(profile, GubsyButton::GP_RIGHT_SHOULDER, Action::hold);
    bind(profile, GubsyButton::GP_LEFT_STICK_BUTTON, Action::hold);
    bind(profile, Gubsy1DAnalog::GP_LEFT_TRIGGER, AnalogAction::drop);
    bind(profile, Gubsy1DAnalog::GP_RIGHT_TRIGGER, AnalogAction::drop);
    bind(profile, GubsyButton::GP_START, Action::start);
    bind(profile, GubsyButton::GP_BACK, Action::select);
    bind(profile, GubsyButton::GP_BACK, Action::restart);
    bind(profile, GubsyButton::GP_GUIDE, Action::quit);
    bind(profile, GubsyButton::GP_RIGHT_STICK_BUTTON, Action::quit);
}

bool has_action(const BindsProfile& profile, Action action) {
    return std::ranges::any_of(profile.button_binds(), [action](const ginput::ButtonBind& bind) {
        return bind.action == static_cast<int>(action);
    });
}

bool has_axis(const BindsProfile& profile, AnalogAction action) {
    return std::ranges::any_of(profile.axis_1d_binds(), [action](const ginput::Axis1DBind& bind) {
        return bind.axis_1d == static_cast<int>(action);
    });
}

bool install_or_migrate_profile(GubsyRuntime& runtime, const BindsProfile& defaults) {
    const BindsProfile* installed = gubsy_find_binds_profile(runtime, defaults.id);
    if (installed == nullptr)
        return gubsy_replace_binds_profile(runtime, defaults);

    // New actions are additive. Preserve every existing user binding and only
    // supply defaults for actions that predate the saved profile schema.
    BindsProfile migrated = *installed;
    bool changed = false;
    for (const Action action : {Action::hard_drop, Action::hold, Action::restart,
                                Action::rotate_counterclockwise,
                                Action::rotate_clockwise}) {
        if (has_action(migrated, action))
            continue;
        for (const ginput::ButtonBind& binding : defaults.button_binds()) {
            if (binding.action != static_cast<int>(action))
                continue;
            bind_button(migrated, binding.device_button, binding.action);
            changed = true;
        }
    }
    if (!has_axis(migrated, AnalogAction::drop)) {
        for (const ginput::Axis1DBind& binding : defaults.axis_1d_binds()) {
            if (binding.axis_1d != static_cast<int>(AnalogAction::drop))
                continue;
            bind_1d_analog(migrated, binding.device_axis, binding.axis_1d);
            changed = true;
        }
    }
    return !changed || gubsy_replace_binds_profile(runtime, migrated);
}

BindsProfile default_profile(int player) {
    BindsProfile profile;
    profile.id = player == 0 ? player_one_profile : player_two_profile;
    profile.name = player == 0 ? "TetrisModernPlayerOneV5" : "TetrisModernPlayerTwoV5";
    if (player == 0) {
        bind(profile, GubsyButton::KB_LEFT, Action::left);
        bind(profile, GubsyButton::KB_A, Action::left);
        bind(profile, GubsyButton::KB_RIGHT, Action::right);
        bind(profile, GubsyButton::KB_D, Action::right);
        bind(profile, GubsyButton::KB_UP, Action::up);
        bind(profile, GubsyButton::KB_UP, Action::hard_drop);
        bind(profile, GubsyButton::KB_W, Action::up);
        bind(profile, GubsyButton::KB_W, Action::hard_drop);
        bind(profile, GubsyButton::KB_DOWN, Action::down);
        bind(profile, GubsyButton::KB_S, Action::down);
        bind(profile, GubsyButton::KB_Z, Action::back);
        bind(profile, GubsyButton::KB_Z, Action::rotate_counterclockwise);
        bind(profile, GubsyButton::KB_X, Action::confirm);
        bind(profile, GubsyButton::KB_X, Action::rotate_clockwise);
        bind(profile, GubsyButton::KB_SPACE, Action::hard_drop);
        bind(profile, GubsyButton::KB_C, Action::hold);
        bind(profile, GubsyButton::KB_R, Action::restart);
        bind(profile, GubsyButton::KB_ENTER, Action::start);
        bind(profile, GubsyButton::KB_BACKSPACE, Action::select);
        bind(profile, GubsyButton::KB_ESCAPE, Action::quit);
    } else {
        bind(profile, GubsyButton::KB_J, Action::left);
        bind(profile, GubsyButton::KB_L, Action::right);
        bind(profile, GubsyButton::KB_I, Action::up);
        bind(profile, GubsyButton::KB_I, Action::hard_drop);
        bind(profile, GubsyButton::KB_K, Action::down);
        bind(profile, GubsyButton::KB_U, Action::back);
        bind(profile, GubsyButton::KB_U, Action::rotate_counterclockwise);
        bind(profile, GubsyButton::KB_O, Action::confirm);
        bind(profile, GubsyButton::KB_O, Action::rotate_clockwise);
        bind(profile, GubsyButton::KB_H, Action::hard_drop);
        bind(profile, GubsyButton::KB_T, Action::hold);
        bind(profile, GubsyButton::KB_N, Action::restart);
        bind(profile, GubsyButton::KB_P, Action::start);
        bind(profile, GubsyButton::KB_Y, Action::select);
    }
    bind_gamepad(profile);
    return profile;
}

bool has_gamepad(const GubsyLobbyPlayer& player) {
    return std::ranges::any_of(player.devices, [](GubsyLobbyDeviceAssignment device) {
        return device.type == InputSourceType::Gamepad;
    });
}

bool assigned(const GubsyLobbyState& lobby, int device_id) {
    for (const GubsyLobbyPlayer& player : lobby.local_players) {
        const bool found =
            std::ranges::any_of(player.devices, [device_id](GubsyLobbyDeviceAssignment device) {
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

bool analog_down(GubsyRuntime& runtime, int player, AnalogAction action) {
    return gubsy_lobby_player_axis_1d_down(runtime, player, static_cast<int>(action),
                                           trigger_threshold);
}

Buttons read_buttons(GubsyRuntime& runtime, int player) {
    return {
        .left = action_down(runtime, player, Action::left),
        .right = action_down(runtime, player, Action::right),
        .up = action_down(runtime, player, Action::up),
        .down = action_down(runtime, player, Action::down),
        .back = action_down(runtime, player, Action::back),
        .confirm = action_down(runtime, player, Action::confirm),
        .rotate_counterclockwise =
            action_down(runtime, player, Action::rotate_counterclockwise),
        .rotate_clockwise = action_down(runtime, player, Action::rotate_clockwise),
        .hard_drop = action_down(runtime, player, Action::hard_drop) ||
                analog_down(runtime, player, AnalogAction::drop),
        .hold = action_down(runtime, player, Action::hold),
        .restart = action_down(runtime, player, Action::restart),
        .start = action_down(runtime, player, Action::start),
        .select = action_down(runtime, player, Action::select),
    };
}

bool register_controls(GubsyRuntime& runtime) {
    // Register the actions displayed by the persistent ImGui binding editor.
    BindsSchema schema;
    (void)schema.add_action(static_cast<int>(Action::left), "Left", "Game Boy");
    (void)schema.add_action(static_cast<int>(Action::right), "Right", "Game Boy");
    (void)schema.add_action(static_cast<int>(Action::up), "Up", "Game Boy");
    (void)schema.add_action(static_cast<int>(Action::down), "Down", "Game Boy");
    (void)schema.add_action(static_cast<int>(Action::back), "Back", "Menu");
    (void)schema.add_action(static_cast<int>(Action::confirm), "Confirm", "Menu");
    (void)schema.add_action(static_cast<int>(Action::rotate_counterclockwise),
                            "Rotate counterclockwise", "Modern");
    (void)schema.add_action(static_cast<int>(Action::rotate_clockwise), "Rotate clockwise",
                            "Modern");
    (void)schema.add_action(static_cast<int>(Action::hard_drop), "Hard / sonic drop", "Modern");
    (void)schema.add_action(static_cast<int>(Action::hold), "Hold", "Modern");
    (void)schema.add_action(static_cast<int>(Action::restart), "Quick restart", "Modern");
    (void)schema.add_action(static_cast<int>(Action::start), "Start", "Game Boy");
    (void)schema.add_action(static_cast<int>(Action::select), "Select", "Game Boy");
    (void)schema.add_action(static_cast<int>(Action::quit), "Quit", "Application");
    (void)schema.add_axis_1d(static_cast<int>(AnalogAction::drop), "Hard / sonic drop", "Modern");
    gubsy_register_binds_schema(runtime, schema);

    // Install separate persistent profiles for both local players.
    const BindsProfile one = default_profile(0);
    const BindsProfile two = default_profile(1);

    // Install missing profiles and attach them to two local lobby players.
    if (!install_or_migrate_profile(runtime, one))
        return false;
    if (!install_or_migrate_profile(runtime, two))
        return false;
    const int second = gubsy_add_lobby_local_player(runtime);
    return second == 1 && gubsy_set_lobby_player_binds_profile(runtime, 0, one.id) &&
           gubsy_set_lobby_player_binds_profile(runtime, 1, two.id);
}

bool reset_controls(GubsyRuntime& runtime, int player) {
    if (player < 0 || player > 1)
        return false;
    return gubsy_replace_binds_profile(runtime, default_profile(player));
}

bool swap_rotation_controls(GubsyRuntime& runtime, int player) {
    if (player < 0 || player > 1)
        return false;
    const GubsyLobbyState& lobby = gubsy_get_lobby_state(runtime);
    if (player >= static_cast<int>(lobby.local_players.size()))
        return false;
    const int profile_id = lobby.local_players[static_cast<std::size_t>(player)].binds_profile_id;
    const BindsProfile* source = gubsy_find_binds_profile(runtime, profile_id);
    if (source == nullptr)
        return false;

    // Snapshot both sets before removing them, then install each under the
    // opposite semantic action. Confirm and Back bindings remain untouched.
    std::vector<int> counterclockwise;
    std::vector<int> clockwise;
    for (const ginput::ButtonBind& binding : source->button_binds()) {
        if (binding.action == static_cast<int>(Action::rotate_counterclockwise))
            counterclockwise.push_back(binding.device_button);
        if (binding.action == static_cast<int>(Action::rotate_clockwise))
            clockwise.push_back(binding.device_button);
    }
    BindsProfile swapped = *source;
    remove_binds_for_action(swapped, BindsActionType::Button,
                            static_cast<int>(Action::rotate_counterclockwise));
    remove_binds_for_action(swapped, BindsActionType::Button,
                            static_cast<int>(Action::rotate_clockwise));
    for (const int button : counterclockwise)
        bind_button(swapped, button, static_cast<int>(Action::rotate_clockwise));
    for (const int button : clockwise)
        bind_button(swapped, button, static_cast<int>(Action::rotate_counterclockwise));
    return gubsy_replace_binds_profile(runtime, swapped);
}

bool assign_gamepad(GubsyRuntime& runtime, int device_id) {
    // Preserve existing assignments before filling the first free player slot.
    const GubsyLobbyState& lobby = gubsy_get_lobby_state(runtime);
    if (assigned(lobby, device_id))
        return true;
    for (int player = 0; player < static_cast<int>(lobby.local_players.size()); ++player) {
        if (has_gamepad(lobby.local_players[static_cast<std::size_t>(player)]))
            continue;
        gubsy_toggle_lobby_player_device(
            runtime, player, GubsyLobbyDeviceAssignment{InputSourceType::Gamepad, device_id});
        return assigned(gubsy_get_lobby_state(runtime), device_id);
    }
    return false;
}

bool set_gamepad_player(GubsyRuntime& runtime, int device_id, int player) {
    const std::vector<GubsyGamepad> gamepads = gubsy_get_gamepads(runtime);
    const bool connected = std::ranges::any_of(gamepads, [device_id](const GubsyGamepad& gamepad) {
        return gamepad.device_id == device_id;
    });
    const GubsyLobbyState& initial_lobby = gubsy_get_lobby_state(runtime);
    if (!connected || player < -1 || player >= static_cast<int>(initial_lobby.local_players.size()))
        return false;

    // A physical controller belongs to at most one local player.
    for (int index = 0; index < static_cast<int>(initial_lobby.local_players.size()); ++index) {
        const GubsyLobbyState& lobby = gubsy_get_lobby_state(runtime);
        if (!assigned(lobby, device_id))
            break;
        const GubsyLobbyPlayer& local = lobby.local_players[static_cast<std::size_t>(index)];
        const bool belongs_here = std::ranges::any_of(
            local.devices, [device_id](GubsyLobbyDeviceAssignment device) {
                return device.type == InputSourceType::Gamepad && device.device_id == device_id;
            });
        if (belongs_here && index != player) {
            gubsy_toggle_lobby_player_device(
                runtime, index,
                GubsyLobbyDeviceAssignment{InputSourceType::Gamepad, device_id});
        }
    }

    if (player < 0)
        return !assigned(gubsy_get_lobby_state(runtime), device_id);
    if (!assigned(gubsy_get_lobby_state(runtime), device_id)) {
        gubsy_toggle_lobby_player_device(
            runtime, player, GubsyLobbyDeviceAssignment{InputSourceType::Gamepad, device_id});
    }
    const GubsyLobbyState& lobby = gubsy_get_lobby_state(runtime);
    const GubsyLobbyPlayer& local = lobby.local_players[static_cast<std::size_t>(player)];
    return std::ranges::any_of(local.devices, [device_id](GubsyLobbyDeviceAssignment device) {
        return device.type == InputSourceType::Gamepad && device.device_id == device_id;
    });
}

void assign_unclaimed_gamepads(GubsyRuntime& runtime) {
    // Assign every SDL gamepad that Gubsy has not already claimed.
    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    if (gamepads == nullptr)
        return;
    for (int index = 0; index < count; ++index)
        (void)assign_gamepad(runtime, static_cast<int>(gamepads[index]));
    SDL_free(gamepads);
}

} // namespace tetris
