#pragma once
#include "game/model/PlayPreview.h"

namespace thorium::presentation {
/** Name the concrete operation selected by the authoritative target query. */
inline const char *playActionName(game::PlayAction action) {
    switch (action) {
    case game::PlayAction::Install:
        return "설치";
    case game::PlayAction::Upgrade:
        return "강화";
    case game::PlayAction::Replace:
        return "교체";
    case game::PlayAction::Summon:
        return "소환";
    case game::PlayAction::Cast:
        return "마법";
    case game::PlayAction::None:
        return "사용 불가";
    }
    return "사용 불가";
}
/** Label the authoritative card category rather than duplicating it in authored text. */
inline const char *cardCategoryName(game::CardCategory category) {
    return category == game::CardCategory::Building ? "건물"
           : category == game::CardCategory::Unit   ? "유닛"
                                                    : "마법";
}
/** Explain placement using the same category that validates the card command. */
inline const char *cardInstruction(game::CardCategory category) {
    return category == game::CardCategory::Building ? "둥근 패드에 설치 · 같은 건물은 강화"
           : category == game::CardCategory::Unit   ? "길 위 배치 칸에 소환 · 슬롯 1 사용"
                                                    : "전장의 원하는 위치에 사용";
}
} // namespace thorium::presentation
