#include "game/high_scores.hpp"

#include <cassert>
#include <utility>

namespace tetris {
namespace {

std::size_t table_index(int level, int height) {
    assert(level >= 0 && level <= 9);
    assert(height >= 0 && height <= 5);
    return static_cast<std::size_t>(level * 6 + height);
}

} // namespace

std::optional<ScorePosition> HighScores::insert(GameType type, int level, int height,
                                                std::uint32_t score) {
    Table& scores = type == GameType::type_a
                        ? type_a_[static_cast<std::size_t>(level)]
                        : type_b_[table_index(level, height)];
    std::size_t rank = scores.size();
    for (std::size_t index = 0; index < scores.size(); ++index) {
        if (score > scores[index].score) {
            rank = index;
            break;
        }
    }
    if (rank == scores.size())
        return std::nullopt;
    for (std::size_t index = scores.size() - 1; index > rank; --index)
        scores[index] = scores[index - 1];
    scores[rank] = {score, "A"};
    return ScorePosition{type, level, height, rank};
}

void HighScores::name(const ScorePosition& position, std::string name) {
    Table& scores = position.type == GameType::type_a
                        ? type_a_[static_cast<std::size_t>(position.level)]
                        : type_b_[table_index(position.level, position.height)];
    assert(position.rank < scores.size());
    scores[position.rank].name = name.substr(0, 6);
}

const HighScores::Table& HighScores::table(GameType type, int level, int height) const {
    return type == GameType::type_a
               ? type_a_[static_cast<std::size_t>(level)]
               : type_b_[table_index(level, height)];
}

bool HighScores::set_entry(GameType type, int level, int height, std::size_t rank,
                           ScoreEntry entry) {
    if (level < 0 || level > 9 || height < 0 || height > 5 || rank >= Table{}.size() ||
        entry.score > 999'999 || entry.name.size() > 6)
        return false;
    Table& scores = type == GameType::type_a
                        ? type_a_[static_cast<std::size_t>(level)]
                        : type_b_[table_index(level, height)];
    scores[rank] = std::move(entry);
    return true;
}

} // namespace tetris
