#pragma once
#include "engine/ui/Ui.h"
#include "game/model/PlayPreview.h"
#include <string>

namespace thorium::presentation {
/** One rendered cursor hint; contains no command authority and is rebuilt for each visible frame. */
struct PlacementPreview {
    game::CardId card = 0;
    game::Point target;
    game::PlayPreview rule;
    engine::ScreenPoint pointer;
    /** Attack/effect radius in world tiles of the card that would be committed, including the target pad's
     *  trait for a building. */
    float range = 0;
    /** Localized name and bonus of the target pad's trait for a building; empty otherwise. */
    std::string terrain;
};
} // namespace thorium::presentation
