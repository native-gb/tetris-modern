#include "app/app.hpp"
#include "content/catalog.hpp"
#include "content/rom.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

namespace {

struct CommandLine {
    std::filesystem::path rom = std::filesystem::path(TETRIS_SOURCE_DIR) / "roms" /
                                "Tetris (JUE) (V1.1) [!].gb";
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
    CommandLine command;
    if (!parse(argc, argv, command)) {
        usage(argv[0]);
        return 2;
    }
    if (command.help) {
        usage(argv[0]);
        return 0;
    }

    tetris::content::Rom rom;
    std::string error;
    if (!tetris::content::load_rom(command.rom, rom, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    tetris::content::Catalog content;
    if (!tetris::content::extract_catalog(rom, content, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    std::printf("Loaded %s revision %u, %zu bytes, SHA-1 %s\n",
                rom.title.c_str(), static_cast<unsigned int>(rom.revision),
                rom.bytes.size(), rom.digest.c_str());
    if (command.smoke)
        return rom.header_checksum_valid && rom.global_checksum_valid ? 0 : 1;
    return tetris::app::run(rom, content, command.render_smoke ? 3 : 0);
}
