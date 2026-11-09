#include "AnimationFrameBaker.h"
#include "Renderer.h"
#include <algorithm>
#include <cmath>
#include <thorvg.h>

namespace thorium::engine {
Renderer::SpriteInstance::SpriteInstance() : picture(tvg::Picture::gen()) {
    picture->ref();
}
Renderer::SpriteInstance::~SpriteInstance() {
    picture->unref();
}
void Renderer::clearSprites(bool all) {
    for (std::map<const AnimationRasterFrame *, SpritePool>::iterator it = sprites_.begin();
         it != sprites_.end();) {
        if (all || it->second.used == 0)
            it = sprites_.erase(it);
        else {
            while (it->second.instances.size() > it->second.used + 4)
                it->second.instances.pop_back();
            ++it;
        }
    }
}
void Renderer::setAnimationBackend(AnimationBackend backend) {
    animationBackend_ = backend;
}
AnimationBackend Renderer::animationBackend() const {
    return animationBackend_;
}
AnimationRenderStats Renderer::animationStats() const {
    AnimationRenderStats result = animationStats_;
    result.cache = frameCache_.stats();
    result.vectorInstances = animations_.size();
    result.pooledVectors = pooledAnimations_;
    for (const std::pair<const AnimationRasterFrame *const, SpritePool> &entry : sprites_)
        result.spriteInstances += entry.second.used;
    result.bakerSources = baker_ ? baker_->sources() : 0;
    return result;
}
AnimationFrameHandle Renderer::rasterFrame(const AnimationSample &sample, int size) {
    const AnimationAssets::const_iterator info = animationAssets_.find(sample.asset);
    if (info == animationAssets_.end() || !baker_)
        return {};
    const std::optional<float> frame =
        sampledAnimationFrame(sample, info->second, cacheConfig_.framesPerSecond);
    if (!frame)
        return {};
    const int padding = static_cast<int>(std::ceil(size * cacheConfig_.padding));
    const AnimationFrameKey key{sample.asset, animationRevisions_.at(sample.asset), *frame, size, padding};
    const AnimationRasterLayout layout =
        animationRasterLayout(size, padding, info->second.width, info->second.height);
    const AnimationFrameHandle result = frameCache_.acquire(
        key, layout.bytes(), [this, &key]() -> AnimationFrameHandle { return baker_->bake(key); });
    if (!result && !baker_->error().empty())
        fail(baker_->error());
    return result;
}
bool Renderer::prewarmAnimation(const AnimationClip &clip, const std::vector<int> &sizes) {
    if (!clip.valid() || !error.empty() || !baker_)
        return false;
    const std::vector<int> &resolutions = sizes.empty() ? cacheConfig_.sizes : sizes;
    const AnimationAssets::const_iterator info = animationAssets_.find(clip.asset);
    if (info == animationAssets_.end())
        return false;
    const int count = animationSampleCount(clip.frames, info->second, cacheConfig_.framesPerSecond);
    if (count == 0)
        return false;
    bool complete = true;
    const std::uint64_t beforeEvictions = frameCache_.stats().evictions;
    for (int size : resolutions) {
        if (size < 1 || size > 1024) {
            complete = false;
            continue;
        }
        for (int index = 0; index < count; ++index) {
            const float progress = count > 1 ? static_cast<float>(index) / (count - 1) : 0;
            const AnimationSample sample{
                clip.asset,
                clip.frames.begin + progress * std::max(0.0F, clip.frames.end - clip.frames.begin - 1),
                clip.frames};
            if (!rasterFrame(sample, size))
                complete = false;
        }
    }
    return complete && frameCache_.stats().evictions == beforeEvictions;
}
void Renderer::vectorAnimation(const AnimationSample &sample, std::uint32_t id, Rect bounds) {
    const AnimationInstance key{sample.asset, id};
    usedAnimations_.insert(key);
    std::unique_ptr<tvg::Animation> &item = animations_[key];
    if (!item) {
        std::vector<std::unique_ptr<tvg::Animation>> &pool = animationPool_[sample.asset];
        if (!pool.empty()) {
            item = std::move(pool.back());
            pool.pop_back();
            --pooledAnimations_;
            ++animationStats_.poolHits;
        }
    }
    if (!item) {
        item.reset(tvg::Animation::gen());
        const std::string &source = animationSources_.at(sample.asset);
        if (!item || item->picture()->load(source.c_str(), static_cast<std::uint32_t>(source.size()),
                                           "lottie", nullptr, true) != tvg::Result::Success) {
            fail("Could not parse animation: " + sample.asset);
            return;
        }
        ++animationStats_.vectorParses;
    }
    item->frame(sample.frame);
    item->picture()->size(bounds.w, bounds.h);
    item->picture()->translate(bounds.x, bounds.y);
    scene_->add(item->picture());
    ++animationStats_.vectorDraws;
}
void Renderer::animation(const AnimationSample &sample, std::uint32_t id, Rect bounds,
                         std::optional<AnimationBackend> backend) {
    const AnimationAssets::const_iterator info = animationAssets_.find(sample.asset);
    if (info == animationAssets_.end()) {
        fail("Unknown animation asset: " + sample.asset);
        return;
    }
    if (!std::isfinite(sample.frame) || sample.frame < 0 || sample.frame >= info->second.frames ||
        !std::isfinite(bounds.x) || !std::isfinite(bounds.y) || !std::isfinite(bounds.w) ||
        !std::isfinite(bounds.h) || bounds.w <= 0 || bounds.h <= 0) {
        fail("Invalid animation frame or bounds: " + sample.asset);
        return;
    }
    if (backend.value_or(animationBackend_) == AnimationBackend::SharedFrames) {
        const std::optional<int> size =
            animationRasterSize(std::max(bounds.w, bounds.h) * density_, cacheConfig_);
        const AnimationFrameHandle frame = size ? rasterFrame(sample, *size) : AnimationFrameHandle{};
        if (frame) {
            SpritePool &pool = sprites_[frame.get()];
            pool.frame = frame;
            if (pool.used == pool.instances.size()) {
                std::unique_ptr<SpriteInstance> fresh = std::make_unique<SpriteInstance>();
                if (fresh->picture->load(frame->pixels.data(), frame->layout.width, frame->layout.height,
                                         tvg::ColorSpace::ABGR8888, false) != tvg::Result::Success) {
                    fail("Could not bind cached animation pixels: " + sample.asset);
                    return;
                }
                fresh->frame = frame;
                pool.instances.push_back(std::move(fresh));
                ++animationStats_.spriteLoads;
            }
            SpriteInstance &sprite = *pool.instances[pool.used++];
            const float scale =
                std::min(bounds.w / frame->layout.coreWidth, bounds.h / frame->layout.coreHeight);
            sprite.picture->size(frame->layout.width * scale, frame->layout.height * scale);
            sprite.picture->translate(bounds.x + frame->layout.offsetX * scale,
                                      bounds.y + frame->layout.offsetY * scale);
            scene_->add(sprite.picture);
            ++animationStats_.cachedDraws;
            return;
        }
        ++animationStats_.fallbacks;
    }
    vectorAnimation(sample, id, bounds);
}
} // namespace thorium::engine
