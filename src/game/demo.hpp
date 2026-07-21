#pragma once

#include "game/demo_data.hpp"
#include "game/input.hpp"

#include <cstddef>
#include <span>

namespace tetris {

struct DemoPlayback {
    std::span<const DemoRun> runs;
    std::size_t next_run{};
    int frames_remaining{};
    Buttons held{};

    void start(std::span<const DemoRun> source);
    Buttons step();
};

} // namespace tetris
