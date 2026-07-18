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
    case Music::a: return 4;
    case Music::b: return 5;
    case Music::c: return 6;
    case Music::off: return -1;
    }
    return -1;
}

bool dangerous(const SinglePlayer& game) {
    for (int y = 0; y < 7; ++y) {
        for (int x = 0; x < board_width; ++x) {
            if (game.board().at({x, y}) != Block::empty)
                return true;
        }
    }
    return false;
}

} // namespace

void Director::attach(const content::AudioCatalog& catalog) {
    reset();
    catalog_ = &catalog;
    sounds_.attach(catalog);
}

void Director::reset() {
    catalog_ = nullptr;
    music_.stop();
    sounds_.stop();
    active_song_ = -1;
    have_snapshot_ = false;
    paused_ = false;
    pause_timer_ = 0;
    pause_note_ = 0;
    pause_note_age_ = 0;
    stereo_mode_ = 3;
    pan_interval_ = 0;
    pan_counter_ = 0;
    pan_frame_ = 0;
    first_mask_ = 0xFF;
    second_mask_ = 0xFF;
    stereo_mask_ = 0xFF;
}

void Director::tick(const GameFlow& flow, std::uint8_t random_sample) {
    if (catalog_ == nullptr)
        return;
    const int requested_song = desired_song(flow);
    const bool restarted_menu = have_snapshot_ && flow.screen() != previous_screen_ &&
        (flow.screen() == Screen::game_type || flow.screen() == Screen::type_a_level ||
         flow.screen() == Screen::type_b_level || flow.screen() == Screen::versus_height);
    if (requested_song != active_song_ || (requested_song >= 0 && restarted_menu)) {
        active_song_ = requested_song;
        if (requested_song < 0) {
            music_.stop();
        } else {
            (void)music_.play(*catalog_, static_cast<std::size_t>(requested_song));
            start_stereo(static_cast<std::size_t>(requested_song));
        }
    }

    sounds_.tick(random_sample);
    update_pause(flow);
    if (!paused_) {
        process_flow(flow, random_sample);
        music_.tick();
    }
    update_stereo();
}

bool Director::audition_sound(std::size_t effect) {
    if (catalog_ == nullptr || effect >= catalog_->effects.size())
        return false;
    const content::SoundEffect& sound = catalog_->effects[effect];
    return sounds_.play(sound.channel, sound.original_id);
}

bool Director::audition_song(std::size_t song) {
    if (catalog_ == nullptr || song >= catalog_->songs.size())
        return false;
    active_song_ = static_cast<int>(song);
    start_stereo(song);
    return music_.play(*catalog_, song);
}

int Director::active_song() const { return active_song_; }
bool Director::paused() const { return paused_; }
int Director::pause_note() const { return pause_note_; }
int Director::pause_note_age() const { return pause_note_age_; }
std::uint8_t Director::stereo_mask() const { return stereo_mask_; }
const MusicPlayer& Director::music() const { return music_; }
const SoundEffects& Director::sounds() const { return sounds_; }

int Director::desired_song(const GameFlow& flow) const {
    switch (flow.screen()) {
    case Screen::title: return 2;
    case Screen::game_type:
    case Screen::music:
    case Screen::type_a_level:
    case Screen::type_b_level:
    case Screen::type_b_height:
    case Screen::versus_height:
    case Screen::gameplay:
    case Screen::demo:
        return gameplay_song(flow.selected_music());
    case Screen::versus_gameplay:
        if (dangerous(flow.versus().player(0)) || dangerous(flow.versus().player(1)))
            return 7;
        return gameplay_song(flow.selected_music());
    case Screen::game_over:
        return flow.ending_stage() == EndingStage::game_over_curtain ? active_song_ : 3;
    case Screen::type_b_celebration: return 1;
    case Screen::dancers: return 9 + flow.selected_height();
    case Screen::buran:
    case Screen::rocket:
        if (flow.ending_stage() == EndingStage::rocket_initialize ||
            flow.ending_stage() == EndingStage::buran_initialize ||
            (flow.ending_stage() == EndingStage::rocket_return && flow.ending_elapsed() >= 4))
            return -1;
        return 15;
    case Screen::scoreboard: return -1;
    case Screen::name_entry: return 0;
    case Screen::versus_round_result: return 8;
    case Screen::versus_match_result: return 16;
    case Screen::copyright_fixed:
    case Screen::copyright_skippable:
        return -1;
    }
    return -1;
}

void Director::process_flow(const GameFlow& flow, std::uint8_t random_sample) {
    if (flow.screen() == Screen::versus_gameplay) {
        process_game(flow.versus().player(0), random_sample);
        process_game(flow.versus().player(1), random_sample);
    } else if (flow.screen() != Screen::demo) {
        process_game(flow.game(), random_sample);
    }

    if (!have_snapshot_) {
        remember(flow);
        return;
    }
    if (flow.screen() != previous_screen_) {
        if (menu(flow.screen()) || flow.screen() == Screen::gameplay ||
            flow.screen() == Screen::versus_gameplay)
            (void)sounds_.play(content::SoundChannel::pulse, 2);
    } else if (menu(flow.screen())) {
        const bool changed = flow.selected_type() != previous_type_ ||
            flow.selected_music() != previous_music_ || flow.selected_level() != previous_level_ ||
            flow.selected_height() != previous_height_ || flow.name_cursor() != previous_name_cursor_ ||
            flow.pending_name() != previous_name_;
        if (changed)
            (void)sounds_.play(content::SoundChannel::pulse, 1);
    }

    const EndingStage stage = flow.ending_stage();
    const bool ignition = stage == EndingStage::rocket_ignition ||
        stage == EndingStage::rocket_liftoff_delay || stage == EndingStage::rocket_liftoff ||
        stage == EndingStage::buran_ignition || stage == EndingStage::buran_final_ignition ||
        stage == EndingStage::buran_liftoff;
    if (ignition && (stage != previous_ending_ ||
                     (flow.ending_elapsed() % 10 == 0 &&
                      flow.ending_elapsed() != previous_ending_elapsed_)))
        (void)sounds_.play(content::SoundChannel::noise, 3);
    const bool rising = (stage == EndingStage::rocket_rising &&
                         previous_ending_ == EndingStage::rocket_liftoff) ||
                        (stage == EndingStage::buran_rising &&
                         previous_ending_ == EndingStage::buran_liftoff);
    if (rising)
        (void)sounds_.play(content::SoundChannel::noise, 4);
    remember(flow);
}

void Director::process_game(const SinglePlayer& game, std::uint8_t random_sample) {
    for (const Event event : game.events()) {
        switch (event.type) {
        case GameEvent::moved:
            if (event.value != 0)
                (void)sounds_.play(content::SoundChannel::pulse, 4);
            break;
        case GameEvent::rotated: (void)sounds_.play(content::SoundChannel::pulse, 3); break;
        case GameEvent::landed: (void)sounds_.play(content::SoundChannel::noise, 2); break;
        case GameEvent::cleared_lines:
            (void)sounds_.play(content::SoundChannel::pulse,
                               static_cast<std::uint8_t>(event.value == 4 ? 7 : 6));
            break;
        case GameEvent::level_changed: (void)sounds_.play(content::SoundChannel::pulse, 8); break;
        case GameEvent::garbage_applied: (void)sounds_.play(content::SoundChannel::pulse, 5); break;
        case GameEvent::game_over:
            (void)sounds_.play(content::SoundChannel::wave, 2, random_sample);
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

void Director::update_pause(const GameFlow& flow) {
    const bool requested = flow.screen() == Screen::gameplay ? flow.game().paused()
                         : flow.screen() == Screen::versus_gameplay ? flow.versus().paused()
                                                                   : false;
    const bool started = requested && !paused_;
    if (requested != paused_) {
        paused_ = requested;
        pause_timer_ = paused_ ? 48 : 0;
        pause_note_ = 0;
        pause_note_age_ = 0;
        if (paused_)
            sounds_.stop();
    }
    if (!paused_ || started)
        return;
    if (pause_timer_ > 16)
        --pause_timer_;
    if (pause_timer_ == 40 || pause_timer_ == 32 || pause_timer_ == 24) {
        pause_note_ = pause_note_ == 0 ? 1 : 0;
        pause_note_age_ = 0;
    } else {
        ++pause_note_age_;
    }
}

void Director::start_stereo(std::size_t song) {
    if (catalog_ == nullptr || song >= catalog_->stereo.size())
        return;
    const auto setting = catalog_->stereo[song];
    stereo_mode_ = setting[0];
    pan_interval_ = setting[1];
    first_mask_ = setting[2];
    second_mask_ = setting[3];
    pan_counter_ = 0;
    pan_frame_ = 0;
}

void Director::update_stereo() {
    if (!music_.playing()) {
        stereo_mask_ = 0xFF;
        return;
    }
    if (stereo_mode_ == 1) {
        stereo_mask_ = first_mask_;
    } else if (stereo_mode_ == 3) {
        stereo_mask_ = 0xFF;
    } else {
        ++pan_frame_;
        if (pan_frame_ == pan_interval_) {
            pan_frame_ = 0;
            ++pan_counter_;
        }
        stereo_mask_ = (pan_counter_ & 1U) == 0 ? first_mask_ : second_mask_;
    }
    if (sounds_.channel(content::SoundChannel::wave).active())
        stereo_mask_ = static_cast<std::uint8_t>(stereo_mask_ | 0x44U);
    if (sounds_.channel(content::SoundChannel::noise).active())
        stereo_mask_ = static_cast<std::uint8_t>(stereo_mask_ | 0x88U);
}

void Director::remember(const GameFlow& flow) {
    previous_screen_ = flow.screen();
    previous_type_ = flow.selected_type();
    previous_music_ = flow.selected_music();
    previous_level_ = flow.selected_level();
    previous_height_ = flow.selected_height();
    previous_ending_ = flow.ending_stage();
    previous_ending_elapsed_ = flow.ending_elapsed();
    previous_name_cursor_ = flow.name_cursor();
    previous_name_ = flow.pending_name();
    have_snapshot_ = true;
}

} // namespace tetris::audio
