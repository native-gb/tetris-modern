#pragma once

#include "content/catalog.hpp"
#include "game/state.hpp"
#include "video/drawing.hpp"

namespace tetris::video {

void draw_screen_ui(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                    const GameState& state, const drawing::Placement& placement);
void draw_versus_status(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                        const VersusMatch& match, const drawing::Placement& placement, int player);

} // namespace tetris::video
