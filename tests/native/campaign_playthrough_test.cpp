#include "game/autoplay/Autoplayer.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

using namespace thorium::game;
using namespace thorium::game::autoplay;
namespace {
/** Construct a shipped stage through the same validated boundary as the application. */
Run stageRun(int stage) {
    RunCreation created = Run::create(stageCatalog(stage));
    if (!created.run) {
        std::cerr << created.validation.summary();
        std::exit(1);
    }
    return std::move(*created.run);
}
} // namespace
/** Regression proof that all stages are winnable with finite real costs and authored enemies. */
int main() {
    for (int stage = 1; stage <= TotalStages; ++stage) {
        Run run = stageRun(stage);
        int steps = 0;
        while (run.state().phase != Phase::Won && run.state().phase != Phase::Lost && steps < 24000) {
            if (run.state().phase == Phase::Ready || run.state().phase == Phase::Prep) {
                if (run.state().phase == Phase::Prep)
                    prepare(run);
                if (!run.startWave().ok) {
                    std::cerr << "Blocked stage " << stage;
                    return 1;
                }
            }
            if (steps % 5 == 0)
                playCard(run);
            run.advance(.1F);
            ++steps;
        }
        std::cout << "stage=" << stage << " phase=" << static_cast<int>(run.state().phase)
                  << " wave=" << run.state().wave << " hp=" << run.state().baseHp
                  << " kills=" << run.state().defeated << " seconds=" << steps / 10.0F << " layer=B"
                  << run.state().mineLayer + 1 << " relics=" << run.state().relics.size() << " towers=";
        for (const Tower &tower : run.state().towers)
            std::cout << run.catalog().card(tower.kind).name << tower.level << ' ';
        std::cout << '\n';
        if (run.state().phase != Phase::Won)
            return 1;
    }
}
