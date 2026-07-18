#include "content/audio.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <string_view>

namespace tetris::content {
namespace {

constexpr std::size_t song_headers = 0x6F3F;
constexpr std::size_t song_headers_end = 0x6FFA;
constexpr std::size_t audio_end = 0x7FF0;

std::uint16_t word(const Rom& rom, std::size_t address) {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(rom.bytes[address]) |
                                     (static_cast<std::uint16_t>(rom.bytes[address + 1]) << 8U));
}

std::array<std::uint8_t, 5> registers(const Rom& rom, std::size_t address, int count) {
    std::array<std::uint8_t, 5> result{};
    for (int index = 0; index < count; ++index)
        result[static_cast<std::size_t>(index)] = rom.bytes[address + static_cast<std::size_t>(index)];
    return result;
}

SoundEffect effect(const Rom& rom, std::string_view id, SoundChannel channel,
                   std::uint8_t original_id, std::uint16_t duration,
                   std::size_t data, int register_count) {
    SoundEffect result;
    result.id = id;
    result.channel = channel;
    result.original_id = original_id;
    result.duration = duration;
    result.initial = registers(rom, data, register_count);
    result.register_count = static_cast<std::uint8_t>(register_count);
    return result;
}

void full_step(SoundEffect& sound, const Rom& rom, std::uint16_t tick, std::size_t address) {
    SoundStep step;
    step.tick = tick;
    step.registers = registers(rom, address, sound.register_count);
    for (std::size_t index = 0; index < sound.register_count; ++index)
        step.writes[index] = true;
    sound.steps.push_back(step);
}

void frequency_step(SoundEffect& sound, std::uint16_t tick, std::uint8_t frequency) {
    SoundStep step;
    step.tick = tick;
    step.registers[3] = frequency;
    step.writes[3] = true;
    sound.steps.push_back(step);
}

void extract_pulse_effects(const Rom& rom, AudioCatalog& audio) {
    SoundEffect cursor = effect(rom, "menu-cursor", SoundChannel::pulse, 1, 10, 0x659B, 5);
    full_step(cursor, rom, 5, 0x65A0);
    audio.effects.push_back(std::move(cursor));
    audio.effects.push_back(effect(rom, "change-screen", SoundChannel::pulse, 2, 3, 0x65A5, 5));

    SoundEffect rotate = effect(rom, "rotate", SoundChannel::pulse, 3, 18, 0x66EC, 5);
    for (int index = 0; index < 5; ++index) {
        SoundStep step;
        step.tick = static_cast<std::uint16_t>((index + 1) * 3);
        step.registers[2] = rom.bytes[0x66F1 + static_cast<std::size_t>(index)];
        step.registers[3] = rom.bytes[0x66F7 + static_cast<std::size_t>(index)];
        step.registers[4] = 0x87;
        step.writes[2] = step.writes[3] = step.writes[4] = true;
        rotate.steps.push_back(step);
    }
    audio.effects.push_back(std::move(rotate));
    audio.effects.push_back(effect(rom, "shift", SoundChannel::pulse, 4, 2, 0x6623, 5));
    audio.effects.push_back(effect(rom, "garbage", SoundChannel::pulse, 5, 40, 0x6740, 5));

    SoundEffect line = effect(rom, "line-clear", SoundChannel::pulse, 6, 66, 0x6695, 5);
    for (int index = 0; index < 10; ++index) {
        SoundStep step;
        step.tick = static_cast<std::uint16_t>((index + 1) * 6);
        step.registers[2] = rom.bytes[0x669A + static_cast<std::size_t>(index)];
        step.registers[3] = rom.bytes[0x66A5 + static_cast<std::size_t>(index)];
        step.registers[4] = 0x86;
        step.writes[2] = step.writes[3] = step.writes[4] = true;
        line.steps.push_back(step);
    }
    audio.effects.push_back(std::move(line));

    SoundEffect tetris = effect(rom, "tetris", SoundChannel::pulse, 7, 24, 0x65E7, 5);
    full_step(tetris, rom, 4, 0x65EC); full_step(tetris, rom, 11, 0x65E7);
    full_step(tetris, rom, 15, 0x65EC);
    audio.effects.push_back(std::move(tetris));

    SoundEffect level = effect(rom, "level-up", SoundChannel::pulse, 8, 35, 0x6640, 5);
    full_step(level, rom, 7, 0x6645); full_step(level, rom, 14, 0x664A);
    full_step(level, rom, 21, 0x664F); full_step(level, rom, 28, 0x6640);
    audio.effects.push_back(std::move(level));
}

void extract_noise_effects(const Rom& rom, AudioCatalog& audio) {
    audio.effects.push_back(effect(rom, "stack-fall", SoundChannel::noise, 1, 32, 0x6749, 4));
    audio.effects.push_back(effect(rom, "lock", SoundChannel::noise, 2, 18, 0x6745, 4));
    audio.effects.push_back(effect(rom, "ignition", SoundChannel::noise, 3, 48, 0x674D, 4));
    SoundEffect liftoff = effect(rom, "liftoff", SoundChannel::noise, 4, 1776, 0x6751, 4);
    for (int index = 0; index < 36; ++index) {
        SoundStep step;
        step.tick = static_cast<std::uint16_t>((index + 1) * 48);
        step.registers[1] = rom.bytes[0x6779 + static_cast<std::size_t>(index)];
        step.registers[2] = rom.bytes[0x6755 + static_cast<std::size_t>(index)];
        step.registers[3] = 0x80;
        step.writes[1] = step.writes[2] = step.writes[3] = true;
        liftoff.steps.push_back(step);
    }
    audio.effects.push_back(std::move(liftoff));
}

void extract_wave_effects(const Rom& rom, AudioCatalog& audio) {
    SoundEffect sweep = effect(rom, "tetris-sweep", SoundChannel::wave, 1, 32, 0x6857, 5);
    sweep.locks_music = true;
    for (std::uint16_t tick = 1; tick < 9; ++tick)
        frequency_step(sweep, tick, static_cast<std::uint8_t>(0x9D + tick * 2));
    full_step(sweep, rom, 9, 0x685C); full_step(sweep, rom, 19, 0x6861);
    for (std::uint16_t tick = 20; tick < 23; ++tick)
        frequency_step(sweep, tick, static_cast<std::uint8_t>(0x96 + (tick - 19) * 2));
    full_step(sweep, rom, 23, 0x6866);
    for (std::uint16_t tick = 24; tick < 32; ++tick)
        frequency_step(sweep, tick, static_cast<std::uint8_t>(0x95 - (tick - 23) * 2));
    audio.effects.push_back(std::move(sweep));
    SoundEffect game_over = effect(rom, "game-over", SoundChannel::wave, 2, 30, 0x67FB, 5);
    game_over.locks_music = true;
    game_over.random_pitch = true;
    audio.effects.push_back(std::move(game_over));
}

bool valid_music_address(std::uint16_t address) {
    return address == 0 || (address >= song_headers_end && address < audio_end);
}

struct DecodedSequence {
    std::uint16_t address{};
    std::vector<std::uint16_t> sections;
    std::uint16_t loop{};
    bool stops{};
};

bool decode_sequence(const Rom& rom, std::uint16_t address,
                     const std::set<std::uint16_t>& starts,
                     DecodedSequence& result, std::string& error) {
    DecodedSequence sequence;
    sequence.address = address;
    std::size_t cursor = address;
    while (cursor + 2 <= audio_end) {
        if (cursor != address && starts.contains(static_cast<std::uint16_t>(cursor)))
            break;
        const std::uint16_t value = word(rom, cursor);
        cursor += 2;
        if (value == 0) { sequence.stops = true; break; }
        if (value == 0xFFFF) {
            sequence.loop = word(rom, cursor);
            if (!valid_music_address(sequence.loop) || sequence.loop == 0) {
                error = "invalid music loop";
                return false;
            }
            break;
        }
        if (value < song_headers_end || value >= audio_end) {
            if (!sequence.sections.empty())
                break;
            error = "invalid music section pointer";
            return false;
        }
        sequence.sections.push_back(value);
    }
    result = std::move(sequence);
    return true;
}

bool decode_section(const Rom& rom, std::uint16_t address,
                    MusicSection& result, std::string& error) {
    MusicSection section;
    section.source_address = address;
    std::size_t cursor = address;
    while (cursor < audio_end) {
        MusicCommand command;
        command.opcode = rom.bytes[cursor++];
        if (command.opcode == 0) {
            section.commands.push_back(command);
            result = std::move(section);
            return true;
        }
        if (command.opcode == 0x9D) {
            if (cursor + 3 > audio_end) {
                error = "truncated music instrument";
                return false;
            }
            for (std::size_t index = 0; index < 3; ++index)
                command.parameters[index] = rom.bytes[cursor + index];
            const std::uint16_t wave_address = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(command.parameters[0]) |
                (static_cast<std::uint16_t>(command.parameters[1]) << 8U));
            if (wave_address >= 0x6EA9 && wave_address < 0x6EF9 &&
                (wave_address - 0x6EA9) % 16 == 0)
                command.wave = static_cast<std::uint8_t>((wave_address - 0x6EA9) / 16);
            cursor += 3;
        }
        section.commands.push_back(command);
    }
    error = "unterminated music section";
    return false;
}

} // namespace

bool extract_audio(const Rom& rom, AudioCatalog& result, std::string& error) {
    if (rom.bytes.size() < audio_end) {
        error = "audio range is outside ROM";
        return false;
    }
    AudioCatalog audio;
    extract_pulse_effects(rom, audio);
    extract_noise_effects(rom, audio);
    extract_wave_effects(rom, audio);
    for (std::size_t note = 0; note < audio.pause_notes.size(); ++note) {
        for (std::size_t byte = 0; byte < 4; ++byte)
            audio.pause_notes[note][byte] = rom.bytes[0x657B + note * 4 + byte];
    }
    for (std::size_t song = 0; song < audio.stereo.size(); ++song) {
        for (std::size_t byte = 0; byte < 4; ++byte)
            audio.stereo[song][byte] = rom.bytes[0x6ABE + song * 4 + byte];
    }
    for (std::size_t index = 0; index < audio.vibrato.size(); ++index)
        audio.vibrato[index] = rom.bytes[0x6DCB + index];
    for (std::size_t index = 0; index < audio.pitches.size(); ++index)
        audio.pitches[index] = word(rom, 0x6E02 + index * 2);
    for (std::size_t index = 0; index < audio.noise_notes.size(); ++index)
        audio.noise_notes[index] = rom.bytes[0x6E94 + index];
    for (std::size_t wave = 0; wave < audio.waves.size(); ++wave) {
        for (std::size_t byte = 0; byte < 16; ++byte)
            audio.waves[wave][byte] = rom.bytes[0x6EA9 + wave * 16 + byte];
    }

    constexpr std::array<std::string_view, 17> names = {
        "top-score", "stage-clear", "title", "game-over", "type-a-korobeiniki",
        "type-b", "type-c-menuet", "danger-toreadors", "versus-round-over",
        "type-b-jingle-1", "type-b-jingle-2", "type-b-jingle-3", "type-b-jingle-4",
        "type-b-jingle-5", "type-b-jingle-6", "rocket-launch", "versus-victory",
    };
    std::set<std::uint16_t> sequence_addresses;
    std::array<std::array<std::uint16_t, 4>, 17> song_sequences{};
    for (std::size_t index = 0; index < names.size(); ++index) {
        const std::size_t address = song_headers + index * 11;
        if (word(rom, 0x64B0 + index * 2) != address) {
            error = "music pointer table mismatch";
            return false;
        }
        Song song;
        song.id = names[index];
        const std::size_t timing_address = word(rom, address + 1);
        if (timing_address + song.durations.size() > audio_end) {
            error = "invalid music duration table";
            return false;
        }
        std::copy_n(rom.bytes.begin() + static_cast<std::ptrdiff_t>(timing_address),
                    static_cast<std::ptrdiff_t>(song.durations.size()), song.durations.begin());
        song.channels.fill(no_music_index);
        for (std::size_t channel = 0; channel < 4; ++channel) {
            song_sequences[index][channel] = word(rom, address + 3 + channel * 2);
            if (song_sequences[index][channel] != 0)
                sequence_addresses.insert(song_sequences[index][channel]);
        }
        audio.songs.push_back(std::move(song));
    }

    const std::set<std::uint16_t> declared = sequence_addresses;
    std::set<std::uint16_t> decoded;
    std::set<std::uint16_t> sections;
    std::vector<DecodedSequence> decoded_sequences;
    while (decoded.size() < sequence_addresses.size()) {
        const auto next = std::ranges::find_if(sequence_addresses, [&decoded](std::uint16_t address) {
            return !decoded.contains(address);
        });
        if (next == sequence_addresses.end())
            break;
        DecodedSequence sequence;
        if (!decode_sequence(rom, *next, declared, sequence, error))
            return false;
        sections.insert(sequence.sections.begin(), sequence.sections.end());
        if (sequence.loop != 0)
            sequence_addresses.insert(sequence.loop);
        decoded.insert(*next);
        decoded_sequences.push_back(std::move(sequence));
    }
    for (const std::uint16_t address : sections) {
        MusicSection section;
        if (!decode_section(rom, address, section, error))
            return false;
        audio.sections.push_back(std::move(section));
    }

    std::map<std::uint16_t, std::size_t> section_indices;
    for (std::size_t index = 0; index < audio.sections.size(); ++index)
        section_indices.emplace(audio.sections[index].source_address, index);
    std::map<std::uint16_t, std::size_t> sequence_indices;
    for (std::size_t index = 0; index < decoded_sequences.size(); ++index)
        sequence_indices.emplace(decoded_sequences[index].address, index);

    audio.sequences.resize(decoded_sequences.size());
    for (std::size_t index = 0; index < decoded_sequences.size(); ++index) {
        const DecodedSequence& decoded_sequence = decoded_sequences[index];
        MusicSequence& sequence = audio.sequences[index];
        sequence.source_address = decoded_sequence.address;
        sequence.stops = decoded_sequence.stops;
        if (decoded_sequence.loop != 0) {
            const auto loop = sequence_indices.find(decoded_sequence.loop);
            if (loop == sequence_indices.end()) {
                error = "unresolved music loop";
                return false;
            }
            sequence.loop = loop->second;
        }
        for (const std::uint16_t section_address : decoded_sequence.sections) {
            const auto section = section_indices.find(section_address);
            if (section == section_indices.end()) {
                error = "unresolved music section";
                return false;
            }
            sequence.sections.push_back(section->second);
        }
    }
    for (std::size_t song_index = 0; song_index < audio.songs.size(); ++song_index) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const std::uint16_t address = song_sequences[song_index][channel];
            if (address == 0)
                continue;
            const auto sequence = sequence_indices.find(address);
            if (sequence == sequence_indices.end()) {
                error = "unresolved song channel";
                return false;
            }
            audio.songs[song_index].channels[channel] = sequence->second;
        }
    }
    result = std::move(audio);
    return true;
}

} // namespace tetris::content
