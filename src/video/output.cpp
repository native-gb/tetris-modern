#include "video/output.hpp"

namespace tetris::video {
namespace {

void shutdown_cpu(CpuOutput& cpu) {
    shutdown_video(cpu.assets);
    SDL_DestroyTexture(cpu.upload);
    SDL_DestroyRenderer(cpu.renderer);
    SDL_DestroySurface(cpu.surface);
    cpu = {};
}

bool prepare_cpu(CpuOutput& cpu, SDL_Renderer* host, const content::Catalog& content,
                 int width, int height) {
    if (cpu.renderer != nullptr && cpu.width == width && cpu.height == height)
        return true;
    shutdown_cpu(cpu);
    cpu.surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA8888);
    if (cpu.surface == nullptr)
        return false;
    cpu.renderer = SDL_CreateSoftwareRenderer(cpu.surface);
    cpu.upload = SDL_CreateTexture(host, SDL_PIXELFORMAT_RGBA8888,
                                   SDL_TEXTUREACCESS_STREAMING, width, height);
    cpu.width = width;
    cpu.height = height;
    if (cpu.renderer == nullptr || cpu.upload == nullptr ||
        !initialize_video(cpu.assets, cpu.renderer, content)) {
        shutdown_cpu(cpu);
        return false;
    }
    (void)SDL_SetTextureScaleMode(cpu.upload, SDL_SCALEMODE_NEAREST);
    return true;
}

bool render_cpu(Output& output, SDL_Renderer* renderer, const GubsyFrame& frame,
                const content::Catalog& content, const GameState& game,
                const settings::Settings& settings, const EffectState& effects,
                DebugView debug, const MotionHistory& motion, float alpha) {
    if (!prepare_cpu(output.cpu, renderer, content, frame.render_width, frame.render_height))
        return false;
    GubsyFrame software_frame = frame;
    software_frame.window = nullptr;
    software_frame.renderer = output.cpu.renderer;
    software_frame.render_target = nullptr;
    render_video(output.cpu.assets, output.cpu.renderer, software_frame, content, game,
                 settings, effects, debug, motion, alpha);
    SDL_RenderPresent(output.cpu.renderer);
    if (!SDL_UpdateTexture(output.cpu.upload, nullptr, output.cpu.surface->pixels,
                           output.cpu.surface->pitch) ||
        !SDL_SetRenderTarget(renderer, frame.render_target)) {
        return false;
    }
    const bool drawn = SDL_RenderTexture(renderer, output.cpu.upload, nullptr, nullptr);
    output.active_backend = RenderBackend::cpu_raster;
    return drawn;
}

} // namespace

bool initialize_output(Output& output, SDL_Renderer* renderer,
                       const content::Catalog& content) {
    shutdown_output(output);
    output.gpu_available = initialize_video(output.gpu, renderer, content);
    return output.gpu_available || renderer != nullptr;
}

bool render_output(Output& output, SDL_Renderer* renderer, const GubsyFrame& frame,
                   const content::Catalog& content, const GameState& game,
                   const settings::Settings& settings, const EffectState& effects,
                   DebugView debug, const MotionHistory& motion, float alpha) {
    if (settings.renderer == RenderBackend::gpu_atlas && output.gpu_available &&
        !output.force_cpu_fallback) {
        render_video(output.gpu, renderer, frame, content, game, settings, effects, debug,
                     motion, alpha);
        output.active_backend = RenderBackend::gpu_atlas;
        return true;
    }
    return render_cpu(output, renderer, frame, content, game, settings, effects, debug,
                      motion, alpha);
}

void shutdown_output(Output& output) {
    shutdown_video(output.gpu);
    shutdown_cpu(output.cpu);
    output = {};
}

} // namespace tetris::video
