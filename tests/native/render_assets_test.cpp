#include "engine/render/Renderer.h"
#include <algorithm>
#include <array>
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
    markerContracts(path, argc > 1 ? argv[1] : "");
    std::cout << checks << " render asset checks passed\n";
}
