#include "Scenario.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace thorium::game;
namespace {
int checks = 0;
/** Keep release builds accountable for every economy sink invariant. */
void require(bool value, const char *message) {
    ++checks;
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Compare every wallet entry and the counters a rejected purchase must not touch. */
bool same(const RunState &a, const RunState &b) {
    return a.iron == b.iron && a.copper == b.copper && a.crystal == b.crystal && a.gold == b.gold &&
           a.tool == b.tool && a.digs == b.digs && a.hires == b.hires && a.supplied == b.supplied &&
           a.bonusEnergy == b.bonusEnergy && a.towers.size() == b.towers.size() &&
           (a.towers.empty() || a.towers.front().level == b.towers.front().level);
}
/** Run one command that must fail, and prove it changed nothing. */
template <typename Command> void rejected(Run &run, Command command, const char *message) {
    const RunState before = run.state();
    const Result result = command();
    require(!result.ok && !result.message.empty() && same(before, run.state()), message);
}
/** A practice run in its first preparation. */
Run preparing() {
    Run run = makeRun(defaultCatalog());
    enterPreparation(run, 1);
    return run;
}

/** Enter the second preparation and resolve its expansion choice, which otherwise closes purchases. */
void secondPreparation(Run &run) {
    enterPreparation(run, 2);
    if (run.state().expansionPending)
        require(run.expand(1).ok, "the expansion choice resolves");
    if (run.state().rewardOffer)
        require(run.skipOffer(run.state().rewardOffer->id).ok, "the expansion reward can be skipped");
}

/** The T3 drill needs the T2 pickaxe, costs crystal once and adds a dig to this and every later preparation.
 */
void drill() {
    Run run = preparing();
    const RunBalance &balance = run.catalog().balance;
    require(balance.drill.crystal == 5 && balance.drill.iron == 0 && balance.drill.copper == 0 &&
                balance.drill.gold == 0,
            "the drill is priced in crystal");
    scenario(run).crystal = 20;
    require(run.quote(UpgradeKind::Drill).failure == PurchaseFailure::Requirement, "the drill follows T2");
    rejected(run, [&run] { return run.upgrade(UpgradeKind::Drill); }, "a T1 miner cannot buy the drill");
    scenario(run).tool = 2;
    scenario(run).crystal = 4;
    const PurchaseQuote short_ = run.quote(UpgradeKind::Drill);
    require(short_.failure == PurchaseFailure::Shortage && short_.open() &&
                short_.message == Message("rules.progression.cost", {{"cost", "#cost/0/0/5/0"}}),
            "a short wallet is reported with the full price");
    rejected(run, [&run] { return run.upgrade(UpgradeKind::Drill); }, "a short wallet keeps every resource");
    scenario(run).crystal = 7;
    const int digs = run.state().digs;
    require(run.quote(UpgradeKind::Drill).allowed() && run.upgrade(UpgradeKind::Drill).ok,
            "the drill is bought");
    require(run.state().tool == 3 && run.state().crystal == 2 && run.state().digs == digs + balance.drillDigs,
            "the drill charges once and adds its dig to the current preparation");
    require(run.prepDigBudget() == balance.prepDigs + balance.drillDigs,
            "later preparations include the drill");
    require(run.quote(UpgradeKind::Drill).failure == PurchaseFailure::Owned, "the drill is owned once");
    rejected(run, [&run] { return run.upgrade(UpgradeKind::Drill); }, "a second drill is refused");
    rejected(
        run, [&run] { return run.upgrade(UpgradeKind::Tool); }, "the T2 recipe cannot downgrade a drill");
    secondPreparation(run);
    require(run.state().digs == balance.prepDigs + balance.drillDigs,
            "the next preparation starts with the drill dig");
}

/** Hired miners add one dig each at a rising price, up to the per-preparation limit, which then resets. */
void hire() {
    Run run = preparing();
    const RunBalance &balance = run.catalog().balance;
    rejected(run, [&run] { return run.upgrade(UpgradeKind::Hire); }, "an empty wallet cannot hire");
    scenario(run).gold = 30;
    const int digs = run.state().digs;
    int paid = 0;
    for (int hire = 0; hire < balance.hireLimit; ++hire) {
        const int price = balance.hireGold + balance.hireStep * hire;
        require(run.quote(UpgradeKind::Hire).cost.gold == price, "each hire quotes the rising price");
        require(run.upgrade(UpgradeKind::Hire).ok, "an affordable hire succeeds");
        paid += price;
    }
    require(run.state().gold == 30 - paid && run.state().digs == digs + balance.hireLimit &&
                run.state().hires == balance.hireLimit,
            "every hire pays its own price and adds exactly one dig");
    require(run.quote(UpgradeKind::Hire).failure == PurchaseFailure::Exhausted, "the limit closes hiring");
    rejected(run, [&run] { return run.upgrade(UpgradeKind::Hire); }, "hiring past the limit changes nothing");
    secondPreparation(run);
    require(run.state().hires == 0 && run.quote(UpgradeKind::Hire).cost.gold == balance.hireGold &&
                run.state().digs == balance.prepDigs,
            "the next preparation resets the hire count and price, and hired digs do not carry over");
    scenario(run).gold = balance.hireGold - 1;
    rejected(run, [&run] { return run.upgrade(UpgradeKind::Hire); }, "one gold short keeps the wallet");
}

/** A supply crate stores battle energy once per preparation and is refused when it would be wasted. */
void supply() {
    Run run = preparing();
    const RunBalance &balance = run.catalog().balance;
    scenario(run).gold = 2 * balance.supply.gold;
    require(run.upgrade(UpgradeKind::Supply).ok && run.state().supplied &&
                run.state().bonusEnergy == balance.supplyEnergy && run.state().gold == balance.supply.gold,
            "the crate charges gold and stores its energy");
    require(run.nextBattleEnergy() == balance.initialEnergy + balance.supplyEnergy,
            "the next battle energy includes the crate");
    rejected(
        run, [&run] { return run.upgrade(UpgradeKind::Supply); },
        "a second crate in one preparation is refused");
    require(run.startWave().ok && run.state().energy == balance.initialEnergy + balance.supplyEnergy &&
                run.state().bonusEnergy == 0,
            "the battle starts with the crate energy once");
    secondPreparation(run);
    require(!run.state().supplied && run.quote(UpgradeKind::Supply).allowed(),
            "each preparation offers one crate");
    scenario(run).bonusEnergy = balance.energyCap;
    require(run.quote(UpgradeKind::Supply).failure == PurchaseFailure::Exhausted,
            "a crate that would exceed the energy cap is closed");
    rejected(run, [&run] { return run.upgrade(UpgradeKind::Supply); }, "a wasted crate keeps the gold");
    scenario(run).bonusEnergy = 0;
    scenario(run).phase = Phase::Battle;
    require(run.quote(UpgradeKind::Supply).failure == PurchaseFailure::Closed,
            "purchases are closed in battle");
    rejected(run, [&run] { return run.upgrade(UpgradeKind::Supply); }, "a battle purchase changes nothing");
}

/** Awakening raises a level-3 building to 4 and 5 for crystal, and each level adds the card level bonus. */
void awaken() {
    Run run = preparing();
    const RunBalance &balance = run.catalog().balance;
    const Point pad = run.state().pads.front();
    scenario(run).towers.push_back({500, CardKind::Gun, pad, 2});
    scenario(run).crystal = 20;
    require(run.awakenedLevel() == CardMaxLevel + 2, "two awakening steps follow the card maximum");
    require(run.awakenQuote(500).failure == PurchaseFailure::Requirement, "a level-2 building cannot awaken");
    rejected(run, [&run] { return run.awaken(500); }, "awakening below level 3 changes nothing");
    require(run.awakenQuote(77).failure == PurchaseFailure::InvalidTarget, "an unknown building is refused");
    rejected(run, [&run] { return run.awaken(77); }, "awakening a missing building changes nothing");
    scenario(run).towers.front().level = CardMaxLevel;
    scenario(run).crystal = balance.awaken[0].crystal - 1;
    require(run.awakenQuote(500).failure == PurchaseFailure::Shortage, "a short wallet is reported");
    rejected(run, [&run] { return run.awaken(500); }, "a short awakening keeps the wallet and level");
    scenario(run).crystal = balance.awaken[0].crystal + balance.awaken[1].crystal + 1;
    require(run.awakenQuote(500).cost.crystal == balance.awaken[0].crystal && run.awaken(500).ok &&
                run.tower(500)->level == 4,
            "the first awakening reaches level 4");
    require(run.awakenQuote(500).cost.crystal == balance.awaken[1].crystal && run.awaken(500).ok &&
                run.tower(500)->level == 5 && run.state().crystal == 1,
            "the second awakening reaches level 5 and each step charges its own price");
    require(run.awakenQuote(500).failure == PurchaseFailure::Owned, "level 5 is the final awakening");
    rejected(run, [&run] { return run.awaken(500); }, "awakening past the final level changes nothing");

    // Combat reads the awakened level through the shared level bonus.
    scenario(run).phase = Phase::Battle;
    require(run.awakenQuote(500).failure == PurchaseFailure::Closed, "awakening is closed in battle");
    const CardDefinition &gun = run.catalog().card(CardKind::Gun);
    float closest = 0;
    for (float progress = 0; progress < run.routeLength(); progress += .1F)
        if (distance(run.routePoint(progress), pad) < distance(run.routePoint(closest), pad))
            closest = progress;
    scenario(run).enemies.push_back({900, EnemyKind::Normal, run.routePoint(closest), closest, 10000});
    run.takeCombatEvents();
    int dealt = 0;
    for (int step = 0; step < 40 && !dealt; ++step) {
        run.advance(.05F);
        for (const CombatEvent &event : run.takeCombatEvents())
            if (event.type == CombatEventKind::Damage && event.subject == 900)
                dealt = event.amount;
    }
    const int expected =
        std::max(1, static_cast<int>(std::floor(gun.damage * (1.0F + 4 * balance.levelBonus))) -
                        run.catalog().enemy(EnemyKind::Normal).armor);
    require(dealt == expected, "a level-5 building hits with four level bonuses");
    for (int slot : run.state().hand) {
        const Card &card = run.state().deck[slot];
        if (card.kind == CardKind::Gun)
            require(run.previewPlay(card.id, pad).failure == PlayFailure::MaximumLevel,
                    "a matching card cannot upgrade an awakened building");
    }
}

/** Authored sink values are validated before any run is constructed. */
void validation() {
    Catalog free = defaultCatalog();
    free.balance.awaken[1] = {};
    require(!validateCatalog(free).ok(), "a free awakening is rejected");
    Catalog wasted = defaultCatalog();
    wasted.balance.supplyEnergy = 0;
    require(!validateCatalog(wasted).ok(), "an empty supply crate is rejected");
    Catalog gift = defaultCatalog();
    gift.balance.hireGold = 0;
    require(!validateCatalog(gift).ok(), "a free hire is rejected");
    Catalog negative = defaultCatalog();
    negative.balance.drill.crystal = -1;
    require(!validateCatalog(negative).ok(), "a negative drill price is rejected");
    for (int stage = 1; stage <= TotalStages; ++stage)
        require(validateCatalog(stageCatalog(stage)).ok(), "shipped stages pass sink validation");
}
} // namespace

/** Native proof of the preparation sinks: price, effect, limits and rejection without partial mutation. */
int main() {
    drill();
    hire();
    supply();
    awaken();
    validation();
    std::cout << "economy sinks: " << checks << " checks\n";
}
