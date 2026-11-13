#pragma once

#include "AnimationCache.h"
#include "engine/animation/Animation.h"
#include "engine/ui/Ui.h"
#include <cstdint>
#include <functional>
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

/** Order-dependent FNV-1a digest of the values a cached layer is drawn from. */
struct LayerKey {
    std::uint64_t value = 1469598103934665603ULL;
    /** Mix the bytes of one trivially copyable value. */
    template <typename T> LayerKey &add(const T &item) {
        const unsigned char *bytes = reinterpret_cast<const unsigned char *>(&item);
        for (std::size_t index = 0; index < sizeof(T); ++index)
            value = (value ^ bytes[index]) * 1099511628211ULL;
        return *this;
    }
};

/** Caller-owned file bindings; the renderer has no knowledge of game content names. */
struct RenderAssets {
    std::string font;
    std::unordered_map<std::string, std::string> animations;
    /** Still vector picture files by logical key; each file is parsed once. */
    std::unordered_map<std::string, std::string> pictures;
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
    /** Most physical pixels per logical pixel a frame renders at, whatever the display offers. */
    static constexpr float MaxDensity = 1.5F;
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
    /** Append a still vector picture stretched to logical bounds; an unregistered key is skipped. Pictures
     * may be drawn inside a cached layer. */
    void picture(const std::string &key, Rect bounds, std::uint8_t opacity = 255);
    /** Check whether a still picture was registered and parsed under a key. */
    bool hasPicture(const std::string &key) const;
    /** Draw a layer from retained pixels while its slot keeps the same key, bounds and density; otherwise
     * record `draw` into a separate scene and rasterize it once into new pixels. Content outside `bounds` is
     * cut off, `draw` must not add text or animations, and `offset` moves the retained image by whole
     * physical pixels without rasterizing it again. */
    void cachedLayer(std::uint32_t slot, std::uint64_t key, Rect bounds, const std::function<void()> &draw,
                     ScreenPoint offset = {});
    /** Start the frame from retained opaque pixels instead of a cleared surface while `key` and the surface
     * size are unchanged; otherwise record `draw` and rasterize it once. `draw` must cover every pixel
     * opaquely and must not add text or animations; later drawing in the frame goes on top. */
    void backdrop(std::uint64_t key, const std::function<void()> &draw);
    /** Read the physical pixels per logical pixel of the current frame. */
    float density() const {
        return density_;
    }
    /** Read how often cached layers were rasterized into new pixels. */
    std::uint64_t layerRasterizations() const {
        return layerRasterizations_;
    }
    /** Read how often cached layers were drawn from retained pixels. */
    std::uint64_t layerReuses() const {
        return layerReuses_;
    }
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
    /** Parsed still pictures; every draw adds a duplicate so the source keeps its own size. */
    std::unordered_map<std::string, tvg::Picture *> pictures_;
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
    /** Retained pixels of one cached layer and the image paint bound to exactly those pixels. */
    struct Layer {
        std::uint64_t key = 0;
        int x = 0, y = 0, width = 0, height = 0;
        float density = 0;
        std::vector<std::uint32_t> pixels;
        tvg::Picture *picture = nullptr;
        /** Release the image paint before its pixels. */
        ~Layer();
    };
    std::map<std::uint32_t, std::unique_ptr<Layer>> layers_;
    /** Opaque pixels copied into the surface before drawing when this frame chose a backdrop. */
    std::vector<std::uint32_t> backdropPixels_;
    std::uint64_t backdropKey_ = 0;
    bool backdropValid_ = false;
    bool backdropUsed_ = false;
    /** Whether the output surface is targeted as premultiplied because the frame starts opaque. */
    bool opaqueTarget_ = false;
    /** Rasterize a recorded scene into caller-owned premultiplied pixels with the offscreen canvas. */
    bool rasterize(tvg::Scene *content, std::vector<std::uint32_t> &target, int width, int height);
    std::unique_ptr<tvg::SwCanvas> layerCanvas_;
    std::uint64_t layerRasterizations_ = 0;
    std::uint64_t layerReuses_ = 0;
    float density_ = 1.0F;
    bool initialized_ = false;
    /** Record a ThorVG operation error while permitting a clean shutdown. */
    void fail(const std::string &context);
};
} // namespace thorium::engine
