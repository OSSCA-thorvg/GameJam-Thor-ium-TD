#include "Scenario.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace thorium::game;
namespace {
int checks = 0;
/** Assert gameplay effects independently of release-build assertion settings. */
void require(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Verify one blocked target agrees with execution and changes no observable rule state. */
void rejects(Run &run, CardId card, Point target, PlayFailure failure) {
    const std::string before = run.debugState();
    const PlayPreview preview = run.previewPlay(card, target);
    require(!preview.allowed() && preview.failure == failure, "preview reports the specific rejection");
    require(run.debugState() == before, "target queries never mutate the live run snapshot");
    const Result committed = run.play(card, target);
    require(!committed.ok && committed.message == preview.message() && run.debugState() == before,
            "execution uses the same rejection without spending or cycling");
}
/** Cover each action family and compare preview price with actual committed effects. */
void actionContracts() {
    Run run;
    rejects(run, 1, {6, 4}, PlayFailure::BattleRequired);
    require(run.startWave().ok, "start preview scenario");
    scenario(run).energy = 10;
    rejects(run, 999, {6, 4}, PlayFailure::CardNotInHand);
    rejects(run, 1, {std::numeric_limits<float>::quiet_NaN(), 4}, PlayFailure::InvalidTarget);
    rejects(run, 1, {5, 5}, PlayFailure::BuildingPadRequired);
    const PlayPreview install = run.previewPlay(1, {6, 4});
    require(install.allowed() && install.action == PlayAction::Install && install.cost == 3,
            "empty pads preview the authored installation cost");
    require(run.play(1, {6, 4}).ok && run.state().energy == 7 && run.state().towers.size() == 1,
            "installation commits exactly the previewed cost and one building");
    const EntityId tower = run.state().towers.front().id;
    const PlayPreview upgrade = run.previewPlay(5, {6, 4});
    require(upgrade.allowed() && upgrade.action == PlayAction::Upgrade && upgrade.cost == 2,
            "same-kind level-one building previews a two-energy upgrade");
    require(run.play(5, {6, 4}).ok && run.tower(tower)->level == 2 && run.state().energy == 5,
            "upgrade preserves identity and commits its effective cost");
    const PlayPreview replacement = run.previewPlay(3, {6, 4});
    require(replacement.allowed() && replacement.action == PlayAction::Replace && replacement.cost == 4,
            "another building kind previews replacement at its authored price");
    require(run.play(3, {6, 4}).ok && run.tower(tower)->kind == CardKind::Cannon &&
                run.tower(tower)->level == 1 && run.state().energy == 1,
            "replacement preserves identity, resets level and charges the previewed price");
    scenario(run).energy = 10;
    rejects(run, 2, {5, 5}, PlayFailure::UnitCellRequired);
    const PlayPreview summon = run.previewPlay(2, {8, 4});
    require(summon.allowed() && summon.action == PlayAction::Summon && summon.cost == 3,
            "free unit cells preview an independent summon");
    require(run.play(2, {8, 4}).ok && run.state().units.size() == 1 && run.state().energy == 7,
            "summoning uses the same three-energy decision");
    rejects(run, 6, {8, 4}, PlayFailure::OccupiedCell);
    scenario(run).slots = 1;
    rejects(run, 6, {8, 6}, PlayFailure::SlotsFull);
    const PlayPreview spell = run.previewPlay(4, {6.25F, 4.25F});
    require(spell.allowed() && spell.action == PlayAction::Cast && spell.cost == 2,
            "spells retain fractional target positions and authored costs");
    rejects(run, 4, {-1, 4}, PlayFailure::OutsideBattlefield);
    require(run.play(4, {6.25F, 4.25F}).ok && run.state().energy == 5,
            "spell execution charges its previewed price");
}
/** Keep aggregate hand availability sensitive to cheap upgrades and actual legal targets. */
void availabilityContracts() {
    Run run;
    run.startWave();
    scenario(run).towers.push_back({700, CardKind::Gun, {6, 4}});
    scenario(run).energy = 2;
    const PlayAvailability cheap = run.playAvailability(1);
    require(cheap.allowed && cheap.hasTarget && cheap.minimumCost == 2 && cheap.maximumCost == 3 &&
                cheap.variableCost(),
            "a cheaper existing-tower upgrade keeps an otherwise unaffordable hand card available");
    rejects(run, 1, {9, 4}, PlayFailure::EnergyShortage);
    require(run.previewPlay(1, {6, 4}).allowed(), "the same two energy can be used at an upgrade target");
    scenario(run).energy = 1;
    const PlayAvailability shortfall = run.playAvailability(1);
    require(!shortfall.allowed && shortfall.hasTarget && shortfall.failure == PlayFailure::EnergyShortage &&
                shortfall.minimumCost == 2,
            "availability distinguishes valid targets from their current energy shortfall");
    scenario(run).energy = 10;
    scenario(run).towers.clear();
    for (Point pad : run.state().pads)
        scenario(run).towers.push_back(
            {static_cast<EntityId>(800 + run.state().towers.size()), CardKind::Gun, pad, 3});
    const PlayAvailability capped = run.playAvailability(1);
    require(!capped.allowed && !capped.hasTarget && capped.failure == PlayFailure::MaximumLevel,
            "max-level buildings are excluded rather than counted as affordable upgrade targets");
    rejects(run, 1, {6, 4}, PlayFailure::MaximumLevel);
    require(run.playAvailability(3).allowed, "a different building can still replace a capped tower");
}
/** A prior positive preview grants no authority after energy, identity or target conditions change. */
void revalidationContracts() {
    Run run;
    run.startWave();
    scenario(run).towers.push_back({700, CardKind::Gun, {6, 4}});
    scenario(run).energy = 2;
    require(run.previewPlay(1, {6, 4}).allowed(), "initial affordable upgrade previews successfully");
    require(run.dismiss(700).ok, "remove the upgrade target between preview and release");
    rejects(run, 1, {6, 4}, PlayFailure::EnergyShortage);
    require(run.state().towers.empty(), "old upgrade price cannot be reused for a new installation");
    scenario(run).energy = 10;
    require(run.previewPlay(1, {6, 4}).allowed(), "preview an affordable install");
    require(run.exchange(1).ok, "cycle the selected identity before its old command arrives");
    rejects(run, 1, {6, 4}, PlayFailure::CardNotInHand);
    Catalog content = defaultCatalog();
    content.starterDeck[1] = CardKind::Gun;
    Run concurrent = makeRun(content);
    concurrent.startWave();
    scenario(concurrent).energy = 10;
    scenario(concurrent).towers.push_back({900, CardKind::Gun, {6, 4}, 2});
    require(concurrent.previewPlay(2, {6, 4}).allowed(), "preview the last available upgrade level");
    require(concurrent.play(1, {6, 4}).ok, "another card reaches the cap before the preview is committed");
    rejects(concurrent, 2, {6, 4}, PlayFailure::MaximumLevel);
    Run occupied;
    occupied.startWave();
    require(occupied.previewPlay(2, {8, 4}).allowed(), "preview a free unit cell");
    scenario(occupied).units.push_back({900, CardKind::Guard, {8, 4}, 120, 120});
    rejects(occupied, 2, {8, 4}, PlayFailure::OccupiedCell);
    Catalog quiet = defaultCatalog();
    quiet.waves = {{}, {}, {}};
    Run transition = makeRun(quiet);
    transition.startWave();
    require(transition.previewPlay(1, {6, 4}).allowed(), "preview during combat");
    transition.advance(1.0F / 60.0F);
    rejects(transition, 1, {6, 4}, PlayFailure::BattleRequired);
}
/** Repeated queries do not emit combat events, rewind cooldowns, cycle hands or reserve entity IDs. */
void purityContracts() {
    Run run;
    run.startWave();
    const std::string before = run.debugState();
    const std::array<int, 4> hand = run.state().hand;
    const std::deque<int> queue = run.state().queue;
    for (int repeat = 0; repeat < 100; ++repeat) {
        run.previewPlay(1, {6, 4});
        run.previewPlay(2, {8, 4});
        run.playAvailability(1);
        run.playAvailability(4);
    }
    require(run.debugState() == before && run.state().hand == hand && run.state().queue == queue &&
                run.takeCombatEvents().empty(),
            "read-only queries preserve the snapshot, hand order and factual event stream");
    require(run.play(1, {6, 4}).ok && run.state().towers.front().id == 1,
            "previewing never reserves or consumes placement identities");
    const std::vector<CombatEvent> events = run.takeCombatEvents();
    require(events.size() == 1 && events.front().type == CombatEventKind::Placement,
            "only the committed action emits one placement fact");
}
/** Resolve tolerant API coordinates to one authored cell before pricing, occupying or emitting placement. */
void canonicalPlacementContracts() {
    Catalog content = defaultCatalog();
    content.balance.initialEnergy = 10;
    Run towers = makeRun(content);
    require(towers.startWave().ok && towers.play(1, {5.86F, 4}).ok,
            "public placement API accepts a target inside the authored pad tolerance");
    const EntityId id = towers.state().towers.front().id;
    require(distance(towers.tower(id)->position, {6, 4}) == 0,
            "stored building coordinates use the authored center rather than the submitted offset");
    const PlayPreview upgrade = towers.previewPlay(5, {6.14F, 4});
    require(upgrade.allowed() && upgrade.action == PlayAction::Upgrade && upgrade.cost == 2,
            "opposite sides of one pad resolve to the same existing building and upgrade price");
    require(towers.play(5, {6.14F, 4}).ok && towers.state().towers.size() == 1 &&
                towers.tower(id)->level == 2 && towers.state().energy == 5,
            "a second offset target upgrades once instead of installing a duplicate tower");
    require(towers.play(3, {6, 4.14F}).ok && towers.state().towers.size() == 1 &&
                towers.tower(id)->kind == CardKind::Cannon &&
                distance(towers.tower(id)->position, {6, 4}) == 0,
            "replacement shares canonical position and stable identity");
    for (const CombatEvent &event : towers.takeCombatEvents())
        require(event.type == CombatEventKind::Placement && distance(event.from, {6, 4}) == 0 &&
                    distance(event.to, {6, 4}) == 0,
                "placement feedback uses the same canonical target as the field entity");

    Run units = makeRun(content);
    units.startWave();
    require(units.play(2, {7.86F, 4}).ok && distance(units.state().units.front().position, {8, 4}) == 0,
            "summons also store the authored cell center");
    require(units.exchange(1).ok, "cycle another public card to draw a second unit copy");
    rejects(units, 6, {8.14F, 4}, PlayFailure::OccupiedCell);
    require(units.state().units.size() == 1, "cell tolerance cannot bypass unit occupancy");

    Catalog quiet = content;
    quiet.waves = {{}, {}, {}};
    Run movement = makeRun(quiet);
    movement.startWave();
    require(movement.play(2, {8, 4}).ok, "summon a survivor before natural preparation");
    require(movement.exchange(1).ok && movement.play(6, {8, 6}).ok,
            "summon a second survivor through the public card cycle");
    movement.advance(1.0F / 60.0F);
    const EntityId survivor = movement.state().units.front().id;
    require(movement.moveUnit(survivor, {8.14F, 6}).ok &&
                distance(movement.unit(survivor)->position, {8, 6}) == 0 &&
                distance(movement.state().units.back().position, {8, 4}) == 0,
            "preparation movement normalizes the target before swapping occupied cells");
    const std::string before = movement.debugState();
    require(!movement.moveUnit(survivor, {std::numeric_limits<float>::infinity(), 6}).ok &&
                movement.debugState() == before,
            "nonfinite movement targets cannot mutate a canonical field");
}
} // namespace
/** Verify the shared target planner without rendering, input devices or a graphics dependency. */
int main() {
    actionContracts();
    availabilityContracts();
    revalidationContracts();
    purityContracts();
    canonicalPlacementContracts();
    std::cout << checks << " play-preview checks passed\n";
}
