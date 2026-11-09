#include "Renderer.h"
#include "AnimationFrameBaker.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <sstream>
#include <thorvg.h>
#include <thorvg_lottie.h>
#include <utility>

namespace thorium::engine {
void Renderer::fail(const std::string &context) {
    if (error.empty())
        error = context;
}
Renderer::Renderer(const RenderAssets &assets, AnimationCacheConfig cache)
    : cacheConfig_(std::move(cache)), frameCache_(cacheConfig_.budgetBytes) {
    if (tvg::Initializer::init(0) != tvg::Result::Success) {
        fail("ThorVG initialization failed");
        return;
    }
    initialized_ = true;
    canvas_.reset(tvg::SwCanvas::gen());
    if (!canvas_) {
        fail("ThorVG canvas creation failed");
        return;
    }
    if (!assets.font.empty() && tvg::Text::load(assets.font.c_str()) != tvg::Result::Success)
        fail("Could not load font: " + assets.font);
    for (const std::pair<const std::string, std::string> &asset : assets.animations) {
        std::ifstream input(asset.second);
        std::string source{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        if (source.empty())
            fail("Could not load animation: " + asset.second);
        else {
            const std::unique_ptr<tvg::LottieAnimation> probe{tvg::LottieAnimation::gen()};
            if (!probe || probe->picture()->load(source.c_str(), static_cast<std::uint32_t>(source.size()),
                                                 "lottie", nullptr, true) != tvg::Result::Success) {
                fail("Could not parse animation: " + asset.first);
                continue;
            }
            AnimationInfo info{probe->totalFrame(), probe->duration(), {}};
            probe->picture()->size(&info.width, &info.height);
            if (!std::isfinite(info.frames) || info.frames <= 0 || !std::isfinite(info.duration) ||
                info.duration <= 0) {
                fail("Invalid animation metadata: " + asset.first);
                continue;
            }
            for (std::uint32_t index = 0; index < probe->markersCnt(); ++index) {
                float begin = 0, end = 0;
                const char *name = probe->marker(index, &begin, &end);
                if (name)
                    info.markers.emplace(name, FrameRange{begin, end});
            }
            animationRevisions_.emplace(asset.first, animationRevision(source));
            animationAssets_.emplace(asset.first, std::move(info));
            animationSources_.emplace(asset.first, std::move(source));
        }
    }
    if (!std::isfinite(cacheConfig_.framesPerSecond) || cacheConfig_.framesPerSecond < 1 ||
        cacheConfig_.framesPerSecond > 120 || !std::isfinite(cacheConfig_.padding) ||
        cacheConfig_.padding < 0 || cacheConfig_.padding > 1)
        fail("Invalid animation cache configuration");
    if (error.empty())
        baker_ = std::make_unique<AnimationFrameBaker>(animationSources_, animationAssets_);
}
Renderer::~Renderer() {
    if (canvas_) {
        canvas_->sync();
        canvas_->remove();
    }
    for (const std::pair<const std::string, tvg::Paint *> &entry : textCache_)
        entry.second->unref();
    textCache_.clear();
    clearSprites(true);
    animations_.clear();
    animationPool_.clear();
    baker_.reset();
    canvas_.reset();
    if (initialized_)
        tvg::Initializer::term();
}
bool Renderer::begin(int logicalWidth, int logicalHeight, float density) {
    if (!error.empty() || !canvas_)
        return false;
    canvas_->sync();
    canvas_->remove();
    // Text geometry depends on viewport coordinates. Release old geometry before
    // rebuilding after a resize, and bound growth from changing HUD values.
    if (width != logicalWidth || height != logicalHeight || textCache_.size() > 1024) {
        for (const std::pair<const std::string, tvg::Paint *> &entry : textCache_)
            entry.second->unref();
        textCache_.clear();
    }
    usedAnimations_.clear();
    for (std::pair<const AnimationRasterFrame *const, SpritePool> &entry : sprites_)
        entry.second.used = 0;
    width = std::max(1, logicalWidth);
    height = std::max(1, logicalHeight);
    density_ = std::clamp(density, 1.0F, 1.5F);
    if (static_cast<float>(width) * height * density_ * density_ > 2200000.0F)
        density_ = std::sqrt(2200000.0F / (width * height));
    const int nextWidth = static_cast<int>(width * density_);
    const int nextHeight = static_cast<int>(height * density_);
    if (pixelWidth != nextWidth || pixelHeight != nextHeight) {
        pixelWidth = nextWidth;
        pixelHeight = nextHeight;
        pixels.assign(static_cast<std::size_t>(pixelWidth) * pixelHeight, 0);
    }
    if (canvas_->target(pixels.data(), pixelWidth, pixelWidth, pixelHeight, tvg::ColorSpace::ABGR8888S) !=
        tvg::Result::Success) {
        fail("Invalid ThorVG output surface");
        return false;
    }
    scene_ = tvg::Scene::gen();
    scene_->scale(density_);
    if (canvas_->add(scene_) != tvg::Result::Success) {
        fail("Could not attach ThorVG scene");
        return false;
    }
    return true;
}
bool Renderer::finish() {
    if (!parents_.empty()) {
        fail("Unclosed ThorVG drawing layer");
        return false;
    }
    if (!error.empty() || !scene_)
        return false;
    if (canvas_->draw(true) != tvg::Result::Success || canvas_->sync() != tvg::Result::Success) {
        fail("ThorVG frame draw failed");
        return false;
    }
    for (std::map<AnimationInstance, std::unique_ptr<tvg::Animation>>::iterator it = animations_.begin();
         it != animations_.end();) {
        if (!usedAnimations_.count(it->first)) {
            std::vector<std::unique_ptr<tvg::Animation>> &pool = animationPool_[it->first.first];
            if (pool.size() < 32 && pooledAnimations_ < 128) {
                pool.push_back(std::move(it->second));
                ++pooledAnimations_;
            }
            it = animations_.erase(it);
        } else
            ++it;
    }
    clearSprites(false);
    return true;
}
void Renderer::rect(Rect bounds, Color color, float radius) {
    tvg::Shape *shape = tvg::Shape::gen();
    shape->appendRect(bounds.x, bounds.y, bounds.w, bounds.h, radius, radius);
    shape->fill(color.r, color.g, color.b, color.a);
    scene_->add(shape);
}
void Renderer::circle(ScreenPoint center, float radius, Color color) {
    tvg::Shape *shape = tvg::Shape::gen();
    shape->appendCircle(center.x, center.y, radius, radius);
    shape->fill(color.r, color.g, color.b, color.a);
    scene_->add(shape);
}
void Renderer::gradientRect(Rect bounds, Color top, Color bottom, float radius) {
    tvg::Shape *shape = tvg::Shape::gen();
    shape->appendRect(bounds.x, bounds.y, bounds.w, bounds.h, radius, radius);
    tvg::LinearGradient *gradient = tvg::LinearGradient::gen();
    gradient->linear(bounds.x, bounds.y, bounds.x, bounds.y + bounds.h);
    const tvg::Fill::ColorStop stops[2] = {{0.0F, top.r, top.g, top.b, top.a},
                                           {1.0F, bottom.r, bottom.g, bottom.b, bottom.a}};
    gradient->colorStops(stops, 2);
    shape->fill(gradient);
    scene_->add(shape);
}
void Renderer::beginClip(Rect bounds, std::uint8_t opacity) {
    tvg::Scene *layer = tvg::Scene::gen();
    tvg::Shape *clip = tvg::Shape::gen();
    clip->appendRect(bounds.x, bounds.y, bounds.w, bounds.h, 0, 0);
    clip->fill(255, 255, 255);
    layer->clip(clip);
    layer->opacity(opacity);
    scene_->add(layer);
    parents_.push_back(scene_);
    scene_ = layer;
}
void Renderer::endClip() {
    if (parents_.empty()) {
        fail("Missing ThorVG drawing layer");
        return;
    }
    scene_ = parents_.back();
    parents_.pop_back();
}
void Renderer::polygon(const std::vector<ScreenPoint> &points, Color color) {
    if (points.empty())
        return;
    tvg::Shape *shape = tvg::Shape::gen();
    shape->moveTo(points.front().x, points.front().y);
    for (std::size_t index = 1; index < points.size(); ++index)
        shape->lineTo(points[index].x, points[index].y);
    shape->close();
    shape->fill(color.r, color.g, color.b, color.a);
    scene_->add(shape);
}
void Renderer::line(const std::vector<ScreenPoint> &points, float stroke, Color color) {
    if (points.size() < 2)
        return;
    tvg::Shape *shape = tvg::Shape::gen();
    shape->moveTo(points[0].x, points[0].y);
    for (std::size_t index = 1; index < points.size(); ++index)
        shape->lineTo(points[index].x, points[index].y);
    shape->fill(0, 0, 0, 0);
    shape->strokeWidth(stroke);
    shape->strokeFill(color.r, color.g, color.b, color.a);
    shape->strokeCap(tvg::StrokeCap::Round);
    shape->strokeJoin(tvg::StrokeJoin::Round);
    scene_->add(shape);
}
void Renderer::text(const std::string &value, ScreenPoint position, float size, Color color, float align,
                    float maxWidth) {
    if (value.empty())
        return;
    std::ostringstream key;
    key << value << '|' << position.x << '|' << position.y << '|' << size << '|' << static_cast<int>(color.r)
        << ',' << static_cast<int>(color.g) << ',' << static_cast<int>(color.b) << ','
        << static_cast<int>(color.a) << '|' << align << '|' << maxWidth;
    tvg::Paint *paint = nullptr;
    const std::unordered_map<std::string, tvg::Paint *>::iterator found = textCache_.find(key.str());
    if (found != textCache_.end())
        paint = found->second;
    else {
        tvg::Text *text = tvg::Text::gen();
        text->font(nullptr);
        text->size(size * 0.75F);
        text->text(value.c_str());
        text->fill(color.r, color.g, color.b);
        text->opacity(color.a);
        text->align(align, 0.0F);
        if (maxWidth > 0) {
            text->layout(maxWidth, 0);
            text->wrap(tvg::TextWrap::Ellipsis);
        }
        text->translate(position.x - maxWidth * align, position.y);
        text->ref();
        paint = text;
        textCache_.emplace(key.str(), paint);
    }
    scene_->add(paint);
}
} // namespace thorium::engine
