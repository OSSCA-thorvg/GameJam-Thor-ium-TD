#include "RuleGeometry.h"
#include "Run.h"
#include "game/content/ContentKeys.h"
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

Message PlayPreview::message() const {
    switch (failure) {
    case PlayFailure::None:
        return Message("rules.cards.preview.ready");
    case PlayFailure::InvalidTarget:
        return Message("rules.cards.preview.invalid_target");
    case PlayFailure::BattleRequired:
        return Message("rules.cards.preview.battle_required");
    case PlayFailure::CardNotInHand:
        return Message("rules.cards.preview.not_in_hand");
    case PlayFailure::BuildingPadRequired:
        return Message("rules.cards.preview.pad_required");
    case PlayFailure::MaximumLevel:
        return Message("rules.cards.preview.max_level");
    case PlayFailure::UnitCellRequired:
        return Message("rules.cards.preview.unit_cell_required");
    case PlayFailure::SlotsFull:
        return Message("rules.cards.preview.slots_full");
    case PlayFailure::OccupiedCell:
        return Message("rules.cards.preview.occupied");
    case PlayFailure::OutsideBattlefield:
        return Message("rules.cards.preview.outside");
    case PlayFailure::EnergyShortage:
        return Message("rules.cards.preview.energy");
    case PlayFailure::PadBlocked:
        return Message("rules.cards.preview.pad_blocked");
    }
    return Message("rules.cards.preview.unavailable");
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
        // A card played onto a unit of its own kind raises that unit's level instead of summoning.
        for (std::size_t index = 0; index < state_.units.size(); ++index) {
            const Unit &unit = state_.units[index];
            if (distance(homeOf(unit), plan.target) >= .15F)
                continue;
            if (unit.hero || unit.kind != plan.kind) {
                plan.preview.failure = PlayFailure::OccupiedCell;
                return plan;
            }
            plan.unitIndex = static_cast<int>(index);
            plan.preview.action = PlayAction::Upgrade;
            plan.preview.cost = unit.level == 1 ? 2 : 3;
            if (unit.level >= CardMaxLevel) {
                plan.preview.failure = PlayFailure::MaximumLevel;
                return plan;
            }
            break;
        }
        if (plan.unitIndex < 0 && squadSize() >= state_.slots) {
            plan.preview.failure = PlayFailure::SlotsFull;
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
    if (plan.unitIndex >= 0) {
        Unit &existing = state_.units[static_cast<std::size_t>(plan.unitIndex)];
        ++existing.level;
        const int maximum = unitHealth(existing.kind, existing.level);
        existing.hp += maximum - existing.maxHp;
        existing.maxHp = maximum;
        emit({CombatEventKind::Placement, existing.id, target, target, kind, existing.level, false});
    } else if (plan.preview.action == PlayAction::Install || plan.preview.action == PlayAction::Upgrade ||
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
        const int hp = unitHealth(kind, 1);
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
    return {true, Message("rules.cards.played", {{"card", cardRef(kind)}})};
}

Result Run::exchange(CardId id) {
    if (state_.phase != Phase::Battle || state_.energy < 1)
        return {false, Message("rules.cards.exchange.closed")};
    for (int index = 0; index < 4; ++index)
        if (state_.deck[state_.hand[index]].id == id) {
            --state_.energy;
            cycle(index);
            return {true, Message("rules.cards.exchange.done")};
        }
    return {false, Message("rules.cards.exchange.not_in_hand")};
}

Result Run::replaceCard(CardKind kind, CardId old) {
    for (Card &item : state_.deck)
        if (item.id == old) {
            item = {nextCard_++, kind};
            acquisitions_.push_back(item.id);
            return {true, Message("rules.cards.acquired", {{"card", cardRef(kind)}})};
        }
    return {false, Message("rules.cards.replace.invalid")};
}

const CardOffer *Run::offer(OfferId id) const {
    if (state_.recruitmentOffer && state_.recruitmentOffer->id == id)
        return &*state_.recruitmentOffer;
    if (state_.rewardOffer && state_.rewardOffer->id == id)
        return &*state_.rewardOffer;
    return nullptr;
}
Result Run::acceptOffer(OfferId id, CardKind candidate, CardId old, bool shards) {
    if (state_.phase != Phase::Prep || state_.expansionPending ||
        (state_.rewardOffer && state_.rewardOffer->id != id))
        return {false, Message("rules.cards.offer.closed")};
    const CardOffer *active = offer(id);
    if (!active ||
        std::find(active->candidates.begin(), active->candidates.end(), candidate) ==
            active->candidates.end() ||
        !card(old))
        return {false, Message("rules.cards.offer.invalid")};
    if (shards && (active->shards <= 0 || state_.shards < active->shards))
        return {false, Message("rules.cards.offer.shards", {{"count", std::to_string(active->shards)}})};
    if (!shards && !canPay(active->cost))
        return {false, Message("rules.cards.offer.cost", {{"cost", costRef(active->cost)}})};
    const ResourceCost cost = shards ? ResourceCost{} : active->cost;
    const int shardCost = shards ? active->shards : 0;
    const OfferSource source = active->source;
    Result result = replaceCard(candidate, old);
    if (result.ok) {
        pay(cost);
        state_.shards -= shardCost;
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
        return {false, Message("rules.cards.skip.none")};
    state_.rewardOffer.reset();
    return {true, Message("rules.cards.skip.done")};
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
