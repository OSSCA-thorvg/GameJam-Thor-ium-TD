#include "Catalog.h"
#include <algorithm>
#include <cmath>

namespace thorium::game {
namespace {
/** Create one enemy role with its common combat fields, named by its animation id; special abilities are
 *  assigned afterwards. */
EnemyDefinition role(const char *animation, int hp, int armor, float speed, int attack, float interval,
                     int leak) {
    EnemyDefinition result;
    result.name = animation;
    result.animation = animation;
    result.hp = hp;
    result.armor = armor;
    result.speed = speed;
    result.attack = attack;
    result.interval = interval;
    result.leak = leak;
    return result;
}
/** Author every enemy role in EnemyKind order. */
std::array<EnemyDefinition, EnemyKindCount> enemyRoster() {
    std::array<EnemyDefinition, EnemyKindCount> roster{{
        role("oreling", 36, 0, 1, 8, 1, 1),
        role("dashling", 24, 0, 1.6F, 6, .8F, 1),
        role("ironback", 100, 4, .65F, 16, 1.5F, 3),
        role("pebbling", 12, 0, 1.35F, 3, .6F, 1),
        role("shieldling", 60, 0, .8F, 10, 1.2F, 2),
        role("mender", 50, 0, .85F, 4, 1, 1),
        role("splitter", 70, 1, .8F, 10, 1.2F, 2),
        role("burrower", 50, 1, 1, 8, 1, 2),
        role("sapper", 45, 0, 1.1F, 6, 1, 2),
        role("bat", 24, 0, 1.2F, 0, 1, 1),
        role("stalker", 55, 0, 1.1F, 8, 1, 2),
        role("leaper", 70, 1, .95F, 10, 1.1F, 2),
        role("wrecker", 85, 2, .8F, 10, 1.2F, 2),
        role("slinger", 60, 0, .85F, 6, 1.2F, 2),
        role("carrier", 60, 0, 1, 0, 1, 1),
        role("golem", 900, 3, .36F, 30, 1.6F, 10),
    }};
    EnemyDefinition &shielded = roster[static_cast<std::size_t>(EnemyKind::Shielded)];
    shielded.shield = 60;
    shielded.shieldCap = 8;
    EnemyDefinition &healer = roster[static_cast<std::size_t>(EnemyKind::Healer)];
    healer.ability = EnemyAbility::Heal;
    healer.abilityInterval = 2.5F;
    healer.abilityRadius = 1.8F;
    healer.abilityAmount = 10;
    roster[static_cast<std::size_t>(EnemyKind::Splitter)].splitCount = 3;
    EnemyDefinition &burrower = roster[static_cast<std::size_t>(EnemyKind::Burrower)];
    burrower.ability = EnemyAbility::Burrow;
    burrower.abilityInterval = 4;
    burrower.abilityDuration = 1.6F;
    burrower.abilitySpeed = 2;
    EnemyDefinition &sapper = roster[static_cast<std::size_t>(EnemyKind::Sapper)];
    sapper.ability = EnemyAbility::Sabotage;
    sapper.abilityInterval = 1;
    sapper.abilityRadius = 1.6F;
    sapper.abilityAmount = 1;
    sapper.abilityDuration = 6;
    sapper.abilityWindup = 3;
    roster[static_cast<std::size_t>(EnemyKind::Flyer)].flying = true;
    roster[static_cast<std::size_t>(EnemyKind::Stalker)].stealth = 1.8F;
    EnemyDefinition &leaper = roster[static_cast<std::size_t>(EnemyKind::Leaper)];
    leaper.ability = EnemyAbility::Leap;
    leaper.abilityRadius = 2.5F;
    leaper.abilityAmount = 1;
    leaper.abilityWindup = .6F;
    EnemyDefinition &wrecker = roster[static_cast<std::size_t>(EnemyKind::Wrecker)];
    wrecker.ability = EnemyAbility::Siege;
    wrecker.abilityInterval = 1.2F;
    wrecker.abilityRadius = 1.4F;
    wrecker.abilityAmount = 12;
    EnemyDefinition &slinger = roster[static_cast<std::size_t>(EnemyKind::Slinger)];
    slinger.ability = EnemyAbility::Siege;
    slinger.abilityInterval = 2;
    slinger.abilityRadius = 3.8F;
    slinger.abilityAmount = 12;
    EnemyDefinition &carrier = roster[static_cast<std::size_t>(EnemyKind::Carrier)];
    carrier.flying = true;
    carrier.ability = EnemyAbility::Airdrop;
    carrier.abilityRadius = .55F;
    carrier.abilityAmount = 3;
    carrier.minionKind = EnemyKind::Normal;
    EnemyDefinition &boss = roster[static_cast<std::size_t>(EnemyKind::Boss)];
    boss.ability = EnemyAbility::Stomp;
    boss.abilityInterval = 7;
    boss.abilityRadius = 2.4F;
    boss.abilityAmount = 2;
    boss.abilityDuration = 1.5F;
    return roster;
}
/** Create one elite modifier with its health multiplier and leak bonus; effects are assigned afterwards. */
EliteDefinition elite(const char *name, float health, int leak) {
    EliteDefinition result;
    result.name = name;
    result.health = health;
    result.leak = leak;
    return result;
}
} // namespace
std::array<EliteDefinition, EliteAffixCount> eliteRoster() {
    std::array<EliteDefinition, EliteAffixCount> roster{{
        elite("", 1, 0),
        elite("steel", 1.3F, 0),
        elite("swift", 1.4F, 0),
        elite("regen", 1.5F, 0),
        elite("enrage", 1.5F, 0),
        elite("leader", 5, 2),
    }};
    roster[static_cast<std::size_t>(EliteAffix::Steel)].armor = 2;
    EliteDefinition &swift = roster[static_cast<std::size_t>(EliteAffix::Swift)];
    swift.auraRadius = 1.8F;
    swift.auraSpeed = 1.2F;
    roster[static_cast<std::size_t>(EliteAffix::Regen)].regen = .04F;
    EliteDefinition &enrage = roster[static_cast<std::size_t>(EliteAffix::Enrage)];
    enrage.enrageBelow = .5F;
    enrage.enrageSpeed = 1.6F;
    EliteDefinition &leader = roster[static_cast<std::size_t>(EliteAffix::Leader)];
    leader.speed = .9F;
    leader.auraRadius = 2.2F;
    leader.auraGuard = .35F;
    return roster;
}
const CardDefinition &Catalog::card(CardKind kind) const {
    return cards.at(static_cast<std::size_t>(kind));
}
bool Catalog::isBuilding(CardKind kind) const {
    const int index = static_cast<int>(kind);
    return index >= 0 && static_cast<std::size_t>(index) < cards.size() &&
           card(kind).category == CardCategory::Building;
}
bool Catalog::isUnit(CardKind kind) const {
    const int index = static_cast<int>(kind);
    return index >= 0 && static_cast<std::size_t>(index) < cards.size() &&
           card(kind).category == CardCategory::Unit;
}
const EnemyDefinition &Catalog::enemy(EnemyKind kind) const {
    return enemies.at(static_cast<std::size_t>(kind));
}
const EliteDefinition &Catalog::elite(EliteAffix affix) const {
    return elites.at(static_cast<std::size_t>(affix));
}
int Catalog::towerHealth(int level) const {
    return std::max(1, static_cast<int>(std::lround(balance.towerHp *
                                                    (1 + balance.towerLevelHp * std::max(0, level - 1)))));
}
int Catalog::health(const Enemy &live) const {
    if (live.maxHp > 0)
        return live.maxHp;
    return static_cast<int>(std::lround(enemy(live.kind).hp * elite(live.affix).health));
}
float Catalog::waveVigor(int wave) const {
    return wave >= 1 && static_cast<std::size_t>(wave) <= vigor.size()
               ? vigor[static_cast<std::size_t>(wave - 1)]
               : 1.0F;
}
int Catalog::shield(const Enemy &live) const {
    return live.maxShield > 0 ? live.maxShield : enemy(live.kind).shield;
}
int Catalog::armor(const Enemy &live) const {
    return enemy(live.kind).armor + elite(live.affix).armor;
}
std::vector<Spawn> Catalog::schedule(int wave) const {
    std::vector<Spawn> result;
    for (const SpawnGroup &group : waves.at(static_cast<std::size_t>(wave - 1)))
        for (int index = 0; index < group.count; ++index)
            result.push_back({group.start + index * group.interval, group.kind, group.route, group.affix});
    std::stable_sort(result.begin(), result.end(),
                     [](const Spawn &a, const Spawn &b) { return a.time < b.time; });
    return result;
}
int Catalog::introductionWave(EnemyKind kind) const {
    for (std::size_t wave = 0; wave < waves.size(); ++wave)
        for (const SpawnGroup &group : waves[wave])
            if (group.kind == kind && group.count > 0)
                return static_cast<int>(wave + 1);
    return 0;
}
namespace {
/** Mark the attackers that reach only enemies on the ground. */
Catalog withGroundAttackers(Catalog content) {
    for (CardKind kind : {CardKind::Cannon, CardKind::Guard, CardKind::Demolition})
        content.cards[static_cast<std::size_t>(kind)].ground = true;
    return content;
}
} // namespace
const Catalog &defaultCatalog() {
    static const Catalog content = withGroundAttackers(
        {{{{"gun", 3, CardCategory::Building, 3, 12, .75F},
           {"cannon", 4, CardCategory::Building, 3.5F, 26, 1.8F, 1.1F},
           {"guard", 3, CardCategory::Unit, 1.1F, 10, 1, 0, 120, 2},
           {"frost", 2, CardCategory::Spell, 2, 0, 1, 0, 0, 0, 1, 0, 3},
           {"blast", 3, CardCategory::Spell, 1.5F, 38},
           {"lightning", 5, CardCategory::Building, 3, 10, .8F, 0, 0, 0, 3, 2},
           {"shooter", 3, CardCategory::Unit, 3, 12, 1, 0, 70, 1},
           {"demolition", 4, CardCategory::Unit, 2.8F, 18, 1.8F, 1.1F, 65, 1},
           {"flame", 4, CardCategory::Building, 2.2F, 6, .25F, 0, 0, 0, 1, 0, 0, 35},
           {"sniper", 5, CardCategory::Building, 7, 70, 2.8F},
           {"generator", 3, CardCategory::Building, 2.3F, 0, 1, 0, 0, 0, 1, 0, 0, 0, .3F},
           {"tar", 2, CardCategory::Spell, 1.8F, 0, 1, 0, 0, 0, 1, 0, 14}}},
         enemyRoster(),
         {{CardKind::Gun, CardKind::Guard, CardKind::Cannon, CardKind::Frost, CardKind::Gun, CardKind::Guard,
           CardKind::Blast, CardKind::Gun}},
         {{4, 3}, {8, 3}, {8, 7}, {14, 7}, {14, 5}, {16, 5}},
         {{6, 4}, {9, 4}, {7, 6}, {10, 6}, {12, 8}, {15, 6}},
         {{8, 4}, {8, 6}, {10, 7}, {12, 7}, {14, 6}, {15, 5}, {9, 7}, {13, 7}},
         {"surface_shaft",
          0,
          5,
          6,
          2,
          {{},
           {Ore::Rock, 0, 1, false, true},
           {Ore::Rock, 0, 1, false, true},
           {Ore::Rock, 0, 1, false, true},
           {},
           {},
           {Ore::Iron, 2, 1, false, false, true},
           {Ore::Copper, 2, 1, false, false, true},
           {Ore::Iron, 2, 1, false, false, true},
           {Ore::Iron, 2},
           {},
           {Ore::Iron, 2},
           {Ore::Copper, 2},
           {Ore::Gold, 4},
           {Ore::Gold, 8, 1, true},
           {Ore::Iron, 4, 1, true},
           {Ore::Gold, 4},
           {Ore::Rock, 0, 2},
           {Ore::Iron, 2},
           {},
           {},
           {},
           {Ore::Rock, 0, 2},
           {Ore::Gold, 4},
           {},
           {},
           {},
           {Ore::Crystal, 2, 2},
           {},
           {}}},
         {{{EnemyKind::Normal, 12, 3, 1.2F}, {EnemyKind::Runner, 4, 17.4F, .8F}},
          {{EnemyKind::Normal, 16, 3, .8F},
           {EnemyKind::Runner, 8, 16.2F, .6F},
           {EnemyKind::Armored, 4, 21.6F, 2}},
          {{EnemyKind::Normal, 24, 3, .6F},
           {EnemyKind::Runner, 12, 18, .5F},
           {EnemyKind::Armored, 8, 24.7F, 1.2F}}},
         {{{{{0, 1}, {3, 1}, {3, 0}, {6, 0}, {6, 2}, {4, 2}}, {{1, 2}, {5, 1}}, {{2, 1}, {5, 0}}},
           {{{0, 3}}, {{1, 2}, {3, 2}, {1, 4}, {3, 4}}, {{1, 3}, {3, 3}}}}},
         {}});
    return content;
}

float distance(Point a, Point b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}
} // namespace thorium::game
