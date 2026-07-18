#pragma once

#include "game/single_player.hpp"
#include "presentation/settings.hpp"

#include <cstdint>
#include <span>

namespace tetris::presentation {

struct EffectState {
    int shake_ticks{};
    int shake_duration{};
    float shake_strength{};
    int pulse_ticks{};
    int pulse_duration{};
    std::uint32_t sequence{1};
};

struct Offset { float x{}; float y{}; };

void advance(EffectState& state, std::span<const Event> events, const Settings& settings);
Offset shake_offset(const EffectState& state, const Settings& settings);
float background_pulse(const EffectState& state, const Settings& settings);

} // namespace tetris::presentation
