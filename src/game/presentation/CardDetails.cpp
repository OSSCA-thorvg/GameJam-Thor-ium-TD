#include "CardDetails.h"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace thorium::presentation {
namespace {
/** Keep short card values readable without rounding small nonzero ranges to zero. */
std::string number(float value) {
    std::ostringstream text;
    text << std::setprecision(3) << value;
    return text.str();
}
} // namespace
CardDetails cardDetails(const game::Catalog &catalog, game::CardKind kind) {
    const game::CardDefinition &definition = catalog.card(kind);
    CardDetails result{kind, definition.name, definition.category, definition.cost, {}, {}};
    const bool spell = definition.category == game::CardCategory::Spell;
    result.stats.push_back({spell ? "효과 범위" : "사거리", number(definition.range) + "칸"});
    if (spell && kind == game::CardKind::Frost) {
        result.description = "범위 안의 적을 둔화시켜 진격을 늦춥니다";
        result.stats.push_back({"둔화", number((1 - catalog.balance.slowFactor) * 100) + "%"});
        result.stats.push_back({"지속 시간", number(definition.slowDuration) + "초"});
    } else if (spell && kind == game::CardKind::Tar) {
        result.description = "길목에 타르를 깔아 지나가는 지상 적을 지속적으로 둔화시킵니다";
        result.stats.push_back({"둔화", number((1 - catalog.balance.slowFactor) * 100) + "%"});
        result.stats.push_back({"유지 시간", number(definition.slowDuration) + "초"});
    } else if (definition.aura > 0) {
        result.description = "주변 아군 건물의 공격력을 증폭합니다 (직접 공격 불가)";
        result.stats.front().label = "증폭 범위";
        result.stats.push_back({"공격력 증폭", "+" + number(definition.aura * 100) + "%"});
        result.stats.push_back({"중첩", "가장 높은 효과 1개"});
    } else {
        result.stats.push_back({spell ? "피해" : "공격력", std::to_string(definition.damage)});
        if (spell)
            result.description = "지정 범위 안의 적에게 즉시 폭발 피해를 입힙니다";
        else
            result.stats.push_back({"공격 간격", number(definition.interval) + "초"});
    }
    if (catalog.isUnit(kind)) {
        result.description = kind == game::CardKind::Demolition ? "주변 적에게 범위 폭발 피해를 입히는 타격병입니다"
                             : kind == game::CardKind::Guard    ? "길목에서 적의 진격을 막아서는 근접 방어병입니다"
                                                                : "지정 위치에서 적을 저지하며 사격하는 원거리 병력입니다";
        result.stats.push_back({"체력", std::to_string(definition.hp)});
        result.stats.push_back({"저지", std::to_string(definition.blockers) + "명"});
    } else if (catalog.isBuilding(kind)) {
        if (definition.aura <= 0)
            result.description = kind == game::CardKind::Cannon      ? "포탄을 발사해 표적과 주변 적을 함께 타격합니다"
                                 : kind == game::CardKind::Lightning ? "가까운 적에게 연쇄 전격을 날립니다 (방어력 무시)"
                                 : kind == game::CardKind::Flame
                                     ? "부채꼴 범위의 적을 빠르게 불태웁니다 (높은 방어력에 취약)"
                                 : kind == game::CardKind::Sniper ? "긴 사거리에서 체력이 가장 높은 표적을 우선 저격합니다"
                                                                  : "사거리 내 단일 표적을 빠르게 사격합니다";
    }
    if (catalog.isBuilding(kind) && definition.arc > 0)
        result.stats.push_back({"화염 각도", number(definition.arc * 2) + "°"});
    if ((catalog.isBuilding(kind) && kind == game::CardKind::Cannon) ||
        (catalog.isUnit(kind) && kind == game::CardKind::Demolition))
        result.stats.push_back({"폭발 범위", number(definition.splash) + "칸"});
    if (catalog.isBuilding(kind) && kind == game::CardKind::Lightning) {
        result.stats.push_back({"연쇄 대상", std::to_string(definition.chains) + "명"});
        result.stats.push_back({"연쇄 거리", number(definition.chainRange) + "칸"});
    }
    return result;
}
} // namespace thorium::presentation
