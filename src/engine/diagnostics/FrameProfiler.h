#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <string>

namespace thorium::engine {
using FrameClock = std::chrono::steady_clock;
/** Mutually exclusive CPU intervals; presentation output may include driver/VSync waiting. */
enum class FrameStage {
    Simulation,
    Presentation,
    CachePreparation,
    Scene,
    Raster,
    Audio,
    Upload,
    Present,
    Count
};
/** One completed iteration; interval measures cadence while total measures the work inside it. */
struct FrameTimings {
    std::array<double, static_cast<std::size_t>(FrameStage::Count)> stages{};
    double totalMs = 0;
    double intervalMs = 0;
    std::uint32_t simulationSteps = 0;
    /** Accumulate one non-overlapping stage, including multiple fixed simulation steps. */
    void add(FrameStage stage, double milliseconds);
    /** Read one named stage without exposing platform timing mechanisms. */
    double stage(FrameStage stage) const;
    /** Sum measured stages; remaining iteration work is reported separately. */
    double measuredMs() const;
};
/** Convert a monotonic interval into milliseconds without using the game clock. */
double frameMilliseconds(FrameClock::time_point start);
/** Summarize bounded history; each percentile is computed independently per stage. */
struct FrameSummary {
    FrameTimings latest;
    FrameTimings p50;
    FrameTimings p95;
    std::size_t samples = 0;
    std::uint64_t frames = 0;
};
/** Keep a fixed-size diagnostic window; recording never sorts or allocates. */
class FrameProfiler {
  public:
    static constexpr std::size_t Window = 120;
    /** Record only finite, nonnegative, non-overlapping completed intervals. */
    bool record(const FrameTimings &timings);
    /** Compute percentiles on demand, outside the normal frame hot path. */
    FrameSummary summary() const;
    /** Serialize read-only CPU diagnostics for the native and WASM adapters. */
    std::string debugState() const;

  private:
    std::array<FrameTimings, Window> history_{};
    std::size_t next_ = 0;
    std::size_t count_ = 0;
    std::uint64_t frames_ = 0;
};
} // namespace thorium::engine
