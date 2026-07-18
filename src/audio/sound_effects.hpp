#pragma once

#include "content/audio.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace tetris::audio {

enum class SoundEventType { started, changed, stopped, rejected };

struct SoundEvent {
    SoundEventType type{};
    content::SoundChannel channel{};
    std::uint8_t effect{};
    std::uint16_t tick{};
    bool triggered{};
};

struct SoundChannelState {
    const content::SoundEffect* effect{};
    std::array<std::uint8_t, 5> registers{};
    std::uint16_t elapsed{};
    std::uint8_t random_pitch{};

    bool active() const { return effect != nullptr; }
};

class SoundEffects {
public:
    void attach(const content::AudioCatalog& catalog);
    bool play(content::SoundChannel channel, std::uint8_t effect,
              std::uint8_t random_sample = 0);
    void tick(std::uint8_t random_sample = 0);
    void stop();

    const SoundChannelState& channel(content::SoundChannel channel) const;
    std::span<const SoundEvent> events() const;
    bool locks_music() const;

private:
    static std::size_t index(content::SoundChannel channel);
    const content::SoundEffect* find(content::SoundChannel channel, std::uint8_t effect) const;
    bool pulse_is_blocked(std::uint8_t effect) const;
    void advance(content::SoundChannel channel, std::uint8_t random_sample);
    void stop(content::SoundChannel channel);

    const content::AudioCatalog* catalog_{};
    std::array<SoundChannelState, 3> channels_{};
    std::vector<SoundEvent> events_;
};

} // namespace tetris::audio
