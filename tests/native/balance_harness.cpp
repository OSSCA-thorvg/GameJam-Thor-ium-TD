#include "game/autoplay/Autoplayer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace thorium::game;
namespace {
/** Optional starter-deck substitutions applied to every simulated stage, for card-value ablations. */
std::vector<std::pair<CardKind, CardKind>> deckSwaps;
/** Print every building and per-lane coverage after each wave. */
bool towerDump = false;
/** Sensitivity knobs: scale group counts of waves in [from, to], and scale non-boss health and speed. */
float countScale = 1;
int countScaleFrom = 1;
int countScaleTo = 1000;
float healthScale = 1;
float speedScale = 1;
/** Mining mode of every harness run, and the wall seed of the trial being played. */
MiningMode miningMode = MiningMode::Sweep;
std::uint32_t wallSeed = 0;
/** Excavation ablation: let strikes target any wall tile. */
bool openWall = false;
/** Sink ablation: preparation purchases priced out of reach, named as in the sink report. */
std::vector<std::string> disabledSinks;
/** Check whether a sink was priced out for this harness run. */
bool disabled(const char *sink) {
    return std::find(disabledSinks.begin(), disabledSinks.end(), sink) != disabledSinks.end();
}
/** Construct a shipped stage, with optional knobs, through the same validated boundary as the application. */
Run stageRun(int stage) {
    Catalog catalog = stageCatalog(stage);
    for (const auto &swap : deckSwaps) {
        for (CardKind &kind : catalog.starterDeck)
            if (kind == swap.first)
                kind = swap.second;
        for (ExpansionDefinition &expansion : catalog.expansions)
            if (expansion.reward == swap.first)
                expansion.reward = swap.second;
    }
    for (std::size_t wave = static_cast<std::size_t>(countScaleFrom - 1);
         wave < catalog.waves.size() && static_cast<int>(wave) < countScaleTo; ++wave)
        for (SpawnGroup &group : catalog.waves[wave])
            if (group.kind != EnemyKind::Boss)
                group.count = static_cast<int>(std::lround(group.count * countScale));
    for (std::size_t kind = 0; kind + 1 < EnemyKindCount; ++kind) {
        catalog.enemies[kind].hp = static_cast<int>(std::lround(catalog.enemies[kind].hp * healthScale));
        catalog.enemies[kind].shield =
            static_cast<int>(std::lround(catalog.enemies[kind].shield * healthScale));
        catalog.enemies[kind].speed *= speedScale;
    }
    const ResourceCost unreachable{0, 0, 0, CatalogLimits::MaxAmount};
    RunBalance &balance = catalog.balance;
    if (disabled("drill"))
        balance.drill = unreachable;
    if (disabled("hire"))
        balance.hireLimit = 0;
    if (disabled("supply"))
        balance.supply = unreachable;
    if (disabled("awaken"))
        balance.awaken = {unreachable, unreachable};
    catalog.mining = miningMode;
    catalog.mineSeed = wallSeed;
    catalog.balance.openWall = openWall;
    RunCreation created = Run::create(std::move(catalog));
    if (!created.run) {
        std::cerr << created.validation.summary();
        std::exit(2);
    }
    return std::move(*created.run);
}
/** Resource totals in recipe order: iron, copper, crystal, gold. */
using Resources = std::array<int, 4>;
Resources wallet(const RunState &state) {
    return {state.iron, state.copper, state.crystal, state.gold};
}
/** Observed facts of one wave, collected only from public state and committed combat events. */
struct WaveRow {
    int wave = 0;
    int hp = 0;
    /** Most enemies alive at once. */
    int peak = 0;
    /** Furthest route fraction a boss reached, or -1 without a boss. */
    float boss = -1;
    int leaks = 0;
    int leakDamage = 0;
    /** Leaked enemies per EnemyKind and per entrance. */
    std::array<int, EnemyKindCount> leakedKinds{};
    std::array<int, 6> leakedLanes{};
    int spawned = 0;
    int minions = 0;
    float time = 0;
    float wasted = 0;
    int energyLeft = 0;
    int towers = 0;
    int units = 0;
    int upgrades = 0;
    std::array<int, CardKindCount> plays{};
    /** Tower-seconds spent sabotaged, sapper charges that went off, and leaked elites. */
    float disabled = 0;
    int sabotages = 0;
    int eliteLeaks = 0;
    /** Furthest route fraction any non-boss enemy reached. */
    float deepest = 0;
};
/** Observed facts of one stage attempt by one bot profile. */
struct StageResult {
    std::string bot;
    unsigned seed = 0;
    int stage = 0;
    bool won = false;
    int waves = 0;
    int hp = 0;
    float time = 0;
    Resources earned{};
    Resources spent{};
    int crafts = 0;
    int recruits = 0;
    int exchanges = 0;
    std::array<int, CardKindCount> plays{};
    std::vector<WaveRow> rows;
    /** Resources paid per preparation sink. */
    autoplay::Ledger ledger;
    /** Boulders cleared off blocked pads. */
    int boulders = 0;
    /** Carts sent and gas tanks detonated over the attempt. */
    int carts = 0;
    int tanks = 0;
};

/** One player model; it may only call public Run commands. */
class Bot {
  public:
    virtual ~Bot() = default;
    virtual std::string name() const = 0;
    /** Called every 0.1 s of battle; the profile decides whether it reacts this tick. */
    virtual void battle(Run &run, int step) = 0;
    /** Called once per preparation, before the next wave is started. */
    virtual void prepare(Run &run) = 0;
    int recruits = 0;
    int exchanges = 0;
    autoplay::Ledger ledger;
};

/** The shared reference autoplayer: acts every 0.5 s with exact coverage scoring. */
class StrongBot : public Bot {
  public:
    std::string name() const override {
        return "strong";
    }
    void battle(Run &run, int step) override {
        if (step % 5 == 0)
            autoplay::playCard(run);
    }
    void prepare(Run &run) override {
        autoplay::prepare(run, &ledger);
    }
};

/** Count route samples inside a radius, weighting each open lane by its share of scheduled pressure and
 *  discounting samples that existing buildings already cover; samples inside a tunnel do not count. */
float laneReach(const Run &run, Point position, float range) {
    const std::vector<LaneForecast> forecast = run.laneForecast();
    int busiest = 1;
    for (const LaneForecast &lane : forecast)
        busiest = std::max(busiest, lane.count);
    /** Lanes whose enemies are visibly close to the base draw attention. */
    std::vector<bool> threatened(forecast.size(), false);
    for (const Enemy &enemy : run.state().enemies)
        if (enemy.progress > run.routeLength(enemy.route) * .7F)
            threatened.at(run.laneOf(enemy.route)) = true;
    float score = 0;
    for (const LaneForecast &lane : forecast) {
        const float weight = (lane.count ? std::max(.6F, static_cast<float>(lane.count) / busiest) : .15F) *
                             (threatened.at(lane.route) ? 3.0F : 1.0F);
        const float length = run.routeLength(lane.route);
        for (float progress = .5F; progress < length; progress += 1) {
            const Point point = run.routePoint(progress, lane.route);
            if (distance(point, position) > range || run.tunnelAt(progress, lane.route))
                continue;
            int defended = 0;
            for (const Tower &tower : run.state().towers)
                defended +=
                    distance(point, tower.position) <= run.buildingRange(tower.kind, tower.position) ? 1 : 0;
            score += weight / (1 + defended);
        }
    }
    return score;
}
const Tower *towerAt(const Run &run, Point pad) {
    for (const Tower &tower : run.state().towers)
        if (distance(tower.position, pad) < .15F)
            return &tower;
    return nullptr;
}
/** Free pads whose reach is at least half of the best free pad for this card; high ground widens the reach
 *  and crystal ground scales it by its damage bonus. */
std::vector<Point> sensiblePads(const Run &run, CardKind kind) {
    const CardDefinition &definition = run.catalog().card(kind);
    std::vector<std::pair<float, Point>> scored;
    float best = 0;
    for (Point pad : run.state().pads) {
        if (towerAt(run, pad))
            continue;
        float reach = 0;
        if (definition.aura > 0) {
            for (const Tower &tower : run.state().towers)
                if (run.catalog().card(tower.kind).aura <= 0 &&
                    distance(tower.position, pad) <= run.buildingRange(kind, pad))
                    reach += 1;
        } else
            reach = laneReach(run, pad, run.buildingRange(kind, pad)) * run.padDamage(pad);
        if (reach <= 0)
            continue;
        best = std::max(best, reach);
        scored.push_back({reach, pad});
    }
    std::vector<Point> result;
    for (const auto &entry : scored)
        if (entry.first >= best * .5F)
            result.push_back(entry.second);
    return result;
}

/** A reasonable first-time player: slow, roughly sensible, no card-cycling or mining optimisation. */
class AverageBot : public Bot {
  public:
    explicit AverageBot(unsigned seed) : random_(seed) {}
    std::string name() const override {
        return "average";
    }
    void battle(Run &run, int) override {
        const float now = run.state().battleTime;
        if (run.state().wave != wave_) {
            wave_ = run.state().wave;
            next_ = uniform(1, 2);
        }
        if (now + .0001F < next_)
            return;
        next_ = now + uniform(1.5F, 2.0F);
        autoplay::operate(run, .5F);
        act(run);
    }
    void prepare(Run &run) override {
        if (run.state().expansionPending)
            run.expand(chance(.5F) ? 1 : 2);
        if (run.state().rewardOffer) {
            const CardOffer offer = *run.state().rewardOffer;
            run.acceptOffer(offer.id, offer.candidates.front(), duplicate(run));
        }
        if (run.descentAvailability().ok && chance(.5F))
            run.descend();
        const bool wall = run.catalog().mining == MiningMode::Excavate;
        std::vector<int> unknown;
        for (int cell = 0; cell < static_cast<int>(run.state().mine.size()); ++cell)
            if ((wall || run.reachable(cell)) && !run.state().mine[cell].info)
                unknown.push_back(cell);
        if (!unknown.empty() && chance(.5F))
            run.scan(pick(unknown));
        // Extra digs are bought before digging, sometimes twice, when the next price is affordable.
        if (chance(.5F) && run.quote(UpgradeKind::Hire).allowed()) {
            autoplay::buy(run, &ledger, autoplay::Sink::Hire, UpgradeKind::Hire);
            if (chance(.4F))
                autoplay::buy(run, &ledger, autoplay::Sink::Hire, UpgradeKind::Hire);
        }
        // On a wall the average player follows what shows through the rock six times in ten, blasts a quarter
        // of the time away from seen nests, and otherwise picks at any covered tile.
        for (int attempt = 0; wall && attempt < 40 && run.state().digs > 0; ++attempt) {
            int chosen = -1;
            StrikeTool tool = StrikeTool::Pick;
            if (chance(.6F)) {
                float best = 1;
                for (int cell = 0; cell < static_cast<int>(run.state().mine.size()); ++cell) {
                    const float score =
                        run.reachable(cell) ? autoplay::strikeScore(run, cell, StrikeTool::Pick) : 0;
                    if (score > best) {
                        best = score;
                        chosen = cell;
                    }
                }
            }
            if (chosen < 0) {
                std::vector<int> covered;
                for (int cell = 0; cell < static_cast<int>(run.state().mine.size()); ++cell) {
                    const MineCell &tile = run.state().mine[cell];
                    if (tile.cover > 0 && tile.grade <= run.state().tool && !(tile.info && tile.nest) &&
                        run.reachable(cell))
                        covered.push_back(cell);
                }
                if (covered.empty())
                    break;
                chosen = pick(covered);
                if (run.state().digs >= run.strikeCost(StrikeTool::Blast) && chance(.25F) &&
                    autoplay::strikeScore(run, chosen, StrikeTool::Blast) >= 0)
                    tool = StrikeTool::Blast;
            }
            if (!run.strike(chosen, tool).ok)
                break;
            if (run.state().rewardOffer) {
                const CardOffer offer = *run.state().rewardOffer;
                run.acceptOffer(offer.id, offer.candidates.front(), duplicate(run));
            }
        }
        while (!wall && run.state().digs > 0) {
            std::vector<int> candidates, revealed;
            for (int cell = 0; cell < static_cast<int>(run.state().mine.size()); ++cell) {
                const MineCell &tile = run.state().mine[cell];
                if (!run.reachable(cell) || tile.grade > run.state().tool || (tile.info && tile.nest))
                    continue;
                candidates.push_back(cell);
                if (tile.info && tile.ore != Ore::Rock)
                    revealed.push_back(cell);
            }
            if (candidates.empty())
                break;
            const int chosen = !revealed.empty() && chance(.6F) ? pick(revealed) : pick(candidates);
            if (!run.dig(chosen).ok)
                break;
            for (int step = 0; run.state().job.active && step < 200; ++step)
                run.advance(.1F);
            if (run.state().rewardOffer) {
                const CardOffer offer = *run.state().rewardOffer;
                run.acceptOffer(offer.id, offer.candidates.front(), duplicate(run));
            }
        }
        std::vector<std::pair<UpgradeKind, autoplay::Sink>> crafts{
            {UpgradeKind::Tool, autoplay::Sink::Tool},
            {UpgradeKind::Armor, autoplay::Sink::Armor},
            {UpgradeKind::Coil, autoplay::Sink::Coil},
            {UpgradeKind::Slots, autoplay::Sink::Slots},
            {UpgradeKind::Drill, autoplay::Sink::Drill}};
        shuffle(crafts);
        for (const auto &craft : crafts)
            autoplay::buy(run, &ledger, craft.second, craft.first);
        // A boulder is cleared in about half of the preparations that can afford one, without weighing its
        // pad.
        if (!run.state().boulders.empty() && run.boulderQuote(run.state().boulders.front()).allowed() &&
            chance(.5F))
            run.clearBoulder(pick(run.state().boulders));
        // Awakening is noticed for most eligible buildings, in no particular order.
        std::vector<EntityId> towers;
        for (const Tower &tower : run.state().towers)
            towers.push_back(tower.id);
        shuffle(towers);
        for (EntityId id : towers)
            if (run.awakenQuote(id).allowed() && chance(.7F))
                autoplay::charge(run, &ledger, autoplay::Sink::Awaken,
                                 [&run, id]() -> Result { return run.awaken(id); });
        for (const Unit &unit : std::vector<Unit>(run.state().units))
            if (unit.hp < unit.maxHp)
                autoplay::charge(run, &ledger, autoplay::Sink::Heal,
                                 [&run, &unit]() -> Result { return run.heal(unit.id); });
        if (chance(.6F))
            autoplay::buy(run, &ledger, autoplay::Sink::Supply, UpgradeKind::Supply);
        if (run.state().recruitmentOffer && chance(.5F)) {
            const CardOffer offer = *run.state().recruitmentOffer;
            const CardKind candidate = offer.candidates[static_cast<std::size_t>(random_() % 3)];
            const CardId replaced = duplicate(run);
            if (autoplay::charge(run, &ledger, autoplay::Sink::Recruit,
                                 [&run, &offer, candidate, replaced]() -> Result {
                                     return run.acceptOffer(offer.id, candidate, replaced);
                                 })
                    .ok)
                ++recruits;
        }
    }

  private:
    std::mt19937 random_;
    int wave_ = -1;
    float next_ = 0;
    /** Standard distributions and std::shuffle differ between standard libraries, so the bot draws from the
     *  engine's raw output, which is the same everywhere. */
    float uniform(float low, float high) {
        return low + (high - low) * static_cast<float>(random_() >> 8) / 16777216.0F;
    }
    template <typename T> void shuffle(std::vector<T> &values) {
        for (std::size_t index = values.size(); index > 1; --index)
            std::swap(values[index - 1], values[static_cast<std::size_t>(random_() % index)]);
    }
    bool chance(float probability) {
        return uniform(0, 1) < probability;
    }
    template <typename T> T pick(const std::vector<T> &values) {
        return values[static_cast<std::size_t>(random_() % values.size())];
    }
    /** Replace a random card whose kind appears more than once, or any random card otherwise. */
    CardId duplicate(Run &run) {
        std::vector<CardId> repeated, all;
        for (const Card &card : run.state().deck) {
            all.push_back(card.id);
            int same = 0;
            for (const Card &other : run.state().deck)
                same += other.kind == card.kind ? 1 : 0;
            if (same > 1)
                repeated.push_back(card.id);
        }
        return pick(repeated.empty() ? all : repeated);
    }
    /** Pick one living enemy near which at least three others stand, with aiming error. */
    bool crowd(const Run &run, float radius, Point &target, bool ahead) {
        std::vector<const Enemy *> crowded;
        for (const Enemy &enemy : run.state().enemies) {
            if (run.burrowed(enemy))
                continue;
            int near = 0;
            for (const Enemy &other : run.state().enemies)
                near += !run.burrowed(other) && distance(enemy.position, other.position) <= radius ? 1 : 0;
            if (near >= 4)
                crowded.push_back(&enemy);
        }
        if (crowded.empty())
            return false;
        const Enemy *chosen = pick(crowded);
        target = ahead ? run.routePoint(chosen->progress + 1.2F, chosen->route) : chosen->position;
        target.x += uniform(-.4F, .4F);
        target.y += uniform(-.4F, .4F);
        return true;
    }
    void act(Run &run) {
        struct Option {
            CardId card;
            Point target;
            bool building;
        };
        std::vector<Option> options;
        bool waitingForBuilding = false;
        for (int slot : run.state().hand) {
            const Card &card = run.state().deck[slot];
            const CardDefinition &definition = run.catalog().card(card.kind);
            Point target{};
            bool found = false;
            if (run.catalog().isBuilding(card.kind)) {
                const std::vector<Point> pads = sensiblePads(run, card.kind);
                std::vector<Point> upgrades;
                for (const Tower &tower : run.state().towers)
                    if (tower.kind == card.kind && tower.level < 3)
                        upgrades.push_back(tower.position);
                // With crystal for an awakening in hand, finishing a building to level 3 becomes more
                // attractive.
                const bool saving = run.state().crystal >= run.catalog().balance.awaken.front().crystal;
                if (!pads.empty() && (upgrades.empty() || chance(saving ? .4F : .65F))) {
                    target = pick(pads);
                    found = true;
                } else if (!upgrades.empty()) {
                    target = pick(upgrades);
                    found = true;
                }
            } else if (run.catalog().isUnit(card.kind)) {
                std::vector<Point> cells;
                for (Point cell : run.state().unitCells)
                    if (run.previewPlay(card.id, cell).allowed() && laneReach(run, cell, .5F) >= 1)
                        cells.push_back(cell);
                if (!cells.empty()) {
                    target = pick(cells);
                    found = true;
                }
            } else
                found = crowd(run, std::max(1.2F, definition.range), target, card.kind == CardKind::Tar);
            if (!found)
                continue;
            const PlayPreview preview = run.previewPlay(card.id, target);
            if (preview.allowed())
                options.push_back({card.id, target, run.catalog().isBuilding(card.kind)});
            else if (preview.failure == PlayFailure::EnergyShortage && run.catalog().isBuilding(card.kind))
                waitingForBuilding = true;
        }
        if (!options.empty()) {
            std::vector<Option> buildings;
            for (const Option &option : options)
                if (option.building)
                    buildings.push_back(option);
            const Option chosen = !buildings.empty() && chance(.6F) ? pick(buildings) : pick(options);
            run.play(chosen.card, chosen.target);
            return;
        }
        if (!waitingForBuilding && run.state().energy >= run.catalog().balance.energyCap) {
            run.exchange(run.state().deck[run.state().hand[random_() % 4]].id);
            ++exchanges;
        }
    }
};

/** A near-idle player: a handful of towers, never upgrades, never mines or crafts. */
class IdleBot : public Bot {
  public:
    std::string name() const override {
        return "idle";
    }
    void battle(Run &run, int step) override {
        if (step % 20 != 0 || static_cast<int>(run.state().towers.size()) >= Towers)
            return;
        for (int slot : run.state().hand) {
            const Card &card = run.state().deck[slot];
            if (!run.catalog().isBuilding(card.kind) || run.catalog().card(card.kind).aura > 0)
                continue;
            const std::vector<Point> pads = sensiblePads(run, card.kind);
            if (pads.empty())
                continue;
            if (run.previewPlay(card.id, pads.front()).allowed())
                run.play(card.id, pads.front());
            return;
        }
        if (run.state().energy >= run.catalog().balance.energyCap) {
            run.exchange(run.state().deck[run.state().hand.front()].id);
            ++exchanges;
        }
    }
    void prepare(Run &run) override {
        if (run.state().expansionPending)
            run.expand(1);
        if (run.state().rewardOffer)
            run.skipOffer(run.state().rewardOffer->id);
    }
    static constexpr int Towers = 4;
};

/** Earned resources of newly dug tiles in the active cutaway since a snapshot of its dug flags; on an
 * excavation wall, of deposits collected since a snapshot of their collected flags. */
Resources dugSince(const Run &run, const std::vector<bool> &before, int layerBefore) {
    Resources result{};
    const RunState &state = run.state();
    if (run.catalog().mining == MiningMode::Excavate) {
        for (std::size_t index = 0; index < state.deposits.size(); ++index) {
            const Deposit &deposit = state.deposits[index];
            if (!deposit.collected || deposit.special != MineSpecial::None ||
                (state.mineLayer == layerBefore && index < before.size() && before[index]))
                continue;
            if (deposit.ore == Ore::Iron)
                result[0] += deposit.amount;
            if (deposit.ore == Ore::Copper)
                result[1] += deposit.amount;
            if (deposit.ore == Ore::Crystal)
                result[2] += deposit.amount;
            if (deposit.ore == Ore::Gold)
                result[3] += deposit.amount;
        }
        return result;
    }
    std::vector<bool> baseline = before;
    if (state.mineLayer != layerBefore) {
        const MineLayerDefinition &layer =
            run.catalog().deeperMines[static_cast<std::size_t>(state.mineLayer - 1)];
        baseline.assign(layer.cells.size(), false);
        for (std::size_t index = 0; index < layer.cells.size(); ++index)
            baseline[index] = layer.cells[index].opened;
    }
    for (std::size_t index = 0; index < state.mine.size(); ++index) {
        const MineCell &cell = state.mine[index];
        if (!cell.dug || baseline[index])
            continue;
        if (cell.ore == Ore::Iron)
            result[0] += cell.amount;
        if (cell.ore == Ore::Copper)
            result[1] += cell.amount;
        if (cell.ore == Ore::Crystal)
            result[2] += cell.amount;
        if (cell.ore == Ore::Gold)
            result[3] += cell.amount;
    }
    return result;
}

const char *shortName(CardKind kind);
/** Count permanent crafted upgrades: pickaxe tier, armor, coil and slot expansion. */
int craftCount(const Run &run) {
    const RunState &state = run.state();
    return state.tool - 1 + (state.armor ? 1 : 0) + (state.coil ? 1 : 0) +
           (state.slots > run.catalog().balance.initialSlots ? 1 : 0);
}
StageResult simulate(Bot &bot, int stage, unsigned seed) {
    wallSeed = seed;
    Run run = stageRun(stage);
    StageResult result;
    result.bot = bot.name();
    result.seed = seed;
    result.stage = stage;
    const RunBalance &balance = run.catalog().balance;
    WaveRow row;
    int defeatedBefore = 0;
    int steps = 0;
    while (run.state().phase != Phase::Won && run.state().phase != Phase::Lost && steps < 60000) {
        if (run.state().phase == Phase::Ready || run.state().phase == Phase::Prep) {
            if (run.state().phase == Phase::Prep) {
                std::vector<bool> dug;
                if (run.catalog().mining == MiningMode::Excavate)
                    for (const Deposit &deposit : run.state().deposits)
                        dug.push_back(deposit.collected);
                else
                    for (const MineCell &cell : run.state().mine)
                        dug.push_back(cell.dug);
                const int layer = run.state().mineLayer;
                const Resources before = wallet(run.state());
                const int crafted = craftCount(run);
                bot.prepare(run);
                const Resources earned = dugSince(run, dug, layer);
                const Resources after = wallet(run.state());
                for (std::size_t index = 0; index < 4; ++index) {
                    result.earned[index] += earned[index];
                    result.spent[index] += earned[index] - (after[index] - before[index]);
                }
                result.crafts += craftCount(run) - crafted;
            }
            if (!run.startWave().ok) {
                std::cerr << bot.name() << " blocked on stage " << stage << '\n';
                std::exit(2);
            }
            row = WaveRow{};
            row.wave = run.state().wave;
            row.spawned = run.waveSpawnCount();
            defeatedBefore = run.state().defeated;
        }
        bot.battle(run, steps);
        if (run.state().phase == Phase::Battle && run.state().energy >= balance.energyCap)
            row.wasted += .1F / balance.energyInterval;
        row.peak = std::max(row.peak, static_cast<int>(run.state().enemies.size()));
        for (const Enemy &enemy : run.state().enemies)
            if (enemy.kind == EnemyKind::Boss)
                row.boss = std::max(row.boss, enemy.progress / run.routeLength(enemy.route));
            else
                row.deepest = std::max(row.deepest, enemy.progress / run.routeLength(enemy.route));
        for (const Tower &tower : run.state().towers)
            row.disabled += tower.disabled > 0 ? .1F : 0;
        std::vector<std::pair<EntityId, std::uint32_t>> lanes;
        for (const Enemy &enemy : run.state().enemies)
            lanes.push_back({enemy.id, run.laneOf(enemy.route)});
        run.advance(.1F);
        ++steps;
        for (const CombatEvent &event : run.takeCombatEvents()) {
            if (event.type == CombatEventKind::Leak) {
                ++row.leaks;
                row.leakDamage += event.amount;
                ++row.leakedKinds[static_cast<std::size_t>(event.enemyKind)];
                row.eliteLeaks += event.affix != EliteAffix::None ? 1 : 0;
                for (const auto &lane : lanes)
                    if (lane.first == event.subject)
                        ++row.leakedLanes[std::min<std::size_t>(lane.second, 5)];
            } else if (event.type == CombatEventKind::Disable) {
                ++row.sabotages;
            } else if (event.type == CombatEventKind::Cart) {
                ++result.carts;
            } else if (event.type == CombatEventKind::Detonation) {
                ++result.tanks;
            } else if (event.type == CombatEventKind::Placement) {
                ++row.plays[static_cast<std::size_t>(event.card)];
                row.upgrades += event.amount > 1 ? 1 : 0;
            } else if (event.type == CombatEventKind::Attack && event.subject == 0 && !event.enemy)
                ++row.plays[static_cast<std::size_t>(event.card)];
        }
        if (run.state().phase != Phase::Battle) {
            row.hp = run.state().baseHp;
            row.time = run.state().battleTime;
            row.energyLeft = run.state().energy;
            row.towers = static_cast<int>(run.state().towers.size());
            row.units = static_cast<int>(run.state().units.size());
            const int killed = run.state().defeated - defeatedBefore;
            row.minions = std::max(0, killed + row.leaks - row.spawned);
            result.time += row.time;
            for (std::size_t kind = 0; kind < row.plays.size(); ++kind)
                result.plays[kind] += row.plays[kind];
            result.rows.push_back(row);
            if (towerDump) {
                std::printf("%s s%d #%u W%d towers:", bot.name().c_str(), stage, seed, row.wave);
                for (const Tower &tower : run.state().towers)
                    std::printf(" %s%d(%.0f,%.0f)", shortName(tower.kind), tower.level, tower.position.x,
                                tower.position.y);
                std::printf("\n  lane dps-tiles:");
                for (std::uint32_t lane = 0; lane <= run.catalog().additionalRoutes.size(); ++lane) {
                    float covered = 0;
                    for (float progress = .5F; progress < run.routeLength(lane); progress += 1)
                        for (const Tower &tower : run.state().towers) {
                            const CardDefinition &card = run.catalog().card(tower.kind);
                            if (!run.tunnelAt(progress, lane) &&
                                distance(run.routePoint(progress, lane), tower.position) <=
                                    run.buildingRange(tower.kind, tower.position))
                                covered += card.damage / card.interval;
                        }
                    std::printf(" %c=%.0f(len %.0f)", 'A' + lane, covered, run.routeLength(lane));
                }
                std::printf("\n");
            }
        }
    }
    result.won = run.state().phase == Phase::Won;
    result.waves = run.state().wave;
    result.hp = run.state().baseHp;
    result.recruits = bot.recruits;
    result.exchanges = bot.exchanges;
    result.ledger = bot.ledger;
    result.boulders = static_cast<int>(run.catalog().boulders.size() - run.state().boulders.size());
    return result;
}

const char *shortName(CardKind kind) {
    static const std::array<const char *, CardKindCount> names{{"gun", "cannon", "guard", "frost", "blast",
                                                                "light", "shoot", "demo", "flame", "snipe",
                                                                "gen", "tar"}};
    return names[static_cast<std::size_t>(kind)];
}
std::string playsLabel(const std::array<int, CardKindCount> &plays) {
    std::string text;
    for (int kind = 0; kind < CardKindCount; ++kind)
        if (plays[static_cast<std::size_t>(kind)]) {
            if (!text.empty())
                text += ' ';
            text += std::string(shortName(static_cast<CardKind>(kind))) +
                    std::to_string(plays[static_cast<std::size_t>(kind)]);
        }
    return text.empty() ? "-" : text;
}
std::string resourcesLabel(const Resources &value) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%d/%d/%d/%d", value[0], value[1], value[2], value[3]);
    return buffer;
}
std::string leakLabel(const WaveRow &row) {
    static const std::array<const char *, EnemyKindCount> names{
        {"norm", "run", "arm", "swarm", "shield", "heal", "split", "burrow", "sapper", "bat", "stalk", "leap",
         "wreck", "sling", "carry", "boss"}};
    std::string text;
    for (std::size_t kind = 0; kind < EnemyKindCount; ++kind)
        if (row.leakedKinds[kind])
            text +=
                std::string(text.empty() ? "" : " ") + names[kind] + std::to_string(row.leakedKinds[kind]);
    if (text.empty())
        return "-";
    text += " /";
    for (std::size_t lane = 0; lane < row.leakedLanes.size(); ++lane)
        if (row.leakedLanes[lane])
            text += std::string(" ") + static_cast<char>('A' + lane) + std::to_string(row.leakedLanes[lane]);
    return text;
}
void printWaves(const StageResult &result) {
    std::printf("\n### %s seed=%u stage=%d -> %s (wave %d, hp %d)\n", result.bot.c_str(), result.seed,
                result.stage, result.won ? "WON" : "LOST", result.waves, result.hp);
    std::printf(
        "| wave | hp | leaks(dmg) | spawned+minions | time | energy wasted/left | peak | "
        "towers/units | upg | sabotage(s) | elite leaks | deepest | "
        "plays | leaked kinds / lanes |\n|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    for (const WaveRow &row : result.rows)
        std::printf("| %d | %d | %d(%d) | %d+%d | %.1f | %.1f/%d | %d | %d/%d | %d | %d(%.0f) | %d | %.0f%% "
                    "| %s | %s "
                    "|\n",
                    row.wave, row.hp, row.leaks, row.leakDamage, row.spawned, row.minions, row.time,
                    row.wasted, row.energyLeft, row.peak, row.towers, row.units, row.upgrades, row.sabotages,
                    row.disabled, row.eliteLeaks, row.deepest * 100, playsLabel(row.plays).c_str(),
                    leakLabel(row).c_str());
    for (const WaveRow &row : result.rows)
        if (row.boss >= 0)
            std::printf("boss reached %.0f%% of its route in wave %d\n", row.boss * 100, row.wave);
    std::printf("mine earned i/c/x/g %s, spent %s, crafts %d, recruits %d, exchanges %d\n",
                resourcesLabel(result.earned).c_str(), resourcesLabel(result.spent).c_str(), result.crafts,
                result.recruits, result.exchanges);
}
void printSummary(const std::vector<StageResult> &results) {
    std::printf(
        "\n| bot | seed | stage | result | hp | leaks | min hp wave | time | earned i/c/x/g | spent | "
        "plays |\n|---|---|---|---|---|---|---|---|---|---|---|\n");
    for (const StageResult &result : results) {
        int leaks = 0, worst = 0, worstDrop = 0, previous = 20;
        for (const WaveRow &row : result.rows) {
            leaks += row.leaks;
            if (previous - row.hp > worstDrop) {
                worstDrop = previous - row.hp;
                worst = row.wave;
            }
            previous = row.hp;
        }
        char outcome[32];
        std::snprintf(outcome, sizeof outcome, result.won ? "WON" : "LOST@%d", result.waves);
        std::printf("| %s | %u | %d | %s | %d | %d | W%d(-%d) | %.0f | %s | %s | %s |\n", result.bot.c_str(),
                    result.seed, result.stage, outcome, result.hp, leaks, worst, worstDrop, result.time,
                    resourcesLabel(result.earned).c_str(), resourcesLabel(result.spent).c_str(),
                    playsLabel(result.plays).c_str());
    }
}
/** Per-wave means of one bot on one stage over the attempts that reached each wave. */
struct WaveCurve {
    std::vector<float> loss, leaks, deepest, disabled;
    /** Mean per-wave health loss of the first, middle and last third of the stage's waves. */
    std::array<float, 3> thirds{};
    bool reached = false;
};
/** Average one bot's base-health loss, leaks, deepest progress and sabotage per wave of one stage. */
WaveCurve waveCurve(const std::vector<StageResult> &results, int stage, const std::string &bot) {
    const std::size_t waves = stageCatalog(stage).waves.size();
    WaveCurve curve;
    std::vector<float> reached(waves, 0);
    curve.loss = curve.leaks = curve.deepest = curve.disabled = reached;
    for (const StageResult &result : results) {
        if (result.stage != stage || result.bot != bot)
            continue;
        int previous = 20;
        for (const WaveRow &row : result.rows) {
            const std::size_t index = static_cast<std::size_t>(row.wave - 1);
            curve.loss[index] += static_cast<float>(previous - row.hp);
            curve.leaks[index] += static_cast<float>(row.leaks);
            curve.deepest[index] += row.deepest;
            curve.disabled[index] += row.disabled;
            reached[index] += 1;
            previous = row.hp;
        }
    }
    curve.reached = reached[0] > 0;
    std::array<float, 3> counts{};
    for (std::size_t index = 0; index < waves; ++index) {
        const float runs = std::max(1.0F, reached[index]);
        for (std::vector<float> *values : {&curve.loss, &curve.leaks, &curve.deepest, &curve.disabled})
            (*values)[index] /= runs;
        const std::size_t third = std::min<std::size_t>(2, index * 3 / waves);
        curve.thirds[third] += curve.loss[index];
        counts[third] += 1;
    }
    for (std::size_t third = 0; third < 3; ++third)
        curve.thirds[third] /= std::max(1.0F, counts[third]);
    return curve;
}
/** Print, per stage and bot, each wave's mean base-health loss and leaks over the attempts that reached it,
 *  then the mean loss of the first, middle and last thirds of the stage. */
void printCurve(const std::vector<StageResult> &results) {
    std::printf("\nper-wave mean hp loss (leaks) over attempts reaching the wave, deepest non-boss route "
                "fraction and sabotaged tower-seconds\n");
    for (int stage = 1; stage <= TotalStages; ++stage)
        for (const char *bot : {"strong", "average"}) {
            const WaveCurve curve = waveCurve(results, stage, bot);
            if (!curve.reached)
                continue;
            std::printf("s%d %-7s", stage, bot);
            for (std::size_t wave = 0; wave < curve.loss.size(); ++wave)
                std::printf(" %4.1f(%.1f)", curve.loss[wave], curve.leaks[wave]);
            std::printf("  | thirds %.2f %.2f %.2f\n", curve.thirds[0], curve.thirds[1], curve.thirds[2]);
            std::printf("   deepest%%");
            for (float deepest : curve.deepest)
                std::printf(" %3.0f", deepest * 100);
            std::printf("\n   sabotaged tower-s");
            for (float disabled : curve.disabled)
                std::printf(" %3.0f", disabled);
            std::printf("\n");
        }
}
/**
 * Per bot and stage: wins, mean earned and spent resources, the unspent share of each, per sink the mean
 * purchases and resources paid per attempt, the mean boulders cleared, carts sent and gas tanks detonated.
 */
void printSinks(const std::vector<StageResult> &results) {
    std::printf("\n| bot | stage | wins | earned i/c/x/g | spent | unspent %% | hp |");
    for (std::size_t sink = 0; sink < autoplay::SinkCount; ++sink)
        std::printf(" %s |", autoplay::sinkName(static_cast<autoplay::Sink>(sink)));
    std::printf(" boulder | cart | tank |\n|---|---|---|---|---|---|---|");
    for (std::size_t sink = 0; sink < autoplay::SinkCount; ++sink)
        std::printf("---|");
    std::printf("---|---|---|\n");
    std::vector<std::pair<std::string, int>> keys;
    for (const StageResult &result : results)
        if (std::find(keys.begin(), keys.end(), std::make_pair(result.bot, result.stage)) == keys.end())
            keys.push_back({result.bot, result.stage});
    for (const auto &key : keys) {
        int runs = 0, wins = 0, hp = 0, boulders = 0, carts = 0, tanks = 0;
        std::array<float, 4> earned{}, spent{};
        std::array<float, autoplay::SinkCount> uses{};
        std::array<std::array<float, 4>, autoplay::SinkCount> paid{};
        for (const StageResult &result : results) {
            if (result.bot != key.first || result.stage != key.second)
                continue;
            ++runs;
            wins += result.won ? 1 : 0;
            hp += result.hp;
            boulders += result.boulders;
            carts += result.carts;
            tanks += result.tanks;
            for (std::size_t index = 0; index < 4; ++index) {
                earned[index] += static_cast<float>(result.earned[index]);
                spent[index] += static_cast<float>(result.spent[index]);
            }
            for (std::size_t sink = 0; sink < autoplay::SinkCount; ++sink) {
                uses[sink] += static_cast<float>(result.ledger.uses[sink]);
                for (std::size_t index = 0; index < 4; ++index)
                    paid[sink][index] += static_cast<float>(result.ledger.spent[sink][index]);
            }
        }
        const float n = static_cast<float>(runs);
        std::string unspent;
        for (std::size_t index = 0; index < 4; ++index) {
            char cell[16];
            if (earned[index] > 0)
                std::snprintf(cell, sizeof cell, "%s%.0f", index ? "/" : "",
                              100 * (earned[index] - spent[index]) / earned[index]);
            else
                std::snprintf(cell, sizeof cell, "%s-", index ? "/" : "");
            unspent += cell;
        }
        std::printf("| %s | %d | %d/%d | %.0f/%.0f/%.0f/%.0f | %.0f/%.0f/%.0f/%.0f | %s | %.1f |",
                    key.first.c_str(), key.second, wins, runs, earned[0] / n, earned[1] / n, earned[2] / n,
                    earned[3] / n, spent[0] / n, spent[1] / n, spent[2] / n, spent[3] / n, unspent.c_str(),
                    static_cast<float>(hp) / n);
        for (std::size_t sink = 0; sink < autoplay::SinkCount; ++sink) {
            if (uses[sink] <= 0) {
                std::printf(" - |");
                continue;
            }
            std::string price;
            for (std::size_t index = 0; index < 4; ++index)
                if (paid[sink][index] > 0) {
                    char cell[24];
                    std::snprintf(cell, sizeof cell, "%s%c%.1f", price.empty() ? "" : " ", "icxg"[index],
                                  paid[sink][index] / n);
                    price += cell;
                }
            std::printf(" %.1f× %s |", uses[sink] / n, price.c_str());
        }
        for (int count : {boulders, carts, tanks})
            if (count)
                std::printf(" %.1f× |", static_cast<float>(count) / n);
            else
                std::printf(" - |");
        std::printf("\n");
    }
}
/** Print each wave's scheduled enemy count and hit points, counting shields and split minions. */
void printSchedule(int stage) {
    const Catalog &catalog = stageCatalog(stage);
    std::printf("\nstage %d schedule (count / total hp incl. shields, splits and elite health / "
                "armored+shielded / elites / sappers):\n",
                stage);
    for (int wave = 1; wave <= static_cast<int>(catalog.waves.size()); ++wave) {
        int count = 0, hp = 0, tough = 0, elites = 0, sappers = 0;
        std::array<int, 6> lanes{};
        for (const Spawn &spawn : catalog.schedule(wave)) {
            ++lanes[std::min<std::size_t>(spawn.route, 5)];
            const EnemyDefinition &enemy = catalog.enemy(spawn.kind);
            ++count;
            Enemy live{};
            live.kind = spawn.kind;
            live.affix = spawn.affix;
            const float vigor = spawn.kind == EnemyKind::Boss ? 1.0F : catalog.waveVigor(wave);
            hp += static_cast<int>(std::lround((catalog.health(live) + enemy.shield +
                                                enemy.splitCount * catalog.enemy(enemy.minionKind).hp) *
                                               vigor));
            tough += spawn.kind == EnemyKind::Armored || spawn.kind == EnemyKind::Shielded ? 1 : 0;
            elites += spawn.affix != EliteAffix::None ? 1 : 0;
            sappers += spawn.kind == EnemyKind::Sapper ? 1 : 0;
        }
        std::printf("  W%-2d %4d %6d %3d %2d %2d  lanes", wave, count, hp, tough, elites, sappers);
        for (std::size_t lane = 0; lane <= catalog.additionalRoutes.size(); ++lane)
            std::printf(" %c%d", static_cast<char>('A' + lane), lanes[lane]);
        std::printf("\n");
    }
}
/**
 * Per-wave headroom: the largest count multiplier for one wave alone that the bot still clears while
 * losing at most a quarter of its starting health in that wave. Higher means easier.
 */
void printCapacity(const std::string &bots, std::pair<int, int> stages, std::pair<int, int> seeds) {
    for (int stage = stages.first; stage <= stages.second; ++stage) {
        const int waves = static_cast<int>(stageCatalog(stage).waves.size());
        for (const std::string name : {"strong", "average"}) {
            if (bots.find(name) == std::string::npos)
                continue;
            std::printf("%-7s s%d:", name.c_str(), stage);
            for (int wave = 1; wave <= waves; ++wave) {
                float total = 0;
                int samples = 0;
                for (int seed = name == "strong" ? 0 : seeds.first;
                     seed <= (name == "strong" ? 0 : seeds.second); ++seed) {
                    float low = 0, high = 6;
                    for (int step = 0; step < 7; ++step) {
                        const float middle = (low + high) / 2;
                        countScale = middle;
                        countScaleFrom = countScaleTo = wave;
                        std::unique_ptr<Bot> bot;
                        if (name == "strong")
                            bot.reset(new StrongBot);
                        else
                            bot.reset(new AverageBot(static_cast<unsigned>(seed)));
                        const StageResult result = simulate(*bot, stage, static_cast<unsigned>(seed));
                        bool cleared = false;
                        if (static_cast<int>(result.rows.size()) >= wave) {
                            const int before =
                                wave == 1 ? 20 : result.rows[static_cast<std::size_t>(wave - 2)].hp;
                            cleared = result.rows[static_cast<std::size_t>(wave - 1)].hp > 0 &&
                                      before - result.rows[static_cast<std::size_t>(wave - 1)].hp <= 5;
                        } else if (result.won)
                            cleared = true;
                        if (static_cast<int>(result.rows.size()) < wave && !result.won) {
                            low = high = -1;
                            break;
                        }
                        (cleared ? low : high) = middle;
                    }
                    if (low >= 0) {
                        total += low;
                        ++samples;
                    }
                }
                countScale = 1;
                countScaleFrom = 1;
                countScaleTo = 1000;
                if (samples)
                    std::printf(" %4.2f", total / samples);
                else
                    std::printf("   - ");
            }
            std::printf("\n");
        }
    }
}
/** Parse "a-b" or "a" into an inclusive range. */
std::pair<int, int> range(const char *text) {
    int low = 0, high = 0;
    if (std::sscanf(text, "%d-%d", &low, &high) == 2)
        return {low, high};
    low = std::atoi(text);
    return {low, low};
}
} // namespace
/**
 * Headless balance harness. Usage:
 *   thorium-balance-harness [--bots strong,average,idle] [--seeds 1-6] [--stages 1-5] [--waves] [--check]
 *                           [--swap FROM=TO]... [--count-scale F[@WAVE]] [--hp-scale F] [--speed-scale F]
 *                           [--capacity] [--schedule] [--towers] [--curve] [--without
 * drill,hire,supply,awaken]
 * --waves prints a per-wave table for every attempt; --check enforces the target difficulty curve;
 * --schedule prints each wave's scheduled count, health and lanes; --towers prints buildings per wave;
 * --swap replaces a CardKind index in starter decks and expansion rewards (card-value ablation);
 * --count-scale multiplies non-boss spawn counts from WAVE on; --hp-scale and --speed-scale multiply
 * non-boss health and speed;
 * --capacity prints, per wave, the largest count multiplier of that wave alone the bot still holds;
 * --curve prints each wave's mean health loss and leaks per stage and bot, with stage-third means;
 * --without prices the listed new preparation sinks out of reach, for sink ablations;
 * --mining excavate plays every run on generated excavation walls instead of tile-by-tile mines, and
 * --open-wall lets its strikes target any tile.
 */
int main(int argc, char **argv) {
    std::string bots = "strong,average,idle";
    std::pair<int, int> seeds{1, 6}, stages{1, TotalStages};
    bool waves = false, check = false, capacity = false, curve = false;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--bots" && index + 1 < argc)
            bots = argv[++index];
        else if (argument == "--seeds" && index + 1 < argc)
            seeds = range(argv[++index]);
        else if (argument == "--stages" && index + 1 < argc)
            stages = range(argv[++index]);
        else if (argument == "--mining" && index + 1 < argc)
            miningMode = std::string(argv[++index]) == "excavate" ? MiningMode::Excavate : MiningMode::Sweep;
        else if (argument == "--open-wall")
            openWall = true;
        else if (argument == "--without" && index + 1 < argc) {
            std::string list = argv[++index];
            for (std::size_t start = 0; start <= list.size();) {
                const std::size_t end = std::min(list.find(',', start), list.size());
                disabledSinks.push_back(list.substr(start, end - start));
                start = end + 1;
            }
        } else if (argument == "--towers")
            towerDump = true;
        else if (argument == "--waves")
            waves = true;
        else if (argument == "--curve")
            curve = true;
        else if (argument == "--swap" && index + 1 < argc) {
            int from = 0, to = 0;
            if (std::sscanf(argv[++index], "%d=%d", &from, &to) == 2)
                deckSwaps.push_back({static_cast<CardKind>(from), static_cast<CardKind>(to)});
        } else if (argument == "--count-scale" && index + 1 < argc) {
            if (std::sscanf(argv[++index], "%f@%d", &countScale, &countScaleFrom) < 1)
                countScale = 1;
        } else if (argument == "--speed-scale" && index + 1 < argc)
            speedScale = static_cast<float>(std::atof(argv[++index]));
        else if (argument == "--hp-scale" && index + 1 < argc)
            healthScale = static_cast<float>(std::atof(argv[++index]));
        else if (argument == "--capacity") {
            capacity = true;
        } else if (argument == "--schedule") {
            for (int stage = 1; stage <= TotalStages; ++stage)
                printSchedule(stage);
            return 0;
        } else if (argument == "--check")
            check = true;
    }
    if (capacity) {
        printCapacity(bots, stages, seeds);
        return 0;
    }
    std::vector<StageResult> results;
    for (int stage = stages.first; stage <= stages.second; ++stage) {
        if (bots.find("strong") != std::string::npos)
            for (unsigned seed = 0; seed < (check ? 3U : 1U); ++seed) {
                StrongBot bot;
                results.push_back(simulate(bot, stage, seed));
            }
        if (bots.find("average") != std::string::npos)
            for (int seed = seeds.first; seed <= seeds.second; ++seed) {
                AverageBot bot(static_cast<unsigned>(seed));
                results.push_back(simulate(bot, stage, static_cast<unsigned>(seed)));
            }
        if (bots.find("idle") != std::string::npos) {
            IdleBot bot;
            results.push_back(simulate(bot, stage, 0));
        }
    }
    if (waves)
        for (const StageResult &result : results)
            printWaves(result);
    printSummary(results);
    printSinks(results);
    if (curve)
        printCurve(results);
    if (!check)
        return 0;
    int failures = 0;
    const auto fail = [&failures](const std::string &message) {
        std::cerr << "CURVE: " << message << '\n';
        ++failures;
    };
    std::array<int, TotalStages + 1> averageRuns{}, averageWins{};
    std::array<std::vector<int>, TotalStages + 1> strongHealth;
    for (const StageResult &result : results) {
        const std::string where =
            result.bot + " seed " + std::to_string(result.seed) + " stage " + std::to_string(result.stage);
        if (result.bot == "strong")
            strongHealth[static_cast<std::size_t>(result.stage)].push_back(result.won ? result.hp : 0);
        if (result.bot == "idle" && result.stage == 1 && result.won)
            fail(where + " should lose");
        if (result.bot == "average") {
            ++averageRuns[static_cast<std::size_t>(result.stage)];
            averageWins[static_cast<std::size_t>(result.stage)] += result.won ? 1 : 0;
        }
    }
    /** One strong run swings widely with small content changes, so the median of three seeds is judged. */
    for (int stage = 1; stage <= TotalStages; ++stage) {
        std::vector<int> health = strongHealth[static_cast<std::size_t>(stage)];
        if (health.empty())
            continue;
        std::sort(health.begin(), health.end());
        if (health[health.size() / 2] < 14)
            fail("strong median stage " + std::to_string(stage) + " should win comfortably");
    }
    /** Average win-rate band per stage: the 32-seed targets widened for the test's smaller seed range. */
    const std::array<std::pair<float, float>, TotalStages + 1> bands{
        {{0, 1}, {.75F, 1}, {.6F, 1}, {.35F, .9F}, {.25F, .85F}, {.15F, .75F}, {.1F, .7F}, {.1F, .7F}}};
    for (int stage = 1; stage <= TotalStages; ++stage) {
        const std::size_t index = static_cast<std::size_t>(stage);
        if (!averageRuns[index])
            continue;
        const float rate = static_cast<float>(averageWins[index]) / static_cast<float>(averageRuns[index]);
        if (rate < bands[index].first)
            fail("average wins stage " + std::to_string(stage) + " too rarely");
        if (rate > bands[index].second)
            fail("average wins stage " + std::to_string(stage) + " too often");
        /** The last third of a stage costs the average player at least as much health as its first third. */
        const WaveCurve curve = waveCurve(results, stage, "average");
        if (curve.thirds[2] < curve.thirds[0])
            fail("average loses more health early than late in stage " + std::to_string(stage));
    }
    return failures ? 1 : 0;
}
