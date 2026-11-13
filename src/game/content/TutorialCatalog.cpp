#include "TutorialCatalog.h"

namespace thorium::game {
Catalog tutorialCatalog() {
    Catalog content = defaultCatalog();
    content.waves = {{{EnemyKind::Normal, 4, 1, 1.8F}}, {{EnemyKind::Normal, 1, 1, 1}}};
    content.mine.cells[6].revealed = false;
    content.mine.cells[13].grade = 2;
    return content;
}
} // namespace thorium::game
