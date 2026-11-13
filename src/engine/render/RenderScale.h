#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace thorium::engine {
/** Lower the render pixel density while frames run over budget and restore it once they have headroom.
 * The scale multiplies the display density a frame asks for; the renderer still keeps at least one physical
 * pixel per logical pixel. */
class RenderScale {
  public:
    /** Density multipliers from full resolution down to the coarsest step. */
    static constexpr std::array<float, 3> Levels{{1.0F, .85F, .7F}};
    /** Smoothed CPU work per frame above which the scale steps down, in milliseconds. */
    static constexpr double Lower = 13.5;
    /** Smoothed CPU work per frame below which the scale steps back up, in milliseconds. */
    static constexpr double Raise = 8.5;
    /** Seconds the scale holds after stepping down and after stepping up. */
    static constexpr float HoldAfterLower = 1.5F;
    static constexpr float HoldAfterRaise = 4.0F;
    /** Account one finished frame's CPU work and its length, and return the multiplier for the next frame.
     * Non-finite or negative samples are ignored. */
    float update(double workMs, float seconds) {
        if (!std::isfinite(workMs) || workMs < 0 || !std::isfinite(seconds) || seconds < 0)
            return value();
        smoothed_ += (workMs - smoothed_) * .1;
        hold_ = std::max(0.0F, hold_ - seconds);
        if (hold_ > 0)
            return value();
        if (smoothed_ > Lower && level_ + 1 < Levels.size()) {
            ++level_;
            hold_ = HoldAfterLower;
        } else if (smoothed_ < Raise && level_ > 0) {
            --level_;
            hold_ = HoldAfterRaise;
        }
        return value();
    }
    /** Read the current density multiplier. */
    float value() const {
        return Levels[level_];
    }
    /** Read the smoothed CPU work per frame in milliseconds. */
    double smoothedMs() const {
        return smoothed_;
    }

  private:
    std::size_t level_ = 0;
    double smoothed_ = 0;
    float hold_ = 0;
};
} // namespace thorium::engine
