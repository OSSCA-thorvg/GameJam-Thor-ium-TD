#pragma once
#include "Catalog.h"

namespace thorium::game {
/** Rock layers of one wall cell and the buried object under it. */
struct ExcavationCell {
    /** Rock layers to strip before the cell is exposed, from 1 to MaxCover. */
    int cover = 1;
    /** Pickaxe tier needed to strip the cell at all. */
    int grade = 1;
    /** One-based index into the wall's deposits, or zero for plain rock. */
    int deposit = 0;
    /** Whether the contents show through the rock before any strike. */
    bool revealed = false;
};
/** One excavation wall: the cells in row-major order and the objects buried in them. */
struct ExcavationWall {
    int width = 0;
    int height = 0;
    std::vector<ExcavationCell> cells;
    std::vector<Deposit> deposits;
};
/** Thickest rock a wall cell starts with. */
inline constexpr int MaxCover = 4;
/** Bury the minerals, nests and special tiles of an authored layer as shaped deposits in a wall of the same
 * size. Up to three tiles of one mineral merge into one deposit worth their sum, so the wall holds the same
 * totals as the layer. Gold, crystal and nests lie in the lower part, and gold and nests sit under one extra
 * layer of rock. The same layer and seed always give the same wall. */
ExcavationWall excavationWall(const MineLayerDefinition &layer, std::uint32_t seed);
} // namespace thorium::game
