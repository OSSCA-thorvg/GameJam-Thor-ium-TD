#pragma once

#include "AnimationCache.h"
#include "engine/animation/Animation.h"
#include "engine/ui/Ui.h"
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace tvg {
struct SwCanvas;
struct Scene;
struct Animation;
struct Paint;
struct Picture;
} // namespace tvg

namespace thorium::engine {
class AnimationFrameBaker;
/** An explicit RGBA drawing color, unrelated to gameplay status. */
struct Color {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a = 255;
};

/** Caller-owned file bindings; the renderer has no knowledge of game content names. */
struct RenderAssets {
    std::string font;
    std::unordered_map<std::string, std::string> animations;
};

/** Read-only render diagnostics distinguish sharing from per-instance vector work. */
struct AnimationRenderStats {
    AnimationCacheStats cache;
    std::size_t vectorInstances = 0;
    std::size_t spriteInstances = 0;
    std::size_t bakerSources = 0;
    std::uint64_t vectorParses = 0;
    std::uint64_t spriteLoads = 0;
    std::uint64_t cachedDraws = 0;
    std::uint64_t vectorDraws = 0;
    std::uint64_t fallbacks = 0;
    std::uint64_t poolHits = 0;
    std::size_t pooledVectors = 0;
};
/** Own ThorVG, fonts, animation instances and a software output surface. */
class Renderer {
  public:
    int width = 0;
    int height = 0;
    int pixelWidth = 0;
    int pixelHeight = 0;
    std::vector<std::uint32_t> pixels;
    std::string error;
    /** Initialize one canvas and caller-supplied assets; report missing assets explicitly. */
    explicit Renderer(const RenderAssets &assets, AnimationCacheConfig cache = {});
    /** Remove scene references before deleting animation and canvas resources. */
    ~Renderer();
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    /** Start a responsive frame with a bounded physical pixel density. */
    bool begin(int logicalWidth, int logicalHeight, float density);
    /** Draw and synchronize the current scene before the platform reads its pixels. */
    bool finish();
    /** Append a rounded rectangle in logical pixels. */
    void rect(Rect bounds, Color color, float radius = 0.0F);
    /** Append a rounded rectangle with a vertical two-stop color gradient. */
    void gradientRect(Rect bounds, Color top, Color bottom, float radius = 0.0F);
    /** Begin an opacity layer clipped to logical bounds; every begin needs one end. */
    void beginClip(Rect bounds, std::uint8_t opacity = 255);
    /** Restore the parent scene after a clipped layer. */
    void endClip();
    /** Append a circle in logical pixels. */
    void circle(ScreenPoint center, float radius, Color color);
    /** Append an arbitrary filled convex or simple polygon. */
    void polygon(const std::vector<ScreenPoint> &points, Color color);
    /** Append a stroked polyline with round ends. */
    void line(const std::vector<ScreenPoint> &points, float width, Color color);
    /** Append cached text; position and alignment do not mutate gameplay. */
    void text(const std::string &value, ScreenPoint position, float size, Color color, float align = 0.0F,
              float maxWidth = 0.0F);
    /** Render one independently timed Lottie instance by stable presentation identity. */
    void animation(const AnimationSample &sample, std::uint32_t id, Rect bounds,
                   std::optional<AnimationBackend> backend = std::nullopt);
    /** Select a rendering path without changing independent animation clocks. */
    void setAnimationBackend(AnimationBackend backend);
    /** Read the selected rendering path for a comparison UI. */
    AnimationBackend animationBackend() const;
    /** Prepare a clip at configured or explicitly supplied physical resolutions during loading. */
    bool prewarmAnimation(const AnimationClip &clip, const std::vector<int> &sizes = {});
    /** Read byte, sharing, parsing and draw counters without exposing mutable buffers. */
    AnimationRenderStats animationStats() const;
    /** Read adapter-resolved source metadata and marker ranges without leaking ThorVG handles. */
    const AnimationAssets &animationAssets() const {
        return animationAssets_;
    }

  private:
    AnimationCacheConfig cacheConfig_;
    AnimationFrameCache frameCache_;
    std::unique_ptr<tvg::SwCanvas> canvas_;
    tvg::Scene *scene_ = nullptr;
    std::vector<tvg::Scene *> parents_;
    std::unordered_map<std::string, tvg::Paint *> textCache_;
    using AnimationInstance = std::pair<std::string, std::uint32_t>;
    std::map<AnimationInstance, std::unique_ptr<tvg::Animation>> animations_;
    std::set<AnimationInstance> usedAnimations_;
    std::map<std::string, std::vector<std::unique_ptr<tvg::Animation>>> animationPool_;
    std::size_t pooledAnimations_ = 0;
    struct SpriteInstance {
        AnimationFrameHandle frame;
        tvg::Picture *picture = nullptr;
        /** Retain an owner reference independent of scene reconstruction. */
        SpriteInstance();
        /** Release the paint before its pinned frame handle. */
        ~SpriteInstance();
    };
    struct SpritePool {
        AnimationFrameHandle frame;
        std::vector<std::unique_ptr<SpriteInstance>> instances;
        std::size_t used = 0;
    };
    std::map<const AnimationRasterFrame *, SpritePool> sprites_;
    AnimationBackend animationBackend_ = AnimationBackend::Vector;
    AnimationRenderStats animationStats_;
    /** Destroy cached raw-image paints before releasing their pinned pixel handles. */
    void clearSprites(bool all);
    /** Resolve or bake one canonical immutable pose at a physical size. */
    AnimationFrameHandle rasterFrame(const AnimationSample &sample, int size);
    /** Draw one vector instance when selected or when the cache cannot satisfy a request. */
    void vectorAnimation(const AnimationSample &sample, std::uint32_t id, Rect bounds);
    std::unordered_map<std::string, std::string> animationSources_;
    AnimationAssets animationAssets_;
    std::unordered_map<std::string, std::uint64_t> animationRevisions_;
    std::unique_ptr<AnimationFrameBaker> baker_;
    float density_ = 1.0F;
    bool initialized_ = false;
    /** Record a ThorVG operation error while permitting a clean shutdown. */
    void fail(const std::string &context);
};
} // namespace thorium::engine
