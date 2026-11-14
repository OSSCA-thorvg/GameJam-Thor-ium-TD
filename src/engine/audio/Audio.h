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
    /** Music level relative to the master volume. */
    float music = .7F;
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

/** Looping MP3 music decoded in small blocks from compressed bytes kept in memory. */
class MusicStream {
  public:
    MusicStream();
    ~MusicStream();
    MusicStream(const MusicStream &) = delete;
    MusicStream &operator=(const MusicStream &) = delete;
    /** Load a bounded MP3; a failed load leaves the stream silent and reports a reason. */
    bool open(const std::string &path, std::string &error);
    /** Read whether a decodable track is loaded. */
    bool loaded() const;
    /** Write frames of stereo PCM at outputRate, restarting from the first frame after the last. */
    void read(float *output, std::size_t frames, int outputRate);
    /** Count completed passes through the track. */
    std::uint64_t loops() const;

  private:
    struct Decoder;
    std::unique_ptr<Decoder> decoder_;
};

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
    /** Drop pending voices and invalidate already queued sink data on pause/reset/mute; music keeps its
     * place. */
    void stop();
    /** Install looping background music mixed under the voices at gain; an unloaded stream is rejected. */
    bool setMusic(std::unique_ptr<MusicStream> music, float gain);
    /** Read the installed music for diagnostics and tests. */
    const MusicStream *music() const;
    /** Install an optional battle track at the music gain; it crossfades with the main track. */
    bool setBattleMusic(std::unique_ptr<MusicStream> music);
    /** Ask for the battle track while a battle runs; without one the main track keeps playing. */
    void setBattle(bool battle);
    /** Read the share of the battle track in the music, from 0 to 1. */
    float battleBlend() const;
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
    std::unique_ptr<MusicStream> music_;
    std::unique_ptr<MusicStream> battleMusic_;
    float musicGain_ = 0;
    bool battle_ = false;
    float blend_ = 0;
    std::vector<float> musicBlock_;
};
} // namespace thorium::engine
