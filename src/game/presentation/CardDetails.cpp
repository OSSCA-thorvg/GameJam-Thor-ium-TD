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
        result.description = "범위 안 적을 느리게 만들어 진입을 늦춰요";
        result.stats.push_back({"둔화", number((1 - catalog.balance.slowFactor) * 100) + "%"});
        result.stats.push_back({"지속 시간", number(definition.slowDuration) + "초"});
    } else {
        result.stats.push_back({spell ? "피해" : "공격력", std::to_string(definition.damage)});
        if (spell)
            result.description = "지정 범위 안의 적에게 즉시 피해를 줘요";
        else
            result.stats.push_back({"공격 간격", number(definition.interval) + "초"});
    }
    if (catalog.isUnit(kind)) {
        result.description = kind == game::CardKind::Demolition ? "주변 적을 함께 공격하는 범위 유닛이에요"
                             : kind == game::CardKind::Guard    ? "길목의 적을 막으며 근접 공격해요"
                                                                : "지정 위치에서 적을 저지하고 공격해요";
        result.stats.push_back({"체력", std::to_string(definition.hp)});
        result.stats.push_back({"저지", std::to_string(definition.blockers) + "명"});
    } else if (catalog.isBuilding(kind)) {
        result.description = kind == game::CardKind::Cannon      ? "포탄으로 주변 적을 함께 공격해요"
                              : kind == game::CardKind::Lightning ? "가까운 적을 연쇄 공격해요 · 방어 무시"
                                                                 : "범위 안의 적 하나를 집중 공격해요";
    }
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
