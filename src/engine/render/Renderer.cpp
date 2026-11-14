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
    for (const std::pair<const std::string, std::string> &asset : assets.pictures) {
        tvg::Picture *picture = tvg::Picture::gen();
        if (!picture || picture->load(asset.second.c_str()) != tvg::Result::Success) {
            fail("Could not load picture: " + asset.second);
            if (picture)
                tvg::Paint::rel(picture);
            continue;
        }
        picture->ref();
        pictures_.emplace(asset.first, picture);
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
    for (const std::pair<const std::string, tvg::Picture *> &entry : pictures_)
        entry.second->unref();
    pictures_.clear();
    clearSprites(true);
    layers_.clear();
    layerCanvas_.reset();
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
    backdropUsed_ = false;
    for (std::pair<const AnimationRasterFrame *const, SpritePool> &entry : sprites_)
        entry.second.used = 0;
    width = std::max(1, logicalWidth);
    height = std::max(1, logicalHeight);
    density_ = std::clamp(density, 1.0F, MaxDensity);
    if (static_cast<float>(width) * height * density_ * density_ > 2200000.0F)
        density_ = std::sqrt(2200000.0F / (width * height));
    const int nextWidth = static_cast<int>(width * density_);
    const int nextHeight = static_cast<int>(height * density_);
    if (pixelWidth != nextWidth || pixelHeight != nextHeight) {
        pixelWidth = nextWidth;
        pixelHeight = nextHeight;
        pixels.assign(static_cast<std::size_t>(pixelWidth) * pixelHeight, 0);
        backdropValid_ = false;
    }
    if (canvas_->target(pixels.data(), pixelWidth, pixelWidth, pixelHeight,
                        opaqueTarget_ ? tvg::ColorSpace::ABGR8888 : tvg::ColorSpace::ABGR8888S) !=
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
    // Over an opaque backdrop every pixel stays opaque, where premultiplied and straight alpha are equal, so
    // the final conversion to straight alpha is skipped.
    if (backdropUsed_ != opaqueTarget_) {
        opaqueTarget_ = backdropUsed_;
        if (canvas_->target(pixels.data(), pixelWidth, pixelWidth, pixelHeight,
                            opaqueTarget_ ? tvg::ColorSpace::ABGR8888 : tvg::ColorSpace::ABGR8888S) !=
            tvg::Result::Success) {
            fail("Invalid ThorVG output surface");
            return false;
        }
    }
    if (backdropUsed_)
        std::copy(backdropPixels_.begin(), backdropPixels_.end(), pixels.begin());
    if (canvas_->draw(!backdropUsed_) != tvg::Result::Success || canvas_->sync() != tvg::Result::Success) {
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
bool Renderer::rasterize(tvg::Scene *content, std::vector<std::uint32_t> &target, int width, int height) {
    if (!layerCanvas_)
        layerCanvas_.reset(tvg::SwCanvas::gen());
    if (!layerCanvas_ ||
        layerCanvas_->target(target.data(), width, width, height, tvg::ColorSpace::ABGR8888) !=
            tvg::Result::Success ||
        layerCanvas_->add(content) != tvg::Result::Success) {
        tvg::Paint::rel(content);
        fail("Could not prepare a cached layer");
        return false;
    }
    const bool drawn =
        layerCanvas_->draw(true) == tvg::Result::Success && layerCanvas_->sync() == tvg::Result::Success;
    layerCanvas_->remove();
    if (!drawn)
        fail("Could not rasterize a cached layer");
    return drawn;
}
void Renderer::backdrop(std::uint64_t key, const std::function<void()> &draw) {
    if (!error.empty() || !scene_)
        return;
    key = LayerKey{}.add(key).add(density_).add(pixelWidth).add(pixelHeight).value;
    if (!backdropValid_ || backdropKey_ != key) {
        backdropValid_ = false;
        backdropPixels_.assign(pixels.size(), 0);
        tvg::Scene *content = tvg::Scene::gen();
        content->scale(density_);
        parents_.push_back(scene_);
        scene_ = content;
        draw();
        scene_ = parents_.back();
        parents_.pop_back();
        if (!rasterize(content, backdropPixels_, pixelWidth, pixelHeight))
            return;
        backdropKey_ = key;
        backdropValid_ = true;
        ++layerRasterizations_;
    } else
        ++layerReuses_;
    backdropUsed_ = true;
}
Renderer::Layer::~Layer() {
    if (picture)
        picture->unref();
}
void Renderer::cachedLayer(std::uint32_t slot, std::uint64_t key, Rect bounds,
                           const std::function<void()> &draw, ScreenPoint offset) {
    if (!error.empty() || !scene_ || !std::isfinite(bounds.x) || !std::isfinite(bounds.y) ||
        !std::isfinite(bounds.w) || !std::isfinite(bounds.h) || bounds.w <= 0 || bounds.h <= 0)
        return;
    // Bounds on whole physical pixels let the retained image be copied without resampling.
    const int x = static_cast<int>(std::floor(bounds.x * density_));
    const int y = static_cast<int>(std::floor(bounds.y * density_));
    const int right = static_cast<int>(std::ceil((bounds.x + bounds.w) * density_));
    const int bottom = static_cast<int>(std::ceil((bounds.y + bounds.h) * density_));
    const float shiftX = std::isfinite(offset.x) ? std::round(offset.x * density_) : 0.0F;
    const float shiftY = std::isfinite(offset.y) ? std::round(offset.y * density_) : 0.0F;
    std::unique_ptr<Layer> &layer = layers_[slot];
    if (layer && layer->key == key && layer->density == density_ && layer->x == x && layer->y == y &&
        layer->width == right - x && layer->height == bottom - y) {
        layer->picture->translate((x + shiftX) / density_, (y + shiftY) / density_);
        scene_->add(layer->picture);
        ++layerReuses_;
        return;
    }
    // A fresh paint and buffer per rasterization: an image paint stays bound to one immutable buffer.
    layer = std::make_unique<Layer>();
    layer->key = key;
    layer->density = density_;
    layer->x = x;
    layer->y = y;
    layer->width = right - x;
    layer->height = bottom - y;
    layer->pixels.assign(static_cast<std::size_t>(layer->width) * layer->height, 0);
    tvg::Scene *content = tvg::Scene::gen();
    content->scale(density_);
    content->translate(static_cast<float>(-x), static_cast<float>(-y));
    parents_.push_back(scene_);
    scene_ = content;
    draw();
    scene_ = parents_.back();
    parents_.pop_back();
    if (!rasterize(content, layer->pixels, layer->width, layer->height)) {
        layer.reset();
        return;
    }
    layer->picture = tvg::Picture::gen();
    layer->picture->ref();
    if (layer->picture->load(layer->pixels.data(), layer->width, layer->height, tvg::ColorSpace::ABGR8888,
                             false) != tvg::Result::Success) {
        fail("Could not bind cached layer pixels");
        layer.reset();
        return;
    }
    layer->picture->size(layer->width / density_, layer->height / density_);
    layer->picture->translate((x + shiftX) / density_, (y + shiftY) / density_);
    scene_->add(layer->picture);
    ++layerRasterizations_;
}
void Renderer::rect(Rect bounds, Color color, float radius) {
    tvg::Shape *shape = tvg::Shape::gen();
    shape->appendRect(bounds.x, bounds.y, bounds.w, bounds.h, radius, radius);
    shape->fill(color.r, color.g, color.b, color.a);
    scene_->add(shape);
}
void Renderer::picture(const std::string &key, Rect bounds, std::uint8_t opacity) {
    const std::unordered_map<std::string, tvg::Picture *>::const_iterator found = pictures_.find(key);
    if (found == pictures_.end() || !scene_ || !std::isfinite(bounds.x) || !std::isfinite(bounds.y) ||
        !std::isfinite(bounds.w) || !std::isfinite(bounds.h) || bounds.w <= 0 || bounds.h <= 0)
        return;
    tvg::Picture *copy = static_cast<tvg::Picture *>(found->second->duplicate());
    if (!copy)
        return;
    copy->size(bounds.w, bounds.h);
    copy->translate(bounds.x, bounds.y);
    copy->opacity(opacity);
    scene_->add(copy);
}
bool Renderer::hasPicture(const std::string &key) const {
    return pictures_.find(key) != pictures_.end();
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
void Renderer::dashedLine(const std::vector<ScreenPoint> &points, float stroke, Color color, float dash,
                          float gap) {
    if (points.size() < 2 || !(dash > 0) || !(gap > 0))
        return;
    tvg::Shape *shape = tvg::Shape::gen();
    shape->moveTo(points[0].x, points[0].y);
    for (std::size_t index = 1; index < points.size(); ++index)
        shape->lineTo(points[index].x, points[index].y);
    shape->fill(0, 0, 0, 0);
    shape->strokeWidth(stroke);
    shape->strokeFill(color.r, color.g, color.b, color.a);
    shape->strokeCap(tvg::StrokeCap::Butt);
    shape->strokeJoin(tvg::StrokeJoin::Round);
    const float pattern[2] = {dash, gap};
    shape->strokeDash(pattern, 2);
    scene_->add(shape);
}
void Renderer::glow(ScreenPoint center, float radius, Color color) {
    if (!(radius > 0))
        return;
    tvg::Shape *shape = tvg::Shape::gen();
    shape->appendCircle(center.x, center.y, radius, radius);
    tvg::RadialGradient *gradient = tvg::RadialGradient::gen();
    gradient->radial(center.x, center.y, radius, center.x, center.y, 0);
    const tvg::Fill::ColorStop stops[2] = {{0.0F, color.r, color.g, color.b, color.a},
                                           {1.0F, color.r, color.g, color.b, 0}};
    gradient->colorStops(stops, 2);
    shape->fill(gradient);
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
