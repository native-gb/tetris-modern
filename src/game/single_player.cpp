#include "game/single_player.hpp"

#include "game/rules.hpp"

#include <algorithm>
#include <array>
#include <cassert>

namespace tetris {
namespace {

constexpr Cell spawn_origin{3, -1};
constexpr int soft_drop_delay = 3;
constexpr std::uint32_t maximum_score = 999'999;
constexpr std::uint32_t modern_maximum_score = 99'999'999;
constexpr int modern_lock_delay = 30;
constexpr int maximum_lock_resets = 15;
constexpr int ultra_duration_steps = 10'751;

struct ClearTiming {
    int flash_delay;
    int flashes;
    int collapse_delay;
};

ClearTiming timing_for(LineClearSpeed speed) {
    switch (speed) {
    case LineClearSpeed::fast:
        return {3, 5, 4};
    case LineClearSpeed::instant:
        return {1, 2, 1};
    case LineClearSpeed::original:
        return {10, 7, 13};
    }
    return {10, 7, 13};
}

int rotation_index(Rotation rotation) {
    switch (rotation) {
    case Rotation::spawn:
        return 0;
    case Rotation::left:
        return 1;
    case Rotation::reverse:
        return 2;
    case Rotation::right:
        return 3;
    }
    return 0;
}

std::array<Cell, 5> kick_tests(PieceKind kind, Rotation from, Rotation to) {
    const int transition = rotation_index(from) * 4 + rotation_index(to);
    if (kind == PieceKind::I) {
        switch (transition) {
        case 1:  return {{{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}}};
        case 4:  return {{{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}}};
        case 6:  return {{{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}}};
        case 9:  return {{{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}}};
        case 11: return {{{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}}};
        case 14: return {{{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}}};
        case 12: return {{{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}}};
        case 3:  return {{{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}}};
        default: return {};
        }
    }
    switch (transition) {
    case 1:  return {{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}};
    case 4:  return {{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}}};
    case 6:  return {{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}}};
    case 9:  return {{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}};
    case 11: return {{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}};
    case 14: return {{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}};
    case 12: return {{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}};
    case 3:  return {{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}};
    default: return {};
    }
}

std::array<Cell, 5> side_pushout_tests() {
    return {{{0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0}}};
}

std::uint32_t score_limit(const GameplayOptions& options) {
    return options.modern_scoring ? modern_maximum_score : maximum_score;
}

} // namespace

void SinglePlayer::start(const GameplayData& source_data, GameRules setup,
                         const StartupRandom& random, std::span<const PieceSpec> fixed_sequence,
                         Buttons initial_buttons, GameplayOptions gameplay) {
    assert(setup.starting_level >= 0 && setup.starting_level <= 9);
    assert(setup.type_b_height >= 0 && setup.type_b_height <= 5);

    // Reset board, pieces, and immediate play state.
    data = source_data;
    rules = setup;
    options = gameplay;
    board.clear();
    wipe_board.clear();
    piece = {};
    preview = {};
    hidden = {};
    held_piece.reset();
    upcoming.clear();
    state = PlayState::falling;
    previous_buttons = initial_buttons;
    pressed_buttons = {};
    paused = false;
    preview_visible = true;
    require_fresh_down_press = false;
    hold_used = false;
    grounded = false;
    last_action_rotation = false;
    spawn_buffer = {};

    // Reset progress and scoring.
    level = rules.starting_level;
    lines = rules.type == GameType::type_a ? 0 : rules.type == GameType::type_b ? 25 : 30;
    score = 0;
    soft_drop_points = 0;
    pending_score_award = 0;
    line_counts = {};
    statistics = {};
    time_remaining_steps = options.challenge == ChallengeMode::ultra ? ultra_duration_steps : 0;

    // Reset movement and animation timers.
    gravity_frames = frames_per_drop(data, level, rules.heart_mode);
    fall_timer = gravity_frames;
    horizontal_repeat_timer = options.horizontal_repeat_delay;
    delay_timer = 0;
    soft_drop_timer = 0;
    soft_drop_steps = 0;
    line_animation_step = 0;
    wipe_step = 0;
    lock_timer = 0;
    lock_resets = 0;
    locks_at_spawn = 0;

    // Reset deterministic randomizer and replay-visible bookkeeping.
    last_random_samples = {};
    last_random_attempts = 0;
    modern_random_state = 0x9E3779B9U;
    for (const RandomSamples& samples : random.pieces) {
        for (const std::uint8_t sample : samples)
            modern_random_state = modern_random_state * 1664525U + sample + 1013904223U;
    }
    random_bag_position = 7;
    random_history = {PieceKind::S, PieceKind::Z, PieceKind::S, PieceKind::Z};
    score_clear_when_wiping = false;
    clearing_rows.clear();
    fixed_pieces.assign(fixed_sequence.begin(), fixed_sequence.end());
    next_fixed_piece = 0;
    step_count = 0;
    events.clear();

    // Generate the initial Type-B or versus garbage rows.
    if (rules.type == GameType::type_b || rules.type == GameType::versus) {
        const int visible_rows = rules.type_b_height * 2;
        for (int source_row = 0; source_row < visible_rows; ++source_row) {
            bool has_hole = false;
            for (int column = 0; column < board_width; ++column) {
                const std::size_t index =
                    static_cast<std::size_t>(source_row * board_width + column);
                const GarbageRandom sample = random.garbage[index];
                const bool occupied = (sample.occupancy & 1U) != 0;
                if (!occupied || (column == board_width - 1 && !has_hole)) {
                    has_hole = true;
                    continue;
                }
                board.set({column, board_height - visible_rows + source_row},
                          garbage_block(sample.appearance));
            }
        }
    }

    // Prime the active, preview, and hidden pieces.
    if (rules.type == GameType::versus && fixed_pieces.size() >= 2) {
        piece = spawn_piece(fixed_pieces[0]);
        preview = fixed_pieces[1];
        hidden = fixed_pieces[1];
        next_fixed_piece = 2;
    } else if (fixed_pieces.empty() && options.randomizer != RandomizerMode::original) {
        fill_modern_queue();
        piece = spawn_piece(upcoming.front());
        upcoming.erase(upcoming.begin());
        fill_modern_queue();
        preview = next_piece(0);
        hidden = next_piece(1);
    } else {
        for (const RandomSamples& samples : random.pieces)
            spawn(samples);
    }
    events.clear();
}

// Run one deterministic Game Boy step.
void SinglePlayer::step(const StepInput& input) {
    // Derive edge-triggered buttons and begin a fresh event list.
    events.clear();
    const Buttons pressed = newly_pressed(input.buttons, previous_buttons);
    pressed_buttons = pressed;
    previous_buttons = input.buttons;
    ++step_count;

    // Handle controls that remain available outside active falling play.
    if (pressed.start)
        set_paused(!paused);
    if (pressed.select) {
        preview_visible = !preview_visible;
        emit(GameEvent::preview_changed, preview_visible ? 1 : 0);
    }

    // Remember one horizontal direction and rotation during entry delays.
    if (options.buffered_inputs && state != PlayState::falling) {
        spawn_buffer.left = input.buttons.left && !input.buttons.right;
        spawn_buffer.right = input.buttons.right && !input.buttons.left;
        const bool counterclockwise =
            counterclockwise_pressed(pressed, options.separate_rotation_inputs);
        const bool clockwise = clockwise_pressed(pressed, options.separate_rotation_inputs);
        if (counterclockwise || clockwise) {
            spawn_buffer.back = !options.separate_rotation_inputs && counterclockwise;
            spawn_buffer.confirm =
                !options.separate_rotation_inputs && !counterclockwise && clockwise;
            spawn_buffer.rotate_counterclockwise =
                options.separate_rotation_inputs && counterclockwise;
            spawn_buffer.rotate_clockwise =
                options.separate_rotation_inputs && !counterclockwise && clockwise;
        }
    }

    // Advance the current gameplay phase.
    if (!paused) {
        if (rules.type == GameType::type_a && options.challenge == ChallengeMode::ultra &&
            state != PlayState::game_over &&
            state != PlayState::complete && time_remaining_steps > 0 &&
            --time_remaining_steps == 0) {
            state = PlayState::complete;
            emit(GameEvent::complete);
        }
        const PlayState state_at_start = state;
        if (state_at_start != PlayState::falling && fall_timer > 0)
            --fall_timer;
        switch (state) {
        case PlayState::falling:
            if (options.hold_piece && pressed.hold) {
                hold(input.random);
            } else {
                rotate(pressed);
                move_horizontally(input.buttons, pressed);
                fall(input.buttons, pressed);
            }
            break;
        case PlayState::locked:
            detect_lines();
            break;
        case PlayState::collapse_pending:
            if (delay_timer == 0)
                collapse_lines();
            break;
        case PlayState::wiping:
            if (wipe_step == 5 && score_clear_when_wiping) {
                const bool live_score = rules.type == GameType::type_a ||
                                        (rules.type == GameType::type_b &&
                                         options.modern_scoring);
                if (live_score) {
                    score = std::min(score_limit(options), score + pending_score_award);
                    emit(GameEvent::score_changed, static_cast<int>(score));
                }
                pending_score_award = 0;
                score_clear_when_wiping = false;
            }
            break;
        case PlayState::resolving:
        case PlayState::clearing:
        case PlayState::game_over:
        case PlayState::complete:
            break;
        }
    }

    // Timers and animations advance after the phase-specific work.
    advance_timers();
    if (!paused)
        advance_animation(input.random);
}

// Apply player movement while a piece is falling.
void SinglePlayer::rotate(const Buttons& pressed) {
    const bool turn_counterclockwise =
        counterclockwise_pressed(pressed, options.separate_rotation_inputs);
    const bool turn_clockwise = clockwise_pressed(pressed, options.separate_rotation_inputs);
    if (!turn_counterclockwise && !turn_clockwise)
        return;
    const Rotation previous = piece.rotation;
    FallingPiece rotated = piece;
    rotated.rotation = turn_counterclockwise ? counterclockwise(previous) : clockwise(previous);
    const auto tests = options.rotation == RotationSystem::srs
                           ? kick_tests(piece.kind, previous, rotated.rotation)
                       : options.rotation == RotationSystem::side_pushout
                           ? side_pushout_tests()
                           : std::array<Cell, 5>{};
    for (const Cell offset : tests) {
        FallingPiece candidate = rotated;
        candidate.origin.x += offset.x;
        candidate.origin.y += offset.y;
        if (collides(candidate))
            continue;
        piece = candidate;
        last_action_rotation = true;
        reset_lock_delay();
        emit(GameEvent::rotated, static_cast<int>(piece.rotation));
        return;
    }
}

void SinglePlayer::move_horizontally(const Buttons& held, const Buttons& pressed) {
    // A new direction moves immediately. Held movement waits for DAS, then
    // repeats at ARR or shifts to the wall when instant autorepeat is enabled.
    const int initial_delay = options.horizontal_repeat_delay;
    const int held_delay = options.horizontal_repeat_interval;
    int direction = 0;
    if (pressed.right) {
        horizontal_repeat_timer = initial_delay;
        direction = 1;
    } else if (held.right) {
        --horizontal_repeat_timer;
        if (horizontal_repeat_timer <= 0) {
            horizontal_repeat_timer = held_delay;
            direction = 1;
        }
    } else if (pressed.left) {
        horizontal_repeat_timer = initial_delay;
        direction = -1;
    } else if (held.left) {
        --horizontal_repeat_timer;
        if (horizontal_repeat_timer <= 0) {
            horizontal_repeat_timer = held_delay;
            direction = -1;
        }
    } else {
        horizontal_repeat_timer = initial_delay;
    }
    if (direction == 0)
        return;
    if (options.instant_autorepeat && !pressed.left && !pressed.right) {
        bool moved = false;
        while (try_move({direction, 0}))
            moved = true;
        if (moved) {
            last_action_rotation = false;
            reset_lock_delay();
        }
        horizontal_repeat_timer = held_delay;
        return;
    }
    if (!try_move({direction, 0})) {
        horizontal_repeat_timer = 1;
        return;
    }
    last_action_rotation = false;
    reset_lock_delay();
}

void SinglePlayer::fall(const Buttons& held, const Buttons& pressed) {
    const bool drop_pressed = pressed.hard_drop;
    if (options.drop_mode != DropMode::soft_only && drop_pressed) {
        const FallingPiece landed = landing_piece();
        const int distance = landed.origin.y - piece.origin.y;
        piece = landed;
        if (distance > 0)
            emit(GameEvent::moved);
        if (options.drop_mode == DropMode::hard) {
            statistics.hard_drop_cells += static_cast<std::uint32_t>(distance);
            if (options.modern_scoring && rules.type != GameType::versus && distance > 0) {
                score = std::min(score_limit(options),
                                 score + static_cast<std::uint32_t>(distance * 2));
                emit(GameEvent::score_changed, static_cast<int>(score));
            }
            land();
        } else {
            if (options.lock_delay)
                begin_lock_delay();
            else
                fall_timer = gravity_frames;
        }
        return;
    }

    // A grounded piece remains movable until its modern lock delay expires.
    if (grounded) {
        FallingPiece below = piece;
        ++below.origin.y;
        if (!collides(below)) {
            grounded = false;
            lock_timer = 0;
            lock_resets = 0;
        } else if (lock_timer > 0) {
            --lock_timer;
            if (lock_timer == 0)
                land();
            return;
        } else {
            land();
            return;
        }
    }

    // Soft drop only engages when down is the sole held direction.
    const bool down_only = held.down && !held.left && !held.right;
    bool soft_drop = down_only;
    if (require_fresh_down_press) {
        soft_drop = down_only && pressed.down;
        if (soft_drop)
            require_fresh_down_press = false;
    }

    // Apply a soft-drop step and award points when it lands.
    if (soft_drop && soft_drop_timer == 0) {
        soft_drop_timer = soft_drop_delay;
        ++soft_drop_steps;
        if (try_move({0, 1})) {
            if (options.modern_scoring && rules.type != GameType::versus) {
                ++soft_drop_points;
                score = std::min(score_limit(options), score + 1U);
                emit(GameEvent::score_changed, static_cast<int>(score));
            }
            return;
        }
        const std::uint32_t earned = options.modern_scoring
                                         ? 0U
                                         : static_cast<std::uint32_t>(
                                               std::max(soft_drop_steps - 1, 0));
        soft_drop_points += earned;
        if (rules.type == GameType::type_a && earned != 0) {
            score = std::min(score_limit(options), score + earned);
            emit(GameEvent::score_changed, static_cast<int>(score));
        }
        soft_drop_steps = 0;
        begin_lock_delay();
        return;
    }
    if (soft_drop)
        return;
    if (!down_only)
        soft_drop_steps = 0;

    // Apply ordinary gravity when its frame timer expires.
    if (fall_timer > 0) {
        --fall_timer;
        return;
    }
    fall_timer = gravity_frames;
    if (!try_move({0, 1})) {
        soft_drop_steps = 0;
        begin_lock_delay();
    }
}

void SinglePlayer::hold(const RandomSamples& random) {
    if (hold_used)
        return;
    const PieceSpec active{piece.kind, Rotation::spawn};
    if (held_piece) {
        const PieceSpec replacement = *held_piece;
        held_piece = active;
        piece = spawn_piece(replacement);
        clearing_rows.clear();
        state = PlayState::falling;
        emit(GameEvent::spawned, static_cast<int>(replacement.kind));
    } else {
        held_piece = active;
        spawn(random);
    }
    hold_used = true;
    grounded = false;
    last_action_rotation = false;
    lock_timer = 0;
    lock_resets = 0;
    ++statistics.holds;
    if (collides(piece))
        lose();
}

void SinglePlayer::reset_lock_delay() {
    if (!options.lock_delay || !grounded)
        return;
    FallingPiece below = piece;
    ++below.origin.y;
    if (!collides(below)) {
        grounded = false;
        lock_timer = 0;
        lock_resets = 0;
        return;
    }
    if (lock_resets >= maximum_lock_resets)
        return;
    lock_timer = modern_lock_delay;
    ++lock_resets;
}

void SinglePlayer::begin_lock_delay() {
    if (!options.lock_delay) {
        land();
        return;
    }
    if (!grounded) {
        grounded = true;
        lock_timer = modern_lock_delay;
        lock_resets = 0;
    }
}

bool SinglePlayer::try_move(Cell distance) {
    FallingPiece moved = piece;
    moved.origin.x += distance.x;
    moved.origin.y += distance.y;
    if (collides(moved))
        return false;
    piece = moved;
    emit(GameEvent::moved, distance.y == 0 ? distance.x : 0);
    return true;
}

bool SinglePlayer::collides(const FallingPiece& candidate) const {
    for (const Cell cell : occupied_cells(data, candidate)) {
        if (board.occupied(cell))
            return true;
    }
    return false;
}

bool SinglePlayer::t_spin() const {
    if (!options.modern_scoring || piece.kind != PieceKind::T || !last_action_rotation)
        return false;
    const Cell center{piece.origin.x + 1, piece.origin.y + 2};
    const std::array corners = {
        Cell{center.x - 1, center.y - 1},
        Cell{center.x + 1, center.y - 1},
        Cell{center.x - 1, center.y + 1},
        Cell{center.x + 1, center.y + 1},
    };
    return std::ranges::count_if(corners, [this](Cell cell) { return board.occupied(cell); }) >= 3;
}

void SinglePlayer::land() {
    // Merge the active piece into the settled board.
    const bool at_spawn = piece.origin == spawn_origin;
    const auto cells = occupied_cells(data, piece);
    const auto blocks = blocks_for(piece);
    for (std::size_t index = 0; index < cells.size(); ++index) {
        const Cell cell = cells[index];
        if (cell.y >= 0 && cell.x >= 0 && cell.x < board_width && cell.y < board_height)
            board.set(cell, blocks[index]);
    }

    // Enter line resolution and detect repeated locks at spawn.
    require_fresh_down_press = true;
    grounded = false;
    lock_timer = 0;
    lock_resets = 0;
    ++statistics.pieces;
    state = PlayState::locked;
    emit(GameEvent::landed, static_cast<int>(piece.kind));
    if (at_spawn && ++locks_at_spawn >= 2)
        lose();
}

// Resolve completed rows, scoring, and board animations.
void SinglePlayer::detect_lines() {
    // Update mode-specific progress and line statistics.
    clearing_rows = board.full_rows();
    const int cleared = static_cast<int>(clearing_rows.size());
    score_clear(cleared);
    if (cleared > 0) {
        if (rules.type == GameType::type_a)
            lines = std::min(lines + cleared, 9'999);
        else
            lines = std::max(lines - cleared, 0);
        if (cleared == 1)
            ++line_counts.singles;
        else if (cleared == 2)
            ++line_counts.doubles;
        else if (cleared == 3)
            ++line_counts.triples;
        else if (cleared == 4)
            ++line_counts.tetrises;
        score_clear_when_wiping = true;
        emit(GameEvent::cleared_lines, cleared);
    }

    // Wait the configured entry delay before flashing or spawning. Line-clear
    // animation contributes its own additional time after this delay.
    state = PlayState::resolving;
    delay_timer = options.entry_delay;
}

void SinglePlayer::score_clear(int cleared) {
    const bool spin = t_spin();
    if (spin)
        ++statistics.t_spins;

    if (!options.modern_scoring) {
        pending_score_award = line_clear_score(cleared, level);
        return;
    }

    if (cleared > 0) {
        ++statistics.combo;
        statistics.maximum_combo = std::max(statistics.maximum_combo, statistics.combo);
    } else {
        statistics.combo = -1;
    }

    constexpr std::array<std::uint32_t, 5> normal = {0, 100, 300, 500, 800};
    constexpr std::array<std::uint32_t, 4> spins = {400, 800, 1'200, 1'600};
    std::uint32_t base = spin ? spins[static_cast<std::size_t>(std::clamp(cleared, 0, 3))]
                              : normal[static_cast<std::size_t>(std::clamp(cleared, 0, 4))];
    const bool difficult = cleared == 4 || (spin && cleared > 0);
    if (difficult && statistics.back_to_back)
        base = base * 3U / 2U;
    if (cleared > 0 && statistics.combo > 0)
        base += static_cast<std::uint32_t>(statistics.combo * 50);
    if (difficult)
        statistics.back_to_back = true;
    else if (cleared > 0)
        statistics.back_to_back = false;

    pending_score_award = base * static_cast<std::uint32_t>(level + 1);
    if (cleared == 0 && pending_score_award > 0 && rules.type != GameType::versus) {
        score = std::min(score_limit(options), score + pending_score_award);
        pending_score_award = 0;
        emit(GameEvent::score_changed, static_cast<int>(score));
    }
}

void SinglePlayer::advance_timers() {
    if (delay_timer > 0)
        --delay_timer;
    if (soft_drop_timer > 0)
        --soft_drop_timer;
}

void SinglePlayer::advance_animation(const RandomSamples& random) {
    const ClearTiming timing = timing_for(line_clear_speed);

    // Leave the resolution delay by spawning or starting the flash sequence.
    if (state == PlayState::resolving && delay_timer == 0) {
        if (clearing_rows.empty()) {
            spawn(random);
            --fall_timer;
            return;
        }
        state = PlayState::clearing;
        line_animation_step = 1;
        delay_timer = timing.flash_delay;
        return;
    }

    // Advance line flashes until the board can collapse.
    if (state == PlayState::clearing && delay_timer == 0) {
        ++line_animation_step;
        if (line_animation_step >= timing.flashes) {
            line_animation_step = 0;
            state = PlayState::collapse_pending;
            delay_timer = timing.collapse_delay;
        } else {
            delay_timer = timing.flash_delay;
        }
        return;
    }

    // Reveal the collapsed board one row at a time.
    if (state == PlayState::wiping)
        advance_wipe(random);
}

void SinglePlayer::collapse_lines() {
    wipe_board = board;
    board.remove_rows(clearing_rows);
    state = PlayState::wiping;
    wipe_step = 2;
}

void SinglePlayer::advance_wipe(const RandomSamples& random) {
    // Copy one collapsed row into the rendering board.
    const int display_row = 19 - wipe_step;
    if (display_row >= 0 && display_row < board_height) {
        for (int column = 0; column < board_width; ++column)
            wipe_board.set({column, display_row}, board.at({column, display_row}));
    }
    if (wipe_step == 16)
        update_level();
    if (wipe_step < 19) {
        ++wipe_step;
        return;
    }

    // Complete Type-B/versus boards or spawn the next Type-A piece.
    wipe_step = 0;
    if (rules.type != GameType::type_a && lines == 0) {
        if (rules.type == GameType::type_b) {
            if (!options.modern_scoring) {
                const std::uint32_t line_score =
                    line_counts.singles * line_clear_score(1, level) +
                    line_counts.doubles * line_clear_score(2, level) +
                    line_counts.triples * line_clear_score(3, level) +
                    line_counts.tetrises * line_clear_score(4, level);
                score = std::min(maximum_score, line_score + soft_drop_points);
                emit(GameEvent::score_changed, static_cast<int>(score));
            }
        }
        state = PlayState::complete;
        emit(GameEvent::complete);
        return;
    }
    if (rules.type == GameType::type_a && options.challenge == ChallengeMode::sprint &&
        lines >= 40) {
        state = PlayState::complete;
        emit(GameEvent::complete);
        return;
    }
    spawn(random);
    --fall_timer;
}

// Advance progress and create the next falling piece.
void SinglePlayer::update_level() {
    if (rules.type != GameType::type_a || level >= 20)
        return;
    const int next = std::min(std::max(rules.starting_level, lines / 10), 20);
    if (next <= level)
        return;
    level = next;
    gravity_frames = frames_per_drop(data, level, rules.heart_mode);
    emit(GameEvent::level_changed, level);
}

void SinglePlayer::spawn(const RandomSamples& random) {
    // Pull from a fixed versus/demo sequence or advance the random queue.
    PieceSpec active = preview;
    if (next_fixed_piece < fixed_pieces.size()) {
        preview = fixed_pieces[next_fixed_piece++];
    } else if (fixed_pieces.empty() && options.randomizer != RandomizerMode::original) {
        fill_modern_queue();
        active = upcoming.front();
        upcoming.erase(upcoming.begin());
        fill_modern_queue();
        preview = next_piece(0);
        hidden = next_piece(1);
    } else {
        const PieceQueue queue = advance_piece_queue(preview, hidden, random);
        active = queue.active;
        preview = queue.preview;
        hidden = queue.hidden;
        last_random_samples = random;
        last_random_attempts = queue.attempts;
    }

    // Reset falling state around the newly active piece.
    piece = spawn_piece(active);
    fall_timer = gravity_frames;
    horizontal_repeat_timer = options.horizontal_repeat_delay;
    grounded = false;
    hold_used = false;
    last_action_rotation = false;
    lock_timer = 0;
    lock_resets = 0;
    clearing_rows.clear();
    state = PlayState::falling;
    emit(GameEvent::spawned, static_cast<int>(active.kind));

    // Apply buffered entry movement after the new piece exists.
    const Buttons buffered = spawn_buffer;
    spawn_buffer = {};
    if (options.buffered_inputs) {
        rotate(buffered);
        if (buffered.left || buffered.right)
            move_horizontally(buffered, buffered);
    }
}

void SinglePlayer::fill_modern_queue() {
    const std::size_t target = static_cast<std::size_t>(std::max(options.next_previews + 1, 6));
    while (upcoming.size() < target)
        upcoming.push_back(generate_modern_piece());
}

PieceSpec SinglePlayer::generate_modern_piece() {
    if (options.randomizer == RandomizerMode::seven_bag) {
        if (random_bag_position >= static_cast<int>(random_bag.size())) {
            random_bag = {PieceKind::L, PieceKind::J, PieceKind::I, PieceKind::O,
                          PieceKind::S, PieceKind::Z, PieceKind::T};
            for (int index = 6; index > 0; --index) {
                const int other = static_cast<int>(next_random() % static_cast<std::uint32_t>(index + 1));
                std::swap(random_bag[static_cast<std::size_t>(index)],
                          random_bag[static_cast<std::size_t>(other)]);
            }
            random_bag_position = 0;
        }
        return {.kind = random_bag[static_cast<std::size_t>(random_bag_position++)]};
    }

    PieceKind candidate = PieceKind::L;
    for (int attempt = 0; attempt < 6; ++attempt) {
        candidate = static_cast<PieceKind>(next_random() % 7U);
        const bool repeated = std::ranges::find(random_history, candidate) != random_history.end();
        if (!repeated || attempt == 5)
            break;
    }
    std::rotate(random_history.begin(), random_history.begin() + 1, random_history.end());
    random_history.back() = candidate;
    return {.kind = candidate};
}

std::uint32_t SinglePlayer::next_random() {
    modern_random_state = modern_random_state * 1664525U + 1013904223U;
    return modern_random_state;
}

void SinglePlayer::lose() {
    state = PlayState::game_over;
    emit(GameEvent::game_over);
}

// Small public operations and developer inspection hooks.
void SinglePlayer::emit(GameEvent event, int value) {
    events.push_back({event, value});
}
void SinglePlayer::set_paused(bool value) {
    if (paused == value)
        return;
    paused = value;
    emit(GameEvent::paused, value ? 1 : 0);
}
void SinglePlayer::add_garbage(int rows, int hole) {
    if (state != PlayState::falling)
        return;
    board.add_garbage(rows, hole);
    emit(GameEvent::garbage_applied, rows);
    if (collides(piece))
        lose();
}

const Board& SinglePlayer::visible_board() const {
    return state == PlayState::wiping ? wipe_board : board;
}
int SinglePlayer::animation_step() const {
    return state == PlayState::wiping ? wipe_step : line_animation_step;
}
FallingPiece SinglePlayer::landing_piece() const {
    FallingPiece landed = piece;
    while (true) {
        FallingPiece below = landed;
        ++below.origin.y;
        if (collides(below))
            return landed;
        landed = below;
    }
}
PieceSpec SinglePlayer::next_piece(int index) const {
    if (index >= 0 && index < static_cast<int>(upcoming.size()))
        return upcoming[static_cast<std::size_t>(index)];
    return index == 0 ? preview : hidden;
}
void SinglePlayer::debug_place_piece(FallingPiece value) {
    piece = value;
    state = PlayState::falling;
}
void SinglePlayer::debug_set_state(PlayState value) {
    state = value;
}
void SinglePlayer::debug_set_score(std::uint32_t value) {
    score = std::min(value, score_limit(options));
}
void SinglePlayer::debug_set_results(LineCounts counts, std::uint32_t points) {
    line_counts = counts;
    soft_drop_points = points;
}

} // namespace tetris
