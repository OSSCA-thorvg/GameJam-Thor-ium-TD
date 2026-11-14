#include "Catalog.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace thorium::game {
namespace {
/** Append an authored placement point once, including shared lane intersections. */
void appendCell(std::vector<Point> &cells, Point point) {
    if (std::none_of(cells.begin(), cells.end(),
                     [point](Point existing) -> bool { return distance(existing, point) < .01F; }))
        cells.push_back(point);
}
/** Put optional blocking positions along every lane, avoiding the spawn and base cells. */
void laneUnitCells(Catalog &content, const std::vector<Point> &path) {
    int step = 0;
    for (std::size_t index = 1; index < path.size(); ++index) {
        const Point a = path[index - 1], b = path[index];
        const int length = static_cast<int>(distance(a, b));
        for (int offset = 1; offset <= length; ++offset) {
            const Point point{a.x + (b.x - a.x) * offset / length, a.y + (b.y - a.y) * offset / length};
            if (++step % 3 == 0 && distance(point, path.back()) > 1)
                appendCell(content.unitCells, point);
        }
    }
}
/** Remove the unit cells a tunnel covers, where a unit could never block. */
void clearTunnelCells(Catalog &content) {
    for (const RouteSpan &span : content.tunnels) {
        const std::vector<Point> &path =
            span.route == 0 ? content.route : content.additionalRoutes[span.route - 1].points;
        float travelled = 0;
        for (std::size_t index = 1; index < path.size(); ++index) {
            const Point a = path[index - 1], b = path[index];
            const float length = distance(a, b);
            for (float along = 0; along <= length; along += 1) {
                const float progress = travelled + along;
                if (progress <= span.enter || progress >= span.exit)
                    continue;
                const Point tile{a.x + (b.x - a.x) * along / length, a.y + (b.y - a.y) * along / length};
                content.unitCells.erase(
                    std::remove_if(content.unitCells.begin(), content.unitCells.end(),
                                   [tile](Point cell) -> bool { return distance(cell, tile) < .01F; }),
                    content.unitCells.end());
            }
            travelled += length;
        }
    }
}
/** Stage and wave that first schedule a new enemy role; later stages field it from their first wave. */
struct Introduction {
    EnemyKind kind;
    int stage;
    int wave;
};
constexpr std::array<Introduction, 12> introductions{{{EnemyKind::Swarm, 1, 4},
                                                      {EnemyKind::Shielded, 1, 6},
                                                      {EnemyKind::Sapper, 1, 7},
                                                      {EnemyKind::Healer, 2, 3},
                                                      {EnemyKind::Splitter, 2, 6},
                                                      {EnemyKind::Wrecker, 2, 8},
                                                      {EnemyKind::Burrower, 3, 3},
                                                      {EnemyKind::Leaper, 3, 6},
                                                      {EnemyKind::Slinger, 3, 8},
                                                      {EnemyKind::Stalker, 4, 3},
                                                      {EnemyKind::Flyer, 4, 6},
                                                      {EnemyKind::Carrier, 4, 9}}};
/** Featured pressure of one wave on top of its common normal/runner/armored body. */
enum class Pressure { Introduce, Swarm, Armor, Escort, Split, Burrow, Flock, Ambush, Vault, Boss };
/** Check whether a role may appear in a stage wave according to the introduction table. */
bool unlocked(EnemyKind kind, int stage, int wave) {
    for (const Introduction &entry : introductions)
        if (entry.kind == kind)
            return stage > entry.stage || (stage == entry.stage && wave >= entry.wave);
    return true;
}
/** Return the role first introduced in this stage wave, or Normal when none is. */
EnemyKind introduced(int stage, int wave) {
    for (const Introduction &entry : introductions)
        if (entry.stage == stage && entry.wave == wave)
            return entry.kind;
    return EnemyKind::Normal;
}
/** Per-entrance share weight and start delay of one wave, indexed by route. */
struct LaneShares {
    std::vector<float> weight;
    std::vector<float> delay;
};
/** Split a total across lanes by weight; earlier full-weight lanes receive the remainder. */
void spread(std::vector<SpawnGroup> &groups, EnemyKind kind, int total, float start, float interval,
            const std::vector<std::uint32_t> &lanes, float stagger, const LaneShares &shares) {
    const auto weight = [&shares, &lanes](std::uint32_t lane) -> float {
        return lanes.size() > 1 ? shares.weight.at(lane) : 1.0F;
    };
    float weights = 0;
    for (std::uint32_t lane : lanes)
        weights += weight(lane);
    std::vector<int> counts;
    int assigned = 0;
    for (std::uint32_t lane : lanes) {
        counts.push_back(static_cast<int>(total * weight(lane) / weights));
        assigned += counts.back();
    }
    for (std::size_t index = 0; assigned < total; index = (index + 1) % counts.size())
        if (weight(lanes[index]) >= 1 || index + 1 == counts.size()) {
            ++counts[index];
            ++assigned;
        }
    for (std::size_t index = 0; index < lanes.size(); ++index)
        if (counts[index] > 0)
            groups.push_back({kind, counts[index], start + index * stagger + shares.delay.at(lanes[index]),
                              interval, lanes[index]});
}
/** Send tight clumps of a role down every lane, so splash damage meets dense packs. */
void clumps(std::vector<SpawnGroup> &groups, EnemyKind kind, int total, float start, float gap,
            const std::vector<std::uint32_t> &lanes, const LaneShares &shares) {
    constexpr int size = 8;
    for (int clump = 0; clump * size < total; ++clump)
        spread(groups, kind, std::min(size, total - clump * size), start + clump * gap, .14F, lanes, .5F,
               shares);
}
/** Tunable numbers of the campaign pressure curve. */
namespace curve {
/** Normals added per stage beyond the first, faded in over the first four waves of a stage. */
constexpr int StageBody = 1;
/** Pattern level added per stage beyond the first, faded in over the first four waves of a stage. */
constexpr int StageLevel = 1;
/** Fractional growth of every repeating group count per wave in the second half of a stage. */
constexpr float Growth = .12F;
/** Share of the normal body replaced by veterans per wave in the second half of a stage, up to VeteranCap;
 * every VeteranRatio replaced normals become one armored enemy. */
constexpr float Veteran = .06F;
constexpr float VeteranCap = .6F;
constexpr int VeteranRatio = 1;
/** Share of an even split for an entrance in its opening wave and in the wave after. */
constexpr float OpeningShare = .4F;
constexpr float SecondShare = .45F;
/** Seconds by which an entrance's enemies arrive later in the wave that first opens it. */
constexpr float OpeningDelay = 8;
/** Group counts start at Opening times a full wave, per stage, and reach a full wave at its middle wave. */
constexpr std::array<float, TotalStages> Opening{{.4F, .4F, .4F, .4F, .2F, .1F, .2F}};
/** A focused raid sends RaidFocus / (1 + open entrances) of a full wave, at most RaidCap, to one entrance. */
constexpr float RaidFocus = 2.5F;
constexpr float RaidCap = .75F;
/** Health and shield multiplier, then speed multiplier, of every non-boss role by stage. */
constexpr std::array<float, TotalStages> Toughness{{1, 1.3F, 1.35F, 1.5F, 1.3F, 1.0F, .95F}};
constexpr std::array<float, TotalStages> Haste{{1, 1.06F, 1.06F, 1.08F, 1.04F, 1.04F, 1.05F}};
/** Health and shield multiplier added to non-boss enemies by the last wave of each stage, rising linearly
 *  over its second half. */
constexpr std::array<float, TotalStages> LateVigor{{.5F, .4F, .55F, .62F, .84F, .8F, .74F}};
/** Elites in the first wave after the middle of a stage, and added by its last wave. */
constexpr float EliteBase = 2;
constexpr float EliteRamp = 4;
/** Sappers in the first wave after the middle of a stage, and added by its last wave. */
constexpr float SapperBase = 2;
constexpr float SapperRamp = 3;
/** Ordinary enemies packed behind the mid-stage pack leader inside its guarding aura. */
constexpr int LeaderPack = 8;
/** Campaign gold prices of one recruitment and of the unit slot expansion. */
constexpr int RecruitGold = 6;
constexpr int SlotGold = 6;
/** Zone boss health: BossBase + BossPerStage per stage up to the core, then BossPastCore per further stage.
 */
constexpr int BossBase = 1000;
constexpr int BossPerStage = 500;
constexpr int BossPastCore = 50;
/** Bats per flock of the flock pressure and of mixed escorts. */
constexpr int Flock = 5;
/** Longest stage in waves. */
constexpr int MaxWaves = 16;
/** Stages whose number raises the body, pattern level and armored count of their opening waves. */
constexpr int RampStages = 5;
} // namespace curve
/** Elite modifiers a stage rotates through: Steel and Swift from the first stage, Regen from the second and
 *  Enrage from the third. */
std::vector<EliteAffix> elitePool(int stage) {
    std::vector<EliteAffix> pool{EliteAffix::Steel, EliteAffix::Swift};
    if (stage >= 2)
        pool.push_back(EliteAffix::Regen);
    if (stage >= 3)
        pool.push_back(EliteAffix::Enrage);
    return pool;
}
/** Role carrying an elite modifier: the speed aura rides with the ordinary body, the others on durable roles.
 */
EnemyKind eliteCarrier(EliteAffix affix, int stage, int wave) {
    if (affix == EliteAffix::Swift)
        return EnemyKind::Normal;
    if (affix == EliteAffix::Regen && unlocked(EnemyKind::Splitter, stage, wave))
        return EnemyKind::Splitter;
    return EnemyKind::Armored;
}
/** Compose authored pressure patterns by stage and wave number for any stage length and lane count. */
void campaignWaves(Catalog &content, int stage, int count) {
    const std::vector<std::vector<SpawnGroup>> introduction = content.waves;
    content.waves.clear();
    for (std::size_t kind = 0; kind + 1 < EnemyKindCount; ++kind) {
        EnemyDefinition &role = content.enemies[kind];
        const std::size_t index = static_cast<std::size_t>(stage - 1);
        role.hp = static_cast<int>(std::lround(role.hp * curve::Toughness[index]));
        role.shield = static_cast<int>(std::lround(role.shield * curve::Toughness[index]));
        role.speed *= curve::Haste[index];
    }
    EnemyDefinition &boss = content.enemies[static_cast<std::size_t>(EnemyKind::Boss)];
    boss.hp = curve::BossBase + std::min(stage, curve::RampStages) * curve::BossPerStage +
              std::max(0, stage - curve::RampStages) * curve::BossPastCore;
    // Each zone's golem has its own name: obsidian adds a shield, frost a wider and longer stun, lava armour
    // and runners, the core armoured guards and the swamp a crowd of orelings.
    switch (stage) {
    case 2:
        boss.name = "흑요석 골렘";
        boss.shield = 200;
        boss.shieldCap = 8;
        break;
    case 3:
        boss.name = "동토 골렘";
        boss.abilityRadius = 3;
        boss.abilityDuration = 2.5F;
        boss.minionKind = EnemyKind::Burrower;
        break;
    case 4:
        boss.name = "용암 골렘";
        boss.armor = 5;
        boss.abilityInterval = 6;
        boss.minionKind = EnemyKind::Runner;
        boss.abilityAmount = 3;
        break;
    case 5:
        boss.name = "토르륨 골렘";
        boss.abilityRadius = 2.8F;
        boss.abilityInterval = 8;
        boss.minionKind = EnemyKind::Armored;
        break;
    case 6:
        boss.name = "수정 골렘";
        break;
    case 7:
        boss.name = "늪 골렘";
        boss.minionKind = EnemyKind::Normal;
        boss.abilityAmount = 3;
        break;
    default:
        break;
    }
    for (int wave = 1; wave <= count; ++wave) {
        if (stage == 1 && wave <= 2) {
            content.waves.push_back(introduction[static_cast<std::size_t>(wave - 1)]);
            content.vigor.push_back(1);
            continue;
        }
        /** Every fourth wave except the last focuses one entrance. */
        const auto raidWave = [count](int number) -> bool { return number % 4 == 0 && number != count; };
        std::vector<std::uint32_t> lanes{0};
        std::vector<std::uint32_t> opening;
        /** Entrances in their first wave of ordinary attack, including one a raid opened a wave earlier. */
        std::vector<std::uint32_t> newest;
        /** Entrances in their first two waves of ordinary attack. */
        std::vector<std::uint32_t> young;
        LaneShares shares{{1}, {0}};
        for (std::size_t index = 0; index < content.additionalRoutes.size(); ++index) {
            const std::uint32_t lane = static_cast<std::uint32_t>(index + 1);
            const int opens = content.additionalRoutes[index].opensAtWave;
            if (opens <= wave)
                lanes.push_back(lane);
            if (opens == wave)
                opening.push_back(lane);
            /** Waves an entrance has been under ordinary attack; a raid that opens it only probes it. */
            const int age = wave - opens - (wave > opens && raidWave(opens) ? 1 : 0);
            if (opens <= wave && age == 0)
                newest.push_back(lane);
            if (opens <= wave && age < 2)
                young.push_back(lane);
            shares.weight.push_back(age == 0 ? curve::OpeningShare : age == 1 ? curve::SecondShare : 1);
            shares.delay.push_back(age == 0 ? curve::OpeningDelay : 0);
        }
        const bool final = wave == count;
        const bool raid = raidWave(wave);
        const float raidScale =
            raid ? std::min(curve::RaidCap, curve::RaidFocus / static_cast<float>(1 + lanes.size())) : 1.0F;
        if (raid) {
            /** A raid focuses one entrance past its first two waves; an entrance opening this wave only
             *  receives its reduced share as a probe. */
            std::vector<std::uint32_t> established;
            for (std::uint32_t lane : lanes)
                if (std::find(young.begin(), young.end(), lane) == young.end())
                    established.push_back(lane);
            lanes = {established[static_cast<std::size_t>((wave + stage) % established.size())]};
            lanes.insert(lanes.end(), opening.begin(), opening.end());
        }
        const EnemyKind fresh = introduced(stage, wave);
        /** A new role debuts on the oldest open entrance and a raid's pattern follows its focused entrance;
         *  other patterns rotate over the entrances past their first wave. */
        std::vector<std::uint32_t> settled;
        for (std::uint32_t lane : lanes)
            if (std::find(newest.begin(), newest.end(), lane) == newest.end())
                settled.push_back(lane);
        const bool leaderWave = wave - count / 2 == 1;
        const std::uint32_t focus = fresh != EnemyKind::Normal || raid || leaderWave || settled.empty()
                                        ? lanes.front()
                                        : settled[static_cast<std::size_t>(wave % settled.size())];
        const std::vector<std::uint32_t> focusLane{focus};
        if (fresh != EnemyKind::Normal)
            content.enemyDebuts[static_cast<std::size_t>(fresh)] = wave;
        if (final)
            content.enemyDebuts[static_cast<std::size_t>(EnemyKind::Boss)] = wave;
        std::vector<Pressure> rotation{Pressure::Armor};
        if (unlocked(EnemyKind::Swarm, stage, wave))
            rotation.push_back(Pressure::Swarm);
        if (unlocked(EnemyKind::Healer, stage, wave))
            rotation.push_back(Pressure::Escort);
        if (unlocked(EnemyKind::Splitter, stage, wave))
            rotation.push_back(Pressure::Split);
        if (unlocked(EnemyKind::Burrower, stage, wave))
            rotation.push_back(Pressure::Burrow);
        /** The roles that ignore blockers, ground attacks or long range join the rotation after a stage's
         *  first two waves. */
        if (wave > 2 && unlocked(EnemyKind::Flyer, stage, wave))
            rotation.push_back(Pressure::Flock);
        if (wave > 2 && unlocked(EnemyKind::Leaper, stage, wave))
            rotation.push_back(Pressure::Vault);
        if (wave > 2 && unlocked(EnemyKind::Stalker, stage, wave))
            rotation.push_back(Pressure::Ambush);
        const Pressure pressure = final ? Pressure::Boss
                                  : fresh != EnemyKind::Normal
                                      ? Pressure::Introduce
                                      : rotation[static_cast<std::size_t>((wave + stage) % rotation.size())];
        /** Every stage restarts the economy, so stage difficulty ramps in over its first waves; stages
         * past the core use the core's ramp. */
        const int climb = std::min(stage, curve::RampStages) - 1;
        const int level = wave + climb * std::min(wave, 4) * curve::StageLevel / 4;
        const float spacing = wave % 3 == 0 ? .3F : 1.2F;
        const int middle = count / 2;
        const float start = curve::Opening[static_cast<std::size_t>(stage - 1)];
        const float growth =
            raidScale *
            std::min(1.0F, start + (1 - start) * static_cast<float>(wave) / static_cast<float>(middle)) *
            (1 + curve::Growth * std::max(0, wave - middle));
        /** Scale a repeating group count by the wave's growth. */
        const auto grown = [growth](int base) -> int { return static_cast<int>(std::lround(base * growth)); };
        const int stageBody = climb * curve::StageBody * std::min(wave, 4) / 4;
        const int body = grown(12 + wave * 4 + stageBody);
        const float veteranShare = std::min(curve::VeteranCap, curve::Veteran * std::max(0, wave - middle));
        const int veterans = static_cast<int>(body * veteranShare) / curve::VeteranRatio;
        const int normals = body - veterans * curve::VeteranRatio;
        const int runners = grown(4 + wave * 2);
        const int armored = (wave < 3 ? 0 : grown(wave - 2 + climb * std::min(wave, 4) / 4)) + veterans;
        std::vector<SpawnGroup> groups;
        spread(groups, EnemyKind::Normal, normals, 3, raid ? .45F : .8F, lanes, spacing, shares);
        spread(groups, EnemyKind::Runner, runners, 12, wave % 3 == 0 ? .35F : .6F, lanes, spacing, shares);
        if (armored)
            spread(groups, EnemyKind::Armored, armored, 8, 1.4F, lanes, spacing, shares);
        /** Small mixed escorts keep earlier roles present outside their featured waves. */
        /** The first two waves of a stage skip mixed escorts while the new economy is built. */
        const bool early = wave <= 2;
        const int mixed = raid || early ? 0 : grown(1 + level / 6);
        if (!raid && !early && pressure != Pressure::Swarm && unlocked(EnemyKind::Swarm, stage, wave) &&
            fresh != EnemyKind::Swarm)
            clumps(groups, EnemyKind::Swarm, grown(6 + level * 2), 16, 5, lanes, shares);
        if (pressure != Pressure::Armor && unlocked(EnemyKind::Shielded, stage, wave) &&
            fresh != EnemyKind::Shielded)
            spread(groups, EnemyKind::Shielded, mixed, 10, 2.2F, lanes, spacing, shares);
        if (pressure != Pressure::Escort && unlocked(EnemyKind::Healer, stage, wave) &&
            fresh != EnemyKind::Healer)
            spread(groups, EnemyKind::Healer, mixed, 11, 3, lanes, spacing, shares);
        if (pressure != Pressure::Split && unlocked(EnemyKind::Splitter, stage, wave) &&
            fresh != EnemyKind::Splitter)
            spread(groups, EnemyKind::Splitter, mixed, 18, 2.5F, lanes, spacing, shares);
        if (pressure != Pressure::Burrow && unlocked(EnemyKind::Burrower, stage, wave) &&
            fresh != EnemyKind::Burrower)
            spread(groups, EnemyKind::Burrower, mixed, 20, 2.5F, lanes, spacing, shares);
        if (pressure != Pressure::Flock && unlocked(EnemyKind::Flyer, stage, wave) &&
            fresh != EnemyKind::Flyer)
            spread(groups, EnemyKind::Flyer, mixed, 15, .4F, lanes, spacing, shares);
        if (pressure != Pressure::Vault && unlocked(EnemyKind::Leaper, stage, wave) &&
            fresh != EnemyKind::Leaper)
            spread(groups, EnemyKind::Leaper, (mixed + 1) / 2, 14, 2.4F, lanes, spacing, shares);
        if (pressure != Pressure::Ambush && unlocked(EnemyKind::Stalker, stage, wave) &&
            fresh != EnemyKind::Stalker)
            spread(groups, EnemyKind::Stalker, (mixed + 1) / 2, 17, 2.6F, lanes, spacing, shares);
        if (pressure != Pressure::Boss && wave % 2 == 0 && unlocked(EnemyKind::Wrecker, stage, wave) &&
            fresh != EnemyKind::Wrecker)
            spread(groups, EnemyKind::Wrecker, (mixed + 2) / 3, 12, 2.8F, lanes, spacing, shares);
        if (mixed > 0 && pressure != Pressure::Boss && unlocked(EnemyKind::Slinger, stage, wave) &&
            fresh != EnemyKind::Slinger && wave % 3 == 0)
            spread(groups, EnemyKind::Slinger, std::min(2, 1 + level / 8), 16, 3, focusLane, 0, shares);
        if (mixed > 0 && pressure != Pressure::Boss && unlocked(EnemyKind::Carrier, stage, wave) &&
            fresh != EnemyKind::Carrier && wave > 6 && wave % 4 == 1)
            spread(groups, EnemyKind::Carrier, std::min(2, 1 + level / 10), 14, 4, focusLane, 0, shares);
        switch (pressure) {
        case Pressure::Introduce:
            if (fresh == EnemyKind::Swarm)
                clumps(groups, EnemyKind::Swarm, 16, 7, 4, focusLane, shares);
            else if (fresh == EnemyKind::Shielded)
                spread(groups, EnemyKind::Shielded, 6, 7, 1.6F, focusLane, 0, shares);
            else if (fresh == EnemyKind::Healer) {
                spread(groups, EnemyKind::Armored, 3, 6.5F, 1.6F, focusLane, 0, shares);
                spread(groups, EnemyKind::Healer, 3, 7.3F, 1.6F, focusLane, 0, shares);
            } else if (fresh == EnemyKind::Splitter)
                spread(groups, EnemyKind::Splitter, 6, 7, 1.5F, focusLane, 0, shares);
            else if (fresh == EnemyKind::Sapper)
                spread(groups, EnemyKind::Sapper, 4, 9, 2.5F, focusLane, 0, shares);
            else if (fresh == EnemyKind::Flyer)
                spread(groups, EnemyKind::Flyer, 2 * curve::Flock, 7, .35F, focusLane, 0, shares);
            else if (fresh == EnemyKind::Leaper)
                spread(groups, EnemyKind::Leaper, 5, 7, 1.4F, focusLane, 0, shares);
            else if (fresh == EnemyKind::Stalker)
                spread(groups, EnemyKind::Stalker, 5, 7, 1.6F, focusLane, 0, shares);
            else if (fresh == EnemyKind::Wrecker)
                spread(groups, EnemyKind::Wrecker, 4, 7, 2.2F, focusLane, 0, shares);
            else if (fresh == EnemyKind::Carrier)
                spread(groups, EnemyKind::Carrier, 2, 8, 4, focusLane, 0, shares);
            else if (fresh == EnemyKind::Slinger)
                spread(groups, EnemyKind::Slinger, 2, 8, 3, focusLane, 0, shares);
            else
                spread(groups, EnemyKind::Burrower, 6, 7, 1.2F, focusLane, 0, shares);
            break;
        case Pressure::Swarm:
            clumps(groups, EnemyKind::Swarm, grown((early ? 8 : 20) + level * 3), 6, 3.5F, lanes, shares);
            break;
        case Pressure::Armor:
            spread(groups, EnemyKind::Armored, grown(2 + level / 3), 5, 1.1F, focusLane, 0, shares);
            if (unlocked(EnemyKind::Shielded, stage, wave))
                spread(groups, EnemyKind::Shielded, grown(2 + level / 3), 4.5F, 1.1F, lanes, spacing, shares);
            if (unlocked(EnemyKind::Healer, stage, wave))
                spread(groups, EnemyKind::Healer, grown(1 + level / 8), 6, 2.2F, focusLane, 0, shares);
            break;
        case Pressure::Escort:
            spread(groups, EnemyKind::Armored, grown(2 + level / 3), 6, 1.6F, lanes, spacing, shares);
            spread(groups, EnemyKind::Healer, grown(2 + level / 5), 6.8F, 1.6F, lanes, spacing, shares);
            break;
        case Pressure::Split:
            spread(groups, EnemyKind::Splitter, grown(3 + level / 2), 6, 1.3F, lanes, spacing, shares);
            break;
        case Pressure::Burrow:
            spread(groups, EnemyKind::Burrower, grown(3 + level / 2), 6, 1.1F, lanes, spacing, shares);
            break;
        case Pressure::Flock:
            for (int flock = 0; flock * curve::Flock < grown(4 + level / 2); ++flock)
                spread(groups, EnemyKind::Flyer, curve::Flock, 6 + flock * 4.5F, .3F, lanes, .6F, shares);
            break;
        case Pressure::Vault:
            spread(groups, EnemyKind::Leaper, grown(3 + level / 2), 6, 1.2F, lanes, spacing, shares);
            spread(groups, EnemyKind::Armored, grown(1 + level / 4), 5, 1.6F, focusLane, 0, shares);
            break;
        case Pressure::Ambush:
            spread(groups, EnemyKind::Stalker, grown(3 + level / 2), 6, 1.2F, lanes, spacing, shares);
            break;
        case Pressure::Boss:
            groups.push_back({EnemyKind::Boss, 1, 14, 0, 0});
            spread(groups, EnemyKind::Shielded, grown(2 + stage), 13, .9F, {0}, 0, shares);
            if (unlocked(EnemyKind::Healer, stage, wave))
                spread(groups, EnemyKind::Healer, 1 + stage / 2, 15.5F, 1.4F, {0}, 0, shares);
            clumps(groups, EnemyKind::Swarm, grown(12 + level * 2), 22, 4, lanes, shares);
            if (unlocked(EnemyKind::Flyer, stage, wave))
                spread(groups, EnemyKind::Flyer, 2 * curve::Flock, 26, .3F, lanes, .6F, shares);
            break;
        }
        /** The second half of a stage adds health, elites, sappers and, right after the middle, a pack
         * leader. */
        const int late = wave - middle;
        content.vigor.push_back(1 + curve::LateVigor[static_cast<std::size_t>(stage - 1)] *
                                        static_cast<float>(std::max(0, late)) /
                                        static_cast<float>(std::max(1, count - middle)));
        if (late > 0) {
            /** Progress through the second half: 0 right after the middle, 1 on the last wave. */
            const float ramp =
                static_cast<float>(late - 1) / static_cast<float>(std::max(1, count - middle - 1));
            const std::vector<EliteAffix> pool = elitePool(stage);
            /** A raid already concentrates the wave on one entrance and carries no elites or sappers. */
            const int elites =
                raid ? 0 : static_cast<int>(std::lround(curve::EliteBase + curve::EliteRamp * ramp));
            for (int pick = 0; pick < 2 && elites > 0; ++pick) {
                const EliteAffix affix = pool[static_cast<std::size_t>(wave + stage + pick) % pool.size()];
                const int share = pick == 0 ? (elites + 1) / 2 : elites / 2;
                const std::size_t first = groups.size();
                spread(groups, eliteCarrier(affix, stage, wave), share, 9 + pick * 1.3F, 2.6F, lanes, spacing,
                       shares);
                for (std::size_t index = first; index < groups.size(); ++index)
                    groups[index].affix = affix;
            }
            if (!raid && fresh != EnemyKind::Sapper && unlocked(EnemyKind::Sapper, stage, wave))
                spread(groups, EnemyKind::Sapper,
                       static_cast<int>(std::lround(curve::SapperBase + curve::SapperRamp * ramp)), 10, 3,
                       lanes, spacing, shares);
        }
        if (late == 1) {
            /** The pack leader takes the oldest open entrance, which is the focused one in a raid. */
            groups.push_back({EnemyKind::Armored, 1, 12, 0, lanes.front(), EliteAffix::Leader});
            spread(groups, EnemyKind::Normal, curve::LeaderPack, 12.4F, .3F, {lanes.front()}, 0, shares);
        }
        for (const SpawnGroup &group : groups)
            if (group.affix != EliteAffix::None && group.count > 0 &&
                !content.eliteDebuts[static_cast<std::size_t>(group.affix)])
                content.eliteDebuts[static_cast<std::size_t>(group.affix)] = wave;
        content.waves.push_back(std::move(groups));
    }
}
/** Offer a long western detour or a short extension in front of a primary entrance at column two. */
std::array<ExpansionDefinition, 2> westExpansions(Point front, CardKind longReward, CardKind shortReward) {
    const float y = front.y;
    return {{{{{0, y + 4}, {3, y + 4}, {3, y + 2}, {1, y + 2}, {1, y}},
              {{2, y + 3}, {4, y + 3}},
              {{2, y + 4}},
              longReward},
             {{{0, y}}, {{1, y - 1}, {1, y + 1}}, {{1, y}}, shortReward}}};
}
/** Build one real campaign map from shared rules, with distinct geography and staged entrances. */
Catalog makeStage(int stage) {
    Catalog content = defaultCatalog();
    if (stage == 1) {
        content.width = 30;
        content.height = 18;
        content.route.insert(content.route.end(), {{20, 5}, {20, 10}, {22, 10}});
        content.additionalRoutes = {
            {"북쪽 우회로", {{18, 1}, {14, 1}, {14, 3}, {18, 3}, {18, 8}, {22, 8}, {22, 10}}, 3},
            {"남서 광산길", {{1, 16}, {12, 16}, {12, 12}, {24, 12}, {24, 10}, {22, 10}}, 4}};
        for (Point point : std::vector<Point>{{17, 4},
                                              {19, 7},
                                              {21, 9},
                                              {13, 2},
                                              {17, 2},
                                              {17, 9},
                                              {21, 11},
                                              {12, 4},
                                              {11, 14},
                                              {16, 13},
                                              {19, 11},
                                              {14, 11},
                                              {7, 15},
                                              {25, 11},
                                              {8, 17}})
            appendCell(content.pads, point);
        /** Rockfall blocks three pads that each overlook two stretches of road. */
        content.boulders = {{13, 6}, {15, 4}, {19, 9}};
    } else if (stage == 2) {
        content.width = 34;
        content.height = 20;
        content.route = {{2, 3}, {10, 3}, {10, 8}, {5, 8}, {5, 14}, {20, 14}, {20, 9}, {29, 9}};
        content.additionalRoutes = {
            {"북쪽 협곡", {{21, 1}, {14, 1}, {14, 6}, {25, 6}, {25, 9}, {29, 9}}, 2},
            {"남쪽 지름길",
             {{33, 19}, {30, 19}, {30, 16}, {18, 16}, {18, 12}, {26, 12}, {26, 9}, {29, 9}},
             5}};
        content.pads = {{4, 2},   {16, 2},  {19, 2},  {13, 3},  {8, 4},   {16, 5},  {19, 5},  {11, 6},
                        {8, 7},   {24, 7},  {30, 8},  {19, 9},  {6, 10},  {23, 10}, {27, 10}, {7, 13},
                        {17, 13}, {24, 13}, {21, 14}, {17, 16}, {31, 17}, {29, 18}, {31, 15}, {24, 17}};
        content.starterDeck = {CardKind::Gun,    CardKind::Guard, CardKind::Cannon, CardKind::Frost,
                               CardKind::Sniper, CardKind::Guard, CardKind::Blast,  CardKind::Gun};
        content.expansions = westExpansions(content.route.front(), CardKind::Lightning, CardKind::Flame);
        /** Two ridges overlook two lanes each; two patches of crystal ground lie right beside a lane. */
        content.padTraits = {{{13, 3}, PadTrait::HighGround},
                             {{19, 9}, PadTrait::HighGround},
                             {{8, 7}, PadTrait::Crystal},
                             {{17, 13}, PadTrait::Crystal}};
    } else if (stage == 3) {
        content.width = 38;
        content.height = 22;
        content.route = {{2, 3},   {12, 3},  {12, 8},  {6, 8},   {6, 17},
                         {17, 17}, {17, 13}, {25, 13}, {25, 11}, {34, 11}};
        content.additionalRoutes = {
            {"북쪽 회랑", {{31, 1}, {21, 1}, {21, 6}, {31, 6}, {31, 11}, {34, 11}}, 2},
            {"남쪽 갱도", {{35, 20}, {20, 20}, {20, 15}, {29, 15}, {29, 11}, {34, 11}}, 4},
            {"중앙 우회로",
             {{1, 11}, {10, 11}, {10, 15}, {15, 15}, {15, 9}, {27, 9}, {27, 11}, {34, 11}},
             7}};
        content.pads = {{22, 3},  {11, 5},  {21, 7},  {29, 7},  {13, 8},  {25, 8},  {32, 9},
                        {5, 10},  {8, 10},  {29, 10}, {11, 11}, {17, 12}, {20, 12}, {23, 12},
                        {33, 12}, {5, 13},  {14, 13}, {26, 13}, {30, 13}, {9, 15},  {18, 15},
                        {12, 16}, {15, 16}, {22, 16}, {25, 16}, {18, 18}};
        content.starterDeck = {CardKind::Gun,   CardKind::Guard, CardKind::Cannon, CardKind::Frost,
                               CardKind::Flame, CardKind::Guard, CardKind::Blast,  CardKind::Sniper};
        content.expansions = westExpansions(content.route.front(), CardKind::Sniper, CardKind::Generator);
        /** The primary lane runs underground for nine tiles along its southern stretch. */
        content.tunnels = {{0, 31, 40}};
    } else if (stage == 4) {
        content.width = 42;
        content.height = 24;
        content.route = {{2, 2},  {11, 2}, {11, 7},  {4, 7},   {4, 13},  {13, 13},
                         {13, 9}, {20, 9}, {20, 16}, {30, 16}, {30, 12}, {38, 12}};
        content.additionalRoutes = {
            {"북쪽 용암길", {{27, 1}, {17, 1}, {17, 5}, {26, 5}, {26, 9}, {34, 9}, {34, 12}, {38, 12}}, 2},
            {"남쪽 분화구", {{40, 22}, {25, 22}, {25, 19}, {34, 19}, {34, 12}, {38, 12}}, 4},
            {"서쪽 균열", {{1, 18}, {9, 18}, {9, 21}, {20, 21}, {20, 16}, {30, 16}, {30, 12}, {38, 12}}, 7},
            {"동쪽 열수로",
             {{41, 2}, {37, 2}, {37, 6}, {30, 6}, {30, 10}, {36, 10}, {36, 12}, {38, 12}},
             10}};
        content.pads = {{18, 3},  {39, 3},  {10, 4},  {36, 4},  {16, 6},  {19, 6},  {24, 6},  {27, 7},
                        {6, 8},   {32, 8},  {35, 8},  {15, 10}, {18, 10}, {29, 10}, {5, 11},  {12, 11},
                        {32, 11}, {35, 11}, {39, 11}, {19, 14}, {31, 14}, {35, 14}, {22, 15}, {25, 15},
                        {28, 17}, {31, 17}, {21, 18}, {25, 18}, {7, 19},  {10, 19}, {32, 20}, {27, 21}};
        content.starterDeck = {CardKind::Gun,   CardKind::Guard, CardKind::Cannon, CardKind::Tar,
                               CardKind::Flame, CardKind::Guard, CardKind::Blast,  CardKind::Sniper};
        content.expansions = westExpansions(content.route.front(), CardKind::Generator, CardKind::Lightning);
        /** Vents sit inside lane bends and beside the crossing of the northern and eastern lanes. */
        content.vents = {{{21, 15}, 2}, {{10, 6}, 4}, {{25, 6}, 6}, {{26, 20}, 8}, {{31, 8}, .5F}};
    } else if (stage == 5) {
        content.width = 48;
        content.height = 26;
        content.route = {{2, 4},   {9, 4},   {9, 10},  {4, 10},  {4, 20},
                         {14, 20}, {14, 15}, {19, 15}, {19, 13}, {24, 13}};
        content.additionalRoutes = {
            {"북쪽 관문", {{30, 1}, {18, 1}, {18, 6}, {28, 6}, {28, 10}, {24, 10}, {24, 13}}, 2},
            {"동쪽 균열", {{46, 5}, {40, 5}, {40, 10}, {34, 10}, {34, 16}, {29, 16}, {29, 13}, {24, 13}}, 4},
            {"남쪽 나선",
             {{38, 24}, {38, 19}, {31, 19}, {31, 22}, {22, 22}, {22, 17}, {26, 17}, {26, 13}, {24, 13}},
             7},
            {"남서 광맥",
             {{1, 23}, {10, 23}, {10, 17}, {17, 17}, {17, 15}, {19, 15}, {19, 13}, {24, 13}},
             11}};
        content.pads = {{19, 3},  {7, 5},   {26, 5},  {42, 6},  {20, 7},  {8, 8},   {27, 8},  {39, 8},
                        {6, 11},  {23, 11}, {26, 11}, {29, 11}, {36, 11}, {18, 13}, {22, 14}, {33, 14},
                        {25, 15}, {28, 15}, {15, 16}, {18, 16}, {21, 17}, {31, 17}, {34, 17}, {5, 18},
                        {9, 18},  {12, 18}, {24, 18}, {27, 18}, {15, 19}, {30, 20}, {33, 20}, {36, 20},
                        {4, 21},  {8, 21},  {11, 21}, {24, 21}};
        content.starterDeck = {CardKind::Gun,   CardKind::Guard, CardKind::Cannon, CardKind::Frost,
                               CardKind::Flame, CardKind::Gun,   CardKind::Blast,  CardKind::Sniper};
        content.expansions = westExpansions(content.route.front(), CardKind::Lightning, CardKind::Generator);
        /** One tunnel on the western lane, three vents on the approaches and four trait pads. */
        content.tunnels = {{0, 22, 26}};
        content.vents = {{{16, 16}, 2}, {{27, 9}, 5}, {{33, 15}, 8}};
        content.padTraits = {{{22, 14}, PadTrait::HighGround},
                             {{36, 11}, PadTrait::HighGround},
                             {{26, 11}, PadTrait::Crystal},
                             {{21, 17}, PadTrait::Crystal}};
    } else if (stage == 6) {
        content.width = 50;
        content.height = 26;
        content.route = {{2, 5}, {12, 5}, {12, 11}, {6, 11}, {6, 19}, {18, 19}, {18, 13}, {36, 13}};
        content.additionalRoutes = {
            {"북쪽 수정맥", {{20, 1}, {20, 8}, {26, 8}, {26, 4}, {32, 4}, {32, 13}, {36, 13}}, 2},
            {"동쪽 광차길", {{49, 2}, {37, 2}, {37, 9}, {46, 9}, {46, 16}, {40, 16}, {40, 13}, {36, 13}}, 4},
            {"남쪽 공명굴", {{48, 24}, {30, 24}, {30, 17}, {36, 17}, {36, 13}}, 7}};
        content.pads = {{11, 4},  {13, 8},  {9, 10},  {7, 13},  {5, 16},  {7, 18},  {10, 20}, {14, 18},
                        {17, 16}, {19, 15}, {19, 11}, {22, 14}, {25, 12}, {28, 14}, {21, 6},  {23, 7},
                        {25, 5},  {29, 3},  {31, 6},  {33, 9},  {34, 14}, {33, 11}, {36, 3},  {38, 6},
                        {40, 8},  {42, 11}, {39, 12}, {44, 12}, {29, 21}, {31, 19}, {35, 18}, {37, 16},
                        {33, 23}, {38, 25}, {15, 12}, {29, 10}, {45, 3},  {41, 3},  {40, 10}, {45, 11},
                        {47, 13}, {43, 15}, {41, 14}};
        content.starterDeck = {CardKind::Gun,   CardKind::Guard, CardKind::Cannon, CardKind::Frost,
                               CardKind::Flame, CardKind::Gun,   CardKind::Blast,  CardKind::Sniper};
        content.expansions = westExpansions(content.route.front(), CardKind::Lightning, CardKind::Generator);
        /** Two cart rails run along the merged approach and the eastern cart road; crystal clusters block
         *  three pads. */
        content.rails = {{{20, 13}, {35, 13}, {27, 15}}, {{48, 2}, {38, 2}, {43, 4}}};
        content.boulders = {{21, 12}, {27, 6}, {32, 22}};
    } else {
        content.width = 52;
        content.height = 28;
        content.route = {{2, 4}, {10, 4}, {10, 10}, {5, 10}, {5, 21}, {15, 21}, {15, 14}, {24, 14}};
        content.additionalRoutes = {
            {"북쪽 갈대밭", {{38, 1}, {38, 3}, {27, 3}, {27, 6}, {20, 6}, {20, 10}, {24, 10}, {24, 14}}, 2},
            {"동쪽 산성천", {{51, 8}, {39, 8}, {39, 17}, {30, 17}, {30, 14}, {24, 14}}, 4},
            {"남쪽 수렁",
             {{45, 27}, {45, 25}, {37, 25}, {37, 23}, {27, 23}, {27, 19}, {24, 19}, {24, 14}},
             7},
            {"남서 썩은 물길",
             {{1, 26}, {11, 26}, {11, 24}, {19, 24}, {19, 17}, {22, 17}, {22, 14}, {24, 14}},
             11}};
        content.pads = {{9, 3},   {11, 7},  {8, 9},   {6, 12},  {4, 15},  {6, 18},  {4, 20},  {9, 20},
                        {13, 22}, {14, 17}, {16, 12}, {19, 13}, {21, 15}, {23, 12}, {26, 15}, {28, 13},
                        {21, 8},  {26, 5},  {29, 2},  {33, 4},  {23, 11}, {35, 2},  {38, 10}, {40, 13},
                        {36, 16}, {32, 18}, {29, 15}, {41, 7},  {28, 22}, {31, 24}, {36, 22}, {41, 26},
                        {26, 20}, {23, 20}, {18, 22}, {12, 25}, {20, 19}, {25, 17}, {44, 24}, {44, 9},
                        {47, 7},  {38, 14}, {33, 16}, {25, 11}, {21, 9},  {30, 4}};
        content.starterDeck = {CardKind::Gun,   CardKind::Guard,     CardKind::Cannon, CardKind::Tar,
                               CardKind::Flame, CardKind::Lightning, CardKind::Blast,  CardKind::Sniper};
        content.expansions = westExpansions(content.route.front(), CardKind::Generator, CardKind::Lightning);
        /** Bogs slow four lanes; six gas tanks stand beside lane bends and crossings. */
        content.bogs = {{0, 20, 29}, {1, 16, 23}, {2, 9, 16}, {4, 13, 20}};
        content.tanks = {{{9, 8}}, {{17, 16}}, {{28, 5}}, {{41, 15}}, {{33, 22}}, {{9, 25}}};
    }
    if (stage > 1)
        content.unitCells.clear();
    // From stage 2 every stage has one barricaded fork with its own trade: a long western detour (2), a
    // shortcut past the tunnel that starts closed (3), a bypass of two lava vents (4), a western detour
    // around a tunnel (5) or a bog (7), and a bypass of the mine cart rail (6).
    struct Fork {
        std::uint32_t route;
        Point split;
        Point merge;
        std::vector<Point> detour;
        std::vector<Point> pads;
    };
    const std::array<Fork, TotalStages> forks{{
        {},
        {0, {5, 8}, {5, 14}, {{2, 8}, {2, 14}}, {{1, 10}, {1, 12}}},
        {0, {12, 8}, {17, 14}, {{12, 14}}, {}},
        {1, {26, 5}, {34, 9}, {{34, 5}}, {{36, 7}}},
        {0, {4, 10}, {4, 20}, {{1, 10}, {1, 20}}, {{0, 13}, {0, 17}}},
        {0, {18, 16}, {33, 13}, {{33, 16}}, {}},
        {0, {5, 10}, {5, 21}, {{2, 10}, {2, 21}}, {{1, 13}, {1, 18}}},
    }};
    if (stage >= 2) {
        const Fork &fork = forks[static_cast<std::size_t>(stage - 1)];
        content.forks = {{fork.route, fork.split, fork.merge, fork.detour}};
        content.pads.insert(content.pads.end(), fork.pads.begin(), fork.pads.end());
        std::vector<Point> branch{fork.split};
        branch.insert(branch.end(), fork.detour.begin(), fork.detour.end());
        branch.push_back(fork.merge);
        laneUnitCells(content, branch);
    }
    laneUnitCells(content, content.route);
    for (const EntranceRoute &entrance : content.additionalRoutes)
        laneUnitCells(content, entrance.points);
    clearTunnelCells(content);
    content.hero = true;
    /** Campaign recruitment and slot prices; the practice board keeps the default balance. */
    content.balance.recruit = {0, 0, 0, curve::RecruitGold};
    content.balance.slots = {0, 0, 0, curve::SlotGold};
    content.mine = campaignMine(stage, 0);
    content.deeperMines.push_back(campaignMine(stage, 1));
    content.deeperMines.push_back(campaignMine(stage, 2));
    campaignWaves(content, stage, std::min(6 + stage * 2, curve::MaxWaves));
    return content;
}
} // namespace
const Catalog &stageCatalog(int stageIndex) {
    static const std::array<Catalog, TotalStages> stages{
        {makeStage(1), makeStage(2), makeStage(3), makeStage(4), makeStage(5), makeStage(6), makeStage(7)}};
    return stages.at(static_cast<std::size_t>(std::clamp(stageIndex, 1, TotalStages) - 1));
}
const StageMetadata &stageMetadata(int stageIndex) {
    static const std::array<StageMetadata, TotalStages> metadata{{
        {1, "표층 방어선", "제1 작전 구역 · 표층 초원", "서쪽을 막고 북쪽 우회로와 남서 광산길에 대비하세요.",
         static_cast<int>(stageCatalog(1).waves.size()), 1, StageBiome::Meadow,
         "낙석 바위 · 정비 중 철로 치워 패드 확보"},
        {2, "흑요석 협곡", "제2 작전 구역 · 흑요석 지대", "교차하는 세 진입로가 거점 앞에서 합류합니다.",
         static_cast<int>(stageCatalog(2).waves.size()), 2, StageBiome::Obsidian,
         "특성 패드 · 고지 사거리↑ · 결정 지반 피해↑"},
        {3, "동토의 고대 갱도", "제3 작전 구역 · 동토 갱도",
         "외곽 순환과 중앙 우회로의 동시 공세를 막으세요.", static_cast<int>(stageCatalog(3).waves.size()), 3,
         StageBiome::Tundra, "갱도 터널 · 안에 든 적은 공격할 수 없음"},
        {4, "용암 심층부", "제4 작전 구역 · 용암 심층", "용암 균열을 따라 다섯 입구가 차례로 열립니다.",
         static_cast<int>(stageCatalog(4).waves.size()), 4, StageBiome::Lava,
         "용암 분출구 · 적과 아군 모두 피해"},
        {5, "토르륨 핵", "제5 작전 구역 · 토르륨 핵",
         "사방에서 중앙의 토르륨 핵으로 몰려드는 공세를 막으세요.",
         static_cast<int>(stageCatalog(5).waves.size()), 5, StageBiome::Core,
         "터널 · 분출구 · 특성 패드 복합 지형"},
        {6, "공명 수정굴", "제6 작전 구역 · 수정 공명굴", "수정 동굴의 네 갈래 공세를 광차로 쓸어내세요.",
         static_cast<int>(stageCatalog(6).waves.size()), 6, StageBiome::Crystal,
         "광차 선로 · 레버를 눌러 선로 위 적을 쓸어냄"},
        {7, "부식의 늪", "제7 작전 구역 · 산성 늪지", "늪을 건너오는 다섯 갈래 공세를 가스통으로 끊으세요.",
         static_cast<int>(stageCatalog(7).waves.size()), 7, StageBiome::Swamp,
         "늪 · 적 감속 · 가스통을 눌러 폭파"},
    }};
    return metadata.at(static_cast<std::size_t>(std::clamp(stageIndex, 1, TotalStages) - 1));
}
} // namespace thorium::game
