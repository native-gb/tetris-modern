#pragma once

#include "game/state.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tetris {

struct ReplayIdentity {
    std::uint32_t schema{2};
    std::string rom_sha1;
    LineClearSpeed pacing{LineClearSpeed::original};
    Screen starting_screen{Screen::title};
    GameRules rules{};
    GameplayOptions gameplay{};

    bool operator==(const ReplayIdentity&) const = default;
};

struct Replay {
    std::optional<GameState> initial;
    ReplayIdentity identity;
    std::vector<GameInput> inputs;
    std::size_t position{};
    std::uint8_t initial_random_state{};
    std::uint8_t final_random_state{};
    bool recording{};
    bool playing{};

    void begin_recording(const GameState& state, ReplayIdentity descriptor,
                         std::uint8_t random_state);
    void append(GameInput input, std::uint8_t random_state);
    void stop(std::uint8_t random_state);
    bool rewind(GameState& state, std::uint8_t& random_state);
    std::optional<GameInput> next(std::uint8_t& random_state);
    void clear();
};

} // namespace tetris
