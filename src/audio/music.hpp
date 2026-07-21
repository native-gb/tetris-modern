#pragma once

#include "content/audio.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tetris::audio {

enum class MusicEventType { instrument, note, rest, stopped };

struct MusicEvent {
    MusicEventType type{};
    int channel{};
};

struct MusicChannel {
    bool active{};
    bool resting{true};
    std::uint8_t note{};
    std::uint8_t duration{1};
    std::uint8_t timer{1};
    std::uint8_t vibrato_step{};
    std::uint8_t wave_volume{};
    std::uint16_t period{};
    std::uint16_t output_period{};
    std::array<std::uint8_t, 3> instrument{};
    std::uint8_t wave{};
    std::array<std::uint8_t, 5> registers{};
    std::size_t sequence{content::no_music_index};
    std::size_t section{};
    std::size_t command{};
};

struct MusicPlayer {
    const content::AudioCatalog* catalog{};
    std::array<MusicChannel, 4> channels{};
    std::vector<MusicEvent> events;
    std::size_t song{};
    bool playing{};

    bool play(const content::AudioCatalog& source, std::size_t song_index);
    void stop();
    void step();

    bool select_section(MusicChannel& channel);
    void advance(std::size_t channel_index);
    void apply_vibrato(MusicChannel& channel);
    void finish();
};

} // namespace tetris::audio
