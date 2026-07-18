#pragma once

#include "content/catalog.hpp"
#include "game/flow.hpp"
#include "presentation/drawing.hpp"

namespace tetris::presentation {

void draw_flow_ui(SDL_Renderer* renderer, const Renderer& video,
                  const content::Catalog& content, const GameFlow& flow,
                  const drawing::Placement& placement);
void draw_versus_status(SDL_Renderer* renderer, const Renderer& video,
                        const content::Catalog& content, const VersusMatch& match,
                        const drawing::Placement& placement, int player);

} // namespace tetris::presentation
