#include "audio/director.hpp"

#include <algorithm>

namespace tetris::audio {
namespace {

bool menu(Screen screen) {
    return screen == Screen::game_type || screen == Screen::music ||
           screen == Screen::type_a_level || screen == Screen::type_b_level ||
           screen == Screen::type_b_height || screen == Screen::versus_height ||
           screen == Screen::name_entry;
}

int gameplay_song(Music music) {
    switch (music) {
    case Music::a:
        return 4;
    case Music::b:
        return 5;
    case Music::c:
        return 6;
    case Music::off:
        return -1;
    }
    return -1;
}

bool dangerous(const SinglePlayer& game) {
    for (int y = 0; y < 7; ++y) {
        for (int x = 0; x < board_width; ++x) {
            if (game.board.at({x, y}) != Block::empty)
                return true;
        }
    }
    return false;
}

} // namespace

void Director::attach(const content::AudioCatalog& source) {
    reset();
    catalog = &source;
    sounds.attach(source);
}

void Director::reset() {
    // Disconnect content and stop both music and effects.
    catalog = nullptr;
    music.stop();
    sounds.stop();
    active_song = -1;
    have_snapshot = false;
    paused = false;
    pause_timer = 0;
    pause_note = 0;
    pause_note_age = 0;

    // Reset the original song-specific stereo sequencer.
    stereo_mode = 3;
    pan_interval = 0;
    pan_counter = 0;
    pan_frame = 0;
    first_mask = 0xFF;
    second_mask = 0xFF;
    stereo_mask = 0xFF;
}

void Director::step(const GameState& state, std::uint8_t random_sample) {
    if (catalog == nullptr)
        return;
    // Change songs when the screen or danger state requests it.
    const int requested_song = desired_song(state);
    const bool restarted_menu =
        have_snapshot && state.screen != previous_screen &&
        (state.screen == Screen::game_type || state.screen == Screen::type_a_level ||
         state.screen == Screen::type_b_level || state.screen == Screen::versus_height);
    if (requested_song != active_song || (requested_song >= 0 && restarted_menu)) {
        active_song = requested_song;
        if (requested_song < 0) {
            music.stop();
        } else {
            (void)music.play(*catalog, static_cast<std::size_t>(requested_song));
            start_stereo(static_cast<std::size_t>(requested_song));
        }
    }

    // Advance effects, pause tones, music, and stereo panning.
    sounds.step(random_sample);
    update_pause(state);
    if (!paused) {
        process_game_state(state, random_sample);
        music.step();
    }
    update_stereo();
}

bool Director::audition_sound(std::size_t effect) {
    if (catalog == nullptr || effect >= catalog->effects.size())
        return false;
    const content::SoundEffect& sound = catalog->effects[effect];
    return sounds.play(sound.channel, sound.original_id);
}

bool Director::audition_song(std::size_t song) {
    if (catalog == nullptr || song >= catalog->songs.size())
        return false;
    active_song = static_cast<int>(song);
    start_stereo(song);
    return music.play(*catalog, song);
}

int Director::desired_song(const GameState& state) const {
    switch (state.screen) {
    case Screen::title:
        return 2;
    case Screen::game_type:
    case Screen::music:
    case Screen::type_a_level:
    case Screen::type_b_level:
    case Screen::type_b_height:
    case Screen::versus_height:
    case Screen::gameplay:
    case Screen::demo:
        return gameplay_song(state.selected_music);
    case Screen::versus_gameplay:
        if (dangerous(state.versus.players[0].game) || dangerous(state.versus.players[1].game))
            return 7;
        return gameplay_song(state.selected_music);
    case Screen::game_over:
        return state.ending_stage == EndingStage::game_over_curtain ? active_song : 3;
    case Screen::type_b_celebration:
        return 1;
    case Screen::dancers:
        return 9 + state.type_b_height;
    case Screen::buran:
    case Screen::rocket:
        if (state.ending_stage == EndingStage::rocket_initialize ||
            state.ending_stage == EndingStage::buran_initialize ||
            (state.ending_stage == EndingStage::rocket_return && state.ending_elapsed >= 4))
            return -1;
        return 15;
    case Screen::scoreboard:
        return -1;
    case Screen::name_entry:
        return 0;
    case Screen::versus_round_result:
        return 8;
    case Screen::versus_match_result:
        return 16;
    case Screen::copyright_fixed:
    case Screen::copyright_skippable:
        return -1;
    }
    return -1;
}

void Director::process_game_state(const GameState& state, std::uint8_t random_sample) {
    // Translate gameplay events from every active board.
    if (state.screen == Screen::versus_gameplay) {
        process_game(state.versus.players[0].game, random_sample);
        process_game(state.versus.players[1].game, random_sample);
    } else if (state.screen != Screen::demo) {
        process_game(state.single_player, random_sample);
    }

    // The first snapshot establishes comparison state without producing UI
    // sounds.
    if (!have_snapshot) {
        remember(state);
        return;
    }

    // Translate screen and menu selection changes.
    if (state.screen != previous_screen) {
        if (menu(state.screen) || state.screen == Screen::gameplay ||
            state.screen == Screen::versus_gameplay)
            (void)sounds.play(content::SoundChannel::pulse, 2);
    } else if (menu(state.screen)) {
        const bool changed =
            state.selected_type != previous_type || state.selected_music != previous_music ||
            selected_level(state) != previous_level || state.type_b_height != previous_height ||
            state.name_cursor != previous_name_cursor || state.pending_name != previous_name;
        if (changed)
            (void)sounds.play(content::SoundChannel::pulse, 1);
    }

    // Translate launch-stage transitions into ignition and liftoff effects.
    const EndingStage stage = state.ending_stage;
    const bool ignition =
        stage == EndingStage::rocket_ignition || stage == EndingStage::rocket_liftoff_delay ||
        stage == EndingStage::rocket_liftoff || stage == EndingStage::buran_ignition ||
        stage == EndingStage::buran_final_ignition || stage == EndingStage::buran_liftoff;
    if (ignition && (stage != previous_ending || (state.ending_elapsed % 10 == 0 &&
                                                  state.ending_elapsed != previous_ending_elapsed)))
        (void)sounds.play(content::SoundChannel::noise, 3);
    const bool rising =
        (stage == EndingStage::rocket_rising && previous_ending == EndingStage::rocket_liftoff) ||
        (stage == EndingStage::buran_rising && previous_ending == EndingStage::buran_liftoff);
    if (rising)
        (void)sounds.play(content::SoundChannel::noise, 4);
    remember(state);
}

void Director::process_game(const SinglePlayer& game, std::uint8_t random_sample) {
    for (const Event event : game.events) {
        switch (event.type) {
        case GameEvent::moved:
            if (event.value != 0)
                (void)sounds.play(content::SoundChannel::pulse, 4);
            break;
        case GameEvent::rotated:
            (void)sounds.play(content::SoundChannel::pulse, 3);
            break;
        case GameEvent::landed:
            (void)sounds.play(content::SoundChannel::noise, 2);
            break;
        case GameEvent::cleared_lines:
            (void)sounds.play(content::SoundChannel::pulse,
                              static_cast<std::uint8_t>(event.value == 4 ? 7 : 6));
            break;
        case GameEvent::level_changed:
            (void)sounds.play(content::SoundChannel::pulse, 8);
            break;
        case GameEvent::garbage_applied:
            (void)sounds.play(content::SoundChannel::pulse, 5);
            break;
        case GameEvent::game_over:
            (void)sounds.play(content::SoundChannel::wave, 2, random_sample);
            break;
        case GameEvent::spawned:
        case GameEvent::score_changed:
        case GameEvent::paused:
        case GameEvent::preview_changed:
        case GameEvent::complete:
            break;
        }
    }
}

void Director::update_pause(const GameState& state) {
    // Read pause state from the active single-player or versus simulation.
    const bool requested = state.screen == Screen::gameplay          ? state.single_player.paused
                           : state.screen == Screen::versus_gameplay ? state.versus.paused
                                                                     : false;
    const bool started = requested && !paused;
    if (requested != paused) {
        paused = requested;
        pause_timer = paused ? 48 : 0;
        pause_note = 0;
        pause_note_age = 0;
        if (paused)
            sounds.stop();
    }
    if (!paused || started)
        return;

    // Alternate the two original pause notes at their fixed intervals.
    if (pause_timer > 16)
        --pause_timer;
    if (pause_timer == 40 || pause_timer == 32 || pause_timer == 24) {
        pause_note = pause_note == 0 ? 1 : 0;
        pause_note_age = 0;
    } else {
        ++pause_note_age;
    }
}

void Director::start_stereo(std::size_t song) {
    if (catalog == nullptr || song >= catalog->stereo.size())
        return;
    const auto setting = catalog->stereo[song];
    stereo_mode = setting[0];
    pan_interval = setting[1];
    first_mask = setting[2];
    second_mask = setting[3];
    pan_counter = 0;
    pan_frame = 0;
}

void Director::update_stereo() {
    if (!music.playing) {
        stereo_mask = 0xFF;
        return;
    }
    // Apply fixed, centered, or alternating song panning.
    if (stereo_mode == 1) {
        stereo_mask = first_mask;
    } else if (stereo_mode == 3) {
        stereo_mask = 0xFF;
    } else {
        ++pan_frame;
        if (pan_frame == pan_interval) {
            pan_frame = 0;
            ++pan_counter;
        }
        stereo_mask = (pan_counter & 1U) == 0 ? first_mask : second_mask;
    }

    // Active effects force their channels into both output sides.
    if (sounds.channel(content::SoundChannel::wave).active())
        stereo_mask = static_cast<std::uint8_t>(stereo_mask | 0x44U);
    if (sounds.channel(content::SoundChannel::noise).active())
        stereo_mask = static_cast<std::uint8_t>(stereo_mask | 0x88U);
}

void Director::remember(const GameState& state) {
    previous_screen = state.screen;
    previous_type = state.selected_type;
    previous_music = state.selected_music;
    previous_level = selected_level(state);
    previous_height = state.type_b_height;
    previous_ending = state.ending_stage;
    previous_ending_elapsed = state.ending_elapsed;
    previous_name_cursor = state.name_cursor;
    previous_name = state.pending_name;
    have_snapshot = true;
}

} // namespace tetris::audio
