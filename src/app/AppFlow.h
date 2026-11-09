#pragma once
#include "FeedbackChannel.h"
#include "engine/ui/Ui.h"
#include "game/presentation/Actions.h"

namespace thorium {
/** Explicit dispatch outcome; only a replaced run invalidates the fixed-step clock. */
enum class FlowResult { Unhandled, Handled, Restarted };
/** Handle navigation, preferences and new-run transitions; query simulation gates without a renderer. */
class AppFlow {
  public:
    /** Bind existing session objects; ownership remains in Application. */
    AppFlow(game::Run &run, presentation::State &state, engine::Ui &ui, engine::AudioMixer &audio,
            presentation::Soundscape &sound, FeedbackChannel &feedback);
    /** Apply an app-level control, leaving gameplay controls to GameplayController. */
    FlowResult act(presentation::Action action);
    /** Handle navigation and global settings shortcuts. */
    FlowResult key(int key);
    /** Gate simulation consistently for menus, manual pause and destructive confirmations. */
    bool allowsSimulation() const;

  private:
    game::Run &run;
    presentation::State &state;
    engine::Ui &ui;
    engine::AudioMixer &audio;
    presentation::Soundscape &sound;
    FeedbackChannel &feedback;
    /** Replace run and disposable interaction state while preserving session preferences. */
    void restart();
};
} // namespace thorium
