#pragma once

#include "audio/music.hpp"
#include "audio/sound_effects.hpp"
#include "game/flow.hpp"

#include <cstdint>
#include <string>

namespace tetris::audio {

class Director {
public:
    void attach(const content::AudioCatalog& catalog);
    void reset();
    void tick(const GameFlow& flow, std::uint8_t random_sample);
    bool audition_sound(std::size_t effect);
    bool audition_song(std::size_t song);

    int active_song() const;
    bool paused() const;
    int pause_note() const;
    int pause_note_age() const;
    std::uint8_t stereo_mask() const;
    const MusicPlayer& music() const;
    const SoundEffects& sounds() const;

private:
    int desired_song(const GameFlow& flow) const;
    void process_flow(const GameFlow& flow, std::uint8_t random_sample);
    void process_game(const SinglePlayer& game, std::uint8_t random_sample);
    void update_pause(const GameFlow& flow);
    void start_stereo(std::size_t song);
    void update_stereo();
    void remember(const GameFlow& flow);

    const content::AudioCatalog* catalog_{};
    MusicPlayer music_;
    SoundEffects sounds_;
    int active_song_{-1};
    Screen previous_screen_{Screen::copyright_fixed};
    GameType previous_type_{GameType::type_a};
    Music previous_music_{Music::a};
    int previous_level_{};
    int previous_height_{};
    EndingStage previous_ending_{EndingStage::none};
    int previous_ending_elapsed_{};
    int previous_name_cursor_{};
    std::string previous_name_;
    bool have_snapshot_{};
    bool paused_{};
    int pause_timer_{};
    int pause_note_{};
    int pause_note_age_{};
    std::uint8_t stereo_mode_{3};
    std::uint8_t pan_interval_{};
    std::uint8_t pan_counter_{};
    std::uint8_t pan_frame_{};
    std::uint8_t first_mask_{0xFF};
    std::uint8_t second_mask_{0xFF};
    std::uint8_t stereo_mask_{0xFF};
};

} // namespace tetris::audio
