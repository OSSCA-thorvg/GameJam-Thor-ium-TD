#include "Layout.h"
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
int Layout::mineCell(ScreenPoint position) const {
    const int x = static_cast<int>(std::floor((position.x - mineOrigin.x) / mineTile));
    const int y = static_cast<int>(std::floor((position.y - mineOrigin.y) / mineTile));
    return x < 0 || x >= 5 || y < 0 || y >= 6 ? -1 : y * 5 + x;
}
} // namespace thorium::presentation
