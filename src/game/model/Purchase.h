#pragma once
#include "Types.h"

namespace thorium::game {
/** Highest building level reachable by playing a matching card; awakening raises a building beyond it. */
inline constexpr int CardMaxLevel = 3;
/** Stable rejection identities of preparation purchases, shared by panel states and commands. */
enum class PurchaseFailure {
    None,
    /** Outside preparation, or while an expansion or reward choice is open. */
    Closed,
    /** A one-time purchase the run already owns, or a building at its final awakening level. */
    Owned,
    /** This preparation's allowance is used up, or the effect would exceed its cap. */
    Exhausted,
    /** A prerequisite tool, deck card or building level is missing. */
    Requirement,
    /** The selected building does not exist. */
    InvalidTarget,
    /** The wallet lacks part of the price. */
    Shortage
};
/** Read-only answer for one purchase; the command recomputes it against the live state before paying. */
struct PurchaseQuote {
    PurchaseFailure failure = PurchaseFailure::Closed;
    /** Price of this purchase now, also reported for rejected purchases with a known price. */
    ResourceCost cost;
    /** Text returned by the command when rejected; empty when allowed. */
    std::string message;
    /** Read whether the same purchase could be committed against the current snapshot. */
    bool allowed() const {
        return failure == PurchaseFailure::None;
    }
    /** Read whether only resources are missing, so the purchase is otherwise open. */
    bool open() const {
        return failure == PurchaseFailure::None || failure == PurchaseFailure::Shortage;
    }
};
} // namespace thorium::game
