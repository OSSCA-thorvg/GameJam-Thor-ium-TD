#pragma once
#include "engine/ui/Ui.h"
#include "game/model/Types.h"

namespace thorium::presentation {
/** Responsive logical geometry shared by drawing and hit testing. */
struct Layout {
    bool compact = false;
    engine::Rect world;
    engine::Rect sidebar;
    engine::Rect hand;
    engine::Rect energyBar;
    engine::Rect energyMeter;
    engine::ScreenPoint mapOrigin;
    float tile = 1.0F;
    engine::ScreenPoint mineOrigin;
    float mineTile = 1.0F;
    /** Convert a screen position to a placement target with optional grid snapping. */
    game::Point surfacePoint(engine::ScreenPoint point, bool snap) const;
    /** Resolve a cutaway cell and reject board margins. */
    int mineCell(engine::ScreenPoint point) const;
    /** Convert one surface tile position to screen coordinates. */
    engine::ScreenPoint screenPoint(game::Point point) const;
};

} // namespace thorium::presentation
