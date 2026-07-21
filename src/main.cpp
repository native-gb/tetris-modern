#include "application.hpp"
#include "content/rom.hpp"
#include "frame.hpp"

#include <SDL3/SDL.h>

#include <cstdio>
#include <filesystem>
#include <string>

namespace {

struct CommandLine {
    std::filesystem::path rom =
        std::filesystem::path(TETRIS_SOURCE_DIR) / "roms" / "Tetris (JUE) (V1.1) [!].gb";
    bool smoke{};
    bool render_smoke{};
    bool help{};
};

bool parse(int argc, char** argv, CommandLine& command) {
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index] == nullptr ? "" : argv[index];
        if (argument == "--rom" && index + 1 < argc) {
            command.rom = argv[++index];
        } else if (argument == "--smoke") {
            command.smoke = true;
        } else if (argument == "--render-smoke") {
            command.render_smoke = true;
        } else if (argument == "--help" || argument == "-h") {
            command.help = true;
        } else {
            std::fprintf(stderr, "unknown argument: %s\n", argument.c_str());
            return false;
        }
    }
    return true;
}

void usage(const char* executable) {
    std::printf("Usage: %s [--rom path] [--smoke] [--render-smoke]\n", executable);
}

} // namespace

int main(int argc, char** argv) {
    // Parse desktop-only command-line options.
    CommandLine command;
    if (!parse(argc, argv, command)) {
        usage(argv[0]);
        return 2;
    }
    if (command.help) {
        usage(argv[0]);
        return 0;
    }

    // Load the user-supplied cartridge through the shared byte decoder.
    tetris::content::Rom rom;
    std::string error;
    if (!tetris::content::load_rom(command.rom, rom, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    std::printf("Loaded %s revision %u, %zu bytes, SHA-1 %s\n", rom.title.c_str(),
                static_cast<unsigned int>(rom.revision), rom.bytes.size(), rom.digest.c_str());
    if (command.smoke)
        return rom.header_checksum_valid && rom.global_checksum_valid ? 0 : 1;

    // The desktop host supplies storage and timing; the application owns every
    // game domain and is shared unchanged with the browser entry point.
    tetris::Application app;
    const tetris::ApplicationConfig config{
        .data_root = std::filesystem::path(TETRIS_SOURCE_DIR) / "data" / "runtime",
        .window = {},
        .host = {},
        .render_limit = command.render_smoke ? 3 : 0,
        .open_tools = command.render_smoke,
    };
    if (!tetris::initialize_application(app, std::move(rom), config, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }

    while (true) {
        tetris::update_clock(app.clock);
        if (!tetris::step_application(app, app.clock.elapsed))
            break;
        tetris::pace(app.clock);
    }
    tetris::shutdown_application(app);
    return 0;
}
