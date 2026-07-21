#pragma once

#include "game/single_player.hpp"
#include "settings.hpp"

#include <cstdint>
#include <span>

namespace tetris::video {

struct EffectState {
    int shake_steps{};
    int shake_duration{};
    float shake_strength{};
    int pulse_steps{};
    int pulse_duration{};
    std::uint32_t sequence{1};
};

struct Offset {
    float x{};
    float y{};
};

void advance(EffectState& state, std::span<const Event> events, const settings::Settings& settings);
void advance(EffectState& state, std::span<const Event> first, std::span<const Event> second,
             const settings::Settings& settings);
Offset shake_offset(const EffectState& state, const settings::Settings& settings);
float background_pulse(const EffectState& state, const settings::Settings& settings);

} // namespace tetris::video
