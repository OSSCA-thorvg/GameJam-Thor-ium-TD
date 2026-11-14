#pragma once
#include "Text.h"
#include "game/content/Catalog.h"
#include <cmath>
#include <string>

namespace thorium::presentation {
/** Return a pad trait's localized name. */
inline std::string padTraitName(game::PadTrait trait) {
    return trait == game::PadTrait::HighGround ? tr("terrain.highGround.name")
           : trait == game::PadTrait::Crystal  ? tr("terrain.crystal.name")
                                               : std::string();
}
/** Format a multiplier above one as a percentage bonus, such as +20%. */
inline std::string bonusPercent(float multiplier) {
    return "+" + std::to_string(static_cast<int>(std::lround((multiplier - 1) * 100))) + "%";
}
/** Return a pad trait's localized effect with the catalog's actual bonus. */
inline std::string padTraitEffect(game::PadTrait trait, const game::RunBalance &balance) {
    return trait == game::PadTrait::HighGround
               ? tr("terrain.highGround.effect", {{"bonus", bonusPercent(balance.highGroundRange)}})
           : trait == game::PadTrait::Crystal
               ? tr("terrain.crystal.effect", {{"bonus", bonusPercent(balance.crystalDamage)}})
               : std::string();
}
} // namespace thorium::presentation
