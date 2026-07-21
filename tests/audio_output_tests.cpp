#include "audio/output.hpp"
#include "content/catalog.hpp"
#include "content/rom.hpp"
#include "game/state.hpp"

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

tetris::GameInput input(tetris::Buttons one = {}, tetris::Buttons two = {}) {
    return {
        .player_one = one,
        .player_two = two,
        .random = {11, 12, 13},
        .startup = startup(),
        .versus = versus_random(),
    };
}

tetris::GameResources resources(const tetris::content::Catalog& content) {
    return {content.gameplay.data, content.type_a_demo.runs, content.type_b_demo.runs,
            content.demo_pieces, content.type_b_demo_garbage.bytes};
}

void step(tetris::GameState& state, tetris::audio::Output& output, tetris::Buttons one = {},
          tetris::Buttons two = {}) {
    step_game(state, input(one, two));
    output.step(state, 7, false);
}

void release(tetris::GameState& state, tetris::audio::Output& output) {
    step(state, output);
}

void press(tetris::GameState& state, tetris::audio::Output& output, tetris::Buttons one,
           tetris::Buttons two = {}) {
    step(state, output, one, two);
    release(state, output);
}

void block_below_spawn(const tetris::GameplayData& data, tetris::SinglePlayer& game) {
    const auto cells = tetris::occupied_cells(data, game.piece);
    const auto lowest = std::ranges::max_element(cells, {}, &tetris::Cell::y);
    game.board.set({lowest->x, lowest->y + 1}, tetris::Block::j);
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

    GameState state;
    start_game(state, resources(content));
    start_session(state, {.type = GameType::type_a}, startup());
    output.step(state, 7, false);
    expect(output.director().active_song == 4, "Type A selects the original Korobeiniki program");
    expect(output.director().stereo_mask == 0xFF,
           "Korobeiniki uses its decoded all-channel stereo mode");
    expect(output.queued_bytes() == 0, "fast-forward advances synthesis without queuing stale PCM");

    step(state, output, {.right = true});
    expect(output.director().sounds.channel(content::SoundChannel::pulse).active() &&
               output.director().sounds.channel(content::SoundChannel::pulse).effect->original_id ==
                   4,
           "successful horizontal movement starts the shift cue");
    release(state, output);
    step(state, output, {.confirm = true});
    expect(output.director().sounds.channel(content::SoundChannel::pulse).effect->original_id == 3,
           "successful rotation starts the rotation cue");

    release(state, output);
    const std::uint8_t timer_before_pause = output.director().music.channels[0].timer;
    step(state, output, {.start = true});
    expect(output.director().paused &&
               !output.director().sounds.channel(content::SoundChannel::pulse).active(),
           "pausing freezes music and clears active effects");
    release(state, output);
    expect(output.director().music.channels[0].timer == timer_before_pause,
           "pause melody preserves the sequencer countdown");
    step(state, output, {.start = true});
    expect(!output.director().paused &&
               output.director().music.channels[0].timer < timer_before_pause,
           "unpausing resumes the existing music program");

    output.reset();
    start_game(state, resources(content));
    start_session(state, {.type = GameType::type_a}, startup());
    set_line_clear_speed(state, LineClearSpeed::instant);
    output.step(state, 7, false);
    for (int y = board_height - 4; y < board_height; ++y) {
        for (int x = 0; x < board_width; ++x) {
            if (x != 6)
                state.single_player.board.set({x, y}, Block::j);
        }
    }
    state.single_player.debug_place_piece(
        {.kind = PieceKind::I, .rotation = Rotation::right, .origin = {5, board_height - 4}});
    step(state, output, {.down = true});
    expect(output.director().sounds.channel(content::SoundChannel::noise).active() &&
               output.director().sounds.channel(content::SoundChannel::noise).effect->original_id ==
                   2,
           "piece lock starts the original noise cue");
    release(state, output);
    expect(output.director().sounds.channel(content::SoundChannel::pulse).active() &&
               output.director().sounds.channel(content::SoundChannel::pulse).effect->original_id ==
                   7,
           "instant clear pacing still emits the semantic Tetris cue");

    output.reset();
    start_game(state, resources(content));
    start_session(state, {.type = GameType::type_a}, startup());
    output.step(state, 7, false);
    block_below_spawn(content.gameplay.data, state.single_player);
    step(state, output, {.down = true});
    int guard = 0;
    while (state.single_player.state != PlayState::falling && guard < 40) {
        release(state, output);
        ++guard;
    }
    block_below_spawn(content.gameplay.data, state.single_player);
    step(state, output, {.down = true});
    expect(guard < 40 && state.screen == Screen::game_over,
           "two spawn locks reach the authentic top-out route");
    expect(output.director().sounds.channel(content::SoundChannel::wave).active() &&
               output.director().sounds.channel(content::SoundChannel::wave).effect->original_id ==
                   2,
           "top-out starts the divider-pitched game-over cue");
    for (int elapsed = 1; elapsed < 30; ++elapsed)
        release(state, output);
    release(state, output);
    expect(!output.director().sounds.channel(content::SoundChannel::wave).active(),
           "game-over cue advances once and stops rather than retriggering");

    output.reset();
    start_game(state, resources(content));
    output.step(state, 7, false);
    press(state, output, {.start = true});
    press(state, output, {.right = true});
    press(state, output, {.right = true});
    press(state, output, {.start = true});
    press(state, output, {.confirm = true});
    expect(state.screen == Screen::versus_gameplay && output.director().active_song == 4,
           "local versus begins with the selected normal gameplay song");
    output.set_polyphonic(true);
    step(state, output, {.right = true}, {.right = true});
    expect(output.polyphonic_voice_count() == 2,
           "enhanced audio preserves simultaneous same-channel versus effects");
    output.set_polyphonic(false);
    expect(output.polyphonic_voice_count() == 0,
           "returning to original audio clears independent effect voices");
    state.versus.players[0].game.board.set({0, 6}, Block::j);
    output.step(state, 7, false);
    expect(output.director().active_song == 7,
           "a twelve-row versus stack switches to the danger song");
    state.versus.players[0].game.board = Board{};
    output.step(state, 7, false);
    expect(output.director().active_song == 4,
           "dropping below danger height restores the selected song");
    state.versus.players[0].game.add_garbage(1, 3);
    output.step(state, 7, false);
    expect(output.director().sounds.channel(content::SoundChannel::pulse).active() &&
               output.director().sounds.channel(content::SoundChannel::pulse).effect->original_id ==
                   5,
           "incoming versus garbage starts its attack cue");

    output.reset();
    start_game(state, resources(content));
    output.step(state, 7, false);
    press(state, output, {.start = true});
    press(state, output, {.confirm = true});
    press(state, output, {.right = true});
    press(state, output, {.down = true});
    press(state, output, {.confirm = true});
    press(state, output, {.confirm = true});
    expect(state.screen == Screen::gameplay && state.selected_music == Music::off &&
               output.director().active_song == -1,
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
