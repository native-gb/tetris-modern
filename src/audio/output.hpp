#pragma once

#include "audio/director.hpp"
#include "gb_audio/game_boy_audio.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <vector>

namespace tetris::audio {

class Output {
public:
    bool initialize(const content::AudioCatalog& catalog);
    void shutdown();
    void reset();
    void tick(const GameFlow& flow, std::uint8_t random_sample, bool submit = true);

    void set_volume(float music, float effects);
    bool available() const;
    int queued_bytes() const;
    const Director& director() const;
    Director& debug_director();
    std::array<float, 4> music_meters() const;
    std::array<float, 4> sound_meters() const;

private:
    void update_states();
    void update_music_state();
    void update_sound_state();
    void render(bool submit);
    void apply_music_event(const MusicEvent& event);
    void apply_sound_event(const SoundEvent& event);
    static constexpr int sample_rate = 48'000;
    static constexpr double ticks_per_second = 59.7275;

    const content::AudioCatalog* catalog_{};
    SDL_AudioStream* stream_{};
    bool owns_audio_{};
    Director director_;
    gb_audio::Synth music_synth_;
    gb_audio::Synth sound_synth_;
    gb_audio::State music_state_;
    gb_audio::State sound_state_;
    std::array<std::uint32_t, 4> music_triggers_{};
    std::array<std::uint32_t, 4> sound_triggers_{};
    std::vector<float> music_samples_;
    std::vector<float> sound_samples_;
    std::vector<float> mixed_samples_;
    double sample_fraction_{};
    float music_volume_{0.8F};
    float effects_volume_{0.9F};
    bool previous_pause_{};
    int previous_pause_note_{};
};

} // namespace tetris::audio
