#pragma once

#include "Actions.h"
#include "CardCharge.h"
#include "CardDetails.h"
#include "Layout.h"
#include "PlacementPreview.h"
#include "Shell.h"
#include "State.h"
#include "engine/audio/Audio.h"
#include "engine/render/Renderer.h"
#include "engine/ui/Ui.h"
#include "game/rules/Run.h"
#include <string>

namespace thorium::presentation {

/** Compose game-specific screens using generic UI and ThorVG drawing tools. */
class View {
  public:
    Layout layout;
    engine::ScreenPoint shakeOffset{};
    /** Build and paint one responsive screen, registering its matching controls. */
    void draw(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run, const State &state,
              const engine::AudioSettings &audio);
    /** Convert a logical screen point to a rounded surface placement target. */
    game::Point surfacePoint(engine::ScreenPoint position, bool snap) const {
        return layout.surfacePoint(position, snap);
    }
    /** Resolve one cutaway tile, rejecting margins rather than clamping to the board. */
    int mineCell(engine::ScreenPoint position) const {
        return layout.mineCell(position);
    }
    /** Transform one logical surface point for effects and entity drawing. */
    engine::ScreenPoint screenPoint(game::Point point) const {
        engine::ScreenPoint p = layout.screenPoint(point);
        p.x += shakeOffset.x;
        p.y += shakeOffset.y;
        return p;
    }

    /** Read the last rendered hint for diagnostics without exposing a mutable command plan. */
    const std::optional<PlacementPreview> &currentPlacement() const {
        return placement_;
    }
    /** Read currently visible card information without granting mutation access. */
    const std::optional<CardInspection> &currentCardInspection() const {
        return inspection_;
    }
    /** Read the incoming card shown above replacement choices. */
    const std::optional<CardDetails> &currentOfferCard() const {
        return offerCard_;
    }
    /** Read actual rendered card prices and energy coverage for diagnostics. */
    const std::array<CardCharge, 4> &currentHandCharge() const {
        return handCharge_;
    }

  private:
    game::CardId placingCard_ = 0;
    std::optional<PlacementPreview> placement_;
    std::optional<CardInspection> inspection_;
    std::optional<CardDetails> offerCard_;
    engine::Rect offerDetailsBounds_;
    std::array<CardCharge, 4> handCharge_{};
    /** Paint a common portrait, role and base-stat panel in a caller-owned rectangle. */
    void cardDetailsPanel(engine::Renderer &graphics, const game::Catalog &catalog,
                          const CardDetails &details, engine::Rect bounds, bool compact);
    /** Resolve generic UI inspection to a current hand/deck/offer card and paint a bounded tooltip. */
    void cardInspection(engine::Renderer &graphics, const engine::Ui &ui, const game::Run &run);
    /** Paint recruitment or incoming-card comparison inside the modal shell. */
    void cardOfferOverlay(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run,
                          const State &state, engine::Rect bounds, bool replacement);
    /** Derive cursor intent once, respecting app-level input gates separately from rule queries. */
    void preparePlacement(const game::Run &run, const engine::Ui &ui, const State &state, bool mineView);
    /** Draw a clipped validity marker and a bounded effective-cost/rejection tooltip. */
    void placementHint(engine::Renderer &graphics) const;
    /** Draw the title, settings, guide or credits with the same native UI primitives. */
    void frontEnd(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run, const State &state,
                  const engine::AudioSettings &audio);
    /** Paint an original surface/mine diorama for the title screen. */
    void titleScene(engine::Renderer &graphics, const game::Catalog &catalog, engine::Rect bounds, float time,
                    const engine::AnimationClip &clip);
    /** Compute geometry once for the active viewport and gameplay phase. */
    void measure(int width, int height, bool prep, bool mine, bool tutorial = false);
    /** Draw the topdown world, placement affordances and independent animated enemies. */
    void surface(engine::Renderer &graphics, const game::Run &run, const State &state);
    /** Draw only revealed mine information and the miner's current job position. */
    void underground(engine::Renderer &graphics, const game::Run &run, const State &state);
    /** Draw a game card silhouette without requiring a texture atlas. */
    void icon(engine::Renderer &graphics, const game::Catalog &catalog, game::CardKind kind,
              engine::ScreenPoint center, float size);
    /** Draw/register a generic button with keyboard focus and an enabled state. */
    void button(engine::Renderer &graphics, engine::Ui &ui, engine::WidgetId id, engine::Rect bounds,
                const std::string &title, bool enabled = true, bool primary = false, bool danger = false);
    /** Paint a riveted metal panel, shared by game-specific HUD and dialogs. */
    void panel(engine::Renderer &graphics, engine::Rect bounds, bool brass = false);
    /** Draw the persistent reactor, unit, resource and wave rail; energy belongs below the hand. */
    void hud(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run, const State &state,
             const engine::AudioSettings &audio);
    /** Draw phase actions or the currently selected entity's inspector. */
    void commandPanel(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run, const State &state);
    /** Paint compact lesson instructions and checkpoint/navigation controls without covering the map. */
    void tutorialPanel(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run, const State &state);
    /** Resolve logical lesson targets against the current viewport, including cards and cutaway cells. */
    void tutorialHighlights(engine::Renderer &graphics, const engine::Ui &ui, const game::Run &run,
                            const State &state);
    /** Paint terminal practice actions after ordinary gameplay and modal layers. */
    void tutorialOverlay(engine::Renderer &graphics, engine::Ui &ui, const State &state);
    /** Draw selection details and withdrawal/move/heal actions inside a measured area. */
    void inspector(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run, const State &state,
                   engine::Rect area);
    /** Paint a tactile card deck with category frames and stable card hit regions. */
    void hand(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run, const State &state);
    /** Paint current energy, regeneration and spent-energy feedback directly below the four cards. */
    void handEnergy(engine::Renderer &graphics, const game::Run &run, const State &state);
    /** Share clipped attack/effect circles between field selection and pre-placement previews. */
    void rangeRing(engine::Renderer &graphics, engine::ScreenPoint center, float radius, engine::Color color,
                   bool filled);
    /** Paint one reusable card face; parent layers provide clipping and opacity. */
    void cardFace(engine::Renderer &graphics, const game::Catalog &catalog, const game::Card &card,
                  engine::Rect bounds, bool affordable, int displayedCost = -1, bool variableCost = false,
                  const CardCharge *charge = nullptr);
    /** Paint combat bursts, tracers and floating damage after world entities. */
    void combatFeedback(engine::Renderer &graphics, const State &state);
    /** Draw/register expansion, recruitment, replacement and confirmation dialogs. */
    void overlay(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run, const State &state,
                 const engine::AudioSettings &audio);
};
} // namespace thorium::presentation
