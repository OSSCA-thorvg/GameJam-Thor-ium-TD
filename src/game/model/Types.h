#pragma once
#include <array>
#include <cstddef>
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
enum class CardKind {
    Gun,
    Cannon,
    Guard,
    Frost,
    Blast,
    Lightning,
    Shooter,
    Demolition,
    Flame,
    Sniper,
    Generator,
    Tar
};
/** Number of supported card definitions; every catalog defines each kind exactly once. */
inline constexpr int CardKindCount = 12;
/** Supported preparation purchases, independent of UI control IDs. */
enum class UpgradeKind { Tool, Armor, Slots, Coil, Drill, Hire, Supply };
/** Enemy roles; the zone boss is the last entry. */
enum class EnemyKind {
    Normal,
    Runner,
    Armored,
    Swarm,
    Shielded,
    Healer,
    Splitter,
    Burrower,
    Sapper,
    Flyer,
    Stalker,
    Leaper,
    Wrecker,
    Slinger,
    Carrier,
    Boss
};
/** Number of supported enemy roles, sized for per-kind tables. */
inline constexpr std::size_t EnemyKindCount = 16;
/** Periodic behaviour of an enemy role; timing and strength are catalog data. */
enum class EnemyAbility { None, Heal, Burrow, Stomp, Sabotage, Leap, Siege, Airdrop };
/** Elite modifier of one spawned enemy; None is an ordinary enemy and Leader is the mid-stage pack leader. */
enum class EliteAffix { None, Steel, Swift, Regen, Enrage, Leader };
/** Number of elite modifiers including None, sized for per-affix tables. */
inline constexpr std::size_t EliteAffixCount = 6;
/** Mining contents; gold is a wallet, not a crafting inventory entry. */
enum class Ore { Rock, Iron, Copper, Crystal, Gold };
/** Non-mineral content of a mine tile, resolved once when the tile is excavated. */
enum class MineSpecial { None, Relic, Fossil, Unstable, Energy };
/** How a preparation's digs are spent underground: Sweep opens single tiles beside the gallery with nest
 * clues, Excavate strips a layered rock wall to uncover shaped deposits. */
enum class MiningMode { Sweep, Excavate };
/** Excavation strike patterns: the pick strips one tile deeply and its four neighbours lightly, the blast
 * strips a three-by-three block and wakes any nest it touches. */
enum class StrikeTool { Pick, Blast };
/** One-run passive bonuses granted by relic chests; each relic is owned at most once per run. */
enum class Relic { Battery, Charm, Ward, Lens };
/** Number of supported relics, sized for per-relic tables. */
inline constexpr int RelicCount = 4;

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
    /** Half-angle in degrees of a cone attack toward the selected target; zero disables the cone. */
    float arc = 0.0F;
    /** Damage bonus fraction granted to other buildings within range; such buildings never attack. */
    float aura = 0.0F;
    /** Attacks reach only enemies on the ground, never flying ones. */
    bool ground = false;
};
/** One atomic recipe or card-offer price, shared by validation, UI and payment. */
struct ResourceCost {
    int iron = 0;
    int copper = 0;
    int crystal = 0;
    int gold = 0;
};
/** Offer identity and lifetime belong to the current run; Expansion and Fossil offers are free single cards.
 */
enum class OfferSource { Recruitment, Expansion, Fossil };
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
    /** Seconds this building stays sabotaged: it neither attacks nor supports. Cleared when a wave ends. */
    float disabled = 0.0F;
    /** Durability; the building is destroyed at zero. Damage carries over between waves. */
    int hp = 0;
    int maxHp = 0;
};
/** A summoned ally; hit points and identity persist between waves. */
struct Unit {
    EntityId id;
    CardKind kind;
    Point position;
    int hp;
    int maxHp;
    float cooldown = 0.0F;
    /** Fraction of one hit point recovered but not yet applied. */
    float mending = 0.0F;
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
    std::uint32_t route = 0;
    int shield = 0;
    float ability = 0.0F;
    float burrowUntil = 0.0F;
    EliteAffix affix = EliteAffix::None;
    /** Maximum health and shield including the wave and elite multipliers; zero means the role's authored
     * values. */
    int maxHp = 0;
    int maxShield = 0;
    /** Tower a sapper is planting a charge on; the ability timer then counts down the plant. */
    EntityId sabotage = 0;
    /** Remaining sabotage charges of a sapper. */
    int charges = 0;
    /** Seconds until the next regeneration pulse of a regenerating elite. */
    float pulse = 0.0F;
    /** Set once an enraging elite falls under its health threshold. */
    bool enraged = false;
    /** Battle time until which a stealthy enemy stays visible at any range after being hit. */
    float revealedUntil = 0.0F;
};
/** Committed combat facts; animation timing belongs to the presentation consumer. */
enum class CombatEventKind {
    Attack,
    Damage,
    Defeated,
    Leak,
    Placement,
    Heal,
    Split,
    Burrow,
    Stomp,
    ShieldBreak,
    /** A sapper starts planting on a tower: from the sapper to the tower, radius is the plant time. */
    Sabotage,
    /** A planted charge disables a tower: subject is the tower, radius is the disabled time. */
    Disable,
    /** An enraging elite fell under its threshold and speeds up. */
    Enrage,
    /** A lava vent erupts: subject is the vent's catalog index, from and to are its position, radius is the
     *  blast radius and amount is the number of enemies and allied units it hit. */
    Eruption,
    /** A leaper jumps over the unit blocking it: from and to are its positions before and after the jump. */
    Leap,
    /** A mine cart leaves on its rail: subject is the rail's catalog index, from and to are the rail's ends.
     */
    Cart,
    /** A gas tank explodes: subject is the tank's catalog index, radius is the blast radius and amount is the
     *  number of enemies and allied units it hit. */
    Detonation,
    /** An enemy strikes a building: subject is the building, from is the enemy, amount is the damage. */
    Batter,
    /** A building is destroyed: subject is the building, from and to are its position. */
    Wreck,
    /** A carrier drops its cargo: subject is the carrier, from is where it dropped, amount is the cargo. */
    Airdrop
};
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
    EliteAffix affix = EliteAffix::None;
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
    MineSpecial special = MineSpecial::None;
    /** Granted relic when `special` is Relic. */
    Relic relic = Relic::Battery;
    /** Offered card when `special` is Fossil. */
    CardKind card = CardKind::Gun;
    /** Opened by a neighbouring cave-in instead of a dig; its contents were lost. */
    bool rubble = false;
    /** Excavate: rock layers still covering the cell; zero means exposed. */
    int cover = 0;
    /** Excavate: one-based index of the buried deposit occupying the cell, or zero for plain rock. */
    int deposit = 0;
};
/** One buried object of an excavation wall; its contents are granted once every cell is exposed. */
struct Deposit {
    Ore ore = Ore::Rock;
    /** Mineral amount, or the next battle's bonus energy for an Energy special. */
    int amount = 0;
    MineSpecial special = MineSpecial::None;
    Relic relic = Relic::Battery;
    CardKind card = CardKind::Gun;
    bool nest = false;
    /** Pickaxe tier needed to strip the rock over the object. */
    int grade = 1;
    /** Row-major wall cells the object occupies. */
    std::vector<int> cells;
    bool collected = false;
    /** Set once a nest has added its reinforcements, by a blast or by being collected. */
    bool woken = false;
};
/** One cell reached by an excavation strike and the rock layers the strike removes from it. */
struct StrikeHit {
    int cell = -1;
    int power = 0;
};
/** A single miner job; resources are granted only on its completion. */
struct MineJob {
    bool active = false;
    int target = -1;
    std::vector<int> path;
    float elapsed = 0.0F;
    float duration = 0.0F;
};
/** A temporary battlefield area created by a spell; cleared when its wave ends. */
struct GroundField {
    EntityId id = 0;
    CardKind kind = CardKind::Tar;
    Point position;
    float radius = 0.0F;
    float until = 0.0F;
};
/** Terrain property of a building pad; None is an ordinary pad. */
enum class PadTrait { None, HighGround, Crystal };
/** A scheduled spawn, including delayed nest threats. */
struct Spawn {
    float time;
    EnemyKind kind;
    std::uint32_t route = 0;
    EliteAffix affix = EliteAffix::None;
};

/** Calculate a Euclidean logical distance. */
float distance(Point a, Point b);

} // namespace thorium::game
