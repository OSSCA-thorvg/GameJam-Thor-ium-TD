#pragma once
#include "engine/animation/Animation.h"
#include <map>
#include <vector>

namespace thorium::game {
struct Catalog;
}
namespace thorium::presentation {
/** Bind a game profile to one source and its authored semantic markers. */
struct ActorAnimationBinding {
    std::string profile;
    std::string asset;
    std::string idle = "idle";
    std::string move = "move";
    std::string attack = "attack";
    std::string death = "death";
};
using AnimationBindings = std::vector<ActorAnimationBinding>;
/** Resolve semantic states to immutable source clips. */
struct ActorAnimationSet {
    engine::AnimationClip idle;
    engine::AnimationClip move;
    engine::AnimationClip attack;
    engine::AnimationClip death;
};
/** Session bindings are published only after every content reference has been validated. */
struct AnimationSets {
    std::map<std::string, ActorAnimationSet> profiles;
    std::string error;
    /** Resolve a content profile without silently inserting a placeholder. */
    const ActorAnimationSet *find(const std::string &key) const;
};
/** Validate catalog profiles, unique bindings, sources and all four required clips atomically. */
AnimationSets animationSets(const engine::AnimationAssets &metadata, const AnimationBindings &bindings,
                            const game::Catalog &catalog);
} // namespace thorium::presentation
