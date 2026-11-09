#pragma once
#include "game/content/CatalogValidation.h"
#include "game/model/PlayPreview.h"
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
    /** Reveal one reachable tile without digging or granting its mineral. */
    Result scan(int cell);
    /** Determine whether an undug tile borders the connected open gallery. */
    bool reachable(int cell) const;
    /** Purchase a tool, armor, coil, or slot upgrade in preparation. */
    Result upgrade(UpgradeKind kind);
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
    float routeLength() const;
    /** Read a position along the current cardinal route. */
    Point routePoint(float progress) const;
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
    /** Advance the one miner job and commit its result once. */
    void advanceMining(float delta);
    /** Execute movement, blocking, simultaneous attacks, and terminal ordering. */
    void advanceBattle(float delta);
    /** Finish a wave only after scheduled and live enemies are exhausted. */
    void finishWave();
    /** Commit a validated deck replacement in place and record its new identity. */
    Result replaceCard(CardKind kind, CardId replace);
};
/** Separate a failed validation report from ownership of a playable run. */
struct RunCreation {
    std::unique_ptr<Run> run;
    CatalogValidation validation;
};
} // namespace thorium::game
