#define SDL_MAIN_USE_CALLBACKS
#include "AudioDevice.h"
#include "VideoOutput.h"
#include "app/Application.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

namespace {
/** Own all SDL resources; destroy textures before renderer and window. */
struct Desktop {
    SDL_Window *window = nullptr;
    thorium::platform::VideoOutput video;
    thorium::engine::FrameProfiler profiler;
    std::unique_ptr<thorium::Application> app;
    thorium::platform::AudioDevice audio;
    std::uint64_t lastTick = 0;
    int frames = 0;
    bool smoke = false;
    /** Release platform handles after game/render resources stop using them. */
    ~Desktop() {
        app.reset();
        video.close();
        if (window)
            SDL_DestroyWindow(window);
    }
};
/** One completed bench battle frame: live enemies and the measured CPU stages. */
struct BenchFrame {
    std::uint32_t enemies = 0;
    thorium::engine::FrameTimings timings;
};
/** Bench battle frames not yet read by the browser; the oldest are overwritten after 4096 frames. */
struct BenchRecorder {
    std::array<BenchFrame, 4096> frames{};
    std::size_t next = 0;
    std::size_t count = 0;
    /** Keep one frame without allocating. */
    void record(const BenchFrame &frame) {
        frames[next] = frame;
        next = (next + 1) % frames.size();
        count = std::min(count + 1, frames.size());
    }
};
BenchRecorder benchRecorder;
thorium::Application *diagnosticApp = nullptr;
thorium::platform::AudioDevice *diagnosticAudio = nullptr;
thorium::engine::FrameProfiler *diagnosticProfiler = nullptr;
std::string diagnosticBuffer;
} // namespace

/** Read-only diagnostics used by browser smoke/regression tests. */
extern "C" const char *thorium_debug_state() {
    diagnosticBuffer = diagnosticApp ? diagnosticApp->debugState() : "{}";
    if (diagnosticAudio) {
        diagnosticBuffer.pop_back();
        diagnosticBuffer +=
            ",\"audioDevice\":" + std::string(diagnosticAudio->available() ? "true" : "false") +
            ",\"audioActive\":" + (diagnosticAudio->active() ? "true" : "false") +
            ",\"audioFrames\":" + std::to_string(diagnosticAudio->submittedFrames) +
            ",\"audioAudibleFrames\":" + std::to_string(diagnosticAudio->audibleFrames) + "}";
    }
    if (diagnosticProfiler) {
        diagnosticBuffer.pop_back();
        diagnosticBuffer += ",\"performance\":" + diagnosticProfiler->debugState() + "}";
    }
    return diagnosticBuffer.c_str();
}
/** Read-only hit geometry; does not bypass normal UI or game command validation. */
extern "C" const char *thorium_debug_ui() {
    diagnosticBuffer = diagnosticApp ? diagnosticApp->debugUi() : "[]";
    return diagnosticBuffer.c_str();
}

/** Drain recorded bench battle frames as `enemies,total,simulation,presentation,scene,raster,upload,present,
 * steps` lines; empty outside a bench session. */
extern "C" const char *thorium_bench_frames() {
    using thorium::engine::FrameStage;
    std::ostringstream output;
    for (std::size_t index = benchRecorder.count; index > 0; --index) {
        const BenchFrame &frame =
            benchRecorder.frames[(benchRecorder.next + benchRecorder.frames.size() - index) %
                                 benchRecorder.frames.size()];
        const thorium::engine::FrameTimings &timing = frame.timings;
        output << frame.enemies << ',' << timing.totalMs << ',' << timing.stage(FrameStage::Simulation) << ','
               << timing.stage(FrameStage::Presentation) << ',' << timing.stage(FrameStage::Scene) << ','
               << timing.stage(FrameStage::Raster) << ',' << timing.stage(FrameStage::Upload) << ','
               << timing.stage(FrameStage::Present) << ',' << timing.simulationSteps << '\n';
    }
    benchRecorder.count = 0;
    diagnosticBuffer = output.str();
    return diagnosticBuffer.c_str();
}
/** Collect the bench query from the page URL or from native `--key=value` arguments. */
std::string benchQuery(int argc, char *argv[]) {
#ifdef __EMSCRIPTEN__
    static_cast<void>(argc);
    static_cast<void>(argv);
    return emscripten_run_script_string("location.search");
#else
    std::string query;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument.rfind("--", 0) == 0 && argument.find('=') != std::string::npos)
            query += argument.substr(2) + '&';
    }
    return query;
#endif
}

/** Decide whether the ThorVG copyright animation plays first: always, except `nosplash` in the page URL or a
 * `--no-splash` argument, and except in an automated browser unless the URL says `splash`. */
bool splashEnabled(int argc, char *argv[]) {
#ifdef __EMSCRIPTEN__
    static_cast<void>(argc);
    static_cast<void>(argv);
    return emscripten_run_script_int(
               "(function(){var q=location.search;if(q.indexOf('nosplash')>=0)return 0;"
               "return q.indexOf('splash')>=0||!navigator.webdriver?1:0;})()") != 0;
#else
    for (int index = 1; index < argc; ++index)
        if (std::string(argv[index]) == "--no-splash")
            return false;
    return true;
#endif
}

/** Initialize the SDL adapter and the same engine/game application on each target. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    std::unique_ptr<Desktop> state = std::make_unique<Desktop>();
    for (int index = 1; index < argc; ++index)
        if (std::string(argv[index]) == "--smoke-test")
            state->smoke = true;
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_Log("SDL init: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    state->window = SDL_CreateWindow("Thor-ium TD", 1200, 800,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                         (state->smoke ? SDL_WINDOW_HIDDEN : 0));
    if (!state->window) {
        SDL_Log("Window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
#ifdef __EMSCRIPTEN__
    SDL_SetWindowMinimumSize(state->window, 320, 560);
#else
    SDL_SetWindowMinimumSize(state->window, 360, 560);
#endif
    if (!state->video.open(state->window)) {
        SDL_Log("Renderer: %s", state->video.error.c_str());
        return SDL_APP_FAILURE;
    }
    state->app = std::make_unique<thorium::Application>();
    if (!state->app->error().empty()) {
        SDL_Log("Application: %s", state->app->error().c_str());
        return SDL_APP_FAILURE;
    }
    diagnosticApp = state->app.get();
    // Measurement-only scene: opened only by an explicit bench query, never by saved progress.
    if (const std::optional<thorium::BenchRequest> bench = thorium::parseBenchQuery(benchQuery(argc, argv))) {
        if (!state->app->startBenchmark(*bench)) {
            SDL_Log("Bench: %s", state->app->error().c_str());
            return SDL_APP_FAILURE;
        }
    }
    if (!state->smoke && !state->app->benchmark() && splashEnabled(argc, argv))
        state->app->startSplash();
    if (!state->audio.open())
        SDL_Log("Audio unavailable: %s", state->audio.error.c_str());
    for (const std::string &error : state->app->audioAssetErrors())
        SDL_Log("Audio fallback: %s", error.c_str());
    diagnosticAudio = &state->audio;
    diagnosticProfiler = &state->profiler;
    if (state->smoke) {
        state->audio.activate();
    }
    state->lastTick = SDL_GetTicksNS();
    *appstate = state.release();
    return SDL_APP_CONTINUE;
}

/** Normalize platform input once; SDL synthetic touch/mouse paths never double-submit. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    Desktop &state = *static_cast<Desktop *>(appstate);
    if (event->type == SDL_EVENT_QUIT || event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        return SDL_APP_SUCCESS;
    if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST || event->type == SDL_EVENT_WINDOW_HIDDEN) {
        state.app->suspend();
        state.audio.suspend();
        return SDL_APP_CONTINUE;
    }
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_FINGER_DOWN ||
        event->type == SDL_EVENT_KEY_DOWN)
        state.audio.activate();
    if (event->type == SDL_EVENT_WINDOW_RESIZED || event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        state.app->viewportChanged();
        return SDL_APP_CONTINUE;
    }
    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        const SDL_Keycode key = event->key.key;
        int mapped = 0;
        if (key >= SDLK_1 && key <= SDLK_4)
            mapped = '1' + static_cast<int>(key - SDLK_1);
        else if (key == SDLK_EQUALS || key == SDLK_PLUS || key == SDLK_KP_PLUS)
            mapped = '+';
        else if (key == SDLK_MINUS || key == SDLK_KP_MINUS)
            mapped = '-';
        else if (key == SDLK_0 || key == SDLK_KP_0)
            mapped = '0';
        else if (key == SDLK_SPACE)
            mapped = ' ';
        else if (key == SDLK_TAB)
            mapped = 9;
        else if (key == SDLK_RETURN)
            mapped = 13;
        else if (key == SDLK_ESCAPE)
            mapped = 27;
        else if (key == SDLK_M)
            mapped = 'm';
        else if (key == SDLK_S)
            mapped = 's';
        else if (key == SDLK_V)
            mapped = 'v';
        if (mapped)
            state.app->key(mapped, (event->key.mod & SDL_KMOD_SHIFT) != 0);
        return SDL_APP_CONTINUE;
    }
    SDL_Event logical = *event;
    if (!state.video.convertEvent(logical))
        return SDL_APP_CONTINUE;
    using thorium::engine::PointerPhase;
    if (logical.type == SDL_EVENT_MOUSE_WHEEL) {
        if (logical.wheel.which != SDL_TOUCH_MOUSEID)
            state.app->wheel({logical.wheel.mouse_x, logical.wheel.mouse_y},
                             logical.wheel.y *
                                 (logical.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0F : 1.0F));
    } else if (logical.type == SDL_EVENT_MOUSE_BUTTON_DOWN || logical.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (logical.button.which == SDL_TOUCH_MOUSEID || logical.button.button != SDL_BUTTON_LEFT)
            return SDL_APP_CONTINUE;
        state.app->pointer(
            {logical.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? PointerPhase::Down : PointerPhase::Up,
             0,
             {logical.button.x, logical.button.y}});
    } else if (logical.type == SDL_EVENT_MOUSE_MOTION) {
        if (logical.motion.which != SDL_TOUCH_MOUSEID)
            state.app->pointer({PointerPhase::Move, 0, {logical.motion.x, logical.motion.y}});
    } else if (logical.type == SDL_EVENT_FINGER_DOWN || logical.type == SDL_EVENT_FINGER_MOTION ||
               logical.type == SDL_EVENT_FINGER_UP || logical.type == SDL_EVENT_FINGER_CANCELED) {
        const PointerPhase phase = logical.type == SDL_EVENT_FINGER_DOWN       ? PointerPhase::Down
                                   : logical.type == SDL_EVENT_FINGER_UP       ? PointerPhase::Up
                                   : logical.type == SDL_EVENT_FINGER_CANCELED ? PointerPhase::Cancel
                                                                               : PointerPhase::Move;
        state.app->pointer({phase,
                            static_cast<std::int64_t>(logical.tfinger.fingerID),
                            {logical.tfinger.x, logical.tfinger.y},
                            thorium::engine::PointerDevice::Touch});
    }
    return SDL_APP_CONTINUE;
}

/** Run fixed game steps, draw with ThorVG and present its pixels through SDL. */
SDL_AppResult SDL_AppIterate(void *appstate) {
    Desktop &state = *static_cast<Desktop *>(appstate);
    const thorium::engine::FrameClock::time_point iteration = thorium::engine::FrameClock::now();
#ifdef __EMSCRIPTEN__
    // SDL writes inline canvas dimensions, so the viewport, rather than that canvas,
    // is authoritative for the responsive web host's logical size.
    const int cssWidth = EM_ASM_INT({ return window.innerWidth; });
    const int cssHeight = EM_ASM_INT({ return window.innerHeight; });
    if (cssWidth > 0 && cssHeight > 0) {
        int oldWidth = 0, oldHeight = 0;
        SDL_GetWindowSize(state.window, &oldWidth, &oldHeight);
        if (static_cast<int>(cssWidth) != oldWidth || static_cast<int>(cssHeight) != oldHeight)
            SDL_SetWindowSize(state.window, std::max(320, static_cast<int>(cssWidth)),
                              std::max(560, static_cast<int>(cssHeight)));
    }
#endif
    int width = 0, height = 0, pixelWidth = 0, pixelHeight = 0;
    SDL_GetWindowSize(state.window, &width, &height);
    SDL_GetWindowSizeInPixels(state.window, &pixelWidth, &pixelHeight);
    if (!state.video.resize(width, height)) {
        SDL_Log("Logical output: %s", state.video.error.c_str());
        return SDL_APP_FAILURE;
    }
    const std::uint64_t now = SDL_GetTicksNS();
    const float elapsed =
        state.smoke ? 1.0F / 60.0F : static_cast<float>(now - state.lastTick) / 1000000000.0F;
    state.lastTick = now;
    if (!state.app->frame(width, height, width ? static_cast<float>(pixelWidth) / width : 1.0F, elapsed)) {
        SDL_Log("Frame: %s", state.app->error().c_str());
        return SDL_APP_FAILURE;
    }
    const thorium::engine::Renderer &graphics = state.app->renderOutput();
    if (state.smoke && state.frames == 0)
        state.app->act({thorium::engine::Interaction::Kind::Click, thorium::presentation::AudioTest, {}});

    thorium::engine::FrameTimings timings = state.app->frameTimings();
    const thorium::engine::FrameClock::time_point mixing = thorium::engine::FrameClock::now();
    state.audio.pump(state.app->audioOutput());
    timings.add(thorium::engine::FrameStage::Audio, thorium::engine::frameMilliseconds(mixing));
    if (!state.video.present(graphics, timings)) {
        SDL_Log("Output: %s", state.video.error.c_str());
        return SDL_APP_FAILURE;
    }
    timings.totalMs = thorium::engine::frameMilliseconds(iteration);
    timings.intervalMs = elapsed * 1000.0;
    if (!state.profiler.record(timings)) {
        SDL_Log("Invalid overlapping frame measurements");
        return SDL_APP_FAILURE;
    }
    if (state.app->benchmark() && state.app->game().state().phase == thorium::game::Phase::Battle)
        benchRecorder.record({static_cast<std::uint32_t>(state.app->game().state().enemies.size()), timings});
    if (state.smoke && ++state.frames >= 4) {
        SDL_Surface *surface = SDL_CreateSurfaceFrom(
            graphics.pixelWidth, graphics.pixelHeight, SDL_PIXELFORMAT_RGBA32,
            const_cast<std::uint32_t *>(graphics.pixels.data()), graphics.pixelWidth * 4);
        const bool saved = surface && SDL_SaveBMP(surface, "native-smoke.bmp");
        if (surface)
            SDL_DestroySurface(surface);
        if (!state.audio.available() || !state.audio.audibleFrames) {
            SDL_Log("Audio smoke failed: %s", state.audio.error.c_str());
            return SDL_APP_FAILURE;
        }
        return saved ? SDL_APP_SUCCESS : SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
}

/** Stop callbacks and release application resources before SDL's own shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    static_cast<void>(result);
    diagnosticApp = nullptr;
    diagnosticAudio = nullptr;
    diagnosticProfiler = nullptr;
    delete static_cast<Desktop *>(appstate);
}
