#include "AudioDevice.h"
#include <array>
#include <cmath>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace thorium::platform {
AudioDevice::~AudioDevice() {
    if (stream_)
        SDL_DestroyAudioStream(stream_);
    if (initialized_)
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
}
bool AudioDevice::open() {
    if (stream_)
        return true;
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512");
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        error = SDL_GetError();
        return false;
    }
    initialized_ = true;
    const SDL_AudioSpec spec{SDL_AUDIO_F32, engine::AudioMixer::Channels, engine::AudioMixer::SampleRate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!stream_) {
        error = SDL_GetError();
        return false;
    }
    return true;
}
void AudioDevice::activate() {
    if (stream_ && !active_)
        active_ = SDL_ResumeAudioStreamDevice(stream_);
}
void AudioDevice::suspend() {
    if (stream_) {
        SDL_PauseAudioStreamDevice(stream_);
        SDL_ClearAudioStream(stream_);
    }
    active_ = false;
}
void AudioDevice::pump(engine::AudioOutput output) {
    if (!stream_)
        return;
    if (generation_ != output.generation() || lastMuted_ != output.muted()) {
        SDL_ClearAudioStream(stream_);
        generation_ = output.generation();
        lastMuted_ = output.muted();
    }
    if (!active_)
        return;
#ifdef __EMSCRIPTEN__
    if (!EM_ASM_INT({
            return Module["SDL3"] && Module["SDL3"].audioContext &&
                   Module["SDL3"].audioContext.state == "running";
        }))
        return;
#endif
    constexpr std::size_t frames = 512;
    constexpr int bytes = frames * engine::AudioMixer::Channels * sizeof(float);
    const int queued = SDL_GetAudioStreamQueued(stream_);
    if (queued < 0) {
        error = SDL_GetError();
        return;
    }
    for (int remaining = (3 * bytes - queued) / bytes; remaining > 0; --remaining) {
        std::array<float, frames * engine::AudioMixer::Channels> samples{};
        output.render(samples.data(), frames);
        if (!SDL_PutAudioStreamData(stream_, samples.data(), bytes)) {
            error = SDL_GetError();
            return;
        }
        submittedFrames += frames;
        for (std::size_t frame = 0; frame < frames; ++frame)
            if (std::abs(samples[frame * 2]) + std::abs(samples[frame * 2 + 1]) > .00001F)
                ++audibleFrames;
    }
}
bool AudioDevice::available() const {
    return stream_ != nullptr;
}
bool AudioDevice::active() const {
    return active_;
}
} // namespace thorium::platform
