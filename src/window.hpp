#pragma once

#include "controls_menu.hpp"
#include "debug_menu.hpp"

#include <gubsy/runtime.hpp>

#include <filesystem>

namespace tetris {

struct WindowConfig {
    int width{1280};
    int height{720};
    bool utility{true};
    bool desktop_placement{true};
    bool apply_display_settings{true};
};

bool initialize_window(GubsyRuntime& runtime, GubsyFrame& frame,
                       const std::filesystem::path& data_root,
                       const WindowConfig& config = {});
bool window_active(SDL_Window* window);
bool poll_window_events(GubsyRuntime& runtime, const GubsyFrame& frame, ControlsMenu& controls,
                        DebugUi& debug, bool& active, bool wait_for_activity = false);
void apply_window_request(SDL_Window* window, DisplayRequest request);
void shutdown_window(GubsyRuntime& runtime);

} // namespace tetris
