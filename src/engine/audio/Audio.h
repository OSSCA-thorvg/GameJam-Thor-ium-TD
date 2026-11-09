#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace thorium::engine {
using SoundId = std::uint16_t;
/** User preferences, retained when a game run is reset. */
struct AudioSettings {
    float volume = .45F;
    bool muted = false;
};
/** Decoded, interleaved PCM; sound identity is independent of its file format. */
struct AudioClip {
    int sampleRate = 48000;
    int channels = 1;
    std::vector<float> samples;
    /** Reject malformed, non-finite, or longer-than-ten-second SFX data. */
    bool valid() const;
};
/** Per-sound scheduling policy, independent of gameplay and audio hardware. */
struct SoundPolicy {
    float gain = 1;
    float cooldown = .04F;
    int priority = 1;
    int maxInstances = 2;
};
/** A temporary synthesized tone with attack/release smoothing and optional noise. */
struct Beep {
    float startHz;
    float endHz;
    float seconds;
    float noise = 0;
};
/** Synthesize deterministic placeholder PCM once, outside the playback hot path. */
AudioClip synthesize(Beep beep);
/** Decode a bounded optional MP3 asset; failures return an empty clip and a reason. */
AudioClip loadMp3(const std::string &path, std::string &error);

class AudioMixer;
/** Borrow only PCM consumption and queue-invalidation metadata; cannot mutate sound policy or settings. */
class AudioOutput {
  public:
    /** Mix the next copied PCM block on the application's main thread. */
    void render(float *samples, std::size_t frames) const;
    /** Read the current generation so a sink can discard previously queued output. */
    std::uint64_t generation() const;
    /** Read mute state without exposing mutable application preferences. */
    bool muted() const;

  private:
    friend class AudioMixer;
    explicit AudioOutput(AudioMixer &mixer) : mixer_(mixer) {}
    AudioMixer &mixer_;
};
/** Main-thread mixer; the SDL sink receives copies rather than reading game-owned voices. */
class AudioMixer {
  public:
    static constexpr int SampleRate = 48000;
    static constexpr int Channels = 2;
    static constexpr std::size_t MaxVoices = 16;
    AudioSettings settings;
    std::uint64_t played = 0;
    std::uint64_t suppressed = 0;
    std::uint64_t generation = 0;
    /** Register decoded PCM and its policy, preserving clip ownership for active voices. */
    bool setClip(SoundId id, AudioClip clip, SoundPolicy policy = {});
    /** Request one voice with a bounded stereo pan; apply mute, cooldown and priority. */
    bool play(SoundId id, float pan = 0);
    /** Advance cooldown time in real seconds, independent of the game's speed setting. */
    void advance(float seconds);
    /** Mix exactly frames of interleaved stereo float PCM, including silence after voices end. */
    void render(float *output, std::size_t frames);
    /** Drop pending voices and invalidate already queued sink data on pause/reset/mute. */
    void stop();
    /** Borrow an output capability whose lifetime must remain inside this mixer owner. */
    AudioOutput output();
    /** Read active population for device diagnostics and bounded-load tests. */
    std::size_t activeVoices() const;

  private:
    struct Entry {
        std::shared_ptr<const AudioClip> clip;
        SoundPolicy policy;
        double last = -1000;
    };
    struct Voice {
        SoundId id;
        std::shared_ptr<const AudioClip> clip;
        double cursor;
        float left;
        float right;
        int priority;
    };
    std::unordered_map<SoundId, Entry> clips_;
    std::vector<Voice> voices_;
    double clock_ = 0;
};
} // namespace thorium::engine
