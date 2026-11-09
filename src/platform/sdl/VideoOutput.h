#pragma once
#include "engine/diagnostics/FrameProfiler.h"
#include "engine/render/Renderer.h"
#include <SDL3/SDL.h>
#include <string>

namespace thorium::platform {
/** Own SDL presentation resources, shared by the game and full-pipeline benchmark. */
class VideoOutput {
  public:
    /** Construct an unopened output; the caller retains ownership of the window. */
    VideoOutput() = default;
    VideoOutput(const VideoOutput &) = delete;
    VideoOutput &operator=(const VideoOutput &) = delete;
    std::string error;
    /** Release textures and renderer before the caller destroys its window. */
    ~VideoOutput();
    /** Create one renderer and select VSync explicitly for gameplay or benchmarks. */
    bool open(SDL_Window *window, bool vsync = true);
    /** Close idempotently so a platform owner can enforce window destruction order. */
    void close();
    /** Update logical coordinates used by both input conversion and output stretching. */
    bool resize(int width, int height);
    /** Normalize one copied SDL event to the current logical output coordinates. */
    bool convertEvent(SDL_Event &event) const;
    /** Upload the actual ThorVG buffer and present it, measuring both intervals separately. */
    bool present(const engine::Renderer &graphics, engine::FrameTimings &timings);

  private:
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *texture_ = nullptr;
    int textureWidth_ = 0;
    int textureHeight_ = 0;
    int width_ = 0;
    int height_ = 0;
};
} // namespace thorium::platform
