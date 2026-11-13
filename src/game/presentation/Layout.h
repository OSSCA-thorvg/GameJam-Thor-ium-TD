#pragma once
#include "engine/ui/Ui.h"
#include "engine/view/Camera2D.h"
#include "game/model/Types.h"

namespace thorium::presentation {
/** Responsive logical geometry shared by drawing and hit testing. */
struct Layout {
    bool compact = false;
    bool denseMine = false;
    engine::Camera2D camera;
    engine::Rect world;
    engine::Rect sidebar;
    engine::Rect hand;
    engine::Rect energyBar;
    engine::Rect energyMeter;
    engine::ScreenPoint mapOrigin;
    float tile = 1.0F;
    /** Clip and hit-test area of the cutaway board; empty means no clipping. */
    engine::Rect mineViewport;
    /** Screen position of cell 0, already offset by the vertical scroll. */
    engine::ScreenPoint mineOrigin;
    float mineTile = 1.0F;
    int fieldWidth = 1;
    int fieldHeight = 1;
    /** Overview inset inside the world; empty when the whole field is already visible. */
    engine::Rect minimap;
    /** Entrance tiles in lane order, used by off-screen indicators to recenter the camera. */
    std::vector<game::Point> entrances;
    int mineColumns = 5;
    int mineRows = 6;
    /** Current and maximum vertical board scroll in logical pixels. */
    float mineScroll = 0.0F;
    float mineScrollMax = 0.0F;
    /** Convert a screen position to a placement target with optional grid snapping. */
    game::Point surfacePoint(engine::ScreenPoint point, bool snap) const;
    /** Resolve a cutaway cell and reject board margins and scrolled-out rows. */
    int mineCell(engine::ScreenPoint point) const;
    /** Return the screen rectangle of one cutaway cell, including the current scroll. */
    engine::Rect mineCellBounds(int index) const;
    /** Convert one surface tile position to screen coordinates. */
    engine::ScreenPoint screenPoint(game::Point point) const;
    /** Convert a point inside the minimap inset to the world tile it represents. */
    game::Point minimapPoint(engine::ScreenPoint point) const;
    /** Convert a world tile position to its location inside the minimap inset. */
    engine::ScreenPoint minimapScreen(game::Point point) const;
};

} // namespace thorium::presentation
