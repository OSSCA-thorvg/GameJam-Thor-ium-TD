#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace thorium::engine {
using WidgetId = std::uint32_t;
/** Logical screen coordinates; never carry SDL-specific event fields. */
struct ScreenPoint {
    float x = 0;
    float y = 0;
};
/** A logical rectangular interaction area. */
struct Rect {
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;
    /** Test half-open bounds, preventing two adjacent controls from claiming one edge. */
    bool contains(ScreenPoint point) const;
    /** Return the center used by keyboard activation. */
    ScreenPoint center() const;
};
/** Platform-neutral pointer phase, including explicit cancellation. */
enum class PointerPhase { Down, Move, Up, Cancel };
/** Distinguish hover-capable pointers from touch without guessing from an identifier. */
enum class PointerDevice { Mouse, Touch };
/** A normalized mouse or touch input, with a stable pointer identity. */
struct Pointer {
    PointerPhase phase;
    std::int64_t id;
    ScreenPoint position;
    PointerDevice device = PointerDevice::Mouse;
};
/** One frame's registered control; state is retained by stable ID. */
struct Widget {
    WidgetId id;
    Rect bounds;
    bool enabled;
    bool draggable;
    bool focusable;
    bool inspectable = false;
};
/** A committed UI interaction, interpreted by game presentation outside the engine. */
struct Interaction {
    enum class Kind { Click, Drop } kind;
    WidgetId widget;
    ScreenPoint position;
};

/** Own focus and one pointer capture; generic UI has no game or renderer dependency. */
class Ui {
  public:
    std::vector<Widget> widgets;
    WidgetId focused = 0;
    WidgetId captured = 0;
    ScreenPoint pointerPosition;
    bool dragging = false;
    /** Clear geometry while preserving capture/focus across frame reconstruction. */
    void beginFrame();
    /** Register controls in paint order; the topmost hit wins even if disabled. */
    void add(WidgetId id, Rect bounds, bool enabled = true, bool draggable = false, bool focusable = true,
             bool inspectable = false);
    /** Advance real UI time for hover/hold inspection independently of simulation and animation settings. */
    void advance(float elapsed);
    /** Read the currently visible inspection target; geometry is rechecked after every frame rebuild. */
    WidgetId inspected() const;
    /** Consume a pointer and emit at most one click or drop on release. */
    std::optional<Interaction> pointer(Pointer event);
    /** Cancel an unfinished gesture without producing an action. */
    void cancel();
    /** Move keyboard focus through enabled controls. */
    void nextFocus(bool backwards);
    /** Activate the focused control without fabricating pointer events. */
    std::optional<Interaction> activate() const;

  private:
    std::int64_t pointerId_ = 0;
    ScreenPoint origin_;
    bool draggable_ = false;
    bool enabledOnPress_ = false;
    PointerDevice device_ = PointerDevice::Mouse;
    bool pointerKnown_ = false;
    bool moved_ = false;
    bool holdConsumed_ = false;
    bool keyboardInspection_ = false;
    WidgetId hover_ = 0;
    WidgetId held_ = 0;
    float hoverTime_ = 0;
    float pressTime_ = 0;
    /** Resolve the topmost hit, including disabled occluders. */
    const Widget *hit(ScreenPoint position) const;
    /** Find a current registration by stable ID. */
    const Widget *find(WidgetId id) const;
    /** Release capture without discarding a completed inspect-only touch gesture. */
    void releaseCapture();
};
} // namespace thorium::engine
