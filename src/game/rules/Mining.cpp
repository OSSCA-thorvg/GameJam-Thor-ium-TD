#include "RuleGeometry.h"
#include "Run.h"
#include "game/content/ContentKeys.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <utility>

namespace thorium::game {
namespace {
/** A message value holding another message with its own values, written `@key?name=value&name=value`. */
std::string nested(const std::string &key, const MessageArgs &args) {
    std::string value = game::ref(key);
    for (std::size_t index = 0; index < args.size(); ++index)
        value += (index ? "&" : "?") + args[index].first + "=" + args[index].second;
    return value;
}
} // namespace
Result Run::descentAvailability() const {
    if (state_.phase != Phase::Prep || state_.expansionPending || state_.rewardOffer)
        return {false, Message("rules.mining.descend.closed")};
    if (state_.job.active)
        return {false, Message("rules.mining.descend.miner_busy")};
    if (static_cast<std::size_t>(state_.mineLayer) >= catalog_.deeperMines.size())
        return {false, Message("rules.mining.descend.last")};
    const MineLayerDefinition &next = catalog_.deeperMines[static_cast<std::size_t>(state_.mineLayer)];
    if (state_.wave < next.opensAfterWave)
        return {false,
                Message("rules.mining.descend.locked", {{"wave", std::to_string(next.opensAfterWave)}})};
    if (state_.tool < 2)
        return {false, Message("rules.mining.descend.tool")};
    return {true, Message("rules.mining.descend.ready")};
}
Result Run::descend() {
    const Result allowed = descentAvailability();
    if (!allowed.ok)
        return allowed;
    const MineLayerDefinition &next = catalog_.deeperMines[static_cast<std::size_t>(state_.mineLayer)];
    initializeMine(next);
    ++state_.mineLayer;
    return {true, Message("rules.mining.descend.done", {{"layer", mineLayerRef(next.name)}})};
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
        return {false, Message("rules.mining.dig.excavate_mode")};
    if (state_.phase != Phase::Prep || state_.expansionPending || state_.rewardOffer)
        return {false, Message("rules.mining.dig.closed")};
    if (state_.job.active)
        return {false, Message("rules.mining.dig.busy")};
    if (state_.digs <= 0)
        return {false, Message("rules.mining.digs_used")};
    if (!reachable(index))
        return {false, Message("rules.mining.dig.unreachable")};
    if (state_.mine[index].grade > state_.tool)
        return {false, Message("rules.mining.dig.hard")};
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
        return {false, Message("rules.mining.dig.no_path")};
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
    return {true, Message("rules.mining.dig.started")};
}

Result Run::scan(int index) {
    if (state_.phase != Phase::Prep || state_.job.active || state_.expansionPending || state_.rewardOffer)
        return {false, Message("rules.mining.scan.closed")};
    if (state_.scans <= 0)
        return {false, Message("rules.mining.scan.used")};
    if (catalog_.mining == MiningMode::Excavate) {
        if (index < 0 || index >= static_cast<int>(state_.mine.size()))
            return {false, Message("rules.mining.inside_wall")};
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
            return {false, Message("rules.mining.scan.visible")};
        --state_.scans;
        return {true, Message("rules.mining.scan.wall_done")};
    }
    if (!reachable(index) || state_.mine[index].info)
        return {false, Message("rules.mining.scan.invalid")};
    state_.mine[index].info = true;
    state_.mine[index].clueKnown = true;
    --state_.scans;
    return {true, Message("rules.mining.scan.done")};
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

std::vector<std::string> Run::settleWall() {
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
                found = deposit.special == MineSpecial::Relic ? "rules.mining.found.relic"
                                                              : "rules.mining.found.fossil";
                resolveSpecial(deposit.cells.front());
            }
        }
    }
    state_.iron += gained.iron;
    state_.copper += gained.copper;
    state_.crystal += gained.crystal;
    state_.gold += gained.gold;
    state_.nextExtra += woken;
    std::vector<std::string> parts;
    if (!found.empty())
        parts.push_back(game::ref(found));
    const std::array<std::pair<Ore, int>, 4> ores{{{Ore::Iron, gained.iron},
                                                   {Ore::Copper, gained.copper},
                                                   {Ore::Crystal, gained.crystal},
                                                   {Ore::Gold, gained.gold}}};
    for (const auto &[ore, amount] : ores)
        if (amount)
            parts.push_back(nested("rules.mining.gain.ore",
                                   {{"ore", game::ref("ore." + std::string(oreKey(ore)) + ".name")},
                                    {"count", std::to_string(amount)}}));
    if (energy)
        parts.push_back(nested("rules.mining.gain.energy", {{"count", std::to_string(energy)}}));
    if (collapses)
        parts.push_back(game::ref("rules.mining.collapse"));
    if (woken)
        parts.push_back(nested("rules.mining.nest", {{"count", std::to_string(woken)}}));
    return parts;
}

Result Run::strike(int index, StrikeTool tool) {
    if (catalog_.mining != MiningMode::Excavate)
        return {false, Message("rules.mining.strike.tunnel_mode")};
    if (state_.phase != Phase::Prep || state_.expansionPending || state_.rewardOffer)
        return {false, Message("rules.mining.strike.closed")};
    if (index < 0 || index >= static_cast<int>(state_.mine.size()))
        return {false, Message("rules.mining.inside_wall")};
    if (!reachable(index))
        return {false, Message("rules.mining.strike.unreachable")};
    const int cost = strikeCost(tool);
    if (state_.digs < cost)
        return {false, state_.digs > 0
                           ? Message("rules.mining.strike.blast_digs", {{"count", std::to_string(cost)}})
                           : Message("rules.mining.digs_used")};
    const std::vector<StrikeHit> hits = strikePattern(index, tool);
    if (std::none_of(hits.begin(), hits.end(), [](const StrikeHit &hit) { return hit.power > 0; })) {
        const MineCell &target = state_.mine[static_cast<std::size_t>(index)];
        return {false, target.cover > 0 && target.grade > state_.tool
                           ? Message("rules.mining.strike.hard")
                           : Message("rules.mining.strike.uncovered")};
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
    std::vector<std::string> parts = settleWall();
    if (woken)
        parts.push_back(nested("rules.mining.strike.blast_nest", {{"count", std::to_string(woken)}}));
    if (parts.empty())
        return {true, Message("rules.mining.strike.done")};
    MessageArgs args;
    for (std::size_t index = 0; index < parts.size(); ++index)
        args.emplace_back("p" + std::to_string(index + 1), parts[index]);
    return {true, Message("rules.mining.summary." + std::to_string(parts.size()), std::move(args))};
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
