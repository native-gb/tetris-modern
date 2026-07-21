#pragma once

#include "content/catalog.hpp"
#include "game/state.hpp"
#include "settings.hpp"
#include "video/effects.hpp"
#include "video/interpolation.hpp"

#include <SDL3/SDL.h>
#include <gubsy/runtime.hpp>

#include <array>

namespace tetris::video {

struct DebugView {
    bool grid{};
    bool sprite_bounds{};
    bool board_cells{};
};

struct Atlas {
    SDL_Texture* opaque{};
    SDL_Texture* sprites{};
    int count{};
};

struct TitleLayer {
    SDL_Texture* texture{};
    int width{};
    int height{};
};

// SDL resources needed to draw the game. Drawing behavior stays in free functions.
struct Video {
    Atlas gameplay{};
    Atlas font{};
    Atlas title{};
    Atlas multiplayer{};
    TitleLayer title_moon{};
    TitleLayer title_city{};
    TitleLayer title_skyline{};
    TitleLayer title_towers{};
    settings::ControllerLabels detected_controller_labels{settings::ControllerLabels::nintendo};
    std::array<SDL_Texture*, 2> scenes{};
};

bool initialize_video(Video& video, SDL_Renderer* renderer, const content::Catalog& content);
void shutdown_video(Video& video);
void render_video(const Video& video, SDL_Renderer* renderer, const GubsyFrame& frame,
                  const content::Catalog& content, const GameState& game,
                  const settings::Settings& settings, const EffectState& effects,
                  DebugView debug = {}, const MotionHistory& motion = {},
                  float alpha = 1.0F);

} // namespace tetris::video
