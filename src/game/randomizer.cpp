#include "game/randomizer.hpp"

namespace tetris {

PieceKind piece_from_divider(std::uint8_t divider) {
    // The hardware counter decrements before the addition loop. Zero therefore
    // wraps to 255 additions rather than selecting the first piece directly.
    const unsigned int additions = divider == 0 ? 255U : divider - 1U;
    return static_cast<PieceKind>(additions % 7U);
}

PieceQueue advance_piece_queue(PieceSpec preview, PieceSpec hidden,
                               const RandomSamples& samples) {
    PieceSpec candidate{};
    int attempts = 0;

    for (const std::uint8_t sample : samples) {
        candidate = {.kind = piece_from_divider(sample)};
        ++attempts;
        const auto history = static_cast<unsigned int>(preview.kind) |
                             static_cast<unsigned int>(hidden.kind) |
                             static_cast<unsigned int>(candidate.kind);
        if (attempts == 3 || history != static_cast<unsigned int>(preview.kind))
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
