#include "audio/output.hpp"
#include "content/catalog.hpp"
#include "content/rom.hpp"
#include "game/flow.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {

#ifdef TETRIS_TEST_ROM
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
        random.pieces[index] = {static_cast<tetris::PieceKind>(index % 7)};
    return random;
}

tetris::FlowInput input(tetris::Buttons one = {}, tetris::Buttons two = {}) {
    return {
        .player_one = one,
        .player_two = two,
        .random = {11, 12, 13},
        .startup = startup(),
        .versus = versus_random(),
    };
}

tetris::FlowResources resources(const tetris::content::Catalog& content) {
    return {content.type_a_demo.runs, content.type_b_demo.runs,
            content.demo_pieces, content.type_b_demo_garbage.bytes};
}

void tick(tetris::GameFlow& flow, tetris::audio::Output& output,
          tetris::Buttons one = {}, tetris::Buttons two = {}) {
    flow.tick(input(one, two));
    output.tick(flow, 7, false);
}

void release(tetris::GameFlow& flow, tetris::audio::Output& output) {
    tick(flow, output);
}

void press(tetris::GameFlow& flow, tetris::audio::Output& output,
           tetris::Buttons one, tetris::Buttons two = {}) {
    tick(flow, output, one, two);
    release(flow, output);
}

void reach_title(tetris::GameFlow& flow, tetris::audio::Output& output) {
    for (int frame = 0; frame < 250; ++frame)
        tick(flow, output);
    press(flow, output, {.rotate_right = true});
}

void block_below_spawn(tetris::SinglePlayer& game) {
    const auto cells = tetris::occupied_cells(game.piece());
    const auto lowest = std::ranges::max_element(cells, {}, &tetris::Cell::y);
    game.edit_board().set({lowest->x, lowest->y + 1}, tetris::Block::j);
}

void test_output() {
    using namespace tetris;
    using namespace tetris::audio;
    content::Rom rom;
    content::Catalog content;
    std::string error;
    expect(content::load_rom(TETRIS_TEST_ROM, rom, error), "audio output ROM loads");
    expect(content::extract_catalog(rom, content, error), "audio output catalog extracts");
    if (!error.empty())
        return;

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    Output output;
    expect(output.initialize(content.audio), "dummy SDL audio output initializes");
    expect((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) != 0,
           "audio output owns subsystem initialization");

    GameFlow flow;
    flow.start(resources(content));
    flow.start_session({.type = GameType::type_a}, startup());
    output.tick(flow, 7, false);
    expect(output.director().active_song() == 4,
           "Type A selects the original Korobeiniki program");
    expect(output.director().stereo_mask() == 0xFF,
           "Korobeiniki uses its decoded all-channel stereo mode");
    expect(output.queued_bytes() == 0,
           "fast-forward advances synthesis without queuing stale PCM");

    tick(flow, output, {.right = true});
    expect(output.director().sounds().channel(content::SoundChannel::pulse).active() &&
               output.director().sounds().channel(content::SoundChannel::pulse)
                       .effect->original_id == 4,
           "successful horizontal movement starts the shift cue");
    release(flow, output);
    tick(flow, output, {.rotate_right = true});
    expect(output.director().sounds().channel(content::SoundChannel::pulse)
                   .effect->original_id == 3,
           "successful rotation starts the rotation cue");

    release(flow, output);
    const std::uint8_t timer_before_pause = output.director().music().channels()[0].timer;
    tick(flow, output, {.start = true});
    expect(output.director().paused() &&
               !output.director().sounds().channel(content::SoundChannel::pulse).active(),
           "pausing freezes music and clears active effects");
    release(flow, output);
    expect(output.director().music().channels()[0].timer == timer_before_pause,
           "pause melody preserves the sequencer countdown");
    tick(flow, output, {.start = true});
    expect(!output.director().paused() &&
               output.director().music().channels()[0].timer < timer_before_pause,
           "unpausing resumes the existing music program");

    output.reset();
    flow.start(resources(content));
    flow.start_session({.type = GameType::type_a}, startup());
    flow.set_line_clear_speed(LineClearSpeed::instant);
    output.tick(flow, 7, false);
    for (int y = board_height - 4; y < board_height; ++y) {
        for (int x = 0; x < board_width; ++x) {
            if (x != 6)
                flow.edit_game().edit_board().set({x, y}, Block::j);
        }
    }
    flow.edit_game().debug_place_piece({.kind = PieceKind::I,
                                           .rotation = Rotation::right,
                                           .origin = {5, board_height - 4}});
    tick(flow, output, {.down = true});
    expect(output.director().sounds().channel(content::SoundChannel::noise).active() &&
               output.director().sounds().channel(content::SoundChannel::noise)
                       .effect->original_id == 2,
           "piece lock starts the original noise cue");
    release(flow, output);
    expect(output.director().sounds().channel(content::SoundChannel::pulse).active() &&
               output.director().sounds().channel(content::SoundChannel::pulse)
                       .effect->original_id == 7,
           "instant clear pacing still emits the semantic Tetris cue");

    output.reset();
    flow.start(resources(content));
    flow.start_session({.type = GameType::type_a}, startup());
    output.tick(flow, 7, false);
    block_below_spawn(flow.edit_game());
    tick(flow, output, {.down = true});
    int guard = 0;
    while (flow.game().state() != PlayState::falling && guard < 40) {
        release(flow, output);
        ++guard;
    }
    block_below_spawn(flow.edit_game());
    tick(flow, output, {.down = true});
    expect(guard < 40 && flow.screen() == Screen::game_over,
           "two spawn locks reach the authentic top-out route");
    expect(output.director().sounds().channel(content::SoundChannel::wave).active() &&
               output.director().sounds().channel(content::SoundChannel::wave)
                       .effect->original_id == 2,
           "top-out starts the divider-pitched game-over cue");
    for (int elapsed = 1; elapsed < 30; ++elapsed)
        release(flow, output);
    release(flow, output);
    expect(!output.director().sounds().channel(content::SoundChannel::wave).active(),
           "game-over cue advances once and stops rather than retriggering");

    output.reset();
    flow.start(resources(content));
    output.tick(flow, 7, false);
    reach_title(flow, output);
    press(flow, output, {.start = true});
    press(flow, output, {.right = true});
    press(flow, output, {.right = true});
    press(flow, output, {.start = true});
    press(flow, output, {.rotate_right = true});
    expect(flow.screen() == Screen::versus_gameplay && output.director().active_song() == 4,
           "local versus begins with the selected normal gameplay song");
    flow.edit_versus().edit_player(0).edit_board().set({0, 6}, Block::j);
    output.tick(flow, 7, false);
    expect(output.director().active_song() == 7,
           "a twelve-row versus stack switches to the danger song");
    flow.edit_versus().edit_player(0).edit_board() = Board{};
    output.tick(flow, 7, false);
    expect(output.director().active_song() == 4,
           "dropping below danger height restores the selected song");
    flow.edit_versus().edit_player(0).add_garbage(1, 3);
    output.tick(flow, 7, false);
    expect(output.director().sounds().channel(content::SoundChannel::pulse).active() &&
               output.director().sounds().channel(content::SoundChannel::pulse)
                       .effect->original_id == 5,
           "incoming versus garbage starts its attack cue");

    output.reset();
    flow.start(resources(content));
    output.tick(flow, 7, false);
    reach_title(flow, output);
    press(flow, output, {.start = true});
    press(flow, output, {.rotate_right = true});
    press(flow, output, {.right = true});
    press(flow, output, {.down = true});
    press(flow, output, {.rotate_right = true});
    press(flow, output, {.rotate_right = true});
    expect(flow.screen() == Screen::gameplay && flow.selected_music() == Music::off &&
               output.director().active_song() == -1,
           "music-off selection remains silent when gameplay begins");

    output.shutdown();
    expect((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0,
           "output shutdown releases the subsystem it initialized");
}
#endif

} // namespace

int main() {
#ifdef TETRIS_TEST_ROM
    test_output();
    if (failures != 0)
        return 1;
#endif
    std::puts("modern Native Tetris audio output tests passed");
    return 0;
}
