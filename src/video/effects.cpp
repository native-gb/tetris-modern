#include "video/effects.hpp"

#include <algorithm>

namespace tetris::video {
namespace {

std::uint32_t mix(std::uint32_t value) {
    value ^= value >> 16U;
    value *= 0x7FEB352DU;
    value ^= value >> 15U;
    value *= 0x846CA68BU;
    return value ^ (value >> 16U);
}

float sample(std::uint32_t value) {
    return static_cast<float>(mix(value) & 0xFFFFU) / 32767.5F - 1.0F;
}

void shake(EffectState& state, float strength, int duration) {
    state.shake_strength = std::max(state.shake_strength, strength);
    state.shake_steps = std::max(state.shake_steps, duration);
    state.shake_duration = std::max(state.shake_duration, duration);
    ++state.sequence;
}

void react(EffectState& state, std::span<const Event> events, const settings::Settings& settings) {
    // Convert gameplay events into short host-side shake and flash envelopes.
    const bool full = settings.effects == settings::Intensity::full;
    for (const Event event : events) {
        if (event.type == GameEvent::landed && settings.screen_shake)
            shake(state, full ? 1.6F : 0.75F, full ? 7 : 5);
        if (event.type == GameEvent::cleared_lines && settings.screen_shake)
            shake(state, event.value == 4 ? (full ? 4.0F : 2.0F) : (full ? 2.0F : 1.0F),
                  event.value == 4 ? 12 : 8);
        if (event.type == GameEvent::game_over && settings.screen_shake)
            shake(state, full ? 4.5F : 2.25F, 14);
        if (event.type == GameEvent::cleared_lines && event.value == 4 && !settings.reduced_flash) {
            state.pulse_steps = full ? 16 : 10;
            state.pulse_duration = state.pulse_steps;
        }
    }
}

} // namespace

void advance(EffectState& state, std::span<const Event> events,
             const settings::Settings& settings) {
    advance(state, events, {}, settings);
}

void advance(EffectState& state, std::span<const Event> first, std::span<const Event> second,
             const settings::Settings& settings) {
    // Decay current effects before reacting to events from this simulation step.
    if (state.shake_steps > 0)
        --state.shake_steps;
    if (state.pulse_steps > 0)
        --state.pulse_steps;
    if (settings.effects == settings::Intensity::off) {
        state = {};
        return;
    }

    // Multiplayer combines both boards into one rendering envelope.
    react(state, first, settings);
    react(state, second, settings);
}

Offset shake_offset(const EffectState& state, const settings::Settings& settings) {
    if (settings.effects == settings::Intensity::off || !settings.screen_shake ||
        state.shake_steps <= 0)
        return {};
    const float envelope =
        static_cast<float>(state.shake_steps) / static_cast<float>(state.shake_duration);
    const auto frame = static_cast<std::uint32_t>(state.shake_steps);
    return {sample(state.sequence * 31U + frame * 17U) * state.shake_strength * envelope,
            sample(state.sequence * 47U + frame * 29U) * state.shake_strength * envelope * 0.7F};
}

float background_pulse(const EffectState& state, const settings::Settings& settings) {
    if (settings.effects == settings::Intensity::off || settings.reduced_flash ||
        state.pulse_steps <= 0)
        return 0.0F;
    return static_cast<float>(state.pulse_steps) / static_cast<float>(state.pulse_duration);
}

} // namespace tetris::video
