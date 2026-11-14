#pragma once
#include "Types.h"

namespace thorium::game {
/** Classify one proposed action without changing a card, resource or field entity. */
enum class PlayAction { None, Install, Upgrade, Replace, Summon, Cast };
/** Stable rejection identities shared by rule queries and execution. */
enum class PlayFailure {
    None,
    InvalidTarget,
    BattleRequired,
    CardNotInHand,
    BuildingPadRequired,
    MaximumLevel,
    UnitCellRequired,
    SlotsFull,
    OccupiedCell,
    OutsideBattlefield,
    EnergyShortage,
    /** The target is a building pad still blocked by a boulder. */
    PadBlocked
};
/** Read-only answer for one target; callers must not treat this value as a commit token. */
struct PlayPreview {
    PlayAction action = PlayAction::None;
    PlayFailure failure = PlayFailure::CardNotInHand;
    int cost = 0;
    /** Read whether the same action could be committed against the current snapshot. */
    bool allowed() const {
        return failure == PlayFailure::None;
    }
    /** Read the same explanation returned by a rejected execution. */
    Message message() const;
};
/** Summarize legal targets for a current hand card, independently of its screen position. */
struct PlayAvailability {
    bool allowed = false;
    bool hasTarget = false;
    int minimumCost = 0;
    int maximumCost = 0;
    PlayFailure failure = PlayFailure::CardNotInHand;
    /** Indicate that install/upgrade/replace targets have different effective prices. */
    bool variableCost() const {
        return minimumCost != maximumCost;
    }
};
} // namespace thorium::game
