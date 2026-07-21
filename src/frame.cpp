#include "frame.hpp"

#include "controls.hpp"
#include "src/imgui_layer.hpp"
#include "window.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace tetris {
namespace {

constexpr double steps_per_second = 59.7275;
constexpr double seconds_per_step = 1.0 / steps_per_second;
constexpr std::uint64_t nanoseconds_per_second = 1'000'000'000ULL;

HostDebug host_debug(const GubsyFrame& frame, const FrameInput& input, const FrameClock& clock,
                     const GameInput& last_input, std::uint8_t random_state,
                     const video::Output& output, const HostPolicy& policy) {
    int gamepad_count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&gamepad_count);
    SDL_free(gamepads);

    return {
        .player_one = input.raw_player_one,
        .player_two = input.raw_player_two,
        .random = last_input.random,
        .divider = random_state,
        .frame_seconds = clock.elapsed,
        .accumulator_seconds = clock.accumulator,
        .simulation_seconds = clock.simulation_seconds,
        .presentation_seconds = clock.presentation_seconds,
        .render_width = frame.render_width,
        .render_height = frame.render_height,
        .connected_gamepads = gamepad_count,
        .render_rate_cap = clock.render_rate_cap,
        .active_backend = output.active_backend,
        .gpu_available = output.gpu_available && !output.force_cpu_fallback,
        .fullscreen = (SDL_GetWindowFlags(frame.window) & SDL_WINDOW_FULLSCREEN) != 0,
        .vsync = clock.vsync,
        .browser_managed_vsync = policy.browser_managed_vsync,
    };
}

} // namespace

void update_clock(FrameClock& clock) {
    clock.frame_started = SDL_GetTicksNS();
    clock.elapsed =
        std::clamp(static_cast<double>(clock.frame_started - clock.previous_time) / 1.0e9, 0.0,
                   0.25);
    clock.previous_time = clock.frame_started;
}

bool read_input(GubsyRuntime& runtime, ControlsMenu& controls, FrameInput& input) {
    gubsy_update_device_state(runtime);
    input.raw_player_one = read_buttons(runtime, 0);
    input.raw_player_two = read_buttons(runtime, 1);
    const bool controls_toggled = controls.chord_pressed(input.raw_player_one);
    if (controls_toggled)
        controls.toggle();

    input.editing_controls = controls.is_open() || controls_toggled;
    if (action_down(runtime, 0, Action::quit) && !input.editing_controls)
        return false;
    if (input.editing_controls || imgui_want_capture_keyboard()) {
        clear_buttons(input.player_one);
        clear_buttons(input.player_two);
    } else {
        sample_buttons(input.player_one, input.raw_player_one);
        sample_buttons(input.player_two, input.raw_player_two);
    }
    return true;
}

bool step_due(FrameClock& clock) {
    if (clock.accumulator < seconds_per_step)
        return false;
    clock.accumulator -= seconds_per_step;
    return true;
}

bool presentation_due(FrameClock& clock, const HostPolicy& host) {
    if (!host.externally_scheduled_presentation)
        return true;

    const double interval = 1.0 / static_cast<double>(std::max(clock.render_rate_cap, 1));
    clock.presentation_accumulator += clock.elapsed;
    if (clock.rendered == 0) {
        clock.presentation_accumulator = 0.0;
        return true;
    }
    if (clock.presentation_accumulator < interval)
        return false;

    clock.presentation_accumulator = std::fmod(clock.presentation_accumulator, interval);
    return true;
}

bool render(GubsyRuntime& runtime, GubsyFrame& frame, const FrameInput& input,
            FrameClock& clock, video::Output& video, const content::Catalog& content,
            GameState& game, settings::Settings& settings, video::EffectState& effects,
            ControlsMenu& controls, DebugUi& debug, Replay& replay, audio::Output& audio,
            const GameInput& last_input, std::uint8_t random_state,
            const video::MotionHistory& motion, const HostPolicy& host) {
    // Draw the game and player-facing settings windows.
    gubsy_update_runtime(runtime, static_cast<float>(clock.elapsed));
    frame = gubsy_get_frame(runtime);
    const float alpha = settings.motion_interpolation
                            ? static_cast<float>(clock.accumulator / seconds_per_step)
                            : 1.0F;
    if (!video::render_output(video, frame.renderer, frame, content, game, settings, effects,
                              debug.view, motion, alpha)) {
        return false;
    }

    // Draw developer tools against current replay and host state.
    debug.replay_recording = replay.recording;
    debug.replay_playing = replay.playing;
    debug.replay_position = replay.position;
    debug.replay_size = replay.inputs.size();
    const HostDebug debug_host =
        host_debug(frame, input, clock, last_input, random_state, video, host);
    if (draw_debug_ui(debug, controls, game, settings, content, audio, debug_host)) {
        set_line_clear_speed(game, settings.line_clear_speed);
        set_gameplay_options(game, settings.gameplay);
        audio.set_volume(settings.music_volume, settings.effects_volume);
        audio.set_polyphonic(settings.polyphonic_sfx);
        if (!host.browser_managed_vsync)
            (void)SDL_SetRenderVSync(frame.renderer, settings.vsync ? 1 : 0);
        clock.vsync = settings.vsync;
        clock.render_rate_cap =
            video::effective_render_rate(settings.motion_interpolation,
                                         settings.render_rate_limit);
    }
    controls.draw(runtime, debug.visible);
    apply_window_request(frame.window, debug.display_request);
    debug.display_request = DisplayRequest::none;

    // Compose ImGui and present the completed host frame.
    if (!gubsy_draw_frame_to_window(runtime))
        return false;
    imgui_render_layer();
    gubsy_present_frame(runtime);
    return true;
}

void pace(const FrameClock& clock) {
    if (clock.render_rate_cap <= 0)
        return;

    const std::uint64_t target =
        nanoseconds_per_second / static_cast<std::uint64_t>(clock.render_rate_cap);
    const std::uint64_t elapsed = SDL_GetTicksNS() - clock.frame_started;
    if (elapsed < target)
        SDL_DelayPrecise(target - elapsed);
}

} // namespace tetris
