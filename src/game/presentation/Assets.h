#pragma once
#include "AnimationBindings.h"
#include "AssetKeys.h"
#include "engine/render/Renderer.h"

namespace thorium::presentation::assets {
/** Declare game profiles separately from renderer-owned source metadata. */
inline AnimationBindings animationBindings() {
    return {{Oreling, Oreling}};
}
/** Bind shipped game content to the renderer's generic asset interface. */
inline engine::RenderAssets renderAssets() {
    const std::string root = THORIUM_CONTENT_DIR;
    return {root + "/fonts/NotoSansKR.ttf", {{Oreling, root + "/animations/oreling.json"}}};
}
} // namespace thorium::presentation::assets
