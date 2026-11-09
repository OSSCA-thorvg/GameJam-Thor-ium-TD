#pragma once
#include "engine/ui/Ui.h"
#include "game/content/Catalog.h"
#include <string>
#include <vector>

namespace thorium::presentation {
/** One concise, localized base statistic; no independently authored balance values. */
struct CardStat {
    std::string label;
    std::string value;
};
/** Base card information shared by acquisition, recruitment and inspection surfaces. */
struct CardDetails {
    game::CardKind kind = game::CardKind::Gun;
    std::string name;
    game::CardCategory category = game::CardCategory::Building;
    int cost = 0;
    std::string description;
    std::vector<CardStat> stats;
};
/** Resolve this catalog's category, values and implemented effect into compact base-card information. */
CardDetails cardDetails(const game::Catalog &catalog, game::CardKind kind);
/** Last rendered inspection panel; carries no card-play or offer acceptance authority. */
struct CardInspection {
    engine::WidgetId widget = 0;
    CardDetails details;
    engine::Rect bounds;
};
} // namespace thorium::presentation
