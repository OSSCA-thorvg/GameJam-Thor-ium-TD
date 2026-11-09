#include "../tests/native/ApplicationScenario.h"
#include "app/Application.h"
#include "platform/sdl/AudioDevice.h"
#include "platform/sdl/VideoOutput.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
/** Own native benchmark resources with the same destruction order as the game adapter. */
struct Host {
    SDL_Window *window = nullptr;
    thorium::platform::VideoOutput video;
    thorium::platform::AudioDevice audio;
    /** Release presentation before the window and let SDL close last in main. */
    ~Host() {
        video.close();
        if (window)
            SDL_DestroyWindow(window);
    }
};
/** Drive real app, PCM pump and SDL output; only fixture population differs from gameplay. */
int measure(int count, int frames, bool cache, float density, int width = 1200, int height = 800, const char *capture = nullptr) {
    using namespace thorium;
    Host host;
    host.window = SDL_CreateWindow("Thorium frame benchmark", width, height, SDL_WINDOW_HIDDEN);
    if (!host.window || !host.video.open(host.window, false) || !host.video.resize(width, height) ||
        !host.audio.open()) {
        std::cerr << "Benchmark device: " << SDL_GetError() << host.video.error << host.audio.error;
        return 1;
    }
    host.audio.activate();
    game::Catalog content = game::defaultCatalog();
    for (game::EnemyDefinition &enemy : content.enemies) {
        enemy.hp = 1000000;
        enemy.leak = 0;
        enemy.speed = .1F;
    }
    content.waves = {{{game::EnemyKind::Normal, 1, 10000, 1}}};
    Application app(game::makeRun(content));
    if (!app.error().empty()) {
        std::cerr << app.error();
        return 1;
    }
    app.act({engine::Interaction::Kind::Click, presentation::HomeNew, {}});
    app.act({engine::Interaction::Kind::Click, presentation::Start, {}});
    if (cache)
        app.act({engine::Interaction::Kind::Click, presentation::ToggleAnimationBackend, {}});
    for (int index = 0; index < count; ++index) {
        const float progress = (index + .5F) / count * (app.game().routeLength() - 2);
        game::scenario(ApplicationTestAccess::run(app))
            .enemies.push_back({static_cast<game::EntityId>(100000 + index),
                                static_cast<game::EnemyKind>(index % 3), app.game().routePoint(progress),
                                progress, 1000000});
    }
    for (std::size_t index = 0; index < app.game().state().pads.size(); ++index)
        game::scenario(ApplicationTestAccess::run(app))
            .towers.push_back({static_cast<game::EntityId>(50000 + index),
                               index % 2 ? game::CardKind::Cannon : game::CardKind::Gun,
                               app.game().state().pads[index]});
    engine::FrameProfiler profiler;
    engine::FrameTimings cold;
    std::uint64_t previous = SDL_GetTicksNS();
    for (int frame = 0; frame < frames + 3; ++frame) {
        const engine::FrameClock::time_point start = engine::FrameClock::now();
        const std::uint64_t now = SDL_GetTicksNS();
        if (!app.frame(width, height, density, 1.0F / 60.0F)) {
            std::cerr << app.error();
            return 1;
        }
        engine::FrameTimings timing = app.frameTimings();
        const engine::FrameClock::time_point audio = engine::FrameClock::now();
        host.audio.pump(app.audioOutput());
        timing.add(engine::FrameStage::Audio, engine::frameMilliseconds(audio));
        if (!host.video.present(app.renderOutput(), timing)) {
            std::cerr << host.video.error;
            return 1;
        }
        timing.totalMs = engine::frameMilliseconds(start);
        timing.intervalMs = static_cast<double>(now - previous) / 1000000;
        previous = now;
        if (frame == 0)
            cold = timing;
        if (frame == 2 && capture) {
            SDL_Surface *surface = SDL_CreateSurfaceFrom(
                static_cast<int>(width * density), static_cast<int>(height * density), SDL_PIXELFORMAT_RGBA32,
                const_cast<std::uint32_t *>(app.renderOutput().pixels.data()), static_cast<int>(width * density * 4));
            if (surface) {
                SDL_SaveBMP(surface, capture);
                SDL_DestroySurface(surface);
            }
        }

        if (frame >= 3 && !profiler.record(timing))
            return 1;
        // Pace fast frames so PCM device consumption is representative; exclude idle waiting from work.
        if (timing.totalMs < 1000.0 / 60)
            SDL_DelayPrecise(static_cast<std::uint64_t>((1000.0 / 60 - timing.totalMs) * 1000000));
    }
    if (app.game().state().enemies.size() != static_cast<std::size_t>(count) || !host.audio.submittedFrames)
        return 1;
    std::cout << "{\"enemies\":" << count << ",\"frames\":" << frames
              << ",\"cache\":" << (cache ? "true" : "false") << ",\"density\":" << density << ",\"video\":\""
              << SDL_GetCurrentVideoDriver() << "\",\"audio\":\"" << SDL_GetCurrentAudioDriver()
              << "\",\"audio_frames\":" << host.audio.submittedFrames << ",\"cold_total_ms\":" << cold.totalMs
              << ",\"cold_prewarm_ms\":" << cold.stage(engine::FrameStage::CachePreparation)
              << ",\"performance\":" << profiler.debugState() << "}\n";
    return 0;
}
} // namespace
/** Run one reproducible process; device drivers are explicit runner configuration. */
int main(int argc, char **argv) {
    if (argc < 5)
        return 2;
    const int count = std::atoi(argv[1]), frames = std::atoi(argv[2]);
    const std::string mode = argv[3];
    const float density = std::strtof(argv[4], nullptr);
    const int width = argc >= 7 ? std::atoi(argv[5]) : 1200;
    const int height = argc >= 7 ? std::atoi(argv[6]) : 800;
    const char *capture = argc >= 8 ? argv[7] : nullptr;
    if (count < 1 || count > 3000 || frames < 3 || frames > 120 || (mode != "vector" && mode != "cache") ||
        (density != 1 && density != 1.5F))
        return 2;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        return 1;
    const int result = measure(count, frames, mode == "cache", density, width, height, capture);
    SDL_Quit();
    return result;
}
