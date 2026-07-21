#pragma once

#include "game/state.hpp"

#include <array>

namespace tetris::video {

struct PieceMotion {
    FallingPiece piece{};
    PlayState state{PlayState::game_over};
    std::uint64_t step{};
};

struct MotionHistory {
    Screen screen{Screen::title};
    PieceMotion single;
    std::array<PieceMotion, 2> versus{};
    bool valid{};
};

struct MotionOffset {
    float x{};
    float y{};
};

void capture_motion(MotionHistory& history, const GameState& game);
MotionOffset piece_motion(const MotionHistory& history, const GameState& game,
                          const SinglePlayer& current, int player, float alpha);

} // namespace tetris::video
