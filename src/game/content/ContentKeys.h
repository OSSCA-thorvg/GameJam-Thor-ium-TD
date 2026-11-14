#pragma once
#include "game/model/Types.h"
#include <array>
#include <cstddef>
#include <string>

namespace thorium::game {
/** String-table id of a card kind, used as `card.<id>.name`. */
inline const char *cardKey(CardKind kind) {
    static const std::array<const char *, CardKindCount> keys{{"gun", "cannon", "guard", "frost", "blast",
                                                               "lightning", "shooter", "demolition", "flame",
                                                               "sniper", "generator", "tar"}};
    return keys[static_cast<std::size_t>(kind)];
}
/** String-table id of an elite modifier, used as `elite.<id>.name`. */
inline const char *eliteKey(EliteAffix affix) {
    static const std::array<const char *, EliteAffixCount> keys{
        {"none", "steel", "swift", "regen", "enrage", "leader"}};
    return keys[static_cast<std::size_t>(affix)];
}
/** String-table id of an ore, used as `ore.<id>.name` and `ore.<id>.short`. */
inline const char *oreKey(Ore ore) {
    static const std::array<const char *, 5> keys{{"rock", "iron", "copper", "crystal", "gold"}};
    return keys[static_cast<std::size_t>(ore)];
}
/** String-table id of a relic, used as `relic.<id>.name`. */
inline const char *relicKey(Relic relic) {
    static const std::array<const char *, RelicCount> keys{{"battery", "charm", "ward", "lens"}};
    return keys[static_cast<std::size_t>(relic)];
}
/** A message value naming a card. */
inline std::string cardRef(CardKind kind) {
    return "@card." + std::string(cardKey(kind)) + ".name";
}
/** A message value naming a mine layer by its id, `MineLayerDefinition::name`. */
inline std::string mineLayerRef(const std::string &id) {
    return "@minelayer." + id + ".name";
}
/** A message value holding a resource price. */
inline std::string costRef(const ResourceCost &cost) {
    return "#cost/" + std::to_string(cost.iron) + "/" + std::to_string(cost.copper) + "/" +
           std::to_string(cost.crystal) + "/" + std::to_string(cost.gold);
}
} // namespace thorium::game
