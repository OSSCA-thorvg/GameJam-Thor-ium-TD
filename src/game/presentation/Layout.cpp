#include "Layout.h"
#include <algorithm>
#include <cmath>

namespace thorium::presentation {
using engine::ScreenPoint;
ScreenPoint Layout::screenPoint(game::Point point) const {
    return {mapOrigin.x + (point.x + 0.5F) * tile, mapOrigin.y + (point.y + 0.5F) * tile};
}
game::Point Layout::surfacePoint(ScreenPoint point, bool snap) const {
    const float x = (point.x - mapOrigin.x) / tile - 0.5F;
    const float y = (point.y - mapOrigin.y) / tile - 0.5F;
    return {snap ? std::round(x) : x, snap ? std::round(y) : y};
}
game::Point Layout::minimapPoint(ScreenPoint point) const {
    return {(point.x - minimap.x) / std::max(1.0F, minimap.w) * fieldWidth - .5F,
            (point.y - minimap.y) / std::max(1.0F, minimap.h) * fieldHeight - .5F};
}
ScreenPoint Layout::minimapScreen(game::Point point) const {
    return {minimap.x + (point.x + .5F) / fieldWidth * minimap.w,
            minimap.y + (point.y + .5F) / fieldHeight * minimap.h};
}
int Layout::mineCell(ScreenPoint position) const {
    if (mineViewport.w > 0 && !mineViewport.contains(position))
        return -1;
    const int x = static_cast<int>(std::floor((position.x - mineOrigin.x) / mineTile));
    const int y = static_cast<int>(std::floor((position.y - mineOrigin.y) / mineTile));
    return x < 0 || x >= mineColumns || y < 0 || y >= mineRows ? -1 : y * mineColumns + x;
}
engine::Rect Layout::mineCellBounds(int index) const {
    return {mineOrigin.x + static_cast<float>(index % mineColumns) * mineTile,
            mineOrigin.y + static_cast<float>(index / mineColumns) * mineTile, mineTile, mineTile};
}
} // namespace thorium::presentation
