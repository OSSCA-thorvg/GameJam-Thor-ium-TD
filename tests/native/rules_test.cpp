#include "Scenario.h"
#include "engine/ui/Ui.h"
#include "game/presentation/Feedback.h"
#include "game/presentation/Shell.h"
#include "game/rules/Run.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>

using namespace thorium::game;
namespace {
int checks = 0;
/** Keep assertions active in release builds and provide an explicit failure reason. */
void require(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Advance reproducible fixed steps, without a renderer or wall-clock sleeps. */
void seconds(Run &run, float duration) {
    for (int tick = 0; tick < static_cast<int>(std::ceil(duration * 60)); ++tick)
        run.advance(1.0F / 60.0F);
}
/** Complete one successful mining job, checking the single active-job contract. */
void excavate(Run &run, int index) {
    require(run.dig(index).ok, "valid boundary excavation");
    require(!run.startWave().ok, "cannot finish prep while working");
    seconds(run, 5);
    require(!scenario(run).job.active, "mining job completes");
}
/** Bring one specific card back through the real four-card cycle. */
void circulate(Run &run, CardId id) {
    for (int count = 0; count < 6; ++count) {
        for (int hand : scenario(run).hand)
            if (scenario(run).deck[hand].id == id)
                return;
        scenario(run).energy = 10;
        require(run.exchange(scenario(run).deck[scenario(run).hand[1]].id).ok, "card exchange circulates");
    }
    require(false, "card returns to hand");
}

/** Exercise costs, stale card identities and repeated independent summons. */
void cardsAndUnits() {
    Run run;
    require(scenario(run).deck.size() == 8 && scenario(run).queue.size() == 4, "8 deck / 4 hand / 4 queue");
    require(!run.play(1, {6, 4}).ok, "ready phase does not spend cards");
    require(scenario(run).energy == 6 && scenario(run).towers.empty(), "invalid input preserves resources");
    require(run.startWave().ok, "manual first wave starts");
    require(!run.play(1, {std::numeric_limits<float>::quiet_NaN(), 0}).ok, "nonfinite coordinates rejected");
    require(run.play(1, {6, 4}).ok && scenario(run).energy == 3, "tower charges base cost");
    require(!run.play(1, {9, 4}).ok, "used instance is no longer a hand card");
    require(run.play(2, {8, 4}).ok && scenario(run).energy == 0, "initial gun + guard fits six energy");
    require(scenario(run).units.size() == 1, "one summon creates one independent unit");
    const EntityId first = scenario(run).units[0].id;
    circulate(run, 2);
    scenario(run).energy = 10;
    require(run.play(2, {8, 6}).ok, "same card can summon a second unit");
    require(scenario(run).units.size() == 2 && scenario(run).units[1].id != first,
            "summons have separate IDs");
    for (std::size_t index = 2; index < 6; ++index) {
        circulate(run, 2);
        scenario(run).energy = 10;
        require(run.play(2, scenario(run).unitCells[index]).ok, "fill global capacity");
    }
    circulate(run, 2);
    scenario(run).energy = 10;
    const std::array<int, 4> before = scenario(run).hand;
    require(!run.play(2, scenario(run).unitCells[6]).ok, "global slot cap enforced");
    require(scenario(run).energy == 10 && scenario(run).hand == before, "capacity rejection spends nothing");
    scenario(run).phase = Phase::Prep;
    scenario(run).units[0].hp = 31;
    const Point old = scenario(run).units[0].position;
    const Point other = scenario(run).units[1].position;
    require(run.moveUnit(first, other).ok, "prep swaps occupied cells");
    require(scenario(run).units[0].hp == 31 && distance(scenario(run).units[0].position, other) == 0 &&
                distance(scenario(run).units[1].position, old) == 0,
            "swap preserves identity and health");
    scenario(run).iron = 6;
    scenario(run).copper = 2;
    require(run.upgrade(thorium::game::UpgradeKind::Armor).ok && scenario(run).units[0].maxHp == 150 &&
                scenario(run).units[0].hp == 31,
            "armor raises maximum without healing");
    require(run.heal(first).ok && scenario(run).units[0].hp == 106, "healing uses ceil half maximum");
    require(run.heal(first).ok && scenario(run).units[0].hp == 150, "healing caps maximum");
    require(!run.heal(first).ok, "full health healing has no charge");
    require(run.dismiss(first).ok && scenario(run).units.size() == 5 && !run.unit(first),
            "dismiss returns an occupied slot");
}

/** Verify field withdrawal is atomic, releases blockers, and never refunds a spent card. */
void withdrawalContracts() {
    Run run;
    require(!run.dismiss(1).ok, "ready phase rejects withdrawal");
    require(run.startWave().ok, "start withdrawal fixture");
    require(run.play(1, {6, 4}).ok && run.play(2, {8, 4}).ok, "place tower and unit for withdrawal");
    const EntityId tower = scenario(run).towers.front().id;
    const EntityId guard = scenario(run).units.front().id;
    scenario(run).towers.front().level = 3;
    scenario(run).gold = 9;
    scenario(run).iron = 5;
    scenario(run).copper = 4;
    scenario(run).crystal = 3;
    const std::array<int, 4> hand = scenario(run).hand;
    const std::deque<int> queue = scenario(run).queue;
    const CardId cardId = scenario(run).deck[0].id;
    scenario(run).enemies.push_back({900, EnemyKind::Normal, {8, 4}, 0, 50, 0, 0, guard});
    require(run.dismiss(tower).ok && !run.tower(tower) && scenario(run).towers.empty(),
            "battle tower withdrawal removes its upgrade levels");
    require(!run.dismiss(tower).ok && !run.dismiss(0).ok, "duplicate and missing withdrawals reject");
    require(run.dismiss(guard).ok && !run.unit(guard) && scenario(run).units.empty(),
            "battle unit withdrawal frees capacity");
    require(scenario(run).enemies.front().blocker == 0, "withdrawal immediately releases blocked enemies");
    require(scenario(run).energy == 0 && scenario(run).gold == 9 && scenario(run).iron == 5 &&
                scenario(run).copper == 4 && scenario(run).crystal == 3,
            "withdrawal neither refunds nor charges resources");
    require(scenario(run).hand == hand && scenario(run).queue == queue && scenario(run).deck[0].id == cardId,
            "withdrawal does not recirculate or remove a card");
    scenario(run).energy = 10;
    require(run.play(5, {6, 4}).ok && scenario(run).towers.front().level == 1 && scenario(run).energy == 7,
            "freed building pad accepts a fresh level-one tower at normal cost");
    const EntityId fresh = scenario(run).towers.front().id;
    scenario(run).phase = Phase::Won;
    require(!run.dismiss(fresh).ok && run.tower(fresh), "terminal state preserves field entities");
    scenario(run).phase = Phase::Prep;
    require(run.dismiss(fresh).ok, "preparation also permits tower withdrawal");
    require(run.catalog().card(CardKind::Cannon).range == 3.5F &&
                run.catalog().card(CardKind::Guard).range == 1.1F,
            "selection display uses the combat range definition");
}

/** Check factual combat delivery and disposable animation state independently of ThorVG. */
void feedbackContracts() {
    using thorium::presentation::Feedback;
    Run run;
    Feedback feedback;
    feedback.observeHand(run);
    require(feedback.handChanges == 0 && !feedback.cardBusy(0), "initial hand is stable without false draws");
    require(run.startWave().ok, "start feedback fixture");
    require(!run.play(1, {0, 0}).ok && run.takeCombatEvents().empty(),
            "failed placement cannot emit a success event");
    require(run.play(1, {6, 4}).ok, "place feedback tower");
    feedback.observeHand(run);
    require(feedback.cards[0].outgoing.id == 1 && feedback.cards[0].incoming.id == 5 &&
                feedback.handChanges == 1,
            "same-kind card instances still produce a replacement");
    require(feedback.cards[0].incoming.kind == feedback.cards[0].outgoing.kind && feedback.cardBusy(0),
            "identical faces get arrival animation and input guard");
    require(feedback.energySpent == 3 && feedback.energyAge == 0,
            "successful card cost starts energy feedback");
    const std::vector<CombatEvent> placed = run.takeCombatEvents();
    require(placed.size() == 1 && placed.front().type == CombatEventKind::Placement,
            "valid placement emits one committed cue");
    require(run.takeCombatEvents().empty(), "drained events are never delivered again");
    feedback.consume(placed);
    for (int tick = 0; tick < 60; ++tick)
        feedback.advance(1.0F / 60.0F, false);
    require(!feedback.cardBusy(0) && feedback.cards[0].age == Feedback::CardDuration,
            "UI transitions settle independently of combat pause");
    require(feedback.cues.size() == 1 && feedback.cues.front().age == 0,
            "combat animation remains frozen while paused");
    const std::uint64_t changes = feedback.handChanges;
    feedback.reject(5);
    feedback.observeHand(run);
    require(feedback.handChanges == changes && feedback.rejectedCard == 5,
            "failed action marks rejection without drawing another card");
    scenario(run).enemies.push_back({900, EnemyKind::Armored, {4, 3}, 0, 7});
    run.advance(1.0F / 60.0F);
    const std::vector<CombatEvent> attacked = run.takeCombatEvents();
    int damage = 0, deaths = 0, shots = 0;
    for (const CombatEvent &event : attacked) {
        if (event.type == CombatEventKind::Damage) {
            damage += event.amount;
            require(event.subject == 900 && event.enemy, "hit identifies removed enemy");
        }
        if (event.type == CombatEventKind::Defeated)
            ++deaths;
        if (event.type == CombatEventKind::Attack)
            ++shots;
    }
    require(damage == 8 && deaths == 1 && shots == 1 && scenario(run).enemies.empty(),
            "armor-correct lethal hit retains damage, death and shot facts");
    feedback.consume(attacked);
    require(feedback.hits == 1 && feedback.hitIntensity(900, true) == 1 &&
                feedback.hitIntensity(900, false) == 0,
            "hit reaction is immediate and respects subject team");
    const Point recoil = feedback.recoil(scenario(run).towers.front().id);
    require(distance(recoil, {}) > .1F, "firing creates a local recoil without moving the logical tower");
    const std::string snapshot = run.debugState();
    for (int tick = 0; tick < 120; ++tick)
        feedback.advance(1.0F / 60.0F, true);
    require(feedback.cues.empty() && feedback.hitIntensity(900, true) == 0 && run.debugState() == snapshot,
            "visual expiration cannot alter gameplay state");
    feedback.advance(std::numeric_limits<float>::quiet_NaN(), true);
    require(std::isfinite(feedback.cards[0].age), "invalid frame delta cannot poison animation clocks");
    Run spell;
    require(spell.startWave().ok, "start spell fixture");
    scenario(spell).energy = 10;
    scenario(spell).deck[3].kind = CardKind::Blast;
    scenario(spell).enemies.push_back({901, EnemyKind::Normal, {5, 3}, 0, 36});
    require(spell.play(4, {5, 3}).ok, "spell feedback fixture commits");
    const std::vector<CombatEvent> blast = spell.takeCombatEvents();
    require(
        std::count_if(blast.begin(), blast.end(),
                      [](const CombatEvent &event) { return event.type == CombatEventKind::Damage; }) == 1 &&
            std::count_if(blast.begin(), blast.end(),
                          [](const CombatEvent &event) { return event.type == CombatEventKind::Defeated; }) ==
                1,
        "instant spell deaths retain their feedback before cleanup");
    Run crowded;
    scenario(crowded).phase = Phase::Battle;
    for (int index = 0; index < 400; ++index)
        scenario(crowded).enemies.push_back(
            {static_cast<EntityId>(1000 + index), EnemyKind::Normal, {16, 5}, crowded.routeLength(), 36});
    crowded.advance(1.0F / 60.0F);
    const std::vector<CombatEvent> bounded = crowded.takeCombatEvents();
    require(bounded.size() == 256,
            "unread event backlog remains bounded even without a presentation consumer");
    feedback.consume(bounded);
    require(feedback.cues.size() == 128, "visual population has an independent hard bound");
    Feedback grouped;
    grouped.consume({{CombatEventKind::Damage, 10, {}, {}, CardKind::Gun, 12},
                     {CombatEventKind::Damage, 10, {}, {}, CardKind::Cannon, 26}});
    require(grouped.hits == 2 && grouped.cues.size() == 1 && grouped.cues.front().event.amount == 38,
            "simultaneous damage uses one legible total rather than overlapping labels");
    grouped.advance(.1F, true);
    grouped.advance(.1F, true);
    grouped.consume({{CombatEventKind::Damage, 10, {}, {1, 2}, CardKind::Gun, 12},
                     {CombatEventKind::Damage, 11, {}, {}, CardKind::Gun, 12}});
    require(grouped.cues.size() == 2 && grouped.cues.front().event.amount == 50 &&
                grouped.cues.front().age == 0 && grouped.cues.front().event.to.y == 2 &&
                grouped.hitIntensity(10, true) == 1,
            "a quick follow-up hit joins the target's running total, moves with it and flashes again");
    for (int tick = 0; tick < 12; ++tick) {
        grouped.advance(.1F, true);
        if (tick % 3 == 2)
            grouped.consume({{CombatEventKind::Damage, 10, {}, {}, CardKind::Gun, 12}});
    }
    require(std::count_if(
                grouped.cues.begin(), grouped.cues.end(),
                [](const thorium::presentation::CombatCue &cue) { return cue.event.subject == 10; }) == 2,
            "a running total closes after its span so a long fight starts a fresh number");
    const int energy = scenario(run).energy;
    feedback.revealHand(run);
    require(feedback.cardBusy(0) && feedback.cards[0].outgoing.id == 0 && scenario(run).energy == energy,
            "returning from preparation deals the current hand without replaying old faces or costs");
}

/** Exercise real board jobs, zero clues, immutable nests and atomic recruitment. */
void miningAndEconomy() {
    Run run;
    scenario(run).phase = Phase::Prep;
    scenario(run).wave = 1;
    scenario(run).digs = 5;
    scenario(run).scans = 1;
    require(run.scan(0).ok, "scan a reachable unknown rock");
    require(!scenario(run).mine[0].dug && scenario(run).digs == 5 && scenario(run).scans == 0,
            "scan gives information without digging");
    require(!scenario(run).mine[10].clueKnown, "zero scan does not flood the board");
    excavate(run, 0);
    require(scenario(run).digs == 4 && scenario(run).mine[10].clueKnown && !scenario(run).mine[10].dug &&
                !scenario(run).mine[10].info,
            "real zero digging reveals clues only");
    Run route;
    enterPreparation(route);
    scenario(route).digs = 5;
    scenario(route).scans = 1;
    excavate(route, 6);
    excavate(route, 7);
    excavate(route, 11);
    excavate(route, 12);
    require(scenario(route).iron == 4 && scenario(route).copper == 4,
            "four-step tool path produces expected resources");
    require(route.upgrade(thorium::game::UpgradeKind::Tool).ok && scenario(route).tool == 2 &&
                scenario(route).iron == 0 && scenario(route).copper == 0,
            "T2 consumes its exact recipe once");
    excavate(route, 13);
    require(scenario(route).gold == 4 && scenario(route).digs == 0, "fifth dig gives gold");
    require(!route.dig(18).ok, "no sixth dig");
    const OfferId recruitment = route.state().recruitmentOffer->id;
    require(!route.acceptOffer(recruitment, CardKind::Shooter, 99999).ok && scenario(route).gold == 4 &&
                scenario(route).recruitmentOffer.has_value(),
            "invalid replacement preserves gold and offer");
    require(route.acceptOffer(recruitment, CardKind::Shooter, scenario(route).deck[7].id).ok &&
                scenario(route).gold == 0 && !scenario(route).recruitmentOffer,
            "successful recruitment charges once");
    const CardId acquired = scenario(route).deck[7].id;
    require(!route.acceptOffer(recruitment, CardKind::Guard, acquired).ok,
            "consumed recruitment opportunity cannot repeat");
    require(route.startWave().ok, "idle preparation can end");
    bool inHand = false;
    for (int index : scenario(route).hand)
        inHand |= scenario(route).deck[index].id == acquired;
    require(inHand, "latest acquired active instance is in next hand");
    Run nest;
    scenario(nest).phase = Phase::Prep;
    scenario(nest).wave = 1;
    scenario(nest).digs = 5;
    scenario(nest).scans = 1;
    const int originalClue = scenario(nest).mine[13].clue;
    excavate(nest, 8);
    excavate(nest, 9);
    excavate(nest, 14);
    require(scenario(nest).nextExtra == 4 && scenario(nest).gold == 8,
            "dug nest gives its mineral and one future threat");
    require(scenario(nest).mine[13].clue == originalClue, "nest excavation never changes clues");
    seconds(nest, 10);
    require(scenario(nest).gold == 8 && scenario(nest).nextExtra == 4, "finished job cannot award twice");
    require(nest.startWave().ok && scenario(nest).nextExtra == 0,
            "threat reservation is consumed at next start");
}

/** Keep validated movement speeds safe during interpolation, idle digging and resource completion. */
void minerInterpolationContracts() {
    for (const float step : {std::numeric_limits<float>::denorm_min(), 1e-12F, .2F, 1000000.0F}) {
        Catalog content = defaultCatalog();
        content.waves = {{}, {}, {}};
        content.balance.minerStep = step;
        Run run = makeRun(content);
        require(run.startWave().ok, "validated timing scenario starts through the public rule API");
        run.advance(1.0F / 60.0F);
        require(run.state().phase == Phase::Prep && run.dig(0).ok,
                "natural preparation accepts a job with a one-edge walk");
        require(distance(run.minerPosition(), {2, 0}) == 0, "the miner starts at the first path cell");
        run.advance(1.0F / 60.0F);
        const Point position = run.minerPosition();
        const double fraction = std::min(1.0, (1.0 / 60.0) / static_cast<double>(step));
        require(std::isfinite(position.x) && std::isfinite(position.y) &&
                    std::abs(position.x - static_cast<float>(2 - fraction)) < .0001F && position.y == 0,
                "all validated positive timings interpolate within the finite path before integer indexing");
        if (step <= .2F) {
            seconds(run, 2);
            require(!run.state().job.active && run.state().minerCell == 0 && run.state().digs == 4 &&
                        distance(run.minerPosition(), {0, 0}) == 0,
                    "fast and ordinary movement both finish exactly one excavation");
            const std::string completed = run.debugState();
            seconds(run, 1);
            require(run.debugState() == completed, "completed interpolation cannot duplicate excavation");
        }
    }
}

/** Check natural wave completion, expansion preservation and victory/defeat ordering. */
void combatAndExpansion() {
    Run undefended;
    require(undefended.startWave().ok, "start undefended wave");
    seconds(undefended, 70);
    require(scenario(undefended).phase == Phase::Prep && scenario(undefended).baseHp == 4 &&
                scenario(undefended).spawned == 16,
            "first wave leaks exactly sixteen damage and enters prep");
    require(undefended.startWave().ok, "start second undefended wave");
    seconds(undefended, 100);
    require(scenario(undefended).phase == Phase::Lost && scenario(undefended).baseHp == 0,
            "base destruction ends the run");
    const std::string ended = undefended.debugState();
    seconds(undefended, 3);
    require(ended == undefended.debugState(), "terminal run stops all gameplay clocks");
    Run defended;
    for (std::size_t index = 0; index < scenario(defended).pads.size(); ++index)
        scenario(defended).towers.push_back({static_cast<EntityId>(1000 + index),
                                             index % 2 ? CardKind::Cannon : CardKind::Gun,
                                             scenario(defended).pads[index], 3});
    for (int wave = 1; wave <= 3; ++wave) {
        require(defended.startWave().ok, "start defended wave");
        seconds(defended, 160);
        require(scenario(defended).baseHp > 0, "defended base survives");
        if (wave < 3)
            require(scenario(defended).phase == Phase::Prep, "natural wave finish enters preparation");
        if (wave == 2) {
            const std::size_t count = scenario(defended).towers.size();
            require(scenario(defended).expansionPending, "expansion after second wave");
            require(defended.expand(1).ok && defended.routeLength() == 30,
                    "long route expands to thirty tiles");
            require(scenario(defended).towers.size() == count && scenario(defended).rewardOffer,
                    "expansion keeps towers and opens reward");
            require(defended.skipOffer(defended.state().rewardOffer->id).ok,
                    "expansion reward can be skipped");
        }
    }
    require(scenario(defended).phase == Phase::Won && scenario(defended).wave == 3,
            "third defended wave produces victory");
    Run alternative;
    scenario(alternative).phase = Phase::Prep;
    scenario(alternative).wave = 2;
    scenario(alternative).expansionPending = true;
    require(alternative.expand(2).ok && alternative.routeLength() == 22 &&
                scenario(alternative).pads.size() == 10,
            "short branch has four added building pads");
}

/** Check nested screen return paths and reduced-motion input without a renderer. */
void navigationContracts() {
    using namespace thorium::presentation;
    Navigation nav;
    require(nav.current() == Screen::Home && !nav.gameplay() && !nav.hasRun(),
            "cold launch opens title without an active expedition");
    nav.open(Screen::Settings);
    require(!nav.gameplay(), "settings never permits run simulation");
    nav.open(Screen::Guide);
    nav.back();
    require(nav.current() == Screen::Settings, "guide returns to the settings page that opened it");
    nav.back();
    require(nav.current() == Screen::Home, "settings returns to the original title");
    nav.enterGame();
    require(nav.gameplay() && nav.hasRun(), "starting an expedition enters gameplay");
    nav.open(Screen::Settings);
    nav.back();
    require(nav.gameplay(), "in-game settings returns to the same game screen");
    nav.home();
    require(!nav.gameplay() && nav.hasRun(), "returning home preserves an active expedition");
    nav.open(Screen::Credits);
    nav.back();
    require(nav.current() == Screen::Home && nav.hasRun(),
            "utility navigation preserves continue availability");
    nav.enterGame();
    nav.home(false);
    require(!nav.hasRun(), "completed expedition can leave title without a false continue action");
    Feedback feedback;
    Run run;
    feedback.observeHand(run);
    run.startWave();
    run.play(1, {6, 4});
    feedback.observeHand(run);
    const CardId incoming = feedback.cards[0].incoming.id;
    const std::uint64_t changes = feedback.handChanges;
    feedback.settleCards();
    require(!feedback.cardBusy(0) && feedback.cards[0].incoming.id == incoming &&
                feedback.handChanges == changes,
            "reduced motion settles visuals without consuming another card or losing its identity");
}

/** Verify generic pointer ownership, disabled overlays, keyboard and cancellation. */
void inputContracts() {
    using namespace thorium::engine;
    Ui ui;
    ui.add(1, {0, 0, 300, 300}, true, false, false);
    ui.add(100, {20, 20, 80, 80}, true, true);
    require(!ui.pointer({PointerPhase::Down, 7, {40, 40}}), "pointer down captures without committing");
    ui.pointer({PointerPhase::Move, 8, {180, 180}});
    require(!ui.dragging, "a different touch cannot move capture");
    ui.pointer({PointerPhase::Move, 7, {180, 180}});
    require(ui.dragging, "owned pointer starts a drag");
    const std::optional<Interaction> drop = ui.pointer({PointerPhase::Up, 7, {180, 180}});
    require(drop && drop->kind == Interaction::Kind::Drop && drop->widget == 100,
            "one drop retains original control ownership");
    require(!ui.pointer({PointerPhase::Up, 7, {180, 180}}), "duplicate release cannot emit twice");
    ui.pointer({PointerPhase::Down, 2, {40, 40}});
    ui.pointer({PointerPhase::Cancel, 2, {40, 40}});
    require(!ui.pointer({PointerPhase::Up, 2, {40, 40}}), "canceled gesture cannot activate");
    ui.add(200, {0, 0, 300, 300}, false);
    ui.pointer({PointerPhase::Down, 0, {10, 10}});
    require(!ui.pointer({PointerPhase::Up, 0, {10, 10}}), "disabled modal blocks underlying world");
    ui.beginFrame();
    ui.add(201, {0, 0, 60, 60}, false);
    ui.add(202, {70, 0, 60, 60});
    ui.nextFocus(false);
    const std::optional<Interaction> keyboard = ui.activate();
    require(keyboard && keyboard->widget == 202, "keyboard skips disabled controls and stale focus");
    ui.beginFrame();
    ui.add(1, {0, 0, 60, 60});
    ui.add(999, {70, 0, 60, 60}, true, false, false);
    ui.nextFocus(false);
    require(ui.activate() && ui.activate()->widget == 1, "widget ID one has no special keyboard policy");
    ui.nextFocus(false);
    require(ui.focused == 1, "nonfocusable surfaces are skipped by explicit capability");
    ui.focused = 999;
    require(!ui.activate(), "nonfocusable targets cannot activate from stale focus");
    ui.beginFrame();
    ui.add(300, {0, 0, 100, 100}, false);
    ui.pointer({PointerPhase::Down, 0, {20, 20}});
    ui.beginFrame();
    ui.add(300, {0, 0, 100, 100}, true);
    require(!ui.pointer({PointerPhase::Up, 0, {20, 20}}),
            "pressing a disabled arriving card cannot activate when it later becomes enabled");
}
/** Distinguish deliberate inspection from activation, even across touch, drag, occlusion and reset. */
void inspectionInputContracts() {
    using namespace thorium::engine;
    Ui ui;
    ui.add(1, {0, 0, 300, 300}, true, false, false);
    ui.add(10, {20, 20, 80, 80}, true, true, true, true);
    ui.pointer({PointerPhase::Move, 0, {40, 40}});
    for (int tick = 0; tick < 5; ++tick)
        ui.advance(.1F);
    require(ui.inspected() == 10, "stationary mouse hover exposes an explicitly inspectable widget");
    ui.pointer({PointerPhase::Move, 0, {200, 200}});
    ui.advance(.1F);
    require(!ui.inspected(), "leaving the card retires its hover panel");
    ui.cancel();
    ui.pointer({PointerPhase::Move, 0, {40, 40}, PointerDevice::Touch});
    for (int tick = 0; tick < 8; ++tick)
        ui.advance(.1F);
    require(!ui.inspected(), "touch identity zero never becomes a synthetic mouse hover");
    ui.pointer({PointerPhase::Down, 0, {40, 40}, PointerDevice::Touch});
    ui.pointer({PointerPhase::Move, 99, {200, 200}, PointerDevice::Touch});
    ui.pointer({PointerPhase::Move, 0, {200, 200}, PointerDevice::Mouse});
    for (int tick = 0; tick < 5; ++tick)
        ui.advance(.1F);
    require(ui.inspected() == 10 && !ui.dragging, "an owned stationary touch opens inspection after a hold");
    ui.pointer({PointerPhase::Move, 0, {200, 200}, PointerDevice::Touch});
    require(!ui.pointer({PointerPhase::Up, 0, {200, 200}, PointerDevice::Touch}) && ui.inspected() == 10 &&
                !ui.activate(),
            "an inspecting hold never becomes a click, drop or keyboard activation on release");
    ui.pointer({PointerPhase::Down, 0, {40, 40}, PointerDevice::Touch});
    require(!ui.inspected() && !ui.pointer({PointerPhase::Up, 0, {40, 40}, PointerDevice::Touch}),
            "the dismissing tap cannot activate the underlying card");
    ui.pointer({PointerPhase::Down, 7, {40, 40}, PointerDevice::Touch});
    ui.pointer({PointerPhase::Move, 7, {200, 200}, PointerDevice::Touch});
    ui.pointer({PointerPhase::Move, 7, {40, 40}, PointerDevice::Touch});
    for (int tick = 0; tick < 8; ++tick)
        ui.advance(.1F);
    const std::optional<Interaction> drag =
        ui.pointer({PointerPhase::Up, 7, {200, 200}, PointerDevice::Touch});
    require(!ui.inspected() && drag && drag->kind == Interaction::Kind::Drop,
            "moving before the hold threshold preserves drag intent even after returning to its origin");
    ui.nextFocus(false);
    require(ui.inspected() == 10, "keyboard focus can inspect an enabled card without committing it");
    ui.cancel();
    ui.pointer({PointerPhase::Down, 7, {40, 40}, PointerDevice::Touch});
    for (int tick = 0; tick < 5; ++tick)
        ui.advance(.1F);
    ui.add(900, {0, 0, 300, 300}, false);
    require(!ui.inspected(), "a later modal occluder hides stale inspection geometry");
    ui.cancel();
    require(!ui.inspected() && !ui.pointer({PointerPhase::Up, 7, {40, 40}, PointerDevice::Touch}),
            "focus loss and viewport cancellation retire inspection and pending activation");
}
/** Preserve simultaneous splash damage with batch-local indices across population changes. */
void batchedDamageContracts() {
    Catalog content = defaultCatalog();
    content.waves = {{}};
    for (EnemyDefinition &enemy : content.enemies)
        enemy.speed = 0;
    Run run = makeRun(content);
    run.startWave();
    scenario(run).towers = {{80000, CardKind::Cannon, {6, 4}}, {17, CardKind::Cannon, {6, 4}}};
    scenario(run).enemies = {{90000, EnemyKind::Normal, {4, 3}, 0, 100},
                             {4, EnemyKind::Armored, {4, 3}, 0, 200},
                             {700000, EnemyKind::Runner, {4, 3}, 0, 90}};
    run.advance(1.0F / 60.0F);
    require(run.state().enemies[0].hp == 48 && run.state().enemies[1].hp == 156 &&
                run.state().enemies[2].hp == 38,
            "sparse nonordered IDs receive each simultaneous cannon hit with armor applied per hit");
    scenario(run).enemies.erase(scenario(run).enemies.begin() + 1);
    for (int index = 0; index < 2048; ++index)
        scenario(run).enemies.push_back(
            {static_cast<EntityId>(800000 + index), EnemyKind::Normal, {4, 3}, 0, 1000});
    for (Tower &tower : scenario(run).towers)
        tower.cooldown = 0;
    run.advance(1.0F / 60.0F);
    require(run.state().defeated == 2 && run.state().enemies.size() == 2048,
            "compaction and population reallocation cannot leave old batch indices alive into a later tick");
    require(std::all_of(run.state().enemies.begin(), run.state().enemies.end(),
                        [](const Enemy &enemy) -> bool { return enemy.hp == 948; }),
            "every dense splash target receives exactly two hits after compaction");
    Catalog mutual = defaultCatalog();
    mutual.waves = {{}};
    mutual.enemies[0].speed = 0;
    mutual.enemies[0].attack = 120;
    Run duel = makeRun(mutual);
    duel.startWave();
    scenario(duel).units = {{88, CardKind::Guard, {4, 3}, 120, 120},
                            {99, CardKind::Guard, {10, 7}, 300, 300}};
    scenario(duel).enemies = {{710, EnemyKind::Normal, {4, 3}, 0, 10, 0, 0, 88},
                              {3, EnemyKind::Normal, {4, 3}, 0, 10, 0, 0, 88},
                              {711, EnemyKind::Normal, {10, 7}, 10, 100, 0, 0, 99}};
    duel.advance(1.0F / 60.0F);
    require(!duel.unit(88) && duel.unit(99) && duel.unit(99)->hp == 180 && duel.state().defeated == 1 &&
                duel.state().enemies.size() == 2,
            "dying attackers still hit simultaneously and independent ally indices keep their own damage");
    const std::vector<CombatEvent> events = duel.takeCombatEvents();
    require(std::count_if(events.begin(), events.end(),
                          [](const CombatEvent &event) -> bool {
                              return event.type == CombatEventKind::Damage && !event.enemy &&
                                     event.subject == 88;
                          }) == 2,
            "both committed hits are retained before their mutual-death target is removed");
}
} // namespace

/** Run meaningful domain and input contracts without SDL or ThorVG linked. */
int main() {
    cardsAndUnits();
    withdrawalContracts();
    feedbackContracts();
    navigationContracts();
    miningAndEconomy();
    minerInterpolationContracts();
    combatAndExpansion();
    batchedDamageContracts();
    inputContracts();
    inspectionInputContracts();
    std::cout << checks << " rules/input checks passed\n";
    return 0;
}
