#pragma once

#include "game/input.hpp"

class GubsyRuntime;

namespace tetris {

enum class Action : int {
    left = 1,
    right = 2,
    up = 3,
    down = 4,
    back = 5,
    confirm = 6,
    start = 7,
    select = 8,
    quit = 9,
    hard_drop = 10,
    hold = 11,
    restart = 12,
    rotate_counterclockwise = 13,
    rotate_clockwise = 14,
};

enum class AnalogAction : int {
    drop = 1,
};

bool register_controls(GubsyRuntime& runtime);
bool reset_controls(GubsyRuntime& runtime, int player);
bool swap_rotation_controls(GubsyRuntime& runtime, int player);
bool assign_gamepad(GubsyRuntime& runtime, int device_id);
bool set_gamepad_player(GubsyRuntime& runtime, int device_id, int player);
void assign_unclaimed_gamepads(GubsyRuntime& runtime);
bool action_down(GubsyRuntime& runtime, int player, Action action);
Buttons read_buttons(GubsyRuntime& runtime, int player);

} // namespace tetris
