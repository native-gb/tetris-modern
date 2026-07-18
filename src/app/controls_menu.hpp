#pragma once

#include "game/input.hpp"

#include <gubsy/runtime.hpp>

namespace tetris::app {

class ControlsMenu {
public:
    bool open(GubsyRuntime& runtime);
    bool is_open(const GubsyRuntime& runtime) const;
    bool chord_pressed(const Buttons& buttons);
    void update(GubsyRuntime& runtime, const Buttons& buttons, float elapsed_seconds,
                int width, int height);
    void draw(GubsyRuntime& runtime, SDL_Renderer* renderer, int width, int height) const;

private:
    Buttons previous_{};
    bool chord_was_down_{};
};

} // namespace tetris::app
