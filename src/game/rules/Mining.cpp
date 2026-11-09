#include "RuleGeometry.h"
#include "Run.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace thorium::game {
bool Run::reachable(int index) const {
    if (index < 0 || index >= 30 || state_.mine[index].dug)
        return false;
    for (int neighbor : neighbors(index))
        if (state_.mine[neighbor].dug)
            return true;
    return false;
}

Result Run::dig(int index) {
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
    std::array<int, 30> previous;
    previous.fill(-1);
    std::deque<int> visit{state_.minerCell};
    previous[state_.minerCell] = state_.minerCell;
    while (!visit.empty()) {
        const int cell = visit.front();
        visit.pop_front();
        for (int next : neighbors(cell))
            if (state_.mine[next].dug && previous[next] < 0) {
                previous[next] = cell;
                visit.push_back(next);
            }
    }
    int work = -1;
    int best = 100;
    for (int candidate : neighbors(index))
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
    if (!reachable(index) || state_.mine[index].info)
        return {false, "아직 확인하지 않은 갱도 경계를 선택해 주세요"};
    state_.mine[index].info = true;
    state_.mine[index].clueKnown = true;
    --state_.scans;
    return {true, "정보만 공개했어요. 광물 획득은 굴착 후 가능해요"};
}

void Run::revealZero(int origin) {
    if (state_.mine[origin].clue != 0 || state_.mine[origin].nest)
        return;
    std::deque<int> visit{origin};
    std::array<bool, 30> visited{};
    visited[origin] = true;
    while (!visit.empty()) {
        const int index = visit.front();
        visit.pop_front();
        for (int next = 0; next < 30; ++next)
            if (!state_.mine[next].nest && std::abs(next % 5 - index % 5) <= 1 &&
                std::abs(next / 5 - index / 5) <= 1) {
                state_.mine[next].clueKnown = true;
                if (!state_.mine[next].clue && !visited[next]) {
                    visited[next] = true;
                    visit.push_back(next);
                }
            }
    }
}

Point Run::minerPosition() const {
    if (!state_.job.active || state_.job.path.empty())
        return minePoint(state_.minerCell);
    const double progress = std::clamp(static_cast<double>(state_.job.elapsed) / catalog_.balance.minerStep,
                                       0.0, static_cast<double>(state_.job.path.size() - 1));
    const int index = static_cast<int>(progress);
    const Point a = minePoint(state_.job.path[index]);
    if (index + 1 >= static_cast<int>(state_.job.path.size()))
        return a;
    const Point b = minePoint(state_.job.path[index + 1]);
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
            state_.nextExtra += catalog_.balance.nestEnemies;
        revealZero(index);
    }
    state_.minerCell = index;
    state_.job = {};
}
} // namespace thorium::game
