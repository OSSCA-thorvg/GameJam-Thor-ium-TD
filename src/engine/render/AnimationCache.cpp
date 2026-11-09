#include "AnimationCache.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <tuple>

namespace thorium::engine {
bool AnimationFrameKey::operator<(const AnimationFrameKey &other) const {
    return std::tie(asset, revision, frame, size, padding) <
           std::tie(other.asset, other.revision, other.frame, other.size, other.padding);
}
std::size_t AnimationRasterLayout::bytes() const {
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * sizeof(std::uint32_t);
}
std::size_t AnimationRasterFrame::bytes() const {
    return pixels.capacity() * sizeof(std::uint32_t);
}
std::uint64_t animationRevision(const std::string &source) {
    std::uint64_t result = 14695981039346656037ULL;
    for (unsigned char byte : source) {
        result ^= byte;
        result *= 1099511628211ULL;
    }
    return result;
}
int animationSampleCount(FrameRange range, const AnimationInfo &info, float framesPerSecond) {
    if (!std::isfinite(range.begin) || !std::isfinite(range.end) || range.end <= range.begin ||
        !std::isfinite(info.frames) || info.frames <= 0 || !std::isfinite(info.duration) ||
        info.duration <= 0 || !std::isfinite(framesPerSecond) || framesPerSecond < 1 || framesPerSecond > 120)
        return 0;
    const double duration = static_cast<double>(info.duration) * (range.end - range.begin) / info.frames;
    return static_cast<int>(std::clamp(std::ceil(duration * framesPerSecond - .00001), 1.0, 4096.0));
}
std::optional<float> sampledAnimationFrame(const AnimationSample &sample, const AnimationInfo &info,
                                           float framesPerSecond) {
    if (!std::isfinite(sample.frame) || !std::isfinite(info.frames) || info.frames <= 0 ||
        !std::isfinite(info.duration) || info.duration <= 0 || sample.frame < 0 ||
        sample.frame >= info.frames || !std::isfinite(framesPerSecond) || framesPerSecond < 1 ||
        framesPerSecond > 120)
        return std::nullopt;
    const bool unspecified = sample.frames.begin == 0 && sample.frames.end == 0;
    const FrameRange range = unspecified ? FrameRange{0, info.frames} : sample.frames;
    if (!std::isfinite(range.begin) || !std::isfinite(range.end) || range.begin < 0 ||
        range.end <= range.begin || range.end > info.frames || sample.frame < range.begin ||
        sample.frame > std::max(range.begin, range.end - 1))
        return std::nullopt;
    const double span = std::max(0.0, static_cast<double>(range.end - range.begin - 1));
    if (span == 0)
        return range.begin;
    const double steps = std::max(1, animationSampleCount(range, info, framesPerSecond) - 1);
    const double progress = std::clamp((sample.frame - range.begin) / span, 0.0, 1.0);
    return range.begin + static_cast<float>(std::round(progress * steps) / steps * span);
}
std::optional<int> animationRasterSize(float requiredPixels, const AnimationCacheConfig &config) {
    if (!std::isfinite(requiredPixels) || requiredPixels <= 0)
        return std::nullopt;
    int result = 0;
    for (int size : config.sizes)
        if (size > 0 && size <= 1024 && size >= requiredPixels && (result == 0 || size < result))
            result = size;
    return result ? std::optional<int>{result} : std::nullopt;
}
AnimationRasterLayout animationRasterLayout(int size, int padding, float sourceWidth, float sourceHeight) {
    if (size <= 0 || size > 1024 || padding < 0 || padding > size || !std::isfinite(sourceWidth) ||
        !std::isfinite(sourceHeight) || sourceWidth <= 0 || sourceHeight <= 0)
        return {};
    const float longest = std::max(sourceWidth, sourceHeight);
    const int width = std::max(1, static_cast<int>(std::round(size * sourceWidth / longest)));
    const int height = std::max(1, static_cast<int>(std::round(size * sourceHeight / longest)));
    return {width, height, padding, width + 2 * padding, height + 2 * padding, -padding, -padding};
}
AnimationFrameCache::AnimationFrameCache(std::size_t budgetBytes) : budget_(budgetBytes) {}
bool AnimationFrameCache::evict() {
    std::map<AnimationFrameKey, Entry>::iterator oldest = entries_.end();
    for (std::map<AnimationFrameKey, Entry>::iterator it = entries_.begin(); it != entries_.end(); ++it)
        if (it->second.frame.use_count() == 1 &&
            (oldest == entries_.end() || it->second.used < oldest->second.used))
            oldest = it;
    if (oldest == entries_.end())
        return false;
    stats_.bytes -= oldest->second.frame->bytes();
    entries_.erase(oldest);
    ++stats_.evictions;
    return true;
}
AnimationFrameHandle AnimationFrameCache::acquire(const AnimationFrameKey &key, std::size_t expectedBytes,
                                                  const Factory &factory) {
    if (key.asset.empty() || key.size < 1 || key.size > 1024 || key.padding < 0 || key.padding > key.size ||
        !std::isfinite(key.frame) || key.frame < 0 || expectedBytes == 0 || expectedBytes > budget_) {
        ++stats_.rejected;
        return {};
    }
    const std::map<AnimationFrameKey, Entry>::iterator found = entries_.find(key);
    if (found != entries_.end()) {
        ++stats_.hits;
        found->second.used = ++clock_;
        return found->second.frame;
    }
    ++stats_.misses;
    while (stats_.bytes > budget_ - expectedBytes)
        if (!evict()) {
            ++stats_.rejected;
            return {};
        }
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    const AnimationFrameHandle frame = factory();
    stats_.bakeMilliseconds +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (!frame || frame->bytes() == 0 || frame->layout.width <= 0 || frame->layout.height <= 0 ||
        frame->bytes() > expectedBytes ||
        frame->pixels.size() != frame->layout.bytes() / sizeof(std::uint32_t)) {
        ++stats_.rejected;
        return {};
    }
    entries_.emplace(key, Entry{frame, ++clock_});
    stats_.bytes += frame->bytes();
    ++stats_.bakes;
    return frame;
}
void AnimationFrameCache::clearUnused() {
    while (evict()) {
    }
}
AnimationCacheStats AnimationFrameCache::stats() const {
    AnimationCacheStats result = stats_;
    result.entries = entries_.size();
    for (const std::pair<const AnimationFrameKey, Entry> &entry : entries_)
        if (entry.second.frame.use_count() > 1)
            ++result.pinnedEntries;
    return result;
}
} // namespace thorium::engine
