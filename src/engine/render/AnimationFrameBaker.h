#pragma once
#include "AnimationCache.h"

namespace tvg {
struct SwCanvas;
struct Animation;
} // namespace tvg
namespace thorium::engine {
/** Adapt a small number of mutable ThorVG animations into immutable reusable raster frames. */
class AnimationFrameBaker {
  public:
    /** Source and metadata maps remain owned by Renderer throughout this adapter's lifetime. */
    AnimationFrameBaker(const std::unordered_map<std::string, std::string> &sources,
                        const AnimationAssets &metadata);
    /** Release canvas references before destroying the scratch animation instances. */
    ~AnimationFrameBaker();
    /** Render one fixed artboard with transparent padding into a premultiplied pixel buffer. */
    AnimationFrameHandle bake(const AnimationFrameKey &key);
    /** Read a concrete adapter error without making cache-budget misses fatal. */
    const std::string &error() const;
    /** Count source parsings separately from generated frames. */
    std::size_t sources() const;

  private:
    const std::unordered_map<std::string, std::string> &sources_;
    const AnimationAssets &metadata_;
    std::unique_ptr<tvg::SwCanvas> canvas_;
    std::map<std::string, std::unique_ptr<tvg::Animation>> animations_;
    std::string error_;
};
} // namespace thorium::engine
