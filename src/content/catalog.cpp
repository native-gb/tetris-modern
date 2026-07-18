#include "content/catalog.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <span>

namespace tetris::content {
namespace {

struct Range {
    std::string_view id;
    std::size_t begin;
    std::size_t end;
    std::string_view format;
};

struct MapRange {
    Range range;
    int width;
    int height;
    bool terminator;
};

Provenance source_for(const Range& range, std::size_t expected_count,
                      std::size_t decoded_count) {
    return provenance(range.id, {range.begin, range.end}, range.format,
                      expected_count, decoded_count);
}

constexpr std::array tile_ranges = {
    Range{"gameplay", 0x323F, 0x3E8F, "gameboy-2bpp"},
    Range{"font", 0x415F, 0x4297, "gameboy-1bpp"},
    Range{"copyright-and-title", 0x4297, 0x4A07, "gameboy-2bpp"},
    Range{"multiplayer-and-buran", 0x55AC, 0x629C, "gameboy-2bpp"},
};

constexpr std::array tilemap_ranges = {
    MapRange{{"left-tower-left", 0x141B, 0x1422, "tilemap"}, 1, 7, false},
    MapRange{{"left-tower-right", 0x1422, 0x1429, "tilemap"}, 1, 7, false},
    MapRange{{"right-tower-left", 0x1429, 0x1430, "tilemap"}, 1, 7, false},
    MapRange{{"right-tower-right", 0x1430, 0x1437, "tilemap"}, 1, 7, false},
    MapRange{{"pause-message", 0x2839, 0x2889, "tilemap"}, 8, 10, false},
    MapRange{{"type-b-scoreboard", 0x2889, 0x293E, "tilemap-ff"}, 10, 18, true},
    MapRange{{"game-over-panel", 0x293E, 0x2976, "tilemap"}, 8, 7, false},
    MapRange{{"try-again-panel", 0x2976, 0x29A6, "tilemap"}, 8, 6, false},
    MapRange{{"type-a-gameplay", 0x3E8F, 0x3FF7, "tilemap"}, 20, 18, false},
    MapRange{{"type-b-gameplay", 0x3FF7, 0x415F, "tilemap"}, 20, 18, false},
    MapRange{{"copyright", 0x4A07, 0x4B6F, "tilemap"}, 20, 18, false},
    MapRange{{"title", 0x4B6F, 0x4CD7, "tilemap"}, 20, 18, false},
    MapRange{{"configuration", 0x4CD7, 0x4E3F, "tilemap"}, 20, 18, false},
    MapRange{{"type-a-difficulty", 0x4E3F, 0x4FA7, "tilemap"}, 20, 18, false},
    MapRange{{"type-b-difficulty", 0x4FA7, 0x510F, "tilemap"}, 20, 18, false},
    MapRange{{"dancers", 0x510F, 0x51C4, "tilemap-ff"}, 10, 18, true},
    MapRange{{"buran-backdrop", 0x51C4, 0x5214, "tilemap"}, 20, 4, false},
    MapRange{{"multiplayer-difficulty", 0x5214, 0x537C, "tilemap"}, 20, 18, false},
    MapRange{{"multiplayer-gameplay", 0x537C, 0x54E4, "tilemap"}, 20, 18, false},
    MapRange{{"multiplayer-victory-top", 0x54E4, 0x5534, "tilemap"}, 20, 4, false},
    MapRange{{"multiplayer-victory-bottom", 0x5534, 0x55AC, "tilemap"}, 20, 6, false},
};

constexpr std::array presentation_ranges = {
    Range{"height-menu-faces", 0x0705, 0x0725, "oam-objects"},
    Range{"player-one-height-coordinates", 0x07F6, 0x0802, "yx-pairs"},
    Range{"player-two-height-coordinates", 0x0802, 0x080E, "yx-pairs"},
    Range{"luigi-gameplay-face", 0x08C4, 0x08D4, "oam-objects"},
    Range{"mario-gameplay-face", 0x08D4, 0x08E4, "oam-objects"},
    Range{"push-start-objects", 0x0F3C, 0x0F60, "oam-objects"},
    Range{"deuce-text", 0x10ED, 0x10F3, "tile-text"},
    Range{"mario-wins-text", 0x10F3, 0x10FE, "tile-text"},
    Range{"luigi-wins-text", 0x10FE, 0x1109, "tile-text"},
    Range{"advantage-text", 0x1109, 0x1112, "tile-text"},
    Range{"congratulations-text", 0x12F5, 0x1305, "tile-text"},
    Range{"music-cursor-coordinates", 0x14A8, 0x14B0, "yx-pairs"},
    Range{"type-a-level-coordinates", 0x1615, 0x1629, "yx-pairs"},
    Range{"type-b-level-coordinates", 0x16D2, 0x16E6, "yx-pairs"},
    Range{"type-b-height-coordinates", 0x1741, 0x174E, "yx-pairs"},
    Range{"multiplayer-pause-text", 0x1CDD, 0x1CE2, "tile-text"},
    Range{"active-piece-placement", 0x26BF, 0x26C7, "sprite-state"},
    Range{"preview-piece-placement", 0x26C7, 0x26CF, "sprite-state"},
    Range{"configuration-sprites", 0x26CF, 0x26DB, "sprite-placements"},
    Range{"type-a-level-sprite", 0x26DB, 0x26E1, "sprite-placements"},
    Range{"type-b-selection-sprites", 0x26E1, 0x26ED, "sprite-placements"},
    Range{"multiplayer-height-sprites", 0x26ED, 0x26F9, "sprite-placements"},
    Range{"mario-victory-sprites", 0x26F9, 0x270B, "sprite-placements"},
    Range{"luigi-victory-sprites", 0x270B, 0x271D, "sprite-placements"},
    Range{"mario-defeat-sprites", 0x271D, 0x2729, "sprite-placements"},
    Range{"luigi-defeat-sprites", 0x2729, 0x2735, "sprite-placements"},
    Range{"dancer-sprites", 0x2735, 0x2771, "sprite-placements"},
    Range{"buran-launch-sprites", 0x2771, 0x2783, "sprite-placements"},
    Range{"rocket-launch-sprites", 0x2783, 0x2795, "sprite-placements"},
};

Buttons buttons_from_byte(std::uint8_t value) {
    return {
        .left = (value & 0x20U) != 0,
        .right = (value & 0x10U) != 0,
        .up = (value & 0x40U) != 0,
        .down = (value & 0x80U) != 0,
        .rotate_left = (value & 0x02U) != 0,
        .rotate_right = (value & 0x01U) != 0,
        .start = (value & 0x08U) != 0,
        .select = (value & 0x04U) != 0,
    };
}

Tile decode_tile(std::span<const std::uint8_t> bytes, bool one_bit) {
    Tile tile;
    for (std::size_t y = 0; y < 8; ++y) {
        const std::uint8_t low = bytes[y * (one_bit ? 1U : 2U)];
        const std::uint8_t high = one_bit ? 0 : bytes[y * 2 + 1];
        for (std::size_t x = 0; x < 8; ++x) {
            const auto shift = static_cast<unsigned int>(7 - x);
            tile.pixels[y * 8 + x] = static_cast<std::uint8_t>(
                ((low >> shift) & 1U) | (((high >> shift) & 1U) << 1U));
        }
    }
    return tile;
}

bool extract_tiles(const Rom& rom, const Range& range, TileSet& result, std::string& error) {
    const bool one_bit = range.format == "gameboy-1bpp";
    const std::size_t stride = one_bit ? 8U : 16U;
    const auto bytes = rom.range(range.begin, range.end);
    if (bytes.empty() || bytes.size() % stride != 0) {
        error = "invalid tile range: " + std::string(range.id);
        return false;
    }
    TileSet tiles;
    tiles.tiles.reserve(bytes.size() / stride);
    for (std::size_t offset = 0; offset < bytes.size(); offset += stride)
        tiles.tiles.push_back(decode_tile(bytes.subspan(offset, stride), one_bit));
    tiles.source = source_for(range, bytes.size() / stride, tiles.tiles.size());
    result = std::move(tiles);
    return true;
}

bool extract_demo(const Rom& rom, const Range& range, Demo& result, std::string& error) {
    const auto bytes = rom.range(range.begin, range.end);
    if (bytes.empty() || bytes.size() % 2 != 0) {
        error = "invalid demo range: " + std::string(range.id);
        return false;
    }
    Demo demo;
    for (std::size_t index = 0; index < bytes.size(); index += 2)
        demo.runs.push_back({buttons_from_byte(bytes[index]), bytes[index + 1]});
    demo.source = source_for(range, bytes.size() / 2, demo.runs.size());
    result = std::move(demo);
    return true;
}

int signed_byte(std::uint8_t value) {
    return value <= 127U ? static_cast<int>(value) : static_cast<int>(value) - 256;
}

std::uint16_t word(const Rom& rom, std::size_t address) {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(rom.bytes[address]) |
                                     (static_cast<std::uint16_t>(rom.bytes[address + 1]) << 8U));
}

bool extract_sprites(const Rom& rom, SpriteCatalog& result, std::string& error) {
    struct Matrix { std::size_t begin; std::size_t count; };
    constexpr std::array matrices = {
        Matrix{0x31A9, 16}, Matrix{0x31C9, 8}, Matrix{0x31D9, 14},
        Matrix{0x31F5, 28}, Matrix{0x322D, 9},
    };
    SpriteCatalog catalog;
    catalog.source = provenance("metasprite-catalog", {0x2B64, 0x323F},
                                "pointer-metasprites", 94, 94);
    for (std::size_t id = 0; id < 94; ++id) {
        const std::size_t descriptor = word(rom, 0x2B64 + id * 2);
        const std::size_t tile_list = word(rom, descriptor);
        const std::size_t matrix_address = word(rom, tile_list);
        const auto matrix = std::ranges::find_if(matrices, [matrix_address](Matrix entry) {
            return entry.begin == matrix_address;
        });
        if (descriptor + 4 > rom.bytes.size() || tile_list + 3 > rom.bytes.size() ||
            matrix == matrices.end()) {
            error = "invalid metasprite pointer for entry " + std::to_string(id);
            return false;
        }

        Sprite sprite;
        sprite.id = static_cast<std::uint8_t>(id);
        sprite.origin_x = signed_byte(rom.bytes[descriptor + 3]);
        sprite.origin_y = signed_byte(rom.bytes[descriptor + 2]);
        std::size_t cursor = tile_list + 2;
        std::size_t position = 0;
        while (cursor < 0x31A9) {
            std::uint8_t token = rom.bytes[cursor++];
            if (token == 0xFF)
                break;
            bool flip = false;
            if (token == 0xFD) {
                flip = true;
                token = rom.bytes[cursor++];
            }
            if (position >= matrix->count) {
                error = "metasprite exceeds coordinate matrix";
                return false;
            }
            const std::size_t coordinate = matrix->begin + position * 2;
            ++position;
            if (token != 0xFE) {
                sprite.objects.push_back({
                    .x = signed_byte(rom.bytes[coordinate + 1]),
                    .y = signed_byte(rom.bytes[coordinate]),
                    .tile = token,
                    .flip_x = flip,
                });
            }
        }
        sprite.source = provenance(
            "metasprite-" + std::to_string(id),
            {{0x2B64 + id * 2, 0x2B66 + id * 2},
             {descriptor, descriptor + 4},
             {tile_list, cursor},
             {matrix->begin, matrix->begin + position * 2}},
            "pointer-descriptor-tile-list-matrix", sprite.objects.size(),
            sprite.objects.size());
        catalog.sprites.push_back(std::move(sprite));
    }
    result = std::move(catalog);
    return true;
}

bool extract_gameplay(const Rom& rom, const SpriteCatalog& sprites,
                      GameplayCatalog& result, std::string& error) {
    GameplayCatalog gameplay;
    constexpr RomSpan gravity_span{0x1B06, 0x1B1B};
    const auto gravity = rom.range(gravity_span.begin, gravity_span.end);
    if (gravity.size() != gameplay.data.gravity_frames.size()) {
        error = "invalid gravity table size";
        return false;
    }
    for (std::size_t index = 0; index < gravity.size(); ++index)
        gameplay.data.gravity_frames[index] = gravity[index];
    gameplay.gravity_source = provenance("gravity-frames", gravity_span,
                                         "byte-per-level", 21, gravity.size());

    if (sprites.sprites.size() < gameplay.tetrominoes.size()) {
        error = "metasprite catalog does not contain all tetromino orientations";
        return false;
    }
    for (std::size_t index = 0; index < gameplay.tetrominoes.size(); ++index) {
        const Sprite& sprite = sprites.sprites[index];
        if (sprite.objects.size() != 4) {
            error = "tetromino metasprite does not contain four blocks";
            return false;
        }
        TetrominoDefinition definition;
        definition.source = sprite.source;
        definition.source.id = "tetromino-" + std::to_string(index);
        definition.piece = {
            .kind = static_cast<PieceKind>(index / 4),
            .rotation = static_cast<Rotation>(index % 4),
        };
        for (std::size_t block = 0; block < sprite.objects.size(); ++block) {
            const SpriteObject& object = sprite.objects[block];
            if (object.x < 0 || object.y < 0 || object.x % 8 != 0 || object.y % 8 != 0) {
                error = "tetromino metasprite is not aligned to the 4x4 cell matrix";
                return false;
            }
            definition.cells[block] = {object.x / 8, object.y / 8};
            definition.tiles[block] = object.tile;
        }
        gameplay.data.piece_shapes[index] = definition.cells;
        gameplay.tetrominoes[index] = std::move(definition);
    }
    result = std::move(gameplay);
    return true;
}

} // namespace

std::size_t Catalog::tile_count() const {
    return gameplay_tiles.tiles.size() + font_tiles.tiles.size() + title_tiles.tiles.size() +
           multiplayer_tiles.tiles.size();
}

const Tilemap* Catalog::find_tilemap(std::string_view id) const {
    const auto found = std::ranges::find(tilemaps, id, [](const Tilemap& map) {
        return std::string_view(map.source.id);
    });
    return found == tilemaps.end() ? nullptr : &*found;
}

const ByteTable* Catalog::find_presentation(std::string_view id) const {
    const auto found = std::ranges::find(presentation, id, [](const ByteTable& table) {
        return std::string_view(table.source.id);
    });
    return found == presentation.end() ? nullptr : &*found;
}

bool extract_catalog(const Rom& rom, Catalog& result, std::string& error) {
    if (!validate_supported(rom, error))
        return false;

    Catalog catalog;
    catalog.profile = "tetris-jue-v1.1";
    catalog.source_sha1 = rom.digest;
    if (!extract_tiles(rom, tile_ranges[0], catalog.gameplay_tiles, error) ||
        !extract_tiles(rom, tile_ranges[1], catalog.font_tiles, error) ||
        !extract_tiles(rom, tile_ranges[2], catalog.title_tiles, error) ||
        !extract_tiles(rom, tile_ranges[3], catalog.multiplayer_tiles, error) ||
        !extract_demo(rom, {"type-a-demo", 0x62B0, 0x63B0, "joypad-rle"}, catalog.type_a_demo, error) ||
        !extract_demo(rom, {"type-b-demo", 0x63B0, 0x6450, "joypad-rle"}, catalog.type_b_demo, error))
        return false;

    for (const std::uint8_t code : rom.range(0x6450, 0x6480)) {
        if (code > 0x1B) {
            error = "invalid piece in demo sequence";
            return false;
        }
        catalog.demo_pieces.push_back({
            .kind = static_cast<PieceKind>(code / 4U),
            .rotation = static_cast<Rotation>(code & 3U),
        });
    }
    catalog.demo_piece_source = provenance("demo-pieces", {0x6450, 0x6480},
                                           "piece-codes", 48,
                                           catalog.demo_pieces.size());
    catalog.type_b_demo_garbage = {
        .source = provenance("type-b-demo-garbage", {0x1B40, 0x1B68},
                             "raw-board-cells", 40, 40),
        .bytes = {rom.bytes.begin() + 0x1B40, rom.bytes.begin() + 0x1B68},
    };

    for (const Range& range : presentation_ranges) {
        catalog.presentation.push_back({
            .source = source_for(range, range.end - range.begin, range.end - range.begin),
            .bytes = {rom.bytes.begin() + static_cast<std::ptrdiff_t>(range.begin),
                      rom.bytes.begin() + static_cast<std::ptrdiff_t>(range.end)},
        });
    }
    for (const MapRange& definition : tilemap_ranges) {
        const std::size_t cells = static_cast<std::size_t>(definition.width * definition.height);
        if (definition.range.end - definition.range.begin != cells + (definition.terminator ? 1U : 0U) ||
            (definition.terminator && rom.bytes[definition.range.end - 1] != 0xFF)) {
            error = "invalid tilemap range: " + std::string(definition.range.id);
            return false;
        }
        catalog.tilemaps.push_back({
            .source = source_for(definition.range, cells, cells),
            .width = definition.width,
            .height = definition.height,
            .tiles = {rom.bytes.begin() + static_cast<std::ptrdiff_t>(definition.range.begin),
                      rom.bytes.begin() + static_cast<std::ptrdiff_t>(definition.range.begin + cells)},
        });
    }
    if (!extract_sprites(rom, catalog.sprites, error) ||
        !extract_gameplay(rom, catalog.sprites, catalog.gameplay, error) ||
        !extract_audio(rom, catalog.audio, error))
        return false;
    result = std::move(catalog);
    return true;
}

} // namespace tetris::content
