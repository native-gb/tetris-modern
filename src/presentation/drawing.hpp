#pragma once

#include "content/catalog.hpp"
#include "game/flow.hpp"
#include "presentation/effects.hpp"
#include "presentation/renderer.hpp"
#include "presentation/settings.hpp"

#include <SDL3/SDL.h>
#include <gubsy/runtime.hpp>

#include <cstdint>

namespace tetris::presentation::drawing {

enum class Bank { gameplay, title, multiplayer };

struct Placement {
    float x{};
    float y{};
    float tile{};
};

Placement centered(const GubsyFrame& frame, Offset offset = {});
void draw_tile(SDL_Renderer* renderer, const Renderer& video, Bank bank,
               std::uint8_t id, const Placement& placement, float column, float row,
               bool sprite = false, bool flip = false);
void draw_map(SDL_Renderer* renderer, const Renderer& video, Bank bank,
              const content::Tilemap& map, const Placement& placement,
              int origin_column = 0, int origin_row = 0);
void draw_sprite(SDL_Renderer* renderer, const Renderer& video,
                 const content::Catalog& content, Bank bank, std::uint8_t id,
                 const Placement& placement, std::uint8_t raw_x, std::uint8_t raw_y,
                 bool flip = false, bool bounds = false);
void number(SDL_Renderer* renderer, const Renderer& video, Bank bank,
            const Placement& placement, std::uint32_t value, int right, int row, int width);
void left_number(SDL_Renderer* renderer, const Renderer& video, Bank bank,
                 const Placement& placement, std::uint32_t value, int column, int row);
void text(SDL_Renderer* renderer, const Renderer& video, Bank bank,
          const Placement& placement, const char* value, int column, int row);
void draw_session(SDL_Renderer* renderer, const Renderer& video,
                  const content::Catalog& content, Bank bank,
                  const SinglePlayer& game, const Placement& placement,
                  bool active, const Settings& settings, bool bounds);
void draw_grid(SDL_Renderer* renderer, const Placement& placement);
void draw_background(SDL_Renderer* renderer, const GubsyFrame& frame,
                     const Settings& settings, float pulse);

} // namespace tetris::presentation::drawing
