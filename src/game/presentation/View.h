#pragma once

#include "Actions.h"
#include "CardCharge.h"
#include "CardDetails.h"
#include "Layout.h"
#include "PlacementPreview.h"
#include "Shell.h"
#include "State.h"
#include "TerrainArt.h"
#include "engine/audio/Audio.h"
#include "engine/render/Renderer.h"
#include "engine/ui/Ui.h"
#include "game/rules/Run.h"
#include <string>

namespace thorium::presentation {
/** Renderer cache slot of the battlefield ground drawn over the backdrop and moved by the screen shake. */
inline constexpr std::uint32_t TerrainLayer = 0;

/** One entrance marker: a badge beside a visible gate, or an edge arrow toward an off-screen one. */
struct LaneMarker {
    std::uint32_t lane = 0;
    bool visible = false;
    bool active = false;
    bool closed = false;
    /** Label bounds in screen space. */
    engine::Rect box;
    /** Gate center when visible; arrow base on the inner frame otherwise. */
    engine::ScreenPoint anchor;
    /** Unit direction from the battlefield center toward an off-screen gate. */
    engine::ScreenPoint direction;
    std::string label;
};
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
    /** Check whether anything drawn within `reach` of a screen point can show inside the battlefield frame.
     */
    bool inWorld(engine::ScreenPoint point, float reach) const {
        return point.x + reach >= layout.world.x && point.x - reach <= layout.world.x + layout.world.w &&
               point.y + reach >= layout.world.y && point.y - reach <= layout.world.y + layout.world.h;
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
    /** Tap area of the introduction cards painted this frame; empty when none are shown. */
    engine::Rect introToggle_;
    /** Excavation miner in wall cell units (row -1 is the ledge above the wall), the cells left on its walk,
     *  the strike it answers, and the seconds of swing left once it arrives. */
    game::Point wallMiner_{0, -1};
    std::vector<game::Point> wallPath_;
    std::uint32_t wallStrikes_ = 0;
    int wallGoal_ = -1;
    float wallSwing_ = 0;
    float wallClock_ = -1;
    /** Advance and paint the excavation miner, and the props it sets in opened rock. */
    void wallMiner(engine::Renderer &graphics, const game::Run &run, const State &state);
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
    /** Paint the title diorama: two lanes, three enemy roles and a boss silhouette over a tunnelled mine. */
    void titleScene(engine::Renderer &graphics, const game::Catalog &catalog, engine::Rect bounds, float time,
                    const AnimationSets &sets);
    /** Compute geometry once for the active viewport and gameplay phase. */
    void measure(int width, int height, bool prep, bool mine, bool tutorial, const game::Catalog &catalog,
                 engine::CameraPose camera, int mineColumns, int mineRows, float mineScroll);
    /** Choose the first-frame zoom of later campaign stages so tiles stay legible, centred on the base. */
    void frameOpening(const game::Run &run, const State &state);
    /** Paint the unshaken biome fill under the battlefield; it belongs to the retained backdrop. */
    void fieldGround(engine::Renderer &graphics, const State &state) const;
    /** Identify the biome fill so the backdrop is rasterized again when it changes. */
    int fieldKey(const State &state) const;
    /** Paint the biome ground features, roads and decorations kept clear of placements over the field fill.
     */
    void terrain(engine::Renderer &graphics, const game::Run &run, const State &state,
                 const std::vector<game::LaneForecast> &forecast);
    /** Paint the fixed terrain gimmicks into the retained ground: vent craters with their scorched radius,
     *  trait pad surrounds and remaining boulders. */
    void terrainFixtures(engine::Renderer &graphics, const game::Run &run);
    /** Paint every vent's blast radius as a thin ring over the roads. */
    void ventReaches(engine::Renderer &graphics, const game::Run &run);
    /** Paint the mouths and the covered section of every tunnel over the roads. */
    void tunnelCovers(engine::Renderer &graphics, const game::Run &run, const TunnelColors &colors);
    /** Draw bog water over the route spans that slow ground enemies. */
    void bogPools(engine::Renderer &graphics, const game::Run &run);
    /** Draw the cart rails laid along the routes. */
    void railTracks(engine::Renderer &graphics, const game::Run &run);
    /** Draw levers, running carts and gas tanks with their current battle state. */
    void machines(engine::Renderer &graphics, const game::Run &run, const State &state);
    /** Paint lane road edges and fills; idle lanes are dimmed. */
    void roads(engine::Renderer &graphics, const game::Run &run,
               const std::vector<game::LaneForecast> &forecast, engine::Color edge, engine::Color fill,
               engine::Color idleEdge, engine::Color idleFill);
    /** Paint travel chevrons and entrance gates over the roads; idle lanes are dimmed. */
    void roadMarks(engine::Renderer &graphics, const game::Run &run, const State &state,
                   const std::vector<game::LaneForecast> &forecast, engine::Color mark);
    /** Paint the defended base plaza for the current biome. */
    void basePlaza(engine::Renderer &graphics, const game::Run &run, const State &state);
    /** Paint lasting spell areas, vent eruption warnings and support-building links beneath field entities.
     */
    void fieldEffects(engine::Renderer &graphics, const game::Run &run, const State &state);
    /** Resolve one entrance marker per lane against the current camera, without drawing. */
    std::vector<LaneMarker> laneMarkers(const game::Run &run) const;
    /** Return the wave-opening highlight strength: 1 when a wave starts, fading to 0 after about 4 s. */
    float opening(const game::Run &run) const;
    /** Paint gate badges and off-screen entrance arrows, and place the minimap for this frame. */
    void mapOverlays(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run, const State &state);
    /** Paint the overview inset placed by mapOverlays, with entrance pulses while a wave opens. */
    void minimap(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run, const State &state);
    /** Draw the topdown world, placement affordances and independent animated enemies. */
    void surface(engine::Renderer &graphics, const game::Run &run, const State &state);
    /** Draw only revealed mine information, hover targets, the miner and excavation cues. */
    void underground(engine::Renderer &graphics, const engine::Ui &ui, const game::Run &run,
                     const State &state);
    /** Draw the excavation wall: rock thickness, contents seen through thin or scanned rock, exposed
     * deposits, the strike preview under the pointer, tool toggles and strike cues. */
    void excavation(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run, const State &state);
    /** Paint the relic gain banner near the bottom of the cutaway; it never blocks input. */
    void relicBanner(engine::Renderer &graphics, const game::Run &run, const State &state);
    /** Draw dig/scan status and grouped crafting, squad and recruitment rows inside `area`. */
    void prepActions(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run, const State &state,
                     engine::Rect area);
    /** Draw a game card silhouette without requiring a texture atlas. */
    void icon(engine::Renderer &graphics, const game::Catalog &catalog, game::CardKind kind,
              engine::ScreenPoint center, float size);
    /** Draw/register a generic button with keyboard focus and an enabled state. */
    void button(engine::Renderer &graphics, engine::Ui &ui, engine::WidgetId id, engine::Rect bounds,
                const std::string &title, bool enabled = true, bool primary = false, bool danger = false);
    /** Draw a compact button with a globe and the selected language's name that switches to the next
     * language. */
    void languageButton(engine::Renderer &graphics, engine::Ui &ui, engine::Rect bounds);
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
    /** Paint the mining lesson details over the cutaway when the sidebar is too short for them. */
    void tutorialTips(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run, const State &state);
    void tutorialHighlights(engine::Renderer &graphics, const engine::Ui &ui, const game::Run &run,
                            const State &state);
    /** Paint terminal practice actions after ordinary gameplay and modal layers. */
    void tutorialOverlay(engine::Renderer &graphics, engine::Ui &ui, const State &state);
    /** Draw a selected boulder's explanation and its priced clear button inside a measured area. */
    void boulderInspector(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run,
                          const State &state, engine::Rect area);
    /** Draw selection details and withdrawal/move/heal actions inside a measured area. */
    void inspector(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run, const State &state,
                   engine::Rect area);
    /** Paint a tactile card deck with category frames and stable card hit regions. */
    void hand(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run, const State &state);
    /** Paint the latest battle message as a fading pill on the card dock's tab row. */
    void toast(engine::Renderer &graphics, const State &state);
    /** Paint current energy, regeneration and spent-energy feedback directly below the four cards. */
    void handEnergy(engine::Renderer &graphics, const game::Run &run, const State &state);
    /** Share clipped attack/effect circles between field selection and pre-placement previews. */
    void rangeRing(engine::Renderer &graphics, engine::ScreenPoint center, float radius, engine::Color color,
                   bool filled);
    /** Paint one reusable card face; parent layers provide clipping and opacity. */
    void cardFace(engine::Renderer &graphics, const game::Catalog &catalog, const game::Card &card,
                  engine::Rect bounds, bool affordable, int displayedCost = -1, bool variableCost = false,
                  const CardCharge *charge = nullptr);
    /** Paint live and dying enemies by role, with shields, auras, burrow mounds and damaged health bars. */
    void enemies(engine::Renderer &graphics, const game::Run &run, const State &state);
    /** Paint sapper fuses and countdowns and elite crowns and name tags above bodies and floating numbers. */
    void enemyMarkers(engine::Renderer &graphics, const game::Run &run, const State &state);
    /** Paint the zone boss banner, wave composition chips and new-role introduction over the battlefield. */
    void enemyHud(engine::Renderer &graphics, const game::Run &run, const State &state);
    /** Paint combat bursts, tracers and floating damage after world entities. */
    void combatFeedback(engine::Renderer &graphics, const State &state);
    /** Draw/register expansion, recruitment, replacement and confirmation dialogs. */
    void overlay(engine::Renderer &graphics, engine::Ui &ui, const game::Run &run, const State &state,
                 const engine::AudioSettings &audio);
};
} // namespace thorium::presentation
