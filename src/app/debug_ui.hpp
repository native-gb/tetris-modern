#pragma once

#include "audio/output.hpp"
#include "content/catalog.hpp"
#include "game/flow.hpp"
#include "presentation/renderer.hpp"
#include "presentation/settings.hpp"

#include <cstddef>
#include <cstdint>

namespace tetris::app {

enum class ReplayRequest { none, record, stop, play, clear };
enum class DisplayRequest { none, window_640, window_1280, window_1920, toggle_fullscreen };
enum class DebugCommandType {
    none,
    start_type_a,
    start_type_b,
    clear_board,
    prepare_tetris,
    toggle_cell,
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
    int render_width{};
    int render_height{};
    int connected_gamepads{};
    bool fullscreen{};
};

struct DebugUi {
    bool visible{};
    bool state_window{true};
    bool board_window{};
    bool flow_window{};
    bool events_window{};
    bool content_window{};
    bool replay_window{};
    bool settings_window{true};
    bool audio_window{};
    bool setup_window{};
    bool randomizer_window{};
    bool input_window{};
    bool display_window{};
    bool open_controls{};
    bool paused{};
    bool step{};
    ReplayRequest replay_request{ReplayRequest::none};
    DisplayRequest display_request{DisplayRequest::none};
    bool replay_recording{};
    bool replay_playing{};
    std::size_t replay_position{};
    std::size_t replay_size{};
    int setup_level{};
    int setup_height{};
    std::uint32_t setup_score{};
    DebugCommand command{};
    presentation::DebugView view{};
};

bool draw_debug_ui(DebugUi& ui, const GameFlow& flow,
                   presentation::Settings& settings,
                   const content::Catalog& content,
                   audio::Output& audio, const HostDebug& host);

} // namespace tetris::app
