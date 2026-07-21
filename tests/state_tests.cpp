#include "game/replay.hpp"
#include "game/state.hpp"
#include "gameplay_fixture.hpp"

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

tetris::GameInput input(tetris::Buttons one = {}, tetris::Buttons two = {}) {
    return {.player_one = one,
            .player_two = two,
            .random = {11, 12, 13},
            .startup = startup(),
            .versus = versus_random()};
}

tetris::GameResources resources() {
    static constexpr std::array<tetris::DemoRun, 2> type_a = {{
        {{.right = true}, 250},
        {{.confirm = true}, 250},
    }};
    static constexpr std::array<tetris::DemoRun, 2> type_b = {{
        {{.left = true}, 250},
        {{.down = true}, 250},
    }};
    static constexpr std::array<tetris::DemoPiece, 48> pieces{};
    static constexpr std::array<std::uint8_t, 40> garbage = {
        0x80, 0x2F, 0x81, 0x2F, 0x82, 0x2F, 0x83, 0x2F, 0x84, 0x2F, 0x2F, 0x82, 0x2F, 0x83,
        0x2F, 0x84, 0x2F, 0x85, 0x2F, 0x86, 0x80, 0x2F, 0x81, 0x2F, 0x82, 0x2F, 0x83, 0x2F,
        0x84, 0x2F, 0x2F, 0x82, 0x2F, 0x83, 0x2F, 0x84, 0x2F, 0x85, 0x2F, 0x86,
    };
    return {tetris::test::gameplay_data(), type_a, type_b, pieces, garbage};
}

void step(tetris::GameState& state, int frames) {
    for (int frame = 0; frame < frames; ++frame)
        step_game(state, input());
}

void press(tetris::GameState& state, tetris::Buttons buttons, tetris::Buttons player_two = {}) {
    step_game(state, input(buttons, player_two));
    step_game(state, input());
}

void reach_title(tetris::GameState& state) {
    expect(state.screen == tetris::Screen::title, "game boots directly to the title");
}

void test_optional_copyright_screen() {
    tetris::GameState state;
    start_game(state, resources());
    state.screen = tetris::Screen::copyright_fixed;
    state.timer = tetris::copyright_skippable_steps;
    press(state, {.confirm = true});
    expect(state.screen == tetris::Screen::title,
           "debug copyright page accepts input on its first step");
}

void test_type_b_menu_flow() {
    tetris::GameState state;
    start_game(state, resources());
    reach_title(state);
    press(state, {.start = true});
    press(state, {.right = true});
    expect(state.selected_type == tetris::GameType::type_b, "right selects Type B");
    press(state, {.confirm = true});
    expect(state.screen == tetris::Screen::music, "A opens music selection");
    press(state, {.confirm = true});
    expect(state.screen == tetris::Screen::type_b_level, "music confirms to Type B level");
    press(state, {.right = true});
    press(state, {.confirm = true});
    expect(state.screen == tetris::Screen::type_b_height, "Type B level confirms to height");
    press(state, {.right = true});
    press(state, {.confirm = true});
    expect(state.screen == tetris::Screen::gameplay, "height starts gameplay");
    expect(state.single_player.rules.starting_level == 1 &&
               state.single_player.rules.type_b_height == 1,
           "game receives menu choices");
}

void test_heart_mode_and_reset() {
    tetris::GameState state;
    start_game(state, resources());
    reach_title(state);
    step_game(state, input({.down = true, .start = true}));
    step_game(state, input());
    expect(state.heart_mode, "Down plus Start enables heart mode");
    step_game(state,
              input({.back = true, .confirm = true, .start = true, .select = true}));
    expect(state.screen == tetris::Screen::title && state.timer == tetris::title_interval_steps,
           "four-button chord resets directly to the title");
}

void test_versus_height_controls() {
    tetris::GameState state;
    start_game(state, resources());
    reach_title(state);
    press(state, {.start = true});
    press(state, {.right = true});
    press(state, {.right = true});
    press(state, {.start = true});
    expect(state.screen == tetris::Screen::versus_height, "Start enters versus heights");
    press(state, {.right = true}, {.down = true});
    expect(state.versus_heights[0] == 1 && state.versus_heights[1] == 3,
           "both players own independent height cursors");
    press(state, {.confirm = true});
    expect(state.screen == tetris::Screen::versus_gameplay,
           "player one starts the local versus round");
    expect(state.versus.stack_height(0) == 2 && state.versus.stack_height(1) == 6,
           "versus state applies both selected garbage heights");
}

void test_alternating_attract_demos() {
    using namespace tetris;
    GameState state;
    start_game(state, resources());
    reach_title(state);
    step(state, 125 * 19);
    expect(state.screen == Screen::demo && !state.demo_type_b,
           "first title timeout starts the Type A attract demo");
    press(state, {.start = true});
    expect(state.screen == Screen::title, "Start exits an attract demo");
    step(state, 125 * 4);
    expect(state.screen == Screen::demo && state.demo_type_b,
           "second timeout alternates to Type B");
    expect(state.single_player.rules.type_b_height == 2 &&
               state.single_player.board.at({0, 14}) != Block::empty &&
               state.single_player.board.at({1, 14}) == Block::empty,
           "Type B demo loads its fixed height and content-authored garbage");
}

void test_menu_edges_and_music() {
    using namespace tetris;
    GameState state;
    start_game(state, resources());
    reach_title(state);
    press(state, {.start = true});
    press(state, {.right = true});
    press(state, {.start = true});
    expect(state.screen == Screen::type_b_level,
           "Start on game type skips music and opens difficulty");
    press(state, {.right = true});
    press(state, {.up = true});
    expect(selected_level(state) == 1, "level cursor does not wrap above its top row");
    press(state, {.confirm = true});
    press(state, {.down = true});
    expect(state.type_b_height == 3, "height Down moves one grid row");
    press(state, {.right = true});
    press(state, {.down = true});
    expect(state.type_b_height == 4, "height cursor does not move below its last row");
    press(state, {.up = true});
    expect(state.type_b_height == 1, "height Up moves one grid row");

    GameState music;
    start_game(music, resources());
    reach_title(music);
    press(music, {.start = true});
    press(music, {.confirm = true});
    press(music, {.right = true});
    expect(music.selected_music == Music::b, "music cursor selects track B");
    press(music, {.down = true});
    expect(music.selected_music == Music::off, "music grid reaches the Off choice");
    press(music, {.back = true});
    expect(music.screen == Screen::game_type, "B returns from music to game type");
}

void test_versus_result_flow() {
    using namespace tetris;
    GameState state;
    start_game(state, resources());
    reach_title(state);
    press(state, {.start = true});
    press(state, {.right = true});
    press(state, {.right = true});
    press(state, {.start = true});
    press(state, {.confirm = true});
    state.versus.players[0].game.debug_set_state(PlayState::complete);
    step_game(state, input());
    expect(state.screen == Screen::versus_gameplay && state.versus.wins[0] == 1,
           "round completion begins the frozen result delay");
    step(state, 39);
    expect(state.screen == Screen::versus_gameplay,
           "versus result remains hidden for thirty-nine frames");
    step(state, 1);
    expect(state.screen == Screen::versus_round_result, "fortieth frame reveals the versus result");
    step(state, 50);
    expect(state.result_step == 2 && versus_result_prompt_visible(state),
           "round result and prompt use twenty-five-frame steps");
    press(state, {.start = true});
    expect(state.screen == Screen::versus_height,
           "Start advances a nonfinal result to height selection");

    for (int win = 1; win < wins_for_match; ++win) {
        press(state, {.confirm = true});
        state.versus.players[1].game.debug_set_state(PlayState::game_over);
        step_game(state, input());
        step(state, 40);
        if (win + 1 < wins_for_match)
            press(state, {.start = true});
    }
    expect(state.screen == Screen::versus_match_result &&
               state.versus.wins[0] == winner_display_wins,
           "fourth win reaches the unskippable final match result");
    press(state, {.start = true});
    expect(state.screen == Screen::versus_match_result, "Start cannot skip final match rendering");
    int guard = 0;
    while (state.screen != Screen::versus_height && guard < 800) {
        step_game(state, input());
        ++guard;
    }
    expect(guard <= 725 && state.screen == Screen::versus_height,
           "final result returns automatically after twenty-nine steps");
}

void test_rocket_thresholds_and_game_over_wipes() {
    using namespace tetris;
    const auto ending = [](std::uint32_t score) {
        GameState state;
        start_game(state, resources());
        start_session(state, {.type = GameType::type_a}, startup());
        state.single_player.debug_set_score(score);
        state.single_player.debug_set_state(PlayState::game_over);
        step_game(state, input());
        step(state, 70);
        return state;
    };
    expect(ending(99'999).ending_stage == EndingStage::game_over_wait,
           "score below 100000 has no rocket");
    expect(ending(100'000).rocket == Rocket::small && ending(149'999).rocket == Rocket::small,
           "100000 through 149999 selects the small rocket");
    expect(ending(150'000).rocket == Rocket::medium && ending(199'999).rocket == Rocket::medium,
           "150000 through 199999 selects the medium rocket");
    expect(ending(200'000).rocket == Rocket::large, "200000 selects the large rocket");

    GameState state;
    start_game(state, resources());
    start_session(state, {.type = GameType::type_a}, startup());
    state.single_player.debug_set_score(200'000);
    state.single_player.debug_set_state(PlayState::game_over);
    step_game(state, input());
    expect(state.ending_stage == EndingStage::game_over_curtain &&
               game_over_curtain_rows(state) == 0,
           "game over begins before the first curtain row");
    step(state, 18);
    expect(game_over_curtain_rows(state) == 18, "curtain covers all eighteen rows bottom-up");
    step(state, 52);
    expect(state.ending_stage == EndingStage::rocket_bonus_delay &&
               game_over_panel_rows(state) == 0,
           "rocket score waits on a newly hidden panel");
    step(state, 18);
    expect(game_over_panel_rows(state) == 18, "second wipe reveals the game-over panel");
    step(state, 126);
    expect(state.screen == Screen::rocket && state.ending_stage == EndingStage::rocket_initialize,
           "bonus delay enters the rocket initializer");
    step(state, 520);
    expect(state.ending_stage == EndingStage::rocket_rising && exhaust_x(state) == 84,
           "rocket reaches its rising exhaust sequence");
    int guard = 0;
    while (state.screen != Screen::name_entry && guard < 2'000) {
        step_game(state, input());
        ++guard;
    }
    expect(guard < 2'000 && state.screen == Screen::name_entry,
           "complete rocket flight returns to high-score entry");
}

void test_type_b_endings_and_scoreboard() {
    using namespace tetris;
    constexpr std::array dancer_lengths = {241, 241, 241, 241, 385};
    for (int height = 0; height < 5; ++height) {
        GameState state;
        start_game(state, resources());
        start_session(state,
                      {.type = GameType::type_b, .starting_level = 9, .type_b_height = height},
                      startup());
        state.single_player.debug_set_state(PlayState::complete);
        step_game(state, input());
        step(state, 100);
        expect(state.screen == Screen::dancers, "each level-nine Type B height reaches dancers");
        step(state, dancer_lengths[static_cast<std::size_t>(height)]);
        expect(state.screen == Screen::scoreboard,
               "Type B heights zero through four return to scoreboard");
    }

    GameState buran;
    start_game(buran, resources());
    start_session(buran, {.type = GameType::type_b, .starting_level = 9, .type_b_height = 5},
                  startup());
    buran.single_player.debug_set_state(PlayState::complete);
    step_game(buran, input());
    step(buran, 100);
    expect(buran.screen == Screen::dancers && dancer_frame(buran, 0) == 0,
           "height five begins its long dancer scene");
    step(buran, 64);
    expect(dancer_frame(buran, 6) == 1 && dancer_vertical_offset(buran, 6) == -10,
           "Cossack dancer uses its independent jump frame");
    step(buran, 705);
    expect(buran.screen == Screen::buran, "height five continues to Buran");
    step(buran, 763);
    expect(buran.ending_stage == EndingStage::buran_rising && exhaust_x(buran) == 76,
           "Buran reaches its rising exhaust sequence");
    expect(exhaust_animation_frame(buran) == 0, "Buran exhaust begins on its first frame");
    step(buran, 1);
    expect(exhaust_animation_frame(buran) == 1, "Buran exhaust advances on its original phase");
    step(buran, 6);
    expect(exhaust_animation_frame(buran) == 0, "Buran exhaust alternates every six frames");
    step(buran, 1'712);
    expect(buran.screen == Screen::scoreboard &&
               buran.ending_stage == EndingStage::scoreboard_delay,
           "Buran, congratulations, and teardown return to scoreboard");

    GameState scoreboard;
    start_game(scoreboard, resources());
    start_session(scoreboard, {.type = GameType::type_b, .starting_level = 2}, startup());
    scoreboard.single_player.debug_set_results({1, 1, 1, 1}, 3);
    scoreboard.single_player.debug_set_state(PlayState::complete);
    step_game(scoreboard, input());
    step(scoreboard, 100 + 128);
    expect(scoreboard.ending_stage == EndingStage::scoreboard_tally &&
               scoreboard.scoreboard.category == 0 && scoreboard.timer == 0,
           "scoreboard begins category zero without an extra delay");
    step_game(scoreboard, input());
    expect(scoreboard.scoreboard.category == 0 && scoreboard.scoreboard.lines.singles == 1 &&
               scoreboard.timer == 5,
           "scoreboard consumes category zero on its first tally step");
    int guard = 0;
    while (scoreboard.ending_stage != EndingStage::scoreboard_wait && guard < 1'000) {
        step_game(scoreboard, input());
        ++guard;
    }
    const Scoreboard& values = scoreboard.scoreboard;
    expect(guard < 1'000 && values.lines.singles == 1 && values.lines.doubles == 1 &&
               values.lines.triples == 1 && values.lines.tetrises == 1,
           "scoreboard visibly consumes every line category");
    expect(values.soft_drop == 3 && values.score == 4'923,
           "scoreboard uses starting-level values and one point per soft drop");

    GameState modern_scoreboard;
    start_game(modern_scoreboard, resources());
    set_gameplay_options(modern_scoreboard, GameplayOptions{.modern_scoring = true});
    start_session(modern_scoreboard, {.type = GameType::type_b, .starting_level = 2}, startup());
    modern_scoreboard.single_player.debug_set_results({2, 1, 0, 3}, 17);
    modern_scoreboard.single_player.debug_set_score(54'321);
    modern_scoreboard.single_player.debug_set_state(PlayState::complete);
    step_game(modern_scoreboard, input());
    step(modern_scoreboard, 100);
    const LineCounts modern_lines = modern_scoreboard.scoreboard.lines;
    expect(modern_scoreboard.screen == Screen::scoreboard &&
               modern_lines.singles == 2 && modern_lines.doubles == 1 &&
               modern_lines.triples == 0 && modern_lines.tetrises == 3 &&
               modern_scoreboard.scoreboard.soft_drop == 17 &&
               modern_scoreboard.scoreboard.score == 54'321,
           "Modern Type B results retain the live score and completed breakdown");
}

void test_high_score_name_entry_and_keys() {
    using namespace tetris;
    GameState state;
    start_game(state, resources());
    start_session(state, {.type = GameType::type_a, .heart_mode = true}, startup());
    state.single_player.debug_set_score(12'345);
    state.single_player.debug_set_state(PlayState::game_over);
    step_game(state, input());
    step(state, 70);
    press(state, {.confirm = true});
    expect(state.screen == Screen::name_entry && state.pending_name == "A" &&
               state.name_cursor == 0 && name_entry_rank(state) == 0,
           "qualifying score opens six-character entry at A");
    const bool visible = state.name_character_visible;
    step(state, 7);
    expect(state.name_character_visible != visible,
           "active name character blinks every seven frames");
    press(state, {.up = true});
    expect(state.pending_name == "B", "Up selects the next name character");
    for (int frame = 0; frame < 10; ++frame)
        step_game(state, input({.up = true}));
    step_game(state, input());
    expect(state.pending_name == "D", "held character input repeats after nine frames");
    press(state, {.confirm = true});
    expect(state.name_cursor == 1 && state.pending_name == "DA",
           "A advances and initializes another character");
    press(state, {.start = true});
    const ScoreEntry& entry = state.high_scores.table(GameType::type_a, 0, 0)[0];
    expect(state.screen == Screen::type_a_level && entry.score == 12'345 && entry.name == "DA",
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

void test_replay_restores_state_and_identity() {
    tetris::GameState state;
    start_game(state, resources());
    reach_title(state);

    tetris::Replay replay;
    std::uint8_t random_state = 17;
    const tetris::ReplayIdentity identity{
        .rom_sha1 = "74591cc9501af93873f9a5d3eb12da12c0723bbc",
        .pacing = tetris::LineClearSpeed::fast,
        .starting_screen = state.screen,
        .rules = state.single_player.rules,
        .gameplay = tetris::GameplayOptions{.drop_mode = tetris::DropMode::hard,
                                            .ghost_piece = true},
    };
    replay.begin_recording(state, identity, random_state);
    const std::array recorded = {
        input({.start = true}),
        input(),
        input({.right = true}),
        input(),
    };
    for (const tetris::GameInput& frame : recorded) {
        step_game(state, frame);
        ++random_state;
        replay.append(frame, random_state);
    }
    replay.stop(random_state);
    const tetris::Screen expected = state.screen;

    expect(replay.rewind(state, random_state), "recorded replay rewinds");
    expect(state.screen == tetris::Screen::title && random_state == 17,
           "replay restores its exact initial state and random state");
    while (const std::optional<tetris::GameInput> frame = replay.next(random_state))
        step_game(state, *frame);
    expect(state.screen == expected && random_state == 21,
           "replay reproduces state and restores continuation randomness");
    expect(replay.identity == identity,
           "replay records ROM, pacing, screen, rules, and gameplay identity");
}

void test_modern_quick_restart() {
    using namespace tetris;
    GameState state;
    start_game(state, resources());
    set_gameplay_options(state, GameplayOptions{.quick_restart = true});
    start_session(state, {.type = GameType::type_a, .starting_level = 4}, startup());
    state.single_player.debug_set_score(12'345);
    state.single_player.board.set({0, 17}, Block::j);
    step_game(state, input({.restart = true}));
    expect(state.screen == Screen::gameplay && state.single_player.score == 0 &&
               state.single_player.level == 4 && state.single_player.board.at({0, 17}) == Block::empty,
           "quick restart rebuilds the current single-player session");

    state.single_player.debug_set_score(7'654);
    state.single_player.debug_set_state(PlayState::game_over);
    step_game(state, input());
    expect(state.screen == Screen::game_over, "top-out enters the game-over presentation");
    press(state, {.restart = true});
    expect(state.screen == Screen::gameplay && state.single_player.score == 0 &&
               state.single_player.level == 4,
           "quick restart also rebuilds a session from game over");

    state.single_player.debug_set_score(5'432);
    press(state, {.hold = true, .restart = true});
    expect(state.single_player.score == 5'432,
           "north-face Hold does not retry during active play");
    press(state, {.restart = true, .select = true});
    expect(state.single_player.score == 5'432,
           "Select does not retry during active play");

    press(state, {.start = true});
    expect(state.single_player.paused, "quick-restart fixture enters the pause menu");
    press(state, {.hold = true, .restart = true});
    expect(state.screen == Screen::gameplay && !state.single_player.paused &&
               state.single_player.score == 0,
           "north face retries from the pause menu");

    state.single_player.debug_set_score(4'321);
    state.single_player.debug_set_state(PlayState::game_over);
    step_game(state, input());
    press(state, {.restart = true, .select = true});
    expect(state.screen == Screen::gameplay && state.single_player.score == 0,
           "Select retries from the game-over menu");
}

void test_session_navigation() {
    using namespace tetris;
    GameState state;
    start_game(state, resources());
    start_session(state, {.type = GameType::type_b, .starting_level = 6, .type_b_height = 3},
                  startup());

    press(state, {.start = true});
    expect(state.screen == Screen::gameplay && state.single_player.paused,
           "Start pauses the active session");
    press(state, {.back = true});
    expect(state.screen == Screen::title, "B returns to the title from pause");

    start_session(state, {.type = GameType::type_b, .starting_level = 6, .type_b_height = 3},
                  startup());
    state.single_player.debug_set_state(PlayState::game_over);
    step_game(state, input());
    press(state, {.back = true});
    expect(state.screen == Screen::title, "B returns to the title from game over");
}

} // namespace

int main() {
    test_optional_copyright_screen();
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
    test_replay_restores_state_and_identity();
    test_modern_quick_restart();
    test_session_navigation();
    if (failures != 0)
        return 1;
    std::puts("modern Native Tetris state tests passed");
    return 0;
}
