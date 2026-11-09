#include "AnimationBindings.h"
#include "game/content/Catalog.h"

namespace thorium::presentation {
const ActorAnimationSet *AnimationSets::find(const std::string &key) const {
    const std::map<std::string, ActorAnimationSet>::const_iterator found = profiles.find(key);
    return found == profiles.end() ? nullptr : &found->second;
}
AnimationSets animationSets(const engine::AnimationAssets &metadata, const AnimationBindings &bindings,
                            const game::Catalog &catalog) {
    AnimationSets result;
    for (const ActorAnimationBinding &binding : bindings) {
        if (binding.profile.empty() || binding.asset.empty() || result.find(binding.profile))
            return {{}, "Empty or duplicate actor animation binding: " + binding.profile};
        const engine::AnimationAssets::const_iterator source = metadata.find(binding.asset);
        if (source == metadata.end())
            return {{},
                    "Missing actor animation source: " + binding.asset + " (profile " + binding.profile +
                        ")"};
        const std::optional<engine::AnimationClip> idle =
            engine::animationClip(binding.asset, source->second, binding.idle, engine::Playback::Pose);
        const std::optional<engine::AnimationClip> move =
            engine::animationClip(binding.asset, source->second, binding.move, engine::Playback::Loop);
        const std::optional<engine::AnimationClip> attack =
            engine::animationClip(binding.asset, source->second, binding.attack, engine::Playback::Once);
        const std::optional<engine::AnimationClip> death =
            engine::animationClip(binding.asset, source->second, binding.death, engine::Playback::Once);
        if (!idle || !move || !attack || !death)
            return {{},
                    "Invalid idle/move/attack/death markers for actor profile: " + binding.profile +
                        " (source " + binding.asset + ")"};
        result.profiles.emplace(binding.profile, ActorAnimationSet{*idle, *move, *attack, *death});
    }
    for (std::size_t index = 0; index < catalog.enemies.size(); ++index) {
        const std::string &profile = catalog.enemies[index].animation;
        if (!result.find(profile))
            return {{},
                    "Missing actor animation profile: " + profile + " (enemy " + std::to_string(index) + ")"};
    }
    return result;
}
} // namespace thorium::presentation
