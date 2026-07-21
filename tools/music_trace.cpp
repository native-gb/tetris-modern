#include "audio/music.hpp"
#include "content/catalog.hpp"
#include "content/rom.hpp"

#include <charconv>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

struct Options {
    std::filesystem::path rom;
    std::filesystem::path output;
    int song{-1};
    int frames{};
};

bool integer(std::string_view text, int& result) {
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto parsed = std::from_chars(begin, end, result);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool options(int argc, char** argv, Options& result) {
    for (int index = 1; index < argc; ++index) {
        if (index + 1 >= argc)
            return false;
        const std::string_view argument = argv[index] != nullptr ? argv[index] : "";
        const char* value = argv[++index] != nullptr ? argv[index] : "";
        if (argument == "--rom")
            result.rom = value;
        else if (argument == "--output")
            result.output = value;
        else if (argument == "--song") {
            if (!integer(value, result.song))
                return false;
        } else if (argument == "--frames") {
            if (!integer(value, result.frames))
                return false;
        } else {
            return false;
        }
    }
    return !result.rom.empty() && !result.output.empty() &&
           result.song >= 0 && result.song < 17 && result.frames > 0;
}

void write_channel(std::ofstream& output, const tetris::audio::MusicChannel& channel) {
    output << "{\"active\":" << (channel.active ? "true" : "false")
           << ",\"resting\":" << (channel.resting ? "true" : "false")
           << ",\"timer\":" << static_cast<unsigned int>(channel.timer)
           << ",\"duration\":" << static_cast<unsigned int>(channel.duration)
           << ",\"period\":" << channel.period
           << ",\"output_period\":" << channel.output_period
           << ",\"vibrato_counter\":" << static_cast<unsigned int>(channel.vibrato_step)
           << ",\"instrument\":[" << static_cast<unsigned int>(channel.instrument[0])
           << ',' << static_cast<unsigned int>(channel.instrument[1]) << ','
           << static_cast<unsigned int>(channel.instrument[2]) << ']'
           << ",\"registers\":[";
    for (std::size_t index = 0; index < channel.registers.size(); ++index) {
        if (index != 0)
            output << ',';
        output << static_cast<unsigned int>(channel.registers[index]);
    }
    output << "]}";
}

} // namespace

int main(int argc, char** argv) {
    Options selected;
    if (!options(argc, argv, selected)) {
        std::fprintf(stderr,
                     "Usage: %s --rom file --output trace.jsonl --song 0..16 --frames N\n",
                     argv[0]);
        return 2;
    }

    tetris::content::Rom rom;
    tetris::content::Catalog content;
    std::string error;
    if (!tetris::content::load_rom(selected.rom, rom, error) ||
        !tetris::content::extract_catalog(rom, content, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    tetris::audio::MusicPlayer player;
    if (!player.play(content.audio, static_cast<std::size_t>(selected.song))) {
        std::fprintf(stderr, "could not start song %d\n", selected.song);
        return 1;
    }
    std::ofstream output(selected.output);
    if (!output) {
        std::fprintf(stderr, "could not open trace output\n");
        return 1;
    }
    output << "{\"kind\":\"metadata\",\"schema\":\"native-gb-tetris.music.v1\","
           << "\"song\":" << selected.song << "}\n";
    for (int frame = 0; frame < selected.frames; ++frame) {
        player.step();
        output << "{\"kind\":\"frame\",\"frame\":" << frame
               << ",\"playing\":" << (player.playing ? "true" : "false")
               << ",\"channels\":[";
        const auto channels = player.channels;
        for (std::size_t channel = 0; channel < channels.size(); ++channel) {
            if (channel != 0)
                output << ',';
            write_channel(output, channels[channel]);
        }
        output << "]}\n";
    }
    return output ? 0 : 1;
}
