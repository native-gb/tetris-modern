#include "game/replay.hpp"

#include <utility>

namespace tetris {

void Replay::begin_recording(const GameState& state, ReplayIdentity descriptor,
                             std::uint8_t random_state) {
    // Capture typed game state instead of serializing object memory.
    initial = state;
    identity = std::move(descriptor);
    inputs.clear();
    position = 0;
    initial_random_state = random_state;
    final_random_state = random_state;
    recording = true;
    playing = false;
}

void Replay::append(GameInput input, std::uint8_t random_state) {
    // Store one explicit step input and its resulting random-byte state.
    if (!recording)
        return;
    inputs.push_back(std::move(input));
    position = inputs.size();
    final_random_state = random_state;
}

void Replay::stop(std::uint8_t random_state) {
    recording = false;
    playing = false;
    final_random_state = random_state;
}

bool Replay::rewind(GameState& state, std::uint8_t& random_state) {
    // Restore the semantic starting point and begin at input zero.
    if (!initial || inputs.empty())
        return false;
    state = *initial;
    random_state = initial_random_state;
    position = 0;
    recording = false;
    playing = true;
    return true;
}

std::optional<GameInput> Replay::next(std::uint8_t& random_state) {
    // Return one recorded input and close playback cleanly at the end.
    if (!playing)
        return std::nullopt;
    if (position == inputs.size()) {
        playing = false;
        random_state = final_random_state;
        return std::nullopt;
    }
    return inputs[position++];
}

void Replay::clear() {
    *this = {};
}

} // namespace tetris
