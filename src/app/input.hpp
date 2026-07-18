#pragma once

#include "game/input.hpp"

class GubsyRuntime;

namespace tetris::app {

enum class Action : int {
    left = 1,
    right,
    up,
    down,
    rotate_left,
    rotate_right,
    start,
    select,
    quit,
};

bool register_controls(GubsyRuntime& runtime);
bool assign_gamepad(GubsyRuntime& runtime, int device_id);
void assign_unclaimed_gamepads(GubsyRuntime& runtime);
bool action_down(GubsyRuntime& runtime, int player, Action action);
Buttons read_buttons(GubsyRuntime& runtime, int player);

} // namespace tetris::app
