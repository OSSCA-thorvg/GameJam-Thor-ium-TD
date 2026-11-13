#pragma once
#include "game/rules/Run.h"
#include <algorithm>
#include <array>
#include <cmath>

/** Strong reference autoplayer shared by the real-cost playthrough test, the balance harness and the
 * measurement-only bench scene. */
namespace thorium::game::autoplay {
/** Preparation sinks reported by the balance harness, in report order. */
enum class Sink { Tool, Drill, Armor, Coil, Slots, Heal, Hire, Supply, Awaken, Recruit };
inline constexpr std::size_t SinkCount = 10;
/** Short report label of one sink. */
inline const char *sinkName(Sink sink) {
    static const std::array<const char *, SinkCount> names{
        {"tool", "drill", "armor", "coil", "slots", "heal", "hire", "supply", "awaken", "recruit"}};
    return names[static_cast<std::size_t>(sink)];
}
/** Wallet in recipe order: iron, copper, crystal, gold. */
using Wallet = std::array<int, 4>;
/** Resources paid and successful purchases per sink. */
struct Ledger {
    std::array<Wallet, SinkCount> spent{};
    std::array<int, SinkCount> uses{};
};
/** Run one purchase command and charge its wallet change to a sink when a ledger is attached. */
template <typename Command> Result charge(Run &run, Ledger *ledger, Sink sink, Command command) {
    const RunState &state = run.state();
    const Wallet before{state.iron, state.copper, state.crystal, state.gold};
    const Result result = command();
    if (ledger && result.ok) {
        const Wallet after{state.iron, state.copper, state.crystal, state.gold};
        Wallet &spent = ledger->spent[static_cast<std::size_t>(sink)];
        for (std::size_t index = 0; index < spent.size(); ++index)
            spent[index] += before[index] - after[index];
        ++ledger->uses[static_cast<std::size_t>(sink)];
    }
    return result;
}
/** Buy one preparation purchase through the shared quote-checked command. */
inline Result buy(Run &run, Ledger *ledger, Sink sink, UpgradeKind kind) {
    return charge(run, ledger, sink, [&run, kind]() -> Result { return run.upgrade(kind); });
}
/** One kind of scheduled enemy on a lane: hit points of the whole group, hit points and armor of one. */
struct Target {
    float total = 0;
    float hp = 0;
    int armor = 0;
    bool small = false;
    /** Share of the hit points behind a shield and the largest hit that shield takes. */
    float shielded = 0;
    int cap = 0;
};
/** Scheduled hit points on one lane, in total and per target kind; Steel elites are their own kind. */
struct LaneThreat {
    float total = 0;
    std::array<Target, EnemyKindCount + 1> targets{};
};
/** Forecast hit points of every lane, counting shields and Steel elites' extra armor and health. */
inline std::vector<LaneThreat> threats(const Run &run, const std::vector<LaneForecast> &lanes) {
    std::vector<LaneThreat> result;
    const Catalog &catalog = run.catalog();
    const EliteDefinition &steel = catalog.elite(EliteAffix::Steel);
    for (const LaneForecast &lane : lanes) {
        LaneThreat threat;
        for (std::size_t kind = 0; kind < EnemyKindCount; ++kind) {
            const EnemyDefinition &role = catalog.enemies[kind];
            const int elites = static_cast<EnemyKind>(kind) == EnemyKind::Armored
                                   ? lane.elites[static_cast<std::size_t>(EliteAffix::Steel)]
                                   : 0;
            const float hp = static_cast<float>(role.hp + role.shield);
            const bool small = static_cast<EnemyKind>(kind) == EnemyKind::Swarm ||
                               static_cast<EnemyKind>(kind) == EnemyKind::Normal ||
                               static_cast<EnemyKind>(kind) == EnemyKind::Runner;
            threat.targets[kind] = {static_cast<float>(std::max(0, lane.kinds[kind] - elites)) * hp,
                                    hp,
                                    role.armor,
                                    small,
                                    static_cast<float>(role.shield) / hp,
                                    role.shieldCap};
            if (elites)
                threat.targets[EnemyKindCount] = {static_cast<float>(elites) * hp * steel.health,
                                                  hp * steel.health, role.armor + steel.armor, false};
        }
        for (const Target &target : threat.targets)
            threat.total += target.total;
        result.push_back(threat);
    }
    return result;
}
/** Damage multiplier of a building against a lane's scheduled enemies: each kind's share of hit points
 *  times the part of one hit that survives armor and shield caps and is not overkill, times the enemies one
 *  attack reaches (splash, cone and chain hits grow on the small clustered kinds). */
inline float suit(const LaneThreat &threat, CardKind kind, const CardDefinition &card) {
    if (card.damage <= 0 || threat.total <= 0)
        return 1;
    const bool electric = kind == CardKind::Lightning;
    float result = 0;
    for (const Target &target : threat.targets) {
        if (target.total <= 0)
            continue;
        const float hit =
            static_cast<float>(electric ? card.damage : std::max(1, card.damage - target.armor));
        const float capped = target.cap > 0 ? std::min(hit, static_cast<float>(target.cap)) : hit;
        const float useful = (target.shielded * capped + (1 - target.shielded) * std::min(hit, target.hp)) /
                             static_cast<float>(card.damage);
        const float reach = card.splash > 0 ? (target.small ? 2.5F : 1.3F)
                            : card.arc > 0  ? (target.small ? 2.5F : 1.5F)
                            : card.chains > 1
                                ? std::min(static_cast<float>(card.chains), target.small ? 2.5F : 1.5F)
                                : 1.0F;
        result += target.total / threat.total * useful * reach;
    }
    return result;
}
/** Route tiles of one lane a building is credited for; past this many, a longer reach adds no damage because
 *  the building is already busy with the crowd in front of it. */
inline constexpr float BusyTiles = 6;
/** Estimate additional coverage using public routes, current defenses and incoming lane pressure. Every open
 *  lane is expected to carry a similar share over the stage, so a lane weighs the mean open length over its
 *  own length; damage follows the lane's armor, enemy health and crowding. A building counts its pad's
 *  range and damage trait, and route tiles inside a tunnel count for nothing. */
inline float coverage(const Run &run, Point position, CardKind kind, const CardDefinition &card, float gain) {
    const bool building = run.catalog().isBuilding(kind);
    const float range = building ? run.buildingRange(kind, position) : card.range;
    gain *= building ? run.padDamage(position) : 1.0F;
    const std::vector<LaneForecast> lanes = run.laneForecast();
    const std::vector<LaneThreat> threat = threats(run, lanes);
    const int wave = run.state().phase == Phase::Battle ? run.state().wave : run.state().wave + 1;
    std::vector<bool> open;
    float meanLength = 0;
    for (std::size_t lane = 0; lane < lanes.size(); ++lane) {
        open.push_back(lane == 0 || run.catalog().additionalRoutes[lane - 1].opensAtWave <= wave);
        meanLength += open.back() ? run.routeLength(lanes[lane].route) : 0;
    }
    meanLength /= static_cast<float>(std::max<std::ptrdiff_t>(1, std::count(open.begin(), open.end(), true)));
    float score = 0;
    for (std::size_t index = 0; index < lanes.size(); ++index) {
        const float length = run.routeLength(lanes[index].route);
        const float weight = open[index] ? std::clamp(meanLength / length, .3F, 2.5F) : .15F;
        const float effective = weight * gain * suit(threat[index], kind, card);
        float lane = 0;
        int covered = 0;
        for (float progress = .5F; progress < length; progress += 1) {
            const Point point = run.routePoint(progress, lanes[index].route);
            if (distance(point, position) > range || run.tunnelAt(progress, lanes[index].route))
                continue;
            float defended = 0;
            for (const Tower &tower : run.state().towers) {
                const CardDefinition &existing = run.catalog().card(tower.kind);
                if (distance(point, tower.position) <= run.buildingRange(tower.kind, tower.position))
                    defended += existing.damage / existing.interval *
                                (1 + (tower.level - 1) * run.catalog().balance.levelBonus);
            }
            lane += effective / (8 + defended);
            ++covered;
        }
        score +=
            static_cast<float>(covered) > BusyTiles ? lane * BusyTiles / static_cast<float>(covered) : lane;
    }
    return score;
}
/** Make at most one ordinary card command, never altering energy, health, entities or the schedule. */
inline void playCard(Run &run) {
    CardId best = 0;
    Point target{};
    float score = 0;
    /** A building in hand that only lacks energy for a free pad or its own kind's next level. */
    bool saving = false;
    for (int slot : run.state().hand) {
        const Card &card = run.state().deck[slot];
        const CardDefinition &definition = run.catalog().card(card.kind);
        if (run.catalog().isBuilding(card.kind)) {
            /** Another kind's building below the card level cap is replaced only once every pad is built. */
            const bool padsLeft = run.state().towers.size() < run.state().pads.size();
            for (Point pad : run.state().pads) {
                const PlayPreview plan = run.previewPlay(card.id, pad);
                const Tower *existing = nullptr;
                for (const Tower &tower : run.state().towers)
                    if (distance(pad, tower.position) < .01F)
                        existing = &tower;
                if (existing && existing->kind != card.kind &&
                    (padsLeft || existing->level >= CardMaxLevel || definition.aura > 0))
                    continue;
                if (!plan.allowed()) {
                    saving = saving || plan.failure == PlayFailure::EnergyShortage;
                    continue;
                }
                if (definition.aura > 0) {
                    float boosted = 0;
                    for (const Tower &tower : run.state().towers) {
                        const CardDefinition &other = run.catalog().card(tower.kind);
                        if (other.aura <= 0 &&
                            distance(tower.position, pad) <= run.buildingRange(card.kind, pad))
                            boosted += coverage(run, tower.position, tower.kind, other,
                                                other.damage / other.interval * definition.aura);
                    }
                    if (boosted / plan.cost > score) {
                        score = boosted / plan.cost;
                        best = card.id;
                        target = pad;
                    }
                    continue;
                }
                const float gain =
                    definition.damage / definition.interval *
                    (existing && existing->kind == card.kind ? run.catalog().balance.levelBonus : 1.0F);
                float value = coverage(run, pad, card.kind, definition, gain);
                if (existing && existing->kind != card.kind) {
                    const CardDefinition &old = run.catalog().card(existing->kind);
                    value -= coverage(run, pad, existing->kind, old,
                                      old.damage / old.interval *
                                          (1 + (existing->level - 1) * run.catalog().balance.levelBonus));
                }
                value /= static_cast<float>(plan.cost);
                if (value > score) {
                    score = value;
                    best = card.id;
                    target = pad;
                }
            }
        } else if (run.catalog().isUnit(card.kind)) {
            for (Point cell : run.state().unitCells) {
                if (!run.previewPlay(card.id, cell).allowed() || run.ventThreatens(cell))
                    continue;
                const float value =
                    coverage(run, cell, card.kind, definition, definition.damage / definition.interval) *
                    .35F / definition.cost;
                if (value > score) {
                    score = value;
                    best = card.id;
                    target = cell;
                }
            }
        } else if (card.kind == CardKind::Tar || card.kind == CardKind::Frost) {
            for (const Enemy &enemy : run.state().enemies) {
                if (!run.previewPlay(card.id, enemy.position).allowed())
                    continue;
                int crowd = 0;
                for (const Enemy &other : run.state().enemies)
                    if (distance(enemy.position, other.position) <= definition.range &&
                        (card.kind == CardKind::Tar || other.slowUntil <= run.state().battleTime))
                        ++crowd;
                if (crowd >= 6 && crowd / 8.0F > score) {
                    score = crowd / 8.0F;
                    best = card.id;
                    target = enemy.position;
                }
            }
        } else if (card.kind == CardKind::Blast) {
            for (const Enemy &enemy : run.state().enemies) {
                if (!run.previewPlay(card.id, enemy.position).allowed())
                    continue;
                float damage = 0;
                for (const Enemy &other : run.state().enemies)
                    if (distance(enemy.position, other.position) <= definition.range)
                        damage += std::min(other.hp, definition.damage);
                const float value = damage / 60;
                if (value > score && damage >= 70) {
                    score = value;
                    best = card.id;
                    target = enemy.position;
                }
            }
        }
    }
    if (best) {
        run.play(best, target);
        return;
    }
    // Saving for an unaffordable building takes priority over spending on circulation; a building that
    // could only replace another kind does not block the hand.
    if (saving)
        return;
    if (run.state().energy >= 2)
        run.exchange(run.state().deck[run.state().hand.front()].id);
}
/** Accept an expansion or fossil card in place of the fourth active deck card. */
inline void acceptReward(Run &run) {
    if (run.state().rewardOffer) {
        const CardOffer offer = *run.state().rewardOffer;
        run.acceptOffer(offer.id, offer.candidates.front(), run.state().deck[3].id);
    }
}
/** Worth of baring one wall tile: known contents by kind, a small exploration value for unseen rock, and a
 * penalty for a known nest. */
inline float wallValue(const Run &run, const MineCell &tile) {
    if (!tile.info)
        return .9F;
    if (!tile.deposit)
        return .05F;
    const Deposit &deposit = run.state().deposits[static_cast<std::size_t>(tile.deposit - 1)];
    if (deposit.collected)
        return 0;
    if (deposit.nest)
        return -6;
    return deposit.special == MineSpecial::Relic                ? 11
           : deposit.special == MineSpecial::Fossil             ? 10
           : deposit.special == MineSpecial::Energy             ? 5
           : deposit.special == MineSpecial::Unstable           ? 3
           : deposit.ore == Ore::Copper && run.state().tool < 2 ? 9
           : deposit.ore == Ore::Iron                           ? 8
           : deposit.ore == Ore::Crystal                        ? 7
                                                                : 6;
}
/** Value of one strike per dig spent: each tile counts by the share of its rock the strike removes. A blast
 * that reaches a known sleeping nest is never chosen, and unseen tiles in a blast count less because they
 * may hide one. */
inline float strikeScore(const Run &run, int cell, StrikeTool tool) {
    float score = 0;
    for (const StrikeHit &hit : run.strikePattern(cell, tool)) {
        const MineCell &tile = run.state().mine[static_cast<std::size_t>(hit.cell)];
        if (tool == StrikeTool::Blast && tile.info && tile.deposit) {
            const Deposit &deposit = run.state().deposits[static_cast<std::size_t>(tile.deposit - 1)];
            if (deposit.nest && !deposit.woken)
                return -1;
        }
        if (hit.power <= 0)
            continue;
        const float value = wallValue(run, tile);
        score += (tool == StrikeTool::Blast && !tile.info ? value - .5F : value) *
                 static_cast<float>(hit.power) / static_cast<float>(tile.cover);
    }
    return score / static_cast<float>(run.strikeCost(tool));
}
/** Scan the block with the most unseen thick rock, then strike wherever a dig bares the most value until the
 * digs run out or nothing is worth a strike. */
inline void excavate(Run &run, Ledger *ledger) {
    const int width = run.state().mineWidth;
    const int cells = static_cast<int>(run.state().mine.size());
    while (run.state().scans > 0) {
        int chosen = -1, most = 3;
        for (int cell = 0; cell < cells; ++cell) {
            int unseen = 0;
            for (int other = 0; other < cells; ++other)
                if (!run.state().mine[other].info && run.state().mine[other].cover >= 2 &&
                    std::abs(other % width - cell % width) <= run.catalog().balance.scanReach &&
                    std::abs(other / width - cell / width) <= run.catalog().balance.scanReach)
                    ++unseen;
            if (unseen >= most) {
                most = unseen;
                chosen = cell;
            }
        }
        if (chosen < 0 || !run.scan(chosen).ok)
            break;
    }
    for (int attempt = 0; attempt < 40 && run.state().digs > 0; ++attempt) {
        int chosen = -1;
        StrikeTool tool = StrikeTool::Pick;
        float best = .05F;
        for (int cell = 0; cell < cells; ++cell)
            for (StrikeTool option : {StrikeTool::Pick, StrikeTool::Blast}) {
                if (run.state().digs < run.strikeCost(option) || !run.reachable(cell))
                    continue;
                const float score = strikeScore(run, cell, option);
                if (score > best) {
                    best = score;
                    chosen = cell;
                    tool = option;
                }
            }
        if (chosen < 0 || !run.strike(chosen, tool).ok)
            break;
        acceptReward(run);
        buy(run, ledger, Sink::Tool, UpgradeKind::Tool);
    }
}
/** Dig the highest-priority visible safe tiles until the preparation's digs run out; in an excavation, strike
 * the wall instead. */
inline void mine(Run &run, Ledger *ledger) {
    if (run.catalog().mining == MiningMode::Excavate) {
        excavate(run, ledger);
        return;
    }
    for (int cell = static_cast<int>(run.state().mine.size()) - 1; cell >= 0 && run.state().scans > 0; --cell)
        if (run.reachable(cell) && !run.state().mine[cell].info)
            run.scan(cell);
    for (int attempt = 0; attempt < 12 && run.state().digs > 0; ++attempt) {
        int chosen = -1;
        int priority = -1;
        for (int cell = 0; cell < static_cast<int>(run.state().mine.size()); ++cell) {
            const MineCell &tile = run.state().mine[cell];
            if (!run.reachable(cell) || tile.grade > run.state().tool || (tile.info && tile.nest))
                continue;
            const int value = tile.info ? (tile.special == MineSpecial::Relic                ? 11
                                           : tile.special == MineSpecial::Fossil             ? 10
                                           : tile.special == MineSpecial::Energy             ? 5
                                           : tile.ore == Ore::Copper && run.state().tool < 2 ? 9
                                           : tile.ore == Ore::Iron                           ? 8
                                           : tile.ore == Ore::Crystal                        ? 7
                                           : tile.ore == Ore::Gold                           ? 6
                                                                                             : 1)
                              : tile.clueKnown && !tile.clue ? 4
                                                             : 0;
            if (value > priority) {
                priority = value;
                chosen = cell;
            }
        }
        if (chosen < 0 || !run.dig(chosen).ok)
            break;
        for (int step = 0; run.state().job.active && step < 200; ++step)
            run.advance(.1F);
        acceptReward(run);
        buy(run, ledger, Sink::Tool, UpgradeKind::Tool);
    }
}
/** Awaken the attacking building whose extra level covers the most incoming pressure, while affordable. */
inline void awaken(Run &run, Ledger *ledger) {
    for (int round = 0; round < 16; ++round) {
        EntityId best = 0;
        float score = 0;
        for (const Tower &tower : run.state().towers) {
            const CardDefinition &definition = run.catalog().card(tower.kind);
            if (definition.aura > 0 || !run.awakenQuote(tower.id).allowed())
                continue;
            const float value =
                coverage(run, tower.position, tower.kind, definition,
                         definition.damage / definition.interval * run.catalog().balance.levelBonus);
            if (value > score) {
                score = value;
                best = tower.id;
            }
        }
        if (!best ||
            !charge(run, ledger, Sink::Awaken, [&run, best]() -> Result { return run.awaken(best); }).ok)
            return;
    }
}
/** Best coverage per energy any attacking building of the active deck would add at a position. */
inline float siteValue(const Run &run, Point position) {
    float best = 0;
    for (const Card &card : run.state().deck) {
        const CardDefinition &definition = run.catalog().card(card.kind);
        if (run.catalog().isBuilding(card.kind) && definition.aura <= 0 && definition.cost > 0)
            best = std::max(best, coverage(run, position, card.kind, definition,
                                           definition.damage / definition.interval) /
                                      static_cast<float>(definition.cost));
    }
    return best;
}
/** Clear boulders with iron left over after the unowned pickaxe and armor recipes: the most valuable blocked
 *  pad first, and only while it would outscore every pad that is still free. Returns the boulders cleared. */
inline int clearBoulders(Run &run) {
    const RunBalance &balance = run.catalog().balance;
    int cleared = 0;
    while (!run.state().boulders.empty()) {
        const int reserve =
            (run.state().tool < 2 ? balance.tool.iron : 0) + (run.state().armor ? 0 : balance.armor.iron);
        if (run.state().iron - reserve < balance.boulder.iron)
            break;
        float free = 0;
        for (Point pad : run.state().pads)
            if (std::none_of(run.state().towers.begin(), run.state().towers.end(),
                             [pad](const Tower &tower) { return distance(tower.position, pad) < .01F; }))
                free = std::max(free, siteValue(run, pad));
        Point best{};
        float score = free;
        for (Point boulder : run.state().boulders)
            if (siteValue(run, boulder) > score) {
                score = siteValue(run, boulder);
                best = boulder;
            }
        if (score <= free || !run.clearBoulder(best).ok)
            break;
        ++cleared;
    }
    return cleared;
}
/** Mine visible safe candidates, craft real recipes, clear boulders, spend crystal and gold, and descend once
 *  the old layer is sparse. */
inline void prepare(Run &run, Ledger *ledger = nullptr) {
    if (run.state().expansionPending)
        run.expand(1);
    acceptReward(run);
    if (run.state().mineLayer == 0 && run.descentAvailability().ok && run.state().wave >= 5)
        run.descend();
    if (run.state().mineLayer == 1 && run.descentAvailability().ok && run.state().wave >= 9)
        run.descend();
    mine(run, ledger);
    buy(run, ledger, Sink::Armor, UpgradeKind::Armor);
    buy(run, ledger, Sink::Coil, UpgradeKind::Coil);
    buy(run, ledger, Sink::Drill, UpgradeKind::Drill);
    buy(run, ledger, Sink::Slots, UpgradeKind::Slots);
    // Hired digs come before the crate: their ore can still be spent in this preparation.
    while (buy(run, ledger, Sink::Hire, UpgradeKind::Hire).ok)
        mine(run, ledger);
    buy(run, ledger, Sink::Drill, UpgradeKind::Drill);
    clearBoulders(run);
    awaken(run, ledger);
    buy(run, ledger, Sink::Supply, UpgradeKind::Supply);
    for (const Unit &unit : std::vector<Unit>(run.state().units))
        charge(run, ledger, Sink::Heal, [&run, &unit]() -> Result { return run.heal(unit.id); });
}
} // namespace thorium::game::autoplay
