#include "application.hpp"

#include "src/imgui_layer.hpp"
#include "step.hpp"
#include "storage/high_scores.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <utility>

namespace tetris {
namespace {

bool save_changed_data(Application& app) {
    std::string error;

    if (app.settings != app.saved_settings && !app.settings_autosave_failed) {
        if (settings::save_settings(app.settings_path, app.settings, error)) {
            app.saved_settings = app.settings;
        } else {
            std::fprintf(stderr, "could not save settings: %s\n", error.c_str());
            app.settings_autosave_failed = true;
        }
    }

    if (!app.game.pending_score && app.game.high_scores != app.saved_high_scores &&
        !app.high_score_autosave_failed) {
        if (merge_and_save_high_scores(app.high_score_path, app.game.high_scores, error)) {
            app.saved_high_scores = app.game.high_scores;
        } else {
            std::fprintf(stderr, "could not save high scores: %s\n", error.c_str());
            app.high_score_autosave_failed = true;
        }
    }
    return !app.settings_autosave_failed && !app.high_score_autosave_failed;
}

} // namespace

bool initialize_application(Application& app, content::Rom rom,
                            const ApplicationConfig& config, std::string& error) {
    error.clear();

    // Extract immutable content before allocating platform resources.
    if (!content::extract_catalog(rom, app.content, error))
        return false;
    app.rom_digest = std::move(rom.digest);

    // Establish writable storage before Gubsy loads input profiles.
    std::error_code filesystem_error;
    std::filesystem::create_directories(config.data_root, filesystem_error);
    if (filesystem_error) {
        error = "could not create runtime data directory: " + filesystem_error.message();
        return false;
    }
    app.settings_path = config.data_root / "enhancements.cfg";
    app.high_score_path = config.data_root / "high-scores.txt";

    // Load persistent settings and initialize deterministic game state.
    std::string load_error;
    const bool settings_exist = std::filesystem::exists(app.settings_path);
    if (!settings::load_settings(app.settings_path, app.settings, load_error))
        std::fprintf(stderr, "could not load settings: %s\n", load_error.c_str());
    if (!settings_exist &&
        !settings::save_settings(app.settings_path, app.settings, load_error)) {
        std::fprintf(stderr, "could not initialize settings storage: %s\n", load_error.c_str());
    }
    app.saved_settings = app.settings;
    start_game(app.game,
               {app.content.gameplay.data, app.content.type_a_demo.runs,
                app.content.type_b_demo.runs, app.content.demo_pieces,
                app.content.type_b_demo_garbage.bytes});
    set_line_clear_speed(app.game, app.settings.line_clear_speed);
    set_gameplay_options(app.game, app.settings.gameplay);

    const bool high_scores_exist = std::filesystem::exists(app.high_score_path);
    if (load_high_scores(app.high_score_path, app.game.high_scores, load_error)) {
        app.saved_high_scores = app.game.high_scores;
    } else {
        std::fprintf(stderr, "could not load high scores: %s\n", load_error.c_str());
        app.saved_high_scores = app.game.high_scores;
    }
    if (!high_scores_exist &&
        !save_high_scores(app.high_score_path, app.game.high_scores, load_error)) {
        std::fprintf(stderr, "could not initialize high-score storage: %s\n", load_error.c_str());
    }

    // Open the host window and upload all ROM-derived graphics.
    if (!initialize_window(app.runtime, app.frame, config.data_root, config.window)) {
        error = "could not initialize the game window";
        return false;
    }
    if (!video::initialize_output(app.video, app.frame.renderer, app.content)) {
        error = "could not initialize ROM graphics";
        shutdown_window(app.runtime);
        return false;
    }
    const std::string title = "Native GB Tetris Modern - " + app.rom_digest;
    (void)SDL_SetWindowTitle(app.frame.window, title.c_str());

    // Audio is optional at startup because browsers may suspend it until a
    // later user gesture. The complete driver still advances when available.
    if (!app.audio.initialize(app.content.audio))
        std::fprintf(stderr, "audio output unavailable: %s\n", SDL_GetError());
    app.audio.set_volume(app.settings.music_volume, app.settings.effects_volume);
    app.audio.set_polyphonic(app.settings.polyphonic_sfx);

    app.debug.visible = config.open_tools;
    app.debug.settings_window = config.open_tools;
    app.debug.display_window = config.open_tools;
    if (config.open_tools)
        app.controls.open();
    app.host = config.host;
    if (!app.host.browser_managed_vsync)
        (void)SDL_SetRenderVSync(app.frame.renderer, app.settings.vsync ? 1 : 0);
    app.clock.previous_time = SDL_GetTicksNS();
    app.clock.limit = config.render_limit;
    app.clock.vsync = app.settings.vsync;
    app.clock.render_rate_cap = video::effective_render_rate(
        app.settings.motion_interpolation, app.settings.render_rate_limit);
    app.host_active = !app.host.suspend_when_window_inactive || window_active(app.frame.window);
    app.initialized = true;
    return true;
}

bool step_application(Application& app, double elapsed_seconds) {
    if (!app.initialized)
        return false;

    // Pump host events and preserve a bounded fixed-step accumulator.
    app.clock.frame_started = SDL_GetTicksNS();
    app.clock.elapsed = std::clamp(elapsed_seconds, 0.0, 0.25);
    app.clock.accumulator += app.clock.elapsed;
    ++app.clock.sampled;
    const bool automated = app.clock.limit > 0;
    const bool waited_for_activity = app.host.suspend_when_window_inactive && !automated &&
                                     !app.host_active;
    if (!poll_window_events(app.runtime, app.frame, app.controls, app.debug,
                            app.host_active, waited_for_activity))
        return false;
    if (app.host.suspend_when_window_inactive && !automated && !app.host_active) {
        app.clock.previous_time = SDL_GetTicksNS();
        app.clock.elapsed = 0.0;
        app.clock.accumulator = 0.0;
        app.clock.presentation_accumulator = 0.0;
        // Controller noise can keep SDL's event queue non-empty while the
        // window is inactive. Back off after draining it so those events do
        // not turn the inactive loop into a precise-delay polling loop.
        SDL_Delay(50);
        return true;
    }
    if (waited_for_activity) {
        app.clock.previous_time = SDL_GetTicksNS();
        app.clock.elapsed = 0.0;
        app.clock.accumulator = 0.0;
        app.clock.presentation_accumulator = 0.0;
    }
    const bool present = presentation_due(app.clock, app.host);
    if (present)
        imgui_new_frame();
    if (!read_input(app.runtime, app.controls, app.input))
        return false;

    // Advance deterministic game time independently from presentation frames.
    process_replay_request(app.debug, app.replay, app.game, app.random_state, app.rom_digest);
    while (step_due(app.clock)) {
        apply_debug_command(app.debug, app.replay, app.game, app.settings, app.random_state);
        if ((!app.debug.paused || app.debug.step) && !app.input.editing_controls) {
            const std::uint64_t step_started = SDL_GetTicksNS();
            video::capture_motion(app.motion, app.game);
            const Buttons player_one = consume_buttons(app.input.player_one);
            const Buttons player_two = consume_buttons(app.input.player_two);
            step(app.game, app.replay, app.effects, app.settings, app.audio, app.last_input,
                 app.random_state, player_one, player_two);
            app.clock.simulation_seconds =
                static_cast<double>(SDL_GetTicksNS() - step_started) / 1.0e9;
            ++app.clock.stepped;
            app.debug.step = false;
        }
    }

    if (!present)
        return true;

    // Render once at the host display rate and promptly persist any UI or
    // completed-score changes.
    const std::uint64_t presentation_started = SDL_GetTicksNS();
    const bool rendered =
        render(app.runtime, app.frame, app.input, app.clock, app.video, app.content, app.game,
               app.settings, app.effects, app.controls, app.debug, app.replay, app.audio,
               app.last_input, app.random_state, app.motion, app.host);
    app.clock.presentation_seconds =
        static_cast<double>(SDL_GetTicksNS() - presentation_started) / 1.0e9;
    if (!rendered) {
        return false;
    }
    (void)save_changed_data(app);
    ++app.clock.rendered;
    return app.clock.limit <= 0 || app.clock.rendered < app.clock.limit;
}

void shutdown_application(Application& app) {
    if (!app.initialized)
        return;
    (void)save_changed_data(app);
    app.audio.shutdown();
    video::shutdown_output(app.video);
    shutdown_window(app.runtime);
    app.initialized = false;
}

} // namespace tetris
