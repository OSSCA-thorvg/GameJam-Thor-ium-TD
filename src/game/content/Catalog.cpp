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

const Catalog &stage2Catalog() {
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
        {{1, 2}, {7, 2}, {7, 5}, {3, 5}, {3, 8}, {12, 8}, {12, 4}, {17, 4}},
        {{5, 3}, {8, 3}, {5, 6}, {2, 6}, {5, 7}, {9, 7}, {13, 6}, {14, 3}},
        {{4, 2}, {7, 3}, {5, 5}, {3, 6}, {6, 8}, {10, 8}, {12, 6}, {15, 4}},
        {{{Ore::Iron, 2},
          {Ore::Rock, 0, 1, false, true},
          {Ore::Rock, 0, 1, false, true},
          {Ore::Rock, 0, 1, false, true},
          {Ore::Copper, 2},
          {Ore::Iron, 2},
          {Ore::Gold, 3},
          {Ore::Iron, 2, 1, false, true},
          {Ore::Crystal, 2},
          {Ore::Copper, 2},
          {Ore::Rock, 0, 2},
          {Ore::Iron, 4, 1, true},
          {Ore::Gold, 4},
          {Ore::Copper, 4, 1, true},
          {Ore::Rock, 0, 2},
          {},
          {Ore::Iron, 3},
          {Ore::Crystal, 3, 2},
          {Ore::Gold, 4},
          {},
          {},
          {Ore::Gold, 6, 1, true},
          {Ore::Crystal, 3},
          {Ore::Gold, 4},
          {},
          {},
          {Ore::Rock, 0, 2},
          {Ore::Crystal, 4, 2},
          {},
          {}}},
        {{{EnemyKind::Normal, 14, 3, 1.0F}, {EnemyKind::Runner, 6, 16.0F, 0.7F}},
         {{EnemyKind::Normal, 18, 3, 0.8F},
          {EnemyKind::Runner, 12, 15.0F, 0.6F},
          {EnemyKind::Armored, 4, 22.0F, 1.8F}},
         {{EnemyKind::Normal, 22, 3, 0.6F},
          {EnemyKind::Runner, 16, 16.0F, 0.5F},
          {EnemyKind::Armored, 8, 24.0F, 1.2F}},
         {{EnemyKind::Normal, 28, 3, 0.5F},
          {EnemyKind::Runner, 24, 15.0F, 0.4F},
          {EnemyKind::Armored, 12, 22.0F, 1.0F}}},
        {{{{{1, 0}, {1, 1}}, {{2, 1}, {3, 1}}, {{2, 0}}, CardKind::Blast},
          {{{0, 2}}, {{0, 1}, {0, 3}}, {{0, 4}}, CardKind::Demolition}}},
        {}};
    return content;
}

const Catalog &stage3Catalog() {
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
        {{2, 1}, {16, 1}, {16, 8}, {6, 8}, {6, 4}, {11, 4}},
        {{8, 3}, {10, 3}, {8, 5}, {10, 5}, {5, 3}, {5, 5}, {14, 3}, {14, 6}, {11, 7}},
        {{6, 1}, {12, 1}, {16, 4}, {16, 6}, {12, 8}, {8, 8}, {6, 6}, {7, 4}, {10, 4}},
        {{{Ore::Iron, 2},
          {Ore::Rock, 0, 1, false, true},
          {Ore::Rock, 0, 1, false, true},
          {Ore::Rock, 0, 1, false, true},
          {Ore::Crystal, 2},
          {Ore::Iron, 2},
          {Ore::Rock, 0, 2},
          {Ore::Iron, 2, 1, false, true},
          {Ore::Copper, 2, 1, false, true},
          {Ore::Copper, 2},
          {Ore::Rock, 0, 2},
          {Ore::Gold, 3},
          {Ore::Crystal, 3},
          {Ore::Iron, 4, 1, true},
          {Ore::Rock, 0, 2},
          {Ore::Iron, 3},
          {Ore::Gold, 4, 1, true},
          {Ore::Crystal, 3, 2},
          {Ore::Gold, 4},
          {Ore::Rock, 0, 2},
          {},
          {Ore::Rock, 0, 2},
          {Ore::Crystal, 4, 2},
          {Ore::Gold, 6, 1, true},
          {},
          {},
          {Ore::Rock, 0, 2},
          {Ore::Crystal, 4, 2},
          {},
          {}}},
        {{{EnemyKind::Normal, 16, 3, 0.9F}, {EnemyKind::Runner, 6, 17.0F, 0.7F}},
         {{EnemyKind::Normal, 20, 3, 0.8F},
          {EnemyKind::Runner, 10, 16.0F, 0.6F},
          {EnemyKind::Armored, 6, 21.0F, 1.8F}},
         {{EnemyKind::Normal, 24, 3, 0.6F},
          {EnemyKind::Runner, 16, 16.0F, 0.5F},
          {EnemyKind::Armored, 8, 23.0F, 1.4F}},
         {{EnemyKind::Normal, 30, 3, 0.5F},
          {EnemyKind::Runner, 20, 15.0F, 0.4F},
          {EnemyKind::Armored, 12, 22.0F, 1.0F}},
         {{EnemyKind::Normal, 36, 3, 0.4F},
          {EnemyKind::Runner, 30, 14.0F, 0.35F},
          {EnemyKind::Armored, 18, 20.0F, 0.9F}}},
        {{{{{2, 3}, {2, 2}}, {{1, 2}, {3, 2}}, {{1, 3}}, CardKind::Lightning},
          {{{0, 1}}, {{1, 0}, {1, 1}}, {{0, 2}}, CardKind::Frost}}},
        {}};
    return content;
}

const Catalog &stageCatalog(int stageIndex) {
    switch (stageIndex) {
    case 2:
        return stage2Catalog();
    case 3:
        return stage3Catalog();
    case 1:
    default:
        return defaultCatalog();
    }
}

const StageMetadata &stageMetadata(int stageIndex) {
    static const std::array<StageMetadata, 3> meta{{
        {1, "표층 방어선", "제1 작전 구역 - 지상 초소",
         "기본적인 S자 경로를 방어하며 자원 채굴과 타워 배치의 기초를 다집니다.", 3, 1},
        {2, "흑요석 협곡", "제2 작전 구역 - 험준한 바위산",
         "지그재그 협곡 통로로 고속 러너들이 쇄도합니다. 범위 공격과 길목 차단이 필수입니다.", 4, 2},
        {3, "동토의 고대 갱도", "제3 작전 구역 - 영구동토 순환로",
         "외곽을 순환하는 적들을 중앙 섬형 진지에서 요격하고, 심층 엘리트 공세를 저지하십시오.", 5, 3}}};
    const int index = std::clamp(stageIndex, 1, TotalStages) - 1;
    return meta[static_cast<std::size_t>(index)];
}
float distance(Point a, Point b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}
} // namespace thorium::game
