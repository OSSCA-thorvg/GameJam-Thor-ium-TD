#pragma once
#include "FeedbackChannel.h"
#include "GameInput.h"
#include "game/presentation/Layout.h"

namespace thorium {
/** Translate gameplay selections and targets into atomic Run commands, without drawing. */
class GameplayController {
  public:
    /** Bind the run, interaction state and the same measured geometry used for drawing. */
    GameplayController(game::Run &run, presentation::State &state, engine::Ui &ui,
                       const presentation::Layout &layout, presentation::Soundscape &sound,
                       FeedbackChannel &feedback);
    /** Dispatch one decoded gameplay interaction. */
    void act(const GameInput &input);
    /** Apply gameplay keyboard shortcuts after app flow has handled navigation. */
    void key(int key);
    /** Clear stale selection identities and enter mining view after a wave transition. */
    void synchronize(game::Phase before);

  private:
    game::Run &run;
    presentation::State &state;
    engine::Ui &ui;
    const presentation::Layout &layout;
    presentation::Soundscape &sound;
    FeedbackChannel &feedback;
};
} // namespace thorium
