#include "Run.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>

namespace thorium::game {
namespace {
/** First token of every snapshot; a different version is rejected rather than migrated. */
constexpr const char *SnapshotTag = "THORIUM-RUN";
constexpr int SnapshotVersion = 6;
/** Largest count of any stored list; longer lists mark a damaged snapshot. */
constexpr int MaxItems = 4096;

/** Space-separated token writer; floats keep enough digits to read back exactly. */
class Writer {
  public:
    Writer() {
        out_ << std::setprecision(9);
    }
    /** Append one integer, enum or boolean token. */
    template <typename T> Writer &operator<<(const T &value) {
        if constexpr (std::is_enum_v<T>)
            out_ << static_cast<int>(value) << ' ';
        else if constexpr (std::is_same_v<T, bool>)
            out_ << (value ? 1 : 0) << ' ';
        else
            out_ << value << ' ';
        return *this;
    }
    /** Append a point as two tokens. */
    Writer &operator<<(Point point) {
        return *this << point.x << point.y;
    }
    /** Read the written text. */
    std::string text() const {
        return out_.str();
    }

  private:
    std::ostringstream out_;
};

/** Token reader that records the first malformed or out-of-range value and then reads zeros. */
class Reader {
  public:
    explicit Reader(const std::string &text) : in_(text) {}
    /** Read an integer within an inclusive range. */
    int integer(int minimum = std::numeric_limits<int>::min(),
                int maximum = std::numeric_limits<int>::max()) {
        long long value = 0;
        if (!ok_ || !(in_ >> value) || value < minimum || value > maximum) {
            ok_ = false;
            return 0;
        }
        return static_cast<int>(value);
    }
    /** Read an unsigned 32-bit value. */
    std::uint32_t word() {
        long long value = 0;
        if (!ok_ || !(in_ >> value) || value < 0 || value > 0xffffffffLL) {
            ok_ = false;
            return 0;
        }
        return static_cast<std::uint32_t>(value);
    }
    /** Read a finite number within an inclusive range. */
    float number(float minimum = -1e6F, float maximum = 1e6F) {
        double value = 0;
        if (!ok_ || !(in_ >> value) || !std::isfinite(value) || value < minimum || value > maximum) {
            ok_ = false;
            return 0;
        }
        return static_cast<float>(value);
    }
    /** Read a boolean stored as 0 or 1. */
    bool flag() {
        return integer(0, 1) == 1;
    }
    /** Read an enum stored as its index, accepting indices below `count`. */
    template <typename T> T choice(int count) {
        return static_cast<T>(integer(0, count - 1));
    }
    /** Read a point. */
    Point point() {
        const float x = number();
        return {x, number()};
    }
    /** Read a list length. */
    int count() {
        return integer(0, MaxItems);
    }
    /** Read a literal token. */
    void expect(const std::string &token) {
        std::string value;
        if (!ok_ || !(in_ >> value) || value != token)
            ok_ = false;
    }
    /** Check that every token was valid and nothing follows the last one. */
    bool finished() {
        std::string rest;
        return ok_ && !(in_ >> rest);
    }
    bool ok() const {
        return ok_;
    }

  private:
    std::istringstream in_;
    bool ok_ = true;
};

/** Write an optional card offer. */
void writeOffer(Writer &out, const std::optional<CardOffer> &offer) {
    out << offer.has_value();
    if (!offer)
        return;
    out << offer->id << offer->source << static_cast<int>(offer->candidates.size());
    for (CardKind kind : offer->candidates)
        out << kind;
    out << offer->cost.iron << offer->cost.copper << offer->cost.crystal << offer->cost.gold << offer->shards;
}
/** Read an optional card offer. */
std::optional<CardOffer> readOffer(Reader &in) {
    if (!in.flag())
        return std::nullopt;
    CardOffer offer;
    offer.id = static_cast<OfferId>(in.integer(1));
    offer.source = in.choice<OfferSource>(3);
    const int candidates = in.integer(1, 8);
    for (int index = 0; index < candidates; ++index)
        offer.candidates.push_back(in.choice<CardKind>(CardKindCount));
    offer.cost = {in.integer(0, CatalogLimits::MaxAmount), in.integer(0, CatalogLimits::MaxAmount),
                  in.integer(0, CatalogLimits::MaxAmount), in.integer(0, CatalogLimits::MaxAmount)};
    offer.shards = in.integer(0, CatalogLimits::MaxAmount);
    return offer;
}
/** Write a list of points with its length. */
void writePoints(Writer &out, const std::vector<Point> &points) {
    out << static_cast<int>(points.size());
    for (Point point : points)
        out << point;
}
/** Read a list of points with its length. */
std::vector<Point> readPoints(Reader &in) {
    std::vector<Point> points(static_cast<std::size_t>(in.count()));
    for (Point &point : points)
        point = in.point();
    return points;
}
/** Check that a point is one of the allowed positions. */
bool among(Point point, const std::vector<Point> &allowed) {
    return std::any_of(allowed.begin(), allowed.end(),
                       [point](Point other) { return distance(point, other) < .01F; });
}
} // namespace

std::string Run::snapshot() const {
    if ((state_.phase != Phase::Ready && state_.phase != Phase::Prep) || state_.job.active)
        return {};
    Writer out;
    out << SnapshotTag << SnapshotVersion << catalog_.mining << catalog_.mineSeed
        << static_cast<int>(catalog_.waves.size());
    out << state_.phase << state_.wave << state_.baseHp << state_.energy << state_.slots << state_.gold
        << state_.iron << state_.copper << state_.crystal << state_.tool << state_.digs << state_.scans
        << state_.nextExtra << state_.spawned << state_.defeated << state_.armor << state_.coil
        << state_.bonusEnergy << state_.hires << state_.supplied << state_.expansionPending << state_.branch
        << state_.battleTime << state_.energyTime << state_.minerCell << state_.mineLayer;
    out << static_cast<int>(state_.relics.size());
    for (Relic relic : state_.relics)
        out << relic;
    writeOffer(out, state_.recruitmentOffer);
    writeOffer(out, state_.rewardOffer);
    for (const Card &card : state_.deck)
        out << card.id << card.kind;
    for (int position : state_.hand)
        out << position;
    out << static_cast<int>(state_.queue.size());
    for (int position : state_.queue)
        out << position;
    out << state_.mineWidth << state_.mineHeight << static_cast<int>(state_.mine.size());
    for (const MineCell &cell : state_.mine)
        out << cell.ore << cell.amount << cell.grade << cell.nest << cell.clue << cell.dug << cell.info
            << cell.clueKnown << cell.flagged << cell.special << cell.relic << cell.card << cell.rubble
            << cell.cover << cell.deposit;
    out << static_cast<int>(state_.deposits.size());
    for (const Deposit &deposit : state_.deposits) {
        out << deposit.ore << deposit.amount << deposit.special << deposit.relic << deposit.card
            << deposit.nest << deposit.grade << deposit.collected << deposit.woken
            << static_cast<int>(deposit.cells.size());
        for (int cell : deposit.cells)
            out << cell;
    }
    out << static_cast<int>(state_.towers.size());
    for (const Tower &tower : state_.towers)
        out << tower.id << tower.kind << tower.position << tower.level << tower.hp << tower.maxHp;
    out << static_cast<int>(state_.units.size());
    for (const Unit &unit : state_.units)
        out << unit.id << unit.kind << homeOf(unit) << unit.hp << unit.maxHp << unit.hero << unit.level;
    writePoints(out, state_.pads);
    writePoints(out, state_.unitCells);
    writePoints(out, state_.boulders);
    out << state_.heroCell << state_.shards << state_.spoils << state_.rerolls << state_.heroLevel;
    for (std::size_t fork = 0; fork < catalog_.forks.size(); ++fork)
        out << detoured(fork);
    out << nextEntity_ << nextCard_ << nextOffer_ << static_cast<int>(acquisitions_.size());
    for (CardId id : acquisitions_)
        out << id;
    out << "END";
    return out.text();
}

Result Run::restore(const std::string &text) {
    const Result rejected{false, Message("rules.snapshot.rejected")};
    if (state_.phase != Phase::Ready || state_.wave != 0)
        return {false, Message("rules.snapshot.not_fresh")};
    Reader in(text);
    in.expect(SnapshotTag);
    if (in.integer() != SnapshotVersion || in.choice<MiningMode>(2) != catalog_.mining ||
        in.word() != catalog_.mineSeed || in.integer() != static_cast<int>(catalog_.waves.size()))
        return rejected;
    const RunBalance &balance = catalog_.balance;
    const int waves = static_cast<int>(catalog_.waves.size());
    const int amount = CatalogLimits::MaxAmount;
    RunState next = state_;
    next.phase = in.choice<Phase>(3);
    next.wave = in.integer(0, waves - 1);
    next.baseHp = in.integer(1, amount);
    next.energy = in.integer(0, balance.energyCap);
    next.slots = in.integer(1, CatalogLimits::MaxSlots);
    next.gold = in.integer(0, amount);
    next.iron = in.integer(0, amount);
    next.copper = in.integer(0, amount);
    next.crystal = in.integer(0, amount);
    next.tool = in.integer(1, 3);
    next.digs = in.integer(0, 1000);
    next.scans = in.integer(0, 1000);
    next.nextExtra = in.integer(0, amount);
    next.spawned = in.integer(0, amount);
    next.defeated = in.integer(0, amount);
    next.armor = in.flag();
    next.coil = in.flag();
    next.bonusEnergy = in.integer(0, amount);
    next.hires = in.integer(0, 100);
    next.supplied = in.flag();
    next.expansionPending = in.flag();
    next.branch = in.integer(0, 2);
    next.battleTime = in.number(0, 100000);
    next.energyTime = in.number(0, 1000);
    next.minerCell = in.integer(0, MaxItems);
    next.mineLayer = in.integer(0, static_cast<int>(catalog_.deeperMines.size()));
    next.relics.assign(static_cast<std::size_t>(in.integer(0, RelicCount)), Relic::Battery);
    for (Relic &relic : next.relics)
        relic = in.choice<Relic>(RelicCount);
    next.recruitmentOffer = readOffer(in);
    next.rewardOffer = readOffer(in);
    for (Card &card : next.deck) {
        card.id = static_cast<CardId>(in.integer(1));
        card.kind = in.choice<CardKind>(CardKindCount);
    }
    for (int &position : next.hand)
        position = in.integer(0, 7);
    next.queue.assign(static_cast<std::size_t>(in.integer(0, 8)), 0);
    for (int &position : next.queue)
        position = in.integer(0, 7);
    next.mineWidth = in.integer(1, 64);
    next.mineHeight = in.integer(1, 64);
    next.mine.assign(static_cast<std::size_t>(in.count()), MineCell{});
    for (MineCell &cell : next.mine) {
        cell.ore = in.choice<Ore>(5);
        cell.amount = in.integer(0, amount);
        cell.grade = in.integer(1, 3);
        cell.nest = in.flag();
        cell.clue = in.integer(0, 8);
        cell.dug = in.flag();
        cell.info = in.flag();
        cell.clueKnown = in.flag();
        cell.flagged = in.flag();
        cell.special = in.choice<MineSpecial>(5);
        cell.relic = in.choice<Relic>(RelicCount);
        cell.card = in.choice<CardKind>(CardKindCount);
        cell.rubble = in.flag();
        cell.cover = in.integer(0, 16);
        cell.deposit = in.integer(0, MaxItems);
    }
    next.deposits.assign(static_cast<std::size_t>(in.count()), Deposit{});
    for (Deposit &deposit : next.deposits) {
        deposit.ore = in.choice<Ore>(5);
        deposit.amount = in.integer(0, amount);
        deposit.special = in.choice<MineSpecial>(5);
        deposit.relic = in.choice<Relic>(RelicCount);
        deposit.card = in.choice<CardKind>(CardKindCount);
        deposit.nest = in.flag();
        deposit.grade = in.integer(1, 3);
        deposit.collected = in.flag();
        deposit.woken = in.flag();
        deposit.cells.assign(static_cast<std::size_t>(in.integer(1, 16)), 0);
        for (int &cell : deposit.cells)
            cell = in.integer(0, MaxItems);
    }
    next.towers.assign(static_cast<std::size_t>(in.count()), Tower{});
    for (Tower &tower : next.towers) {
        tower.id = static_cast<EntityId>(in.integer(1));
        tower.kind = in.choice<CardKind>(CardKindCount);
        tower.position = in.point();
        tower.level = in.integer(1, 10);
        tower.hp = in.integer(1, amount);
        tower.maxHp = in.integer(tower.hp, amount);
        tower.cooldown = 0;
        tower.disabled = 0;
    }
    next.units.assign(static_cast<std::size_t>(in.count()), Unit{0, CardKind::Guard, {}, 1, 1});
    for (Unit &unit : next.units) {
        unit.id = static_cast<EntityId>(in.integer(1));
        unit.kind = in.choice<CardKind>(CardKindCount);
        unit.position = in.point();
        unit.hp = in.integer(1, amount);
        unit.maxHp = in.integer(unit.hp, amount);
        unit.hero = in.flag();
        unit.level = in.integer(1, 10);
        unit.cooldown = 0;
    }
    next.pads = readPoints(in);
    next.unitCells = readPoints(in);
    next.boulders = readPoints(in);
    next.heroCell = in.point();
    next.shards = in.integer(0, CatalogLimits::MaxAmount);
    next.spoils = in.integer(0, CatalogLimits::MaxAmount);
    next.rerolls = in.integer(0, CatalogLimits::MaxAmount);
    next.heroLevel = in.integer(1, 10);
    next.detours.assign(catalog_.forks.size(), 0);
    for (int &detour : next.detours)
        detour = in.flag() ? 1 : 0;
    const EntityId nextEntity = static_cast<EntityId>(in.integer(1));
    const CardId nextCard = static_cast<CardId>(in.integer(1));
    const OfferId nextOffer = static_cast<OfferId>(in.integer(1));
    std::vector<CardId> acquisitions(static_cast<std::size_t>(in.count()));
    for (CardId &id : acquisitions)
        id = static_cast<CardId>(in.integer(1));
    in.expect("END");
    if (!in.finished() || next.phase == Phase::Battle || (next.phase == Phase::Ready && next.wave != 0))
        return rejected;

    // The cutaway must be the authored layer it claims to be.
    const MineLayerDefinition &layer =
        next.mineLayer == 0 ? catalog_.mine
                            : catalog_.deeperMines[static_cast<std::size_t>(next.mineLayer - 1)];
    if (next.mineWidth != layer.width || next.mineHeight != layer.height ||
        static_cast<int>(next.mine.size()) != layer.width * layer.height ||
        next.minerCell >= static_cast<int>(next.mine.size()))
        return rejected;
    for (const MineCell &cell : next.mine)
        if (cell.deposit > static_cast<int>(next.deposits.size()))
            return rejected;
    for (const Deposit &deposit : next.deposits)
        for (int cell : deposit.cells)
            if (cell >= static_cast<int>(next.mine.size()))
                return rejected;
    if (catalog_.mining == MiningMode::Sweep && !next.deposits.empty())
        return rejected;

    // Hand and draw queue hold each deck position exactly once.
    std::set<int> positions(next.hand.begin(), next.hand.end());
    positions.insert(next.queue.begin(), next.queue.end());
    if (positions.size() != next.deck.size() || next.queue.size() != 4)
        return rejected;
    std::set<CardId> cards;
    for (const Card &card : next.deck)
        if (card.id >= nextCard || !cards.insert(card.id).second)
            return rejected;
    for (CardId id : acquisitions)
        if (id >= nextCard)
            return rejected;
    for (const std::optional<CardOffer> *offer : {&next.recruitmentOffer, &next.rewardOffer})
        if (*offer && (*offer)->id >= nextOffer)
            return rejected;

    // Placements must sit on positions this stage can offer: authored or expansion cells and cleared
    // boulders.
    std::vector<Point> padSpots = catalog_.pads, unitSpots = catalog_.unitCells;
    padSpots.insert(padSpots.end(), catalog_.boulders.begin(), catalog_.boulders.end());
    if (next.branch > 0) {
        const ExpansionDefinition &expansion = catalog_.expansions[static_cast<std::size_t>(next.branch - 1)];
        padSpots.insert(padSpots.end(), expansion.pads.begin(), expansion.pads.end());
        unitSpots.insert(unitSpots.end(), expansion.unitCells.begin(), expansion.unitCells.end());
    }
    for (Point point : next.pads)
        if (!among(point, padSpots))
            return rejected;
    for (Point point : next.unitCells)
        if (!among(point, unitSpots))
            return rejected;
    for (Point point : next.boulders)
        if (!among(point, catalog_.boulders) || among(point, next.pads))
            return rejected;
    std::set<EntityId> entities;
    for (const Tower &tower : next.towers)
        if (!catalog_.isBuilding(tower.kind) || !among(tower.position, next.pads) || tower.id >= nextEntity ||
            !entities.insert(tower.id).second)
            return rejected;
    for (const Unit &unit : next.units)
        if (!catalog_.isUnit(unit.kind) || !among(unit.position, next.unitCells) || unit.id >= nextEntity ||
            !entities.insert(unit.id).second)
            return rejected;

    // The route is rebuilt from the authored path and the chosen expansion rather than trusted.
    next.route = catalog_.route;
    if (next.branch > 0) {
        const ExpansionDefinition &expansion = catalog_.expansions[static_cast<std::size_t>(next.branch - 1)];
        next.route.insert(next.route.begin(), expansion.prefix.begin(), expansion.prefix.end());
    }
    next.job = {};
    next.enemies.clear();
    next.fields.clear();
    state_ = std::move(next);
    nextEntity_ = nextEntity;
    nextCard_ = nextCard;
    nextOffer_ = nextOffer;
    acquisitions_ = std::move(acquisitions);
    spawns_.clear();
    nextSpawn_ = 0;
    combatEvents_.clear();
    resolveTunnels();
    return {true, state_.phase == Phase::Ready
                      ? Message("rules.snapshot.resumed.start")
                      : Message("rules.snapshot.resumed.prep", {{"wave", std::to_string(state_.wave)}})};
}
} // namespace thorium::game
