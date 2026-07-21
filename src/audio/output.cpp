#include "audio/output.hpp"

#include <algorithm>
#include <cmath>

namespace tetris::audio {
namespace {

std::size_t voice_for(content::SoundChannel channel) {
    switch (channel) {
    case content::SoundChannel::pulse:
        return 0;
    case content::SoundChannel::wave:
        return 2;
    case content::SoundChannel::noise:
        return 3;
    }
    return 0;
}

std::array<std::uint8_t, 5> music_registers(std::size_t index, const MusicChannel& channel) {
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
    return {channel.registers[1], channel.registers[0], channel.registers[3], channel.registers[4],
            0};
}

} // namespace

bool Output::initialize(const content::AudioCatalog& catalog) {
    // Initialize SDL audio only when the surrounding program has not done so.
    shutdown();
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
            return false;
        owns_audio_ = true;
    }

    // Open a stereo floating-point stream at the synth sample rate.
    const SDL_AudioSpec specification{SDL_AUDIO_F32, 2, sample_rate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &specification, nullptr,
                                        nullptr);
    if (stream_ == nullptr || !SDL_ResumeAudioStreamDevice(stream_)) {
        shutdown();
        return false;
    }

    // Attach decoded audio and reset both four-voice synths.
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
    // Release the SDL stream and any audio subsystem ownership.
    if (stream_ != nullptr)
        SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
    if (owns_audio_)
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    owns_audio_ = false;

    // Clear native driver and synth-facing state.
    catalog_ = nullptr;
    director_.reset();
    music_state_ = {};
    sound_state_ = {};
    clear_polyphonic_effects();
    music_triggers_ = {};
    sound_triggers_ = {};
    sample_fraction_ = 0.0;
    previous_pause_ = false;
    previous_pause_note_ = 0;
    polyphonic_ = false;
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
    clear_polyphonic_effects();
    music_triggers_ = {};
    sound_triggers_ = {};
    sample_fraction_ = 0.0;
    previous_pause_ = false;
    previous_pause_note_ = 0;
}

void Output::step(const GameState& state, std::uint8_t random_sample, bool submit) {
    if (catalog_ == nullptr || stream_ == nullptr)
        return;
    if (polyphonic_)
        advance_polyphonic_effects(random_sample);

    // Advance the native audio driver and detect pause-note triggers.
    director_.step(state, random_sample);
    if (polyphonic_ && director_.paused && !previous_pause_)
        clear_polyphonic_effects();
    if (director_.paused && (!previous_pause_ || director_.pause_note != previous_pause_note_))
        ++sound_triggers_[0];
    previous_pause_ = director_.paused;
    previous_pause_note_ = director_.pause_note;

    // Translate driver state into synth voices and render one game step.
    update_states(random_sample);
    render(submit);
}

void Output::set_volume(float music, float effects) {
    music_volume_ = std::clamp(music, 0.0F, 1.0F);
    effects_volume_ = std::clamp(effects, 0.0F, 1.0F);
}

void Output::set_polyphonic(bool enabled) {
    if (polyphonic_ == enabled)
        return;
    director_.sounds.stop();
    clear_polyphonic_effects();
    polyphonic_ = enabled;
}

bool Output::resume() {
    return stream_ != nullptr && SDL_ResumeAudioStreamDevice(stream_);
}

bool Output::available() const {
    return stream_ != nullptr;
}
int Output::queued_bytes() const {
    return stream_ == nullptr ? 0 : SDL_GetAudioStreamQueued(stream_);
}
const Director& Output::director() const {
    return director_;
}
Director& Output::director() {
    return director_;
}

std::array<float, 4> Output::music_meters() const {
    return {music_synth_.channel_meter(gb_audio::Channel::Pulse1),
            music_synth_.channel_meter(gb_audio::Channel::Pulse2),
            music_synth_.channel_meter(gb_audio::Channel::Wave),
            music_synth_.channel_meter(gb_audio::Channel::Noise)};
}

std::array<float, 4> Output::sound_meters() const {
    std::array meters = {sound_synth_.channel_meter(gb_audio::Channel::Pulse1),
                         sound_synth_.channel_meter(gb_audio::Channel::Pulse2),
                         sound_synth_.channel_meter(gb_audio::Channel::Wave),
                         sound_synth_.channel_meter(gb_audio::Channel::Noise)};
    for (const PolyphonicVoice& voice : polyphonic_voices_) {
        if (!voice.active)
            continue;
        meters[0] = std::max(meters[0], voice.synth.channel_meter(gb_audio::Channel::Pulse1));
        meters[1] = std::max(meters[1], voice.synth.channel_meter(gb_audio::Channel::Pulse2));
        meters[2] = std::max(meters[2], voice.synth.channel_meter(gb_audio::Channel::Wave));
        meters[3] = std::max(meters[3], voice.synth.channel_meter(gb_audio::Channel::Noise));
    }
    return meters;
}

std::size_t Output::polyphonic_voice_count() const {
    return static_cast<std::size_t>(
        std::ranges::count_if(polyphonic_voices_, [](const PolyphonicVoice& voice) {
            return voice.active;
        }));
}

void Output::update_states(std::uint8_t random_sample) {
    for (const MusicEvent& event : director_.music.events)
        apply_music_event(event);
    for (const SoundEvent& event : director_.sounds.events) {
        if (polyphonic_ &&
            (event.type == SoundEventType::started || event.type == SoundEventType::rejected)) {
            start_polyphonic_effect(event, random_sample);
        } else if (!polyphonic_) {
            apply_sound_event(event);
        }
    }
    update_music_state();
    update_sound_state();
}

void Output::update_music_state() {
    // Copy each active music-driver channel into one synth voice.
    music_state_.stereo_mask = director_.stereo_mask;
    const auto channels = director_.music.channels;
    for (std::size_t index = 0; index < channels.size(); ++index) {
        const MusicChannel& channel = channels[index];
        gb_audio::Voice& voice = music_state_.voices[index];
        voice.active =
            director_.music.playing && channel.active && !channel.resting && !director_.paused;
        voice.registers = music_registers(index, channel);
        voice.trigger_id = music_triggers_[index];
        if (index == 2 && catalog_ != nullptr)
            voice.wave_ram = catalog_->waves[channel.wave];
    }

    // Original mode reproduces Game Boy effect ownership of music channels.
    if (polyphonic_) {
        return;
    } else if (director_.sounds.locks_music()) {
        for (gb_audio::Voice& voice : music_state_.voices)
            voice.active = false;
    } else {
        if (director_.sounds.channel(content::SoundChannel::pulse).active())
            music_state_.voices[0].active = false;
        if (director_.sounds.channel(content::SoundChannel::noise).active())
            music_state_.voices[3].active = false;
    }
}

void Output::update_sound_state() {
    // Effects start from silence on every driver step.
    sound_state_.stereo_mask = director_.stereo_mask;
    for (gb_audio::Voice& voice : sound_state_.voices)
        voice.active = false;

    // Pausing temporarily owns pulse voice one.
    if (director_.paused && catalog_ != nullptr && director_.pause_note_age < 4) {
        const auto note = catalog_->pause_notes[static_cast<std::size_t>(director_.pause_note)];
        gb_audio::Voice& pulse = sound_state_.voices[0];
        pulse.active = true;
        pulse.registers = {0, note[0], note[1], note[2],
                           static_cast<std::uint8_t>(note[3] | 0x80U)};
        pulse.trigger_id = sound_triggers_[0];
        return;
    }
    if (polyphonic_)
        return;

    // Copy each active effect channel into its matching synth voice.
    for (const content::SoundChannel channel :
         {content::SoundChannel::pulse, content::SoundChannel::wave,
          content::SoundChannel::noise}) {
        const SoundChannelState& state = director_.sounds.channel(channel);
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
    // Convert the fractional samples-per-step rate into an integer frame count.
    sample_fraction_ += static_cast<double>(sample_rate) / steps_per_second;
    const int frames = static_cast<int>(sample_fraction_);
    sample_fraction_ -= static_cast<double>(frames);
    const std::size_t samples = static_cast<std::size_t>(frames) * 2U;
    music_samples_.assign(samples, 0.0F);
    sound_samples_.assign(samples, 0.0F);
    mixed_samples_.resize(samples);

    // Render music and effects separately so their volumes remain independent.
    music_synth_.render(music_state_, music_samples_);
    sound_synth_.render(sound_state_, sound_samples_);

    // Enhanced mode mixes every requested effect as an independent native voice.
    const std::size_t active_effects = polyphonic_voice_count();
    const float polyphonic_gain =
        active_effects == 0 ? 0.0F : 1.0F / std::sqrt(static_cast<float>(active_effects));
    for (PolyphonicVoice& voice : polyphonic_voices_) {
        if (!voice.active)
            continue;
        voice.samples.assign(samples, 0.0F);
        voice.synth.render(voice.state, voice.samples);
    }
    for (std::size_t index = 0; index < samples; ++index) {
        float effects_sample = sound_samples_[index];
        for (const PolyphonicVoice& voice : polyphonic_voices_) {
            if (voice.active)
                effects_sample += voice.samples[index] * polyphonic_gain;
        }
        mixed_samples_[index] = std::clamp(music_samples_[index] * music_volume_ +
                                               effects_sample * effects_volume_,
                                           -1.0F, 1.0F);
    }

    // Tests may render without submitting samples to the host device.
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

void Output::start_polyphonic_effect(const SoundEvent& event, std::uint8_t random_sample) {
    if (catalog_ == nullptr)
        return;
    auto slot = std::ranges::find_if(polyphonic_voices_, [](const PolyphonicVoice& voice) {
        return !voice.active;
    });
    if (slot == polyphonic_voices_.end()) {
        slot = polyphonic_voices_.begin() +
               static_cast<std::ptrdiff_t>(next_polyphonic_voice_);
        next_polyphonic_voice_ = (next_polyphonic_voice_ + 1U) % polyphonic_voices_.size();
    }

    slot->active = false;
    slot->sounds.attach(*catalog_);
    slot->state = {};
    slot->triggers = {};
    slot->samples.clear();
    slot->synth.reset(sample_rate);
    if (!slot->sounds.play(event.channel, event.effect, random_sample))
        return;
    ++slot->triggers[voice_for(event.channel)];
    slot->active = true;
    update_polyphonic_state(*slot);
}

void Output::advance_polyphonic_effects(std::uint8_t random_sample) {
    for (PolyphonicVoice& voice : polyphonic_voices_) {
        if (!voice.active)
            continue;
        voice.sounds.step(random_sample);
        for (const SoundEvent& event : voice.sounds.events) {
            if (event.type == SoundEventType::started || event.triggered)
                ++voice.triggers[voice_for(event.channel)];
        }
        const bool active = voice.sounds.channel(content::SoundChannel::pulse).active() ||
                            voice.sounds.channel(content::SoundChannel::wave).active() ||
                            voice.sounds.channel(content::SoundChannel::noise).active();
        if (!active) {
            voice.active = false;
            continue;
        }
        update_polyphonic_state(voice);
    }
}

void Output::clear_polyphonic_effects() {
    for (PolyphonicVoice& voice : polyphonic_voices_) {
        voice.active = false;
        voice.sounds.stop();
        voice.state = {};
        voice.triggers = {};
        voice.samples.clear();
    }
    next_polyphonic_voice_ = 0;
}

void Output::update_polyphonic_state(PolyphonicVoice& voice) {
    voice.state = {};
    voice.state.stereo_mask = director_.stereo_mask;
    for (const content::SoundChannel channel :
         {content::SoundChannel::pulse, content::SoundChannel::wave,
          content::SoundChannel::noise}) {
        const SoundChannelState& effect = voice.sounds.channel(channel);
        if (!effect.active())
            continue;
        const std::size_t index = voice_for(channel);
        gb_audio::Voice& output = voice.state.voices[index];
        output.active = true;
        output.registers = effect.registers;
        output.trigger_id = voice.triggers[index];
        if (channel == content::SoundChannel::wave && catalog_ != nullptr) {
            const std::size_t wave = effect.effect->original_id == 2 ? 3U : 0U;
            output.wave_ram = catalog_->waves[wave];
        }
    }
}

} // namespace tetris::audio
