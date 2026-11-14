#include "Scenario.h"
#include "game/autoplay/Autoplayer.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <string>

using namespace thorium::game;
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
/** Index a role's authored definition. */
const EnemyDefinition &role(const Run &run, EnemyKind kind) {
    return run.catalog().enemy(kind);
}
/** Create an enemy fixture with authored health and an idle ability timer on the primary route. */
Enemy fixture(const Run &run, EntityId id, EnemyKind kind, float progress, float ability = 99) {
    Enemy enemy{id, kind, run.routePoint(progress), progress, role(run, kind).hp, 0, 0, 0, 0};
    enemy.shield = role(run, kind).shield;
    enemy.ability = ability;
    return enemy;
}
/** Find a live enemy fixture by identity. */
const Enemy &enemy(const Run &run, EntityId id) {
    for (const Enemy &candidate : run.state().enemies)
        if (candidate.id == id)
            return candidate;
    require(false, "the enemy fixture is still alive");
    return run.state().enemies.front();
}
/** Count committed facts of one kind. */
int count(const std::vector<CombatEvent> &events, CombatEventKind type) {
    return static_cast<int>(std::count_if(events.begin(), events.end(),
                                          [type](const CombatEvent &event) { return event.type == type; }));
}
/** Check whether a point is one of the given tiles. */
bool among(const std::vector<Point> &points, Point point) {
    return std::any_of(points.begin(), points.end(),
                       [point](Point other) { return distance(other, point) < .01F; });
}
/** The practice board without the unit cells that lie strictly inside route progress 5 to 9. */
Catalog board() {
    Catalog content = defaultCatalog();
    content.unitCells = {{6, 3}, {8, 4}, {10, 7}, {12, 7}};
    return content;
}
/** Start a battle whose scheduled spawns are still seconds away, so fixtures act alone. */
Run battle(const Catalog &content) {
    Run run = makeRun(content);
    require(run.startWave().ok, "fixture battle starts");
    return run;
}
/** Find a card of one kind in the hand, circulating the hand until it arrives. */
CardId cardInHand(Run &run, CardKind kind) {
    for (int attempt = 0; attempt < 9; ++attempt) {
        for (int slot : run.state().hand)
            if (run.state().deck[slot].kind == kind)
                return run.state().deck[slot].id;
        scenario(run).energy = 10;
        run.exchange(run.state().deck[run.state().hand[0]].id);
    }
    require(false, "the deck holds the requested card");
    return 0;
}
/** Compare the wallet, the boulders and the buildings a rejected command must not touch. */
bool same(const RunState &a, const RunState &b) {
    return a.iron == b.iron && a.copper == b.copper && a.crystal == b.crystal && a.gold == b.gold &&
           a.energy == b.energy && a.boulders.size() == b.boulders.size() && a.pads.size() == b.pads.size() &&
           a.towers.size() == b.towers.size();
}
/** Run one command that must fail, and prove it changed nothing. */
template <typename Command> void rejected(Run &run, Command command, const char *message) {
    const RunState before = run.state();
    const Result result = command();
    require(!result.ok && !result.message.empty() && same(before, run.state()), message);
}

/** A boulder blocks its pad until it is cleared for iron during preparation. */
void boulders() {
    Catalog content = defaultCatalog();
    content.boulders = {{11, 5}};
    const Point rock{11, 5};
    Run run = makeRun(content);
    const RunBalance &balance = run.catalog().balance;
    require(balance.boulder.iron == 3 && balance.boulder.copper == 0 && balance.boulder.crystal == 0 &&
                balance.boulder.gold == 0,
            "a boulder is priced at three iron");
    require(run.state().boulders.size() == 1 && !among(run.state().pads, rock),
            "a boulder starts outside the usable pads");
    require(run.boulderQuote(rock).failure == PurchaseFailure::Closed, "boulders are cleared in preparation");
    rejected(
        run, [&run, rock] { return run.clearBoulder(rock); }, "a boulder cannot be cleared before a wave");
    require(run.startWave().ok, "the first wave starts");
    const CardId gun = cardInHand(run, CardKind::Gun);
    const PlayPreview blocked = run.previewPlay(gun, rock);
    require(blocked.failure == PlayFailure::PadBlocked && blocked.action == PlayAction::Install &&
                std::string(blocked.message()).find("바위") != std::string::npos,
            "a building aimed at a boulder is refused with its own message");
    rejected(run, [&run, gun, rock] { return run.play(gun, rock); }, "a blocked pad rejects placement");
    require(run.previewPlay(gun, {5, 5}).failure == PlayFailure::BuildingPadRequired,
            "open ground still asks for a pad");
    require(run.previewPlay(cardInHand(run, CardKind::Guard), rock).failure == PlayFailure::UnitCellRequired,
            "a boulder is not a unit cell");
    require(run.boulderQuote(rock).failure == PurchaseFailure::Closed, "boulders stay during battle");
    enterPreparation(run, 1);
    const PurchaseQuote shortage = run.boulderQuote(rock);
    require(shortage.failure == PurchaseFailure::Shortage && shortage.open() && shortage.cost.iron == 3 &&
                shortage.message == "철3가 필요해요",
            "an empty wallet is quoted the full price");
    rejected(run, [&run, rock] { return run.clearBoulder(rock); }, "a short wallet keeps the boulder");
    scenario(run).iron = 5;
    scenario(run).copper = 9;
    require(run.boulderQuote({6, 4}).failure == PurchaseFailure::InvalidTarget, "a pad holds no boulder");
    rejected(run, [&run] { return run.clearBoulder({6, 4}); }, "clearing a pad is refused");
    rejected(
        run, [&run] { return run.clearBoulder({std::numeric_limits<float>::quiet_NaN(), 5}); },
        "a non-finite target is refused");
    const std::size_t pads = run.state().pads.size();
    require(run.boulderQuote(rock).allowed() && run.clearBoulder({11.1F, 4.95F}).ok,
            "a tolerant tap clears the boulder");
    require(run.state().iron == 2 && run.state().copper == 9 && run.state().boulders.empty() &&
                run.state().pads.size() == pads + 1 && distance(run.state().pads.back(), rock) < .001F,
            "clearing charges iron once and turns the boulder into a pad at its tile");
    require(run.boulderQuote(rock).failure == PurchaseFailure::InvalidTarget, "a cleared boulder is gone");
    rejected(run, [&run, rock] { return run.clearBoulder(rock); }, "a boulder is cleared only once");
    require(run.startWave().ok, "the next wave starts");
    const CardId next = cardInHand(run, CardKind::Gun);
    require(run.previewPlay(next, rock).allowed() && run.play(next, rock).ok &&
                run.state().towers.size() == 1 && distance(run.state().towers.front().position, rock) < .001F,
            "the cleared pad accepts a building");
    run.reset();
    require(run.state().boulders.size() == 1 && !among(run.state().pads, rock),
            "a new run starts with its boulders again");
    Run choosing = makeRun(content);
    enterPreparation(choosing, choosing.catalog().balance.expansionWave);
    scenario(choosing).iron = 9;
    require(choosing.state().expansionPending &&
                choosing.boulderQuote(rock).failure == PurchaseFailure::Closed,
            "an open expansion choice closes boulder clearing");
}

/** High ground widens a building's range and crystal ground raises its damage. */
void traitPads() {
    Catalog content = defaultCatalog();
    content.enemies[static_cast<std::size_t>(EnemyKind::Normal)].speed = 0;
    const Catalog plain = content;
    content.padTraits = {{{6, 4}, PadTrait::HighGround}, {{9, 4}, PadTrait::Crystal}};
    Run run = battle(content);
    const RunBalance &balance = run.catalog().balance;
    const CardDefinition &gun = run.catalog().card(CardKind::Gun);
    require(balance.highGroundRange == 1.2F && balance.crystalDamage == 1.2F, "traits grant twenty percent");
    require(run.padTrait({6, 4}) == PadTrait::HighGround && run.padTrait({9, 4}) == PadTrait::Crystal &&
                run.padTrait({7, 6}) == PadTrait::None && run.padTrait({6.1F, 4.05F}) == PadTrait::HighGround,
            "a pad reports its trait at its tile");
    require(std::abs(run.buildingRange(CardKind::Gun, {6, 4}) - gun.range * 1.2F) < .0001F &&
                run.buildingRange(CardKind::Gun, {9, 4}) == gun.range &&
                run.buildingRange(CardKind::Gun, {7, 6}) == gun.range,
            "only high ground changes the range");
    require(run.padDamage({9, 4}) == 1.2F && run.padDamage({6, 4}) == 1 && run.padDamage({7, 6}) == 1,
            "only crystal ground changes the damage");
    /** Fire one building at one stationary enemy and return the damage dealt, or zero out of range. */
    const auto shot = [](const Catalog &catalog, Tower tower, float progress, Tower *support) -> int {
        Run duel = battle(catalog);
        scenario(duel).towers.push_back(tower);
        if (support)
            scenario(duel).towers.push_back(*support);
        scenario(duel).enemies.push_back(fixture(duel, 700, EnemyKind::Normal, progress));
        duel.advance(.01F);
        int dealt = 0;
        for (const CombatEvent &event : duel.takeCombatEvents())
            dealt += event.type == CombatEventKind::Damage ? event.amount : 0;
        return dealt;
    };
    /** Route progress 7.5 is the point (8, 6.5), 3.2 tiles from the pad at (6, 4). */
    require(shot(plain, {500, CardKind::Gun, {6, 4}, 1, 0}, 7.5F, nullptr) == 0 &&
                shot(content, {500, CardKind::Gun, {6, 4}, 1, 0}, 7.5F, nullptr) == gun.damage,
            "high ground reaches an enemy beyond the authored range at unchanged damage");
    require(shot(plain, {500, CardKind::Gun, {9, 4}, 1, 0}, 5, nullptr) == gun.damage &&
                shot(content, {500, CardKind::Gun, {9, 4}, 1, 0}, 5, nullptr) ==
                    static_cast<int>(std::floor(gun.damage * 1.2F)),
            "crystal ground multiplies damage");
    require(shot(content, {500, CardKind::Gun, {9, 4}, 2, 0}, 5, nullptr) ==
                static_cast<int>(std::floor(gun.damage * (1 + balance.levelBonus) * 1.2F)),
            "the crystal bonus multiplies the level bonus");
    /** A generator's authored 2.3 reach misses a building 2.6 tiles away; on high ground it supports it. */
    Tower generator{501, CardKind::Generator, {6, 4}, 1, 0};
    const float aura = run.catalog().card(CardKind::Generator).aura;
    require(shot(plain, {500, CardKind::Gun, {8.6F, 4}, 1, 0}, 5, &generator) == gun.damage &&
                shot(content, {500, CardKind::Gun, {8.6F, 4}, 1, 0}, 5, &generator) ==
                    static_cast<int>(std::floor(gun.damage * (1 + aura))),
            "high ground widens a support building's reach");
}

/** Enemies inside a tunnel cannot be targeted, blocked or hit by area effects, and move faster. */
void tunnels() {
    Catalog content = board();
    content.tunnels = {{0, 5, 9}};
    Run run = battle(content);
    const float boost = run.catalog().balance.tunnelSpeed;
    require(boost == 1.5F, "a tunnel speeds enemies up by half");
    require(run.tunnels().size() == 1 && run.tunnelAt(5, 0) && run.tunnelAt(8.9F, 0) &&
                !run.tunnelAt(4.9F, 0) && !run.tunnelAt(9, 0) && !run.tunnelAt(6, 1),
            "a span covers its route from the entry up to the exit");
    scenario(run).towers.push_back({500, CardKind::Gun, {7, 6}, 1, 0});
    scenario(run).enemies.push_back(fixture(run, 700, EnemyKind::Normal, 7));
    const int health = run.state().enemies.front().hp;
    run.advance(.01F);
    std::vector<CombatEvent> events = run.takeCombatEvents();
    require(run.tunneled(enemy(run, 700)) && run.underground(enemy(run, 700)) &&
                !run.burrowed(enemy(run, 700)),
            "an enemy on the span is inside the tunnel");
    require(count(events, CombatEventKind::Attack) == 0 && enemy(run, 700).hp == health,
            "a tower one tile from a tunnelled enemy does not fire");
    require(std::abs(enemy(run, 700).progress - 7 - role(run, EnemyKind::Normal).speed * boost * .01F) <
                .0001F,
            "a tunnelled enemy moves at the tunnel speed");
    scenario(run).enemies.front().progress = 9.2F;
    run.advance(.01F);
    require(!run.tunneled(enemy(run, 700)) && enemy(run, 700).hp < health,
            "the same tower fires once the enemy leaves the tunnel");
    require(std::abs(enemy(run, 700).progress - 9.2F - role(run, EnemyKind::Normal).speed * .01F) < .0001F,
            "an enemy outside the tunnel moves at its own speed");

    Run guarded = battle(content);
    scenario(guarded).units.push_back({600, CardKind::Guard, {8, 4}, 120, 120});
    scenario(guarded).enemies.push_back(fixture(guarded, 701, EnemyKind::Normal, 4.8F));
    scenario(guarded).enemies.push_back(fixture(guarded, 702, EnemyKind::Normal, 5.2F));
    guarded.advance(.01F);
    require(enemy(guarded, 701).blocker == 600 && enemy(guarded, 702).blocker == 0,
            "a guard at the mouth blocks the enemy outside and not the one inside");
    for (int step = 0; step < 20; ++step)
        guarded.advance(.1F);
    require(enemy(guarded, 702).hp == role(guarded, EnemyKind::Normal).hp,
            "a unit cannot attack an enemy inside the tunnel");

    Run spells = battle(content);
    scenario(spells).enemies.push_back(fixture(spells, 703, EnemyKind::Armored, 8.5F));
    scenario(spells).enemies.push_back(fixture(spells, 704, EnemyKind::Armored, 9.5F));
    const int armored = role(spells, EnemyKind::Armored).hp;
    require(spells.play(cardInHand(spells, CardKind::Frost), {9, 7}).ok, "frost is cast over the mouth");
    require(enemy(spells, 703).slowUntil == 0 && enemy(spells, 704).slowUntil > spells.state().battleTime,
            "frost slows only the enemy outside the tunnel");
    require(spells.play(cardInHand(spells, CardKind::Blast), {9, 7}).ok, "blast is cast over the mouth");
    require(enemy(spells, 703).hp == armored && enemy(spells, 704).hp < armored,
            "blast damages only the enemy outside the tunnel");
    Run tar = battle(content);
    scenario(tar).enemies.push_back(fixture(tar, 705, EnemyKind::Armored, 8));
    scenario(tar).enemies.push_back(fixture(tar, 706, EnemyKind::Armored, 9.5F));
    scenario(tar).fields.push_back({900, CardKind::Tar, {9, 7}, 1.8F, 99});
    tar.advance(.01F);
    require(enemy(tar, 705).slowUntil == 0 && enemy(tar, 706).slowUntil > tar.state().battleTime,
            "a tar area slows only the enemy outside the tunnel");

    Run healing = battle(content);
    scenario(healing).enemies.push_back(fixture(healing, 710, EnemyKind::Healer, 7, .005F));
    scenario(healing).enemies.push_back(fixture(healing, 711, EnemyKind::Normal, 9.3F));
    scenario(healing).enemies.back().hp = 5;
    healing.advance(.01F);
    require(count(healing.takeCombatEvents(), CombatEventKind::Heal) == 0 && enemy(healing, 711).hp == 5 &&
                enemy(healing, 710).ability == .005F,
            "an ability timer stands still inside a tunnel");
    Run sheltered = battle(content);
    scenario(sheltered).enemies.push_back(fixture(sheltered, 712, EnemyKind::Healer, 9.5F, .005F));
    scenario(sheltered).enemies.push_back(fixture(sheltered, 713, EnemyKind::Normal, 8.5F));
    scenario(sheltered).enemies.back().hp = 5;
    sheltered.advance(.01F);
    require(count(sheltered.takeCombatEvents(), CombatEventKind::Heal) == 0 && enemy(sheltered, 713).hp == 5,
            "a healer outside cannot reach an enemy inside the tunnel");

    Run digging = battle(content);
    scenario(digging).enemies.push_back(fixture(digging, 720, EnemyKind::Burrower, 6));
    scenario(digging).enemies.back().burrowUntil = 99;
    digging.advance(.01F);
    const EnemyDefinition &burrower = role(digging, EnemyKind::Burrower);
    require(std::abs(enemy(digging, 720).progress - 6 -
                     burrower.speed * std::max(burrower.abilitySpeed, boost) * .01F) < .0001F,
            "burrowing inside a tunnel uses the faster multiplier instead of both");

    Run expanded = makeRun(content);
    require(expanded.tunnels().front().enter == 5 && expanded.tunnels().front().exit == 9,
            "a span starts at its authored progress");
    const Point mouth = expanded.routePoint(5);
    enterPreparation(expanded, expanded.catalog().balance.expansionWave);
    require(expanded.expand(1).ok, "the long expansion is applied");
    const RouteSpan moved = expanded.tunnels().front();
    require(std::abs(moved.enter - 17) < .001F && std::abs(moved.exit - 21) < .001F &&
                distance(expanded.routePoint(moved.enter), mouth) < .001F && expanded.tunnelAt(18, 0) &&
                !expanded.tunnelAt(6, 0),
            "a surface expansion keeps the tunnel on the same tiles");
    require(expanded.catalog().tunnels.front().enter == 5, "the authored span itself is unchanged");
}

/** A lava vent erupts on its fixed period and hits surfaced enemies and allied units in its radius. */
void vents() {
    Catalog content = board();
    content.enemies[static_cast<std::size_t>(EnemyKind::Normal)].speed = 0;
    content.enemies[static_cast<std::size_t>(EnemyKind::Armored)].speed = 0;
    content.enemies[static_cast<std::size_t>(EnemyKind::Burrower)].speed = 0;
    content.vents = {{{12, 6}, 1}};
    const Point vent{12, 6};
    Run idle = makeRun(content);
    require(idle.ventCountdown(0) == -1 && idle.ventCountdown(1) == -1, "a vent is dormant outside battle");
    require(idle.ventThreatens({12, 7}) && idle.ventThreatens({13, 7}) && !idle.ventThreatens({10, 7}),
            "a vent threatens the tiles inside its radius");
    Run run = battle(content);
    const RunBalance &balance = run.catalog().balance;
    require(balance.ventPeriod == 9 && balance.ventWarning == 1.5F && balance.ventRadius == 1.5F &&
                balance.ventDamage == 30,
            "vents erupt every nine seconds for thirty damage after a short warning");
    require(std::abs(run.ventCountdown(0) - 1) < .001F, "the first eruption follows the authored phase");
    /** Enemies stand 1.4 tiles from the vent, the first unit 1.4 tiles and the second 2.2 tiles away. */
    scenario(run).enemies.push_back(fixture(run, 700, EnemyKind::Armored, 13));
    scenario(run).enemies.push_back(fixture(run, 701, EnemyKind::Normal, 13));
    scenario(run).enemies.push_back(fixture(run, 702, EnemyKind::Burrower, 13));
    scenario(run).enemies.back().burrowUntil = 99;
    scenario(run).enemies.push_back(fixture(run, 703, EnemyKind::Normal, 4));
    scenario(run).units.push_back({600, CardKind::Guard, {11, 7}, 120, 120});
    scenario(run).units.push_back({601, CardKind::Guard, {10, 7}, 120, 120});
    scenario(run).towers.push_back({500, CardKind::Generator, {12, 5}, 2, 0});
    int eruptions = 0;
    for (int step = 0; step < 9; ++step) {
        run.advance(.1F);
        eruptions += count(run.takeCombatEvents(), CombatEventKind::Eruption);
    }
    require(eruptions == 0 && enemy(run, 700).hp == role(run, EnemyKind::Armored).hp &&
                run.ventCountdown(0) > 0 && run.ventCountdown(0) < balance.ventWarning,
            "nothing is hit while the warning counts down");
    run.advance(.1F);
    const std::vector<CombatEvent> events = run.takeCombatEvents();
    require(count(events, CombatEventKind::Eruption) == 1, "the eruption is one committed fact");
    for (const CombatEvent &event : events) {
        if (event.type == CombatEventKind::Eruption)
            require(event.amount == 3 && event.radius == balance.ventRadius && event.subject == 0 &&
                        !event.enemy && distance(event.from, vent) < .001F,
                    "the eruption fact carries its vent, radius and hit count");
        if (event.type == CombatEventKind::Damage)
            require(event.amount == balance.ventDamage && distance(event.from, vent) < .001F,
                    "every hit of this step comes from the vent");
    }
    require(count(events, CombatEventKind::Damage) == 3, "three targets are hit once each");
    require(enemy(run, 700).hp == role(run, EnemyKind::Armored).hp - balance.ventDamage,
            "an eruption ignores armor");
    require(enemy(run, 701).hp == role(run, EnemyKind::Normal).hp - balance.ventDamage &&
                enemy(run, 703).hp == role(run, EnemyKind::Normal).hp,
            "only enemies inside the radius are hit");
    require(enemy(run, 702).hp == role(run, EnemyKind::Burrower).hp, "a burrowed enemy is not hit");
    require(run.unit(600)->hp == 120 - balance.ventDamage && run.unit(601)->hp == 120,
            "an allied unit inside the radius is hit and one outside is not");
    require(run.tower(500) && run.tower(500)->level == 2 && run.tower(500)->disabled == 0 &&
                run.tower(500)->cooldown == 0,
            "a building beside the vent is unaffected");
    require(run.ventCountdown(0) > balance.ventPeriod - .2F, "the countdown restarts after an eruption");
    for (int step = 0; step < 85; ++step) {
        run.advance(.1F);
        eruptions += count(run.takeCombatEvents(), CombatEventKind::Eruption);
    }
    require(eruptions == 0, "the vent stays quiet for a full period");
    require(run.unit(600)->hp > 120 - balance.ventDamage, "a unit holding no enemy recovers over time");
    const int before = run.unit(600)->hp;
    for (int step = 0; step < 10; ++step) {
        run.advance(.1F);
        eruptions += count(run.takeCombatEvents(), CombatEventKind::Eruption);
    }
    require(eruptions == 1 && run.unit(600)->hp <= before - balance.ventDamage + 2,
            "the second eruption follows exactly one period later");
    /** The same battle at sixty steps per second erupts the same number of times. */
    Run fine = battle(content);
    int fineEruptions = 0;
    for (int step = 0; step < 630; ++step) {
        fine.advance(1.0F / 60);
        fineEruptions += count(fine.takeCombatEvents(), CombatEventKind::Eruption);
    }
    require(fineEruptions == 2, "eruptions do not depend on the step size");
    Run resting = makeRun(content);
    enterPreparation(resting, 1);
    require(resting.ventCountdown(0) == -1, "a vent is dormant during preparation");

    Catalog covered = board();
    covered.enemies[static_cast<std::size_t>(EnemyKind::Normal)].speed = 0;
    covered.tunnels = {{0, 5, 9}};
    covered.vents = {{{9, 5}, .1F}};
    Run shelter = battle(covered);
    scenario(shelter).enemies.push_back(fixture(shelter, 710, EnemyKind::Normal, 6));
    shelter.advance(.1F);
    const std::vector<CombatEvent> quiet = shelter.takeCombatEvents();
    require(count(quiet, CombatEventKind::Eruption) == 1 && count(quiet, CombatEventKind::Damage) == 0 &&
                enemy(shelter, 710).hp == role(shelter, EnemyKind::Normal).hp,
            "an eruption does not reach an enemy inside a tunnel");
}

/** Verify one rejected catalog and its field-specific diagnostic. */
void reject(const std::function<void(Catalog &)> &mutate, const std::string &field) {
    Catalog candidate = defaultCatalog();
    mutate(candidate);
    RunCreation result = Run::create(std::move(candidate));
    require(!result.run && !result.validation.ok(), "invalid gimmick data must not construct a run");
    require(std::any_of(result.validation.issues.begin(), result.validation.issues.end(),
                        [&field](const CatalogIssue &issue) -> bool { return issue.path == field; }),
            field.c_str());
}
/** Malformed boulders, traits, tunnels and vents are rejected with the path of the offending field. */
void validation() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    reject([](Catalog &data) { data.boulders = {{-1, 2}}; }, "boulders[0]");
    reject([](Catalog &data) { data.boulders = {{11, 5}, {11, 5}}; }, "boulders");
    reject([](Catalog &data) { data.boulders = {{6, 4}}; }, "boulders");
    reject([](Catalog &data) { data.boulders = {{1, 2}}; }, "boulders");
    reject([](Catalog &data) { data.boulders = {{8, 4}}; }, "boulders");
    reject([](Catalog &data) { data.balance.boulder = {}; }, "balance.boulder");
    reject([](Catalog &data) { data.balance.boulder.iron = -1; }, "balance.boulder.iron");
    reject([](Catalog &data) { data.padTraits = {{{11, 5}, PadTrait::HighGround}}; },
           "padTraits[0].position");
    reject([](Catalog &data) { data.padTraits = {{{6, 4}, PadTrait::None}}; }, "padTraits[0].trait");
    reject([](Catalog &data) { data.padTraits = {{{6, 4}, static_cast<PadTrait>(7)}}; },
           "padTraits[0].trait");
    reject(
        [](Catalog &data) { data.padTraits = {{{6, 4}, PadTrait::HighGround}, {{6, 4}, PadTrait::Crystal}}; },
        "padTraits[1].position");
    reject([](Catalog &data) { data.balance.highGroundRange = .5F; }, "balance.highGroundRange");
    reject([nan](Catalog &data) { data.balance.crystalDamage = nan; }, "balance.crystalDamage");
    /** The practice route is 18 tiles long; tunnel fixtures drop the unit cells unless they test them. */
    reject([](Catalog &data) { data.tunnels = {{3, 5, 8}}; }, "tunnels[0].route");
    reject(
        [](Catalog &data) {
            data.unitCells.clear();
            data.tunnels = {{0, 0, 3}};
        },
        "tunnels[0].enter");
    reject(
        [nan](Catalog &data) {
            data.unitCells.clear();
            data.tunnels = {{0, nan, 8}};
        },
        "tunnels[0].enter");
    reject(
        [](Catalog &data) {
            data.unitCells.clear();
            data.tunnels = {{0, 20, 24}};
        },
        "tunnels[0].enter");
    reject(
        [](Catalog &data) {
            data.unitCells.clear();
            data.tunnels = {{0, 12, 18}};
        },
        "tunnels[0].exit");
    reject(
        [](Catalog &data) {
            data.unitCells.clear();
            data.tunnels = {{0, 12, 17.5F}};
        },
        "tunnels[0].exit");
    reject(
        [](Catalog &data) {
            data.unitCells.clear();
            data.tunnels = {{0, 8, 5}};
        },
        "tunnels[0].exit");
    reject(
        [](Catalog &data) {
            data.unitCells.clear();
            data.tunnels = {{0, 5, 6}};
        },
        "tunnels[0].exit");
    reject(
        [](Catalog &data) {
            data.unitCells.clear();
            data.tunnels = {{0, 2, 6}, {0, 5, 8}};
        },
        "tunnels[1]");
    reject([](Catalog &data) { data.tunnels = {{0, 5, 9}}; }, "tunnels[0]");
    reject([](Catalog &data) { data.tunnels.assign(CatalogLimits::MaxTunnels + 1, {0, 5, 9}); }, "tunnels");
    reject([](Catalog &data) { data.balance.tunnelSpeed = 0; }, "balance.tunnelSpeed");
    reject([](Catalog &data) { data.vents = {{{-1, 3}, 1}}; }, "vents[0].position");
    reject([](Catalog &data) { data.vents = {{{12, 40}, 1}}; }, "vents[0].position");
    reject([](Catalog &data) { data.vents = {{{6, 4}, 1}}; }, "vents[0].position");
    reject([](Catalog &data) { data.vents = {{{12, 6}, 1}, {{12, 6}, 3}}; }, "vents[1].position");
    reject([](Catalog &data) { data.vents = {{{12, 6}, 0}}; }, "vents[0].phase");
    reject([](Catalog &data) { data.vents = {{{12, 6}, 9.5F}}; }, "vents[0].phase");
    reject([](Catalog &data) { data.vents = {{{12, 6}, -1}}; }, "vents[0].phase");
    reject([nan](Catalog &data) { data.vents = {{{12, 6}, nan}}; }, "vents[0].phase");
    reject([](Catalog &data) { data.vents.assign(CatalogLimits::MaxVents + 1, {{12, 6}, 1}); }, "vents");
    reject([](Catalog &data) { data.balance.ventPeriod = 0; }, "balance.ventPeriod");
    reject([](Catalog &data) { data.balance.ventPeriod = -9; }, "balance.ventPeriod");
    reject([](Catalog &data) { data.balance.ventWarning = 10; }, "balance.ventWarning");
    reject([](Catalog &data) { data.balance.ventRadius = 0; }, "balance.ventRadius");
    reject([](Catalog &data) { data.balance.ventDamage = 0; }, "balance.ventDamage");
    Catalog authored = board();
    authored.boulders = {{11, 5}};
    authored.padTraits = {
        {{6, 4}, PadTrait::HighGround}, {{11, 5}, PadTrait::Crystal}, {{1, 2}, PadTrait::Crystal}};
    authored.tunnels = {{0, 5, 9}, {0, 13, 15}};
    authored.vents = {{{12, 6}, 9}, {{9, 5}, 8.5F}};
    const CatalogValidation valid = validateCatalog(authored);
    if (!valid.ok())
        std::cerr << valid.summary() << '\n';
    require(valid.ok(),
            "traits on pads, boulders and expansion pads, separate tunnels and vents are accepted");
    require(defaultCatalog().boulders.empty() && defaultCatalog().padTraits.empty() &&
                defaultCatalog().tunnels.empty() && defaultCatalog().vents.empty(),
            "the practice board has no gimmick");
}

/** Every stage ships exactly its own gimmick, placed on its existing layout. */
void shipped() {
    for (int stage = 1; stage <= TotalStages; ++stage) {
        const Catalog &catalog = stageCatalog(stage);
        const CatalogValidation valid = validateCatalog(catalog);
        if (!valid.ok())
            std::cerr << "stage " << stage << ": " << valid.summary() << '\n';
        require(valid.ok(), "every shipped stage validates with its gimmick");
        require(!stageMetadata(stage).gimmick.empty(), "every stage names its gimmick");
        require(catalog.boulders.empty() == (stage != 1 && stage != 6),
                "only the first and sixth stages have boulders");
        require(catalog.padTraits.empty() == (stage != 2 && stage != 5),
                "trait pads belong to stages 2 and 5");
        require(catalog.tunnels.empty() == (stage != 3 && stage != 5), "tunnels belong to stages 3 and 5");
        require(catalog.vents.empty() == (stage != 4 && stage != 5), "vents belong to stages 4 and 5");
        require(catalog.rails.empty() == (stage != 6), "cart rails belong to stage 6");
        require(catalog.bogs.empty() == (stage != 7) && catalog.tanks.empty() == (stage != 7),
                "bogs and gas tanks belong to stage 7");
        std::vector<Point> sites = catalog.pads;
        sites.insert(sites.end(), catalog.boulders.begin(), catalog.boulders.end());
        for (const TraitPad &pad : catalog.padTraits)
            require(among(sites, pad.position), "a shipped trait sits on a pad");
        Run run = makeRun(catalog);
        for (const VentDefinition &vent : catalog.vents) {
            bool beside = false;
            for (std::uint32_t lane = 0; lane <= catalog.additionalRoutes.size(); ++lane)
                for (float progress = 0; progress <= run.routeLength(lane); progress += .5F)
                    beside = beside || distance(run.routePoint(progress, lane), vent.position) <= 1.01F;
            require(beside, "a shipped vent stands next to a route");
        }
        for (const RouteSpan &span : run.tunnels())
            for (Point cell : catalog.unitCells)
                for (float progress = span.enter + .5F; progress < span.exit; progress += .5F)
                    require(distance(run.routePoint(progress, span.route), cell) > .01F,
                            "no shipped unit cell lies inside a tunnel");
    }
    const Catalog &first = stageCatalog(1);
    require(first.boulders.size() == 3 && among(first.pads, {6, 4}) && among(first.pads, {9, 4}),
            "the first stage has three boulders beside its original pads");
    int high = 0, crystal = 0;
    for (const TraitPad &pad : stageCatalog(2).padTraits)
        (pad.trait == PadTrait::HighGround ? high : crystal) += 1;
    require(high == 2 && crystal == 2, "the second stage has two pads of each trait");
    require(stageCatalog(3).tunnels.size() == 1 && stageCatalog(4).vents.size() == 5,
            "the third stage has one tunnel and the fourth five vents");
    const Catalog &last = stageCatalog(5);
    require(last.tunnels.size() == 1 && last.vents.size() >= 2 && last.vents.size() <= 3 &&
                last.padTraits.size() >= 3,
            "the last stage mixes one tunnel, two or three vents and a few trait pads");

    /** Stage 4 vents erupt at 0.5, 2, 4, 6, 8, 9.5 and 11 seconds of a battle. */
    Run lava = makeRun(stageCatalog(4));
    require(lava.startWave().ok, "the lava stage starts");
    int eruptions = 0;
    for (int step = 0; step < 120; ++step) {
        lava.advance(.1F);
        eruptions += count(lava.takeCombatEvents(), CombatEventKind::Eruption);
    }
    require(eruptions == 7, "authored phases stagger the lava vents deterministically");

    /** An undefended first wave of stage 3 walks through the tunnel and leaks to the last enemy. */
    Run tundra = makeRun(stageCatalog(3));
    scenario(tundra).units.clear();
    const std::vector<LaneForecast> forecast = tundra.laneForecast();
    require(tundra.startWave().ok, "the tunnel stage starts");
    const int scheduled = tundra.waveSpawnCount();
    require(forecast.front().count == scheduled, "the forecast counts the lane that runs through the tunnel");
    int leaks = 0, hidden = 0;
    for (int step = 0; step < 3000 && tundra.state().phase == Phase::Battle; ++step) {
        tundra.advance(.1F);
        for (const Enemy &walker : tundra.state().enemies)
            hidden += tundra.tunneled(walker) ? 1 : 0;
        leaks += count(tundra.takeCombatEvents(), CombatEventKind::Leak);
    }
    require(hidden > 0 && leaks == scheduled && tundra.state().phase == Phase::Prep,
            "every enemy enters the tunnel, leaves it and reaches the base");
}

/** The reference autoplayer values trait pads, ignores tunnelled road and clears worthwhile boulders. */
void bots() {
    Catalog content = board();
    const Point pad{7, 6};
    Run open = makeRun(content);
    const CardDefinition &gun = open.catalog().card(CardKind::Gun);
    const float gain = gun.damage / gun.interval;
    const float plain = autoplay::coverage(open, pad, CardKind::Gun, gun, gain);
    content.tunnels = {{0, 5, 9}};
    Run covered = makeRun(content);
    /** The pad reaches six route samples; the four between progress 5 and 9 are under the roof. */
    require(plain > 0 && std::abs(autoplay::coverage(covered, pad, CardKind::Gun, gun, gain) - plain / 3) <
                             plain * .001F,
            "tunnelled road adds no coverage");
    /** High ground adds the samples at progress 1.5, 2.5 and 3.5 to the two that stay in the open. */
    content.padTraits = {{pad, PadTrait::HighGround}};
    Run high = makeRun(content);
    require(std::abs(autoplay::coverage(high, pad, CardKind::Gun, gun, gain) - plain * 5 / 6) < plain * .001F,
            "high ground covers more road");
    require(
        autoplay::coverage(high, pad, CardKind::Guard, high.catalog().card(CardKind::Guard), gain) ==
            autoplay::coverage(covered, pad, CardKind::Guard, covered.catalog().card(CardKind::Guard), gain),
        "pad traits do not change a unit's coverage");
    content.tunnels.clear();
    content.padTraits = {{pad, PadTrait::Crystal}};
    Run crystal = makeRun(content);
    require(std::abs(autoplay::coverage(crystal, pad, CardKind::Gun, gun, gain) - plain * 1.2F) <
                plain * .001F,
            "crystal ground scales coverage by its damage bonus");

    Catalog rocky = defaultCatalog();
    rocky.boulders = {{11, 5}, {1, 8}};
    Run run = makeRun(rocky);
    enterPreparation(run, 1);
    EntityId id = 500;
    for (Point built : run.state().pads)
        scenario(run).towers.push_back({id++, CardKind::Gun, built, 1, 0});
    const RunBalance &balance = run.catalog().balance;
    scenario(run).iron = balance.tool.iron + balance.armor.iron + balance.boulder.iron - 1;
    require(autoplay::clearBoulders(run) == 0 && run.state().boulders.size() == 2,
            "iron reserved for the pickaxe and armor is not spent on a boulder");
    scenario(run).iron = 30;
    require(autoplay::clearBoulders(run) == 1 && run.state().iron == 30 - balance.boulder.iron &&
                among(run.state().pads, {11, 5}) && among(run.state().boulders, {1, 8}),
            "spare iron clears the boulder beside the road and leaves the one that covers nothing");
    Run choosy = makeRun(rocky);
    enterPreparation(choosy, 1);
    scenario(choosy).iron = 30;
    scenario(choosy).tool = 2;
    scenario(choosy).armor = true;
    float freePad = 0;
    for (Point built : choosy.state().pads)
        freePad = std::max(freePad, autoplay::siteValue(choosy, built));
    require((autoplay::siteValue(choosy, {11, 5}) > freePad) == (autoplay::clearBoulders(choosy) == 1),
            "a boulder is cleared only when its pad beats every free pad");
}
} // namespace
/** Exercise every stage gimmick through the public run boundary plus fixture setup. */
int main() {
    boulders();
    traitPads();
    tunnels();
    vents();
    validation();
    shipped();
    bots();
    std::cout << checks << " stage gimmick checks passed\n";
}
