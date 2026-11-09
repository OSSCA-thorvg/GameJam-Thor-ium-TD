#include "CatalogValidation.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <set>
#include <utility>

namespace thorium::game {
namespace {
/** Accumulate field errors while preserving independent diagnostics in one validation pass. */
class Validator {
  public:
    CatalogValidation result;
    /** Record a failed invariant with the exact authored field path. */
    void check(bool condition, const std::string &path, const std::string &message) {
        if (!condition)
            result.issues.push_back({path, message});
    }
    /** Keep integer amounts within the prototype's arithmetic budget. */
    void integer(int value, int minimum, int maximum, const std::string &path) {
        check(value >= minimum && value <= maximum, path,
              "expected integer in [" + std::to_string(minimum) + ", " + std::to_string(maximum) + "]");
    }
    /** Reject non-finite authored floats before arithmetic or coordinate conversion. */
    void number(float value, float minimum, float maximum, const std::string &path, bool positive = false) {
        check(std::isfinite(value) && value >= minimum && value <= maximum && (!positive || value > 0), path,
              positive ? "expected finite positive value within supported bounds"
                       : "expected finite value within supported bounds");
    }
    /** Validate a cardinal prototype coordinate independently of any renderer pixels. */
    bool point(Point value, const std::string &path) {
        const bool valid = std::isfinite(value.x) && std::isfinite(value.y) && value.x >= 0 &&
                           value.x <= 17 && value.y >= 0 && value.y <= 9 && value.x == std::round(value.x) &&
                           value.y == std::round(value.y);
        check(valid, path, "expected an integer tile inside the 18 by 10 battlefield");
        return valid;
    }
    /** Validate consecutive route segments, including an expansion-to-core join. */
    void route(const std::vector<Point> &points, const std::string &path) {
        check(points.size() >= 2, path, "route needs at least two points");
        for (std::size_t index = 0; index < points.size(); ++index) {
            const bool valid = point(points[index], path + "[" + std::to_string(index) + "]");
            if (index && valid) {
                const Point previous = points[index - 1], current = points[index];
                check((previous.x == current.x) != (previous.y == current.y),
                      path + "[" + std::to_string(index) + "]", "route segment must be nonzero and cardinal");
            }
        }
    }
    /** Validate unique placement cells after combining a possible expansion with the base map. */
    void cells(const std::vector<Point> &points, const std::string &path) {
        std::set<std::pair<float, float>> seen;
        for (std::size_t index = 0; index < points.size(); ++index)
            if (point(points[index], path + "[" + std::to_string(index) + "]"))
                check(seen.emplace(points[index].x, points[index].y).second, path,
                      "duplicate placement tile");
    }
    /** Check all currencies so negative prices cannot grant resources at payment time. */
    void price(const ResourceCost &cost, const std::string &path) {
        integer(cost.iron, 0, CatalogLimits::MaxAmount, path + ".iron");
        integer(cost.copper, 0, CatalogLimits::MaxAmount, path + ".copper");
        integer(cost.crystal, 0, CatalogLimits::MaxAmount, path + ".crystal");
        integer(cost.gold, 0, CatalogLimits::MaxAmount, path + ".gold");
    }
};
/** Check enum bounds before any indexed catalog lookup. */
bool cardKind(CardKind kind, const Catalog &catalog) {
    return static_cast<std::size_t>(kind) < catalog.cards.size();
}
} // namespace
std::string CatalogValidation::summary() const {
    std::string text;
    for (const CatalogIssue &issue : issues)
        text += (text.empty() ? "" : "\n") + issue.path + ": " + issue.message;
    return text;
}
CatalogValidation validateCatalog(const Catalog &catalog) {
    Validator validator;
    for (std::size_t index = 0; index < catalog.cards.size(); ++index) {
        const CardDefinition &card = catalog.cards[index];
        const std::string path = "cards[" + std::to_string(index) + "]";
        validator.check(!card.name.empty(), path + ".name", "card name must not be empty");
        validator.integer(card.cost, 0, CatalogLimits::MaxAmount, path + ".cost");
        validator.integer(static_cast<int>(card.category), 0, 2, path + ".category");
        validator.integer(card.damage, 0, CatalogLimits::MaxDamage, path + ".damage");
        validator.integer(card.hp, card.category == CardCategory::Unit ? 1 : 0, CatalogLimits::MaxAmount,
                          path + ".hp");
        validator.integer(card.blockers, 0, CatalogLimits::MaxSlots, path + ".blockers");
        validator.integer(card.chains, 1, 100, path + ".chains");
        validator.number(card.range, 0, 100, path + ".range");
        validator.number(card.interval, 0, 1000000, path + ".interval", true);
        validator.number(card.splash, 0, 100, path + ".splash");
        validator.number(card.chainRange, 0, 100, path + ".chainRange");
        validator.number(card.slowDuration, 0, 1000000, path + ".slowDuration");
    }
    for (std::size_t index = 0; index < catalog.enemies.size(); ++index) {
        const EnemyDefinition &enemy = catalog.enemies[index];
        const std::string path = "enemies[" + std::to_string(index) + "]";
        validator.integer(enemy.hp, 1, CatalogLimits::MaxAmount, path + ".hp");
        validator.integer(enemy.armor, 0, CatalogLimits::MaxDamage, path + ".armor");
        validator.integer(enemy.attack, 0, CatalogLimits::MaxDamage, path + ".attack");
        validator.integer(enemy.leak, 0, CatalogLimits::MaxDamage, path + ".leak");
        validator.number(enemy.speed, 0, 100, path + ".speed");
        validator.number(enemy.interval, 0, 1000000, path + ".interval", true);
        validator.check(!enemy.animation.empty(), path + ".animation", "animation profile must not be empty");
    }
    for (std::size_t index = 0; index < catalog.starterDeck.size(); ++index)
        validator.check(cardKind(catalog.starterDeck[index], catalog),
                        "starterDeck[" + std::to_string(index) + "]", "unknown card kind");
    std::set<CardKind> candidates;
    for (std::size_t index = 0; index < catalog.recruitmentCandidates.size(); ++index) {
        const CardKind kind = catalog.recruitmentCandidates[index];
        const std::string path = "recruitmentCandidates[" + std::to_string(index) + "]";
        validator.check(catalog.isUnit(kind), path, "candidate must reference a supported unit card");
        validator.check(candidates.insert(kind).second, path, "recruitment candidates must be different");
    }
    validator.route(catalog.route, "route");
    validator.cells(catalog.pads, "pads");
    validator.cells(catalog.unitCells, "unitCells");
    for (std::size_t index = 0; index < catalog.expansions.size(); ++index) {
        const ExpansionDefinition &expansion = catalog.expansions[index];
        const std::string path = "expansions[" + std::to_string(index) + "]";
        validator.check(!expansion.prefix.empty(), path + ".prefix", "expansion must extend the route");
        std::vector<Point> route = expansion.prefix;
        route.insert(route.end(), catalog.route.begin(), catalog.route.end());
        validator.route(route, path + ".route");
        std::vector<Point> pads = catalog.pads, units = catalog.unitCells;
        pads.insert(pads.end(), expansion.pads.begin(), expansion.pads.end());
        units.insert(units.end(), expansion.unitCells.begin(), expansion.unitCells.end());
        validator.cells(pads, path + ".pads");
        validator.cells(units, path + ".unitCells");
        validator.check(cardKind(expansion.reward, catalog), path + ".reward", "unknown reward card");
    }
    validator.check(!catalog.waves.empty() && catalog.waves.size() <= CatalogLimits::MaxWaves, "waves",
                    "expected 1 to 100 waves");
    for (std::size_t wave = 0; wave < catalog.waves.size(); ++wave) {
        std::int64_t count = 0;
        for (std::size_t index = 0; index < catalog.waves[wave].size(); ++index) {
            const SpawnGroup &group = catalog.waves[wave][index];
            const std::string path = "waves[" + std::to_string(wave) + "][" + std::to_string(index) + "]";
            validator.check(static_cast<std::size_t>(group.kind) < catalog.enemies.size(), path + ".kind",
                            "unknown enemy kind");
            validator.integer(group.count, 0, CatalogLimits::MaxWaveEnemies, path + ".count");
            validator.number(group.start, 0, 1000000, path + ".start");
            validator.number(group.interval, 0, 1000000, path + ".interval");
            count += group.count;
            validator.check(std::isfinite(static_cast<double>(group.start) +
                                          std::max(0.0, static_cast<double>(group.count) - 1.0) *
                                              static_cast<double>(group.interval)) &&
                                static_cast<double>(group.start) +
                                        std::max(0.0, static_cast<double>(group.count) - 1.0) *
                                            static_cast<double>(group.interval) <=
                                    1000000,
                            path, "spawn schedule exceeds supported duration");
        }
        validator.check(count <= CatalogLimits::MaxWaveEnemies, "waves[" + std::to_string(wave) + "]",
                        "wave exceeds spawn budget");
    }
    const RunBalance &balance = catalog.balance;
    validator.integer(balance.baseHp, 1, CatalogLimits::MaxAmount, "balance.baseHp");
    validator.integer(balance.energyCap, 1, CatalogLimits::MaxAmount, "balance.energyCap");
    validator.integer(balance.initialEnergy, 0, balance.energyCap, "balance.initialEnergy");
    validator.integer(balance.initialSlots, 1, CatalogLimits::MaxSlots, "balance.initialSlots");
    validator.integer(balance.upgradedSlots, balance.initialSlots, CatalogLimits::MaxSlots,
                      "balance.upgradedSlots");
    validator.integer(balance.prepDigs, 0, 30, "balance.prepDigs");
    validator.integer(balance.prepScans, 0, 30, "balance.prepScans");
    validator.integer(balance.nestEnemies, 0, CatalogLimits::MaxNestEnemies, "balance.nestEnemies");
    validator.integer(balance.expansionWave, 0, CatalogLimits::MaxWaves, "balance.expansionWave");
    validator.integer(balance.armoredGuardHp, 1, CatalogLimits::MaxAmount, "balance.armoredGuardHp");
    if (catalog.isUnit(CardKind::Guard))
        validator.check(balance.armoredGuardHp >= catalog.card(CardKind::Guard).hp, "balance.armoredGuardHp",
                        "armor must not reduce guard maximum HP");
    validator.number(balance.levelBonus, 0, 10, "balance.levelBonus");
    validator.number(balance.coilBonus, 1, 10, "balance.coilBonus");
    validator.number(balance.slowFactor, 0, 1, "balance.slowFactor");
    validator.number(balance.energyInterval, 0, 1000000, "balance.energyInterval", true);
    validator.number(balance.minerStep, 0, 1000000, "balance.minerStep", true);
    validator.number(balance.softDig, 0, 1000000, "balance.softDig", true);
    validator.number(balance.hardDig, 0, 1000000, "balance.hardDig", true);
    validator.price(balance.tool, "balance.tool");
    validator.price(balance.armor, "balance.armor");
    validator.price(balance.slots, "balance.slots");
    validator.price(balance.coil, "balance.coil");
    validator.price(balance.heal, "balance.heal");
    validator.price(balance.recruit, "balance.recruit");
    std::array<bool, 30> connected{};
    std::deque<int> pending;
    if (catalog.mine[2].opened) {
        connected[2] = true;
        pending.push_back(2);
    }
    validator.check(catalog.mine[2].opened, "mine[2].opened", "the initial miner cell must be open");
    while (!pending.empty()) {
        const int cell = pending.front();
        pending.pop_front();
        for (int next = 0; next < 30; ++next)
            if (!connected[next] && catalog.mine[next].opened &&
                std::abs(next % 5 - cell % 5) + std::abs(next / 5 - cell / 5) == 1) {
                connected[next] = true;
                pending.push_back(next);
            }
    }
    for (std::size_t index = 0; index < catalog.mine.size(); ++index) {
        const MineTileDefinition &tile = catalog.mine[index];
        const std::string path = "mine[" + std::to_string(index) + "]";
        validator.integer(static_cast<int>(tile.ore), 0, 4, path + ".ore");
        validator.integer(tile.amount, 0, CatalogLimits::MaxAmount, path + ".amount");
        validator.integer(tile.grade, 1, 2, path + ".grade");
        validator.check(!tile.opened || connected[index], path + ".opened",
                        "open galleries must connect to the initial miner cell");
        validator.check(!tile.opened || !tile.nest, path + ".nest",
                        "initial open cells cannot contain a live nest");
    }
    return std::move(validator.result);
}
} // namespace thorium::game
