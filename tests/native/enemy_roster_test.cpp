#include "Scenario.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>

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
/** Create an enemy fixture with authored health, shield and ability timer on the primary route. */
Enemy fixture(const Run &run, EntityId id, EnemyKind kind, float progress, float ability = 99) {
    Enemy enemy{id, kind, run.routePoint(progress), progress, role(run, kind).hp, 0, 0, 0, 0};
    enemy.shield = role(run, kind).shield;
    enemy.ability = ability;
    return enemy;
}
/** Start a battle whose scheduled spawns are still seconds away, so fixtures act alone. */
Run battle() {
    Run run = makeRun(defaultCatalog());
    require(run.startWave().ok, "fixture battle starts");
    return run;
}
/** Count committed facts of one kind. */
int count(const std::vector<CombatEvent> &events, CombatEventKind type) {
    return static_cast<int>(std::count_if(events.begin(), events.end(),
                                          [type](const CombatEvent &event) { return event.type == type; }));
}
/** Defeating a splitter creates minions at its route position in the same tick. */
void splitting() {
    Run run = battle();
    scenario(run).enemies.push_back(fixture(run, 700, EnemyKind::Splitter, 2));
    scenario(run).enemies.back().hp = 1;
    scenario(run).towers.push_back({500, CardKind::Gun, {6, 4}, 1, 0});
    run.advance(.01F);
    const std::vector<CombatEvent> events = run.takeCombatEvents();
    const int split = role(run, EnemyKind::Splitter).splitCount;
    require(split == 3 && run.state().enemies.size() == static_cast<std::size_t>(split),
            "a defeated splitter is replaced by its authored minions");
    std::set<EntityId> ids;
    for (const Enemy &minion : run.state().enemies) {
        ids.insert(minion.id);
        require(minion.kind == EnemyKind::Swarm && minion.hp == role(run, EnemyKind::Swarm).hp &&
                    std::abs(minion.progress - 2) <= .31F && minion.route == 0,
                "minions are full-health swarm members beside the parent on its route");
        require(distance(minion.position, run.routePoint(minion.progress)) < .01F,
                "minions stand on their route position");
    }
    require(ids.size() == 3 && !ids.count(700), "every minion receives a new identity");
    require(count(events, CombatEventKind::Split) == 1 && count(events, CombatEventKind::Defeated) == 1 &&
                run.state().defeated == 1,
            "one defeat and one split fact are committed");
    require(run.state().phase == Phase::Battle, "the wave continues while minions live");
    Run spell = battle();
    scenario(spell).energy = 10;
    scenario(spell).enemies.push_back(fixture(spell, 701, EnemyKind::Splitter, 2));
    scenario(spell).enemies.back().hp = 1;
    CardId blast = 0;
    for (int slot : spell.state().hand)
        if (spell.state().deck[slot].kind == CardKind::Blast)
            blast = spell.state().deck[slot].id;
    for (int attempt = 0; !blast && attempt < 8; ++attempt) {
        spell.exchange(spell.state().deck[spell.state().hand[0]].id);
        scenario(spell).energy = 10;
        for (int slot : spell.state().hand)
            if (spell.state().deck[slot].kind == CardKind::Blast)
                blast = spell.state().deck[slot].id;
    }
    require(blast && spell.play(blast, spell.routePoint(2)).ok, "blast spell reaches the splitter");
    require(spell.state().enemies.size() == 3 &&
                std::all_of(spell.state().enemies.begin(), spell.state().enemies.end(),
                            [](const Enemy &enemy) { return enemy.kind == EnemyKind::Swarm; }),
            "spell defeats split exactly like tower defeats");
}
/** A healer restores nearby wounded allies up to their authored maximum on its interval. */
void healing() {
    Run run = battle();
    const EnemyDefinition &healer = role(run, EnemyKind::Healer);
    const int maximum = role(run, EnemyKind::Normal).hp;
    scenario(run).enemies.push_back(fixture(run, 710, EnemyKind::Healer, 3, .005F));
    scenario(run).enemies.push_back(fixture(run, 711, EnemyKind::Normal, 3.5F));
    scenario(run).enemies.back().hp = 5;
    scenario(run).enemies.push_back(fixture(run, 712, EnemyKind::Normal, 2.8F));
    scenario(run).enemies.back().hp = maximum - 3;
    scenario(run).enemies.push_back(fixture(run, 713, EnemyKind::Normal, 9));
    scenario(run).enemies.back().hp = 5;
    run.advance(.01F);
    const std::vector<CombatEvent> events = run.takeCombatEvents();
    require(run.state().enemies[1].hp == 5 + healer.abilityAmount, "a wounded ally in range is healed");
    require(run.state().enemies[2].hp == maximum, "healing never exceeds the authored maximum");
    require(run.state().enemies[3].hp == 5, "allies outside the radius are not healed");
    require(run.state().enemies[0].hp == healer.hp, "the healer does not heal itself");
    require(count(events, CombatEventKind::Heal) == 3, "two heal amounts and one pulse fact are committed");
    require(std::abs(run.state().enemies[0].ability - healer.abilityInterval) < .01F,
            "the next heal waits a full interval");
    for (int step = 0; step < 10; ++step)
        run.advance(.1F);
    require(run.state().enemies[1].hp == 5 + healer.abilityAmount, "no heal is repeated before its interval");
}
/** A shield caps and absorbs each hit until it breaks, then health takes full damage. */
void shielding() {
    Run run = battle();
    const EnemyDefinition &shielded = role(run, EnemyKind::Shielded);
    scenario(run).enemies.push_back(fixture(run, 720, EnemyKind::Shielded, 2));
    scenario(run).towers.push_back({500, CardKind::Cannon, {6, 4}, 1, 0});
    run.advance(.01F);
    std::vector<CombatEvent> events = run.takeCombatEvents();
    const Enemy &target = run.state().enemies.front();
    require(target.hp == shielded.hp && target.shield == shielded.shield - shielded.shieldCap,
            "a heavy hit is capped and absorbed by the shield");
    require(std::any_of(events.begin(), events.end(),
                        [&shielded](const CombatEvent &event) {
                            return event.type == CombatEventKind::Damage &&
                                   event.amount == shielded.shieldCap;
                        }),
            "the displayed damage is the capped amount");
    scenario(run).enemies.front().shield = 3;
    scenario(run).towers.front().cooldown = 0;
    run.advance(.01F);
    events = run.takeCombatEvents();
    require(run.state().enemies.front().shield == 0 &&
                run.state().enemies.front().hp == shielded.hp - (shielded.shieldCap - 3),
            "capped damage beyond the remaining shield reaches health");
    require(count(events, CombatEventKind::ShieldBreak) == 1, "breaking the shield is one committed fact");
    const int before = run.state().enemies.front().hp;
    scenario(run).towers.front().cooldown = 0;
    run.advance(.01F);
    require(run.state().enemies.front().hp ==
                before - std::max(1, run.catalog().card(CardKind::Cannon).damage - shielded.armor),
            "without a shield hits are no longer capped");
}
/** A burrower drops its blocker, is untargetable and moves faster until it surfaces. */
void burrowing() {
    Run run = battle();
    const EnemyDefinition &burrower = role(run, EnemyKind::Burrower);
    scenario(run).units.push_back({600, CardKind::Guard, run.routePoint(2), 120, 120});
    scenario(run).enemies.push_back(fixture(run, 730, EnemyKind::Burrower, 2));
    run.advance(.01F);
    require(run.state().enemies.front().blocker == 600, "a surfaced burrower can be blocked");
    scenario(run).enemies.front().ability = .005F;
    scenario(run).towers.push_back({500, CardKind::Gun, {6, 4}, 1, 0});
    const float progress = run.state().enemies.front().progress;
    const int health = run.state().enemies.front().hp;
    run.advance(.01F);
    const std::vector<CombatEvent> events = run.takeCombatEvents();
    const Enemy &digger = run.state().enemies.front();
    require(run.burrowed(digger) && digger.blocker == 0, "burrowing releases its blocker");
    require(count(events, CombatEventKind::Burrow) == 1, "burrowing is one committed fact");
    require(std::abs(digger.progress - progress - burrower.speed * burrower.abilitySpeed * .01F) < .0001F,
            "an underground burrower moves at its ability speed");
    require(digger.hp == health, "an underground burrower is not targeted");
    for (int step = 0; step < 5; ++step)
        run.advance(.1F);
    require(run.state().enemies.front().blocker == 0 && run.state().enemies.front().hp == health,
            "guards cannot block and towers cannot hit underground");
    for (int step = 0; step < 15; ++step)
        run.advance(.1F);
    require(!run.burrowed(run.state().enemies.front()) && run.state().enemies.front().hp < health,
            "a surfaced burrower is targetable again");
}
/** Create an elite fixture whose health is the modifier's maximum. */
Enemy eliteFixture(const Run &run, EntityId id, EnemyKind kind, EliteAffix affix, float progress) {
    Enemy enemy = fixture(run, id, kind, progress);
    enemy.affix = affix;
    enemy.hp = run.catalog().health(enemy);
    return enemy;
}
/** A sapper stops beside a tower, plants for its windup, then disables that tower until the time runs out. */
void sabotage() {
    Run run = battle();
    const EnemyDefinition &sapper = role(run, EnemyKind::Sapper);
    scenario(run).towers.push_back({500, CardKind::Gun, {6, 4}, 1, 0});
    scenario(run).enemies.push_back(fixture(run, 750, EnemyKind::Sapper, 2, .005F));
    scenario(run).enemies.back().hp = 100000;
    scenario(run).enemies.back().charges = sapper.abilityAmount;
    run.advance(.01F);
    std::vector<CombatEvent> events = run.takeCombatEvents();
    const float planted = run.state().enemies.front().progress;
    require(run.state().enemies.front().sabotage == 500 && count(events, CombatEventKind::Sabotage) == 1,
            "a sapper in reach of a tower starts one telegraphed plant");
    require(std::abs(run.state().enemies.front().ability - sapper.abilityWindup) < .02F,
            "the plant lasts the authored windup");
    run.advance(.1F);
    require(std::abs(run.state().enemies.front().progress - planted) < .0001F,
            "a planting sapper stands still");
    require(run.tower(500)->disabled == 0, "the tower works until the charge goes off");
    for (int step = 0; step < static_cast<int>(sapper.abilityWindup * 10) + 1; ++step)
        run.advance(.1F);
    events = run.takeCombatEvents();
    require(count(events, CombatEventKind::Disable) == 1 &&
                run.tower(500)->disabled > sapper.abilityDuration - .5F,
            "the charge disables the tower for the authored duration");
    require(run.state().enemies.front().sabotage == 0 && run.state().enemies.front().charges == 0,
            "the sapper spends its charge and walks on");
    run.advance(.1F);
    require(run.state().enemies.front().progress > planted, "a spent sapper keeps moving");
    scenario(run).enemies.push_back(fixture(run, 751, EnemyKind::Normal, 2.2F));
    const int health = run.state().enemies.back().hp;
    for (int step = 0; step < 5; ++step)
        run.advance(.1F);
    require(run.state().enemies.back().hp == health && run.tower(500)->disabled > 0,
            "a disabled tower holds fire while its timer counts down");
    scenario(run).enemies.clear();
    for (int step = 0; step < 400 && run.state().phase == Phase::Battle; ++step) {
        scenario(run).enemies.clear();
        run.advance(.1F);
    }
    require(run.state().phase != Phase::Battle && run.tower(500)->disabled == 0,
            "the end of a wave repairs every sabotaged tower");

    Run blocked = battle();
    scenario(blocked).towers.push_back({500, CardKind::Gun, {6, 4}, 1, 0});
    scenario(blocked).enemies.push_back(fixture(blocked, 752, EnemyKind::Sapper, 2, .005F));
    scenario(blocked).enemies.back().hp = 100000;
    scenario(blocked).enemies.back().charges = 1;
    blocked.advance(.01F);
    require(blocked.state().enemies.front().sabotage == 500, "the second sapper starts planting");
    scenario(blocked).units.push_back(
        {600, CardKind::Guard, blocked.state().enemies.front().position, 120, 120});
    for (int step = 0; step < 40; ++step)
        blocked.advance(.1F);
    require(blocked.tower(500)->disabled == 0 && blocked.state().enemies.front().charges == 1,
            "a guard that blocks the sapper cancels the plant");

    Run killed = battle();
    scenario(killed).towers.push_back({500, CardKind::Sniper, {6, 4}, 1, 1});
    scenario(killed).enemies.push_back(fixture(killed, 753, EnemyKind::Sapper, 2, .005F));
    scenario(killed).enemies.back().charges = 1;
    killed.advance(.01F);
    require(killed.state().enemies.front().sabotage == 500, "the third sapper starts planting");
    for (int step = 0; step < 40; ++step)
        killed.advance(.1F);
    require(killed.tower(500)->disabled == 0 &&
                std::none_of(killed.state().enemies.begin(), killed.state().enemies.end(),
                             [](const Enemy &enemy) { return enemy.id == 753; }),
            "killing a sapper during the plant keeps the tower working");
}
/** Elite modifiers change armor, neighbour speed, regeneration, enrage speed and neighbour damage taken. */
void elites() {
    const Catalog &catalog = defaultCatalog();
    {
        Run run = battle();
        scenario(run).towers.push_back({500, CardKind::Gun, {6, 4}, 1, 0});
        scenario(run).enemies.push_back(eliteFixture(run, 760, EnemyKind::Normal, EliteAffix::Steel, 2));
        const int health = run.state().enemies.front().hp;
        require(health == static_cast<int>(std::lround(role(run, EnemyKind::Normal).hp *
                                                       catalog.elite(EliteAffix::Steel).health)),
                "an elite carries the modifier's health multiplier");
        run.advance(.01F);
        require(health - run.state().enemies.front().hp ==
                    catalog.card(CardKind::Gun).damage - catalog.elite(EliteAffix::Steel).armor,
                "steel armor reduces each physical hit");
    }
    {
        Run run = battle();
        scenario(run).enemies.push_back(eliteFixture(run, 761, EnemyKind::Normal, EliteAffix::Swift, 5));
        scenario(run).enemies.push_back(fixture(run, 762, EnemyKind::Normal, 5.5F));
        scenario(run).enemies.push_back(fixture(run, 763, EnemyKind::Normal, 12));
        run.advance(.1F);
        const float speed = role(run, EnemyKind::Normal).speed * .1F;
        require(std::abs(run.state().enemies[1].progress - 5.5F -
                         speed * catalog.elite(EliteAffix::Swift).auraSpeed) < .001F,
                "a swift aura speeds nearby enemies");
        require(std::abs(run.state().enemies[0].progress - 5 - speed) < .001F &&
                    std::abs(run.state().enemies[2].progress - 12 - speed) < .001F,
                "the swift elite itself and distant enemies keep their speed");
    }
    {
        Run run = battle();
        scenario(run).enemies.push_back(eliteFixture(run, 764, EnemyKind::Normal, EliteAffix::Regen, 2));
        const int maximum = run.state().enemies.front().hp;
        scenario(run).enemies.front().hp = 10;
        scenario(run).enemies.front().pulse = .005F;
        run.advance(.01F);
        const int healed =
            std::max(1, static_cast<int>(std::lround(maximum * catalog.elite(EliteAffix::Regen).regen)));
        require(run.state().enemies.front().hp == 10 + healed &&
                    count(run.takeCombatEvents(), CombatEventKind::Heal) == 1,
                "a regenerating elite restores its share of health once per second");
        for (int step = 0; step < 9; ++step)
            run.advance(.1F);
        require(run.state().enemies.front().hp == 10 + healed, "no second pulse before a full second");
        scenario(run).enemies.front().hp = maximum;
        scenario(run).enemies.front().pulse = .005F;
        run.advance(.01F);
        require(run.state().enemies.front().hp == maximum, "regeneration never exceeds maximum health");
    }
    {
        Run run = battle();
        scenario(run).enemies.push_back(eliteFixture(run, 765, EnemyKind::Normal, EliteAffix::Enrage, 2));
        run.advance(.01F);
        require(!run.state().enemies.front().enraged, "a healthy enraging elite stays calm");
        scenario(run).enemies.front().hp =
            static_cast<int>(run.catalog().health(run.state().enemies.front()) *
                             catalog.elite(EliteAffix::Enrage).enrageBelow);
        const float before = run.state().enemies.front().progress;
        run.advance(.1F);
        require(run.state().enemies.front().enraged &&
                    count(run.takeCombatEvents(), CombatEventKind::Enrage) == 1,
                "falling under the threshold enrages once");
        require(std::abs(run.state().enemies.front().progress - before -
                         role(run, EnemyKind::Normal).speed * catalog.elite(EliteAffix::Enrage).enrageSpeed *
                             .1F) < .001F,
                "an enraged elite moves at its enrage speed");
    }
    {
        Run run = battle();
        scenario(run).towers.push_back({500, CardKind::Gun, {6, 4}, 1, 0});
        scenario(run).enemies.push_back(eliteFixture(run, 766, EnemyKind::Armored, EliteAffix::Leader, 1.6F));
        scenario(run).enemies.push_back(fixture(run, 767, EnemyKind::Normal, 2.4F));
        const int health = run.state().enemies.back().hp;
        run.advance(.01F);
        const int guarded = static_cast<int>(std::lround(catalog.card(CardKind::Gun).damage *
                                                         (1 - catalog.elite(EliteAffix::Leader).auraGuard)));
        require(health - run.state().enemies.back().hp == guarded,
                "an enemy beside the pack leader takes reduced damage");
        scenario(run).enemies.back().progress = run.routeLength() - .001F;
        scenario(run).enemies.front().progress = run.routeLength() - .001F;
        const int base = run.state().baseHp;
        run.advance(.01F);
        require(base - run.state().baseHp == role(run, EnemyKind::Armored).leak +
                                                 catalog.elite(EliteAffix::Leader).leak +
                                                 role(run, EnemyKind::Normal).leak,
                "a leaking leader adds its leak bonus");
    }
}
/** The second half of every stage escalates: elites, sappers, one pack leader and rising vigor. */
void latePressure() {
    for (int stage = 1; stage <= TotalStages; ++stage) {
        const Catalog &catalog = stageCatalog(stage);
        const int waves = static_cast<int>(catalog.waves.size());
        const int middle = waves / 2;
        int leaders = 0;
        float vigor = 1;
        std::array<int, EliteAffixCount> first{};
        for (int wave = 1; wave <= waves; ++wave) {
            int elites = 0, sappers = 0;
            for (const Spawn &spawn : catalog.schedule(wave)) {
                elites += spawn.affix != EliteAffix::None ? 1 : 0;
                sappers += spawn.kind == EnemyKind::Sapper ? 1 : 0;
                leaders += spawn.affix == EliteAffix::Leader ? 1 : 0;
                require(spawn.affix != EliteAffix::Leader || wave == middle + 1,
                        "the pack leader arrives right after the middle of a stage");
                if (spawn.affix != EliteAffix::None && !first[static_cast<std::size_t>(spawn.affix)])
                    first[static_cast<std::size_t>(spawn.affix)] = wave;
            }
            require(wave > middle || (elites == 0 && sappers == 0),
                    "the first half of a stage fields no elites or sappers");
            require(catalog.waveVigor(wave) >= vigor && (wave > middle || catalog.waveVigor(wave) == 1),
                    "vigor starts after the middle and never falls");
            vigor = catalog.waveVigor(wave);
        }
        require(leaders == 1, "every stage has one pack leader");
        require(vigor > 1, "the last wave is tougher than the first half");
        require(first == catalog.eliteDebuts,
                "elite debuts match the first scheduled elite of each modifier");
        Run run = makeRun(catalog);
        enterPreparation(run, middle);
        int forecast = 0;
        for (const LaneForecast &lane : run.laneForecast())
            forecast += lane.elites[static_cast<std::size_t>(EliteAffix::Leader)];
        require(forecast == 1, "the leader is forecast before its wave");
        require(run.startWave().ok, "leader wave starts");
        const Enemy *leader = nullptr;
        for (int step = 0; step < 400 && !leader; ++step) {
            run.advance(.1F);
            for (const Enemy &enemy : run.state().enemies)
                if (enemy.affix == EliteAffix::Leader)
                    leader = &enemy;
        }
        require(leader &&
                    leader->maxHp == std::max(1, static_cast<int>(std::lround(
                                                     std::lround(catalog.enemy(leader->kind).hp *
                                                                 catalog.elite(EliteAffix::Leader).health) *
                                                     catalog.waveVigor(middle + 1)))),
                "a spawned leader carries the modifier and wave health multipliers");
    }
    Catalog invalid = defaultCatalog();
    invalid.waves[0][0].affix = static_cast<EliteAffix>(EliteAffixCount);
    require(!Run::create(invalid).run, "unknown elite modifiers are rejected");
    invalid = defaultCatalog();
    invalid.vigor = {0};
    require(!Run::create(invalid).run, "non-positive wave vigor is rejected");
    invalid = defaultCatalog();
    invalid.enemies[static_cast<std::size_t>(EnemyKind::Sapper)].abilityWindup = 0;
    require(!Run::create(invalid).run, "a sapper without a plant time is rejected");
}
/** Every stage ends with exactly one zone boss whose stomp stuns nearby towers and summons minions. */
void bosses() {
    for (int stage = 1; stage <= TotalStages; ++stage) {
        const Catalog &catalog = stageCatalog(stage);
        for (int wave = 1; wave <= static_cast<int>(catalog.waves.size()); ++wave) {
            const std::vector<Spawn> spawns = catalog.schedule(wave);
            const long bosses = std::count_if(spawns.begin(), spawns.end(), [](const Spawn &spawn) {
                return spawn.kind == EnemyKind::Boss;
            });
            require(bosses == (wave == static_cast<int>(catalog.waves.size()) ? 1 : 0),
                    "only the final wave of each stage fields one boss");
        }
        Run run = makeRun(catalog);
        enterPreparation(run, static_cast<int>(catalog.waves.size()) - 1);
        require(run.laneForecast().front().kinds[static_cast<std::size_t>(EnemyKind::Boss)] == 1,
                "the boss is forecast before the final wave starts");
        require(run.nextArrival(EnemyKind::Boss) < 0, "boss arrival time is only reported during battle");
        require(run.startWave().ok && run.nextArrival(EnemyKind::Boss) > 0,
                "the boss arrives after a warning");
        for (int step = 0; step < 400 && run.nextArrival(EnemyKind::Boss) >= 0; ++step)
            run.advance(.1F);
        require(std::any_of(run.state().enemies.begin(), run.state().enemies.end(),
                            [](const Enemy &enemy) { return enemy.kind == EnemyKind::Boss; }),
                "the boss appears on the final wave");
        require(stage == 1 ||
                    catalog.enemy(EnemyKind::Boss).hp > stageCatalog(stage - 1).enemy(EnemyKind::Boss).hp,
                "later zone bosses are tougher");
    }
    Run run = battle();
    const EnemyDefinition &boss = role(run, EnemyKind::Boss);
    scenario(run).towers.push_back({500, CardKind::Gun, run.routePoint(3), 1, 0});
    scenario(run).towers.push_back({501, CardKind::Gun, run.routePoint(12), 1, 0});
    scenario(run).enemies.push_back(fixture(run, 740, EnemyKind::Boss, 3, .005F));
    run.advance(.01F);
    const std::vector<CombatEvent> events = run.takeCombatEvents();
    require(run.tower(500)->cooldown >= boss.abilityDuration - .02F,
            "a stomp stuns towers within its radius");
    require(run.tower(501)->cooldown < boss.abilityDuration - .02F, "distant towers keep firing");
    require(static_cast<int>(run.state().enemies.size()) == 1 + boss.abilityAmount &&
                run.state().enemies.back().kind == boss.minionKind &&
                run.state().enemies.back().progress < run.state().enemies.front().progress,
            "a stomp summons minions behind the boss");
    require(count(events, CombatEventKind::Stomp) == 1, "the stomp is one committed fact");
    scenario(run).enemies.front().progress = run.routeLength() - .001F;
    const int hp = run.state().baseHp;
    run.advance(.01F);
    require(run.state().baseHp == std::max(0, hp - boss.leak), "a leaking boss deals its heavy leak damage");
}
/** Each lane forecast reports exact per-kind counts, and the battle spawns exactly that schedule. */
void forecasts() {
    for (int stage = 1; stage <= TotalStages; ++stage) {
        Catalog catalog = stageCatalog(stage);
        catalog.balance.baseHp = 1000000;
        for (EnemyDefinition &enemy : catalog.enemies) {
            enemy.splitCount = 0;
            enemy.ability = EnemyAbility::None;
        }
        for (int wave = 1; wave <= static_cast<int>(catalog.waves.size()); ++wave) {
            Run run = makeRun(catalog);
            if (wave > 1)
                enterPreparation(run, wave - 1);
            if (run.state().expansionPending) {
                run.expand(1);
                run.skipOffer(run.state().rewardOffer->id);
            }
            std::map<std::pair<std::uint32_t, int>, int> forecast;
            for (const LaneForecast &lane : run.laneForecast()) {
                int total = 0;
                for (std::size_t kind = 0; kind < EnemyKindCount; ++kind) {
                    forecast[{lane.route, static_cast<int>(kind)}] = lane.kinds[kind];
                    total += lane.kinds[kind];
                }
                require(total == lane.count, "per-kind forecast sums to the lane count");
            }
            require(run.startWave().ok, "forecast fixture wave starts");
            std::map<std::pair<std::uint32_t, int>, int> spawned;
            std::set<EntityId> seen;
            for (int step = 0; step < 2000 && run.waveSpawned() < run.waveSpawnCount(); ++step) {
                run.advance(.1F);
                for (const Enemy &enemy : run.state().enemies)
                    if (seen.insert(enemy.id).second)
                        ++spawned[{enemy.route, static_cast<int>(enemy.kind)}];
            }
            for (const std::pair<const std::pair<std::uint32_t, int>, int> &entry : forecast)
                require(spawned[entry.first] == entry.second,
                        "every forecast kind and lane count is spawned");
        }
    }
}
/** The campaign introduces at most one previously unseen role per wave. */
void introductions() {
    std::set<EnemyKind> known{EnemyKind::Normal, EnemyKind::Runner};
    for (int stage = 1; stage <= TotalStages; ++stage) {
        const Catalog &catalog = stageCatalog(stage);
        for (int wave = 1; wave <= static_cast<int>(catalog.waves.size()); ++wave) {
            int fresh = 0;
            for (const Spawn &spawn : catalog.schedule(wave))
                if (known.insert(spawn.kind).second)
                    ++fresh;
            require(fresh <= 1, "a wave introduces at most one new enemy role");
        }
    }
    require(known.size() == EnemyKindCount, "the campaign fields every enemy role");
    for (std::size_t kind = 0; kind < EnemyKindCount; ++kind)
        require(!defaultCatalog().enemies[kind].name.empty(), "every role has a display name");
    Catalog recursive = defaultCatalog();
    recursive.enemies[static_cast<std::size_t>(EnemyKind::Splitter)].minionKind = EnemyKind::Splitter;
    require(!Run::create(recursive).run, "minions that split again are rejected");
}
} // namespace
/** Exercise every new enemy mechanic through the public run boundary plus fixture setup. */
int main() {
    splitting();
    healing();
    shielding();
    burrowing();
    sabotage();
    elites();
    latePressure();
    bosses();
    forecasts();
    introductions();
    std::cout << checks << " enemy roster checks passed\n";
}
