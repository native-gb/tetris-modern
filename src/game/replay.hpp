#pragma once

#include "game/flow.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tetris {

struct ReplayIdentity {
    std::uint32_t schema{1};
    std::string rom_sha1;
    LineClearSpeed pacing{LineClearSpeed::original};
    Screen starting_screen{Screen::copyright_fixed};
    GameRules rules{};

    bool operator==(const ReplayIdentity&) const = default;
};

class Replay {
public:
    void begin_recording(const GameFlow& flow, ReplayIdentity identity,
                         std::uint8_t random_state);
    void append(FlowInput input, std::uint8_t random_state);
    void stop(std::uint8_t random_state);
    bool rewind(GameFlow& flow, std::uint8_t& random_state);
    std::optional<FlowInput> next(std::uint8_t& random_state);
    void clear();

    bool recording() const;
    bool playing() const;
    bool available() const;
    std::size_t position() const;
    std::size_t size() const;
    const ReplayIdentity& identity() const;

private:
    std::optional<GameFlow> initial_;
    ReplayIdentity identity_{};
    std::vector<FlowInput> inputs_;
    std::size_t position_{};
    std::uint8_t initial_random_state_{};
    std::uint8_t final_random_state_{};
    bool recording_{};
    bool playing_{};
};

} // namespace tetris
