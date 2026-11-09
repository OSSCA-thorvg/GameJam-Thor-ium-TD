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
};
/** Own disposable animation state; never changes costs, targeting, or simulation time. */
class Feedback {
  public:
    static constexpr float CardDuration = 0.85F;
    static constexpr float CardExit = 0.16F;
    static constexpr float CardArrival = 0.38F;
    std::array<CardTransition, 4> cards{};
    std::vector<CombatCue> cues;
    std::uint64_t handChanges = 0;
    std::uint64_t hits = 0;
    game::CardId rejectedCard = 0;
    float rejectionAge = 1.0F;
    float energyAge = 1.0F;
    int energySpent = 0;
    /** Compare stable card identities, keeping a copy of the outgoing face for its exit. */
    void observeHand(const game::Run &run);
    /** Deal the visible hand after preparation, including recently acquired cards. */
    void revealHand(const game::Run &run);
    /** Finish card motion immediately when the user requests reduced animation. */
    void settleCards();
    /** Accept committed facts once and bound the number of concurrent visual cues. */
    void consume(const std::vector<game::CombatEvent> &events);
    /** Advance UI in real seconds; paused combat keeps its visual cues frozen. */
    void advance(float seconds, bool combatActive);
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
};
} // namespace thorium::presentation
