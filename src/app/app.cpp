#include "app/app.hpp"

#include "app/debug_ui.hpp"
#include "audio/output.hpp"
#include "game/flow.hpp"
#include "presentation/effects.hpp"
#include "presentation/renderer.hpp"
#include "presentation/settings.hpp"
#include "src/imgui_layer.hpp"

#include <SDL3/SDL.h>
#include <gubsy/input/binds_profile.hpp>
#include <gubsy/runtime.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <vector>

namespace tetris::app {
namespace {

constexpr double ticks_per_second = 59.7275;
constexpr double fixed_step = 1.0 / ticks_per_second;
constexpr int player_one_profile = 4101;
constexpr int player_two_profile = 4102;

enum class Action : int {
    left = 1, right, up, down, rotate_left, rotate_right, start, select, quit,
};

struct Replay {
    std::optional<GameFlow> initial;
    std::vector<FlowInput> inputs;
    std::size_t position{};
    std::uint8_t divider{};
    std::uint8_t final_divider{};
    bool recording{};
    bool playing{};
};

struct State {
    GameFlow flow;
    presentation::Settings settings;
    presentation::EffectState effects;
    DebugUi debug;
    Replay replay;
    audio::Output audio;
    std::uint8_t divider{1};
};

bool down(GubsyRuntime& runtime, int player, Action action) {
    return gubsy_lobby_player_action_down(runtime, player, static_cast<int>(action));
}

Buttons read_buttons(GubsyRuntime& runtime, int player) {
    return {
        .left = down(runtime, player, Action::left),
        .right = down(runtime, player, Action::right),
        .up = down(runtime, player, Action::up),
        .down = down(runtime, player, Action::down),
        .rotate_left = down(runtime, player, Action::rotate_left),
        .rotate_right = down(runtime, player, Action::rotate_right),
        .start = down(runtime, player, Action::start),
        .select = down(runtime, player, Action::select),
    };
}

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

    if (!gubsy_replace_binds_profile(runtime, one) || !gubsy_replace_binds_profile(runtime, two))
        return false;
    const int second = gubsy_add_lobby_local_player(runtime);
    return second == 1 &&
           gubsy_set_lobby_player_binds_profile(runtime, 0, one.id) &&
           gubsy_set_lobby_player_binds_profile(runtime, 1, two.id);
}

void assign_gamepads(GubsyRuntime& runtime) {
    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    if (gamepads == nullptr)
        return;
    for (int index = 0; index < std::min(count, 2); ++index) {
        gubsy_toggle_lobby_player_device(runtime, index,
            GubsyLobbyDeviceAssignment{InputSourceType::Gamepad, static_cast<int>(gamepads[index])});
    }
    SDL_free(gamepads);
}

std::uint8_t random_byte(State& state) {
    state.divider = static_cast<std::uint8_t>(state.divider * 17U + 31U);
    return state.divider;
}

RandomSamples random_samples(State& state) {
    return {random_byte(state), random_byte(state), random_byte(state)};
}

StartupRandom startup_random(State& state) {
    StartupRandom random;
    for (RandomSamples& samples : random.pieces)
        samples = random_samples(state);
    for (GarbageRandom& cell : random.garbage)
        cell = {random_byte(state), random_byte(state)};
    return random;
}

VersusRandom versus_random(State& state) {
    std::array<RandomSamples, versus_piece_count + 3> samples{};
    for (RandomSamples& sample : samples)
        sample = random_samples(state);
    VersusRandom random;
    random.pieces = make_versus_piece_sequence(samples);
    for (GarbageRandom& cell : random.garbage)
        cell = {random_byte(state), random_byte(state)};
    return random;
}

FlowInput make_input(State& state, Buttons one, Buttons two) {
    return {one, two, random_samples(state), startup_random(state), versus_random(state)};
}

void initialize(State& state, const content::Catalog& content) {
    state.flow.start({content.type_a_demo.runs, content.type_b_demo.runs,
                      content.demo_pieces, content.type_b_demo_garbage.bytes});
    state.flow.set_line_clear_speed(state.settings.line_clear_speed);
    state.effects = {};
}

void process_replay(State& state) {
    const ReplayRequest request = state.debug.replay_request;
    state.debug.replay_request = ReplayRequest::none;
    if (request == ReplayRequest::record) {
        state.replay.initial = state.flow;
        state.replay.inputs.clear();
        state.replay.position = 0;
        state.replay.divider = state.divider;
        state.replay.recording = true;
        state.replay.playing = false;
    } else if (request == ReplayRequest::stop) {
        state.replay.recording = false;
        state.replay.playing = false;
        state.replay.final_divider = state.divider;
    } else if (request == ReplayRequest::play && state.replay.initial && !state.replay.inputs.empty()) {
        state.flow = *state.replay.initial;
        state.divider = state.replay.divider;
        state.replay.position = 0;
        state.replay.recording = false;
        state.replay.playing = true;
    } else if (request == ReplayRequest::clear) {
        state.replay = {};
    }
}

void step(State& state, Buttons one, Buttons two) {
    if (state.replay.playing) {
        if (state.replay.position == state.replay.inputs.size()) {
            state.replay.playing = false;
            state.divider = state.replay.final_divider;
            return;
        }
        state.flow.tick(state.replay.inputs[state.replay.position++]);
    } else {
        FlowInput input = make_input(state, one, two);
        state.flow.tick(input);
        if (state.replay.recording) {
            state.replay.inputs.push_back(input);
            state.replay.position = state.replay.inputs.size();
            state.replay.final_divider = state.divider;
        }
    }
    presentation::advance(state.effects, state.flow.game().events(), state.settings);
    state.audio.tick(state.flow, state.divider);
}

bool place_window(SDL_Window* window) {
    if (window == nullptr)
        return false;
    (void)SDL_SetWindowFullscreen(window, false);
    (void)SDL_SetWindowSize(window, 640, 576);
    (void)SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    return SDL_SyncWindow(window);
}

} // namespace

int run(const content::Rom& rom, const content::Catalog& content, int frame_limit) {
    const std::filesystem::path data_root =
        std::filesystem::path(TETRIS_SOURCE_DIR) / "data" / "runtime";
    std::error_code error;
    std::filesystem::create_directories(data_root, error);

    GubsyAppConfig config;
    config.enable_mods = false;
    config.project_root = TETRIS_SOURCE_DIR;
    config.data_root = data_root.string();
    config.engine_assets_root = TETRIS_GUBSY_ASSETS_DIR;
    config.window_title = "Native GB Tetris Modern";
    config.window_width = 640;
    config.window_height = 576;
    config.render_width = 640;
    config.render_height = 576;
    config.utility_window = true;
    config.resizable_window = true;

    GubsyRuntime runtime;
    if (!init_gubsy_runtime(runtime, config) || !gubsy_init_sdl_renderer(runtime)) {
        std::fprintf(stderr, "could not initialize Gubsy/SDL3: %s\n", SDL_GetError());
        return 1;
    }
    GubsyFrame frame = gubsy_get_frame(runtime);
    if (!place_window(frame.window) || !register_controls(runtime) ||
        !init_imgui_layer(frame.window, frame.renderer)) {
        std::fprintf(stderr, "could not initialize window, controls, or ImGui\n");
        return 1;
    }
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    assign_gamepads(runtime);

    presentation::Renderer video;
    if (!video.initialize(frame.renderer, content)) {
        std::fprintf(stderr, "could not initialize ROM graphics\n");
        shutdown_imgui_layer();
        return 1;
    }
    const std::string title = "Native GB Tetris Modern - " + rom.digest;
    (void)SDL_SetWindowTitle(frame.window, title.c_str());

    State state;
    std::string settings_error;
    const std::filesystem::path settings_path = data_root / "enhancements.cfg";
    (void)presentation::load_settings(settings_path, state.settings, settings_error);
    initialize(state, content);
    if (!state.audio.initialize(content.audio))
        std::fprintf(stderr, "audio output unavailable: %s\n", SDL_GetError());
    state.audio.set_volume(state.settings.music_volume, state.settings.effects_volume);
    state.debug.visible = frame_limit > 0;

    bool running = true;
    double accumulator = 0;
    std::uint64_t previous_time = SDL_GetTicksNS();
    int rendered = 0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            gubsy_process_sdl_event(runtime, event);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_F1)
                state.debug.visible = !state.debug.visible;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_F11) {
                const bool fullscreen = (SDL_GetWindowFlags(frame.window) & SDL_WINDOW_FULLSCREEN) != 0;
                (void)SDL_SetWindowFullscreen(frame.window, !fullscreen);
            }
        }
        imgui_new_frame();
        gubsy_update_device_state(runtime);
        Buttons one = read_buttons(runtime, 0);
        Buttons two = read_buttons(runtime, 1);
        if (down(runtime, 0, Action::quit))
            running = false;
        if (imgui_want_capture_keyboard()) {
            one = {};
            two = {};
        }

        process_replay(state);
        const std::uint64_t now = SDL_GetTicksNS();
        const double elapsed = std::clamp(static_cast<double>(now - previous_time) / 1.0e9, 0.0, 0.25);
        previous_time = now;
        accumulator += elapsed;
        while (accumulator >= fixed_step) {
            if (!state.debug.paused || state.debug.step) {
                step(state, one, two);
                state.debug.step = false;
            }
            accumulator -= fixed_step;
        }

        gubsy_update_runtime(runtime, static_cast<float>(elapsed));
        frame = gubsy_get_frame(runtime);
        video.draw(frame.renderer, frame, content, state.flow, state.settings,
                   state.effects, state.debug.view);
        state.debug.replay_recording = state.replay.recording;
        state.debug.replay_playing = state.replay.playing;
        state.debug.replay_position = state.replay.position;
        state.debug.replay_size = state.replay.inputs.size();
        if (draw_debug_ui(state.debug, state.flow, state.settings, content, state.audio)) {
            state.flow.set_line_clear_speed(state.settings.line_clear_speed);
            state.audio.set_volume(state.settings.music_volume, state.settings.effects_volume);
            (void)presentation::save_settings(settings_path, state.settings, settings_error);
        }
        if (!gubsy_draw_frame_to_window(runtime))
            running = false;
        imgui_render_layer();
        gubsy_present_frame(runtime);
        ++rendered;
        if (frame_limit > 0 && rendered >= frame_limit)
            running = false;
    }
    video.shutdown();
    state.audio.shutdown();
    shutdown_imgui_layer();
    return 0;
}

} // namespace tetris::app
