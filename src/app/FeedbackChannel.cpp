#include "FeedbackChannel.h"

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
            state.play.toast = "적 둥지 각성! 다음 공세에 추가 적이 침투합니다.";
            state.play.toastTime = 5.0F;
            state.play.error = true;
        } else if (!hints.towerUpgrade) {
            for (int slot = 0; slot < 4; ++slot) {
                const auto &handCard = run.state().deck[run.state().hand[slot]];
                if (run.catalog().isBuilding(handCard.kind)) {
                    for (const auto &tower : run.state().towers) {
                        if (tower.kind == handCard.kind) {
                            hints.towerUpgrade = true;
                            state.play.toast = "기존 타워 위에 같은 카드를 놓으면 할인된 비용으로 강화돼요!";
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
                    state.play.toast = "부상당한 아군을 누르면 철과 구리로 즉시 치료할 수 있어요.";
                    state.play.toastTime = 5.0F;
                    state.play.error = false;
                    break;
                }
            }
        } else if (!hints.firstGold && run.state().gold >= 4) {
            hints.firstGold = true;
            state.play.toast = "금을 획득했습니다! 새 카드를 모집하거나 부대 슬롯을 늘릴 수 있어요.";
            state.play.toastTime = 5.0F;
            state.play.error = false;
        }
    }
    // The boulder hint stands apart from the chain above, which holds later hints back during preparation.
    if (state.play.toastTime <= 0.0F && state.app.navigation.gameplay() && !state.play.hints.boulder &&
        !run.state().boulders.empty() && run.boulderQuote(run.state().boulders.front()).allowed()) {
        state.play.hints.boulder = true;
        state.play.toast = run.catalog().rails.empty()
                               ? "철이 모였어요! 지상 화면에서 낙석 바위를 누르면 치울 수 있어요."
                               : "철이 모였어요! 지상 화면에서 수정 군집을 누르면 치울 수 있어요.";
        state.play.toastTime = 5.0F;
        state.play.error = false;
    }
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
