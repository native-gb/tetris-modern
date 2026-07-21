#pragma once

#include "content/catalog.hpp"
#include "game/state.hpp"
#include "settings.hpp"
#include "video/effects.hpp"
#include "video/video.hpp"

#include <SDL3/SDL.h>
#include <gubsy/runtime.hpp>

#include <cstdint>

namespace tetris::video::drawing {

enum class Bank { gameplay, title, multiplayer };

struct Placement {
    float x{};
    float y{};
    float tile{};
};

struct SpritePosition {
    int x{};
    int y{};
    bool flip{};
};

Placement centered(const GubsyFrame& frame, Offset offset = {}, int columns = 20);
SpritePosition sprite_object_position(const content::Sprite& sprite,
                                      const content::SpriteObject& object, std::uint8_t raw_x,
                                      std::uint8_t raw_y, bool flip = false);
void draw_tile(SDL_Renderer* renderer, const Video& video, Bank bank, std::uint8_t id,
               const Placement& placement, float column, float row, bool sprite = false,
               bool flip = false, const SDL_Color* tint = nullptr);
void draw_map(SDL_Renderer* renderer, const Video& video, Bank bank, const content::Tilemap& map,
              const Placement& placement, int origin_column = 0, int origin_row = 0);
void draw_sprite(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                 Bank bank, std::uint8_t id, const Placement& placement, std::uint8_t raw_x,
                 std::uint8_t raw_y, bool flip = false, bool bounds = false,
                 const SDL_Color* tint = nullptr, MotionOffset motion = {});
void number(SDL_Renderer* renderer, const Video& video, Bank bank, const Placement& placement,
            std::uint32_t value, int right, int row, int width);
void left_number(SDL_Renderer* renderer, const Video& video, Bank bank, const Placement& placement,
                 std::uint32_t value, int column, int row);
void text(SDL_Renderer* renderer, const Video& video, Bank bank, const Placement& placement,
          const char* value, int column, int row);
void draw_panel(SDL_Renderer* renderer, const Placement& placement, int column, int row, int width,
                int height);
void draw_session(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                  Bank bank, const SinglePlayer& game, const Placement& placement, bool active,
                  const settings::Settings& settings, bool sprite_bounds, bool board_cells,
                  bool extended_hud = false, MotionOffset motion = {});
void draw_grid(SDL_Renderer* renderer, const Placement& placement);
void draw_background(SDL_Renderer* renderer, const GubsyFrame& frame,
                     const settings::Settings& settings, float pulse);

} // namespace tetris::video::drawing
