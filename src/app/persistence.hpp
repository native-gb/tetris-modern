#pragma once

#include "game/high_scores.hpp"

#include <filesystem>
#include <string>

namespace tetris::app {

bool load_high_scores(const std::filesystem::path& path, HighScores& scores, std::string& error);
bool save_high_scores(const std::filesystem::path& path, const HighScores& scores,
                      std::string& error);

} // namespace tetris::app
