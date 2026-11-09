#pragma once
#include "State.h"
#include "game/model/Types.h"

namespace thorium::presentation {
/** Share restart eligibility between visible controls and execution; stale confirmations grant no authority.
 */
inline bool canConfirmNewRun(game::Phase phase, const State &state) {
    const Screen screen = state.app.navigation.current();
    if (screen != Screen::Home && screen != Screen::Game)
        return false;
    if (state.play.modal == Modal::NewRun)
        return true;
    const bool terminal = phase == game::Phase::Won || phase == game::Phase::Lost;
    return screen == Screen::Game && terminal && state.play.modal == Modal::None;
}
} // namespace thorium::presentation
