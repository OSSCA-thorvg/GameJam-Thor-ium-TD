#include "TutorialCatalog.h"

namespace thorium::game {
Catalog tutorialCatalog() {
    Catalog content = defaultCatalog();
    content.waves = {{{EnemyKind::Normal, 4, 1, 1.8F}}, {{EnemyKind::Normal, 1, 1, 1}}};
    content.mine.cells[6].revealed = false;
    content.mine.cells[13].grade = 2;
    for (MineTileDefinition &tile : content.mine.cells)
        tile.nest = false;
    // The wall of seed 1 holds iron at cells 0, 1, 5 and 6, copper at 2 to 4 and gold at 7 and 8; these
    // covers let five strikes collect the iron, the copper and, with the T2 pick, the gold.
    content.mining = MiningMode::Excavate;
    content.mineSeed = 1;
    content.wallCover = {{6, 1}, {7, 4}, {8, 4}};
    // A fork beside the first gun: the detour turns east along row 3 and rejoins the route at (11, 7).
    content.forks = {{0, {8, 3}, {11, 7}, {{11, 3}}}};
    return content;
}
} // namespace thorium::game
