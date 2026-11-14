#pragma once
#include <array>

namespace thorium::presentation::assets {
/** Logical source key shared by game animation bindings and file registration. */
inline constexpr const char *Oreling = "oreling";
/** Every enemy creature source; each file is content/animations/<key>.json and binds a profile of the same
 * name. */
inline constexpr std::array<const char *, 16> Creatures{
    {Oreling, "dashling", "ironback", "pebbling", "shieldling", "mender", "splitter", "burrower", "sapper",
     "bat", "stalker", "leaper", "wrecker", "slinger", "carrier", "golem"}};
/** Official ThorVG copyright animation shown once before the title; the file is kept unmodified. */
inline constexpr const char *ThorvgSplash = "powered-by-thorvg";
/** Every still picture; each file is content/art/<key>.svg. */
inline constexpr std::array<const char *, 12> Pictures{{"ore-iron", "ore-copper", "ore-gold", "ore-crystal",
                                                        "nest", "chest", "fossil", "energy", "cracked",
                                                        "tool-pick", "tool-blast", "tool-scan"}};
} // namespace thorium::presentation::assets
