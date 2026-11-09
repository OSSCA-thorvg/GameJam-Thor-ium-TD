#include "Ui.h"
#include <algorithm>
#include <cmath>

namespace thorium::engine {
bool Rect::contains(ScreenPoint point) const {
    return point.x >= x && point.y >= y && point.x < x + w && point.y < y + h;
}
ScreenPoint Rect::center() const {
    return {x + w * 0.5F, y + h * 0.5F};
}
void Ui::beginFrame() {
    widgets.clear();
}
void Ui::add(WidgetId id, Rect bounds, bool enabled, bool draggable, bool focusable, bool inspectable) {
    widgets.push_back({id, bounds, enabled, draggable, focusable, inspectable});
}
const Widget *Ui::hit(ScreenPoint position) const {
    for (std::vector<Widget>::const_reverse_iterator item = widgets.rbegin(); item != widgets.rend(); ++item)
        if (item->bounds.contains(position))
            return &*item;
    return nullptr;
}
const Widget *Ui::find(WidgetId id) const {
    for (const Widget &item : widgets)
        if (item.id == id)
            return &item;
    return nullptr;
}
void Ui::releaseCapture() {
    captured = 0;
    dragging = false;
    draggable_ = false;
    enabledOnPress_ = false;
    moved_ = false;
    holdConsumed_ = false;
    pressTime_ = 0;
}
void Ui::cancel() {
    releaseCapture();
    hover_ = held_ = 0;
    hoverTime_ = 0;
    pointerKnown_ = false;
    keyboardInspection_ = false;
}
void Ui::advance(float elapsed) {
    const float delta = std::isfinite(elapsed) ? std::clamp(elapsed, 0.0F, .1F) : 0;
    if (captured) {
        hover_ = 0;
        hoverTime_ = 0;
        const Widget *item = find(captured);
        if (!holdConsumed_ && !moved_ && enabledOnPress_ && item && item->enabled && item->inspectable) {
            pressTime_ += delta;
            if (pressTime_ >= .45F) {
                held_ = captured;
                holdConsumed_ = true;
                dragging = false;
            }
        }
        return;
    }
    const Widget *item = pointerKnown_ && device_ == PointerDevice::Mouse ? hit(pointerPosition) : nullptr;
    const WidgetId target = item && item->enabled && item->inspectable ? item->id : 0;
    if (hover_ != target) {
        hover_ = target;
        hoverTime_ = 0;
    } else
        hoverTime_ += delta;
}
WidgetId Ui::inspected() const {
    const WidgetId id = held_ ? held_ : keyboardInspection_ ? focused : hoverTime_ >= .3F ? hover_ : 0;
    const Widget *item = find(id);
    if (!id || !item || !item->enabled || !item->inspectable)
        return 0;
    const Widget *visible = hit(held_ || keyboardInspection_ ? item->bounds.center() : pointerPosition);
    return visible && visible->id == id ? id : 0;
}
std::optional<Interaction> Ui::pointer(Pointer event) {
    if (event.phase == PointerPhase::Cancel) {
        if (captured && event.id == pointerId_ && event.device == device_)
            cancel();
        return std::nullopt;
    }
    if (event.phase == PointerPhase::Down) {
        if (captured)
            return std::nullopt;
        const bool dismissInspection = held_ != 0;
        pointerPosition = event.position;
        device_ = event.device;
        pointerKnown_ = true;
        keyboardInspection_ = false;
        hover_ = held_ = 0;
        hoverTime_ = pressTime_ = 0;
        for (std::vector<Widget>::const_reverse_iterator it = widgets.rbegin(); it != widgets.rend(); ++it)
            if (it->bounds.contains(event.position)) {
                captured = it->id;
                focused = it->enabled && it->focusable ? it->id : focused;
                pointerId_ = event.id;
                origin_ = event.position;
                draggable_ = it->enabled && it->draggable;
                enabledOnPress_ = it->enabled;
                dragging = false;
                moved_ = false;
                holdConsumed_ = dismissInspection;
                return std::nullopt;
            }
        return std::nullopt;
    }
    if (captured && (event.id != pointerId_ || event.device != device_))
        return std::nullopt;
    pointerPosition = event.position;
    if (!captured) {
        device_ = event.device;
        pointerKnown_ = true;
        keyboardInspection_ = false;
        if (event.phase == PointerPhase::Move)
            held_ = 0;
    }
    if (event.phase == PointerPhase::Move && captured) {
        moved_ = moved_ || std::hypot(event.position.x - origin_.x, event.position.y - origin_.y) > 7.0F;
        dragging = draggable_ && moved_ && !holdConsumed_;
        return std::nullopt;
    }
    if (event.phase != PointerPhase::Up || !captured)
        return std::nullopt;
    const WidgetId id = captured;
    const bool dropped = dragging;
    const bool eligible = enabledOnPress_;
    const bool inspectedHold = holdConsumed_;
    releaseCapture();
    if (!eligible || inspectedHold)
        return std::nullopt;
    for (const Widget &widget : widgets)
        if (widget.id == id && widget.enabled) {
            if (dropped && widget.draggable)
                return Interaction{Interaction::Kind::Drop, id, event.position};
            if (widget.bounds.contains(event.position))
                return Interaction{Interaction::Kind::Click, id, event.position};
            break;
        }
    return std::nullopt;
}
void Ui::nextFocus(bool backwards) {
    held_ = hover_ = 0;
    keyboardInspection_ = true;
    std::vector<WidgetId> enabled;
    for (const Widget &widget : widgets)
        if (widget.enabled && widget.focusable)
            enabled.push_back(widget.id);
    if (enabled.empty()) {
        focused = 0;
        return;
    }
    const std::vector<WidgetId>::iterator current = std::find(enabled.begin(), enabled.end(), focused);
    int index = current == enabled.end() ? (backwards ? 0 : -1) : static_cast<int>(current - enabled.begin());
    index =
        (index + (backwards ? -1 : 1) + static_cast<int>(enabled.size())) % static_cast<int>(enabled.size());
    focused = enabled[index];
}
std::optional<Interaction> Ui::activate() const {
    if (held_)
        return std::nullopt;
    for (const Widget &widget : widgets)
        if (widget.id == focused && widget.enabled && widget.focusable)
            return Interaction{Interaction::Kind::Click, focused, widget.bounds.center()};
    return std::nullopt;
}
} // namespace thorium::engine
