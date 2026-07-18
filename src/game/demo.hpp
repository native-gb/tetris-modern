#pragma once

#include "game/demo_data.hpp"
#include "game/input.hpp"

#include <cstddef>
#include <span>

namespace tetris {

class DemoPlayback {
public:
    void start(std::span<const DemoRun> runs);
    Buttons tick();

    bool finished() const;
    std::size_t run_index() const;

private:
    std::span<const DemoRun> runs_;
    std::size_t next_run_{};
    int frames_remaining_{};
    Buttons held_{};
};

} // namespace tetris
