#include "RuleGeometry.h"
#include "Run.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace thorium::game {
const Card *Run::card(CardId id) const {
    for (const Card &item : state_.deck)
        if (item.id == id)
            return &item;
    return nullptr;
}

void Run::cycle(int position) {
    const int used = state_.hand[position];
    state_.hand[position] = state_.queue.front();
    state_.queue.pop_front();
    state_.queue.push_back(used);
}

const char *PlayPreview::message() const {
    switch (failure) {
    case PlayFailure::None:
        return "사용할 수 있어요";
    case PlayFailure::InvalidTarget:
        return "유효한 전장 위치를 선택해 주세요";
    case PlayFailure::BattleRequired:
        return "전투 시작 후 카드를 사용할 수 있어요";
    case PlayFailure::CardNotInHand:
        return "현재 손패의 카드를 선택해 주세요";
    case PlayFailure::BuildingPadRequired:
        return "둥근 건물 패드에 설치해 주세요";
    case PlayFailure::MaximumLevel:
        return "이미 최고 레벨이에요";
    case PlayFailure::UnitCellRequired:
        return "길 위의 작은 배치 칸에 놓아 주세요";
    case PlayFailure::SlotsFull:
        return "유닛 슬롯이 가득 찼어요";
    case PlayFailure::OccupiedCell:
        return "다른 유닛이 있는 칸이에요";
    case PlayFailure::OutsideBattlefield:
        return "열린 전장 안을 선택해 주세요";
    case PlayFailure::EnergyShortage:
        return "에너지가 부족해요";
    case PlayFailure::PadBlocked:
        return "바위가 막은 패드예요 · 정비 중에 치워 주세요";
    }
    return "현재는 사용할 수 없어요";
}
Run::PlayPlan Run::planPlay(CardId id, Point target) const {
    PlayPlan plan;
    if (!std::isfinite(target.x) || !std::isfinite(target.y)) {
        plan.preview.failure = PlayFailure::InvalidTarget;
        return plan;
    }
    plan.target = target;
    if (state_.phase != Phase::Battle) {
        plan.preview.failure = PlayFailure::BattleRequired;
        return plan;
    }
    for (int index = 0; index < 4; ++index)
        if (state_.deck[state_.hand[index]].id == id)
            plan.handPosition = index;
    if (plan.handPosition < 0)
        return plan;
    plan.kind = state_.deck[state_.hand[plan.handPosition]].kind;
    plan.preview.cost = catalog_.card(plan.kind).cost;
    if (catalog_.isBuilding(plan.kind)) {
        plan.preview.action = PlayAction::Install;
        const std::optional<Point> pad = resolveCell(state_.pads, target);
        if (!pad) {
            plan.preview.failure = resolveCell(state_.boulders, target) ? PlayFailure::PadBlocked
                                                                        : PlayFailure::BuildingPadRequired;
            return plan;
        }
        plan.target = *pad;
        for (std::size_t index = 0; index < state_.towers.size(); ++index)
            if (distance(state_.towers[index].position, plan.target) < .15F)
                plan.towerIndex = static_cast<int>(index);
        if (plan.towerIndex >= 0) {
            const Tower &existing = state_.towers[plan.towerIndex];
            if (existing.kind == plan.kind) {
                plan.preview.action = PlayAction::Upgrade;
                plan.preview.cost = existing.level == 1 ? 2 : 3;
                if (existing.level >= CardMaxLevel) {
                    plan.preview.failure = PlayFailure::MaximumLevel;
                    return plan;
                }
            } else
                plan.preview.action = PlayAction::Replace;
        }
    } else if (catalog_.isUnit(plan.kind)) {
        plan.preview.action = PlayAction::Summon;
        const std::optional<Point> cell = resolveCell(state_.unitCells, target);
        if (!cell) {
            plan.preview.failure = PlayFailure::UnitCellRequired;
            return plan;
        }
        plan.target = *cell;
        if (squadSize() >= state_.slots) {
            plan.preview.failure = PlayFailure::SlotsFull;
            return plan;
        }
        for (const Unit &unit : state_.units)
            if (distance(unit.position, plan.target) < .15F) {
                plan.preview.failure = PlayFailure::OccupiedCell;
                return plan;
            }
    } else {
        plan.preview.action = PlayAction::Cast;
        if (target.x < 0 || target.x > catalog_.width - 1 || target.y < 0 || target.y > catalog_.height - 1) {
            plan.preview.failure = PlayFailure::OutsideBattlefield;
            return plan;
        }
    }
    plan.preview.failure =
        state_.energy < plan.preview.cost ? PlayFailure::EnergyShortage : PlayFailure::None;
    return plan;
}
PlayPreview Run::previewPlay(CardId id, Point target) const {
    return planPlay(id, target).preview;
}
PlayAvailability Run::playAvailability(CardId id) const {
    PlayAvailability result;
    const Card *selected = card(id);
    if (selected) {
        result.minimumCost = catalog_.card(selected->kind).cost;
        result.maximumCost = result.minimumCost;
    }
    const PlayPlan initial = planPlay(id, {});
    result.failure = initial.preview.failure;
    if (initial.handPosition < 0)
        return result;
    result.failure = catalog_.isBuilding(initial.kind) ? PlayFailure::BuildingPadRequired
                     : catalog_.isUnit(initial.kind)   ? PlayFailure::UnitCellRequired
                                                       : PlayFailure::OutsideBattlefield;
    bool first = true;
    /** Aggregate current target answers without allocating candidate lists or duplicating placement rules. */
    const auto inspect = [this, id, &result, &first](Point target) -> void {
        const PlayPreview preview = previewPlay(id, target);
        if (first) {
            result.failure = preview.failure;
            first = false;
        }
        if (preview.allowed() || preview.failure == PlayFailure::EnergyShortage) {
            if (!result.hasTarget)
                result.minimumCost = result.maximumCost = preview.cost;
            else {
                result.minimumCost = std::min(result.minimumCost, preview.cost);
                result.maximumCost = std::max(result.maximumCost, preview.cost);
            }
            result.hasTarget = true;
            result.allowed = result.allowed || preview.allowed();
        }
    };
    if (catalog_.isBuilding(initial.kind)) {
        for (Point target : state_.pads)
            inspect(target);
    } else if (catalog_.isUnit(initial.kind)) {
        for (Point target : state_.unitCells)
            inspect(target);
    } else
        inspect(state_.route.front());
    if (result.hasTarget)
        result.failure = result.allowed ? PlayFailure::None : PlayFailure::EnergyShortage;
    return result;
}
Result Run::play(CardId id, Point target) {
    const PlayPlan plan = planPlay(id, target);
    if (!plan.preview.allowed())
        return {false, plan.preview.message()};
    target = plan.target;
    const CardKind kind = plan.kind;
    if (plan.preview.action == PlayAction::Install || plan.preview.action == PlayAction::Upgrade ||
        plan.preview.action == PlayAction::Replace) {
        if (plan.preview.action == PlayAction::Install) {
            state_.towers.push_back({nextEntity_++, kind, target});
            state_.towers.back().maxHp = catalog_.towerHealth(1);
            state_.towers.back().hp = state_.towers.back().maxHp;
        } else {
            Tower &existing = state_.towers[plan.towerIndex];
            if (plan.preview.action == PlayAction::Upgrade) {
                ++existing.level;
                const int maximum = catalog_.towerHealth(existing.level);
                existing.hp += maximum - existing.maxHp;
                existing.maxHp = maximum;
            } else {
                existing.kind = kind;
                existing.level = 1;
                existing.cooldown = 0;
                existing.maxHp = catalog_.towerHealth(1);
                existing.hp = existing.maxHp;
            }
        }
        const Tower &placed = plan.preview.action == PlayAction::Install ? state_.towers.back()
                                                                         : state_.towers[plan.towerIndex];
        emit({CombatEventKind::Placement, placed.id, target, target, kind, placed.level, false});
    } else if (plan.preview.action == PlayAction::Summon) {
        const int hp = kind == CardKind::Guard && state_.armor ? catalog_.balance.armoredGuardHp
                                                               : catalog_.card(kind).hp;
        state_.units.push_back({nextEntity_++, kind, target, hp, hp});
        emit({CombatEventKind::Placement, state_.units.back().id, target, target, kind, 1, false});
    } else if (kind == CardKind::Tar) {
        const CardDefinition &definition = catalog_.card(kind);
        state_.fields.push_back(
            {nextEntity_++, kind, target, definition.range, state_.battleTime + definition.slowDuration});
        emit({CombatEventKind::Attack, 0, target, target, kind, 0, false, definition.range});
    } else {
        const std::vector<std::size_t> guards = auraSources();
        for (Enemy &enemy : state_.enemies)
            if (!underground(enemy) && distance(enemy.position, target) <= catalog_.card(kind).range) {
                if (kind == CardKind::Frost)
                    enemy.slowUntil = state_.battleTime + catalog_.card(kind).slowDuration;
                else {
                    const int damage =
                        damageEnemy(enemy, mitigate(enemy, catalog_.card(kind).damage, false, guards));
                    emit({CombatEventKind::Damage, enemy.id, target, enemy.position, kind, damage, true});
                }
            }
        emit({CombatEventKind::Attack, 0, target, target, kind, 0, false, catalog_.card(kind).range});
        removeDefeated();
    }
    state_.energy -= plan.preview.cost;
    cycle(plan.handPosition);
    return {true, std::string(catalog_.card(kind).name) + " 사용!"};
}

Result Run::exchange(CardId id) {
    if (state_.phase != Phase::Battle || state_.energy < 1)
        return {false, "전투 중 에너지 1로 교환할 수 있어요"};
    for (int index = 0; index < 4; ++index)
        if (state_.deck[state_.hand[index]].id == id) {
            --state_.energy;
            cycle(index);
            return {true, "카드를 순환했어요"};
        }
    return {false, "현재 손패를 선택해 주세요"};
}

Result Run::replaceCard(CardKind kind, CardId old) {
    for (Card &item : state_.deck)
        if (item.id == old) {
            item = {nextCard_++, kind};
            acquisitions_.push_back(item.id);
            return {true, std::string(catalog_.card(kind).name) + " 카드를 획득했어요"};
        }
    return {false, "교체할 활성 덱 카드를 선택해 주세요"};
}

const CardOffer *Run::offer(OfferId id) const {
    if (state_.recruitmentOffer && state_.recruitmentOffer->id == id)
        return &*state_.recruitmentOffer;
    if (state_.rewardOffer && state_.rewardOffer->id == id)
        return &*state_.rewardOffer;
    return nullptr;
}
Result Run::acceptOffer(OfferId id, CardKind candidate, CardId old) {
    if (state_.phase != Phase::Prep || state_.expansionPending ||
        (state_.rewardOffer && state_.rewardOffer->id != id))
        return {false, "먼저 현재 정비의 확장과 보상을 확인해 주세요"};
    const CardOffer *active = offer(id);
    if (!active ||
        std::find(active->candidates.begin(), active->candidates.end(), candidate) ==
            active->candidates.end() ||
        !card(old))
        return {false, "현재 제시된 후보와 교체할 활성 덱 카드를 선택해 주세요"};
    if (!canPay(active->cost))
        return {false, costLabel(active->cost) + "가 필요해요"};
    const ResourceCost cost = active->cost;
    const OfferSource source = active->source;
    Result result = replaceCard(candidate, old);
    if (result.ok) {
        pay(cost);
        if (source == OfferSource::Recruitment)
            state_.recruitmentOffer.reset();
        else
            state_.rewardOffer.reset();
    }
    return result;
}
Result Run::skipOffer(OfferId id) {
    if (state_.phase != Phase::Prep || state_.expansionPending || !state_.rewardOffer ||
        state_.rewardOffer->id != id)
        return {false, "현재 선택할 확장 보상이 없어요"};
    state_.rewardOffer.reset();
    return {true, "보상을 건너뛰었어요"};
}

void Run::rebuildHand() {
    std::vector<int> chosen;
    for (auto it = acquisitions_.rbegin(); it != acquisitions_.rend(); ++it) {
        for (int index = 0; index < 8; ++index)
            if (state_.deck[index].id == *it)
                chosen.push_back(index);
        if (!chosen.empty())
            break;
    }
    for (int group = 0; group < 3; ++group) {
        bool included = false;
        for (int index : chosen)
            included |= static_cast<int>(catalog_.card(state_.deck[index].kind).category) == group;
        if (!included)
            for (int index = 0; index < 8; ++index)
                if (static_cast<int>(catalog_.card(state_.deck[index].kind).category) == group &&
                    std::find(chosen.begin(), chosen.end(), index) == chosen.end()) {
                    chosen.push_back(index);
                    break;
                }
    }
    for (int index = 0; index < 8 && chosen.size() < 4; ++index)
        if (std::find(chosen.begin(), chosen.end(), index) == chosen.end())
            chosen.push_back(index);
    for (int index = 0; index < 4; ++index)
        state_.hand[index] = chosen[index];
    state_.queue.clear();
    for (int index = 0; index < 8; ++index)
        if (std::find(chosen.begin(), chosen.end(), index) == chosen.end())
            state_.queue.push_back(index);
}
} // namespace thorium::game
