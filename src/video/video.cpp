#include "video/video.hpp"
#include "video/controller_labels.hpp"
#include "video/drawing.hpp"
#include "video/endings.hpp"
#include "video/ui.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

namespace tetris::video {
namespace drawing {

constexpr int atlas_columns = 16;
constexpr std::array<SDL_Color, 4> palette = {{
    {224, 239, 207, 255},
    {155, 181, 139, 255},
    {75, 105, 73, 255},
    {22, 36, 28, 255},
}};

struct SelectedTile {
    const Atlas* atlas{};
    int index{};
};

// Build one RGBA texture from an extracted ROM tile bank.
SDL_Texture* build_texture(SDL_Renderer* renderer, const content::TileSet& tiles, bool one_bit,
                           int transparent_color) {
    const bool transparent = transparent_color >= 0;
    const int count = static_cast<int>(tiles.tiles.size());
    const int rows = std::max((count + atlas_columns - 1) / atlas_columns, 1);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                             SDL_TEXTUREACCESS_TARGET, atlas_columns * 8, rows * 8);
    if (texture == nullptr)
        return nullptr;
    (void)SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    (void)SDL_SetTextureBlendMode(texture, transparent ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);

    // Clear the atlas to an opaque background or transparent alpha.
    SDL_Texture* previous = SDL_GetRenderTarget(renderer);
    (void)SDL_SetRenderTarget(renderer, texture);
    SDL_SetRenderDrawColor(renderer, palette[0].r, palette[0].g, palette[0].b,
                           transparent ? 0 : 255);
    (void)SDL_RenderClear(renderer);

    // Expand each 2-bit Game Boy pixel into the RGBA atlas.
    for (int tile = 0; tile < count; ++tile) {
        const content::Tile& source = tiles.tiles[static_cast<std::size_t>(tile)];
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                std::uint8_t pixel = source.pixels[static_cast<std::size_t>(y * 8 + x)];
                if (one_bit && pixel != 0)
                    pixel = 3;
                if (transparent && pixel == transparent_color)
                    continue;
                const SDL_Color color = palette[static_cast<std::size_t>(pixel)];
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                const SDL_FRect point{static_cast<float>(tile % atlas_columns * 8 + x),
                                      static_cast<float>(tile / atlas_columns * 8 + y), 1, 1};
                (void)SDL_RenderFillRect(renderer, &point);
            }
        }
    }
    (void)SDL_SetRenderTarget(renderer, previous);
    return texture;
}

bool build_atlas(SDL_Renderer* renderer, const content::TileSet& tiles, Atlas& atlas,
                 bool one_bit = false) {
    atlas.opaque = build_texture(renderer, tiles, one_bit, -1);
    atlas.sprites = build_texture(renderer, tiles, one_bit, 0);
    atlas.count = static_cast<int>(tiles.tiles.size());
    return atlas.opaque != nullptr && atlas.sprites != nullptr;
}

constexpr std::uint8_t transparent_pixel = 0xFF;

const content::Tile* title_tile(const content::TileSet& tiles, std::uint8_t id) {
    constexpr int first_title_tile = 0x27;
    const int index = static_cast<int>(id) - first_title_tile;
    return index >= 0 && index < static_cast<int>(tiles.tiles.size())
               ? &tiles.tiles[static_cast<std::size_t>(index)]
               : nullptr;
}

void copy_title_tile(std::vector<std::uint8_t>& pixels, int width, int tile_x, int tile_y,
                     const content::Tile* tile) {
    if (tile == nullptr)
        return;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            pixels[static_cast<std::size_t>((tile_y * 8 + y) * width + tile_x * 8 + x)] =
                tile->pixels[static_cast<std::size_t>(y * 8 + x)];
        }
    }
}

void remove_exterior_sky(std::vector<std::uint8_t>& pixels, int width, int height) {
    std::vector<int> pending;
    const auto enqueue = [&](int x, int y) {
        const int index = y * width + x;
        if (pixels[static_cast<std::size_t>(index)] == 1)
            pending.push_back(index);
    };
    for (int x = 0; x < width; ++x)
        enqueue(x, 0);
    for (int y = 0; y < height; ++y) {
        enqueue(0, y);
        enqueue(width - 1, y);
    }
    while (!pending.empty()) {
        const int index = pending.back();
        pending.pop_back();
        if (pixels[static_cast<std::size_t>(index)] != 1)
            continue;
        pixels[static_cast<std::size_t>(index)] = transparent_pixel;
        const int x = index % width;
        const int y = index / width;
        if (x > 0)
            enqueue(x - 1, y);
        if (x + 1 < width)
            enqueue(x + 1, y);
        if (y > 0)
            enqueue(x, y - 1);
        if (y + 1 < height)
            enqueue(x, y + 1);
    }
}

bool upload_title_layer(SDL_Renderer* renderer, std::vector<std::uint8_t> pixels, int width,
                        int height, TitleLayer& layer) {
    layer.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                      SDL_TEXTUREACCESS_TARGET, width, height);
    if (layer.texture == nullptr)
        return false;
    layer.width = width;
    layer.height = height;
    (void)SDL_SetTextureScaleMode(layer.texture, SDL_SCALEMODE_NEAREST);
    (void)SDL_SetTextureBlendMode(layer.texture, SDL_BLENDMODE_BLEND);

    SDL_Texture* previous = SDL_GetRenderTarget(renderer);
    (void)SDL_SetRenderTarget(renderer, layer.texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    (void)SDL_RenderClear(renderer);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::uint8_t value = pixels[static_cast<std::size_t>(y * width + x)];
            if (value == transparent_pixel)
                continue;
            const SDL_Color color = palette[static_cast<std::size_t>(value)];
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
            const SDL_FRect point{static_cast<float>(x), static_cast<float>(y), 1, 1};
            (void)SDL_RenderFillRect(renderer, &point);
        }
    }
    (void)SDL_SetRenderTarget(renderer, previous);
    return true;
}

bool build_title_layers(SDL_Renderer* renderer, const content::Catalog& content, Video& video) {
    const content::Tilemap* title = content.find_tilemap("title");
    if (title == nullptr || title->width != 20 || title->height != 18)
        return false;
    const auto map_tile = [&](int column, int row) {
        const std::uint8_t id =
            title->tiles[static_cast<std::size_t>(row * title->width + column)];
        return title_tile(content.title_tiles, id);
    };

    std::vector<std::uint8_t> moon(16U * 16U, transparent_pixel);
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 2; ++column)
            copy_title_tile(moon, 16, column, row, map_tile(3 + column, 8 + row));

    std::vector<std::uint8_t> city(32U * 16U, transparent_pixel);
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 4; ++column)
            copy_title_tile(city, 32, column, row, map_tile(9 + column, 11 + row));

    // Assemble the foreground as one 48x48 image before masking. Flood filling
    // the connected sky preserves light palette pixels enclosed by buildings.
    std::vector<std::uint8_t> towers(48U * 48U, transparent_pixel);
    for (int row = 0; row < 5; ++row) {
        for (int column = 0; column < 6; ++column) {
            const std::uint8_t id = title->tiles[static_cast<std::size_t>(
                (8 + row) * title->width + 13 + column)];
            if (id != 0x3A && id != 0x3B)
                copy_title_tile(towers, 48, column, row + 1,
                                title_tile(content.title_tiles, id));
        }
    }
    remove_exterior_sky(towers, 48, 48);

    // The steeple shares two tiles with the static separator. Copy only pixels
    // that differ from the ordinary separator tile, after the sky flood fill.
    const content::Tile* separator = title_tile(content.title_tiles, 0x3C);
    for (int column = 0; column < 2; ++column) {
        const content::Tile* steeple = title_tile(content.title_tiles,
                                                  static_cast<std::uint8_t>(0x3D + column));
        if (steeple == nullptr || separator == nullptr)
            return false;
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                const auto source = static_cast<std::size_t>(y * 8 + x);
                if (steeple->pixels[source] == separator->pixels[source])
                    continue;
                const int destination_x = (column + 1) * 8 + x;
                towers[static_cast<std::size_t>(y * 48 + destination_x)] =
                    steeple->pixels[source];
            }
        }
    }

    // The source tiles also contain low skyline pixels at the lower-left edge
    // and between the two onion structures. Split those pixels by the tapered
    // building silhouettes instead of assigning the whole tile rectangle to
    // the foreground.
    std::vector<std::uint8_t> skyline(48U * 48U, transparent_pixel);
    const auto foreground_building = [](int x, int y) {
        if (y < 42)
            return true;
        const int row = y - 42;
        const int large_shrink = std::min(std::max(row - 1, 0), 3);
        const int large_left = 2 + large_shrink;
        const int large_right = 29 - large_shrink;
        const int small_left = row == 0 ? 38 : 37 - (row - 1);
        const int small_right = row == 0 ? 41 : 42 + (row - 1);
        return (x >= large_left && x <= large_right) ||
               (x >= small_left && x <= small_right);
    };
    for (int y = 42; y < 48; ++y) {
        for (int x = 0; x < 48; ++x) {
            const auto index = static_cast<std::size_t>(y * 48 + x);
            if (towers[index] == transparent_pixel || foreground_building(x, y))
                continue;
            skyline[index] = towers[index];
            towers[index] = transparent_pixel;
        }
    }

    remove_exterior_sky(moon, 16, 16);
    remove_exterior_sky(city, 32, 16);

    return upload_title_layer(renderer, std::move(moon), 16, 16, video.title_moon) &&
           upload_title_layer(renderer, std::move(city), 32, 16, video.title_city) &&
           upload_title_layer(renderer, std::move(skyline), 48, 48, video.title_skyline) &&
           upload_title_layer(renderer, std::move(towers), 48, 48, video.title_towers);
}

// Choose the tile bank implied by one screen tile identifier.
SelectedTile select(const Video& video, Bank bank, std::uint8_t id) {
    const Atlas* atlas = nullptr;
    int index = id;
    if (bank == Bank::multiplayer) {
        atlas = &video.multiplayer;
    } else if (id < 0x27) {
        atlas = &video.font;
    } else if (bank == Bank::title || id < 0x30) {
        atlas = &video.title;
        index -= 0x27;
    } else {
        atlas = &video.gameplay;
        index -= 0x30;
    }
    return index >= 0 && index < atlas->count ? SelectedTile{atlas, index} : SelectedTile{};
}

// Calculate an integer-scaled host placement for an original or extended scene.
Placement centered(const GubsyFrame& frame, Offset offset, int columns) {
    const float scene_width = static_cast<float>(columns * 8);
    const float scale =
        std::max(1.0F, std::floor(std::min(static_cast<float>(frame.render_width) / scene_width,
                                           static_cast<float>(frame.render_height) / 144.0F)));
    return {
        .x = std::round((static_cast<float>(frame.render_width) - scene_width * scale) * 0.5F +
                        offset.x * scale),
        .y = std::round((static_cast<float>(frame.render_height) - 144.0F * scale) * 0.5F +
                        offset.y * scale),
        .tile = 8.0F * scale,
    };
}

std::array<Placement, 2> versus_placements(const GubsyFrame& frame, Offset offset) {
    constexpr float screen_width = 160.0F;
    constexpr float screen_height = 144.0F;
    constexpr float gap_width = 16.0F;
    const float scale = std::max(
        1.0F, std::floor(std::min(static_cast<float>(frame.render_width) /
                                      (screen_width * 2.0F + gap_width),
                                  static_cast<float>(frame.render_height) / screen_height)));
    const float tile = 8.0F * scale;
    const float gap = gap_width * scale;
    const float width = screen_width * 2.0F * scale + gap;
    const float x = std::round((static_cast<float>(frame.render_width) - width) * 0.5F +
                               offset.x * scale);
    const float y = std::round(
        (static_cast<float>(frame.render_height) - screen_height * scale) * 0.5F +
        offset.y * scale);
    return {{{x, y, tile}, {x + screen_width * scale + gap, y, tile}}};
}

// Select the static background map for the current screen.
const char* map_for(const GameState& state, Bank& bank) {
    bank = Bank::gameplay;
    switch (state.screen) {
    case Screen::copyright_fixed:
    case Screen::copyright_skippable:
        bank = Bank::title;
        return "copyright";
    case Screen::title:
        bank = Bank::title;
        return "title";
    case Screen::game_type:
    case Screen::music:
        return "configuration";
    case Screen::type_a_level:
        return "type-a-difficulty";
    case Screen::type_b_level:
    case Screen::type_b_height:
    case Screen::name_entry:
        return state.selected_type == GameType::type_a ? "type-a-difficulty" : "type-b-difficulty";
    case Screen::versus_height:
        bank = Bank::multiplayer;
        return "multiplayer-difficulty";
    case Screen::versus_gameplay:
        bank = Bank::multiplayer;
        return "multiplayer-gameplay";
    case Screen::gameplay:
    case Screen::demo:
    case Screen::game_over:
    case Screen::type_b_celebration:
    case Screen::dancers:
    case Screen::scoreboard:
        return state.single_player.rules.type == GameType::type_b ? "type-b-gameplay"
                                                                  : "type-a-gameplay";
    default:
        return nullptr;
    }
}

// Draw raw tiles, maps, and OAM-style composite sprites.
void draw_texture_tile(SDL_Renderer* renderer, const Atlas& atlas, int index, float x, float y,
                       float size, bool sprite, bool flip, const SDL_Color* tint) {
    if (index < 0 || index >= atlas.count)
        return;
    const SDL_FRect source{static_cast<float>(index % atlas_columns * 8),
                           static_cast<float>(index / atlas_columns * 8), 8, 8};
    const SDL_FRect destination{x, y, size, size};
    SDL_Texture* texture = sprite ? atlas.sprites : atlas.opaque;
    if (tint != nullptr)
        (void)SDL_SetTextureColorMod(texture, tint->r, tint->g, tint->b);
    if (flip)
        (void)SDL_RenderTextureRotated(renderer, texture, &source, &destination, 0, nullptr,
                                       SDL_FLIP_HORIZONTAL);
    else
        (void)SDL_RenderTexture(renderer, texture, &source, &destination);
    if (tint != nullptr)
        (void)SDL_SetTextureColorMod(texture, 255, 255, 255);
}

void draw_tile(SDL_Renderer* renderer, const Video& video, Bank bank, std::uint8_t id,
               const Placement& placement, float column, float row, bool sprite, bool flip,
               const SDL_Color* tint) {
    const SelectedTile tile = select(video, bank, id);
    if (tile.atlas == nullptr)
        return;
    draw_texture_tile(renderer, *tile.atlas, tile.index, placement.x + column * placement.tile,
                      placement.y + row * placement.tile, placement.tile, sprite, flip, tint);
}

void draw_map(SDL_Renderer* renderer, const Video& video, Bank bank, const content::Tilemap& map,
              const Placement& placement, int origin_column, int origin_row) {
    for (int row = 0; row < map.height; ++row) {
        for (int column = 0; column < map.width; ++column) {
            const auto index = static_cast<std::size_t>(row * map.width + column);
            draw_tile(renderer, video, bank, map.tiles[index], placement,
                      static_cast<float>(origin_column + column),
                      static_cast<float>(origin_row + row));
        }
    }
}

void draw_title_layer(SDL_Renderer* renderer, const TitleLayer& layer,
                      const Placement& placement, float logical_x, float logical_y) {
    if (layer.texture == nullptr)
        return;
    const float pixel = placement.tile / 8.0F;
    const SDL_FRect destination{placement.x + logical_x * pixel,
                                placement.y + logical_y * pixel,
                                static_cast<float>(layer.width) * pixel,
                                static_cast<float>(layer.height) * pixel};
    (void)SDL_RenderTexture(renderer, layer.texture, nullptr, &destination);
}

int title_sway(std::uint64_t elapsed, int amplitude, float phase = 0.0F) {
    constexpr float circle = 6.28318530718F;
    constexpr float period = 12'000.0F;
    const float angle = static_cast<float>(elapsed % 12'000U) / period * circle + phase;
    return static_cast<int>(std::round(std::sin(angle) * static_cast<float>(amplitude)));
}

void draw_title_parallax(SDL_Renderer* renderer, const Video& video,
                         const Placement& placement) {
    if (video.title_moon.texture == nullptr || video.title_city.texture == nullptr ||
        video.title_skyline.texture == nullptr || video.title_towers.texture == nullptr)
        return;

    // Limit animation to the five-row landscape viewport inside the original
    // title frame. The logo, separator, and copyright text remain untouched.
    const SDL_Rect landscape_clip{
        static_cast<int>(std::round(placement.x + placement.tile)),
        static_cast<int>(std::round(placement.y + 7.0F * placement.tile)),
        static_cast<int>(std::round(18.0F * placement.tile)),
        static_cast<int>(std::round(6.0F * placement.tile)),
    };
    (void)SDL_SetRenderClipRect(renderer, &landscape_clip);
    SDL_SetRenderDrawColor(renderer, palette[1].r, palette[1].g, palette[1].b, 255);
    const SDL_FRect sky{static_cast<float>(landscape_clip.x),
                        placement.y + 8.0F * placement.tile,
                        static_cast<float>(landscape_clip.w),
                        5.0F * placement.tile};
    (void)SDL_RenderFillRect(renderer, &sky);

    // Remove the original stationary church steeple from the separator line.
    // Its replacement travels with the rest of the foreground motif below.
    draw_tile(renderer, video, Bank::title, 0x3C, placement, 14.0F, 7.0F);
    draw_tile(renderer, video, Bank::title, 0x3C, placement, 15.0F, 7.0F);

    // The moon is the farthest layer, so it traces only a two-pixel oval.
    const std::uint64_t elapsed = SDL_GetTicks();
    const float moon_x = static_cast<float>(24 + title_sway(elapsed, 2));
    const float moon_y = static_cast<float>(64 + title_sway(elapsed, 1, 1.57079632679F));
    draw_title_layer(renderer, video.title_moon, placement, moon_x, moon_y);

    // The distant city sways five pixels with the implied camera movement.
    const int city_origin = 9 * 8 + title_sway(elapsed, 5);
    draw_title_layer(renderer, video.title_city, placement, static_cast<float>(city_origin),
                     88.0F);
    draw_title_layer(renderer, video.title_skyline, placement,
                     static_cast<float>(13 * 8 + title_sway(elapsed, 5)), 56.0F);

    // Snow belongs in the middle depth plane: it passes in front of the moon
    // and distant city, then disappears behind the foreground onion towers.
    struct Flake {
        int x;
        int phase;
        int direction;
    };
    constexpr std::array flakes = {
        Flake{18, 0, 0},  Flake{34, 13, 1}, Flake{51, 27, 0}, Flake{66, 7, 1},
        Flake{82, 34, 1}, Flake{99, 19, 0}, Flake{116, 4, 1}, Flake{132, 23, 0},
        Flake{143, 37, 1},
    };
    const int flake_step = static_cast<int>(SDL_GetTicks() / 90U);
    const float pixel = placement.tile / 8.0F;
    SDL_SetRenderDrawColor(renderer, palette[0].r, palette[0].g, palette[0].b, 255);
    for (const Flake flake : flakes) {
        const int age = (flake_step + flake.phase) % 40;
        const int diagonal = (age + flake.direction * 8) % 16;
        const int drift = diagonal < 8 ? diagonal - 4 : 12 - diagonal;
        const int x = 8 + (flake.x + drift + 144) % 144;
        const int y = 64 + age;
        const float host_x = placement.x + static_cast<float>(x) * pixel;
        const float host_y = placement.y + static_cast<float>(y) * pixel;
        const SDL_FRect center{host_x, host_y, pixel, pixel};
        (void)SDL_RenderFillRect(renderer, &center);
        if (flake.phase % 3 == 0) {
            const SDL_FRect lower{host_x, host_y + pixel, pixel, pixel};
            (void)SDL_RenderFillRect(renderer, &lower);
        }
    }

    // The foreground church moves twelve pixels in the opposite direction.
    // Its steeple and low adjacent buildings remain part of the same motif.
    const int tower_origin = 13 * 8 - title_sway(elapsed, 12);
    draw_title_layer(renderer, video.title_towers, placement, static_cast<float>(tower_origin),
                     56.0F);
    (void)SDL_SetRenderClipRect(renderer, nullptr);
}

std::uint8_t piece_id(const FallingPiece& piece) {
    return static_cast<std::uint8_t>(static_cast<unsigned int>(piece.kind) * 4U +
                                     static_cast<unsigned int>(piece.rotation));
}

std::uint8_t piece_id(PieceSpec piece) {
    return static_cast<std::uint8_t>(static_cast<unsigned int>(piece.kind) * 4U +
                                     static_cast<unsigned int>(piece.rotation));
}

struct ByteResult {
    std::uint8_t value{};
    bool carry{};
};

ByteResult add_byte(std::uint8_t first, std::uint8_t second, bool carry = false) {
    const unsigned int sum = static_cast<unsigned int>(first) +
                             static_cast<unsigned int>(second) +
                             static_cast<unsigned int>(carry);
    return {.value = static_cast<std::uint8_t>(sum), .carry = sum > 0xFFU};
}

ByteResult subtract_byte(std::uint8_t first, std::uint8_t second, bool borrow = false) {
    const unsigned int amount =
        static_cast<unsigned int>(second) + static_cast<unsigned int>(borrow);
    return {
        .value = static_cast<std::uint8_t>(static_cast<unsigned int>(first) - amount),
        .carry = static_cast<unsigned int>(first) < amount,
    };
}

SpritePosition sprite_object_position(const content::Sprite& sprite,
                                      const content::SpriteObject& object, std::uint8_t raw_x,
                                      std::uint8_t raw_y, bool flip) {
    std::uint8_t oam_x{};
    if (flip) {
        const ByteResult without_origin =
            subtract_byte(raw_x, static_cast<std::uint8_t>(sprite.origin_x));
        const ByteResult without_object =
            subtract_byte(without_origin.value, static_cast<std::uint8_t>(object.x),
                          without_origin.carry);
        oam_x = subtract_byte(without_object.value, 8, without_object.carry).value;
    } else {
        const ByteResult with_origin =
            add_byte(raw_x, static_cast<std::uint8_t>(sprite.origin_x));
        oam_x = add_byte(with_origin.value, static_cast<std::uint8_t>(object.x),
                         with_origin.carry)
                    .value;
    }

    const ByteResult with_origin =
        add_byte(raw_y, static_cast<std::uint8_t>(sprite.origin_y));
    const std::uint8_t oam_y =
        add_byte(with_origin.value, static_cast<std::uint8_t>(object.y), with_origin.carry).value;
    return {
        .x = static_cast<int>(oam_x) - 8,
        .y = static_cast<int>(oam_y) - 16,
        .flip = object.flip_x != flip,
    };
}

void draw_sprite(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                 Bank bank, std::uint8_t id, const Placement& placement, std::uint8_t raw_x,
                 std::uint8_t raw_y, bool flip, bool bounds, const SDL_Color* tint,
                 MotionOffset motion) {
    if (id >= content.sprites.sprites.size())
        return;
    const content::Sprite& sprite = content.sprites.sprites[id];
    for (const content::SpriteObject object : sprite.objects) {
        const SpritePosition position =
            sprite_object_position(sprite, object, raw_x, raw_y, flip);
        const float column = (static_cast<float>(position.x) + motion.x) / 8.0F;
        const float row = (static_cast<float>(position.y) + motion.y) / 8.0F;
        draw_tile(renderer, video, bank, object.tile, placement, column, row, true, position.flip,
                  tint);
        if (bounds) {
            SDL_SetRenderDrawColor(renderer, 255, 72, 72, 220);
            const SDL_FRect rect{placement.x + column * placement.tile,
                                 placement.y + row * placement.tile, placement.tile,
                                 placement.tile};
            (void)SDL_RenderRect(renderer, &rect);
        }
    }
}

// Draw numbers and text with the extracted Game Boy font.
void number(SDL_Renderer* renderer, const Video& video, Bank bank, const Placement& placement,
            std::uint32_t value, int right, int row, int width) {
    for (int digit = 0; digit < width; ++digit) {
        draw_tile(renderer, video, bank, static_cast<std::uint8_t>(value % 10U), placement,
                  static_cast<float>(right - digit), static_cast<float>(row));
        value /= 10U;
        if (value == 0)
            break;
    }
}

void left_number(SDL_Renderer* renderer, const Video& video, Bank bank, const Placement& placement,
                 std::uint32_t value, int column, int row) {
    std::array<std::uint8_t, 10> digits{};
    int count = 0;
    do {
        digits[static_cast<std::size_t>(count++)] = static_cast<std::uint8_t>(value % 10U);
        value /= 10U;
    } while (value != 0 && count < static_cast<int>(digits.size()));
    for (int index = count - 1; index >= 0; --index) {
        draw_tile(renderer, video, bank, digits[static_cast<std::size_t>(index)], placement,
                  static_cast<float>(column + count - 1 - index), static_cast<float>(row));
    }
}

std::uint8_t character_tile(char character) {
    if (character >= '0' && character <= '9')
        return static_cast<std::uint8_t>(character - '0');
    if (character >= 'a' && character <= 'z')
        character = static_cast<char>(character - 'a' + 'A');
    if (character >= 'A' && character <= 'Z')
        return static_cast<std::uint8_t>(character - 'A' + 0x0A);
    if (character == '.')
        return 0x24;
    if (character == '-')
        return 0x25;
    if (character == '*')
        return 0x26;
    if (character == '~')
        return 0x27;
    return 0x2F;
}

void text(SDL_Renderer* renderer, const Video& video, Bank bank, const Placement& placement,
          const char* value, int column, int row) {
    for (int index = 0; value[index] != '\0'; ++index) {
        draw_tile(renderer, video, bank, character_tile(value[index]), placement,
                  static_cast<float>(column + index), static_cast<float>(row));
    }
}

// Convert logical board blocks to their extracted gameplay tiles.
std::uint8_t tile_for(Block block) {
    switch (block) {
    case Block::empty:
        return 0;
    case Block::l:
        return 0x84;
    case Block::j:
        return 0x81;
    case Block::o:
        return 0x83;
    case Block::s:
        return 0x82;
    case Block::z:
        return 0x86;
    case Block::t:
        return 0x85;
    case Block::i_horizontal_first:
        return 0x8A;
    case Block::i_horizontal_middle:
        return 0x8B;
    case Block::i_horizontal_last:
        return 0x8F;
    case Block::i_vertical_first:
        return 0x80;
    case Block::i_vertical_middle:
        return 0x88;
    case Block::i_vertical_last:
        return 0x89;
    default:
        return static_cast<std::uint8_t>(0x80U + static_cast<unsigned int>(block) -
                                         static_cast<unsigned int>(Block::garbage_0));
    }
}

Block block_for(PieceKind kind) {
    switch (kind) {
    case PieceKind::L: return Block::l;
    case PieceKind::J: return Block::j;
    case PieceKind::O: return Block::o;
    case PieceKind::S: return Block::s;
    case PieceKind::Z: return Block::z;
    case PieceKind::T: return Block::t;
    case PieceKind::I: return Block::i_horizontal_middle;
    }
    return Block::t;
}

SDL_Color guideline_color(PieceKind kind) {
    switch (kind) {
    case PieceKind::I: return {70, 205, 215, 255};
    case PieceKind::J: return {75, 105, 205, 255};
    case PieceKind::L: return {225, 145, 55, 255};
    case PieceKind::O: return {225, 205, 70, 255};
    case PieceKind::S: return {85, 185, 95, 255};
    case PieceKind::Z: return {210, 75, 75, 255};
    case PieceKind::T: return {155, 90, 190, 255};
    }
    return palette[1];
}

PieceKind piece_kind(Block block) {
    switch (block) {
    case Block::l: return PieceKind::L;
    case Block::j: return PieceKind::J;
    case Block::o: return PieceKind::O;
    case Block::s: return PieceKind::S;
    case Block::z: return PieceKind::Z;
    case Block::t: return PieceKind::T;
    case Block::i_horizontal_first:
    case Block::i_horizontal_middle:
    case Block::i_horizontal_last:
    case Block::i_vertical_first:
    case Block::i_vertical_middle:
    case Block::i_vertical_last:
        return PieceKind::I;
    default:
        return PieceKind::T;
    }
}

bool is_piece_block(Block block) {
    return block >= Block::l && block <= Block::i_vertical_last;
}

void draw_mini_piece(SDL_Renderer* renderer, const GameplayData& data, PieceSpec piece,
                     const Placement& placement, float column, float row,
                     settings::PieceColors colors) {
    const PieceShape cells = piece_cells(data, piece);
    int minimum_x = cells[0].x;
    int minimum_y = cells[0].y;
    for (const Cell cell : cells) {
        minimum_x = std::min(minimum_x, cell.x);
        minimum_y = std::min(minimum_y, cell.y);
    }
    const float size = placement.tile * 0.5F;
    const float inset = std::max(1.0F, placement.tile / 8.0F);
    const SDL_Color fill_color = colors == settings::PieceColors::guideline
                                     ? guideline_color(piece.kind)
                                     : palette[1];
    for (const Cell cell : cells) {
        const float x = placement.x + column * placement.tile +
                        static_cast<float>(cell.x - minimum_x) * size;
        const float y = placement.y + row * placement.tile +
                        static_cast<float>(cell.y - minimum_y) * size;
        const SDL_FRect block{x, y, size, size};
        SDL_SetRenderDrawColor(renderer, palette[3].r, palette[3].g, palette[3].b, 255);
        (void)SDL_RenderFillRect(renderer, &block);
        const SDL_FRect center{x + inset, y + inset, std::max(size - inset * 2.0F, 1.0F),
                               std::max(size - inset * 2.0F, 1.0F)};
        SDL_SetRenderDrawColor(renderer, fill_color.r, fill_color.g, fill_color.b, 255);
        (void)SDL_RenderFillRect(renderer, &center);
    }
}

void draw_panel(SDL_Renderer* renderer, const Placement& placement, int column, int row, int width,
                int height) {
    // Draw a scalable Game Boy-style frame in logical pixels. ROM panel-edge
    // tiles contain horizontal decoration and cannot be repeated vertically.
    const float pixel = placement.tile / 8.0F;
    const SDL_FRect outer{placement.x + static_cast<float>(column) * placement.tile,
                          placement.y + static_cast<float>(row) * placement.tile,
                          static_cast<float>(width) * placement.tile,
                          static_cast<float>(height) * placement.tile};
    SDL_SetRenderDrawColor(renderer, palette[2].r, palette[2].g, palette[2].b, 255);
    (void)SDL_RenderFillRect(renderer, &outer);

    const SDL_FRect interior{outer.x + pixel, outer.y + pixel, outer.w - pixel * 2.0F,
                             outer.h - pixel * 2.0F};
    SDL_SetRenderDrawColor(renderer, palette[0].r, palette[0].g, palette[0].b, 255);
    (void)SDL_RenderFillRect(renderer, &interior);
    SDL_SetRenderDrawColor(renderer, palette[3].r, palette[3].g, palette[3].b, 255);
    (void)SDL_RenderRect(renderer, &outer);

    const SDL_FRect inner_line{outer.x + pixel * 2.0F, outer.y + pixel * 2.0F,
                               outer.w - pixel * 4.0F, outer.h - pixel * 4.0F};
    SDL_SetRenderDrawColor(renderer, palette[1].r, palette[1].g, palette[1].b, 255);
    (void)SDL_RenderRect(renderer, &inner_line);
}

bool uses_modern_hud(const SinglePlayer& game) {
    return game.rules.type != GameType::versus &&
           (game.options.hold_piece || game.options.next_previews > 1 ||
            game.options.challenge != ChallengeMode::marathon || game.options.modern_scoring);
}

void draw_extended_modern_hud(SDL_Renderer* renderer, const Video& video, Bank bank,
                              const SinglePlayer& game, const Placement& placement,
                              settings::PieceColors colors) {
    // Mirror the outer wall at the left edge. Composition crops the original
    // cartridge's unused column zero, leaving one deliberate wall on each side.
    for (int row = 0; row < 18; ++row) {
        const auto wall = static_cast<std::uint8_t>(0x7B + row % 3);
        draw_tile(renderer, video, bank, wall, placement, 1.0F, static_cast<float>(row), false,
                  true);
    }

    // Preserve the board's original right wall at column 12. The Modern HUD
    // occupies twelve new interior columns and receives its own outer wall.
    SDL_SetRenderDrawColor(renderer, palette[3].r, palette[3].g, palette[3].b, 255);
    const SDL_FRect background{placement.x + 13.0F * placement.tile, placement.y,
                               13.0F * placement.tile, 18.0F * placement.tile};
    (void)SDL_RenderFillRect(renderer, &background);
    for (int row = 0; row < 18; ++row) {
        const auto wall = static_cast<std::uint8_t>(0x7B + row % 3);
        draw_tile(renderer, video, bank, wall, placement, 25.0F, static_cast<float>(row));
    }

    // Two equal upper bays keep HOLD and NEXT readable at the original font size.
    draw_panel(renderer, placement, 13, 0, 6, 10);
    draw_panel(renderer, placement, 19, 0, 6, 10);
    text(renderer, video, bank, placement, "HOLD", 14, 1);
    if (game.held_piece)
        draw_mini_piece(renderer, game.data, *game.held_piece, placement, 15.0F, 4.0F, colors);

    text(renderer, video, bank, placement, "NEXT", 20, 1);
    const int previews = std::clamp(game.options.next_previews, 1, 5);
    for (int index = 0; index < previews; ++index) {
        const float row = 2.2F + static_cast<float>(index) * 1.35F;
        draw_mini_piece(renderer, game.data, game.next_piece(index), placement, 21.0F, row,
                        colors);
    }

    // One full-width lower bay leaves room for unabridged labels and values.
    draw_panel(renderer, placement, 13, 10, 12, 8);
    text(renderer, video, bank, placement, "SCORE", 14, 11);
    number(renderer, video, bank, placement, game.score, 23, 12, 9);
    if (game.rules.type == GameType::type_b) {
        text(renderer, video, bank, placement, "LVL", 14, 13);
        number(renderer, video, bank, placement, static_cast<std::uint32_t>(game.level), 18, 13,
               2);
        text(renderer, video, bank, placement, "HT", 20, 13);
        number(renderer, video, bank, placement,
               static_cast<std::uint32_t>(game.rules.type_b_height), 23, 13, 1);
        text(renderer, video, bank, placement, "LEFT", 14, 14);
    } else {
        text(renderer, video, bank, placement, "LEVEL", 14, 13);
        number(renderer, video, bank, placement, static_cast<std::uint32_t>(game.level), 23, 13,
               2);
        text(renderer, video, bank, placement, "LINES", 14, 14);
    }
    number(renderer, video, bank, placement, static_cast<std::uint32_t>(game.lines), 23, 14, 5);
    if (game.rules.type == GameType::type_a &&
        game.options.challenge == ChallengeMode::sprint) {
        text(renderer, video, bank, placement, "SPRINT", 14, 15);
    } else if (game.rules.type == GameType::type_a &&
               game.options.challenge == ChallengeMode::ultra) {
        text(renderer, video, bank, placement, "ULTRA", 14, 15);
        const int seconds = std::max((game.time_remaining_steps + 59) / 60, 0);
        number(renderer, video, bank, placement, static_cast<std::uint32_t>(seconds), 23, 16, 4);
    } else {
        text(renderer, video, bank, placement, "COMBO", 14, 15);
        if (game.statistics.combo >= 0)
            number(renderer, video, bank, placement,
                   static_cast<std::uint32_t>(game.statistics.combo), 23, 16, 3);
    }
}

void draw_compact_modern_hud(SDL_Renderer* renderer, const Video& video, Bank bank,
                             const SinglePlayer& game, const Placement& placement,
                             settings::PieceColors colors) {
    // The compact HUD stays inside the original seven-column side panel.
    SDL_SetRenderDrawColor(renderer, palette[0].r, palette[0].g, palette[0].b, 255);
    const SDL_FRect panel{placement.x + 13.0F * placement.tile, placement.y,
                          7.0F * placement.tile, 18.0F * placement.tile};
    (void)SDL_RenderFillRect(renderer, &panel);

    text(renderer, video, bank, placement, "HOLD", 13, 0);
    if (game.held_piece)
        draw_mini_piece(renderer, game.data, *game.held_piece, placement, 15.0F, 1.2F, colors);
    text(renderer, video, bank, placement, "NEXT", 13, 3);
    const int previews = std::clamp(game.options.next_previews, 1, 5);
    for (int index = 0; index < previews; ++index) {
        const float column = 13.0F + static_cast<float>(index % 2) * 3.0F;
        const float row = 4.2F + static_cast<float>(index / 2) * 1.45F;
        draw_mini_piece(renderer, game.data, game.next_piece(index), placement, column, row,
                        colors);
    }

    text(renderer, video, bank, placement, "SCORE", 13, 9);
    number(renderer, video, bank, placement, game.score, 19, 10, 7);
    text(renderer, video, bank, placement, "LVL", 13, 12);
    number(renderer, video, bank, placement, static_cast<std::uint32_t>(game.level), 19, 12, 2);
    text(renderer, video, bank, placement,
         game.rules.type == GameType::type_b ? "LEFT" : "LINE", 13, 13);
    number(renderer, video, bank, placement, static_cast<std::uint32_t>(game.lines), 19, 13, 3);
    if (game.rules.type == GameType::type_b) {
        text(renderer, video, bank, placement, "HT", 17, 14);
        number(renderer, video, bank, placement,
               static_cast<std::uint32_t>(game.rules.type_b_height), 19, 14, 1);
    }
    text(renderer, video, bank, placement, "COMBO", 13, 15);
    if (game.statistics.combo >= 0)
        number(renderer, video, bank, placement,
               static_cast<std::uint32_t>(game.statistics.combo), 19, 16, 3);
}

void draw_modern_hud(SDL_Renderer* renderer, const Video& video, Bank bank,
                     const SinglePlayer& game, const Placement& placement, bool extended,
                     settings::PieceColors colors) {
    if (extended)
        draw_extended_modern_hud(renderer, video, bank, game, placement, colors);
    else
        draw_compact_modern_hud(renderer, video, bank, game, placement, colors);
}

void draw_session(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                  Bank bank, const SinglePlayer& game, const Placement& placement, bool active,
                  const settings::Settings& settings, bool sprite_bounds, bool board_cells,
                  bool extended_hud, MotionOffset motion) {
    // Draw the settled board and line-clear animation.
    for (int row = 0; row < board_height; ++row) {
        const bool clearing =
            std::ranges::find(game.clearing_rows, row) != game.clearing_rows.end();
        for (int column = 0; column < board_width; ++column) {
            const Block block = game.visible_board().at({column, row});
            std::uint8_t tile = tile_for(block);
            if (game.state == PlayState::clearing && clearing) {
                const int final_step = settings.line_clear_speed == LineClearSpeed::original ? 6
                                       : settings.line_clear_speed == LineClearSpeed::fast   ? 4
                                                                                             : 1;
                if (game.animation_step() == final_step)
                    tile = 0;
                else if (!settings.reduced_flash && game.animation_step() % 2 == 0)
                    tile = 0x8C;
            }
            if (tile != 0) {
                const SDL_Color tint = guideline_color(piece_kind(block));
                const SDL_Color* tint_pointer =
                    settings.piece_colors == settings::PieceColors::guideline &&
                            is_piece_block(block) && tile == tile_for(block)
                        ? &tint
                        : nullptr;
                draw_tile(renderer, video, bank, tile, placement, static_cast<float>(column + 2),
                          static_cast<float>(row), false, false, tint_pointer);
            }
            if (board_cells && block != Block::empty) {
                SDL_SetRenderDrawColor(renderer, 255, 196, 64, 220);
                const SDL_FRect rect{placement.x + static_cast<float>(column + 2) * placement.tile,
                                     placement.y + static_cast<float>(row) * placement.tile,
                                     placement.tile, placement.tile};
                (void)SDL_RenderRect(renderer, &rect);
            }
        }
    }

    // The modern ghost is a translucent palette-grey landing silhouette.
    if (active && game.state == PlayState::falling && game.options.ghost_piece) {
        const FallingPiece ghost = game.landing_piece();
        if (ghost.origin != game.piece.origin) {
            (void)SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, palette[1].r, palette[1].g, palette[1].b, 135);
            for (const Cell cell : occupied_cells(game.data, ghost)) {
                if (cell.y < 0)
                    continue;
                const float inset = std::max(1.0F, placement.tile / 8.0F);
                const SDL_FRect shadow{
                    placement.x + static_cast<float>(cell.x + 2) * placement.tile + inset,
                    placement.y + static_cast<float>(cell.y) * placement.tile + inset,
                    placement.tile - inset * 2.0F, placement.tile - inset * 2.0F};
                (void)SDL_RenderFillRect(renderer, &shadow);
            }
            (void)SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }
    }

    // Draw the active piece from its ROM placement table.
    const content::ByteTable* active_placement = content.find_layout("active-piece-placement");
    if (active && game.state == PlayState::falling && active_placement != nullptr) {
        const FallingPiece& piece = game.piece;
        const auto raw_x =
            static_cast<std::uint8_t>(active_placement->bytes[2] + (piece.origin.x - 3) * 8);
        const auto raw_y =
            static_cast<std::uint8_t>(active_placement->bytes[1] + (piece.origin.y + 1) * 8);
        const SDL_Color tint = guideline_color(piece.kind);
        const SDL_Color* tint_pointer =
            settings.piece_colors == settings::PieceColors::guideline ? &tint : nullptr;
        draw_sprite(renderer, video, content, bank, piece_id(piece), placement, raw_x, raw_y,
                    false, sprite_bounds, tint_pointer, motion);
        if (board_cells) {
            SDL_SetRenderDrawColor(renderer, 72, 224, 255, 230);
            for (const Cell cell : occupied_cells(content.gameplay.data, piece)) {
                const SDL_FRect rect{placement.x + static_cast<float>(cell.x + 2) * placement.tile,
                                     placement.y + static_cast<float>(cell.y) * placement.tile,
                                     placement.tile, placement.tile};
                (void)SDL_RenderRect(renderer, &rect);
            }
        }
    }

    // Draw the next-piece preview.
    const bool modern_hud = uses_modern_hud(game);
    if (game.preview_visible && !modern_hud) {
        const content::ByteTable* preview = content.find_layout("preview-piece-placement");
        if (preview != nullptr) {
            const SDL_Color tint = guideline_color(game.preview.kind);
            const SDL_Color* tint_pointer =
                settings.piece_colors == settings::PieceColors::guideline ? &tint : nullptr;
            draw_sprite(renderer, video, content, bank, piece_id(game.preview), placement,
                        preview->bytes[2], preview->bytes[1], false, sprite_bounds,
                        tint_pointer);
        }
    }

    // Draw mode-specific score, level, height, and line values.
    if (modern_hud) {
        draw_modern_hud(renderer, video, bank, game, placement, extended_hud,
                        settings.piece_colors);
    } else if (game.rules.type == GameType::type_a) {
        number(renderer, video, bank, placement, game.score, 18, 3, 6);
        number(renderer, video, bank, placement, static_cast<std::uint32_t>(game.level), 17, 7, 2);
        if (game.rules.heart_mode)
            draw_tile(renderer, video, bank, 0x27, placement, 18, 7);
    } else {
        number(renderer, video, bank, placement, static_cast<std::uint32_t>(game.level), 16, 2, 1);
        number(renderer, video, bank, placement,
               static_cast<std::uint32_t>(game.rules.type_b_height), 16, 5, 1);
    }
    if (!modern_hud)
        number(renderer, video, bank, placement, static_cast<std::uint32_t>(game.lines), 17, 10, 4);

    if (game.state == PlayState::complete && game.rules.type == GameType::type_a &&
        game.options.challenge != ChallengeMode::marathon) {
        text(renderer, video, bank, placement, "COMPLETE", 3, 8);
    }

    // Add the optional native line-clear beam and embers.
    if (settings.effects != settings::Intensity::off && game.state == PlayState::clearing &&
        !game.clearing_rows.empty()) {
        const int final_step = settings.line_clear_speed == LineClearSpeed::original ? 6
                               : settings.line_clear_speed == LineClearSpeed::fast   ? 4
                                                                                     : 1;
        const float progress = std::clamp(static_cast<float>(game.animation_step()) /
                                              static_cast<float>(std::max(final_step, 1)),
                                          0.0F, 1.0F);
        const float width = 10.0F * placement.tile * progress;
        SDL_SetRenderDrawColor(renderer, 224, 239, 207,
                               settings.effects == settings::Intensity::full ? 235 : 170);
        (void)SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        for (const int row : game.clearing_rows) {
            const SDL_FRect beam{placement.x + 7.0F * placement.tile - width * 0.5F,
                                 placement.y + (static_cast<float>(row) + 0.4F) * placement.tile,
                                 width, std::max(1.0F, placement.tile * 0.2F)};
            (void)SDL_RenderFillRect(renderer, &beam);
            const int embers = settings.effects == settings::Intensity::full ? 18 : 8;
            for (int ember = 0; ember < embers; ++ember) {
                const std::uint32_t seed =
                    static_cast<std::uint32_t>(row * 131 + ember * 47 + game.animation_step() * 73);
                const float pixel = std::max(1.0F, placement.tile / 8.0F);
                const float side = (seed & 1U) != 0 ? 1.0F : -1.0F;
                const SDL_FRect spark{
                    placement.x + 7.0F * placement.tile + side * width * 0.5F +
                        side * static_cast<float>((seed >> 4U) % 7U) * pixel,
                    placement.y + (static_cast<float>(row) + 0.5F) * placement.tile +
                        static_cast<float>(static_cast<int>((seed >> 12U) % 9U) - 4) * pixel,
                    pixel, pixel};
                (void)SDL_RenderFillRect(renderer, &spark);
            }
        }
        (void)SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
}

void draw_grid(SDL_Renderer* renderer, const Placement& placement) {
    (void)SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 80, 220, 255, 110);
    for (int column = 0; column <= 20; ++column) {
        const float x = placement.x + static_cast<float>(column) * placement.tile;
        (void)SDL_RenderLine(renderer, x, placement.y, x, placement.y + 18 * placement.tile);
    }
    for (int row = 0; row <= 18; ++row) {
        const float y = placement.y + static_cast<float>(row) * placement.tile;
        (void)SDL_RenderLine(renderer, placement.x, y, placement.x + 20 * placement.tile, y);
    }
    (void)SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

std::uint32_t particle_seed(int index) {
    std::uint32_t seed = static_cast<std::uint32_t>(index) + 17U;
    seed ^= seed >> 16U;
    seed *= 0x7FEB352DU;
    seed ^= seed >> 15U;
    seed *= 0x846CA68BU;
    seed ^= seed >> 16U;
    return seed;
}

void draw_particle_pixel(SDL_Renderer* renderer, int x, int y, int pixel) {
    const SDL_FRect point{static_cast<float>(x * pixel), static_cast<float>(y * pixel),
                          static_cast<float>(pixel), static_cast<float>(pixel)};
    (void)SDL_RenderFillRect(renderer, &point);
}

void draw_star(SDL_Renderer* renderer, int x, int y, int pixel, int detail) {
    draw_particle_pixel(renderer, x, y, pixel);
    if (detail == 0)
        return;
    draw_particle_pixel(renderer, x - 1, y, pixel);
    draw_particle_pixel(renderer, x + 1, y, pixel);
    draw_particle_pixel(renderer, x, y - 1, pixel);
    draw_particle_pixel(renderer, x, y + 1, pixel);
    if (detail == 1)
        return;
    draw_particle_pixel(renderer, x - 2, y, pixel);
    draw_particle_pixel(renderer, x + 2, y, pixel);
    draw_particle_pixel(renderer, x, y - 2, pixel);
    draw_particle_pixel(renderer, x, y + 2, pixel);
}

void draw_snowflake(SDL_Renderer* renderer, int x, int y, int pixel, int detail) {
    draw_particle_pixel(renderer, x, y, pixel);
    if (detail == 0)
        return;
    if (detail == 1) {
        draw_particle_pixel(renderer, x - 1, y - 1, pixel);
        draw_particle_pixel(renderer, x + 1, y - 1, pixel);
        draw_particle_pixel(renderer, x - 1, y + 1, pixel);
        draw_particle_pixel(renderer, x + 1, y + 1, pixel);
        return;
    }
    draw_particle_pixel(renderer, x - 1, y, pixel);
    draw_particle_pixel(renderer, x + 1, y, pixel);
    draw_particle_pixel(renderer, x, y - 1, pixel);
    draw_particle_pixel(renderer, x, y + 1, pixel);
}

// Draw host-side rendering around the losslessly scaled Game Boy scene.
void draw_background(SDL_Renderer* renderer, const GubsyFrame& frame,
                     const settings::Settings& settings, float pulse) {
    SDL_SetRenderDrawColor(renderer, palette[3].r, palette[3].g, palette[3].b, 255);
    (void)SDL_RenderClear(renderer);
    if (settings.layout != settings::Layout::widescreen_frame ||
        settings.background == settings::Background::off)
        return;
    constexpr std::array<SDL_Color, 3> star_colors = {{
        {48, 98, 48, 255},
        {139, 172, 15, 255},
        {224, 248, 208, 255},
    }};
    constexpr std::array<SDL_Color, 3> snow_colors = {{
        {75, 105, 73, 255},
        {224, 239, 207, 255},
        {224, 239, 207, 255},
    }};
    const int width = std::max(frame.render_width, 1);
    const int height = std::max(frame.render_height, 1);
    const int pixel = std::max(1, std::min(width / 320, height / 180));
    const int logical_width = std::max(width / pixel, 1);
    const int logical_height = std::max(height / pixel, 1);
    const std::uint64_t elapsed = settings.moving_background ? SDL_GetTicks() / 90U : 0U;
    const bool snow = settings.background == settings::Background::snow;
    const int particle_count = snow ? 80 : 56;
    for (int particle = 0; particle < particle_count; ++particle) {
        const std::uint32_t seed = particle_seed(particle);
        const int layer = static_cast<int>(seed % 3U);
        int x = static_cast<int>((seed >> 4U) % static_cast<std::uint32_t>(logical_width));
        int y = static_cast<int>((seed >> 12U) % static_cast<std::uint32_t>(logical_height));
        int detail = layer;
        if (snow) {
            y = (y + static_cast<int>(elapsed * static_cast<std::uint64_t>(layer + 1))) %
                logical_height;
            const int sway = static_cast<int>((elapsed / 3U + (seed >> 20U)) % 12U);
            x = (x + (sway < 6 ? sway : 12 - sway) + logical_width) % logical_width;
        } else if (settings.moving_background) {
            const auto phase = static_cast<std::uint64_t>(particle);
            detail = (layer + static_cast<int>((elapsed / 5U + phase) % 3U)) % 3;
        }
        const auto& colors = snow ? snow_colors : star_colors;
        const SDL_Color color = colors[static_cast<std::size_t>(layer)];
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
        if (snow)
            draw_snowflake(renderer, x, y, pixel, detail);
        else
            draw_star(renderer, x, y, pixel, detail);
    }
    if (pulse > 0.0F) {
        (void)SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, palette[0].r, palette[0].g, palette[0].b,
                               static_cast<Uint8>(pulse * 72.0F));
        (void)SDL_RenderFillRect(renderer, nullptr);
        (void)SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
}

} // namespace drawing

using namespace drawing;

namespace {

constexpr int original_scene_columns = 20;
constexpr int modern_scene_columns = 25;
constexpr int scene_texture_columns = 26;
constexpr int scene_texture_width = scene_texture_columns * 8;
constexpr int scene_height = 144;
constexpr Placement scene_placement{0, 0, 8};

SDL_Texture* make_scene(SDL_Renderer* renderer) {
    SDL_Texture* scene = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                           SDL_TEXTUREACCESS_TARGET, scene_texture_width,
                                           scene_height);
    if (scene != nullptr) {
        (void)SDL_SetTextureScaleMode(scene, SDL_SCALEMODE_NEAREST);
        (void)SDL_SetTextureBlendMode(scene, SDL_BLENDMODE_NONE);
    }
    return scene;
}

void begin_scene(SDL_Renderer* renderer, SDL_Texture* scene, bool extended) {
    (void)SDL_SetRenderTarget(renderer, scene);
    const SDL_Color clear = extended ? palette[3] : palette[0];
    SDL_SetRenderDrawColor(renderer, clear.r, clear.g, clear.b, 255);
    (void)SDL_RenderClear(renderer);
}

void compose_scene(SDL_Renderer* renderer, SDL_Texture* host, SDL_Texture* scene,
                   const Placement& placement, int columns, int source_column = 0) {
    (void)SDL_SetRenderTarget(renderer, host);
    const SDL_FRect source{static_cast<float>(source_column * 8), 0,
                           static_cast<float>(columns * 8), 144};
    const SDL_FRect destination{placement.x, placement.y,
                                static_cast<float>(columns) * placement.tile,
                                18.0F * placement.tile};
    (void)SDL_RenderTexture(renderer, scene, &source, &destination);
}

bool gameplay_family(Screen screen) {
    return screen == Screen::gameplay || screen == Screen::demo || screen == Screen::game_over;
}

bool extended_canvas_fits(const GubsyFrame& frame, const settings::Settings& settings) {
    if (!settings.extended_canvas)
        return false;
    const float original_scale = std::max(
        1.0F, std::floor(std::min(static_cast<float>(frame.render_width) / 160.0F,
                                  static_cast<float>(frame.render_height) / 144.0F)));
    return static_cast<float>(modern_scene_columns * 8) * original_scale <=
           static_cast<float>(frame.render_width);
}

// Render one complete original or responsive extended scene before host scaling.
void render_single_scene(SDL_Renderer* renderer, const Video& video,
                         const content::Catalog& content, const GameState& state,
                         const settings::Settings& settings, DebugView debug, bool extended,
                         const MotionHistory& motion, float alpha) {
    const bool extended_hud =
        extended && gameplay_family(state.screen) && uses_modern_hud(state.single_player);
    Placement placement = scene_placement;
    if (extended && !extended_hud)
        placement.x = 3.0F * placement.tile;
    if (state.screen == Screen::buran || state.screen == Screen::rocket) {
        draw_launch(renderer, video, content, state, placement);
    } else if (state.screen == Screen::versus_round_result ||
               state.screen == Screen::versus_match_result) {
        draw_versus_result(renderer, video, content, state, placement);
    } else {
        Bank bank = Bank::gameplay;
        const char* map_id = map_for(state, bank);
        if (map_id != nullptr) {
            if (const content::Tilemap* map = content.find_tilemap(map_id)) {
                draw_map(renderer, video, bank, *map, placement);
                if (state.screen == Screen::title && settings.title_parallax)
                    draw_title_parallax(renderer, video, placement);
            }
        }

        if (state.screen == Screen::gameplay || state.screen == Screen::demo) {
            draw_session(renderer, video, content, bank, state.single_player, placement, true,
                         settings, debug.sprite_bounds, debug.board_cells, extended_hud,
                         piece_motion(motion, state, state.single_player, -1, alpha));
        }
        if (state.screen == Screen::game_over) {
            if (uses_modern_hud(state.single_player))
                draw_session(renderer, video, content, bank, state.single_player, placement, false,
                             settings, debug.sprite_bounds, debug.board_cells, extended_hud);
            draw_game_over(renderer, video, content, state, bank, placement, settings,
                           debug.sprite_bounds, extended_hud);
        }
        if ((state.screen == Screen::gameplay || state.screen == Screen::demo) &&
            state.single_player.paused) {
            if (state.single_player.options.quick_restart) {
                // Modern play exposes navigation directly instead of requiring
                // an undocumented reset chord or closing the application.
                const settings::ControllerLabels labels = resolve_controller_labels(
                    settings.controller_labels, video.detected_controller_labels);
                const std::string retry =
                    controller_prompt(labels, FaceButton::north, "RETRY");
                const std::string menu =
                    controller_prompt(labels, FaceButton::south, "MAIN MENU");
                draw_panel(renderer, placement, 1, 4, 18, 10);
                text(renderer, video, bank, placement, "PAUSED", 7, 5);
                text(renderer, video, bank, placement, "START RESUME", 3, 8);
                text(renderer, video, bank, placement, retry.c_str(), 3, 10);
                text(renderer, video, bank, placement, menu.c_str(), 3, 12);
            } else if (const content::Tilemap* pause = content.find_tilemap("pause-message")) {
                draw_map(renderer, video, bank, *pause, placement, 3, 3);
            }
        }
        draw_type_b_result(renderer, video, content, state, placement);
        draw_screen_ui(renderer, video, content, state, placement);
    }
    if (debug.grid)
        draw_grid(renderer, placement);
}

} // namespace

bool initialize_video(Video& video, SDL_Renderer* renderer, const content::Catalog& content) {
    // Upload every tile bank before creating the intermediate scenes.
    shutdown_video(video);
    const bool ready = build_atlas(renderer, content.gameplay_tiles, video.gameplay) &&
                       build_atlas(renderer, content.font_tiles, video.font, true) &&
                       build_atlas(renderer, content.title_tiles, video.title) &&
                       build_atlas(renderer, content.multiplayer_tiles, video.multiplayer) &&
                       build_title_layers(renderer, content, video);
    if (ready) {
        video.detected_controller_labels = detect_controller_labels();
        video.scenes[0] = make_scene(renderer);
        video.scenes[1] = make_scene(renderer);
    }
    if (ready && video.scenes[0] != nullptr && video.scenes[1] != nullptr)
        return true;
    shutdown_video(video);
    return false;
}

void shutdown_video(Video& video) {
    // Destroy every SDL texture owned by video.
    SDL_DestroyTexture(video.gameplay.opaque);
    SDL_DestroyTexture(video.gameplay.sprites);
    SDL_DestroyTexture(video.font.opaque);
    SDL_DestroyTexture(video.font.sprites);
    SDL_DestroyTexture(video.title.opaque);
    SDL_DestroyTexture(video.title.sprites);
    SDL_DestroyTexture(video.multiplayer.opaque);
    SDL_DestroyTexture(video.multiplayer.sprites);
    SDL_DestroyTexture(video.title_moon.texture);
    SDL_DestroyTexture(video.title_city.texture);
    SDL_DestroyTexture(video.title_skyline.texture);
    SDL_DestroyTexture(video.title_towers.texture);
    for (SDL_Texture*& scene : video.scenes) {
        SDL_DestroyTexture(scene);
        scene = nullptr;
    }
    video.gameplay = {};
    video.font = {};
    video.title = {};
    video.multiplayer = {};
    video.title_moon = {};
    video.title_city = {};
    video.title_skyline = {};
    video.title_towers = {};
}

void render_video(const Video& video, SDL_Renderer* renderer, const GubsyFrame& frame,
                  const content::Catalog& content, const GameState& state,
                  const settings::Settings& settings, const EffectState& effects, DebugView debug,
                  const MotionHistory& motion, float alpha) {
    // Clear the host target and draw outside-screen rendering.
    (void)SDL_SetRenderTarget(renderer, frame.render_target);
    draw_background(renderer, frame, settings, background_pulse(effects, settings));

    // Multiplayer renders two independent 160x144 scenes side by side.
    if (state.screen == Screen::versus_gameplay) {
        const auto placements = versus_placements(frame, shake_offset(effects, settings));
        for (int player = 0; player < 2; ++player) {
            begin_scene(renderer, video.scenes[static_cast<std::size_t>(player)], false);
            if (const content::Tilemap* map = content.find_tilemap("multiplayer-gameplay"))
                draw_map(renderer, video, Bank::multiplayer, *map, scene_placement);
            draw_session(renderer, video, content, Bank::multiplayer,
                         state.versus.players[static_cast<std::size_t>(player)].game,
                         scene_placement, true, settings, debug.sprite_bounds, debug.board_cells,
                         false,
                         piece_motion(motion, state,
                                      state.versus.players[static_cast<std::size_t>(player)].game,
                                      player, alpha));
            draw_versus_status(renderer, video, content, state.versus, scene_placement, player);
            if (debug.grid)
                draw_grid(renderer, scene_placement);
            compose_scene(renderer, frame.render_target,
                          video.scenes[static_cast<std::size_t>(player)],
                          placements[static_cast<std::size_t>(player)], original_scene_columns);
        }
        return;
    }

    // Single-screen content keeps its original scale and gains columns only
    // when the responsive canvas can do so without making pixels smaller.
    const bool extended = extended_canvas_fits(frame, settings);
    begin_scene(renderer, video.scenes[0], extended);
    render_single_scene(renderer, video, content, state, settings, debug, extended, motion, alpha);
    const int columns = extended ? modern_scene_columns : original_scene_columns;
    compose_scene(renderer, frame.render_target, video.scenes[0],
                  centered(frame, shake_offset(effects, settings), columns), columns,
                  extended ? 1 : 0);
}

} // namespace tetris::video
