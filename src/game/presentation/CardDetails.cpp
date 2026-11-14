#include "CardDetails.h"
#include "Text.h"
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
/** Fill a value unit template such as `carddetail.value.tiles` with one number. */
std::string unit(std::string_view key, const std::string &value) {
    return tr(key, {{"value", value}});
}
} // namespace
CardDetails cardDetails(const game::Catalog &catalog, game::CardKind kind) {
    const game::CardDefinition &definition = catalog.card(kind);
    CardDetails result{kind, cardName(kind), definition.category, definition.cost, {}, {}};
    const bool spell = definition.category == game::CardCategory::Spell;
    result.stats.push_back({tr(spell ? "carddetail.stat.area" : "carddetail.stat.range"),
                            unit("carddetail.value.tiles", number(definition.range))});
    if (spell && kind == game::CardKind::Frost) {
        result.description = tr("carddetail.desc.frost");
        result.stats.push_back(
            {tr("carddetail.stat.slow"), number((1 - catalog.balance.slowFactor) * 100) + "%"});
        result.stats.push_back({tr("carddetail.stat.duration"),
                                unit("carddetail.value.seconds", number(definition.slowDuration))});
    } else if (spell && kind == game::CardKind::Tar) {
        result.description = tr("carddetail.desc.tar");
        result.stats.push_back(
            {tr("carddetail.stat.slow"), number((1 - catalog.balance.slowFactor) * 100) + "%"});
        result.stats.push_back({tr("carddetail.stat.linger"),
                                unit("carddetail.value.seconds", number(definition.slowDuration))});
    } else if (definition.aura > 0) {
        result.description = tr("carddetail.desc.generator");
        result.stats.front().label = tr("carddetail.stat.auraRange");
        result.stats.push_back({tr("carddetail.stat.boost"), "+" + number(definition.aura * 100) + "%"});
        result.stats.push_back({tr("carddetail.stat.stacking"), tr("carddetail.value.stacking")});
    } else {
        result.stats.push_back({tr(spell ? "carddetail.stat.damage" : "carddetail.stat.attack"),
                                std::to_string(definition.damage)});
        if (spell)
            result.description = tr("carddetail.desc.blast");
        else
            result.stats.push_back({tr("carddetail.stat.interval"),
                                    unit("carddetail.value.seconds", number(definition.interval))});
    }
    if (catalog.isUnit(kind)) {
        result.description = tr(kind == game::CardKind::Demolition ? "carddetail.desc.demolition"
                                : kind == game::CardKind::Guard    ? "carddetail.desc.guard"
                                                                   : "carddetail.desc.shooter");
        result.stats.push_back({tr("carddetail.stat.hp"), std::to_string(definition.hp)});
        result.stats.push_back({tr("carddetail.stat.block"),
                                unit("carddetail.value.count", std::to_string(definition.blockers))});
    } else if (catalog.isBuilding(kind)) {
        if (definition.aura <= 0)
            result.description = tr(kind == game::CardKind::Cannon      ? "carddetail.desc.cannon"
                                    : kind == game::CardKind::Lightning ? "carddetail.desc.lightning"
                                    : kind == game::CardKind::Flame     ? "carddetail.desc.flame"
                                    : kind == game::CardKind::Sniper    ? "carddetail.desc.sniper"
                                                                        : "carddetail.desc.gun");
    }
    if (catalog.isBuilding(kind) && definition.arc > 0)
        result.stats.push_back({tr("carddetail.stat.arc"), number(definition.arc * 2) + "°"});
    if ((catalog.isBuilding(kind) && kind == game::CardKind::Cannon) ||
        (catalog.isUnit(kind) && kind == game::CardKind::Demolition))
        result.stats.push_back(
            {tr("carddetail.stat.splash"), unit("carddetail.value.tiles", number(definition.splash))});
    if (catalog.isBuilding(kind) && kind == game::CardKind::Lightning) {
        result.stats.push_back({tr("carddetail.stat.chains"),
                                unit("carddetail.value.count", std::to_string(definition.chains))});
        result.stats.push_back({tr("carddetail.stat.chainRange"),
                                unit("carddetail.value.tiles", number(definition.chainRange))});
    }
    return result;
}
} // namespace thorium::presentation
