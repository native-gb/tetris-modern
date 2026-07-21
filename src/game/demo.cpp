#include "game/demo.hpp"
#include "game/state.hpp"

#include <vector>

namespace tetris {

void DemoPlayback::start(std::span<const DemoRun> source) {
    runs = source;
    next_run = 0;
    frames_remaining = 0;
    held = {};
}

Buttons DemoPlayback::step() {
    // Expand one frame from the ROM's run-length encoded input stream.
    if (frames_remaining == 0 && next_run < runs.size()) {
        held = runs[next_run].held;
        frames_remaining = runs[next_run].frames;
        ++next_run;
    } else if (frames_remaining > 0) {
        --frames_remaining;
    }
    return held;
}

void begin_demo(GameState& state, const GameInput& input) {
    // Alternate Type A and Type B each time title idling starts a demo.
    state.demo_type_b = state.next_demo_type_b;
    state.next_demo_type_b = !state.next_demo_type_b;

    std::vector<PieceSpec> pieces;
    const std::size_t begin = state.demo_type_b ? 17U : 0U;
    for (std::size_t index = begin; index < state.resources.demo_pieces.size(); ++index)
        pieces.push_back(state.resources.demo_pieces[index]);

    // Start ordinary gameplay with the ROM-authored piece fixture.
    state.single_player.start(state.resources.gameplay,
                              {.type = state.demo_type_b ? GameType::type_b : GameType::type_a,
                               .starting_level = 9,
                               .type_b_height = state.demo_type_b ? 2 : 0},
                              input.startup, pieces, {},
                              {.drop_mode = DropMode::soft_only,
                               .randomizer = RandomizerMode::original,
                               .rotation = RotationSystem::original,
                               .challenge = ChallengeMode::marathon,
                               .next_previews = 1,
                               .horizontal_repeat_delay = 23,
                               .horizontal_repeat_interval = 9,
                               .instant_autorepeat = false,
                               .separate_rotation_inputs = false});
    state.single_player.line_clear_speed = state.line_clear_speed;

    // Type B also replaces the generated bottom rows with its recorded board.
    if (state.demo_type_b && state.resources.type_b_demo_garbage.size() >= 40) {
        for (int index = 0; index < 40; ++index) {
            const std::uint8_t tile =
                state.resources.type_b_demo_garbage[static_cast<std::size_t>(index)];
            if (tile != 0x2F)
                state.single_player.board.set({index % board_width, 14 + index / board_width},
                                              garbage_block(tile));
        }
    }

    state.demo.start(state.demo_type_b ? state.resources.type_b_demo : state.resources.type_a_demo);
    state.screen = Screen::demo;
}

void step_demo(GameState& state, const GameInput& input, const Buttons& pressed) {
    // Start leaves playback immediately.
    if (pressed.start) {
        return_to_title(state, 4);
        return;
    }

    // Feed one decoded demo frame through the normal simulation.
    state.single_player.step({state.demo.step(), input.random});
    const std::size_t end_piece = state.demo_type_b ? 12U : 16U;
    if (state.single_player.next_fixed_piece >= end_piece ||
        state.single_player.state == PlayState::game_over ||
        state.single_player.state == PlayState::complete) {
        // Completed and failed demos both return to the short title interval.
        return_to_title(state, 4);
    }
}

} // namespace tetris
