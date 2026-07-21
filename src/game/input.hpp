#pragma once

namespace tetris {

struct Buttons {
    bool left{};
    bool right{};
    bool up{};
    bool down{};
    bool back{};
    bool confirm{};
    bool rotate_counterclockwise{};
    bool rotate_clockwise{};
    bool hard_drop{};
    bool hold{};
    bool restart{};
    bool start{};
    bool select{};

    bool operator==(const Buttons&) const = default;
};

bool counterclockwise_pressed(const Buttons& buttons, bool separate_rotation_inputs);
bool clockwise_pressed(const Buttons& buttons, bool separate_rotation_inputs);

struct ButtonBuffer {
    Buttons held{};
    Buttons previous_sample{};
    Buttons pending_presses{};
};

Buttons newly_pressed(const Buttons& current, const Buttons& previous);
void sample_buttons(ButtonBuffer& buffer, Buttons current);
Buttons consume_buttons(ButtonBuffer& buffer);
void clear_buttons(ButtonBuffer& buffer);

} // namespace tetris
