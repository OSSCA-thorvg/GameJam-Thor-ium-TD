#include "VideoOutput.h"

namespace thorium::platform {
VideoOutput::~VideoOutput() {
    close();
}
bool VideoOutput::open(SDL_Window *window, bool vsync) {
    if (renderer_)
        return true;
    renderer_ = SDL_CreateRenderer(window, nullptr);
    if (!renderer_) {
        error = SDL_GetError();
        return false;
    }
    SDL_SetRenderVSync(renderer_, vsync ? 1 : 0);
    return true;
}
void VideoOutput::close() {
    if (texture_)
        SDL_DestroyTexture(texture_);
    texture_ = nullptr;
    if (renderer_)
        SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
    textureWidth_ = textureHeight_ = width_ = height_ = 0;
}
bool VideoOutput::resize(int width, int height) {
    width_ = width;
    height_ = height;
    if (renderer_ &&
        SDL_SetRenderLogicalPresentation(renderer_, width, height, SDL_LOGICAL_PRESENTATION_STRETCH))
        return true;
    error = SDL_GetError();
    return false;
}
bool VideoOutput::convertEvent(SDL_Event &event) const {
    return renderer_ && SDL_ConvertEventToRenderCoordinates(renderer_, &event);
}
bool VideoOutput::present(const engine::Renderer &graphics, engine::FrameTimings &timings) {
    const engine::FrameClock::time_point upload = engine::FrameClock::now();
    if (!texture_ || textureWidth_ != graphics.pixelWidth || textureHeight_ != graphics.pixelHeight) {
        if (texture_)
            SDL_DestroyTexture(texture_);
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                                     graphics.pixelWidth, graphics.pixelHeight);
        if (!texture_) {
            error = SDL_GetError();
            return false;
        }
        SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_NONE);
        textureWidth_ = graphics.pixelWidth;
        textureHeight_ = graphics.pixelHeight;
    }
    if (!SDL_UpdateTexture(texture_, nullptr, graphics.pixels.data(), graphics.pixelWidth * 4)) {
        error = SDL_GetError();
        return false;
    }
    timings.add(engine::FrameStage::Upload, engine::frameMilliseconds(upload));
    const engine::FrameClock::time_point output = engine::FrameClock::now();
    SDL_SetRenderDrawColor(renderer_, 249, 247, 239, 255);
    const SDL_FRect destination{0, 0, static_cast<float>(width_), static_cast<float>(height_)};
    if (!SDL_RenderClear(renderer_) || !SDL_RenderTexture(renderer_, texture_, nullptr, &destination) ||
        !SDL_RenderPresent(renderer_)) {
        error = SDL_GetError();
        return false;
    }
    timings.add(engine::FrameStage::Present, engine::frameMilliseconds(output));
    return true;
}
} // namespace thorium::platform
