#pragma once
#include "AnimationBindings.h"
#include "game/model/Types.h"
#include <map>

namespace thorium::game {
class Run;
}
namespace thorium::presentation {
/** Game-specific animation states; the engine player never interprets these names. */
/** Stable presentation-only variation never consumes or changes gameplay randomness. */
float actorLoopPhase(game::EntityId id, std::uint32_t seed);
/** Game-specific animation states. */
enum class ActorMotion { Idle, Move, Attack, Death };
/** Own a small explicit actor state machine and an independent engine playback clock. */
class ActorAnimator {
  public:
    /** Copy resolved clip values so no mutable registry or ThorVG object can outlive its owner. */
    explicit ActorAnimator(ActorAnimationSet clips, float loopPhase = 0);
    /** Change desired locomotion without interrupting an attack or restarting the same loop. */
    void locomotion(bool moving, float moveSpeed = 1);
    /** Restart one committed attack unless death has already taken priority. */
    void attack();
    /** Enter the absorbing death state once; repeated facts cannot restart it. */
    void die();
    /** Advance the supplied game-clock delta and return from attack to current locomotion. */
    void advance(float seconds);
    /** Sample a static idle pose when reduced motion is requested. */
    engine::AnimationSample sample(bool reducedMotion = false) const;
    /** Read the current semantic motion state. */
    ActorMotion motion() const;
    /** Read local clip time for diagnostics, never for game rules. */
    float elapsed() const;
    /** Read deterministic loop variation for diagnostics and replay checks. */
    float loopPhase() const;
    /** Decide when a visual-only corpse has completed its one-shot. */
    bool finished() const;

  private:
    ActorAnimationSet clips_;
    engine::AnimationPlayer player_;
    ActorMotion motion_ = ActorMotion::Idle;
    ActorMotion locomotion_ = ActorMotion::Idle;
    float moveSpeed_ = 1;
    /** Apply the latest locomotion request after an attack, preserving identical loop requests. */
    void resumeLocomotion();
};
/** Minimal visual snapshot retained after the authoritative enemy has been removed. */
struct ActorVisual {
    game::EntityId id;
    game::EnemyKind kind;
    game::Point position;
    ActorAnimator animator;
    bool slowed = false;
    bool seen = false;
    std::uint64_t deathOrder = 0;
    game::EliteAffix affix = game::EliteAffix::None;
};
/** Own live enemy animation instances and bounded visual-only death tails. */
class ActorAnimations {
  public:
    static constexpr std::size_t MaxDeathVisuals = 128;
    /** Drop old identities, playback clocks and corpses when a run is replaced. */
    void reset(std::uint32_t seed = 0);
    /** Synchronize living snapshots, then consume chronological attack/death facts exactly once. */
    void observe(const game::Run &run, const std::vector<game::CombatEvent> &events,
                 const AnimationSets &sets);
    /** Advance only when the app supplies game time; menus and pause withhold this call. */
    void advance(float seconds);
    /** Read deterministic draw order without exposing mutable animation instances. */
    const std::map<game::EntityId, ActorVisual> &actors() const;
    /** Read one visual instance by stable game identity. */
    const ActorVisual *find(game::EntityId id) const;
    /** Count visual-only corpses independently of scheduled and living game enemies. */
    std::size_t deaths() const;
    /** Expose a bounded read-only snapshot for browser integration tests. */
    std::string debugState() const;
    /** Read an unexpected runtime binding failure; reset clears the failed session. */
    const std::string &error() const {
        return error_;
    }

  private:
    std::map<game::EntityId, ActorVisual> actors_;
    std::uint64_t deathOrder_ = 0;
    std::uint32_t seed_ = 0;
    std::string error_;
    /** Evict the oldest cosmetic corpse when a large defeat burst reaches the visual cap. */
    void makeDeathRoom();
};
} // namespace thorium::presentation
