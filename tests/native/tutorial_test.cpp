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
    /** Complete an actual miner job through the public fixed-step path. */
    void dig(int cell) {
        require(run.dig(cell).ok, "the guided tile is actually reachable and affordable");
        require(run.state().job.active, "selecting a tile starts an asynchronous miner job");
        for (int step = 0; step < 600 && run.state().job.active; ++step)
            tick();
        require(!run.state().job.active && run.state().mine[cell].dug, "excavation completes normally");
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
    require(app.run.state().phase == game::Phase::Prep && app.state.play.tutorial.lesson == Lesson::Scan &&
                app.run.state().defeated == 4 && app.run.state().baseHp > 0,
            "the authored defenses survive all four ordinary enemies and reach preparation");
    require(app.run.state().digs == 5 && app.run.state().scans == 1,
            "preparation grants the normal five digs and one scan");
    require(app.run.scan(6).ok, "scan the authored first boundary cell");
    app.director.observe();
    require(app.state.play.tutorial.lesson == Lesson::FirstDig && app.run.state().iron == 0 &&
                app.run.state().digs == 5,
            "scan alone reveals information without granting ore or finishing excavation");
    app.dig(6);
    require(app.state.play.tutorial.lesson == Lesson::Crafting && app.run.state().iron == 2,
            "completed excavation and actual ore payment unlock crafting");
    app.dig(7);
    app.dig(11);
    app.dig(12);
    require(app.run.state().iron == 4 && app.run.state().copper == 4 && app.run.state().digs == 1,
            "four normal digs fund exactly the shipped T2 recipe");
    require(app.run.upgrade(game::UpgradeKind::Tool).ok, "craft the real T2 upgrade");
    app.director.observe();
    require(app.state.play.tutorial.lesson == Lesson::Investment && app.run.state().iron == 0 &&
                app.run.state().copper == 0 && app.run.catalog().mine[13].grade == 2,
            "paid crafting unlocks a genuinely T2 practice stratum");
    app.dig(13);
    require(app.state.play.tutorial.lesson == Lesson::Complete && app.state.app.tutorialCompleted &&
                app.run.state().gold == 4 && app.run.state().digs == 0,
            "the fifth dig grants gold and records completion");
    app.director.retry();
    require(app.state.play.tutorial.lesson == Lesson::Investment && app.run.state().gold == 0 &&
                app.run.state().tool == 2 && app.run.state().digs == 1,
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
