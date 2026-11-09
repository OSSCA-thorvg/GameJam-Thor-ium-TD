#include "Catalog.h"
#include <algorithm>
#include <cmath>

namespace thorium::game {
std::string costLabel(const ResourceCost &cost, bool compact) {
    const std::array<int, 4> amounts{{cost.iron, cost.copper, cost.crystal, cost.gold}};
    const std::array<const char *, 4> names = compact
                                                  ? std::array<const char *, 4>{{"철", "구", "수정", "금"}}
                                                  : std::array<const char *, 4>{{"철", "구리", "수정", "금"}};
    std::string result;
    for (std::size_t index = 0; index < amounts.size(); ++index)
        if (amounts[index] > 0) {
            if (!compact && !result.empty())
                result += " ";
            result += std::string(names[index]) + std::to_string(amounts[index]);
        }
    return result.empty() ? "무료" : result;
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
std::vector<Spawn> Catalog::schedule(int wave) const {
    std::vector<Spawn> result;
    for (const SpawnGroup &group : waves.at(static_cast<std::size_t>(wave - 1)))
        for (int index = 0; index < group.count; ++index)
            result.push_back({group.start + index * group.interval, group.kind});
    std::stable_sort(result.begin(), result.end(),
                     [](const Spawn &a, const Spawn &b) { return a.time < b.time; });
    return result;
}
const Catalog &defaultCatalog() {
    static const Catalog content{
        {{{"연사탑", 3, CardCategory::Building, 3, 12, .75F},
          {"포격탑", 4, CardCategory::Building, 3.5F, 26, 1.8F, 1.1F},
          {"경비병", 3, CardCategory::Unit, 1.1F, 10, 1, 0, 120, 2},
          {"서리", 2, CardCategory::Spell, 2, 0, 1, 0, 0, 0, 1, 0, 3},
          {"폭발", 3, CardCategory::Spell, 1.5F, 38},
          {"전격탑", 5, CardCategory::Building, 3, 10, .8F, 0, 0, 0, 3, 2},
          {"사수", 3, CardCategory::Unit, 3, 12, 1, 0, 70, 1},
          {"폭파병", 4, CardCategory::Unit, 2.8F, 18, 1.8F, 1.1F, 65, 1}}},
        {{{36, 0, 1, 8, 1, 1}, {24, 0, 1.6F, 6, .8F, 1}, {100, 4, .65F, 16, 1.5F, 3}}},
        {{CardKind::Gun, CardKind::Guard, CardKind::Cannon, CardKind::Frost, CardKind::Gun, CardKind::Guard,
          CardKind::Blast, CardKind::Gun}},
        {{4, 3}, {8, 3}, {8, 7}, {14, 7}, {14, 5}, {16, 5}},
        {{6, 4}, {9, 4}, {7, 6}, {10, 6}, {12, 8}, {15, 6}},
        {{8, 4}, {8, 6}, {10, 7}, {12, 7}, {14, 6}, {15, 5}, {9, 7}, {13, 7}},
        {{{},
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
        {}};
    return content;
}
float distance(Point a, Point b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}
} // namespace thorium::game
