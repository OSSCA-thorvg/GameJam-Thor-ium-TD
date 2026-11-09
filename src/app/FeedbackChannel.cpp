#include "FeedbackChannel.h"

namespace thorium {
FeedbackChannel::FeedbackChannel(game::Run &run, presentation::State &state, presentation::Soundscape &sound)
    : run(run), state(state), sound(sound) {}
void FeedbackChannel::reset() {
    state.play.actors.reset(state.app.animationSeed);
    state.play.actors.observe(run, {}, state.app.animationSets);
    state.play.feedback.observeHand(run);
    sound.reset(run);
}
void FeedbackChannel::collect() {
    const std::vector<game::CombatEvent> events = run.takeCombatEvents();
    state.play.actors.observe(run, events, state.app.animationSets);
    state.play.feedback.consume(events);
    state.play.feedback.observeHand(run);
    if (state.app.preferences.reducedMotion)
        state.play.feedback.settleCards();
    sound.observe(run);
    sound.consume(events);
}
void FeedbackChannel::notify(const game::Result &result, presentation::Sound success) {
    state.play.toast = result.message;
    state.play.toastTime = 5.0F;
    state.play.error = !result.ok;
    collect();
    sound.play(result.ok ? success : presentation::Sound::Error);
    if (!result.ok)
        state.play.feedback.reject(state.play.selectedCard);
}
} // namespace thorium
