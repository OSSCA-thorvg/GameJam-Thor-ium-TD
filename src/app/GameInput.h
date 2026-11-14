#pragma once
#include "game/model/Types.h"
#include "game/presentation/Actions.h"
#include <optional>

namespace thorium {
/** Typed UI target families; widget ranges are decoded only at this boundary. */
enum class InputKind { Control, Hand, Deck, Candidate };
/** One decoded game input, retaining pointer intent and a stable card identity. */
struct GameInput {
    InputKind kind;
    presentation::Action action;
    std::uint32_t payload;
    engine::Interaction::Kind interaction;
    engine::ScreenPoint position;
};
/** Reject unknown controls and unsupported candidate enums before rule dispatch. */
inline std::optional<GameInput> decodeInput(engine::Interaction interaction) {
    using namespace presentation;
    const engine::WidgetId id = interaction.widget;
    if (id == World || (id >= Start && id <= LastControl))
        return GameInput{InputKind::Control, static_cast<Action>(id), 0, interaction.kind,
                         interaction.position};
    if (id > CardBase && id < DeckBase)
        return GameInput{InputKind::Hand, World, id - CardBase, interaction.kind, interaction.position};
    if (id > DeckBase && id < CandidateBase)
        return GameInput{InputKind::Deck, World, id - DeckBase, interaction.kind, interaction.position};
    if (id >= CandidateBase && id < CandidateBase + game::CardKindCount)
        return GameInput{InputKind::Candidate, World, id - CandidateBase, interaction.kind,
                         interaction.position};
    if ((id >= LaneFocusBase && id < LaneFocusBase + 8) || id == MinimapFocus ||
        (id >= LanguageBase && id < LanguageBase + 16))
        return GameInput{InputKind::Control, static_cast<Action>(id), 0, interaction.kind,
                         interaction.position};
    return std::nullopt;
}
} // namespace thorium
