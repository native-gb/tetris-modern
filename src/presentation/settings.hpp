#pragma once

#include "game/single_player.hpp"

#include <filesystem>
#include <string>

namespace tetris::presentation {

enum class Preset { original, enhanced, custom };
enum class Layout { original_screen, widescreen_frame };
enum class Background { off, stars };
enum class Intensity { off, subtle, full };

struct Settings {
    Preset preset{Preset::enhanced};
    LineClearSpeed line_clear_speed{LineClearSpeed::fast};
    Layout layout{Layout::widescreen_frame};
    Background background{Background::stars};
    Intensity effects{Intensity::subtle};
    bool reduced_flash{};
    bool reduced_motion{};
    float music_volume{0.8F};
    float effects_volume{0.9F};
};

Settings original_settings();
Settings enhanced_settings();
void apply_preset(Settings& settings, Preset preset);

bool load_settings(const std::filesystem::path& path, Settings& settings, std::string& error);
bool save_settings(const std::filesystem::path& path, const Settings& settings, std::string& error);

const char* name(Preset value);
const char* name(LineClearSpeed value);
const char* name(Layout value);
const char* name(Background value);
const char* name(Intensity value);

} // namespace tetris::presentation
