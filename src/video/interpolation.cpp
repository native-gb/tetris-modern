#include "video/interpolation.hpp"

#include <algorithm>
#include <cmath>

namespace tetris::video {
namespace {

PieceMotion snapshot(const SinglePlayer& game) {
    return {.piece = game.piece, .state = game.state, .step = game.step_count};
}

bool compatible(const PieceMotion& previous, const SinglePlayer& current) {
    return previous.state == PlayState::falling && current.state == PlayState::falling &&
           previous.step + 1U == current.step_count &&
           previous.piece.kind == current.piece.kind &&
           previous.piece.rotation == current.piece.rotation &&
           std::abs(previous.piece.origin.x - current.piece.origin.x) <= 2 &&
           std::abs(previous.piece.origin.y - current.piece.origin.y) <= 2;
}

} // namespace

void capture_motion(MotionHistory& history, const GameState& game) {
    history.screen = game.screen;
    history.single = snapshot(game.single_player);
    for (std::size_t player = 0; player < history.versus.size(); ++player)
        history.versus[player] = snapshot(game.versus.players[player].game);
    history.valid = true;
}

MotionOffset piece_motion(const MotionHistory& history, const GameState& game,
                          const SinglePlayer& current, int player, float alpha) {
    if (!history.valid || history.screen != game.screen)
        return {};
    const PieceMotion& previous = player < 0
                                      ? history.single
                                      : history.versus[static_cast<std::size_t>(player)];
    if (!compatible(previous, current))
        return {};
    const float blend = 1.0F - std::clamp(alpha, 0.0F, 1.0F);
    return {
        static_cast<float>(previous.piece.origin.x - current.piece.origin.x) * 8.0F * blend,
        static_cast<float>(previous.piece.origin.y - current.piece.origin.y) * 8.0F * blend,
    };
}

} // namespace tetris::video
