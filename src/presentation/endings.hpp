#pragma once

#include "content/catalog.hpp"
#include "game/flow.hpp"
#include "presentation/drawing.hpp"

namespace tetris::presentation {

void draw_game_over(SDL_Renderer* renderer, const Renderer& video,
                    const content::Catalog& content, const GameFlow& flow,
                    drawing::Bank bank, const drawing::Placement& placement,
                    const Settings& settings, bool sprite_bounds);
void draw_type_b_result(SDL_Renderer* renderer, const Renderer& video,
                        const content::Catalog& content, const GameFlow& flow,
                        const drawing::Placement& placement);
void draw_launch(SDL_Renderer* renderer, const Renderer& video,
                 const content::Catalog& content, const GameFlow& flow,
                 const drawing::Placement& placement);
void draw_versus_result(SDL_Renderer* renderer, const Renderer& video,
                        const content::Catalog& content, const GameFlow& flow,
                        const drawing::Placement& placement);

} // namespace tetris::presentation
