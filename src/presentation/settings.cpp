#include "presentation/settings.hpp"

#include <charconv>
#include <fstream>
#include <string_view>

namespace tetris::presentation {
namespace {

constexpr int current_schema = 1;

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
    if (key == "preset") {
        settings.preset = value == "original" ? Preset::original
                        : value == "enhanced" ? Preset::enhanced
                                                : Preset::custom;
        return value == "original" || value == "enhanced" || value == "custom";
    }
    if (key == "line_clear_speed") {
        settings.line_clear_speed = value == "original" ? LineClearSpeed::original
                                  : value == "fast" ? LineClearSpeed::fast
                                                    : LineClearSpeed::instant;
        return value == "original" || value == "fast" || value == "instant";
    }
    if (key == "layout") {
        settings.layout = value == "original" ? Layout::original_screen
                                               : Layout::widescreen_frame;
        return value == "original" || value == "widescreen";
    }
    if (key == "background") {
        settings.background = value == "off" ? Background::off : Background::stars;
        return value == "off" || value == "stars";
    }
    if (key == "effects") {
        settings.effects = value == "off" ? Intensity::off
                         : value == "subtle" ? Intensity::subtle
                                             : Intensity::full;
        return value == "off" || value == "subtle" || value == "full";
    }
    if (key == "reduced_flash")
        return parse_bool(value, settings.reduced_flash);
    if (key == "reduced_motion")
        return parse_bool(value, settings.reduced_motion);
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
    settings.effects = Intensity::off;
    return settings;
}

Settings enhanced_settings() { return {}; }

void apply_preset(Settings& settings, Preset preset) {
    const bool reduced_flash = settings.reduced_flash;
    const bool reduced_motion = settings.reduced_motion;
    const float music_volume = settings.music_volume;
    const float effects_volume = settings.effects_volume;
    if (preset == Preset::original)
        settings = original_settings();
    else if (preset == Preset::enhanced)
        settings = enhanced_settings();
    else
        settings.preset = Preset::custom;
    settings.reduced_flash = reduced_flash;
    settings.reduced_motion = reduced_motion;
    settings.music_volume = music_volume;
    settings.effects_volume = effects_volume;
}

bool load_settings(const std::filesystem::path& path, Settings& settings, std::string& error) {
    error.clear();
    std::ifstream input(path);
    if (!input)
        return !std::filesystem::exists(path);
    Settings loaded;
    int schema = 0;
    std::string line;
    int number = 0;
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
    settings = loaded;
    return true;
}

bool save_settings(const std::filesystem::path& path, const Settings& settings, std::string& error) {
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
           << "effects=" << name(settings.effects) << '\n'
           << "reduced_flash=" << (settings.reduced_flash ? "true" : "false") << '\n'
           << "reduced_motion=" << (settings.reduced_motion ? "true" : "false") << '\n'
           << "music_volume=" << settings.music_volume << '\n'
           << "effects_volume=" << settings.effects_volume << '\n';
    output.close();
    if (!output) {
        error = "could not finish settings file";
        return false;
    }
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
    return value == Preset::original ? "original" : value == Preset::enhanced ? "enhanced" : "custom";
}
const char* name(LineClearSpeed value) {
    return value == LineClearSpeed::original ? "original" : value == LineClearSpeed::fast ? "fast" : "instant";
}
const char* name(Layout value) { return value == Layout::original_screen ? "original" : "widescreen"; }
const char* name(Background value) { return value == Background::off ? "off" : "stars"; }
const char* name(Intensity value) {
    return value == Intensity::off ? "off" : value == Intensity::subtle ? "subtle" : "full";
}

} // namespace tetris::presentation
