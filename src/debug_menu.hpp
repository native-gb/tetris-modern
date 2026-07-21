#pragma once

#include "audio/output.hpp"
#include "content/catalog.hpp"
#include "game/state.hpp"
#include "settings.hpp"
#include "video/video.hpp"

#include <cstddef>
#include <cstdint>

namespace tetris {

struct ControlsMenu;

enum class ReplayRequest { none, record, stop, play, clear };
enum class DisplayRequest { none, window_640, window_1280, window_1920, toggle_fullscreen };
enum class DebugCommandType {
    none,
    start_type_a,
    start_type_b,
    clear_board,
    prepare_tetris,
    set_score,
    force_game_over,
    force_complete,
};

struct DebugCommand {
    DebugCommandType type{DebugCommandType::none};
    int first{};
    int second{};
    std::uint32_t value{};
};

struct HostDebug {
    Buttons player_one;
    Buttons player_two;
    RandomSamples random;
    std::uint8_t divider{};
    double frame_seconds{};
    double accumulator_seconds{};
    double simulation_seconds{};
    double presentation_seconds{};
    int render_width{};
    int render_height{};
    int connected_gamepads{};
    int render_rate_cap{};
    video::RenderBackend active_backend{video::RenderBackend::gpu_atlas};
    bool gpu_available{};
    bool fullscreen{};
    bool vsync{};
    bool browser_managed_vsync{};
};

struct DebugUi {
    // Window visibility.
    bool visible{};
    bool debug_tools_window{};
    bool overlays_window{};
    bool state_window{};
    bool game_state_window{};
    bool events_window{};
    bool content_window{};
    bool replay_window{};
    bool settings_window{true};
    bool audio_window{};
    bool setup_window{};
    bool randomizer_window{};
    bool input_window{};
    bool display_window{};

    // Immediate host requests.
    bool paused{};
    bool step{};
    ReplayRequest replay_request{ReplayRequest::none};
    DisplayRequest display_request{DisplayRequest::none};

    // Replay status shown in the menu.
    bool replay_recording{};
    bool replay_playing{};
    std::size_t replay_position{};
    std::size_t replay_size{};

    // State setup fields and queued mutation.
    int setup_level{};
    int setup_height{};
    std::uint32_t setup_score{};
    DebugCommand command{};

    // Overlays drawn over the native game image.
    video::DebugView view{};
};

void toggle_debug_ui(DebugUi& ui, ControlsMenu& controls);
bool draw_debug_ui(DebugUi& ui, ControlsMenu& controls, const GameState& state,
                   settings::Settings& settings, const content::Catalog& content,
                   audio::Output& audio, const HostDebug& host);

} // namespace tetris
