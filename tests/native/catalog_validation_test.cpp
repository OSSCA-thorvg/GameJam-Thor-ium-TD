#include "game/rules/Run.h"
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <type_traits>

using namespace thorium::game;
namespace {
int checks = 0;
/** Keep invalid authored content from producing a partially playable object in release builds. */
void require(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Verify both rejection and field-specific diagnostics through the production creation boundary. */
void reject(const std::function<void(Catalog &)> &mutate, const std::string &field) {
    Catalog candidate = defaultCatalog();
    mutate(candidate);
    RunCreation result = Run::create(std::move(candidate));
    require(!result.run && !result.validation.ok(), "invalid content must not construct a run");
    require(std::any_of(result.validation.issues.begin(), result.validation.issues.end(),
                        [&field](const CatalogIssue &issue) -> bool { return issue.path == field; }),
            field.c_str());
}
/** Exercise malformed numbers, missing references, geometry joins and mining prerequisites. */
void rejectionContracts() {
    reject([](Catalog &data) -> void { data.cards[0].cost = -3; }, "cards[0].cost");
    reject([](Catalog &data) -> void { data.balance.recruit.gold = -4; }, "balance.recruit.gold");
    reject([](Catalog &data) -> void { data.cards[0].range = std::numeric_limits<float>::quiet_NaN(); },
           "cards[0].range");
    reject([](Catalog &data) -> void { data.enemies[0].interval = 0; }, "enemies[0].interval");
    reject([](Catalog &data) -> void { data.enemies[0].hp = 0; }, "enemies[0].hp");
    reject([](Catalog &data) -> void { data.cards[2].hp = 0; }, "cards[2].hp");
    reject([](Catalog &data) -> void { data.starterDeck[0] = static_cast<CardKind>(-1); }, "starterDeck[0]");
    reject([](Catalog &data) -> void { data.recruitmentCandidates[0] = CardKind::Gun; },
           "recruitmentCandidates[0]");
    reject([](Catalog &data) -> void { data.recruitmentCandidates[1] = data.recruitmentCandidates[0]; },
           "recruitmentCandidates[1]");
    reject([](Catalog &data) -> void { data.route.clear(); }, "route");
    reject([](Catalog &data) -> void { data.route.insert(data.route.begin(), data.route.front()); },
           "route[1]");
    reject([](Catalog &data) -> void { data.route[0].x = std::numeric_limits<float>::infinity(); },
           "route[0]");
    reject([](Catalog &data) -> void { data.expansions[0].prefix.push_back(data.route.front()); },
           "expansions[0].route[7]");
    reject([](Catalog &data) -> void { data.expansions[0].reward = static_cast<CardKind>(99); },
           "expansions[0].reward");
    reject([](Catalog &data) -> void { data.expansions[0].pads.push_back(data.pads.front()); },
           "expansions[0].pads");
    reject([](Catalog &data) -> void { data.waves[0][0].kind = static_cast<EnemyKind>(99); },
           "waves[0][0].kind");
    reject([](Catalog &data) -> void { data.waves[0][0].count = std::numeric_limits<int>::min(); },
           "waves[0][0].count");
    reject([](Catalog &data) -> void { data.waves[0][0].count = CatalogLimits::MaxWaveEnemies + 1; },
           "waves[0][0].count");
    reject([](Catalog &data) -> void { data.balance.minerStep = 0; }, "balance.minerStep");
    reject([](Catalog &data) -> void { data.balance.initialEnergy = data.balance.energyCap + 1; },
           "balance.initialEnergy");
    reject([](Catalog &data) -> void { data.mine[2].opened = false; }, "mine[2].opened");
    reject([](Catalog &data) -> void { data.mine[29].opened = true; }, "mine[29].opened");
    reject([](Catalog &data) -> void { data.mine[2].nest = true; }, "mine[2].nest");
    reject([](Catalog &data) -> void { data.mine[6].grade = 3; }, "mine[6].grade");
    Catalog empty;
    RunCreation malformed = Run::create(empty);
    require(
        !malformed.run && malformed.validation.issues.size() > 10,
        "default-initialized raw definitions report multiple errors without reading indeterminate fields");
}
/** Preserve deliberate zero values and permit valid non-default content with a stable snapshot. */
void acceptanceContracts() {
    require(validateCatalog(defaultCatalog()).ok(),
            "all shipped definitions pass numeric and geometry validation");
    Catalog authored = defaultCatalog();
    authored.enemies[0].speed = 0;
    authored.cards[0].cost = 0;
    authored.balance.recruit = {};
    authored.waves = {{}, {{EnemyKind::Normal, 2, 0, 0}}, {}};
    RunCreation result = Run::create(authored);
    require(result.run && result.validation.ok(),
            "empty waves, free prices, simultaneous spawns and stationary enemies remain supported");
    authored.cards[0].cost = -3;
    require(result.run->catalog().cards[0].cost == 0,
            "a committed run is independent of later edits to its source");
    require(result.run->startWave().ok, "valid authored run starts normally");
    result.run->advance(1.0F / 60.0F);
    require(result.run->state().phase == Phase::Prep && result.run->state().recruitmentOffer,
            "validated empty first wave enters the real preparation offer transition");
    result.run->reset();
    require(result.run->state().phase == Phase::Ready && !result.run->state().recruitmentOffer &&
                result.run->catalog().cards[0].cost == 0,
            "reset clears live offers and progress while preserving the validated catalog");
}
} // namespace
/** Check the creation contract without SDL, ThorVG, exceptions or UI initialization. */
int main() {
    static_assert(!std::is_constructible_v<Run, Catalog>);
    rejectionContracts();
    acceptanceContracts();
    std::cout << checks << " catalog validation checks passed\n";
}
