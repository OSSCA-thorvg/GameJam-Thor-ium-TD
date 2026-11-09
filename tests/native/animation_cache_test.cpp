#include "engine/render/Renderer.h"
#include "game/presentation/ActorAnimator.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace thorium::engine;
namespace {
int checks = 0;
/** Keep ownership and rendering assertions enabled in release builds. */
void require(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Build a tiny immutable fake raster to test cache policy independently of ThorVG. */
AnimationFrameHandle fakeFrame(std::uint32_t value) {
    const std::shared_ptr<AnimationRasterFrame> frame = std::make_shared<AnimationRasterFrame>();
    frame->layout = {4, 1, 0, 4, 1};
    frame->pixels.assign(4, value);
    return frame;
}
/** Verify sharing, revisions, LRU, pins, capacity and failed factories. */
void cacheContracts() {
    AnimationFrameCache cache(32);
    int factories = 0;
    const AnimationFrameCache::Factory make = [&factories]() -> AnimationFrameHandle {
        ++factories;
        return fakeFrame(9);
    };
    const AnimationFrameKey a{"body", 1, 0, 4, 0}, b{"body", 1, 1, 4, 0}, c{"body", 2, 0, 4, 0};
    AnimationFrameHandle first = cache.acquire(a, 16, make);
    AnimationFrameHandle same = cache.acquire(a, 16, make);
    require(first && first == same && factories == 1,
            "many consumers share immutable pixels without repeating the factory");
    same.reset();
    AnimationFrameHandle second = cache.acquire(b, 16, make);
    second.reset();
    AnimationFrameHandle revised = cache.acquire(c, 16, make);
    require(revised && revised != first && factories == 3 && cache.stats().evictions == 1,
            "a source revision cannot reuse stale pixels and eviction preserves pinned consumers");
    require(cache.stats().bytes == 32 && cache.stats().pinnedEntries == 2,
            "resident bytes include all pinned frames");
    require(!cache.acquire(b, 16, make) && factories == 3 && cache.stats().bytes == 32,
            "fully pinned cache rejects before allocating beyond its budget");
    cache.clearUnused();
    require(first->pixels[0] == 9 && revised->pixels[0] == 9 && cache.stats().entries == 2,
            "clearing unused data cannot invalidate active draw handles");
    first.reset();
    cache.clearUnused();
    require(cache.stats().entries == 1 && cache.stats().bytes == 16, "unpinned frames become evictable");
    revised.reset();
    cache.clearUnused();
    require(cache.stats().bytes == 0, "all pixel ownership is released after consumers retire");
    AnimationFrameKey invalid = a;
    invalid.frame = std::numeric_limits<float>::quiet_NaN();
    require(!cache.acquire(invalid, 16, make) && !cache.acquire(a, 64, make) && factories == 3,
            "invalid keys and oversized requests cannot run factories");
    require(!cache.acquire(a, 16, []() -> AnimationFrameHandle { return {}; }) && cache.stats().bytes == 0,
            "failed factories leave no half-registered entries");
    require(!cache.acquire(a, 16,
                           []() -> AnimationFrameHandle { return std::make_shared<AnimationRasterFrame>(); }),
            "empty pixel buffers cannot become successful cache entries");
    require(animationRevision("one") == animationRevision("one") &&
                animationRevision("one") != animationRevision("two"),
            "source content hashes are stable and distinguish revisions");
}
/** Protect endpoint sampling, physical size selection and fixed non-square pivots. */
void samplingContracts() {
    const AnimationInfo info{75, 2.5F, {}};
    require(animationSampleCount({45, 57}, info, 30) == 12 && animationSampleCount({57, 75}, info, 30) == 18,
            "source-sized clips do not gain samples through floating-point rounding");
    require(sampledAnimationFrame({"body", 45, {45, 57}}, info, 24) == 45 &&
                sampledAnimationFrame({"body", 56, {45, 57}}, info, 24) == 56 &&
                sampledAnimationFrame({"body", 74, {57, 75}}, info, 24) == 74,
            "lower cache FPS preserves exact first and last clip frames");
    const std::optional<float> middle = sampledAnimationFrame({"body", 50.3F, {45, 57}}, info, 24);
    require(middle && *middle >= 45 && *middle <= 56, "sampled attacks never cross into an adjacent state");
    require(!sampledAnimationFrame({"body", 58, {45, 57}}, info, 30), "out-of-clip requests are rejected");
    require(!sampledAnimationFrame({"body", 50, {57, 45}}, info, 30) &&
                !sampledAnimationFrame({"body", 50, {std::numeric_limits<float>::quiet_NaN(), 57}}, info, 30),
            "malformed clip intervals cannot fall back to the entire animation");
    require(animationRasterSize(60, {}) == 64 && !animationRasterSize(1000, {}),
            "physical resolutions choose a bounded bucket and large images keep the vector path");
    const AnimationRasterLayout layout = animationRasterLayout(64, 8, 200, 100);
    require(layout.coreWidth == 64 && layout.coreHeight == 32 && layout.width == 80 && layout.height == 48 &&
                layout.bytes() == 15360,
            "transparent padding preserves an asymmetric source artboard rather than cropping each pose");
    require(animationRasterLayout(-1, 0, 100, 100).bytes() == 0, "invalid raster dimensions cannot allocate");
}
/** Exercise real shared raw-image ownership across instances, frame switches and mode switches. */
void rendererContracts(const std::string &path) {
    AnimationCacheConfig config;
    config.sizes = {64, 96};
    Renderer renderer({"", {{"body", path}}}, config);
    renderer.setAnimationBackend(AnimationBackend::SharedFrames);
    require(renderer.begin(600, 100, 1), "begin first shared-frame test");
    for (std::uint32_t index = 0; index < 50; ++index)
        renderer.animation({"body", 0, {0, 45}}, index + 1, {index * 12.0F, 10, 10, 10});
    require(renderer.finish(), "many distinct raw-image paints render a shared buffer");
    const AnimationRenderStats first = renderer.animationStats();
    require(first.cache.bakes == 1 && first.cache.pinnedEntries == 1 && first.spriteInstances == 50 &&
                first.vectorInstances == 0 && first.bakerSources == 1,
            "fifty actors require one baked pose and one source parsing instead of fifty animations");
    const std::vector<std::uint32_t> pixels = renderer.pixels;
    require(renderer.begin(600, 100, 1), "begin shared pose replacement");
    for (std::uint32_t index = 0; index < 50; ++index)
        renderer.animation({"body", 22, {0, 45}}, index + 1, {index * 12.0F, 10, 10, 10});
    require(renderer.finish() && renderer.pixels != pixels && renderer.animationStats().cache.bakes == 2,
            "rebound raw pictures display the new pose without sharing mutable transforms");
    renderer.setAnimationBackend(AnimationBackend::Vector);
    require(renderer.begin(600, 100, 1), "begin vector comparison frame");
    renderer.animation({"body", 22, {0, 45}}, 1, {20, 10, 48, 48});
    require(renderer.finish() && renderer.animationStats().spriteInstances == 0 &&
                renderer.animationStats().cache.pinnedEntries == 0,
            "mode switching releases raw paints before making their pixels evictable");
    renderer.setAnimationBackend(AnimationBackend::SharedFrames);
    require(renderer.begin(600, 100, 1), "return to shared cache");
    renderer.animation({"body", 22, {0, 45}}, 1, {20, 10, 48, 48});
    require(renderer.finish() && renderer.animationStats().cache.bakes == 2 &&
                renderer.animationStats().vectorInstances == 0,
            "comparison mode preserves reusable pixel cache while dropping unused vector instances");
    require(renderer.begin(600, 100, 1.5F), "begin a higher physical density");
    renderer.animation({"body", 22, {0, 45}}, 1, {20, 10, 48, 48});
    require(renderer.finish() && renderer.animationStats().cache.bakes == 3,
            "DPR changes choose a separate raster bucket instead of scaling an undersized cache");
    require(renderer.begin(600, 100, 1), "begin oversized image request");
    renderer.animation({"body", 22, {0, 45}}, 1, {20, 10, 120, 120});
    require(renderer.finish() && renderer.animationStats().fallbacks == 1,
            "oversized art remains available through an explicit vector fallback");
}
/** Keep repeated spawn bursts from repeating source parsing or retaining every past actor. */
void poolContracts(const std::string &path) {
    Renderer renderer({"", {{"body", path}}});
    require(renderer.begin(640, 100, 1), "begin a vector spawn burst");
    for (std::uint32_t id = 1; id <= 40; ++id)
        renderer.animation({"body", 74, {57, 75}}, id, {id * 12.0F, 10, 10, 10});
    require(renderer.finish(), "finish old vector instances");
    require(renderer.begin(640, 100, 1) && renderer.finish(), "retire a complete spawn burst");
    require(renderer.animationStats().pooledVectors == 32 && renderer.animationStats().vectorInstances == 0,
            "retired animations are pooled to a bounded count instead of retaining every actor");
    require(renderer.begin(640, 100, 1), "begin fresh identities using pooled animation objects");
    for (std::uint32_t id = 101; id <= 132; ++id)
        renderer.animation({"body", 0, {0, 45}}, id, {(id - 101) * 12.0F, 10, 10, 10});
    require(renderer.finish() && renderer.animationStats().vectorParses == 40 &&
                renderer.animationStats().poolHits == 32,
            "new identities reuse sources and replace the former death pose with fresh movement samples");
    require(std::any_of(renderer.pixels.begin(), renderer.pixels.end(),
                        [](std::uint32_t pixel) { return pixel != 0; }),
            "reused death instances do not stay invisible when a new actor appears");
}
/** Cycle multiple independent poses long enough to expose stale image-loader surfaces. */
void sustainedPoseContracts(const std::string &path) {
    AnimationCacheConfig config;
    config.sizes = {100};
    Renderer renderer({"", {{"body", path}}}, config);
    Renderer reference({"", {{"body", path}}}, config);
    renderer.setAnimationBackend(AnimationBackend::SharedFrames);
    for (int step = 0; step < 120; ++step) {
        require(renderer.begin(320, 120, 1) && reference.begin(320, 120, 1), "begin sustained pose frame");
        renderer.rect({0, 0, 320, 120}, {18, 34, 43});
        reference.rect({0, 0, 320, 120}, {18, 34, 43});
        for (std::uint32_t actor = 0; actor < 3; ++actor) {
            const float frame = static_cast<float>((step + actor * 13) % 45);
            const AnimationSample sample{"body", frame, {0, 45}};
            const Rect bounds{actor * 100.0F, 10, 100, 100};
            renderer.animation(sample, actor + 1, bounds);
            reference.animation(sample, actor + 1, bounds);
        }
        require(renderer.finish() && reference.finish() && renderer.animationStats().spriteInstances == 3,
                "independent frame changes retain valid immutable image bindings");
        if (step % 10 == 0) {
            double difference = 0;
            std::size_t foreground = 0;
            for (std::size_t index = 0; index < reference.pixels.size(); ++index) {
                if (reference.pixels[index] == 0xff2b2212U && renderer.pixels[index] == 0xff2b2212U)
                    continue;
                ++foreground;
                for (int channel = 0; channel < 3; ++channel)
                    difference +=
                        std::abs(static_cast<int>((reference.pixels[index] >> (channel * 8)) & 255) -
                                 static_cast<int>((renderer.pixels[index] >> (channel * 8)) & 255));
            }
            require(foreground > 0 && difference / (foreground * 3) < 2,
                    "changing cached poses match current vector pixels instead of retaining an old surface");
        }
    }
}
/** Compare real pixels at identical sampling, scale and background to catch alpha/pivot errors. */
void pixelContracts(const std::string &path) {
    AnimationCacheConfig config;
    config.sizes = {100};
    Renderer renderer({"", {{"body", path}}}, config);
    require(renderer.begin(160, 160, 1), "begin parity reference");
    renderer.rect({0, 0, 160, 160}, {18, 34, 43});
    renderer.animation({"body", 49, {45, 57}}, 1, {30, 20, 100, 100});
    require(renderer.finish(), "render vector parity reference");
    const std::vector<std::uint32_t> reference = renderer.pixels;
    renderer.setAnimationBackend(AnimationBackend::SharedFrames);
    require(renderer.begin(160, 160, 1), "begin cached parity output");
    renderer.rect({0, 0, 160, 160}, {18, 34, 43});
    renderer.animation({"body", 49, {45, 57}}, 1, {30, 20, 100, 100});
    require(renderer.finish(), "render cached parity output");
    double error = 0;
    for (std::size_t index = 0; index < reference.size(); ++index)
        for (int channel = 0; channel < 3; ++channel)
            error += std::abs(static_cast<int>((reference[index] >> (channel * 8)) & 255) -
                              static_cast<int>((renderer.pixels[index] >> (channel * 8)) & 255));
    require(error / (reference.size() * 3) < 1.5,
            "fixed pivots and premultiplied alpha preserve vector appearance at native raster size");
    const AnimationInfo &info = renderer.animationAssets().at("body");
    const std::optional<AnimationClip> death = animationClip("body", info, "death", Playback::Once);
    require(death && renderer.prewarmAnimation(*death), "one-shot frames can be prepared before combat");
    require(renderer.begin(160, 160, 1), "begin exact death endpoint");
    renderer.animation({"body", 74, death->frames}, 1, {30, 20, 100, 100});
    require(renderer.finish() && std::all_of(renderer.pixels.begin(), renderer.pixels.end(),
                                             [](std::uint32_t pixel) { return pixel == 0; }),
            "cached death finishes transparent rather than using an earlier sampled frame");
}
/** Verify stable loop variation, pause and one-shot timing stay independent of cache policy. */
void phaseContracts() {
    using namespace thorium::presentation;
    const float phase = actorLoopPhase(17, 99);
    require(phase == actorLoopPhase(17, 99) && phase != actorLoopPhase(18, 99) && phase >= 0 && phase < 1,
            "phase variation is reproducible and independent across actor identities");
    AnimationPlayer player;
    const AnimationClip move{"body", {0, 45}, 1.5F, Playback::Loop};
    player.play(move);
    player.setLoopPhase(.5F);
    require(player.sample()->frame == 22 && player.elapsed() == 0,
            "loop phase affects sampling without changing local time");
    player.pause();
    player.advance(.5F);
    require(player.sample()->frame == 22, "paused phase does not drift");
    require(!player.setLoopPhase(1) && !player.setLoopPhase(-.1F),
            "invalid phases preserve current variation");
    player.play({"body", {45, 57}, .4F, Playback::Once});
    require(player.sample()->frame == 45,
            "attack one-shots start at the committed event despite loop variation");
    player.play(move);
    require(player.sample()->frame == 22, "returning from attack retains the actor's loop phase");
}
} // namespace
/** Run ownership, visual parity and phase tests against the actual pinned ThorVG adapter. */
int main() {
    const std::string path = std::string(THORIUM_CONTENT_DIR) + "/animations/oreling.json";
    cacheContracts();
    samplingContracts();
    rendererContracts(path);
    poolContracts(path);
    sustainedPoseContracts(path);
    pixelContracts(path);
    phaseContracts();
    std::cout << checks << " shared-frame/phase checks passed\n";
}
