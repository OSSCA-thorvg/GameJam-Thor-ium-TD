#pragma once
#include "engine/ui/Ui.h"
#include "game/model/PlayPreview.h"

namespace thorium::presentation {
/** One rendered cursor hint; contains no command authority and is rebuilt for each visible frame. */
struct PlacementPreview {
    game::CardId card = 0;
    game::Point target;
    game::PlayPreview rule;
    engine::ScreenPoint pointer;
    /** Authored attack/effect radius in world tiles, matching the card that would be committed. */
    float range = 0;
};
} // namespace thorium::presentation
