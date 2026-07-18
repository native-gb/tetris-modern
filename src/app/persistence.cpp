#include "app/persistence.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace tetris::app {
namespace {

constexpr int current_schema = 1;

bool read_entry(const std::string& line, HighScores& scores) {
    std::istringstream input(line);
    char type = '\0';
    int level = 0;
    int height = 0;
    std::size_t rank = 0;
    std::uint32_t score = 0;
    std::string name;
    if (!(input >> type >> level >> height >> rank >> score >> std::quoted(name)))
        return false;
    input >> std::ws;
    if (!input.eof() || (type != 'A' && type != 'B'))
        return false;
    const GameType game_type = type == 'A' ? GameType::type_a : GameType::type_b;
    return scores.set_entry(game_type, level, height, rank, {score, std::move(name)});
}

void write_table(std::ostream& output, char type, int level, int height,
                 const HighScores::Table& table) {
    for (std::size_t rank = 0; rank < table.size(); ++rank) {
        output << type << ' ' << level << ' ' << height << ' ' << rank << ' '
               << table[rank].score << ' ' << std::quoted(table[rank].name) << '\n';
    }
}

bool replace_file(const std::filesystem::path& temporary, const std::filesystem::path& path,
                  std::string& error) {
    std::error_code filesystem_error;
    std::filesystem::rename(temporary, path, filesystem_error);
    if (!filesystem_error)
        return true;
    std::filesystem::remove(path, filesystem_error);
    filesystem_error.clear();
    std::filesystem::rename(temporary, path, filesystem_error);
    if (!filesystem_error)
        return true;
    error = filesystem_error.message();
    return false;
}

} // namespace

bool load_high_scores(const std::filesystem::path& path, HighScores& scores, std::string& error) {
    error.clear();
    std::ifstream input(path);
    if (!input)
        return !std::filesystem::exists(path);

    std::string line;
    if (!std::getline(input, line) || line != "native-gb-tetris-high-scores 1") {
        error = "unsupported high-score file schema";
        return false;
    }
    HighScores loaded;
    int entries = 0;
    while (std::getline(input, line)) {
        if (line.empty() || line.front() == '#')
            continue;
        if (!read_entry(line, loaded)) {
            error = "invalid high-score entry on line " + std::to_string(entries + 2);
            return false;
        }
        ++entries;
    }
    constexpr int expected_entries = (10 + 60) * 3;
    if (entries != expected_entries) {
        error = "high-score file has " + std::to_string(entries) + " entries; expected " +
                std::to_string(expected_entries);
        return false;
    }
    scores = std::move(loaded);
    return true;
}

bool save_high_scores(const std::filesystem::path& path, const HighScores& scores,
                      std::string& error) {
    error.clear();
    const std::filesystem::path temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) {
        error = "could not create high-score file";
        return false;
    }
    output << "native-gb-tetris-high-scores " << current_schema << '\n';
    for (int level = 0; level < 10; ++level)
        write_table(output, 'A', level, 0, scores.table(GameType::type_a, level, 0));
    for (int level = 0; level < 10; ++level) {
        for (int height = 0; height < 6; ++height)
            write_table(output, 'B', level, height,
                        scores.table(GameType::type_b, level, height));
    }
    output.close();
    if (!output) {
        error = "could not finish high-score file";
        return false;
    }
    return replace_file(temporary, path, error);
}

} // namespace tetris::app
