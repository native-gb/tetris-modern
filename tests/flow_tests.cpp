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
        random.pieces[index].kind = static_cast<tetris::PieceKind>(index % 7);
    for (std::size_t index = 0; index < random.garbage.size(); ++index)
        random.garbage[index] = {1, static_cast<std::uint8_t>(index)};
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
    static constexpr std::array<std::uint8_t, 40> garbage = {
        0x80, 0x2F, 0x81, 0x2F, 0x82, 0x2F, 0x83, 0x2F, 0x84, 0x2F,
        0x2F, 0x82, 0x2F, 0x83, 0x2F, 0x84, 0x2F, 0x85, 0x2F, 0x86,
        0x80, 0x2F, 0x81, 0x2F, 0x82, 0x2F, 0x83, 0x2F, 0x84, 0x2F,
        0x2F, 0x82, 0x2F, 0x83, 0x2F, 0x84, 0x2F, 0x85, 0x2F, 0x86,
    };
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
    expect(flow.versus().stack_height(0) == 2 && flow.versus().stack_height(1) == 6,
           "versus flow applies both selected garbage heights");
}

void test_alternating_attract_demos() {
    using namespace tetris;
    GameFlow flow;
    flow.start(resources());
    reach_title(flow);
    tick(flow, 125 * 19);
    expect(flow.screen() == Screen::demo && !flow.demo_is_type_b(),
           "first title timeout starts the Type A attract demo");
    press(flow, {.start = true});
    expect(flow.screen() == Screen::title, "Start exits an attract demo");
    tick(flow, 125 * 4);
    expect(flow.screen() == Screen::demo && flow.demo_is_type_b(),
           "second timeout alternates to Type B");
    expect(flow.game().rules().type_b_height == 2 &&
               flow.game().board().at({0, 14}) != Block::empty &&
               flow.game().board().at({1, 14}) == Block::empty,
           "Type B demo loads its fixed height and content-authored garbage");
}

void test_menu_edges_and_music() {
    using namespace tetris;
    GameFlow flow;
    flow.start(resources());
    reach_title(flow);
    press(flow, {.start = true});
    press(flow, {.right = true});
    press(flow, {.start = true});
    expect(flow.screen() == Screen::type_b_level,
           "Start on game type skips music and opens difficulty");
    press(flow, {.right = true});
    press(flow, {.up = true});
    expect(flow.selected_level() == 1, "level cursor does not wrap above its top row");
    press(flow, {.rotate_right = true});
    press(flow, {.down = true});
    expect(flow.selected_height() == 3, "height Down moves one grid row");
    press(flow, {.right = true});
    press(flow, {.down = true});
    expect(flow.selected_height() == 4, "height cursor does not move below its last row");
    press(flow, {.up = true});
    expect(flow.selected_height() == 1, "height Up moves one grid row");

    GameFlow music;
    music.start(resources());
    reach_title(music);
    press(music, {.start = true});
    press(music, {.rotate_right = true});
    press(music, {.right = true});
    expect(music.selected_music() == Music::b, "music cursor selects track B");
    press(music, {.down = true});
    expect(music.selected_music() == Music::off, "music grid reaches the Off choice");
    press(music, {.rotate_left = true});
    expect(music.screen() == Screen::game_type, "B returns from music to game type");
}

void test_versus_result_flow() {
    using namespace tetris;
    GameFlow flow;
    flow.start(resources());
    reach_title(flow);
    press(flow, {.start = true});
    press(flow, {.right = true});
    press(flow, {.right = true});
    press(flow, {.start = true});
    press(flow, {.rotate_right = true});
    flow.edit_versus().edit_player(0).set_state_for_test(PlayState::complete);
    flow.tick(input());
    expect(flow.screen() == Screen::versus_gameplay && flow.versus().wins(0) == 1,
           "round completion begins the frozen result delay");
    tick(flow, 39);
    expect(flow.screen() == Screen::versus_gameplay,
           "versus result remains hidden for thirty-nine frames");
    tick(flow, 1);
    expect(flow.screen() == Screen::versus_round_result,
           "fortieth frame reveals the versus result");
    tick(flow, 50);
    expect(flow.versus_result_step() == 2 && flow.versus_result_prompt_visible(),
           "round result and prompt use twenty-five-frame steps");
    press(flow, {.start = true});
    expect(flow.screen() == Screen::versus_height,
           "Start advances a nonfinal result to height selection");

    for (int win = 1; win < wins_for_match; ++win) {
        press(flow, {.rotate_right = true});
        flow.edit_versus().edit_player(1).set_state_for_test(PlayState::game_over);
        flow.tick(input());
        tick(flow, 40);
        if (win + 1 < wins_for_match)
            press(flow, {.start = true});
    }
    expect(flow.screen() == Screen::versus_match_result &&
               flow.versus().wins(0) == winner_display_wins,
           "fourth win reaches the unskippable final match result");
    press(flow, {.start = true});
    expect(flow.screen() == Screen::versus_match_result,
           "Start cannot skip final match presentation");
    int guard = 0;
    while (flow.screen() != Screen::versus_height && guard < 800) {
        flow.tick(input());
        ++guard;
    }
    expect(guard <= 725 && flow.screen() == Screen::versus_height,
           "final result returns automatically after twenty-nine steps");
}

void test_rocket_thresholds_and_game_over_wipes() {
    using namespace tetris;
    const auto ending = [](std::uint32_t score) {
        GameFlow flow;
        flow.start(resources());
        flow.start_session({.type = GameType::type_a}, startup());
        flow.edit_game().set_score_for_test(score);
        flow.edit_game().set_state_for_test(PlayState::game_over);
        flow.tick(input());
        tick(flow, 70);
        return flow;
    };
    expect(ending(99'999).ending_stage() == EndingStage::game_over_wait,
           "score below 100000 has no rocket");
    expect(ending(100'000).rocket() == Rocket::small &&
               ending(149'999).rocket() == Rocket::small,
           "100000 through 149999 selects the small rocket");
    expect(ending(150'000).rocket() == Rocket::medium &&
               ending(199'999).rocket() == Rocket::medium,
           "150000 through 199999 selects the medium rocket");
    expect(ending(200'000).rocket() == Rocket::large,
           "200000 selects the large rocket");

    GameFlow flow;
    flow.start(resources());
    flow.start_session({.type = GameType::type_a}, startup());
    flow.edit_game().set_score_for_test(200'000);
    flow.edit_game().set_state_for_test(PlayState::game_over);
    flow.tick(input());
    expect(flow.ending_stage() == EndingStage::game_over_curtain &&
               flow.game_over_curtain_rows() == 0,
           "game over begins before the first curtain row");
    tick(flow, 18);
    expect(flow.game_over_curtain_rows() == 18,
           "curtain covers all eighteen rows bottom-up");
    tick(flow, 52);
    expect(flow.ending_stage() == EndingStage::rocket_bonus_delay &&
               flow.game_over_panel_rows() == 0,
           "rocket score waits on a newly hidden panel");
    tick(flow, 18);
    expect(flow.game_over_panel_rows() == 18,
           "second wipe reveals the game-over panel");
    tick(flow, 126);
    expect(flow.screen() == Screen::rocket &&
               flow.ending_stage() == EndingStage::rocket_initialize,
           "bonus delay enters the rocket initializer");
    tick(flow, 520);
    expect(flow.ending_stage() == EndingStage::rocket_rising && flow.exhaust_x() == 0x54,
           "rocket reaches its rising exhaust sequence");
    int guard = 0;
    while (flow.screen() != Screen::name_entry && guard < 2'000) {
        flow.tick(input());
        ++guard;
    }
    expect(guard < 2'000 && flow.screen() == Screen::name_entry,
           "complete rocket flight returns to high-score entry");
}

void test_type_b_endings_and_scoreboard() {
    using namespace tetris;
    constexpr std::array dancer_lengths = {241, 241, 241, 241, 385};
    for (int height = 0; height < 5; ++height) {
        GameFlow flow;
        flow.start(resources());
        flow.start_session({.type = GameType::type_b, .starting_level = 9,
                                  .type_b_height = height}, startup());
        flow.edit_game().set_state_for_test(PlayState::complete);
        flow.tick(input());
        tick(flow, 100);
        expect(flow.screen() == Screen::dancers,
               "each level-nine Type B height reaches dancers");
        tick(flow, dancer_lengths[static_cast<std::size_t>(height)]);
        expect(flow.screen() == Screen::scoreboard,
               "Type B heights zero through four return to scoreboard");
    }

    GameFlow buran;
    buran.start(resources());
    buran.start_session({.type = GameType::type_b, .starting_level = 9,
                               .type_b_height = 5}, startup());
    buran.edit_game().set_state_for_test(PlayState::complete);
    buran.tick(input());
    tick(buran, 100);
    expect(buran.screen() == Screen::dancers && buran.dancer_frame(0) == 0,
           "height five begins its long dancer scene");
    tick(buran, 64);
    expect(buran.dancer_frame(6) == 1 && buran.dancer_vertical_offset(6) == -10,
           "Cossack dancer uses its independent jump frame");
    tick(buran, 705);
    expect(buran.screen() == Screen::buran, "height five continues to Buran");
    tick(buran, 763);
    expect(buran.ending_stage() == EndingStage::buran_rising && buran.exhaust_x() == 0x4C,
           "Buran reaches its rising exhaust sequence");
    expect(buran.exhaust_animation_frame() == 0,
           "Buran exhaust begins on its first frame");
    tick(buran, 1);
    expect(buran.exhaust_animation_frame() == 1,
           "Buran exhaust advances on its original phase");
    tick(buran, 6);
    expect(buran.exhaust_animation_frame() == 0,
           "Buran exhaust alternates every six frames");
    tick(buran, 1'712);
    expect(buran.screen() == Screen::scoreboard &&
               buran.ending_stage() == EndingStage::scoreboard_delay,
           "Buran, congratulations, and teardown return to scoreboard");

    GameFlow scoreboard;
    scoreboard.start(resources());
    scoreboard.start_session({.type = GameType::type_b, .starting_level = 2}, startup());
    scoreboard.edit_game().set_results_for_test({1, 1, 1, 1}, 3);
    scoreboard.edit_game().set_state_for_test(PlayState::complete);
    scoreboard.tick(input());
    tick(scoreboard, 100 + 128);
    int guard = 0;
    while (scoreboard.ending_stage() != EndingStage::scoreboard_wait && guard < 1'000) {
        scoreboard.tick(input());
        ++guard;
    }
    const Scoreboard& values = scoreboard.scoreboard();
    expect(guard < 1'000 && values.lines.singles == 1 && values.lines.doubles == 1 &&
               values.lines.triples == 1 && values.lines.tetrises == 1,
           "scoreboard visibly consumes every line category");
    expect(values.soft_drop == 3 && values.score == 4'923,
           "scoreboard uses starting-level values and one point per soft drop");
}

void test_high_score_name_entry_and_keys() {
    using namespace tetris;
    GameFlow flow;
    flow.start(resources());
    flow.start_session({.type = GameType::type_a, .heart_mode = true}, startup());
    flow.edit_game().set_score_for_test(12'345);
    flow.edit_game().set_state_for_test(PlayState::game_over);
    flow.tick(input());
    tick(flow, 70);
    press(flow, {.rotate_right = true});
    expect(flow.screen() == Screen::name_entry && flow.pending_name() == "A" &&
               flow.name_cursor() == 0 && flow.name_entry_rank() == 0,
           "qualifying score opens six-character entry at A");
    const bool visible = flow.name_character_visible();
    tick(flow, 7);
    expect(flow.name_character_visible() != visible,
           "active name character blinks every seven frames");
    press(flow, {.up = true});
    expect(flow.pending_name() == "B", "Up selects the next name character");
    for (int frame = 0; frame < 10; ++frame)
        flow.tick(input({.up = true}));
    flow.tick(input());
    expect(flow.pending_name() == "D", "held character input repeats after nine frames");
    press(flow, {.rotate_right = true});
    expect(flow.name_cursor() == 1 && flow.pending_name() == "DA",
           "A advances and initializes another character");
    press(flow, {.start = true});
    const ScoreEntry& entry = flow.high_scores().table(GameType::type_a, 0, 0)[0];
    expect(flow.screen() == Screen::type_a_level && entry.score == 12'345 && entry.name == "DA",
           "submitted name remains in its level table");

    HighScores scores;
    (void)scores.insert(GameType::type_a, 2, 0, 100);
    (void)scores.insert(GameType::type_a, 2, 0, 300);
    (void)scores.insert(GameType::type_a, 2, 0, 200);
    const HighScores::Table& table = scores.table(GameType::type_a, 2, 0);
    expect(table[0].score == 300 && table[1].score == 200 && table[2].score == 100,
           "scores remain descending");
    (void)scores.insert(GameType::type_b, 2, 5, 50);
    expect(scores.table(GameType::type_b, 2, 4)[0].score == 0,
           "Type B level-height keys remain independent");
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
    test_alternating_attract_demos();
    test_menu_edges_and_music();
    test_versus_result_flow();
    test_rocket_thresholds_and_game_over_wipes();
    test_type_b_endings_and_scoreboard();
    test_high_score_name_entry_and_keys();
    test_high_scores();
    test_replay_restores_flow_and_identity();
    if (failures != 0)
        return 1;
    std::puts("modern Native Tetris flow tests passed");
    return 0;
}
