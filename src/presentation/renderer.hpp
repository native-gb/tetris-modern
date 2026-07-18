#pragma once

#include "content/catalog.hpp"
#include "game/flow.hpp"
#include "presentation/effects.hpp"
#include "presentation/settings.hpp"

#include <SDL3/SDL.h>
#include <gubsy/runtime.hpp>

namespace tetris::presentation {

struct DebugView {
    bool grid{};
    bool sprite_bounds{};
    bool board_cells{};
};

class Renderer {
public:
    struct Atlas {
        SDL_Texture* opaque{};
        SDL_Texture* sprites{};
        int count{};
    };

    bool initialize(SDL_Renderer* renderer, const content::Catalog& content);
    void shutdown();
    void draw(SDL_Renderer* renderer, const GubsyFrame& frame,
              const content::Catalog& content, const GameFlow& flow,
              const Settings& settings, const EffectState& effects,
              DebugView debug = {}) const;

    const Atlas& gameplay() const { return gameplay_; }
    const Atlas& font() const { return font_; }
    const Atlas& title() const { return title_; }
    const Atlas& multiplayer() const { return multiplayer_; }

private:

    Atlas gameplay_{};
    Atlas font_{};
    Atlas title_{};
    Atlas multiplayer_{};
};

} // namespace tetris::presentation
