#pragma once
#include "AppFlow.h"
#include "BenchScene.h"
#include "GameplayController.h"
#include "MapCamera.h"
#include "MineScroll.h"
#include "Persistence.h"
#include "TutorialDirector.h"
#include "engine/diagnostics/FrameProfiler.h"
#include "engine/render/RenderScale.h"
#include "engine/render/Renderer.h"
#include "engine/ui/Ui.h"
#include "game/presentation/Soundscape.h"
#include "game/presentation/View.h"
#include "game/rules/Run.h"
#include <string>

namespace thorium {
/** Compose one run, UI, presentation and renderer without exposing SDL to rules. */
class Application {
  public:
    /** Compose the shipped campaign and seed its first hand without playing a replacement animation. */
    Application();
    /** Compose an isolated authored run, retaining custom content on reset rather than selecting a campaign.
     */
    explicit Application(game::Run initialRun);
    /** Advance a fixed simulation and draw a frame at the current viewport. */
    bool frame(int width, int height, float density, float elapsed);
    /** Receive a platform-neutral mouse or touch input. */
    void pointer(engine::Pointer event);
    /** Receive normalized wheel zoom without exposing SDL to application rules. */
    void wheel(engine::ScreenPoint point, float steps);
    /** Apply a generic UI interaction to the game-specific command boundary. */
    void act(engine::Interaction interaction);
    /** Interpret keyboard shortcuts without injecting synthetic pointer events. */
    void key(int key, bool shift);
    /** Stop unfinished input and background simulation on focus loss. */
    void suspend();
    /** Read only public game state and presentation state for local diagnostics. */
    std::string debugState() const;
    /** Read current UI hit geometry for reproducible pointer tests. */
    std::string debugUi() const;
    /** Read the current app stages; the platform adds audio, upload and presentation. */
    const engine::FrameTimings &frameTimings() const {
        return frameTimings_;
    }

    /** Read authoritative state and definitions without exposing run commands to the platform. */
    const game::Run &game() const {
        return run;
    }
    /** Read the completed framebuffer without allowing renderer mutation. */
    const engine::Renderer &renderOutput() const {
        return graphics;
    }
    /** Borrow only PCM output for a synchronous platform pump. */
    engine::AudioOutput audioOutput() {
        return audio.output();
    }
    /** Cancel captured input after a platform viewport change. */
    void viewportChanged() {
        ui.cancel();
        camera.cancel();
        mineScroll.cancel();
    }
    /** Read the composition/render failure without exposing engine ownership. */
    const std::string &error() const {
        return graphics.error;
    }
    /** Read optional audio-asset diagnostics at startup. */
    const std::vector<std::string> &audioAssetErrors() const {
        return sound.assetErrors;
    }
    /** Read current interaction geometry for diagnostics and native integration tests. */
    const std::vector<engine::Widget> &widgets() const {
        return ui.widgets;
    }

    /** Save persistent player settings and progression; a bench session never writes. */
    void savePersistence();
    /** Store the campaign expedition between waves so a reload can continue it, and remove the stored copy
     * once the expedition ends. Checks at most twice a second unless the phase changed or `force` is set. */
    void syncRunSave(float elapsed, bool force = false);
    /** Open a measurement-only late battle in place of the title and disable saving for this session. */
    bool startBenchmark(const BenchRequest &request);
    /** Read the active bench request, if this session opened one. */
    const std::optional<BenchRequest> &benchmark() const {
        return benchmark_;
    }

  private:
    friend struct ApplicationTestAccess;
    game::Run run;
    engine::Ui ui;
    presentation::State state;
    presentation::View view;
    engine::Renderer graphics;
    engine::AudioMixer audio;
    presentation::Soundscape sound;
    engine::FrameTimings frameTimings_;
    float accumulator_ = 0.0F;
    bool audioPaused_ = false;
    bool animationCachePrepared_ = false;
    /** Last stored expedition text, the phase it was checked in and the time since the last check. */
    std::string savedRun_;
    game::Phase savedPhase_ = game::Phase::Ready;
    float runSaveTimer_ = 0;
    /** Continue a stored campaign expedition from the title when it still matches its stage content;
     * otherwise discard it. */
    void restoreRun();
    /** Lowers the render density while the measured CPU work of recent frames exceeds the frame budget. */
    engine::RenderScale renderScale_;
    std::optional<BenchRequest> benchmark_;
    /** Fixed steps since the bench battle opened; the autoplayer decides a card every 30 steps. */
    std::uint64_t benchmarkSteps_ = 0;
    FeedbackChannel feedback;
    AppFlow flow;
    GameplayController gameplay;
    TutorialDirector tutorial;
    MapCamera camera;
    MineScroll mineScroll;
    /** Invalidate timing only after an app transition replaced the run. */
    void applyFlow(FlowResult result);
};
} // namespace thorium
