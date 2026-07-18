#pragma once

#include "content/catalog.hpp"
#include "content/rom.hpp"

namespace tetris::app {

int run(const content::Rom& rom, const content::Catalog& content, int frame_limit = 0);

} // namespace tetris::app
