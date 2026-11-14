#include "Scenario.h"
#include "app/AppFlow.h"
#include "app/GameInput.h"
#include "app/MapCamera.h"
#include "engine/view/Camera2D.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numeric>

using namespace thorium;
using namespace thorium::game;
using namespace thorium::presentation;
namespace {
int checks = 0;
/** Keep release builds accountable for every campaign invariant. */
void require(bool value, const char *message) {
    ++checks;
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Compare projected coordinates without relying on floating point identity. */
bool near(float a, float b) {
    return std::abs(a - b) < .001F;
}
/** Validate shipped bounds, lane schedules, mining capacity and rejected external references. */
void content() {
    for (int stage = 1; stage <= TotalStages; ++stage) {
        const Catalog &catalog = stageCatalog(stage);
        require(validateCatalog(catalog).ok(), "every campaign catalog passes the actual import validator");
        require(catalog.waves.size() == static_cast<std::size_t>(std::min(6 + stage * 2, 16)),
                "campaign stages last 8/10/12/14/16/16/16 waves");
        require(stageMetadata(stage).wavesCount == static_cast<int>(catalog.waves.size()),
                "stage metadata derives its length from content");
        require(catalog.width >= 24 && catalog.height >= 14, "campaign bounds exceed the former 18x10 field");
        constexpr std::array<std::size_t, TotalStages> entrances{{3, 3, 4, 5, 5, 4, 5}};
        require(catalog.additionalRoutes.size() + 1 == entrances[static_cast<std::size_t>(stage - 1)],
                "campaign stages provide 3/3/4/5/5/4/5 entrances");
        require(stage == 1 || (catalog.width > stageCatalog(stage - 1).width &&
                               catalog.height >= stageCatalog(stage - 1).height),
                "each later stage has a larger battlefield");
        const auto undug = [](const MineLayerDefinition &layer) -> int {
            return static_cast<int>(
                std::count_if(layer.cells.begin(), layer.cells.end(),
                              [](const MineTileDefinition &cell) -> bool { return !cell.opened; }));
        };
        int capacity = undug(catalog.mine);
        for (const MineLayerDefinition &layer : catalog.deeperMines)
            capacity += undug(layer);
        require(catalog.mine.width * catalog.mine.height > 30,
                "campaign cutaways are larger than the compact starter board");
        require(capacity >= (static_cast<int>(catalog.waves.size()) - 1) * catalog.balance.prepDigs,
                "authored layers supply all inter-wave digging budgets");
        Run run = makeRun(catalog);
        for (int wave = 1; wave <= static_cast<int>(catalog.waves.size()); ++wave) {
            if (wave > 1)
                enterPreparation(run, wave - 1);
            const std::vector<LaneForecast> forecast = run.laneForecast();
            const int total =
                std::accumulate(forecast.begin(), forecast.end(), 0,
                                [](int sum, const LaneForecast &lane) -> int { return sum + lane.count; });
            require(total == static_cast<int>(catalog.schedule(wave).size()),
                    "forecast matches the next committed schedule");
            for (const LaneForecast &lane : forecast) {
                require(distance(run.route(lane.route).back(), catalog.route.back()) < .01F,
                        "every lane reaches the same defended base");
                if (lane.route && wave == catalog.additionalRoutes[lane.route - 1].opensAtWave)
                    require(lane.count > 0, "new entrances attack on their advertised opening wave even when "
                                            "it is a focused raid");
                if (lane.route && wave < catalog.additionalRoutes[lane.route - 1].opensAtWave)
                    require(lane.count == 0, "an entrance cannot attack before its advertised opening");
            }
        }
        const std::vector<LaneForecast> final = run.laneForecast();
        require(std::all_of(final.begin(), final.end(),
                            [](const LaneForecast &lane) -> bool { return lane.count > 0; }),
                "final assault uses every entrance");
        Catalog invalid = catalog;
        invalid.waves.back().front().route = 99;
        require(!Run::create(invalid).run, "unknown spawn routes are rejected before runtime indexing");
        invalid = catalog;
        invalid.additionalRoutes[0].points.back().x -= 1;
        require(!Run::create(invalid).run, "side entrances must terminate at the actual base");
        invalid = catalog;
        invalid.width = 18;
        require(!Run::create(invalid).run, "authored placements and routes obey their field bounds");
        invalid = catalog;
        invalid.deeperMines[0].opensAfterWave = 99;
        require(!Run::create(invalid).run, "unreachable mining layer gates fail validation");
    }
}
/** Confirm lane identity drives movement, base leaks and remaining-distance target priority. */
void combat() {
    Catalog content = stageCatalog(1);
    content.cards[0].range = 100;
    Run run = makeRun(content);
    enterPreparation(run, 2);
    require(run.expand(1).ok, "the original expansion still extends the primary entrance");
    require(run.skipOffer(run.state().rewardOffer->id).ok, "expansion reward can be declined");
    scenario(run).nextExtra = 3;
    const std::vector<Spawn> planned = run.catalog().schedule(3);
    const int primaryPlanned = static_cast<int>(std::count_if(
        planned.begin(), planned.end(), [](const Spawn &spawn) -> bool { return spawn.route == 0; }));
    const int primary = run.laneForecast().front().count;
    require(primary == primaryPlanned + 3 &&
                run.laneForecast().front().kinds[static_cast<std::size_t>(EnemyKind::Normal)] >= 3,
            "nest reinforcements are forecast on the primary entrance");
    require(primaryPlanned < static_cast<int>(planned.size()), "wave three also schedules the side entrance");
    require(run.startWave().ok && run.waveSpawnCount() == static_cast<int>(planned.size()) + 3,
            "wave three commits both entrances and nest reinforcements");
    float sideArrival = 1000;
    for (const Spawn &spawn : planned)
        if (spawn.route == 1)
            sideArrival = std::min(sideArrival, spawn.time);
    for (int step = 0; step < static_cast<int>(sideArrival * 10) + 5; ++step)
        run.advance(.1F);
    require(std::any_of(run.state().enemies.begin(), run.state().enemies.end(),
                        [](const Enemy &enemy) -> bool { return enemy.route == 1; }),
            "side entrance spawns real independent enemies");
    for (const Enemy &enemy : run.state().enemies)
        require(distance(enemy.position, run.routePoint(enemy.progress, enemy.route)) < .01F,
                "each actor moves along its assigned path");
    scenario(run).enemies.clear();
    scenario(run).towers.push_back({500, CardKind::Gun, {21, 9}, 1, 0});
    scenario(run).enemies.push_back({501, EnemyKind::Normal, {21, 9}, 10, 100, 0, 0, 0, 0});
    scenario(run).enemies.push_back({502, EnemyKind::Normal, {21, 9}, 10, 100, 0, 0, 0, 1});
    run.advance(.01F);
    require(run.state().enemies[1].hp < run.state().enemies[0].hp,
            "equal progress on unequal paths targets the enemy with least distance remaining");
    scenario(run).enemies.clear();
    scenario(run).enemies.push_back(
        {503, EnemyKind::Runner, run.route(1).back(), run.routeLength(1), 24, 0, 0, 0, 1});
    const int hp = run.state().baseHp;
    run.advance(.01F);
    require(run.state().baseHp == hp - 1, "a short side lane leaks at its own endpoint");
    enterPreparation(run, 3);
    require(run.state().phase == Phase::Prep, "the expedition continues after the old third-wave ending");
    enterPreparation(run, 8);
    require(run.state().phase == Phase::Won, "only the actual final campaign wave wins");
}
/** Verify one-way layer changes preserve all paid progress and cannot refill consumable budgets. */
void mining() {
    Run run = makeRun(stageCatalog(3));
    require(!run.descend().ok, "descent is not available before preparation");
    enterPreparation(run, 1);
    scenario(run).tool = 2;
    require(!run.descend().ok, "early T2 crafting cannot bypass the wave gate");
    enterPreparation(run, 3);
    scenario(run).tool = 1;
    require(!run.descend().ok, "the deeper layer requires a crafted T2 tool");
    scenario(run).tool = 2;
    scenario(run).iron = 9;
    scenario(run).gold = 12;
    scenario(run).nextExtra = 4;
    scenario(run).digs = 2;
    scenario(run).scans = 0;
    scenario(run).job.active = true;
    require(!run.descend().ok, "descent cannot abandon a moving or excavating miner");
    scenario(run).job.active = false;
    require(run.descend().ok && run.state().mineLayer == 1, "the first authored deeper layer becomes active");
    require(
        run.state().iron == 9 && run.state().gold == 12 && run.state().tool == 2 && run.state().digs == 2 &&
            !run.state().scans && run.state().nextExtra == 4,
        "descent preserves inventory, tool, remaining digs, exhausted scans and pending nest reinforcements");
    const MineLayerDefinition &next = run.catalog().deeperMines[0];
    require(run.state().mineWidth == next.width && run.state().mine.size() == next.cells.size(),
            "the active board adopts the next layer's dimensions");
    require(run.state().minerCell == next.start && run.state().mine[next.start].dug &&
                run.reachable(next.start + next.width),
            "the next cutaway has a connected safe starting gallery");
    for (int index = 0; index < static_cast<int>(next.cells.size()); ++index) {
        int nests = 0;
        for (int other = 0; other < static_cast<int>(next.cells.size()); ++other)
            nests += other != index && next.cells[other].nest &&
                     std::abs(index % next.width - other % next.width) <= 1 &&
                     std::abs(index / next.width - other / next.width) <= 1;
        require(run.state().mine[index].clue == nests, "new clues use only the next layer's original nests");
    }
    require(!run.descend().ok, "repeated descent cannot skip the second layer gate");
    enterPreparation(run, 7);
    require(run.descend().ok && run.state().mineLayer == 2 && !run.descend().ok,
            "the last layer opens once and has no out-of-range successor");
}
/** Exercise real flow controls without allowing stage browsing to mutate a suspended expedition. */
void stageFlow() {
    Run run = makeRun(stageCatalog(1));
    State state;
    state.app.campaign = true;
    state.app.highestUnlockedStage = 3;
    engine::Ui ui;
    engine::AudioMixer audio;
    Soundscape sound(audio, "/thorium-no-optional-audio");
    FeedbackChannel feedback(run, state, sound);
    AppFlow flow(run, state, ui, audio, sound, feedback);
    require(decodeInput({engine::Interaction::Kind::Click, SelectStageNext, {}}).has_value(),
            "stage selectors are decoded by the actual app input boundary");
    require(flow.act(HomeNew) == FlowResult::Restarted, "campaign starts through the normal home control");
    run.startWave();
    run.play(1, {6, 4});
    const int hp = run.state().baseHp;
    flow.act(GoHome);
    flow.act(SelectStageNext);
    require(state.app.selectedStage == 2 && run.state().towers.size() == 1 && run.catalog().waves.size() == 8,
            "browsing changes only the selection, preserving the live expedition");
    flow.act(HomeContinue);
    require(state.play.currentStage == 1 && run.state().baseHp == hp && run.state().towers.size() == 1,
            "continue returns to the actual active stage");
    require(flow.act(NextStageRun) != FlowResult::Restarted, "stage advance cannot bypass victory");
    state.app.highestUnlockedStage = 1;
    enterPreparation(run, 8);
    require(flow.synchronizeProgress() && state.app.highestUnlockedStage == 2 && !flow.synchronizeProgress(),
            "victory unlocks once without requiring a next-stage click");
    require(flow.act(NextStageRun) == FlowResult::Restarted && run.catalog().waves.size() == 10 &&
                state.play.currentStage == 2,
            "next-stage action starts the selected authored stage");
    enterPreparation(run, 10);
    flow.synchronizeProgress();
    flow.act(GoHome);
    flow.act(SelectStageNext);
    flow.act(HomeNew);
    require(run.catalog().waves.size() == 12 && state.play.currentStage == 3 && run.state().towers.empty(),
            "home selection starts a fresh third-stage expedition");
    flow.act(SelectStagePrev);
    require(state.app.selectedStage == 3, "in-game stage browsing is rejected");
}
/** Verify reversible projection, anchored zoom, bounded drags and modal-safe pointer capture. */
void camera() {
    const engine::Rect world{20, 100, 800, 450};
    const engine::Camera2D fit(world, 36, 20);
    const engine::ScreenPoint screen = fit.project({17, 9});
    const engine::WorldPoint point = fit.unproject(screen);
    require(near(point.x, 17) && near(point.y, 9), "world/screen projection round trips on larger fields");
    const engine::Camera2D zoom(world, 36, 20, fit.zoomed(2, screen));
    const engine::WorldPoint anchored = zoom.unproject(screen);
    require(near(anchored.x, 17) && near(anchored.y, 9), "zoom preserves the world point beneath the cursor");
    const engine::Camera2D moved(world, 36, 20, zoom.moved({10000, -10000}));
    require(std::abs(moved.pose().panX) < 18 && std::abs(moved.pose().panY) < 10,
            "camera panning cannot leave authored bounds");
    const engine::Camera2D invalid(world, 36, 20,
                                   {std::numeric_limits<float>::quiet_NaN(), INFINITY, INFINITY});
    require(invalid.pose().zoom == 1 && invalid.pose().panX == 0,
            "nonfinite input cannot corrupt projection");
    State state;
    state.app.navigation.enterGame();
    state.play.camera = zoom.pose();
    engine::Ui ui;
    Layout layout;
    layout.world = world;
    layout.camera = zoom;
    ui.add(World, world, true, true, false);
    MapCamera controller(state, ui, layout);
    const engine::ScreenPoint start = world.center();
    const engine::Pointer down{engine::PointerPhase::Down, 0, start};
    ui.pointer(down);
    controller.pointer(down);
    const engine::Pointer move{engine::PointerPhase::Move, 0, {start.x + 60, start.y + 30}};
    ui.pointer(move);
    controller.pointer(move);
    const engine::CameraPose once = state.play.camera;
    ui.pointer(move);
    controller.pointer(move);
    require(near(once.panX, state.play.camera.panX),
            "repeated pointer samples never accumulate the same total displacement twice");
    const engine::Pointer up{engine::PointerPhase::Up, 0, move.position};
    ui.pointer(up);
    require(controller.pointer(up), "panning release is consumed instead of selecting an entity");
    ui.add(999, world, false, false, false);
    controller.wheel(start, 1);
    require(near(state.play.camera.zoom, once.zoom),
            "disabled overlay occluders block wheel zoom just like pointer clicks");
    state.play.modal = Modal::MineDescent;
    controller.wheel(start, 1);
    require(near(state.play.camera.zoom, once.zoom), "modal ownership blocks camera changes");
}
/** Verify legible opening zoom, entrance focus and minimap recentering through real camera controls. */
void openingCamera() {
    const engine::Rect world{12, 138, 366, 324};
    const Catalog &catalog = stageCatalog(TotalStages);
    const engine::Camera2D fit(world, catalog.width, catalog.height);
    const Point base = catalog.route.back();
    const engine::Camera2D opening(world, catalog.width, catalog.height, fit.framed(26, {base.x, base.y}));
    const engine::ScreenPoint center = opening.project({base.x, base.y});
    require(fit.tileSize() < 26 && opening.tileSize() >= 25.99F,
            "large stages open with legible tiles instead of the full overview");
    require(near(center.x, world.center().x) && near(center.y, world.center().y),
            "the opening view centers the central defended base");
    const engine::Camera2D corner(world, catalog.width, catalog.height, opening.centered({0, 0}));
    require(corner.visibleWorld().x > -1 && corner.visibleWorld().y > -1,
            "recentering on an edge entrance stays inside authored bounds");
    State state;
    state.app.navigation.enterGame();
    engine::Ui ui;
    Layout layout;
    layout.world = world;
    layout.camera = opening;
    layout.fieldWidth = catalog.width;
    layout.fieldHeight = catalog.height;
    layout.minimap = {world.x + 10, world.y + 10, 96, 52};
    layout.entrances = {catalog.route.front()};
    ui.add(World, world, true, true, false);
    MapCamera controller(state, ui, layout);
    require(controller.act(LaneFocusBase, {}) && state.play.camera.panX < opening.pose().panX,
            "an off-screen entrance indicator pans toward its entrance");
    require(controller.act(MinimapFocus, {layout.minimap.x + layout.minimap.w, layout.minimap.y}) &&
                state.play.camera.panX > 0,
            "minimap taps recenter the camera on the tapped area");
    require(decodeInput({engine::Interaction::Kind::Click, LaneFocusBase + 2, {}})->kind ==
                    InputKind::Control &&
                decodeInput({engine::Interaction::Kind::Click, MinimapFocus, {}}).has_value(),
            "camera overlay controls are decoded by the app input boundary");
}
} // namespace
/** Run independent domain and input contracts without initializing SDL or a renderer. */
int main() {
    content();
    combat();
    mining();
    stageFlow();
    camera();
    openingCamera();
    std::cout << checks << " campaign checks passed\n";
}
