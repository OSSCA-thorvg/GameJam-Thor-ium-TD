#pragma once
#include "FeedbackChannel.h"
#include "GameInput.h"
#include "game/presentation/Layout.h"
#include <optional>

namespace thorium {
/** Orchestrate practice with ordinary commands, independent simulation gates and recoverable checkpoints. */
class TutorialDirector {
  public:
    /** Borrow session objects; never consume the domain event queue a second time. */
    TutorialDirector(game::Run &run, presentation::State &state, engine::Ui &ui, FeedbackChannel &feedback,
                     presentation::Soundscape &sound);
    /** Start/restart practice without discarding the suspended regular expedition. */
    bool begin();
    /** Restore the regular expedition and show its title, preserving settings and completion history. */
    void exit();
    /** Restore the current lesson's last safe checkpoint, including resources and card instances. */
    void retry();
    /** Acknowledge the first explanation; clicks cannot complete action lessons. */
    void next();
    /** Observe authoritative entities, excavation and payments after commands or simulation. */
    void observe();
    /** Permit only the controls needed by the current lesson and normal utility navigation. */
    bool allows(const GameInput &input, const presentation::Layout &layout) const;
    /** Freeze explanations and setup without disabling pointer, drag or card inspection. */
    bool allowsSimulation() const;
    /** Read whether a practice run currently owns the game viewport. */
    bool active() const {
        return state.play.tutorial.active();
    }

  private:
    game::Run &run;
    presentation::State &state;
    engine::Ui &ui;
    FeedbackChannel &feedback;
    presentation::Soundscape &sound;
    std::optional<game::Run> regularRun_;
    std::optional<presentation::PlayState> regularPlay_;
    presentation::Navigation regularNavigation_;
    std::optional<game::Run> checkpoint_;
    presentation::Lesson checkpointLesson_ = presentation::Lesson::None;
    /** Enter an observed lesson and capture a safe retry point before any player action. */
    void enter(presentation::Lesson lesson);
    /** Resolve the next authored mining target without modifying hidden information. */
    int mineTarget() const;
};
} // namespace thorium
