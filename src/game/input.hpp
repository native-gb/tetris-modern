#pragma once

namespace tetris {

struct Buttons {
    bool left{};
    bool right{};
    bool up{};
    bool down{};
    bool rotate_left{};
    bool rotate_right{};
    bool start{};
    bool select{};

    bool operator==(const Buttons&) const = default;
};

Buttons newly_pressed(const Buttons& current, const Buttons& previous);

inline Buttons newly_pressed(const Buttons& current, const Buttons& previous) {
    return {
        .left = current.left && !previous.left,
        .right = current.right && !previous.right,
        .up = current.up && !previous.up,
        .down = current.down && !previous.down,
        .rotate_left = current.rotate_left && !previous.rotate_left,
        .rotate_right = current.rotate_right && !previous.rotate_right,
        .start = current.start && !previous.start,
        .select = current.select && !previous.select,
    };
}

} // namespace tetris
