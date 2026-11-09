#include "GameplayController.h"
#include <algorithm>
#include <cmath>
namespace thorium {
GameplayController::GameplayController(game::Run &run, presentation::State &state, engine::Ui &ui,
                                       const presentation::Layout &layout, presentation::Soundscape &sound,
                                       FeedbackChannel &feedback)
    : run(run), state(state), ui(ui), layout(layout), sound(sound), feedback(feedback) {}
void GameplayController::act(const GameInput &input) {
    using namespace presentation;
    if (!state.app.navigation.gameplay())
        return;
    const presentation::Action action = input.action;
    const Modal modal = state.play.modal;
    if (modal == Modal::NewRun)
        return;
    if (state.play.paused && modal == Modal::None &&
        (input.kind != InputKind::Control || (action != Pause && action != Resume)))
        return;
    if (modal == Modal::Withdrawal && (input.kind != InputKind::Control || action != ConfirmDismiss))
        return;
    if (modal == Modal::Recruit && input.kind != InputKind::Candidate)
        return;
    if (modal == Modal::Replacement && input.kind != InputKind::Deck)
        return;
    if (run.state().expansionPending &&
        (input.kind != InputKind::Control || (action != ExpandLong && action != ExpandShort)))
        return;
    if (run.state().rewardOffer && input.kind != InputKind::Deck &&
        (input.kind != InputKind::Control || action != RewardSkip))
        return;
    if (action == ConfirmDismiss && modal != Modal::Withdrawal)
        return;
    if (input.kind == InputKind::Hand) {
        const game::CardId id = input.payload;
        const game::Card *card = run.card(id);
        if (!card)
            return;
        bool inHand = false;
        for (int index = 0; index < 4; ++index)
            if (run.state().deck[run.state().hand[index]].id == id) {
                if (state.play.feedback.cardBusy(index))
                    return;
                inHand = true;
            }
        if (!inHand)
            return;
        state.play.guideOpen = false;
        const bool deselect =
            input.interaction == engine::Interaction::Kind::Click && state.play.selectedCard == id;
        state.play.selectedUnit = 0;
        state.play.selectedTower = 0;
        state.play.movingUnit = false;
        state.play.selectedCard = deselect ? 0 : id;
        if (deselect)
            return;
        sound.play(Sound::Select);
        if (input.interaction == engine::Interaction::Kind::Drop) {
            if (!layout.world.contains(input.position)) {
                state.play.selectedCard = 0;
                feedback.notify({false, "배치를 취소했어요"});
                return;
            }
            const game::Result result =
                run.play(id, layout.surfacePoint(input.position, run.catalog().isBuilding(card->kind) ||
                                                                     run.catalog().isUnit(card->kind)));
            feedback.notify(result, Sound::None);
            if (result.ok)
                state.play.selectedCard = 0;
        }
        return;
    }
    if (input.kind == InputKind::Deck) {
        if (!run.state().rewardOffer && modal != Modal::Replacement)
            return;
        const game::CardOffer *active =
            run.state().rewardOffer ? &*run.state().rewardOffer : run.offer(state.play.selectedOffer);
        if (!active)
            return;
        const game::Result result =
            run.acceptOffer(active->id,
                            active->source == game::OfferSource::Expansion ? active->candidates.front()
                                                                           : state.play.candidate,
                            input.payload);
        feedback.notify(result, Sound::Reward);
        if (result.ok) {
            state.play.modal = presentation::Modal::None;
            state.play.selectedOffer = 0;
        }
        return;
    }
    if (input.kind == InputKind::Candidate) {
        const game::CardOffer *active = run.offer(state.play.selectedOffer);
        if (modal != Modal::Recruit || !active || active->source != game::OfferSource::Recruitment ||
            std::find(active->candidates.begin(), active->candidates.end(),
                      static_cast<game::CardKind>(input.payload)) == active->candidates.end())
            return;
        state.play.candidate = static_cast<game::CardKind>(input.payload);
        sound.play(Sound::Select);
        state.play.modal = presentation::Modal::Replacement;
        ui.cancel();
        return;
    }
    switch (action) {
    case World: {
        if (run.state().phase == game::Phase::Prep && state.play.mine) {
            const int cell = layout.mineCell(input.position);
            feedback.notify(state.play.scan ? run.scan(cell) : run.dig(cell),
                            state.play.scan ? Sound::Scan : Sound::None);
            if (state.play.scan && run.state().scans == 0)
                state.play.scan = false;
            return;
        }
        const game::Card *card = run.card(state.play.selectedCard);
        if (card) {
            const game::Result result =
                run.play(card->id, layout.surfacePoint(input.position, run.catalog().isBuilding(card->kind) ||
                                                                           run.catalog().isUnit(card->kind)));
            feedback.notify(result, Sound::None);
            if (result.ok)
                state.play.selectedCard = 0;
            return;
        }
        const game::Point target = layout.surfacePoint(input.position, true);
        if (state.play.selectedUnit && state.play.movingUnit && run.state().phase == game::Phase::Prep) {
            const game::Result result = run.moveUnit(state.play.selectedUnit, target);
            feedback.notify(result);
            if (result.ok)
                state.play.movingUnit = false;
            return;
        }
        for (const game::Unit &unit : run.state().units)
            if (game::distance(unit.position, target) < 0.5F) {
                state.play.selectedUnit = unit.id;
                state.play.selectedTower = 0;
                feedback.notify({true, std::string(run.catalog().card(unit.kind).name) + " · 체력 " +
                                           std::to_string(unit.hp) + " / " + std::to_string(unit.maxHp)});
                return;
            }
        for (const game::Tower &tower : run.state().towers)
            if (game::distance(tower.position, target) < 0.5F) {
                state.play.selectedTower = tower.id;
                state.play.selectedUnit = 0;
                state.play.movingUnit = false;
                state.play.toastTime = 0;
                return;
            }
        state.play.selectedUnit = 0;
        state.play.selectedTower = 0;
        state.play.movingUnit = false;
        break;
    }
    case Start: {
        const bool reveal = run.state().phase == game::Phase::Prep;
        const game::Result result = run.startWave();
        feedback.notify(result, Sound::None);
        if (result.ok) {
            if (reveal && !state.app.preferences.reducedMotion)
                state.play.feedback.revealHand(run);
            state.play.mine = false;
            state.play.scan = false;
            state.play.selectedCard = 0;
            state.play.selectedUnit = 0;
            state.play.selectedTower = 0;
            state.play.movingUnit = false;
            state.play.paused = false;
            state.play.guideOpen = false;
            ui.cancel();
        }
        break;
    }
    case Pause:
        state.play.paused = !state.play.paused;
        ui.cancel();
        break;
    case Resume:
        state.play.paused = false;
        ui.cancel();
        break;
    case Speed:
        state.play.speed = state.play.speed == 1 ? 2 : 1;
        sound.play(Sound::Select);
        break;
    case Surface:
        sound.play(Sound::Select);
        state.play.mine = false;
        state.play.scan = false;
        state.play.selectedCard = 0;
        ui.cancel();
        break;
    case Mine:
        sound.play(Sound::Select);
        state.play.mine = true;
        state.play.selectedUnit = 0;
        state.play.selectedTower = 0;
        state.play.movingUnit = false;
        state.play.selectedCard = 0;
        ui.cancel();
        break;
    case Scan:
        state.play.scan = !state.play.scan;
        state.play.mine = true;
        feedback.notify({true, state.play.scan ? "스캔할 경계 1칸을 눌러 주세요" : "스캔 선택을 취소했어요"});
        break;
    case Exchange: {
        const game::Result result = run.exchange(state.play.selectedCard);
        feedback.notify(result, Sound::None);
        if (result.ok)
            state.play.selectedCard = 0;
        break;
    }
    case Tool:
        feedback.notify(run.upgrade(thorium::game::UpgradeKind::Tool), Sound::Upgrade);
        break;
    case Armor:
        feedback.notify(run.upgrade(thorium::game::UpgradeKind::Armor), Sound::Upgrade);
        break;
    case Slots:
        feedback.notify(run.upgrade(thorium::game::UpgradeKind::Slots), Sound::Upgrade);
        break;
    case Coil:
        feedback.notify(run.upgrade(thorium::game::UpgradeKind::Coil), Sound::Upgrade);
        break;
    case Heal:
        feedback.notify(run.heal(state.play.selectedUnit));
        break;
    case Dismiss:
        if (!run.unit(state.play.selectedUnit) && !run.tower(state.play.selectedTower)) {
            feedback.notify({false, "철수할 건물이나 유닛을 먼저 선택해 주세요"});
            break;
        }
        state.play.modal = presentation::Modal::Withdrawal;
        state.play.error = false;
        ui.cancel();
        break;
    case ConfirmDismiss:
        feedback.notify(
            run.dismiss(state.play.selectedTower ? state.play.selectedTower : state.play.selectedUnit));
        state.play.selectedUnit = 0;
        state.play.selectedTower = 0;
        state.play.movingUnit = false;
        state.play.modal = presentation::Modal::None;
        break;
    case MoveUnit:
        if (run.state().phase == game::Phase::Prep && run.unit(state.play.selectedUnit)) {
            state.play.movingUnit = true;
            feedback.notify({true, "이동할 배치 칸을 선택해 주세요. 점유 칸이면 자리를 교환해요"});
        }
        break;
    case ClearSelection:
        state.play.selectedUnit = 0;
        state.play.selectedTower = 0;
        state.play.selectedCard = 0;
        state.play.movingUnit = false;
        break;
    case ToggleDrawer:
        state.play.guideOpen = !state.play.guideOpen;
        ui.cancel();
        break;
    case CloseDrawer:
    case AcknowledgeDrawer:
        state.play.guideOpen = false;
        ui.cancel();
        break;
    case Recruit:
        if (run.state().phase != game::Phase::Prep || !run.state().recruitmentOffer)
            return;
        state.play.selectedOffer = run.state().recruitmentOffer->id;
        state.play.modal = presentation::Modal::Recruit;
        state.play.error = false;
        ui.cancel();
        break;
    case ExpandLong:
        feedback.notify(run.expand(1));
        break;
    case ExpandShort:
        feedback.notify(run.expand(2));
        break;
    case RewardSkip:
        if (run.state().rewardOffer)
            feedback.notify(run.skipOffer(run.state().rewardOffer->id));
        break;
    default:
        break;
    }
}
void GameplayController::key(int key) {
    if (!state.app.navigation.gameplay())
        return;
    if (key == 27) {
        ui.cancel();
        if (state.play.modal != presentation::Modal::None)
            state.play.modal = presentation::Modal::None;
        else if (state.play.selectedCard || state.play.selectedUnit || state.play.selectedTower ||
                 state.play.scan) {
            state.play.selectedCard = 0;
            state.play.selectedUnit = 0;
            state.play.selectedTower = 0;
            state.play.movingUnit = false;
            state.play.scan = false;
        } else if (run.state().phase == game::Phase::Battle)
            state.play.paused = !state.play.paused;
        return;
    }
    if (key == ' ' && state.play.paused && state.play.modal == presentation::Modal::None &&
        state.app.navigation.gameplay()) {
        state.play.paused = false;
        return;
    }
    if (state.play.modal != presentation::Modal::None || state.play.paused || run.state().expansionPending ||
        run.state().rewardOffer)
        return;
    if (key >= '1' && key <= '4') {
        if (state.play.feedback.cardBusy(key - '1'))
            return;
        state.play.selectedCard = run.state().deck[run.state().hand[key - '1']].id;
        state.play.selectedUnit = 0;
        state.play.selectedTower = 0;
        state.play.movingUnit = false;
        return;
    }
    if (key == ' ') {
        if (run.state().phase == game::Phase::Ready || run.state().phase == game::Phase::Prep)
            act({InputKind::Control, presentation::Start, 0, engine::Interaction::Kind::Click, {}});
        else
            state.play.paused = !state.play.paused;
    }
    if (key == 'm' && run.state().phase == game::Phase::Prep) {
        state.play.mine = !state.play.mine;
        state.play.selectedUnit = 0;
        state.play.selectedTower = 0;
        state.play.movingUnit = false;
        ui.cancel();
    }
}
void GameplayController::synchronize(game::Phase before) {
    if (before != run.state().phase) {
        ui.cancel();
        state.play.selectedCard = 0;
        state.play.selectedUnit = 0;
        state.play.selectedTower = 0;
        state.play.movingUnit = false;
        state.play.modal = presentation::Modal::None;
        state.play.selectedOffer = 0;
        state.play.scan = false;
        if (run.state().phase == game::Phase::Prep) {
            state.play.mine = true;
            feedback.notify({true, "정비 시작! 광물을 캐서 다음 전투를 준비하세요"},
                            presentation::Sound::None);
        }
    }
    if (state.play.selectedUnit && !run.unit(state.play.selectedUnit)) {
        state.play.selectedUnit = 0;
        state.play.movingUnit = false;
    }
    if (state.play.selectedTower && !run.tower(state.play.selectedTower))
        state.play.selectedTower = 0;
}
} // namespace thorium
