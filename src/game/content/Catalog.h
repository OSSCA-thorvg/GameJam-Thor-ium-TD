#pragma once
#include "game/model/Types.h"

namespace thorium::game {
/** Immutable enemy balance in seconds, tiles and hit points. */
struct EnemyDefinition {
    int hp = 0;
    int armor = 0;
    float speed = 0;
    int attack = 0;
    float interval = 0;
    int leak = 0;
    std::string animation = "oreling";
    std::string name;
    /** Hit points absorbed before health; each hit absorbs at most shieldCap while it remains. */
    int shield = 0;
    int shieldCap = 0;
    EnemyAbility ability = EnemyAbility::None;
    /** Seconds between activations; the first activation follows one full interval after spawning. */
    float abilityInterval = 0;
    /** Heal or stomp radius in tiles; for Sabotage, the reach from the sapper to a tower. */
    float abilityRadius = 0;
    /** Healed hit points for Heal, summoned minions for Stomp, charges for Sabotage. */
    int abilityAmount = 0;
    /** Underground time for Burrow, tower stun time for Stomp, tower disabled time for Sabotage. */
    float abilityDuration = 0;
    /** Seconds a Sabotage charge is planted, standing still, before it disables the tower. */
    float abilityWindup = 0;
    /** Movement multiplier while the ability lasts. */
    float abilitySpeed = 1;
    /** Minions created on defeat, at the defeated enemy's route progress. */
    int splitCount = 0;
    EnemyKind minionKind = EnemyKind::Swarm;
};
/** Immutable balance of one elite modifier, applied on top of the carrier's role. */
struct EliteDefinition {
    std::string name;
    /** Multiplier of the role's maximum health. */
    float health = 1;
    /** Armor added to the role's armor. */
    int armor = 0;
    /** Multiplier of the carrier's own movement. */
    float speed = 1;
    /** Base damage added when the carrier leaks. */
    int leak = 0;
    /** Radius in tiles of the effect on other enemies; zero means no aura. */
    float auraRadius = 0;
    /** Movement multiplier of other enemies inside the aura; the strongest aura applies. */
    float auraSpeed = 1;
    /** Fraction of incoming damage removed from other enemies inside the aura; the strongest aura applies. */
    float auraGuard = 0;
    /** Fraction of maximum health restored once per second. */
    float regen = 0;
    /** Health fraction at or under which the carrier enrages, and its movement multiplier afterwards. */
    float enrageBelow = 0;
    float enrageSpeed = 1;
};
/** One deterministic spawn group; schedules remain private live run state. */
struct SpawnGroup {
    EnemyKind kind = EnemyKind::Normal;
    int count = 0;
    float start = 0;
    float interval = 0;
    std::uint32_t route = 0;
    EliteAffix affix = EliteAffix::None;
};
/** Authored tile contents and initial visibility, without excavation progress. */
struct MineTileDefinition {
    Ore ore = Ore::Rock;
    /** Mineral amount, or the next battle's bonus energy for an Energy special. */
    int amount = 0;
    int grade = 1;
    bool nest = false;
    bool opened = false;
    bool revealed = false;
    MineSpecial special = MineSpecial::None;
    Relic relic = Relic::Battery;
    CardKind card = CardKind::Gun;
};
/** An additional authored entrance; every route ends at the same defended base. */
struct EntranceRoute {
    std::string name;
    std::vector<Point> points;
    int opensAtWave = 1;
};
/** Ground palette and decoration family used to draw a stage's battlefield and cutaways. */
enum class StageBiome { Meadow, Obsidian, Tundra, Lava, Core };
/** One row-major cutaway board; deeper layers are entered voluntarily without refilling the preparation
 * budget. */
struct MineLayerDefinition {
    std::string name;
    int opensAfterWave = 3;
    int width = 5;
    int height = 6;
    /** Opened cell where the miner stands when the layer becomes active. */
    int start = 2;
    std::vector<MineTileDefinition> cells;
    /** Rock and vein palette of the cutaway. */
    StageBiome biome = StageBiome::Meadow;
};
/** Public forecast of scheduled pressure, excluding hidden mine contents. */
struct LaneForecast {
    std::uint32_t route = 0;
    int count = 0;
    int runners = 0;
    int armored = 0;
    /** Scheduled spawns per EnemyKind, including nest reinforcements as Normal. */
    std::array<int, EnemyKindCount> kinds{};
    /** Scheduled elite spawns per EliteAffix; index None stays zero. */
    std::array<int, EliteAffixCount> elites{};
};
/** Additional route prefix and placement cells for one expansion choice. */
struct ExpansionDefinition {
    std::vector<Point> prefix;
    std::vector<Point> pads;
    std::vector<Point> unitCells;
    CardKind reward = CardKind::Lightning;
};
/** One building pad with a terrain trait; its position is an authored pad, expansion pad or boulder. */
struct TraitPad {
    Point position;
    PadTrait trait = PadTrait::None;
};
/** An underground span of one route, from `enter` up to `exit` tiles of progress along the authored route. */
struct TunnelSpan {
    std::uint32_t route = 0;
    float enter = 0;
    float exit = 0;
};
/** A fixed lava vent; it first erupts `phase` seconds into every battle, more than zero and at most one vent
 *  period, and then once per period. */
struct VentDefinition {
    Point position;
    float phase = 0;
};
/** Format an authored price consistently for rules and game UI. */
std::string costLabel(const ResourceCost &cost, bool compact = false);
/** Balance that links economy, wave transitions and permanent run upgrades. */
struct RunBalance {
    int baseHp = 20;
    int initialEnergy = 6;
    int energyCap = 10;
    float energyInterval = 2.5F;
    int initialSlots = 6;
    int upgradedSlots = 8;
    int prepDigs = 5;
    int prepScans = 1;
    int nestEnemies = 4;
    int expansionWave = 2;
    int armoredGuardHp = 150;
    float levelBonus = .25F;
    float coilBonus = 1.25F;
    float slowFactor = .6F;
    float minerStep = .2F;
    float softDig = .5F;
    float hardDig = .8F;
    ResourceCost tool{4, 4};
    ResourceCost armor{4};
    ResourceCost slots{0, 0, 0, 4};
    ResourceCost coil{2, 0, 2};
    ResourceCost heal{1, 1};
    ResourceCost recruit{0, 0, 0, 4};
    /** Battery relic: added to every battle's starting energy. */
    int relicEnergy = 1;
    /** Charm relic: added to every preparation's dig budget. */
    int relicDigs = 1;
    /** Ward relic: subtracted from each excavated nest's reinforcements. */
    int relicNestReduction = 2;
    /** Lens relic: added to every preparation's free scans. */
    int relicScans = 1;
    /** Third pickaxe tier; requires the T2 pickaxe and adds drillDigs to every preparation's dig budget. */
    ResourceCost drill{0, 0, 5};
    int drillDigs = 1;
    /** Hired miner: one extra dig in the current preparation for hireGold plus hireStep per earlier hire. */
    int hireGold = 4;
    int hireStep = 3;
    int hireLimit = 2;
    /** Supply crate: bought once per preparation and added to the next battle's starting energy. */
    ResourceCost supply{0, 0, 0, 4};
    int supplyEnergy = 2;
    /** Awakening prices from card level 3 to 4, then 4 to 5; a building's top level is 3 plus their count. */
    std::array<ResourceCost, 2> awaken{{{0, 0, 3}, {0, 0, 5}}};
    /** Excavate: rock layers a T1 pick strips from the struck tile; each pickaxe tier adds one. */
    int pickPower = 2;
    /** Excavate: rock layers a blast strips from each tile of its block; T2 and above add one. */
    int blastPower = 1;
    /** Excavate: digs one blast costs. */
    int blastDigs = 2;
    /** Excavate: tiles around the target that one scan shows through, as a distance in tiles. */
    int scanReach = 1;
    /** Excavate: strikes may target any tile; otherwise only the top row, bare tiles and tiles touching a
     * bare tile, so the wall is dug from the surface down. */
    bool openWall = false;
    /** Price of clearing one boulder off a blocked building pad during preparation. */
    ResourceCost boulder{3};
    /** Range multiplier of a building on high ground, and damage multiplier of one on crystal ground. */
    float highGroundRange = 1.2F;
    float crystalDamage = 1.2F;
    /** Movement multiplier of enemies inside a tunnel. */
    float tunnelSpeed = 1.5F;
    /** Lava vents: seconds between eruptions, seconds of warning before each one, blast radius in tiles and
     *  the damage dealt to every surfaced enemy and allied unit inside it. */
    float ventPeriod = 9;
    float ventWarning = 1.5F;
    float ventRadius = 1.5F;
    int ventDamage = 30;
};
/** Author every elite modifier in EliteAffix order. */
std::array<EliteDefinition, EliteAffixCount> eliteRoster();
/** Typed content; Run owns a copy and exposes only a const view for its lifetime. */
struct Catalog {
    std::array<CardDefinition, CardKindCount> cards{};
    std::array<EnemyDefinition, EnemyKindCount> enemies{};
    std::array<CardKind, 8> starterDeck{};
    std::vector<Point> route;
    std::vector<Point> pads;
    std::vector<Point> unitCells;
    MineLayerDefinition mine;
    std::vector<std::vector<SpawnGroup>> waves;
    std::array<ExpansionDefinition, 2> expansions{};
    RunBalance balance;
    std::array<CardKind, 3> recruitmentCandidates{CardKind::Guard, CardKind::Shooter, CardKind::Demolition};
    int width = 18;
    int height = 10;
    std::string entranceName = "서쪽 입구";
    std::vector<EntranceRoute> additionalRoutes;
    std::vector<MineLayerDefinition> deeperMines;
    /** One-based wave that presents a role as new in this stage, or 0 when it is not introduced here. */
    std::array<int, EnemyKindCount> enemyDebuts{};
    /** Elite modifiers in EliteAffix order; None is the neutral entry. */
    std::array<EliteDefinition, EliteAffixCount> elites = eliteRoster();
    /** Health and shield multiplier of non-boss enemies created in each one-based wave, indexed from wave
     * one; missing entries are 1. */
    std::vector<float> vigor;
    /** One-based wave that first fields an elite modifier in this stage, or 0 when none does. */
    std::array<int, EliteAffixCount> eliteDebuts{};
    /** How this run spends digs; Excavate buries each layer's contents in a generated wall. */
    MiningMode mining = MiningMode::Sweep;
    /** Seed of the generated excavation walls; unused by Sweep. */
    std::uint32_t mineSeed = 0;
    /** Building pads that start blocked by a boulder; a cleared one becomes an ordinary pad. */
    std::vector<Point> boulders;
    /** Building pads that carry a terrain trait. */
    std::vector<TraitPad> padTraits;
    /** Underground route spans; enemies inside cannot be targeted, blocked or hit by area effects. */
    std::vector<TunnelSpan> tunnels;
    /** Lava vents that periodically damage enemies and allied units around them. */
    std::vector<VentDefinition> vents;
    /** Resolve one supported card; invalid enum values are rejected at the command boundary. */
    const CardDefinition &card(CardKind kind) const;
    /** Check whether this catalog defines the kind as a reusable building. */
    bool isBuilding(CardKind kind) const;
    /** Check supported enum bounds and the independent summon category. */
    bool isUnit(CardKind kind) const;
    /** Resolve one authored enemy role. */
    const EnemyDefinition &enemy(EnemyKind kind) const;
    /** Resolve one elite modifier. */
    const EliteDefinition &elite(EliteAffix affix) const;
    /** Health and shield multiplier of non-boss enemies created in a one-based wave. */
    float waveVigor(int wave) const;
    /** Maximum health of a live enemy, including its wave and elite multipliers. */
    int health(const Enemy &enemy) const;
    /** Maximum shield of a live enemy, including its wave multiplier. */
    int shield(const Enemy &enemy) const;
    /** Armor of a live enemy, including its elite modifier. */
    int armor(const Enemy &enemy) const;
    /** Build a chronological schedule for a one-based wave number. */
    std::vector<Spawn> schedule(int wave) const;
    /** Find the first one-based wave that schedules a kind, or 0 when no wave does. */
    int introductionWave(EnemyKind kind) const;
};
/** Compact starter scenario shared by rule fixtures and the separate practice expedition. */
const Catalog &defaultCatalog();

/** Authored presentation metadata for an expedition stage. */
struct StageMetadata {
    int id = 1;
    std::string name;
    std::string subtitle;
    std::string description;
    int wavesCount = 0;
    int difficulty = 1;
    StageBiome biome = StageBiome::Meadow;
    /** One-line summary of the stage's battlefield gimmick; empty when it has none. */
    std::string gimmick;
};

/** Total number of playable campaign stages. */
inline constexpr int TotalStages = 5;

/** Resolve authored content for a specific 1-based stage index. */
const Catalog &stageCatalog(int stageIndex);

/** Author one campaign cutaway for a 1-based stage; depth 0 is the starting layer. */
MineLayerDefinition campaignMine(int stage, int depth);

/** Resolve display metadata for a specific 1-based stage index. */
const StageMetadata &stageMetadata(int stageIndex);
} // namespace thorium::game
