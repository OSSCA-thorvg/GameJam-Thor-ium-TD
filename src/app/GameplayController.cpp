#include "GameplayController.h"
#include "game/presentation/TerrainPresentation.h"
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
    if (modal == Modal::MineDescent && (input.kind != InputKind::Control || action != ConfirmDescend))
        return;
    if (action == ConfirmDescend && modal != Modal::MineDescent)
        return;
    if (modal == Modal::Recruit && input.kind != InputKind::Candidate &&
        !(input.kind == InputKind::Control &&
          (action == PayGold || action == PayShards || action == RerollOffer)))
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
        state.play.selectedBoulder.reset();
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
            // A drop is a complete attempt: a rejection keeps its message but releases the card.
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
        const game::Result result = run.acceptOffer(
            active->id,
            active->source != game::OfferSource::Recruitment ? active->candidates.front()
                                                             : state.play.candidate,
            input.payload, active->source == game::OfferSource::Recruitment && state.play.payShards);
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
            if (input.interaction == engine::Interaction::Kind::Drop)
                return;
            const int cell = layout.mineCell(input.position);
            if (!state.play.scan && run.catalog().mining == game::MiningMode::Excavate) {
                const int buried = run.buriedDeposits();
                const game::Result result =
                    run.strike(cell, state.play.blast ? game::StrikeTool::Blast : game::StrikeTool::Pick);
                feedback.notify(result,
                                run.buriedDeposits() < buried ? Sound::MineComplete : Sound::MineStart);
                if (result.ok) {
                    state.play.strikeCell = cell;
                    ++state.play.strikes;
                }
                if (result.ok && state.play.blast)
                    state.play.feedback.screenShake = .2F;
                return;
            }
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
        const game::Unit *mover = run.unit(state.play.selectedUnit);
        if (mover && state.play.movingUnit &&
            (run.state().phase == game::Phase::Prep ||
             (mover->hero && run.state().phase == game::Phase::Battle))) {
            const game::Result result = run.moveUnit(state.play.selectedUnit, target);
            feedback.notify(result);
            if (result.ok)
                state.play.movingUnit = false;
            return;
        }
        state.play.selectedBoulder.reset();
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
        // A lever sends its cart and a gas tank explodes during battle; outside battle both explain
        // themselves.
        const bool battle = run.state().phase == game::Phase::Battle;
        for (std::size_t rail = 0; rail < run.catalog().rails.size(); ++rail)
            if (game::distance(run.catalog().rails[rail].lever, target) < 0.6F) {
                if (battle) {
                    const game::Result result = run.launchCart(rail);
                    feedback.notify(result, result.ok ? Sound::Upgrade : Sound::None);
                    if (result.ok)
                        state.play.feedback.screenShake = .12F;
                } else
                    feedback.notify({true, "광차 레버 · 전투 중에 누르면 광차가 선로 위 지상 적에게 " +
                                               std::to_string(run.catalog().balance.cartDamage) + " 피해"},
                                    Sound::None);
                return;
            }
        for (std::size_t tank = 0; tank < run.catalog().tanks.size(); ++tank)
            if (game::distance(run.catalog().tanks[tank].position, target) < 0.6F) {
                if (battle) {
                    const game::Result result = run.detonateTank(tank);
                    feedback.notify(result, Sound::None);
                    if (result.ok)
                        state.play.feedback.screenShake = .3F;
                } else
                    feedback.notify({true, "가스통 · 전투 중에 누르면 폭발해 주변 적과 아군에게 " +
                                               std::to_string(run.catalog().balance.tankDamage) +
                                               " 피해 · 웨이브마다 다시 채워져요"},
                                    Sound::None);
                return;
            }
        // A barricade, or the mouth of the open branch, moves the barricade between waves.
        for (std::size_t fork = 0; fork < run.catalog().forks.size(); ++fork) {
            const std::vector<game::Point> open = run.forkBranch(fork, run.detoured(fork));
            const float gap = std::max(.001F, game::distance(open[0], open[1]));
            const game::Point mouth{open[0].x + (open[1].x - open[0].x) / gap,
                                    open[0].y + (open[1].y - open[0].y) / gap};
            if (game::distance(run.barricade(fork), target) >= 0.6F && game::distance(mouth, target) >= 0.6F)
                continue;
            if (battle)
                feedback.notify({true, "바리케이드 · 정비 중에 누르면 막을 갈래를 바꿔요"}, Sound::None);
            else
                feedback.notify(run.switchFork(fork), Sound::Placement);
            return;
        }
        // A boulder opens its inspector in preparation; in battle it only explains itself.
        for (game::Point boulder : run.state().boulders)
            if (game::distance(boulder, target) < 0.5F) {
                if (run.state().phase == game::Phase::Prep) {
                    state.play.selectedBoulder = boulder;
                    state.play.toastTime = 0;
                } else
                    feedback.notify(
                        {true, std::string(run.catalog().rails.empty() ? "낙석 바위" : "수정 군집") +
                                   " · 정비 중에 치우면 건물 패드가 돼요 · " +
                                   game::costLabel(run.catalog().balance.boulder)},
                        Sound::None);
                return;
            }
        // An empty trait pad names its bonus.
        const game::PadTrait trait = run.padTrait(target);
        if (trait != game::PadTrait::None)
            feedback.notify({true, std::string(padTraitName(trait)) + " 패드 · 이 위의 건물 " +
                                       padTraitEffect(trait, run.catalog().balance)},
                            Sound::None);
        break;
    }
    case RerollOffer: {
        const game::Result result = run.rerollRecruitment();
        feedback.notify(result, Sound::CardDraw);
        if (result.ok && run.state().recruitmentOffer)
            state.play.selectedOffer = run.state().recruitmentOffer->id;
        break;
    }
    case TrainHero:
        feedback.notify(run.trainHero(), Sound::Upgrade);
        break;
    case PayGold:
    case PayShards:
        state.play.payShards = action == PayShards;
        sound.play(Sound::Select);
        break;
    case Repair:
        feedback.notify(run.repair(state.play.selectedTower), Sound::Upgrade);
        break;
    case ClearBoulder: {
        if (!state.play.selectedBoulder)
            break;
        const game::Result result = run.clearBoulder(*state.play.selectedBoulder);
        feedback.notify(result, Sound::Upgrade);
        if (result.ok)
            state.play.selectedBoulder.reset();
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
            state.play.selectedBoulder.reset();
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
        state.play.selectedBoulder.reset();
        state.play.movingUnit = false;
        state.play.selectedCard = 0;
        ui.cancel();
        break;
    case DescendMine:
        if (run.descentAvailability().ok) {
            state.play.modal = Modal::MineDescent;
            ui.cancel();
        } else {
            feedback.notify(run.descentAvailability());
        }
        break;
    case ConfirmDescend: {
        const game::Result result = run.descend();
        feedback.notify(result, Sound::Confirm);
        if (result.ok) {
            state.play.modal = Modal::None;
            state.play.scan = false;
            ui.cancel();
        }
        break;
    }
    case Scan:
        state.play.scan = !state.play.scan;
        state.play.mine = true;
        feedback.notify({true, !state.play.scan ? "스캔 선택을 취소했어요"
                               : run.catalog().mining == game::MiningMode::Excavate
                                   ? "속을 비춰 볼 암벽을 눌러 주세요"
                                   : "스캔할 경계 1칸을 눌러 주세요"});
        break;
    case ToolPick:
    case ToolBlast:
        state.play.blast = action == ToolBlast;
        state.play.scan = false;
        state.play.mine = true;
        sound.play(Sound::Select);
        feedback.notify({true, state.play.blast ? "발파: 3×3을 깎고 닿은 둥지를 깨워요"
                                                : "곡괭이: 한 칸을 깊게, 옆 네 칸을 얕게 깎아요"},
                        Sound::None);
        break;
    case Exchange: {
        const game::Result result = run.exchange(state.play.selectedCard);
        feedback.notify(result, Sound::None);
        if (result.ok)
            state.play.selectedCard = 0;
        break;
    }
    case Tool:
        feedback.notify(run.upgrade(run.state().tool < 2 ? thorium::game::UpgradeKind::Tool
                                                         : thorium::game::UpgradeKind::Drill),
                        Sound::Upgrade);
        break;
    case Hire:
        feedback.notify(run.upgrade(thorium::game::UpgradeKind::Hire), Sound::Upgrade);
        break;
    case Supply:
        feedback.notify(run.upgrade(thorium::game::UpgradeKind::Supply), Sound::Upgrade);
        break;
    case AwakenPick: {
        // Prefer an affordable candidate, then any open one, so the inspector shows a usable price.
        const game::Tower *chosen = nullptr;
        for (const game::Tower &tower : run.state().towers) {
            const game::PurchaseQuote price = run.awakenQuote(tower.id);
            if (price.open() && (!chosen || (price.allowed() && !run.awakenQuote(chosen->id).allowed())))
                chosen = &tower;
        }
        if (!chosen) {
            feedback.notify({false, "Lv3 건물이 없어요 · 같은 카드로 Lv3까지 강화해 주세요"});
            break;
        }
        state.play.mine = false;
        state.play.scan = false;
        state.play.selectedTower = chosen->id;
        state.play.selectedUnit = 0;
        state.play.selectedBoulder.reset();
        state.play.movingUnit = false;
        state.play.selectedCard = 0;
        feedback.notify({true, "각성할 건물을 고르고 각성 버튼을 눌러 주세요"});
        ui.cancel();
        break;
    }
    case Awaken:
        feedback.notify(run.awaken(state.play.selectedTower), Sound::Upgrade);
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
    case MoveUnit: {
        const game::Unit *selected = run.unit(state.play.selectedUnit);
        const bool charge = selected && selected->hero && run.state().phase == game::Phase::Battle &&
                            run.state().heroMove <= 0;
        if (selected && (run.state().phase == game::Phase::Prep || charge)) {
            state.play.movingUnit = true;
            feedback.notify({true, charge ? "영웅이 달려갈 배치 칸을 선택해 주세요"
                                          : "이동할 배치 칸을 선택해 주세요. 점유 칸이면 자리를 교환해요"});
        }
        break;
    }
    case ClearSelection:
        state.play.selectedUnit = 0;
        state.play.selectedTower = 0;
        state.play.selectedBoulder.reset();
        state.play.selectedCard = 0;
        state.play.movingUnit = false;
        break;
    case ToggleDrawer:
        state.play.guideOpen = !state.play.guideOpen;
        ui.cancel();
        break;
    case ToggleIntro:
        state.app.introFolded = !state.app.introFolded;
        break;
    case ToggleMinimap:
        state.app.minimapFolded = !state.app.minimapFolded;
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
                 state.play.selectedBoulder || state.play.scan) {
            state.play.selectedCard = 0;
            state.play.selectedUnit = 0;
            state.play.selectedTower = 0;
            state.play.selectedBoulder.reset();
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
        state.play.selectedBoulder.reset();
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
        state.play.selectedBoulder.reset();
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
    if (state.play.selectedBoulder && !run.boulderQuote(*state.play.selectedBoulder).open())
        state.play.selectedBoulder.reset();
}
} // namespace thorium
