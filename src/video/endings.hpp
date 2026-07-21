#pragma once

#include "content/catalog.hpp"
#include "game/state.hpp"
#include "video/drawing.hpp"

namespace tetris::video {

void draw_game_over(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                    const GameState& state, drawing::Bank bank, const drawing::Placement& placement,
                    const settings::Settings& settings, bool sprite_bounds, bool extended_hud);
void draw_type_b_result(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                        const GameState& state, const drawing::Placement& placement);
void draw_launch(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                 const GameState& state, const drawing::Placement& placement);
void draw_versus_result(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                        const GameState& state, const drawing::Placement& placement);

} // namespace tetris::video
