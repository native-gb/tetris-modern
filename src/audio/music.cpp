#include "audio/music.hpp"

#include <algorithm>

namespace tetris::audio {

bool MusicPlayer::play(const content::AudioCatalog& source, std::size_t song_index) {
    if (song_index >= source.songs.size())
        return false;

    catalog = &source;
    song = song_index;
    playing = true;
    events.clear();

    // Reset each driver channel to the song header's first sequence.
    for (std::size_t index = 0; index < channels.size(); ++index) {
        MusicChannel channel = channels[index];
        channel.sequence = source.songs[song_index].channels[index];
        channel.active = channel.sequence != content::no_music_index;
        channel.section = 0;
        channel.command = 0;
        channel.timer = 1;
        if (index < 3)
            channel.vibrato_step = 0;
        channels[index] = channel;
    }
    return std::ranges::any_of(channels, &MusicChannel::active);
}

void MusicPlayer::stop() {
    catalog = nullptr;
    channels = {};
    events.clear();
    song = 0;
    playing = false;
}

void MusicPlayer::step() {
    events.clear();
    if (!playing)
        return;
    for (std::size_t index = 0; index < channels.size() && playing; ++index)
        advance(index);
}

bool MusicPlayer::select_section(MusicChannel& channel) {
    if (catalog == nullptr || channel.sequence >= catalog->sequences.size()) {
        channel.active = false;
        return false;
    }
    const content::MusicSequence& sequence = catalog->sequences[channel.sequence];
    if (channel.section < sequence.sections.size())
        return sequence.sections[channel.section] < catalog->sections.size();

    // Reaching the sequence end either stops playback or follows its loop.
    if (sequence.stops) {
        finish();
        return false;
    }
    if (sequence.loop != content::no_music_index) {
        channel.sequence = sequence.loop;
        channel.section = 0;
        channel.command = 0;
        return select_section(channel);
    }
    channel.active = false;
    return false;
}

void MusicPlayer::advance(std::size_t channel_index) {
    if (channel_index >= channels.size())
        return;
    MusicChannel& channel = channels[channel_index];
    if (!channel.active)
        return;

    // Hold the current note while applying wave decay and pulse vibrato.
    if (channel.timer > 0) {
        --channel.timer;
        if (channel.timer > 0) {
            if (channel_index == 2 && (channel.instrument[2] & 0x80U) != 0 && channel.timer == 6)
                channel.wave_volume = 0x40;
            if (channel_index < 3) {
                apply_vibrato(channel);
                ++channel.vibrato_step;
            }
            return;
        }
    }

    // Consume zero-time driver commands until a note or rest begins.
    while (playing && channel.active) {
        if (!select_section(channel))
            return;
        const content::MusicSequence& sequence = catalog->sequences[channel.sequence];
        const content::MusicSection& section =
            catalog->sections[sequence.sections[channel.section]];
        if (channel.command >= section.commands.size()) {
            ++channel.section;
            channel.command = 0;
            continue;
        }

        const content::MusicCommand command = section.commands[channel.command++];
        if (command.opcode == 0) {
            ++channel.section;
            channel.command = 0;
            continue;
        }

        // Instrument commands update registers without consuming a step.
        if (command.opcode == 0x9D) {
            channel.instrument = command.parameters;
            if (command.wave != 0xFF)
                channel.wave = command.wave;
            events.push_back({MusicEventType::instrument, static_cast<int>(channel_index)});
            continue;
        }

        // Duration commands select an entry from the song timing table.
        if ((command.opcode & 0xF0U) == 0xA0U) {
            channel.duration = catalog->songs[song].durations[command.opcode & 0x0FU];
            if (channel.duration == 0)
                channel.duration = 1;
            continue;
        }

        // Every remaining opcode starts a pitched note, rest, or noise note.
        channel.note = command.opcode;
        channel.resting = channel_index < 3 && command.opcode == 1;
        if (!channel.resting && channel_index < 3) {
            const std::size_t pitch = command.opcode / 2U;
            channel.period = pitch < catalog->pitches.size() ? catalog->pitches[pitch] : 0;
            channel.output_period = channel.period;
            if (channel_index == 2)
                channel.wave_volume = channel.instrument[2];
            channel.registers = {channel.instrument[0], channel.instrument[1],
                                 channel.instrument[2],
                                 static_cast<std::uint8_t>(channel.period & 0xFFU),
                                 static_cast<std::uint8_t>(channel.period >> 8U)};
        } else if (channel_index == 3) {
            const std::size_t begin = command.opcode;
            if (begin + channel.registers.size() <= catalog->noise_notes.size())
                std::copy_n(catalog->noise_notes.begin() + static_cast<std::ptrdiff_t>(begin),
                            static_cast<std::ptrdiff_t>(channel.registers.size()),
                            channel.registers.begin());
        }
        channel.timer = channel.duration;
        channel.vibrato_step = channel_index < 3 ? 1 : 0;
        events.push_back({channel.resting ? MusicEventType::rest : MusicEventType::note,
                          static_cast<int>(channel_index)});
        return;
    }
}

void MusicPlayer::apply_vibrato(MusicChannel& channel) {
    channel.output_period = channel.period;
    if (catalog == nullptr || channel.resting || (channel.instrument[2] & 0x0FU) == 0)
        return;
    const std::size_t index = channel.vibrato_step / 2U;
    if (index >= catalog->vibrato.size())
        return;
    std::uint8_t packed = catalog->vibrato[index];
    if ((channel.vibrato_step & 1U) == 0)
        packed = static_cast<std::uint8_t>(packed >> 4U);
    const std::uint8_t nibble = packed & 0x0FU;
    const int offset =
        (nibble & 8U) != 0 ? static_cast<int>(nibble) - 16 : static_cast<int>(nibble);
    channel.output_period = static_cast<std::uint16_t>(static_cast<int>(channel.period) + offset);
}

void MusicPlayer::finish() {
    playing = false;
    for (MusicChannel& channel : channels)
        channel.active = false;
    events.push_back({MusicEventType::stopped, 0});
}

} // namespace tetris::audio
