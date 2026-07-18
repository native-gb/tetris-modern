#include "presentation/ui.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace tetris::presentation {
namespace {

using drawing::Bank;
using drawing::Placement;

std::array<std::uint8_t, 2> coordinate(const content::Catalog& content,
                                       std::string_view id, int index) {
    const content::ByteTable* table = content.find_presentation(id);
    const std::size_t offset = static_cast<std::size_t>(index) * 2U;
    if (table == nullptr || offset + 1 >= table->bytes.size())
        return {};
    return {table->bytes[offset + 1], table->bytes[offset]};
}

std::uint8_t sprite_code(const content::Catalog& content,
                         std::string_view id, int index) {
    const content::ByteTable* table = content.find_presentation(id);
    const std::size_t offset = static_cast<std::size_t>(index) * 6U + 3U;
    return table != nullptr && offset < table->bytes.size() ? table->bytes[offset] : 0;
}

void draw_oam(SDL_Renderer* renderer, const Renderer& video,
              const content::Catalog& content, const Placement& placement,
              std::string_view id) {
    const content::ByteTable* objects = content.find_presentation(id);
    if (objects == nullptr)
        return;
    for (std::size_t offset = 0; offset + 3 < objects->bytes.size(); offset += 4) {
        const float column = static_cast<float>(static_cast<int>(objects->bytes[offset + 1]) - 8) / 8.0F;
        const float row = static_cast<float>(static_cast<int>(objects->bytes[offset]) - 16) / 8.0F;
        drawing::draw_tile(renderer, video, Bank::multiplayer, objects->bytes[offset + 2],
                           placement, column, row, false,
                           (objects->bytes[offset + 3] & 0x20U) != 0);
    }
}

void draw_high_scores(SDL_Renderer* renderer, const Renderer& video,
                      const GameFlow& flow, const Placement& placement) {
    const GameType type = flow.selected_type() == GameType::type_a
                              ? GameType::type_a : GameType::type_b;
    const auto& scores = flow.high_scores().table(type, flow.selected_level(),
                                                  type == GameType::type_b ? flow.selected_height() : 0);
    for (std::size_t rank = 0; rank < scores.size(); ++rank) {
        const ScoreEntry& entry = scores[rank];
        const int row = 13 + static_cast<int>(rank);
        drawing::text(renderer, video, Bank::gameplay, placement,
                      entry.name.substr(0, 6).c_str(), 4, row);
        if (entry.score != 0)
            drawing::number(renderer, video, Bank::gameplay, placement,
                            entry.score, 17, row, 6);
    }
    if (flow.screen() != Screen::name_entry || flow.name_entry_rank() < 0)
        return;
    const int row = 13 + flow.name_entry_rank();
    for (int character = static_cast<int>(flow.pending_name().size()); character < 6; ++character)
        drawing::draw_tile(renderer, video, Bank::gameplay, 0x60, placement,
                           static_cast<float>(4 + character), static_cast<float>(row));
    if (!flow.name_character_visible())
        drawing::draw_tile(renderer, video, Bank::gameplay, 0x2F, placement,
                           static_cast<float>(4 + flow.name_cursor()), static_cast<float>(row));
}

void draw_configuration(SDL_Renderer* renderer, const Renderer& video,
                        const content::Catalog& content, const GameFlow& flow,
                        const Placement& placement) {
    const int music = std::clamp(static_cast<int>(flow.selected_music()), 0, 3);
    const auto music_position = coordinate(content, "music-cursor-coordinates", music);
    const std::uint8_t base = sprite_code(content, "configuration-sprites", 0);
    drawing::draw_sprite(renderer, video, content, Bank::gameplay,
                         static_cast<std::uint8_t>(base + music), placement,
                         music_position[0], music_position[1]);
    if (flow.selected_type() == GameType::versus) {
        drawing::text(renderer, video, Bank::gameplay, placement, "2-player", 6, 7);
        drawing::draw_tile(renderer, video, Bank::gameplay, 0x27, placement, 4, 7);
        return;
    }
    const bool type_b = flow.selected_type() == GameType::type_b;
    const auto position = coordinate(content, "music-cursor-coordinates", type_b ? 1 : 0);
    drawing::draw_sprite(renderer, video, content, Bank::gameplay,
                         static_cast<std::uint8_t>(base + (type_b ? 1 : 0)), placement,
                         position[0], 0x38);
}

void draw_level_selection(SDL_Renderer* renderer, const Renderer& video,
                          const content::Catalog& content, const GameFlow& flow,
                          const Placement& placement) {
    const int level = std::clamp(flow.selected_level(), 0, 9);
    if (flow.selected_type() == GameType::type_a) {
        const auto position = coordinate(content, "type-a-level-coordinates", level);
        drawing::draw_sprite(renderer, video, content, Bank::gameplay,
                             sprite_code(content, "type-a-level-sprite", 0), placement,
                             position[0], position[1]);
    } else {
        const auto level_position = coordinate(content, "type-b-level-coordinates", level);
        drawing::draw_sprite(renderer, video, content, Bank::gameplay,
                             sprite_code(content, "type-b-selection-sprites", 0), placement,
                             level_position[0], level_position[1]);
        const auto height_position = coordinate(content, "type-b-height-coordinates",
                                                std::clamp(flow.selected_height(), 0, 5));
        drawing::draw_sprite(renderer, video, content, Bank::gameplay,
                             sprite_code(content, "type-b-selection-sprites", 1), placement,
                             height_position[0], height_position[1]);
    }
    draw_high_scores(renderer, video, flow, placement);
}

void draw_versus_height(SDL_Renderer* renderer, const Renderer& video,
                        const content::Catalog& content, const GameFlow& flow,
                        const Placement& placement) {
    for (int player = 0; player < 2; ++player) {
        const auto position = coordinate(content,
            player == 0 ? "player-one-height-coordinates" : "player-two-height-coordinates",
            std::clamp(flow.versus_height(player), 0, 5));
        drawing::draw_sprite(renderer, video, content, Bank::multiplayer,
                             sprite_code(content, "multiplayer-height-sprites", player),
                             placement, position[0], position[1]);
    }
    draw_oam(renderer, video, content, placement, "height-menu-faces");
}

} // namespace

void draw_flow_ui(SDL_Renderer* renderer, const Renderer& video,
                  const content::Catalog& content, const GameFlow& flow,
                  const Placement& placement) {
    if (flow.screen() == Screen::game_type || flow.screen() == Screen::music)
        draw_configuration(renderer, video, content, flow, placement);
    else if (flow.screen() == Screen::type_a_level || flow.screen() == Screen::type_b_level ||
             flow.screen() == Screen::type_b_height || flow.screen() == Screen::name_entry)
        draw_level_selection(renderer, video, content, flow, placement);
    else if (flow.screen() == Screen::versus_height)
        draw_versus_height(renderer, video, content, flow, placement);
}

void draw_versus_status(SDL_Renderer* renderer, const Renderer& video,
                        const content::Catalog& content, const VersusMatch& match,
                        const Placement& placement, int player) {
    const int opponent = 1 - player;
    for (int row = 0; row < match.stack_height(opponent); ++row)
        drawing::draw_tile(renderer, video, Bank::multiplayer, 0xB6, placement, 1,
                           static_cast<float>(17 - row));
    draw_oam(renderer, video, content, placement,
             player == 0 ? "luigi-gameplay-face" : "mario-gameplay-face");
    if (!match.paused())
        return;
    const content::ByteTable* pause = content.find_presentation("multiplayer-pause-text");
    if (pause == nullptr)
        return;
    for (std::size_t index = 0; index < pause->bytes.size(); ++index) {
        drawing::draw_tile(renderer, video, Bank::multiplayer, pause->bytes[index], placement,
                           static_cast<float>(14 + index), 7);
    }
}

} // namespace tetris::presentation
