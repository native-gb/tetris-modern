#include "audio/output.hpp"

#include <algorithm>
#include <cmath>

namespace tetris::audio {
namespace {

std::size_t voice_for(content::SoundChannel channel) {
    switch (channel) {
    case content::SoundChannel::pulse: return 0;
    case content::SoundChannel::wave: return 2;
    case content::SoundChannel::noise: return 3;
    }
    return 0;
}

std::array<std::uint8_t, 5> music_registers(std::size_t index,
                                            const MusicChannel& channel) {
    if (index < 2) {
        return {0, channel.instrument[2], channel.instrument[0],
                static_cast<std::uint8_t>(channel.output_period & 0xFFU),
                static_cast<std::uint8_t>((channel.output_period >> 8U) | 0x80U)};
    }
    if (index == 2) {
        return {0x80, 0, channel.wave_volume,
                static_cast<std::uint8_t>(channel.output_period & 0xFFU),
                static_cast<std::uint8_t>((channel.output_period >> 8U) | 0x80U)};
    }
    return {channel.registers[1], channel.registers[0], channel.registers[3],
            channel.registers[4], 0};
}

} // namespace

bool Output::initialize(const content::AudioCatalog& catalog) {
    shutdown();
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
            return false;
        owns_audio_ = true;
    }
    const SDL_AudioSpec specification{SDL_AUDIO_F32, 2, sample_rate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                        &specification, nullptr, nullptr);
    if (stream_ == nullptr || !SDL_ResumeAudioStreamDevice(stream_)) {
        shutdown();
        return false;
    }
    catalog_ = &catalog;
    director_.attach(catalog);
    music_synth_.reset(sample_rate);
    sound_synth_.reset(sample_rate);
    music_samples_.reserve(2048);
    sound_samples_.reserve(2048);
    mixed_samples_.reserve(2048);
    return true;
}

void Output::shutdown() {
    if (stream_ != nullptr)
        SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
    if (owns_audio_)
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    owns_audio_ = false;
    catalog_ = nullptr;
    director_.reset();
    music_state_ = {};
    sound_state_ = {};
    music_triggers_ = {};
    sound_triggers_ = {};
    sample_fraction_ = 0.0;
    previous_pause_ = false;
    previous_pause_note_ = 0;
}

void Output::reset() {
    if (catalog_ != nullptr)
        director_.attach(*catalog_);
    if (stream_ != nullptr)
        (void)SDL_ClearAudioStream(stream_);
    music_synth_.reset(sample_rate);
    sound_synth_.reset(sample_rate);
    music_state_ = {};
    sound_state_ = {};
    music_triggers_ = {};
    sound_triggers_ = {};
    sample_fraction_ = 0.0;
    previous_pause_ = false;
    previous_pause_note_ = 0;
}

void Output::tick(const GameFlow& flow, std::uint8_t random_sample, bool submit) {
    if (catalog_ == nullptr || stream_ == nullptr)
        return;
    director_.tick(flow, random_sample);
    if (director_.paused() && (!previous_pause_ || director_.pause_note() != previous_pause_note_))
        ++sound_triggers_[0];
    previous_pause_ = director_.paused();
    previous_pause_note_ = director_.pause_note();
    update_states();
    render(submit);
}

void Output::set_volume(float music, float effects) {
    music_volume_ = std::clamp(music, 0.0F, 1.0F);
    effects_volume_ = std::clamp(effects, 0.0F, 1.0F);
}

bool Output::available() const { return stream_ != nullptr; }
int Output::queued_bytes() const { return stream_ == nullptr ? 0 : SDL_GetAudioStreamQueued(stream_); }
const Director& Output::director() const { return director_; }
Director& Output::debug_director() { return director_; }

std::array<float, 4> Output::music_meters() const {
    return {music_synth_.channel_meter(gb_audio::Channel::Pulse1),
            music_synth_.channel_meter(gb_audio::Channel::Pulse2),
            music_synth_.channel_meter(gb_audio::Channel::Wave),
            music_synth_.channel_meter(gb_audio::Channel::Noise)};
}

std::array<float, 4> Output::sound_meters() const {
    return {sound_synth_.channel_meter(gb_audio::Channel::Pulse1),
            sound_synth_.channel_meter(gb_audio::Channel::Pulse2),
            sound_synth_.channel_meter(gb_audio::Channel::Wave),
            sound_synth_.channel_meter(gb_audio::Channel::Noise)};
}

void Output::update_states() {
    for (const MusicEvent& event : director_.music().events())
        apply_music_event(event);
    for (const SoundEvent& event : director_.sounds().events())
        apply_sound_event(event);
    update_music_state();
    update_sound_state();
}

void Output::update_music_state() {
    music_state_.stereo_mask = director_.stereo_mask();
    const auto channels = director_.music().channels();
    for (std::size_t index = 0; index < channels.size(); ++index) {
        const MusicChannel& channel = channels[index];
        gb_audio::Voice& voice = music_state_.voices[index];
        voice.active = director_.music().playing() && channel.active && !channel.resting &&
                       !director_.paused();
        voice.registers = music_registers(index, channel);
        voice.trigger_id = music_triggers_[index];
        if (index == 2 && catalog_ != nullptr)
            voice.wave_ram = catalog_->waves[channel.wave];
    }
    if (director_.sounds().locks_music()) {
        for (gb_audio::Voice& voice : music_state_.voices)
            voice.active = false;
    } else {
        if (director_.sounds().channel(content::SoundChannel::pulse).active())
            music_state_.voices[0].active = false;
        if (director_.sounds().channel(content::SoundChannel::noise).active())
            music_state_.voices[3].active = false;
    }
}

void Output::update_sound_state() {
    sound_state_.stereo_mask = director_.stereo_mask();
    for (gb_audio::Voice& voice : sound_state_.voices)
        voice.active = false;
    if (director_.paused() && catalog_ != nullptr && director_.pause_note_age() < 4) {
        const auto note = catalog_->pause_notes[static_cast<std::size_t>(director_.pause_note())];
        gb_audio::Voice& pulse = sound_state_.voices[0];
        pulse.active = true;
        pulse.registers = {0, note[0], note[1], note[2],
                           static_cast<std::uint8_t>(note[3] | 0x80U)};
        pulse.trigger_id = sound_triggers_[0];
        return;
    }
    for (const content::SoundChannel channel : {content::SoundChannel::pulse,
                                                 content::SoundChannel::wave,
                                                 content::SoundChannel::noise}) {
        const SoundChannelState& state = director_.sounds().channel(channel);
        if (!state.active())
            continue;
        const std::size_t voice_index = voice_for(channel);
        gb_audio::Voice& voice = sound_state_.voices[voice_index];
        voice.active = true;
        voice.registers = state.registers;
        voice.trigger_id = sound_triggers_[voice_index];
        if (channel == content::SoundChannel::wave && catalog_ != nullptr) {
            const std::size_t wave = state.effect->original_id == 2 ? 3U : 0U;
            voice.wave_ram = catalog_->waves[wave];
        }
    }
}

void Output::render(bool submit) {
    sample_fraction_ += static_cast<double>(sample_rate) / ticks_per_second;
    const int frames = static_cast<int>(sample_fraction_);
    sample_fraction_ -= static_cast<double>(frames);
    const std::size_t samples = static_cast<std::size_t>(frames) * 2U;
    music_samples_.assign(samples, 0.0F);
    sound_samples_.assign(samples, 0.0F);
    mixed_samples_.resize(samples);
    music_synth_.render(music_state_, music_samples_);
    sound_synth_.render(sound_state_, sound_samples_);
    for (std::size_t index = 0; index < samples; ++index) {
        mixed_samples_[index] = std::clamp(music_samples_[index] * music_volume_ +
                                           sound_samples_[index] * effects_volume_,
                                           -1.0F, 1.0F);
    }
    if (submit) {
        (void)SDL_PutAudioStreamData(stream_, mixed_samples_.data(),
                                     static_cast<int>(samples * sizeof(float)));
    }
}

void Output::apply_music_event(const MusicEvent& event) {
    if (event.channel < 0 || event.channel >= 4)
        return;
    const std::size_t index = static_cast<std::size_t>(event.channel);
    if (event.type == MusicEventType::note)
        ++music_triggers_[index];
}

void Output::apply_sound_event(const SoundEvent& event) {
    const std::size_t index = voice_for(event.channel);
    if (event.type == SoundEventType::started || event.triggered)
        ++sound_triggers_[index];
}

} // namespace tetris::audio
