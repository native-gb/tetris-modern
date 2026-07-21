#pragma once

#include "content/audio.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace tetris::audio {

enum class SoundEventType { started, changed, stopped, rejected };

struct SoundEvent {
    SoundEventType type{};
    content::SoundChannel channel{};
    std::uint8_t effect{};
    std::uint16_t at_step{};
    bool triggered{};
};

struct SoundChannelState {
    const content::SoundEffect* effect{};
    std::array<std::uint8_t, 5> registers{};
    std::uint16_t elapsed{};
    std::uint8_t random_pitch{};

    bool active() const { return effect != nullptr; }
};

struct SoundEffects {
    const content::AudioCatalog* catalog{};
    std::array<SoundChannelState, 3> channels{};
    std::vector<SoundEvent> events;

    void attach(const content::AudioCatalog& source);
    bool play(content::SoundChannel channel, std::uint8_t effect, std::uint8_t random_sample = 0);
    void step(std::uint8_t random_sample = 0);
    void stop();

    const SoundChannelState& channel(content::SoundChannel channel) const;
    bool locks_music() const;

    static std::size_t index(content::SoundChannel channel);
    const content::SoundEffect* find(content::SoundChannel channel, std::uint8_t effect) const;
    bool pulse_is_blocked(std::uint8_t effect) const;
    void advance(content::SoundChannel channel, std::uint8_t random_sample);
    void stop(content::SoundChannel channel);
};

} // namespace tetris::audio
