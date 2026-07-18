#pragma once

#include "audio/output.hpp"
#include "content/catalog.hpp"
#include "game/flow.hpp"
#include "presentation/renderer.hpp"
#include "presentation/settings.hpp"

#include <cstddef>

namespace tetris::app {

enum class ReplayRequest { none, record, stop, play, clear };

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
    bool open_controls{};
    bool paused{};
    bool step{};
    ReplayRequest replay_request{ReplayRequest::none};
    bool replay_recording{};
    bool replay_playing{};
    std::size_t replay_position{};
    std::size_t replay_size{};
    presentation::DebugView view{};
};

bool draw_debug_ui(DebugUi& ui, GameFlow& flow,
                   presentation::Settings& settings,
                   const content::Catalog& content,
                   audio::Output& audio);

} // namespace tetris::app
