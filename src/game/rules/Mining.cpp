#include "RuleGeometry.h"
#include "Run.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace thorium::game {
Result Run::descentAvailability() const {
    if (state_.phase != Phase::Prep || state_.expansionPending || state_.rewardOffer)
        return {false, "정비 중 확장·보상 선택을 마친 뒤 이동할 수 있어요"};
    if (state_.job.active)
        return {false, "광부가 작업을 마칠 때까지 기다려 주세요"};
    if (static_cast<std::size_t>(state_.mineLayer) >= catalog_.deeperMines.size())
        return {false, "이 구역의 마지막 지층이에요"};
    const MineLayerDefinition &next = catalog_.deeperMines[static_cast<std::size_t>(state_.mineLayer)];
    if (state_.wave < next.opensAfterWave)
        return {false, std::to_string(next.opensAfterWave) + "웨이브 뒤 다음 지층이 열려요"};
    if (state_.tool < 2)
        return {false, "다음 지층은 T2 곡괭이가 필요해요"};
    return {true, "다음 지층으로 이동할 수 있어요"};
}
Result Run::descend() {
    const Result allowed = descentAvailability();
    if (!allowed.ok)
        return allowed;
    const MineLayerDefinition &next = catalog_.deeperMines[static_cast<std::size_t>(state_.mineLayer)];
    initializeMine(next);
    ++state_.mineLayer;
    return {true, next.name + " 도착 · 남은 굴착·스캔과 보유 자원은 유지돼요"};
}
bool Run::reachable(int index) const {
    if (catalog_.mining == MiningMode::Excavate) {
        if (index < 0 || index >= static_cast<int>(state_.mine.size()))
            return false;
        if (catalog_.balance.openWall || index < state_.mineWidth || state_.mine[index].cover == 0)
            return true;
        for (int other = 0; other < static_cast<int>(state_.mine.size()); ++other)
            if (state_.mine[other].cover == 0 && surrounding(other, index, state_.mineWidth))
                return true;
        return false;
    }
    if (index < 0 || index >= static_cast<int>(state_.mine.size()) || state_.mine[index].dug)
        return false;
    for (int neighbor : neighbors(index, state_.mineWidth, state_.mineHeight))
        if (state_.mine[neighbor].dug)
            return true;
    return false;
}

Result Run::dig(int index) {
    if (catalog_.mining == MiningMode::Excavate)
        return {false, "이 원정은 암벽을 두드려 발굴해요"};
    if (state_.phase != Phase::Prep || state_.expansionPending || state_.rewardOffer)
        return {false, "정비 중에 채굴할 수 있어요"};
    if (state_.job.active)
        return {false, "광부가 이동·굴착 중이에요"};
    if (state_.digs <= 0)
        return {false, "이번 정비의 굴착을 모두 사용했어요"};
    if (!reachable(index))
        return {false, "열린 갱도 옆의 타일을 선택해 주세요"};
    if (state_.mine[index].grade > state_.tool)
        return {false, "단단한 지층은 T2 곡괭이가 필요해요"};
    std::vector<int> previous(state_.mine.size(), -1);
    std::deque<int> visit{state_.minerCell};
    previous[state_.minerCell] = state_.minerCell;
    while (!visit.empty()) {
        const int cell = visit.front();
        visit.pop_front();
        for (int next : neighbors(cell, state_.mineWidth, state_.mineHeight))
            if (state_.mine[next].dug && previous[next] < 0) {
                previous[next] = cell;
                visit.push_back(next);
            }
    }
    int work = -1;
    int best = static_cast<int>(state_.mine.size()) + 1;
    for (int candidate : neighbors(index, state_.mineWidth, state_.mineHeight))
        if (state_.mine[candidate].dug && previous[candidate] >= 0) {
            int length = 0;
            for (int cell = candidate; cell != state_.minerCell; cell = previous[cell])
                ++length;
            if (length < best || (length == best && candidate < work)) {
                best = length;
                work = candidate;
            }
        }
    if (work < 0)
        return {false, "연결된 작업 위치가 없어요"};
    std::vector<int> path;
    for (int cell = work;; cell = previous[cell]) {
        path.push_back(cell);
        if (cell == state_.minerCell)
            break;
    }
    std::reverse(path.begin(), path.end());
    state_.job = {true, index, path, 0.0F,
                  static_cast<float>(path.size() - 1) * catalog_.balance.minerStep +
                      (state_.mine[index].grade == 2 ? catalog_.balance.hardDig : catalog_.balance.softDig)};
    return {true, "광부가 자동으로 이동해 굴착해요"};
}

Result Run::scan(int index) {
    if (state_.phase != Phase::Prep || state_.job.active || state_.expansionPending || state_.rewardOffer)
        return {false, "작업이 끝난 정비 중에 스캔해 주세요"};
    if (state_.scans <= 0)
        return {false, "무료 스캔을 이미 사용했어요"};
    if (catalog_.mining == MiningMode::Excavate) {
        if (index < 0 || index >= static_cast<int>(state_.mine.size()))
            return {false, "암벽 안쪽을 선택해 주세요"};
        const int reach = catalog_.balance.scanReach;
        int shown = 0;
        for (int other = 0; other < static_cast<int>(state_.mine.size()); ++other) {
            MineCell &cell = state_.mine[other];
            if (cell.info || std::abs(other % state_.mineWidth - index % state_.mineWidth) > reach ||
                std::abs(other / state_.mineWidth - index / state_.mineWidth) > reach)
                continue;
            cell.info = true;
            ++shown;
        }
        if (!shown)
            return {false, "이미 속이 보이는 곳이에요"};
        --state_.scans;
        return {true, "암벽 속을 비춰 봤어요. 획득은 전부 드러낸 뒤에 가능해요"};
    }
    if (!reachable(index) || state_.mine[index].info)
        return {false, "아직 확인하지 않은 갱도 경계를 선택해 주세요"};
    state_.mine[index].info = true;
    state_.mine[index].clueKnown = true;
    --state_.scans;
    return {true, "정보만 공개했어요. 광물 획득은 굴착 후 가능해요"};
}

std::vector<StrikeHit> Run::strikePattern(int index, StrikeTool tool) const {
    std::vector<StrikeHit> hits;
    if (catalog_.mining != MiningMode::Excavate || index < 0 || index >= static_cast<int>(state_.mine.size()))
        return hits;
    const RunBalance &balance = catalog_.balance;
    const int column = index % state_.mineWidth, row = index / state_.mineWidth;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            const int x = column + dx, y = row + dy;
            if (x < 0 || y < 0 || x >= state_.mineWidth || y >= state_.mineHeight ||
                (tool == StrikeTool::Pick && dx && dy))
                continue;
            const MineCell &cell = state_.mine[static_cast<std::size_t>(y * state_.mineWidth + x)];
            int power = tool == StrikeTool::Blast ? balance.blastPower + (state_.tool >= 2 ? 1 : 0)
                        : dx || dy                ? (state_.tool >= 3 ? 2 : 1)
                                                  : balance.pickPower + state_.tool - 1;
            if (cell.grade > state_.tool)
                power = 0;
            hits.push_back({y * state_.mineWidth + x, std::min(power, cell.cover)});
        }
    return hits;
}

int Run::strikeCost(StrikeTool tool) const {
    return tool == StrikeTool::Blast ? catalog_.balance.blastDigs : 1;
}

int Run::buriedDeposits() const {
    return static_cast<int>(std::count_if(state_.deposits.begin(), state_.deposits.end(),
                                          [](const Deposit &deposit) { return !deposit.collected; }));
}

void Run::strip(int index, int power) {
    MineCell &cell = state_.mine[static_cast<std::size_t>(index)];
    cell.cover = std::max(0, cell.cover - power);
    if (cell.cover <= 1)
        cell.info = true;
    if (cell.cover == 0) {
        cell.dug = true;
        cell.clueKnown = true;
    }
}

std::string Run::settleWall() {
    ResourceCost gained;
    int energy = 0, woken = 0, collapses = 0;
    std::string found;
    for (bool changed = true; changed;) {
        changed = false;
        for (Deposit &deposit : state_.deposits) {
            if (deposit.collected ||
                !std::all_of(deposit.cells.begin(), deposit.cells.end(), [this](int cell) {
                    return state_.mine[static_cast<std::size_t>(cell)].cover == 0;
                }))
                continue;
            deposit.collected = true;
            if (deposit.ore == Ore::Iron)
                gained.iron += deposit.amount;
            if (deposit.ore == Ore::Copper)
                gained.copper += deposit.amount;
            if (deposit.ore == Ore::Crystal)
                gained.crystal += deposit.amount;
            if (deposit.ore == Ore::Gold)
                gained.gold += deposit.amount;
            if (deposit.nest && !deposit.woken) {
                deposit.woken = true;
                woken += nestReinforcements();
            }
            if (deposit.special == MineSpecial::Unstable) {
                // A cave-in strips one layer around the cracked rock, hard rock included, and wakes nothing.
                const int origin = deposit.cells.front();
                for (int other = 0; other < static_cast<int>(state_.mine.size()); ++other)
                    if (surrounding(other, origin, state_.mineWidth) && state_.mine[other].cover > 0)
                        strip(other, 1);
                ++collapses;
                changed = true;
            } else if (deposit.special == MineSpecial::Energy) {
                energy += deposit.amount;
                resolveSpecial(deposit.cells.front());
            } else if (deposit.special != MineSpecial::None) {
                found = deposit.special == MineSpecial::Relic ? "유물 상자" : "카드 화석";
                resolveSpecial(deposit.cells.front());
            }
        }
    }
    state_.iron += gained.iron;
    state_.copper += gained.copper;
    state_.crystal += gained.crystal;
    state_.gold += gained.gold;
    state_.nextExtra += woken;
    std::string summary;
    const auto add = [&summary](const std::string &part) {
        summary += (summary.empty() ? "" : " · ") + part;
    };
    if (!found.empty())
        add(found + " 발굴");
    if (gained.iron)
        add("철 +" + std::to_string(gained.iron));
    if (gained.copper)
        add("구리 +" + std::to_string(gained.copper));
    if (gained.crystal)
        add("수정 +" + std::to_string(gained.crystal));
    if (gained.gold)
        add("금 +" + std::to_string(gained.gold));
    if (energy)
        add("다음 전투 에너지 +" + std::to_string(energy));
    if (collapses)
        add("암반 붕괴");
    if (woken)
        add("둥지 각성 · 다음 웨이브 +" + std::to_string(woken));
    return summary;
}

Result Run::strike(int index, StrikeTool tool) {
    if (catalog_.mining != MiningMode::Excavate)
        return {false, "이 원정은 갱도를 따라 한 칸씩 굴착해요"};
    if (state_.phase != Phase::Prep || state_.expansionPending || state_.rewardOffer)
        return {false, "정비 중에 발굴할 수 있어요"};
    if (index < 0 || index >= static_cast<int>(state_.mine.size()))
        return {false, "암벽 안쪽을 선택해 주세요"};
    if (!reachable(index))
        return {false, "맨 윗줄이나 이미 드러난 곳 옆부터 파 들어가요"};
    const int cost = strikeCost(tool);
    if (state_.digs < cost)
        return {false, state_.digs > 0 ? "발파에는 굴착 " + std::to_string(cost) + "회가 필요해요"
                                       : "이번 정비의 굴착을 모두 사용했어요"};
    const std::vector<StrikeHit> hits = strikePattern(index, tool);
    if (std::none_of(hits.begin(), hits.end(), [](const StrikeHit &hit) { return hit.power > 0; })) {
        const MineCell &target = state_.mine[static_cast<std::size_t>(index)];
        return {false, target.cover > 0 && target.grade > state_.tool ? "단단한 암반은 T2 곡괭이가 필요해요"
                                                                      : "이미 다 드러난 곳이에요"};
    }
    state_.digs -= cost;
    int woken = 0;
    for (const StrikeHit &hit : hits) {
        strip(hit.cell, hit.power);
        const int id = state_.mine[static_cast<std::size_t>(hit.cell)].deposit;
        if (tool != StrikeTool::Blast || !id)
            continue;
        Deposit &deposit = state_.deposits[static_cast<std::size_t>(id - 1)];
        if (deposit.nest && !deposit.woken) {
            deposit.woken = true;
            woken += nestReinforcements();
        }
    }
    state_.nextExtra += woken;
    std::string summary = settleWall();
    if (woken)
        summary += (summary.empty() ? "" : " · ") + std::string("발파에 둥지 각성 · 다음 웨이브 +") +
                   std::to_string(woken);
    return {true, summary.empty() ? "암벽을 깎았어요" : summary};
}

void Run::revealZero(int origin) {
    if (state_.mine[origin].clue != 0 || state_.mine[origin].nest)
        return;
    std::deque<int> visit{origin};
    std::vector<bool> visited(state_.mine.size(), false);
    visited[origin] = true;
    while (!visit.empty()) {
        const int index = visit.front();
        visit.pop_front();
        for (int next = 0; next < static_cast<int>(state_.mine.size()); ++next)
            if (!state_.mine[next].nest && (next == index || surrounding(next, index, state_.mineWidth))) {
                state_.mine[next].clueKnown = true;
                if (!state_.mine[next].clue && !visited[next]) {
                    visited[next] = true;
                    visit.push_back(next);
                }
            }
    }
}

bool Run::hasRelic(Relic relic) const {
    return std::find(state_.relics.begin(), state_.relics.end(), relic) != state_.relics.end();
}

int Run::nestReinforcements() const {
    const RunBalance &balance = catalog_.balance;
    return std::max(0, balance.nestEnemies - (hasRelic(Relic::Ward) ? balance.relicNestReduction : 0));
}

int Run::prepDigBudget() const {
    return catalog_.balance.prepDigs + (hasRelic(Relic::Charm) ? catalog_.balance.relicDigs : 0) +
           (state_.tool >= 3 ? catalog_.balance.drillDigs : 0);
}

int Run::prepScanBudget() const {
    return catalog_.balance.prepScans + (hasRelic(Relic::Lens) ? catalog_.balance.relicScans : 0);
}

void Run::resolveSpecial(int index) {
    const MineCell &cell = state_.mine[index];
    switch (cell.special) {
    case MineSpecial::Relic:
        if (hasRelic(cell.relic))
            return;
        state_.relics.push_back(cell.relic);
        if (cell.relic == Relic::Charm)
            state_.digs += catalog_.balance.relicDigs;
        if (cell.relic == Relic::Lens)
            state_.scans += catalog_.balance.relicScans;
        return;
    case MineSpecial::Fossil:
        state_.rewardOffer = CardOffer{nextOffer_++, OfferSource::Fossil, {cell.card}, {}};
        return;
    case MineSpecial::Energy:
        state_.bonusEnergy += cell.amount;
        return;
    case MineSpecial::Unstable:
        collapse(index);
        return;
    case MineSpecial::None:
        return;
    }
}

void Run::collapse(int origin) {
    for (int next : neighbors(origin, state_.mineWidth, state_.mineHeight)) {
        MineCell &cell = state_.mine[next];
        if (cell.dug || cell.nest || cell.special != MineSpecial::None)
            continue;
        cell.dug = true;
        cell.info = true;
        cell.clueKnown = true;
        cell.rubble = true;
        cell.ore = Ore::Rock;
        cell.amount = 0;
        revealZero(next);
    }
}

Point Run::minerPosition() const {
    if (!state_.job.active || state_.job.path.empty())
        return minePoint(state_.minerCell, state_.mineWidth);
    const double progress = std::clamp(static_cast<double>(state_.job.elapsed) / catalog_.balance.minerStep,
                                       0.0, static_cast<double>(state_.job.path.size() - 1));
    const int index = static_cast<int>(progress);
    const Point a = minePoint(state_.job.path[index], state_.mineWidth);
    if (index + 1 >= static_cast<int>(state_.job.path.size()))
        return a;
    const Point b = minePoint(state_.job.path[index + 1], state_.mineWidth);
    const float part = static_cast<float>(progress - index);
    return {a.x + (b.x - a.x) * part, a.y + (b.y - a.y) * part};
}

void Run::advanceMining(float delta) {
    if (!state_.job.active)
        return;
    state_.job.elapsed += delta;
    if (state_.job.elapsed + 0.0001F < state_.job.duration)
        return;
    const int index = state_.job.target;
    MineCell &cell = state_.mine[index];
    if (!cell.dug) {
        cell.dug = true;
        cell.info = true;
        cell.clueKnown = true;
        --state_.digs;
        if (cell.ore == Ore::Iron)
            state_.iron += cell.amount;
        if (cell.ore == Ore::Copper)
            state_.copper += cell.amount;
        if (cell.ore == Ore::Crystal)
            state_.crystal += cell.amount;
        if (cell.ore == Ore::Gold)
            state_.gold += cell.amount;
        if (cell.nest)
            state_.nextExtra += nestReinforcements();
        revealZero(index);
        resolveSpecial(index);
    }
    state_.minerCell = index;
    state_.job = {};
}
} // namespace thorium::game
