#include "game/randomizer.hpp"

namespace tetris {

PieceKind piece_from_divider(std::uint8_t divider) {
    // The hardware counter decrements before the addition loop. Zero therefore
    // wraps to 255 additions rather than selecting the first piece directly.
    const unsigned int additions = divider == 0 ? 255U : divider - 1U;
    return static_cast<PieceKind>(additions % 7U);
}

PieceQueue advance_piece_queue(PieceKind preview, PieceKind hidden,
                               const RandomSamples& samples) {
    PieceKind candidate = PieceKind::L;
    int attempts = 0;

    for (const std::uint8_t sample : samples) {
        candidate = piece_from_divider(sample);
        ++attempts;
        if (attempts == 3 || (candidate != preview && hidden != preview))
            break;
    }

    return {
        .active = preview,
        .preview = hidden,
        .hidden = candidate,
        .attempts = attempts,
    };
}

} // namespace tetris
