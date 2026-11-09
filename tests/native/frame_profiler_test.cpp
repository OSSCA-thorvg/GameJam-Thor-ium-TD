#include "engine/diagnostics/FrameProfiler.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace thorium::engine;
namespace {
int checks = 0;
/** Assert contracts in release builds as well as debug builds. */
void require(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Exercise the actual ring window, percentile semantics and malformed input rejection. */
void historyContracts() {
    FrameProfiler profiler;
    require(profiler.summary().samples == 0 && profiler.summary().frames == 0, "empty profiler is readable");
    for (int index = 1; index <= 120; ++index) {
        FrameTimings timing;
        timing.add(FrameStage::Simulation, index / 2.0);
        timing.add(FrameStage::Simulation, index / 2.0);
        timing.totalMs = index + 1.0;
        timing.intervalMs = index + 2.0;
        timing.simulationSteps = 2;
        require(profiler.record(timing), "valid mutually exclusive timings are accepted");
    }
    const FrameSummary initial = profiler.summary();
    require(initial.samples == 120 && initial.frames == 120 && initial.latest.totalMs == 121,
            "window retains the latest complete iteration");
    require(initial.p50.stage(FrameStage::Simulation) == 60 && initial.p95.totalMs == 115,
            "nearest-rank percentiles are computed from actual samples");
    FrameTimings small;
    small.totalMs = 1;
    for (std::size_t index = 0; index < FrameProfiler::Window; ++index)
        require(profiler.record(small), "history wraps without growing");
    require(profiler.summary().samples == FrameProfiler::Window && profiler.summary().frames == 240 &&
                profiler.summary().p95.totalMs == 1,
            "expired spikes leave the bounded window");
    FrameTimings invalid = small;
    invalid.add(FrameStage::Audio, 2);
    require(!profiler.record(invalid), "overlapping or overcounted stages are rejected");
    invalid = small;
    invalid.totalMs = std::numeric_limits<double>::quiet_NaN();
    require(!profiler.record(invalid), "NaN cannot poison percentile history or JSON");
    invalid = small;
    invalid.add(FrameStage::Upload, -1);
    require(!profiler.record(invalid) && profiler.summary().frames == 240,
            "invalid measurements never advance the frame sequence");
    require(profiler.debugState().find("\"upload\"") != std::string::npos &&
                profiler.debugState().find("\"other\":1") != std::string::npos,
            "diagnostics include output work and uninstrumented iteration overhead");
}
} // namespace
/** Run deterministic profiling contracts without a graphics, audio or platform device. */
int main() {
    historyContracts();
    std::cout << checks << " frame profiler checks passed\n";
}
