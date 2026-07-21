#include "window.hpp"

#include "controls.hpp"
#include "src/imgui_layer.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <cstdio>

namespace tetris {
namespace {

bool place_window(SDL_Window* window, const WindowConfig& config) {
    if (window == nullptr)
        return false;
    if (!config.desktop_placement)
        return true;

    (void)SDL_SetWindowFullscreen(window, false);
    (void)SDL_SetWindowSize(window, config.width, config.height);
    (void)SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    // Some tiling window managers ignore placement. The utility hint still requests floating.
    (void)SDL_SyncWindow(window);
    return true;
}

} // namespace

bool window_active(SDL_Window* window) {
    if (window == nullptr)
        return false;
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    return (flags & SDL_WINDOW_MINIMIZED) == 0 &&
           (flags & SDL_WINDOW_INPUT_FOCUS) != 0;
}

bool initialize_window(GubsyRuntime& runtime, GubsyFrame& frame,
                       const std::filesystem::path& data_root,
                       const WindowConfig& window_config) {
    // Describe the window and fixed-size render target.
    GubsyAppConfig config;
    config.enable_mods = false;
    config.project_root = TETRIS_SOURCE_DIR;
    config.data_root = data_root.string();
    config.engine_assets_root = TETRIS_GUBSY_ASSETS_DIR;
    config.window_title = "Native GB Tetris Modern";
    config.window_width = window_config.width;
    config.window_height = window_config.height;
    config.render_width = window_config.width;
    config.render_height = window_config.height;
    config.utility_window = window_config.utility;
    config.resizable_window = true;
    config.apply_display_settings = window_config.apply_display_settings;

    // Initialize SDL, the Gubsy runtime, and its renderer.
    if (!init_gubsy_runtime(runtime, config)) {
        std::fprintf(stderr, "could not initialize Gubsy: %s\n", SDL_GetError());
        return false;
    }
    if (!gubsy_init_sdl_renderer(runtime)) {
        std::fprintf(stderr, "could not initialize SDL3 rendering: %s\n", SDL_GetError());
        cleanup_gubsy_runtime(runtime);
        return false;
    }

    // Validate the render target before initializing input and developer UI.
    frame = gubsy_get_frame(runtime);
    if (!place_window(frame.window, window_config) || frame.renderer == nullptr ||
        frame.render_target == nullptr) {
        std::fprintf(stderr, "Gubsy did not provide a complete game window\n");
        cleanup_gubsy_runtime(runtime);
        return false;
    }

    // Present at the display refresh rate when the active renderer supports it.
    (void)SDL_SetRenderVSync(frame.renderer, 1);

    if (!register_controls(runtime)) {
        std::fprintf(stderr, "could not initialize game controls\n");
        cleanup_gubsy_runtime(runtime);
        return false;
    }
    if (!init_imgui_layer(frame.window, frame.renderer)) {
        std::fprintf(stderr, "could not initialize the ImGui developer interface\n");
        cleanup_gubsy_runtime(runtime);
        return false;
    }

    // Gamepads belong to the game, never ImGui navigation.
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    assign_unclaimed_gamepads(runtime);
    return true;
}

bool poll_window_events(GubsyRuntime& runtime, const GubsyFrame& frame, ControlsMenu& controls,
                        DebugUi& debug, bool& active, bool wait_for_activity) {
    const auto process = [&](SDL_Event& event) {
        gubsy_process_sdl_event(runtime, event);
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST ||
            event.type == SDL_EVENT_WINDOW_MINIMIZED ||
            event.type == SDL_EVENT_WINDOW_HIDDEN) {
            active = false;
        } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED ||
                   event.type == SDL_EVENT_WINDOW_RESTORED ||
                   event.type == SDL_EVENT_WINDOW_SHOWN) {
            active = window_active(frame.window);
        }
        if (event.type == SDL_EVENT_GAMEPAD_ADDED)
            assign_unclaimed_gamepads(runtime);
        if (event.type == SDL_EVENT_QUIT)
            return false;
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_F1)
            toggle_debug_ui(debug, controls);
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_F2)
            controls.toggle();
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_F11) {
            const bool fullscreen =
                (SDL_GetWindowFlags(frame.window) & SDL_WINDOW_FULLSCREEN) != 0;
            (void)SDL_SetWindowFullscreen(frame.window, !fullscreen);
        }
        return true;
    };
    SDL_Event event;
    if (wait_for_activity && SDL_WaitEvent(&event) && !process(event))
        return false;
    while (SDL_PollEvent(&event)) {
        if (!process(event))
            return false;
    }
    return true;
}

void apply_window_request(SDL_Window* window, DisplayRequest request) {
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

void shutdown_window(GubsyRuntime& runtime) {
    shutdown_imgui_layer();
    cleanup_gubsy_runtime(runtime);
}

} // namespace tetris
