#include "audio/sound_effects.hpp"

#include <algorithm>

namespace tetris::audio {

void SoundEffects::attach(const content::AudioCatalog& catalog) {
    stop();
    catalog_ = &catalog;
}

bool SoundEffects::play(content::SoundChannel channel, std::uint8_t effect,
                        std::uint8_t random_sample) {
    const content::SoundEffect* definition = find(channel, effect);
    if (definition == nullptr)
        return false;
    if (channel == content::SoundChannel::pulse && pulse_is_blocked(effect)) {
        events_.push_back({SoundEventType::rejected, channel, effect});
        return false;
    }

    SoundChannelState state;
    state.effect = definition;
    state.registers = definition->initial;
    if (definition->random_pitch)
        state.random_pitch = static_cast<std::uint8_t>(0xD0U + (random_sample & 0x1FU));
    channels_[index(channel)] = state;
    events_.push_back({SoundEventType::started, channel, effect, 0, true});
    return true;
}

void SoundEffects::tick(std::uint8_t random_sample) {
    events_.clear();
    advance(content::SoundChannel::pulse, random_sample);
    advance(content::SoundChannel::noise, random_sample);
    advance(content::SoundChannel::wave, random_sample);
}

void SoundEffects::stop() {
    channels_ = {};
    events_.clear();
}

const SoundChannelState& SoundEffects::channel(content::SoundChannel channel) const {
    return channels_[index(channel)];
}

std::span<const SoundEvent> SoundEffects::events() const { return events_; }

bool SoundEffects::locks_music() const {
    const SoundChannelState& wave = channel(content::SoundChannel::wave);
    return wave.active() && wave.effect->locks_music;
}

std::size_t SoundEffects::index(content::SoundChannel channel) {
    switch (channel) {
    case content::SoundChannel::pulse: return 0;
    case content::SoundChannel::wave: return 1;
    case content::SoundChannel::noise: return 2;
    }
    return 0;
}

const content::SoundEffect* SoundEffects::find(content::SoundChannel channel,
                                               std::uint8_t effect) const {
    if (catalog_ == nullptr)
        return nullptr;
    const auto found = std::ranges::find_if(catalog_->effects, [channel, effect](const auto& entry) {
        return entry.channel == channel && entry.original_id == effect;
    });
    return found == catalog_->effects.end() ? nullptr : &*found;
}

bool SoundEffects::pulse_is_blocked(std::uint8_t effect) const {
    const SoundChannelState& pulse = channel(content::SoundChannel::pulse);
    const SoundChannelState& wave = channel(content::SoundChannel::wave);
    const std::uint8_t current = pulse.active() ? pulse.effect->original_id : 0;
    const bool sweep = wave.active() && wave.effect->original_id == 1;
    if (effect == 3 || effect == 4)
        return sweep || current == 5 || current == 7 || current == 8;
    if (effect == 6)
        return sweep || current == 7 || current == 8;
    if (effect == 5 || effect == 8)
        return current == 7;
    return false;
}

void SoundEffects::advance(content::SoundChannel sound_channel,
                           std::uint8_t random_sample) {
    SoundChannelState& state = channels_[index(sound_channel)];
    if (!state.active())
        return;
    ++state.elapsed;
    if (state.elapsed >= state.effect->duration) {
        const bool start_sweep = sound_channel == content::SoundChannel::pulse &&
                                 state.effect->original_id == 7;
        stop(sound_channel);
        if (start_sweep)
            (void)play(content::SoundChannel::wave, 1, random_sample);
        return;
    }

    if (state.effect->random_pitch) {
        if (state.elapsed < 14)
            state.random_pitch = static_cast<std::uint8_t>(state.random_pitch + 2U);
        else
            state.random_pitch = static_cast<std::uint8_t>(state.random_pitch - 3U);
        state.registers[3] = static_cast<std::uint8_t>((state.random_pitch & 0xF0U) |
                                                       (random_sample & 0x0FU));
        events_.push_back({SoundEventType::changed, sound_channel,
                           state.effect->original_id, state.elapsed});
        return;
    }

    for (const content::SoundStep& step : state.effect->steps) {
        if (step.tick != state.elapsed)
            continue;
        for (std::size_t register_index = 0; register_index < state.registers.size();
             ++register_index) {
            if (step.writes[register_index])
                state.registers[register_index] = step.registers[register_index];
        }
        events_.push_back({SoundEventType::changed, sound_channel,
                           state.effect->original_id, state.elapsed,
                           step.writes[4] && (step.registers[4] & 0x80U) != 0});
    }
}

void SoundEffects::stop(content::SoundChannel sound_channel) {
    SoundChannelState& state = channels_[index(sound_channel)];
    if (!state.active())
        return;
    events_.push_back({SoundEventType::stopped, sound_channel,
                       state.effect->original_id, state.elapsed});
    state = {};
}

} // namespace tetris::audio
