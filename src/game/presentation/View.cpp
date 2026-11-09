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
constexpr Color cream{249, 247, 239};
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
/** Return a localized ore name without leaking unknown mineral contents. */
const char *oreName(game::Ore ore) {
    switch (ore) {
    case game::Ore::Iron:
        return "철";
    case game::Ore::Copper:
        return "구리";
    case game::Ore::Crystal:
        return "수정";
    case game::Ore::Gold:
        return "금";
    default:
        return "암석";
    }
}
/** Return a mineral's presentation color. */
Color oreColor(game::Ore ore) {
    switch (ore) {
    case game::Ore::Iron:
        return {171, 193, 204};
    case game::Ore::Copper:
        return {226, 144, 104};
    case game::Ore::Crystal:
        return {150, 139, 218};
    case game::Ore::Gold:
        return gold;
    default:
        return {172, 157, 134};
    }
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

void View::measure(int width, int height, bool prep, bool mineView, bool tutorial) {
    layout.compact = width < 980;
    const float margin = layout.compact ? 12.0F : 24.0F;
    if (!layout.compact) {
        layout.world = {margin, 140.0F, width - 354.0F, height - 140.0F - (prep ? 24.0F : 234.0F)};
        layout.sidebar = {width - 294.0F, 140.0F, 270.0F, height - 164.0F};
    } else {
        const float worldHeight = tutorial && prep && mineView ? std::min(420.0F, height - 340.0F)
                                  : prep && mineView
                                      ? std::min(420.0F, height - 514.0F)
                                      : std::min((width - 24.0F) / 1.8F + 42.0F, height - 460.0F);
        layout.world = {margin, 138.0F, width - 2 * margin, std::max(100.0F, worldHeight)};
        const float sidebarY = layout.world.y + layout.world.h + 12;
        layout.sidebar = {margin, sidebarY, width - 2 * margin, height - sidebarY - (prep ? 16.0F : 186.0F)};
    }
    layout.hand = {margin, height - (layout.compact ? 174.0F : 218.0F), layout.world.w,
                   layout.compact ? 158.0F : 198.0F};
    layout.energyBar =
        prep ? Rect{} : Rect{layout.hand.x + 12, layout.hand.y + layout.hand.h - 35, layout.hand.w - 24, 26};
    layout.energyMeter =
        prep ? Rect{} : Rect{layout.energyBar.x + 100, layout.energyBar.y + 8, layout.energyBar.w - 128, 10};
    layout.tile = std::min((layout.world.w - 24.0F) / 18.0F, (layout.world.h - 32.0F) / 10.0F);
    layout.mapOrigin = {layout.world.x + (layout.world.w - layout.tile * 18) / 2,
                        layout.world.y + (layout.world.h - layout.tile * 10) / 2};
    layout.mineTile = std::min((layout.world.w - 40.0F) / 5.0F, (layout.world.h - 42.0F) / 6.0F);
    layout.mineOrigin = {layout.world.x + (layout.world.w - layout.mineTile * 5) / 2,
                         layout.world.y + (layout.world.h - layout.mineTile * 6) / 2 + 8};
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
        shakeOffset.x = std::sin(state.play.visualTime * 52.0F) * layout.tile * 0.14F * intensity;
        shakeOffset.y = std::cos(state.play.visualTime * 67.0F) * layout.tile * 0.10F * intensity;
    } else {
        shakeOffset = {};
    }
    const float tile = layout.tile;
    graphics.gradientRect(layout.world, {163, 190, 147}, {198, 211, 165}, 12);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 18; ++x) {
            const ScreenPoint p = screenPoint({static_cast<float>(x), static_cast<float>(y)});
            if ((x * 17 + y * 13) % 9 == 0) {
                graphics.circle({p.x, p.y}, tile * .15F, {125, 162, 119, 85});
                graphics.line({{p.x - tile * .09F, p.y + tile * .04F},
                               {p.x - tile * .02F, p.y - tile * .09F},
                               {p.x + tile * .01F, p.y + tile * .03F},
                               {p.x + tile * .09F, p.y - tile * .04F}},
                              1, {113, 150, 103, 110});
            }
            if ((y == 1 || y == 8) && (x == 1 || x == 3 || x == 14 || x == 16)) {
                graphics.circle({p.x, p.y + tile * .11F}, tile * .36F, {48, 83, 61, 60});
                graphics.circle({p.x - tile * .12F, p.y}, tile * .25F, {87, 133, 94});
                graphics.circle({p.x + tile * .12F, p.y - tile * .11F}, tile * .29F, {104, 150, 100});
                graphics.circle({p.x + tile * .05F, p.y - tile * .22F}, tile * .15F, {128, 171, 111});
            }
        }
    std::vector<ScreenPoint> route;
    for (game::Point point : run.state().route)
        route.push_back(screenPoint(point));
    graphics.line(route, tile * .96F, {169, 151, 119});
    graphics.line(route, tile * .77F, {228, 209, 165});
    for (std::size_t index = 1; index < route.size(); index += 2)
        graphics.circle({route[index].x - tile * .2F, route[index].y + tile * .19F}, tile * .04F,
                        {179, 156, 116});
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
    for (game::Point point : run.state().pads) {
        const ScreenPoint p = screenPoint(point);
        graphics.circle({p.x, p.y + tile * .06F}, tile * .42F, {148, 173, 144});
        graphics.circle(p, tile * .38F, {79, 106, 91});
        graphics.circle(p, tile * .33F, {232, 228, 194});
        graphics.circle(p, tile * .25F,
                        selected && run.catalog().isBuilding(selected->kind)
                            ? (run.previewPlay(selected->id, point).allowed() ? theme::green : theme::red)
                            : Color{217, 226, 198});
        for (float offset : {-.23F, .23F})
            graphics.circle({p.x + tile * offset, p.y}, tile * .035F, {125, 151, 128});
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
        rangeRing(graphics, screenPoint(position), run.catalog().card(kind).range * tile,
                  {255, 236, 169, 200}, false);
    }
    const ScreenPoint base = screenPoint(run.state().route.back());
    graphics.circle({base.x, base.y + tile * .2F}, tile * .65F, {129, 164, 135, 80});
    graphics.rect({base.x - tile * .48F, base.y - tile * .65F, tile * .96F, tile * 1.0F}, {243, 237, 209},
                  tile * .18F);
    graphics.rect({base.x - tile * .32F, base.y - tile * .98F, tile * .64F, tile * .4F}, mint, tile * .1F);
    graphics.polygon({{base.x, base.y - tile * .35F},
                      {base.x + tile * .24F, base.y},
                      {base.x, base.y + tile * .22F},
                      {base.x - tile * .24F, base.y}},
                     gold);
    for (const game::Tower &tower : run.state().towers) {
        const ScreenPoint p = screenPoint(tower.position);
        if (tower.id == state.play.selectedTower) {
            graphics.circle(p, tile * .47F, {255, 226, 126});
            graphics.circle(p, tile * .40F, {71, 110, 92});
        }
        const game::Point recoil =
            state.app.preferences.reducedMotion ? game::Point{} : state.play.feedback.recoil(tower.id);
        icon(graphics, run.catalog(), tower.kind, {p.x + recoil.x * tile, p.y + recoil.y * tile}, tile * .8F);
        for (int level = 0; level < tower.level; ++level)
            graphics.circle({p.x + (level - 1) * tile * .12F, p.y + tile * .42F}, tile * .045F, gold);
    }
    for (const game::Unit &unit : run.state().units) {
        const ScreenPoint p = screenPoint(unit.position);
        if (unit.id == state.play.selectedUnit)
            graphics.circle(p, tile * .44F, {255, 226, 126, 220});
        const game::Point recoil =
            state.app.preferences.reducedMotion ? game::Point{} : state.play.feedback.recoil(unit.id);
        icon(graphics, run.catalog(), unit.kind, {p.x + recoil.x * tile, p.y + recoil.y * tile}, tile * .62F);
        const float hit =
            state.app.preferences.reducedMotion ? 0.0F : state.play.feedback.hitIntensity(unit.id, false);
        if (hit > 0)
            graphics.circle(p, tile * .25F, {255, 235, 215, static_cast<std::uint8_t>(170 * hit)});
        graphics.rect({p.x - tile * .25F, p.y - tile * .47F, tile * .5F, tile * .07F}, {95, 118, 108}, 2);
        graphics.rect({p.x - tile * .25F, p.y - tile * .47F, tile * .5F * unit.hp / unit.maxHp, tile * .07F},
                      mint, 2);
    }
    for (const std::pair<const game::EntityId, ActorVisual> &entry : state.play.actors.actors()) {
        const ActorVisual &enemy = entry.second;
        const bool dying = enemy.animator.motion() == ActorMotion::Death;
        if (dying && state.app.preferences.reducedMotion)
            continue;
        const ScreenPoint p = screenPoint(enemy.position);
        const float size = tile * (enemy.kind == game::EnemyKind::Armored ? 1.0F : 0.85F);
        if (!dying && enemy.kind == game::EnemyKind::Armored)
            graphics.circle(p, tile * .46F, {95, 122, 138});
        if (!dying && enemy.slowed)
            graphics.circle(p, tile * .48F, {155, 219, 234, 180});
        const float hit = state.app.preferences.reducedMotion || dying
                              ? 0.0F
                              : state.play.feedback.hitIntensity(enemy.id, true);
        const float width = size * (1 + hit * .18F), height = size * (1 - hit * .15F);
        graphics.animation(enemy.animator.sample(state.app.preferences.reducedMotion), enemy.id,
                           {p.x - width / 2, p.y - height * .62F, width, height});
        if (hit > 0)
            graphics.circle({p.x, p.y - size * .1F}, size * .3F,
                            {255, 249, 221, static_cast<std::uint8_t>(170 * hit)});
    }
    combatFeedback(graphics, state);
    graphics.rect({layout.world.x + 10, layout.world.y + 9, layout.compact ? 116.0F : 184.0F, 25},
                  {34, 67, 56, 220}, 5);
    const auto &stageMeta = game::stageMetadata(state.play.currentStage);
    graphics.text(run.state().phase == game::Phase::Prep ? "정비 구역 · 지상" : stageMeta.name,
                  {layout.world.x + 21, layout.world.y + 15}, layout.compact ? 11 : 12, {235, 231, 198});
}

void View::underground(engine::Renderer &graphics, const game::Run &run, const State &state) {
    graphics.gradientRect(layout.world, {80, 69, 57}, {40, 41, 41}, 12);
    for (int row = 0; row < 6; ++row)
        graphics.rect({layout.world.x + 8, layout.world.y + row * layout.world.h / 6 + 6, layout.world.w - 16,
                       layout.world.h / 6 - 9},
                      row % 2 ? Color{132, 98, 65, 60} : Color{177, 135, 85, 40}, 8);
    const float tile = layout.mineTile;
    for (int index = 0; index < 30; ++index) {
        const game::MineCell &cell = run.state().mine[index];
        const float x = layout.mineOrigin.x + (index % 5) * tile,
                    y = layout.mineOrigin.y + (index / 5) * tile;
        const Rect box{x + 3, y + 3, tile - 6, tile - 6};
        graphics.rect({x + 3, y + 6, tile - 6, tile - 6}, {136, 107, 83, 80}, 9);
        const Color rock = cell.dug               ? Color{94, 84, 78}
                           : run.reachable(index) ? Color{194, 161, 122}
                                                  : Color{139, 118, 97};
        graphics.rect(box, cell.dug ? Color{39, 45, 45} : run.reachable(index) ? gold : Color{86, 77, 64}, 8);
        graphics.gradientRect({box.x + 2, box.y + 2, box.w - 4, box.h - 4}, rock,
                              cell.dug ? Color{48, 51, 49}
                                       : Color{static_cast<std::uint8_t>(rock.r - 25),
                                               static_cast<std::uint8_t>(rock.g - 23),
                                               static_cast<std::uint8_t>(rock.b - 20)},
                              6);
        if (!cell.dug) {
            graphics.line(
                {{box.x + box.w * .12F, box.y + box.h * .12F}, {box.x + box.w * .38F, box.y + box.h * .08F}},
                1, {240, 213, 169, 85});
            graphics.line({{box.x + box.w * .75F, box.y + box.h * .62F},
                           {box.x + box.w * .67F, box.y + box.h * .79F},
                           {box.x + box.w * .77F, box.y + box.h * .9F}},
                          1, {77, 62, 47, 110});
        }
        if (run.state().job.active && run.state().job.target == index)
            graphics.rect({box.x - 2, box.y - 2, box.w + 4, box.h + 4}, {244, 222, 159, 110}, 10);
        if (cell.dug) {
            if (cell.clueKnown)
                graphics.text(std::to_string(cell.clue), {x + tile * .78F, y + tile * .12F}, tile * .19F,
                              {201, 206, 186}, 0.5F);
        } else {
            if (cell.info && cell.ore != game::Ore::Rock) {
                const Color color = oreColor(cell.ore);
                graphics.polygon({{x + tile * .5F, y + tile * .19F},
                                  {x + tile * .71F, y + tile * .41F},
                                  {x + tile * .55F, y + tile * .65F},
                                  {x + tile * .29F, y + tile * .48F}},
                                 color);
                graphics.polygon({{x + tile * .5F, y + tile * .19F},
                                  {x + tile * .71F, y + tile * .41F},
                                  {x + tile * .47F, y + tile * .43F}},
                                 {255, 245, 208, 100});
                graphics.text(oreName(cell.ore), {x + tile * .5F, y + tile * .69F}, tile * .17F,
                              {255, 241, 214}, 0.5F);
            } else
                graphics.text(cell.grade == 2 ? "T2" : "?", {x + tile * .5F, y + tile * .32F}, tile * .25F,
                              {230, 206, 168}, 0.5F);
            if (cell.clueKnown)
                graphics.text(std::to_string(cell.clue), {x + tile * .81F, y + tile * .12F}, tile * .2F,
                              cell.clue ? Color{252, 232, 164} : Color{231, 218, 187}, 0.5F);
            if (cell.info && cell.nest)
                graphics.circle({x + tile * .2F, y + tile * .2F}, tile * .08F, coral);
        }
    }
    const game::Point miner = run.minerPosition();
    const ScreenPoint p{layout.mineOrigin.x + (miner.x + .5F) * tile,
                        layout.mineOrigin.y + (miner.y + .5F) * tile};
    graphics.circle({p.x, p.y + tile * .22F}, tile * .24F, {20, 20, 20, 70});
    graphics.rect({p.x - tile * .2F, p.y - tile * .08F, tile * .4F, tile * .34F}, mint, 6);
    graphics.circle({p.x, p.y - tile * .13F}, tile * .18F, {245, 205, 155});
    graphics.rect({p.x - tile * .24F, p.y - tile * .33F, tile * .48F, tile * .19F}, gold, 5);
    graphics.circle({p.x + tile * .08F, p.y - tile * .25F}, tile * .07F, cream);
    if (run.state().job.active)
        graphics.line(
            {{p.x + tile * .2F, p.y},
             {p.x + tile * .38F, p.y - tile * .22F - std::sin(state.play.visualTime * 14) * tile * .08F}},
            tile * .06F, cream);
    graphics.text(layout.compact ? "지하 탐사 · 숫자 = 인접 둥지 수"
                                 : "지하 탐사 / 숫자는 주변 8칸의 최초 둥지 수",
                  {layout.world.x + 16, layout.world.y + 12}, layout.compact ? 10 : 12, {237, 216, 175});
}

} // namespace thorium::presentation
