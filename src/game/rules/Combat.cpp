#include "RuleGeometry.h"
#include "Run.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace thorium::game {
float Run::routeLength() const {
    float total = 0;
    for (std::size_t index = 1; index < state_.route.size(); ++index)
        total += distance(state_.route[index - 1], state_.route[index]);
    return total;
}

Point Run::routePoint(float progress) const {
    for (std::size_t index = 1; index < state_.route.size(); ++index) {
        const Point a = state_.route[index - 1], b = state_.route[index];
        const float length = distance(a, b);
        if (progress <= length)
            return {a.x + (b.x - a.x) * progress / length, a.y + (b.y - a.y) * progress / length};
        progress -= length;
    }
    return state_.route.back();
}

void Run::advanceBattle(float delta) {
    state_.battleTime += delta;
    if (state_.energy < catalog_.balance.energyCap) {
        state_.energyTime += delta;
        if (state_.energyTime + 0.0001F >= catalog_.balance.energyInterval) {
            state_.energyTime -= catalog_.balance.energyInterval;
            ++state_.energy;
            if (state_.energy == catalog_.balance.energyCap)
                state_.energyTime = 0;
        }
    }
    while (nextSpawn_ < spawns_.size() && spawns_[nextSpawn_].time <= state_.battleTime + 0.0001F) {
        const EnemyKind kind = spawns_[nextSpawn_++].kind;
        state_.enemies.push_back({nextEntity_++, kind, state_.route.front(), 0.0F, catalog_.enemy(kind).hp});
        ++state_.spawned;
    }
    for (Enemy &enemy : state_.enemies) {
        enemy.cooldown = std::max(0.0F, enemy.cooldown - delta);
        if (enemy.blocker && !unit(enemy.blocker))
            enemy.blocker = 0;
        if (!enemy.blocker) {
            const EnemyDefinition &stats = catalog_.enemy(enemy.kind);
            enemy.progress += stats.speed *
                              (state_.battleTime < enemy.slowUntil ? catalog_.balance.slowFactor : 1.0F) *
                              delta;
            enemy.position = routePoint(enemy.progress);
        }
    }
    for (Unit &ally : state_.units) {
        ally.cooldown = std::max(0.0F, ally.cooldown - delta);
        int blocked = 0;
        for (const Enemy &enemy : state_.enemies)
            if (enemy.blocker == ally.id)
                ++blocked;
        for (Enemy &enemy : state_.enemies)
            if (!enemy.blocker && blocked < catalog_.card(ally.kind).blockers &&
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
    /** Select the closest-to-base living enemy, optionally preferring an ally's blockers. */
    const auto targetFor = [this](Point position, float range, EntityId blocker) -> Enemy * {
        Enemy *best = nullptr;
        for (Enemy &enemy : state_.enemies)
            if (enemy.hp > 0 && distance(position, enemy.position) <= range) {
                const bool preferred = blocker && enemy.blocker == blocker;
                const bool bestPreferred = best && blocker && best->blocker == blocker;
                if (!best || (preferred && !bestPreferred) ||
                    (preferred == bestPreferred &&
                     (enemy.progress > best->progress ||
                      (enemy.progress == best->progress && enemy.id < best->id))))
                    best = &enemy;
            }
        return best;
    };
    /** Record one physical or electric hit without changing the target during selection. */
    const auto hit = [this, &hits](const Enemy &enemy, int damage, bool electric, CardKind card,
                                   Point origin) {
        hits.push_back({static_cast<std::size_t>(&enemy - state_.enemies.data()),
                        electric ? damage : std::max(1, damage - catalog_.enemy(enemy.kind).armor), true,
                        card, origin});
    };
    for (Tower &tower : state_.towers) {
        tower.cooldown = std::max(0.0F, tower.cooldown - delta);
        if (tower.cooldown > 0)
            continue;
        Enemy *target = targetFor(tower.position, catalog_.card(tower.kind).range, 0);
        if (!target)
            continue;
        const float factor = 1.0F + (tower.level - 1) * catalog_.balance.levelBonus;
        const CardDefinition &definition = catalog_.card(tower.kind);
        const int damage = static_cast<int>(std::floor(
            definition.damage * factor *
            (tower.kind == CardKind::Lightning && state_.coil ? catalog_.balance.coilBonus : 1.0F)));
        if (tower.kind == CardKind::Cannon) {
            for (const Enemy &enemy : state_.enemies)
                if (distance(enemy.position, target->position) <= definition.splash)
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
                    if (std::find(chained.begin(), chained.end(), candidate.id) == chained.end()) {
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
            Enemy *target = targetFor(ally.position, catalog_.card(ally.kind).range, ally.id);
            if (target) {
                const CardDefinition &definition = catalog_.card(ally.kind);
                if (ally.kind == CardKind::Demolition) {
                    for (const Enemy &enemy : state_.enemies)
                        if (distance(enemy.position, target->position) <= definition.splash)
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
    for (const Hit &damage : hits) {
        if (damage.enemy) {
            Enemy &enemy = state_.enemies[damage.targetIndex];
            enemy.hp -= damage.amount;
            emit({CombatEventKind::Damage, enemy.id, damage.origin, enemy.position, damage.card,
                  damage.amount, true});
        } else {
            Unit &ally = state_.units[damage.targetIndex];
            ally.hp -= damage.amount;
            emit({CombatEventKind::Damage, ally.id, damage.origin, ally.position, damage.card, damage.amount,
                  false});
        }
    }
    state_.units.erase(std::remove_if(state_.units.begin(), state_.units.end(),
                                      [this](const Unit &ally) {
                                          if (ally.hp > 0)
                                              return false;
                                          emit({CombatEventKind::Defeated, ally.id, ally.position,
                                                ally.position, ally.kind, 0, false});
                                          return true;
                                      }),
                       state_.units.end());
    state_.enemies.erase(std::remove_if(state_.enemies.begin(), state_.enemies.end(),
                                        [this](const Enemy &enemy) {
                                            if (enemy.hp <= 0) {
                                                ++state_.defeated;
                                                emit({CombatEventKind::Defeated, enemy.id, enemy.position,
                                                      enemy.position, CardKind::Gun, 0, true, 0, enemy.kind});
                                                return true;
                                            }
                                            if (enemy.progress >= routeLength()) {
                                                const int amount = catalog_.enemy(enemy.kind).leak;
                                                state_.baseHp -= amount;
                                                emit({CombatEventKind::Leak, enemy.id, enemy.position,
                                                      state_.route.back(), CardKind::Gun, amount});
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
    if (nextSpawn_ == spawns_.size() && state_.enemies.empty())
        finishWave();
}

Result Run::moveUnit(EntityId id, Point target) {
    const std::optional<Point> cell = resolveCell(state_.unitCells, target);
    if (state_.phase != Phase::Prep || !cell)
        return {false, "정비 중 배치 칸으로 옮겨 주세요"};
    target = *cell;
    Unit *selected = nullptr;
    for (Unit &ally : state_.units)
        if (ally.id == id)
            selected = &ally;
    if (!selected)
        return {false, "유닛을 선택해 주세요"};
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
