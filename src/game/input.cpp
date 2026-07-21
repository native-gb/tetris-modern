#include "game/input.hpp"

namespace tetris {
namespace {

Buttons combine(Buttons first, Buttons second) {
    return {
        .left = first.left || second.left,
        .right = first.right || second.right,
        .up = first.up || second.up,
        .down = first.down || second.down,
        .back = first.back || second.back,
        .confirm = first.confirm || second.confirm,
        .rotate_counterclockwise =
            first.rotate_counterclockwise || second.rotate_counterclockwise,
        .rotate_clockwise = first.rotate_clockwise || second.rotate_clockwise,
        .hard_drop = first.hard_drop || second.hard_drop,
        .hold = first.hold || second.hold,
        .restart = first.restart || second.restart,
        .start = first.start || second.start,
        .select = first.select || second.select,
    };
}

} // namespace

Buttons newly_pressed(const Buttons& current, const Buttons& previous) {
    return {
        .left = current.left && !previous.left,
        .right = current.right && !previous.right,
        .up = current.up && !previous.up,
        .down = current.down && !previous.down,
        .back = current.back && !previous.back,
        .confirm = current.confirm && !previous.confirm,
        .rotate_counterclockwise =
            current.rotate_counterclockwise && !previous.rotate_counterclockwise,
        .rotate_clockwise = current.rotate_clockwise && !previous.rotate_clockwise,
        .hard_drop = current.hard_drop && !previous.hard_drop,
        .hold = current.hold && !previous.hold,
        .restart = current.restart && !previous.restart,
        .start = current.start && !previous.start,
        .select = current.select && !previous.select,
    };
}

bool counterclockwise_pressed(const Buttons& buttons, bool separate_rotation_inputs) {
    return separate_rotation_inputs ? buttons.rotate_counterclockwise : buttons.back;
}

bool clockwise_pressed(const Buttons& buttons, bool separate_rotation_inputs) {
    return separate_rotation_inputs ? buttons.rotate_clockwise : buttons.confirm;
}

void sample_buttons(ButtonBuffer& buffer, Buttons current) {
    buffer.pending_presses =
        combine(buffer.pending_presses, newly_pressed(current, buffer.previous_sample));
    buffer.held = current;
    buffer.previous_sample = current;
}

Buttons consume_buttons(ButtonBuffer& buffer) {
    const Buttons result = combine(buffer.held, buffer.pending_presses);
    buffer.pending_presses = {};
    return result;
}

void clear_buttons(ButtonBuffer& buffer) {
    buffer = {};
}

} // namespace tetris
