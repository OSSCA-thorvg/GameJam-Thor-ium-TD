#include "Run.h"
#include "RuleGeometry.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace thorium::game {
namespace {
/** Treat an invalid compiled-in default as a developer invariant; imports use the fallible factory. */
Catalog checkedDefaultCatalog() {
    static const CatalogValidation validation = validateCatalog(defaultCatalog());
    if (!validation.ok()) {
        std::fprintf(stderr, "Invalid shipped catalog: %s\n", validation.summary().c_str());
        std::abort();
    }
    return defaultCatalog();
}
/** Serialize only visible offer candidates and prices, without exposing hidden mining contents. */
void writeOffer(std::ostringstream &output, const std::optional<CardOffer> &offer) {
    if (!offer) {
        output << "null";
        return;
    }
    output << "{\"id\":" << offer->id << ",\"source\":" << static_cast<int>(offer->source)
           << ",\"cost\":{\"iron\":" << offer->cost.iron << ",\"copper\":" << offer->cost.copper
           << ",\"crystal\":" << offer->cost.crystal << ",\"gold\":" << offer->cost.gold
           << "},\"candidates\":[";
    for (std::size_t index = 0; index < offer->candidates.size(); ++index) {
        if (index)
            output << ',';
        output << static_cast<int>(offer->candidates[index]);
    }
    output << "]}";
}
} // namespace
Run::Run() : Run(checkedDefaultCatalog(), Checked{}) {}
RunCreation Run::create(Catalog catalog) {
    RunCreation result;
    result.validation = validateCatalog(catalog);
    if (result.validation.ok())
        result.run.reset(new Run(std::move(catalog), Checked{}));
    return result;
}
void Run::reset() {
    *this = Run(catalog_, Checked{});
}
Run::Run(Catalog catalog, Checked) : catalog_(std::move(catalog)) {
    state_.baseHp = catalog_.balance.baseHp;
    state_.energy = catalog_.balance.initialEnergy;
    state_.slots = catalog_.balance.initialSlots;
    for (int index = 0; index < 8; ++index)
        state_.deck[index] = {nextCard_++, catalog_.starterDeck[index]};
    state_.hand = {0, 1, 2, 3};
    state_.queue = {4, 5, 6, 7};
    state_.route = catalog_.route;
    state_.pads = catalog_.pads;
    state_.unitCells = catalog_.unitCells;
    for (int index = 0; index < 30; ++index) {
        const MineTileDefinition &tile = catalog_.mine[index];
        MineCell &cell = state_.mine[index];
        cell.ore = tile.ore;
        cell.amount = tile.amount;
        cell.grade = tile.grade;
        cell.nest = tile.nest;
        cell.dug = tile.opened;
        cell.info = tile.opened || tile.revealed;
        cell.clueKnown = tile.opened;
        for (int other = 0; other < 30; ++other)
            if (other != index && catalog_.mine[other].nest && std::abs(index % 5 - other % 5) <= 1 &&
                std::abs(index / 5 - other / 5) <= 1)
                ++cell.clue;
    }
}

const Tower *Run::tower(EntityId id) const {
    for (const Tower &item : state_.towers)
        if (item.id == id)
            return &item;
    return nullptr;
}

const Unit *Run::unit(EntityId id) const {
    for (const Unit &item : state_.units)
        if (item.id == id)
            return &item;
    return nullptr;
}

int Run::waveSpawnCount() const {
    return static_cast<int>(spawns_.size());
}

int Run::waveSpawned() const {
    return static_cast<int>(nextSpawn_);
}

void Run::emit(CombatEvent event) {
    if (combatEvents_.size() >= 256)
        combatEvents_.pop_front();
    combatEvents_.push_back(event);
}

std::vector<CombatEvent> Run::takeCombatEvents() {
    std::vector<CombatEvent> result(combatEvents_.begin(), combatEvents_.end());
    combatEvents_.clear();
    return result;
}

void Run::advance(float delta) {
    if (!std::isfinite(delta) || delta <= 0 || delta > 0.1F)
        return;
    if (state_.phase == Phase::Won || state_.phase == Phase::Lost)
        return;
    if (state_.phase == Phase::Battle)
        advanceBattle(delta);
    else if (state_.phase == Phase::Prep)
        advanceMining(delta);
}

std::string Run::debugState() const {
    std::ostringstream output;
    output << "{\"phase\":" << static_cast<int>(state_.phase) << ",\"wave\":" << state_.wave
           << ",\"hp\":" << state_.baseHp << ",\"energy\":" << state_.energy
           << ",\"towers\":" << state_.towers.size() << ",\"units\":" << state_.units.size()
           << ",\"enemies\":" << state_.enemies.size() << ",\"gold\":" << state_.gold
           << ",\"iron\":" << state_.iron << ",\"copper\":" << state_.copper
           << ",\"crystal\":" << state_.crystal << ",\"tool\":" << state_.tool << ",\"digs\":" << state_.digs
           << ",\"scans\":" << state_.scans << ",\"busy\":" << (state_.job.active ? "true" : "false")
           << ",\"spawned\":" << state_.spawned << ",\"defeated\":" << state_.defeated
           << ",\"nextExtra\":" << state_.nextExtra << ",\"branch\":" << state_.branch
           << ",\"time\":" << state_.battleTime << ",\"hand\":[";
    for (int index = 0; index < 4; ++index) {
        if (index)
            output << ',';
        output << static_cast<int>(state_.deck[state_.hand[index]].kind);
    }
    output << "],\"offers\":{\"recruitment\":";
    writeOffer(output, state_.recruitmentOffer);
    output << ",\"reward\":";
    writeOffer(output, state_.rewardOffer);
    output << "}}";
    return output.str();
}
} // namespace thorium::game
