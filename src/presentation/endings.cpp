#include "presentation/endings.hpp"

#include "game/rules.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace tetris::presentation {
namespace {

using drawing::Bank;
using drawing::Placement;

const content::ByteTable* table(const content::Catalog& content, std::string_view id) {
    return content.find_presentation(id);
}

void placement_sprite(SDL_Renderer* renderer, const Renderer& video,
                      const content::Catalog& content, const Placement& placement,
                      const content::ByteTable& sprites, int index, int delta = 0,
                      int raw_y_override = -1, Bank bank = Bank::multiplayer) {
    const std::size_t offset = static_cast<std::size_t>(index) * 6U;
    if (offset + 5 >= sprites.bytes.size())
        return;
    const std::uint8_t raw_y = static_cast<std::uint8_t>(raw_y_override >= 0
        ? raw_y_override : sprites.bytes[offset + 1]);
    const std::uint8_t code = static_cast<std::uint8_t>(
        static_cast<int>(sprites.bytes[offset + 3]) + delta);
    drawing::draw_sprite(renderer, video, content, bank, code, placement,
                         sprites.bytes[offset + 2], raw_y,
                         (sprites.bytes[offset + 5] & 0x20U) != 0);
}

void scoreboard_values(SDL_Renderer* renderer, const Renderer& video,
                       const GameFlow& flow, const Placement& placement) {
    const Scoreboard& scoreboard = flow.scoreboard();
    const std::array counts = {scoreboard.lines.singles, scoreboard.lines.doubles,
                               scoreboard.lines.triples, scoreboard.lines.tetrises};
    for (int category = 0; category < 4; ++category) {
        const int row = 1 + category * 3;
        drawing::number(renderer, video, Bank::gameplay, placement,
                        counts[static_cast<std::size_t>(category)], 3, row, 2);
        drawing::left_number(renderer, video, Bank::gameplay, placement,
                             line_clear_score(category + 1, flow.game().rules().starting_level),
                             7, row);
    }
    drawing::number(renderer, video, Bank::gameplay, placement,
                    scoreboard.soft_drop, 10, 13, 6);
    drawing::number(renderer, video, Bank::gameplay, placement,
                    scoreboard.score, 10, 17, 6);
}

void dancers(SDL_Renderer* renderer, const Renderer& video,
             const content::Catalog& content, const GameFlow& flow,
             const Placement& placement) {
    const content::ByteTable* sprites = table(content, "dancer-sprites");
    if (sprites == nullptr)
        return;
    const int height = flow.game().rules().type_b_height;
    const int available = static_cast<int>(sprites->bytes.size() / 6U);
    const int visible = std::min(height == 5 ? 10 : std::clamp(height + 1, 1, 10), available);
    for (int index = 0; index < visible; ++index) {
        const std::size_t offset = static_cast<std::size_t>(index) * 6U;
        const int delta = flow.dancer_frame(index);
        const int y = static_cast<int>(sprites->bytes[offset + 1]) +
                      flow.dancer_vertical_offset(index);
        placement_sprite(renderer, video, content, placement, *sprites, index, delta, y,
                         Bank::gameplay);
    }
}

void congratulations(SDL_Renderer* renderer, const Renderer& video,
                     const content::Catalog& content, const GameFlow& flow,
                     const Placement& placement) {
    const content::ByteTable* characters = table(content, "congratulations-text");
    if (characters == nullptr)
        return;
    const int count = std::clamp(flow.congratulations_characters(), 0,
                                 static_cast<int>(characters->bytes.size()));
    for (int index = 0; index < count; ++index) {
        drawing::draw_tile(renderer, video, Bank::multiplayer,
                           characters->bytes[static_cast<std::size_t>(index)], placement,
                           static_cast<float>(2 + index), 4);
        drawing::draw_tile(renderer, video, Bank::multiplayer, 0xB6, placement,
                           static_cast<float>(2 + index), 5);
    }
}

void result_stamp(SDL_Renderer* renderer, const Renderer& video,
                  const Placement& placement, int column, int row,
                  std::uint8_t tile) {
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x)
            drawing::draw_tile(renderer, video, Bank::multiplayer,
                static_cast<std::uint8_t>(tile + y * 2 + x), placement,
                static_cast<float>(column + x), static_cast<float>(row + y));
    }
}

void result_stamps(SDL_Renderer* renderer, const Renderer& video,
                   const Placement& placement, int count, int row, std::uint8_t tile) {
    for (int index = 0; index < std::min(count, 4); ++index)
        result_stamp(renderer, video, placement, 7 + index * 3, row, tile);
}

void result_text(SDL_Renderer* renderer, const Renderer& video,
                 const content::Catalog& content, const Placement& placement,
                 std::string_view id) {
    const content::ByteTable* value = table(content, id);
    if (value == nullptr)
        return;
    const int column = (21 - static_cast<int>(value->bytes.size())) / 2;
    for (std::size_t index = 0; index < value->bytes.size(); ++index) {
        drawing::draw_tile(renderer, video, Bank::multiplayer, value->bytes[index], placement,
                           static_cast<float>(column + static_cast<int>(index)), 5);
        drawing::draw_tile(renderer, video, Bank::multiplayer, 0xB6, placement,
                           static_cast<float>(column + static_cast<int>(index)), 6);
    }
}

void result_actors(SDL_Renderer* renderer, const Renderer& video,
                   const content::Catalog& content, const GameFlow& flow,
                   const Placement& placement) {
    const bool local_victory = flow.versus().winner() != RoundWinner::player_two;
    const bool final = flow.versus().state() == MatchState::match_over;
    const int animation = flow.versus_result_step() % 2;
    const int cry = (flow.versus_result_elapsed() / 15) % 2;
    const int step = flow.versus_result_step();
    const int within = flow.versus_result_elapsed() % 25;
    const content::ByteTable* actors = table(content,
        local_victory ? "mario-victory-sprites" : "mario-defeat-sprites");
    if (actors == nullptr)
        return;
    if (local_victory) {
        const int winner_y = animation == 0 ? 0x60 : 0x50;
        placement_sprite(renderer, video, content, placement, *actors, 0, animation, winner_y);
        placement_sprite(renderer, video, content, placement, *actors, 1, animation, winner_y);
        if (!final || step <= 21)
            placement_sprite(renderer, video, content, placement, *actors, 2, cry);
        else if (step == 22 && within < 3)
            placement_sprite(renderer, video, content, placement, *actors, 2, cry,
                             0x68 + within * 2);
        else if (step == 22)
            drawing::draw_sprite(renderer, video, content, Bank::multiplayer, 0x57,
                                 placement, 0x38, 0x69);
        return;
    }
    if (!final || step <= 22) {
        const int loser_y = final && step == 22 ? std::min(0x60 + within * 4, 0x72) : 0x60;
        placement_sprite(renderer, video, content, placement, *actors, 0, cry, loser_y);
    } else if (step == 23) {
        drawing::draw_sprite(renderer, video, content, Bank::multiplayer, 0x56,
                             placement, 0x60, 0x61);
    }
    placement_sprite(renderer, video, content, placement, *actors, 1, animation,
                     animation == 0 ? 0x68 : 0x60);
}

} // namespace

void draw_game_over(SDL_Renderer* renderer, const Renderer& video,
                    const content::Catalog& content, const GameFlow& flow,
                    Bank bank, const Placement& placement,
                    const Settings& settings, bool sprite_bounds) {
    if (flow.ending_stage() == EndingStage::game_over_curtain) {
        drawing::draw_session(renderer, video, content, bank, flow.game(), placement,
                              false, settings, sprite_bounds, false);
        for (int row = board_height - flow.game_over_curtain_rows(); row < board_height; ++row) {
            for (int column = 0; column < board_width; ++column)
                drawing::draw_tile(renderer, video, bank, 0x87, placement,
                                   static_cast<float>(2 + column), static_cast<float>(row));
        }
        return;
    }
    if (const content::Tilemap* panel = content.find_tilemap("game-over-panel"))
        drawing::draw_map(renderer, video, bank, *panel, placement, 3, 2);
    if (const content::Tilemap* retry = content.find_tilemap("try-again-panel"))
        drawing::draw_map(renderer, video, bank, *retry, placement, 3, 12);
    for (int row = 0; row < board_height - flow.game_over_panel_rows(); ++row) {
        for (int column = 0; column < board_width; ++column)
            drawing::draw_tile(renderer, video, bank, 0x87, placement,
                               static_cast<float>(2 + column), static_cast<float>(row));
    }
}

void draw_type_b_result(SDL_Renderer* renderer, const Renderer& video,
                        const content::Catalog& content, const GameFlow& flow,
                        const Placement& placement) {
    if (flow.screen() == Screen::type_b_celebration || flow.screen() == Screen::scoreboard) {
        if (const content::Tilemap* board = content.find_tilemap("type-b-scoreboard"))
            drawing::draw_map(renderer, video, Bank::gameplay, *board, placement, 2, 0);
        if (flow.screen() == Screen::scoreboard)
            scoreboard_values(renderer, video, flow, placement);
    } else if (flow.screen() == Screen::dancers) {
        if (const content::Tilemap* map = content.find_tilemap("dancers"))
            drawing::draw_map(renderer, video, Bank::gameplay, *map, placement, 2, 0);
        dancers(renderer, video, content, flow, placement);
    }
}

void draw_launch(SDL_Renderer* renderer, const Renderer& video,
                 const content::Catalog& content, const GameFlow& flow,
                 const Placement& placement) {
    const auto submap = [&](const char* id, int column, int row) {
        if (const content::Tilemap* map = content.find_tilemap(id))
            drawing::draw_map(renderer, video, Bank::multiplayer, *map, placement, column, row);
    };
    submap("buran-backdrop", 0, 14);
    submap("left-tower-left", 6, 7); submap("left-tower-right", 7, 7);
    submap("right-tower-left", 12, 7); submap("right-tower-right", 13, 7);
    const bool umbilicals = flow.screen() == Screen::buran &&
        (flow.ending_stage() == EndingStage::buran_initialize ||
         flow.ending_stage() == EndingStage::buran_prelaunch ||
         flow.ending_stage() == EndingStage::buran_ignition ||
         flow.ending_stage() == EndingStage::buran_final_ignition);
    if (umbilicals) {
        drawing::draw_tile(renderer, video, Bank::multiplayer, 0x72, placement, 8, 8);
        drawing::draw_tile(renderer, video, Bank::multiplayer, 0xC4, placement, 9, 8);
        drawing::draw_tile(renderer, video, Bank::multiplayer, 0xB7, placement, 8, 9);
        drawing::draw_tile(renderer, video, Bank::multiplayer, 0xB8, placement, 9, 9);
    }
    const content::ByteTable* launch = table(content,
        flow.screen() == Screen::buran ? "buran-launch-sprites" : "rocket-launch-sprites");
    if (launch == nullptr || launch->bytes.size() < 18)
        return;
    std::uint8_t rocket = launch->bytes[3];
    if (flow.screen() == Screen::rocket) {
        if (flow.rocket() == Rocket::medium) ++rocket;
        if (flow.rocket() == Rocket::small) rocket = static_cast<std::uint8_t>(rocket + 2U);
    }
    drawing::draw_sprite(renderer, video, content, Bank::multiplayer, rocket, placement,
                         launch->bytes[2], static_cast<std::uint8_t>(flow.launch_y()));
    if (flow.launch_smoke_visible()) {
        const int delta = flow.ending_stage() == EndingStage::buran_final_ignition ? 1 : 0;
        placement_sprite(renderer, video, content, placement, *launch, 1, delta);
        placement_sprite(renderer, video, content, placement, *launch, 2, delta);
    }
    if (flow.ending_stage() == EndingStage::rocket_rising ||
        flow.ending_stage() == EndingStage::buran_rising) {
        const std::uint8_t base = flow.screen() == Screen::buran ? 0x40 : 0x5C;
        drawing::draw_sprite(renderer, video, content, Bank::multiplayer,
                             static_cast<std::uint8_t>(base + flow.exhaust_animation_frame()),
                             placement, static_cast<std::uint8_t>(flow.exhaust_x()),
                             static_cast<std::uint8_t>(flow.exhaust_y()));
    }
    if (flow.ending_stage() == EndingStage::congratulations ||
        flow.ending_stage() == EndingStage::congratulations_wait)
        congratulations(renderer, video, content, flow, placement);
}

void draw_versus_result(SDL_Renderer* renderer, const Renderer& video,
                        const content::Catalog& content, const GameFlow& flow,
                        const Placement& placement) {
    if (const content::Tilemap* top = content.find_tilemap("multiplayer-victory-top"))
        drawing::draw_map(renderer, video, Bank::multiplayer, *top, placement, 0, 0);
    if (const content::Tilemap* bottom = content.find_tilemap("multiplayer-victory-bottom"))
        drawing::draw_map(renderer, video, Bank::multiplayer, *bottom, placement, 0, 12);
    result_stamps(renderer, video, placement, flow.versus().wins(1), 1, 0x93);
    result_stamps(renderer, video, placement, flow.versus().wins(0), 15, 0x8F);
    if (flow.versus().state() == MatchState::match_over)
        result_text(renderer, video, content, placement,
                    flow.versus().wins(0) > flow.versus().wins(1)
                        ? "mario-wins-text" : "luigi-wins-text");
    result_actors(renderer, video, content, flow, placement);
    if (flow.versus_result_prompt_visible()) {
        const content::ByteTable* prompt = table(content, "push-start-objects");
        if (prompt != nullptr) {
            for (std::size_t offset = 0; offset + 3 < prompt->bytes.size(); offset += 4) {
                drawing::draw_tile(renderer, video, Bank::multiplayer, prompt->bytes[offset + 2],
                                   placement,
                                   static_cast<float>(static_cast<int>(prompt->bytes[offset + 1]) - 8) / 8.0F,
                                   static_cast<float>(static_cast<int>(prompt->bytes[offset]) - 16) / 8.0F,
                                   false, (prompt->bytes[offset + 3] & 0x20U) != 0);
            }
        }
    }
}

} // namespace tetris::presentation
