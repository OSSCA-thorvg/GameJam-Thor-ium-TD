#pragma once
#include "engine/audio/Audio.h"
#include <SDL3/SDL.h>
#include <cstdint>
#include <string>

namespace thorium::platform {
/** Own a playback-only SDL stream; main-thread PCM pumping avoids game/audio-thread races. */
class AudioDevice {
  public:
    /** Construct an unopened playback endpoint. */
    AudioDevice() = default;
    AudioDevice(const AudioDevice &) = delete;
    AudioDevice &operator=(const AudioDevice &) = delete;
    std::string error;
    std::uint64_t submittedFrames = 0;
    std::uint64_t audibleFrames = 0;
    /** Close the stream and its device before SDL subsystem shutdown. */
    ~AudioDevice();
    /** Initialize optional audio without making device failure fatal to the game. */
    bool open();
    /** Activate after a mouse/touch/key gesture; the browser handles its trusted unlock. */
    void activate();
    /** Stop device output and discard queued game sound on focus loss. */
    void suspend();
    /** Queue at most three short blocks, waiting for a running browser AudioContext. */
    void pump(engine::AudioOutput output);
    /** Read actual device creation independently of whether autoplay was unlocked. */
    bool available() const;
    /** Read whether playback was activated by input and not suspended by focus loss. */
    bool active() const;

  private:
    SDL_AudioStream *stream_ = nullptr;
    bool active_ = false;
    bool initialized_ = false;
    bool lastMuted_ = false;
    std::uint64_t generation_ = 0;
};
} // namespace thorium::platform
