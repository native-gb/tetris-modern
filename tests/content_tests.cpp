#include "content/catalog.hpp"
#include "content/rom.hpp"
#include "content/sha1.hpp"

#include <array>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (condition)
        return;
    std::fprintf(stderr, "FAIL: %s\n", message.c_str());
    ++failures;
}

void expect_provenance(const tetris::content::Provenance& source,
                       const tetris::content::Rom& rom) {
    expect(!source.id.empty(), "provenance has a semantic id");
    expect(!source.decoder.empty(), source.id + " names its decoder");
    expect(!source.spans.empty(), source.id + " has at least one exact ROM span");
    expect(source.validated && source.expected_count == source.decoded_count,
           source.id + " has a validated decoded count");
    for (const tetris::content::RomSpan span : source.spans) {
        expect(span.begin < span.end && span.end <= rom.bytes.size(),
               source.id + " stays inside the validated ROM");
    }
}

std::vector<std::uint8_t> encode_tiles(const tetris::content::TileSet& tiles) {
    const bool one_bit = tiles.source.decoder == "gameboy-1bpp";
    std::vector<std::uint8_t> bytes;
    bytes.reserve(tiles.tiles.size() * (one_bit ? 8U : 16U));
    for (const tetris::content::Tile& tile : tiles.tiles) {
        for (std::size_t y = 0; y < 8; ++y) {
            std::uint8_t low = 0;
            std::uint8_t high = 0;
            for (std::size_t x = 0; x < 8; ++x) {
                const auto shift = static_cast<unsigned int>(7 - x);
                const std::uint8_t pixel = tile.pixels[y * 8 + x];
                low = static_cast<std::uint8_t>(low | ((pixel & 1U) << shift));
                high = static_cast<std::uint8_t>(high | (((pixel >> 1U) & 1U) << shift));
            }
            bytes.push_back(low);
            if (!one_bit)
                bytes.push_back(high);
        }
    }
    return bytes;
}

std::uint8_t encode_buttons(const tetris::Buttons& buttons) {
    return static_cast<std::uint8_t>(
        (buttons.left ? 0x20U : 0U) | (buttons.right ? 0x10U : 0U) |
        (buttons.up ? 0x40U : 0U) | (buttons.down ? 0x80U : 0U) |
        (buttons.rotate_left ? 0x02U : 0U) | (buttons.rotate_right ? 0x01U : 0U) |
        (buttons.start ? 0x08U : 0U) | (buttons.select ? 0x04U : 0U));
}

void expect_tile_round_trip(const tetris::content::TileSet& tiles,
                            const tetris::content::Rom& rom) {
    expect_provenance(tiles.source, rom);
    const auto encoded = encode_tiles(tiles);
    const auto span = tiles.source.spans.front();
    expect(std::equal(encoded.begin(), encoded.end(), rom.bytes.begin() +
                          static_cast<std::ptrdiff_t>(span.begin)),
           tiles.source.id + " round-trips to the source bytes");
}

void expect_demo_round_trip(const tetris::content::Demo& demo,
                            const tetris::content::Rom& rom) {
    expect_provenance(demo.source, rom);
    std::vector<std::uint8_t> encoded;
    for (const tetris::DemoRun& run : demo.runs) {
        encoded.push_back(encode_buttons(run.held));
        encoded.push_back(static_cast<std::uint8_t>(run.frames));
    }
    const auto span = demo.source.spans.front();
    expect(std::equal(encoded.begin(), encoded.end(), rom.bytes.begin() +
                          static_cast<std::ptrdiff_t>(span.begin)),
           demo.source.id + " round-trips to the source bytes");
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

    expect_tile_round_trip(catalog.gameplay_tiles, rom);
    expect_tile_round_trip(catalog.font_tiles, rom);
    expect_tile_round_trip(catalog.title_tiles, rom);
    expect_tile_round_trip(catalog.multiplayer_tiles, rom);
    expect_demo_round_trip(catalog.type_a_demo, rom);
    expect_demo_round_trip(catalog.type_b_demo, rom);
    expect_provenance(catalog.demo_piece_source, rom);
    expect_provenance(catalog.type_b_demo_garbage.source, rom);
    expect_provenance(catalog.sprites.source, rom);
    expect_provenance(catalog.gameplay.gravity_source, rom);
    expect(catalog.gameplay.gravity_frames[0] == 52 &&
               catalog.gameplay.gravity_frames[20] == 2,
           "the complete gravity table is extracted from the ROM");
    for (const TetrominoDefinition& piece : catalog.gameplay.tetrominoes) {
        expect_provenance(piece.source, rom);
        expect(piece.cells == tetris::piece_cells(piece.piece),
               "every semantic tetromino orientation matches its ROM matrix");
    }
    expect(catalog.gameplay.tetrominoes[3].cells[0] == tetris::Cell{0, 1},
           "the left-facing L retains its ROM-authored vertical placement");

    for (const Tilemap& map : catalog.tilemaps) {
        expect_provenance(map.source, rom);
        const RomSpan span = map.source.spans.front();
        expect(std::equal(map.tiles.begin(), map.tiles.end(), rom.bytes.begin() +
                              static_cast<std::ptrdiff_t>(span.begin)),
               map.source.id + " tilemap cells round-trip");
    }
    for (const ByteTable& table : catalog.presentation) {
        expect_provenance(table.source, rom);
        const RomSpan span = table.source.spans.front();
        expect(std::equal(table.bytes.begin(), table.bytes.end(), rom.bytes.begin() +
                              static_cast<std::ptrdiff_t>(span.begin)),
               table.source.id + " bytes round-trip");
    }
    expect_provenance(catalog.audio.source, rom);
    expect(std::equal(catalog.audio.source_bytes.begin(), catalog.audio.source_bytes.end(),
                      rom.bytes.begin() + static_cast<std::ptrdiff_t>(
                                              catalog.audio.source.spans.front().begin)),
           "the complete original audio range round-trips");
    for (const Song& song : catalog.audio.songs)
        expect_provenance(song.source, rom);
    for (const MusicSequence& sequence : catalog.audio.sequences)
        expect_provenance(sequence.source, rom);
    for (const MusicSection& section : catalog.audio.sections)
        expect_provenance(section.source, rom);
    for (const SoundEffect& effect : catalog.audio.effects)
        expect_provenance(effect.source, rom);

    std::vector<RomSpan> content_ranges;
    const auto add = [&content_ranges](const Provenance& source) {
        content_ranges.insert(content_ranges.end(), source.spans.begin(), source.spans.end());
    };
    add(catalog.gameplay_tiles.source);
    add(catalog.font_tiles.source);
    add(catalog.title_tiles.source);
    add(catalog.multiplayer_tiles.source);
    add(catalog.type_a_demo.source);
    add(catalog.type_b_demo.source);
    add(catalog.demo_piece_source);
    add(catalog.type_b_demo_garbage.source);
    add(catalog.gameplay.gravity_source);
    add(catalog.sprites.source);
    add(catalog.audio.source);
    for (const Tilemap& map : catalog.tilemaps) add(map.source);
    for (const ByteTable& table : catalog.presentation) add(table.source);
    std::ranges::sort(content_ranges, {}, &RomSpan::begin);
    for (std::size_t index = 1; index < content_ranges.size(); ++index) {
        expect(content_ranges[index - 1].end <= content_ranges[index].begin,
               "top-level extracted content ranges do not overlap");
    }
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
