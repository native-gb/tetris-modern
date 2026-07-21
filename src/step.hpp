#pragma once

#include "audio/output.hpp"
#include "debug_menu.hpp"
#include "game/replay.hpp"
#include "settings.hpp"
#include "video/effects.hpp"

#include <cstdint>
#include <string>

namespace tetris {

void process_replay_request(DebugUi& debug, Replay& replay, GameState& game,
                            std::uint8_t& random_state, const std::string& rom_sha1);
void apply_debug_command(DebugUi& debug, Replay& replay, GameState& game,
                         const settings::Settings& settings, std::uint8_t& random_state);
void step(GameState& game, Replay& replay, video::EffectState& effects,
          const settings::Settings& settings, audio::Output& audio, GameInput& last_input,
          std::uint8_t& random_state, Buttons player_one, Buttons player_two);

} // namespace tetris
