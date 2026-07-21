#pragma once

#include "game/single_player.hpp"
#include "game/options.hpp"
#include "video/backend.hpp"

#include <filesystem>
#include <string>

namespace tetris::settings {

enum class Preset { original, enhanced, modern, custom };
enum class Layout { original_screen, widescreen_frame };
enum class Background { off, snow, stars };
enum class PieceColors { game_boy, guideline };
enum class Intensity { off, subtle, full };
enum class ControllerLabels { automatic, nintendo, xbox, playstation };

struct Settings {
    Preset preset{Preset::modern};
    LineClearSpeed line_clear_speed{LineClearSpeed::fast};
    Layout layout{Layout::widescreen_frame};
    Background background{Background::snow};
    PieceColors piece_colors{PieceColors::guideline};
    Intensity effects{Intensity::full};
    GameplayOptions gameplay{
        .drop_mode = DropMode::hard,
        .randomizer = RandomizerMode::seven_bag,
        .rotation = RotationSystem::srs,
        .challenge = ChallengeMode::marathon,
        .next_previews = 5,
        .horizontal_repeat_delay = 12,
        .horizontal_repeat_interval = 2,
        .entry_delay = 6,
        .instant_autorepeat = false,
        .separate_rotation_inputs = true,
        .buffered_inputs = true,
        .ghost_piece = true,
        .hold_piece = true,
        .lock_delay = true,
        .modern_scoring = true,
        .quick_restart = true,
    };
    bool extended_canvas{true};
    bool polyphonic_sfx{true};
    bool screen_shake{true};
    bool moving_background{true};
    bool title_parallax{true};
    bool reduced_flash{};
    video::RenderBackend renderer{video::RenderBackend::gpu_atlas};
    bool motion_interpolation{};
    int render_rate_limit{60};
    bool vsync{true};
    ControllerLabels controller_labels{ControllerLabels::nintendo};
    float music_volume{0.8F};
    float effects_volume{0.9F};

    bool operator==(const Settings&) const = default;
};

Settings original_settings();
Settings enhanced_settings();
Settings modern_settings();
void apply_preset(Settings& settings, Preset preset);

bool load_settings(const std::filesystem::path& path, Settings& settings, std::string& error);
bool save_settings(const std::filesystem::path& path, const Settings& settings, std::string& error);

const char* name(Preset value);
const char* name(LineClearSpeed value);
const char* name(Layout value);
const char* name(Background value);
const char* name(PieceColors value);
const char* name(Intensity value);
const char* name(ControllerLabels value);
const char* name(DropMode value);
const char* name(RandomizerMode value);
const char* name(RotationSystem value);
const char* name(ChallengeMode value);
const char* name(video::RenderBackend value);

} // namespace tetris::settings
