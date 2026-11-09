#include "FrameProfiler.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <vector>

namespace thorium::engine {
namespace {
/** Select a nearest-rank percentile from a nonempty sorted sample set. */
double percentile(const std::vector<double> &values, double fraction) {
    const std::size_t index = static_cast<std::size_t>(std::ceil(values.size() * fraction)) - 1;
    return values[std::min(index, values.size() - 1)];
}
/** Write stable stage names; other work includes platform setup and instrumentation overhead. */
void writeTiming(std::ostringstream &output, const FrameTimings &timing) {
    constexpr std::array<const char *, static_cast<std::size_t>(FrameStage::Count)> names{
        "simulation", "presentation", "cachePreparation", "scene", "raster", "audio", "upload", "present"};
    output << "{\"total\":" << timing.totalMs << ",\"interval\":" << timing.intervalMs
           << ",\"steps\":" << timing.simulationSteps;
    for (std::size_t index = 0; index < names.size(); ++index)
        output << ",\"" << names[index] << "\":" << timing.stages[index];
    output << '}';
}
} // namespace
void FrameTimings::add(FrameStage stage, double milliseconds) {
    stages[static_cast<std::size_t>(stage)] += milliseconds;
}
double FrameTimings::stage(FrameStage stage) const {
    return stages[static_cast<std::size_t>(stage)];
}
double FrameTimings::measuredMs() const {
    return std::accumulate(stages.begin(), stages.end(), 0.0);
}
double frameMilliseconds(FrameClock::time_point start) {
    return std::chrono::duration<double, std::milli>(FrameClock::now() - start).count();
}
bool FrameProfiler::record(const FrameTimings &timings) {
    if (!std::isfinite(timings.totalMs) || timings.totalMs < 0 || !std::isfinite(timings.intervalMs) ||
        timings.intervalMs < 0 ||
        !std::all_of(
            timings.stages.begin(), timings.stages.end(),
            [](double value) -> bool { return std::isfinite(value) && value >= 0; }) ||
        timings.measuredMs() > timings.totalMs + .000001)
        return false;
    history_[next_] = timings;
    next_ = (next_ + 1) % Window;
    count_ = std::min(count_ + 1, Window);
    ++frames_;
    return true;
}
FrameSummary FrameProfiler::summary() const {
    FrameSummary result;
    result.frames = frames_;
    result.samples = count_;
    if (count_ == 0)
        return result;
    result.latest = history_[(next_ + Window - 1) % Window];
    std::vector<double> values;
    values.reserve(count_);
    // Stage percentiles are independent; their sum is not a percentile of the total.
    for (std::size_t field = 0; field < static_cast<std::size_t>(FrameStage::Count) + 3; ++field) {
        values.clear();
        for (std::size_t index = 0; index < count_; ++index) {
            const FrameTimings &sample = history_[index];
            values.push_back(field < sample.stages.size()        ? sample.stages[field]
                             : field == sample.stages.size()     ? sample.totalMs
                             : field == sample.stages.size() + 1 ? sample.intervalMs
                                                                 : sample.simulationSteps);
        }
        std::sort(values.begin(), values.end());
        const double p50 = percentile(values, .5), p95 = percentile(values, .95);
        if (field < result.p50.stages.size()) {
            result.p50.stages[field] = p50;
            result.p95.stages[field] = p95;
        } else if (field == result.p50.stages.size()) {
            result.p50.totalMs = p50;
            result.p95.totalMs = p95;
        } else if (field == result.p50.stages.size() + 1) {
            result.p50.intervalMs = p50;
            result.p95.intervalMs = p95;
        } else {
            result.p50.simulationSteps = static_cast<std::uint32_t>(p50);
            result.p95.simulationSteps = static_cast<std::uint32_t>(p95);
        }
    }
    return result;
}
std::string FrameProfiler::debugState() const {
    const FrameSummary result = summary();
    std::ostringstream output;
    output << "{\"samples\":" << result.samples << ",\"frames\":" << result.frames << ",\"latest\":";
    writeTiming(output, result.latest);
    output << ",\"other\":" << std::max(0.0, result.latest.totalMs - result.latest.measuredMs())
           << ",\"p50\":";
    writeTiming(output, result.p50);
    output << ",\"p95\":";
    writeTiming(output, result.p95);
    return output.str() + '}';
}
} // namespace thorium::engine
