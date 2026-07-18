#include "app/input.hpp"
#include "game/flow.hpp"

#include <SDL3/SDL.h>
#include <gubsy/runtime.hpp>

#include <cstdio>
#include <filesystem>
#include <stdexcept>

using namespace tetris;
using namespace tetris::app;

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
        require(player < static_cast<int>(lobby.local_players.size()),
                "missing local player");
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
    require(buttons.rotate_left == (action == Action::rotate_left), "B action mismatch");
    require(buttons.rotate_right == (action == Action::rotate_right), "A action mismatch");
    require(buttons.start == (action == Action::start), "start action mismatch");
    require(buttons.select == (action == Action::select), "select action mismatch");
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

FlowInput flow_input(Buttons buttons) {
    FlowInput input;
    input.player_one = buttons;
    return input;
}

void controller_press(GubsyRuntime& runtime, VirtualGamepad& gamepad,
                      GameFlow& flow, SDL_GamepadButton button) {
    gamepad.set(button, true);
    pump_events(runtime);
    flow.tick(flow_input(read_buttons(runtime, 0)));
    gamepad.set(button, false);
    pump_events(runtime);
    flow.tick(flow_input(read_buttons(runtime, 0)));
}

void verify_controller_only_flow(GubsyRuntime& runtime, VirtualGamepad& gamepad) {
    GameFlow flow;
    flow.start({});
    for (int frame = 0; frame < 250; ++frame)
        flow.tick(flow_input(read_buttons(runtime, 0)));
    require(flow.screen() == Screen::copyright_skippable,
            "controller flow did not reach skippable copyright screen");
    controller_press(runtime, gamepad, flow, SDL_GAMEPAD_BUTTON_SOUTH);
    require(flow.screen() == Screen::title, "controller could not dismiss copyright screen");
    controller_press(runtime, gamepad, flow, SDL_GAMEPAD_BUTTON_START);
    require(flow.screen() == Screen::game_type, "controller could not enter game menu");
    controller_press(runtime, gamepad, flow, SDL_GAMEPAD_BUTTON_SOUTH);
    require(flow.screen() == Screen::music, "controller A could not select game type");
    controller_press(runtime, gamepad, flow, SDL_GAMEPAD_BUTTON_SOUTH);
    require(flow.screen() == Screen::type_a_level, "controller A could not select music");
    controller_press(runtime, gamepad, flow, SDL_GAMEPAD_BUTTON_SOUTH);
    require(flow.screen() == Screen::gameplay, "controller could not start gameplay");
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
        require(init_gubsy_runtime(runtime, config), "could not initialize Gubsy");
        initialized = true;
        BindsProfile reference_one;
        reference_one.id = 4101;
        reference_one.name = "TetrisPlayerOne";
        BindsProfile reference_two;
        reference_two.id = 4102;
        reference_two.name = "TetrisPlayerTwo";
        require(gubsy_replace_binds_profile(runtime, reference_one) &&
                    gubsy_replace_binds_profile(runtime, reference_two),
                "could not create reference-port profile fixtures");
        require(register_controls(runtime), "could not register Tetris controls");
        clear_gamepad_assignments(runtime);

        VirtualGamepad one("Tetris virtual player one");
        pump_events(runtime);
        require(assign_gamepad(runtime, static_cast<int>(one.id)),
                "could not assign player-one gamepad");

        VirtualGamepad two("Tetris virtual player two");
        pump_events(runtime);
        require(assign_gamepad(runtime, static_cast<int>(two.id)),
                "could not assign player-two gamepad");

        verify_button(runtime, one, 0, SDL_GAMEPAD_BUTTON_DPAD_LEFT, Action::left);
        verify_button(runtime, one, 0, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, Action::right);
        verify_button(runtime, one, 0, SDL_GAMEPAD_BUTTON_DPAD_UP, Action::up);
        verify_button(runtime, one, 0, SDL_GAMEPAD_BUTTON_DPAD_DOWN, Action::down);
        verify_button(runtime, one, 0, SDL_GAMEPAD_BUTTON_WEST, Action::rotate_left);
        verify_button(runtime, one, 0, SDL_GAMEPAD_BUTTON_SOUTH, Action::rotate_right);
        verify_button(runtime, one, 0, SDL_GAMEPAD_BUTTON_START, Action::start);
        verify_button(runtime, one, 0, SDL_GAMEPAD_BUTTON_BACK, Action::select);
        one.set(SDL_GAMEPAD_BUTTON_RIGHT_STICK, true);
        pump_events(runtime);
        require(action_down(runtime, 0, Action::quit), "controller stick click did not map to Quit");
        one.set(SDL_GAMEPAD_BUTTON_RIGHT_STICK, false);
        pump_events(runtime);

        two.set(SDL_GAMEPAD_BUTTON_SOUTH, true);
        pump_events(runtime);
        expect_only(read_buttons(runtime, 1), Action::rotate_right);
        require(read_buttons(runtime, 0) == Buttons{}, "player two leaked into player one");
        two.set(SDL_GAMEPAD_BUTTON_SOUTH, false);
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
        verify_button(runtime, replacement, 1, SDL_GAMEPAD_BUTTON_SOUTH,
                      Action::rotate_right);

        cleanup_gubsy_runtime(runtime);
        initialized = false;
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS);
        sdl_initialized = false;
        std::puts("[input] controller-only flow, hot-plug, and two-player assignment passed");
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
