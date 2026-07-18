#include "game/demo.hpp"

namespace tetris {

void DemoPlayback::start(std::span<const DemoRun> runs) {
    runs_ = runs;
    next_run_ = 0;
    frames_remaining_ = 0;
    held_ = {};
}

Buttons DemoPlayback::tick() {
    if (frames_remaining_ == 0 && next_run_ < runs_.size()) {
        held_ = runs_[next_run_].held;
        frames_remaining_ = runs_[next_run_].frames;
        ++next_run_;
    } else if (frames_remaining_ > 0) {
        --frames_remaining_;
    }
    return held_;
}

bool DemoPlayback::finished() const {
    return next_run_ == runs_.size() && frames_remaining_ == 0;
}

std::size_t DemoPlayback::run_index() const { return next_run_; }

} // namespace tetris
