#include "RuleGeometry.h"
#include "Run.h"
#include "game/content/ContentKeys.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace thorium::game {
Result Run::startWave() {
    if (state_.phase != Phase::Ready && state_.phase != Phase::Prep)
        return {false, Message("rules.progression.wave.closed")};
    if (state_.job.active)
        return {false, Message("rules.progression.wave.miner_busy")};
    if (state_.expansionPending || state_.rewardOffer)
        return {false, Message("rules.progression.wave.choice_pending")};
    if (state_.wave >= static_cast<int>(catalog_.waves.size()))
        return {false, Message("rules.progression.wave.finished")};
    if (state_.phase == Phase::Prep)
        rebuildHand();
    ++state_.wave;
    state_.recruitmentOffer.reset();
    state_.phase = Phase::Battle;
    state_.energy = nextBattleEnergy();
    state_.bonusEnergy = 0;
    state_.energyTime = 0;
    state_.battleTime = 0;
    state_.fields.clear();
    resetMachines();
    nextSpawn_ = 0;
    spawns_.clear();
    spawns_ = catalog_.schedule(state_.wave);
    const float extraStart = (spawns_.empty() ? 0.0F : spawns_.back().time) + 1.2F;
    for (int index = 0; index < state_.nextExtra; ++index)
        spawns_.push_back({extraStart + index * 0.6F, EnemyKind::Normal});
    state_.nextExtra = 0;
    return {true, Message("rules.progression.wave.started")};
}

void Run::finishWave() {
    state_.fields.clear();
    state_.heroMove = 0;
    if (state_.heroDown > 0)
        reviveHero();
    for (Unit &ally : state_.units)
        ally.position = homeOf(ally);
    for (Unit &ally : state_.units)
        if (ally.hero)
            ally.hp = ally.maxHp;
    barricadeHp_.assign(catalog_.forks.size(), catalog_.balance.barricadeHp);
    if (state_.wave >= static_cast<int>(catalog_.waves.size())) {
        state_.phase = Phase::Won;
        return;
    }
    state_.phase = Phase::Prep;
    state_.digs = prepDigBudget();
    state_.scans = prepScanBudget();
    state_.hires = 0;
    state_.supplied = false;
    const std::vector<CardKind> candidates = recruitmentDraw(state_.wave);
    state_.recruitmentOffer = CardOffer{nextOffer_++, OfferSource::Recruitment, candidates,
                                        catalog_.balance.recruit, catalog_.balance.recruitShards};
    state_.expansionPending = state_.wave == catalog_.balance.expansionWave;
}

std::vector<CardKind> Run::recruitmentDraw(int turn) const {
    // Two of the three units and two buildings, both rotating with the turn.
    std::vector<CardKind> candidates(catalog_.recruitmentCandidates.begin(),
                                     catalog_.recruitmentCandidates.end());
    const std::vector<CardKind> &buildings = catalog_.recruitmentBuildings;
    if (buildings.empty())
        return candidates;
    const std::size_t step = static_cast<std::size_t>(std::max(0, turn));
    candidates.erase(candidates.begin() + static_cast<std::ptrdiff_t>(step % candidates.size()));
    candidates.push_back(buildings[step % buildings.size()]);
    candidates.push_back(buildings[(step + buildings.size() / 2 + 1) % buildings.size()]);
    if (candidates[2] == candidates[3])
        candidates[3] = buildings[(step + 1) % buildings.size()];
    return candidates;
}
Result Run::rerollRecruitment() {
    const int price = catalog_.balance.rerollShards;
    if (state_.phase != Phase::Prep || !state_.recruitmentOffer)
        return {false, Message("rules.progression.reroll.closed")};
    if (state_.shards < price)
        return {false, Message("rules.progression.shards", {{"count", std::to_string(price)}})};
    state_.shards -= price;
    ++state_.rerolls;
    const std::vector<CardKind> previous = state_.recruitmentOffer->candidates;
    std::vector<CardKind> next = recruitmentDraw(state_.wave + state_.rerolls * 5);
    for (int shift = 1; next == previous && shift < 12; ++shift)
        next = recruitmentDraw(state_.wave + state_.rerolls * 5 + shift);
    state_.recruitmentOffer->candidates = next;
    state_.recruitmentOffer->id = nextOffer_++;
    return {true, Message("rules.progression.reroll.done", {{"count", std::to_string(price)}})};
}
int Run::heroHealth() const {
    return static_cast<int>(std::lround(catalog_.balance.heroHp *
                                        (1 + catalog_.balance.heroLevelBonus * (state_.heroLevel - 1))));
}
int Run::heroTrainCost() const {
    return state_.heroLevel >= catalog_.balance.heroMaxLevel
               ? 0
               : catalog_.balance.heroTrainShards * state_.heroLevel;
}
Result Run::trainHero() {
    const int price = heroTrainCost();
    if (state_.phase != Phase::Prep || !catalog_.hero)
        return {false, Message("rules.progression.train.closed")};
    if (price == 0)
        return {false, Message("rules.progression.train.max")};
    if (state_.shards < price)
        return {false, Message("rules.progression.shards", {{"count", std::to_string(price)}})};
    state_.shards -= price;
    ++state_.heroLevel;
    for (Unit &ally : state_.units)
        if (ally.hero) {
            ally.maxHp = heroHealth();
            ally.hp = ally.maxHp;
        }
    return {true, Message("rules.progression.train.done",
                          {{"level", std::to_string(state_.heroLevel)},
                           {"bonus", std::to_string(static_cast<int>(
                                         std::lround(catalog_.balance.heroLevelBonus * 100)))}})};
}
int Run::nextBattleEnergy() const {
    const RunBalance &balance = catalog_.balance;
    return std::min(balance.energyCap, balance.initialEnergy + state_.bonusEnergy +
                                           (hasRelic(Relic::Battery) ? balance.relicEnergy : 0));
}

PurchaseQuote Run::quote(UpgradeKind kind) const {
    PurchaseQuote result;
    /** Record one rejection reason and return the finished quote. */
    const auto reject = [&result](PurchaseFailure failure, Message message) -> PurchaseQuote {
        result.failure = failure;
        result.message = std::move(message);
        return result;
    };
    if (state_.phase != Phase::Prep || state_.expansionPending || state_.rewardOffer)
        return reject(PurchaseFailure::Closed, Message("rules.progression.upgrade.closed"));
    const RunBalance &balance = catalog_.balance;
    switch (kind) {
    case UpgradeKind::Tool:
        result.cost = balance.tool;
        if (state_.tool >= 2)
            return reject(PurchaseFailure::Owned, Message("rules.progression.upgrade.tool_owned"));
        break;
    case UpgradeKind::Drill:
        result.cost = balance.drill;
        if (state_.tool >= 3)
            return reject(PurchaseFailure::Owned, Message("rules.progression.upgrade.drill_owned"));
        if (state_.tool < 2)
            return reject(PurchaseFailure::Requirement, Message("rules.progression.upgrade.drill_requires"));
        break;
    case UpgradeKind::Armor:
        result.cost = balance.armor;
        if (state_.armor)
            return reject(PurchaseFailure::Owned, Message("rules.progression.upgrade.armor_owned"));
        break;
    case UpgradeKind::Slots:
        result.cost = balance.slots;
        if (state_.slots >= balance.upgradedSlots)
            return reject(PurchaseFailure::Owned,
                          Message("rules.progression.upgrade.slots_owned",
                                  {{"count", std::to_string(balance.upgradedSlots)}}));
        break;
    case UpgradeKind::Coil: {
        result.cost = balance.coil;
        if (state_.coil)
            return reject(PurchaseFailure::Owned, Message("rules.progression.upgrade.coil_owned"));
        bool owns = false;
        for (const Card &item : state_.deck)
            owns |= item.kind == CardKind::Lightning;
        if (state_.tool < 2 || !owns)
            return reject(PurchaseFailure::Requirement, Message("rules.progression.upgrade.coil_requires"));
        break;
    }
    case UpgradeKind::Hire:
        result.cost = {0, 0, 0, balance.hireGold + balance.hireStep * state_.hires};
        if (state_.hires >= balance.hireLimit)
            return reject(PurchaseFailure::Exhausted, Message("rules.progression.upgrade.hire_exhausted"));
        break;
    case UpgradeKind::Supply:
        result.cost = balance.supply;
        if (state_.supplied)
            return reject(PurchaseFailure::Exhausted, Message("rules.progression.upgrade.supply_exhausted"));
        if (nextBattleEnergy() >= balance.energyCap)
            return reject(PurchaseFailure::Exhausted, Message("rules.progression.upgrade.energy_full"));
        break;
    default:
        return reject(PurchaseFailure::InvalidTarget, Message("rules.progression.upgrade.unknown"));
    }
    if (!canPay(result.cost))
        return reject(PurchaseFailure::Shortage,
                      Message("rules.progression.cost", {{"cost", costRef(result.cost)}}));
    result.failure = PurchaseFailure::None;
    return result;
}

Result Run::upgrade(UpgradeKind kind) {
    const PurchaseQuote price = quote(kind);
    if (!price.allowed())
        return {false, price.message};
    pay(price.cost);
    const RunBalance &balance = catalog_.balance;
    switch (kind) {
    case UpgradeKind::Tool:
        state_.tool = 2;
        return {true, Message("rules.progression.upgrade.tool_done")};
    case UpgradeKind::Drill:
        state_.tool = 3;
        state_.digs += balance.drillDigs;
        return {true, Message("rules.progression.upgrade.drill_done",
                              {{"count", std::to_string(balance.drillDigs)}})};
    case UpgradeKind::Armor:
        state_.armor = true;
        for (Unit &ally : state_.units)
            if (ally.kind == CardKind::Guard && !ally.hero)
                ally.maxHp = unitHealth(ally.kind, ally.level);
        return {true, Message("rules.progression.upgrade.armor_done",
                              {{"hp", std::to_string(balance.armoredGuardHp)}})};
    case UpgradeKind::Slots:
        state_.slots = balance.upgradedSlots;
        return {true, Message("rules.progression.upgrade.slots_done",
                              {{"count", std::to_string(balance.upgradedSlots)}})};
    case UpgradeKind::Coil:
        state_.coil = true;
        return {true,
                Message("rules.progression.upgrade.coil_done",
                        {{"bonus",
                          std::to_string(static_cast<int>(std::lround((balance.coilBonus - 1) * 100)))}})};
    case UpgradeKind::Hire:
        ++state_.hires;
        ++state_.digs;
        return {true, Message("rules.progression.upgrade.hire_done")};
    case UpgradeKind::Supply:
        state_.supplied = true;
        state_.bonusEnergy += balance.supplyEnergy;
        return {true, Message("rules.progression.upgrade.supply_done",
                              {{"count", std::to_string(balance.supplyEnergy)}})};
    }
    return {false, Message("rules.progression.upgrade.unknown")};
}

int Run::awakenedLevel() const {
    return CardMaxLevel + static_cast<int>(catalog_.balance.awaken.size());
}

PurchaseQuote Run::awakenQuote(EntityId id) const {
    PurchaseQuote result;
    /** Record one rejection reason and return the finished quote. */
    const auto reject = [&result](PurchaseFailure failure, Message message) -> PurchaseQuote {
        result.failure = failure;
        result.message = std::move(message);
        return result;
    };
    if (state_.phase != Phase::Prep || state_.expansionPending || state_.rewardOffer)
        return reject(PurchaseFailure::Closed, Message("rules.progression.awaken.closed"));
    const Tower *target = tower(id);
    if (!target)
        return reject(PurchaseFailure::InvalidTarget, Message("rules.progression.awaken.invalid"));
    const std::array<ResourceCost, 2> &prices = catalog_.balance.awaken;
    const int step = std::max(0, target->level - CardMaxLevel);
    result.cost = prices[static_cast<std::size_t>(std::min(step, static_cast<int>(prices.size()) - 1))];
    if (target->level < CardMaxLevel)
        return reject(PurchaseFailure::Requirement, Message("rules.progression.awaken.requires",
                                                            {{"level", std::to_string(CardMaxLevel)}}));
    if (target->level >= awakenedLevel())
        return reject(PurchaseFailure::Owned, Message("rules.progression.awaken.max"));
    if (!canPay(result.cost))
        return reject(PurchaseFailure::Shortage,
                      Message("rules.progression.cost", {{"cost", costRef(result.cost)}}));
    result.failure = PurchaseFailure::None;
    return result;
}

Result Run::awaken(EntityId id) {
    const PurchaseQuote price = awakenQuote(id);
    if (!price.allowed())
        return {false, price.message};
    pay(price.cost);
    for (Tower &item : state_.towers)
        if (item.id == id) {
            ++item.level;
            const int maximum = catalog_.towerHealth(item.level);
            item.hp += maximum - item.maxHp;
            item.maxHp = maximum;
            const int bonus =
                static_cast<int>(std::lround((item.level - 1) * catalog_.balance.levelBonus * 100));
            return {true, Message("rules.progression.awaken.done", {{"card", cardRef(item.kind)},
                                                                    {"level", std::to_string(item.level)},
                                                                    {"bonus", std::to_string(bonus)}})};
        }
    return {false, Message("rules.progression.awaken.invalid")};
}

PurchaseQuote Run::repairQuote(EntityId id) const {
    PurchaseQuote result;
    const Tower *building = tower(id);
    if (building && building->maxHp > building->hp)
        result.cost.iron =
            (building->maxHp - building->hp + catalog_.balance.repairHp - 1) / catalog_.balance.repairHp;
    if (state_.phase != Phase::Prep || state_.expansionPending || state_.rewardOffer) {
        result.failure = PurchaseFailure::Closed;
        result.message = Message("rules.progression.repair.closed");
    } else if (!building) {
        result.failure = PurchaseFailure::InvalidTarget;
        result.message = Message("rules.progression.repair.invalid");
    } else if (building->hp >= building->maxHp) {
        result.failure = PurchaseFailure::Owned;
        result.message = Message("rules.progression.repair.full");
    } else if (!canPay(result.cost)) {
        result.failure = PurchaseFailure::Shortage;
        result.message = Message("rules.progression.cost", {{"cost", costRef(result.cost)}});
    } else
        result.failure = PurchaseFailure::None;
    return result;
}
Result Run::repair(EntityId id) {
    const PurchaseQuote price = repairQuote(id);
    if (!price.allowed())
        return {false, price.message};
    pay(price.cost);
    for (Tower &item : state_.towers)
        if (item.id == id)
            item.hp = item.maxHp;
    return {true, Message("rules.progression.repair.done", {{"cost", costRef(price.cost)}})};
}
PurchaseQuote Run::boulderQuote(Point position) const {
    PurchaseQuote result;
    /** Record one rejection reason and return the finished quote. */
    const auto reject = [&result](PurchaseFailure failure, Message message) -> PurchaseQuote {
        result.failure = failure;
        result.message = std::move(message);
        return result;
    };
    result.cost = catalog_.balance.boulder;
    if (state_.phase != Phase::Prep || state_.expansionPending || state_.rewardOffer)
        return reject(PurchaseFailure::Closed, Message("rules.progression.boulder.closed"));
    if (!resolveCell(state_.boulders, position))
        return reject(PurchaseFailure::InvalidTarget, Message("rules.progression.boulder.invalid"));
    if (!canPay(result.cost))
        return reject(PurchaseFailure::Shortage,
                      Message("rules.progression.cost", {{"cost", costRef(result.cost)}}));
    result.failure = PurchaseFailure::None;
    return result;
}

Result Run::clearBoulder(Point position) {
    const PurchaseQuote price = boulderQuote(position);
    if (!price.allowed())
        return {false, price.message};
    const Point pad = *resolveCell(state_.boulders, position);
    pay(price.cost);
    state_.boulders.erase(std::remove_if(state_.boulders.begin(), state_.boulders.end(),
                                         [pad](Point boulder) { return distance(boulder, pad) < .01F; }),
                          state_.boulders.end());
    state_.pads.push_back(pad);
    return {true, Message("rules.progression.boulder.done")};
}

Result Run::heal(EntityId id) {
    if (state_.phase != Phase::Prep)
        return {false, Message("rules.progression.heal.closed")};
    for (Unit &ally : state_.units)
        if (ally.id == id) {
            if (ally.hero)
                return {false, Message("rules.progression.heal.hero")};
            if (ally.hp == ally.maxHp)
                return {false, Message("rules.progression.heal.full")};
            if (!canPay(catalog_.balance.heal))
                return {false, Message("rules.progression.cost", {{"cost", costRef(catalog_.balance.heal)}})};
            pay(catalog_.balance.heal);
            ally.hp = std::min(ally.maxHp, ally.hp + (ally.maxHp + 1) / 2);
            return {true, Message("rules.progression.heal.done")};
        }
    return {false, Message("rules.progression.heal.invalid")};
}

Result Run::expand(int choice) {
    if (state_.phase != Phase::Prep || !state_.expansionPending || (choice != 1 && choice != 2))
        return {false, Message("rules.progression.expand.invalid")};
    state_.branch = choice;
    const ExpansionDefinition &expansion = catalog_.expansions.at(static_cast<std::size_t>(choice - 1));
    const std::vector<Point> core = state_.route;
    state_.route = expansion.prefix;
    state_.route.insert(state_.route.end(), core.begin(), core.end());
    state_.pads.insert(state_.pads.end(), expansion.pads.begin(), expansion.pads.end());
    state_.unitCells.insert(state_.unitCells.end(), expansion.unitCells.begin(), expansion.unitCells.end());
    resolveTunnels();
    state_.expansionPending = false;
    state_.rewardOffer = CardOffer{nextOffer_++, OfferSource::Expansion, {expansion.reward}, {}};
    return {true, Message("rules.progression.expand.done", {{"card", cardRef(expansion.reward)}})};
}

bool Run::canPay(const ResourceCost &cost) const {
    return state_.iron >= cost.iron && state_.copper >= cost.copper && state_.crystal >= cost.crystal &&
           state_.gold >= cost.gold;
}
void Run::pay(const ResourceCost &cost) {
    state_.iron -= cost.iron;
    state_.copper -= cost.copper;
    state_.crystal -= cost.crystal;
    state_.gold -= cost.gold;
}
} // namespace thorium::game
