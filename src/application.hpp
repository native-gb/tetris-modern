#pragma once

#include "audio/output.hpp"
#include "content/catalog.hpp"
#include "content/rom.hpp"
#include "controls_menu.hpp"
#include "debug_menu.hpp"
#include "frame.hpp"
#include "game/replay.hpp"
#include "game/state.hpp"
#include "settings.hpp"
#include "video/effects.hpp"
#include "video/output.hpp"
#include "window.hpp"

#include <filesystem>
#include <string>

namespace tetris {

struct ApplicationConfig {
    std::filesystem::path data_root;
    WindowConfig window;
    HostPolicy host;
    int render_limit{};
    bool open_tools{};
};

// The host owns one explicit set of independent game domains. Platform entry
// points supply ROM bytes, elapsed time, and a writable data directory.
struct Application {
    content::Catalog content;
    GameState game;
    settings::Settings settings;
    settings::Settings saved_settings;
    video::EffectState effects;
    video::Output video;
    video::MotionHistory motion;
    audio::Output audio;
    ControlsMenu controls;
    DebugUi debug;
    Replay replay;
    GameInput last_input{};
    FrameInput input;
    FrameClock clock;
    HostPolicy host;
    GubsyRuntime runtime;
    GubsyFrame frame{};
    HighScores saved_high_scores;
    std::filesystem::path settings_path;
    std::filesystem::path high_score_path;
    std::string rom_digest;
    std::uint8_t random_state{1};
    bool initialized{};
    bool settings_autosave_failed{};
    bool high_score_autosave_failed{};
    bool host_active{true};
};

bool initialize_application(Application& app, content::Rom rom,
                            const ApplicationConfig& config, std::string& error);
bool step_application(Application& app, double elapsed_seconds);
void shutdown_application(Application& app);

} // namespace tetris
