#include "game/flow.hpp"
#include "game/replay.hpp"

#include <array>
#include <cstdio>
#include <optional>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (condition)
        return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

tetris::StartupRandom startup() {
    tetris::StartupRandom random;
    random.pieces = {{{2, 3, 4}, {5, 6, 7}, {8, 9, 10}}};
    return random;
}

tetris::VersusRandom versus_random() {
    tetris::VersusRandom random;
    for (std::size_t index = 0; index < random.pieces.size(); ++index)
        random.pieces[index] = static_cast<tetris::PieceKind>(index % 7);
    return random;
}

tetris::FlowInput input(tetris::Buttons one = {}, tetris::Buttons two = {}) {
    return {.player_one = one, .player_two = two, .random = {11, 12, 13},
            .startup = startup(), .versus = versus_random()};
}

tetris::FlowResources resources() {
    static constexpr std::array<tetris::DemoRun, 2> type_a = {{
        {{.right = true}, 250}, {{.rotate_right = true}, 250},
    }};
    static constexpr std::array<tetris::DemoRun, 2> type_b = {{
        {{.left = true}, 250}, {{.down = true}, 250},
    }};
    static constexpr std::array<tetris::DemoPiece, 48> pieces{};
    static constexpr std::array<std::uint8_t, 40> garbage{};
    return {type_a, type_b, pieces, garbage};
}

void tick(tetris::GameFlow& flow, int frames) {
    for (int frame = 0; frame < frames; ++frame)
        flow.tick(input());
}

void press(tetris::GameFlow& flow, tetris::Buttons buttons,
           tetris::Buttons player_two = {}) {
    flow.tick(input(buttons, player_two));
    flow.tick(input());
}

void reach_title(tetris::GameFlow& flow) {
    tick(flow, 250);
    expect(flow.screen() == tetris::Screen::copyright_skippable,
           "first copyright interval cannot be skipped");
    press(flow, {.rotate_right = true});
    expect(flow.screen() == tetris::Screen::title, "confirm skips second copyright interval");
}

void test_type_b_menu_flow() {
    tetris::GameFlow flow;
    flow.start(resources());
    reach_title(flow);
    press(flow, {.start = true});
    press(flow, {.right = true});
    expect(flow.selected_type() == tetris::GameType::type_b, "right selects Type B");
    press(flow, {.rotate_right = true});
    expect(flow.screen() == tetris::Screen::music, "A opens music selection");
    press(flow, {.rotate_right = true});
    expect(flow.screen() == tetris::Screen::type_b_level, "music confirms to Type B level");
    press(flow, {.right = true});
    press(flow, {.rotate_right = true});
    expect(flow.screen() == tetris::Screen::type_b_height, "Type B level confirms to height");
    press(flow, {.right = true});
    press(flow, {.rotate_right = true});
    expect(flow.screen() == tetris::Screen::gameplay, "height starts gameplay");
    expect(flow.game().rules().starting_level == 1 && flow.game().rules().type_b_height == 1,
           "game receives menu choices");
}

void test_heart_mode_and_reset() {
    tetris::GameFlow flow;
    flow.start(resources());
    reach_title(flow);
    flow.tick(input({.down = true, .start = true}));
    flow.tick(input());
    expect(flow.heart_mode(), "Down plus Start enables heart mode");
    flow.tick(input({.rotate_left = true, .rotate_right = true,
                     .start = true, .select = true}));
    expect(flow.screen() == tetris::Screen::copyright_fixed && flow.timer() == 250,
           "four-button chord performs cartridge soft reset");
}

void test_versus_height_controls() {
    tetris::GameFlow flow;
    flow.start(resources());
    reach_title(flow);
    press(flow, {.start = true});
    press(flow, {.right = true});
    press(flow, {.right = true});
    press(flow, {.start = true});
    expect(flow.screen() == tetris::Screen::versus_height, "Start enters versus heights");
    press(flow, {.right = true}, {.down = true});
    expect(flow.versus_height(0) == 1 && flow.versus_height(1) == 3,
           "both players own independent height cursors");
    press(flow, {.rotate_right = true});
    expect(flow.screen() == tetris::Screen::versus_gameplay,
           "player one starts the local versus round");
}

void test_high_scores() {
    tetris::HighScores scores;
    const auto first = scores.insert(tetris::GameType::type_a, 0, 0, 100);
    expect(first && first->rank == 0, "first nonzero score enters at rank one");
    scores.name(*first, "ABCDEFZ");
    expect(scores.table(tetris::GameType::type_a, 0, 0)[0].name == "ABCDEF",
           "score names are six characters");
}

void test_replay_restores_flow_and_identity() {
    tetris::GameFlow flow;
    flow.start(resources());
    reach_title(flow);

    tetris::Replay replay;
    std::uint8_t random_state = 17;
    const tetris::ReplayIdentity identity{
        .rom_sha1 = "74591cc9501af93873f9a5d3eb12da12c0723bbc",
        .pacing = tetris::LineClearSpeed::fast,
        .starting_screen = flow.screen(),
        .rules = flow.game().rules(),
    };
    replay.begin_recording(flow, identity, random_state);
    const std::array recorded = {
        input({.start = true}), input(), input({.right = true}), input(),
    };
    for (const tetris::FlowInput& frame : recorded) {
        flow.tick(frame);
        ++random_state;
        replay.append(frame, random_state);
    }
    replay.stop(random_state);
    const tetris::Screen expected = flow.screen();

    expect(replay.rewind(flow, random_state), "recorded replay rewinds");
    expect(flow.screen() == tetris::Screen::title && random_state == 17,
           "replay restores its exact initial flow and random state");
    while (const std::optional<tetris::FlowInput> frame = replay.next(random_state))
        flow.tick(*frame);
    expect(flow.screen() == expected && random_state == 21,
           "replay reproduces flow and restores continuation randomness");
    expect(replay.identity() == identity,
           "replay records ROM, pacing, screen, and rules identity");
}

} // namespace

int main() {
    test_type_b_menu_flow();
    test_heart_mode_and_reset();
    test_versus_height_controls();
    test_high_scores();
    test_replay_restores_flow_and_identity();
    if (failures != 0)
        return 1;
    std::puts("modern Native Tetris flow tests passed");
    return 0;
}
