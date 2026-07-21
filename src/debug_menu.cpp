#include "debug_menu.hpp"

#include "controls_menu.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>

namespace tetris {
namespace {

const char* screen_name(Screen screen) {
    constexpr std::array names = {
        "Copyright",
        "Copyright (skippable)",
        "Title",
        "Game type",
        "Music",
        "Type A level",
        "Type B level",
        "Type B height",
        "Versus height",
        "Gameplay",
        "Versus gameplay",
        "Versus round result",
        "Versus match result",
        "Demo",
        "Game over",
        "Type B celebration",
        "Dancers",
        "Buran",
        "Rocket",
        "Scoreboard",
        "Name entry",
    };
    return names[static_cast<std::size_t>(screen)];
}

const char* state_name(PlayState state) {
    constexpr std::array names = {
        "Falling",          "Locked", "Resolving", "Clearing",
        "Collapse pending", "Wiping", "Game over", "Complete",
    };
    return names[static_cast<std::size_t>(state)];
}

const char* event_name(GameEvent event) {
    constexpr std::array names = {
        "spawned",         "moved",           "rotated",       "landed",
        "cleared lines",   "score changed",   "level changed", "paused",
        "preview changed", "garbage applied", "game over",     "complete",
    };
    return names[static_cast<std::size_t>(event)];
}

bool versus_session(Screen screen) {
    return screen == Screen::versus_gameplay || screen == Screen::versus_round_result ||
           screen == Screen::versus_match_result;
}

void queue(DebugUi& ui, DebugCommandType type, int first = 0, int second = 0,
           std::uint32_t value = 0) {
    ui.command = {type, first, second, value};
}

void draw_buttons(const char* label, const Buttons& buttons) {
    ImGui::Text("%s  L:%d R:%d U:%d D:%d  Back:%d Confirm:%d CCW:%d CW:%d Drop:%d Hold:%d", label,
                buttons.left, buttons.right, buttons.up, buttons.down, buttons.back,
                buttons.confirm, buttons.rotate_counterclockwise, buttons.rotate_clockwise,
                buttons.hard_drop, buttons.hold);
}

void draw_player_state(const char* label, const SinglePlayer& game) {
    ImGui::SeparatorText(label);
    ImGui::Text("State: %s", state_name(game.state));
    ImGui::Text("Level %d  Lines %d  Score %u", game.level, game.lines, game.score);
    ImGui::Text("Piece %d:%d  Origin (%d, %d)", static_cast<int>(game.piece.kind),
                static_cast<int>(game.piece.rotation), game.piece.origin.x, game.piece.origin.y);
    ImGui::Text("Preview %d:%d  Hidden %d:%d  Visible: %s", static_cast<int>(game.preview.kind),
                static_cast<int>(game.preview.rotation), static_cast<int>(game.hidden.kind),
                static_cast<int>(game.hidden.rotation), game.preview_visible ? "yes" : "no");
    ImGui::Text("Pieces %u  Holds %u  Combo %d  Max %d  T-spins %u",
                game.statistics.pieces, game.statistics.holds, game.statistics.combo,
                game.statistics.maximum_combo, game.statistics.t_spins);
    ImGui::Text("Grounded: %s  Lock: %d  Resets: %d  Upcoming: %zu",
                game.grounded ? "yes" : "no", game.lock_timer, game.lock_resets,
                game.upcoming.size());
}

void draw_events(const char* label, const SinglePlayer& game) {
    if (game.events.empty())
        return;
    ImGui::SeparatorText(label);
    for (const Event event : game.events)
        ImGui::BulletText("%s, value %d", event_name(event.type), event.value);
}

bool draw_settings(settings::Settings& settings) {
    bool changed = false;
    const auto custom = [&] {
        settings.preset = settings::Preset::custom;
        changed = true;
    };

    int preset = static_cast<int>(settings.preset);
    if (ImGui::Combo("Preset", &preset, "Original\0Enhanced\0Modern\0Custom\0")) {
        settings::apply_preset(settings, static_cast<settings::Preset>(preset));
        changed = true;
    }

    if (ImGui::CollapsingHeader("Modes", ImGuiTreeNodeFlags_DefaultOpen)) {
        int challenge = static_cast<int>(settings.gameplay.challenge);
        if (ImGui::Combo("Type A mode", &challenge, "Marathon\0Sprint 40 lines\0Ultra 3 min\0")) {
            settings.gameplay.challenge = static_cast<ChallengeMode>(challenge);
            custom();
        }
        if (ImGui::Checkbox("Quick restart", &settings.gameplay.quick_restart))
            custom();
        ImGui::TextDisabled(
            "R retries directly; north face or Select retries from pause/game over.");
    }

    if (ImGui::CollapsingHeader("Handling", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SetNextItemWidth(180.0F);
        if (ImGui::InputInt("DAS delay (frames)",
                            &settings.gameplay.horizontal_repeat_delay, 1, 5)) {
            settings.gameplay.horizontal_repeat_delay =
                std::clamp(settings.gameplay.horizontal_repeat_delay, 0, 30);
            custom();
        }
        ImGui::SetItemTooltip(
            "Delayed Auto Shift. Holding Left or Right moves once immediately, then waits "
            "this many game frames before automatic movement begins. Lower values begin "
            "repeating sooner; 12 frames is the default.");
        ImGui::SetNextItemWidth(180.0F);
        if (ImGui::InputInt("ARR interval (frames)",
                            &settings.gameplay.horizontal_repeat_interval, 1, 5)) {
            settings.gameplay.horizontal_repeat_interval =
                std::clamp(settings.gameplay.horizontal_repeat_interval, 1, 10);
            custom();
        }
        ImGui::SetItemTooltip(
            "Auto Repeat Rate. After the DAS delay, the piece moves one column every this "
            "many game frames. Lower values move faster; 1 moves every frame and 2 is the "
            "default.");
        ImGui::SetNextItemWidth(180.0F);
        if (ImGui::InputInt("Entry delay / ARE (frames)",
                            &settings.gameplay.entry_delay, 1, 5)) {
            settings.gameplay.entry_delay = std::clamp(settings.gameplay.entry_delay, 0, 30);
            custom();
        }
        ImGui::SetItemTooltip(
            "Appearance/entry delay. After a piece locks, wait this many game frames before "
            "the next piece appears or line clearing begins. Tetris 99 is commonly measured "
            "at 6 frames; Original retains the Game Boy port's 3-frame timing.");
        if (ImGui::Checkbox("Instant autorepeat after DAS",
                            &settings.gameplay.instant_autorepeat))
            custom();
        ImGui::SetItemTooltip(
            "After the DAS delay, move the piece all the way toward the held direction in "
            "one step. ARR is ignored while this is enabled.");
        ImGui::TextDisabled("DAS waits before repeat; ARR is the delay between shifts.");
        if (ImGui::Checkbox("Buffered spawn inputs", &settings.gameplay.buffered_inputs))
            custom();
        int drop = static_cast<int>(settings.gameplay.drop_mode);
        if (ImGui::Combo("Up / Drop action", &drop, "Disabled\0Sonic drop\0Hard drop\0")) {
            settings.gameplay.drop_mode = static_cast<DropMode>(drop);
            custom();
        }
        int rotation = static_cast<int>(settings.gameplay.rotation);
        if (ImGui::Combo("Rotation", &rotation,
                         "Original rollback\0Side pushout\0SRS wall/floor kicks\0")) {
            settings.gameplay.rotation = static_cast<RotationSystem>(rotation);
            custom();
        }
        if (ImGui::Checkbox("Separate clockwise / counterclockwise inputs",
                            &settings.gameplay.separate_rotation_inputs))
            custom();
        if (ImGui::Checkbox("Lock delay and move resets", &settings.gameplay.lock_delay))
            custom();
    }

    if (ImGui::CollapsingHeader("Assistance", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Checkbox("Ghost piece", &settings.gameplay.ghost_piece))
            custom();
        if (ImGui::Checkbox("Hold piece", &settings.gameplay.hold_piece))
            custom();
        if (ImGui::SliderInt("Next previews", &settings.gameplay.next_previews, 1, 5))
            custom();
    }

    if (ImGui::CollapsingHeader("Randomizer and scoring", ImGuiTreeNodeFlags_DefaultOpen)) {
        int randomizer = static_cast<int>(settings.gameplay.randomizer);
        if (ImGui::Combo("Randomizer", &randomizer, "Original\0Seven bag\0TGM history\0")) {
            settings.gameplay.randomizer = static_cast<RandomizerMode>(randomizer);
            custom();
        }
        if (ImGui::Checkbox("Modern scoring", &settings.gameplay.modern_scoring))
            custom();
        ImGui::TextDisabled("Adds T-spins, combos, back-to-back, and uncapped play.");
    }

    if (ImGui::CollapsingHeader("Visuals", ImGuiTreeNodeFlags_DefaultOpen)) {
        int clear = static_cast<int>(settings.line_clear_speed);
        if (ImGui::Combo("Line clear", &clear, "Original\0Fast\0Instant\0")) {
            settings.line_clear_speed = static_cast<LineClearSpeed>(clear);
            custom();
        }
        int layout = static_cast<int>(settings.layout);
        if (ImGui::Combo("Layout", &layout, "Original 160x144\0Widescreen frame\0")) {
            settings.layout = static_cast<settings::Layout>(layout);
            custom();
        }
        int background = static_cast<int>(settings.background);
        if (ImGui::Combo("Background", &background, "Solid dark green\0Snow\0Stars\0")) {
            settings.background = static_cast<settings::Background>(background);
            custom();
        }
        int piece_colors = static_cast<int>(settings.piece_colors);
        if (ImGui::Combo("Piece colors", &piece_colors, "Game Boy\0Guideline colors\0")) {
            settings.piece_colors = static_cast<settings::PieceColors>(piece_colors);
            custom();
        }
        if (settings.background != settings::Background::off &&
            ImGui::Checkbox("Moving background", &settings.moving_background))
            custom();
        if (ImGui::Checkbox("Title landscape parallax", &settings.title_parallax))
            custom();
        int effects = static_cast<int>(settings.effects);
        if (ImGui::Combo("Effects", &effects, "Off\0Subtle\0Full\0")) {
            settings.effects = static_cast<settings::Intensity>(effects);
            custom();
        }
        if (settings.effects != settings::Intensity::off &&
            ImGui::Checkbox("Screen shake", &settings.screen_shake))
            custom();
        if (ImGui::Checkbox("Allow extended canvas", &settings.extended_canvas))
            custom();
        ImGui::TextDisabled("Adds HUD columns only when they fit without reducing pixel scale.");
    }

    if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Checkbox("Unlimited sound effects", &settings.polyphonic_sfx))
            custom();
        ImGui::TextDisabled("Mixes overlapping effects instead of stealing a Game Boy channel.");
        changed |= ImGui::SliderFloat("Music volume", &settings.music_volume, 0.0F, 1.0F);
        changed |= ImGui::SliderFloat("Effects volume", &settings.effects_volume, 0.0F, 1.0F);
    }

    if (ImGui::CollapsingHeader("Accessibility", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= ImGui::Checkbox("Reduced flash", &settings.reduced_flash);
        int labels = static_cast<int>(settings.controller_labels);
        if (ImGui::Combo("Controller button labels", &labels,
                         "Auto detect\0Nintendo\0Xbox\0PlayStation\0")) {
            settings.controller_labels = static_cast<settings::ControllerLabels>(labels);
            changed = true;
        }
    }
    return changed;
}

void draw_audio(audio::Output& output, const content::Catalog& content) {
    const audio::Director& director = output.director();
    ImGui::Text("Output: %s", output.available() ? "available" : "unavailable");
    ImGui::Text("Queued: %d bytes", output.queued_bytes());
    ImGui::Text("Song: %d  Stereo: 0x%02X", director.active_song, director.stereo_mask);
    ImGui::Text("Independent SFX voices: %zu", output.polyphonic_voice_count());
    ImGui::Text("Pause tune: %s", director.paused ? "active" : "off");
    const auto music = output.music_meters();
    const auto sounds = output.sound_meters();
    constexpr std::array names = {"Pulse 1", "Pulse 2", "Wave", "Noise"};
    for (std::size_t channel = 0; channel < names.size(); ++channel) {
        ImGui::TextUnformatted(names[channel]);
        ImGui::SameLine(80.0F);
        ImGui::ProgressBar(music[channel], ImVec2(100.0F, 0.0F), "music");
        ImGui::SameLine();
        ImGui::ProgressBar(sounds[channel], ImVec2(100.0F, 0.0F), "sfx");
    }
    ImGui::SeparatorText("Audition");
    for (std::size_t index = 0; index < content.audio.songs.size(); ++index) {
        ImGui::PushID(static_cast<int>(index));
        if (ImGui::SmallButton(content.audio.songs[index].id.c_str()))
            (void)output.director().audition_song(index);
        ImGui::PopID();
        if (index % 3 != 2 && index + 1 != content.audio.songs.size())
            ImGui::SameLine();
    }
    for (std::size_t index = 0; index < content.audio.effects.size(); ++index) {
        ImGui::PushID(static_cast<int>(100 + index));
        if (ImGui::SmallButton(content.audio.effects[index].id.c_str()))
            (void)output.director().audition_sound(index);
        ImGui::PopID();
        if (index % 3 != 2 && index + 1 != content.audio.effects.size())
            ImGui::SameLine();
    }
}

} // namespace

void toggle_debug_ui(DebugUi& ui, ControlsMenu& controls) {
    if (!ui.visible && !controls.visible) {
        ui.visible = true;
        ui.settings_window = true;
        ui.display_window = true;
        controls.open();
        return;
    }

    ui.visible = false;
    ui.debug_tools_window = false;
    ui.overlays_window = false;
    ui.state_window = false;
    ui.game_state_window = false;
    ui.events_window = false;
    ui.content_window = false;
    ui.replay_window = false;
    ui.settings_window = false;
    ui.audio_window = false;
    ui.setup_window = false;
    ui.randomizer_window = false;
    ui.input_window = false;
    ui.display_window = false;
    controls.visible = false;
}

bool draw_debug_ui(DebugUi& ui, ControlsMenu& controls, const GameState& state,
                   settings::Settings& settings, const content::Catalog& content,
                   audio::Output& audio, const HostDebug& host) {
    if (!ui.visible)
        return false;
    bool settings_changed = false;
    // Keep ordinary settings separate from developer-only inspection tools.
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Settings")) {
            ImGui::MenuItem("Enhancements", nullptr, &ui.settings_window);
            if (ImGui::MenuItem("Controls", "F2", controls.visible)) {
                if (controls.visible)
                    controls.visible = false;
                else
                    controls.open();
            }
            ImGui::MenuItem("Display", nullptr, &ui.display_window);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("Debug tools", nullptr, &ui.debug_tools_window);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // Small launcher for the most frequently used tool windows.
    constexpr float window_margin = 16.0F;
    constexpr float window_top = 36.0F;
    constexpr float tools_width = 315.0F;
    ImGui::SetNextWindowPos(ImVec2(window_margin, window_top), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(tools_width, 0.0F), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Native Tetris Tools###ModernTetrisTools", &ui.visible,
                     ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextDisabled("F1 opens or closes all tool windows.");
        ImGui::SeparatorText("Settings");
        ImGui::Checkbox("Enhancements", &ui.settings_window);
        bool controls_visible = controls.visible;
        if (ImGui::Checkbox("Controls", &controls_visible)) {
            if (controls_visible)
                controls.open();
            else
                controls.visible = false;
        }
        ImGui::Checkbox("Display", &ui.display_window);

        if (ImGui::CollapsingHeader("Debug"))
            ImGui::Checkbox("Debug tools", &ui.debug_tools_window);
    }
    ImGui::End();

    // Developer-only windows are launched from one dedicated tool index.
    if (ui.debug_tools_window) {
        ImGui::SetNextWindowSize(ImVec2(290.0F, 0.0F), ImGuiCond_FirstUseEver);
        const bool open = ImGui::Begin("Debug Tools", &ui.debug_tools_window,
                                       ImGuiWindowFlags_AlwaysAutoResize);
        if (open) {
            ImGui::Checkbox("Debug overlays", &ui.overlays_window);
            ImGui::Separator();
            ImGui::Checkbox("State", &ui.state_window);
            ImGui::Checkbox("Game state", &ui.game_state_window);
            ImGui::Checkbox("Events", &ui.events_window);
            ImGui::Checkbox("Replay", &ui.replay_window);
            ImGui::Checkbox("State setup", &ui.setup_window);
            ImGui::Checkbox("Randomizer", &ui.randomizer_window);
            ImGui::Checkbox("Input", &ui.input_window);
            ImGui::Checkbox("Audio", &ui.audio_window);
            ImGui::Checkbox("ROM content", &ui.content_window);
        }
        ImGui::End();
    }

    // Rendering diagnostics are kept out of both the normal menu and tool index.
    if (ui.overlays_window) {
        ImGui::SetNextWindowSize(ImVec2(240.0F, 0.0F), ImGuiCond_FirstUseEver);
        const bool open = ImGui::Begin("Debug Overlays", &ui.overlays_window,
                                       ImGuiWindowFlags_AlwaysAutoResize);
        if (open) {
            ImGui::Checkbox("Tile grid", &ui.view.grid);
            ImGui::Checkbox("Sprite bounds", &ui.view.sprite_bounds);
            ImGui::Checkbox("Board cells", &ui.view.board_cells);
        }
        ImGui::End();
    }

    // Live deterministic state.
    if (ui.state_window) {
        const bool open = ImGui::Begin("Tetris State", &ui.state_window);
        if (open) {
            if (ImGui::Button(ui.paused ? "Resume" : "Pause"))
                ui.paused = !ui.paused;
            ImGui::SameLine();
            if (ImGui::Button("Step"))
                ui.step = true;
            ImGui::SeparatorText("Session");
            ImGui::Text("Screen: %s", screen_name(state.screen));
            if (versus_session(state.screen)) {
                draw_player_state("Player one", state.versus.players[0].game);
                draw_player_state("Player two", state.versus.players[1].game);
            } else {
                draw_player_state("Player", state.single_player);
            }
        }
        ImGui::End();
    }

    // Top-level screen and ending state.
    if (ui.game_state_window) {
        if (ImGui::Begin("Game State", &ui.game_state_window)) {
            ImGui::Text("Screen: %s", screen_name(state.screen));
            ImGui::Text("Timer: %d", state.timer);
            ImGui::Text("Ending stage: %d", static_cast<int>(state.ending_stage));
            ImGui::Text("Ending elapsed: %d", state.ending_elapsed);
            ImGui::Text("Mode: %d  Level: %d  Height: %d", static_cast<int>(state.selected_type),
                        selected_level(state), state.type_b_height);
        }
        ImGui::End();
    }

    // Events emitted during the latest fixed step.
    if (ui.events_window) {
        if (ImGui::Begin("Game Events", &ui.events_window)) {
            if (versus_session(state.screen)) {
                draw_events("Player one", state.versus.players[0].game);
                draw_events("Player two", state.versus.players[1].game);
            } else {
                draw_events("Player", state.single_player);
            }
        }
        ImGui::End();
    }

    // Extracted ROM content and provenance.
    if (ui.content_window) {
        const bool open = ImGui::Begin("ROM Content", &ui.content_window);
        if (open) {
            ImGui::Text("Profile: %s", content.profile.c_str());
            ImGui::Text("SHA-1: %s", content.source_sha1.c_str());
            ImGui::Text("Tiles: %zu", content.tile_count());
            ImGui::Text("Tilemaps: %zu", content.tilemaps.size());
            ImGui::Text("Sprites: %zu", content.sprites.sprites.size());
            ImGui::Text("Rendering tables: %zu", content.layout_tables.size());
            ImGui::Text("Songs: %zu  Effects: %zu", content.audio.songs.size(),
                        content.audio.effects.size());
            if (ImGui::BeginTable("Provenance", 4,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                for (const content::Tilemap& map : content.tilemaps) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(map.source.id.c_str());
                    const content::RomSpan span = map.source.spans.front();
                    ImGui::TableNextColumn();
                    ImGui::Text("0x%04zX", span.begin);
                    ImGui::TableNextColumn();
                    ImGui::Text("0x%04zX", span.end);
                    ImGui::TableNextColumn();
                    ImGui::Text("%zu/%zu%s", map.source.decoded_count, map.source.expected_count,
                                map.source.validated ? "" : " !");
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    // Replay recording and playback.
    if (ui.replay_window) {
        const bool open = ImGui::Begin("Deterministic Replay", &ui.replay_window);
        if (open) {
            if (ImGui::Button("Record"))
                ui.replay_request = ReplayRequest::record;
            ImGui::SameLine();
            if (ImGui::Button("Stop"))
                ui.replay_request = ReplayRequest::stop;
            ImGui::SameLine();
            if (ImGui::Button("Play"))
                ui.replay_request = ReplayRequest::play;
            ImGui::SameLine();
            if (ImGui::Button("Clear"))
                ui.replay_request = ReplayRequest::clear;
            ImGui::Text("%s  %zu / %zu",
                        ui.replay_recording ? "recording"
                        : ui.replay_playing ? "playing"
                                            : "idle",
                        ui.replay_position, ui.replay_size);
        }
        ImGui::End();
    }

    // Semantic game-state mutation commands.
    if (ui.setup_window) {
        const bool open = ImGui::Begin("Semantic State Setup", &ui.setup_window);
        if (open) {
            ImGui::SliderInt("Level", &ui.setup_level, 0, 9);
            ImGui::SliderInt("Type B height", &ui.setup_height, 0, 5);
            if (ImGui::Button("Start Type A"))
                queue(ui, DebugCommandType::start_type_a, ui.setup_level);
            ImGui::SameLine();
            if (ImGui::Button("Start Type B"))
                queue(ui, DebugCommandType::start_type_b, ui.setup_level, ui.setup_height);
            if (ImGui::Button("Clear board"))
                queue(ui, DebugCommandType::clear_board);
            ImGui::SameLine();
            if (ImGui::Button("Prepare Tetris"))
                queue(ui, DebugCommandType::prepare_tetris);
            if (ImGui::Button("Force game over"))
                queue(ui, DebugCommandType::force_game_over);
            ImGui::SameLine();
            if (ImGui::Button("Force complete"))
                queue(ui, DebugCommandType::force_complete);
            ImGui::InputScalar("Score", ImGuiDataType_U32, &ui.setup_score);
            if (ImGui::Button("Set score"))
                queue(ui, DebugCommandType::set_score, 0, 0, ui.setup_score);
            ImGui::TextDisabled("Commands apply at the next fixed-step boundary.");
        }
        ImGui::End();
    }

    // Random samples and piece-queue history.
    if (ui.randomizer_window) {
        const bool open = ImGui::Begin("Piece Randomizer", &ui.randomizer_window);
        if (open) {
            const SinglePlayer& game =
                versus_session(state.screen) ? state.versus.players[0].game : state.single_player;
            ImGui::Text("Host divider state: %u", static_cast<unsigned int>(host.divider));
            ImGui::Text("Current host samples: %u, %u, %u",
                        static_cast<unsigned int>(host.random[0]),
                        static_cast<unsigned int>(host.random[1]),
                        static_cast<unsigned int>(host.random[2]));
            const RandomSamples accepted = game.last_random_samples;
            ImGui::Text("Last accepted samples: %u, %u, %u  Attempts: %d",
                        static_cast<unsigned int>(accepted[0]),
                        static_cast<unsigned int>(accepted[1]),
                        static_cast<unsigned int>(accepted[2]), game.last_random_attempts);
            ImGui::Text("Active %d:%d  Preview %d:%d  Hidden %d:%d",
                        static_cast<int>(game.piece.kind), static_cast<int>(game.piece.rotation),
                        static_cast<int>(game.preview.kind),
                        static_cast<int>(game.preview.rotation), static_cast<int>(game.hidden.kind),
                        static_cast<int>(game.hidden.rotation));
            ImGui::Text("Fixed sequence consumed: %zu", game.next_fixed_piece);
        }
        ImGui::End();
    }

    // Host input beside the buttons received by the simulation.
    if (ui.input_window) {
        const bool open = ImGui::Begin("Input Ownership", &ui.input_window);
        if (open) {
            draw_buttons("Player one", host.player_one);
            draw_buttons("Player two", host.player_two);
            const SinglePlayer& one =
                versus_session(state.screen) ? state.versus.players[0].game : state.single_player;
            draw_buttons("Simulation held", one.previous_buttons);
            draw_buttons("Simulation pressed", one.pressed_buttons);
            ImGui::Text("P1 DAS timer: %d  Drop timer: %d / %d", one.horizontal_repeat_timer,
                        one.fall_timer, one.gravity_frames);
            if (versus_session(state.screen)) {
                const SinglePlayer& two = state.versus.players[1].game;
                draw_buttons("P2 simulation held", two.previous_buttons);
                draw_buttons("P2 simulation pressed", two.pressed_buttons);
                ImGui::Text("P2 DAS timer: %d  Drop timer: %d / %d", two.horizontal_repeat_timer,
                            two.fall_timer, two.gravity_frames);
            }
            ImGui::Text("Connected gamepads: %d", host.connected_gamepads);
            ImGui::TextUnformatted("ImGui gamepad navigation: disabled");
            if (ImGui::Button("Open controls"))
                controls.open();
        }
        ImGui::End();
    }

    // Window sizing and fixed-step timing.
    if (ui.display_window) {
        constexpr float display_y = 205.0F;
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float available_height =
            std::max(1.0F, display.y - display_y - window_margin);
        const float minimum_height = std::min(240.0F, available_height);
        ImGui::SetNextWindowPos(ImVec2(window_margin, display_y), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(tools_width, available_height), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(tools_width, minimum_height),
            ImVec2(tools_width, available_height));
        const bool open = ImGui::Begin("Display and Performance", &ui.display_window);
        if (open) {
            ImGui::Text("Render target: %d x %d", host.render_width, host.render_height);
            ImGui::Text("Window mode: %s", host.fullscreen ? "borderless fullscreen" : "windowed");
            if (ImGui::Button("640 x 576"))
                ui.display_request = DisplayRequest::window_640;
            ImGui::SameLine();
            if (ImGui::Button("1280 x 720"))
                ui.display_request = DisplayRequest::window_1280;
            ImGui::SameLine();
            if (ImGui::Button("1920 x 1080"))
                ui.display_request = DisplayRequest::window_1920;
            if (ImGui::Button(host.fullscreen ? "Return to window" : "Borderless fullscreen"))
                ui.display_request = DisplayRequest::toggle_fullscreen;
            ImGui::SameLine();
            ImGui::TextDisabled("F11");
            ImGui::Text("Host sample interval: %.3f ms", host.frame_seconds * 1'000.0);
            ImGui::Text("Last simulation CPU: %.3f ms",
                        host.simulation_seconds * 1'000.0);
            ImGui::Text("Previous presentation CPU: %.3f ms",
                        host.presentation_seconds * 1'000.0);
            ImGui::TextDisabled("CPU draw/submission time; GPU completion is asynchronous.");
            ImGui::Text("Fixed-step accumulator: %.3f ms", host.accumulator_seconds * 1'000.0);
            ImGui::SeparatorText("Renderer");
            int renderer = static_cast<int>(settings.renderer);
            if (ImGui::Combo("Backend", &renderer, "GPU atlas\0CPU raster\0")) {
                settings.renderer = static_cast<video::RenderBackend>(renderer);
                settings_changed = true;
            }
            ImGui::Text("Active: %s", host.active_backend == video::RenderBackend::gpu_atlas
                                           ? "GPU atlas"
                                           : "CPU raster");
            if (!host.gpu_available)
                ImGui::TextDisabled("GPU atlas unavailable; using CPU fallback.");

            ImGui::SeparatorText("Motion and pacing");
            settings_changed |=
                ImGui::Checkbox("Motion interpolation", &settings.motion_interpolation);
            if (host.browser_managed_vsync) {
                ImGui::BeginDisabled();
                bool browser_vsync = true;
                (void)ImGui::Checkbox("VSync", &browser_vsync);
                ImGui::EndDisabled();
                ImGui::TextDisabled("Browser-managed by requestAnimationFrame.");
            } else {
                settings_changed |= ImGui::Checkbox("VSync", &settings.vsync);
            }
            if (settings.motion_interpolation) {
                constexpr const char* rates =
                    "60 Hz\0" "120 Hz\0" "144 Hz\0" "165 Hz\0" "240 Hz\0";
                const auto found = std::ranges::find(video::render_rate_limits,
                                                     settings.render_rate_limit);
                int selected = found == video::render_rate_limits.end()
                                   ? 0
                                   : static_cast<int>(found - video::render_rate_limits.begin());
                if (ImGui::Combo("Frame limit", &selected, rates)) {
                    settings.render_rate_limit =
                        video::render_rate_limits[static_cast<std::size_t>(selected)];
                    settings_changed = true;
                }
            } else {
                ImGui::TextUnformatted("Frame limit: 60 Hz (interpolation off)");
            }
            ImGui::Text("Effective hard cap: %d Hz", host.render_rate_cap);
            if (!host.browser_managed_vsync)
                ImGui::Text("VSync: %s", host.vsync ? "on" : "off");
            const std::uint64_t step = versus_session(state.screen)
                                           ? state.versus.players[0].game.step_count
                                           : state.single_player.step_count;
            ImGui::Text("Simulation step: %llu", static_cast<unsigned long long>(step));
            ImGui::Text("Layout: %s", settings::name(settings.layout));
            ImGui::Text("Effects: %s", settings::name(settings.effects));
            ImGui::TextUnformatted("Simulation: 59.7275 Hz; rendering is independent.");
        }
        ImGui::End();
    }

    // Enhancement settings and live audio inspection.
    if (ui.settings_window) {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float enhancement_x = window_margin + tools_width + 12.0F;
        constexpr float column_gap = 12.0F;
        const float available_columns =
            std::max(2.0F, display.x - enhancement_x - column_gap - window_margin);
        const float available_width = available_columns * 0.5F;
        const float available_height = std::max(1.0F, display.y - window_top - window_margin);
        const float minimum_width = std::min(360.0F, available_width);
        const float minimum_height = std::min(240.0F, available_height);
        ImGui::SetNextWindowPos(ImVec2(enhancement_x, window_top), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(
            ImVec2(available_width, available_height),
            ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(minimum_width, minimum_height),
            ImVec2(available_width, available_height));
        const bool open = ImGui::Begin("Enhancements", &ui.settings_window);
        if (open)
            settings_changed = draw_settings(settings);
        ImGui::End();
    }
    if (ui.audio_window) {
        const bool open = ImGui::Begin("Audio", &ui.audio_window);
        if (open)
            draw_audio(audio, content);
        ImGui::End();
    }
    return settings_changed;
}

} // namespace tetris
