#include "View.h"
#include "Assets.h"
#include "Theme.h"
#include <algorithm>
#include <cmath>

namespace thorium::presentation {
using engine::Color;
using engine::Rect;
using engine::ScreenPoint;
namespace {
constexpr Color ink{37, 54, 61};
constexpr Color mint{45, 130, 108};
constexpr Color gold{231, 176, 64};
constexpr Color blue{88, 149, 192};
constexpr Color coral{213, 104, 81};
/** Map a card action to its recognizable accent color. */
Color cardColor(game::CardCategory category) {
    return category == game::CardCategory::Building ? mint
           : category == game::CardCategory::Unit   ? blue
                                                    : coral;
}
} // namespace

void View::rangeRing(engine::Renderer &graphics, ScreenPoint center, float radius, Color color, bool filled) {
    if (radius <= 0)
        return;
    graphics.beginClip(layout.world);
    if (filled)
        graphics.circle(center, radius, {color.r, color.g, color.b, 28});
    for (int index = 0; index < 48; ++index) {
        const float angle = index * 6.2831853F / 48;
        const float end = angle + 0.075F;
        const ScreenPoint a{center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius};
        const ScreenPoint b{center.x + std::cos(end) * radius, center.y + std::sin(end) * radius};
        graphics.line({a, b}, 2, color);
    }
    graphics.endClip();
}

void View::measure(int width, int height, bool prep, bool mineView, bool tutorial,
                   const game::Catalog &catalog, engine::CameraPose camera, int mineColumns, int mineRows,
                   float mineScroll) {
    layout.compact = width < 980;
    layout.denseMine = layout.compact && prep && mineView && !tutorial && height < 700;
    const float margin = layout.compact ? 12.0F : 24.0F;
    if (!layout.compact && prep) {
        layout.world = {margin, 140.0F, width - 354.0F, height - 164.0F};
        layout.sidebar = {width - 294.0F, 140.0F, 270.0F, height - 164.0F};
        layout.hand = {margin, height - 218.0F, layout.world.w, 198.0F};
    } else if (!layout.compact) {
        // Battle: a slim command column and a short card dock leave the rest to the battlefield.
        const float top = 124.0F;
        const float side = width >= 1600 ? 270.0F : 240.0F;
        const float dock = height >= 1000 ? 184.0F : 156.0F;
        layout.hand = {margin, height - dock - 10.0F, width - 2 * margin - side - 12.0F, dock};
        layout.world = {margin, top, layout.hand.w, std::max(140.0F, layout.hand.y - 6.0F - top)};
        layout.sidebar = {width - margin - side, top, side, height - top - margin};
    } else {
        // Battle keeps only a two-row command strip under the map; preparation and lessons keep the full
        // panel.
        const float worldHeight =
            layout.denseMine               ? height - 370.0F
            : tutorial && prep && mineView ? std::min(420.0F, height - 340.0F)
            : prep && mineView             ? std::clamp(height - 430.0F, 200.0F, 560.0F)
            : prep || tutorial
                ? std::min(std::max((width - 24.0F) / 1.8F + 42.0F, height - 575.0F), height - 460.0F)
                : height - 460.0F;
        layout.world = {margin, layout.denseMine ? 92.0F : 138.0F, width - 2 * margin,
                        std::max(100.0F, worldHeight)};
        const float sidebarY = layout.world.y + layout.world.h + 12;
        layout.sidebar = {margin, sidebarY, width - 2 * margin, height - sidebarY - (prep ? 16.0F : 186.0F)};
        layout.hand = {margin, height - 174.0F, layout.world.w, 158.0F};
    }
    if (prep)
        layout.energyBar = layout.energyMeter = Rect{};
    else {
        const float bar = layout.compact ? 26.0F : 24.0F;
        layout.energyBar = {layout.hand.x + 12,
                            layout.hand.y + layout.hand.h - bar - (layout.compact ? 9 : 7),
                            layout.hand.w - 24, bar};
        layout.energyMeter = {layout.energyBar.x + 100, layout.energyBar.y + (bar - 10) / 2,
                              layout.energyBar.w - 128, 10};
    }
    layout.camera = engine::Camera2D(layout.world, catalog.width, catalog.height, camera);
    layout.tile = layout.camera.tileSize();
    layout.mapOrigin = layout.camera.origin();
    // Cutaway: a header band for the layer legend, a footer band for messages, and a vertically
    // scrolling board whose tiles never shrink below a tappable size.
    layout.mineColumns = std::max(1, mineColumns);
    layout.mineRows = std::max(1, mineRows);
    const float header = layout.denseMine ? 34.0F : 38.0F;
    const float footer = layout.denseMine ? 6.0F : 44.0F;
    layout.mineViewport = {layout.world.x + 4, layout.world.y + header, layout.world.w - 8,
                           std::max(48.0F, layout.world.h - header - footer)};
    const Rect &view = layout.mineViewport;
    const float columns = static_cast<float>(layout.mineColumns), rows = static_cast<float>(layout.mineRows);
    const float sideRoom = layout.compact ? 10.0F : 48.0F;
    const float surfaceRows = .6F;
    const float widthTile = (view.w - 2 * sideRoom) / columns;
    const float fitTile = view.h / (rows + surfaceRows);
    const float minimumTile = layout.compact ? 34.0F : 44.0F;
    layout.mineTile = std::max(1.0F, std::min({widthTile, std::max(fitTile, minimumTile), 72.0F}));
    const float content = layout.mineTile * (rows + surfaceRows);
    layout.mineScrollMax = std::max(0.0F, content - view.h);
    layout.mineScroll = std::clamp(mineScroll * layout.mineTile, 0.0F, layout.mineScrollMax);
    const float offset = layout.mineScrollMax > 0 ? 0.0F : (view.h - content) / 2;
    layout.mineOrigin = {view.x + (view.w - layout.mineTile * columns) / 2,
                         view.y + offset + layout.mineTile * surfaceRows - layout.mineScroll};
}
void View::icon(engine::Renderer &graphics, const game::Catalog &catalog, game::CardKind kind,
                ScreenPoint center, float size) {
    const Color color = cardColor(catalog.card(kind).category);
    if (catalog.isBuilding(kind)) {
        graphics.circle({center.x, center.y + size * .27F}, size * .46F, {12, 30, 33, 90});
        graphics.circle({center.x, center.y + size * .19F}, size * .43F, ink);
        graphics.circle({center.x, center.y + size * .14F}, size * .37F, {139, 163, 146});
        graphics.rect({center.x - size * .39F, center.y - size * .31F, size * .78F, size * .66F}, ink,
                      size * .16F);
        graphics.rect({center.x - size * .36F, center.y - size * .28F, size * .72F, size * .63F}, color,
                      size * .15F);
        graphics.rect({center.x - size * .28F, center.y - size * .23F, size * .56F, size * .13F},
                      {137, 201, 170}, size * .04F);
        if (kind == game::CardKind::Lightning) {
            graphics.polygon({{center.x + size * .08F, center.y - size * .45F},
                              {center.x - size * .17F, center.y},
                              {center.x + size * .02F, center.y},
                              {center.x - size * .04F, center.y + size * .3F},
                              {center.x + size * .25F, center.y - size * .1F},
                              {center.x + size * .07F, center.y - size * .1F}},
                             gold);
        } else if (kind == game::CardKind::Flame) {
            graphics.circle({center.x + size * .2F, center.y + size * .02F}, size * .13F, {214, 92, 56});
            graphics.polygon({{center.x - size * .1F, center.y - size * .02F},
                              {center.x + size * .1F, center.y - size * .02F},
                              {center.x + size * .2F, center.y - size * .44F},
                              {center.x - size * .2F, center.y - size * .44F}},
                             ink);
            graphics.polygon({{center.x - size * .16F, center.y - size * .46F},
                              {center.x, center.y - size * .78F},
                              {center.x + size * .16F, center.y - size * .46F}},
                             {255, 146, 60});
            graphics.polygon({{center.x - size * .07F, center.y - size * .46F},
                              {center.x, center.y - size * .64F},
                              {center.x + size * .07F, center.y - size * .46F}},
                             {255, 226, 120});
        } else if (kind == game::CardKind::Sniper) {
            graphics.rect({center.x - size * .05F, center.y - size * .78F, size * .1F, size * .86F}, ink,
                          size * .04F);
            graphics.rect({center.x - size * .09F, center.y - size * .82F, size * .18F, size * .08F}, ink,
                          size * .03F);
            graphics.circle({center.x + size * .16F, center.y - size * .28F}, size * .1F, ink);
            graphics.circle({center.x + size * .16F, center.y - size * .28F}, size * .055F, {150, 220, 255});
        } else if (kind == game::CardKind::Generator) {
            graphics.rect({center.x - size * .15F, center.y - size * .44F, size * .3F, size * .5F}, ink,
                          size * .08F);
            for (float y : {-.36F, -.22F, -.08F})
                graphics.rect({center.x - size * .19F, center.y + size * y, size * .38F, size * .06F}, gold,
                              size * .03F);
            graphics.circle({center.x, center.y - size * .54F}, size * .15F, {120, 240, 180, 120});
            graphics.circle({center.x, center.y - size * .54F}, size * .09F, {200, 255, 225});
        } else {
            graphics.rect({center.x - size * .12F, center.y - size * .54F, size * .24F, size * .62F}, ink,
                          size * .08F);
            if (kind == game::CardKind::Cannon)
                graphics.circle({center.x, center.y - size * .13F}, size * .27F, {100, 130, 108});
            graphics.rect({center.x - size * .16F, center.y - size * .55F, size * .32F, size * .13F},
                          {163, 187, 176}, size * .04F);
            graphics.rect({center.x - size * .08F, center.y - size * .56F, size * .16F, size * .07F}, ink,
                          size * .02F);
        }
        for (float x : {-.25F, .25F})
            graphics.circle({center.x + size * x, center.y + size * .19F}, size * .035F, gold);
    } else if (catalog.isUnit(kind)) {
        graphics.circle({center.x, center.y + size * .51F}, size * .42F, {12, 30, 33, 65});
        for (float x : {-.16F, .16F})
            graphics.rect(
                {center.x + size * x - size * .09F, center.y + size * .32F, size * .19F, size * .22F}, ink,
                size * .04F);
        graphics.circle({center.x, center.y - size * .05F}, size * .30F, {244, 205, 163});
        graphics.rect({center.x - size * .35F, center.y - size * .42F, size * .7F, size * .31F}, color,
                      size * .12F);
        graphics.rect({center.x - size * .34F, center.y - size * .19F, size * .68F, size * .09F}, ink,
                      size * .03F);
        graphics.circle({center.x, center.y - size * .29F}, size * .075F, gold);
        graphics.rect({center.x - size * .31F, center.y + size * .14F, size * .62F, size * .4F}, color,
                      size * .12F);
        graphics.circle({center.x - size * .1F, center.y - size * .03F}, size * .035F, ink);
        graphics.circle({center.x + size * .1F, center.y - size * .03F}, size * .035F, ink);
        if (kind == game::CardKind::Guard) {
            graphics.rect({center.x + size * .10F, center.y + size * .03F, size * .38F, size * .46F}, ink,
                          size * .08F);
            graphics.rect({center.x + size * .14F, center.y + size * .05F, size * .30F, size * .39F},
                          {188, 215, 221}, size * .07F);
            graphics.line({{center.x + size * .29F, center.y + size * .13F},
                           {center.x + size * .29F, center.y + size * .34F}},
                          size * .04F, {109, 150, 174});
        } else
            graphics.line({{center.x + size * .22F, center.y + size * .2F},
                           {center.x + size * .5F, center.y - size * .1F}},
                          size * .13F, ink);
    } else if (kind == game::CardKind::Frost) {
        for (int index = 0; index < 3; ++index) {
            const float angle = index * 1.04719755F;
            const float x = std::cos(angle) * size * .4F, y = std::sin(angle) * size * .4F;
            graphics.line({{center.x - x, center.y - y}, {center.x + x, center.y + y}}, size * .09F, blue);
        }
    } else if (kind == game::CardKind::Tar) {
        graphics.circle({center.x, center.y + size * .08F}, size * .42F, {46, 32, 26});
        graphics.circle({center.x - size * .1F, center.y + size * .02F}, size * .3F, {74, 52, 40});
        graphics.circle({center.x - size * .17F, center.y - size * .08F}, size * .08F, {170, 148, 128});
        graphics.circle({center.x + size * .18F, center.y - size * .12F}, size * .06F, {110, 88, 70});
        graphics.circle({center.x + size * .05F, center.y - size * .3F}, size * .1F, {74, 52, 40});
    } else {
        graphics.circle(center, size * .34F, coral);
        graphics.line({{center.x + size * .12F, center.y - size * .3F},
                       {center.x + size * .27F, center.y - size * .5F}},
                      size * .08F, gold);
        graphics.circle({center.x - size * .08F, center.y - size * .1F}, size * .08F, {244, 164, 128});
    }
}
void View::surface(engine::Renderer &graphics, const game::Run &run, const State &state) {
    if (state.play.feedback.screenShake > 0.0F && !state.app.preferences.reducedMotion) {
        const float intensity = state.play.feedback.screenShake / 0.35F;
        // Whole physical pixels, so the cached ground moves with the shaken entities without rasterizing.
        const float density = graphics.density();
        shakeOffset.x =
            std::round(std::sin(state.play.visualTime * 52.0F) * layout.tile * 0.14F * intensity * density) /
            density;
        shakeOffset.y =
            std::round(std::cos(state.play.visualTime * 67.0F) * layout.tile * 0.10F * intensity * density) /
            density;
    } else {
        shakeOffset = {};
    }
    const float tile = layout.tile;
    graphics.beginClip(layout.world);
    const std::vector<game::LaneForecast> forecast = run.laneForecast();
    terrain(graphics, run, state, forecast);
    if (run.state().phase == game::Phase::Ready) {
        const ScreenPoint entry = screenPoint(run.state().route.front());
        graphics.animation(
            engine::loopingSample(state.app.animationSets.profiles.at(assets::Oreling).move,
                                  state.app.preferences.reducedMotion ? 0 : state.play.visualTime),
            0, {entry.x - tile * .4F, entry.y - tile * .55F, tile * .8F, tile * .8F},
            engine::AnimationBackend::Vector);
    }
    for (game::Point point : run.state().unitCells) {
        const ScreenPoint p = screenPoint(point);
        if (!inWorld(p, tile))
            continue;
        const game::Card *placing = run.card(placingCard_);
        const Color cellColor =
            placing && run.catalog().isUnit(placing->kind)
                ? (run.previewPlay(placing->id, point).allowed() ? theme::green : theme::red)
                : Color{201, 181, 137};
        graphics.rect({p.x - tile * .23F, p.y - tile * .23F, tile * .46F, tile * .46F}, cellColor,
                      tile * .08F);
        graphics.circle(p, tile * .07F, {245, 231, 196});
    }
    const game::Card *selected = run.card(placingCard_);
    const bool placingBuilding = selected && run.catalog().isBuilding(selected->kind);
    for (game::Point point : run.state().pads) {
        const ScreenPoint p = screenPoint(point);
        if (!inWorld(p, tile))
            continue;
        const game::PadTrait trait = run.padTrait(point);
        graphics.circle({p.x, p.y + tile * .06F}, tile * .42F, {148, 173, 144});
        graphics.circle(p, tile * .38F, {79, 106, 91});
        graphics.circle(p, tile * .33F, {232, 228, 194});
        graphics.circle(p, tile * .25F,
                        placingBuilding
                            ? (run.previewPlay(selected->id, point).allowed() ? theme::green : theme::red)
                        : trait == game::PadTrait::HighGround ? Color{246, 226, 160}
                        : trait == game::PadTrait::Crystal    ? Color{184, 230, 246}
                                                              : Color{217, 226, 198});
        for (float offset : {-.23F, .23F})
            graphics.circle({p.x + tile * offset, p.y}, tile * .035F, {125, 151, 128});
    }
    // A boulder is ringed red while a building card is held and gold while it is selected.
    for (game::Point point : run.state().boulders) {
        const ScreenPoint p = screenPoint(point);
        const bool chosen =
            state.play.selectedBoulder && game::distance(*state.play.selectedBoulder, point) < .01F;
        if (!inWorld(p, tile) || (!chosen && !placingBuilding))
            continue;
        drawRing(graphics, p, tile * .54F, std::max(2.0F, tile * .08F),
                 chosen ? Color{255, 226, 126} : theme::red);
    }
    const game::Tower *chosenTower = run.tower(state.play.selectedTower);
    const game::Unit *chosenUnit = run.unit(state.play.selectedUnit);
    if (placement_) {
        const Color color = placement_->rule.allowed() ? theme::green : theme::red;
        rangeRing(graphics, screenPoint(placement_->target), placement_->range * tile,
                  {color.r, color.g, color.b, 220}, true);
    } else if (chosenTower || chosenUnit) {
        const game::Point position = chosenTower ? chosenTower->position : chosenUnit->position;
        const game::CardKind kind = chosenTower ? chosenTower->kind : chosenUnit->kind;
        rangeRing(graphics, screenPoint(position),
                  (chosenTower ? run.buildingRange(kind, position) : run.catalog().card(kind).range) * tile,
                  {255, 236, 169, 200}, false);
    }
    basePlaza(graphics, run, state);
    fieldEffects(graphics, run, state);
    for (const game::Tower &tower : run.state().towers) {
        const ScreenPoint p = screenPoint(tower.position);
        if (!inWorld(p, tile))
            continue;
        if (tower.id == state.play.selectedTower) {
            graphics.circle(p, tile * .47F, {255, 226, 126});
            graphics.circle(p, tile * .40F, {71, 110, 92});
        }
        const game::Point recoil =
            state.app.preferences.reducedMotion ? game::Point{} : state.play.feedback.recoil(tower.id);
        icon(graphics, run.catalog(), tower.kind, {p.x + recoil.x * tile, p.y + recoil.y * tile}, tile * .8F);
        // Level pips stay centred under the icon; awakened levels beyond the card maximum glow violet.
        for (int level = 0; level < tower.level; ++level)
            graphics.circle(
                {p.x + (static_cast<float>(level) - static_cast<float>(tower.level - 1) / 2) * tile * .12F,
                 p.y + tile * .42F},
                tile * .045F, level < game::CardMaxLevel ? gold : Color{190, 150, 255});
    }
    // While a unit card is held, a unit of its kind shows the same gold upgrade ring and tag.
    const bool placingUnit = selected && run.catalog().isUnit(selected->kind);
    if (placingUnit)
        for (const game::Unit &unit : run.state().units) {
            const ScreenPoint p = screenPoint(game::Run::homeOf(unit));
            const game::PlayPreview preview = run.previewPlay(selected->id, game::Run::homeOf(unit));
            const bool maxed = preview.failure == game::PlayFailure::MaximumLevel;
            if (!inWorld(p, tile * 2) || (preview.action != game::PlayAction::Upgrade && !maxed))
                continue;
            const Color accent = maxed ? Color{160, 160, 160} : theme::gold;
            const float pulse = state.app.preferences.reducedMotion
                                    ? 1.0F
                                    : .65F + .35F * std::sin(state.play.visualTime * 5.0F);
            drawRing(graphics, p, tile * .46F, std::max(2.0F, tile * .08F),
                     {accent.r, accent.g, accent.b,
                      static_cast<std::uint8_t>((preview.allowed() || maxed ? 235.0F : 120.0F) * pulse)});
            const std::string tag = maxed ? "최대" : "강화 Lv" + std::to_string(unit.level + 1);
            const float size = std::clamp(tile * .3F, 10.0F, 13.0F);
            const float width = size * (maxed ? 2.6F : 4.6F);
            const engine::Rect plate{p.x - width / 2, p.y - tile * .62F - size * 1.4F, width, size * 1.4F};
            graphics.rect(plate, {16, 24, 30, 225}, size * .4F);
            graphics.rect({plate.x, plate.y + plate.h - 2, plate.w, 2}, accent, 1);
            graphics.text(tag, {p.x, plate.y + size * .18F}, size, accent, .5F);
        }
    // While a building card is held, a building it can stack onto shows a gold upgrade ring and one it would
    // replace an orange ring, each with a tag; the ring dims while the energy is short and a maxed building
    // shows grey.
    if (placingBuilding)
        for (const game::Tower &tower : run.state().towers) {
            const ScreenPoint p = screenPoint(tower.position);
            if (!inWorld(p, tile * 2))
                continue;
            const game::PlayPreview preview = run.previewPlay(selected->id, tower.position);
            const bool upgrade = preview.action == game::PlayAction::Upgrade;
            const bool replace = preview.action == game::PlayAction::Replace;
            const bool maxed = preview.failure == game::PlayFailure::MaximumLevel;
            if (!upgrade && !replace && !maxed)
                continue;
            const Color accent = maxed ? Color{160, 160, 160} : upgrade ? theme::gold : Color{255, 140, 60};
            const float pulse = state.app.preferences.reducedMotion
                                    ? 1.0F
                                    : .65F + .35F * std::sin(state.play.visualTime * 5.0F);
            const std::uint8_t opacity =
                static_cast<std::uint8_t>((preview.allowed() || maxed ? 235.0F : 120.0F) * pulse);
            drawRing(graphics, p, tile * .52F, std::max(2.0F, tile * .09F),
                     {accent.r, accent.g, accent.b, opacity});
            const std::string tag = maxed     ? "최대"
                                    : upgrade ? "강화 Lv" + std::to_string(tower.level + 1)
                                              : "교체";
            const float size = std::clamp(tile * .3F, 10.0F, 13.0F);
            const float width = size * (upgrade ? 4.6F : 2.6F);
            const engine::Rect plate{p.x - width / 2, p.y - tile * .62F - size * 1.4F, width, size * 1.4F};
            graphics.rect(plate, {16, 24, 30, 225}, size * .4F);
            graphics.rect({plate.x, plate.y + plate.h - 2, plate.w, 2}, accent, 1);
            graphics.text(tag, {p.x, plate.y + size * .18F}, size, accent, .5F);
        }
    for (const game::Unit &unit : run.state().units) {
        const ScreenPoint p = screenPoint(unit.position);
        if (!inWorld(p, tile))
            continue;
        if (unit.id == state.play.selectedUnit)
            graphics.circle(p, tile * .44F, {255, 226, 126, 220});
        const game::Point recoil =
            state.app.preferences.reducedMotion ? game::Point{} : state.play.feedback.recoil(unit.id);
        if (unit.hero)
            graphics.circle(p, tile * .4F, {255, 200, 80, 70});
        icon(graphics, run.catalog(), unit.kind, {p.x + recoil.x * tile, p.y + recoil.y * tile}, tile * .62F);
        if (unit.hero) {
            // A gold crown above the hero.
            const float w = tile * .3F, base = p.y - tile * .52F;
            graphics.polygon({{p.x - w / 2, base},
                              {p.x - w / 2, base - tile * .14F},
                              {p.x - w / 4, base - tile * .06F},
                              {p.x, base - tile * .18F},
                              {p.x + w / 4, base - tile * .06F},
                              {p.x + w / 2, base - tile * .14F},
                              {p.x + w / 2, base}},
                             {255, 206, 72});
        }
        const float hit =
            state.app.preferences.reducedMotion ? 0.0F : state.play.feedback.hitIntensity(unit.id, false);
        if (hit > 0)
            graphics.circle(p, tile * .25F, {255, 235, 215, static_cast<std::uint8_t>(170 * hit)});
        graphics.rect({p.x - tile * .25F, p.y - tile * .47F, tile * .5F, tile * .07F}, {95, 118, 108}, 2);
        graphics.rect({p.x - tile * .25F, p.y - tile * .47F, tile * .5F * unit.hp / unit.maxHp, tile * .07F},
                      mint, 2);
        // Level pips under a raised unit, as under a building.
        for (int level = 0; level < unit.level && unit.level > 1; ++level)
            graphics.circle(
                {p.x + (static_cast<float>(level) - static_cast<float>(unit.level - 1) / 2) * tile * .12F,
                 p.y + tile * .38F},
                tile * .045F, {255, 214, 110});
    }
    enemies(graphics, run, state);
    combatFeedback(graphics, state);
    // Sabotage markers and enemy labels go above enemies and floating numbers so a crowd on the road never
    // hides a countdown or an elite name.
    for (const game::Tower &tower : run.state().towers) {
        const ScreenPoint p = screenPoint(tower.position);
        if (tower.maxHp > 0 && tower.hp < tower.maxHp && inWorld(p, tile * 2)) {
            const float share = std::clamp(static_cast<float>(tower.hp) / tower.maxHp, 0.0F, 1.0F);
            const Color bar = share > .5F    ? Color{150, 220, 130}
                              : share > .25F ? Color{240, 200, 90}
                                             : Color{240, 100, 80};
            graphics.rect({p.x - tile * .32F, p.y + tile * .4F, tile * .64F, tile * .09F}, {40, 30, 30, 210},
                          2);
            graphics.rect({p.x - tile * .32F, p.y + tile * .4F, tile * .64F * share, tile * .09F}, bar, 2);
        }
        if (tower.disabled > 0 && inWorld(p, tile * 2)) {
            // A sabotaged building is greyed out with a draining ring and the seconds until it works again.
            const float total = std::max(.01F, run.catalog().enemy(game::EnemyKind::Sapper).abilityDuration);
            const float left = std::clamp(tower.disabled / total, 0.0F, 1.0F);
            graphics.circle(p, tile * .42F, {30, 26, 30, 170});
            const float flicker =
                state.app.preferences.reducedMotion ? 1.0F : .6F + .4F * std::sin(state.play.visualTime * 17);
            const float r = tile * .22F;
            graphics.line({{p.x - r, p.y - r * .3F},
                           {p.x - r * .1F, p.y + r * .2F},
                           {p.x + r * .2F, p.y - r * .4F},
                           {p.x + r, p.y + r * .3F}},
                          std::max(1.5F, tile * .05F),
                          {255, 196, 90, static_cast<std::uint8_t>(230 * flicker)});
            std::vector<ScreenPoint> ring;
            const int steps = std::max(2, static_cast<int>(36 * left));
            for (int index = 0; index <= steps; ++index) {
                const float angle = -1.5707963F + 6.2831853F * left * index / steps;
                ring.push_back({p.x + std::cos(angle) * tile * .46F, p.y + std::sin(angle) * tile * .46F});
            }
            graphics.line(ring, std::max(2.0F, tile * .06F), {255, 136, 48, 220});
            const float label = std::clamp(tile * .34F, 10.0F, 15.0F);
            graphics.rect({p.x - label * 1.9F, p.y + tile * .5F, label * 3.8F, label * 1.2F},
                          {58, 20, 16, 220}, 4);
            graphics.text("마비 " + std::to_string(static_cast<int>(std::ceil(tower.disabled))),
                          {p.x, p.y + tile * .5F + 1}, label, {255, 214, 150}, .5F);
        }
    }
    enemyMarkers(graphics, run, state);
    enemyHud(graphics, run, state);
    graphics.endClip();
    graphics.rect({layout.world.x + 10, layout.world.y + 9, layout.compact ? 116.0F : 184.0F, 25},
                  {34, 67, 56, 220}, 5);
    const auto &stageMeta = game::stageMetadata(state.play.currentStage);
    graphics.text(run.state().phase == game::Phase::Prep ? "정비 구역 · 지상" : stageMeta.name,
                  {layout.world.x + 21, layout.world.y + 15}, layout.compact ? 11 : 12, {235, 231, 198});
}

} // namespace thorium::presentation
