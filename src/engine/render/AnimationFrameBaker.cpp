#include "AnimationFrameBaker.h"
#include <algorithm>
#include <cstring>
#include <thorvg.h>

namespace thorium::engine {
AnimationFrameBaker::AnimationFrameBaker(const std::unordered_map<std::string, std::string> &sources,
                                         const AnimationAssets &metadata)
    : sources_(sources), metadata_(metadata), canvas_(tvg::SwCanvas::gen()) {}
AnimationFrameBaker::~AnimationFrameBaker() {
    if (canvas_) {
        canvas_->sync();
        canvas_->remove();
    }
    animations_.clear();
}
AnimationFrameHandle AnimationFrameBaker::bake(const AnimationFrameKey &key) {
    const std::unordered_map<std::string, std::string>::const_iterator source = sources_.find(key.asset);
    const AnimationAssets::const_iterator info = metadata_.find(key.asset);
    if (!canvas_ || source == sources_.end() || info == metadata_.end()) {
        error_ = "Missing frame-baker source: " + key.asset;
        return {};
    }
    canvas_->sync();
    canvas_->remove();
    const AnimationRasterLayout layout =
        animationRasterLayout(key.size, key.padding, info->second.width, info->second.height);
    if (!layout.bytes()) {
        error_ = "Invalid frame-baker layout: " + key.asset;
        return {};
    }
    std::unique_ptr<tvg::Animation> &animation = animations_[key.asset];
    if (!animation) {
        animation.reset(tvg::Animation::gen());
        if (!animation || animation->picture()->load(source->second.c_str(),
                                                     static_cast<std::uint32_t>(source->second.size()),
                                                     "lottie", nullptr, true) != tvg::Result::Success) {
            error_ = "Could not parse frame-baker source: " + key.asset;
            return {};
        }
    }
    const std::shared_ptr<AnimationRasterFrame> result = std::make_shared<AnimationRasterFrame>();
    result->layout = layout;
    result->pixels.resize(layout.bytes() / sizeof(std::uint32_t));
    if (canvas_->target(result->pixels.data(), layout.width, layout.width, layout.height,
                        tvg::ColorSpace::ABGR8888) != tvg::Result::Success) {
        error_ = "Invalid frame-baker surface: " + key.asset;
        return {};
    }
    animation->frame(key.frame);
    animation->picture()->size(static_cast<float>(layout.coreWidth), static_cast<float>(layout.coreHeight));
    animation->picture()->translate(static_cast<float>(layout.padding), static_cast<float>(layout.padding));
    if (canvas_->add(animation->picture()) != tvg::Result::Success ||
        canvas_->draw(true) != tvg::Result::Success || canvas_->sync() != tvg::Result::Success) {
        error_ = "Could not bake animation frame: " + key.asset;
        return {};
    }
    canvas_->remove();
    int left = layout.width, top = layout.height, right = -1, bottom = -1;
    for (int y = 0; y < layout.height; ++y)
        for (int x = 0; x < layout.width; ++x)
            if ((result->pixels[static_cast<std::size_t>(y * layout.width + x)] >> 24) != 0) {
                left = std::min(left, x);
                top = std::min(top, y);
                right = std::max(right, x);
                bottom = std::max(bottom, y);
            }
    if (right < 0) {
        left = top = right = bottom = 0;
    }
    // Keep one transparent texel around visible content for bilinear edge sampling.
    left = std::max(0, left - 1);
    top = std::max(0, top - 1);
    right = std::min(layout.width - 1, right + 1);
    bottom = std::min(layout.height - 1, bottom + 1);
    const int width = right - left + 1, height = bottom - top + 1;
    for (int y = 0; y < height; ++y)
        std::memmove(result->pixels.data() + y * width,
                     result->pixels.data() + (top + y) * layout.width + left,
                     static_cast<std::size_t>(width) * sizeof(std::uint32_t));
    result->pixels.resize(static_cast<std::size_t>(width) * height);
    result->layout.width = width;
    result->layout.height = height;
    result->layout.offsetX += left;
    result->layout.offsetY += top;
    return result;
}
const std::string &AnimationFrameBaker::error() const {
    return error_;
}
std::size_t AnimationFrameBaker::sources() const {
    return animations_.size();
}
} // namespace thorium::engine
