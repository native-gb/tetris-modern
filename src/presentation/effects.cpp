#include "presentation/effects.hpp"

#include <algorithm>

namespace tetris::presentation {
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
    state.shake_ticks = std::max(state.shake_ticks, duration);
    state.shake_duration = std::max(state.shake_duration, duration);
    ++state.sequence;
}

} // namespace

void advance(EffectState& state, std::span<const Event> events, const Settings& settings) {
    if (state.shake_ticks > 0)
        --state.shake_ticks;
    if (state.pulse_ticks > 0)
        --state.pulse_ticks;
    if (settings.effects == Intensity::off) {
        state = {};
        return;
    }
    const bool full = settings.effects == Intensity::full;
    for (const Event event : events) {
        if (event.type == GameEvent::landed && !settings.reduced_motion)
            shake(state, full ? 1.6F : 0.75F, full ? 7 : 5);
        if (event.type == GameEvent::cleared_lines && !settings.reduced_motion)
            shake(state, event.value == 4 ? (full ? 4.0F : 2.0F) : (full ? 2.0F : 1.0F),
                  event.value == 4 ? 12 : 8);
        if (event.type == GameEvent::game_over && !settings.reduced_motion)
            shake(state, full ? 4.5F : 2.25F, 14);
        if (event.type == GameEvent::cleared_lines && event.value == 4 && !settings.reduced_flash) {
            state.pulse_ticks = full ? 16 : 10;
            state.pulse_duration = state.pulse_ticks;
        }
    }
}

Offset shake_offset(const EffectState& state, const Settings& settings) {
    if (settings.effects == Intensity::off || settings.reduced_motion || state.shake_ticks <= 0)
        return {};
    const float envelope = static_cast<float>(state.shake_ticks) /
                           static_cast<float>(state.shake_duration);
    const auto frame = static_cast<std::uint32_t>(state.shake_ticks);
    return {sample(state.sequence * 31U + frame * 17U) * state.shake_strength * envelope,
            sample(state.sequence * 47U + frame * 29U) * state.shake_strength * envelope * 0.7F};
}

float background_pulse(const EffectState& state, const Settings& settings) {
    if (settings.effects == Intensity::off || settings.reduced_flash || state.pulse_ticks <= 0)
        return 0.0F;
    return static_cast<float>(state.pulse_ticks) / static_cast<float>(state.pulse_duration);
}

} // namespace tetris::presentation
