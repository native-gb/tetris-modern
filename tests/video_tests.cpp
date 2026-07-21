#include "content/catalog.hpp"
#include "content/rom.hpp"
#include "game/state.hpp"
#include "settings.hpp"
#include "video/controller_labels.hpp"
#include "video/drawing.hpp"
#include "video/video.hpp"
#include "video/output.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace {

#ifdef TETRIS_TEST_ROM
int failures = 0;

void expect(bool condition, const char* message) {
    if (condition)
        return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

void test_controller_labels() {
    using namespace tetris::settings;
    using namespace tetris::video;
    expect(resolve_controller_labels(ControllerLabels::automatic, ControllerLabels::xbox) ==
               ControllerLabels::xbox,
           "automatic controller labels use the detected family");
    expect(std::string_view(face_button_label(ControllerLabels::nintendo, FaceButton::north)) ==
               "X" &&
               std::string_view(face_button_label(ControllerLabels::xbox, FaceButton::north)) ==
                   "Y" &&
               std::string_view(
                   face_button_label(ControllerLabels::playstation, FaceButton::north)) ==
                   "TRIANGLE",
           "north face prompt follows Nintendo, Xbox, and PlayStation legends");
    expect(controller_prompt(ControllerLabels::xbox, FaceButton::north, "RETRY") == "Y RETRY",
           "Xbox retry prompt uses the familiar Y label");
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

tetris::GameInput input(tetris::Buttons one = {}, tetris::Buttons two = {}) {
    return {
        .player_one = one,
        .player_two = two,
        .random = {11, 12, 13},
        .startup = startup(),
        .versus = versus_random(),
    };
}

void step(tetris::GameState& state, int frames) {
    for (int frame = 0; frame < frames; ++frame)
        step_game(state, input());
}

void press(tetris::GameState& state, tetris::Buttons one, tetris::Buttons two = {}) {
    step_game(state, input(one, two));
    step_game(state, input());
}

struct Target {
    SDL_Texture* texture{};
    int width{};
    int height{};
};

Target make_target(SDL_Renderer* renderer, int width, int height) {
    return {
        .texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                     width, height),
        .width = width,
        .height = height,
    };
}

void draw(SDL_Renderer* renderer, SDL_Window* window, const Target& target,
          const tetris::video::Video& video, const tetris::content::Catalog& content,
          const tetris::GameState& state, const tetris::settings::Settings& settings,
          const tetris::video::EffectState& effects = {}, tetris::video::DebugView debug = {}) {
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
    tetris::video::render_video(video, renderer, frame, content, state, settings, effects, debug);
}

std::uint64_t checksum(SDL_Renderer* renderer) {
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
    if (surface == nullptr)
        return 0;
    const auto* bytes = static_cast<const std::uint8_t*>(surface->pixels);
    const std::size_t count =
        static_cast<std::size_t>(surface->pitch) * static_cast<std::size_t>(surface->h);
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

void test_sprite_transparency(SDL_Renderer* renderer, const tetris::video::Video& video) {
    SDL_Texture* previous = SDL_GetRenderTarget(renderer);
    expect(SDL_SetRenderTarget(renderer, video.gameplay.sprites),
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

    // Title layers must remove only externally connected sky. The foreground
    // reuses that light shade inside its buildings, where it remains opaque.
    expect(SDL_SetRenderTarget(renderer, video.title_towers.texture),
           "assembled title foreground is render-readable");
    surface = SDL_RenderReadPixels(renderer, nullptr);
    if (surface != nullptr) {
        std::uint8_t red = 0;
        std::uint8_t green = 0;
        std::uint8_t blue = 0;
        std::uint8_t exterior_alpha = 0;
        std::uint8_t interior_alpha = 0;
        std::uint8_t misplaced_skyline_alpha = 0;
        std::uint8_t left_base_alpha = 0;
        std::uint8_t right_base_alpha = 0;
        (void)SDL_ReadSurfacePixel(surface, 0, 0, &red, &green, &blue, &exterior_alpha);
        (void)SDL_ReadSurfacePixel(surface, 15, 21, &red, &green, &blue, &interior_alpha);
        (void)SDL_ReadSurfacePixel(surface, 33, 42, &red, &green, &blue,
                                   &misplaced_skyline_alpha);
        (void)SDL_ReadSurfacePixel(surface, 5, 47, &red, &green, &blue, &left_base_alpha);
        (void)SDL_ReadSurfacePixel(surface, 26, 47, &red, &green, &blue, &right_base_alpha);
        expect(exterior_alpha == 0 && interior_alpha == 255,
               "title mask removes exterior sky but preserves enclosed light building pixels");
        expect(misplaced_skyline_alpha == 0,
               "low interstitial buildings are excluded from the title foreground");
        expect(left_base_alpha == 255 && right_base_alpha == 255,
               "both dark base endpoints remain attached to the large foreground tower");
        SDL_DestroySurface(surface);
    } else {
        expect(false, "assembled title foreground pixels can be inspected");
    }

    expect(SDL_SetRenderTarget(renderer, video.title_skyline.texture),
           "separated low title skyline is render-readable");
    surface = SDL_RenderReadPixels(renderer, nullptr);
    if (surface != nullptr) {
        std::uint8_t red = 0;
        std::uint8_t green = 0;
        std::uint8_t blue = 0;
        std::uint8_t between_alpha = 0;
        std::uint8_t left_alpha = 0;
        std::uint8_t foreground_endpoint_alpha = 0;
        (void)SDL_ReadSurfacePixel(surface, 33, 42, &red, &green, &blue, &between_alpha);
        (void)SDL_ReadSurfacePixel(surface, 1, 45, &red, &green, &blue, &left_alpha);
        (void)SDL_ReadSurfacePixel(surface, 5, 47, &red, &green, &blue,
                                   &foreground_endpoint_alpha);
        expect(between_alpha == 255 && left_alpha == 255,
               "between-tower and lower-left buildings belong to the distant skyline");
        expect(foreground_endpoint_alpha == 0,
               "foreground base endpoints are absent from the distant skyline");
        SDL_DestroySurface(surface);
    } else {
        expect(false, "separated low title skyline pixels can be inspected");
    }
    (void)SDL_SetRenderTarget(renderer, previous);
}

void test_sprite_geometry(const tetris::content::Catalog& content) {
    using namespace tetris::video::drawing;
    const tetris::content::Sprite& piece = content.sprites.sprites[0];
    const SpritePosition active =
        sprite_object_position(piece, piece.objects[0], 0x3F, 0x18);
    expect(active.x == 40 && active.y == 8,
           "active-piece sprite coordinates retain the Game Boy carry between additions");

    bool pieces_align_to_board = true;
    for (std::size_t piece_index = 0; piece_index < content.gameplay.tetrominoes.size();
         ++piece_index) {
        const tetris::content::Sprite& sprite = content.sprites.sprites[piece_index];
        const tetris::content::TetrominoDefinition& definition =
            content.gameplay.tetrominoes[piece_index];
        for (std::size_t block = 0; block < sprite.objects.size(); ++block) {
            const SpritePosition position =
                sprite_object_position(sprite, sprite.objects[block], 0x3F, 0x18);
            const tetris::Cell cell = definition.cells[block];
            pieces_align_to_board = pieces_align_to_board && position.x == (cell.x + 5) * 8 &&
                                    position.y == (cell.y - 1) * 8;
        }
    }
    expect(pieces_align_to_board, "all 28 falling-piece sprites align to their board cells");

    const SpritePosition preview =
        sprite_object_position(piece, piece.objects[0], 0x8F, 0x80);
    expect(preview.x == 120 && preview.y == 112,
           "preview-piece sprite coordinates retain their ROM-authored placement");
}

void test_scaling_and_widescreen(SDL_Renderer* renderer, SDL_Window* window,
                                 const tetris::video::Video& video,
                                 const tetris::content::Catalog& content) {
    using namespace tetris;
    using namespace tetris::settings;
    using namespace tetris::video;
    GameState state;
    start_game(state, {content.gameplay.data, content.type_a_demo.runs, content.type_b_demo.runs,
                       content.demo_pieces, content.type_b_demo_garbage.bytes});
    // Use the full-canvas copyright composition to measure scaling independently
    // of the title screen's intentional dark border.
    state.screen = Screen::copyright_skippable;
    const Settings original = original_settings();

    Target exact = make_target(renderer, 640, 576);
    draw(renderer, window, exact, video, content, state, original);
    const std::array<std::uint8_t, 4> host_background{22, 36, 28, 255};
    expect(pixel(renderer, 0, 0) != host_background && pixel(renderer, 639, 575) != host_background,
           "an exact four-times original view reaches all target edges");

    Target hd = make_target(renderer, 1920, 1080);
    draw(renderer, window, hd, video, content, state, original);
    expect(pixel(renderer, 0, 0) == host_background && pixel(renderer, 400, 36) != host_background,
           "1080p uses centered lossless seven-times scaling and stable margins");

    Target ultra = make_target(renderer, 3840, 2160);
    draw(renderer, window, ultra, video, content, state, original);
    expect(pixel(renderer, 0, 0) == host_background && pixel(renderer, 720, 0) != host_background,
           "4K uses centered lossless fifteen-times scaling");

    Settings custom = original;
    custom.layout = Layout::original_screen;
    custom.background = Background::stars;
    draw(renderer, window, hd, video, content, state, custom);
    const std::uint64_t original_with_stars_selected = checksum(renderer);
    draw(renderer, window, hd, video, content, state, original);
    expect(checksum(renderer) == original_with_stars_selected,
           "original layout does not leak widescreen background effects");
    custom.layout = Layout::widescreen_frame;
    draw(renderer, window, hd, video, content, state, custom);
    expect(checksum(renderer) != original_with_stars_selected,
           "widescreen frame draws its deterministic Game Boy palette background");
    const std::uint64_t stars = checksum(renderer);
    custom.background = Background::snow;
    draw(renderer, window, hd, video, content, state, custom);
    expect(checksum(renderer) != stars,
           "snowflake and four-point-star backgrounds have distinct pixel glyphs");

    SDL_DestroyTexture(ultra.texture);
    SDL_DestroyTexture(hd.texture);
    SDL_DestroyTexture(exact.texture);
}

void test_screen_compositions(SDL_Renderer* renderer, SDL_Window* window,
                              const tetris::video::Video& video,
                              const tetris::content::Catalog& content) {
    using namespace tetris;
    using namespace tetris::settings;
    using namespace tetris::video;
    const Settings settings = enhanced_settings();
    Target target = make_target(renderer, 1280, 720);
    GameState state;
    start_game(state, {content.gameplay.data, content.type_a_demo.runs, content.type_b_demo.runs,
                       content.demo_pieces, content.type_b_demo_garbage.bytes});
    press(state, {.start = true});
    draw(renderer, window, target, video, content, state, settings);
    const std::uint64_t type_a_cursor = checksum(renderer);
    press(state, {.right = true});
    draw(renderer, window, target, video, content, state, settings);
    expect(checksum(renderer) != type_a_cursor,
           "moving the game-type selection visibly moves its ROM cursor");
    const std::uint64_t ordinary_menu = checksum(renderer);
    draw(renderer, window, target, video, content, state, settings, {}, {.grid = true});
    expect(checksum(renderer) != ordinary_menu,
           "semantic rendering grid visibly overlays the scene");

    GameState game;
    start_game(game, {content.gameplay.data, content.type_a_demo.runs, content.type_b_demo.runs,
                      content.demo_pieces, content.type_b_demo_garbage.bytes});
    start_session(game, {.type = GameType::type_a}, startup());
    draw(renderer, window, target, video, content, game, settings);
    const std::uint64_t empty_score = checksum(renderer);
    game.single_player.debug_set_score(123'456);
    draw(renderer, window, target, video, content, game, settings);
    expect(checksum(renderer) != empty_score, "Type-A HUD renders live score digits");
    const std::uint64_t active = checksum(renderer);
    draw(renderer, window, target, video, content, game, settings, {}, {.board_cells = true});
    expect(checksum(renderer) != active,
           "semantic board-cell and active-piece collision overlays are visible");
    game.single_player.set_paused(true);
    draw(renderer, window, target, video, content, game, settings);
    expect(checksum(renderer) != active, "pause map visibly overlays gameplay");

    game.single_player.set_paused(false);
    game.single_player.debug_set_score(200'000);
    game.single_player.debug_set_state(PlayState::game_over);
    step_game(game, input());
    draw(renderer, window, target, video, content, game, settings);
    const std::uint64_t curtain_start = checksum(renderer);
    step(game, 18);
    draw(renderer, window, target, video, content, game, settings);
    expect(checksum(renderer) != curtain_start,
           "game-over curtain visibly advances through the board");
    step(game, 196);
    expect(game.screen == Screen::rocket, "large-score fixture reaches rocket scene");
    draw(renderer, window, target, video, content, game, settings);
    expect(checksum(renderer) != 0, "rocket composition renders to a readable target");

    GameState modern_game;
    start_game(modern_game,
               {content.gameplay.data, content.type_a_demo.runs, content.type_b_demo.runs,
                content.demo_pieces, content.type_b_demo_garbage.bytes});
    Settings modern = modern_settings();
    modern.background = Background::off;
    set_gameplay_options(modern_game, modern.gameplay);
    start_session(modern_game, {.type = GameType::type_a}, startup());
    draw(renderer, window, target, video, content, modern_game, modern);
    const std::array<std::uint8_t, 4> host_background{22, 36, 28, 255};
    expect(pixel(renderer, 139, 360) == host_background &&
               pixel(renderer, 140, 360) != host_background &&
               pixel(renderer, 1139, 360) != host_background &&
               pixel(renderer, 1140, 360) == host_background,
           "Modern Type-A uses a centered lossless 25-tile composition");

    Target square = make_target(renderer, 640, 576);
    draw(renderer, window, square, video, content, modern_game, modern);
    const std::uint64_t responsive_square = checksum(renderer);
    Settings compact = modern;
    compact.extended_canvas = false;
    draw(renderer, window, square, video, content, modern_game, compact);
    expect(checksum(renderer) == responsive_square,
           "a square window falls back to the original 20-tile composition");
    SDL_DestroyTexture(square.texture);

    modern_game.single_player.debug_set_state(PlayState::game_over);
    step_game(modern_game, input());
    draw(renderer, window, target, video, content, modern_game, modern);
    expect(pixel(renderer, 139, 360) == host_background &&
               pixel(renderer, 140, 360) != host_background &&
               pixel(renderer, 1139, 360) != host_background &&
               pixel(renderer, 1140, 360) == host_background,
           "game over retains the responsive extended canvas");

    start_session(modern_game, {.type = GameType::type_a}, startup());
    draw(renderer, window, target, video, content, modern_game, modern);
    const std::uint64_t guideline_pieces = checksum(renderer);
    modern.piece_colors = PieceColors::game_boy;
    draw(renderer, window, target, video, content, modern_game, modern);
    expect(checksum(renderer) != guideline_pieces,
           "Guideline colors tint the active, settled, Hold, and Next pieces");
    modern.piece_colors = PieceColors::guideline;
    const std::uint64_t modern_with_ghost = checksum(renderer);
    modern_game.single_player.options.ghost_piece = false;
    draw(renderer, window, target, video, content, modern_game, modern);
    expect(checksum(renderer) != modern_with_ghost,
           "Modern HUD and landing ghost render as semantic game layers");

    start_session(modern_game,
                  {.type = GameType::type_b, .starting_level = 7, .type_b_height = 4}, startup());
    modern_game.single_player.score = 12'345;
    modern_game.single_player.lines = 19;
    draw(renderer, window, target, video, content, modern_game, modern);
    const std::uint64_t modern_type_b = checksum(renderer);
    expect(pixel(renderer, 139, 360) == host_background &&
               pixel(renderer, 1139, 360) != host_background,
           "Modern Type B receives the responsive extended canvas");
    modern_game.single_player.held_piece = PieceSpec{PieceKind::T};
    draw(renderer, window, target, video, content, modern_game, modern);
    expect(checksum(renderer) != modern_type_b,
           "Modern Type B renders hold, queue, score, level, height, and remaining lines");

    SDL_DestroyTexture(target.texture);
}

void finish_versus_round(tetris::GameState& state, int winner) {
    state.versus.players[static_cast<std::size_t>(winner)].game.debug_set_state(
        tetris::PlayState::complete);
    step_game(state, input());
    step(state, 40);
}

void continue_versus(tetris::GameState& state) {
    press(state, {.start = true});
    press(state, {.confirm = true});
}

void test_versus_results(SDL_Renderer* renderer, SDL_Window* window,
                         const tetris::video::Video& video,
                         const tetris::content::Catalog& content) {
    using namespace tetris;
    GameState state;
    start_game(state, {content.gameplay.data, content.type_a_demo.runs, content.type_b_demo.runs,
                       content.demo_pieces, content.type_b_demo_garbage.bytes});
    press(state, {.start = true});
    press(state, {.right = true});
    press(state, {.right = true});
    press(state, {.start = true});
    press(state, {.confirm = true});

    Target target = make_target(renderer, 1280, 720);
    finish_versus_round(state, 0);
    expect(state.screen == Screen::versus_round_result, "render fixture reaches a round result");
    draw(renderer, window, target, video, content, state, tetris::settings::original_settings());
    const std::uint64_t round_result = checksum(renderer);

    for (int round = 1; round < 4; ++round) {
        continue_versus(state);
        finish_versus_round(state, 0);
    }
    expect(state.screen == Screen::versus_match_result,
           "fourth win reaches the original match result");
    draw(renderer, window, target, video, content, state, tetris::settings::original_settings());
    expect(checksum(renderer) != round_result,
           "final Mario win composition differs from an ordinary round result");
    SDL_DestroyTexture(target.texture);
}

void test_versus_fit(SDL_Renderer* renderer, SDL_Window* window, const tetris::video::Video& video,
                     const tetris::content::Catalog& content) {
    using namespace tetris;
    GameState state;
    start_game(state, {content.gameplay.data, content.type_a_demo.runs, content.type_b_demo.runs,
                       content.demo_pieces, content.type_b_demo_garbage.bytes});
    press(state, {.start = true});
    press(state, {.right = true});
    press(state, {.right = true});
    press(state, {.start = true});
    press(state, {.confirm = true});
    expect(state.screen == Screen::versus_gameplay, "render fixture reaches local versus gameplay");

    Target target = make_target(renderer, 1280, 720);
    draw(renderer, window, target, video, content, state, tetris::settings::enhanced_settings());
    const std::array<std::uint8_t, 4> host_background{22, 36, 28, 255};
    expect(pixel(renderer, 136, 144) != host_background &&
               pixel(renderer, 808, 144) != host_background,
           "both losslessly scaled versus screens fit inside the host target");
    SDL_DestroyTexture(target.texture);
}

void test_output_backends(SDL_Renderer* renderer, SDL_Window* window,
                          const tetris::content::Catalog& content) {
    using namespace tetris;
    video::Output output;
    expect(video::initialize_output(output, renderer, content),
           "selectable output initializes");
    GameState state;
    start_game(state, {content.gameplay.data, content.type_a_demo.runs,
                       content.type_b_demo.runs, content.demo_pieces,
                       content.type_b_demo_garbage.bytes});
    Target gpu = make_target(renderer, 1280, 720);
    Target cpu = make_target(renderer, 1280, 720);
    const auto frame_for = [&](SDL_Texture* target) {
        return GubsyFrame{
            .backend = GubsyRenderBackend::SDLRenderer,
            .window = window,
            .renderer = renderer,
            .render_target = target,
            .window_width = 1280,
            .window_height = 720,
            .render_width = 1280,
            .render_height = 720,
        };
    };
    settings::Settings settings = settings::original_settings();
    settings.renderer = video::RenderBackend::gpu_atlas;
    expect(video::render_output(output, renderer, frame_for(gpu.texture), content, state,
                                settings, {}, {}, {}, 1.0F),
           "GPU output renders");
    (void)SDL_SetRenderTarget(renderer, gpu.texture);
    const std::uint64_t gpu_hash = checksum(renderer);
    settings.renderer = video::RenderBackend::cpu_raster;
    expect(video::render_output(output, renderer, frame_for(cpu.texture), content, state,
                                settings, {}, {}, {}, 1.0F),
           "CPU output renders");
    (void)SDL_SetRenderTarget(renderer, cpu.texture);
    expect(checksum(renderer) == gpu_hash,
           "GPU atlas and CPU raster output match for a complete frame");
    video::shutdown_output(output);
    SDL_DestroyTexture(cpu.texture);
    SDL_DestroyTexture(gpu.texture);
}
#endif

} // namespace

int main() {
#ifndef TETRIS_TEST_ROM
    std::puts("modern Native Tetris rendering tests skipped without a ROM");
    return 0;
#else
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("rendering-test", 1280, 720, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window != nullptr ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (window == nullptr || renderer == nullptr) {
        std::fprintf(stderr, "SDL renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }

    tetris::content::Rom rom;
    tetris::content::Catalog content;
    tetris::video::Video video;
    std::string error;
    expect(tetris::content::load_rom(TETRIS_TEST_ROM, rom, error), "test ROM loads");
    expect(tetris::content::extract_catalog(rom, content, error), "content catalog extracts");
    expect(tetris::video::initialize_video(video, renderer, content), "render atlases initialize");
    if (failures == 0) {
        test_controller_labels();
        test_sprite_geometry(content);
        test_sprite_transparency(renderer, video);
        test_scaling_and_widescreen(renderer, window, video, content);
        test_screen_compositions(renderer, window, video, content);
        test_versus_fit(renderer, window, video, content);
        test_versus_results(renderer, window, video, content);
        test_output_backends(renderer, window, content);
    }

    tetris::video::shutdown_video(video);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (failures != 0)
        return 1;
    std::puts("modern Native Tetris rendering tests passed");
    return 0;
#endif
}
