#include "video/endings.hpp"
#include "video/controller_labels.hpp"

#include "game/rules.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace tetris::video {
namespace {

using drawing::Bank;
using drawing::Placement;

const content::ByteTable* table(const content::Catalog& content, std::string_view id) {
    return content.find_layout(id);
}

void placement_sprite(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                      const Placement& placement, const content::ByteTable& sprites, int index,
                      int delta = 0, int raw_y_override = -1, Bank bank = Bank::multiplayer) {
    const std::size_t offset = static_cast<std::size_t>(index) * 6U;
    if (offset + 5 >= sprites.bytes.size())
        return;
    const std::uint8_t raw_y =
        static_cast<std::uint8_t>(raw_y_override >= 0 ? raw_y_override : sprites.bytes[offset + 1]);
    const std::uint8_t code =
        static_cast<std::uint8_t>(static_cast<int>(sprites.bytes[offset + 3]) + delta);
    drawing::draw_sprite(renderer, video, content, bank, code, placement, sprites.bytes[offset + 2],
                         raw_y, (sprites.bytes[offset + 5] & 0x20U) != 0);
}

void scoreboard_values(SDL_Renderer* renderer, const Video& video, const GameState& state,
                       const Placement& placement) {
    const Scoreboard& scoreboard = state.scoreboard;
    const int score_width = state.single_player.options.modern_scoring ? 9 : 6;
    const std::array counts = {scoreboard.lines.singles, scoreboard.lines.doubles,
                               scoreboard.lines.triples, scoreboard.lines.tetrises};
    for (int category = 0; category < 4; ++category) {
        const int row = 1 + category * 3;
        drawing::number(renderer, video, Bank::gameplay, placement,
                        counts[static_cast<std::size_t>(category)], 3, row, 2);
        drawing::left_number(
            renderer, video, Bank::gameplay, placement,
            line_clear_score(category + 1, state.single_player.rules.starting_level), 7, row);
    }
    drawing::number(renderer, video, Bank::gameplay, placement, scoreboard.soft_drop, 10, 13, 6);
    drawing::number(renderer, video, Bank::gameplay, placement, scoreboard.score, 10, 17,
                    score_width);
}

void dancers(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
             const GameState& state, const Placement& placement) {
    const content::ByteTable* sprites = table(content, "dancer-sprites");
    if (sprites == nullptr)
        return;
    const int height = state.single_player.rules.type_b_height;
    const int available = static_cast<int>(sprites->bytes.size() / 6U);
    const int visible = std::min(height == 5 ? 10 : std::clamp(height + 1, 1, 10), available);
    for (int index = 0; index < visible; ++index) {
        const std::size_t offset = static_cast<std::size_t>(index) * 6U;
        const int delta = dancer_frame(state, index);
        const int y =
            static_cast<int>(sprites->bytes[offset + 1]) + dancer_vertical_offset(state, index);
        placement_sprite(renderer, video, content, placement, *sprites, index, delta, y,
                         Bank::gameplay);
    }
}

void congratulations(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                     const GameState& state, const Placement& placement) {
    const content::ByteTable* characters = table(content, "congratulations-text");
    if (characters == nullptr)
        return;
    const int count =
        std::clamp(state.congratulations_characters, 0, static_cast<int>(characters->bytes.size()));
    for (int index = 0; index < count; ++index) {
        drawing::draw_tile(renderer, video, Bank::multiplayer,
                           characters->bytes[static_cast<std::size_t>(index)], placement,
                           static_cast<float>(2 + index), 4);
        drawing::draw_tile(renderer, video, Bank::multiplayer, 0xB6, placement,
                           static_cast<float>(2 + index), 5);
    }
}

void result_stamp(SDL_Renderer* renderer, const Video& video, const Placement& placement,
                  int column, int row, std::uint8_t tile) {
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x)
            drawing::draw_tile(renderer, video, Bank::multiplayer,
                               static_cast<std::uint8_t>(tile + y * 2 + x), placement,
                               static_cast<float>(column + x), static_cast<float>(row + y));
    }
}

void result_stamps(SDL_Renderer* renderer, const Video& video, const Placement& placement,
                   int count, int row, std::uint8_t tile) {
    for (int index = 0; index < std::min(count, 4); ++index)
        result_stamp(renderer, video, placement, 7 + index * 3, row, tile);
}

void result_text(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                 const Placement& placement, std::string_view id) {
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

void result_actors(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                   const GameState& state, const Placement& placement) {
    // Derive the current winner/loser animation step.
    const bool local_victory = state.versus.winner != RoundWinner::player_two;
    const bool final = state.versus.state == MatchState::match_over;
    const int animation = state.result_step % 2;
    const int cry = (state.result_elapsed / 15) % 2;
    const int step = state.result_step;
    const int within = state.result_elapsed % 25;
    const content::ByteTable* actors =
        table(content, local_victory ? "mario-victory-sprites" : "mario-defeat-sprites");
    if (actors == nullptr)
        return;

    // Animate Mario's victory and Luigi's reaction.
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
            drawing::draw_sprite(renderer, video, content, Bank::multiplayer, 0x57, placement, 0x38,
                                 0x69);
        return;
    }

    // Animate Mario's defeat and Luigi's celebration.
    if (!final || step <= 22) {
        const int loser_y = final && step == 22 ? std::min(0x60 + within * 4, 0x72) : 0x60;
        placement_sprite(renderer, video, content, placement, *actors, 0, cry, loser_y);
    } else if (step == 23) {
        drawing::draw_sprite(renderer, video, content, Bank::multiplayer, 0x56, placement, 0x60,
                             0x61);
    }
    placement_sprite(renderer, video, content, placement, *actors, 1, animation,
                     animation == 0 ? 0x68 : 0x60);
}

} // namespace

void draw_game_over(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                    const GameState& state, Bank bank, const Placement& placement,
                    const settings::Settings& settings, bool sprite_bounds, bool extended_hud) {
    // The curtain replaces board rows from bottom to top.
    if (state.ending_stage == EndingStage::game_over_curtain) {
        drawing::draw_session(renderer, video, content, bank, state.single_player, placement, false,
                              settings, sprite_bounds, false, extended_hud);
        for (int row = board_height - game_over_curtain_rows(state); row < board_height; ++row) {
            for (int column = 0; column < board_width; ++column)
                drawing::draw_tile(renderer, video, bank, 0x87, placement,
                                   static_cast<float>(2 + column), static_cast<float>(row));
        }
        return;
    }

    // The completed curtain reveals the game-over and retry panels.
    if (const content::Tilemap* panel = content.find_tilemap("game-over-panel"))
        drawing::draw_map(renderer, video, bank, *panel, placement, 3, 2);
    if (state.single_player.options.quick_restart) {
        const settings::ControllerLabels labels = resolve_controller_labels(
            settings.controller_labels, video.detected_controller_labels);
        const std::string continue_game =
            controller_prompt(labels, FaceButton::east, "CONTINUE");
        const std::string retry = controller_prompt(labels, FaceButton::north, "RETRY");
        const std::string menu = controller_prompt(labels, FaceButton::south, "MAIN MENU");
        drawing::draw_panel(renderer, placement, 1, 10, 18, 8);
        drawing::text(renderer, video, bank, placement, continue_game.c_str(), 3, 11);
        drawing::text(renderer, video, bank, placement, retry.c_str(), 3, 13);
        drawing::text(renderer, video, bank, placement, menu.c_str(), 3, 15);
    } else if (const content::Tilemap* retry = content.find_tilemap("try-again-panel")) {
        drawing::draw_map(renderer, video, bank, *retry, placement, 3, 12);
    }
    for (int row = 0; row < board_height - game_over_panel_rows(state); ++row) {
        for (int column = 0; column < board_width; ++column)
            drawing::draw_tile(renderer, video, bank, 0x87, placement,
                               static_cast<float>(2 + column), static_cast<float>(row));
    }
}

void draw_type_b_result(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                        const GameState& state, const Placement& placement) {
    if (state.screen == Screen::type_b_celebration || state.screen == Screen::scoreboard) {
        if (const content::Tilemap* board = content.find_tilemap("type-b-scoreboard"))
            drawing::draw_map(renderer, video, Bank::gameplay, *board, placement, 2, 0);
        if (state.screen == Screen::scoreboard)
            scoreboard_values(renderer, video, state, placement);
    } else if (state.screen == Screen::dancers) {
        if (const content::Tilemap* map = content.find_tilemap("dancers"))
            drawing::draw_map(renderer, video, Bank::gameplay, *map, placement, 2, 0);
        dancers(renderer, video, content, state, placement);
    }
}

void draw_launch(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                 const GameState& state, const Placement& placement) {
    // Assemble the launch pad from its extracted tilemap fragments.
    const auto submap = [&](const char* id, int column, int row) {
        if (const content::Tilemap* map = content.find_tilemap(id))
            drawing::draw_map(renderer, video, Bank::multiplayer, *map, placement, column, row);
    };
    submap("buran-backdrop", 0, 14);
    submap("left-tower-left", 6, 7);
    submap("left-tower-right", 7, 7);
    submap("right-tower-left", 12, 7);
    submap("right-tower-right", 13, 7);

    // Keep the Buran umbilicals visible through preflight and ignition.
    const bool umbilicals =
        state.screen == Screen::buran && (state.ending_stage == EndingStage::buran_initialize ||
                                          state.ending_stage == EndingStage::buran_prelaunch ||
                                          state.ending_stage == EndingStage::buran_ignition ||
                                          state.ending_stage == EndingStage::buran_final_ignition);
    if (umbilicals) {
        drawing::draw_tile(renderer, video, Bank::multiplayer, 0x72, placement, 8, 8);
        drawing::draw_tile(renderer, video, Bank::multiplayer, 0xC4, placement, 9, 8);
        drawing::draw_tile(renderer, video, Bank::multiplayer, 0xB7, placement, 8, 9);
        drawing::draw_tile(renderer, video, Bank::multiplayer, 0xB8, placement, 9, 9);
    }

    // Select and place the rocket or Buran body.
    const content::ByteTable* launch = table(
        content, state.screen == Screen::buran ? "buran-launch-sprites" : "rocket-launch-sprites");
    if (launch == nullptr || launch->bytes.size() < 18)
        return;
    std::uint8_t rocket = launch->bytes[3];
    if (state.screen == Screen::rocket) {
        if (state.rocket == Rocket::medium)
            ++rocket;
        if (state.rocket == Rocket::small)
            rocket = static_cast<std::uint8_t>(rocket + 2U);
    }
    drawing::draw_sprite(renderer, video, content, Bank::multiplayer, rocket, placement,
                         launch->bytes[2], static_cast<std::uint8_t>(state.launch_y));

    // Draw ignition smoke while the vehicle remains on the pad.
    if (launch_smoke_visible(state)) {
        const int delta = state.ending_stage == EndingStage::buran_final_ignition ? 1 : 0;
        placement_sprite(renderer, video, content, placement, *launch, 1, delta);
        placement_sprite(renderer, video, content, placement, *launch, 2, delta);
    }

    // Draw the animated exhaust after liftoff.
    if (state.ending_stage == EndingStage::rocket_rising ||
        state.ending_stage == EndingStage::buran_rising) {
        const std::uint8_t base = state.screen == Screen::buran ? 0x40 : 0x5C;
        drawing::draw_sprite(renderer, video, content, Bank::multiplayer,
                             static_cast<std::uint8_t>(base + exhaust_animation_frame(state)),
                             placement, static_cast<std::uint8_t>(exhaust_x(state)),
                             static_cast<std::uint8_t>(state.exhaust_y));
    }

    // Reveal congratulations text after the Buran leaves the screen.
    if (state.ending_stage == EndingStage::congratulations ||
        state.ending_stage == EndingStage::congratulations_wait)
        congratulations(renderer, video, content, state, placement);
}

void draw_versus_result(SDL_Renderer* renderer, const Video& video, const content::Catalog& content,
                        const GameState& state, const Placement& placement) {
    // Compose the static result frame and accumulated win stamps.
    if (const content::Tilemap* top = content.find_tilemap("multiplayer-victory-top"))
        drawing::draw_map(renderer, video, Bank::multiplayer, *top, placement, 0, 0);
    if (const content::Tilemap* bottom = content.find_tilemap("multiplayer-victory-bottom"))
        drawing::draw_map(renderer, video, Bank::multiplayer, *bottom, placement, 0, 12);
    result_stamps(renderer, video, placement, state.versus.wins[1], 1, 0x93);
    result_stamps(renderer, video, placement, state.versus.wins[0], 15, 0x8F);

    // Add final match text, actors, and the blinking start prompt.
    if (state.versus.state == MatchState::match_over)
        result_text(renderer, video, content, placement,
                    state.versus.wins[0] > state.versus.wins[1] ? "mario-wins-text"
                                                                : "luigi-wins-text");
    result_actors(renderer, video, content, state, placement);
    if (versus_result_prompt_visible(state)) {
        const content::ByteTable* prompt = table(content, "push-start-objects");
        if (prompt != nullptr) {
            for (std::size_t offset = 0; offset + 3 < prompt->bytes.size(); offset += 4) {
                drawing::draw_tile(
                    renderer, video, Bank::multiplayer, prompt->bytes[offset + 2], placement,
                    static_cast<float>(static_cast<int>(prompt->bytes[offset + 1]) - 8) / 8.0F,
                    static_cast<float>(static_cast<int>(prompt->bytes[offset]) - 16) / 8.0F, false,
                    (prompt->bytes[offset + 3] & 0x20U) != 0);
            }
        }
    }
}

} // namespace tetris::video
