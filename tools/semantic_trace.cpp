#include "audio/director.hpp"
#include "content/catalog.hpp"
#include "content/rom.hpp"
#include "game/state.hpp"

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace tetris;

struct InputEvent {
    int frame{};
    std::uint8_t keys{};
};

struct Setup {
    int apply_step{};
    GameRules rules{};
    std::optional<std::uint32_t> score;
    std::optional<PlayState> state;
};

struct Options {
    std::filesystem::path rom;
    std::filesystem::path input;
    std::filesystem::path output;
    std::filesystem::path setup;
    int frames{};
};

std::string_view trim(std::string_view text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
        return {};
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

bool integer(std::string_view text, int& value) {
    text = trim(text);
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool parse_options(int argc, char** argv, Options& options) {
    for (int index = 1; index < argc; ++index) {
        if (index + 1 >= argc)
            return false;
        const std::string_view argument = argv[index] == nullptr ? "" : argv[index];
        const char* value = argv[++index] == nullptr ? "" : argv[index];
        if (argument == "--rom")
            options.rom = value;
        else if (argument == "--input")
            options.input = value;
        else if (argument == "--output")
            options.output = value;
        else if (argument == "--frames") {
            if (!integer(value, options.frames))
                return false;
        } else if (argument == "--setup") {
            options.setup = value;
        } else {
            return false;
        }
    }
    return !options.rom.empty() && !options.input.empty() && !options.output.empty() &&
           options.frames > 0;
}

std::uint8_t key_bit(std::string_view name) {
    if (name == "A")
        return 1U << 0U;
    if (name == "B")
        return 1U << 1U;
    if (name == "SELECT")
        return 1U << 2U;
    if (name == "START")
        return 1U << 3U;
    if (name == "RIGHT")
        return 1U << 4U;
    if (name == "LEFT")
        return 1U << 5U;
    if (name == "UP")
        return 1U << 6U;
    if (name == "DOWN")
        return 1U << 7U;
    return 0;
}

bool parse_keys(std::string_view text, std::uint8_t& keys) {
    keys = 0;
    text = trim(text);
    if (text.empty() || text == "NONE")
        return true;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t separator = text.find_first_of("+| ", start);
        const std::string_view name = trim(text.substr(start, separator - start));
        const std::uint8_t bit = key_bit(name);
        if (bit == 0)
            return false;
        keys = static_cast<std::uint8_t>(keys | bit);
        if (separator == std::string_view::npos)
            return true;
        start = separator + 1;
        while (start < text.size() && text[start] == ' ')
            ++start;
    }
    return true;
}

bool load_input(const std::filesystem::path& path, std::vector<InputEvent>& events,
                std::string& error) {
    std::ifstream input(path);
    if (!input) {
        error = "could not open input script: " + path.string();
        return false;
    }
    std::string line;
    int line_number = 0;
    int previous_frame = -1;
    while (std::getline(input, line)) {
        ++line_number;
        const std::size_t comment = line.find('#');
        const std::string_view content = trim(std::string_view(line).substr(0, comment));
        if (content.empty())
            continue;
        const std::size_t comma = content.find(',');
        InputEvent event;
        if (comma == std::string_view::npos || !integer(content.substr(0, comma), event.frame) ||
            event.frame < 0 || event.frame <= previous_frame ||
            !parse_keys(content.substr(comma + 1), event.keys)) {
            error = "invalid input event on line " + std::to_string(line_number);
            return false;
        }
        events.push_back(event);
        previous_frame = event.frame;
    }
    return true;
}

Buttons buttons(std::uint8_t keys) {
    return {
        .left = (keys & (1U << 5U)) != 0,
        .right = (keys & (1U << 4U)) != 0,
        .up = (keys & (1U << 6U)) != 0,
        .down = (keys & (1U << 7U)) != 0,
        .back = (keys & (1U << 1U)) != 0,
        .confirm = (keys & (1U << 0U)) != 0,
        .start = (keys & (1U << 3U)) != 0,
        .select = (keys & (1U << 2U)) != 0,
    };
}

std::uint8_t sample(int frame, int salt) {
    const unsigned int value =
        static_cast<unsigned int>(frame) * 73U + static_cast<unsigned int>(salt) * 151U;
    return static_cast<std::uint8_t>(value & 0xFFU);
}

RandomSamples random_samples(int frame, int salt) {
    return {sample(frame, salt), sample(frame, salt + 1), sample(frame, salt + 2)};
}

StartupRandom startup_random(int frame) {
    StartupRandom random;
    for (std::size_t piece = 0; piece < random.pieces.size(); ++piece)
        random.pieces[piece] = random_samples(frame, static_cast<int>(piece * 3U));
    for (std::size_t index = 0; index < random.garbage.size(); ++index) {
        const int salt = static_cast<int>(index);
        random.garbage[index] = {sample(frame, salt + 20), sample(frame, salt + 120)};
    }
    return random;
}

VersusRandom versus_random(int frame) {
    std::array<RandomSamples, versus_piece_count + 3> samples{};
    for (std::size_t index = 0; index < samples.size(); ++index)
        samples[index] = random_samples(frame, static_cast<int>(index * 3U + 500U));
    VersusRandom random;
    random.pieces = make_versus_piece_sequence(samples);
    for (std::size_t index = 0; index < random.garbage.size(); ++index) {
        const int salt = static_cast<int>(index);
        random.garbage[index] = {sample(frame, salt + 600), sample(frame, salt + 700)};
    }
    return random;
}

std::optional<std::string> json_string(std::string_view text, std::string_view key) {
    const std::string marker = "\"" + std::string(key) + "\"";
    std::size_t position = text.find(marker);
    if (position == std::string_view::npos)
        return std::nullopt;
    position = text.find(':', position + marker.size());
    if (position == std::string_view::npos)
        return std::nullopt;
    position = text.find('"', position + 1);
    if (position == std::string_view::npos)
        return std::nullopt;
    const std::size_t end = text.find('"', position + 1);
    if (end == std::string_view::npos)
        return std::nullopt;
    return std::string(text.substr(position + 1, end - position - 1));
}

std::optional<int> json_integer(std::string_view text, std::string_view key) {
    const std::string marker = "\"" + std::string(key) + "\"";
    std::size_t position = text.find(marker);
    if (position == std::string_view::npos)
        return std::nullopt;
    position = text.find(':', position + marker.size());
    if (position == std::string_view::npos)
        return std::nullopt;
    const std::size_t begin = text.find_first_of("-0123456789", position + 1);
    if (begin == std::string_view::npos)
        return std::nullopt;
    const std::size_t end = text.find_first_not_of("0123456789", begin + 1);
    int value = 0;
    if (!integer(text.substr(begin, end - begin), value))
        return std::nullopt;
    return value;
}

bool load_setup(const std::filesystem::path& path, Setup& setup, std::string& error) {
    std::ifstream input(path);
    if (!input) {
        error = "could not open semantic setup: " + path.string();
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();
    const auto schema = json_string(text, "schema");
    const auto mode = json_string(text, "mode");
    if (!schema || *schema != "native-gb-tetris.setup.v1" || !mode) {
        error = "unsupported semantic setup schema";
        return false;
    }
    setup.apply_step = json_integer(text, "apply_step").value_or(0);
    setup.rules.type = *mode == "type-a" ? GameType::type_a : GameType::type_b;
    if (*mode != "type-a" && *mode != "type-b") {
        error = "semantic setup supports Type A and Type B only";
        return false;
    }
    setup.rules.starting_level = json_integer(text, "level").value_or(0);
    setup.rules.type_b_height = json_integer(text, "height").value_or(0);
    const auto score = json_integer(text, "score");
    if (score)
        setup.score = static_cast<std::uint32_t>(*score);
    const auto phase = json_string(text, "phase");
    if (phase) {
        if (*phase == "game-over")
            setup.state = PlayState::game_over;
        else if (*phase == "complete")
            setup.state = PlayState::complete;
        else {
            error = "semantic setup phase is not supported";
            return false;
        }
    }
    if (setup.apply_step < 0 || setup.rules.starting_level < 0 || setup.rules.starting_level > 20 ||
        setup.rules.type_b_height < 0 || setup.rules.type_b_height > 5 ||
        (setup.score && *setup.score > 999'999U)) {
        error = "semantic setup value is outside its supported range";
        return false;
    }
    return true;
}

void apply_setup(const Setup& setup, int frame, GameState& state) {
    if (frame != setup.apply_step)
        return;
    start_session(state, setup.rules, startup_random(frame));
    if (setup.score)
        state.single_player.debug_set_score(*setup.score);
    if (setup.state)
        state.single_player.debug_set_state(*setup.state);
}

const char* screen_name(Screen screen) {
    switch (screen) {
    case Screen::copyright_fixed: return "CopyrightFixed";
    case Screen::copyright_skippable: return "CopyrightSkippable";
    case Screen::title: return "Title";
    case Screen::game_type: return "GameTypeMenu";
    case Screen::music: return "MusicMenu";
    case Screen::type_a_level: return "TypeALevelMenu";
    case Screen::type_b_level: return "TypeBLevelMenu";
    case Screen::type_b_height: return "TypeBHeightMenu";
    case Screen::versus_height: return "MultiplayerHeightMenu";
    case Screen::gameplay: return "Gameplay";
    case Screen::versus_gameplay: return "MultiplayerGameplay";
    case Screen::versus_round_result: return "MultiplayerRoundResult";
    case Screen::versus_match_result: return "MultiplayerMatchResult";
    case Screen::demo: return "DemoGameplay";
    case Screen::game_over: return "GameOver";
    case Screen::type_b_celebration: return "TypeBCelebration";
    case Screen::dancers: return "TypeBDancers";
    case Screen::buran: return "BuranEnding";
    case Screen::rocket: return "RocketEnding";
    case Screen::scoreboard: return "Scoreboard";
    case Screen::name_entry: return "NameEntry";
    }
    return "Unknown";
}

const char* phase_name(PlayState state) {
    switch (state) {
    case PlayState::falling: return "Falling";
    case PlayState::locked: return "Locked";
    case PlayState::resolving: return "Resolving";
    case PlayState::clearing: return "Clearing";
    case PlayState::collapse_pending: return "CollapsePending";
    case PlayState::wiping: return "Wiping";
    case PlayState::game_over: return "GameOver";
    case PlayState::complete: return "Complete";
    }
    return "Unknown";
}

const char* ending_name(EndingStage stage) {
    switch (stage) {
    case EndingStage::none: return "None";
    case EndingStage::game_over_curtain: return "Game-over curtain";
    case EndingStage::game_over_wait: return "Game-over wait";
    case EndingStage::rocket_bonus_delay: return "Rocket bonus delay";
    case EndingStage::rocket_initialize: return "Rocket graphics initialization";
    case EndingStage::rocket_prelaunch: return "Rocket prelaunch";
    case EndingStage::rocket_ignition: return "Rocket ignition";
    case EndingStage::rocket_liftoff_delay: return "Rocket liftoff delay";
    case EndingStage::rocket_liftoff: return "Rocket liftoff";
    case EndingStage::rocket_rising: return "Rocket rising";
    case EndingStage::rocket_return: return "Rocket ending teardown";
    case EndingStage::type_b_victory_delay: return "Type-B victory delay";
    case EndingStage::dancers: return "Type-B dancers";
    case EndingStage::buran_initialize: return "Buran graphics initialization";
    case EndingStage::buran_prelaunch: return "Buran prelaunch";
    case EndingStage::buran_ignition: return "Buran ignition";
    case EndingStage::buran_final_ignition: return "Buran final ignition";
    case EndingStage::buran_liftoff: return "Buran liftoff";
    case EndingStage::buran_rising: return "Buran rising";
    case EndingStage::congratulations: return "Congratulations text";
    case EndingStage::congratulations_wait: return "Congratulations wait";
    case EndingStage::buran_teardown: return "Buran ending teardown";
    case EndingStage::scoreboard_delay: return "Scoreboard delay";
    case EndingStage::scoreboard_tally: return "Scoreboard tally";
    case EndingStage::scoreboard_wait: return "Scoreboard wait";
    }
    return "Unknown";
}

unsigned int piece_code(PieceSpec piece) {
    return static_cast<unsigned int>(piece.kind) * 4U + static_cast<unsigned int>(piece.rotation);
}

unsigned int piece_code(const FallingPiece& piece) {
    return piece_code(PieceSpec{piece.kind, piece.rotation});
}

std::string board_hex(const Board& board) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(board.cells.size() * 2);
    for (const Block block : board.cells) {
        const auto value = static_cast<unsigned int>(block);
        result.push_back(digits[(value >> 4U) & 0xFU]);
        result.push_back(digits[value & 0xFU]);
    }
    return result;
}

unsigned int effect_id(const audio::Director& audio, content::SoundChannel channel) {
    const audio::SoundChannelState& state = audio.sounds.channel(channel);
    return state.active() ? state.effect->original_id : 0U;
}

void write_snapshot(std::ofstream& output, int frame, std::uint8_t keys, const GameState& state,
                    const audio::Director& audio) {
    const SinglePlayer& game = state.single_player;
    const auto channels = audio.music.channels;
    output << "{\"kind\":\"frame\",\"frame\":" << frame
           << ",\"input_mask\":" << static_cast<unsigned int>(keys) << ",\"screen\":\""
           << screen_name(state.screen) << "\"" << ",\"ending_stage\":\""
           << ending_name(state.ending_stage) << "\""
           << ",\"ending_elapsed\":" << state.ending_elapsed
           << ",\"launch_y\":" << (state.launch_y & 0xff) << ",\"exhaust_x\":" << exhaust_x(state)
           << ",\"exhaust_y\":" << (state.exhaust_y & 0xff)
           << ",\"exhaust_frame\":" << exhaust_animation_frame(state)
           << ",\"congratulations_characters\":" << state.congratulations_characters
           << ",\"timer\":" << state.timer
           << ",\"selected_mode\":" << static_cast<int>(state.selected_type)
           << ",\"selected_music\":" << static_cast<int>(state.selected_music)
           << ",\"selected_level\":" << selected_level(state)
           << ",\"selected_height\":" << state.type_b_height
           << ",\"heart_mode\":" << (state.heart_mode ? "true" : "false")
           << ",\"demo_type_b\":" << (state.demo_type_b ? "true" : "false") << ",\"phase\":\""
           << phase_name(game.state) << "\"" << ",\"level\":" << game.level
           << ",\"game_height\":" << game.rules.type_b_height << ",\"lines\":" << game.lines
           << ",\"score\":" << game.score
           << ",\"scoreboard_category\":" << state.scoreboard.category
           << ",\"scoreboard_score\":" << state.scoreboard.score << ",\"active\":["
           << piece_code(game.piece) << ',' << game.piece.origin.x << ',' << game.piece.origin.y
           << ']' << ",\"preview\":" << piece_code(game.preview)
           << ",\"next_preview\":" << piece_code(game.hidden)
           << ",\"preview_hidden\":" << (!game.preview_visible ? "true" : "false")
           << ",\"drop_timer\":" << game.fall_timer
           << ",\"fixed_pieces_consumed\":" << game.next_fixed_piece
           << ",\"key_repeat_timer\":" << game.horizontal_repeat_timer
           << ",\"audio_song\":" << audio.active_song
           << ",\"audio_song_playing\":" << (audio.music.playing ? "true" : "false")
           << ",\"audio_stereo\":" << static_cast<unsigned int>(audio.stereo_mask)
           << ",\"audio_paused\":" << (audio.paused ? "true" : "false")
           << ",\"square_sfx\":" << effect_id(audio, content::SoundChannel::pulse)
           << ",\"wave_sfx\":" << effect_id(audio, content::SoundChannel::wave)
           << ",\"noise_sfx\":" << effect_id(audio, content::SoundChannel::noise)
           << ",\"music_periods\":[" << channels[0].output_period << ','
           << channels[1].output_period << ',' << channels[2].output_period << ']'
           << ",\"music_base_periods\":[" << channels[0].period << ',' << channels[1].period << ','
           << channels[2].period << ']' << ",\"music_timers\":["
           << static_cast<unsigned int>(channels[0].timer) << ','
           << static_cast<unsigned int>(channels[1].timer) << ','
           << static_cast<unsigned int>(channels[2].timer) << ']' << ",\"board\":\""
           << board_hex(game.board) << "\"}\n";
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        std::fprintf(stderr,
                     "Usage: %s --rom file --input events.csv --output trace.jsonl "
                     "--frames N [--setup setup.json]\n",
                     argv[0]);
        return 2;
    }

    content::Rom rom;
    content::Catalog content;
    std::vector<InputEvent> events;
    std::optional<Setup> setup;
    std::string error;
    if (!content::load_rom(options.rom, rom, error) ||
        !content::extract_catalog(rom, content, error) ||
        !load_input(options.input, events, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    if (!options.setup.empty()) {
        Setup value;
        if (!load_setup(options.setup, value, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        setup = value;
    }
    std::ofstream output(options.output);
    if (!output) {
        std::fprintf(stderr, "could not open semantic trace: %s\n", options.output.c_str());
        return 1;
    }

    GameState state;
    start_game(state, {content.gameplay.data, content.type_a_demo.runs, content.type_b_demo.runs,
                       content.demo_pieces, content.type_b_demo_garbage.bytes});
    audio::Director audio;
    audio.attach(content.audio);
    output << "{\"kind\":\"metadata\",\"schema\":\"native-gb-tetris.trace.v1\","
           << "\"rom_sha1\":\"" << content.source_sha1 << "\",\"frame_limit\":" << options.frames
           << "}\n";

    std::size_t next_event = 0;
    std::uint8_t held_keys = 0;
    for (int frame = 0; frame < options.frames; ++frame) {
        while (next_event < events.size() && events[next_event].frame <= frame) {
            held_keys = events[next_event].keys;
            ++next_event;
        }
        if (setup)
            apply_setup(*setup, frame, state);
        step_game(state, {.player_one = buttons(held_keys),
                          .player_two = {},
                          .random = random_samples(frame, 300),
                          .startup = startup_random(frame),
                          .versus = versus_random(frame)});
        audio.step(state, static_cast<std::uint8_t>(state.single_player.step_count));
        write_snapshot(output, frame, held_keys, state, audio);
    }
    return output ? 0 : 1;
}
