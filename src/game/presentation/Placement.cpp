#include "CardPresentation.h"
#include "Theme.h"
#include "View.h"
#include <algorithm>
#include <cmath>

namespace thorium::presentation {
void View::preparePlacement(const game::Run &run, const engine::Ui &ui, const State &state, bool mineView) {
    placement_.reset();
    placingCard_ = 0;
    if (!state.app.navigation.gameplay() || mineView || state.play.paused ||
        state.play.modal != Modal::None || run.state().expansionPending || run.state().rewardOffer)
        return;
    const game::CardId id = ui.dragging && ui.captured > CardBase && ui.captured < DeckBase
                                ? ui.captured - CardBase
                                : state.play.selectedCard;
    const game::Card *card = run.card(id);
    if (!card)
        return;
    placingCard_ = id;
    if (!layout.world.contains(ui.pointerPosition))
        return;
    const game::Point target = layout.surfacePoint(ui.pointerPosition, run.catalog().isBuilding(card->kind) ||
                                                                           run.catalog().isUnit(card->kind));
    placement_ = PlacementPreview{id, target, run.previewPlay(id, target), ui.pointerPosition,
                                  run.catalog().card(card->kind).range};
}
void View::placementHint(engine::Renderer &graphics) const {
    if (!placement_)
        return;
    const game::PlayPreview &preview = placement_->rule;
    const engine::Color color = preview.allowed() ? theme::green : theme::red;
    const engine::ScreenPoint center = layout.screenPoint(placement_->target);
    std::vector<engine::ScreenPoint> ring;
    ring.reserve(33);
    for (int step = 0; step <= 32; ++step) {
        const float angle = step * 6.283185307F / 32;
        ring.push_back({center.x + std::cos(angle) * layout.tile * .42F,
                        center.y + std::sin(angle) * layout.tile * .42F});
    }
    graphics.beginClip(layout.world);
    graphics.line(ring, layout.tile * .08F, color);
    graphics.endClip();
    const float width = std::min(330.0F, layout.world.w - 16);
    const float height = preview.allowed() ? 32.0F : 52.0F;
    const float preferredY = placement_->pointer.y - height - 22 < layout.world.y + 35
                                 ? placement_->pointer.y + 22
                                 : placement_->pointer.y - height - 22;
    const engine::Rect hint{
        std::clamp(placement_->pointer.x - width / 2, layout.world.x + 8,
                   layout.world.x + layout.world.w - width - 8),
        std::clamp(preferredY, layout.world.y + 35, layout.world.y + layout.world.h - height - 8), width,
        height};
    graphics.rect(hint, {12, 28, 35, 245}, 6);
    graphics.rect({hint.x, hint.y, 3, hint.h}, color, 1);
    const std::string title =
        std::string(playActionName(preview.action)) + " · 에너지 " + std::to_string(preview.cost);
    graphics.text(title, {hint.x + 12, hint.y + 9}, 12, color, 0, hint.w - 22);
    if (!preview.allowed())
        graphics.text(preview.message(), {hint.x + 12, hint.y + 30}, 11, theme::text, 0, hint.w - 22);
}
} // namespace thorium::presentation
