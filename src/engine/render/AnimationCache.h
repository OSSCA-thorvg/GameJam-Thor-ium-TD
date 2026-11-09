#pragma once
#include "engine/animation/Animation.h"
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace thorium::engine {
/** Render policy can be compared without changing simulation or animation clocks. */
enum class AnimationBackend { Vector, SharedFrames };
/** Physical image sizes and a hard resident pixel budget, independent of game content. */
struct AnimationCacheConfig {
    std::size_t budgetBytes = 32 * 1024 * 1024;
    std::vector<int> sizes{32, 64, 96};
    float framesPerSecond = 30;
    float padding = .125F;
};
/** Immutable source pixels depend on source revision, pose and physical resolution, never actor ID. */
struct AnimationFrameKey {
    std::string asset;
    std::uint64_t revision = 0;
    float frame = 0;
    int size = 0;
    int padding = 0;
    /** Provide deterministic cache ordering for valid, finite keys. */
    bool operator<(const AnimationFrameKey &other) const;
};
/** Fixed source artboard and transparent padding preserve pivots through every pose. */
struct AnimationRasterLayout {
    int coreWidth = 0;
    int coreHeight = 0;
    int padding = 0;
    int width = 0;
    int height = 0;
    int offsetX = 0;
    int offsetY = 0;
    /** Read exact pixel bytes before allocating or evicting anything. */
    std::size_t bytes() const;
};
/** Cached images use premultiplied ABGR8888, matching the adapter's raw-image input. */
struct AnimationRasterFrame {
    AnimationRasterLayout layout;
    std::vector<std::uint32_t> pixels;
    /** Count owned allocation, including retained capacity after transparent-border trimming. */
    std::size_t bytes() const;
};
using AnimationFrameHandle = std::shared_ptr<const AnimationRasterFrame>;
/** CPU cache counters distinguish resident pixels from total process memory. */
struct AnimationCacheStats {
    std::size_t bytes = 0;
    std::size_t entries = 0;
    std::size_t pinnedEntries = 0;
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t bakes = 0;
    std::uint64_t evictions = 0;
    std::uint64_t rejected = 0;
    double bakeMilliseconds = 0;
};
/** Hash source bytes stably across native and WASM, without consuming a gameplay RNG. */
std::uint64_t animationRevision(const std::string &source);
/** Derive a stable finite sample count without adding frames through floating-point rounding. */
int animationSampleCount(FrameRange range, const AnimationInfo &info, float framesPerSecond);
/** Choose a sampling point inside the requested clip, explicitly preserving both endpoints. */
std::optional<float> sampledAnimationFrame(const AnimationSample &sample, const AnimationInfo &info,
                                           float framesPerSecond);
/** Select the smallest configured physical size; oversized requests use the vector path. */
std::optional<int> animationRasterSize(float requiredPixels, const AnimationCacheConfig &config);
/** Calculate one padded raster layout, rejecting invalid or oversized allocations. */
AnimationRasterLayout animationRasterLayout(int size, int padding, float sourceWidth, float sourceHeight);

/** Share immutable frame pixels with LRU eviction; external handles pin in-flight resources. */
class AnimationFrameCache {
  public:
    using Factory = std::function<AnimationFrameHandle()>;
    /** Set a resident pixel budget that is enforced before calling an allocation factory. */
    explicit AnimationFrameCache(std::size_t budgetBytes);
    /** Reuse a pose or create it once; pinned frames are never evicted to satisfy a miss. */
    AnimationFrameHandle acquire(const AnimationFrameKey &key, std::size_t expectedBytes,
                                 const Factory &factory);
    /** Release only unpinned frames, keeping pixels valid for all active draw objects. */
    void clearUnused();
    /** Read current counters and byte ownership. */
    AnimationCacheStats stats() const;

  private:
    struct Entry {
        AnimationFrameHandle frame;
        std::uint64_t used = 0;
    };
    std::map<AnimationFrameKey, Entry> entries_;
    std::size_t budget_;
    std::uint64_t clock_ = 0;
    AnimationCacheStats stats_;
    /** Evict the least-recently-used unpinned entry, or report that every entry is in use. */
    bool evict();
};
} // namespace thorium::engine
