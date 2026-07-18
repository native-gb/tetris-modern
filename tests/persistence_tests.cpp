#include "app/persistence.hpp"
#include "presentation/settings.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (condition)
        return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

std::filesystem::path temporary(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

void test_high_scores_round_trip() {
    const std::filesystem::path path = temporary("native-gb-tetris-modern-scores.txt");
    std::filesystem::remove(path);
    tetris::HighScores source;
    expect(source.set_entry(tetris::GameType::type_a, 4, 0, 0, {123'456, "VEGA"}),
           "valid score can be installed");
    expect(source.set_entry(tetris::GameType::type_b, 9, 5, 2, {999'999, "ABC ."}),
           "Type B score can be installed");
    std::string error;
    expect(tetris::app::save_high_scores(path, source, error), "high scores save atomically");
    tetris::HighScores loaded;
    expect(tetris::app::load_high_scores(path, loaded, error), "high scores load");
    expect(loaded.table(tetris::GameType::type_a, 4, 0)[0] ==
               tetris::ScoreEntry{123'456, "VEGA"},
           "Type A score round-trips");
    expect(loaded.table(tetris::GameType::type_b, 9, 5)[2] ==
               tetris::ScoreEntry{999'999, "ABC ."},
           "quoted six-character name round-trips");
    std::filesystem::remove(path);
}

void test_settings_schema_and_migration() {
    const std::filesystem::path path = temporary("native-gb-tetris-modern-settings.cfg");
    std::filesystem::remove(path);
    tetris::presentation::Settings source = tetris::presentation::original_settings();
    source.music_volume = 0.25F;
    std::string error;
    expect(tetris::presentation::save_settings(path, source, error), "settings save");
    tetris::presentation::Settings loaded;
    expect(tetris::presentation::load_settings(path, loaded, error), "schema-one settings load");
    expect(loaded.preset == tetris::presentation::Preset::original &&
               loaded.music_volume == 0.25F,
           "settings values round-trip");

    std::ofstream legacy(path, std::ios::trunc);
    legacy << "preset=enhanced\nline_clear_speed=fast\nlayout=widescreen\n"
              "background=stars\neffects=subtle\nreduced_flash=false\n"
              "reduced_motion=true\nmusic_volume=0.5\neffects_volume=0.75\n";
    legacy.close();
    expect(tetris::presentation::load_settings(path, loaded, error),
           "schema-zero settings migrate without a header");
    expect(loaded.reduced_motion && loaded.effects_volume == 0.75F,
           "legacy settings retain their values");
    std::filesystem::remove(path);
}

} // namespace

int main() {
    test_high_scores_round_trip();
    test_settings_schema_and_migration();
    if (failures != 0)
        return 1;
    std::puts("modern Native Tetris persistence tests passed");
    return 0;
}
