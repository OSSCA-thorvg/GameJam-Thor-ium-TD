#include "RuleGeometry.h"
#include "Run.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace thorium::game {
Result Run::startWave() {
    if (state_.phase != Phase::Ready && state_.phase != Phase::Prep)
        return {false, "지금은 전투를 시작할 수 없어요"};
    if (state_.job.active)
        return {false, "광부가 작업을 마칠 때까지 기다려 주세요"};
    if (state_.expansionPending || state_.rewardOffer)
        return {false, "먼저 맵 확장과 보상을 선택해 주세요"};
    if (state_.wave >= static_cast<int>(catalog_.waves.size()))
        return {false, "마지막 웨이브를 이미 완료했어요"};
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
    return {true, "카드를 끌어다 전장에 배치하세요"};
}

void Run::finishWave() {
    state_.fields.clear();
    if (state_.wave >= static_cast<int>(catalog_.waves.size())) {
        state_.phase = Phase::Won;
        return;
    }
    state_.phase = Phase::Prep;
    state_.digs = prepDigBudget();
    state_.scans = prepScanBudget();
    state_.hires = 0;
    state_.supplied = false;
    state_.recruitmentOffer = CardOffer{
        nextOffer_++, OfferSource::Recruitment,
        std::vector<CardKind>(catalog_.recruitmentCandidates.begin(), catalog_.recruitmentCandidates.end()),
        catalog_.balance.recruit};
    state_.expansionPending = state_.wave == catalog_.balance.expansionWave;
}

int Run::nextBattleEnergy() const {
    const RunBalance &balance = catalog_.balance;
    return std::min(balance.energyCap, balance.initialEnergy + state_.bonusEnergy +
                                           (hasRelic(Relic::Battery) ? balance.relicEnergy : 0));
}

PurchaseQuote Run::quote(UpgradeKind kind) const {
    PurchaseQuote result;
    /** Record one rejection reason and return the finished quote. */
    const auto reject = [&result](PurchaseFailure failure, std::string message) -> PurchaseQuote {
        result.failure = failure;
        result.message = std::move(message);
        return result;
    };
    if (state_.phase != Phase::Prep || state_.expansionPending || state_.rewardOffer)
        return reject(PurchaseFailure::Closed, "정비 중에 강화할 수 있어요");
    const RunBalance &balance = catalog_.balance;
    switch (kind) {
    case UpgradeKind::Tool:
        result.cost = balance.tool;
        if (state_.tool >= 2)
            return reject(PurchaseFailure::Owned, "이미 T2 곡괭이예요");
        break;
    case UpgradeKind::Drill:
        result.cost = balance.drill;
        if (state_.tool >= 3)
            return reject(PurchaseFailure::Owned, "이미 T3 드릴이에요");
        if (state_.tool < 2)
            return reject(PurchaseFailure::Requirement, "T2 곡괭이가 먼저 필요해요");
        break;
    case UpgradeKind::Armor:
        result.cost = balance.armor;
        if (state_.armor)
            return reject(PurchaseFailure::Owned, "이미 장갑을 제작했어요");
        break;
    case UpgradeKind::Slots:
        result.cost = balance.slots;
        if (state_.slots >= balance.upgradedSlots)
            return reject(PurchaseFailure::Owned,
                          "이미 최대 " + std::to_string(balance.upgradedSlots) + "슬롯이에요");
        break;
    case UpgradeKind::Coil: {
        result.cost = balance.coil;
        if (state_.coil)
            return reject(PurchaseFailure::Owned, "이미 코일을 제작했어요");
        bool owns = false;
        for (const Card &item : state_.deck)
            owns |= item.kind == CardKind::Lightning;
        if (state_.tool < 2 || !owns)
            return reject(PurchaseFailure::Requirement, "T2 곡괭이와 활성 덱의 전격탑이 필요해요");
        break;
    }
    case UpgradeKind::Hire:
        result.cost = {0, 0, 0, balance.hireGold + balance.hireStep * state_.hires};
        if (state_.hires >= balance.hireLimit)
            return reject(PurchaseFailure::Exhausted, "이번 정비의 광부 고용을 모두 썼어요");
        break;
    case UpgradeKind::Supply:
        result.cost = balance.supply;
        if (state_.supplied)
            return reject(PurchaseFailure::Exhausted, "이번 정비의 보급을 이미 받았어요");
        if (nextBattleEnergy() >= balance.energyCap)
            return reject(PurchaseFailure::Exhausted, "다음 전투 에너지가 이미 최대예요");
        break;
    default:
        return reject(PurchaseFailure::InvalidTarget, "알 수 없는 강화예요");
    }
    if (!canPay(result.cost))
        return reject(PurchaseFailure::Shortage, costLabel(result.cost) + "가 필요해요");
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
        return {true, "단단한 지층과 수정을 캘 수 있어요"};
    case UpgradeKind::Drill:
        state_.tool = 3;
        state_.digs += balance.drillDigs;
        return {true,
                "정비마다 굴착 +" + std::to_string(balance.drillDigs) + " · 이번 정비에도 바로 적용돼요"};
    case UpgradeKind::Armor:
        state_.armor = true;
        for (Unit &ally : state_.units)
            if (ally.kind == CardKind::Guard)
                ally.maxHp = balance.armoredGuardHp;
        return {true,
                "경비병 최대 체력 " + std::to_string(balance.armoredGuardHp) + " · 현재 체력은 유지돼요"};
    case UpgradeKind::Slots:
        state_.slots = balance.upgradedSlots;
        return {true, "유닛 슬롯이 " + std::to_string(balance.upgradedSlots) + "개로 늘었어요"};
    case UpgradeKind::Coil:
        state_.coil = true;
        return {true, "모든 전격탑 피해 +" +
                          std::to_string(static_cast<int>(std::lround((balance.coilBonus - 1) * 100))) + "%"};
    case UpgradeKind::Hire:
        ++state_.hires;
        ++state_.digs;
        return {true, "광부를 고용했어요 · 이번 정비 굴착 +1"};
    case UpgradeKind::Supply:
        state_.supplied = true;
        state_.bonusEnergy += balance.supplyEnergy;
        return {true, "보급 상자 · 다음 전투 에너지 +" + std::to_string(balance.supplyEnergy)};
    }
    return {false, "알 수 없는 강화예요"};
}

int Run::awakenedLevel() const {
    return CardMaxLevel + static_cast<int>(catalog_.balance.awaken.size());
}

PurchaseQuote Run::awakenQuote(EntityId id) const {
    PurchaseQuote result;
    /** Record one rejection reason and return the finished quote. */
    const auto reject = [&result](PurchaseFailure failure, std::string message) -> PurchaseQuote {
        result.failure = failure;
        result.message = std::move(message);
        return result;
    };
    if (state_.phase != Phase::Prep || state_.expansionPending || state_.rewardOffer)
        return reject(PurchaseFailure::Closed, "정비 중에 각성할 수 있어요");
    const Tower *target = tower(id);
    if (!target)
        return reject(PurchaseFailure::InvalidTarget, "각성할 건물을 선택해 주세요");
    const std::array<ResourceCost, 2> &prices = catalog_.balance.awaken;
    const int step = std::max(0, target->level - CardMaxLevel);
    result.cost = prices[static_cast<std::size_t>(std::min(step, static_cast<int>(prices.size()) - 1))];
    if (target->level < CardMaxLevel)
        return reject(PurchaseFailure::Requirement, "같은 카드로 Lv" + std::to_string(CardMaxLevel) +
                                                        "까지 강화한 건물만 각성할 수 있어요");
    if (target->level >= awakenedLevel())
        return reject(PurchaseFailure::Owned, "이미 최고 각성 단계예요");
    if (!canPay(result.cost))
        return reject(PurchaseFailure::Shortage, costLabel(result.cost) + "가 필요해요");
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
            return {true, catalog_.card(item.kind).name + " Lv" + std::to_string(item.level) +
                              " 각성 · 기본 대비 +" + std::to_string(bonus) + "%"};
        }
    return {false, "각성할 건물을 선택해 주세요"};
}

PurchaseQuote Run::boulderQuote(Point position) const {
    PurchaseQuote result;
    /** Record one rejection reason and return the finished quote. */
    const auto reject = [&result](PurchaseFailure failure, std::string message) -> PurchaseQuote {
        result.failure = failure;
        result.message = std::move(message);
        return result;
    };
    result.cost = catalog_.balance.boulder;
    if (state_.phase != Phase::Prep || state_.expansionPending || state_.rewardOffer)
        return reject(PurchaseFailure::Closed, "정비 중에 바위를 치울 수 있어요");
    if (!resolveCell(state_.boulders, position))
        return reject(PurchaseFailure::InvalidTarget, "치울 바위를 선택해 주세요");
    if (!canPay(result.cost))
        return reject(PurchaseFailure::Shortage, costLabel(result.cost) + "가 필요해요");
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
    return {true, "바위를 치웠어요 · 이제 건물을 지을 수 있어요"};
}

Result Run::heal(EntityId id) {
    if (state_.phase != Phase::Prep)
        return {false, "정비 중에 치료할 수 있어요"};
    for (Unit &ally : state_.units)
        if (ally.id == id) {
            if (ally.hp == ally.maxHp)
                return {false, "이미 체력이 가득해요"};
            if (!canPay(catalog_.balance.heal))
                return {false, costLabel(catalog_.balance.heal) + "가 필요해요"};
            pay(catalog_.balance.heal);
            ally.hp = std::min(ally.maxHp, ally.hp + (ally.maxHp + 1) / 2);
            return {true, "유닛을 치료했어요"};
        }
    return {false, "치료할 유닛을 선택해 주세요"};
}

Result Run::expand(int choice) {
    if (state_.phase != Phase::Prep || !state_.expansionPending || (choice != 1 && choice != 2))
        return {false, "확장 후보를 선택해 주세요"};
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
    return {true, catalog_.card(expansion.reward).name + " 보상을 덱에 넣거나 건너뛸 수 있어요"};
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
