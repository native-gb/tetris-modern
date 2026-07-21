#include "video/ui.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace tetris::video {
namespace {

using drawing::Bank;
using drawing::Placement;

std::array<std::uint8_t, 2> coordinate(const content::Catalog& content, std::string_view id,
                                       int index) {
    const content::ByteTable* table = content.find_layout(id);
    const std::size_t offset = static_cast<std::size_t>(index) * 2U;
    if (table == nullptr || offset + 1 >= table->bytes.size())
        return {};
    return {table->bytes[offset + 1], table->bytes[offset]};
}

std::uint8_t sprite_code(const content::Catalog& content, std::string_view id, int index) {
    const content::ByteTable* table = content.find_layout(id);
    const std::size_t offset = static_cast<std::size_t>(index) * 6U + 3U;
    return table != nullptr && offset < table->bytes.size() ? table->bytes[offset] : 0;
}

void draw_number_cursor(SDL_Renderer* renderer, const Video& video,
                        const content::Catalog& content, const Placement& placement,
                        std::uint8_t code, std::uint8_t raw_x, std::uint8_t raw_y, int number) {
    if (code >= content.sprites.sprites.size())
        return;
    const content::Sprite& cursor = content.sprites.sprites[code];
    if (cursor.objects.empty())
        return;

    // The original menu copies the selected number into the cursor's OAM tile.
    // The metasprite table only supplies its position, so drawing its static tile
    // zero would incorrectly place a literal 0 over every selected value.
    const drawing::SpritePosition position =
        drawing::sprite_object_position(cursor, cursor.objects.front(), raw_x, raw_y);
    drawing::draw_tile(renderer, video, Bank::gameplay,
                       static_cast<std::uint8_t>(std::clamp(number, 0, 9)), placement,
                       static_cast<float>(position.x) / 8.0F,
                       static_cast<float>(position.y) / 8.0F, true);
}

void draw_oam(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
              const Placement& placement, std::string_view id) {
    const content::ByteTable* objects = content.find_layout(id);
    if (objects == nullptr)
        return;
    for (std::size_t offset = 0; offset + 3 < objects->bytes.size(); offset += 4) {
        const float column =
            static_cast<float>(static_cast<int>(objects->bytes[offset + 1]) - 8) / 8.0F;
        const float row = static_cast<float>(static_cast<int>(objects->bytes[offset]) - 16) / 8.0F;
        drawing::draw_tile(renderer, video, Bank::multiplayer, objects->bytes[offset + 2],
                           placement, column, row, false,
                           (objects->bytes[offset + 3] & 0x20U) != 0);
    }
}

void draw_high_scores(SDL_Renderer* renderer, const Video& video, const GameState& state,
                      const Placement& placement) {
    // Draw the table selected by the current mode, level, and height.
    const GameType type =
        state.selected_type == GameType::type_a ? GameType::type_a : GameType::type_b;
    const auto& scores = state.high_scores.table(
        type, selected_level(state), type == GameType::type_b ? state.type_b_height : 0);
    for (std::size_t rank = 0; rank < scores.size(); ++rank) {
        const ScoreEntry& entry = scores[rank];
        const int row = 13 + static_cast<int>(rank);
        drawing::text(renderer, video, Bank::gameplay, placement, entry.name.substr(0, 6).c_str(),
                      4, row);
        if (entry.score != 0)
            drawing::number(renderer, video, Bank::gameplay, placement, entry.score, 17, row, 6);
    }

    // Name entry blanks unused characters and blinks the active slot.
    if (state.screen != Screen::name_entry || name_entry_rank(state) < 0)
        return;
    const int row = 13 + name_entry_rank(state);
    for (int character = static_cast<int>(state.pending_name.size()); character < 6; ++character)
        drawing::draw_tile(renderer, video, Bank::gameplay, 0x60, placement,
                           static_cast<float>(4 + character), static_cast<float>(row));
    if (!state.name_character_visible)
        drawing::draw_tile(renderer, video, Bank::gameplay, 0x2F, placement,
                           static_cast<float>(4 + state.name_cursor), static_cast<float>(row));
}

void draw_configuration(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                        const GameState& state, const Placement& placement) {
    // Place the music cursor from its extracted coordinate table.
    const int music = std::clamp(static_cast<int>(state.selected_music), 0, 3);
    const auto music_position = coordinate(content, "music-cursor-coordinates", music);
    const std::uint8_t base = sprite_code(content, "configuration-sprites", 0);
    drawing::draw_sprite(renderer, video, content, Bank::gameplay,
                         static_cast<std::uint8_t>(base + music), placement, music_position[0],
                         music_position[1]);

    // Place the selected game-type marker.
    if (state.selected_type == GameType::versus) {
        drawing::text(renderer, video, Bank::gameplay, placement, "2-player", 6, 7);
        drawing::draw_tile(renderer, video, Bank::gameplay, 0x27, placement, 4, 7);
        return;
    }
    const bool type_b = state.selected_type == GameType::type_b;
    const auto position = coordinate(content, "music-cursor-coordinates", type_b ? 1 : 0);
    drawing::draw_sprite(renderer, video, content, Bank::gameplay,
                         static_cast<std::uint8_t>(base + (type_b ? 1 : 0)), placement, position[0],
                         0x38);
}

void draw_level_selection(SDL_Renderer* renderer, const Video& video,
                          const content::Catalog& content, const GameState& state,
                          const Placement& placement) {
    // Type A has one cursor; Type B has independent level and height cursors.
    const int level = std::clamp(selected_level(state), 0, 9);
    if (state.selected_type == GameType::type_a) {
        const auto position = coordinate(content, "type-a-level-coordinates", level);
        draw_number_cursor(renderer, video, content, placement,
                           sprite_code(content, "type-a-level-sprite", 0), position[0],
                           position[1], level);
    } else {
        const auto level_position = coordinate(content, "type-b-level-coordinates", level);
        draw_number_cursor(renderer, video, content, placement,
                           sprite_code(content, "type-b-selection-sprites", 0), level_position[0],
                           level_position[1], level);
        const auto height_position =
            coordinate(content, "type-b-height-coordinates", std::clamp(state.type_b_height, 0, 5));
        draw_number_cursor(renderer, video, content, placement,
                           sprite_code(content, "type-b-selection-sprites", 1), height_position[0],
                           height_position[1], state.type_b_height);
    }

    // Both selection screens share the persistent score table.
    draw_high_scores(renderer, video, state, placement);
}

void draw_versus_height(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                        const GameState& state, const Placement& placement) {
    for (int player = 0; player < 2; ++player) {
        const auto position = coordinate(
            content,
            player == 0 ? "player-one-height-coordinates" : "player-two-height-coordinates",
            std::clamp(state.versus_heights[static_cast<std::size_t>(player)], 0, 5));
        drawing::draw_sprite(renderer, video, content, Bank::multiplayer,
                             sprite_code(content, "multiplayer-height-sprites", player), placement,
                             position[0], position[1]);
    }
    draw_oam(renderer, video, content, placement, "height-menu-faces");
}

} // namespace

void draw_screen_ui(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                    const GameState& state, const Placement& placement) {
    if (state.screen == Screen::game_type || state.screen == Screen::music)
        draw_configuration(renderer, video, content, state, placement);
    else if (state.screen == Screen::type_a_level || state.screen == Screen::type_b_level ||
             state.screen == Screen::type_b_height || state.screen == Screen::name_entry)
        draw_level_selection(renderer, video, content, state, placement);
    else if (state.screen == Screen::versus_height)
        draw_versus_height(renderer, video, content, state, placement);
}

void draw_versus_status(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                        const VersusMatch& match, const Placement& placement, int player) {
    // Mirror the opponent's stack height beside the local board.
    const int opponent = 1 - player;
    for (int row = 0; row < match.stack_height(opponent); ++row)
        drawing::draw_tile(renderer, video, Bank::multiplayer, 0xB6, placement, 1,
                           static_cast<float>(17 - row));
    draw_oam(renderer, video, content, placement,
             player == 0 ? "luigi-gameplay-face" : "mario-gameplay-face");

    // Overlay multiplayer pause text when the shared match is paused.
    if (!match.paused)
        return;
    const content::ByteTable* pause = content.find_layout("multiplayer-pause-text");
    if (pause == nullptr)
        return;
    for (std::size_t index = 0; index < pause->bytes.size(); ++index) {
        drawing::draw_tile(renderer, video, Bank::multiplayer, pause->bytes[index], placement,
                           static_cast<float>(14 + index), 7);
    }
}

} // namespace tetris::video
