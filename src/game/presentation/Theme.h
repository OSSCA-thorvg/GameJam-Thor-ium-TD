#pragma once
#include "engine/render/Renderer.h"
#include "game/model/Types.h"

namespace thorium::presentation::theme {
inline constexpr engine::Color background{14, 27, 36};
inline constexpr engine::Color shadow{6, 15, 22, 200};
inline constexpr engine::Color edge{88, 111, 115};
inline constexpr engine::Color top{49, 72, 81};
inline constexpr engine::Color bottom{25, 43, 53};
inline constexpr engine::Color inset{20, 36, 45};
inline constexpr engine::Color text{244, 235, 208};
inline constexpr engine::Color muted{165, 188, 184};
inline constexpr engine::Color gold{241, 192, 102};
inline constexpr engine::Color copper{180, 115, 66};
inline constexpr engine::Color green{82, 180, 145};
inline constexpr engine::Color blue{109, 184, 221};
inline constexpr engine::Color purple{187, 149, 224};
inline constexpr engine::Color red{225, 119, 99};
inline constexpr engine::Color cardInk{43, 53, 54};

/** Return the crown, ring and chip color of one elite modifier. */
inline engine::Color elite(game::EliteAffix affix) {
    switch (affix) {
    case game::EliteAffix::Steel:
        return {164, 190, 212};
    case game::EliteAffix::Swift:
        return {190, 238, 84};
    case game::EliteAffix::Regen:
        return {84, 226, 170};
    case game::EliteAffix::Enrage:
        return {246, 86, 58};
    case game::EliteAffix::Leader:
        return {200, 140, 255};
    default:
        return gold;
    }
}
/** Return a persistent color for the three card action categories. */
inline engine::Color accent(game::CardCategory category) {
    return category == game::CardCategory::Building ? green
           : category == game::CardCategory::Unit   ? blue
                                                    : purple;
}
} // namespace thorium::presentation::theme
