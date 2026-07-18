#include "game/replay.hpp"

#include <utility>

namespace tetris {

void Replay::begin_recording(const GameFlow& flow, ReplayIdentity identity,
                             std::uint8_t random_state) {
    initial_ = flow;
    identity_ = std::move(identity);
    inputs_.clear();
    position_ = 0;
    initial_random_state_ = random_state;
    final_random_state_ = random_state;
    recording_ = true;
    playing_ = false;
}

void Replay::append(FlowInput input, std::uint8_t random_state) {
    if (!recording_)
        return;
    inputs_.push_back(std::move(input));
    position_ = inputs_.size();
    final_random_state_ = random_state;
}

void Replay::stop(std::uint8_t random_state) {
    recording_ = false;
    playing_ = false;
    final_random_state_ = random_state;
}

bool Replay::rewind(GameFlow& flow, std::uint8_t& random_state) {
    if (!initial_ || inputs_.empty())
        return false;
    flow = *initial_;
    random_state = initial_random_state_;
    position_ = 0;
    recording_ = false;
    playing_ = true;
    return true;
}

std::optional<FlowInput> Replay::next(std::uint8_t& random_state) {
    if (!playing_)
        return std::nullopt;
    if (position_ == inputs_.size()) {
        playing_ = false;
        random_state = final_random_state_;
        return std::nullopt;
    }
    return inputs_[position_++];
}

void Replay::clear() { *this = {}; }

bool Replay::recording() const { return recording_; }
bool Replay::playing() const { return playing_; }
bool Replay::available() const { return initial_.has_value() && !inputs_.empty(); }
std::size_t Replay::position() const { return position_; }
std::size_t Replay::size() const { return inputs_.size(); }
const ReplayIdentity& Replay::identity() const { return identity_; }

} // namespace tetris
