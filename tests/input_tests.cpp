#include "controls.hpp"
#include "game/state.hpp"
#include "gameplay_fixture.hpp"

#include <SDL3/SDL.h>
#include <ginput/io.hpp>
#include <gubsy/runtime.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

using namespace tetris;

namespace {

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

struct VirtualGamepad {
    SDL_JoystickID id{};
    SDL_Joystick* joystick{};

    explicit VirtualGamepad(const char* name) {
        SDL_VirtualJoystickDesc description{};
        SDL_INIT_INTERFACE(&description);
        description.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        description.naxes = SDL_GAMEPAD_AXIS_COUNT;
        description.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        description.name = name;
        id = SDL_AttachVirtualJoystick(&description);
        require(id != 0, SDL_GetError());
        joystick = SDL_OpenJoystick(id);
        require(joystick != nullptr, SDL_GetError());
    }

    VirtualGamepad(const VirtualGamepad&) = delete;
    VirtualGamepad& operator=(const VirtualGamepad&) = delete;

    ~VirtualGamepad() {
        if (joystick != nullptr)
            SDL_CloseJoystick(joystick);
        if (id != 0)
            (void)SDL_DetachVirtualJoystick(id);
    }

    void set(SDL_GamepadButton button, bool pressed) {
        require(SDL_SetJoystickVirtualButton(joystick, button, pressed), SDL_GetError());
        SDL_UpdateJoysticks();
    }

    void set(SDL_GamepadAxis axis, Sint16 value) {
        require(SDL_SetJoystickVirtualAxis(joystick, axis, value), SDL_GetError());
        SDL_UpdateJoysticks();
    }

    void detach() {
        if (joystick != nullptr) {
            SDL_CloseJoystick(joystick);
            joystick = nullptr;
        }
        require(SDL_DetachVirtualJoystick(id), SDL_GetError());
        id = 0;
    }
};

void pump_events(GubsyRuntime& runtime) {
    SDL_Event event;
    while (SDL_PollEvent(&event))
        gubsy_process_sdl_event(runtime, event);
    gubsy_update_device_state(runtime);
}

void clear_gamepad_assignments(GubsyRuntime& runtime) {
    for (int player = 0; player < 2; ++player) {
        const GubsyLobbyState& lobby = gubsy_get_lobby_state(runtime);
        require(player < static_cast<int>(lobby.local_players.size()), "missing local player");
        const auto devices = lobby.local_players[static_cast<std::size_t>(player)].devices;
        for (GubsyLobbyDeviceAssignment device : devices) {
            if (device.type == InputSourceType::Gamepad)
                gubsy_toggle_lobby_player_device(runtime, player, device);
        }
    }
}

void expect_only(Buttons buttons, Action action) {
    require(buttons.left == (action == Action::left), "left action mismatch");
    require(buttons.right == (action == Action::right), "right action mismatch");
    require(buttons.up == (action == Action::up), "up action mismatch");
    require(buttons.down == (action == Action::down), "down action mismatch");
    require(buttons.back == (action == Action::back), "Back action mismatch");
    require(buttons.confirm == (action == Action::confirm), "Confirm action mismatch");
    require(buttons.rotate_counterclockwise ==
                (action == Action::rotate_counterclockwise),
            "counterclockwise action mismatch");
    require(buttons.rotate_clockwise == (action == Action::rotate_clockwise),
            "clockwise action mismatch");
    require(buttons.hard_drop == (action == Action::hard_drop), "drop action mismatch");
    require(buttons.hold == (action == Action::hold), "hold action mismatch");
    require(buttons.restart == (action == Action::restart), "restart action mismatch");
    require(buttons.start == (action == Action::start), "start action mismatch");
    require(buttons.select == (action == Action::select), "select action mismatch");
}

void verify_buttons(GubsyRuntime& runtime, VirtualGamepad& gamepad, int player,
                    SDL_GamepadButton button, Buttons expected) {
    gamepad.set(button, true);
    pump_events(runtime);
    require(read_buttons(runtime, player) == expected, "controller action set mismatch");
    gamepad.set(button, false);
    pump_events(runtime);
    require(read_buttons(runtime, player) == Buttons{}, "released button stayed active");
}

void verify_button(GubsyRuntime& runtime, VirtualGamepad& gamepad, int player,
                   SDL_GamepadButton button, Action action) {
    gamepad.set(button, true);
    pump_events(runtime);
    expect_only(read_buttons(runtime, player), action);
    gamepad.set(button, false);
    pump_events(runtime);
    require(read_buttons(runtime, player) == Buttons{}, "released button stayed active");
}

void verify_trigger(GubsyRuntime& runtime, VirtualGamepad& gamepad, int player,
                    SDL_GamepadAxis axis) {
    gamepad.set(axis, SDL_MAX_SINT16);
    pump_events(runtime);
    expect_only(read_buttons(runtime, player), Action::hard_drop);
    // Virtual joystick trigger axes use the full signed range; SDL's gamepad
    // mapping converts the signed minimum to the trigger's zero position.
    gamepad.set(axis, SDL_MIN_SINT16);
    pump_events(runtime);
    require(read_buttons(runtime, player) == Buttons{}, "released trigger stayed active");
}

GameInput game_input(Buttons buttons) {
    GameInput input;
    input.player_one = buttons;
    return input;
}

void controller_press(GubsyRuntime& runtime, VirtualGamepad& gamepad, GameState& state,
                      SDL_GamepadButton button) {
    gamepad.set(button, true);
    pump_events(runtime);
    step_game(state, game_input(read_buttons(runtime, 0)));
    gamepad.set(button, false);
    pump_events(runtime);
    step_game(state, game_input(read_buttons(runtime, 0)));
}

void verify_controller_only_flow(GubsyRuntime& runtime, VirtualGamepad& gamepad) {
    GameState state;
    start_game(state, {.gameplay = tetris::test::gameplay_data(),
                       .type_a_demo = {},
                       .type_b_demo = {},
                       .demo_pieces = {},
                       .type_b_demo_garbage = {}});
    require(state.screen == Screen::title, "controller flow did not begin at the title");
    controller_press(runtime, gamepad, state, SDL_GAMEPAD_BUTTON_START);
    require(state.screen == Screen::game_type, "controller could not enter game menu");
    controller_press(runtime, gamepad, state, SDL_GAMEPAD_BUTTON_SOUTH);
    require(state.screen == Screen::music, "controller A could not select game type");
    controller_press(runtime, gamepad, state, SDL_GAMEPAD_BUTTON_SOUTH);
    require(state.screen == Screen::type_a_level, "controller A could not select music");
    controller_press(runtime, gamepad, state, SDL_GAMEPAD_BUTTON_SOUTH);
    require(state.screen == Screen::gameplay, "controller could not start gameplay");
}

void verify_binding_management(GubsyRuntime& runtime, VirtualGamepad& gamepad) {
    const GubsyLobbyState& lobby = gubsy_get_lobby_state(runtime);
    const int profile_id = lobby.local_players[0].binds_profile_id;
    const BindsProfile* source = gubsy_find_binds_profile(runtime, profile_id);
    require(source != nullptr, "player-one binding profile is missing");

    BindsProfile edited = *source;
    remove_binds_for_action(edited, BindsActionType::Button,
                            static_cast<int>(Action::rotate_clockwise));
    bind_button(edited, GubsyButton::GP_MISC1,
                static_cast<int>(Action::rotate_clockwise));
    require(gubsy_replace_binds_profile(runtime, edited), "could not save edited bindings");
    verify_button(runtime, gamepad, 0, SDL_GAMEPAD_BUTTON_MISC1, Action::rotate_clockwise);

    require(reset_controls(runtime, 0), "could not restore default bindings");
    verify_buttons(runtime, gamepad, 0, SDL_GAMEPAD_BUTTON_SOUTH,
                   {.confirm = true, .rotate_counterclockwise = true});
    require(swap_rotation_controls(runtime, 0), "could not swap rotation directions");
    verify_buttons(runtime, gamepad, 0, SDL_GAMEPAD_BUTTON_SOUTH,
                   {.confirm = true, .rotate_clockwise = true});
    require(reset_controls(runtime, 0), "could not restore defaults after rotation swap");
}

bool assigned_to_any_player(GubsyRuntime& runtime, int device_id) {
    const GubsyLobbyState& lobby = gubsy_get_lobby_state(runtime);
    for (const GubsyLobbyPlayer& player : lobby.local_players) {
        for (GubsyLobbyDeviceAssignment device : player.devices) {
            if (device.type == InputSourceType::Gamepad && device.device_id == device_id)
                return true;
        }
    }
    return false;
}

bool assigned_to_player(GubsyRuntime& runtime, int device_id, int player) {
    const GubsyLobbyState& lobby = gubsy_get_lobby_state(runtime);
    if (player < 0 || player >= static_cast<int>(lobby.local_players.size()))
        return false;
    return std::ranges::any_of(
        lobby.local_players[static_cast<std::size_t>(player)].devices,
        [device_id](GubsyLobbyDeviceAssignment device) {
            return device.type == InputSourceType::Gamepad && device.device_id == device_id;
        });
}

bool profile_has_action(const BindsProfile& profile, Action action) {
    return std::ranges::any_of(profile.button_binds(), [action](const ginput::ButtonBind& bind) {
        return bind.action == static_cast<int>(action);
    });
}

bool profile_has_axis(const BindsProfile& profile, AnalogAction action) {
    return std::ranges::any_of(profile.axis_1d_binds(), [action](const ginput::Axis1DBind& bind) {
        return bind.axis_1d == static_cast<int>(action);
    });
}

} // namespace

int main() {
    GubsyRuntime runtime;
    bool initialized = false;
    bool sdl_initialized = false;
    try {
        require(SDL_InitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS),
                "could not initialize SDL gamepad input");
        sdl_initialized = true;
        GubsyAppConfig config;
        config.enable_mods = false;
        config.project_root = std::filesystem::current_path().string();
        const std::filesystem::path data_root =
            std::filesystem::temp_directory_path() / "tetris-controller-test";
        std::filesystem::remove_all(data_root);
        config.data_root = data_root.string();

        // Reproduce the profiles persisted by the Modern port before drop,
        // hold, and restart were added. Write the file before registering the
        // new schema, just as it exists at real application startup.
        BindsProfile reference_one;
        reference_one.id = 4101;
        reference_one.name = "TetrisPlayerOne";
        BindsProfile reference_two;
        reference_two.id = 4102;
        reference_two.name = "TetrisPlayerTwo";
        BindsProfile legacy_one;
        legacy_one.id = 4201;
        legacy_one.name = "TetrisModernPlayerOne";
        bind_button(legacy_one, GubsyButton::GP_Y, static_cast<int>(Action::confirm));
        BindsProfile legacy_two;
        legacy_two.id = 4202;
        legacy_two.name = "TetrisModernPlayerTwo";
        bind_button(legacy_two, GubsyButton::GP_Y, static_cast<int>(Action::confirm));
        const std::vector<BindsProfile> legacy_profiles = {
            reference_one, reference_two, legacy_one, legacy_two};
        require(ginput::save_profiles_file(data_root / "binds_profiles/default.lisp",
                                           legacy_profiles, "binds_profiles"),
                "could not create legacy binding profile fixture");

        require(init_gubsy_runtime(runtime, config), "could not initialize Gubsy");
        initialized = true;
        require(register_controls(runtime), "could not register Tetris controls");
        const GubsyLobbyState& registered_lobby = gubsy_get_lobby_state(runtime);
        const int active_profile_id = registered_lobby.local_players[0].binds_profile_id;
        const BindsProfile* active_profile =
            gubsy_find_binds_profile(runtime, active_profile_id);
        require(active_profile != nullptr && active_profile_id != 4201 &&
                    profile_has_action(*active_profile, Action::confirm) &&
                    profile_has_action(*active_profile, Action::rotate_counterclockwise) &&
                    profile_has_action(*active_profile, Action::rotate_clockwise) &&
                    profile_has_action(*active_profile, Action::hard_drop) &&
                    profile_has_action(*active_profile, Action::hold) &&
                    profile_has_action(*active_profile, Action::restart) &&
                    profile_has_axis(*active_profile, AnalogAction::drop),
                "versioned modern default profile was not installed completely");
        require(reset_controls(runtime, 0) && reset_controls(runtime, 1),
                "could not install complete default profiles after migration test");
        clear_gamepad_assignments(runtime);

        VirtualGamepad one("Tetris virtual player one");
        pump_events(runtime);
        require(assign_gamepad(runtime, static_cast<int>(one.id)),
                "could not assign player-one gamepad");

        VirtualGamepad two("Tetris virtual player two");
        pump_events(runtime);
        require(assign_gamepad(runtime, static_cast<int>(two.id)),
                "could not assign player-two gamepad");

        const std::vector<GubsyGamepad> detected = gubsy_get_gamepads(runtime);
        require(detected.size() >= 2, "connected gamepads were not exposed to the host UI");
        require(std::ranges::any_of(detected, [](const GubsyGamepad& gamepad) {
                    return gamepad.name == "Tetris virtual player one";
                }),
                "connected gamepad name was not exposed to the host UI");
        gubsy_refresh_gamepads(runtime);
        require(gubsy_get_gamepads(runtime).size() >= 2,
                "controller rescan lost connected gamepads");
        require(assigned_to_player(runtime, static_cast<int>(one.id), 0) &&
                    assigned_to_player(runtime, static_cast<int>(two.id), 1),
                "controller rescan lost stable player assignments");
        require(set_gamepad_player(runtime, static_cast<int>(two.id), 0) &&
                    assigned_to_player(runtime, static_cast<int>(two.id), 0) &&
                    !assigned_to_player(runtime, static_cast<int>(two.id), 1),
                "controller could not be reassigned through the controls UI");
        require(set_gamepad_player(runtime, static_cast<int>(two.id), 1),
                "controller could not be restored to player two");

        verify_button(runtime, one, 0, SDL_GAMEPAD_BUTTON_DPAD_LEFT, Action::left);
        verify_button(runtime, one, 0, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, Action::right);
        verify_buttons(runtime, one, 0, SDL_GAMEPAD_BUTTON_DPAD_UP,
                       {.up = true, .hard_drop = true});
        verify_button(runtime, one, 0, SDL_GAMEPAD_BUTTON_DPAD_DOWN, Action::down);
        verify_buttons(runtime, one, 0, SDL_GAMEPAD_BUTTON_SOUTH,
                       {.confirm = true, .rotate_counterclockwise = true});
        verify_buttons(runtime, one, 0, SDL_GAMEPAD_BUTTON_EAST,
                       {.back = true, .rotate_clockwise = true});
        verify_buttons(runtime, one, 0, SDL_GAMEPAD_BUTTON_NORTH,
                       {.hold = true, .restart = true});
        verify_button(runtime, one, 0, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, Action::hold);
        verify_button(runtime, one, 0, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, Action::hold);
        verify_button(runtime, one, 0, SDL_GAMEPAD_BUTTON_LEFT_STICK, Action::hold);
        verify_trigger(runtime, one, 0, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
        verify_trigger(runtime, one, 0, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
        verify_button(runtime, one, 0, SDL_GAMEPAD_BUTTON_START, Action::start);
        verify_buttons(runtime, one, 0, SDL_GAMEPAD_BUTTON_BACK,
                       {.restart = true, .select = true});
        one.set(SDL_GAMEPAD_BUTTON_RIGHT_STICK, true);
        pump_events(runtime);
        require(action_down(runtime, 0, Action::quit),
                "controller stick click did not map to Quit");
        one.set(SDL_GAMEPAD_BUTTON_RIGHT_STICK, false);
        pump_events(runtime);

        verify_binding_management(runtime, one);

        two.set(SDL_GAMEPAD_BUTTON_EAST, true);
        pump_events(runtime);
        require(read_buttons(runtime, 1) == Buttons{.back = true, .rotate_clockwise = true},
                "player-two rotation bindings did not stay isolated");
        require(read_buttons(runtime, 0) == Buttons{}, "player two leaked into player one");
        two.set(SDL_GAMEPAD_BUTTON_EAST, false);
        pump_events(runtime);

        verify_controller_only_flow(runtime, one);

        const int detached_id = static_cast<int>(two.id);
        two.detach();
        pump_events(runtime);
        require(!assigned_to_any_player(runtime, detached_id),
                "detached gamepad remained assigned");
        VirtualGamepad replacement("Tetris replacement player two");
        pump_events(runtime);
        require(assign_gamepad(runtime, static_cast<int>(replacement.id)),
                "hot-plugged replacement gamepad could not be assigned");
        verify_buttons(runtime, replacement, 1, SDL_GAMEPAD_BUTTON_EAST,
                       {.back = true, .rotate_clockwise = true});

        cleanup_gubsy_runtime(runtime);
        initialized = false;
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS);
        sdl_initialized = false;
        std::puts("[input] controller-only state, hot-plug, and two-player "
                  "assignment passed");
        return 0;
    } catch (const std::exception& exception) {
        if (initialized)
            cleanup_gubsy_runtime(runtime);
        if (sdl_initialized)
            SDL_QuitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS);
        std::fprintf(stderr, "[input] %s\n", exception.what());
        return 1;
    }
}
