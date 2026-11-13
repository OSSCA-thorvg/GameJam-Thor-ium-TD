#pragma once
#include "AnimationBindings.h"
#include "AssetKeys.h"
#include "engine/render/Renderer.h"

namespace thorium::presentation::assets {
/** Declare game profiles separately from renderer-owned source metadata. */
inline AnimationBindings animationBindings() {
    AnimationBindings bindings;
    for (const char *creature : Creatures)
        bindings.push_back({creature, creature});
    return bindings;
}
/** Bind shipped game content to the renderer's generic asset interface. */
inline engine::RenderAssets renderAssets() {
    const std::string root = THORIUM_CONTENT_DIR;
    engine::RenderAssets result{root + "/fonts/NotoSansKR.ttf", {}, {}};
    for (const char *creature : Creatures)
        result.animations.emplace(creature, root + "/animations/" + creature + ".json");
    for (const char *picture : Pictures)
        result.pictures.emplace(picture, root + "/art/" + picture + ".svg");
    return result;
}
} // namespace thorium::presentation::assets
