#pragma once

#include "content/provenance.hpp"
#include "content/rom.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace tetris::content {

enum class SoundChannel { pulse, wave, noise };

struct SoundStep {
    std::uint16_t tick{};
    std::array<std::uint8_t, 5> registers{};
    std::array<bool, 5> writes{};
};

struct SoundEffect {
    Provenance source;
    std::string id;
    SoundChannel channel{};
    std::uint8_t original_id{};
    std::uint16_t duration{};
    bool locks_music{};
    bool random_pitch{};
    std::array<std::uint8_t, 5> initial{};
    std::uint8_t register_count{};
    std::vector<SoundStep> steps;
};

struct Song {
    Provenance source;
    std::string id;
    std::array<std::uint8_t, 16> durations{};
    std::array<std::size_t, 4> channels{};
};

constexpr std::size_t no_music_index = std::numeric_limits<std::size_t>::max();

struct MusicCommand {
    std::uint8_t opcode{};
    std::array<std::uint8_t, 3> parameters{};
    std::uint8_t wave{0xFF};
};

struct MusicSection {
    Provenance source;
    std::uint16_t source_address{};
    std::vector<MusicCommand> commands;
};

struct MusicSequence {
    Provenance source;
    std::uint16_t source_address{};
    std::vector<std::size_t> sections;
    std::size_t loop{no_music_index};
    bool stops{};
};

struct AudioCatalog {
    Provenance source;
    std::vector<std::uint8_t> source_bytes;
    std::vector<Song> songs;
    std::vector<MusicSequence> sequences;
    std::vector<MusicSection> sections;
    std::vector<SoundEffect> effects;
    std::array<std::array<std::uint8_t, 16>, 5> waves{};
    std::array<std::array<std::uint8_t, 4>, 2> pause_notes{};
    std::array<std::array<std::uint8_t, 4>, 17> stereo{};
    std::array<std::uint8_t, 55> vibrato{};
    std::array<std::uint16_t, 73> pitches{};
    std::array<std::uint8_t, 21> noise_notes{};
    Provenance pause_notes_source;
    Provenance stereo_source;
    Provenance vibrato_source;
    Provenance pitches_source;
    Provenance noise_notes_source;
    Provenance waves_source;
};

bool extract_audio(const Rom& rom, AudioCatalog& result, std::string& error);

} // namespace tetris::content
