#pragma once
#include "game/content/CatalogValidation.h"
#include "game/model/PlayPreview.h"
#include "game/model/Purchase.h"
#include "game/model/RunState.h"
#include <deque>
#include <memory>

namespace thorium::game {
struct RunCreation;
/** Own one run and its atomic commands; no SDL, UI, or ThorVG dependency. */
class Run {
  public:
    /** Create a fully reset, reproducible starter run. */
    Run();
    /** Validate authored content atomically; failed candidates never construct a run. */
    static RunCreation create(Catalog catalog);
    /** Reset only live run state while retaining the already validated immutable catalog. */
    void reset();
    /** Read the live snapshot without granting mutation access to UI or platform code. */
    const RunState &state() const {
        return state_;
    }
    /** Read this run's immutable content snapshot, including balance and initial layout. */
    const Catalog &catalog() const {
        return catalog_;
    }
    /** Start one manually confirmed wave, preserving surviving field entities. */
    Result startWave();
    /** Query one logical target without spending, cycling, allocating identities or emitting facts. */
    PlayPreview previewPlay(CardId card, Point target) const;
    /** Query whether any legal target is affordable, including cheaper existing-tower upgrades. */
    PlayAvailability playAvailability(CardId card) const;
    /** Revalidate the current snapshot and play one current hand instance at a logical target. */
    Result play(CardId card, Point target);
    /** Spend one energy to circulate a hand instance without an effect. */
    Result exchange(CardId card);
    /** Move simulation by one fixed delta; presentation clocks never grant rewards. */
    void advance(float delta);
    /** Drain optional combat facts once; not reading them never blocks simulation. */
    std::vector<CombatEvent> takeCombatEvents();
    /** Queue one reachable boundary tile for automatic movement and digging. */
    Result dig(int cell);
    /** Reveal one reachable tile without digging or granting its mineral; in an excavation, show what lies
     * under the block of tiles around any wall tile. */
    Result scan(int cell);
    /** Read the tiles an excavation strike would reach and the rock layers it would strip from each, without
     * changing anything; empty outside an excavation or for a tile outside the wall. */
    std::vector<StrikeHit> strikePattern(int cell, StrikeTool tool) const;
    /** Read how many digs one excavation strike with a tool costs. */
    int strikeCost(StrikeTool tool) const;
    /** Strike an excavation wall tile: strip rock, wake nests a blast touches, and collect every deposit
     * that becomes fully exposed. */
    Result strike(int cell, StrikeTool tool);
    /** Count the active wall's deposits that are not collected yet; zero outside an excavation. */
    int buriedDeposits() const;
    /** Enter an unlocked deeper cutaway, preserving inventory, equipment, digs and scans. */
    Result descend();
    /** Explain whether the next cutaway can be entered without modifying the current layer. */
    Result descentAvailability() const;
    /** Determine whether an undug tile borders the connected open gallery; in an excavation, whether a
     * strike may target the tile. */
    bool reachable(int cell) const;
    /** Check whether this run has already excavated a relic. */
    bool hasRelic(Relic relic) const;
    /** Read the enemies one excavated nest adds to the next wave, after relic reductions. */
    int nestReinforcements() const;
    /** Read the dig budget granted at the start of each preparation, including relic and drill bonuses. */
    int prepDigBudget() const;
    /** Read the free scans granted at the start of each preparation, including relic bonuses. */
    int prepScanBudget() const;
    /** Read the energy the next battle starts with, including stored bonuses and the battery relic. */
    int nextBattleEnergy() const;
    /** Price and check one preparation purchase without paying or changing any state. */
    PurchaseQuote quote(UpgradeKind kind) const;
    /** Make one preparation purchase: tool, drill, armor, coil, slots, hired dig or supply crate. */
    Result upgrade(UpgradeKind kind);
    /** Price and check raising one placed building a level above the card maximum, without paying. */
    PurchaseQuote awakenQuote(EntityId tower) const;
    /** Spend crystal and copper in preparation to raise one placed building a level above the card maximum.
     */
    Result awaken(EntityId tower);
    /** Read the highest level awakening can reach. */
    int awakenedLevel() const;
    /** Heal one living unit with iron and copper. */
    Result heal(EntityId id);
    /** Move or swap allies in preparation without changing health or identity. */
    Result moveUnit(EntityId id, Point target);
    /** Withdraw a placed unit or tower during battle/prep, with no refund. */
    Result dismiss(EntityId id);
    /** Resolve an active offer by run-scoped identity without granting mutation access. */
    const CardOffer *offer(OfferId id) const;
    /** Commit one offered candidate and replacement; consume the offer and price exactly once. */
    Result acceptOffer(OfferId offer, CardKind candidate, CardId replace);
    /** Skip only an active expansion reward; dialog cancellation never consumes recruitment. */
    Result skipOffer(OfferId offer);
    /** Apply one surface branch after wave two. */
    Result expand(int choice);
    /** Read a unit's current state by stable identity. */
    const Unit *unit(EntityId id) const;
    /** Read a placed building by stable identity. */
    const Tower *tower(EntityId id) const;
    /** Read total scheduled enemies for the current wave, including nest additions. */
    int waveSpawnCount() const;
    /** Read how many scheduled enemies have entered this wave. */
    int waveSpawned() const;
    /** Read a current card instance by stable identity. */
    const Card *card(CardId id) const;
    /** Return an interpolated miner position for the active job. */
    Point minerPosition() const;
    /** Return the current route length for target and preview calculations. */
    float routeLength(std::uint32_t route = 0) const;
    /** Read a position along the current cardinal route. */
    Point routePoint(float progress, std::uint32_t route = 0) const;
    /** Read one current path; only the primary entrance is extended by surface expansion choices. */
    const std::vector<Point> &route(std::uint32_t index = 0) const;
    /** Read the exact current or next scheduled entrance pressure before committing a wave. */
    std::vector<LaneForecast> laneForecast() const;
    /** Read seconds until the next not-yet-spawned scheduled enemy of a kind, or -1 when none remains. */
    float nextArrival(EnemyKind kind) const;
    /** Read seconds until the next not-yet-spawned scheduled elite of a modifier, or -1 when none remains. */
    float nextElite(EliteAffix affix) const;
    /** Check whether an enemy is underground: untargetable, unblockable and immune to area effects. */
    bool burrowed(const Enemy &enemy) const;
    /** Read the underground spans in progress along the current routes, including a surface expansion's
     *  added length on the primary route. */
    const std::vector<TunnelSpan> &tunnels() const {
        return tunnels_;
    }
    /** Check whether a position along a current route lies inside a tunnel. */
    bool tunnelAt(float progress, std::uint32_t route) const;
    /** Check whether an enemy is inside a tunnel. */
    bool tunneled(const Enemy &enemy) const;
    /** Check whether an enemy is burrowed or inside a tunnel: untargetable, unblockable and immune to area
     *  effects. */
    bool underground(const Enemy &enemy) const;
    /** Read the terrain trait of the building pad at a position; None away from trait pads. */
    PadTrait padTrait(Point position) const;
    /** Read a building kind's attack or support radius when it stands at a position, including the pad's
     *  trait. */
    float buildingRange(CardKind kind, Point position) const;
    /** Read the damage multiplier the pad at a position grants the building standing on it. */
    float padDamage(Point position) const;
    /** Price and check clearing the boulder at a position, without paying or changing any state. */
    PurchaseQuote boulderQuote(Point position) const;
    /** Pay in preparation to clear the boulder at a position; its pad then accepts buildings. */
    Result clearBoulder(Point position);
    /** Read seconds until a vent's next eruption during a battle, or -1 outside battle and for an unknown
     *  vent. */
    float ventCountdown(std::size_t vent) const;
    /** Check whether a position lies inside the blast radius of any lava vent. */
    bool ventThreatens(Point position) const;
    /** Serialize everything a run keeps between waves, as text; empty during a battle, while the miner works
     * and after the run has ended. Hidden mine contents are included, so the text is save data and never
     * shown to the player. */
    std::string snapshot() const;
    /** Replace a new run's state with a snapshot taken from a run of the same catalog, mining mode and wall
     * seed. Every count, enum, placement and identity is checked against the catalog first; a rejected
     * snapshot leaves the run unchanged. */
    Result restore(const std::string &snapshot);
    /** Expose a small debug snapshot, excluding hidden mine contents. */
    std::string debugState() const;

  private:
    friend struct RunTestAccess;
    /** Ephemeral internal plan; only this run may produce and immediately execute it. */
    struct PlayPlan {
        PlayPreview preview;
        /** Authored cell center for buildings/units; unchanged fractional position for spells. */
        Point target;
        CardKind kind = CardKind::Gun;
        int handPosition = -1;
        int towerIndex = -1;
    };
    /** Resolve card, action, cost and every precondition through one read-only path. */
    PlayPlan planPlay(CardId card, Point target) const;
    /** Only factory-validated or shipped content may call the actual constructor. */
    struct Checked {};
    Run(Catalog catalog, Checked);
    Catalog catalog_;
    RunState state_;
    /** Check all recipe resources before committing any payment. */
    bool canPay(const ResourceCost &cost) const;
    /** Deduct a previously validated recipe exactly once. */
    void pay(const ResourceCost &cost);
    EntityId nextEntity_ = 1;
    CardId nextCard_ = 1;
    OfferId nextOffer_ = 1;
    std::vector<CardId> acquisitions_;
    std::vector<Spawn> spawns_;
    std::size_t nextSpawn_ = 0;
    std::deque<CombatEvent> combatEvents_;
    /** Bound unread optional events to 256, dropping oldest facts under backpressure. */
    void emit(CombatEvent event);
    /** Cycle exactly one hand position through the four-item draw queue. */
    void cycle(int handPosition);
    /** Rebuild the hand with the most recent still-active acquired instance. */
    void rebuildHand();
    /** Reveal only safe zero-region and boundary clues after a real excavation. */
    void revealZero(int origin);
    /** Initialize only the active cutaway, keeping one owner of tile state and fixed original clues. */
    void initializeMine(const MineLayerDefinition &layer);
    /** Advance the one miner job and commit its result once. */
    void advanceMining(float delta);
    /** Apply the one-time content of an excavated special tile. */
    void resolveSpecial(int cell);
    /** Open undug, non-nest, non-special cardinal neighbours as empty rubble without spending digs. */
    void collapse(int origin);
    /** Remove rock layers from one wall tile; thin rock shows its contents and bare rock counts as dug. */
    void strip(int cell, int power);
    /** Collect every fully exposed deposit, running cave-ins until nothing changes, and describe the gains.
     */
    std::string settleWall();
    /** Execute movement, blocking, simultaneous attacks, and terminal ordering. */
    void advanceBattle(float delta);
    /** Create a live enemy with authored health, shield, ability timer and elite modifier at a route
     * position. */
    Enemy makeEnemy(EnemyKind kind, std::uint32_t route, float progress, EliteAffix affix = EliteAffix::None);
    /** Trigger due heal, burrow, stomp and sabotage abilities; summoned enemies are appended to arrivals. */
    void advanceAbilities(float delta, std::vector<Enemy> &arrivals);
    /** Advance one sapper: pick a tower in reach, plant while standing still, then disable the tower. */
    void advanceSabotage(Enemy &sapper, float delta);
    /** Apply elite regeneration pulses and enrage thresholds. */
    void advanceElites(float delta);
    /** Indices of living, surfaced enemies whose elite modifier projects an aura on other enemies. */
    std::vector<std::size_t> auraSources() const;
    /** Movement multiplier granted to an enemy by the strongest speed aura among the sources. */
    float auraSpeed(const Enemy &enemy, const std::vector<std::size_t> &sources) const;
    /** Reduce one hit by armor unless electric, then by the strongest guarding aura among the sources. */
    int mitigate(const Enemy &enemy, int damage, bool electric,
                 const std::vector<std::size_t> &sources) const;
    /** Apply one already armor-reduced hit through the shield and return the displayed amount. */
    int damageEnemy(Enemy &enemy, int amount);
    /** Remove defeated enemies, emit their facts once and append split minions afterwards. */
    void removeDefeated();
    /** Finish a wave only after scheduled and live enemies are exhausted. */
    void finishWave();
    /** Commit a validated deck replacement in place and record its new identity. */
    Result replaceCard(CardKind kind, CardId replace);
    /** Tunnel spans in progress along the current routes. */
    std::vector<TunnelSpan> tunnels_;
    /** Rebuild the tunnel spans after the primary route is created or extended at its entrance. */
    void resolveTunnels();
};
/** Separate a failed validation report from ownership of a playable run. */
struct RunCreation {
    std::unique_ptr<Run> run;
    CatalogValidation validation;
};
} // namespace thorium::game
