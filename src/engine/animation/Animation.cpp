#include "Animation.h"
#include <algorithm>
#include <cmath>

namespace thorium::engine {
bool AnimationClip::valid() const {
    return !asset.empty() && std::isfinite(frames.begin) && std::isfinite(frames.end) && frames.begin >= 0 &&
           frames.end > frames.begin && std::isfinite(duration) && duration > 0;
}
bool AnimationClip::operator==(const AnimationClip &other) const {
    return asset == other.asset && frames.begin == other.frames.begin && frames.end == other.frames.end &&
           duration == other.duration && playback == other.playback;
}
std::optional<AnimationClip> animationClip(const std::string &asset, const AnimationInfo &info,
                                           const std::string &marker, Playback playback) {
    const std::unordered_map<std::string, FrameRange>::const_iterator range = info.markers.find(marker);
    if (range == info.markers.end() || !std::isfinite(info.frames) || info.frames <= 0 ||
        !std::isfinite(info.duration) || info.duration <= 0 || range->second.end > info.frames)
        return std::nullopt;
    const AnimationClip result{asset, range->second,
                               info.duration * (range->second.end - range->second.begin) / info.frames,
                               playback};
    return result.valid() ? std::optional<AnimationClip>{result} : std::nullopt;
}
AnimationSample loopingSample(const AnimationClip &clip, float seconds) {
    const double time = std::isfinite(seconds) ? std::max(0.0, static_cast<double>(seconds)) : 0;
    const double progress = clip.valid() ? std::fmod(time, clip.duration) / clip.duration : 0;
    return {clip.asset,
            clip.frames.begin +
                static_cast<float>(progress) * std::max(0.0F, clip.frames.end - clip.frames.begin - 1),
            clip.frames};
}
bool AnimationPlayer::play(const AnimationClip &clip, Restart restart) {
    if (!clip.valid())
        return false;
    if (clip_ && *clip_ == clip && restart == Restart::IfChanged)
        return true;
    clip_ = clip;
    elapsed_ = 0;
    finished_ = false;
    completion_ = false;
    paused_ = false;
    return true;
}
void AnimationPlayer::stop() {
    clip_.reset();
    elapsed_ = 0;
    finished_ = false;
    completion_ = false;
    paused_ = false;
}
void AnimationPlayer::pause() {
    paused_ = true;
}
void AnimationPlayer::resume() {
    paused_ = false;
}
bool AnimationPlayer::setSpeed(float speed) {
    if (!std::isfinite(speed) || speed < 0)
        return false;
    speed_ = speed;
    return true;
}
void AnimationPlayer::advance(float seconds) {
    if (!clip_ || paused_ || finished_ || clip_->playback == Playback::Pose || !std::isfinite(seconds) ||
        seconds <= 0)
        return;
    const double time = elapsed_ + static_cast<double>(seconds) * speed_;
    if (clip_->playback == Playback::Loop)
        elapsed_ = std::fmod(time, clip_->duration);
    else {
        elapsed_ = std::min(time, static_cast<double>(clip_->duration));
        if (elapsed_ >= clip_->duration) {
            finished_ = true;
            completion_ = true;
        }
    }
}
std::optional<AnimationSample> AnimationPlayer::sample() const {
    if (!clip_)
        return std::nullopt;
    const double progressTime = clip_->playback == Playback::Loop
                                    ? std::fmod(elapsed_ / clip_->duration + loopPhase_, 1.0)
                                    : elapsed_ / clip_->duration;
    const float progress = static_cast<float>(progressTime);
    return AnimationSample{clip_->asset,
                           clip_->frames.begin +
                               progress * std::max(0.0F, clip_->frames.end - clip_->frames.begin - 1),
                           clip_->frames};
}
bool AnimationPlayer::setLoopPhase(float phase) {
    if (!std::isfinite(phase) || phase < 0 || phase >= 1)
        return false;
    loopPhase_ = phase;
    return true;
}
float AnimationPlayer::loopPhase() const {
    return loopPhase_;
}
bool AnimationPlayer::finished() const {
    return finished_;
}
bool AnimationPlayer::takeCompletion() {
    const bool result = completion_;
    completion_ = false;
    return result;
}
float AnimationPlayer::elapsed() const {
    return static_cast<float>(elapsed_);
}
} // namespace thorium::engine
