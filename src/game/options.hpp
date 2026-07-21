#pragma once

namespace tetris {

enum class DropMode {
    soft_only,
    sonic,
    hard,
};

enum class RandomizerMode {
    original,
    seven_bag,
    history,
};

enum class RotationSystem {
    original,
    side_pushout,
    srs,
};

enum class ChallengeMode {
    marathon,
    sprint,
    ultra,
};

struct GameplayOptions {
    DropMode drop_mode{DropMode::soft_only};
    RandomizerMode randomizer{RandomizerMode::original};
    RotationSystem rotation{RotationSystem::original};
    ChallengeMode challenge{ChallengeMode::marathon};
    int next_previews{1};
    int horizontal_repeat_delay{23};
    int horizontal_repeat_interval{9};
    int entry_delay{3};
    bool instant_autorepeat{};
    bool separate_rotation_inputs{};
    bool buffered_inputs{};
    bool ghost_piece{};
    bool hold_piece{};
    bool lock_delay{};
    bool modern_scoring{};
    bool quick_restart{};

    bool operator==(const GameplayOptions&) const = default;
};

} // namespace tetris
