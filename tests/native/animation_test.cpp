#include "Scenario.h"
#include "app/FeedbackChannel.h"
#include "game/presentation/ActorAnimator.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace thorium;
using namespace thorium::engine;
using namespace thorium::game;
using namespace thorium::presentation;
namespace {
int checks = 0;
/** Fail clearly even when assertions are disabled in optimized builds. */
void require(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Compare deterministic clip time with a small allowance for float source metadata. */
bool near(float a, float b) {
    return std::abs(a - b) < .0001F;
}
/** Supply an adapter-independent authored source, without linking any graphics library. */
AnimationAssets metadata() {
    return {{"oreling",
             {75, 2.5F, {{"idle", {0, 1}}, {"move", {0, 45}}, {"attack", {45, 57}}, {"death", {57, 75}}}}}};
}
/** Verify playback identity, loop boundaries, one-shot edges and invalid input atomicity. */
void playerContracts() {
    const AnimationSets sets = animationSets(metadata(), {{"oreling", "oreling"}}, defaultCatalog());
    require(sets.error.empty() && sets.find("oreling"), "semantic profiles resolve from source metadata");
    const ActorAnimationSet clips = *sets.find("oreling");
    require(near(clips.move.duration, 1.5F) && near(clips.attack.duration, .4F) &&
                near(clips.death.duration, .6F),
            "marker durations derive from source FPS");
    AnimationPlayer player;
    require(!player.sample() && !player.takeCompletion(), "stopped player has no sample or completion");
    require(player.play(clips.move), "valid movement clip starts");
    player.advance(.3F);
    require(near(player.elapsed(), .3F) && player.sample()->frame > 0 && player.sample()->frame < 45,
            "movement advances inside its source segment");
    player.play(clips.move);
    require(near(player.elapsed(), .3F), "identical state requests preserve clip time");
    player.play(clips.move, Restart::Always);
    require(player.elapsed() == 0, "explicit restart rewinds the same clip");
    player.advance(3.2F);
    require(near(player.elapsed(), .2F) && !player.finished() && !player.takeCompletion(),
            "large deltas wrap loops without publishing one-shot completion");
    player.pause();
    player.advance(.2F);
    require(near(player.elapsed(), .2F), "local pause preserves frame and time");
    player.resume();
    player.setSpeed(2);
    player.advance(.1F);
    require(near(player.elapsed(), .4F), "local speed scales clip time once");
    require(!player.setSpeed(-1) && !player.setSpeed(std::numeric_limits<float>::infinity()),
            "invalid speeds reject without replacing the last valid rate");
    player.setSpeed(0);
    player.advance(10);
    require(near(player.elapsed(), .4F), "zero speed holds a live clip");
    player.setSpeed(1);
    player.play(clips.attack);
    player.advance(10);
    require(player.finished() && near(player.sample()->frame, 56),
            "one-shot holds its last frame and never enters the adjacent death segment");
    require(player.takeCompletion() && !player.takeCompletion(), "completion is consumed once");
    player.advance(.1F);
    player.play(clips.attack);
    require(player.finished() && !player.takeCompletion(),
            "repeated requests cannot replay a completed one-shot");
    player.play(clips.attack, Restart::Always);
    require(!player.finished() && near(player.sample()->frame, 45),
            "explicit one-shot restart re-arms completion");
    const float before = player.sample()->frame;
    AnimationClip invalid = clips.move;
    invalid.duration = 0;
    require(!player.play(invalid) && player.sample()->frame == before,
            "invalid clip cannot partially replace playback");
    player.advance(std::numeric_limits<float>::quiet_NaN());
    player.advance(-1);
    player.advance(std::numeric_limits<float>::infinity());
    require(player.sample()->frame == before, "invalid deltas cannot corrupt the playback clock");
    player.play(clips.idle);
    player.advance(100);
    require(player.sample()->frame == 0 && !player.finished() && !player.takeCompletion(),
            "static poses stay visible without generating repeated completion events");
    player.stop();
    require(!player.sample() && !player.finished(), "stop drops the clip and its completion state");
    const AnimationInfo info = metadata().at("oreling");
    require(!animationClip("oreling", info, "missing", Playback::Loop),
            "unknown semantic marker is rejected");
    AnimationInfo broken = info;
    broken.markers["move"].end = 76;
    require(!animationClip("oreling", broken, "move", Playback::Loop),
            "out-of-source clip bounds are rejected");
    require(!animationSets({}, {{"oreling", "oreling"}}, defaultCatalog()).error.empty(),
            "missing required bindings have an actionable initialization error");
    require(loopingSample(clips.move, 100).frame < 45 && loopingSample(clips.move, -10).frame == 0,
            "explicit UI clocks loop only the requested movement segment");
}
/** Protect state priority and resume the most recent locomotion after an attack. */
void stateMachineContracts() {
    const ActorAnimationSet clips =
        *animationSets(metadata(), {{"oreling", "oreling"}}, defaultCatalog()).find("oreling");
    ActorAnimator actor(clips);
    require(actor.motion() == ActorMotion::Idle && actor.sample().frame == 0, "actors begin in an idle pose");
    actor.locomotion(true);
    actor.advance(.2F);
    actor.locomotion(true);
    require(actor.motion() == ActorMotion::Move && near(actor.elapsed(), .2F),
            "movement requests do not rewind loops");
    actor.attack();
    actor.advance(.1F);
    actor.attack();
    require(actor.motion() == ActorMotion::Attack && actor.elapsed() == 0,
            "a new committed attack restarts its one-shot");
    actor.locomotion(false);
    actor.advance(.4F);
    require(actor.motion() == ActorMotion::Idle && actor.sample().frame == 0,
            "attack completion resumes the latest idle request");
    actor.attack();
    actor.locomotion(true, .5F);
    actor.advance(.6F);
    require(actor.motion() == ActorMotion::Move && near(actor.elapsed(), .1F),
            "attack overshoot transfers remaining time to slowed locomotion");
    require(actor.sample(true).frame == 0 && actor.motion() == ActorMotion::Move,
            "reduced motion samples a pose without changing the state machine");
    actor.die();
    actor.advance(.1F);
    actor.die();
    actor.attack();
    actor.locomotion(true);
    require(actor.motion() == ActorMotion::Death && near(actor.elapsed(), .1F),
            "death absorbs attack and locomotion requests without restarting");
    actor.advance(.6F);
    require(actor.finished() && actor.motion() == ActorMotion::Death && near(actor.sample().frame, 74),
            "death completes once at its final source frame");
}
/** Keep ghost lifetime independent of live enemies, phase transitions and reset identity reuse. */
void lifecycleContracts() {
    Catalog final = defaultCatalog();
    final.waves = {{}};
    Run run = makeRun(final);
    run.startWave();
    scenario(run).deck[3].kind = CardKind::Blast;
    scenario(run).enemies.push_back({77, EnemyKind::Armored, {4, 3}, 0, 1});
    const AnimationSets sets = animationSets(metadata(), {{"oreling", "oreling"}}, defaultCatalog());
    ActorAnimations actors;
    actors.observe(run, {}, sets);
    actors.advance(.1F);
    actors.observe(run, {}, sets);
    require(actors.find(77) && actors.find(77)->animator.motion() == ActorMotion::Move &&
                near(actors.find(77)->animator.elapsed(), .1F),
            "snapshots preserve per-entity playback identity");
    require(run.play(4, {4, 3}).ok && run.state().enemies.empty(),
            "spell removes a defeated enemy immediately");
    const std::vector<CombatEvent> events = run.takeCombatEvents();
    actors.observe(run, events, sets);
    require(actors.deaths() == 1 && actors.find(77)->kind == EnemyKind::Armored,
            "retired visuals retain source role and position after the game entity disappears");
    actors.advance(.1F);
    actors.observe(run, {}, sets);
    require(near(actors.find(77)->animator.elapsed(), .1F),
            "later empty collections cannot restart a death tail");
    run.advance(1.0F / 60.0F);
    require(run.state().phase == Phase::Won && actors.deaths() == 1,
            "final-wave completion does not wait for visual death completion");
    const std::string finishedRun = run.debugState();
    actors.advance(1);
    require(actors.actors().empty() && run.debugState() == finishedRun,
            "retiring death visuals never mutates game state or rewards");
    ActorAnimations unseen;
    unseen.observe(run, events, sets);
    require(unseen.find(77) && unseen.find(77)->kind == EnemyKind::Armored,
            "spawn-and-defeat within one collection still creates the correct death visual");
    unseen.reset();
    run = Run{};
    run.startWave();
    scenario(run).enemies.push_back({77, EnemyKind::Normal, {4, 3}, 0, 36});
    unseen.observe(run, {}, sets);
    require(unseen.find(77)->animator.motion() == ActorMotion::Move && unseen.deaths() == 0,
            "new-run identity reuse cannot inherit a former corpse");
    scenario(run).phase = Phase::Lost;
    unseen.observe(run, {}, sets);
    require(unseen.find(77)->animator.motion() == ActorMotion::Idle,
            "surviving enemies settle to idle after combat instead of walking in place");
    scenario(run).enemies.clear();
    unseen.observe(run, {}, sets);
    require(unseen.actors().empty(),
            "leaked or otherwise removed living instances do not leave orphan animation state");
    std::vector<CombatEvent> burst;
    for (std::size_t index = 0; index < ActorAnimations::MaxDeathVisuals + 10; ++index)
        burst.push_back({CombatEventKind::Defeated,
                         static_cast<EntityId>(1000 + index),
                         {},
                         {},
                         CardKind::Gun,
                         0,
                         true,
                         0,
                         EnemyKind::Runner});
    unseen.observe(run, burst, sets);
    require(unseen.deaths() == ActorAnimations::MaxDeathVisuals && !unseen.find(1000),
            "large defeat bursts evict only the oldest cosmetic corpses at a bounded cap");
    unseen.advance(1);
    require(unseen.actors().empty(), "all bounded death tails eventually release their instances");
}
/** Verify enemy attack facts are emitted without coupling damage to animation completion. */
void combatEventContracts() {
    Run run;
    run.startWave();
    scenario(run).units.push_back({80, CardKind::Guard, {8, 4}, 120, 120});
    scenario(run).enemies.push_back({81, EnemyKind::Armored, {8, 4}, 5, 100, 0, 0, 80});
    run.advance(1.0F / 60.0F);
    bool attacked = false;
    const std::vector<CombatEvent> events = run.takeCombatEvents();
    for (const CombatEvent &event : events)
        if (event.type == CombatEventKind::Attack && event.enemy && event.subject == 81)
            attacked = event.enemyKind == EnemyKind::Armored && distance(event.to, {8, 4}) == 0;
    require(attacked && run.unit(80)->hp == 104,
            "one enemy attack fact accompanies already-committed damage");
    ActorAnimations actors;
    actors.observe(run, events, animationSets(metadata(), {{"oreling", "oreling"}}, defaultCatalog()));
    require(actors.find(81)->animator.motion() == ActorMotion::Attack,
            "enemy attack facts drive the visual one-shot");
    actors.advance(.5F);
    require(actors.find(81)->animator.motion() == ActorMotion::Idle && run.unit(80)->hp == 104,
            "visual completion returns to blocked idle without applying damage twice");
}
/** Validate multiple profiles and every content/source/marker link before publishing bindings. */
void bindingContracts() {
    AnimationAssets sources = metadata();
    sources.emplace("beetle-source", sources.at("oreling"));
    Catalog content = defaultCatalog();
    content.enemies[0].animation = "crystal-beetle";
    const AnimationBindings bindings{{"oreling", "oreling"}, {"crystal-beetle", "beetle-source"}};
    const AnimationSets sets = animationSets(sources, bindings, content);
    require(sets.error.empty() && sets.profiles.size() == 2 &&
                sets.find("crystal-beetle")->move.asset == "beetle-source",
            "profile keys may differ from source keys and register multiple actor families");
    Run run = makeRun(content);
    run.startWave();
    scenario(run).enemies.push_back({900, EnemyKind::Normal, {4, 3}, 0, 36});
    ActorAnimations actors;
    actors.observe(run, {}, sets);
    require(actors.find(900) && actors.find(900)->animator.sample().asset == "beetle-source",
            "a second authored monster creates its own visible animation instance");
    const AnimationSets missingProfile = animationSets(sources, {{"oreling", "oreling"}}, content);
    require(missingProfile.profiles.empty() &&
                missingProfile.error.find("crystal-beetle") != std::string::npos,
            "unbound catalog references fail atomically with the offending profile");
    const AnimationSets missingSource = animationSets(metadata(), bindings, content);
    require(missingSource.profiles.empty() && missingSource.error.find("beetle-source") != std::string::npos,
            "missing source metadata cannot leave a partially usable registry");
    sources.at("beetle-source").markers.erase("death");
    require(!animationSets(sources, bindings, content).error.empty(),
            "missing semantic clips fail initialization");
    require(!animationSets(metadata(), {{"oreling", "oreling"}, {"oreling", "oreling"}}, defaultCatalog())
                 .error.empty(),
            "duplicate profile keys are rejected rather than silently shadowing a binding");
    actors.reset();
    actors.observe(run, {}, {});
    require(!actors.error().empty(),
            "unexpected runtime binding loss reports an error instead of invisible enemies");
    actors.reset();
    require(actors.error().empty(), "new-run reset clears failed presentation state");
}
} // namespace

/** Run portable animation and game-state-machine contracts without SDL or ThorVG linkage. */
int main() {
    bindingContracts();
    playerContracts();
    stateMachineContracts();
    lifecycleContracts();
    combatEventContracts();
    std::cout << checks << " animation checks passed\n";
}
