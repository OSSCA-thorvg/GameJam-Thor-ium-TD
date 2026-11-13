#include "Scenario.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

using namespace thorium::game;
namespace {
int checks = 0;
/** Keep release builds accountable for every mine content invariant. */
void require(bool value, const char *message) {
    ++checks;
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Build a special tile definition for fixture boards. */
MineTileDefinition special(MineSpecial kind, int grade = 1) {
    MineTileDefinition tile;
    tile.special = kind;
    tile.grade = grade;
    return tile;
}
/** Return the compact starter catalog with one special tile below the entry gallery (cell 7). */
Catalog fixture(MineTileDefinition tile) {
    Catalog content = defaultCatalog();
    content.mine.cells[7] = tile;
    return content;
}
/** Dig one reachable tile in preparation and run the miner until the job is committed. */
void excavate(Run &run, int cell) {
    const Result result = run.dig(cell);
    if (!result.ok) {
        std::cerr << "dig " << cell << " failed: " << result.message << '\n';
        std::exit(1);
    }
    for (int step = 0; run.state().job.active && step < 400; ++step)
        run.advance(.05F);
    require(!run.state().job.active, "the miner finishes its job");
}
/** Count authored tiles of a layer that satisfy a predicate. */
template <typename Predicate> int count(const MineLayerDefinition &layer, Predicate predicate) {
    return static_cast<int>(std::count_if(layer.cells.begin(), layer.cells.end(), predicate));
}
/** Each stage ships three distinct biome cutaways with one chest per layer and more nests deeper. */
void stageContent() {
    std::set<std::string> boards;
    for (int stage = 1; stage <= TotalStages; ++stage) {
        const Catalog &catalog = stageCatalog(stage);
        require(validateCatalog(catalog).ok(), "stage mine content passes validation");
        require(catalog.deeperMines.size() == 2, "every stage authors B1, B2 and B3");
        std::vector<const MineLayerDefinition *> layers{&catalog.mine};
        for (const MineLayerDefinition &layer : catalog.deeperMines)
            layers.push_back(&layer);
        std::set<int> relics;
        int previousNests = 0;
        int fossils = 0;
        for (const MineLayerDefinition *layer : layers) {
            require(layer->biome == stageMetadata(stage).biome, "cutaway palette matches the stage biome");
            require(layer->width == 7 && layer->height == 11, "campaign cutaways are 7x11");
            const int chests = count(*layer, [](const MineTileDefinition &tile) -> bool {
                return tile.special == MineSpecial::Relic;
            });
            require(chests == 1, "each layer hides exactly one relic chest");
            for (const MineTileDefinition &tile : layer->cells)
                if (tile.special == MineSpecial::Relic)
                    relics.insert(static_cast<int>(tile.relic));
            fossils += count(*layer, [](const MineTileDefinition &tile) -> bool {
                return tile.special == MineSpecial::Fossil;
            });
            require(count(*layer,
                          [](const MineTileDefinition &tile) -> bool {
                              return tile.special == MineSpecial::Energy;
                          }) >= 1,
                    "each layer has an energy vein");
            require(count(*layer,
                          [](const MineTileDefinition &tile) -> bool {
                              return tile.special == MineSpecial::Unstable;
                          }) >= 2,
                    "each layer has unstable rock");
            const int nests = count(*layer, [](const MineTileDefinition &tile) -> bool { return tile.nest; });
            require(nests >= previousNests, "deeper layers never have fewer nests");
            previousNests = nests;
            std::string board;
            for (const MineTileDefinition &tile : layer->cells)
                board += std::to_string(static_cast<int>(tile.ore)) + std::to_string(tile.grade) +
                         (tile.nest ? "n" : "") + std::to_string(static_cast<int>(tile.special)) + ",";
            require(boards.insert(board).second, "no two layers of the campaign share a board");
        }
        require(relics.size() == 3, "a stage's three chests grant three different relics");
        require(fossils == 2, "a stage carries two card fossils");
    }
}
/** Chests grant passive relics once; their effects apply immediately and to later preparations. */
void relics() {
    MineTileDefinition charm = special(MineSpecial::Relic);
    charm.relic = Relic::Charm;
    Run run = makeRun(fixture(charm));
    enterPreparation(run, 1);
    const int digs = run.state().digs;
    excavate(run, 7);
    require(run.hasRelic(Relic::Charm) && run.state().relics.size() == 1, "the chest grants its relic");
    require(run.state().digs == digs - 1 + run.catalog().balance.relicDigs,
            "the charm refunds its own dig with a bonus dig at once");
    require(run.state().iron == 0 && run.state().gold == 0, "a chest carries no minerals");
    require(run.debugState().find("\"relics\":[1]") != std::string::npos, "debug state lists relics");
    require(run.startWave().ok, "the wave starts after a relic");
    enterPreparation(run, 1);
    require(run.state().digs == run.catalog().balance.prepDigs + run.catalog().balance.relicDigs &&
                run.prepDigBudget() == run.state().digs,
            "the charm adds a dig to every later preparation");

    MineTileDefinition lens = special(MineSpecial::Relic);
    lens.relic = Relic::Lens;
    Run scanner = makeRun(fixture(lens));
    enterPreparation(scanner, 1);
    excavate(scanner, 7);
    require(scanner.state().scans ==
                scanner.catalog().balance.prepScans + scanner.catalog().balance.relicScans,
            "the lens grants a scan at once");
    enterPreparation(scanner, 1);
    require(scanner.state().scans == scanner.prepScanBudget() && scanner.prepScanBudget() == 2,
            "the lens adds a free scan to every preparation");

    MineTileDefinition battery = special(MineSpecial::Relic);
    battery.relic = Relic::Battery;
    Run charged = makeRun(fixture(battery));
    enterPreparation(charged, 1);
    excavate(charged, 7);
    require(charged.startWave().ok, "battle starts with the battery");
    require(charged.state().energy ==
                charged.catalog().balance.initialEnergy + charged.catalog().balance.relicEnergy,
            "the battery adds starting energy to every battle");

    Catalog warded = fixture(special(MineSpecial::Relic));
    warded.mine.cells[7].relic = Relic::Ward;
    warded.mine.cells[12] = {Ore::Gold, 4, 1, true};
    Run ward = makeRun(warded);
    enterPreparation(ward, 1);
    excavate(ward, 7);
    require(ward.nestReinforcements() ==
                ward.catalog().balance.nestEnemies - ward.catalog().balance.relicNestReduction,
            "the ward reduces each nest's reinforcements");
    excavate(ward, 12);
    require(ward.state().nextExtra == ward.nestReinforcements() && ward.state().gold == 4,
            "a warded nest still pays its gold with fewer reinforcements");

    Catalog duplicate = warded;
    duplicate.mine.cells[9] = duplicate.mine.cells[7];
    require(!Run::create(duplicate).run, "one relic cannot be placed twice in a catalog");
}
/** Fossils reuse the expansion reward flow: one free card offer that blocks digging until resolved. */
void fossils() {
    MineTileDefinition fossil = special(MineSpecial::Fossil);
    fossil.card = CardKind::Flame;
    Run run = makeRun(fixture(fossil));
    enterPreparation(run, 1);
    excavate(run, 7);
    require(run.state().rewardOffer && run.state().rewardOffer->source == OfferSource::Fossil &&
                run.state().rewardOffer->candidates == std::vector<CardKind>{CardKind::Flame},
            "the fossil opens a free single-card reward");
    require(!run.dig(6).ok && !run.startWave().ok, "the fossil reward must be resolved before mining on");
    const CardId replaced = run.state().deck[0].id;
    require(run.acceptOffer(run.state().rewardOffer->id, CardKind::Flame, replaced).ok,
            "the fossil card replaces an active deck card");
    require(run.state().deck[0].kind == CardKind::Flame && !run.state().rewardOffer && run.state().gold == 0,
            "the fossil card is free and keeps the deck at eight cards");
    require(run.dig(6).ok, "mining resumes after the fossil reward");

    Run skipped = makeRun(fixture(fossil));
    enterPreparation(skipped, 1);
    excavate(skipped, 7);
    require(skipped.skipOffer(skipped.state().rewardOffer->id).ok && skipped.dig(6).ok,
            "the fossil reward can be skipped");
}
/** Energy veins add bonus energy to the next battle only, bounded by the energy cap. */
void energy() {
    MineTileDefinition vein = special(MineSpecial::Energy);
    vein.amount = 3;
    Catalog content = fixture(vein);
    content.mine.cells[6] = vein;
    content.mine.cells[6].revealed = false;
    Run run = makeRun(content);
    enterPreparation(run, 1);
    excavate(run, 7);
    require(run.state().bonusEnergy == 3, "the vein stores bonus energy for the next battle");
    require(run.startWave().ok, "battle starts with stored energy");
    const RunBalance &balance = run.catalog().balance;
    require(run.state().energy == std::min(balance.energyCap, balance.initialEnergy + 3) &&
                run.state().bonusEnergy == 0,
            "stored energy is spent on the next battle start");
    enterPreparation(run, 1);
    excavate(run, 6);
    scenario(run).bonusEnergy = 99;
    require(run.startWave().ok && run.state().energy == balance.energyCap,
            "bonus energy never exceeds the cap");
    content.mine.cells[7].amount = 0;
    require(!Run::create(content).run, "energy veins need a positive amount");
}
/** Unstable rock opens cardinal neighbours as lootless rubble, sparing nests and specials. */
void collapse() {
    Catalog content = fixture(special(MineSpecial::Unstable));
    content.mine.cells[8].grade = 2;
    MineTileDefinition vein = special(MineSpecial::Energy);
    vein.amount = 2;
    content.mine.cells[6] = vein;
    content.mine.cells[12] = {Ore::Gold, 4, 1, true};
    Run run = makeRun(content);
    enterPreparation(run, 1);
    require(run.scan(7).ok && run.state().mine[7].info &&
                run.state().mine[7].special == MineSpecial::Unstable,
            "a scan reveals unstable rock like ore");
    const int digs = run.state().digs;
    excavate(run, 7);
    const RunState &state = run.state();
    require(state.digs == digs - 1, "a cave-in costs only the one dig");
    require(state.mine[8].dug && state.mine[8].rubble && state.mine[8].ore == Ore::Rock &&
                !state.mine[8].amount,
            "hard neighbours collapse into rubble even with the T1 pickaxe and lose their ore");
    require(state.iron == 0 && state.copper == 0, "collapsed tiles grant no minerals");
    require(!state.mine[6].dug && !state.mine[12].dug && !state.nextExtra,
            "special tiles and nests withstand the cave-in");
    require(state.mine[8].clueKnown && run.reachable(9), "rubble opens the gallery beyond");
    require(!state.mine[2].rubble, "already open galleries are untouched");
}
/** Reject malformed special tiles at the import boundary. */
void validation() {
    Catalog content = fixture(special(MineSpecial::Fossil));
    content.mine.cells[7].card = static_cast<CardKind>(99);
    require(!Run::create(content).run, "fossils must offer a defined card");
    content = fixture(special(MineSpecial::Unstable));
    content.mine.cells[7].nest = true;
    require(!Run::create(content).run, "special tiles cannot hide nests");
    content = fixture(special(MineSpecial::Unstable));
    content.mine.cells[7].ore = Ore::Gold;
    require(!Run::create(content).run, "special tiles carry no mineral");
    content = fixture(special(static_cast<MineSpecial>(9)));
    require(!Run::create(content).run, "unknown special kinds are rejected");
    content = fixture(special(MineSpecial::Relic));
    content.mine.cells[7].relic = static_cast<Relic>(9);
    require(!Run::create(content).run, "unknown relics are rejected");
    content = defaultCatalog();
    content.balance.relicEnergy = -1;
    require(!Run::create(content).run, "relic balance stays non-negative");
}
} // namespace
/** Verify authored per-stage cutaways and every special tile effect through real commands. */
int main() {
    stageContent();
    relics();
    fossils();
    energy();
    collapse();
    validation();
    std::cout << "mine content checks: " << checks << '\n';
}
