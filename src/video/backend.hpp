#pragma once

#include <array>
#include <cstdint>

namespace tetris::video {

enum class RenderBackend : std::uint8_t { gpu_atlas, cpu_raster };

inline constexpr std::array render_rate_limits{60, 120, 144, 165, 240};

constexpr bool valid_render_rate(int rate) {
    for (int supported : render_rate_limits) {
        if (rate == supported)
            return true;
    }
    return false;
}

constexpr int effective_render_rate(bool interpolation, int selected) {
    return interpolation ? selected : 60;
}

} // namespace tetris::video
