#include "audio/music.hpp"

#include <algorithm>

namespace tetris::audio {

bool MusicPlayer::play(const content::AudioCatalog& catalog, std::size_t song) {
    if (song >= catalog.songs.size())
        return false;

    catalog_ = &catalog;
    song_ = song;
    playing_ = true;
    events_.clear();
    for (std::size_t index = 0; index < channels_.size(); ++index) {
        MusicChannel channel = channels_[index];
        channel.sequence = catalog.songs[song].channels[index];
        channel.active = channel.sequence != content::no_music_index;
        channel.section = 0;
        channel.command = 0;
        channel.timer = 1;
        if (index < 3)
            channel.vibrato_tick = 0;
        channels_[index] = channel;
    }
    return std::ranges::any_of(channels_, &MusicChannel::active);
}

void MusicPlayer::stop() {
    catalog_ = nullptr;
    channels_ = {};
    events_.clear();
    song_ = 0;
    playing_ = false;
}

void MusicPlayer::tick() {
    events_.clear();
    if (!playing_)
        return;
    for (std::size_t index = 0; index < channels_.size() && playing_; ++index)
        advance(index);
}

bool MusicPlayer::playing() const { return playing_; }
std::size_t MusicPlayer::song() const { return song_; }
std::span<const MusicChannel> MusicPlayer::channels() const { return channels_; }
std::span<const MusicEvent> MusicPlayer::events() const { return events_; }

bool MusicPlayer::select_section(MusicChannel& channel) {
    if (catalog_ == nullptr || channel.sequence >= catalog_->sequences.size()) {
        channel.active = false;
        return false;
    }
    const content::MusicSequence& sequence = catalog_->sequences[channel.sequence];
    if (channel.section < sequence.sections.size())
        return sequence.sections[channel.section] < catalog_->sections.size();
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
    MusicChannel& channel = channels_[channel_index];
    if (!channel.active)
        return;

    if (channel.timer > 0) {
        --channel.timer;
        if (channel.timer > 0) {
            if (channel_index == 2 && (channel.instrument[2] & 0x80U) != 0 &&
                channel.timer == 6)
                channel.wave_volume = 0x40;
            if (channel_index < 3) {
                apply_vibrato(channel);
                ++channel.vibrato_tick;
            }
            return;
        }
    }

    while (playing_ && channel.active) {
        if (!select_section(channel))
            return;
        const content::MusicSequence& sequence = catalog_->sequences[channel.sequence];
        const content::MusicSection& section = catalog_->sections[sequence.sections[channel.section]];
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
        if (command.opcode == 0x9D) {
            channel.instrument = command.parameters;
            if (command.wave != 0xFF)
                channel.wave = command.wave;
            events_.push_back({MusicEventType::instrument, static_cast<int>(channel_index)});
            continue;
        }
        if ((command.opcode & 0xF0U) == 0xA0U) {
            channel.duration = catalog_->songs[song_].durations[command.opcode & 0x0FU];
            if (channel.duration == 0)
                channel.duration = 1;
            continue;
        }

        channel.note = command.opcode;
        channel.resting = channel_index < 3 && command.opcode == 1;
        if (!channel.resting && channel_index < 3) {
            const std::size_t pitch = command.opcode / 2U;
            channel.period = pitch < catalog_->pitches.size() ? catalog_->pitches[pitch] : 0;
            channel.output_period = channel.period;
            if (channel_index == 2)
                channel.wave_volume = channel.instrument[2];
            channel.registers = {channel.instrument[0], channel.instrument[1],
                                 channel.instrument[2],
                                 static_cast<std::uint8_t>(channel.period & 0xFFU),
                                 static_cast<std::uint8_t>(channel.period >> 8U)};
        } else if (channel_index == 3) {
            const std::size_t begin = command.opcode;
            if (begin + channel.registers.size() <= catalog_->noise_notes.size())
                std::copy_n(catalog_->noise_notes.begin() + static_cast<std::ptrdiff_t>(begin),
                            static_cast<std::ptrdiff_t>(channel.registers.size()),
                            channel.registers.begin());
        }
        channel.timer = channel.duration;
        channel.vibrato_tick = channel_index < 3 ? 1 : 0;
        events_.push_back({channel.resting ? MusicEventType::rest : MusicEventType::note,
                           static_cast<int>(channel_index)});
        return;
    }
}

void MusicPlayer::apply_vibrato(MusicChannel& channel) {
    channel.output_period = channel.period;
    if (catalog_ == nullptr || channel.resting || (channel.instrument[2] & 0x0FU) == 0)
        return;
    const std::size_t index = channel.vibrato_tick / 2U;
    if (index >= catalog_->vibrato.size())
        return;
    std::uint8_t packed = catalog_->vibrato[index];
    if ((channel.vibrato_tick & 1U) == 0)
        packed = static_cast<std::uint8_t>(packed >> 4U);
    const std::uint8_t nibble = packed & 0x0FU;
    const int offset = (nibble & 8U) != 0 ? static_cast<int>(nibble) - 16
                                          : static_cast<int>(nibble);
    channel.output_period = static_cast<std::uint16_t>(static_cast<int>(channel.period) + offset);
}

void MusicPlayer::finish() {
    playing_ = false;
    for (MusicChannel& channel : channels_)
        channel.active = false;
    events_.push_back({MusicEventType::stopped, 0});
}

} // namespace tetris::audio
