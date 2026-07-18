#pragma once

#include "content/rom.hpp"
#include "content/audio.hpp"
#include "game/demo_data.hpp"
#include "game/input.hpp"
#include "game/piece.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tetris::content {

struct Provenance {
    std::string id;
    std::size_t begin{};
    std::size_t end{};
    std::string format;
};

struct Tile {
    std::array<std::uint8_t, 64> pixels{};
};

struct TileSet {
    Provenance source;
    std::vector<Tile> tiles;
};

struct ByteTable {
    Provenance source;
    std::vector<std::uint8_t> bytes;
};

struct Tilemap {
    Provenance source;
    int width{};
    int height{};
    std::vector<std::uint8_t> tiles;
};

struct Demo {
    Provenance source;
    std::vector<tetris::DemoRun> runs;
};

struct SpriteObject {
    int x{};
    int y{};
    std::uint8_t tile{};
    bool flip_x{};
};

struct Sprite {
    std::uint8_t id{};
    int origin_x{};
    int origin_y{};
    std::vector<SpriteObject> objects;
};

struct SpriteCatalog {
    Provenance source;
    std::vector<Sprite> sprites;
};

struct Catalog {
    std::string profile;
    std::string source_sha1;
    TileSet gameplay_tiles;
    TileSet font_tiles;
    TileSet title_tiles;
    TileSet multiplayer_tiles;
    Demo type_a_demo;
    Demo type_b_demo;
    Provenance demo_piece_source;
    std::vector<tetris::DemoPiece> demo_pieces;
    ByteTable type_b_demo_garbage;
    std::vector<ByteTable> presentation;
    std::vector<Tilemap> tilemaps;
    SpriteCatalog sprites;
    AudioCatalog audio;

    std::size_t tile_count() const;
    const Tilemap* find_tilemap(std::string_view id) const;
    const ByteTable* find_presentation(std::string_view id) const;
};

bool extract_catalog(const Rom& rom, Catalog& result, std::string& error);

} // namespace tetris::content
