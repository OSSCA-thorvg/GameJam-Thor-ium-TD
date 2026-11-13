#include "Scenario.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>

using namespace thorium::game;
namespace {
int checks = 0;
/** Keep release builds accountable for every tower-card invariant. */
void require(bool value, const char *message) {
    ++checks;
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Start a fixture battle on the compact default field whose only spawn never arrives during a test. */
Run battle(CardKind first = CardKind::Gun) {
    Catalog content = defaultCatalog();
    content.starterDeck[0] = first;
    content.waves = {{{EnemyKind::Normal, 1, 1000, 0}}, {}};
    Run run = makeRun(content);
    require(run.startWave().ok, "fixture battle starts");
    return run;
}
/** Add one enemy at a progress along the primary route, keeping its position consistent with movement. */
void place(Run &run, EntityId id, EnemyKind kind, float progress) {
    scenario(run).enemies.push_back(
        {id, kind, run.routePoint(progress), progress, run.catalog().enemy(kind).hp});
}
/** Find one live enemy by fixture identity. */
const Enemy *enemy(const Run &run, EntityId id) {
    for (const Enemy &item : run.state().enemies)
        if (item.id == id)
            return &item;
    return nullptr;
}
/** Flame hits every enemy inside its cone, ignores enemies behind it and is weak against armor. */
void flame() {
    Run run = battle();
    scenario(run).towers.push_back({100, CardKind::Flame, {6, 4}});
    place(run, 201, EnemyKind::Normal, 5);
    place(run, 202, EnemyKind::Normal, 4.6F);
    place(run, 203, EnemyKind::Normal, 1);
    place(run, 204, EnemyKind::Armored, 4.2F);
    run.advance(.01F);
    const int damage = run.catalog().card(CardKind::Flame).damage;
    require(enemy(run, 201)->hp == 36 - damage && enemy(run, 202)->hp == 36 - damage,
            "every enemy inside the flame cone is hit at once");
    require(enemy(run, 203)->hp == 36, "enemies behind the flame tower are outside the cone");
    require(enemy(run, 204)->hp == 100 - std::max(1, damage - run.catalog().enemy(EnemyKind::Armored).armor),
            "armor reduces each flame hit");
}
/** Sniper prefers the highest-HP enemy over the one closest to the base. */
void sniper() {
    Run run = battle();
    scenario(run).towers.push_back({100, CardKind::Sniper, {6, 4}});
    place(run, 201, EnemyKind::Normal, 10);
    place(run, 202, EnemyKind::Armored, 2);
    run.advance(.01F);
    require(enemy(run, 201)->hp == 36 && enemy(run, 202)->hp < 100,
            "sniper targets the strongest enemy even when another is closer to the base");
    require(run.catalog().card(CardKind::Sniper).range > 2 * run.catalog().card(CardKind::Gun).range,
            "sniper covers more than twice the gun range");
}
/** Generators never attack and grant the strongest nearby bonus to other buildings only. */
void generator() {
    Run plain = battle();
    scenario(plain).towers.push_back({100, CardKind::Gun, {6, 4}});
    place(plain, 201, EnemyKind::Normal, 5);
    plain.advance(.01F);
    const int base = 36 - enemy(plain, 201)->hp;
    Run boosted = battle(CardKind::Generator);
    const CardId card = boosted.state().deck[0].id;
    require(boosted.previewPlay(card, {7, 6}).action == PlayAction::Install && boosted.play(card, {7, 6}).ok,
            "support buildings use ordinary pad placement rules");
    scenario(boosted).towers.push_back({100, CardKind::Gun, {6, 4}});
    scenario(boosted).towers.push_back({101, CardKind::Generator, {6, 6}});
    place(boosted, 201, EnemyKind::Normal, 5);
    boosted.takeCombatEvents();
    boosted.advance(.01F);
    const float aura = boosted.catalog().card(CardKind::Generator).aura;
    require(36 - enemy(boosted, 201)->hp == static_cast<int>(base * (1 + aura)),
            "two generators in range grant one non-stacking bonus");
    const std::vector<CombatEvent> events = boosted.takeCombatEvents();
    require(std::none_of(events.begin(), events.end(),
                         [](const CombatEvent &event) -> bool {
                             return event.type == CombatEventKind::Attack &&
                                    event.card == CardKind::Generator;
                         }),
            "generators never fire");
}
/** Tar creates a lasting area that slows enemies entering it until it expires. */
void tar() {
    Run run = battle(CardKind::Tar);
    const CardId card = run.state().deck[0].id;
    require(run.previewPlay(card, {6, 3}).allowed(), "tar uses the ordinary spell target rule");
    require(run.play(card, {6, 3}).ok && run.state().fields.size() == 1, "tar leaves one ground field");
    place(run, 201, EnemyKind::Normal, 2);
    place(run, 202, EnemyKind::Normal, 12);
    run.advance(.1F);
    const float speed = run.catalog().enemy(EnemyKind::Normal).speed;
    require(enemy(run, 201)->progress < 2 + speed * .1F - .001F, "enemies inside tar move slower");
    require(enemy(run, 202)->progress > 12 + speed * .1F - .001F, "enemies outside tar keep full speed");
    const int steps = static_cast<int>(run.catalog().card(CardKind::Tar).slowDuration * 10) + 10;
    for (int step = 0; step < steps; ++step)
        run.advance(.1F);
    require(run.state().fields.empty(), "tar expires after its authored duration");
}
/** Every new card appears in at least one shipped starter deck or expansion reward. */
void availability() {
    for (CardKind kind : {CardKind::Flame, CardKind::Sniper, CardKind::Generator, CardKind::Tar}) {
        bool offered = false;
        for (int stage = 1; stage <= TotalStages; ++stage) {
            const Catalog &catalog = stageCatalog(stage);
            offered |= std::find(catalog.starterDeck.begin(), catalog.starterDeck.end(), kind) !=
                       catalog.starterDeck.end();
            for (const ExpansionDefinition &expansion : catalog.expansions)
                offered |= expansion.reward == kind;
        }
        require(offered, "new cards reach a campaign run through a starter deck or expansion reward");
    }
}
} // namespace
/** Run headless rule checks for the flame, sniper, generator and tar cards. */
int main() {
    flame();
    sniper();
    generator();
    tar();
    availability();
    std::cout << checks << " tower card checks passed\n";
}
