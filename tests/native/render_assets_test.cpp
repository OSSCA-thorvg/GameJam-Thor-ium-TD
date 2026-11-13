#include "engine/render/RenderScale.h"
#include "engine/render/Renderer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

namespace {
int checks = 0;
/** Keep generic renderer integration assertions active in optimized builds. */
void require(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Detect actual drawn pixels in a logical region at the test's unit density. */
bool visible(const thorium::engine::Renderer &renderer, int left, int right) {
    for (int y = 0; y < 40; ++y)
        for (int x = left; x < right; ++x)
            if (renderer.pixels[static_cast<std::size_t>(y * renderer.pixelWidth + x)] != 0)
                return true;
    return false;
}
/** Verify absolute frames select different authored poses and the last death frame disappears. */
void frameContracts(const std::string &source) {
    using namespace thorium::engine;
    Renderer renderer({"", {{"character", source}}});
    require(renderer.begin(100, 100, 1), "begin idle-frame comparison");
    renderer.animation({"character", 0}, 1, {0, 0, 100, 100});
    require(renderer.finish(), "render absolute idle frame");
    const std::vector<std::uint32_t> idle = renderer.pixels;
    require(renderer.begin(100, 100, 1), "begin attack-frame comparison");
    renderer.animation({"character", 49}, 1, {0, 0, 100, 100});
    require(renderer.finish() && renderer.pixels != idle,
            "the same cached instance paints a different authored attack pose");
    require(renderer.begin(100, 100, 1), "begin final death frame");
    renderer.animation({"character", 74}, 1, {0, 0, 100, 100});
    require(renderer.finish() && std::all_of(renderer.pixels.begin(), renderer.pixels.end(),
                                             [](std::uint32_t pixel) { return pixel == 0; }),
            "the last death frame finishes fully transparent without wrapping to idle");
}
/** Draw a translucent ground with antialiased shapes, either directly or through one cached layer. */
void ground(thorium::engine::Renderer &renderer, bool cached, std::uint64_t key, float shift = 0) {
    using namespace thorium::engine;
    const auto shapes = [&renderer, shift]() {
        renderer.gradientRect({10, 10, 100, 60}, {40, 90, 60}, {20, 50, 70, 200}, 8);
        renderer.circle({40 + shift, 40}, 17.3F, {230, 200, 120, 140});
        renderer.line({{12 + shift, 65}, {70 + shift, 22}, {105 + shift, 60}}, 4.5F, {200, 220, 255, 180});
    };
    renderer.rect({0, 0, 120, 80}, {18, 34, 43});
    if (cached)
        renderer.cachedLayer(3, key, {10, 10, 100, 60}, shapes);
    else {
        renderer.beginClip({10, 10, 100, 60});
        shapes();
        renderer.endClip();
    }
}
/** Compare a cached layer with direct drawing at unit and fractional density, and check its reuse rules. */
void layerContracts() {
    using namespace thorium::engine;
    for (float density : {1.0F, 1.5F}) {
        Renderer direct({"", {}}), cached({"", {}});
        require(direct.begin(120, 80, density) && cached.begin(120, 80, density), "begin layer comparison");
        ground(direct, false, 0);
        ground(cached, true, 7);
        require(direct.finish() && cached.finish(), "render direct and cached ground");
        int largest = 0;
        for (std::size_t index = 0; index < direct.pixels.size(); ++index)
            for (int shift = 0; shift < 32; shift += 8)
                largest = std::max(largest, std::abs(static_cast<int>(direct.pixels[index] >> shift & 255) -
                                                     static_cast<int>(cached.pixels[index] >> shift & 255)));
        require(largest <= 2, "a cached layer matches direct drawing within premultiplied rounding");
        const std::vector<std::uint32_t> first = cached.pixels;
        require(cached.begin(120, 80, density), "begin reused layer frame");
        ground(cached, true, 7, 30);
        require(cached.finish() && cached.pixels == first && cached.layerRasterizations() == 1 &&
                    cached.layerReuses() == 1,
                "an unchanged key reuses retained pixels without recording its drawing again");
        require(cached.begin(120, 80, density), "begin changed layer frame");
        ground(cached, true, 8, 30);
        require(cached.finish() && cached.pixels != first && cached.layerRasterizations() == 2,
                "a changed key rasterizes the layer again");
    }
    Renderer moved({"", {}});
    require(moved.begin(120, 80, 1), "begin offset layer frame");
    moved.cachedLayer(0, 1, {0, 0, 20, 20}, [&moved]() { moved.rect({0, 0, 20, 20}, {255, 255, 255}); });
    require(moved.finish() && moved.pixels[0] != 0, "layer paints at its own bounds");
    require(moved.begin(120, 80, 1), "begin shifted layer frame");
    moved.cachedLayer(0, 1, {0, 0, 20, 20}, [&moved]() { moved.rect({0, 0, 20, 20}, {0, 0, 0}); },
                      {30.4F, 0});
    require(moved.finish() && moved.pixels[0] == 0 && moved.pixels[35] != 0 && moved.pixels[29] == 0 &&
                moved.layerRasterizations() == 1,
            "an offset moves retained pixels by whole pixels without rasterizing again");
}
/** The render scale ignores short spikes, steps down under sustained load and climbs back with headroom. */
void scaleContracts() {
    using thorium::engine::RenderScale;
    RenderScale scale;
    const float frame = 1.0F / 60.0F;
    for (int index = 0; index < 5; ++index)
        scale.update(30, frame);
    require(scale.value() == 1.0F, "a short spike keeps full resolution");
    for (int index = 0; index < 40; ++index)
        scale.update(20, frame);
    require(scale.value() == RenderScale::Levels[1],
            "sustained work over the budget steps the scale down once");
    for (int index = 0; index < 30; ++index)
        scale.update(20, frame);
    require(scale.value() == RenderScale::Levels[1], "the scale holds before stepping again");
    for (int index = 0; index < 120; ++index)
        scale.update(20, frame);
    require(scale.value() == RenderScale::Levels[2],
            "continued overload reaches the coarsest step and stops");
    scale.update(std::nan(""), frame);
    scale.update(-1, frame);
    require(scale.value() == RenderScale::Levels[2] && scale.smoothedMs() > 19,
            "invalid samples are ignored");
    for (int index = 0; index < 200; ++index)
        scale.update(6, frame);
    require(scale.value() == RenderScale::Levels[1], "headroom raises the scale one step after the hold");
    for (int index = 0; index < 300; ++index)
        scale.update(6, frame);
    require(scale.value() == 1.0F, "lasting headroom restores full resolution");
}
/** Render authored marker poses and optionally save portable RGBA pixels for visual review. */
void markerContracts(const std::string &source, const std::string &output) {
    using namespace thorium::engine;
    Renderer renderer({std::string(THORIUM_CONTENT_DIR) + "/fonts/NotoSansKR.ttf", {{"character", source}}});
    const AnimationInfo &info = renderer.animationAssets().at("character");
    require(info.frames == 75 && info.duration == 2.5F && info.markers.size() == 4,
            "real Lottie source exposes frame count, duration and four semantic markers");
    const std::array<const char *, 4> markers{{"idle", "move", "attack", "death"}};
    const std::array<const char *, 4> labels{{"IDLE", "MOVE", "ATTACK", "DEATH"}};
    require(renderer.begin(480, 160, 1), "begin semantic pose preview");
    renderer.rect({0, 0, 480, 160}, {18, 34, 43});
    for (std::size_t index = 0; index < markers.size(); ++index) {
        const std::optional<AnimationClip> clip =
            animationClip("character", info, markers[index], Playback::Once);
        require(clip.has_value(), "authored marker resolves against actual adapter metadata");
        AnimationPlayer player;
        player.play(*clip);
        player.advance(clip->duration * (index == 2 ? 1.0F / 3.0F : .5F));
        const float left = static_cast<float>(index) * 120;
        renderer.rect({left + 5, 5, 110, 150}, {28, 49, 57}, 10);
        renderer.text(labels[index], {left + 60, 12}, 15, {239, 221, 172}, .5F, 100);
        renderer.animation(*player.sample(), static_cast<std::uint32_t>(index + 1), {left + 15, 38, 90, 90});
        renderer.text(std::to_string(clip->frames.begin) + " - " + std::to_string(clip->frames.end),
                      {left + 60, 136}, 9, {161, 190, 190}, .5F, 100);
    }
    require(renderer.finish(), "all semantic marker poses render using one shared source asset");
    if (!output.empty()) {
        std::ofstream pixels(output, std::ios::binary);
        for (std::uint32_t pixel : renderer.pixels) {
            const std::array<char, 4> rgba{
                {static_cast<char>(pixel & 255), static_cast<char>((pixel >> 8) & 255),
                 static_cast<char>((pixel >> 16) & 255), static_cast<char>((pixel >> 24) & 255)}};
            pixels.write(rgba.data(), 4);
        }
        require(pixels.good(), "write marker preview pixels for visual inspection");
    }
}
} // namespace

/** Verify caller-defined assets and instance identity with the real ThorVG renderer. */
int main(int argc, char **argv) {
    using namespace thorium::engine;
    const std::string path = std::string(THORIUM_CONTENT_DIR) + "/animations/oreling.json";
    {
        Renderer renderer({"", {{"first", path}, {"second", path}}});
        require(renderer.error.empty() && renderer.begin(160, 100, 1),
                "caller-defined keys load without game configuration");
        renderer.animation({"first", 0}, 7, {0, 0, 40, 40});
        renderer.animation({"second", 9}, 7, {80, 0, 40, 40});
        require(renderer.finish(), "different assets may share an instance number in one frame");
        require(visible(renderer, 0, 40) && visible(renderer, 80, 120),
                "both asset instances paint distinct regions instead of moving one shared picture");
        require(renderer.begin(160, 100, 1), "begin second frame to release unused animation instances");
        renderer.animation({"second", 18}, 7, {60, 0, 40, 40});
        require(renderer.finish(), "retained instance survives removal of the other asset instance");
        require(!visible(renderer, 0, 40) && visible(renderer, 60, 100),
                "unused picture disappears while the other asset keeps painting");
        require(renderer.begin(160, 100, 1), "begin unknown-asset validation frame");
        renderer.animation({"missing", 0}, 7, {0, 0, 40, 40});
        require(!renderer.finish() && renderer.error.find("missing") != std::string::npos,
                "unknown assets report the actual caller key");
    }
    {
        Renderer renderer({"", {{"broken", "/thorium-does-not-exist.json"}}});
        require(!renderer.error.empty() && !renderer.begin(100, 100, 1),
                "missing required assets fail before drawing");
    }
    frameContracts(path);
    layerContracts();
    scaleContracts();
    markerContracts(path, argc > 1 ? argv[1] : "");
    std::cout << checks << " render asset checks passed\n";
}
