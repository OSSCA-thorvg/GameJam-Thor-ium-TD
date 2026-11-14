#include "Audio.h"
#include <algorithm>
#include <cmath>

namespace thorium::engine {
void AudioOutput::render(float *samples, std::size_t frames) const {
    mixer_.render(samples, frames);
}
std::uint64_t AudioOutput::generation() const {
    return mixer_.generation;
}
bool AudioOutput::muted() const {
    return mixer_.settings.muted;
}
AudioOutput AudioMixer::output() {
    return AudioOutput(*this);
}

bool AudioClip::valid() const {
    return sampleRate >= 8000 && sampleRate <= 192000 && (channels == 1 || channels == 2) &&
           !samples.empty() && samples.size() % channels == 0 &&
           samples.size() / channels <= static_cast<std::size_t>(sampleRate) * 10 &&
           std::all_of(samples.begin(), samples.end(),
                       [](float value) { return std::isfinite(value) && std::abs(value) <= 1; });
}
AudioClip synthesize(Beep beep) {
    AudioClip clip;
    if (!std::isfinite(beep.startHz) || !std::isfinite(beep.endHz) || !std::isfinite(beep.seconds) ||
        !std::isfinite(beep.noise) || beep.startHz < 20 || beep.startHz > 12000 || beep.endHz < 20 ||
        beep.endHz > 12000 || beep.seconds <= .01F || beep.seconds > 1)
        return clip;
    const std::size_t frames = static_cast<std::size_t>(beep.seconds * clip.sampleRate);
    clip.samples.resize(frames);
    double phase = 0;
    std::uint32_t noise = 0x54a121e7;
    const float blend = std::clamp(beep.noise, 0.0F, 1.0F);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float t = static_cast<float>(frame) / std::max<std::size_t>(1, frames - 1);
        const float hz = beep.startHz + (beep.endHz - beep.startHz) * t;
        phase += 6.283185307179586 * hz / clip.sampleRate;
        noise ^= noise << 13;
        noise ^= noise >> 17;
        noise ^= noise << 5;
        const float random = (noise & 65535) / 32767.5F - 1;
        const float envelope =
            std::min(1.0F, t * beep.seconds / .005F) * std::min(1.0F, (1 - t) * beep.seconds / .015F);
        clip.samples[frame] =
            .5F * envelope * ((1 - blend) * static_cast<float>(std::sin(phase)) + blend * random);
    }
    clip.samples.front() = 0;
    clip.samples.back() = 0;
    return clip;
}
bool AudioMixer::setClip(SoundId id, AudioClip clip, SoundPolicy policy) {
    if (!clip.valid() || !std::isfinite(policy.gain) || !std::isfinite(policy.cooldown) ||
        policy.maxInstances < 1 || policy.maxInstances > 16)
        return false;
    policy.gain = std::clamp(policy.gain, 0.0F, 1.0F);
    policy.cooldown = std::clamp(policy.cooldown, 0.0F, 10.0F);
    clips_[id] = {std::make_shared<const AudioClip>(std::move(clip)), policy, -1000};
    return true;
}
bool AudioMixer::play(SoundId id, float pan) {
    const std::unordered_map<SoundId, Entry>::iterator found = clips_.find(id);
    if (found == clips_.end() || settings.muted || !std::isfinite(pan) || !std::isfinite(settings.volume) ||
        settings.volume <= 0) {
        ++suppressed;
        return false;
    }
    Entry &entry = found->second;
    if (clock_ - entry.last + 1e-6 < entry.policy.cooldown ||
        std::count_if(voices_.begin(), voices_.end(), [id](const Voice &voice) { return voice.id == id; }) >=
            entry.policy.maxInstances) {
        ++suppressed;
        return false;
    }
    if (voices_.size() >= MaxVoices) {
        const std::vector<Voice>::iterator weakest =
            std::min_element(voices_.begin(), voices_.end(),
                             [](const Voice &a, const Voice &b) { return a.priority < b.priority; });
        if (weakest->priority > entry.policy.priority) {
            ++suppressed;
            return false;
        }
        voices_.erase(weakest);
    }
    const float position = std::clamp(pan, -1.0F, 1.0F);
    voices_.push_back({id, entry.clip, 0, entry.policy.gain * std::sqrt((1 - position) * .5F),
                       entry.policy.gain * std::sqrt((1 + position) * .5F), entry.policy.priority});
    entry.last = clock_;
    ++played;
    return true;
}
void AudioMixer::advance(float seconds) {
    if (std::isfinite(seconds) && seconds > 0)
        clock_ += std::min(seconds, .1F);
}
void AudioMixer::render(float *output, std::size_t frames) {
    if (!output)
        return;
    std::fill(output, output + frames * Channels, 0.0F);
    if (settings.muted || !std::isfinite(settings.volume) || settings.volume <= 0) {
        voices_.clear();
        return;
    }
    for (Voice &voice : voices_) {
        const AudioClip &clip = *voice.clip;
        const std::size_t length = clip.samples.size() / clip.channels;
        const double step = static_cast<double>(clip.sampleRate) / SampleRate;
        for (std::size_t frame = 0; frame < frames && voice.cursor < length; ++frame, voice.cursor += step) {
            const std::size_t a = static_cast<std::size_t>(voice.cursor), b = std::min(a + 1, length - 1);
            const float fraction = static_cast<float>(voice.cursor - a);
            for (int channel = 0; channel < Channels; ++channel) {
                const int source = clip.channels == 1 ? 0 : channel;
                const float sample = clip.samples[a * clip.channels + source] * (1 - fraction) +
                                     clip.samples[b * clip.channels + source] * fraction;
                output[frame * Channels + channel] += sample * (channel == 0 ? voice.left : voice.right);
            }
        }
    }
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
                                 [](const Voice &voice) {
                                     return voice.cursor >= voice.clip->samples.size() / voice.clip->channels;
                                 }),
                  voices_.end());
    // The battle track fades in and out over about a second, at the main track's gain.
    const float target = battle_ && battleMusic_ ? 1.0F : 0.0F;
    const float step = static_cast<float>(frames) / SampleRate;
    blend_ = target > blend_ ? std::min(target, blend_ + step) : std::max(target, blend_ - step);
    const float gain = musicGain_ * std::clamp(settings.music, 0.0F, 1.0F);
    musicBlock_.resize(frames * Channels);
    for (const std::pair<MusicStream *, float> &track :
         {std::make_pair(music_.get(), 1 - blend_), std::make_pair(battleMusic_.get(), blend_)}) {
        if (!track.first || track.second <= 0)
            continue;
        track.first->read(musicBlock_.data(), frames, SampleRate);
        for (std::size_t sample = 0; sample < frames * Channels; ++sample)
            output[sample] += musicBlock_[sample] * gain * track.second;
    }
    const float volume = std::clamp(settings.volume, 0.0F, 1.0F);
    for (std::size_t sample = 0; sample < frames * Channels; ++sample)
        output[sample] = std::tanh(output[sample] * volume);
}
void AudioMixer::stop() {
    voices_.clear();
    ++generation;
}
bool AudioMixer::setMusic(std::unique_ptr<MusicStream> music, float gain) {
    if (!music || !music->loaded() || !std::isfinite(gain))
        return false;
    music_ = std::move(music);
    musicGain_ = std::clamp(gain, 0.0F, 1.0F);
    return true;
}
const MusicStream *AudioMixer::music() const {
    return music_.get();
}
bool AudioMixer::setBattleMusic(std::unique_ptr<MusicStream> music) {
    if (!music || !music->loaded())
        return false;
    battleMusic_ = std::move(music);
    return true;
}
void AudioMixer::setBattle(bool battle) {
    battle_ = battle;
}
float AudioMixer::battleBlend() const {
    return blend_;
}
std::size_t AudioMixer::activeVoices() const {
    return voices_.size();
}
} // namespace thorium::engine
