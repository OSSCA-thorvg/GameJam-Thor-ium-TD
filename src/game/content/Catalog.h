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
};
/** One deterministic spawn group; schedules remain private live run state. */
struct SpawnGroup {
    EnemyKind kind = EnemyKind::Normal;
    int count = 0;
    float start = 0;
    float interval = 0;
};
/** Authored tile contents and initial visibility, without excavation progress. */
struct MineTileDefinition {
    Ore ore = Ore::Rock;
    int amount = 0;
    int grade = 1;
    bool nest = false;
    bool opened = false;
    bool revealed = false;
};
/** Additional route prefix and placement cells for one expansion choice. */
struct ExpansionDefinition {
    std::vector<Point> prefix;
    std::vector<Point> pads;
    std::vector<Point> unitCells;
    CardKind reward = CardKind::Lightning;
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
};
/** Typed content; Run owns a copy and exposes only a const view for its lifetime. */
struct Catalog {
    std::array<CardDefinition, 8> cards{};
    std::array<EnemyDefinition, 3> enemies{};
    std::array<CardKind, 8> starterDeck{};
    std::vector<Point> route;
    std::vector<Point> pads;
    std::vector<Point> unitCells;
    std::array<MineTileDefinition, 30> mine{};
    std::vector<std::vector<SpawnGroup>> waves;
    std::array<ExpansionDefinition, 2> expansions{};
    RunBalance balance;
    std::array<CardKind, 3> recruitmentCandidates{CardKind::Guard, CardKind::Shooter, CardKind::Demolition};
    /** Resolve one supported card; invalid enum values are rejected at the command boundary. */
    const CardDefinition &card(CardKind kind) const;
    /** Check whether this catalog defines the kind as a reusable building. */
    bool isBuilding(CardKind kind) const;
    /** Check supported enum bounds and the independent summon category. */
    bool isUnit(CardKind kind) const;
    /** Resolve one authored enemy role. */
    const EnemyDefinition &enemy(EnemyKind kind) const;
    /** Build a chronological schedule for a one-based wave number. */
    std::vector<Spawn> schedule(int wave) const;
};
/** Shipped content is immutable; copies may be edited before constructing a new run. */
const Catalog &defaultCatalog();

/** Authored presentation metadata for an expedition stage. */
struct StageMetadata {
    int id = 1;
    std::string name;
    std::string subtitle;
    std::string description;
    int wavesCount = 3;
    int difficulty = 1;
};

/** Total number of playable campaign stages. */
inline constexpr int TotalStages = 3;

/** Resolve authored content for a specific 1-based stage index. */
const Catalog &stageCatalog(int stageIndex);

/** Resolve display metadata for a specific 1-based stage index. */
const StageMetadata &stageMetadata(int stageIndex);
} // namespace thorium::game
