#include "content/catalog.hpp"
#include "content/rom.hpp"
#include "content/sha1.hpp"

#include <array>
#include <cstdio>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (condition)
        return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

void test_sha1() {
    constexpr std::array<std::uint8_t, 3> abc = {'a', 'b', 'c'};
    expect(tetris::content::sha1(abc) == "a9993e364706816aba3e25717850c26c9cd0d89d",
           "SHA-1 matches its standard test vector");
}

#ifdef TETRIS_TEST_ROM
void test_supported_catalog() {
    using namespace tetris::content;
    Rom rom;
    std::string error;
    expect(load_rom(TETRIS_TEST_ROM, rom, error), "supported test ROM loads");
    if (rom.bytes.empty())
        return;
    expect(is_supported(rom), "supported test ROM matches the exact profile");
    expect(rom.header_checksum_valid && rom.global_checksum_valid,
           "supported test ROM passes both cartridge checksums");

    Catalog catalog;
    expect(extract_catalog(rom, catalog, error), "supported ROM extracts into a semantic catalog");
    if (!error.empty() || catalog.source_sha1.empty())
        return;
    expect(catalog.tile_count() == 562, "all 562 graphics tiles are represented");
    expect(catalog.type_a_demo.runs.size() == 128, "Type A demo has 128 input runs");
    expect(catalog.type_b_demo.runs.size() == 80, "Type B demo has 80 input runs");
    expect(catalog.demo_pieces.size() == 48, "demo piece sequence has 48 pieces");
    expect(catalog.tilemaps.size() == 21, "all 21 screen and ending maps are represented");
    expect(catalog.presentation.size() == 29, "all 29 placement and text tables are represented");
    expect(catalog.sprites.sprites.size() == 94, "all 94 original metasprites are represented");
    expect(catalog.audio.songs.size() == 17, "all 17 songs are represented");
    expect(catalog.audio.effects.size() == 14, "all 14 sound effects are represented");
    expect(!catalog.audio.sequences.empty() && !catalog.audio.sections.empty(),
           "music pointer graphs resolve into typed sequences and sections");
    expect(catalog.audio.songs[4].durations[3] == 24,
           "Korobeiniki retains its decoded A3 duration");
    const Tilemap* title = catalog.find_tilemap("title");
    expect(title != nullptr && title->width == 20 && title->height == 18,
           "title map keeps its original dimensions");
    const ByteTable* cursor = catalog.find_presentation("music-cursor-coordinates");
    expect(cursor != nullptr && cursor->bytes ==
               std::vector<std::uint8_t>{0x70, 0x37, 0x70, 0x77, 0x80, 0x37, 0x80, 0x77},
           "music menu coordinates round-trip through the catalog");
}
#endif

} // namespace

int main() {
    test_sha1();
#ifdef TETRIS_TEST_ROM
    test_supported_catalog();
#endif
    if (failures != 0)
        return 1;
    std::puts("modern Native Tetris content tests passed");
    return 0;
}
