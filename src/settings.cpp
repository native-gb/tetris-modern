#include "settings.hpp"

#include <charconv>
#include <fstream>
#include <string_view>

namespace tetris::settings {
namespace {

constexpr int current_schema = 12;

bool parse_bool(std::string_view text, bool& value) {
    if (text == "true") {
        value = true;
        return true;
    }
    if (text == "false") {
        value = false;
        return true;
    }
    return false;
}

bool parse_float(std::string_view text, float& value) {
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end && value >= 0.0F && value <= 1.0F;
}

bool parse_int(std::string_view text, int& value) {
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool parse(Settings& settings, std::string_view key, std::string_view value) {
    // Enumerated settings use their stable human-readable names.
    if (key == "preset") {
        settings.preset = value == "original"   ? Preset::original
                          : value == "enhanced" ? Preset::enhanced
                          : value == "modern"   ? Preset::modern
                                                : Preset::custom;
        return value == "original" || value == "enhanced" || value == "modern" ||
               value == "custom";
    }
    if (key == "line_clear_speed") {
        settings.line_clear_speed = value == "original" ? LineClearSpeed::original
                                    : value == "fast"   ? LineClearSpeed::fast
                                                        : LineClearSpeed::instant;
        return value == "original" || value == "fast" || value == "instant";
    }
    if (key == "layout") {
        settings.layout = value == "original" ? Layout::original_screen : Layout::widescreen_frame;
        return value == "original" || value == "widescreen";
    }
    if (key == "background") {
        settings.background = value == "off"    ? Background::off
                              : value == "snow" ? Background::snow
                                                  : Background::stars;
        return value == "off" || value == "snow" || value == "stars";
    }
    if (key == "piece_colors") {
        settings.piece_colors =
            value == "game_boy" ? PieceColors::game_boy : PieceColors::guideline;
        return value == "game_boy" || value == "guideline";
    }
    if (key == "effects") {
        settings.effects = value == "off"      ? Intensity::off
                           : value == "subtle" ? Intensity::subtle
                                               : Intensity::full;
        return value == "off" || value == "subtle" || value == "full";
    }
    if (key == "controller_labels") {
        settings.controller_labels =
            value == "auto"       ? ControllerLabels::automatic
            : value == "nintendo" ? ControllerLabels::nintendo
            : value == "xbox"     ? ControllerLabels::xbox
                                    : ControllerLabels::playstation;
        return value == "auto" || value == "nintendo" || value == "xbox" ||
               value == "playstation";
    }
    if (key == "renderer") {
        settings.renderer = value == "gpu" ? video::RenderBackend::gpu_atlas
                                            : video::RenderBackend::cpu_raster;
        return value == "gpu" || value == "cpu";
    }
    if (key == "drop_mode") {
        settings.gameplay.drop_mode = value == "soft"    ? DropMode::soft_only
                                      : value == "sonic" ? DropMode::sonic
                                                          : DropMode::hard;
        return value == "soft" || value == "sonic" || value == "hard";
    }
    if (key == "randomizer") {
        settings.gameplay.randomizer = value == "original" ? RandomizerMode::original
                                       : value == "bag"    ? RandomizerMode::seven_bag
                                                           : RandomizerMode::history;
        return value == "original" || value == "bag" || value == "history";
    }
    if (key == "rotation") {
        settings.gameplay.rotation = value == "original" ? RotationSystem::original
                                     : value == "pushout" ? RotationSystem::side_pushout
                                                           : RotationSystem::srs;
        return value == "original" || value == "pushout" || value == "srs";
    }
    if (key == "challenge") {
        settings.gameplay.challenge = value == "marathon" ? ChallengeMode::marathon
                                      : value == "sprint" ? ChallengeMode::sprint
                                                          : ChallengeMode::ultra;
        return value == "marathon" || value == "sprint" || value == "ultra";
    }
    if (key == "next_previews") {
        int previews = 0;
        if (!parse_int(value, previews) || previews < 1 || previews > 5)
            return false;
        settings.gameplay.next_previews = previews;
        return true;
    }

    // Scalar settings share strict conversion helpers.
    if (key == "horizontal_repeat_delay") {
        int frames = 0;
        if (!parse_int(value, frames) || frames < 0 || frames > 60)
            return false;
        settings.gameplay.horizontal_repeat_delay = frames;
        return true;
    }
    if (key == "horizontal_repeat_interval") {
        int frames = 0;
        if (!parse_int(value, frames) || frames < 1 || frames > 30)
            return false;
        settings.gameplay.horizontal_repeat_interval = frames;
        return true;
    }
    if (key == "entry_delay") {
        int frames = 0;
        if (!parse_int(value, frames) || frames < 0 || frames > 30)
            return false;
        settings.gameplay.entry_delay = frames;
        return true;
    }
    if (key == "instant_autorepeat")
        return parse_bool(value, settings.gameplay.instant_autorepeat);
    if (key == "separate_rotation_inputs")
        return parse_bool(value, settings.gameplay.separate_rotation_inputs);
    if (key == "dx_repeat") {
        bool enabled = false;
        if (!parse_bool(value, enabled))
            return false;
        settings.gameplay.horizontal_repeat_delay = enabled ? 8 : 23;
        settings.gameplay.horizontal_repeat_interval = enabled ? 3 : 9;
        return true;
    }
    if (key == "buffered_inputs")
        return parse_bool(value, settings.gameplay.buffered_inputs);
    if (key == "ghost_piece")
        return parse_bool(value, settings.gameplay.ghost_piece);
    if (key == "hold_piece")
        return parse_bool(value, settings.gameplay.hold_piece);
    if (key == "lock_delay")
        return parse_bool(value, settings.gameplay.lock_delay);
    if (key == "modern_scoring")
        return parse_bool(value, settings.gameplay.modern_scoring);
    if (key == "quick_restart")
        return parse_bool(value, settings.gameplay.quick_restart);
    if (key == "extended_canvas")
        return parse_bool(value, settings.extended_canvas);
    if (key == "polyphonic_sfx")
        return parse_bool(value, settings.polyphonic_sfx);
    if (key == "screen_shake")
        return parse_bool(value, settings.screen_shake);
    if (key == "moving_background" || key == "moving_stars")
        return parse_bool(value, settings.moving_background);
    if (key == "title_parallax")
        return parse_bool(value, settings.title_parallax);
    if (key == "reduced_flash")
        return parse_bool(value, settings.reduced_flash);
    if (key == "motion_interpolation")
        return parse_bool(value, settings.motion_interpolation);
    if (key == "render_rate_limit") {
        int rate = 0;
        if (!parse_int(value, rate) || !video::valid_render_rate(rate))
            return false;
        settings.render_rate_limit = rate;
        return true;
    }
    if (key == "vsync")
        return parse_bool(value, settings.vsync);
    if (key == "reduced_motion") {
        bool reduced = false;
        if (!parse_bool(value, reduced))
            return false;
        settings.screen_shake = !reduced;
        settings.moving_background = !reduced;
        settings.title_parallax = !reduced;
        return true;
    }
    if (key == "music_volume")
        return parse_float(value, settings.music_volume);
    if (key == "effects_volume")
        return parse_float(value, settings.effects_volume);
    return false;
}

} // namespace

Settings original_settings() {
    Settings settings;
    settings.preset = Preset::original;
    settings.line_clear_speed = LineClearSpeed::original;
    settings.layout = Layout::original_screen;
    settings.background = Background::off;
    settings.piece_colors = PieceColors::game_boy;
    settings.effects = Intensity::off;
    settings.extended_canvas = false;
    settings.gameplay = {
        .drop_mode = DropMode::soft_only,
        .randomizer = RandomizerMode::original,
        .rotation = RotationSystem::original,
        .challenge = ChallengeMode::marathon,
        .next_previews = 1,
        .horizontal_repeat_delay = 23,
        .horizontal_repeat_interval = 9,
        .entry_delay = 3,
        .instant_autorepeat = false,
        .separate_rotation_inputs = false,
    };
    settings.polyphonic_sfx = false;
    settings.title_parallax = false;
    return settings;
}

Settings enhanced_settings() {
    Settings settings;
    settings.preset = Preset::enhanced;
    settings.effects = Intensity::subtle;
    settings.extended_canvas = false;
    settings.gameplay = {
        .drop_mode = DropMode::hard,
        .randomizer = RandomizerMode::original,
        .rotation = RotationSystem::side_pushout,
        .challenge = ChallengeMode::marathon,
        .next_previews = 1,
        .horizontal_repeat_delay = 12,
        .horizontal_repeat_interval = 2,
        .entry_delay = 6,
        .instant_autorepeat = false,
        .separate_rotation_inputs = true,
    };
    return settings;
}

Settings modern_settings() {
    return {};
}

void apply_preset(Settings& settings, Preset preset) {
    // Accessibility and volume controls survive visual preset changes.
    const bool reduced_flash = settings.reduced_flash;
    const bool screen_shake = settings.screen_shake;
    const bool moving_background = settings.moving_background;
    const ControllerLabels controller_labels = settings.controller_labels;
    const float music_volume = settings.music_volume;
    const float effects_volume = settings.effects_volume;
    const video::RenderBackend renderer = settings.renderer;
    const bool motion_interpolation = settings.motion_interpolation;
    const int render_rate_limit = settings.render_rate_limit;
    const bool vsync = settings.vsync;

    // Replace the visual settings with the selected preset.
    if (preset == Preset::original)
        settings = original_settings();
    else if (preset == Preset::enhanced)
        settings = enhanced_settings();
    else if (preset == Preset::modern)
        settings = modern_settings();
    else
        settings.preset = Preset::custom;

    // Restore the settings that are deliberately outside presets.
    settings.reduced_flash = reduced_flash;
    settings.screen_shake = screen_shake;
    settings.moving_background = moving_background;
    settings.controller_labels = controller_labels;
    settings.music_volume = music_volume;
    settings.effects_volume = effects_volume;
    settings.renderer = renderer;
    settings.motion_interpolation = motion_interpolation;
    settings.render_rate_limit = render_rate_limit;
    settings.vsync = vsync;
}

bool load_settings(const std::filesystem::path& path, Settings& settings, std::string& error) {
    // A missing file means the compiled defaults remain active.
    error.clear();
    std::ifstream input(path);
    if (!input)
        return !std::filesystem::exists(path);
    Settings loaded;
    int schema = 0;
    std::string line;
    int number = 0;

    // Parse one key=value pair per non-comment line.
    while (std::getline(input, line)) {
        ++number;
        if (line.empty() || line.front() == '#')
            continue;
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) {
            error = "invalid setting on line " + std::to_string(number);
            return false;
        }
        const std::string_view key = std::string_view(line).substr(0, separator);
        const std::string_view value = std::string_view(line).substr(separator + 1);
        if (key == "schema") {
            if (!parse_int(value, schema) || schema < 0 || schema > current_schema) {
                error = "unsupported settings schema on line " + std::to_string(number);
                return false;
            }
            continue;
        }
        if (!parse(loaded, key, value)) {
            error = "invalid setting on line " + std::to_string(number);
            return false;
        }
    }
    if (schema < 2)
        loaded.polyphonic_sfx = loaded.preset != Preset::original;
    if (schema < 4 && loaded.preset == Preset::modern)
        loaded.extended_canvas = true;
    if (schema < 5 && loaded.preset == Preset::original)
        loaded.title_parallax = false;
    // The old Stars mode was a field of square particles falling like snow.
    // Schema six gives that established default an explicit thematic identity.
    if (schema < 6 && loaded.background == Background::stars)
        loaded.background = Background::snow;
    if (schema < 9) {
        if (loaded.preset == Preset::original) {
            loaded.gameplay.separate_rotation_inputs = false;
            loaded.piece_colors = PieceColors::game_boy;
        } else if (loaded.preset == Preset::enhanced) {
            loaded.gameplay.drop_mode = DropMode::hard;
            loaded.gameplay.rotation = RotationSystem::side_pushout;
            loaded.gameplay.horizontal_repeat_delay = 12;
            loaded.gameplay.horizontal_repeat_interval = 2;
            loaded.gameplay.separate_rotation_inputs = true;
            loaded.piece_colors = PieceColors::guideline;
        } else if (loaded.preset == Preset::modern) {
            loaded.gameplay.horizontal_repeat_delay = 12;
            loaded.gameplay.horizontal_repeat_interval = 2;
            loaded.gameplay.separate_rotation_inputs = true;
            loaded.piece_colors = PieceColors::guideline;
        }
    }
    if (schema < 10) {
        loaded.gameplay.entry_delay =
            loaded.preset == Preset::enhanced || loaded.preset == Preset::modern ? 6 : 3;
    }
    if (schema < 12 && loaded.preset == Preset::enhanced) {
        const bool title_parallax = loaded.title_parallax;
        apply_preset(loaded, Preset::modern);
        loaded.title_parallax = title_parallax;
    }
    settings = loaded;
    return true;
}

bool save_settings(const std::filesystem::path& path, const Settings& settings,
                   std::string& error) {
    // Write the complete settings file to a sibling temporary path.
    error.clear();
    const std::filesystem::path temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) {
        error = "could not create settings file";
        return false;
    }
    output << "schema=" << current_schema << '\n'
           << "preset=" << name(settings.preset) << '\n'
           << "line_clear_speed=" << name(settings.line_clear_speed) << '\n'
           << "layout=" << name(settings.layout) << '\n'
           << "background=" << name(settings.background) << '\n'
           << "piece_colors=" << name(settings.piece_colors) << '\n'
           << "effects=" << name(settings.effects) << '\n'
           << "drop_mode=" << name(settings.gameplay.drop_mode) << '\n'
           << "randomizer=" << name(settings.gameplay.randomizer) << '\n'
           << "rotation=" << name(settings.gameplay.rotation) << '\n'
           << "challenge=" << name(settings.gameplay.challenge) << '\n'
           << "next_previews=" << settings.gameplay.next_previews << '\n'
           << "horizontal_repeat_delay=" << settings.gameplay.horizontal_repeat_delay << '\n'
           << "horizontal_repeat_interval=" << settings.gameplay.horizontal_repeat_interval
           << '\n'
           << "entry_delay=" << settings.gameplay.entry_delay << '\n'
           << "instant_autorepeat="
           << (settings.gameplay.instant_autorepeat ? "true" : "false") << '\n'
           << "separate_rotation_inputs="
           << (settings.gameplay.separate_rotation_inputs ? "true" : "false") << '\n'
           << "buffered_inputs=" << (settings.gameplay.buffered_inputs ? "true" : "false")
           << '\n'
           << "ghost_piece=" << (settings.gameplay.ghost_piece ? "true" : "false") << '\n'
           << "hold_piece=" << (settings.gameplay.hold_piece ? "true" : "false") << '\n'
           << "lock_delay=" << (settings.gameplay.lock_delay ? "true" : "false") << '\n'
           << "modern_scoring=" << (settings.gameplay.modern_scoring ? "true" : "false")
           << '\n'
           << "quick_restart=" << (settings.gameplay.quick_restart ? "true" : "false") << '\n'
           << "extended_canvas=" << (settings.extended_canvas ? "true" : "false") << '\n'
           << "polyphonic_sfx=" << (settings.polyphonic_sfx ? "true" : "false") << '\n'
           << "screen_shake=" << (settings.screen_shake ? "true" : "false") << '\n'
           << "moving_background=" << (settings.moving_background ? "true" : "false") << '\n'
           << "title_parallax=" << (settings.title_parallax ? "true" : "false") << '\n'
           << "reduced_flash=" << (settings.reduced_flash ? "true" : "false") << '\n'
           << "renderer=" << name(settings.renderer) << '\n'
           << "motion_interpolation="
           << (settings.motion_interpolation ? "true" : "false") << '\n'
           << "render_rate_limit=" << settings.render_rate_limit << '\n'
           << "vsync=" << (settings.vsync ? "true" : "false") << '\n'
           << "controller_labels=" << name(settings.controller_labels) << '\n'
           << "music_volume=" << settings.music_volume << '\n'
           << "effects_volume=" << settings.effects_volume << '\n';
    output.close();
    if (!output) {
        error = "could not finish settings file";
        return false;
    }

    // Replace the live file, with an explicit remove fallback for platforms
    // that cannot rename over an existing path.
    std::error_code filesystem_error;
    std::filesystem::rename(temporary, path, filesystem_error);
    if (!filesystem_error)
        return true;
    std::filesystem::remove(path, filesystem_error);
    filesystem_error.clear();
    std::filesystem::rename(temporary, path, filesystem_error);
    if (!filesystem_error)
        return true;
    error = filesystem_error.message();
    return false;
}

const char* name(Preset value) {
    return value == Preset::original   ? "original"
           : value == Preset::enhanced ? "enhanced"
           : value == Preset::modern   ? "modern"
                                       : "custom";
}
const char* name(LineClearSpeed value) {
    return value == LineClearSpeed::original ? "original"
           : value == LineClearSpeed::fast   ? "fast"
                                             : "instant";
}
const char* name(Layout value) {
    return value == Layout::original_screen ? "original" : "widescreen";
}
const char* name(Background value) {
    return value == Background::off    ? "off"
           : value == Background::snow ? "snow"
                                       : "stars";
}
const char* name(PieceColors value) {
    return value == PieceColors::game_boy ? "game_boy" : "guideline";
}
const char* name(Intensity value) {
    return value == Intensity::off ? "off" : value == Intensity::subtle ? "subtle" : "full";
}
const char* name(ControllerLabels value) {
    return value == ControllerLabels::automatic ? "auto"
           : value == ControllerLabels::nintendo ? "nintendo"
           : value == ControllerLabels::xbox     ? "xbox"
                                                 : "playstation";
}
const char* name(DropMode value) {
    return value == DropMode::soft_only ? "soft" : value == DropMode::sonic ? "sonic" : "hard";
}
const char* name(RandomizerMode value) {
    return value == RandomizerMode::original   ? "original"
           : value == RandomizerMode::seven_bag ? "bag"
                                                 : "history";
}
const char* name(RotationSystem value) {
    return value == RotationSystem::original      ? "original"
           : value == RotationSystem::side_pushout ? "pushout"
                                                    : "srs";
}
const char* name(ChallengeMode value) {
    return value == ChallengeMode::marathon ? "marathon"
           : value == ChallengeMode::sprint ? "sprint"
                                             : "ultra";
}

const char* name(video::RenderBackend value) {
    return value == video::RenderBackend::gpu_atlas ? "gpu" : "cpu";
}

} // namespace tetris::settings
