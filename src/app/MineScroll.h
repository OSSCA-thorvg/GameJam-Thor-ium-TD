#pragma once
#include "game/presentation/Actions.h"
#include "game/presentation/Layout.h"
#include "game/presentation/State.h"
#include "game/rules/Run.h"

namespace thorium {
/** Translate wheel and drag input over the cutaway into vertical board scrolling, and follow the miner. */
class MineScroll {
  public:
    /** Borrow the frame geometry that the view used for the last cutaway. */
    MineScroll(const game::Run &run, presentation::State &state, engine::Ui &ui,
               const presentation::Layout &layout)
        : run(run), state(state), ui(ui), layout(layout) {}
    /** Observe pointer input after UI capture; true consumes a completed scroll drag. */
    bool pointer(engine::Pointer event);
    /** Scroll by wheel steps while the cutaway is visible; true when the wheel was consumed. */
    bool wheel(float steps);
    /** Keep the walking or digging miner visible and reset the scroll when the layer changes. */
    void follow(float elapsed);
    /** Cancel an unfinished drag gesture. */
    void cancel();

  private:
    const game::Run &run;
    presentation::State &state;
    engine::Ui &ui;
    const presentation::Layout &layout;
    std::int64_t pointerId_ = 0;
    engine::PointerDevice device_ = engine::PointerDevice::Mouse;
    engine::ScreenPoint origin_;
    float initial_ = 0.0F;
    bool tracking_ = false;
    bool dragged_ = false;
    int layer_ = -1;
    /** Check that the preparation cutaway is the active, unobstructed view. */
    bool available() const;
    /** Clamp a scroll value in tiles to the last measured board. */
    float clamp(float tiles) const;
};
} // namespace thorium
