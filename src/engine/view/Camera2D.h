#pragma once
#include "engine/ui/Ui.h"
#include <algorithm>
#include <cmath>

namespace thorium::engine {
/** Persistent view intent in world-tile offsets, independent of any game or render device. */
struct CameraPose {
    float zoom = 1;
    float panX = 0;
    float panY = 0;
};
/** A point in tile coordinates; projection adds the half-tile center exactly once. */
struct WorldPoint {
    float x = 0;
    float y = 0;
};
/** Immutable projection shared by drawing and input for one measured viewport. */
class Camera2D {
  public:
    /** Fit authored bounds and clamp a finite pose while preserving square world tiles. */
    Camera2D(Rect viewport = {0, 0, 1, 1}, int width = 18, int height = 10, CameraPose pose = {})
        : viewport_(viewport), width_(std::max(1, width)), height_(std::max(1, height)), pose_(pose) {
        pose_.zoom = std::isfinite(pose.zoom) ? std::clamp(pose.zoom, 1.0F, 4.0F) : 1;
        const float availableW = std::max(1.0F, viewport.w - 24),
                    availableH = std::max(1.0F, viewport.h - 32);
        tile_ = std::max(.001F, std::min(availableW / width_, availableH / height_) * pose_.zoom);
        const float limitX = std::max(0.0F, (width_ - availableW / tile_) / 2);
        const float limitY = std::max(0.0F, (height_ - availableH / tile_) / 2);
        pose_.panX = std::isfinite(pose.panX) ? std::clamp(pose.panX, -limitX, limitX) : 0;
        pose_.panY = std::isfinite(pose.panY) ? std::clamp(pose.panY, -limitY, limitY) : 0;
        origin_ = {viewport.center().x - (width_ / 2.0F + pose_.panX) * tile_,
                   viewport.center().y - (height_ / 2.0F + pose_.panY) * tile_};
    }
    /** Read the bounded pose actually represented by this transform. */
    CameraPose pose() const {
        return pose_;
    }
    /** Read the screen origin of the first tile's top-left corner. */
    ScreenPoint origin() const {
        return origin_;
    }
    /** Read one square tile's logical pixel size. */
    float tileSize() const {
        return tile_;
    }
    /** Project a world tile center into logical screen coordinates. */
    ScreenPoint project(WorldPoint point) const {
        return {origin_.x + (point.x + .5F) * tile_, origin_.y + (point.y + .5F) * tile_};
    }
    /** Invert projection without rounding or clamping a placement target. */
    WorldPoint unproject(ScreenPoint point) const {
        return {(point.x - origin_.x) / tile_ - .5F, (point.y - origin_.y) / tile_ - .5F};
    }
    /** Pan from this original transform using total gesture displacement, avoiding frame-rate dependence. */
    CameraPose moved(ScreenPoint delta) const {
        return Camera2D(viewport_, width_, height_,
                        {pose_.zoom, pose_.panX - delta.x / tile_, pose_.panY - delta.y / tile_})
            .pose();
    }
    /** Zoom around the pointer, retaining its world target unless an authored edge clamps the view. */
    CameraPose zoomed(float factor, ScreenPoint anchor) const {
        if (!std::isfinite(factor) || factor <= 0)
            return pose_;
        const WorldPoint before = unproject(anchor);
        CameraPose next = pose_;
        next.zoom = std::clamp(next.zoom * factor, 1.0F, 4.0F);
        const WorldPoint after = Camera2D(viewport_, width_, height_, next).unproject(anchor);
        next.panX += before.x - after.x;
        next.panY += before.y - after.y;
        return Camera2D(viewport_, width_, height_, next).pose();
    }

    /** Keep the current zoom and center a world point, clamped to authored bounds. */
    CameraPose centered(WorldPoint point) const {
        return Camera2D(viewport_, width_, height_,
                        {pose_.zoom, point.x + .5F - width_ / 2.0F, point.y + .5F - height_ / 2.0F})
            .pose();
    }
    /** Zoom in from the fitted view until tiles reach a minimum size, then center a world point. */
    CameraPose framed(float minimumTile, WorldPoint point) const {
        const float fitted = tile_ / pose_.zoom;
        const float zoom = std::isfinite(minimumTile) ? std::clamp(minimumTile / fitted, 1.0F, 4.0F) : 1.0F;
        return Camera2D(viewport_, width_, height_, {zoom, 0, 0}).centered(point);
    }
    /** Read the world rectangle currently visible through the viewport, in tile-edge coordinates. */
    Rect visibleWorld() const {
        const WorldPoint first = unproject({viewport_.x, viewport_.y});
        const WorldPoint last = unproject({viewport_.x + viewport_.w, viewport_.y + viewport_.h});
        return {first.x + .5F, first.y + .5F, last.x - first.x, last.y - first.y};
    }

  private:
    Rect viewport_;
    int width_;
    int height_;
    CameraPose pose_;
    ScreenPoint origin_;
    float tile_ = 1;
};
} // namespace thorium::engine
