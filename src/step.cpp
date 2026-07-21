#include "step.hpp"

#include <array>
#include <optional>
#include <utility>

namespace tetris {
namespace {

std::uint8_t random_byte(std::uint8_t& state) {
    state = static_cast<std::uint8_t>(state * 17U + 31U);
    return state;
}

RandomSamples random_samples(std::uint8_t& state) {
    return {random_byte(state), random_byte(state), random_byte(state)};
}

StartupRandom startup_random(std::uint8_t& state) {
    StartupRandom random;
    for (RandomSamples& samples : random.pieces)
        samples = random_samples(state);
    for (GarbageRandom& cell : random.garbage)
        cell = {random_byte(state), random_byte(state)};
    return random;
}

VersusRandom versus_random(std::uint8_t& state) {
    std::array<RandomSamples, versus_piece_count + 3> samples{};
    for (RandomSamples& sample : samples)
        sample = random_samples(state);

    VersusRandom random;
    random.pieces = make_versus_piece_sequence(samples);
    for (GarbageRandom& cell : random.garbage)
        cell = {random_byte(state), random_byte(state)};
    return random;
}

GameInput make_input(std::uint8_t& state, Buttons player_one, Buttons player_two) {
    return {player_one, player_two, random_samples(state), startup_random(state),
            versus_random(state)};
}

SinglePlayer& primary_game(GameState& game) {
    if (game.screen == Screen::versus_gameplay)
        return game.versus.players[0].game;
    return game.single_player;
}

} // namespace

void process_replay_request(DebugUi& debug, Replay& replay, GameState& game,
                            std::uint8_t& random_state, const std::string& rom_sha1) {
    const ReplayRequest request = debug.replay_request;
    debug.replay_request = ReplayRequest::none;

    if (request == ReplayRequest::record) {
        replay.begin_recording(game,
                               ReplayIdentity{.rom_sha1 = rom_sha1,
                                              .pacing = game.line_clear_speed,
                                              .starting_screen = game.screen,
                                              .rules = game.single_player.rules,
                                              .gameplay = game.gameplay_options},
                               random_state);
    } else if (request == ReplayRequest::stop) {
        replay.stop(random_state);
    } else if (request == ReplayRequest::play) {
        (void)replay.rewind(game, random_state);
    } else if (request == ReplayRequest::clear) {
        replay.clear();
    }
}

void apply_debug_command(DebugUi& debug, Replay& replay, GameState& game,
                         const settings::Settings& settings, std::uint8_t& random_state) {
    const DebugCommand command = debug.command;
    debug.command = {};
    if (command.type == DebugCommandType::none)
        return;

    replay.stop(random_state);
    if (command.type == DebugCommandType::start_type_a ||
        command.type == DebugCommandType::start_type_b) {
        const GameType type =
            command.type == DebugCommandType::start_type_a ? GameType::type_a : GameType::type_b;
        start_session(game,
                      {.type = type,
                       .starting_level = command.first,
                       .type_b_height = type == GameType::type_b ? command.second : 0},
                      startup_random(random_state));
        set_line_clear_speed(game, settings.line_clear_speed);
        return;
    }

    SinglePlayer& player_game = primary_game(game);
    if (command.type == DebugCommandType::clear_board) {
        player_game.board.clear();
    } else if (command.type == DebugCommandType::prepare_tetris) {
        player_game.board.clear();
        for (int row = 14; row < board_height; ++row) {
            for (int column = 0; column < board_width; ++column) {
                if (column != 6)
                    player_game.board.set({column, row}, Block::j);
            }
        }
        player_game.debug_place_piece(
            {.kind = PieceKind::I, .rotation = Rotation::right, .origin = {5, 14}});
    } else if (command.type == DebugCommandType::set_score) {
        player_game.debug_set_score(command.value);
    } else if (command.type == DebugCommandType::force_game_over) {
        player_game.debug_set_state(PlayState::game_over);
    } else if (command.type == DebugCommandType::force_complete) {
        player_game.debug_set_state(PlayState::complete);
    }
}

void step(GameState& game, Replay& replay, video::EffectState& effects,
          const settings::Settings& settings, audio::Output& audio, GameInput& last_input,
          std::uint8_t& random_state, Buttons player_one, Buttons player_two) {
    // Select recorded or live input, then dispatch one game simulation step.
    if (replay.playing) {
        const std::optional<GameInput> input = replay.next(random_state);
        if (!input)
            return;
        last_input = *input;
        step_game(game, *input);
    } else {
        GameInput input = make_input(random_state, player_one, player_two);
        last_input = input;
        step_game(game, input);
        replay.append(std::move(input), random_state);
    }

    // Convert emitted game events into video effects.
    if (game.screen == Screen::versus_gameplay || game.screen == Screen::versus_round_result ||
        game.screen == Screen::versus_match_result) {
        video::advance(effects, game.versus.players[0].game.events,
                       game.versus.players[1].game.events, settings);
    } else {
        video::advance(effects, game.single_player.events, settings);
    }

    // Audio consumes the same completed game step.
    audio.step(game, random_state);
}

} // namespace tetris
