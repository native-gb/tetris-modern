#include "game/randomizer.hpp"

namespace tetris {
namespace {

bool original_history_rejects(PieceKind preview, PieceKind hidden,
                              PieceKind candidate) {
    // This intentionally preserves the original generator's unusual bitwise
    // history rule; equality with the preview means the candidate is rerolled.
    const auto merged = static_cast<unsigned int>(preview) |
                        static_cast<unsigned int>(hidden) |
                        static_cast<unsigned int>(candidate);
    return merged == static_cast<unsigned int>(preview);
}

} // namespace

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
        if (attempts == 3 ||
            !original_history_rejects(preview.kind, hidden.kind, candidate.kind))
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
