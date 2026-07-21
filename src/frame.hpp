#pragma once

#include "audio/output.hpp"
#include "controls_menu.hpp"
#include "debug_menu.hpp"
#include "game/replay.hpp"
#include "settings.hpp"
#include "video/effects.hpp"
#include "video/output.hpp"

#include <gubsy/runtime.hpp>

#include <cstdint>
#include <string>

namespace tetris {

struct HostPolicy {
    bool suspend_when_window_inactive{true};
    bool externally_scheduled_presentation{};
    bool browser_managed_vsync{};
};

struct FrameClock {
    std::uint64_t previous_time{};
    std::uint64_t frame_started{};
    double elapsed{};
    double accumulator{};
    double presentation_accumulator{};
    double simulation_seconds{};
    double presentation_seconds{};
    std::uint64_t sampled{};
    std::uint64_t stepped{};
    int rendered{};
    int limit{};
    int render_rate_cap{60};
    bool vsync{};
};

struct FrameInput {
    ButtonBuffer player_one{};
    ButtonBuffer player_two{};
    Buttons raw_player_one{};
    Buttons raw_player_two{};
    bool editing_controls{};
};

void update_clock(FrameClock& clock);
bool read_input(GubsyRuntime& runtime, ControlsMenu& controls, FrameInput& input);
bool step_due(FrameClock& clock);
bool presentation_due(FrameClock& clock, const HostPolicy& host);
bool render(GubsyRuntime& runtime, GubsyFrame& frame, const FrameInput& input,
            FrameClock& clock, video::Output& video, const content::Catalog& content,
            GameState& game, settings::Settings& settings, video::EffectState& effects,
            ControlsMenu& controls, DebugUi& debug, Replay& replay, audio::Output& audio,
            const GameInput& last_input, std::uint8_t random_state,
            const video::MotionHistory& motion, const HostPolicy& host);
void pace(const FrameClock& clock);

} // namespace tetris
