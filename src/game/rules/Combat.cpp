#include "RuleGeometry.h"
#include "Run.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace thorium::game {
namespace {
/** Expire spell areas and keep enemies inside a remaining tar area slowed for the next movement step;
 *  enemies inside a tunnel are out of reach. */
void applyGroundFields(RunState &state, const Run &run) {
    state.fields.erase(std::remove_if(state.fields.begin(), state.fields.end(),
                                      [&state](const GroundField &field) -> bool {
                                          return field.until <= state.battleTime;
                                      }),
                       state.fields.end());
    for (const GroundField &field : state.fields)
        for (Enemy &enemy : state.enemies)
            if (!run.tunneled(enemy) && !run.flying(enemy) &&
                distance(enemy.position, field.position) <= field.radius)
                enemy.slowUntil = std::max(enemy.slowUntil, state.battleTime + .2F);
}
/** Return the strongest support bonus covering one attacking building; bonuses do not stack. */
float auraBonus(const Run &run, const Tower &tower) {
    const Catalog &catalog = run.catalog();
    float bonus = 0;
    for (const Tower &support : run.state().towers) {
        const CardDefinition &definition = catalog.card(support.kind);
        if (support.id != tower.id && definition.aura > 0 && support.disabled <= 0 &&
            distance(support.position, tower.position) <= run.buildingRange(support.kind, support.position))
            bonus =
                std::max(bonus, definition.aura * (1.0F + (support.level - 1) * catalog.balance.levelBonus));
    }
    return bonus;
}
/** Check whether an enemy lies inside a cone from origin toward aim with the given half-angle in degrees. */
bool insideCone(Point origin, Point aim, Point enemy, float arc) {
    const float ax = aim.x - origin.x, ay = aim.y - origin.y;
    const float ex = enemy.x - origin.x, ey = enemy.y - origin.y;
    const float aimLength = std::hypot(ax, ay), enemyLength = std::hypot(ex, ey);
    if (aimLength < .001F || enemyLength < .001F)
        return true;
    return (ax * ex + ay * ey) / (aimLength * enemyLength) >= std::cos(arc * 3.14159265F / 180.0F);
}
/** Sum the segment lengths of a cardinal polyline. */
float pathLength(const std::vector<Point> &points) {
    float total = 0;
    for (std::size_t index = 1; index < points.size(); ++index)
        total += distance(points[index - 1], points[index]);
    return total;
}
/** Distance from a point to the segment between a and b. */
float segmentDistance(Point point, Point a, Point b) {
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float length = dx * dx + dy * dy;
    const float t =
        length < .0001F ? 0 : std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) / length, 0.0F, 1.0F);
    return distance(point, {a.x + dx * t, a.y + dy * t});
}
/** Progress of a point along a polyline, or -1 when the point is not on it. */
float progressOf(const std::vector<Point> &points, Point point) {
    float total = 0;
    for (std::size_t index = 1; index < points.size(); ++index) {
        if (segmentDistance(point, points[index - 1], points[index]) < .01F)
            return total + distance(points[index - 1], point);
        total += distance(points[index - 1], points[index]);
    }
    return points.size() == 1 && distance(points.front(), point) < .01F ? 0 : -1;
}
/** Corners of a polyline strictly between two progress values, in order. */
std::vector<Point> cornersBetween(const std::vector<Point> &points, float from, float to) {
    std::vector<Point> result;
    float total = 0;
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (index)
            total += distance(points[index - 1], points[index]);
        if (total > from + .01F && total < to - .01F)
            result.push_back(points[index]);
    }
    return result;
}
/** Count the eruptions of a vent due by a battle time: one at its phase and one per period afterwards. */
int eruptionsBy(float time, float phase, float period) {
    return time + .0001F < phase ? 0 : static_cast<int>(std::floor((time + .0001F - phase) / period)) + 1;
}
} // namespace
const std::vector<Point> &Run::route(std::uint32_t index) const {
    return routes_.at(index);
}
bool Run::detoured(std::size_t fork) const {
    return fork < state_.detours.size() && state_.detours[fork] != 0;
}
std::vector<Point> Run::forkBranch(std::size_t index, bool detour) const {
    const ForkDefinition &fork = catalog_.forks.at(index);
    const std::vector<Point> &base =
        fork.route == 0 ? state_.route : catalog_.additionalRoutes.at(fork.route - 1).points;
    std::vector<Point> branch{fork.split};
    const std::vector<Point> inner =
        detour ? fork.detour
               : cornersBetween(base, progressOf(base, fork.split), progressOf(base, fork.merge));
    branch.insert(branch.end(), inner.begin(), inner.end());
    branch.push_back(fork.merge);
    return branch;
}
Point Run::barricade(std::size_t fork) const {
    const std::vector<Point> closed = forkBranch(fork, !detoured(fork));
    const float gap = distance(closed[0], closed[1]);
    const float step = std::min(1.0F, gap);
    return {closed[0].x + (closed[1].x - closed[0].x) / gap * step,
            closed[0].y + (closed[1].y - closed[0].y) / gap * step};
}
Result Run::switchFork(std::size_t fork) {
    if (fork >= catalog_.forks.size())
        return {false, "갈림길이 없어요"};
    if (state_.phase != Phase::Prep && state_.phase != Phase::Ready)
        return {false, "바리케이드는 정비 중에만 옮길 수 있어요"};
    state_.detours.resize(catalog_.forks.size(), 0);
    state_.detours[fork] = state_.detours[fork] ? 0 : 1;
    resolveTunnels();
    return {true, state_.detours[fork] ? "바리케이드로 원래 길을 막았어요 · 적이 우회로로 와요"
                                       : "바리케이드로 우회로를 막았어요 · 적이 원래 길로 와요"};
}
float Run::routeLength(std::uint32_t lane) const {
    return pathLength(route(lane));
}

Point Run::routePoint(float progress, std::uint32_t lane) const {
    const std::vector<Point> &points = route(lane);
    for (std::size_t index = 1; index < points.size(); ++index) {
        const Point a = points[index - 1], b = points[index];
        const float length = distance(a, b);
        if (progress <= length)
            return {a.x + (b.x - a.x) * progress / length, a.y + (b.y - a.y) * progress / length};
        progress -= length;
    }
    return points.back();
}
std::vector<LaneForecast> Run::laneForecast() const {
    std::vector<LaneForecast> result(catalog_.additionalRoutes.size() + 1);
    for (std::size_t index = 0; index < result.size(); ++index)
        result[index].route = static_cast<std::uint32_t>(index);
    const int wave = state_.phase == Phase::Battle ? state_.wave : state_.wave + 1;
    if (wave > static_cast<int>(catalog_.waves.size()))
        return result;
    const std::vector<Spawn> scheduled = state_.phase == Phase::Battle ? spawns_ : catalog_.schedule(wave);
    for (const Spawn &spawn : scheduled) {
        LaneForecast &lane = result.at(spawn.route);
        ++lane.count;
        ++lane.kinds.at(static_cast<std::size_t>(spawn.kind));
        if (spawn.affix != EliteAffix::None)
            ++lane.elites.at(static_cast<std::size_t>(spawn.affix));
        lane.runners += spawn.kind == EnemyKind::Runner ? 1 : 0;
        lane.armored += spawn.kind == EnemyKind::Armored ? 1 : 0;
    }
    if (state_.phase != Phase::Battle) {
        result[0].count += state_.nextExtra;
        result[0].kinds[static_cast<std::size_t>(EnemyKind::Normal)] += state_.nextExtra;
    }
    return result;
}
float Run::nextArrival(EnemyKind kind) const {
    if (state_.phase != Phase::Battle)
        return -1;
    for (std::size_t index = nextSpawn_; index < spawns_.size(); ++index)
        if (spawns_[index].kind == kind)
            return std::max(0.0F, spawns_[index].time - state_.battleTime);
    return -1;
}
float Run::nextElite(EliteAffix affix) const {
    if (state_.phase != Phase::Battle)
        return -1;
    for (std::size_t index = nextSpawn_; index < spawns_.size(); ++index)
        if (spawns_[index].affix == affix)
            return std::max(0.0F, spawns_[index].time - state_.battleTime);
    return -1;
}
bool Run::burrowed(const Enemy &enemy) const {
    return state_.battleTime < enemy.burrowUntil;
}
void Run::resolveTunnels() {
    const std::size_t lanes = catalog_.additionalRoutes.size() + 1;
    std::vector<std::vector<Point>> base(1, state_.route);
    for (const EntranceRoute &entrance : catalog_.additionalRoutes)
        base.push_back(entrance.points);
    std::vector<RouteSpan> tunnels = catalog_.tunnels, bogs = catalog_.bogs;
    const float added = pathLength(state_.route) - pathLength(catalog_.route);
    for (std::vector<RouteSpan> *spans : {&tunnels, &bogs})
        for (RouteSpan &span : *spans)
            if (span.route == 0) {
                span.enter += added;
                span.exit += added;
            }
    routes_ = base;
    tunnels_ = tunnels;
    bogs_ = bogs;
    /** Send one route through a fork's detour, shifting spans after it and dropping spans inside the stretch.
     */
    const auto detour = [this](std::size_t index, std::uint32_t route) {
        const ForkDefinition &fork = catalog_.forks[index];
        std::vector<Point> &path = routes_.at(route);
        const float split = progressOf(path, fork.split), merge = progressOf(path, fork.merge);
        const std::vector<Point> branch = forkBranch(index, true);
        const float shift = pathLength(branch) - (merge - split);
        std::vector<Point> joined = cornersBetween(path, -1, split);
        joined.insert(joined.end(), branch.begin(), branch.end());
        const std::vector<Point> rest = cornersBetween(path, merge, pathLength(path) + 1);
        joined.insert(joined.end(), rest.begin(), rest.end());
        path = joined;
        for (std::vector<RouteSpan> *spans : {&tunnels_, &bogs_}) {
            spans->erase(std::remove_if(spans->begin(), spans->end(),
                                        [&](const RouteSpan &span) {
                                            return span.route == route && span.enter >= split - .01F &&
                                                   span.exit <= merge + .01F;
                                        }),
                         spans->end());
            for (RouteSpan &span : *spans)
                if (span.route == route && span.enter >= merge - .01F) {
                    span.enter += shift;
                    span.exit += shift;
                }
        }
    };
    state_.detours.resize(catalog_.forks.size(), 0);
    for (std::size_t index = 0; index < catalog_.forks.size(); ++index)
        if (state_.detours[index])
            detour(index, catalog_.forks[index].route);
    // Each fork's closed branch is kept as one more route for enemies rerouted by a broken barricade.
    for (std::size_t index = 0; index < catalog_.forks.size(); ++index) {
        const std::uint32_t lane = catalog_.forks[index].route;
        const std::uint32_t other = static_cast<std::uint32_t>(lanes + index);
        routes_.push_back(base[lane]);
        for (const auto &pair : {std::make_pair(&tunnels, &tunnels_), std::make_pair(&bogs, &bogs_)})
            for (const RouteSpan &span : *pair.first)
                if (span.route == lane)
                    pair.second->push_back({other, span.enter, span.exit});
        if (!state_.detours[index])
            detour(index, other);
    }
}
bool Run::tunnelAt(float progress, std::uint32_t lane) const {
    for (const RouteSpan &span : tunnels_)
        if (span.route == lane && progress >= span.enter && progress < span.exit)
            return true;
    return false;
}
bool Run::tunneled(const Enemy &enemy) const {
    return !flying(enemy) && tunnelAt(enemy.progress, enemy.route);
}
bool Run::flying(const Enemy &enemy) const {
    return catalog_.enemy(enemy.kind).flying;
}
bool Run::reaches(const Enemy &enemy, CardKind attacker) const {
    return !underground(enemy) && !(catalog_.card(attacker).ground && flying(enemy));
}
bool Run::revealed(const Enemy &enemy) const {
    return state_.battleTime < enemy.revealedUntil;
}
bool Run::visibleFrom(const Enemy &enemy, Point position) const {
    const float stealth = catalog_.enemy(enemy.kind).stealth;
    return stealth <= 0 || revealed(enemy) || distance(enemy.position, position) <= stealth;
}
bool Run::bogged(const Enemy &enemy) const {
    if (flying(enemy))
        return false;
    for (const RouteSpan &span : bogs_)
        if (span.route == enemy.route && enemy.progress >= span.enter && enemy.progress < span.exit)
            return true;
    return false;
}
float Run::cartCountdown(std::size_t rail) const {
    if (state_.phase != Phase::Battle || rail >= carts_.size() || carts_[rail].travelled >= 0)
        return -1;
    return carts_[rail].cooldown;
}
float Run::cartProgress(std::size_t rail) const {
    if (rail >= carts_.size() || carts_[rail].travelled < 0)
        return -1;
    const RailDefinition &track = catalog_.rails[rail];
    return std::min(1.0F, carts_[rail].travelled / std::max(.01F, distance(track.from, track.to)));
}
Result Run::launchCart(std::size_t rail) {
    if (state_.phase != Phase::Battle)
        return {false, "광차는 전투 중에만 보낼 수 있어요"};
    if (rail >= carts_.size())
        return {false, "광차 선로가 없어요"};
    CartRun &cart = carts_[rail];
    if (cart.travelled >= 0)
        return {false, "광차가 달리는 중이에요"};
    if (cart.cooldown > .0001F) {
        const int seconds = static_cast<int>(std::ceil(cart.cooldown));
        return {false, "광차가 돌아오는 중이에요 · " + std::to_string(seconds) + "초"};
    }
    cart.travelled = 0;
    cart.struck.clear();
    const RailDefinition &track = catalog_.rails[rail];
    emit({CombatEventKind::Cart, static_cast<EntityId>(rail), track.from, track.to, CardKind::Gun, 0, false,
          catalog_.balance.cartWidth});
    return {true, "광차 출발!"};
}
bool Run::tankReady(std::size_t tank) const {
    return state_.phase == Phase::Battle && tank < tanksSpent_.size() && !tanksSpent_[tank];
}
Result Run::detonateTank(std::size_t tank) {
    if (state_.phase != Phase::Battle)
        return {false, "가스통은 전투 중에만 터뜨릴 수 있어요"};
    if (tank >= tanksSpent_.size())
        return {false, "가스통이 없어요"};
    if (tanksSpent_[tank])
        return {false, "이번 전투에서 이미 터뜨렸어요 · 다음 웨이브에 다시 채워져요"};
    tanksSpent_[tank] = true;
    const TankDefinition &definition = catalog_.tanks[tank];
    const RunBalance &balance = catalog_.balance;
    const std::vector<std::size_t> guards = auraSources();
    int struck = 0;
    for (Enemy &enemy : state_.enemies)
        if (enemy.hp > 0 && !underground(enemy) &&
            distance(enemy.position, definition.position) <= balance.tankRadius) {
            const int damage = damageEnemy(enemy, mitigate(enemy, balance.tankDamage, false, guards));
            emit({CombatEventKind::Damage, enemy.id, definition.position, enemy.position, CardKind::Blast,
                  damage, true});
            ++struck;
        }
    for (Unit &ally : state_.units)
        if (distance(ally.position, definition.position) <= balance.tankRadius) {
            ally.hp -= balance.tankDamage;
            emit({CombatEventKind::Damage, ally.id, definition.position, ally.position, CardKind::Blast,
                  balance.tankDamage, false});
            ++struck;
        }
    emit({CombatEventKind::Detonation, static_cast<EntityId>(tank), definition.position, definition.position,
          CardKind::Blast, struck, false, balance.tankRadius});
    markFallenHero();
    state_.units.erase(std::remove_if(state_.units.begin(), state_.units.end(),
                                      [this](const Unit &ally) {
                                          if (ally.hp > 0)
                                              return false;
                                          emit({CombatEventKind::Defeated, ally.id, ally.position,
                                                ally.position, ally.kind, 0, false});
                                          return true;
                                      }),
                       state_.units.end());
    for (Enemy &enemy : state_.enemies)
        if (enemy.blocker && !unit(enemy.blocker))
            enemy.blocker = 0;
    removeDefeated();
    return {true, "가스통 폭발!"};
}
void Run::resetMachines() {
    carts_.assign(catalog_.rails.size(), CartRun{catalog_.balance.cartFirst, -1, {}});
    tanksSpent_.assign(catalog_.tanks.size(), false);
    barricadeHp_.assign(catalog_.forks.size(), catalog_.balance.barricadeHp);
}
int Run::squadSize() const {
    return static_cast<int>(
        std::count_if(state_.units.begin(), state_.units.end(), [](const Unit &ally) { return !ally.hero; }));
}
const Unit *Run::hero() const {
    for (const Unit &ally : state_.units)
        if (ally.hero)
            return &ally;
    return nullptr;
}
void Run::markFallenHero() {
    for (const Unit &ally : state_.units)
        if (ally.hero && ally.hp <= 0) {
            state_.heroDown = catalog_.balance.heroRespawn;
            state_.heroCell = ally.position;
        }
}
void Run::reviveHero() {
    state_.heroDown = 0;
    if (hero())
        return;
    Point cell = state_.heroCell;
    const auto taken = [this](Point point) {
        return std::any_of(state_.units.begin(), state_.units.end(),
                           [point](const Unit &ally) { return distance(ally.position, point) < .15F; });
    };
    if (taken(cell)) {
        float best = 1e9F;
        for (Point candidate : state_.unitCells)
            if (!taken(candidate) && distance(candidate, state_.heroCell) < best) {
                best = distance(candidate, state_.heroCell);
                cell = candidate;
            }
    }
    state_.units.push_back(
        {nextEntity_++, CardKind::Guard, cell, catalog_.balance.heroHp, catalog_.balance.heroHp});
    state_.units.back().hero = true;
    emit({CombatEventKind::Placement, state_.units.back().id, cell, cell, CardKind::Guard, 1, false});
}
int Run::barricadeHp(std::size_t fork) const {
    return fork < barricadeHp_.size() ? barricadeHp_[fork] : 0;
}
std::uint32_t Run::laneOf(std::uint32_t route) const {
    const std::size_t lanes = catalog_.additionalRoutes.size() + 1;
    return route < lanes ? route : catalog_.forks.at(route - lanes).route;
}
void Run::breach(std::size_t fork) {
    const ForkDefinition &definition = catalog_.forks[fork];
    const std::uint32_t lane = definition.route;
    const std::uint32_t other = static_cast<std::uint32_t>(catalog_.additionalRoutes.size() + 1 + fork);
    emit({CombatEventKind::Breach, static_cast<EntityId>(fork), barricade(fork), barricade(fork),
          CardKind::Gun, 0, false});
    if (routeLength(other) >= routeLength(lane))
        return;
    const float split = progressOf(route(lane), definition.split);
    for (Enemy &enemy : state_.enemies)
        if (enemy.route == lane && enemy.progress < split - .01F)
            enemy.route = other;
}
bool Run::underground(const Enemy &enemy) const {
    return burrowed(enemy) || tunneled(enemy);
}
PadTrait Run::padTrait(Point position) const {
    for (const TraitPad &pad : catalog_.padTraits)
        if (distance(pad.position, position) < .15F)
            return pad.trait;
    return PadTrait::None;
}
float Run::buildingRange(CardKind kind, Point position) const {
    return catalog_.card(kind).range *
           (padTrait(position) == PadTrait::HighGround ? catalog_.balance.highGroundRange : 1.0F);
}
float Run::padDamage(Point position) const {
    return padTrait(position) == PadTrait::Crystal ? catalog_.balance.crystalDamage : 1.0F;
}
float Run::ventCountdown(std::size_t vent) const {
    if (state_.phase != Phase::Battle || vent >= catalog_.vents.size())
        return -1;
    const float period = catalog_.balance.ventPeriod;
    const float elapsed = state_.battleTime - catalog_.vents[vent].phase;
    return elapsed < 0 ? -elapsed : std::max(0.0F, period - std::fmod(elapsed + .0001F, period));
}
bool Run::ventThreatens(Point position) const {
    for (const VentDefinition &vent : catalog_.vents)
        if (distance(vent.position, position) <= catalog_.balance.ventRadius)
            return true;
    return false;
}
Enemy Run::makeEnemy(EnemyKind kind, std::uint32_t lane, float progress, EliteAffix affix) {
    const EnemyDefinition &stats = catalog_.enemy(kind);
    Enemy enemy{nextEntity_++, kind, routePoint(progress, lane), progress, stats.hp, 0, 0, 0, lane};
    enemy.affix = affix;
    const float vigor = kind == EnemyKind::Boss ? 1.0F : catalog_.waveVigor(state_.wave);
    enemy.maxHp = std::max(1, static_cast<int>(std::lround(catalog_.health(enemy) * vigor)));
    enemy.hp = enemy.maxHp;
    enemy.maxShield = static_cast<int>(std::lround(stats.shield * vigor));
    enemy.shield = enemy.maxShield;
    enemy.ability = stats.ability == EnemyAbility::Leap ? stats.abilityWindup : stats.abilityInterval;
    enemy.charges = stats.ability == EnemyAbility::Sabotage || stats.ability == EnemyAbility::Leap
                        ? stats.abilityAmount
                    : stats.ability == EnemyAbility::Airdrop ? 1
                                                             : 0;
    enemy.pulse = 1;
    return enemy;
}
std::vector<std::size_t> Run::auraSources() const {
    std::vector<std::size_t> sources;
    for (std::size_t index = 0; index < state_.enemies.size(); ++index) {
        const Enemy &enemy = state_.enemies[index];
        if (enemy.affix != EliteAffix::None && enemy.hp > 0 && !underground(enemy) &&
            catalog_.elite(enemy.affix).auraRadius > 0)
            sources.push_back(index);
    }
    return sources;
}
float Run::auraSpeed(const Enemy &enemy, const std::vector<std::size_t> &sources) const {
    float factor = 1;
    for (std::size_t index : sources) {
        const Enemy &source = state_.enemies[index];
        const EliteDefinition &elite = catalog_.elite(source.affix);
        if (source.id != enemy.id && distance(source.position, enemy.position) <= elite.auraRadius)
            factor = std::max(factor, elite.auraSpeed);
    }
    return factor;
}
int Run::mitigate(const Enemy &enemy, int damage, bool electric,
                  const std::vector<std::size_t> &sources) const {
    const int amount = electric ? damage : std::max(1, damage - catalog_.armor(enemy));
    float guard = 0;
    for (std::size_t index : sources) {
        const Enemy &source = state_.enemies[index];
        const EliteDefinition &elite = catalog_.elite(source.affix);
        if (source.id != enemy.id && distance(source.position, enemy.position) <= elite.auraRadius)
            guard = std::max(guard, elite.auraGuard);
    }
    return guard > 0 ? std::max(1, static_cast<int>(std::lround(amount * (1 - guard)))) : amount;
}
void Run::advanceSabotage(Enemy &sapper, float delta) {
    const EnemyDefinition &stats = catalog_.enemy(sapper.kind);
    Tower *target = nullptr;
    for (Tower &tower : state_.towers)
        if (tower.id == sapper.sabotage)
            target = &tower;
    // A blocked sapper, or one whose tower was withdrawn, abandons the charge and looks again later.
    if (sapper.sabotage && (!target || sapper.blocker || underground(sapper))) {
        sapper.sabotage = 0;
        sapper.ability = stats.abilityInterval;
        return;
    }
    if (target) {
        sapper.ability -= delta;
        if (sapper.ability > 0.0001F)
            return;
        target->disabled = std::max(target->disabled, stats.abilityDuration);
        emit({CombatEventKind::Disable, target->id, sapper.position, target->position, target->kind, 0, false,
              stats.abilityDuration, sapper.kind, sapper.affix});
        sapper.sabotage = 0;
        --sapper.charges;
        sapper.ability = stats.abilityInterval;
        return;
    }
    if (sapper.charges <= 0 || sapper.blocker || underground(sapper))
        return;
    sapper.ability = std::max(0.0F, sapper.ability - delta);
    if (sapper.ability > 0.0001F)
        return;
    Tower *best = nullptr;
    float closest = 0;
    for (Tower &tower : state_.towers) {
        const float gap = distance(tower.position, sapper.position);
        if (tower.disabled > 0 || gap > stats.abilityRadius)
            continue;
        const bool claimed = std::any_of(state_.enemies.begin(), state_.enemies.end(),
                                         [&tower](const Enemy &other) { return other.sabotage == tower.id; });
        if (!claimed && (!best || gap < closest || (gap == closest && tower.id < best->id))) {
            best = &tower;
            closest = gap;
        }
    }
    if (!best)
        return;
    sapper.sabotage = best->id;
    sapper.ability = stats.abilityWindup;
    emit({CombatEventKind::Sabotage, sapper.id, sapper.position, best->position, best->kind, 0, true,
          stats.abilityWindup, sapper.kind, sapper.affix});
}
void Run::advanceSiege(Enemy &wrecker, float delta) {
    const EnemyDefinition &stats = catalog_.enemy(wrecker.kind);
    // The target is a building or, above BarricadeBase, a standing barricade.
    const auto barricadeOf = [this](EntityId id) -> int {
        return id >= BarricadeBase && id - BarricadeBase < barricadeHp_.size() &&
                       barricadeHp_[id - BarricadeBase] > 0
                   ? static_cast<int>(id - BarricadeBase)
                   : -1;
    };
    Tower *target = nullptr;
    for (Tower &tower : state_.towers)
        if (tower.id == wrecker.sabotage)
            target = &tower;
    int fence = wrecker.sabotage ? barricadeOf(wrecker.sabotage) : -1;
    // A blocked or buried attacker, or one whose target is gone, lets go and walks on.
    if (wrecker.sabotage && ((!target && fence < 0) || wrecker.blocker || underground(wrecker))) {
        wrecker.sabotage = 0;
        target = nullptr;
        fence = -1;
    }
    if (!target && fence < 0) {
        if (wrecker.blocker || underground(wrecker))
            return;
        float closest = 0;
        EntityId chosen = 0;
        for (Tower &tower : state_.towers) {
            const float gap = distance(tower.position, wrecker.position);
            if (gap <= stats.abilityRadius &&
                (!chosen || gap < closest || (gap == closest && tower.id < chosen))) {
                chosen = tower.id;
                closest = gap;
            }
        }
        for (std::size_t fork = 0; fork < barricadeHp_.size(); ++fork) {
            const float gap = distance(barricade(fork), wrecker.position);
            if (barricadeHp_[fork] > 0 && gap <= stats.abilityRadius && (!chosen || gap < closest)) {
                chosen = BarricadeBase + static_cast<EntityId>(fork);
                closest = gap;
            }
        }
        if (!chosen)
            return;
        wrecker.sabotage = chosen;
        wrecker.ability = stats.abilityInterval;
        return;
    }
    wrecker.ability -= delta;
    if (wrecker.ability > 0.0001F)
        return;
    wrecker.ability += stats.abilityInterval;
    if (fence >= 0) {
        const std::size_t fork = static_cast<std::size_t>(fence);
        barricadeHp_[fork] -= stats.abilityAmount;
        emit({CombatEventKind::Batter, wrecker.sabotage, wrecker.position, barricade(fork), CardKind::Gun,
              stats.abilityAmount, false, 0, wrecker.kind, wrecker.affix});
        if (barricadeHp_[fork] <= 0) {
            barricadeHp_[fork] = 0;
            wrecker.sabotage = 0;
            breach(fork);
        }
        return;
    }
    target->hp -= stats.abilityAmount;
    emit({CombatEventKind::Batter, target->id, wrecker.position, target->position, target->kind,
          stats.abilityAmount, false, 0, wrecker.kind, wrecker.affix});
    if (target->hp > 0)
        return;
    const EntityId id = target->id;
    const Point at = target->position;
    const CardKind kind = target->kind;
    state_.towers.erase(std::remove_if(state_.towers.begin(), state_.towers.end(),
                                       [id](const Tower &item) { return item.id == id; }),
                        state_.towers.end());
    wrecker.sabotage = 0;
    emit({CombatEventKind::Wreck, id, at, at, kind, 0, false, 0, wrecker.kind, wrecker.affix});
}
void Run::advanceElites(float delta) {
    for (Enemy &enemy : state_.enemies) {
        if (enemy.affix == EliteAffix::None || enemy.hp <= 0)
            continue;
        const EliteDefinition &elite = catalog_.elite(enemy.affix);
        const int maximum = catalog_.health(enemy);
        if (elite.regen > 0) {
            enemy.pulse -= delta;
            if (enemy.pulse <= 0.0001F) {
                enemy.pulse += 1;
                const int amount = std::min(std::max(1, static_cast<int>(std::lround(maximum * elite.regen))),
                                            maximum - enemy.hp);
                if (amount > 0) {
                    enemy.hp += amount;
                    emit({CombatEventKind::Heal, enemy.id, enemy.position, enemy.position, CardKind::Gun,
                          amount, true, 0, enemy.kind, enemy.affix});
                }
            }
        }
        if (elite.enrageBelow > 0 && !enemy.enraged && enemy.hp <= maximum * elite.enrageBelow) {
            enemy.enraged = true;
            emit({CombatEventKind::Enrage, enemy.id, enemy.position, enemy.position, CardKind::Gun, 0, true,
                  0, enemy.kind, enemy.affix});
        }
    }
}
void Run::advanceAbilities(float delta, std::vector<Enemy> &arrivals) {
    for (Enemy &enemy : state_.enemies) {
        const EnemyDefinition &stats = catalog_.enemy(enemy.kind);
        if (stats.ability == EnemyAbility::Leap && enemy.hp > 0) {
            // The windup runs only while a unit blocks the leaper; the jump clears the block.
            if (!enemy.blocker || enemy.charges <= 0)
                continue;
            enemy.ability -= delta;
            if (enemy.ability > 0.0001F)
                continue;
            const Point from = enemy.position;
            enemy.progress = std::min(routeLength(enemy.route), enemy.progress + stats.abilityRadius);
            enemy.position = routePoint(enemy.progress, enemy.route);
            enemy.blocker = 0;
            enemy.ability = stats.abilityWindup;
            --enemy.charges;
            emit({CombatEventKind::Leap, enemy.id, from, enemy.position, CardKind::Gun, 0, true,
                  stats.abilityRadius, enemy.kind, enemy.affix});
            continue;
        }
        if (stats.ability == EnemyAbility::Airdrop) {
            // The cargo lands behind the carrier, once, after its share of the route.
            if (enemy.hp > 0 && enemy.charges > 0 &&
                enemy.progress >= routeLength(enemy.route) * stats.abilityRadius) {
                --enemy.charges;
                for (int index = 0; index < stats.abilityAmount; ++index)
                    arrivals.push_back(
                        makeEnemy(stats.minionKind, enemy.route,
                                  std::max(0.0F, enemy.progress - .35F * static_cast<float>(index))));
                emit({CombatEventKind::Airdrop, enemy.id, enemy.position, enemy.position, CardKind::Gun,
                      stats.abilityAmount, true, 0, enemy.kind, enemy.affix});
            }
            continue;
        }
        if (stats.ability == EnemyAbility::None || stats.abilityInterval <= 0 || enemy.hp <= 0)
            continue;
        if (stats.ability == EnemyAbility::Sabotage) {
            advanceSabotage(enemy, delta);
            continue;
        }
        if (stats.ability == EnemyAbility::Siege) {
            advanceSiege(enemy, delta);
            continue;
        }
        // Ability timers stand still inside a tunnel, so nothing activates out of sight.
        if (tunneled(enemy))
            continue;
        enemy.ability -= delta;
        if (enemy.ability > 0.0001F)
            continue;
        enemy.ability += stats.abilityInterval;
        if (stats.ability == EnemyAbility::Burrow) {
            enemy.burrowUntil = state_.battleTime + stats.abilityDuration;
            enemy.blocker = 0;
            emit({CombatEventKind::Burrow, enemy.id, enemy.position, enemy.position, CardKind::Gun, 0, true,
                  0, enemy.kind});
        } else if (stats.ability == EnemyAbility::Heal) {
            int healed = 0;
            for (Enemy &other : state_.enemies) {
                const int maximum = catalog_.health(other);
                if (&other == &enemy || other.hp <= 0 || other.hp >= maximum || underground(other) ||
                    distance(other.position, enemy.position) > stats.abilityRadius)
                    continue;
                const int amount = std::min(stats.abilityAmount, maximum - other.hp);
                other.hp += amount;
                ++healed;
                emit({CombatEventKind::Heal, other.id, enemy.position, other.position, CardKind::Gun, amount,
                      true, 0, other.kind});
            }
            if (healed)
                emit({CombatEventKind::Heal, enemy.id, enemy.position, enemy.position, CardKind::Gun, 0, true,
                      stats.abilityRadius, enemy.kind});
        } else if (stats.ability == EnemyAbility::Stomp) {
            for (Tower &tower : state_.towers)
                if (distance(tower.position, enemy.position) <= stats.abilityRadius)
                    tower.cooldown = std::max(tower.cooldown, stats.abilityDuration);
            for (int index = 0; index < stats.abilityAmount; ++index)
                arrivals.push_back(makeEnemy(stats.minionKind, enemy.route,
                                             std::max(0.0F, enemy.progress - .35F * (index + 1))));
            emit({CombatEventKind::Stomp, enemy.id, enemy.position, enemy.position, CardKind::Gun,
                  stats.abilityAmount, true, stats.abilityRadius, enemy.kind});
        }
    }
}
int Run::damageEnemy(Enemy &enemy, int amount) {
    if (catalog_.enemy(enemy.kind).stealth > 0)
        enemy.revealedUntil = state_.battleTime + catalog_.balance.revealTime;
    if (enemy.shield <= 0) {
        enemy.hp -= amount;
        return amount;
    }
    const int cap = catalog_.enemy(enemy.kind).shieldCap;
    const int capped = cap > 0 ? std::min(amount, cap) : amount;
    const int absorbed = std::min(capped, enemy.shield);
    enemy.shield -= absorbed;
    enemy.hp -= capped - absorbed;
    if (enemy.shield == 0)
        emit({CombatEventKind::ShieldBreak, enemy.id, enemy.position, enemy.position, CardKind::Gun, 0, true,
              0, enemy.kind});
    return capped;
}
void Run::removeDefeated() {
    std::vector<Enemy> minions;
    state_.enemies.erase(
        std::remove_if(state_.enemies.begin(), state_.enemies.end(),
                       [this, &minions](const Enemy &enemy) {
                           if (enemy.hp > 0)
                               return false;
                           ++state_.defeated;
                           emit({CombatEventKind::Defeated, enemy.id, enemy.position, enemy.position,
                                 CardKind::Gun, 0, true, 0, enemy.kind, enemy.affix});
                           const EnemyDefinition &stats = catalog_.enemy(enemy.kind);
                           const float length = routeLength(enemy.route);
                           for (int index = 0; index < stats.splitCount; ++index) {
                               const float offset = (index - (stats.splitCount - 1) / 2.0F) * .3F;
                               minions.push_back(
                                   makeEnemy(stats.minionKind, enemy.route,
                                             std::clamp(enemy.progress + offset, 0.0F, length)));
                           }
                           if (stats.splitCount > 0)
                               emit({CombatEventKind::Split, enemy.id, enemy.position, enemy.position,
                                     CardKind::Gun, stats.splitCount, true, 0, enemy.kind});
                           return true;
                       }),
        state_.enemies.end());
    state_.enemies.insert(state_.enemies.end(), minions.begin(), minions.end());
}

void Run::advanceBattle(float delta) {
    std::vector<float> lengths;
    for (std::size_t index = 0; index < routes_.size(); ++index)
        lengths.push_back(routeLength(static_cast<std::uint32_t>(index)));
    const float previousTime = state_.battleTime;
    state_.battleTime += delta;
    state_.heroMove = std::max(0.0F, state_.heroMove - delta);
    if (state_.heroDown > 0) {
        state_.heroDown -= delta;
        if (state_.heroDown <= 0)
            reviveHero();
    }
    if (state_.energy < catalog_.balance.energyCap) {
        state_.energyTime += delta;
        if (state_.energyTime + 0.0001F >= catalog_.balance.energyInterval) {
            state_.energyTime = std::max(0.0F, state_.energyTime - catalog_.balance.energyInterval);
            ++state_.energy;
            if (state_.energy == catalog_.balance.energyCap)
                state_.energyTime = 0;
        }
    }
    while (nextSpawn_ < spawns_.size() && spawns_[nextSpawn_].time <= state_.battleTime + 0.0001F) {
        const Spawn spawn = spawns_[nextSpawn_++];
        std::uint32_t entry = spawn.route;
        for (std::size_t fork = 0; fork < catalog_.forks.size(); ++fork) {
            const std::uint32_t other =
                static_cast<std::uint32_t>(catalog_.additionalRoutes.size() + 1 + fork);
            if (catalog_.forks[fork].route == spawn.route && barricadeHp_[fork] <= 0 &&
                routeLength(other) < routeLength(spawn.route))
                entry = other;
        }
        state_.enemies.push_back(makeEnemy(spawn.kind, entry, 0, spawn.affix));
        ++state_.spawned;
    }
    applyGroundFields(state_, *this);
    std::vector<Enemy> arrivals;
    advanceAbilities(delta, arrivals);
    advanceElites(delta);
    const std::vector<std::size_t> hasteSources = auraSources();
    for (Enemy &enemy : state_.enemies) {
        enemy.cooldown = std::max(0.0F, enemy.cooldown - delta);
        const bool burrow = burrowed(enemy), tunnel = tunneled(enemy);
        if (burrow || tunnel || (enemy.blocker && !unit(enemy.blocker)))
            enemy.blocker = 0;
        if (!enemy.blocker && !enemy.sabotage) {
            const EnemyDefinition &stats = catalog_.enemy(enemy.kind);
            const EliteDefinition &elite = catalog_.elite(enemy.affix);
            /** Burrowing and tunnels do not stack; the faster of the two applies. */
            const float cover =
                std::max(burrow ? stats.abilitySpeed : 1.0F, tunnel ? catalog_.balance.tunnelSpeed : 1.0F);
            enemy.progress +=
                stats.speed * (state_.battleTime < enemy.slowUntil ? catalog_.balance.slowFactor : 1.0F) *
                cover * (bogged(enemy) ? catalog_.balance.bogSpeed : 1.0F) * elite.speed *
                (enemy.enraged ? elite.enrageSpeed : 1.0F) * auraSpeed(enemy, hasteSources) * delta;
            enemy.position = routePoint(enemy.progress, enemy.route);
        }
    }
    state_.enemies.insert(state_.enemies.end(), arrivals.begin(), arrivals.end());
    for (Unit &ally : state_.units) {
        ally.cooldown = std::max(0.0F, ally.cooldown - delta);
        int blocked = 0;
        for (const Enemy &enemy : state_.enemies)
            if (enemy.blocker == ally.id)
                ++blocked;
        if (!blocked && ally.hp < ally.maxHp) {
            ally.mending +=
                ally.maxHp * (ally.hero ? catalog_.balance.heroRegen : catalog_.balance.unitRegen) * delta;
            const int whole = static_cast<int>(ally.mending);
            ally.hp = std::min(ally.maxHp, ally.hp + whole);
            ally.mending -= static_cast<float>(whole);
        } else if (ally.hp >= ally.maxHp)
            ally.mending = 0;
        for (Enemy &enemy : state_.enemies)
            if (!enemy.blocker && !underground(enemy) && !flying(enemy) &&
                blocked < (ally.hero ? catalog_.balance.heroBlockers : catalog_.card(ally.kind).blockers) &&
                distance(enemy.position, ally.position) <= 0.35F) {
                enemy.blocker = ally.id;
                ++blocked;
            }
    }
    /** Spawn insertion is complete; batch-local indices stay valid until both death passes.
     *  Deferring every erase preserves simultaneous attacks, overkill and mutual death. */
    struct Hit {
        std::size_t targetIndex;
        int amount;
        bool enemy;
        CardKind card;
        Point origin;
    };
    std::vector<Hit> hits;
    const std::vector<std::size_t> guardSources = auraSources();
    /** Select the closest-to-base living enemy, optionally preferring an ally's blockers. */
    const auto targetFor = [this, &lengths](Point position, float range, EntityId blocker,
                                            CardKind attacker) -> Enemy * {
        Enemy *best = nullptr;
        for (Enemy &enemy : state_.enemies)
            if (enemy.hp > 0 && reaches(enemy, attacker) && visibleFrom(enemy, position) &&
                distance(position, enemy.position) <= range) {
                const bool preferred = blocker && enemy.blocker == blocker;
                const bool bestPreferred = best && blocker && best->blocker == blocker;
                if (!best || (preferred && !bestPreferred) ||
                    (preferred == bestPreferred &&
                     (lengths[enemy.route] - enemy.progress < lengths[best->route] - best->progress ||
                      (lengths[enemy.route] - enemy.progress == lengths[best->route] - best->progress &&
                       enemy.id < best->id))))
                    best = &enemy;
            }
        return best;
    };
    /** Select the highest-HP enemy in range, breaking ties by least distance remaining to the base. */
    const auto strongestFor = [this, &lengths](Point position, float range, CardKind attacker) -> Enemy * {
        Enemy *best = nullptr;
        for (Enemy &enemy : state_.enemies)
            if (enemy.hp > 0 && reaches(enemy, attacker) && visibleFrom(enemy, position) &&
                distance(position, enemy.position) <= range) {
                const float remaining = lengths[enemy.route] - enemy.progress;
                if (!best || enemy.hp > best->hp ||
                    (enemy.hp == best->hp && remaining < lengths[best->route] - best->progress))
                    best = &enemy;
            }
        return best;
    };
    /** Record one physical or electric hit without changing the target during selection. */
    const auto hit = [this, &hits, &guardSources](const Enemy &enemy, int damage, bool electric,
                                                  CardKind card, Point origin) {
        hits.push_back({static_cast<std::size_t>(&enemy - state_.enemies.data()),
                        mitigate(enemy, damage, electric, guardSources), true, card, origin});
    };
    for (Tower &tower : state_.towers) {
        tower.cooldown = std::max(0.0F, tower.cooldown - delta);
        tower.disabled = std::max(0.0F, tower.disabled - delta);
        if (tower.cooldown > 0 || tower.disabled > 0)
            continue;
        const CardDefinition &definition = catalog_.card(tower.kind);
        if (definition.aura > 0)
            continue;
        const float range = buildingRange(tower.kind, tower.position);
        Enemy *target = tower.kind == CardKind::Sniper ? strongestFor(tower.position, range, tower.kind)
                                                       : targetFor(tower.position, range, 0, tower.kind);
        if (!target)
            continue;
        const float factor = (1.0F + (tower.level - 1) * catalog_.balance.levelBonus) *
                             (1.0F + auraBonus(*this, tower)) * padDamage(tower.position);
        const int damage = static_cast<int>(std::floor(
            definition.damage * factor *
            (tower.kind == CardKind::Lightning && state_.coil ? catalog_.balance.coilBonus : 1.0F)));
        if (definition.arc > 0) {
            for (const Enemy &enemy : state_.enemies)
                if (enemy.hp > 0 && reaches(enemy, tower.kind) &&
                    distance(enemy.position, tower.position) <= range &&
                    insideCone(tower.position, target->position, enemy.position, definition.arc))
                    hit(enemy, damage, false, tower.kind, tower.position);
            tower.cooldown = definition.interval;
        } else if (tower.kind == CardKind::Cannon) {
            for (const Enemy &enemy : state_.enemies)
                if (reaches(enemy, tower.kind) &&
                    distance(enemy.position, target->position) <= definition.splash)
                    hit(enemy, damage, false, tower.kind, tower.position);
            tower.cooldown = definition.interval;
        } else if (tower.kind == CardKind::Lightning) {
            std::vector<EntityId> chained;
            Enemy *current = target;
            for (int count = 0; count < definition.chains && current; ++count) {
                hit(*current, damage, true, tower.kind, tower.position);
                chained.push_back(current->id);
                Enemy *next = nullptr;
                float closest = definition.chainRange + .0001F;
                for (Enemy &candidate : state_.enemies)
                    if (!underground(candidate) &&
                        std::find(chained.begin(), chained.end(), candidate.id) == chained.end()) {
                        const float gap = distance(current->position, candidate.position);
                        if (gap <= definition.chainRange &&
                            (gap < closest || (gap == closest && next && candidate.id < next->id))) {
                            closest = gap;
                            next = &candidate;
                        }
                    }
                current = next;
            }
            tower.cooldown = definition.interval;
        } else {
            hit(*target, damage, false, tower.kind, tower.position);
            tower.cooldown = definition.interval;
        }
        emit({CombatEventKind::Attack, tower.id, tower.position, target->position, tower.kind, 0, false,
              definition.splash});
    }
    for (Unit &ally : state_.units) {
        if (ally.cooldown <= 0) {
            const RunBalance &balance = catalog_.balance;
            Enemy *target =
                targetFor(ally.position, ally.hero ? balance.heroRange : catalog_.card(ally.kind).range,
                          ally.id, ally.kind);
            if (target && ally.hero) {
                hit(*target, balance.heroDamage, false, ally.kind, ally.position);
                ally.cooldown = balance.heroInterval;
                emit({CombatEventKind::Attack, ally.id, ally.position, target->position, ally.kind, 0, false,
                      0});
            } else if (target) {
                const CardDefinition &definition = catalog_.card(ally.kind);
                if (ally.kind == CardKind::Demolition) {
                    for (const Enemy &enemy : state_.enemies)
                        if (reaches(enemy, ally.kind) &&
                            distance(enemy.position, target->position) <= definition.splash)
                            hit(enemy, definition.damage, false, ally.kind, ally.position);
                } else
                    hit(*target, definition.damage, false, ally.kind, ally.position);
                ally.cooldown = definition.interval;
                emit({CombatEventKind::Attack, ally.id, ally.position, target->position, ally.kind, 0, false,
                      definition.splash});
            }
        }
    }
    for (Enemy &enemy : state_.enemies)
        if (enemy.blocker && enemy.cooldown <= 0) {
            const EnemyDefinition &stats = catalog_.enemy(enemy.kind);
            const Unit *target = unit(enemy.blocker);
            if (!target) {
                enemy.blocker = 0;
                continue;
            }
            hits.push_back({static_cast<std::size_t>(target - state_.units.data()), stats.attack, false,
                            CardKind::Guard, enemy.position});
            enemy.cooldown = stats.interval;
            emit({CombatEventKind::Attack, enemy.id, enemy.position,
                  target ? target->position : enemy.position, CardKind::Guard, 0, true, 0, enemy.kind});
        }
    /** A vent due this step hits every surfaced enemy, ignoring armor, and every allied unit in its radius.
     */
    for (std::size_t index = 0; index < catalog_.vents.size(); ++index) {
        const VentDefinition &vent = catalog_.vents[index];
        const RunBalance &balance = catalog_.balance;
        if (eruptionsBy(state_.battleTime, vent.phase, balance.ventPeriod) ==
            eruptionsBy(previousTime, vent.phase, balance.ventPeriod))
            continue;
        int struck = 0;
        for (const Enemy &enemy : state_.enemies)
            if (!underground(enemy) && !flying(enemy) &&
                distance(enemy.position, vent.position) <= balance.ventRadius) {
                hit(enemy, balance.ventDamage, true, CardKind::Gun, vent.position);
                ++struck;
            }
        for (std::size_t ally = 0; ally < state_.units.size(); ++ally)
            if (distance(state_.units[ally].position, vent.position) <= balance.ventRadius) {
                hits.push_back({ally, balance.ventDamage, false, CardKind::Gun, vent.position});
                ++struck;
            }
        emit({CombatEventKind::Eruption, static_cast<EntityId>(index), vent.position, vent.position,
              CardKind::Gun, struck, false, balance.ventRadius});
    }
    /** A running cart hits every surfaced ground enemy it passes this step, each once per run. */
    for (std::size_t index = 0; index < carts_.size(); ++index) {
        CartRun &cart = carts_[index];
        const RailDefinition &track = catalog_.rails[index];
        const RunBalance &balance = catalog_.balance;
        if (cart.travelled < 0) {
            cart.cooldown = cart.cooldown - delta > .0001F ? cart.cooldown - delta : 0.0F;
            continue;
        }
        const float length = std::max(.01F, distance(track.from, track.to));
        const auto along = [&track, length](float travelled) -> Point {
            const float t = std::min(1.0F, travelled / length);
            return {track.from.x + (track.to.x - track.from.x) * t,
                    track.from.y + (track.to.y - track.from.y) * t};
        };
        const Point start = along(cart.travelled);
        cart.travelled += balance.cartSpeed * delta;
        const Point end = along(cart.travelled);
        for (const Enemy &enemy : state_.enemies)
            if (!underground(enemy) && !flying(enemy) &&
                std::find(cart.struck.begin(), cart.struck.end(), enemy.id) == cart.struck.end() &&
                segmentDistance(enemy.position, start, end) <= balance.cartWidth) {
                cart.struck.push_back(enemy.id);
                hit(enemy, balance.cartDamage, false, CardKind::Cannon, end);
            }
        if (cart.travelled >= length) {
            cart.travelled = -1;
            cart.cooldown = balance.cartCooldown;
        }
    }
    for (const Hit &damage : hits) {
        if (damage.enemy) {
            Enemy &enemy = state_.enemies[damage.targetIndex];
            const int amount = damageEnemy(enemy, damage.amount);
            emit({CombatEventKind::Damage, enemy.id, damage.origin, enemy.position, damage.card, amount,
                  true});
        } else {
            Unit &ally = state_.units[damage.targetIndex];
            ally.hp -= damage.amount;
            emit({CombatEventKind::Damage, ally.id, damage.origin, ally.position, damage.card, damage.amount,
                  false});
        }
    }
    markFallenHero();
    state_.units.erase(std::remove_if(state_.units.begin(), state_.units.end(),
                                      [this](const Unit &ally) {
                                          if (ally.hp > 0)
                                              return false;
                                          emit({CombatEventKind::Defeated, ally.id, ally.position,
                                                ally.position, ally.kind, 0, false});
                                          return true;
                                      }),
                       state_.units.end());
    removeDefeated();
    state_.enemies.erase(std::remove_if(state_.enemies.begin(), state_.enemies.end(),
                                        [this, &lengths](const Enemy &enemy) {
                                            if (enemy.progress >= lengths[enemy.route]) {
                                                const int amount = catalog_.enemy(enemy.kind).leak +
                                                                   catalog_.elite(enemy.affix).leak;
                                                state_.baseHp -= amount;
                                                emit({CombatEventKind::Leak, enemy.id, enemy.position,
                                                      state_.route.back(), CardKind::Gun, amount, true, 0,
                                                      enemy.kind, enemy.affix});
                                                return true;
                                            }
                                            return false;
                                        }),
                         state_.enemies.end());
    if (state_.baseHp <= 0) {
        state_.baseHp = 0;
        state_.phase = Phase::Lost;
        return;
    }
    if (nextSpawn_ == spawns_.size() && state_.enemies.empty()) {
        for (Tower &tower : state_.towers) {
            tower.disabled = 0;
            tower.hp = std::min(tower.maxHp, tower.hp + static_cast<int>(std::lround(
                                                            tower.maxHp * catalog_.balance.towerRepair)));
        }
        finishWave();
    }
}

Result Run::moveUnit(EntityId id, Point target) {
    const std::optional<Point> cell = resolveCell(state_.unitCells, target);
    Unit *selected = nullptr;
    for (Unit &ally : state_.units)
        if (ally.id == id)
            selected = &ally;
    const bool charge = selected && selected->hero && state_.phase == Phase::Battle;
    if (charge && state_.heroMove > 0)
        return {false, "영웅은 " + std::to_string(static_cast<int>(std::ceil(state_.heroMove))) +
                           "초 뒤 다시 움직여요"};
    if ((state_.phase != Phase::Prep && !charge) || !cell)
        return {false, "정비 중 배치 칸으로 옮겨 주세요"};
    target = *cell;
    if (!selected)
        return {false, "유닛을 선택해 주세요"};
    if (charge) {
        state_.heroMove = catalog_.balance.heroMove;
        for (Enemy &enemy : state_.enemies)
            if (enemy.blocker == id)
                enemy.blocker = 0;
    }
    for (Unit &ally : state_.units)
        if (ally.id != id && distance(ally.position, target) < 0.15F)
            ally.position = selected->position;
    selected->position = target;
    return {true, "체력과 슬롯을 유지하며 무료로 이동했어요"};
}

Result Run::dismiss(EntityId id) {
    if (state_.phase != Phase::Battle && state_.phase != Phase::Prep)
        return {false, "전투 또는 정비 중에 철수할 수 있어요"};
    const bool building = tower(id) != nullptr;
    if (!building && !unit(id))
        return {false, "이미 철수했거나 존재하지 않는 대상이에요"};
    if (!building && unit(id)->hero)
        return {false, "영웅은 철수하지 않아요"};
    state_.units.erase(std::remove_if(state_.units.begin(), state_.units.end(),
                                      [id](const Unit &ally) { return ally.id == id; }),
                       state_.units.end());
    state_.towers.erase(std::remove_if(state_.towers.begin(), state_.towers.end(),
                                       [id](const Tower &item) { return item.id == id; }),
                        state_.towers.end());
    for (Enemy &enemy : state_.enemies)
        if (enemy.blocker == id)
            enemy.blocker = 0;
    return {true, building ? "건물을 철거했어요. 사용한 비용은 반환되지 않아요"
                           : "유닛을 철수했어요. 슬롯은 돌아오고 비용은 반환되지 않아요"};
}
} // namespace thorium::game
