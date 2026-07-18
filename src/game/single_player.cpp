#include "game/single_player.hpp"

#include "game/rules.hpp"

#include <algorithm>
#include <array>
#include <cassert>

namespace tetris {
namespace {

constexpr Cell spawn_origin{3, -1};
constexpr int initial_repeat_delay = 23;
constexpr int repeat_delay = 9;
constexpr int soft_drop_delay = 3;
constexpr std::uint32_t maximum_score = 999'999;

struct ClearTiming {
    int flash_delay;
    int flashes;
    int collapse_delay;
};

ClearTiming timing_for(LineClearSpeed speed) {
    switch (speed) {
    case LineClearSpeed::fast: return {3, 5, 4};
    case LineClearSpeed::instant: return {1, 2, 1};
    case LineClearSpeed::original: return {10, 7, 13};
    }
    return {10, 7, 13};
}

} // namespace

void SinglePlayer::start(GameRules rules, const StartupRandom& random,
                         std::span<const PieceSpec> fixed_pieces, Buttons initial_buttons) {
    assert(rules.starting_level >= 0 && rules.starting_level <= 9);
    assert(rules.type_b_height >= 0 && rules.type_b_height <= 5);

    rules_ = rules;
    board_.clear();
    wipe_board_.clear();
    piece_ = {};
    preview_ = {};
    hidden_ = {};
    state_ = PlayState::falling;
    previous_buttons_ = initial_buttons;
    pressed_buttons_ = {};
    paused_ = false;
    preview_visible_ = true;
    require_fresh_down_press_ = false;
    level_ = rules.starting_level;
    lines_ = rules.type == GameType::type_a ? 0 : rules.type == GameType::type_b ? 25 : 30;
    score_ = 0;
    soft_drop_points_ = 0;
    line_counts_ = {};
    gravity_frames_ = frames_per_drop(level_, rules.heart_mode);
    fall_timer_ = gravity_frames_;
    horizontal_repeat_timer_ = initial_repeat_delay;
    delay_timer_ = 0;
    soft_drop_timer_ = 0;
    soft_drop_steps_ = 0;
    line_animation_step_ = 0;
    wipe_step_ = 0;
    locks_at_spawn_ = 0;
    last_random_samples_ = {};
    last_random_attempts_ = 0;
    score_clear_when_wiping_ = false;
    clearing_rows_.clear();
    fixed_pieces_.assign(fixed_pieces.begin(), fixed_pieces.end());
    next_fixed_piece_ = 0;
    tick_count_ = 0;
    events_.clear();

    if (rules_.type == GameType::type_b || rules_.type == GameType::versus) {
        const int visible_rows = rules_.type_b_height * 2;
        for (int source_row = 0; source_row < visible_rows; ++source_row) {
            bool has_hole = false;
            for (int column = 0; column < board_width; ++column) {
                const std::size_t index = static_cast<std::size_t>(source_row * board_width + column);
                const GarbageRandom sample = random.garbage[index];
                const bool occupied = (sample.occupancy & 1U) != 0;
                if (!occupied || (column == board_width - 1 && !has_hole)) {
                    has_hole = true;
                    continue;
                }
                board_.set({column, board_height - visible_rows + source_row},
                           garbage_block(sample.appearance));
            }
        }
    }

    if (rules_.type == GameType::versus && fixed_pieces_.size() >= 2) {
        piece_ = spawn_piece(fixed_pieces_[0]);
        preview_ = fixed_pieces_[1];
        hidden_ = fixed_pieces_[1];
        next_fixed_piece_ = 2;
    } else {
        for (const RandomSamples& samples : random.pieces)
            spawn(samples);
    }
    events_.clear();
}

void SinglePlayer::tick(const TickInput& input) {
    events_.clear();
    const Buttons pressed = newly_pressed(input.buttons, previous_buttons_);
    pressed_buttons_ = pressed;
    previous_buttons_ = input.buttons;
    ++tick_count_;

    if (pressed.start)
        set_paused(!paused_);
    if (pressed.select) {
        preview_visible_ = !preview_visible_;
        emit(GameEvent::preview_changed, preview_visible_ ? 1 : 0);
    }

    if (!paused_) {
        const PlayState state_at_start = state_;
        if (state_at_start != PlayState::falling && fall_timer_ > 0)
            --fall_timer_;
        switch (state_) {
        case PlayState::falling:
            rotate(pressed);
            move_horizontally(input.buttons, pressed);
            fall(input.buttons, pressed);
            break;
        case PlayState::locked:
            detect_lines();
            break;
        case PlayState::collapse_pending:
            if (delay_timer_ == 0)
                collapse_lines();
            break;
        case PlayState::wiping:
            if (wipe_step_ == 5 && score_clear_when_wiping_) {
                if (rules_.type == GameType::type_a) {
                    score_ = std::min(maximum_score,
                        score_ + line_clear_score(static_cast<int>(clearing_rows_.size()), level_));
                    emit(GameEvent::score_changed, static_cast<int>(score_));
                }
                score_clear_when_wiping_ = false;
            }
            break;
        case PlayState::resolving:
        case PlayState::clearing:
        case PlayState::game_over:
        case PlayState::complete:
            break;
        }
    }

    advance_timers();
    if (!paused_)
        advance_animation(input.random);
}

void SinglePlayer::rotate(const Buttons& pressed) {
    if (!pressed.rotate_left && !pressed.rotate_right)
        return;
    const Rotation previous = piece_.rotation;
    piece_.rotation = pressed.rotate_left ? counterclockwise(previous) : clockwise(previous);
    if (collides(piece_)) {
        piece_.rotation = previous;
        return;
    }
    emit(GameEvent::rotated, static_cast<int>(piece_.rotation));
}

void SinglePlayer::move_horizontally(const Buttons& held, const Buttons& pressed) {
    int direction = 0;
    if (pressed.right) {
        horizontal_repeat_timer_ = initial_repeat_delay;
        direction = 1;
    } else if (held.right) {
        --horizontal_repeat_timer_;
        if (horizontal_repeat_timer_ == 0) {
            horizontal_repeat_timer_ = repeat_delay;
            direction = 1;
        }
    } else if (pressed.left) {
        horizontal_repeat_timer_ = initial_repeat_delay;
        direction = -1;
    } else if (held.left) {
        --horizontal_repeat_timer_;
        if (horizontal_repeat_timer_ == 0) {
            horizontal_repeat_timer_ = repeat_delay;
            direction = -1;
        }
    } else {
        horizontal_repeat_timer_ = initial_repeat_delay;
    }
    if (direction != 0 && !try_move({direction, 0}))
        horizontal_repeat_timer_ = 1;
}

void SinglePlayer::fall(const Buttons& held, const Buttons& pressed) {
    const bool down_only = held.down && !held.left && !held.right;
    bool soft_drop = down_only;
    if (require_fresh_down_press_) {
        soft_drop = down_only && pressed.down;
        if (soft_drop)
            require_fresh_down_press_ = false;
    }

    if (soft_drop && soft_drop_timer_ == 0) {
        soft_drop_timer_ = soft_drop_delay;
        ++soft_drop_steps_;
        if (try_move({0, 1}))
            return;
        const std::uint32_t earned = static_cast<std::uint32_t>(std::max(soft_drop_steps_ - 1, 0));
        soft_drop_points_ += earned;
        if (rules_.type == GameType::type_a && earned != 0) {
            score_ = std::min(maximum_score, score_ + earned);
            emit(GameEvent::score_changed, static_cast<int>(score_));
        }
        soft_drop_steps_ = 0;
        land();
        return;
    }
    if (soft_drop)
        return;
    if (!down_only)
        soft_drop_steps_ = 0;
    if (fall_timer_ > 0) {
        --fall_timer_;
        return;
    }
    fall_timer_ = gravity_frames_;
    if (!try_move({0, 1})) {
        soft_drop_steps_ = 0;
        land();
    }
}

bool SinglePlayer::try_move(Cell distance) {
    FallingPiece moved = piece_;
    moved.origin.x += distance.x;
    moved.origin.y += distance.y;
    if (collides(moved))
        return false;
    piece_ = moved;
    emit(GameEvent::moved, distance.y == 0 ? distance.x : 0);
    return true;
}

bool SinglePlayer::collides(const FallingPiece& piece) const {
    for (const Cell cell : occupied_cells(piece)) {
        if (board_.occupied(cell))
            return true;
    }
    return false;
}

void SinglePlayer::land() {
    const bool at_spawn = piece_.origin == spawn_origin;
    const auto cells = occupied_cells(piece_);
    const auto blocks = blocks_for(piece_);
    for (std::size_t index = 0; index < cells.size(); ++index) {
        const Cell cell = cells[index];
        if (cell.y >= 0 && cell.x >= 0 && cell.x < board_width && cell.y < board_height)
            board_.set(cell, blocks[index]);
    }
    require_fresh_down_press_ = true;
    state_ = PlayState::locked;
    emit(GameEvent::landed, static_cast<int>(piece_.kind));
    if (at_spawn && ++locks_at_spawn_ >= 2)
        lose();
}

void SinglePlayer::detect_lines() {
    clearing_rows_ = board_.full_rows();
    const int cleared = static_cast<int>(clearing_rows_.size());
    if (cleared > 0) {
        if (rules_.type == GameType::type_a)
            lines_ = std::min(lines_ + cleared, 9'999);
        else
            lines_ = std::max(lines_ - cleared, 0);
        if (cleared == 1) ++line_counts_.singles;
        else if (cleared == 2) ++line_counts_.doubles;
        else if (cleared == 3) ++line_counts_.triples;
        else if (cleared == 4) ++line_counts_.tetrises;
        score_clear_when_wiping_ = true;
        emit(GameEvent::cleared_lines, cleared);
    }
    state_ = PlayState::resolving;
    delay_timer_ = 3;
}

void SinglePlayer::advance_timers() {
    if (delay_timer_ > 0)
        --delay_timer_;
    if (soft_drop_timer_ > 0)
        --soft_drop_timer_;
}

void SinglePlayer::advance_animation(const RandomSamples& random) {
    const ClearTiming timing = timing_for(line_clear_speed_);
    if (state_ == PlayState::resolving && delay_timer_ == 0) {
        if (clearing_rows_.empty()) {
            spawn(random);
            --fall_timer_;
            return;
        }
        state_ = PlayState::clearing;
        line_animation_step_ = 1;
        delay_timer_ = timing.flash_delay;
        return;
    }
    if (state_ == PlayState::clearing && delay_timer_ == 0) {
        ++line_animation_step_;
        if (line_animation_step_ >= timing.flashes) {
            line_animation_step_ = 0;
            state_ = PlayState::collapse_pending;
            delay_timer_ = timing.collapse_delay;
        } else {
            delay_timer_ = timing.flash_delay;
        }
        return;
    }
    if (state_ == PlayState::wiping)
        advance_wipe(random);
}

void SinglePlayer::collapse_lines() {
    wipe_board_ = board_;
    board_.remove_rows(clearing_rows_);
    state_ = PlayState::wiping;
    wipe_step_ = 2;
}

void SinglePlayer::advance_wipe(const RandomSamples& random) {
    const int display_row = 19 - wipe_step_;
    if (display_row >= 0 && display_row < board_height) {
        for (int column = 0; column < board_width; ++column)
            wipe_board_.set({column, display_row}, board_.at({column, display_row}));
    }
    if (wipe_step_ == 16)
        update_level();
    if (wipe_step_ < 19) {
        ++wipe_step_;
        return;
    }

    wipe_step_ = 0;
    if (rules_.type != GameType::type_a && lines_ == 0) {
        if (rules_.type == GameType::type_b) {
            const std::uint32_t line_score =
                line_counts_.singles * line_clear_score(1, level_) +
                line_counts_.doubles * line_clear_score(2, level_) +
                line_counts_.triples * line_clear_score(3, level_) +
                line_counts_.tetrises * line_clear_score(4, level_);
            score_ = std::min(maximum_score, line_score + soft_drop_points_);
            emit(GameEvent::score_changed, static_cast<int>(score_));
        }
        state_ = PlayState::complete;
        emit(GameEvent::complete);
        return;
    }
    spawn(random);
    --fall_timer_;
}

void SinglePlayer::update_level() {
    if (rules_.type != GameType::type_a || level_ >= 20)
        return;
    const int next = std::min(std::max(rules_.starting_level, lines_ / 10), 20);
    if (next <= level_)
        return;
    level_ = next;
    gravity_frames_ = frames_per_drop(level_, rules_.heart_mode);
    emit(GameEvent::level_changed, level_);
}

void SinglePlayer::spawn(const RandomSamples& random) {
    PieceSpec active = preview_;
    if (next_fixed_piece_ < fixed_pieces_.size()) {
        preview_ = fixed_pieces_[next_fixed_piece_++];
    } else {
        const PieceQueue queue = advance_piece_queue(preview_, hidden_, random);
        active = queue.active;
        preview_ = queue.preview;
        hidden_ = queue.hidden;
        last_random_samples_ = random;
        last_random_attempts_ = queue.attempts;
    }
    piece_ = spawn_piece(active);
    fall_timer_ = gravity_frames_;
    clearing_rows_.clear();
    state_ = PlayState::falling;
    emit(GameEvent::spawned, static_cast<int>(active.kind));
}

void SinglePlayer::lose() {
    state_ = PlayState::game_over;
    emit(GameEvent::game_over);
}

void SinglePlayer::emit(GameEvent event, int value) { events_.push_back({event, value}); }
void SinglePlayer::set_line_clear_speed(LineClearSpeed speed) { line_clear_speed_ = speed; }
void SinglePlayer::set_paused(bool paused) {
    if (paused_ == paused)
        return;
    paused_ = paused;
    emit(GameEvent::paused, paused ? 1 : 0);
}
void SinglePlayer::add_garbage(int rows, int hole) {
    if (state_ != PlayState::falling)
        return;
    board_.add_garbage(rows, hole);
    emit(GameEvent::garbage_applied, rows);
    if (collides(piece_))
        lose();
}

const Board& SinglePlayer::board() const { return board_; }
const Board& SinglePlayer::presentation_board() const {
    return state_ == PlayState::wiping ? wipe_board_ : board_;
}
Board& SinglePlayer::edit_board() { return board_; }
const FallingPiece& SinglePlayer::piece() const { return piece_; }
PieceSpec SinglePlayer::preview() const { return preview_; }
PieceSpec SinglePlayer::hidden_piece() const { return hidden_; }
PlayState SinglePlayer::state() const { return state_; }
GameRules SinglePlayer::rules() const { return rules_; }
int SinglePlayer::level() const { return level_; }
int SinglePlayer::lines() const { return lines_; }
std::uint32_t SinglePlayer::score() const { return score_; }
std::uint32_t SinglePlayer::soft_drop_points() const { return soft_drop_points_; }
const LineCounts& SinglePlayer::line_counts() const { return line_counts_; }
bool SinglePlayer::paused() const { return paused_; }
bool SinglePlayer::preview_visible() const { return preview_visible_; }
std::span<const int> SinglePlayer::clearing_rows() const { return clearing_rows_; }
std::span<const Event> SinglePlayer::events() const { return events_; }
void SinglePlayer::clear_events() { events_.clear(); }
int SinglePlayer::animation_step() const {
    return state_ == PlayState::wiping ? wipe_step_ : line_animation_step_;
}
std::uint64_t SinglePlayer::tick_count() const { return tick_count_; }
int SinglePlayer::gravity_frames() const { return gravity_frames_; }
int SinglePlayer::drop_timer() const { return fall_timer_; }
int SinglePlayer::horizontal_repeat_timer() const { return horizontal_repeat_timer_; }
Buttons SinglePlayer::held_buttons() const { return previous_buttons_; }
Buttons SinglePlayer::pressed_buttons() const { return pressed_buttons_; }
RandomSamples SinglePlayer::last_random_samples() const { return last_random_samples_; }
int SinglePlayer::last_random_attempts() const { return last_random_attempts_; }
std::size_t SinglePlayer::fixed_pieces_consumed() const { return next_fixed_piece_; }
int SinglePlayer::locks_at_spawn() const { return locks_at_spawn_; }
void SinglePlayer::debug_place_piece(FallingPiece piece) { piece_ = piece; state_ = PlayState::falling; }
void SinglePlayer::debug_set_state(PlayState state) { state_ = state; }
void SinglePlayer::debug_set_score(std::uint32_t score) { score_ = std::min(score, maximum_score); }
void SinglePlayer::debug_set_results(LineCounts counts, std::uint32_t soft_drop_points) {
    line_counts_ = counts;
    soft_drop_points_ = soft_drop_points;
}

} // namespace tetris
