#pragma once

#include "game/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace tetris {

struct ScoreEntry {
    std::uint32_t score{};
    std::string name;

    bool operator==(const ScoreEntry&) const = default;
};

struct ScorePosition {
    GameType type{};
    int level{};
    int height{};
    std::size_t rank{};
};

struct HighScores {
    using Table = std::array<ScoreEntry, 3>;

    std::array<Table, 10> type_a{};
    std::array<Table, 60> type_b{};

    bool operator==(const HighScores&) const = default;

    std::optional<ScorePosition> insert(GameType type, int level, int height,
                                        std::uint32_t score);
    void name(const ScorePosition& position, std::string name);
    const Table& table(GameType type, int level, int height) const;
    bool set_entry(GameType type, int level, int height, std::size_t rank, ScoreEntry entry);
};

} // namespace tetris
