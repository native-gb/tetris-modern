#include "presentation/renderer.hpp"
#include "presentation/drawing.hpp"
#include "presentation/endings.hpp"
#include "presentation/ui.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace tetris::presentation {
namespace drawing {

constexpr int atlas_columns = 16;
constexpr std::array<SDL_Color, 4> palette = {{
    {224, 239, 207, 255}, {155, 181, 139, 255},
    {75, 105, 73, 255}, {22, 36, 28, 255},
}};

struct SelectedTile {
    const Renderer::Atlas* atlas{};
    int index{};
};

SDL_Texture* build_texture(SDL_Renderer* renderer, const content::TileSet& tiles,
                           bool one_bit, bool transparent) {
    const int count = static_cast<int>(tiles.tiles.size());
    const int rows = std::max((count + atlas_columns - 1) / atlas_columns, 1);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                             SDL_TEXTUREACCESS_TARGET,
                                             atlas_columns * 8, rows * 8);
    if (texture == nullptr)
        return nullptr;
    (void)SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    (void)SDL_SetTextureBlendMode(texture, transparent ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_Texture* previous = SDL_GetRenderTarget(renderer);
    (void)SDL_SetRenderTarget(renderer, texture);
    SDL_SetRenderDrawColor(renderer, palette[0].r, palette[0].g, palette[0].b,
                           transparent ? 0 : 255);
    (void)SDL_RenderClear(renderer);
    for (int tile = 0; tile < count; ++tile) {
        const content::Tile& source = tiles.tiles[static_cast<std::size_t>(tile)];
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                std::uint8_t pixel = source.pixels[static_cast<std::size_t>(y * 8 + x)];
                if (one_bit && pixel != 0)
                    pixel = 3;
                if (transparent && pixel == 0)
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

bool build_atlas(SDL_Renderer* renderer, const content::TileSet& tiles,
                 Renderer::Atlas& atlas, bool one_bit = false) {
    atlas.opaque = build_texture(renderer, tiles, one_bit, false);
    atlas.sprites = build_texture(renderer, tiles, one_bit, true);
    atlas.count = static_cast<int>(tiles.tiles.size());
    return atlas.opaque != nullptr && atlas.sprites != nullptr;
}

SelectedTile select(const Renderer& video, Bank bank, std::uint8_t id) {
    const Renderer::Atlas* atlas = nullptr;
    int index = id;
    if (bank == Bank::multiplayer) {
        atlas = &video.multiplayer();
    } else if (id < 0x27) {
        atlas = &video.font();
    } else if (bank == Bank::title || id < 0x30) {
        atlas = &video.title();
        index -= 0x27;
    } else {
        atlas = &video.gameplay();
        index -= 0x30;
    }
    return index >= 0 && index < atlas->count ? SelectedTile{atlas, index} : SelectedTile{};
}

Placement centered(const GubsyFrame& frame, Offset offset) {
    const float scale = std::max(1.0F, std::floor(std::min(
        static_cast<float>(frame.render_width) / 160.0F,
        static_cast<float>(frame.render_height) / 144.0F)));
    return {
        .x = (static_cast<float>(frame.render_width) - 160.0F * scale) * 0.5F + offset.x * scale,
        .y = (static_cast<float>(frame.render_height) - 144.0F * scale) * 0.5F + offset.y * scale,
        .tile = 8.0F * scale,
    };
}

std::array<Placement, 2> versus_placements(const GubsyFrame& frame, Offset offset) {
    constexpr float screen_width = 160.0F;
    constexpr float screen_height = 144.0F;
    constexpr float gap_width = 16.0F;
    const float scale = std::max(1.0F, std::floor(std::min(
        static_cast<float>(frame.render_width) / (screen_width * 2.0F + gap_width),
        static_cast<float>(frame.render_height) / screen_height)));
    const float tile = 8.0F * scale;
    const float gap = gap_width * scale;
    const float width = screen_width * 2.0F * scale + gap;
    const float x = (static_cast<float>(frame.render_width) - width) * 0.5F + offset.x * scale;
    const float y = (static_cast<float>(frame.render_height) - screen_height * scale) * 0.5F +
                    offset.y * scale;
    return {{{x, y, tile}, {x + screen_width * scale + gap, y, tile}}};
}

const char* map_for(const GameFlow& flow, Bank& bank) {
    bank = Bank::gameplay;
    switch (flow.screen()) {
    case Screen::copyright_fixed:
    case Screen::copyright_skippable: bank = Bank::title; return "copyright";
    case Screen::title: bank = Bank::title; return "title";
    case Screen::game_type:
    case Screen::music: return "configuration";
    case Screen::type_a_level: return "type-a-difficulty";
    case Screen::type_b_level:
    case Screen::type_b_height:
    case Screen::name_entry:
        return flow.selected_type() == GameType::type_a ? "type-a-difficulty"
                                                        : "type-b-difficulty";
    case Screen::versus_height: bank = Bank::multiplayer; return "multiplayer-difficulty";
    case Screen::versus_gameplay: bank = Bank::multiplayer; return "multiplayer-gameplay";
    case Screen::gameplay:
    case Screen::demo:
    case Screen::game_over:
    case Screen::type_b_celebration:
    case Screen::dancers:
    case Screen::scoreboard:
        return flow.game().rules().type == GameType::type_b ? "type-b-gameplay"
                                                            : "type-a-gameplay";
    default: return nullptr;
    }
}

void draw_texture_tile(SDL_Renderer* renderer, const Renderer::Atlas& atlas, int index,
                       float x, float y, float size, bool sprite, bool flip) {
    if (index < 0 || index >= atlas.count)
        return;
    const SDL_FRect source{static_cast<float>(index % atlas_columns * 8),
                           static_cast<float>(index / atlas_columns * 8), 8, 8};
    const SDL_FRect destination{x, y, size, size};
    SDL_Texture* texture = sprite ? atlas.sprites : atlas.opaque;
    if (flip)
        (void)SDL_RenderTextureRotated(renderer, texture, &source, &destination,
                                       0, nullptr, SDL_FLIP_HORIZONTAL);
    else
        (void)SDL_RenderTexture(renderer, texture, &source, &destination);
}

void draw_tile(SDL_Renderer* renderer, const Renderer& video, Bank bank,
               std::uint8_t id, const Placement& placement, float column, float row,
               bool sprite, bool flip) {
    const SelectedTile tile = select(video, bank, id);
    if (tile.atlas == nullptr)
        return;
    draw_texture_tile(renderer, *tile.atlas, tile.index,
                      placement.x + column * placement.tile,
                      placement.y + row * placement.tile,
                      placement.tile, sprite, flip);
}

void draw_map(SDL_Renderer* renderer, const Renderer& video, Bank bank,
              const content::Tilemap& map, const Placement& placement,
              int origin_column, int origin_row) {
    for (int row = 0; row < map.height; ++row) {
        for (int column = 0; column < map.width; ++column) {
            const auto index = static_cast<std::size_t>(row * map.width + column);
            draw_tile(renderer, video, bank, map.tiles[index], placement,
                      static_cast<float>(origin_column + column),
                      static_cast<float>(origin_row + row));
        }
    }
}

std::uint8_t piece_id(const FallingPiece& piece) {
    return static_cast<std::uint8_t>(static_cast<unsigned int>(piece.kind) * 4U +
                                     static_cast<unsigned int>(piece.rotation));
}

std::uint8_t piece_id(PieceSpec piece) {
    return static_cast<std::uint8_t>(static_cast<unsigned int>(piece.kind) * 4U +
                                     static_cast<unsigned int>(piece.rotation));
}

int wrapped_add(std::uint8_t raw, int first, int second) {
    return (static_cast<int>(raw) + first + second) & 0xFF;
}

void draw_sprite(SDL_Renderer* renderer, const Renderer& video,
                 const content::Catalog& content, Bank bank, std::uint8_t id,
                 const Placement& placement, std::uint8_t raw_x, std::uint8_t raw_y,
                 bool flip, bool bounds) {
    if (id >= content.sprites.sprites.size())
        return;
    const content::Sprite& sprite = content.sprites.sprites[id];
    for (const content::SpriteObject object : sprite.objects) {
        int oam_x = wrapped_add(raw_x, sprite.origin_x, object.x);
        if (flip)
            oam_x = (static_cast<int>(raw_x) - sprite.origin_x - object.x - 8) & 0xFF;
        const int oam_y = wrapped_add(raw_y, sprite.origin_y, object.y);
        const float column = static_cast<float>(oam_x - 8) / 8.0F;
        const float row = static_cast<float>(oam_y - 16) / 8.0F;
        draw_tile(renderer, video, bank, object.tile, placement, column, row,
                  true, object.flip_x != flip);
        if (bounds) {
            SDL_SetRenderDrawColor(renderer, 255, 72, 72, 220);
            const SDL_FRect rect{placement.x + column * placement.tile,
                                 placement.y + row * placement.tile,
                                 placement.tile, placement.tile};
            (void)SDL_RenderRect(renderer, &rect);
        }
    }
}

void number(SDL_Renderer* renderer, const Renderer& video, Bank bank,
            const Placement& placement, std::uint32_t value, int right, int row, int width) {
    for (int digit = 0; digit < width; ++digit) {
        draw_tile(renderer, video, bank, static_cast<std::uint8_t>(value % 10U),
                  placement, static_cast<float>(right - digit), static_cast<float>(row));
        value /= 10U;
        if (value == 0)
            break;
    }
}

void left_number(SDL_Renderer* renderer, const Renderer& video, Bank bank,
                 const Placement& placement, std::uint32_t value, int column, int row) {
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
    if (character == '.') return 0x24;
    if (character == '-') return 0x25;
    if (character == '*') return 0x26;
    if (character == '~') return 0x27;
    return 0x2F;
}

void text(SDL_Renderer* renderer, const Renderer& video, Bank bank,
          const Placement& placement, const char* value, int column, int row) {
    for (int index = 0; value[index] != '\0'; ++index) {
        draw_tile(renderer, video, bank, character_tile(value[index]), placement,
                  static_cast<float>(column + index), static_cast<float>(row));
    }
}

std::uint8_t tile_for(Block block) {
    switch (block) {
    case Block::empty: return 0;
    case Block::l: return 0x84;
    case Block::j: return 0x81;
    case Block::o: return 0x83;
    case Block::s: return 0x82;
    case Block::z: return 0x86;
    case Block::t: return 0x85;
    case Block::i_horizontal_first: return 0x8A;
    case Block::i_horizontal_middle: return 0x8B;
    case Block::i_horizontal_last: return 0x8F;
    case Block::i_vertical_first: return 0x80;
    case Block::i_vertical_middle: return 0x88;
    case Block::i_vertical_last: return 0x89;
    default:
        return static_cast<std::uint8_t>(0x80U + static_cast<unsigned int>(block) -
                                         static_cast<unsigned int>(Block::garbage_0));
    }
}

void draw_session(SDL_Renderer* renderer, const Renderer& video,
                  const content::Catalog& content, Bank bank,
                  const SinglePlayer& game, const Placement& placement,
                  bool active, const Settings& settings, bool bounds) {
    for (int row = 0; row < board_height; ++row) {
        const bool clearing = std::ranges::find(game.clearing_rows(), row) != game.clearing_rows().end();
        for (int column = 0; column < board_width; ++column) {
            const Block block = game.presentation_board().at({column, row});
            std::uint8_t tile = tile_for(block);
            if (game.state() == PlayState::clearing && clearing) {
                const int final_step = settings.line_clear_speed == LineClearSpeed::original ? 6
                                     : settings.line_clear_speed == LineClearSpeed::fast ? 4 : 1;
                if (game.animation_step() == final_step)
                    tile = 0;
                else if (!settings.reduced_flash && game.animation_step() % 2 == 0)
                    tile = 0x8C;
            }
            if (tile != 0)
                draw_tile(renderer, video, bank, tile,
                          placement, static_cast<float>(column + 2), static_cast<float>(row));
        }
    }
    const content::ByteTable* active_placement = content.find_presentation("active-piece-placement");
    if (active && game.state() == PlayState::falling && active_placement != nullptr) {
        const FallingPiece& piece = game.piece();
        const auto raw_x = static_cast<std::uint8_t>(active_placement->bytes[2] + (piece.origin.x - 3) * 8);
        const auto raw_y = static_cast<std::uint8_t>(active_placement->bytes[1] + (piece.origin.y + 1) * 8);
        draw_sprite(renderer, video, content, bank, piece_id(piece), placement,
                    raw_x, raw_y, false, bounds);
    }
    if (game.preview_visible()) {
        const content::ByteTable* preview = content.find_presentation("preview-piece-placement");
        if (preview != nullptr)
            draw_sprite(renderer, video, content, bank, piece_id(game.preview()), placement,
                        preview->bytes[2], preview->bytes[1], false, bounds);
    }
    if (game.rules().type == GameType::type_a) {
        number(renderer, video, bank, placement, game.score(), 18, 3, 6);
        number(renderer, video, bank, placement, static_cast<std::uint32_t>(game.level()), 17, 7, 2);
        if (game.rules().heart_mode)
            draw_tile(renderer, video, bank, 0x27, placement, 18, 7);
    } else {
        number(renderer, video, bank, placement, static_cast<std::uint32_t>(game.level()), 16, 2, 1);
        number(renderer, video, bank, placement,
               static_cast<std::uint32_t>(game.rules().type_b_height), 16, 5, 1);
    }
    number(renderer, video, bank, placement, static_cast<std::uint32_t>(game.lines()), 17, 10, 4);

    if (settings.effects != Intensity::off && game.state() == PlayState::clearing &&
        !game.clearing_rows().empty()) {
        const int final_step = settings.line_clear_speed == LineClearSpeed::original ? 6
                             : settings.line_clear_speed == LineClearSpeed::fast ? 4 : 1;
        const float progress = std::clamp(static_cast<float>(game.animation_step()) /
                                          static_cast<float>(std::max(final_step, 1)), 0.0F, 1.0F);
        const float width = 10.0F * placement.tile * progress;
        SDL_SetRenderDrawColor(renderer, 224, 239, 207,
                               settings.effects == Intensity::full ? 235 : 170);
        (void)SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        for (const int row : game.clearing_rows()) {
            const SDL_FRect beam{placement.x + 7.0F * placement.tile - width * 0.5F,
                                 placement.y + (static_cast<float>(row) + 0.4F) * placement.tile,
                                 width, std::max(1.0F, placement.tile * 0.2F)};
            (void)SDL_RenderFillRect(renderer, &beam);
            const int embers = settings.effects == Intensity::full ? 18 : 8;
            for (int ember = 0; ember < embers; ++ember) {
                const std::uint32_t seed = static_cast<std::uint32_t>(row * 131 + ember * 47 +
                    game.animation_step() * 73);
                const float pixel = std::max(1.0F, placement.tile / 8.0F);
                const float side = (seed & 1U) != 0 ? 1.0F : -1.0F;
                const SDL_FRect spark{placement.x + 7.0F * placement.tile + side * width * 0.5F +
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

void draw_background(SDL_Renderer* renderer, const GubsyFrame& frame,
                     const Settings& settings, float pulse) {
    SDL_SetRenderDrawColor(renderer, 9, 12, 18, 255);
    (void)SDL_RenderClear(renderer);
    if (settings.layout != Layout::widescreen_frame ||
        settings.background != Background::stars)
        return;
    for (int star = 0; star < 180; ++star) {
        const std::uint32_t seed = static_cast<std::uint32_t>(star) * 747796405U + 2891336453U;
        const float x = static_cast<float>(seed % static_cast<std::uint32_t>(std::max(frame.render_width, 1)));
        const float y = static_cast<float>((seed >> 12U) % static_cast<std::uint32_t>(std::max(frame.render_height, 1)));
        const SDL_Color color = palette[static_cast<std::size_t>((seed >> 24U) % 3U)];
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
        const SDL_FRect point{x, y, (seed & 1U) != 0 ? 2.0F : 1.0F, (seed & 1U) != 0 ? 2.0F : 1.0F};
        (void)SDL_RenderFillRect(renderer, &point);
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

bool Renderer::initialize(SDL_Renderer* renderer, const content::Catalog& content) {
    shutdown();
    return build_atlas(renderer, content.gameplay_tiles, gameplay_) &&
           build_atlas(renderer, content.font_tiles, font_, true) &&
           build_atlas(renderer, content.title_tiles, title_) &&
           build_atlas(renderer, content.multiplayer_tiles, multiplayer_);
}

void Renderer::shutdown() {
    SDL_DestroyTexture(gameplay_.opaque); SDL_DestroyTexture(gameplay_.sprites);
    SDL_DestroyTexture(font_.opaque); SDL_DestroyTexture(font_.sprites);
    SDL_DestroyTexture(title_.opaque); SDL_DestroyTexture(title_.sprites);
    SDL_DestroyTexture(multiplayer_.opaque); SDL_DestroyTexture(multiplayer_.sprites);
    gameplay_ = {}; font_ = {}; title_ = {}; multiplayer_ = {};
}

void Renderer::draw(SDL_Renderer* renderer, const GubsyFrame& frame,
                    const content::Catalog& content, const GameFlow& flow,
                    const Settings& settings, const EffectState& effects,
                    DebugView debug) const {
    (void)SDL_SetRenderTarget(renderer, frame.render_target);
    draw_background(renderer, frame, settings, background_pulse(effects, settings));
    const Placement placement = centered(frame, shake_offset(effects, settings));
    if (flow.screen() == Screen::buran || flow.screen() == Screen::rocket) {
        draw_launch(renderer, *this, content, flow, placement);
        if (debug.grid)
            draw_grid(renderer, placement);
        return;
    }
    if (flow.screen() == Screen::versus_round_result ||
        flow.screen() == Screen::versus_match_result) {
        draw_versus_result(renderer, *this, content, flow, placement);
        if (debug.grid)
            draw_grid(renderer, placement);
        return;
    }
    if (flow.screen() == Screen::versus_gameplay) {
        const auto placements = versus_placements(frame, shake_offset(effects, settings));
        const Placement first = placements[0];
        const Placement second = placements[1];
        if (const content::Tilemap* map = content.find_tilemap("multiplayer-gameplay")) {
            draw_map(renderer, *this, Bank::multiplayer, *map, first);
            draw_map(renderer, *this, Bank::multiplayer, *map, second);
        }
        draw_session(renderer, *this, content, Bank::multiplayer, flow.versus().player(0),
                     first, true, settings, debug.sprite_bounds);
        draw_session(renderer, *this, content, Bank::multiplayer, flow.versus().player(1),
                     second, true, settings, debug.sprite_bounds);
        draw_versus_status(renderer, *this, content, flow.versus(), first, second);
        if (debug.grid) { draw_grid(renderer, first); draw_grid(renderer, second); }
        return;
    }

    Bank bank = Bank::gameplay;
    const char* map_id = map_for(flow, bank);
    if (map_id != nullptr) {
        if (const content::Tilemap* map = content.find_tilemap(map_id))
            draw_map(renderer, *this, bank, *map, placement);
    }

    if (flow.screen() == Screen::gameplay || flow.screen() == Screen::demo) {
        draw_session(renderer, *this, content, bank, flow.game(), placement,
                     true, settings, debug.sprite_bounds);
    }
    if (flow.screen() == Screen::game_over)
        draw_game_over(renderer, *this, content, flow, bank, placement,
                       settings, debug.sprite_bounds);
    if ((flow.screen() == Screen::gameplay || flow.screen() == Screen::demo) && flow.game().paused()) {
        if (const content::Tilemap* pause = content.find_tilemap("pause-message"))
            draw_map(renderer, *this, bank, *pause, placement, 3, 3);
    }
    draw_type_b_result(renderer, *this, content, flow, placement);
    draw_flow_ui(renderer, *this, content, flow, placement);
    if (debug.grid)
        draw_grid(renderer, placement);
}

} // namespace tetris::presentation
