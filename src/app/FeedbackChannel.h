#pragma once
#include "game/presentation/Soundscape.h"
#include "game/presentation/State.h"

namespace thorium {
/** Fan out committed game facts once to visual and sound consumers. */
class FeedbackChannel {
  public:
    /** Bind run and presentation lifetimes owned by the composition root. */
    FeedbackChannel(game::Run &run, presentation::State &state, presentation::Soundscape &sound);
    /** Seed a new run without producing a false hand-replacement cue. */
    void reset();
    /** Drain combat facts and observe committed card/phase changes. */
    void collect();
    /** Present a command result after collecting its authoritative effects. */
    void notify(const game::Result &result, presentation::Sound success = presentation::Sound::Confirm);

  private:
    game::Run &run;
    presentation::State &state;
    presentation::Soundscape &sound;
};
} // namespace thorium
