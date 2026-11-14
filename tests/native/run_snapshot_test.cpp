#include "Scenario.h"
#include "game/autoplay/Autoplayer.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace thorium::game;
namespace {
int checks = 0;
/** Keep release builds accountable for every snapshot invariant. */
void require(bool value, const std::string &message) {
    ++checks;
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Author a campaign stage with a mining mode and wall seed. */
Catalog campaign(int stage, MiningMode mining, std::uint32_t seed) {
    Catalog content = stageCatalog(stage);
    content.mining = mining;
    content.mineSeed = seed;
    return content;
}
/** Advance a run with the reference autoplayer at 0.1 s steps until it rests in preparation after `wave`,
 * or until it ends. Preparation is played out only for waves before `wave`. */
void playTo(Run &run, int wave) {
    for (int step = 0; step < 60000; ++step) {
        const RunState &state = run.state();
        if (state.phase == Phase::Won || state.phase == Phase::Lost)
            return;
        if (state.phase == Phase::Ready || state.phase == Phase::Prep) {
            if (state.wave >= wave)
                return;
            if (state.phase == Phase::Prep)
                autoplay::prepare(run);
            require(run.startWave().ok, "the autoplayer can start each wave");
        }
        if (step % 5 == 0)
            autoplay::playCard(run);
        run.advance(.1F);
    }
}
/** Play a run to its end and describe the outcome. */
std::string finish(Run &run) {
    playTo(run, 1000);
    return run.debugState();
}
/** A restored run continues exactly like the original: same snapshot right away, same outcome at the end. */
void lockstep(int stage, MiningMode mining, int wave, bool prepared) {
    const std::string label = "stage " + std::to_string(stage) + " mode " +
                              std::to_string(static_cast<int>(mining)) + " wave " + std::to_string(wave) +
                              (prepared ? " after preparation" : "");
    const Catalog content = campaign(stage, mining, 41 + static_cast<std::uint32_t>(stage));
    Run original = makeRun(content);
    playTo(original, wave);
    if (prepared && original.state().phase == Phase::Prep)
        autoplay::prepare(original);
    const std::string text = original.snapshot();
    require(!text.empty(), "a run between waves has a snapshot: " + label);
    Run copy = makeRun(content);
    const Result restored = copy.restore(text);
    require(restored.ok,
            "the snapshot restores into a new run: " + label + " (" + restored.message.key + ")");
    require(copy.snapshot() == text && copy.debugState() == original.debugState(),
            "the restored run reads exactly like the original: " + label);
    require(finish(copy) == finish(original), "both runs finish identically: " + label);
}
/** Snapshots exist only between waves, and restore rejects anything that does not match its catalog. */
void rejections() {
    const Catalog content = campaign(2, MiningMode::Excavate, 9);
    Run run = makeRun(content);
    playTo(run, 3);
    require(run.state().phase == Phase::Prep, "the fixture rests in preparation");
    const std::string text = run.snapshot();
    Run battle = makeRun(content);
    require(battle.startWave().ok && battle.snapshot().empty(), "a battle has no snapshot");
    Run ended = makeRun(content);
    playTo(ended, 1000);
    require(ended.snapshot().empty(), "an ended run has no snapshot");

    const auto rejected = [&text](Catalog catalog, const std::string &snapshot, const std::string &message) {
        Run fresh = makeRun(std::move(catalog));
        const std::string before = fresh.debugState();
        require(!fresh.restore(snapshot).ok && fresh.debugState() == before, message);
        static_cast<void>(text);
    };
    rejected(campaign(2, MiningMode::Excavate, 10), text, "another wall seed is rejected");
    rejected(campaign(2, MiningMode::Sweep, 9), text, "another mining mode is rejected");
    rejected(campaign(3, MiningMode::Excavate, 9), text, "another stage is rejected");
    rejected(content, text.substr(0, text.size() / 2), "a truncated snapshot is rejected");
    rejected(content, text + " 7", "trailing data is rejected");
    rejected(content, "", "an empty snapshot is rejected");
    rejected(content, "THORIUM-RUN 1" + text.substr(13), "another snapshot version is rejected");
    require(!run.state().towers.empty(), "the fixture has a building to move");
    Catalog moved = content;
    const Point pad = run.state().towers.front().position;
    moved.pads.erase(std::remove_if(moved.pads.begin(), moved.pads.end(),
                                    [pad](Point point) { return distance(point, pad) < .01F; }),
                     moved.pads.end());
    moved.padTraits.erase(
        std::remove_if(moved.padTraits.begin(), moved.padTraits.end(),
                       [pad](const TraitPad &trait) { return distance(trait.position, pad) < .01F; }),
        moved.padTraits.end());
    rejected(moved, text, "a building on a pad the stage no longer has is rejected");
    Run started = makeRun(content);
    require(started.startWave().ok && !started.restore(text).ok, "only a new run can take a snapshot");
    std::string cards = text;
    const std::size_t deck = cards.find(" END");
    require(deck != std::string::npos, "the snapshot ends with its marker");
    rejected(content, cards.replace(deck, 4, " 1 END"), "an extra token before the marker is rejected");
}
} // namespace

/** Run every snapshot contract. */
int main() {
    for (MiningMode mining : {MiningMode::Sweep, MiningMode::Excavate}) {
        lockstep(1, mining, 0, false);
        lockstep(1, mining, 3, false);
        lockstep(1, mining, 3, true);
        lockstep(3, mining, 6, true);
        lockstep(5, mining, 9, false);
    }
    rejections();
    std::cout << checks << " run snapshot checks passed\n";
}
