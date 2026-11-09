#pragma once
#include "engine/audio/Audio.h"
#include "game/rules/Run.h"
#include <array>
#include <string>
#include <vector>

namespace thorium::presentation {
/** Semantic game cues; replacing audio files never changes these call sites. */
enum class Sound : engine::SoundId {
    Select,
    Confirm,
    Error,
    CardUse,
    CardDraw,
    Placement,
    Shot,
    Cannon,
    Lightning,
    Frost,
    Blast,
    Hit,
    DefeatEnemy,
    BaseHit,
    MineStart,
    MineComplete,
    Scan,
    Upgrade,
    Reward,
    WaveStart,
    WaveClear,
    Victory,
    Defeat,
    Count,
    None = 65535
};
/** One optional filename and procedural fallback, including its voice scheduling policy. */
struct SoundDefinition {
    Sound id;
    const char *key;
    engine::Beep beep;
    engine::SoundPolicy policy;
};
/** Read stable sound filenames and policies for tooling, documentation and tests. */
const std::array<SoundDefinition, static_cast<std::size_t>(Sound::Count)> &soundDefinitions();
/** Map committed game facts to audio, separately from visual lifetimes and rendering. */
class Soundscape {
  public:
    int mp3Clips = 0;
    int beepClips = 0;
    std::vector<std::string> assetErrors;
    /** Preload optional MP3 clips, generating missing/invalid fallback tones once. */
    explicit Soundscape(engine::AudioMixer &mixer,
                        const std::string &directory = std::string(THORIUM_CONTENT_DIR) + "/audio");
    /** Request a semantic cue with optional world-based stereo placement. */
    bool play(Sound sound, float pan = 0);
    /** Advance real-time cooldowns and a single coalesced pending card arrival. */
    void advance(float seconds);
    /** Observe hand, phase and miner-job commits without relying on toast messages. */
    void observe(const game::Run &run);
    /** Map attack/placement/damage/death/base-leak facts, applying mixer rate limits. */
    void consume(const std::vector<game::CombatEvent> &events);
    /** Seed a fresh run without replaying old card/mining/phase transitions. */
    void reset(const game::Run &run);
    /** Drop delayed arrivals when suspending or pausing the application. */
    void cancelPending();

  private:
    engine::AudioMixer &mixer_;
    std::array<game::CardId, 4> hand_{};
    game::Phase phase_ = game::Phase::Ready;
    bool mining_ = false;
    float drawDelay_ = -1;
    bool initialized_ = false;
};
} // namespace thorium::presentation
