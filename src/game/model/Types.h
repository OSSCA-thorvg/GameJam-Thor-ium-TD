#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace thorium::game {
using EntityId = std::uint32_t;
using CardId = std::uint32_t;
using OfferId = std::uint32_t;

/** Logical surface coordinates; independent of drawing pixels and SDL events. */
struct Point {
    float x = 0.0F;
    float y = 0.0F;
};

/** Game phase; changing the visible view never changes this state. */
enum class Phase { Ready, Battle, Prep, Won, Lost };
/** Supported card definitions, shared by initial and acquired instances. */
enum class CardKind { Gun, Cannon, Guard, Frost, Blast, Lightning, Shooter, Demolition };
/** Supported preparation upgrades, independent of UI control IDs. */
enum class UpgradeKind { Tool, Armor, Slots, Coil };
/** Initial three enemy roles. */
enum class EnemyKind { Normal, Runner, Armored };
/** Mining contents; gold is a wallet, not a crafting inventory entry. */
enum class Ore { Rock, Iron, Copper, Crystal, Gold };

/** An operation result suitable for a UI message without partial mutation. */
struct Result {
    bool ok = false;
    std::string message;
};
/** Category selects a rule family without teaching the engine about cards. */
enum class CardCategory { Building, Unit, Spell };
/** Immutable card balance and action definition copied into a run catalog. */
struct CardDefinition {
    std::string name;
    int cost = 0;
    CardCategory category = CardCategory::Spell;
    float range = 3.0F;
    int damage = 0;
    float interval = 1.0F;
    float splash = 0.0F;
    int hp = 0;
    int blockers = 0;
    int chains = 1;
    float chainRange = 0.0F;
    float slowDuration = 0.0F;
};
/** One atomic recipe or card-offer price, shared by validation, UI and payment. */
struct ResourceCost {
    int iron = 0;
    int copper = 0;
    int crystal = 0;
    int gold = 0;
};
/** Offer identity and lifetime belong to the current run, independently of open dialogs. */
enum class OfferSource { Recruitment, Expansion };
/** Immutable while active; consuming or skipping removes this offer from the run. */
struct CardOffer {
    OfferId id = 0;
    OfferSource source = OfferSource::Recruitment;
    std::vector<CardKind> candidates;
    ResourceCost cost;
};
/** Stable card instance whose identity survives hand circulation. */
struct Card {
    CardId id = 0;
    CardKind kind = CardKind::Gun;
};
/** A surviving building, independent of the card that originally placed it. */
struct Tower {
    EntityId id;
    CardKind kind;
    Point position;
    int level = 1;
    float cooldown = 0.0F;
};
/** A summoned ally; hit points and identity persist between waves. */
struct Unit {
    EntityId id;
    CardKind kind;
    Point position;
    int hp;
    int maxHp;
    float cooldown = 0.0F;
};
/** One enemy progressing along the current route in logical tile units. */
struct Enemy {
    EntityId id;
    EnemyKind kind;
    Point position;
    float progress = 0.0F;
    int hp = 0;
    float cooldown = 0.0F;
    float slowUntil = 0.0F;
    EntityId blocker = 0;
};
/** Committed combat facts; animation timing belongs to the presentation consumer. */
enum class CombatEventKind { Attack, Damage, Defeated, Leak, Placement };
/** One immutable combat result, retained even when its subject was removed this tick. */
struct CombatEvent {
    CombatEventKind type;
    EntityId subject;
    Point from;
    Point to;
    CardKind card = CardKind::Gun;
    int amount = 0;
    bool enemy = true;
    float radius = 0.0F;
    EnemyKind enemyKind = EnemyKind::Normal;
};
/** Public and hidden mining fields remain distinct even in the native process. */
struct MineCell {
    Ore ore = Ore::Rock;
    int amount = 0;
    int grade = 1;
    bool nest = false;
    int clue = 0;
    bool dug = false;
    bool info = false;
    bool clueKnown = false;
    bool flagged = false;
};
/** A single miner job; resources are granted only on its completion. */
struct MineJob {
    bool active = false;
    int target = -1;
    std::vector<int> path;
    float elapsed = 0.0F;
    float duration = 0.0F;
};
/** A scheduled spawn, including delayed nest threats. */
struct Spawn {
    float time;
    EnemyKind kind;
};

/** Calculate a Euclidean logical distance. */
float distance(Point a, Point b);

} // namespace thorium::game
