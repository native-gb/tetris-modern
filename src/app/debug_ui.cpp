#include "app/debug_ui.hpp"

#include <imgui.h>

#include <array>

namespace tetris::app {
namespace {

const char* screen_name(Screen screen) {
    constexpr std::array names = {
        "Copyright", "Copyright (skippable)", "Title", "Game type", "Music",
        "Type A level", "Type B level", "Type B height", "Versus height",
        "Gameplay", "Versus gameplay", "Versus round result", "Versus match result",
        "Demo", "Game over", "Type B celebration", "Dancers", "Buran", "Rocket",
        "Scoreboard", "Name entry",
    };
    return names[static_cast<std::size_t>(screen)];
}

const char* state_name(PlayState state) {
    constexpr std::array names = {
        "Falling", "Locked", "Resolving", "Clearing", "Collapse pending",
        "Wiping", "Game over", "Complete",
    };
    return names[static_cast<std::size_t>(state)];
}

void queue(DebugUi& ui, DebugCommandType type, int first = 0, int second = 0,
           std::uint32_t value = 0) {
    ui.command = {type, first, second, value};
}

void draw_board(DebugUi& ui, const Board& board, int player) {
    for (int row = 0; row < board_height; ++row) {
        for (int column = 0; column < board_width; ++column) {
            ImGui::PushID(player * 1'000 + row * board_width + column);
            if (ImGui::SmallButton(board.at({column, row}) == Block::empty ? "." : "#"))
                queue(ui, DebugCommandType::toggle_cell, column, row,
                      static_cast<std::uint32_t>(player));
            ImGui::PopID();
            if (column + 1 != board_width)
                ImGui::SameLine(0, 0);
        }
    }
}

void draw_buttons(const char* label, const Buttons& buttons) {
    ImGui::Text("%s  L:%d R:%d U:%d D:%d  B:%d A:%d Start:%d Select:%d", label,
                buttons.left, buttons.right, buttons.up, buttons.down,
                buttons.rotate_left, buttons.rotate_right, buttons.start, buttons.select);
}

bool draw_settings(presentation::Settings& settings) {
    bool changed = false;
    int preset = static_cast<int>(settings.preset);
    if (ImGui::Combo("Preset", &preset, "Original\0Enhanced\0Custom\0")) {
        presentation::apply_preset(settings, static_cast<presentation::Preset>(preset));
        changed = true;
    }
    int clear = static_cast<int>(settings.line_clear_speed);
    if (ImGui::Combo("Line clear", &clear, "Original\0Fast\0Instant\0")) {
        settings.line_clear_speed = static_cast<LineClearSpeed>(clear);
        settings.preset = presentation::Preset::custom;
        changed = true;
    }
    int layout = static_cast<int>(settings.layout);
    if (ImGui::Combo("Layout", &layout, "Original 160x144\0Widescreen frame\0")) {
        settings.layout = static_cast<presentation::Layout>(layout);
        settings.preset = presentation::Preset::custom;
        changed = true;
    }
    int background = static_cast<int>(settings.background);
    if (ImGui::Combo("Background", &background, "Off\0Stars\0")) {
        settings.background = static_cast<presentation::Background>(background);
        settings.preset = presentation::Preset::custom;
        changed = true;
    }
    int effects = static_cast<int>(settings.effects);
    if (ImGui::Combo("Effects", &effects, "Off\0Subtle\0Full\0")) {
        settings.effects = static_cast<presentation::Intensity>(effects);
        settings.preset = presentation::Preset::custom;
        changed = true;
    }
    changed |= ImGui::Checkbox("Reduced flash", &settings.reduced_flash);
    changed |= ImGui::Checkbox("Reduced motion", &settings.reduced_motion);
    changed |= ImGui::SliderFloat("Music volume", &settings.music_volume, 0.0F, 1.0F);
    changed |= ImGui::SliderFloat("Effects volume", &settings.effects_volume, 0.0F, 1.0F);
    return changed;
}

void draw_audio(audio::Output& output, const content::Catalog& content) {
    const audio::Director& director = output.director();
    ImGui::Text("Output: %s", output.available() ? "available" : "unavailable");
    ImGui::Text("Queued: %d bytes", output.queued_bytes());
    ImGui::Text("Song: %d  Stereo: 0x%02X", director.active_song(), director.stereo_mask());
    ImGui::Text("Pause tune: %s", director.paused() ? "active" : "off");
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
            (void)output.debug_director().audition_song(index);
        ImGui::PopID();
        if (index % 3 != 2 && index + 1 != content.audio.songs.size())
            ImGui::SameLine();
    }
    for (std::size_t index = 0; index < content.audio.effects.size(); ++index) {
        ImGui::PushID(static_cast<int>(100 + index));
        if (ImGui::SmallButton(content.audio.effects[index].id.c_str()))
            (void)output.debug_director().audition_sound(index);
        ImGui::PopID();
        if (index % 3 != 2 && index + 1 != content.audio.effects.size())
            ImGui::SameLine();
    }
}

} // namespace

bool draw_debug_ui(DebugUi& ui, const GameFlow& flow,
                   presentation::Settings& settings,
                   const content::Catalog& content,
                   audio::Output& audio, const HostDebug& host) {
    if (!ui.visible)
        return false;
    bool settings_changed = false;
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Tetris")) {
            ImGui::MenuItem("State", nullptr, &ui.state_window);
            ImGui::MenuItem("Board", nullptr, &ui.board_window);
            ImGui::MenuItem("Flow", nullptr, &ui.flow_window);
            ImGui::MenuItem("Events", nullptr, &ui.events_window);
            ImGui::MenuItem("Content", nullptr, &ui.content_window);
            ImGui::MenuItem("Replay", nullptr, &ui.replay_window);
            ImGui::MenuItem("State setup", nullptr, &ui.setup_window);
            ImGui::MenuItem("Randomizer", nullptr, &ui.randomizer_window);
            ImGui::MenuItem("Input", nullptr, &ui.input_window);
            ImGui::MenuItem("Display / performance", nullptr, &ui.display_window);
            ImGui::MenuItem("Enhancements", nullptr, &ui.settings_window);
            ImGui::MenuItem("Audio", nullptr, &ui.audio_window);
            if (ImGui::MenuItem("Controller / keyboard bindings", "F2"))
                ui.open_controls = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug view")) {
            ImGui::MenuItem("Tile grid", nullptr, &ui.view.grid);
            ImGui::MenuItem("Sprite bounds", nullptr, &ui.view.sprite_bounds);
            ImGui::MenuItem("Board cells", nullptr, &ui.view.board_cells);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    ImGui::SetNextWindowSize(ImVec2(315.0F, 0.0F), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Native Tetris Tools###ModernTetrisTools", &ui.visible,
                     ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextDisabled("F1 toggles this launcher; tool windows stay open.");
        ImGui::Checkbox("State", &ui.state_window);
        ImGui::SameLine(); ImGui::Checkbox("Board", &ui.board_window);
        ImGui::SameLine(); ImGui::Checkbox("Flow", &ui.flow_window);
        ImGui::Checkbox("Setup", &ui.setup_window);
        ImGui::SameLine(); ImGui::Checkbox("Input", &ui.input_window);
        ImGui::SameLine(); ImGui::Checkbox("Randomizer", &ui.randomizer_window);
        ImGui::Checkbox("Enhancements", &ui.settings_window);
        ImGui::SameLine(); ImGui::Checkbox("Display", &ui.display_window);
        ImGui::SameLine(); ImGui::Checkbox("Audio", &ui.audio_window);
        if (ImGui::Button("Controller / keyboard bindings (F2)"))
            ui.open_controls = true;
    }
    ImGui::End();

    if (ui.state_window) {
        const bool open = ImGui::Begin("Tetris State", &ui.state_window);
        if (open) {
            if (ImGui::Button(ui.paused ? "Resume" : "Pause"))
                ui.paused = !ui.paused;
            ImGui::SameLine();
            if (ImGui::Button("Step"))
                ui.step = true;
            const SinglePlayer& game = flow.game();
            ImGui::SeparatorText("Session");
            ImGui::Text("Screen: %s", screen_name(flow.screen()));
            ImGui::Text("State: %s", state_name(game.state()));
            ImGui::Text("Level %d  Lines %d  Score %u", game.level(), game.lines(), game.score());
            ImGui::Text("Piece %d  Rotation %d  Origin (%d, %d)",
                        static_cast<int>(game.piece().kind), static_cast<int>(game.piece().rotation),
                        game.piece().origin.x, game.piece().origin.y);
            ImGui::Text("Preview: %d:%d  Visible: %s",
                        static_cast<int>(game.preview().kind),
                        static_cast<int>(game.preview().rotation),
                        game.preview_visible() ? "yes" : "no");
        }
        ImGui::End();
    }

    if (ui.board_window) {
        if (ImGui::Begin("Board", &ui.board_window)) {
            if (flow.screen() == Screen::versus_gameplay) {
                ImGui::TextUnformatted("Player one");
                draw_board(ui, flow.versus().player(0).board(), 0);
                ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::TextUnformatted("Player two");
                draw_board(ui, flow.versus().player(1).board(), 1);
                ImGui::EndGroup();
            } else {
                draw_board(ui, flow.game().board(), 0);
            }
        }
        ImGui::End();
    }

    if (ui.flow_window) {
        if (ImGui::Begin("Game Flow", &ui.flow_window)) {
            ImGui::Text("Screen: %s", screen_name(flow.screen()));
            ImGui::Text("Timer: %d", flow.timer());
            ImGui::Text("Ending stage: %d", static_cast<int>(flow.ending_stage()));
            ImGui::Text("Ending elapsed: %d", flow.ending_elapsed());
            ImGui::Text("Mode: %d  Level: %d  Height: %d", static_cast<int>(flow.selected_type()),
                        flow.selected_level(), flow.selected_height());
        }
        ImGui::End();
    }

    if (ui.events_window) {
        if (ImGui::Begin("Game Events", &ui.events_window)) {
            for (const Event event : flow.game().events())
                ImGui::BulletText("event %d, value %d", static_cast<int>(event.type), event.value);
        }
        ImGui::End();
    }

    if (ui.content_window) {
        const bool open = ImGui::Begin("ROM Content", &ui.content_window);
        if (open) {
            ImGui::Text("Profile: %s", content.profile.c_str());
            ImGui::Text("SHA-1: %s", content.source_sha1.c_str());
            ImGui::Text("Tiles: %zu", content.tile_count());
            ImGui::Text("Tilemaps: %zu", content.tilemaps.size());
            ImGui::Text("Sprites: %zu", content.sprites.sprites.size());
            ImGui::Text("Presentation tables: %zu", content.presentation.size());
            ImGui::Text("Songs: %zu  Effects: %zu", content.audio.songs.size(),
                        content.audio.effects.size());
            if (ImGui::BeginTable("Provenance", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                for (const content::Tilemap& map : content.tilemaps) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(map.source.id.c_str());
                    const content::RomSpan span = map.source.spans.front();
                    ImGui::TableNextColumn(); ImGui::Text("0x%04zX", span.begin);
                    ImGui::TableNextColumn(); ImGui::Text("0x%04zX", span.end);
                    ImGui::TableNextColumn(); ImGui::Text("%zu/%zu%s", map.source.decoded_count,
                        map.source.expected_count, map.source.validated ? "" : " !");
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    if (ui.replay_window) {
        const bool open = ImGui::Begin("Deterministic Replay", &ui.replay_window);
        if (open) {
            if (ImGui::Button("Record")) ui.replay_request = ReplayRequest::record;
            ImGui::SameLine();
            if (ImGui::Button("Stop")) ui.replay_request = ReplayRequest::stop;
            ImGui::SameLine();
            if (ImGui::Button("Play")) ui.replay_request = ReplayRequest::play;
            ImGui::SameLine();
            if (ImGui::Button("Clear")) ui.replay_request = ReplayRequest::clear;
            ImGui::Text("%s  %zu / %zu", ui.replay_recording ? "recording" : ui.replay_playing ? "playing" : "idle",
                        ui.replay_position, ui.replay_size);
        }
        ImGui::End();
    }

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
            ImGui::TextDisabled("Commands apply at the next fixed-tick boundary.");
        }
        ImGui::End();
    }

    if (ui.randomizer_window) {
        const bool open = ImGui::Begin("Piece Randomizer", &ui.randomizer_window);
        if (open) {
            const SinglePlayer& game = flow.game();
            ImGui::Text("Host divider state: %u", static_cast<unsigned int>(host.divider));
            ImGui::Text("Last samples: %u, %u, %u",
                        static_cast<unsigned int>(host.random[0]),
                        static_cast<unsigned int>(host.random[1]),
                        static_cast<unsigned int>(host.random[2]));
            ImGui::Text("Active: %d:%d  Preview: %d:%d",
                        static_cast<int>(game.piece().kind),
                        static_cast<int>(game.piece().rotation),
                        static_cast<int>(game.preview().kind),
                        static_cast<int>(game.preview().rotation));
            ImGui::Text("Fixed sequence consumed: %zu", game.fixed_pieces_consumed());
        }
        ImGui::End();
    }

    if (ui.input_window) {
        const bool open = ImGui::Begin("Input Ownership", &ui.input_window);
        if (open) {
            draw_buttons("Player one", host.player_one);
            draw_buttons("Player two", host.player_two);
            ImGui::Text("Connected gamepads: %d", host.connected_gamepads);
            ImGui::TextUnformatted("ImGui gamepad navigation: disabled");
            if (ImGui::Button("Open persistent bindings"))
                ui.open_controls = true;
        }
        ImGui::End();
    }

    if (ui.display_window) {
        const bool open = ImGui::Begin("Display and Performance", &ui.display_window);
        if (open) {
            ImGui::Text("Render target: %d x %d", host.render_width, host.render_height);
            ImGui::Text("Frame: %.3f ms", host.frame_seconds * 1'000.0);
            ImGui::Text("Fixed-tick accumulator: %.3f ms",
                        host.accumulator_seconds * 1'000.0);
            ImGui::Text("Simulation tick: %llu",
                        static_cast<unsigned long long>(flow.game().tick_count()));
            ImGui::Text("Layout: %s", presentation::name(settings.layout));
            ImGui::Text("Effects: %s", presentation::name(settings.effects));
            ImGui::TextUnformatted("Simulation: 59.7275 Hz; rendering is independent.");
        }
        ImGui::End();
    }

    if (ui.settings_window) {
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

} // namespace tetris::app
