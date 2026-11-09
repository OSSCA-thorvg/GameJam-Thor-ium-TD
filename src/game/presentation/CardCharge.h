#pragma once
#include "game/rules/Run.h"
#include <algorithm>

namespace thorium::presentation {
/** Separate renewable energy shortfall from placement conditions that waiting cannot solve. */
enum class CardChargeMode { Ready, Charging, Blocked };
/** One rendered card's actual price and energy coverage; contains no independent clock. */
struct CardCharge {
    game::CardId card = 0;
    int cost = 0;
    CardChargeMode mode = CardChargeMode::Blocked;
    float fraction = 0;
};
/** Derive a continuous shared-energy fraction from the current rule answer and simulation clock. */
inline CardCharge cardCharge(const game::Run &run, game::CardId card, int cost, game::PlayFailure failure) {
    CardCharge result{card, cost};
    if (run.state().phase != game::Phase::Battle || failure == game::PlayFailure::None) {
        result.mode = CardChargeMode::Ready;
        result.fraction = 1;
    } else if (failure == game::PlayFailure::EnergyShortage && cost > run.state().energy && cost > 0 &&
               cost <= run.catalog().balance.energyCap) {
        const float next =
            std::clamp(run.state().energyTime / run.catalog().balance.energyInterval, 0.0F, 1.0F);
        result.mode = CardChargeMode::Charging;
        result.fraction = std::clamp((run.state().energy + next) / cost, 0.0F, 1.0F);
    }
    return result;
}
} // namespace thorium::presentation
