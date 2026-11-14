#include "FeedbackChannel.h"
#include "game/presentation/Text.h"

namespace thorium {
FeedbackChannel::FeedbackChannel(game::Run &run, presentation::State &state, presentation::Soundscape &sound)
    : run(run), state(state), sound(sound) {}
void FeedbackChannel::reset() {
    state.play.actors.reset(state.app.animationSeed);
    state.play.actors.observe(run, {}, state.app.animationSets);
    state.play.feedback.observeHand(run);
    state.play.feedback.observeMine(run);
    sound.reset(run);
}
void FeedbackChannel::collect() {
    const std::vector<game::CombatEvent> events = run.takeCombatEvents();
    state.play.actors.observe(run, events, state.app.animationSets);
    state.play.feedback.consume(events);
    state.play.feedback.observeHand(run);
    state.play.feedback.observeMine(run);
    if (state.app.preferences.reducedMotion)
        state.play.feedback.settleCards();
    sound.observe(run);
    sound.consume(events);

    // Contextual one-time interactive hints during regular expedition
    if (state.play.toastTime <= 0.0F && state.app.navigation.gameplay()) {
        auto &hints = state.play.hints;
        if (!hints.nestExcavated && run.state().nextExtra > 0) {
            hints.nestExcavated = true;
            state.play.toast = presentation::tr("toast.nestAwakened");
            state.play.toastTime = 5.0F;
            state.play.error = true;
        } else if (!hints.towerUpgrade) {
            for (int slot = 0; slot < 4; ++slot) {
                const auto &handCard = run.state().deck[run.state().hand[slot]];
                if (run.catalog().isBuilding(handCard.kind)) {
                    for (const auto &tower : run.state().towers) {
                        if (tower.kind == handCard.kind) {
                            hints.towerUpgrade = true;
                            state.play.toast = presentation::tr("toast.towerUpgrade");
                            state.play.toastTime = 5.0F;
                            state.play.error = false;
                            break;
                        }
                    }
                }
                if (hints.towerUpgrade)
                    break;
            }
        } else if (!hints.woundedUnit && run.state().phase == game::Phase::Prep) {
            for (const auto &unit : run.state().units) {
                if (unit.hp < unit.maxHp) {
                    hints.woundedUnit = true;
                    state.play.toast = presentation::tr("toast.woundedUnit");
                    state.play.toastTime = 5.0F;
                    state.play.error = false;
                    break;
                }
            }
        } else if (!hints.firstGold && run.state().gold >= 4) {
            hints.firstGold = true;
            state.play.toast = presentation::tr("toast.firstGold");
            state.play.toastTime = 5.0F;
            state.play.error = false;
        }
    }
    if (state.play.toastTime <= 0.0F && state.app.navigation.gameplay() && !state.play.hints.hero &&
        run.hero() && run.state().phase == game::Phase::Battle) {
        state.play.hints.hero = true;
        state.play.toast = presentation::tr(
            "toast.hero", {{"seconds", std::to_string(static_cast<int>(run.catalog().balance.heroRespawn))}});
        state.play.toastTime = 5.0F;
        state.play.error = false;
    }
    // The boulder hint stands apart from the chain above, which holds later hints back during preparation.
    if (state.play.toastTime <= 0.0F && state.app.navigation.gameplay() && !state.play.hints.boulder &&
        !run.state().boulders.empty() && run.boulderQuote(run.state().boulders.front()).allowed()) {
        state.play.hints.boulder = true;
        state.play.toast =
            presentation::tr(run.catalog().rails.empty() ? "toast.boulder" : "toast.crystalCluster");
        state.play.toastTime = 5.0F;
        state.play.error = false;
    }
}
void FeedbackChannel::notify(const game::Result &result, presentation::Sound success) {
    state.play.toast = presentation::tr(result.message);
    state.play.toastTime = 5.0F;
    state.play.error = !result.ok;
    collect();
    sound.play(result.ok ? success : presentation::Sound::Error);
    if (!result.ok)
        state.play.feedback.reject(state.play.selectedCard);
}
} // namespace thorium
