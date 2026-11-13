#include "MineScroll.h"
#include <algorithm>
#include <cmath>

namespace thorium {
bool MineScroll::available() const {
    return state.app.navigation.gameplay() && state.play.mine && run.state().phase == game::Phase::Prep &&
           !state.play.paused && state.play.modal == presentation::Modal::None && layout.mineScrollMax > 0;
}
float MineScroll::clamp(float tiles) const {
    const float tile = std::max(1.0F, layout.mineTile);
    return std::clamp(tiles, 0.0F, layout.mineScrollMax / tile);
}
void MineScroll::cancel() {
    tracking_ = dragged_ = false;
}
bool MineScroll::wheel(float steps) {
    if (!state.app.navigation.gameplay() || !state.play.mine || run.state().phase != game::Phase::Prep)
        return false;
    if (available() && std::isfinite(steps))
        state.play.mineScroll = clamp(state.play.mineScroll - std::clamp(steps, -8.0F, 8.0F) * .75F);
    return true;
}
bool MineScroll::pointer(engine::Pointer event) {
    if (event.phase == engine::PointerPhase::Cancel || !available()) {
        cancel();
        return false;
    }
    if (event.phase == engine::PointerPhase::Down && !tracking_ && ui.captured == presentation::World) {
        tracking_ = true;
        dragged_ = false;
        pointerId_ = event.id;
        device_ = event.device;
        origin_ = event.position;
        initial_ = state.play.mineScroll;
    }
    if (!tracking_ || pointerId_ != event.id || device_ != event.device)
        return false;
    if (event.phase == engine::PointerPhase::Move && ui.dragging && ui.captured == presentation::World) {
        dragged_ = true;
        state.play.mineScroll =
            clamp(initial_ - (event.position.y - origin_.y) / std::max(1.0F, layout.mineTile));
    }
    if (event.phase == engine::PointerPhase::Up) {
        const bool consumed = dragged_;
        cancel();
        return consumed;
    }
    return false;
}
void MineScroll::follow(float elapsed) {
    if (run.state().mineLayer != layer_) {
        layer_ = run.state().mineLayer;
        state.play.mineScroll = 0;
    }
    if (!run.state().job.active || tracking_ || layout.mineScrollMax <= 0 || layout.mineTile <= 0)
        return;
    const float tile = layout.mineTile;
    const float visible = layout.mineViewport.h / tile;
    const float row =
        std::max(run.minerPosition().y, static_cast<float>(run.state().job.target / run.state().mineWidth));
    // Board rows start below a surface strip of .6 tiles; keep one row of margin below the target.
    const float lowest = row + .6F + 1.6F - visible;
    const float highest = run.minerPosition().y + .6F - .4F;
    float goal = state.play.mineScroll;
    if (goal < lowest)
        goal = lowest;
    if (goal > highest)
        goal = highest;
    goal = clamp(goal);
    if (state.app.preferences.reducedMotion)
        state.play.mineScroll = goal;
    else
        state.play.mineScroll += (goal - state.play.mineScroll) * std::min(1.0F, elapsed * 8.0F);
}
} // namespace thorium
