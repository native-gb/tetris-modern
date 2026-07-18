#pragma once

#include "content/audio.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
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
    std::uint8_t vibrato_tick{};
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

class MusicPlayer {
public:
    bool play(const content::AudioCatalog& catalog, std::size_t song);
    void stop();
    void tick();

    bool playing() const;
    std::size_t song() const;
    std::span<const MusicChannel> channels() const;
    std::span<const MusicEvent> events() const;

private:
    bool select_section(MusicChannel& channel);
    void advance(std::size_t channel);
    void apply_vibrato(MusicChannel& channel);
    void finish();

    const content::AudioCatalog* catalog_{};
    std::array<MusicChannel, 4> channels_{};
    std::vector<MusicEvent> events_;
    std::size_t song_{};
    bool playing_{};
};

} // namespace tetris::audio
