#include "../tests/native/Scenario.h"
#include "engine/diagnostics/FrameProfiler.h"
#include <algorithm>
#include <iostream>
#include <vector>

using namespace thorium;
namespace {
/** Isolate a synchronized cannon volley; only Run::advance belongs to the measurement. */
double measure(int count, int towerCount) {
    game::Catalog content = game::defaultCatalog();
    content.enemies[0].speed = 0;
    content.enemies[0].hp = 1000000;
    content.waves = {{{game::EnemyKind::Normal, count, 0, 0}}};
    game::Run run = game::makeRun(content);
    run.startWave();
    for (int index = 0; index < towerCount; ++index)
        game::scenario(run).towers.push_back(
            {static_cast<game::EntityId>(100000 + index), game::CardKind::Cannon, {6, 4}});
    run.advance(1.0F / 60.0F);
    std::vector<double> times;
    for (int frame = 0; frame < 40; ++frame) {
        for (game::Tower &tower : game::scenario(run).towers)
            tower.cooldown = 0;
        run.takeCombatEvents();
        const engine::FrameClock::time_point start = engine::FrameClock::now();
        run.advance(1.0F / 60.0F);
        times.push_back(engine::frameMilliseconds(start));
    }
    std::sort(times.begin(), times.end());
    return times[times.size() / 2];
}
} // namespace
/** Emit review-compatible raw CPU timings; fixture mutation never ships in the game. */
int main() {
    std::cout << '[';
    bool first = true;
    for (int count : {100, 300, 1000, 3000})
        for (int towers : {0, 1, 8}) {
            if (!first)
                std::cout << ',';
            first = false;
            std::cout << "{\"enemies\":" << count << ",\"towers\":" << towers
                      << ",\"volley_p50_ms\":" << measure(count, towers) << '}';
        }
    std::cout << "]\n";
}
