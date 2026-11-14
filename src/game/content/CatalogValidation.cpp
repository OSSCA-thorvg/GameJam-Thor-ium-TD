#include "CatalogValidation.h"
#include "Excavation.h"
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
    /** Bind authored battlefield bounds without importing any viewport or renderer type. */
    Validator(int width, int height) : width(width), height(height) {}
    int width;
    int height;
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
                           value.x < width && value.y >= 0 && value.y < height &&
                           value.x == std::round(value.x) && value.y == std::round(value.y);
        check(valid, path, "expected an integer tile inside the authored battlefield");
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
/** Validate each cutaway independently, including its bounds and connected safe entry cells. */
void validateMine(Validator &validator, const Catalog &catalog, const MineLayerDefinition &layer,
                  const std::string &prefix, std::set<int> &relics) {
    validator.integer(layer.width, 3, CatalogLimits::MaxMineWidth, prefix + ".width");
    validator.integer(layer.height, 3, CatalogLimits::MaxMineHeight, prefix + ".height");
    const int count = static_cast<int>(layer.cells.size());
    if (layer.width < 3 || layer.width > CatalogLimits::MaxMineWidth || layer.height < 3 ||
        layer.height > CatalogLimits::MaxMineHeight || count != layer.width * layer.height) {
        validator.check(false, prefix + ".cells", "expected width x height authored cells");
        return;
    }
    const std::string cells = prefix + ".cells";
    validator.integer(static_cast<int>(layer.biome), 0, static_cast<int>(StageBiome::Swamp),
                      prefix + ".biome");
    const bool startValid = layer.start >= 0 && layer.start < count;
    validator.check(startValid, prefix + ".start", "the initial miner cell must lie on the board");
    if (!startValid)
        return;
    std::vector<bool> connected(layer.cells.size(), false);
    std::deque<int> pending;
    const MineTileDefinition &entry = layer.cells[static_cast<std::size_t>(layer.start)];
    if (entry.opened) {
        connected[static_cast<std::size_t>(layer.start)] = true;
        pending.push_back(layer.start);
    }
    validator.check(entry.opened, cells + "[" + std::to_string(layer.start) + "].opened",
                    "the initial miner cell must be open");
    while (!pending.empty()) {
        const int cell = pending.front();
        pending.pop_front();
        for (int next = 0; next < count; ++next)
            if (!connected[static_cast<std::size_t>(next)] &&
                layer.cells[static_cast<std::size_t>(next)].opened &&
                std::abs(next % layer.width - cell % layer.width) +
                        std::abs(next / layer.width - cell / layer.width) ==
                    1) {
                connected[static_cast<std::size_t>(next)] = true;
                pending.push_back(next);
            }
    }
    for (std::size_t index = 0; index < layer.cells.size(); ++index) {
        const MineTileDefinition &tile = layer.cells[index];
        const std::string path = cells + "[" + std::to_string(index) + "]";
        validator.integer(static_cast<int>(tile.ore), 0, 4, path + ".ore");
        validator.integer(tile.amount, 0, CatalogLimits::MaxAmount, path + ".amount");
        validator.integer(tile.grade, 1, 2, path + ".grade");
        validator.check(!tile.opened || connected[index], path + ".opened",
                        "open galleries must connect to the initial miner cell");
        validator.check(!tile.opened || !tile.nest, path + ".nest",
                        "initial open cells cannot contain a live nest");
        const int special = static_cast<int>(tile.special);
        validator.integer(special, 0, 4, path + ".special");
        if (tile.special == MineSpecial::None)
            continue;
        validator.check(!tile.nest && !tile.opened && tile.ore == Ore::Rock, path + ".special",
                        "special tiles are closed rock without a nest or mineral");
        if (tile.special == MineSpecial::Relic) {
            const int relic = static_cast<int>(tile.relic);
            validator.integer(relic, 0, RelicCount - 1, path + ".relic");
            validator.check(relics.insert(relic).second, path + ".relic",
                            "each relic is placed at most once per catalog");
        }
        if (tile.special == MineSpecial::Fossil)
            validator.check(cardKind(tile.card, catalog), path + ".card", "fossil must offer a defined card");
        if (tile.special == MineSpecial::Energy)
            validator.integer(tile.amount, 1, catalog.balance.energyCap, path + ".amount");
        else
            validator.check(tile.amount == 0, path + ".amount", "only energy veins carry an amount");
    }
}
/** Validate boulders, trait pads, tunnels, bogs, rails, gas tanks and lava vents against the battlefield they
 * are authored on. */
void validateGimmicks(Validator &validator, const Catalog &catalog) {
    const RunBalance &balance = catalog.balance;
    validator.price(balance.boulder, "balance.boulder");
    validator.check(
        balance.boulder.iron + balance.boulder.copper + balance.boulder.crystal + balance.boulder.gold > 0,
        "balance.boulder", "clearing a boulder must cost at least one resource");
    validator.number(balance.highGroundRange, 1, 10, "balance.highGroundRange");
    validator.number(balance.crystalDamage, 1, 10, "balance.crystalDamage");
    validator.number(balance.tunnelSpeed, 0, 10, "balance.tunnelSpeed", true);
    validator.number(balance.ventPeriod, 0, 1000, "balance.ventPeriod", true);
    validator.number(balance.ventWarning, 0, 1000, "balance.ventWarning");
    validator.check(!(balance.ventWarning > balance.ventPeriod), "balance.ventWarning",
                    "the warning must fit inside one vent period");
    validator.number(balance.ventRadius, 0, 100, "balance.ventRadius", true);
    validator.integer(balance.ventDamage, 1, CatalogLimits::MaxDamage, "balance.ventDamage");
    validator.number(balance.revealTime, 0, 1000, "balance.revealTime");
    validator.number(balance.cartSpeed, 0, 100, "balance.cartSpeed", true);
    validator.integer(balance.cartDamage, 1, CatalogLimits::MaxDamage, "balance.cartDamage");
    validator.number(balance.cartWidth, 0, 10, "balance.cartWidth", true);
    validator.number(balance.cartCooldown, 0, 1000, "balance.cartCooldown");
    validator.number(balance.cartFirst, 0, 1000, "balance.cartFirst");
    validator.number(balance.tankRadius, 0, 100, "balance.tankRadius", true);
    validator.integer(balance.tankDamage, 1, CatalogLimits::MaxDamage, "balance.tankDamage");
    validator.number(balance.bogSpeed, 0, 1, "balance.bogSpeed", true);
    validator.integer(balance.towerHp, 1, CatalogLimits::MaxAmount, "balance.towerHp");
    validator.number(balance.towerLevelHp, 0, 10, "balance.towerLevelHp");
    validator.number(balance.towerRepair, 0, 1, "balance.towerRepair");
    validator.number(balance.unitRegen, 0, 1, "balance.unitRegen");
    validator.integer(balance.repairHp, 1, CatalogLimits::MaxAmount, "balance.repairHp");
    validator.integer(balance.barricadeHp, 1, CatalogLimits::MaxAmount, "balance.barricadeHp");
    validator.integer(balance.heroHp, 1, CatalogLimits::MaxAmount, "balance.heroHp");
    validator.integer(balance.heroDamage, 0, CatalogLimits::MaxDamage, "balance.heroDamage");
    validator.number(balance.heroRange, 0, 100, "balance.heroRange", true);
    validator.number(balance.heroInterval, 0, 1000, "balance.heroInterval", true);
    validator.integer(balance.heroBlockers, 0, 100, "balance.heroBlockers");
    validator.number(balance.heroRespawn, 0, 1000, "balance.heroRespawn");
    validator.number(balance.heroMove, 0, 1000, "balance.heroMove");
    validator.number(balance.heroRegen, 0, 1, "balance.heroRegen");
    validator.number(balance.unitLeash, 0, 100, "balance.unitLeash");
    validator.integer(balance.spoilsPerShard, 1, CatalogLimits::MaxAmount, "balance.spoilsPerShard");
    validator.integer(balance.bossShards, 0, CatalogLimits::MaxAmount, "balance.bossShards");
    validator.integer(balance.recruitShards, 0, CatalogLimits::MaxAmount, "balance.recruitShards");
    validator.integer(balance.rerollShards, 1, CatalogLimits::MaxAmount, "balance.rerollShards");
    validator.integer(balance.heroTrainShards, 1, CatalogLimits::MaxAmount, "balance.heroTrainShards");
    validator.number(balance.heroLevelBonus, 0, 10, "balance.heroLevelBonus");
    validator.integer(balance.heroMaxLevel, 1, 10, "balance.heroMaxLevel");
    validator.number(balance.unitSway, 0, 1, "balance.unitSway");
    validator.number(balance.unitSpeed, 0, 100, "balance.unitSpeed");
    validator.number(balance.heroSpeed, 0, 100, "balance.heroSpeed");
    validator.check(!catalog.hero || !catalog.unitCells.empty(), "hero", "a hero needs a unit cell");
    /** Check whether a point is one of the given placement tiles. */
    const auto among = [](const std::vector<Point> &points, Point point) -> bool {
        return std::any_of(points.begin(), points.end(),
                           [point](Point other) { return distance(other, point) < .01F; });
    };
    /** Every tile a building can ever stand on: pads, both expansions' pads and boulders. */
    std::vector<Point> sites = catalog.pads;
    for (const ExpansionDefinition &expansion : catalog.expansions)
        sites.insert(sites.end(), expansion.pads.begin(), expansion.pads.end());
    validator.cells(catalog.boulders, "boulders");
    for (Point boulder : catalog.boulders)
        validator.check(!among(sites, boulder) && !among(catalog.unitCells, boulder), "boulders",
                        "a boulder needs a tile of its own, off every pad and unit cell");
    sites.insert(sites.end(), catalog.boulders.begin(), catalog.boulders.end());
    std::vector<Point> traits;
    for (std::size_t index = 0; index < catalog.padTraits.size(); ++index) {
        const TraitPad &pad = catalog.padTraits[index];
        const std::string path = "padTraits[" + std::to_string(index) + "]";
        validator.check(pad.trait == PadTrait::HighGround || pad.trait == PadTrait::Crystal, path + ".trait",
                        "unknown pad trait");
        validator.check(among(sites, pad.position), path + ".position", "a trait needs a building pad");
        validator.check(!among(traits, pad.position), path + ".position", "a pad carries at most one trait");
        traits.push_back(pad.position);
    }
    validator.check(catalog.tunnels.size() <= CatalogLimits::MaxTunnels, "tunnels", "too many tunnels");
    for (std::size_t index = 0; index < catalog.tunnels.size(); ++index) {
        const RouteSpan &span = catalog.tunnels[index];
        const std::string path = "tunnels[" + std::to_string(index) + "]";
        const bool known = span.route <= catalog.additionalRoutes.size();
        validator.check(known, path + ".route", "unknown entrance route");
        if (!known)
            continue;
        const std::vector<Point> &points =
            span.route == 0 ? catalog.route : catalog.additionalRoutes[span.route - 1].points;
        float length = 0;
        for (std::size_t point = 1; point < points.size(); ++point)
            length += distance(points[point - 1], points[point]);
        /** The entrance tile and the last tile before the base stay in the open. */
        const bool entered = std::isfinite(span.enter) && span.enter >= 1 && span.enter < length;
        validator.check(entered, path + ".enter", "a tunnel starts at least one tile past the entrance");
        const bool left = std::isfinite(span.exit) && span.exit >= span.enter + 2 && span.exit <= length - 1;
        validator.check(left, path + ".exit",
                        "a tunnel is at least two tiles long and ends at least one tile before the base");
        if (!entered || !left)
            continue;
        for (std::size_t other = 0; other < index; ++other)
            validator.check(catalog.tunnels[other].route != span.route ||
                                span.enter >= catalog.tunnels[other].exit ||
                                span.exit <= catalog.tunnels[other].enter,
                            path, "tunnels on one route must not overlap");
        /** Walk the covered tiles; a unit standing there could never block. */
        float travelled = 0;
        for (std::size_t point = 1; point < points.size(); ++point) {
            const Point a = points[point - 1], b = points[point];
            const float segment = distance(a, b);
            for (float along = 0; segment > 0 && along <= segment; along += .5F) {
                const float progress = travelled + along;
                if (progress <= span.enter || progress >= span.exit)
                    continue;
                const Point tile{a.x + (b.x - a.x) * along / segment, a.y + (b.y - a.y) * along / segment};
                validator.check(!among(catalog.unitCells, tile), path, "a tunnel covers a unit cell");
            }
            travelled += segment;
        }
    }
    validator.check(catalog.bogs.size() <= CatalogLimits::MaxTunnels, "bogs", "too many bogs");
    for (std::size_t index = 0; index < catalog.bogs.size(); ++index) {
        const RouteSpan &span = catalog.bogs[index];
        const std::string path = "bogs[" + std::to_string(index) + "]";
        const bool known = span.route <= catalog.additionalRoutes.size();
        validator.check(known, path + ".route", "unknown entrance route");
        if (!known)
            continue;
        const std::vector<Point> &points =
            span.route == 0 ? catalog.route : catalog.additionalRoutes[span.route - 1].points;
        float length = 0;
        for (std::size_t point = 1; point < points.size(); ++point)
            length += distance(points[point - 1], points[point]);
        validator.check(std::isfinite(span.enter) && span.enter >= 0 && std::isfinite(span.exit) &&
                            span.exit >= span.enter + 1 && span.exit <= length,
                        path, "a bog is at least one tile long and lies on its route");
        for (std::size_t other = 0; other < index; ++other)
            validator.check(catalog.bogs[other].route != span.route ||
                                span.enter >= catalog.bogs[other].exit ||
                                span.exit <= catalog.bogs[other].enter,
                            path, "bogs on one route must not overlap");
        for (const RouteSpan &tunnel : catalog.tunnels)
            validator.check(tunnel.route != span.route || span.enter >= tunnel.exit ||
                                span.exit <= tunnel.enter,
                            path, "a bog must not lie inside a tunnel");
    }
    for (std::size_t index = 0; index < catalog.forks.size(); ++index) {
        const ForkDefinition &fork = catalog.forks[index];
        const std::string path = "forks[" + std::to_string(index) + "]";
        const bool known = fork.route <= catalog.additionalRoutes.size();
        validator.check(known, path + ".route", "unknown entrance route");
        if (!known)
            continue;
        for (std::size_t other = 0; other < index; ++other)
            validator.check(catalog.forks[other].route != fork.route, path, "one fork per route");
        const std::vector<Point> &points =
            fork.route == 0 ? catalog.route : catalog.additionalRoutes[fork.route - 1].points;
        /** Progress of a point along the route, or -1 when it is not on it. */
        const auto along = [&points](Point point) -> float {
            float total = 0;
            for (std::size_t corner = 1; corner < points.size(); ++corner) {
                const Point a = points[corner - 1], b = points[corner];
                const float length = distance(a, b);
                const float t =
                    length < .0001F
                        ? 0
                        : std::clamp(((point.x - a.x) * (b.x - a.x) + (point.y - a.y) * (b.y - a.y)) /
                                         (length * length),
                                     0.0F, 1.0F);
                if (distance(point, {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}) < .01F)
                    return total + length * t;
                total += length;
            }
            return -1;
        };
        const float split = along(fork.split), merge = along(fork.merge);
        validator.check(split >= 0 && merge >= split + 1, path,
                        "split and merge lie on the route, the merge at least a tile after the split");
        std::vector<Point> branch{fork.split};
        branch.insert(branch.end(), fork.detour.begin(), fork.detour.end());
        branch.push_back(fork.merge);
        for (std::size_t corner = 1; corner < branch.size(); ++corner) {
            const Point a = branch[corner - 1], b = branch[corner];
            const bool straight = (a.x == b.x) != (a.y == b.y);
            validator.check(validator.point(b, path + ".detour") && straight, path,
                            "the detour runs in straight cardinal steps");
            const float length = distance(a, b);
            for (float step = 1; straight && step < length; step += 1) {
                const Point tile{a.x + (b.x - a.x) * step / length, a.y + (b.y - a.y) * step / length};
                validator.check(!among(sites, tile) && !among(catalog.boulders, tile), path,
                                "the detour crosses no pad or boulder");
            }
        }
        for (const std::vector<RouteSpan> *spans : {&catalog.tunnels, &catalog.bogs})
            for (const RouteSpan &span : *spans)
                validator.check(span.route != fork.route || span.exit <= split + .01F ||
                                    span.enter >= merge - .01F ||
                                    (span.enter >= split - .01F && span.exit <= merge + .01F),
                                path, "a span lies wholly before, inside or after the forked stretch");
    }
    validator.check(catalog.rails.size() <= CatalogLimits::MaxRails, "rails", "too many rails");
    for (std::size_t index = 0; index < catalog.rails.size(); ++index) {
        const RailDefinition &rail = catalog.rails[index];
        const std::string path = "rails[" + std::to_string(index) + "]";
        const bool ends =
            validator.point(rail.from, path + ".from") && validator.point(rail.to, path + ".to");
        validator.check(!ends || distance(rail.from, rail.to) >= 2, path,
                        "a rail is at least two tiles long");
        if (validator.point(rail.lever, path + ".lever"))
            validator.check(!among(sites, rail.lever) && !among(catalog.unitCells, rail.lever),
                            path + ".lever", "a lever needs a tile of its own, off every pad and unit cell");
    }
    validator.check(catalog.tanks.size() <= CatalogLimits::MaxTanks, "tanks", "too many gas tanks");
    std::vector<Point> tanks;
    for (std::size_t index = 0; index < catalog.tanks.size(); ++index) {
        const Point position = catalog.tanks[index].position;
        const std::string path = "tanks[" + std::to_string(index) + "].position";
        if (validator.point(position, path)) {
            validator.check(!among(sites, position) && !among(catalog.unitCells, position) &&
                                !among(tanks, position),
                            path, "a gas tank needs a tile of its own, off every pad and unit cell");
            tanks.push_back(position);
        }
    }
    validator.check(catalog.vents.size() <= CatalogLimits::MaxVents, "vents", "too many lava vents");
    std::vector<Point> vents;
    for (std::size_t index = 0; index < catalog.vents.size(); ++index) {
        const VentDefinition &vent = catalog.vents[index];
        const std::string path = "vents[" + std::to_string(index) + "]";
        if (validator.point(vent.position, path + ".position")) {
            validator.check(!among(sites, vent.position) && !among(vents, vent.position), path + ".position",
                            "a vent needs a tile of its own, off every building pad");
            vents.push_back(vent.position);
        }
        validator.check(std::isfinite(vent.phase) && vent.phase > 0 && vent.phase <= balance.ventPeriod,
                        path + ".phase",
                        "the first eruption comes after the battle starts and within one vent period");
    }
}
} // namespace
std::string CatalogValidation::summary() const {
    std::string text;
    for (const CatalogIssue &issue : issues)
        text += (text.empty() ? "" : "\n") + issue.path + ": " + issue.message;
    return text;
}
CatalogValidation validateCatalog(const Catalog &catalog) {
    Validator validator(catalog.width, catalog.height);
    validator.integer(catalog.width, 4, 128, "width");
    validator.integer(catalog.height, 4, 128, "height");
    validator.check(catalog.additionalRoutes.size() <= 5, "additionalRoutes",
                    "at most six entrances are supported");
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
        validator.number(card.arc, 0, 180, path + ".arc");
        validator.number(card.aura, 0, 10, path + ".aura");
        validator.check(card.aura == 0 || card.category == CardCategory::Building, path + ".aura",
                        "only buildings can support nearby buildings");
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
        validator.check(!enemy.name.empty(), path + ".name", "enemy name must not be empty");
        validator.integer(enemy.shield, 0, CatalogLimits::MaxAmount, path + ".shield");
        validator.integer(enemy.shieldCap, 0, CatalogLimits::MaxDamage, path + ".shieldCap");
        validator.check(enemy.ability >= EnemyAbility::None && enemy.ability <= EnemyAbility::Airdrop,
                        path + ".ability", "unknown enemy ability");
        if (enemy.ability == EnemyAbility::Airdrop) {
            validator.check(enemy.abilityAmount > 0, path + ".abilityAmount", "a carrier needs cargo");
            validator.check(enemy.abilityRadius > 0 && enemy.abilityRadius < 1, path + ".abilityRadius",
                            "the drop must happen partway along the route");
        }
        if (enemy.ability != EnemyAbility::None && enemy.ability != EnemyAbility::Leap &&
            enemy.ability != EnemyAbility::Airdrop)
            validator.number(enemy.abilityInterval, 0, 1000, path + ".abilityInterval", true);
        if (enemy.ability == EnemyAbility::Leap) {
            validator.check(enemy.abilityAmount > 0, path + ".abilityAmount", "a leaper needs a jump");
            validator.number(enemy.abilityRadius, 0, 100, path + ".abilityRadius", true);
        }
        validator.number(enemy.stealth, 0, 100, path + ".stealth");
        validator.number(enemy.abilityRadius, 0, 100, path + ".abilityRadius");
        validator.integer(enemy.abilityAmount, 0, CatalogLimits::MaxNestEnemies, path + ".abilityAmount");
        validator.number(enemy.abilityDuration, 0, 1000, path + ".abilityDuration");
        validator.number(enemy.abilitySpeed, 0, 100, path + ".abilitySpeed");
        validator.number(enemy.abilityWindup, 0, 1000, path + ".abilityWindup");
        if (enemy.ability == EnemyAbility::Siege) {
            validator.check(enemy.abilityAmount > 0, path + ".abilityAmount", "a siege enemy needs damage");
            validator.number(enemy.abilityRadius, 0, 100, path + ".abilityRadius", true);
        }
        if (enemy.ability == EnemyAbility::Sabotage) {
            validator.check(enemy.abilityAmount > 0, path + ".abilityAmount", "a sapper needs a charge");
            validator.number(enemy.abilityWindup, 0, 1000, path + ".abilityWindup", true);
            validator.number(enemy.abilityRadius, 0, 100, path + ".abilityRadius", true);
        }
        validator.integer(enemy.splitCount, 0, 16, path + ".splitCount");
        const bool spawnsMinions = enemy.splitCount > 0 || enemy.ability == EnemyAbility::Stomp;
        const bool knownMinion = static_cast<std::size_t>(enemy.minionKind) < catalog.enemies.size();
        validator.check(!spawnsMinions || knownMinion, path + ".minionKind", "unknown minion kind");
        if (spawnsMinions && knownMinion) {
            const EnemyDefinition &minion = catalog.enemies[static_cast<std::size_t>(enemy.minionKind)];
            validator.check(minion.splitCount == 0 && minion.ability != EnemyAbility::Stomp,
                            path + ".minionKind", "minions must not create further minions");
        }
    }
    for (std::size_t index = 0; index < catalog.elites.size(); ++index) {
        const EliteDefinition &elite = catalog.elites[index];
        const std::string path = "elites[" + std::to_string(index) + "]";
        validator.number(elite.health, 0, 100, path + ".health", true);
        validator.integer(elite.armor, 0, CatalogLimits::MaxDamage, path + ".armor");
        validator.number(elite.speed, 0, 10, path + ".speed", true);
        validator.integer(elite.leak, 0, CatalogLimits::MaxDamage, path + ".leak");
        validator.number(elite.auraRadius, 0, 100, path + ".auraRadius");
        validator.number(elite.auraSpeed, 0, 10, path + ".auraSpeed", true);
        validator.number(elite.auraGuard, 0, .95F, path + ".auraGuard");
        validator.number(elite.regen, 0, 1, path + ".regen");
        validator.number(elite.enrageBelow, 0, 1, path + ".enrageBelow");
        validator.number(elite.enrageSpeed, 0, 10, path + ".enrageSpeed", true);
        if (index == static_cast<std::size_t>(EliteAffix::None))
            validator.check(elite.health == 1 && elite.armor == 0 && elite.speed == 1 && elite.leak == 0 &&
                                elite.auraRadius == 0 && elite.regen == 0 && elite.enrageBelow == 0,
                            path, "an ordinary enemy carries no elite effect");
        else
            validator.check(!elite.name.empty(), path + ".name", "elite modifier name must not be empty");
    }
    for (std::size_t index = 0; index < catalog.starterDeck.size(); ++index)
        validator.check(cardKind(catalog.starterDeck[index], catalog),
                        "starterDeck[" + std::to_string(index) + "]", "unknown card kind");
    std::set<CardKind> candidates;
    for (std::size_t index = 0; index < catalog.recruitmentBuildings.size(); ++index)
        validator.check(catalog.isBuilding(catalog.recruitmentBuildings[index]),
                        "recruitmentBuildings[" + std::to_string(index) + "]",
                        "a recruited building must be a building");
    validator.check(catalog.recruitmentBuildings.empty() || catalog.recruitmentBuildings.size() >= 2,
                    "recruitmentBuildings", "recruitment offers two buildings");
    for (std::size_t index = 0; index < catalog.recruitmentCandidates.size(); ++index) {
        const CardKind kind = catalog.recruitmentCandidates[index];
        const std::string path = "recruitmentCandidates[" + std::to_string(index) + "]";
        validator.check(catalog.isUnit(kind), path, "candidate must reference a supported unit card");
        validator.check(candidates.insert(kind).second, path, "recruitment candidates must be different");
    }
    validator.route(catalog.route, "route");
    for (std::size_t index = 0; index < catalog.additionalRoutes.size(); ++index) {
        const EntranceRoute &entrance = catalog.additionalRoutes[index];
        const std::string path = "additionalRoutes[" + std::to_string(index) + "]";
        validator.check(!entrance.name.empty(), path + ".name", "entrance needs an id");
        validator.route(entrance.points, path + ".points");
        validator.integer(entrance.opensAtWave, 1, static_cast<int>(catalog.waves.size()),
                          path + ".opensAtWave");
        if (!catalog.route.empty() && !entrance.points.empty())
            validator.check(distance(entrance.points.back(), catalog.route.back()) == 0, path + ".points",
                            "all entrances must reach the same defended base");
    }
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
            validator.check(group.route <= catalog.additionalRoutes.size(), path + ".route",
                            "unknown entrance route");
            validator.check(static_cast<std::size_t>(group.affix) < catalog.elites.size(), path + ".affix",
                            "unknown elite modifier");
            if (group.route > 0 && group.route <= catalog.additionalRoutes.size())
                validator.check(catalog.additionalRoutes[group.route - 1].opensAtWave <=
                                    static_cast<int>(wave + 1),
                                path + ".route", "wave uses an entrance before it opens");
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
    for (std::size_t kind = 0; kind < catalog.enemyDebuts.size(); ++kind)
        validator.integer(catalog.enemyDebuts[kind], 0, static_cast<int>(catalog.waves.size()),
                          "enemyDebuts[" + std::to_string(kind) + "]");
    validator.check(catalog.vigor.size() <= catalog.waves.size(), "vigor", "more multipliers than waves");
    for (std::size_t wave = 0; wave < catalog.vigor.size(); ++wave)
        validator.number(catalog.vigor[wave], 0, 10, "vigor[" + std::to_string(wave) + "]", true);
    for (std::size_t affix = 0; affix < catalog.eliteDebuts.size(); ++affix)
        validator.integer(catalog.eliteDebuts[affix], 0, static_cast<int>(catalog.waves.size()),
                          "eliteDebuts[" + std::to_string(affix) + "]");
    const RunBalance &balance = catalog.balance;
    validator.integer(balance.baseHp, 1, CatalogLimits::MaxAmount, "balance.baseHp");
    validator.integer(balance.energyCap, 1, CatalogLimits::MaxAmount, "balance.energyCap");
    validator.integer(balance.initialEnergy, 0, balance.energyCap, "balance.initialEnergy");
    validator.integer(balance.initialSlots, 1, CatalogLimits::MaxSlots, "balance.initialSlots");
    validator.integer(balance.upgradedSlots, balance.initialSlots, CatalogLimits::MaxSlots,
                      "balance.upgradedSlots");
    validator.integer(balance.prepDigs, 0, 30, "balance.prepDigs");
    validator.integer(balance.prepScans, 0, 30, "balance.prepScans");
    validator.integer(static_cast<int>(catalog.mining), 0, static_cast<int>(MiningMode::Excavate), "mining");
    validator.integer(balance.pickPower, 1, 8, "balance.pickPower");
    validator.integer(balance.blastPower, 1, 8, "balance.blastPower");
    validator.integer(balance.blastDigs, 1, 30, "balance.blastDigs");
    validator.integer(balance.scanReach, 0, 4, "balance.scanReach");
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
    validator.integer(balance.relicEnergy, 0, balance.energyCap, "balance.relicEnergy");
    validator.integer(balance.relicDigs, 0, 30, "balance.relicDigs");
    validator.integer(balance.relicNestReduction, 0, CatalogLimits::MaxNestEnemies,
                      "balance.relicNestReduction");
    validator.integer(balance.relicScans, 0, 30, "balance.relicScans");
    validator.price(balance.drill, "balance.drill");
    validator.integer(balance.drillDigs, 0, 30, "balance.drillDigs");
    validator.integer(balance.hireGold, 1, CatalogLimits::MaxAmount, "balance.hireGold");
    validator.integer(balance.hireStep, 0, CatalogLimits::MaxAmount, "balance.hireStep");
    validator.integer(balance.hireLimit, 0, 30, "balance.hireLimit");
    validator.price(balance.supply, "balance.supply");
    validator.integer(balance.supplyEnergy, 1, balance.energyCap, "balance.supplyEnergy");
    for (std::size_t index = 0; index < balance.awaken.size(); ++index) {
        const ResourceCost &cost = balance.awaken[index];
        const std::string path = "balance.awaken[" + std::to_string(index) + "]";
        validator.price(cost, path);
        validator.check(cost.iron + cost.copper + cost.crystal + cost.gold > 0, path,
                        "awakening must cost at least one resource");
    }
    validateGimmicks(validator, catalog);
    std::set<int> relics;
    validateMine(validator, catalog, catalog.mine, "mine", relics);
    for (const std::pair<int, int> &cover : catalog.wallCover) {
        validator.check(cover.first >= 0 && cover.first < static_cast<int>(catalog.mine.cells.size()),
                        "wallCover", "a wall cell must lie inside the first layer");
        validator.integer(cover.second, 1, MaxCover, "wallCover.layers");
    }
    validator.check(catalog.deeperMines.size() <= 8, "deeperMines",
                    "at most eight additional layers are supported");
    for (std::size_t index = 0; index < catalog.deeperMines.size(); ++index) {
        const MineLayerDefinition &layer = catalog.deeperMines[index];
        const std::string path = "deeperMines[" + std::to_string(index) + "]";
        validator.check(!layer.name.empty(), path + ".name", "layer needs an id");
        validator.integer(layer.opensAfterWave, 1, static_cast<int>(catalog.waves.size()) - 1,
                          path + ".opensAfterWave");
        if (index)
            validator.check(layer.opensAfterWave >= catalog.deeperMines[index - 1].opensAfterWave,
                            path + ".opensAfterWave", "deeper layers must unlock in order");
        validateMine(validator, catalog, layer, path, relics);
    }
    return std::move(validator.result);
}
} // namespace thorium::game
