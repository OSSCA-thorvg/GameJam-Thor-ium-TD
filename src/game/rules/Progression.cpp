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
    state_.energy = catalog_.balance.initialEnergy;
    state_.energyTime = 0;
    state_.battleTime = 0;
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
    if (state_.wave >= static_cast<int>(catalog_.waves.size())) {
        state_.phase = Phase::Won;
        return;
    }
    state_.phase = Phase::Prep;
    state_.digs = catalog_.balance.prepDigs;
    state_.scans = catalog_.balance.prepScans;
    state_.recruitmentOffer = CardOffer{
        nextOffer_++, OfferSource::Recruitment,
        std::vector<CardKind>(catalog_.recruitmentCandidates.begin(), catalog_.recruitmentCandidates.end()),
        catalog_.balance.recruit};
    state_.expansionPending = state_.wave == catalog_.balance.expansionWave;
}

Result Run::upgrade(UpgradeKind kind) {
    if (state_.phase != Phase::Prep || state_.expansionPending || state_.rewardOffer)
        return {false, "정비 중에 강화할 수 있어요"};
    const RunBalance &balance = catalog_.balance;
    if (kind == UpgradeKind::Tool) {
        if (state_.tool >= 2)
            return {false, "이미 T2 곡괭이예요"};
        if (!canPay(balance.tool))
            return {false, costLabel(balance.tool) + "가 필요해요"};
        pay(balance.tool);
        state_.tool = 2;
        return {true, "단단한 지층과 수정을 캘 수 있어요"};
    }
    if (kind == UpgradeKind::Armor) {
        if (state_.armor)
            return {false, "이미 장갑을 제작했어요"};
        if (!canPay(balance.armor))
            return {false, costLabel(balance.armor) + "가 필요해요"};
        pay(balance.armor);
        state_.armor = true;
        for (Unit &ally : state_.units)
            if (ally.kind == CardKind::Guard)
                ally.maxHp = balance.armoredGuardHp;
        return {true,
                "경비병 최대 체력 " + std::to_string(balance.armoredGuardHp) + " · 현재 체력은 유지돼요"};
    }
    if (kind == UpgradeKind::Slots) {
        if (state_.slots >= balance.upgradedSlots)
            return {false, "이미 최대 " + std::to_string(balance.upgradedSlots) + "슬롯이에요"};
        if (!canPay(balance.slots))
            return {false, costLabel(balance.slots) + "가 필요해요"};
        pay(balance.slots);
        state_.slots = balance.upgradedSlots;
        return {true, "유닛 슬롯이 " + std::to_string(balance.upgradedSlots) + "개로 늘었어요"};
    }
    if (kind == UpgradeKind::Coil) {
        if (state_.coil)
            return {false, "이미 코일을 제작했어요"};
        bool owns = false;
        for (const Card &item : state_.deck)
            owns |= item.kind == CardKind::Lightning;
        if (state_.tool < 2 || !owns)
            return {false, "T2 곡괭이와 활성 덱의 전격탑이 필요해요"};
        if (!canPay(balance.coil))
            return {false, costLabel(balance.coil) + "가 필요해요"};
        pay(balance.coil);
        state_.coil = true;
        return {true, "모든 전격탑 피해 +" +
                          std::to_string(static_cast<int>(std::lround((balance.coilBonus - 1) * 100))) + "%"};
    }
    return {false, "알 수 없는 강화예요"};
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
