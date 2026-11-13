#include "Scenario.h"
#include "game/content/Excavation.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

using namespace thorium::game;
namespace {
int checks = 0;
/** Keep release builds accountable for every excavation invariant. */
void require(bool value, const char *message) {
    ++checks;
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Mineral, nest and special totals of a layer or a wall, for comparing the two. */
struct Totals {
    std::array<int, 5> ore{};
    int nests = 0;
    int nestOre = 0;
    int relics = 0;
    int fossils = 0;
    int energy = 0;
    int unstable = 0;
    /** Compare every total. */
    bool operator==(const Totals &other) const {
        return ore == other.ore && nests == other.nests && nestOre == other.nestOre &&
               relics == other.relics && fossils == other.fossils && energy == other.energy &&
               unstable == other.unstable;
    }
};
/** Add one tile's or deposit's contents to the totals. */
void add(Totals &totals, Ore ore, int amount, bool nest, MineSpecial special) {
    if (special == MineSpecial::Relic)
        ++totals.relics;
    else if (special == MineSpecial::Fossil)
        ++totals.fossils;
    else if (special == MineSpecial::Energy)
        totals.energy += amount;
    else if (special == MineSpecial::Unstable)
        ++totals.unstable;
    else if (nest) {
        ++totals.nests;
        totals.nestOre += amount;
    } else
        totals.ore[static_cast<std::size_t>(ore)] += amount;
}
/** Every campaign layer buries exactly its authored contents in a valid, reproducible wall. */
void wallContent() {
    for (int stage = 1; stage <= TotalStages; ++stage)
        for (int depth = 0; depth < 3; ++depth)
            for (std::uint32_t seed : {0U, 1U, 77U, 4096U}) {
                const MineLayerDefinition layer = campaignMine(stage, depth);
                const ExcavationWall wall = excavationWall(layer, seed);
                require(wall.width == layer.width && wall.height == layer.height &&
                            wall.cells.size() == layer.cells.size(),
                        "a wall has the size of its layer");
                Totals authored, buried;
                for (const MineTileDefinition &tile : layer.cells)
                    if (!tile.opened && tile.ore == Ore::Rock && tile.special == MineSpecial::None &&
                        !tile.nest)
                        continue;
                    else if (!tile.opened)
                        add(authored, tile.ore, tile.amount, tile.nest, tile.special);
                std::set<int> used;
                for (std::size_t index = 0; index < wall.deposits.size(); ++index) {
                    const Deposit &deposit = wall.deposits[index];
                    add(buried, deposit.ore, deposit.amount, deposit.nest, deposit.special);
                    require(!deposit.cells.empty() && deposit.cells.size() <= 4,
                            "a deposit spans 1 to 4 cells");
                    for (int cell : deposit.cells) {
                        require(cell >= 0 && cell < static_cast<int>(wall.cells.size()) &&
                                    used.insert(cell).second,
                                "deposit cells stay inside the wall and never overlap");
                        require(wall.cells[static_cast<std::size_t>(cell)].deposit ==
                                    static_cast<int>(index) + 1,
                                "wall cells point back at their deposit");
                        require(wall.cells[static_cast<std::size_t>(cell)].grade == deposit.grade,
                                "rock over a deposit has the deposit's hardness");
                    }
                }
                require(authored == buried, "a wall buries exactly the layer's minerals, nests and specials");
                for (const ExcavationCell &cell : wall.cells)
                    require(cell.cover >= 1 && cell.cover <= MaxCover &&
                                (cell.grade == 1 || cell.cover == MaxCover),
                            "cover stays in range and hard rock is full thickness");
                for (int column = 0; column < wall.width; ++column)
                    require(wall.cells[static_cast<std::size_t>(column)].grade > 1 ||
                                wall.cells[static_cast<std::size_t>(column)].cover == 1,
                            "the soft top row is one layer thick");
                const ExcavationWall again = excavationWall(layer, seed);
                bool same = again.deposits.size() == wall.deposits.size();
                for (std::size_t index = 0; same && index < wall.cells.size(); ++index)
                    same = again.cells[index].cover == wall.cells[index].cover &&
                           again.cells[index].deposit == wall.cells[index].deposit;
                require(same, "the same layer and seed give the same wall");
            }
    const MineLayerDefinition layer = campaignMine(1, 0);
    const ExcavationWall first = excavationWall(layer, 1), second = excavationWall(layer, 2);
    bool differs = false;
    for (std::size_t index = 0; index < first.cells.size(); ++index)
        differs = differs || first.cells[index].deposit != second.cells[index].deposit;
    require(differs, "another seed moves the deposits");
}
/** Start a preparation on an empty 5x6 wall of two soft layers per cell. */
Run blankWall() {
    Catalog content = defaultCatalog();
    content.mining = MiningMode::Excavate;
    content.balance.openWall = true;
    Run run = makeRun(content);
    enterPreparation(run);
    RunState &state = scenario(run);
    state.deposits.clear();
    for (MineCell &cell : state.mine) {
        cell = MineCell{};
        cell.cover = 2;
    }
    return run;
}
/** Bury one deposit under the given cover and mirror its contents into its cells. */
void bury(Run &run, Deposit deposit, std::vector<int> cells, int cover) {
    RunState &state = scenario(run);
    deposit.cells = std::move(cells);
    state.deposits.push_back(deposit);
    for (int index : deposit.cells) {
        MineCell &cell = state.mine[static_cast<std::size_t>(index)];
        cell.deposit = static_cast<int>(state.deposits.size());
        cell.cover = cover;
        cell.grade = deposit.grade;
        cell.ore = deposit.ore;
        cell.amount = deposit.amount;
        cell.nest = deposit.nest;
        cell.special = deposit.special;
        cell.relic = deposit.relic;
        cell.card = deposit.card;
    }
}
/** Build a mineral deposit. */
Deposit vein(Ore ore, int amount) {
    Deposit deposit;
    deposit.ore = ore;
    deposit.amount = amount;
    return deposit;
}
/** Read a wall cell's remaining cover. */
int cover(const Run &run, int cell) {
    return run.state().mine[static_cast<std::size_t>(cell)].cover;
}
/** Picks strip the target deeply and its neighbours lightly; a deposit pays once it is fully exposed. */
void pickAndCollect() {
    Run run = blankWall();
    bury(run, vein(Ore::Iron, 6), {7, 8}, 2);
    require(run.buriedDeposits() == 1 && run.strikeCost(StrikeTool::Pick) == 1,
            "one deposit waits in the wall");
    require(run.strikePattern(7, StrikeTool::Pick).size() == 5 &&
                run.strikePattern(0, StrikeTool::Pick).size() == 3 &&
                run.strikePattern(-1, StrikeTool::Pick).empty(),
            "a pick reaches the target and its cardinal neighbours inside the wall");
    require(run.strike(7, StrikeTool::Pick).ok, "strike the first deposit cell");
    require(cover(run, 7) == 0 && cover(run, 8) == 1 && cover(run, 2) == 1 && cover(run, 6) == 1 &&
                cover(run, 12) == 1 && cover(run, 13) == 2,
            "a T1 pick strips two layers at the target and one beside it");
    require(run.state().digs == 4 && run.state().iron == 0 && run.buriedDeposits() == 1,
            "a partly exposed deposit pays nothing yet");
    require(run.state().mine[8].info && run.state().mine[7].dug && !run.state().mine[13].info,
            "thin rock shows its contents and bare rock counts as dug");
    require(run.strike(8, StrikeTool::Pick).ok && run.state().iron == 6 && run.buriedDeposits() == 0 &&
                run.state().deposits[0].collected && run.state().digs == 3,
            "exposing the last cell collects the whole deposit once");
    require(run.strike(7, StrikeTool::Pick).ok && run.state().iron == 6,
            "a collected deposit never pays again");
    require(!run.dig(3).ok, "tile digging is rejected in an excavation");
    Run sweep;
    enterPreparation(sweep);
    require(!sweep.strike(7, StrikeTool::Pick).ok && sweep.strikePattern(7, StrikeTool::Pick).empty() &&
                sweep.buriedDeposits() == 0,
            "strikes are rejected in a tile-by-tile mine");
}
/** A blast strips a block for two digs and wakes nests it touches; a nest's ore still costs its
 * reinforcements only once. */
void blastAndNests() {
    Run run = blankWall();
    Deposit nest = vein(Ore::Gold, 8);
    nest.nest = true;
    bury(run, nest, {12, 13}, 2);
    require(run.strikeCost(StrikeTool::Blast) == 2 && run.strikePattern(12, StrikeTool::Blast).size() == 9,
            "a blast reaches a three-by-three block for two digs");
    require(run.strike(12, StrikeTool::Blast).ok && run.state().digs == 3, "blast the nest");
    require(cover(run, 12) == 1 && cover(run, 13) == 1 && cover(run, 6) == 1 && cover(run, 18) == 1 &&
                cover(run, 14) == 2,
            "a T1 blast strips one layer from every tile of the block");
    require(run.state().nextExtra == run.nestReinforcements() && run.state().gold == 0 &&
                run.state().deposits[0].woken && !run.state().deposits[0].collected,
            "a blast wakes a buried nest without giving its ore");
    require(run.strike(12, StrikeTool::Pick).ok && run.strike(13, StrikeTool::Pick).ok &&
                run.state().gold == 8 && run.state().nextExtra == run.nestReinforcements() &&
                run.state().deposits[0].collected,
            "collecting a woken nest pays its ore without waking it again");
    Run quiet = blankWall();
    bury(quiet, nest, {7, 8}, 1);
    require(quiet.strike(7, StrikeTool::Pick).ok && quiet.state().gold == 8 &&
                quiet.state().nextExtra == quiet.nestReinforcements(),
            "exposing a nest with picks pays its ore and its reinforcements");
    Run careful = blankWall();
    bury(careful, nest, {7, 8}, 2);
    require(careful.strike(2, StrikeTool::Pick).ok && careful.state().mine[7].info &&
                careful.state().nextExtra == 0,
            "a pick beside a nest shows it without waking it");
    scenario(careful).digs = 1;
    require(!careful.strike(20, StrikeTool::Blast).ok && careful.state().digs == 1,
            "a blast needs its full dig price");
    scenario(careful).digs = 0;
    require(!careful.strike(20, StrikeTool::Pick).ok, "strikes stop when the digs run out");
}
/** Hard rock needs the T2 pickaxe, and better tools strip more. */
void hardRockAndTools() {
    Run run = blankWall();
    Deposit crystal = vein(Ore::Crystal, 4);
    crystal.grade = 2;
    bury(run, crystal, {17}, 3);
    for (int soft : {12, 16, 18, 22})
        scenario(run).mine[static_cast<std::size_t>(soft)].cover = 0;
    require(!run.strike(17, StrikeTool::Pick).ok && run.state().digs == 5 && cover(run, 17) == 3,
            "a T1 pick cannot scratch hard rock and a useless strike costs nothing");
    require(run.strike(11, StrikeTool::Pick).ok && cover(run, 17) == 3,
            "nearby strikes leave hard rock alone");
    scenario(run).tool = 2;
    require(run.strike(17, StrikeTool::Pick).ok && cover(run, 17) == 0 && run.state().crystal == 4,
            "a T2 pick strips three layers, hard rock included");
    Run drill = blankWall();
    scenario(drill).tool = 3;
    for (MineCell &cell : scenario(drill).mine)
        cell.cover = 3;
    require(drill.strike(7, StrikeTool::Pick).ok && cover(drill, 7) == 0 && cover(drill, 8) == 1,
            "the T3 drill strips four layers at the target and two beside it");
    require(drill.strike(22, StrikeTool::Blast).ok && cover(drill, 22) == 1 && cover(drill, 28) == 1,
            "a T2 or better blast strips two layers");
}
/** Scans, cave-ins, energy veins, relic chests and card fossils work inside a wall. */
void specials() {
    Run run = blankWall();
    bury(run, vein(Ore::Copper, 3), {18, 19}, 3);
    scenario(run).scans = 2;
    require(run.scan(13).ok && run.state().scans == 1 && run.state().mine[18].info &&
                run.state().mine[7].info && !run.state().mine[23].info && cover(run, 18) == 3,
            "a scan shows the block around its target without stripping rock");
    require(!run.scan(13).ok && run.state().scans == 1, "a scan that shows nothing new is not spent");
    require(!run.scan(99).ok, "a scan outside the wall is rejected");

    Run caveIn = blankWall();
    Deposit cracked;
    cracked.special = MineSpecial::Unstable;
    bury(caveIn, cracked, {12}, 1);
    scenario(caveIn).mine[13].grade = 2;
    scenario(caveIn).mine[13].cover = 3;
    Deposit nest = vein(Ore::Iron, 5);
    nest.nest = true;
    bury(caveIn, nest, {6}, 1);
    require(caveIn.strike(17, StrikeTool::Pick).ok, "strike below the cracked rock");
    require(cover(caveIn, 12) == 0 && cover(caveIn, 13) == 2 && cover(caveIn, 8) == 1 &&
                cover(caveIn, 6) == 0,
            "an exposed cracked rock strips one layer all around it, hard rock included");
    require(caveIn.state().iron == 5 && caveIn.state().nextExtra == caveIn.nestReinforcements(),
            "a cave-in that exposes a deposit collects it in the same strike");

    Run energy = blankWall();
    Deposit crystal;
    crystal.special = MineSpecial::Energy;
    crystal.amount = 2;
    bury(energy, crystal, {7}, 2);
    require(energy.strike(7, StrikeTool::Pick).ok && energy.state().bonusEnergy == 2,
            "an energy vein adds to the next battle's energy");

    Run chest = blankWall();
    Deposit relic;
    relic.special = MineSpecial::Relic;
    relic.relic = Relic::Charm;
    bury(chest, relic, {7}, 2);
    require(chest.strike(7, StrikeTool::Pick).ok && chest.hasRelic(Relic::Charm) && chest.state().digs == 5,
            "a relic chest grants its relic, and the charm returns the dig at once");

    Run fossil = blankWall();
    Deposit card;
    card.special = MineSpecial::Fossil;
    card.card = CardKind::Lightning;
    bury(fossil, card, {7}, 2);
    require(fossil.strike(7, StrikeTool::Pick).ok && fossil.state().rewardOffer &&
                fossil.state().rewardOffer->source == OfferSource::Fossil,
            "a card fossil offers its card");
    require(!fossil.strike(20, StrikeTool::Pick).ok, "strikes wait until the fossil offer is resolved");
    require(fossil.skipOffer(fossil.state().rewardOffer->id).ok && fossil.strike(20, StrikeTool::Pick).ok,
            "strikes continue after the offer");
}
/** A wall is dug from the surface: strikes start in the top row and continue beside bare rock. */
void reach() {
    Catalog content = defaultCatalog();
    content.mining = MiningMode::Excavate;
    Run run = makeRun(content);
    enterPreparation(run);
    RunState &state = scenario(run);
    state.deposits.clear();
    for (MineCell &cell : state.mine) {
        cell = MineCell{};
        cell.cover = 2;
    }
    require(run.reachable(0) && run.reachable(4) && !run.reachable(5) && !run.reachable(17) &&
                !run.reachable(-1) && !run.reachable(30),
            "only the top row of a fresh wall can be struck");
    require(!run.strike(7, StrikeTool::Pick).ok && run.state().digs == 5 && cover(run, 7) == 2,
            "a strike below untouched rock is rejected without spending a dig");
    require(run.strike(2, StrikeTool::Pick).ok && cover(run, 2) == 0 && cover(run, 7) == 1,
            "strike the top row");
    require(run.reachable(2) && run.reachable(6) && run.reachable(7) && run.reachable(8) &&
                !run.reachable(12),
            "tiles touching bare rock, diagonals included, can be struck next");
    require(run.strike(7, StrikeTool::Pick).ok && cover(run, 7) == 0 && run.reachable(12) &&
                run.reachable(11),
            "the reach follows the bare rock down");
    require(run.scan(22).ok, "a scan may target any tile");
}
/** A campaign run in excavation mode starts each layer on a generated wall and rejects bad settings. */
void campaignWalls() {
    Catalog content = stageCatalog(3);
    content.mining = MiningMode::Excavate;
    content.mineSeed = 5;
    Run run = makeRun(content);
    require(run.state().mine.size() == 77 && run.buriedDeposits() > 8 &&
                run.state().deposits.size() == excavationWall(content.mine, 5).deposits.size(),
            "a campaign excavation opens on the generated wall of its first layer");
    require(std::none_of(run.state().mine.begin(), run.state().mine.end(),
                         [](const MineCell &cell) { return cell.dug || cell.cover < 1; }),
            "a fresh wall has no exposed cell");
    enterPreparation(run, 9);
    scenario(run).tool = 2;
    const std::size_t before = run.state().deposits.size();
    require(run.descend().ok && run.state().mineLayer == 1 &&
                run.state().deposits.size() == excavationWall(content.deeperMines[0], 5).deposits.size() &&
                (before > 0),
            "descending replaces the wall with the next layer's");
    Catalog invalid = stageCatalog(1);
    invalid.balance.pickPower = 0;
    require(!validateCatalog(invalid).ok(), "a pick that strips nothing is rejected");
    invalid = stageCatalog(1);
    invalid.balance.blastDigs = 0;
    require(!validateCatalog(invalid).ok(), "a free blast is rejected");
    invalid = stageCatalog(1);
    invalid.mining = static_cast<MiningMode>(7);
    require(!validateCatalog(invalid).ok(), "an unknown mining mode is rejected");
}
} // namespace

/** Run every excavation contract. */
int main() {
    wallContent();
    pickAndCollect();
    blastAndNests();
    hardRockAndTools();
    specials();
    reach();
    campaignWalls();
    std::cout << checks << " excavation checks passed\n";
}
