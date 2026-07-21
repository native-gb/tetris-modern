#include "settings.hpp"
#include "storage/high_scores.hpp"
#include "video/effects.hpp"

#include <array>
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
    expect(tetris::save_high_scores(path, source, error), "high scores save atomically");
    tetris::HighScores loaded;
    expect(tetris::load_high_scores(path, loaded, error), "high scores load");
    expect(loaded.table(tetris::GameType::type_a, 4, 0)[0] == tetris::ScoreEntry{123'456, "VEGA"},
           "Type A score round-trips");
    expect(loaded.table(tetris::GameType::type_b, 9, 5)[2] == tetris::ScoreEntry{999'999, "ABC ."},
           "quoted six-character name round-trips");
    std::filesystem::remove(path);
}

void test_high_score_merge_preserves_other_process() {
    const std::filesystem::path path = temporary("native-gb-tetris-modern-score-merge.txt");
    std::filesystem::remove(path);
    tetris::HighScores first;
    tetris::HighScores second;
    expect(first.set_entry(tetris::GameType::type_a, 0, 0, 0, {20'000, "FIRST"}),
           "first process score can be installed");
    std::string error;
    expect(tetris::save_high_scores(path, first, error), "first process score reaches disk");
    expect(second.set_entry(tetris::GameType::type_a, 0, 0, 0, {30'000, "SECOND"}),
           "second process score can be installed");
    expect(tetris::merge_and_save_high_scores(path, second, error),
           "second process merges before saving");
    const auto& table = second.table(tetris::GameType::type_a, 0, 0);
    expect(table[0] == tetris::ScoreEntry{30'000, "SECOND"} &&
               table[1] == tetris::ScoreEntry{20'000, "FIRST"},
           "merged leaderboard retains both processes' scores");
    std::filesystem::remove(path);
}

void test_settings_schema_and_migration() {
    const std::filesystem::path path = temporary("native-gb-tetris-modern-settings.cfg");
    std::filesystem::remove(path);
    tetris::settings::Settings source = tetris::settings::original_settings();
    source.music_volume = 0.25F;
    std::string error;
    expect(tetris::settings::save_settings(path, source, error), "settings save");
    tetris::settings::Settings loaded;
    expect(tetris::settings::load_settings(path, loaded, error), "schema-one settings load");
    expect(loaded.preset == tetris::settings::Preset::original && loaded.music_volume == 0.25F &&
               !loaded.polyphonic_sfx,
           "settings values round-trip");

    source = tetris::settings::modern_settings();
    source.renderer = tetris::video::RenderBackend::cpu_raster;
    source.motion_interpolation = false;
    source.render_rate_limit = 240;
    source.vsync = false;
    expect(tetris::settings::save_settings(path, source, error), "modern settings save");
    expect(tetris::settings::load_settings(path, loaded, error), "modern settings load");
    expect(loaded == source, "all modern gameplay settings round-trip");
    expect(tetris::video::effective_render_rate(false, 240) == 60 &&
               tetris::video::effective_render_rate(true, 240) == 240,
           "presentation rate stays bounded and interpolation-independent");

    std::ofstream previous_default(path, std::ios::trunc);
    previous_default << "schema=11\npreset=enhanced\ncontroller_labels=xbox\n"
                        "music_volume=0.25\neffects_volume=0.75\n";
    previous_default.close();
    expect(tetris::settings::load_settings(path, loaded, error),
           "previous Enhanced default loads");
    expect(loaded.preset == tetris::settings::Preset::modern &&
               loaded.gameplay.randomizer == tetris::RandomizerMode::seven_bag &&
               loaded.gameplay.rotation == tetris::RotationSystem::srs &&
               loaded.gameplay.hold_piece && loaded.gameplay.ghost_piece &&
               loaded.controller_labels == tetris::settings::ControllerLabels::xbox &&
               loaded.music_volume == 0.25F,
           "previous Enhanced default migrates to Modern without losing preferences");

    std::ofstream legacy(path, std::ios::trunc);
    legacy << "preset=enhanced\nline_clear_speed=fast\nlayout=widescreen\n"
              "background=stars\neffects=subtle\nreduced_flash=false\n"
              "reduced_motion=true\nmusic_volume=0.5\neffects_volume=0.75\n";
    legacy.close();
    expect(tetris::settings::load_settings(path, loaded, error),
           "schema-zero settings migrate without a header");
    expect(loaded.background == tetris::settings::Background::snow && !loaded.screen_shake &&
               !loaded.moving_background && !loaded.title_parallax &&
               loaded.effects_volume == 0.75F && loaded.polyphonic_sfx &&
               loaded.preset == tetris::settings::Preset::modern,
           "legacy particle field and Reduced Motion migrate to explicit modern settings");
    std::filesystem::remove(path);
}

void test_presets_and_semantic_effects() {
    using namespace tetris;
    using namespace tetris::settings;
    using namespace tetris::video;
    Settings settings = original_settings();
    const Settings compiled_defaults;
    expect(compiled_defaults.preset == Preset::modern &&
               compiled_defaults.gameplay.randomizer == RandomizerMode::seven_bag &&
               compiled_defaults.gameplay.rotation == RotationSystem::srs &&
               compiled_defaults.gameplay.hold_piece && compiled_defaults.gameplay.ghost_piece &&
               compiled_defaults.gameplay.next_previews == 5 &&
               compiled_defaults.extended_canvas,
           "fresh profiles use the complete Modern preset");
    expect(settings.controller_labels == ControllerLabels::nintendo,
           "Nintendo controller legends are the compiled default");
    expect(!settings.motion_interpolation &&
               effective_render_rate(settings.motion_interpolation,
                                     settings.render_rate_limit) == 60,
           "Tetris defaults to discrete 60 Hz presentation");
    settings.reduced_flash = true;
    settings.music_volume = 0.25F;
    settings.controller_labels = ControllerLabels::playstation;
    apply_preset(settings, Preset::enhanced);
    expect(settings.preset == Preset::enhanced &&
               settings.line_clear_speed == LineClearSpeed::fast &&
               settings.layout == Layout::widescreen_frame &&
               settings.background == Background::snow && settings.effects == Intensity::subtle &&
               settings.polyphonic_sfx && settings.title_parallax &&
               settings.gameplay.drop_mode == DropMode::hard &&
               settings.gameplay.rotation == RotationSystem::side_pushout &&
               settings.gameplay.horizontal_repeat_delay == 12 &&
               settings.gameplay.horizontal_repeat_interval == 2 &&
               settings.gameplay.entry_delay == 6 &&
               settings.gameplay.separate_rotation_inputs &&
               settings.piece_colors == PieceColors::guideline,
           "Enhanced preset selects every recommended enhancement");
    expect(settings.reduced_flash && settings.music_volume == 0.25F &&
               settings.controller_labels == ControllerLabels::playstation,
           "accessibility, controller-label, and volume overrides survive preset changes");

    EffectState effects;
    constexpr std::array<Event, 1> landed{{{GameEvent::landed, 0}}};
    advance(effects, landed, settings);
    expect(effects.shake_steps > 0 && (shake_offset(effects, settings).x != 0.0F ||
                                       shake_offset(effects, settings).y != 0.0F),
           "landing event starts a visible semantic shake");

    constexpr std::array<Event, 1> tetris{{{GameEvent::cleared_lines, 4}}};
    advance(effects, tetris, settings);
    expect(effects.shake_strength >= 2.0F && background_pulse(effects, settings) == 0.0F,
           "Tetris strengthens shake while Reduced Flash suppresses its pulse");

    settings.reduced_flash = false;
    settings.screen_shake = false;
    effects = {};
    advance(effects, tetris, settings);
    expect(effects.shake_steps == 0 && background_pulse(effects, settings) > 0.0F,
           "disabled screen shake does not suppress an allowed pulse");

    apply_preset(settings, Preset::original);
    expect(!settings.polyphonic_sfx && !settings.title_parallax &&
               settings.piece_colors == PieceColors::game_boy &&
               settings.gameplay.horizontal_repeat_delay == 23 &&
               settings.gameplay.horizontal_repeat_interval == 9 &&
               settings.gameplay.entry_delay == 3,
           "Original preset restores Game Boy sound and title presentation");
    effects = {};
    advance(effects, tetris, settings);
    expect(effects.shake_steps == 0 && background_pulse(effects, settings) == 0.0F,
           "Original preset disables host effects");

    apply_preset(settings, Preset::modern);
    expect(settings.preset == Preset::modern &&
               settings.gameplay.drop_mode == DropMode::hard &&
               settings.gameplay.randomizer == RandomizerMode::seven_bag &&
               settings.gameplay.rotation == RotationSystem::srs &&
               settings.gameplay.next_previews == 5 && settings.gameplay.ghost_piece &&
               settings.gameplay.hold_piece && settings.gameplay.lock_delay &&
               settings.gameplay.horizontal_repeat_delay == 12 &&
               settings.gameplay.horizontal_repeat_interval == 2 &&
               settings.gameplay.entry_delay == 6 &&
               settings.gameplay.modern_scoring && settings.gameplay.quick_restart &&
               settings.extended_canvas && settings.title_parallax &&
               settings.piece_colors == PieceColors::guideline,
           "Modern preset enables the complete modern ruleset");
}

} // namespace

int main() {
    test_high_scores_round_trip();
    test_high_score_merge_preserves_other_process();
    test_settings_schema_and_migration();
    test_presets_and_semantic_effects();
    if (failures != 0)
        return 1;
    std::puts("modern Native Tetris persistence tests passed");
    return 0;
}
