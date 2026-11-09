#include "engine/render/Renderer.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#if defined(__APPLE__) || defined(__linux__)
#include <sys/resource.h>
#endif

namespace {
using Clock = std::chrono::steady_clock;
/** Read peak RSS for this isolated benchmark process, returning zero on unsupported platforms. */
std::uint64_t peakResidentBytes() {
#if defined(__APPLE__) || defined(__linux__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0)
        return 0;
#if defined(__APPLE__)
    return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
    return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#endif
#else
    return 0;
#endif
}
/** Convert one measured interval to milliseconds. */
double milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
/** Select a nearest-rank percentile from sorted frame intervals. */
double percentile(const std::vector<double> &sorted, double fraction) {
    return sorted[std::min(sorted.size() - 1,
                           static_cast<std::size_t>(std::ceil(sorted.size() * fraction)) - 1)];
}
} // namespace

/** Measure one backend/count/density combination in a fresh process for independent RSS. */
int main(int argc, char **argv) {
    using namespace thorium::engine;
    if (argc != 6) {
        std::cerr << "usage: benchmark count continuous|vector|cache frames density cache-fps\n";
        return 2;
    }
    const int count = std::stoi(argv[1]);
    const std::string mode = argv[2];
    const int frames = std::stoi(argv[3]);
    const float density = std::stof(argv[4]);
    const float fps = std::stof(argv[5]);
    if (count < 1 || count > 10000 || frames < 3 || frames > 240 || !std::isfinite(density) || density < 1 ||
        density > 1.5F || (mode != "continuous" && mode != "vector" && mode != "cache") ||
        !std::isfinite(fps) || fps < 1 || fps > 120)
        return 2;
    AnimationCacheConfig config;
    config.sizes = {density > 1 ? 96 : 64};
    config.framesPerSecond = fps;
    const Clock::time_point initialize = Clock::now();
    Renderer renderer({"", {{"character", std::string(THORIUM_CONTENT_DIR) + "/animations/oreling.json"}}},
                      config);
    if (!renderer.error.empty()) {
        std::cerr << renderer.error;
        return 1;
    }
    const AnimationInfo &info = renderer.animationAssets().at("character");
    const std::optional<AnimationClip> clip = animationClip("character", info, "move", Playback::Loop);
    if (!clip)
        return 1;
    const double initialization = milliseconds(initialize);
    const Clock::time_point prepare = Clock::now();
    if (mode == "cache" && !renderer.prewarmAnimation(*clip)) {
        std::cerr << "prewarm failed";
        return 1;
    }
    const double prewarm = milliseconds(prepare);
    renderer.setAnimationBackend(mode == "cache" ? AnimationBackend::SharedFrames : AnimationBackend::Vector);
    std::vector<double> times;
    double firstFrame = 0;
    for (int frame = 0; frame < frames + 3; ++frame) {
        const Clock::time_point start = Clock::now();
        if (!renderer.begin(1280, 720, density)) {
            std::cerr << renderer.error;
            return 1;
        }
        for (int index = 0; index < count; ++index) {
            const std::uint32_t phaseBits = (static_cast<std::uint32_t>(index + 1) * 2654435761U) >> 8;
            const float phase = static_cast<float>(phaseBits) / 16777216.0F;
            AnimationSample sample = loopingSample(*clip, frame / 60.0F + phase * clip->duration);
            if (mode == "vector")
                sample.frame = *sampledAnimationFrame(sample, info, fps);
            const float x = std::fmod((index % 40) * 32.0F + frame * .35F, 1280.0F) - 8;
            const float y = (index / 40) * 28.0F - 8;
            renderer.animation(sample, static_cast<std::uint32_t>(index + 1), {x, y, 48, 48});
        }
        if (!renderer.finish()) {
            std::cerr << renderer.error;
            return 1;
        }
        const double elapsed = milliseconds(start);
        if (frame == 0)
            firstFrame = elapsed;
        if (frame >= 3)
            times.push_back(elapsed);
    }
    std::sort(times.begin(), times.end());
    const AnimationRenderStats stats = renderer.animationStats();
    std::cout << "{\"count\":" << count << ",\"mode\":\"" << mode << "\",\"frames\":" << frames
              << ",\"density\":" << density << ",\"fps\":" << fps << ",\"bucket\":" << config.sizes.front()
              << ",\"initialize_ms\":" << initialization << ",\"prewarm_ms\":" << prewarm
              << ",\"first_frame_ms\":" << firstFrame << ",\"median_ms\":" << percentile(times, .5)
              << ",\"p95_ms\":" << percentile(times, .95) << ",\"pixel_bytes\":" << stats.cache.bytes
              << ",\"cache_entries\":" << stats.cache.entries << ",\"bakes\":" << stats.cache.bakes
              << ",\"hits\":" << stats.cache.hits << ",\"fallbacks\":" << stats.fallbacks
              << ",\"vector_parses\":" << stats.vectorParses << ",\"baker_sources\":" << stats.bakerSources
              << ",\"peak_rss_bytes\":" << peakResidentBytes() << "}\n";
}
