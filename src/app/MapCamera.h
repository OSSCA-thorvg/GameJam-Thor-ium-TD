#pragma once
#include "game/presentation/Actions.h"
#include "game/presentation/Layout.h"
#include "game/presentation/State.h"

namespace thorium {
/** Translate ordinary world drags and wheel input into camera intent without issuing gameplay commands. */
class MapCamera {
  public:
    /** Borrow the same immutable frame geometry used for placement previews. */
    MapCamera(presentation::State &state, engine::Ui &ui, const presentation::Layout &layout)
        : state(state), ui(ui), layout(layout) {}
    /** Observe after UI capture resolution; true consumes a completed map drag. */
    bool pointer(engine::Pointer event);
    /** Zoom around a logical world position without claiming card/UI pointer capture. */
    void wheel(engine::ScreenPoint point, float steps);
    /** Interpret only camera controls, leaving all gameplay actions to their own boundary. */
    bool act(presentation::Action action, engine::ScreenPoint position = {});
    /** Cancel a gesture on navigation, focus loss or viewport changes. */
    void cancel();

  private:
    presentation::State &state;
    engine::Ui &ui;
    const presentation::Layout &layout;
    engine::Camera2D initial_;
    engine::ScreenPoint origin_;
    std::int64_t pointerId_ = 0;
    engine::PointerDevice device_ = engine::PointerDevice::Mouse;
    bool tracking_ = false;
    bool dragged_ = false;
    /** Gate camera input independently of simulation while respecting actual modal ownership. */
    bool available() const;
};
} // namespace thorium
