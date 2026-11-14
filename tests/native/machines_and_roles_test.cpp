#include "Scenario.h"
#include "game/autoplay/Autoplayer.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
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
Enemy fixture(const Run &run, EntityId id, EnemyKind kind, float progress) {
    Enemy enemy{id, kind, run.routePoint(progress), progress, role(run, kind).hp, 0, 0, 0, 0};
    enemy.shield = role(run, kind).shield;
    enemy.ability = role(run, kind).ability == EnemyAbility::Leap ? role(run, kind).abilityWindup : 99;
    enemy.charges = role(run, kind).ability == EnemyAbility::Leap ? role(run, kind).abilityAmount : 0;
    return enemy;
}
/** Find a live enemy fixture by identity, or nullptr once it was removed. */
const Enemy *find(const Run &run, EntityId id) {
    for (const Enemy &candidate : run.state().enemies)
        if (candidate.id == id)
            return &candidate;
    return nullptr;
}
/** Find a live enemy fixture that must still exist. */
const Enemy &enemy(const Run &run, EntityId id) {
    const Enemy *found = find(run, id);
    require(found != nullptr, "the enemy fixture is still alive");
    return *found;
}
/** Count committed facts of one kind. */
int count(const std::vector<CombatEvent> &events, CombatEventKind type) {
    return static_cast<int>(std::count_if(events.begin(), events.end(),
                                          [type](const CombatEvent &event) { return event.type == type; }));
}
/** The practice board with every role standing still and no unit cell inside route progress 5 to 9. */
Catalog board() {
    Catalog content = defaultCatalog();
    content.unitCells = {{6, 3}, {8, 4}, {10, 7}, {12, 7}};
    for (EnemyDefinition &definition : content.enemies)
        definition.speed = 0;
    return content;
}
/** Start a battle whose scheduled spawns are still seconds away, so fixtures act alone. */
Run battle(const Catalog &content) {
    Run run = makeRun(content);
    require(run.startWave().ok, "fixture battle starts");
    return run;
}

/** Bats fly over blockers, ground-only attacks, tar, vents and tunnels. */
void flyers() {
    Run run = battle(board());
    const EnemyDefinition &bat = role(run, EnemyKind::Flyer);
    require(bat.flying && bat.hp == 24 && bat.name == "동굴박쥐", "the cave bat is a flying role");
    require(run.catalog().card(CardKind::Cannon).ground && run.catalog().card(CardKind::Guard).ground &&
                run.catalog().card(CardKind::Demolition).ground &&
                !run.catalog().card(CardKind::Gun).ground && !run.catalog().card(CardKind::Shooter).ground &&
                !run.catalog().card(CardKind::Sniper).ground,
            "cannon, guard and demolition attack the ground only");
    scenario(run).enemies.push_back(fixture(run, 700, EnemyKind::Flyer, 10));
    scenario(run).enemies.push_back(fixture(run, 701, EnemyKind::Normal, 10));
    scenario(run).units.push_back({600, CardKind::Guard, {10, 7}, 1000, 1000});
    run.advance(.1F);
    require(enemy(run, 701).blocker == 600 && enemy(run, 700).blocker == 0,
            "a guard blocks the ground enemy and never the bat");
    require(enemy(run, 700).hp == bat.hp, "a guard's strike never reaches a bat");

    Run archer = battle(board());
    scenario(archer).enemies.push_back(fixture(archer, 702, EnemyKind::Flyer, 10));
    scenario(archer).units.push_back({601, CardKind::Shooter, {10, 7}, 70, 70});
    archer.advance(.1F);
    require(enemy(archer, 702).hp == bat.hp - archer.catalog().card(CardKind::Shooter).damage,
            "a shooter hits a bat");

    Run artillery = battle(board());
    scenario(artillery).enemies.push_back(fixture(artillery, 703, EnemyKind::Flyer, 10));
    scenario(artillery).towers.push_back({500, CardKind::Cannon, {10, 6}, 1, 0});
    artillery.advance(.1F);
    require(enemy(artillery, 703).hp == bat.hp &&
                count(artillery.takeCombatEvents(), CombatEventKind::Attack) == 0,
            "a cannon finds no target in a lone bat");
    Catalog tough = board();
    tough.enemies[static_cast<std::size_t>(EnemyKind::Normal)].hp = 500;
    Run splash = battle(tough);
    scenario(splash).enemies.push_back(fixture(splash, 704, EnemyKind::Normal, 10));
    scenario(splash).enemies.push_back(fixture(splash, 705, EnemyKind::Flyer, 10.3F));
    scenario(splash).towers.push_back({501, CardKind::Cannon, {10, 6}, 1, 0});
    splash.advance(.1F);
    require(enemy(splash, 704).hp < 500 && enemy(splash, 705).hp == bat.hp,
            "cannon splash skips a bat beside its ground target");
    Run rifle = battle(board());
    scenario(rifle).enemies.push_back(fixture(rifle, 706, EnemyKind::Flyer, 10));
    scenario(rifle).towers.push_back({502, CardKind::Gun, {10, 6}, 1, 0});
    rifle.advance(.1F);
    require(enemy(rifle, 706).hp == bat.hp - rifle.catalog().card(CardKind::Gun).damage, "a gun hits a bat");

    Run tar = battle(board());
    scenario(tar).enemies.push_back(fixture(tar, 707, EnemyKind::Flyer, 10));
    scenario(tar).enemies.push_back(fixture(tar, 708, EnemyKind::Normal, 10));
    scenario(tar).fields.push_back({900, CardKind::Tar, {10, 7}, 1.8F, 99});
    tar.advance(.1F);
    require(enemy(tar, 707).slowUntil == 0 && enemy(tar, 708).slowUntil > 0,
            "tar slows the ground, not a bat");

    Catalog vented = board();
    vented.vents = {{{11, 6}, .1F}};
    Run vent = battle(vented);
    scenario(vent).enemies.push_back(fixture(vent, 709, EnemyKind::Flyer, 11));
    scenario(vent).enemies.push_back(fixture(vent, 710, EnemyKind::Normal, 11));
    vent.advance(.1F);
    require(enemy(vent, 709).hp == bat.hp &&
                enemy(vent, 710).hp == role(vent, EnemyKind::Normal).hp - vent.catalog().balance.ventDamage,
            "an eruption hits the ground, not a bat");

    Catalog covered = board();
    covered.tunnels = {{0, 5, 9}};
    Run tunnel = battle(covered);
    scenario(tunnel).enemies.push_back(fixture(tunnel, 711, EnemyKind::Flyer, 6));
    scenario(tunnel).enemies.push_back(fixture(tunnel, 712, EnemyKind::Normal, 6));
    require(!tunnel.tunneled(enemy(tunnel, 711)) && !tunnel.underground(enemy(tunnel, 711)) &&
                tunnel.tunneled(enemy(tunnel, 712)),
            "a bat flies over a tunnel that hides the ground enemy beside it");
}

/** A stalker is picked as a target only up close until a hit reveals it at any range. */
void stalkers() {
    Catalog content = board();
    content.enemies[static_cast<std::size_t>(EnemyKind::Stalker)].hp = 500;
    Run run = battle(content);
    const EnemyDefinition &stalker = role(run, EnemyKind::Stalker);
    require(stalker.stealth == 1.8F && run.catalog().balance.revealTime == 2.5F,
            "a stalker hides beyond 1.8 tiles for 2.5 seconds after each hit");
    scenario(run).enemies.push_back(fixture(run, 720, EnemyKind::Stalker, 14));
    scenario(run).towers.push_back({510, CardKind::Sniper, {9, 4}, 1, 0});
    require(!run.visibleFrom(enemy(run, 720), {9, 4}) && run.visibleFrom(enemy(run, 720), {15, 6}),
            "a stalker is visible only within its stealth distance");
    run.advance(.1F);
    require(enemy(run, 720).hp == 500 && count(run.takeCombatEvents(), CombatEventKind::Attack) == 0,
            "a distant sniper cannot pick a hidden stalker");
    scenario(run).towers.push_back({511, CardKind::Gun, {15, 6}, 1, 0});
    run.advance(.1F);
    const int afterGun = enemy(run, 720).hp;
    require(afterGun == 500 - run.catalog().card(CardKind::Gun).damage && run.revealed(enemy(run, 720)),
            "a gun up close hits the stalker and reveals it");
    run.advance(.1F);
    require(enemy(run, 720).hp == afterGun - run.catalog().card(CardKind::Sniper).damage,
            "the revealed stalker is in the distant sniper's sights");
    scenario(run).towers.clear();
    for (int step = 0; step < 26; ++step)
        run.advance(.1F);
    require(!run.revealed(enemy(run, 720)), "the stalker hides again once nothing hits it");

    Catalog crowd = content;
    crowd.enemies[static_cast<std::size_t>(EnemyKind::Normal)].hp = 500;
    Run area = battle(crowd);
    scenario(area).enemies.push_back(fixture(area, 721, EnemyKind::Normal, 14));
    scenario(area).enemies.push_back(fixture(area, 722, EnemyKind::Stalker, 14.4F));
    scenario(area).towers.push_back({512, CardKind::Cannon, {12, 8}, 1, 0});
    area.advance(.1F);
    require(enemy(area, 722).hp < 500 && area.revealed(enemy(area, 722)),
            "splash aimed at a visible enemy hits and reveals a hidden stalker");
}

/** A leaper jumps over the first unit that blocks it and then stays blocked like any other enemy. */
void leapers() {
    Catalog content = board();
    content.enemies[static_cast<std::size_t>(EnemyKind::Leaper)].speed = .5F;
    Run run = battle(content);
    const EnemyDefinition &leaper = role(run, EnemyKind::Leaper);
    require(leaper.ability == EnemyAbility::Leap && leaper.abilityWindup == .6F &&
                leaper.abilityRadius == 2.5F && leaper.abilityAmount == 1,
            "a leaper jumps 2.5 tiles once after a 0.6 second windup");
    scenario(run).enemies.push_back(fixture(run, 730, EnemyKind::Leaper, 9.9F));
    scenario(run).units.push_back({620, CardKind::Guard, {10, 7}, 1000, 1000});
    int leaps = 0;
    float blockedAt = -1, landed = -1;
    for (int step = 0; step < 12 && leaps == 0; ++step) {
        run.advance(.1F);
        if (enemy(run, 730).blocker == 620 && blockedAt < 0)
            blockedAt = enemy(run, 730).progress;
        for (const CombatEvent &event : run.takeCombatEvents())
            if (event.type == CombatEventKind::Leap) {
                ++leaps;
                landed = enemy(run, 730).progress;
                require(event.subject == 730 && distance(event.to, event.from) > 2.4F &&
                            distance(event.to, enemy(run, 730).position) < .1F,
                        "the leap fact runs from the take-off to the landing");
            }
    }
    require(blockedAt >= 0 && leaps == 1, "a blocked leaper jumps once its windup ends");
    require(std::abs(landed - blockedAt - leaper.abilityRadius) < .06F && enemy(run, 730).charges == 0,
            "the jump carries the leaper 2.5 tiles along its route and spends its charge");
    scenario(run).units.push_back({621, CardKind::Guard, {13, 7}, 1000, 1000});
    for (int step = 0; step < 40; ++step) {
        run.advance(.1F);
        leaps += count(run.takeCombatEvents(), CombatEventKind::Leap);
    }
    require(leaps == 1 && enemy(run, 730).blocker == 621, "the second blocker holds the spent leaper");

    Run free = battle(content);
    scenario(free).enemies.push_back(fixture(free, 731, EnemyKind::Leaper, 1));
    for (int step = 0; step < 20; ++step)
        free.advance(.1F);
    require(count(free.takeCombatEvents(), CombatEventKind::Leap) == 0 && enemy(free, 731).charges == 1,
            "a leaper nobody blocks keeps its jump");
}

/** A mine cart waits for its first departure, hits every ground enemy on its rail once and comes back. */
void carts() {
    Catalog content = board();
    content.enemies[static_cast<std::size_t>(EnemyKind::Normal)].hp = 500;
    content.rails = {{{9, 7}, {13, 7}, {11, 9}}};
    Run idle = makeRun(content);
    require(idle.cartCountdown(0) == -1 && !idle.launchCart(0).ok && idle.cartProgress(0) == -1,
            "a cart stands outside battle");
    Run run = battle(content);
    const RunBalance &balance = run.catalog().balance;
    require(balance.cartDamage == 70 && balance.cartCooldown == 16 && balance.cartFirst == 4,
            "a cart deals 70, returns in 16 seconds and first leaves 4 seconds into a battle");
    require(std::abs(run.cartCountdown(0) - balance.cartFirst) < .001F && !run.launchCart(0).ok,
            "the first departure waits for its countdown");
    require(!run.launchCart(1).ok && run.cartCountdown(1) == -1, "an unknown rail is rejected");
    scenario(run).enemies.push_back(fixture(run, 740, EnemyKind::Normal, 9));
    scenario(run).enemies.push_back(fixture(run, 741, EnemyKind::Armored, 11));
    scenario(run).enemies.push_back(fixture(run, 742, EnemyKind::Flyer, 12));
    scenario(run).enemies.push_back(fixture(run, 743, EnemyKind::Normal, 2));
    for (int step = 0; step < 40; ++step)
        run.advance(.1F);
    run.takeCombatEvents();
    require(run.cartCountdown(0) == 0, "the cart is ready after its first countdown");
    require(run.launchCart(0).ok && run.cartProgress(0) == 0 && run.cartCountdown(0) == -1,
            "a ready cart leaves on its rail");
    require(count(run.takeCombatEvents(), CombatEventKind::Cart) == 1, "the departure is one committed fact");
    require(!run.launchCart(0).ok, "a running cart cannot leave again");
    for (int step = 0; step < 10; ++step)
        run.advance(.1F);
    require(run.cartProgress(0) == -1 && run.cartCountdown(0) > balance.cartCooldown - 1,
            "the cart finishes its run and starts its return countdown");
    require(enemy(run, 740).hp == 500 - balance.cartDamage, "the cart hits a ground enemy on the rail once");
    require(enemy(run, 741).hp == role(run, EnemyKind::Armored).hp - (balance.cartDamage - 4),
            "armor reduces the cart's hit");
    require(enemy(run, 742).hp == role(run, EnemyKind::Flyer).hp && enemy(run, 743).hp == 500,
            "a bat and an enemy away from the rail are untouched");
    Run again = battle(content);
    require(std::abs(again.cartCountdown(0) - balance.cartFirst) < .001F,
            "every battle starts the cart anew");
}

/** A gas tank explodes once per battle and hurts everything surfaced in its radius, allies included. */
void tanks() {
    Catalog content = board();
    content.enemies[static_cast<std::size_t>(EnemyKind::Normal)].hp = 500;
    content.tanks = {{{11, 8}}};
    Run idle = makeRun(content);
    require(!idle.tankReady(0) && !idle.detonateTank(0).ok, "a tank cannot explode outside battle");
    Run run = battle(content);
    const RunBalance &balance = run.catalog().balance;
    require(balance.tankDamage == 90 && balance.tankRadius == 1.8F, "a tank deals 90 within 1.8 tiles");
    require(run.tankReady(0) && !run.tankReady(1) && !run.detonateTank(1).ok,
            "only an authored tank is ready");
    scenario(run).enemies.push_back(fixture(run, 750, EnemyKind::Normal, 11));
    scenario(run).enemies.push_back(fixture(run, 751, EnemyKind::Flyer, 12));
    scenario(run).enemies.push_back(fixture(run, 752, EnemyKind::Armored, 2));
    scenario(run).enemies.push_back(fixture(run, 753, EnemyKind::Normal, 10));
    scenario(run).units.push_back({630, CardKind::Guard, {10, 7}, 120, 120});
    scenario(run).units.push_back({631, CardKind::Shooter, {12, 7}, 70, 70});
    run.advance(.1F);
    run.takeCombatEvents();
    require(enemy(run, 753).blocker == 630, "the guard holds an enemy before the blast");
    const int guardHp = run.unit(630)->hp;
    require(run.detonateTank(0).ok, "a ready tank explodes");
    const std::vector<CombatEvent> events = run.takeCombatEvents();
    require(count(events, CombatEventKind::Detonation) == 1, "the explosion is one committed fact");
    for (const CombatEvent &event : events)
        if (event.type == CombatEventKind::Detonation)
            require(event.amount == 5 && event.radius == balance.tankRadius, "the fact counts every target");
    require(enemy(run, 750).hp == 410 && !find(run, 751) &&
                enemy(run, 752).hp == role(run, EnemyKind::Armored).hp,
            "the blast hits enemies in its radius, bats included, and nothing beyond");
    require(run.unit(630) && run.unit(630)->hp == guardHp - balance.tankDamage && !run.unit(631),
            "the blast hurts allied units and removes one it kills");
    require(!run.tankReady(0) && !run.detonateTank(0).ok, "a tank explodes once per battle");
    Run again = battle(content);
    require(again.tankReady(0), "the tank is refilled for the next battle");
}

/** A bog slows the ground enemies wading through it and leaves bats alone. */
void bogs() {
    Catalog content = defaultCatalog();
    content.bogs = {{0, 8, 14}};
    Run run = battle(content);
    require(run.catalog().balance.bogSpeed == .7F, "a bog slows to 70 percent");
    scenario(run).enemies.push_back(fixture(run, 760, EnemyKind::Normal, 9));
    scenario(run).enemies.push_back(fixture(run, 761, EnemyKind::Flyer, 9));
    scenario(run).enemies.push_back(fixture(run, 762, EnemyKind::Normal, 1));
    require(run.bogged(enemy(run, 760)) && !run.bogged(enemy(run, 761)) && !run.bogged(enemy(run, 762)),
            "only the ground enemy inside the span is bogged");
    for (int step = 0; step < 10; ++step)
        run.advance(.1F);
    const float normal = role(run, EnemyKind::Normal).speed, bat = role(run, EnemyKind::Flyer).speed;
    require(std::abs(enemy(run, 760).progress - (9 + normal * .7F)) < .02F, "the bog slows a ground enemy");
    require(std::abs(enemy(run, 761).progress - (9 + bat)) < .02F, "a bat crosses at full speed");
    require(std::abs(enemy(run, 762).progress - (1 + normal)) < .02F, "the dry route keeps its speed");
    require(run.bogs().size() == 1 && run.bogs().front().enter == 8,
            "the bog span follows the authored route");
}

/** Verify one rejected catalog and its field-specific diagnostic. */
void reject(const std::function<void(Catalog &)> &mutate, const std::string &field) {
    Catalog candidate = defaultCatalog();
    mutate(candidate);
    RunCreation result = Run::create(std::move(candidate));
    require(!result.run && !result.validation.ok(), "invalid content must not construct a run");
    require(std::any_of(result.validation.issues.begin(), result.validation.issues.end(),
                        [&field](const CatalogIssue &issue) -> bool { return issue.path == field; }),
            field.c_str());
}
/** Malformed rails, tanks, bogs and new role fields are rejected with the path of the offending field. */
void validation() {
    reject([](Catalog &data) { data.rails = {{{9, 7}, {10, 7}, {11, 9}}}; }, "rails[0]");
    reject([](Catalog &data) { data.rails = {{{-1, 7}, {13, 7}, {11, 9}}}; }, "rails[0].from");
    reject([](Catalog &data) { data.rails = {{{9, 7}, {13, 7}, {10, 6}}}; }, "rails[0].lever");
    reject([](Catalog &data) { data.rails = {{{9, 7}, {13, 7}, {10, 7}}}; }, "rails[0].lever");
    reject([](Catalog &data) { data.rails.assign(5, {{9, 7}, {13, 7}, {11, 9}}); }, "rails");
    reject([](Catalog &data) { data.tanks = {{{10, 6}}}; }, "tanks[0].position");
    reject([](Catalog &data) { data.tanks = {{{11, 8}}, {{11, 8}}}; }, "tanks[1].position");
    reject([](Catalog &data) { data.tanks = {{{30, 8}}}; }, "tanks[0].position");
    reject([](Catalog &data) { data.bogs = {{5, 1, 3}}; }, "bogs[0].route");
    reject([](Catalog &data) { data.bogs = {{0, 2, 2.5F}}; }, "bogs[0]");
    reject([](Catalog &data) { data.bogs = {{0, 10, 30}}; }, "bogs[0]");
    reject([](Catalog &data) { data.bogs = {{0, 2, 6}, {0, 5, 8}}; }, "bogs[1]");
    reject(
        [](Catalog &data) {
            data.unitCells = {{6, 3}, {10, 7}, {12, 7}};
            data.tunnels = {{0, 5, 9}};
            data.bogs = {{0, 6, 8}};
        },
        "bogs[0]");
    reject([](Catalog &data) { data.balance.cartSpeed = 0; }, "balance.cartSpeed");
    reject([](Catalog &data) { data.balance.cartDamage = 0; }, "balance.cartDamage");
    reject([](Catalog &data) { data.balance.tankDamage = 0; }, "balance.tankDamage");
    reject([](Catalog &data) { data.balance.tankRadius = 0; }, "balance.tankRadius");
    reject([](Catalog &data) { data.balance.bogSpeed = 1.5F; }, "balance.bogSpeed");
    reject([](Catalog &data) { data.balance.revealTime = -1; }, "balance.revealTime");
    reject([](Catalog &data) { data.enemies[static_cast<std::size_t>(EnemyKind::Leaper)].abilityAmount = 0; },
           "enemies[11].abilityAmount");
    reject([](Catalog &data) { data.enemies[static_cast<std::size_t>(EnemyKind::Stalker)].stealth = -1; },
           "enemies[10].stealth");
}

/** The two new stages ship their machines beside the routes and every new role debuts in the campaign. */
void shipped() {
    require(TotalStages == 7, "the campaign has seven stages");
    const auto nearRoute = [](const Run &run, Point point, float reach) -> bool {
        for (std::uint32_t lane = 0; lane <= run.catalog().additionalRoutes.size(); ++lane)
            for (float progress = 0; progress <= run.routeLength(lane); progress += .5F)
                if (distance(run.routePoint(progress, lane), point) <= reach)
                    return true;
        return false;
    };
    const Catalog &cavern = stageCatalog(6);
    Run cave = makeRun(cavern);
    require(cavern.rails.size() == 2 && cavern.tanks.empty() && cavern.bogs.empty() &&
                !cavern.boulders.empty(),
            "stage 6 ships cart rails and crystal clusters");
    for (const RailDefinition &rail : cavern.rails)
        for (float part = 0; part <= 1.001F; part += .25F)
            require(nearRoute(cave,
                              {rail.from.x + (rail.to.x - rail.from.x) * part,
                               rail.from.y + (rail.to.y - rail.from.y) * part},
                              .26F),
                    "a shipped rail runs along a route");
    const Catalog &swamp = stageCatalog(7);
    Run bog = makeRun(swamp);
    require(swamp.tanks.size() == 6 && swamp.bogs.size() == 4 && swamp.rails.empty(),
            "stage 7 ships bogs and gas tanks");
    for (const TankDefinition &tank : swamp.tanks)
        require(nearRoute(bog, tank.position, 2.01F), "a shipped tank stands within two tiles of a route");
    require(stageMetadata(6).biome == StageBiome::Crystal && stageMetadata(7).biome == StageBiome::Swamp &&
                stageCatalog(6).mine.biome == StageBiome::Crystal &&
                stageCatalog(7).deeperMines.back().biome == StageBiome::Swamp,
            "the new stages draw their own biomes above and below ground");
    require(stageCatalog(3).enemyDebuts[static_cast<std::size_t>(EnemyKind::Leaper)] == 6 &&
                stageCatalog(4).enemyDebuts[static_cast<std::size_t>(EnemyKind::Stalker)] == 3 &&
                stageCatalog(4).enemyDebuts[static_cast<std::size_t>(EnemyKind::Flyer)] == 6,
            "leapers debut on stage 3, stalkers and bats on stage 4");
    for (int stage = 1; stage <= 2; ++stage)
        for (EnemyKind kind : {EnemyKind::Flyer, EnemyKind::Stalker, EnemyKind::Leaper})
            require(stageCatalog(stage).introductionWave(kind) == 0,
                    "the first two stages keep the old roster");
    for (int stage = 6; stage <= TotalStages; ++stage) {
        require(stageCatalog(stage).waves.size() == 16, "the new stages last sixteen waves");
        for (EnemyKind kind : {EnemyKind::Flyer, EnemyKind::Stalker, EnemyKind::Leaper})
            require(stageCatalog(stage).introductionWave(kind) > 2,
                    "the new roles join the new stages after their opening waves");
    }
    require(stageCatalog(7).enemy(EnemyKind::Boss).hp == stageCatalog(5).enemy(EnemyKind::Boss).hp + 100,
            "the boss grows slower past the core");
}

/** The shared autoplayer sends a cart into a crowd and leaves an empty rail alone. */
void autoplayer() {
    Catalog content = board();
    content.enemies[static_cast<std::size_t>(EnemyKind::Normal)].hp = 500;
    content.rails = {{{9, 7}, {13, 7}, {11, 9}}};
    content.tanks = {{{11, 2}}};
    Run run = battle(content);
    for (int step = 0; step < 40; ++step)
        run.advance(.1F);
    autoplay::operate(run);
    require(run.cartProgress(0) == -1 && run.tankReady(0), "an empty field leaves the machines alone");
    for (EntityId id = 770; id < 774; ++id)
        scenario(run).enemies.push_back(fixture(run, id, EnemyKind::Normal, 9.5F + (id - 770) * .8F));
    autoplay::operate(run);
    require(run.cartProgress(0) == 0, "a crowd on the rail sends the cart");
    require(run.tankReady(0), "a tank away from the crowd stays ready");
}
/** A building fixture with full durability for its level. */
Tower building(const Run &run, EntityId id, CardKind kind, Point position) {
    Tower tower{id, kind, position};
    tower.maxHp = run.catalog().towerHealth(1);
    tower.hp = tower.maxHp;
    return tower;
}
/** Wreckers and slingers stop at a building in reach and break it; units recover while they hold no enemy. */
void sieges() {
    Run run = battle(board());
    const EnemyDefinition &wrecker = role(run, EnemyKind::Wrecker);
    const int durability = run.catalog().towerHealth(1);
    require(wrecker.ability == EnemyAbility::Siege && wrecker.abilityAmount > 0 && wrecker.name == "망치병",
            "the wrecker strikes buildings");
    require(durability == run.catalog().balance.towerHp && run.catalog().towerHealth(2) > durability,
            "durability grows with the building's level");
    scenario(run).enemies.push_back(fixture(run, 710, EnemyKind::Wrecker, 10));
    scenario(run).towers.push_back(building(run, 510, CardKind::Generator, {10, 6}));
    run.advance(.1F);
    require(enemy(run, 710).sabotage == 510, "a wrecker locks onto a building in reach");
    int strikes = 0, wrecks = 0;
    for (int step = 0; step < 600 && run.tower(510); ++step) {
        run.advance(.1F);
        const std::vector<CombatEvent> events = run.takeCombatEvents();
        strikes += count(events, CombatEventKind::Batter);
        wrecks += count(events, CombatEventKind::Wreck);
        if (run.tower(510) && strikes == 1)
            require(run.tower(510)->hp == durability - wrecker.abilityAmount,
                    "each strike removes its damage");
    }
    require(!run.tower(510) && wrecks == 1 &&
                strikes == (durability + wrecker.abilityAmount - 1) / wrecker.abilityAmount,
            "the building breaks when its durability reaches zero");
    require(enemy(run, 710).sabotage == 0, "the wrecker lets go of a broken building");

    Run held = battle(board());
    scenario(held).enemies.push_back(fixture(held, 711, EnemyKind::Wrecker, 10));
    scenario(held).towers.push_back(building(held, 511, CardKind::Generator, {10, 6}));
    scenario(held).units.push_back({612, CardKind::Guard, {10, 7}, 1000, 1000});
    for (int step = 0; step < 20; ++step)
        held.advance(.1F);
    require(held.tower(511)->hp == durability && enemy(held, 711).sabotage == 0,
            "a blocked wrecker cannot strike a building");

    Run range = battle(board());
    const EnemyDefinition &slinger = role(range, EnemyKind::Slinger);
    require(slinger.ability == EnemyAbility::Siege &&
                slinger.abilityRadius > range.catalog().card(CardKind::Gun).range,
            "the slinger throws from beyond a gun tower's range");
    scenario(range).enemies.push_back(fixture(range, 712, EnemyKind::Slinger, 10));
    const Point origin = range.routePoint(10);
    scenario(range).towers.push_back(building(range, 512, CardKind::Gun, {origin.x, origin.y - 3.6F}));
    int attacks = 0;
    for (int step = 0; step < 30; ++step) {
        range.advance(.1F);
        attacks += count(range.takeCombatEvents(), CombatEventKind::Attack);
    }
    require(enemy(range, 712).sabotage == 512 && range.tower(512)->hp < durability && attacks == 0,
            "a slinger strikes a gun tower that cannot answer");

    Run rest = battle(board());
    scenario(rest).units.push_back({613, CardKind::Guard, {6, 3}, 60, 120});
    for (int step = 0; step < 100; ++step)
        rest.advance(.1F);
    const int recovered = rest.unit(613)->hp - 60;
    const int expected = static_cast<int>(120 * rest.catalog().balance.unitRegen * 10);
    require(recovered >= expected - 1 && recovered <= expected + 1, "an idle unit recovers over time");
}
/** A barricade reroutes its entrance between waves only, shifting spans after the fork and dropping those
 *  inside the closed stretch; snapshots keep the choice. */
void forks() {
    Catalog content = defaultCatalog();
    content.forks = {{0, {8, 3}, {14, 5}, {{12, 3}, {12, 5}}}};
    content.bogs = {{0, 5, 7}, {0, 16.5F, 17.5F}};
    require(validateCatalog(content).ok(), "a fork whose spans lie inside or after its stretch is valid");
    Run run = makeRun(content);
    const float authored = run.routeLength();
    require(!run.detoured(0) && authored == 18, "the authored branch is open at first");
    const std::vector<Point> closed = run.forkBranch(0, true);
    require(closed.size() == 4 && distance(run.barricade(0), {9, 3}) < .01F,
            "the barricade stands one tile into the closed detour");
    require(run.switchFork(0).ok && run.detoured(0), "a barricade moves before the first wave");
    require(std::abs(run.routeLength() - 14) < .01F && distance(run.routePoint(9), {12, 4}) < .01F,
            "enemies now follow the detour");
    require(distance(run.barricade(0), {8, 4}) < .01F, "the barricade closes the authored branch");
    std::vector<RouteSpan> primary;
    for (const RouteSpan &span : run.bogs())
        if (span.route == 0)
            primary.push_back(span);
    require(primary.size() == 1 && std::abs(primary[0].enter - 12.5F) < .01F,
            "a bog after the fork shifts and one inside its closed stretch lapses");
    require(std::count_if(run.bogs().begin(), run.bogs().end(),
                          [](const RouteSpan &span) { return span.route == 1; }) == 2,
            "the closed authored branch keeps both bogs on its own route");
    const std::string text = run.snapshot();
    Run copy = makeRun(content);
    require(copy.restore(text).ok && copy.detoured(0) && copy.routeLength() == run.routeLength(),
            "a snapshot keeps the open branch");
    require(run.startWave().ok && !run.switchFork(0).ok && run.detoured(0),
            "a barricade holds during battle");

    Catalog bad = content;
    bad.forks.front().merge = {4, 3};
    require(!validateCatalog(bad).ok(), "a merge before the split is rejected");
    bad = content;
    bad.forks.front().detour = {{11, 4}};
    require(!validateCatalog(bad).ok(), "a diagonal detour is rejected");
    bad = content;
    bad.bogs = {{0, 2, 7}};
    require(!validateCatalog(bad).ok(), "a span across the split is rejected");
    bad = content;
    bad.forks.push_back(bad.forks.front());
    require(!validateCatalog(bad).ok(), "two forks on one route are rejected");
    for (int stage = 2; stage <= TotalStages; ++stage)
        require(stageCatalog(stage).forks.size() == 1, "every stage from 2 has a fork on its first entrance");
    require(stageCatalog(1).forks.empty(), "stage 1 keeps a single path");
}
/** A carrier flies its cargo past the front line and drops it once, partway along its route. */
void carriers() {
    Catalog content = defaultCatalog();
    content.enemies[static_cast<std::size_t>(EnemyKind::Normal)].speed = 0;
    Run run = battle(content);
    const EnemyDefinition &carrier = role(run, EnemyKind::Carrier);
    require(carrier.flying && carrier.ability == EnemyAbility::Airdrop && carrier.abilityAmount == 3,
            "the carrier flies with three passengers");
    Enemy flyer = fixture(run, 720, EnemyKind::Carrier, run.routeLength() * carrier.abilityRadius - .2F);
    flyer.charges = 1;
    scenario(run).enemies.push_back(flyer);
    int drops = 0;
    for (int step = 0; step < 10; ++step) {
        run.advance(.1F);
        drops += count(run.takeCombatEvents(), CombatEventKind::Airdrop);
    }
    const auto cargo = std::count_if(run.state().enemies.begin(), run.state().enemies.end(),
                                     [](const Enemy &enemy) { return enemy.kind == EnemyKind::Normal; });
    require(drops == 1 && cargo >= 3 && enemy(run, 720).charges == 0,
            "the cargo lands once, behind the carrier");
    for (int step = 0; step < 10; ++step) {
        run.advance(.1F);
        drops += count(run.takeCombatEvents(), CombatEventKind::Airdrop);
    }
    require(drops == 1, "an empty carrier drops nothing more");
    require(stageCatalog(4).introductionWave(EnemyKind::Carrier) == 9, "carriers arrive in stage 4");
}
/** A siege enemy at a fork breaks its barricade; enemies short of the split then take the shorter branch. */
void breaches() {
    Catalog content = defaultCatalog();
    content.forks = {{0, {8, 3}, {14, 5}, {{12, 3}, {12, 5}}}};
    for (EnemyDefinition &definition : content.enemies)
        definition.speed = 0;
    Run run = battle(content);
    const EnemyDefinition &wrecker = role(run, EnemyKind::Wrecker);
    const int durability = run.catalog().balance.barricadeHp;
    require(run.barricadeHp(0) == durability && run.routeLength(1) < run.routeLength(0),
            "a full barricade closes the shorter detour, kept as route 1");
    scenario(run).enemies.push_back(fixture(run, 730, EnemyKind::Wrecker, 4));
    scenario(run).enemies.push_back(fixture(run, 731, EnemyKind::Normal, 1));
    scenario(run).enemies.push_back(fixture(run, 732, EnemyKind::Normal, 7));
    run.advance(.1F);
    require(enemy(run, 730).sabotage == Run::BarricadeBase,
            "a wrecker beside the split locks onto the barricade");
    int breaches = 0;
    for (int step = 0; step < 600 && run.barricadeHp(0) > 0; ++step) {
        run.advance(.1F);
        breaches += count(run.takeCombatEvents(), CombatEventKind::Breach);
    }
    require(run.barricadeHp(0) == 0 && breaches == 1, "strikes break the barricade once");
    require(enemy(run, 731).route == 1 && run.laneOf(1) == 0,
            "an enemy short of the split takes the shortcut");
    require(enemy(run, 732).route == 0 && distance(enemy(run, 732).position, run.routePoint(7, 0)) < .01F,
            "an enemy past the split keeps its branch and position");
    require(enemy(run, 730).sabotage == 0, "the wrecker walks on");
    (void)wrecker;
}
/** A damaged building is repaired to full during preparation for one iron per repairHp missing. */
void repairs() {
    Run run = makeRun(defaultCatalog());
    Tower damaged = building(run, 540, CardKind::Gun, {6, 4});
    damaged.hp = 30;
    scenario(run).towers.push_back(damaged);
    require(run.repairQuote(540).failure == PurchaseFailure::Closed, "repairs wait for preparation");
    require(run.startWave().ok, "fixture battle starts");
    for (int step = 0; step < 3000 && run.state().phase == Phase::Battle; ++step)
        run.advance(.1F);
    require(run.state().phase == Phase::Prep, "the fixture reaches preparation");
    const int left = run.tower(540)->hp;
    const int missing = run.tower(540)->maxHp - left;
    const int iron = (missing + run.catalog().balance.repairHp - 1) / run.catalog().balance.repairHp;
    require(missing > 0 && run.repairQuote(540).cost.iron == iron, "the price covers the missing durability");
    scenario(run).iron = iron - 1;
    require(!run.repair(540).ok && run.tower(540)->hp == left, "a short wallet repairs nothing");
    scenario(run).iron = iron;
    require(run.repair(540).ok && run.tower(540)->hp == run.tower(540)->maxHp && run.state().iron == 0,
            "a repair restores full durability for its price");
    require(run.repairQuote(540).failure == PurchaseFailure::Owned, "a full building needs no repair");
}
/** The stage hero takes no slot, moves during battle on a cooldown and returns after it falls. */
void heroes() {
    Run run = makeRun(stageCatalog(2));
    const RunBalance &balance = run.catalog().balance;
    const Unit *hero = run.hero();
    require(hero && hero->hp == balance.heroHp && run.squadSize() == 0 &&
                distance(hero->position, run.state().heroCell) < .01F,
            "a campaign run starts with a slot-free hero");
    require(!makeRun(defaultCatalog()).hero(), "the practice board has no hero");
    const EntityId id = hero->id;
    require(!run.dismiss(id).ok && !run.heal(id).ok, "the hero cannot be dismissed or paid to heal");
    require(run.startWave().ok, "the hero's battle starts");
    Point other = run.state().unitCells.front();
    for (Point cell : run.state().unitCells)
        if (distance(cell, hero->position) > 2) {
            other = cell;
            break;
        }
    require(run.moveUnit(id, other).ok && distance(run.unit(id)->position, other) < .01F,
            "the hero moves during battle");
    require(!run.moveUnit(id, run.state().heroCell).ok && run.state().heroMove > 0, "the next move waits");
    for (Unit &ally : scenario(run).units)
        if (ally.hero)
            ally.hp = 0;
    run.advance(.1F);
    require(!run.hero() && run.state().heroDown > balance.heroRespawn - .2F,
            "a fallen hero leaves the field");
    for (int step = 0; step < static_cast<int>(balance.heroRespawn * 10) + 5; ++step)
        run.advance(.1F);
    require(run.hero() && run.hero()->hp == balance.heroHp && run.state().heroDown == 0,
            "the hero returns at full health");
}
} // namespace

int main() {
    flyers();
    stalkers();
    leapers();
    carts();
    tanks();
    bogs();
    validation();
    shipped();
    autoplayer();
    sieges();
    forks();
    carriers();
    breaches();
    repairs();
    heroes();
    std::cout << "machines and roles: " << checks << " checks passed\n";
    return 0;
}
