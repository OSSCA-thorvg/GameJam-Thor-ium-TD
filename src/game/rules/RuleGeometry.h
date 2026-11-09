#pragma once
#include "game/model/Types.h"
#include <algorithm>
#include <cmath>
#include <optional>

namespace thorium::game {
/** Return four cardinal mine neighbors without wrapping board rows. */
inline std::vector<int> neighbors(int index) {
    std::vector<int> result;
    const int x = index % 5;
    const int y = index / 5;
    if (x > 0)
        result.push_back(index - 1);
    if (x < 4)
        result.push_back(index + 1);
    if (y > 0)
        result.push_back(index - 5);
    if (y < 5)
        result.push_back(index + 5);
    return result;
}
/** Convert a mine index into horizontal/depth coordinates. */
inline Point minePoint(int index) {
    return {static_cast<float>(index % 5), static_cast<float>(index / 5)};
}
/** Resolve a finite tolerant target to its authored center before occupancy, pricing or movement. */
inline std::optional<Point> resolveCell(const std::vector<Point> &points, Point target) {
    if (!std::isfinite(target.x) || !std::isfinite(target.y))
        return std::nullopt;
    const std::vector<Point>::const_iterator found =
        std::find_if(points.begin(), points.end(),
                     [target](Point point) -> bool { return distance(point, target) < .15F; });
    return found == points.end() ? std::nullopt : std::optional<Point>{*found};
}
} // namespace thorium::game
