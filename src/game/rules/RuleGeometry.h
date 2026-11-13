#pragma once
#include "game/model/Types.h"
#include <algorithm>
#include <cmath>
#include <optional>

namespace thorium::game {
/** Return four cardinal neighbors of a row-major board without wrapping rows. */
inline std::vector<int> neighbors(int index, int width, int height) {
    std::vector<int> result;
    const int x = index % width;
    const int y = index / width;
    if (x > 0)
        result.push_back(index - 1);
    if (x < width - 1)
        result.push_back(index + 1);
    if (y > 0)
        result.push_back(index - width);
    if (y < height - 1)
        result.push_back(index + width);
    return result;
}
/** Check whether two row-major cells are distinct and touch horizontally, vertically or diagonally. */
inline bool surrounding(int a, int b, int width) {
    return a != b && std::abs(a % width - b % width) <= 1 && std::abs(a / width - b / width) <= 1;
}
/** Convert a mine index into horizontal/depth coordinates. */
inline Point minePoint(int index, int width) {
    return {static_cast<float>(index % width), static_cast<float>(index / width)};
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
