#include "app/app.hpp"

#include "app/controls_menu.hpp"
#include "app/debug_ui.hpp"
#include "app/input.hpp"
#include "app/persistence.hpp"
#include "audio/output.hpp"
#include "game/flow.hpp"
#include "game/replay.hpp"
#include "presentation/effects.hpp"
#include "presentation/renderer.hpp"
#include "presentation/settings.hpp"
#include "src/imgui_layer.hpp"

#include <SDL3/SDL.h>
#include <gubsy/runtime.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <utility>

namespace tetris::app {
namespace {

constexpr double ticks_per_second = 59.7275;
constexpr double fixed_step = 1.0 / ticks_per_second;
struct State {
    GameFlow flow;
    presentation::Settings settings;
    presentation::EffectState effects;
    DebugUi debug;
    ControlsMenu controls;
    Replay replay;
    audio::Output audio;
    FlowInput last_input{};
    std::uint8_t divider{1};
};

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

void process_replay(State& state, const std::string& rom_sha1) {
    const ReplayRequest request = state.debug.replay_request;
    state.debug.replay_request = ReplayRequest::none;
    if (request == ReplayRequest::record) {
        state.replay.begin_recording(
            state.flow,
            ReplayIdentity{.rom_sha1 = rom_sha1,
                           .pacing = state.flow.line_clear_speed(),
                           .starting_screen = state.flow.screen(),
                           .rules = state.flow.game().rules()},
            state.divider);
    } else if (request == ReplayRequest::stop) {
        state.replay.stop(state.divider);
    } else if (request == ReplayRequest::play) {
        (void)state.replay.rewind(state.flow, state.divider);
    } else if (request == ReplayRequest::clear) {
        state.replay.clear();
    }
}

SinglePlayer& command_game(State& state, std::uint32_t player) {
    if (state.flow.screen() == Screen::versus_gameplay)
        return state.flow.edit_versus().edit_player(player == 0 ? 0 : 1);
    return state.flow.edit_game();
}

void apply_debug_command(State& state) {
    const DebugCommand command = state.debug.command;
    state.debug.command = {};
    if (command.type == DebugCommandType::none)
        return;
    state.replay.stop(state.divider);
    if (command.type == DebugCommandType::start_type_a ||
        command.type == DebugCommandType::start_type_b) {
        const GameType type = command.type == DebugCommandType::start_type_a
                                  ? GameType::type_a
                                  : GameType::type_b;
        state.flow.start_game_for_test(
            {.type = type, .starting_level = command.first,
             .type_b_height = type == GameType::type_b ? command.second : 0},
            startup_random(state));
        state.flow.set_line_clear_speed(state.settings.line_clear_speed);
        return;
    }

    const std::uint32_t player = command.type == DebugCommandType::toggle_cell
                                     ? command.value
                                     : 0;
    SinglePlayer& game = command_game(state, player);
    if (command.type == DebugCommandType::clear_board) {
        game.edit_board().clear();
    } else if (command.type == DebugCommandType::prepare_tetris) {
        game.edit_board().clear();
        for (int row = 14; row < board_height; ++row) {
            for (int column = 0; column < board_width; ++column) {
                if (column != 6)
                    game.edit_board().set({column, row}, Block::j);
            }
        }
        game.place_piece_for_test({.kind = PieceKind::I, .rotation = Rotation::right,
                                   .origin = {5, 14}});
    } else if (command.type == DebugCommandType::toggle_cell) {
        const Cell cell{command.first, command.second};
        const Block next = game.board().at(cell) == Block::empty ? Block::t : Block::empty;
        game.edit_board().set(cell, next);
    } else if (command.type == DebugCommandType::set_score) {
        game.set_score_for_test(command.value);
    } else if (command.type == DebugCommandType::force_game_over) {
        game.set_state_for_test(PlayState::game_over);
    } else if (command.type == DebugCommandType::force_complete) {
        game.set_state_for_test(PlayState::complete);
    }
}

void step(State& state, Buttons one, Buttons two) {
    if (state.replay.playing()) {
        const std::optional<FlowInput> input = state.replay.next(state.divider);
        if (!input)
            return;
        state.last_input = *input;
        state.flow.tick(*input);
    } else {
        FlowInput input = make_input(state, one, two);
        state.last_input = input;
        state.flow.tick(input);
        state.replay.append(std::move(input), state.divider);
    }
    if (state.flow.screen() == Screen::versus_gameplay ||
        state.flow.screen() == Screen::versus_round_result ||
        state.flow.screen() == Screen::versus_match_result) {
        presentation::advance(state.effects, state.flow.versus().player(0).events(),
                              state.flow.versus().player(1).events(), state.settings);
    } else {
        presentation::advance(state.effects, state.flow.game().events(), state.settings);
    }
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

void apply_display_request(SDL_Window* window, DisplayRequest request) {
    if (window == nullptr || request == DisplayRequest::none)
        return;
    if (request == DisplayRequest::toggle_fullscreen) {
        const bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
        (void)SDL_SetWindowFullscreen(window, !fullscreen);
        return;
    }
    (void)SDL_SetWindowFullscreen(window, false);
    if (request == DisplayRequest::window_640)
        (void)SDL_SetWindowSize(window, 640, 576);
    else if (request == DisplayRequest::window_1280)
        (void)SDL_SetWindowSize(window, 1280, 720);
    else if (request == DisplayRequest::window_1920)
        (void)SDL_SetWindowSize(window, 1920, 1080);
    (void)SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

} // namespace

int run(const content::Rom& rom, const content::Catalog& content, int frame_limit) {
    const std::filesystem::path data_root =
        std::filesystem::path(TETRIS_SOURCE_DIR) / "data" / "runtime";
    std::error_code error;
    std::filesystem::create_directories(data_root, error);
    if (error) {
        std::fprintf(stderr, "could not create runtime data directory: %s\n",
                     error.message().c_str());
        return 1;
    }

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
    if (!init_gubsy_runtime(runtime, config)) {
        std::fprintf(stderr, "could not initialize Gubsy: %s\n", SDL_GetError());
        return 1;
    }
    if (!gubsy_init_sdl_renderer(runtime)) {
        std::fprintf(stderr, "could not initialize SDL3 rendering: %s\n", SDL_GetError());
        cleanup_gubsy_runtime(runtime);
        return 1;
    }
    GubsyFrame frame = gubsy_get_frame(runtime);
    if (!place_window(frame.window) || !register_controls(runtime) ||
        !init_imgui_layer(frame.window, frame.renderer)) {
        std::fprintf(stderr, "could not initialize window, controls, or ImGui\n");
        cleanup_gubsy_runtime(runtime);
        return 1;
    }
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    assign_unclaimed_gamepads(runtime);

    presentation::Renderer video;
    if (!video.initialize(frame.renderer, content)) {
        std::fprintf(stderr, "could not initialize ROM graphics\n");
        shutdown_imgui_layer();
        cleanup_gubsy_runtime(runtime);
        return 1;
    }
    const std::string title = "Native GB Tetris Modern - " + rom.digest;
    (void)SDL_SetWindowTitle(frame.window, title.c_str());

    State state;
    std::string settings_error;
    const std::filesystem::path settings_path = data_root / "enhancements.cfg";
    (void)presentation::load_settings(settings_path, state.settings, settings_error);
    initialize(state, content);
    std::string high_score_error;
    const std::filesystem::path high_score_path = data_root / "high-scores.txt";
    HighScores high_scores;
    if (load_high_scores(high_score_path, high_scores, high_score_error)) {
        state.flow.set_high_scores(std::move(high_scores));
    } else {
        std::fprintf(stderr, "could not load high scores: %s\n", high_score_error.c_str());
    }
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
            if (event.type == SDL_EVENT_GAMEPAD_ADDED)
                assign_unclaimed_gamepads(runtime);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_F1)
                state.debug.visible = !state.debug.visible;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_F2)
                (void)state.controls.open(runtime);
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_F11) {
                const bool fullscreen = (SDL_GetWindowFlags(frame.window) & SDL_WINDOW_FULLSCREEN) != 0;
                (void)SDL_SetWindowFullscreen(frame.window, !fullscreen);
            }
        }
        imgui_new_frame();
        const std::uint64_t now = SDL_GetTicksNS();
        const double elapsed = std::clamp(
            static_cast<double>(now - previous_time) / 1.0e9, 0.0, 0.25);
        previous_time = now;
        accumulator += elapsed;
        gubsy_update_device_state(runtime);
        Buttons one = read_buttons(runtime, 0);
        Buttons two = read_buttons(runtime, 1);
        const Buttons raw_one = one;
        const Buttons raw_two = two;
        if (state.controls.chord_pressed(one))
            (void)state.controls.open(runtime);
        if (state.debug.open_controls) {
            (void)state.controls.open(runtime);
            state.debug.open_controls = false;
        }
        const bool editing_controls = state.controls.is_open(runtime);
        state.controls.update(runtime, one, static_cast<float>(elapsed), frame.render_width,
                              frame.render_height);
        if (action_down(runtime, 0, Action::quit) && !editing_controls)
            running = false;
        if (imgui_want_capture_keyboard()) {
            one = {};
            two = {};
        }

        process_replay(state, rom.digest);
        while (accumulator >= fixed_step) {
            apply_debug_command(state);
            if ((!state.debug.paused || state.debug.step) && !editing_controls) {
                step(state, one, two);
                state.debug.step = false;
            }
            accumulator -= fixed_step;
        }

        gubsy_update_runtime(runtime, static_cast<float>(elapsed));
        frame = gubsy_get_frame(runtime);
        video.draw(frame.renderer, frame, content, state.flow, state.settings,
                   state.effects, state.debug.view);
        state.controls.draw(runtime, frame.renderer, frame.render_width, frame.render_height);
        state.debug.replay_recording = state.replay.recording();
        state.debug.replay_playing = state.replay.playing();
        state.debug.replay_position = state.replay.position();
        state.debug.replay_size = state.replay.size();
        int gamepad_count = 0;
        SDL_JoystickID* gamepads = SDL_GetGamepads(&gamepad_count);
        SDL_free(gamepads);
        const HostDebug host{
            .player_one = raw_one,
            .player_two = raw_two,
            .random = state.last_input.random,
            .divider = state.divider,
            .frame_seconds = elapsed,
            .accumulator_seconds = accumulator,
            .render_width = frame.render_width,
            .render_height = frame.render_height,
            .connected_gamepads = gamepad_count,
            .fullscreen = (SDL_GetWindowFlags(frame.window) & SDL_WINDOW_FULLSCREEN) != 0,
        };
        if (draw_debug_ui(state.debug, state.flow, state.settings, content, state.audio, host)) {
            state.flow.set_line_clear_speed(state.settings.line_clear_speed);
            state.audio.set_volume(state.settings.music_volume, state.settings.effects_volume);
            (void)presentation::save_settings(settings_path, state.settings, settings_error);
        }
        apply_display_request(frame.window, state.debug.display_request);
        state.debug.display_request = DisplayRequest::none;
        if (!gubsy_draw_frame_to_window(runtime))
            running = false;
        imgui_render_layer();
        gubsy_present_frame(runtime);
        ++rendered;
        if (frame_limit > 0 && rendered >= frame_limit)
            running = false;
    }
    video.shutdown();
    if (!save_high_scores(high_score_path, state.flow.high_scores(), high_score_error))
        std::fprintf(stderr, "could not save high scores: %s\n", high_score_error.c_str());
    state.audio.shutdown();
    shutdown_imgui_layer();
    cleanup_gubsy_runtime(runtime);
    return 0;
}

} // namespace tetris::app
