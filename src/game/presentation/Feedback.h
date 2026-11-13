#pragma once
#include "game/rules/Run.h"
#include <array>
#include <cstdint>
#include <vector>

namespace thorium::presentation {
/** One hand-slot replacement, including visually identical card definitions. */
struct CardTransition {
    game::Card outgoing;
    game::Card incoming;
    float age = 1.0F;
};
/** A visual lifetime attached to an already committed combat fact. */
struct CombatCue {
    game::CombatEvent event;
    float age = 0.0F;
    float lifetime = 0.65F;
    /** Seconds since the cue first appeared; a damage cue that absorbs later hits keeps counting. */
    float span = 0.0F;
};
/** A visual lifetime attached to one completed excavation. */
struct MineCue {
    int cell = -1;
    game::Ore ore = game::Ore::Rock;
    int amount = 0;
    bool nest = false;
    int extra = 0;
    float age = 0.0F;
    float lifetime = 1.4F;
    game::MineSpecial special = game::MineSpecial::None;
    game::Relic relic = game::Relic::Battery;
    game::CardKind card = game::CardKind::Gun;
    /** Opened by a neighbouring cave-in; drawn as dust without a resource label. */
    bool rubble = false;
    /** Tiles opened by this cave-in, for an unstable-rock cue. */
    int collapsed = 0;
    /** Rock layers an excavation strike removed from the cell; such a cue only shows dust. */
    int stripped = 0;
    /** An excavation deposit was collected at the cell. */
    bool collected = false;
};
/** A modal-free announcement of a newly excavated relic. */
struct RelicBanner {
    game::Relic relic = game::Relic::Battery;
    float age = 0.0F;
    float lifetime = 3.4F;
    bool active = false;
};
/** Own disposable animation state; never changes costs, targeting, or simulation time. */
class Feedback {
  public:
    static constexpr float CardDuration = 0.85F;
    static constexpr float CardExit = 0.16F;
    static constexpr float CardArrival = 0.38F;
    /** A hit on the same subject joins its damage cue when the previous hit is at most DamageMergeGap
     *  seconds old and the cue appeared at most DamageMergeSpan seconds ago. */
    static constexpr float DamageMergeGap = 0.35F;
    static constexpr float DamageMergeSpan = 1.0F;
    std::array<CardTransition, 4> cards{};
    std::vector<CombatCue> cues;
    std::vector<MineCue> mineCues;
    RelicBanner relicBanner;
    std::uint64_t handChanges = 0;
    std::uint64_t hits = 0;
    game::CardId rejectedCard = 0;
    float rejectionAge = 1.0F;
    float energyAge = 1.0F;
    int energySpent = 0;
    float screenShake = 0.0F;
    /** Compare stable card identities, keeping a copy of the outgoing face for its exit. */
    void observeHand(const game::Run &run);
    /** Deal the visible hand after preparation, including recently acquired cards. */
    void revealHand(const game::Run &run);
    /** Finish card motion immediately when the user requests reduced animation. */
    void settleCards();
    /** Accept committed facts once, fold recent hits on one subject into one damage total, and bound the
     *  number of concurrent visual cues. */
    void consume(const std::vector<game::CombatEvent> &events);
    /** Advance UI in real seconds; paused combat keeps its visual cues frozen. */
    void advance(float seconds, bool combatActive);
    /** Compare excavation flags with the previous frame and add one cue per newly dug cell. */
    void observeMine(const game::Run &run);
    /** Compare an excavation wall with the previous frame: dust for stripped rock, and one cue per collected
     * deposit or woken nest. */
    void observeWall(const game::Run &run);
    /** Mark a failed card action without creating a successful draw animation. */
    void reject(game::CardId card);
    /** Block a new face only until it has visibly arrived at its stable hit rectangle. */
    bool cardBusy(int slot) const;
    /** Read a short hit intensity for an existing subject, including enemy/ally identity. */
    float hitIntensity(game::EntityId id, bool enemy) const;
    /** Read the latest shot's normalized recoil displacement in world coordinates. */
    game::Point recoil(game::EntityId id) const;

  private:
    bool initialized_ = false;
    int previousEnergy_ = 6;
    std::vector<bool> previousDug_;
    std::vector<int> previousCover_;
    std::vector<bool> previousCollected_;
    std::vector<bool> previousWoken_;
    int previousLayer_ = -1;
    std::size_t previousRelics_ = 0;
};
} // namespace thorium::presentation
