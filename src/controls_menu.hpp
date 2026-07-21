#pragma once

#include "game/input.hpp"

#include <gubsy/runtime.hpp>

#include <string>

namespace tetris {

struct ControlsMenu {
    bool visible{};
    bool chord_was_down{};
    bool select_player_one{true};
    std::string status;

    void open();
    void toggle();
    bool is_open() const;
    bool chord_pressed(const Buttons& buttons);
    void draw(GubsyRuntime& runtime, bool arranged_with_tools);
};

} // namespace tetris
