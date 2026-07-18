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
};

struct ScorePosition {
    GameType type{};
    int level{};
    int height{};
    std::size_t rank{};
};

class HighScores {
public:
    using Table = std::array<ScoreEntry, 3>;

    std::optional<ScorePosition> insert(GameType type, int level, int height,
                                        std::uint32_t score);
    void name(const ScorePosition& position, std::string name);
    const Table& table(GameType type, int level, int height) const;

private:
    std::array<Table, 10> type_a_{};
    std::array<Table, 60> type_b_{};
};

} // namespace tetris
