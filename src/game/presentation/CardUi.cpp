#include "CardPresentation.h"
#include "Theme.h"
#include "View.h"
#include <algorithm>

namespace thorium::presentation {
void View::cardDetailsPanel(engine::Renderer &g, const game::Catalog &catalog, const CardDetails &details,
                            engine::Rect box, bool compact) {
    const engine::Color accent = theme::accent(details.category);
    g.rect(box, {12, 28, 37, 250}, 9);
    g.rect({box.x, box.y + 8, 3, box.h - 16}, accent, 1);
    g.circle({box.x + 37, box.y + 36}, 27, theme::top);
    icon(g, catalog, details.kind, {box.x + 37, box.y + 34}, 43);
    g.text(details.name, {box.x + 73, box.y + 13}, compact ? 16 : 19, theme::text, 0, box.w - 85);
    g.text(tr(compact ? "cardui.panel.metaBase" : "cardui.panel.meta",
              {{"category", cardCategoryName(details.category)}, {"cost", std::to_string(details.cost)}}),
           {box.x + 73, box.y + 40}, 11, accent, 0, box.w - 85);
    g.text(details.description, {box.x + 12, box.y + 72}, compact ? 10 : 11, theme::text, 0, box.w - 24);
    const float start = compact ? 90.0F : 104.0F;
    const float rowHeight = compact ? 18.0F : 28.0F;
    const float gap = compact ? 1.0F : 5.0F;
    const float cellWidth = (box.w - 32) / 2;
    for (std::size_t index = 0; index < details.stats.size(); ++index) {
        const CardStat &stat = details.stats[index];
        const engine::Rect cell{box.x + 12 + (index % 2) * (cellWidth + 8),
                                box.y + start + (index / 2) * (rowHeight + gap), cellWidth, rowHeight};
        g.rect(cell, {43, 65, 74}, 3);
        g.text(stat.label, {cell.x + 5, cell.y + (compact ? 3 : 7)}, 10, theme::muted, 0, cell.w * .52F);
        g.text(stat.value, {cell.x + cell.w - 5, cell.y + (compact ? 2 : 6)}, compact ? 11 : 12, accent, 1,
               cell.w * .43F);
    }
    if (!compact) {
        g.text(tr("cardui.panel.base"), {box.x + 12, box.y + box.h - 33}, 9, theme::muted, 0, box.w - 24);
        g.text(tr("cardui.panel.hint"), {box.x + 12, box.y + box.h - 18}, 9, theme::muted, 0, box.w - 24);
    }
}
void View::cardInspection(engine::Renderer &g, const engine::Ui &ui, const game::Run &run) {
    inspection_.reset();
    const engine::WidgetId id = ui.inspected();
    if (!id)
        return;
    game::CardKind kind = game::CardKind::Gun;
    if (id > CardBase && id < CandidateBase) {
        const game::Card *card = run.card(id < DeckBase ? id - CardBase : id - DeckBase);
        if (!card)
            return;
        kind = card->kind;
    } else if (id >= CandidateBase && id < CandidateBase + game::CardKindCount)
        kind = static_cast<game::CardKind>(id - CandidateBase);
    else
        return;
    const engine::Widget *anchor = nullptr;
    for (const engine::Widget &widget : ui.widgets)
        if (widget.id == id)
            anchor = &widget;
    if (!anchor)
        return;
    float width = std::min(360.0F, g.width - 24.0F);
    const float height = 244;
    const float above = anchor->bounds.y - height - 12;
    float y = above >= 12 ? above : anchor->bounds.y + anchor->bounds.h + 12;
    float x = anchor->bounds.center().x - width / 2;
    if (id > DeckBase && id < CandidateBase && offerDetailsBounds_.h > 0) {
        const float beside = offerDetailsBounds_.x - 24;
        if (beside >= 240) {
            width = std::min(width, beside);
            x = 12;
            y = offerDetailsBounds_.y;
        } else
            y = offerDetailsBounds_.y + offerDetailsBounds_.h + 10;
    }
    const engine::Rect box{std::clamp(x, 12.0F, g.width - width - 12),
                           std::clamp(y, 12.0F, g.height - height - 12), width, height};
    inspection_ = CardInspection{id, cardDetails(run.catalog(), kind), box};
    g.rect({box.x - 2, box.y - 2, box.w + 4, box.h + 4}, theme::accent(inspection_->details.category), 10);
    cardDetailsPanel(g, run.catalog(), inspection_->details, box, false);
}
void View::cardOfferOverlay(engine::Renderer &g, engine::Ui &ui, const game::Run &run, const State &state,
                            engine::Rect box, bool replacement) {
    const float x = box.x + 16, w = box.w - 32;
    const game::CardOffer *active =
        run.state().rewardOffer ? &*run.state().rewardOffer : run.offer(state.play.selectedOffer);
    if (!active)
        return;
    if (replacement) {
        const bool reward = active->source != game::OfferSource::Recruitment;
        const game::CardKind incoming = reward ? active->candidates.front() : state.play.candidate;
        offerCard_ = cardDetails(run.catalog(), incoming);
        g.text(tr(active->source == game::OfferSource::Fossil ? "cardui.offer.fossil.title"
                  : reward                                    ? "cardui.offer.reward.title"
                                                              : "cardui.offer.recruit.title"),
               {x, box.y + 17}, 21, theme::gold, 0, w);
        g.text(active->source == game::OfferSource::Fossil ? tr("cardui.offer.fossil.note")
               : reward ? tr("cardui.offer.reward.note")
                        : tr("cardui.offer.recruit.note", {{"cost", costText(active->cost)}}),
               {x, box.y + 47}, 11, theme::muted, 0, w);
        offerDetailsBounds_ = {box.x + 12, box.y + 70, box.w - 24, 150};
        cardDetailsPanel(g, run.catalog(), *offerCard_, offerDetailsBounds_, true);
        g.text(tr("cardui.offer.pick"), {x, box.y + 233}, 11, theme::text, 0, w);
        const float gap = 8, cardWidth = (w - 3 * gap) / 4;
        for (int index = 0; index < 8; ++index) {
            const game::Card &card = run.state().deck[index];
            const game::CardDefinition &definition = run.catalog().card(card.kind);
            const engine::Rect cell{x + (index % 4) * (cardWidth + gap), box.y + 255 + (index / 4) * 108,
                                    cardWidth, 100};
            ui.add(DeckBase + card.id, cell, true, false, true, true);
            g.rect(cell, theme::accent(definition.category), 7);
            g.gradientRect({cell.x + 2, cell.y + 2, cell.w - 4, cell.h - 4}, {240, 226, 188}, {196, 176, 136},
                           5);
            icon(g, run.catalog(), card.kind, {cell.x + cell.w / 2, cell.y + 33}, 35);
            g.text(cardName(card.kind), {cell.x + cell.w / 2, cell.y + 62}, layout.compact ? 10 : 13,
                   theme::cardInk, .5F, cell.w - 8);
            g.text(tr("cardui.energy", {{"cost", std::to_string(definition.cost)}}),
                   {cell.x + cell.w / 2, cell.y + 83}, 9, theme::cardInk, .5F, cell.w - 8);
        }
        button(g, ui, run.state().rewardOffer ? RewardSkip : Close, {x, box.y + box.h - 54, w, 42},
               tr(run.state().rewardOffer ? "cardui.offer.skip" : "cardui.offer.cancel"));
        return;
    }
    g.text(tr("cardui.recruit.title"), {x, box.y + 20}, 25, theme::gold, 0, w);
    const int reroll = run.catalog().balance.rerollShards;
    button(g, ui, RerollOffer, {x + w - 140, box.y + 18, 140, 32},
           tr("cardui.recruit.reroll", {{"shards", std::to_string(reroll)}}), run.state().shards >= reroll);
    g.text(tr("cardui.recruit.choose", {{"count", std::to_string(active->candidates.size())}}),
           {x, box.y + 61}, 11, theme::muted, 0, w);
    const float gap = 10;
    const float count = static_cast<float>(active->candidates.size());
    const float cardWidth = (w - gap * (count - 1)) / count;
    for (std::size_t index = 0; index < active->candidates.size(); ++index) {
        const game::CardKind kind = active->candidates[index];
        const CardDetails details = cardDetails(run.catalog(), kind);
        const engine::Rect cell{x + index * (cardWidth + gap), box.y + 92, cardWidth, 212};
        ui.add(CandidateBase + static_cast<int>(kind), cell, true, false, true, true);
        g.rect(cell, theme::accent(details.category), 7);
        g.gradientRect({cell.x + 2, cell.y + 2, cell.w - 4, cell.h - 4}, {65, 100, 112}, {24, 48, 62}, 5);
        icon(g, run.catalog(), kind, {cell.x + cell.w / 2, cell.y + 43}, 48);
        g.text(details.name, {cell.x + cell.w / 2, cell.y + 85}, layout.compact ? 12 : 16, theme::text, .5F,
               cell.w - 8);
        g.text(tr("cardui.panel.meta",
                  {{"category", tr(run.catalog().isBuilding(kind) ? "cardrole.building" : "cardrole.unit")},
                   {"cost", std::to_string(details.cost)}}),
               {cell.x + cell.w / 2, cell.y + 108}, 11, theme::gold, .5F, cell.w - 8);
        for (std::size_t row = 0; row < details.stats.size() && row < 4; ++row) {
            const CardStat &stat = details.stats[row];
            g.text(stat.label + " " + stat.value, {cell.x + cell.w / 2, cell.y + 132 + row * 18},
                   layout.compact ? 10 : 12, theme::text, .5F, cell.w - 10);
        }
    }
    // Payment: gold or battle shards; the chosen one is lit and applies to the next replacement.
    const float half = (w - 8) / 2;
    const int shards = run.state().shards;
    button(
        g, ui, PayGold, {x, box.y + box.h - 104, half, 40},
        tr("cardui.pay.gold", {{"cost", costText(active->cost)}, {"gold", std::to_string(run.state().gold)}}),
        true, !state.play.payShards);
    button(g, ui, PayShards, {x + half + 8, box.y + box.h - 104, half, 40},
           tr("cardui.pay.shards",
              {{"shards", std::to_string(active->shards)}, {"owned", std::to_string(shards)}}),
           active->shards > 0, state.play.payShards);
    button(g, ui, Close, {x, box.y + box.h - 54, w, 42}, tr("cardui.recruit.later"));
}
} // namespace thorium::presentation
