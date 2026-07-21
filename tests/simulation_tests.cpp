#include "game/demo.hpp"
#include "game/rules.hpp"
#include "game/single_player.hpp"
#include "gameplay_fixture.hpp"

#include <array>
#include <cstdio>
#include <set>

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

void step(tetris::SinglePlayer& game, tetris::Buttons buttons = {},
          tetris::RandomSamples random = {11, 12, 13}) {
    game.step({buttons, random});
}

void force_tetris(tetris::SinglePlayer& game) {
    using namespace tetris;
    for (int row = 14; row < board_height; ++row) {
        for (int column = 0; column < board_width; ++column)
            game.board.set({column, row}, Block::j);
    }
    game.debug_place_piece({.kind = PieceKind::I, .origin = {6, 15}});
    step(game, {.down = true});
    step(game);
    int guard = 0;
    while (game.state != PlayState::falling && game.state != PlayState::complete && guard < 200) {
        step(game);
        ++guard;
    }
    expect(guard < 200, "Tetris resolution reaches a stable state");
}

void test_tables() {
    using namespace tetris;
    constexpr std::array expected = {
        52, 48, 44, 40, 36, 32, 27, 21, 16, 10, 9, 8, 7, 6, 5, 5, 4, 4, 3, 3, 2,
    };
    for (int level = 0; level <= 20; ++level)
        expect(frames_per_drop(test::gameplay_data(), level, false) ==
                   expected[static_cast<std::size_t>(level)],
               "all gravity entries match the original table");
    expect(frames_per_drop(test::gameplay_data(), 15, true) == 2,
           "heart gravity clamps to level twenty");
    expect(line_clear_score(1, 0) == 40 && line_clear_score(2, 0) == 100 &&
               line_clear_score(3, 0) == 300 && line_clear_score(4, 9) == 12'000,
           "all line score categories use the original multipliers");
}

void test_movement_rotation_and_das() {
    using namespace tetris;
    SinglePlayer game;
    game.start(test::gameplay_data(), {}, startup());
    const int original_x = game.piece.origin.x;
    step(game, {.right = true});
    expect(game.piece.origin.x == original_x + 1, "new direction moves immediately");
    for (int frame = 0; frame < 22; ++frame)
        step(game, {.right = true});
    expect(game.piece.origin.x == original_x + 1, "DAS waits twenty-three held frames");
    step(game, {.right = true});
    expect(game.piece.origin.x == original_x + 2, "DAS repeats after its initial delay");

    step(game);
    game.debug_place_piece({.kind = PieceKind::I, .origin = {0, 8}});
    for (int row = 0; row < board_height; ++row)
        game.board.set({1, row}, Block::o);
    step(game, {.back = true});
    expect(game.piece.rotation == Rotation::spawn,
           "colliding rotation rolls back without wall kicks");

    game.board.clear();
    game.debug_place_piece({.kind = PieceKind::I, .origin = {4, -1}});
    step(game);
    step(game, {.confirm = true});
    expect(game.piece.rotation == Rotation::left,
           "piece can rotate while its matrix is above the board");
}

void test_clear_pipeline_and_score() {
    using namespace tetris;
    SinglePlayer game;
    game.start(test::gameplay_data(), {}, startup());
    game.board.set({0, 0}, Block::t);
    for (int column = 0; column < 6; ++column)
        game.board.set({column, 17}, Block::j);
    game.debug_place_piece({.kind = PieceKind::I, .origin = {6, 15}});
    step(game, {.down = true});
    expect(game.state == PlayState::locked, "blocked soft drop locks the piece");
    step(game);
    expect(game.state == PlayState::resolving && game.lines == 1 && game.line_counts.singles == 1,
           "locked board detects and records a single on the next step");
    while (game.state != PlayState::wiping)
        step(game);
    expect(game.board.at({0, 0}) == Block::empty && game.visible_board().at({0, 0}) == Block::t,
           "wipe rendering trails the logically collapsed board");
    int guard = 0;
    while (game.state != PlayState::falling && guard < 200) {
        step(game);
        ++guard;
    }
    expect(guard < 200 && game.score == 40,
           "clear pipeline awards the correct score and spawns again");
}

void test_top_out_after_second_spawn_lock() {
    using namespace tetris;
    SinglePlayer game;
    game.start(test::gameplay_data(), {}, startup());
    const auto block_below_piece = [](SinglePlayer& session) {
        Cell bottom = occupied_cells(test::gameplay_data(), session.piece)[0];
        for (const Cell cell : occupied_cells(test::gameplay_data(), session.piece)) {
            if (cell.y > bottom.y)
                bottom = cell;
        }
        session.board.set({bottom.x, bottom.y + 1}, Block::o);
    };
    block_below_piece(game);
    step(game, {.down = true});
    expect(game.state == PlayState::locked && game.locks_at_spawn == 1,
           "first spawn-origin lock is tolerated");
    int guard = 0;
    while (game.state != PlayState::falling && guard < 20) {
        step(game);
        ++guard;
    }
    expect(guard < 20, "first spawn lock hands off to the next piece");
    block_below_piece(game);
    step(game, {.down = true});
    expect(game.state == PlayState::game_over && game.locks_at_spawn == 2,
           "second spawn-origin lock causes top-out");
}

void test_type_b_completion_and_garbage() {
    using namespace tetris;
    StartupRandom random = startup();
    for (std::size_t index = 0; index < random.garbage.size(); ++index) {
        random.garbage[index] = {1, static_cast<std::uint8_t>(index)};
    }
    SinglePlayer garbage;
    garbage.start(test::gameplay_data(), {.type = GameType::type_b, .type_b_height = 2}, random);
    expect(garbage.board.at({0, 13}) == Block::empty,
           "Type B leaves rows above selected height empty");
    for (int row = 14; row < 18; ++row) {
        expect(garbage.board.at({0, row}) != Block::empty, "odd samples create Type B blocks");
        expect(garbage.board.at({9, row}) == Block::empty,
               "every Type B garbage row receives a guaranteed hole");
    }
    random.garbage[0] = {2, 7};
    garbage.start(test::gameplay_data(), {.type = GameType::type_b, .type_b_height = 1}, random);
    expect(garbage.board.at({0, 16}) == Block::empty &&
               garbage.board.at({1, 16}) == Block::garbage_1,
           "occupancy parity and appearance bits remain independent");

    SinglePlayer completion;
    completion.start(test::gameplay_data(), {.type = GameType::type_b}, startup());
    for (int clear = 0; clear < 7; ++clear)
        force_tetris(completion);
    expect(completion.lines == 0 && completion.state == PlayState::complete,
           "twenty-five Type B lines complete the game");
    expect(completion.line_counts.tetrises == 7 && completion.score == 8'400,
           "oversized final clear is tallied in Type B score");
}

void test_level_saturation_pause_and_pacing() {
    using namespace tetris;
    SinglePlayer progression;
    progression.start(test::gameplay_data(), {}, startup());
    force_tetris(progression);
    force_tetris(progression);
    expect(progression.level == 0, "eight lines remain level zero");
    force_tetris(progression);
    expect(progression.lines == 12 && progression.level == 1 && progression.gravity_frames == 48,
           "crossing ten lines advances level and gravity");

    SinglePlayer saturated;
    saturated.start(test::gameplay_data(), {}, startup());
    saturated.debug_set_score(999'990);
    for (int column = 0; column < 6; ++column)
        saturated.board.set({column, 17}, Block::j);
    saturated.debug_place_piece({.kind = PieceKind::I, .origin = {6, 15}});
    step(saturated, {.down = true});
    int guard = 0;
    while (saturated.state != PlayState::falling && guard < 200) {
        step(saturated);
        ++guard;
    }
    expect(saturated.score == 999'999, "score saturates at six display digits");

    SinglePlayer paused;
    paused.start(test::gameplay_data(), {}, startup());
    const std::uint64_t before = paused.step_count;
    step(paused, {.start = true});
    expect(paused.paused && paused.step_count == before + 1,
           "pause preserves deterministic step time");
    step(paused);
    step(paused, {.select = true});
    expect(!paused.preview_visible, "Select toggles the preview");

    const auto clear_steps = [](LineClearSpeed speed) {
        SinglePlayer game;
        game.start(test::gameplay_data(), {}, startup());
        game.line_clear_speed = speed;
        for (int column = 0; column < 6; ++column)
            game.board.set({column, 17}, Block::j);
        game.debug_place_piece({.kind = PieceKind::I, .origin = {6, 15}});
        step(game, {.down = true});
        step(game);
        int count = 0;
        while (game.state != PlayState::falling && count < 200) {
            step(game);
            ++count;
        }
        return count;
    };
    const int original = clear_steps(LineClearSpeed::original);
    const int fast = clear_steps(LineClearSpeed::fast);
    const int instant = clear_steps(LineClearSpeed::instant);
    expect(original > fast && fast > instant,
           "line-clear profiles preserve strict original/fast/instant ordering");
}

void test_demo_and_oriented_fixed_pieces() {
    using namespace tetris;
    constexpr std::array runs = {
        DemoRun{{}, 2},
        DemoRun{{.left = true, .back = true}, 1},
        DemoRun{{}, 0},
    };
    DemoPlayback playback;
    playback.start(runs);
    expect(playback.step() == Buttons{} && playback.step() == Buttons{} &&
               playback.step() == Buttons{},
           "demo run countdown retains its first held state");
    const Buttons changed = playback.step();
    expect(changed.left && changed.back, "demo changes to its next semantic command");

    constexpr std::array fixed = {
        PieceSpec{PieceKind::T},
        PieceSpec{PieceKind::I, Rotation::reverse},
        PieceSpec{PieceKind::O},
    };
    SinglePlayer game;
    game.start(test::gameplay_data(), {.starting_level = 9}, startup(), fixed);
    expect(game.piece.kind == PieceKind::I && game.piece.rotation == Rotation::reverse &&
               game.preview == PieceSpec{PieceKind::O},
           "startup handoffs preserve oriented demo pieces and preview");
}

void test_modern_randomizer_hold_and_drop() {
    using namespace tetris;
    const GameplayOptions modern{
        .drop_mode = DropMode::hard,
        .randomizer = RandomizerMode::seven_bag,
        .next_previews = 5,
        .ghost_piece = true,
        .hold_piece = true,
        .lock_delay = true,
        .modern_scoring = true,
    };
    SinglePlayer game;
    game.start(test::gameplay_data(), {}, startup(), {}, {}, modern);

    std::set<PieceKind> first_bag{game.piece.kind};
    for (int index = 0; index < 6; ++index)
        first_bag.insert(game.next_piece(index).kind);
    expect(first_bag.size() == 7, "seven-bag emits every piece exactly once per bag");
    expect(game.landing_piece().origin.y > game.piece.origin.y,
           "ghost landing is derived from board collision");

    const PieceKind original = game.piece.kind;
    const PieceKind next = game.next_piece(0).kind;
    step(game, {.hold = true});
    expect(game.held_piece && game.held_piece->kind == original && game.piece.kind == next,
           "first hold stores the active piece and advances the queue");
    const PieceKind after_hold = game.piece.kind;
    step(game);
    step(game, {.hold = true});
    expect(game.piece.kind == after_hold && game.statistics.holds == 1,
           "hold can only be used once before locking");

    const int landing_y = game.landing_piece().origin.y;
    const std::uint32_t score_before = game.score;
    const int start_y = game.piece.origin.y;
    step(game);
    step(game, {.hard_drop = true});
    expect(game.state == PlayState::locked && game.piece.origin.y == landing_y,
           "hard drop lands and locks immediately");
    expect(game.score == score_before + static_cast<std::uint32_t>((landing_y - start_y) * 2),
           "hard drop awards two points per descended cell");

    GameplayOptions sonic = modern;
    sonic.drop_mode = DropMode::sonic;
    sonic.lock_delay = false;
    sonic.modern_scoring = false;
    SinglePlayer sonic_game;
    sonic_game.start(test::gameplay_data(), {}, startup(), {}, {}, sonic);
    const int sonic_y = sonic_game.landing_piece().origin.y;
    step(sonic_game, {.hard_drop = true});
    expect(sonic_game.state == PlayState::falling && sonic_game.piece.origin.y == sonic_y,
           "sonic drop reaches the floor without locking the piece");
}

void test_modern_handling_and_scoring() {
    using namespace tetris;
    GameplayOptions options{
        .rotation = RotationSystem::srs,
        .horizontal_repeat_delay = 8,
        .horizontal_repeat_interval = 3,
        .separate_rotation_inputs = true,
        .buffered_inputs = true,
        .lock_delay = true,
        .modern_scoring = true,
    };
    SinglePlayer game;
    game.start(test::gameplay_data(), {}, startup(), {}, {}, options);

    const int start_x = game.piece.origin.x;
    step(game, {.right = true});
    for (int frame = 0; frame < 7; ++frame)
        step(game, {.right = true});
    expect(game.piece.origin.x == start_x + 1, "DX DAS waits eight held steps");
    step(game, {.right = true});
    expect(game.piece.origin.x == start_x + 2, "DX DAS repeats on its eighth held step");

    game.board.clear();
    game.debug_place_piece({.kind = PieceKind::T, .rotation = Rotation::left, .origin = {-1, 5}});
    step(game);
    step(game, {.rotate_counterclockwise = true});
    expect(game.piece.rotation == Rotation::spawn && game.piece.origin.x == 0,
           "SRS kicks a rotation away from the left wall");

    game.board.clear();
    game.debug_place_piece(game.landing_piece());
    game.fall_timer = 0;
    step(game);
    expect(game.state == PlayState::falling && game.grounded && game.lock_timer == 30,
           "ground contact starts a thirty-step lock delay");
    for (int frame = 0; frame < 29; ++frame)
        step(game);
    expect(game.state == PlayState::falling, "piece remains controllable during lock delay");
    step(game);
    expect(game.state == PlayState::locked, "piece locks when the lock delay expires");

    SinglePlayer scoring;
    scoring.start(test::gameplay_data(), {}, startup(), {}, {}, options);
    scoring.score_clear(4);
    expect(scoring.pending_score_award == 800 && scoring.statistics.back_to_back,
           "first modern Tetris starts back-to-back scoring");
    scoring.score_clear(4);
    expect(scoring.pending_score_award == 1'250 && scoring.statistics.combo == 1,
           "back-to-back Tetris and combo bonuses compose");

    scoring.board.clear();
    scoring.debug_place_piece({.kind = PieceKind::T, .origin = {3, 5}});
    scoring.last_action_rotation = true;
    scoring.board.set({3, 6}, Block::j);
    scoring.board.set({5, 6}, Block::j);
    scoring.board.set({3, 8}, Block::j);
    scoring.score = 0;
    scoring.score_clear(0);
    expect(scoring.score == 400 && scoring.statistics.t_spins == 1,
           "three-corner rotated T scores as a T-spin");
}

void test_pushout_and_frame_exact_repeat() {
    using namespace tetris;
    GameplayOptions options{
        .drop_mode = DropMode::hard,
        .rotation = RotationSystem::side_pushout,
        .horizontal_repeat_delay = 2,
        .horizontal_repeat_interval = 2,
        .separate_rotation_inputs = true,
    };
    SinglePlayer game;
    game.start(test::gameplay_data(), {}, startup(), {}, {}, options);

    game.debug_place_piece({.kind = PieceKind::T, .rotation = Rotation::left, .origin = {-1, 5}});
    step(game, {.rotate_counterclockwise = true});
    expect(game.piece.rotation == Rotation::spawn && game.piece.origin.x == 0,
           "side pushout moves a legal rotation away from the left wall");

    step(game);
    const Rotation before_confirm = game.piece.rotation;
    step(game, {.confirm = true});
    expect(game.piece.rotation == before_confirm,
           "separate modern rotations do not consume the Confirm action");

    step(game);
    game.board.clear();
    game.debug_place_piece({.kind = PieceKind::O, .origin = {3, 5}});
    step(game, {.right = true});
    const int after_press = game.piece.origin.x;
    step(game, {.right = true});
    expect(game.piece.origin.x == after_press, "DAS waits for its configured frame count");
    step(game, {.right = true});
    expect(game.piece.origin.x == after_press + 1,
           "ARR begins on the configured post-press frame");

    options.instant_autorepeat = true;
    SinglePlayer instant;
    instant.start(test::gameplay_data(), {}, startup(), {}, {}, options);
    instant.debug_place_piece({.kind = PieceKind::O, .origin = {3, 5}});
    step(instant, {.right = true});
    step(instant, {.right = true});
    step(instant, {.right = true});
    const int wall = instant.piece.origin.x;
    expect(wall > 5, "instant autorepeat shifts to the wall when DAS expires");
    step(instant, {.right = true});
    expect(instant.piece.origin.x == wall, "instant autorepeat stops cleanly at collision");
}

void test_configured_entry_delay() {
    using namespace tetris;
    GameplayOptions options{.entry_delay = 6};
    SinglePlayer game;
    game.start(test::gameplay_data(), {}, startup(), {}, {}, options);
    game.debug_place_piece(game.landing_piece());
    game.fall_timer = 0;
    step(game);
    expect(game.state == PlayState::locked, "grounded piece locks before entry delay");

    for (int frame = 0; frame < 5; ++frame)
        step(game);
    expect(game.state == PlayState::resolving,
           "next piece remains hidden through the first five ARE frames");
    step(game);
    expect(game.state == PlayState::falling,
           "next piece appears on the configured sixth ARE frame");
}

void test_modern_type_b() {
    using namespace tetris;
    const GameplayOptions modern{
        .drop_mode = DropMode::hard,
        .randomizer = RandomizerMode::seven_bag,
        .rotation = RotationSystem::srs,
        .next_previews = 5,
        .ghost_piece = true,
        .hold_piece = true,
        .lock_delay = true,
        .modern_scoring = true,
    };

    SinglePlayer game;
    game.start(test::gameplay_data(),
               {.type = GameType::type_b, .starting_level = 3, .type_b_height = 2}, startup(), {},
               {}, modern);
    expect(game.lines == 25 && game.upcoming.size() >= 6,
           "Modern Type B keeps its objective and exposes the modern queue");
    const PieceKind held = game.piece.kind;
    step(game, {.hold = true});
    expect(game.held_piece && game.held_piece->kind == held,
           "Modern Type B supports visible hold state");
    step(game);
    const int start_y = game.piece.origin.y;
    const int landing_y = game.landing_piece().origin.y;
    step(game, {.hard_drop = true});
    expect(game.score == static_cast<std::uint32_t>((landing_y - start_y) * 2),
           "Modern Type B awards live hard-drop points");

    SinglePlayer scoring;
    scoring.start(test::gameplay_data(), {.type = GameType::type_b}, startup(), {}, {}, modern);
    scoring.score_clear(4);
    scoring.state = PlayState::wiping;
    scoring.wipe_step = 5;
    scoring.score_clear_when_wiping = true;
    step(scoring);
    expect(scoring.score == 800 && scoring.statistics.back_to_back,
           "Modern Type B applies combo-capable line scoring during play");

    scoring.score = 12'345;
    scoring.lines = 0;
    scoring.state = PlayState::wiping;
    scoring.wipe_step = 19;
    scoring.score_clear_when_wiping = false;
    step(scoring);
    expect(scoring.state == PlayState::complete && scoring.score == 12'345,
           "Modern Type B completion preserves its live score");
}

void test_modern_challenge_completion() {
    using namespace tetris;
    GameplayOptions sprint{.challenge = ChallengeMode::sprint};
    SinglePlayer game;
    game.start(test::gameplay_data(), {}, startup(), {}, {}, sprint);
    game.lines = 39;
    for (int column = 0; column < 6; ++column)
        game.board.set({column, 17}, Block::j);
    game.debug_place_piece({.kind = PieceKind::I, .origin = {6, 15}});
    step(game, {.down = true});
    int guard = 0;
    while (game.state != PlayState::complete && guard < 200) {
        step(game);
        ++guard;
    }
    expect(guard < 200 && game.lines == 40,
           "Sprint completes after the fortieth cleared line");

    GameplayOptions ultra{.challenge = ChallengeMode::ultra};
    SinglePlayer timed;
    timed.start(test::gameplay_data(), {}, startup(), {}, {}, ultra);
    timed.time_remaining_steps = 1;
    step(timed);
    expect(timed.state == PlayState::complete, "Ultra completes when its timer reaches zero");
}

} // namespace

int main() {
    test_tables();
    test_movement_rotation_and_das();
    test_clear_pipeline_and_score();
    test_top_out_after_second_spawn_lock();
    test_type_b_completion_and_garbage();
    test_level_saturation_pause_and_pacing();
    test_demo_and_oriented_fixed_pieces();
    test_modern_randomizer_hold_and_drop();
    test_modern_handling_and_scoring();
    test_pushout_and_frame_exact_repeat();
    test_configured_entry_delay();
    test_modern_type_b();
    test_modern_challenge_completion();
    if (failures != 0)
        return 1;
    std::puts("modern Native Tetris simulation tests passed");
    return 0;
}
