#include "MapCamera.h"
#include <cmath>

namespace thorium {
bool MapCamera::available() const {
    return state.app.navigation.gameplay() && !state.play.mine && !state.play.paused &&
           !state.play.tutorial.active() && state.play.modal == presentation::Modal::None &&
           std::any_of(ui.widgets.begin(), ui.widgets.end(), [](const engine::Widget &widget) -> bool {
               return widget.id == presentation::World && widget.enabled;
           });
}
bool MapCamera::act(presentation::Action action, engine::ScreenPoint position) {
    const std::size_t lane = static_cast<std::size_t>(action - presentation::LaneFocusBase);
    if ((action >= presentation::LaneFocusBase && action < presentation::LaneFocusBase + 8) ||
        action == presentation::MinimapFocus) {
        if (available() && (action == presentation::MinimapFocus || lane < layout.entrances.size())) {
            const game::Point target =
                action == presentation::MinimapFocus ? layout.minimapPoint(position) : layout.entrances[lane];
            state.play.camera = layout.camera.centered({target.x, target.y});
            ui.cancel();
            cancel();
        }
        return true;
    }
    if (action != presentation::ZoomIn && action != presentation::ZoomOut && action != presentation::ZoomFit)
        return false;
    if (available()) {
        state.play.camera = action == presentation::ZoomFit
                                ? engine::CameraPose{}
                                : layout.camera.zoomed(action == presentation::ZoomIn ? 1.3F : 1.0F / 1.3F,
                                                       layout.world.center());
        ui.cancel();
        cancel();
    }
    return true;
}
void MapCamera::cancel() {
    tracking_ = dragged_ = false;
}
bool MapCamera::pointer(engine::Pointer event) {
    if (event.phase == engine::PointerPhase::Cancel || !available()) {
        cancel();
        return false;
    }
    if (event.phase == engine::PointerPhase::Down && !tracking_ && ui.captured == presentation::World &&
        !state.play.selectedCard && !state.play.movingUnit) {
        tracking_ = true;
        dragged_ = false;
        pointerId_ = event.id;
        device_ = event.device;
        origin_ = event.position;
        initial_ = layout.camera;
    }
    if (!tracking_ || pointerId_ != event.id || device_ != event.device)
        return false;
    if (event.phase == engine::PointerPhase::Move && ui.dragging && ui.captured == presentation::World) {
        dragged_ = true;
        state.play.camera = initial_.moved({event.position.x - origin_.x, event.position.y - origin_.y});
    }
    if (event.phase == engine::PointerPhase::Up) {
        const bool consumed = dragged_;
        cancel();
        return consumed;
    }
    return false;
}
void MapCamera::wheel(engine::ScreenPoint point, float steps) {
    if (!available() || !layout.world.contains(point) || ui.captured || !std::isfinite(steps))
        return;
    const engine::Widget *target = ui.hit(point);
    if (!target || target->id != presentation::World || !target->enabled)
        return;
    state.play.camera = layout.camera.zoomed(std::pow(1.2F, std::clamp(steps, -8.0F, 8.0F)), point);
}
} // namespace thorium
