#include "game/demo.hpp"
#include "game/rules.hpp"
#include "game/single_player.hpp"

#include <array>
#include <cstdio>

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

void tick(tetris::SinglePlayer& game, tetris::Buttons buttons = {},
          tetris::RandomSamples random = {11, 12, 13}) {
    game.tick({buttons, random});
}

void force_tetris(tetris::SinglePlayer& game) {
    using namespace tetris;
    for (int row = 14; row < board_height; ++row) {
        for (int column = 0; column < board_width; ++column)
            game.edit_board().set({column, row}, Block::j);
    }
    game.debug_place_piece({.kind = PieceKind::I, .origin = {6, 15}});
    tick(game, {.down = true});
    tick(game);
    int guard = 0;
    while (game.state() != PlayState::falling && game.state() != PlayState::complete &&
           guard < 200) {
        tick(game);
        ++guard;
    }
    expect(guard < 200, "Tetris resolution reaches a stable state");
}

void test_tables() {
    using namespace tetris;
    constexpr std::array expected = {
        52, 48, 44, 40, 36, 32, 27, 21, 16, 10, 9,
        8, 7, 6, 5, 5, 4, 4, 3, 3, 2,
    };
    for (int level = 0; level <= 20; ++level)
        expect(frames_per_drop(level, false) == expected[static_cast<std::size_t>(level)],
               "all gravity entries match the original table");
    expect(frames_per_drop(15, true) == 2, "heart gravity clamps to level twenty");
    expect(line_clear_score(1, 0) == 40 && line_clear_score(2, 0) == 100 &&
               line_clear_score(3, 0) == 300 && line_clear_score(4, 9) == 12'000,
           "all line score categories use the original multipliers");
}

void test_movement_rotation_and_das() {
    using namespace tetris;
    SinglePlayer game;
    game.start({}, startup());
    const int original_x = game.piece().origin.x;
    tick(game, {.right = true});
    expect(game.piece().origin.x == original_x + 1, "new direction moves immediately");
    for (int frame = 0; frame < 22; ++frame)
        tick(game, {.right = true});
    expect(game.piece().origin.x == original_x + 1, "DAS waits twenty-three held frames");
    tick(game, {.right = true});
    expect(game.piece().origin.x == original_x + 2, "DAS repeats after its initial delay");

    tick(game);
    game.debug_place_piece({.kind = PieceKind::I, .origin = {0, 8}});
    for (int row = 0; row < board_height; ++row)
        game.edit_board().set({1, row}, Block::o);
    tick(game, {.rotate_left = true});
    expect(game.piece().rotation == Rotation::spawn,
           "colliding rotation rolls back without wall kicks");

    game.edit_board().clear();
    game.debug_place_piece({.kind = PieceKind::I, .origin = {4, -1}});
    tick(game);
    tick(game, {.rotate_right = true});
    expect(game.piece().rotation == Rotation::left,
           "piece can rotate while its matrix is above the board");
}

void test_clear_pipeline_and_score() {
    using namespace tetris;
    SinglePlayer game;
    game.start({}, startup());
    game.edit_board().set({0, 0}, Block::t);
    for (int column = 0; column < 6; ++column)
        game.edit_board().set({column, 17}, Block::j);
    game.debug_place_piece({.kind = PieceKind::I, .origin = {6, 15}});
    tick(game, {.down = true});
    expect(game.state() == PlayState::locked, "blocked soft drop locks the piece");
    tick(game);
    expect(game.state() == PlayState::resolving && game.lines() == 1 &&
               game.line_counts().singles == 1,
           "locked board detects and records a single on the next tick");
    while (game.state() != PlayState::wiping)
        tick(game);
    expect(game.board().at({0, 0}) == Block::empty &&
               game.presentation_board().at({0, 0}) == Block::t,
           "wipe presentation trails the logically collapsed board");
    int guard = 0;
    while (game.state() != PlayState::falling && guard < 200) {
        tick(game);
        ++guard;
    }
    expect(guard < 200 && game.score() == 40,
           "clear pipeline awards the correct score and spawns again");
}

void test_top_out_after_second_spawn_lock() {
    using namespace tetris;
    SinglePlayer game;
    game.start({}, startup());
    const auto block_below_piece = [](SinglePlayer& session) {
        Cell bottom = occupied_cells(session.piece())[0];
        for (const Cell cell : occupied_cells(session.piece())) {
            if (cell.y > bottom.y)
                bottom = cell;
        }
        session.edit_board().set({bottom.x, bottom.y + 1}, Block::o);
    };
    block_below_piece(game);
    tick(game, {.down = true});
    expect(game.state() == PlayState::locked && game.locks_at_spawn() == 1,
           "first spawn-origin lock is tolerated");
    int guard = 0;
    while (game.state() != PlayState::falling && guard < 20) {
        tick(game);
        ++guard;
    }
    expect(guard < 20, "first spawn lock hands off to the next piece");
    block_below_piece(game);
    tick(game, {.down = true});
    expect(game.state() == PlayState::game_over && game.locks_at_spawn() == 2,
           "second spawn-origin lock causes top-out");
}

void test_type_b_completion_and_garbage() {
    using namespace tetris;
    StartupRandom random = startup();
    for (std::size_t index = 0; index < random.garbage.size(); ++index) {
        random.garbage[index] = {1, static_cast<std::uint8_t>(index)};
    }
    SinglePlayer garbage;
    garbage.start({.type = GameType::type_b, .type_b_height = 2}, random);
    expect(garbage.board().at({0, 13}) == Block::empty,
           "Type B leaves rows above selected height empty");
    for (int row = 14; row < 18; ++row) {
        expect(garbage.board().at({0, row}) != Block::empty,
               "odd samples create Type B blocks");
        expect(garbage.board().at({9, row}) == Block::empty,
               "every Type B garbage row receives a guaranteed hole");
    }
    random.garbage[0] = {2, 7};
    garbage.start({.type = GameType::type_b, .type_b_height = 1}, random);
    expect(garbage.board().at({0, 16}) == Block::empty &&
               garbage.board().at({1, 16}) == Block::garbage_1,
           "occupancy parity and appearance bits remain independent");

    SinglePlayer completion;
    completion.start({.type = GameType::type_b}, startup());
    for (int clear = 0; clear < 7; ++clear)
        force_tetris(completion);
    expect(completion.lines() == 0 && completion.state() == PlayState::complete,
           "twenty-five Type B lines complete the game");
    expect(completion.line_counts().tetrises == 7 && completion.score() == 8'400,
           "oversized final clear is tallied in Type B score");
}

void test_level_saturation_pause_and_pacing() {
    using namespace tetris;
    SinglePlayer progression;
    progression.start({}, startup());
    force_tetris(progression);
    force_tetris(progression);
    expect(progression.level() == 0, "eight lines remain level zero");
    force_tetris(progression);
    expect(progression.lines() == 12 && progression.level() == 1 &&
               progression.gravity_frames() == 48,
           "crossing ten lines advances level and gravity");

    SinglePlayer saturated;
    saturated.start({}, startup());
    saturated.debug_set_score(999'990);
    for (int column = 0; column < 6; ++column)
        saturated.edit_board().set({column, 17}, Block::j);
    saturated.debug_place_piece({.kind = PieceKind::I, .origin = {6, 15}});
    tick(saturated, {.down = true});
    int guard = 0;
    while (saturated.state() != PlayState::falling && guard < 200) {
        tick(saturated);
        ++guard;
    }
    expect(saturated.score() == 999'999, "score saturates at six display digits");

    SinglePlayer paused;
    paused.start({}, startup());
    const std::uint64_t before = paused.tick_count();
    tick(paused, {.start = true});
    expect(paused.paused() && paused.tick_count() == before + 1,
           "pause preserves deterministic tick time");
    tick(paused);
    tick(paused, {.select = true});
    expect(!paused.preview_visible(), "Select toggles the preview");

    const auto clear_ticks = [](LineClearSpeed speed) {
        SinglePlayer game;
        game.start({}, startup());
        game.set_line_clear_speed(speed);
        for (int column = 0; column < 6; ++column)
            game.edit_board().set({column, 17}, Block::j);
        game.debug_place_piece({.kind = PieceKind::I, .origin = {6, 15}});
        tick(game, {.down = true});
        tick(game);
        int count = 0;
        while (game.state() != PlayState::falling && count < 200) {
            tick(game);
            ++count;
        }
        return count;
    };
    const int original = clear_ticks(LineClearSpeed::original);
    const int fast = clear_ticks(LineClearSpeed::fast);
    const int instant = clear_ticks(LineClearSpeed::instant);
    expect(original > fast && fast > instant,
           "line-clear profiles preserve strict original/fast/instant ordering");
}

void test_demo_and_oriented_fixed_pieces() {
    using namespace tetris;
    constexpr std::array runs = {
        DemoRun{{}, 2}, DemoRun{{.left = true, .rotate_left = true}, 1}, DemoRun{{}, 0},
    };
    DemoPlayback playback;
    playback.start(runs);
    expect(playback.tick() == Buttons{} && playback.tick() == Buttons{} &&
               playback.tick() == Buttons{},
           "demo run countdown retains its first held state");
    const Buttons changed = playback.tick();
    expect(changed.left && changed.rotate_left, "demo changes to its next semantic command");

    constexpr std::array fixed = {
        PieceSpec{PieceKind::T}, PieceSpec{PieceKind::I, Rotation::reverse},
        PieceSpec{PieceKind::O},
    };
    SinglePlayer game;
    game.start({.starting_level = 9}, startup(), fixed);
    expect(game.piece().kind == PieceKind::I && game.piece().rotation == Rotation::reverse &&
               game.preview() == PieceSpec{PieceKind::O},
           "startup handoffs preserve oriented demo pieces and preview");
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
    if (failures != 0)
        return 1;
    std::puts("modern Native Tetris simulation tests passed");
    return 0;
}
