#include "content/catalog.hpp"
#include "content/rom.hpp"
#include "game/flow.hpp"
#include "presentation/renderer.hpp"
#include "presentation/settings.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {

#ifdef TETRIS_TEST_ROM
int failures = 0;

void expect(bool condition, const char* message) {
    if (condition)
        return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

tetris::StartupRandom startup() {
    tetris::StartupRandom random;
    random.pieces = {{{2, 3, 4}, {5, 6, 7}, {8, 9, 10}}};
    return random;
}

tetris::VersusRandom versus_random() {
    tetris::VersusRandom random;
    for (std::size_t index = 0; index < random.pieces.size(); ++index)
        random.pieces[index] = {static_cast<tetris::PieceKind>(index % 7)};
    return random;
}

tetris::FlowInput input(tetris::Buttons one = {}, tetris::Buttons two = {}) {
    return {
        .player_one = one,
        .player_two = two,
        .random = {11, 12, 13},
        .startup = startup(),
        .versus = versus_random(),
    };
}

void tick(tetris::GameFlow& flow, int frames) {
    for (int frame = 0; frame < frames; ++frame)
        flow.tick(input());
}

void press(tetris::GameFlow& flow, tetris::Buttons one,
           tetris::Buttons two = {}) {
    flow.tick(input(one, two));
    flow.tick(input());
}

struct Target {
    SDL_Texture* texture{};
    int width{};
    int height{};
};

Target make_target(SDL_Renderer* renderer, int width, int height) {
    return {
        .texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                     SDL_TEXTUREACCESS_TARGET, width, height),
        .width = width,
        .height = height,
    };
}

void draw(SDL_Renderer* renderer, SDL_Window* window, const Target& target,
          const tetris::presentation::Renderer& video,
          const tetris::content::Catalog& content, const tetris::GameFlow& flow,
          const tetris::presentation::Settings& settings,
          const tetris::presentation::EffectState& effects = {},
          tetris::presentation::DebugView debug = {}) {
    const GubsyFrame frame{
        .backend = GubsyRenderBackend::SDLRenderer,
        .window = window,
        .renderer = renderer,
        .render_target = target.texture,
        .window_width = target.width,
        .window_height = target.height,
        .render_width = target.width,
        .render_height = target.height,
    };
    video.draw(renderer, frame, content, flow, settings, effects, debug);
}

std::uint64_t checksum(SDL_Renderer* renderer) {
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
    if (surface == nullptr)
        return 0;
    const auto* bytes = static_cast<const std::uint8_t*>(surface->pixels);
    const std::size_t count = static_cast<std::size_t>(surface->pitch) *
                              static_cast<std::size_t>(surface->h);
    std::uint64_t hash = 1'469'598'103'934'665'603ULL;
    for (std::size_t index = 0; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 1'099'511'628'211ULL;
    }
    SDL_DestroySurface(surface);
    return hash;
}

std::array<std::uint8_t, 4> pixel(SDL_Renderer* renderer, int x, int y) {
    const SDL_Rect area{x, y, 1, 1};
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, &area);
    std::array<std::uint8_t, 4> result{};
    if (surface != nullptr) {
        (void)SDL_ReadSurfacePixel(surface, 0, 0, &result[0], &result[1], &result[2], &result[3]);
        SDL_DestroySurface(surface);
    }
    return result;
}

void test_sprite_transparency(SDL_Renderer* renderer,
                              const tetris::presentation::Renderer& video) {
    SDL_Texture* previous = SDL_GetRenderTarget(renderer);
    expect(SDL_SetRenderTarget(renderer, video.gameplay().sprites),
           "transparent sprite atlas is render-readable");
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
    expect(surface != nullptr, "transparent sprite atlas can be inspected");
    int transparent = 0;
    int opaque = 0;
    if (surface != nullptr) {
        for (int y = 0; y < surface->h; ++y) {
            for (int x = 0; x < surface->w; ++x) {
                std::uint8_t red = 0;
                std::uint8_t green = 0;
                std::uint8_t blue = 0;
                std::uint8_t alpha = 0;
                (void)SDL_ReadSurfacePixel(surface, x, y, &red, &green, &blue, &alpha);
                transparent += alpha == 0 ? 1 : 0;
                opaque += alpha == 255 ? 1 : 0;
            }
        }
        SDL_DestroySurface(surface);
    }
    expect(transparent > 0 && opaque > 0,
           "sprite color zero is transparent while visible pixels are opaque");
    (void)SDL_SetRenderTarget(renderer, previous);
}

void test_scaling_and_widescreen(SDL_Renderer* renderer, SDL_Window* window,
                                 const tetris::presentation::Renderer& video,
                                 const tetris::content::Catalog& content) {
    using namespace tetris;
    using namespace tetris::presentation;
    GameFlow flow;
    flow.start({content.type_a_demo.runs, content.type_b_demo.runs,
                content.demo_pieces, content.type_b_demo_garbage.bytes});
    const Settings original = original_settings();

    Target exact = make_target(renderer, 640, 576);
    draw(renderer, window, exact, video, content, flow, original);
    const std::array<std::uint8_t, 4> host_background{9, 12, 18, 255};
    expect(pixel(renderer, 0, 0) != host_background &&
               pixel(renderer, 639, 575) != host_background,
           "an exact four-times original view reaches all target edges");

    Target hd = make_target(renderer, 1920, 1080);
    draw(renderer, window, hd, video, content, flow, original);
    expect(pixel(renderer, 0, 0) == host_background &&
               pixel(renderer, 400, 36) != host_background,
           "1080p uses centered lossless seven-times scaling and stable margins");

    Target ultra = make_target(renderer, 3840, 2160);
    draw(renderer, window, ultra, video, content, flow, original);
    expect(pixel(renderer, 0, 0) == host_background &&
               pixel(renderer, 720, 0) != host_background,
           "4K uses centered lossless fifteen-times scaling");

    Settings custom = original;
    custom.layout = Layout::original_screen;
    custom.background = Background::stars;
    draw(renderer, window, hd, video, content, flow, custom);
    const std::uint64_t original_with_stars_selected = checksum(renderer);
    draw(renderer, window, hd, video, content, flow, original);
    expect(checksum(renderer) == original_with_stars_selected,
           "original layout does not leak widescreen background effects");
    custom.layout = Layout::widescreen_frame;
    draw(renderer, window, hd, video, content, flow, custom);
    expect(checksum(renderer) != original_with_stars_selected,
           "widescreen frame draws its deterministic Game Boy palette background");

    SDL_DestroyTexture(ultra.texture);
    SDL_DestroyTexture(hd.texture);
    SDL_DestroyTexture(exact.texture);
}

void test_flow_compositions(SDL_Renderer* renderer, SDL_Window* window,
                            const tetris::presentation::Renderer& video,
                            const tetris::content::Catalog& content) {
    using namespace tetris;
    using namespace tetris::presentation;
    const Settings settings = enhanced_settings();
    Target target = make_target(renderer, 1280, 720);
    GameFlow flow;
    flow.start({content.type_a_demo.runs, content.type_b_demo.runs,
                content.demo_pieces, content.type_b_demo_garbage.bytes});
    tick(flow, 250);
    press(flow, {.rotate_right = true});
    press(flow, {.start = true});
    draw(renderer, window, target, video, content, flow, settings);
    const std::uint64_t type_a_cursor = checksum(renderer);
    press(flow, {.right = true});
    draw(renderer, window, target, video, content, flow, settings);
    expect(checksum(renderer) != type_a_cursor,
           "moving the game-type selection visibly moves its ROM cursor");
    const std::uint64_t ordinary_menu = checksum(renderer);
    draw(renderer, window, target, video, content, flow, settings, {}, {.grid = true});
    expect(checksum(renderer) != ordinary_menu,
           "semantic presentation grid visibly overlays the scene");

    GameFlow game;
    game.start({content.type_a_demo.runs, content.type_b_demo.runs,
                content.demo_pieces, content.type_b_demo_garbage.bytes});
    game.start_session({.type = GameType::type_a}, startup());
    draw(renderer, window, target, video, content, game, settings);
    const std::uint64_t empty_score = checksum(renderer);
    game.edit_game().set_score_for_test(123'456);
    draw(renderer, window, target, video, content, game, settings);
    expect(checksum(renderer) != empty_score, "Type-A HUD renders live score digits");
    const std::uint64_t active = checksum(renderer);
    draw(renderer, window, target, video, content, game, settings, {},
         {.board_cells = true});
    expect(checksum(renderer) != active,
           "semantic board-cell and active-piece collision overlays are visible");
    game.edit_game().set_paused(true);
    draw(renderer, window, target, video, content, game, settings);
    expect(checksum(renderer) != active, "pause map visibly overlays gameplay");

    game.edit_game().set_paused(false);
    game.edit_game().set_score_for_test(200'000);
    game.edit_game().set_state_for_test(PlayState::game_over);
    game.tick(input());
    draw(renderer, window, target, video, content, game, settings);
    const std::uint64_t curtain_start = checksum(renderer);
    tick(game, 18);
    draw(renderer, window, target, video, content, game, settings);
    expect(checksum(renderer) != curtain_start,
           "game-over curtain visibly advances through the board");
    tick(game, 196);
    expect(game.screen() == Screen::rocket, "large-score fixture reaches rocket scene");
    draw(renderer, window, target, video, content, game, settings);
    expect(checksum(renderer) != 0, "rocket composition renders to a readable target");

    SDL_DestroyTexture(target.texture);
}

void finish_versus_round(tetris::GameFlow& flow, int winner) {
    flow.edit_versus().edit_player(winner).set_state_for_test(tetris::PlayState::complete);
    flow.tick(input());
    tick(flow, 40);
}

void continue_versus(tetris::GameFlow& flow) {
    press(flow, {.start = true});
    press(flow, {.rotate_right = true});
}

void test_versus_results(SDL_Renderer* renderer, SDL_Window* window,
                         const tetris::presentation::Renderer& video,
                         const tetris::content::Catalog& content) {
    using namespace tetris;
    GameFlow flow;
    flow.start({content.type_a_demo.runs, content.type_b_demo.runs,
                content.demo_pieces, content.type_b_demo_garbage.bytes});
    tick(flow, 250);
    press(flow, {.rotate_right = true});
    press(flow, {.start = true});
    press(flow, {.right = true});
    press(flow, {.right = true});
    press(flow, {.start = true});
    press(flow, {.rotate_right = true});

    Target target = make_target(renderer, 1280, 720);
    finish_versus_round(flow, 0);
    expect(flow.screen() == Screen::versus_round_result,
           "render fixture reaches a round result");
    draw(renderer, window, target, video, content, flow,
         tetris::presentation::original_settings());
    const std::uint64_t round_result = checksum(renderer);

    for (int round = 1; round < 4; ++round) {
        continue_versus(flow);
        finish_versus_round(flow, 0);
    }
    expect(flow.screen() == Screen::versus_match_result,
           "fourth win reaches the original match result");
    draw(renderer, window, target, video, content, flow,
         tetris::presentation::original_settings());
    expect(checksum(renderer) != round_result,
           "final Mario win composition differs from an ordinary round result");
    SDL_DestroyTexture(target.texture);
}

void test_versus_fit(SDL_Renderer* renderer, SDL_Window* window,
                     const tetris::presentation::Renderer& video,
                     const tetris::content::Catalog& content) {
    using namespace tetris;
    GameFlow flow;
    flow.start({content.type_a_demo.runs, content.type_b_demo.runs,
                content.demo_pieces, content.type_b_demo_garbage.bytes});
    tick(flow, 250);
    press(flow, {.rotate_right = true});
    press(flow, {.start = true});
    press(flow, {.right = true});
    press(flow, {.right = true});
    press(flow, {.start = true});
    press(flow, {.rotate_right = true});
    expect(flow.screen() == Screen::versus_gameplay,
           "render fixture reaches local versus gameplay");

    Target target = make_target(renderer, 1280, 720);
    draw(renderer, window, target, video, content, flow,
         tetris::presentation::enhanced_settings());
    const std::array<std::uint8_t, 4> host_background{9, 12, 18, 255};
    expect(pixel(renderer, 136, 144) != host_background &&
               pixel(renderer, 808, 144) != host_background,
           "both losslessly scaled versus screens fit inside the host target");
    SDL_DestroyTexture(target.texture);
}
#endif

} // namespace

int main() {
#ifndef TETRIS_TEST_ROM
    std::puts("modern Native Tetris presentation tests skipped without a ROM");
    return 0;
#else
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("presentation-test", 1280, 720, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window != nullptr ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (window == nullptr || renderer == nullptr) {
        std::fprintf(stderr, "SDL renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }

    tetris::content::Rom rom;
    tetris::content::Catalog content;
    tetris::presentation::Renderer video;
    std::string error;
    expect(tetris::content::load_rom(TETRIS_TEST_ROM, rom, error), "test ROM loads");
    expect(tetris::content::extract_catalog(rom, content, error), "content catalog extracts");
    expect(video.initialize(renderer, content), "render atlases initialize");
    if (failures == 0) {
        test_sprite_transparency(renderer, video);
        test_scaling_and_widescreen(renderer, window, video, content);
        test_flow_compositions(renderer, window, video, content);
        test_versus_fit(renderer, window, video, content);
        test_versus_results(renderer, window, video, content);
    }

    video.shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (failures != 0)
        return 1;
    std::puts("modern Native Tetris presentation tests passed");
    return 0;
#endif
}
