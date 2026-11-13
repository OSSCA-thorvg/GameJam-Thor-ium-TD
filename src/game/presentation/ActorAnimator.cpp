#include "ActorAnimator.h"
#include "game/rules/Run.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace thorium::presentation {
float actorLoopPhase(game::EntityId id, std::uint32_t seed) {
    std::uint64_t value = (static_cast<std::uint64_t>(seed) << 32) | id;
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    value ^= value >> 31;
    return static_cast<float>(value >> 40) / 16777216.0F;
}
ActorAnimator::ActorAnimator(ActorAnimationSet clips, float loopPhase) : clips_(std::move(clips)) {
    player_.setLoopPhase(loopPhase);
    player_.play(clips_.idle);
}
void ActorAnimator::resumeLocomotion() {
    motion_ = locomotion_;
    player_.play(motion_ == ActorMotion::Move ? clips_.move : clips_.idle);
    player_.setSpeed(motion_ == ActorMotion::Move ? moveSpeed_ : 1);
}
void ActorAnimator::locomotion(bool moving, float moveSpeed) {
    if (motion_ == ActorMotion::Death)
        return;
    locomotion_ = moving ? ActorMotion::Move : ActorMotion::Idle;
    moveSpeed_ = std::isfinite(moveSpeed) && moveSpeed >= 0 ? moveSpeed : 1;
    if (motion_ != ActorMotion::Attack)
        resumeLocomotion();
}
void ActorAnimator::attack() {
    if (motion_ == ActorMotion::Death)
        return;
    motion_ = ActorMotion::Attack;
    player_.setSpeed(1);
    player_.play(clips_.attack, engine::Restart::Always);
}
void ActorAnimator::die() {
    if (motion_ == ActorMotion::Death)
        return;
    motion_ = ActorMotion::Death;
    player_.setSpeed(1);
    player_.play(clips_.death, engine::Restart::Always);
}
void ActorAnimator::advance(float seconds) {
    const float remaining = std::max(0.0F, clips_.attack.duration - player_.elapsed());
    player_.advance(seconds);
    if (motion_ == ActorMotion::Attack && player_.takeCompletion()) {
        resumeLocomotion();
        if (seconds > remaining)
            player_.advance(seconds - remaining);
    }
}
engine::AnimationSample ActorAnimator::sample(bool reducedMotion) const {
    if (reducedMotion)
        return {clips_.idle.asset, clips_.idle.frames.begin, clips_.idle.frames};
    return player_.sample().value_or(engine::AnimationSample{clips_.idle.asset, clips_.idle.frames.begin});
}
ActorMotion ActorAnimator::motion() const {
    return motion_;
}
float ActorAnimator::loopPhase() const {
    return player_.loopPhase();
}
float ActorAnimator::elapsed() const {
    return player_.elapsed();
}
bool ActorAnimator::finished() const {
    return player_.finished();
}
void ActorAnimations::reset(std::uint32_t seed) {
    seed_ = seed;
    error_.clear();
    actors_.clear();
    deathOrder_ = 0;
}
void ActorAnimations::observe(const game::Run &run, const std::vector<game::CombatEvent> &events,
                              const AnimationSets &sets) {
    if (!error_.empty())
        return;
    for (std::pair<const game::EntityId, ActorVisual> &entry : actors_)
        entry.second.seen = false;
    for (const game::Enemy &enemy : run.state().enemies) {
        const ActorAnimationSet *profile = sets.find(run.catalog().enemy(enemy.kind).animation);
        if (!profile) {
            error_ = "Missing live actor animation profile: " + run.catalog().enemy(enemy.kind).animation;
            return;
        }
        std::map<game::EntityId, ActorVisual>::iterator found = actors_.find(enemy.id);
        if (found == actors_.end())
            found =
                actors_
                    .emplace(enemy.id, ActorVisual{enemy.id, enemy.kind, enemy.position,
                                                   ActorAnimator{*profile, actorLoopPhase(enemy.id, seed_)}})
                    .first;
        ActorVisual &actor = found->second;
        actor.position = enemy.position;
        actor.slowed = enemy.slowUntil > run.state().battleTime;
        actor.seen = true;
        actor.affix = enemy.affix;
        actor.animator.locomotion(run.state().phase == game::Phase::Battle && !enemy.blocker &&
                                      !enemy.sabotage,
                                  actor.slowed ? run.catalog().balance.slowFactor : 1);
    }
    for (const game::CombatEvent &event : events) {
        if (!event.enemy || !event.subject)
            continue;
        std::map<game::EntityId, ActorVisual>::iterator found = actors_.find(event.subject);
        if (event.type == game::CombatEventKind::Attack) {
            if (found != actors_.end())
                found->second.animator.attack();
        } else if (event.type == game::CombatEventKind::Defeated) {
            if (found != actors_.end() && found->second.animator.motion() == ActorMotion::Death)
                continue;
            makeDeathRoom();
            found = actors_.find(event.subject);
            if (found == actors_.end()) {
                const ActorAnimationSet *profile = sets.find(run.catalog().enemy(event.enemyKind).animation);
                if (!profile) {
                    error_ = "Missing defeated actor animation profile: " +
                             run.catalog().enemy(event.enemyKind).animation;
                    return;
                }
                found =
                    actors_
                        .emplace(event.subject,
                                 ActorVisual{event.subject, event.enemyKind, event.to,
                                             ActorAnimator{*profile, actorLoopPhase(event.subject, seed_)}})
                        .first;
            }
            found->second.position = event.to;
            found->second.affix = event.affix;
            found->second.animator.die();
            found->second.deathOrder = ++deathOrder_;
        }
    }
    for (std::map<game::EntityId, ActorVisual>::iterator it = actors_.begin(); it != actors_.end();) {
        if (!it->second.seen && it->second.animator.motion() != ActorMotion::Death)
            it = actors_.erase(it);
        else
            ++it;
    }
}
void ActorAnimations::advance(float seconds) {
    for (std::map<game::EntityId, ActorVisual>::iterator it = actors_.begin(); it != actors_.end();) {
        it->second.animator.advance(seconds);
        if (it->second.animator.motion() == ActorMotion::Death && it->second.animator.finished())
            it = actors_.erase(it);
        else
            ++it;
    }
}
const std::map<game::EntityId, ActorVisual> &ActorAnimations::actors() const {
    return actors_;
}
const ActorVisual *ActorAnimations::find(game::EntityId id) const {
    const std::map<game::EntityId, ActorVisual>::const_iterator found = actors_.find(id);
    return found == actors_.end() ? nullptr : &found->second;
}
std::size_t ActorAnimations::deaths() const {
    return static_cast<std::size_t>(std::count_if(
        actors_.begin(), actors_.end(), [](const std::pair<const game::EntityId, ActorVisual> &entry) {
            return entry.second.animator.motion() == ActorMotion::Death;
        }));
}
void ActorAnimations::makeDeathRoom() {
    if (deaths() < MaxDeathVisuals)
        return;
    std::map<game::EntityId, ActorVisual>::iterator oldest = actors_.end();
    for (std::map<game::EntityId, ActorVisual>::iterator it = actors_.begin(); it != actors_.end(); ++it)
        if (it->second.animator.motion() == ActorMotion::Death &&
            (oldest == actors_.end() || it->second.deathOrder < oldest->second.deathOrder))
            oldest = it;
    if (oldest != actors_.end())
        actors_.erase(oldest);
}
std::string ActorAnimations::debugState() const {
    std::ostringstream output;
    output << '[';
    std::size_t count = 0;
    for (const std::pair<const game::EntityId, ActorVisual> &entry : actors_) {
        if (count == 64)
            break;
        if (count++)
            output << ',';
        const ActorVisual &actor = entry.second;
        output << "{\"id\":" << actor.id << ",\"kind\":" << static_cast<int>(actor.kind)
               << ",\"affix\":" << static_cast<int>(actor.affix)
               << ",\"state\":" << static_cast<int>(actor.animator.motion())
               << ",\"phase\":" << actor.animator.loopPhase() << ",\"time\":" << actor.animator.elapsed()
               << ",\"frame\":" << actor.animator.sample().frame << ",\"x\":" << actor.position.x
               << ",\"y\":" << actor.position.y << '}';
    }
    output << ']';
    return output.str();
}
} // namespace thorium::presentation
