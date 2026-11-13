#pragma once
#include "game/content/Catalog.h"
#include <cmath>
#include <string>

namespace thorium::presentation {
/** Return a pad trait's localized name. */
inline const char *padTraitName(game::PadTrait trait) {
    return trait == game::PadTrait::HighGround ? "고지" : trait == game::PadTrait::Crystal ? "결정 지반" : "";
}
/** Format a multiplier above one as a percentage bonus, such as +20%. */
inline std::string bonusPercent(float multiplier) {
    return "+" + std::to_string(static_cast<int>(std::lround((multiplier - 1) * 100))) + "%";
}
/** Return a pad trait's effect with the catalog's actual bonus, such as "사거리 +20%". */
inline std::string padTraitEffect(game::PadTrait trait, const game::RunBalance &balance) {
    return trait == game::PadTrait::HighGround ? "사거리 " + bonusPercent(balance.highGroundRange)
           : trait == game::PadTrait::Crystal  ? "피해 " + bonusPercent(balance.crystalDamage)
                                               : "";
}
} // namespace thorium::presentation
