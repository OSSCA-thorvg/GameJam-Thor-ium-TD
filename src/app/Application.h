#pragma once
#include "AppFlow.h"
#include "GameplayController.h"
#include "Persistence.h"
#include "TutorialDirector.h"
#include "engine/diagnostics/FrameProfiler.h"
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
    /** Seed the first hand snapshot without playing a false replacement animation. */
    explicit Application(game::Run initialRun = game::Run{});
    /** Advance a fixed simulation and draw a frame at the current viewport. */
    bool frame(int width, int height, float density, float elapsed);
    /** Receive a platform-neutral mouse or touch input. */
    void pointer(engine::Pointer event);
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

    /** Save persistent player settings and progression. */
    void savePersistence();
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
    FeedbackChannel feedback;
    AppFlow flow;
    GameplayController gameplay;
    TutorialDirector tutorial;
    /** Invalidate timing only after an app transition replaced the run. */
    void applyFlow(FlowResult result);
};
} // namespace thorium
