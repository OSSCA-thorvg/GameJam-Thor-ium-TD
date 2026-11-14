#pragma once
#include "Text.h"
#include "game/model/PlayPreview.h"
#include <string>

namespace thorium::presentation {
/** Name the concrete operation selected by the authoritative target query. */
inline std::string playActionName(game::PlayAction action) {
    switch (action) {
    case game::PlayAction::Install:
        return tr("cardrole.action.install");
    case game::PlayAction::Upgrade:
        return tr("cardrole.action.upgrade");
    case game::PlayAction::Replace:
        return tr("cardrole.action.replace");
    case game::PlayAction::Summon:
        return tr("cardrole.action.summon");
    case game::PlayAction::Cast:
        return tr("cardrole.action.cast");
    case game::PlayAction::None:
        return tr("cardrole.action.none");
    }
    return tr("cardrole.action.none");
}
/** Label the authoritative card category rather than duplicating it in authored text. */
inline std::string cardCategoryName(game::CardCategory category) {
    return category == game::CardCategory::Building ? tr("cardrole.building")
           : category == game::CardCategory::Unit   ? tr("cardrole.unit")
                                                    : tr("cardrole.spell");
}
/** Explain placement using the same category that validates the card command. */
inline std::string cardInstruction(game::CardCategory category) {
    return category == game::CardCategory::Building ? tr("cardrole.hint.building")
           : category == game::CardCategory::Unit   ? tr("cardrole.hint.unit")
                                                    : tr("cardrole.hint.spell");
}
} // namespace thorium::presentation
