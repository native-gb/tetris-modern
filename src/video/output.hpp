#pragma once

#include "video/backend.hpp"
#include "video/video.hpp"

namespace tetris::video {

struct CpuOutput {
    SDL_Surface* surface{};
    SDL_Renderer* renderer{};
    SDL_Texture* upload{};
    Video assets;
    int width{};
    int height{};
};

struct Output {
    Video gpu;
    CpuOutput cpu;
    RenderBackend active_backend{RenderBackend::gpu_atlas};
    bool gpu_available{};
    bool force_cpu_fallback{};
};

bool initialize_output(Output& output, SDL_Renderer* renderer,
                       const content::Catalog& content);
bool render_output(Output& output, SDL_Renderer* renderer, const GubsyFrame& frame,
                   const content::Catalog& content, const GameState& game,
                   const settings::Settings& settings, const EffectState& effects,
                   DebugView debug, const MotionHistory& motion, float alpha);
void shutdown_output(Output& output);

} // namespace tetris::video
