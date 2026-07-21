#pragma once

#include "audio/music.hpp"
#include "audio/sound_effects.hpp"
#include "game/state.hpp"

#include <cstdint>
#include <string>

namespace tetris::audio {

struct Director {
    // Source data and active players.
    const content::AudioCatalog* catalog{};
    MusicPlayer music;
    SoundEffects sounds;
    int active_song{-1};

    // Previous game state used to detect transitions and menu changes.
    Screen previous_screen{Screen::title};
    GameType previous_type{GameType::type_a};
    Music previous_music{Music::a};
    int previous_level{};
    int previous_height{};
    EndingStage previous_ending{EndingStage::none};
    int previous_ending_elapsed{};
    int previous_name_cursor{};
    std::string previous_name;
    bool have_snapshot{};

    // Pause jingle state.
    bool paused{};
    int pause_timer{};
    int pause_note{};
    int pause_note_age{};

    // Original stereo-panning sequence state.
    std::uint8_t stereo_mode{3};
    std::uint8_t pan_interval{};
    std::uint8_t pan_counter{};
    std::uint8_t pan_frame{};
    std::uint8_t first_mask{0xFF};
    std::uint8_t second_mask{0xFF};
    std::uint8_t stereo_mask{0xFF};

    void attach(const content::AudioCatalog& source);
    void reset();
    void step(const GameState& state, std::uint8_t random_sample);
    bool audition_sound(std::size_t effect);
    bool audition_song(std::size_t song);

    int desired_song(const GameState& state) const;
    void process_game_state(const GameState& state, std::uint8_t random_sample);
    void process_game(const SinglePlayer& game, std::uint8_t random_sample);
    void update_pause(const GameState& state);
    void start_stereo(std::size_t song);
    void update_stereo();
    void remember(const GameState& state);
};

} // namespace tetris::audio
