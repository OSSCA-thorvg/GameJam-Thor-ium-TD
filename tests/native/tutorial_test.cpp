#include "Scenario.h"
#include "app/AppFlow.h"
#include "app/TutorialDirector.h"
#include "game/content/TutorialCatalog.h"
#include <cstdlib>
#include <iostream>

using namespace thorium;
using namespace thorium::presentation;
namespace {
int checks = 0;
/** Fail independently of release-build assertions. */
void require(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Compose practice orchestration without a renderer, SDL, or privileged fixture mutation. */
struct Practice {
    game::Run run;
    State state;
    engine::Ui ui;
    engine::AudioMixer audio;
    Soundscape sound{audio, "/no-optional-practice-audio"};
    FeedbackChannel feedback{run, state, sound};
    TutorialDirector director{run, state, ui, feedback, sound};
    /** Supply real animation metadata for the ordinary event consumer. */
    Practice() {
        const engine::AnimationInfo info{
            75, 2.5F, {{"idle", {0, 1}}, {"move", {0, 45}}, {"attack", {45, 57}}, {"death", {57, 75}}}};
        state.app.animationSets = animationSets({{"oreling", info}}, {{"oreling", "oreling"}}, run.catalog());
        feedback.reset();
    }
    /** Advance only when practice policy allows simulation, then observe committed state. */
    void tick() {
        if (director.allowsSimulation())
            run.advance(1.0F / 60);
        feedback.collect();
        director.observe();
    }
    /** Strike the guided wall cell with the pick through the ordinary command, then observe. */
    void strike(int cell) {
        require(state.play.tutorial.mineCell == cell, "the lesson guides this strike");
        require(run.strike(cell, game::StrikeTool::Pick).ok, "the guided cell is reachable and affordable");
        tick();
    }
};
/** Validate seven lessons, ordinary costs, safe checkpoints and suspended-run isolation. */
void practiceContracts() {
    require(game::validateCatalog(game::tutorialCatalog()).ok(),
            "practice content passes production validation");
    Practice app;
    app.state.app.navigation.enterGame();
    require(app.run.startWave().ok && app.run.play(1, {9, 4}).ok, "seed a real regular expedition");
    app.run.advance(.1F);
    app.state.app.preferences.reducedMotion = true;
    const std::string regular = app.run.debugState();
    require(app.director.begin(), "practice starts through the validated content factory");
    require(app.run.state().energy == 6 && app.run.state().battleTime == 0 && !app.state.play.paused,
            "practice starts with real energy and its own simulation gate instead of manual pause");
    app.tick();
    require(app.run.state().battleTime == 0,
            "reading the initial briefing never spawns enemies or regenerates energy");
    app.director.next();
    require(app.state.play.tutorial.lesson == Lesson::Building,
            "acknowledging the goal enters building practice");
    app.director.next();
    require(app.state.play.tutorial.lesson == Lesson::Building,
            "a next click cannot complete an action lesson");
    require(!app.run.play(1, {5, 5}).ok && app.run.state().energy == 6, "invalid placement costs nothing");
    require(app.run.play(1, {9, 4}).ok, "an ordinary alternative pad remains a valid domain command");
    app.director.observe();
    require(app.state.play.tutorial.lesson == Lesson::Building && app.run.state().towers.empty() &&
                app.run.state().energy == 6,
            "a misplaced practice tower restores its safe checkpoint");
    require(app.run.play(1, {6, 4}).ok, "place the recommended gun through the normal command");
    app.tick();
    require(app.state.play.tutorial.lesson == Lesson::Unit && app.run.state().energy == 3 &&
                app.run.state().battleTime == 0,
            "actual tower creation spends three and unlocks the unit lesson");
    require(app.run.play(2, {8, 4}).ok, "the guard spends the remaining real energy");
    app.director.observe();
    require(app.state.play.tutorial.lesson == Lesson::Combat && app.run.state().energy == 0,
            "the actual guard starts the scheduled normal combat");
    for (int step = 0; step < 3600 && app.run.state().phase == game::Phase::Battle; ++step)
        app.tick();
    require(app.run.state().phase == game::Phase::Prep && app.state.play.tutorial.lesson == Lesson::Fork &&
                app.run.state().defeated == 4 && app.run.state().baseHp > 0,
            "the authored defenses survive all four ordinary enemies and reach preparation");
    require(app.run.state().digs == 5 && app.run.state().scans == 1,
            "preparation grants the normal five digs and one scan");
    require(!app.state.play.mine && !app.run.detoured(0),
            "the fork lesson shows the surface and its barricade");
    const float before = app.run.routeLength();
    require(app.run.switchFork(0).ok && app.run.detoured(0),
            "the barricade moves through the ordinary command");
    app.director.observe();
    require(app.state.play.tutorial.lesson == Lesson::Scan && app.state.play.mine &&
                app.run.routeLength() == before,
            "rerouting through the equal-length detour opens the scan lesson underground");
    require(app.run.catalog().mining == game::MiningMode::Excavate, "practice teaches the excavation wall");
    require(app.run.scan(6).ok, "scan the authored first boundary cell");
    app.director.observe();
    require(app.state.play.tutorial.lesson == Lesson::FirstDig && app.run.state().iron == 0 &&
                app.run.state().digs == 5,
            "scan alone reveals information without granting ore or finishing excavation");
    app.strike(0);
    require(app.state.play.tutorial.lesson == Lesson::FirstDig && app.run.state().iron == 0,
            "a partly bared vein pays nothing yet");
    app.strike(5);
    require(app.state.play.tutorial.lesson == Lesson::Crafting && app.run.state().iron >= 4,
            "baring the whole iron vein pays it and unlocks crafting");
    app.strike(3);
    require(app.run.state().iron >= 4 && app.run.state().copper >= 4 && app.run.state().digs == 2,
            "three strikes fund the shipped T2 recipe");
    require(app.run.upgrade(game::UpgradeKind::Tool).ok, "craft the real T2 upgrade");
    app.director.observe();
    require(app.state.play.tutorial.lesson == Lesson::Investment && app.run.state().tool == 2 &&
                app.run.state().copper == 0,
            "paid crafting unlocks the gold lesson");
    app.strike(8);
    require(app.run.state().gold == 0 && app.director.allowsSimulation(),
            "the first gold strike leaves rock");
    app.strike(7);
    require(app.state.play.tutorial.lesson == Lesson::Investment && app.director.completing() &&
                app.state.app.tutorialCompleted && app.run.state().gold == 4 && app.run.state().digs == 0 &&
                app.state.play.tutorial.mineCell == -1 && app.director.allowsSimulation(),
            "the fifth strike grants gold and records completion while the field stays in view");
    GameInput tap{};
    tap.kind = InputKind::Control;
    tap.action = World;
    require(!app.director.allows(tap, Layout{}), "the field ignores taps while the completion window waits");
    for (int step = 0; step < 20; ++step)
        app.director.advance(.1F);
    require(app.state.play.tutorial.lesson == Lesson::Investment,
            "the completion window waits at least two seconds after the gold dig");
    for (int step = 0; step < 6; ++step)
        app.director.advance(.1F);
    require(app.state.play.tutorial.lesson == Lesson::Complete && !app.director.completing(),
            "the completion window opens once the hold ends");
    app.director.retry();
    require(app.state.play.tutorial.lesson == Lesson::Investment && app.run.state().gold == 0 &&
                app.run.state().tool == 2 && app.run.state().digs == 2,
            "retry restores the current checkpoint without duplicating its gold reward");
    app.director.begin();
    require(app.state.play.tutorial.lesson == Lesson::Briefing && app.run.state().energy == 6,
            "restarting from the guide creates a fresh practice without replacing the suspended run");
    app.director.exit();
    require(app.run.debugState() == regular && app.state.app.navigation.current() == Screen::Home &&
                app.state.app.navigation.hasRun() && app.state.app.preferences.reducedMotion &&
                app.state.app.tutorialCompleted && !app.director.active(),
            "exit restores regular progress while retaining settings and session completion");
}
} // namespace
/** Run the complete practice contract with production rules and no privileged state injection. */
int main() {
    practiceContracts();
    std::cout << checks << " tutorial checks passed\n";
}
