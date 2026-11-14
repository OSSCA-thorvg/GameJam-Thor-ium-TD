#include "Actions.h"
#include "MineArt.h"
#include "Text.h"
#include "Theme.h"
#include "View.h"
#include <algorithm>
#include <cmath>

namespace thorium::presentation {
using engine::Color;
using engine::Rect;
using engine::ScreenPoint;
namespace {
/** Deterministic value in [0, 1) for per-cell rock texture. */
float grain(int index, int salt) {
    unsigned value = static_cast<unsigned>(index) * 374761393U + static_cast<unsigned>(salt) * 668265263U;
    value = (value ^ (value >> 13)) * 1274126177U;
    return static_cast<float>((value ^ (value >> 16)) & 0xffffU) / 65536.0F;
}
/** Scale a color's channels, keeping its alpha. */
Color shade(Color color, float factor) {
    const auto channel = [factor](std::uint8_t value) {
        return static_cast<std::uint8_t>(std::clamp(static_cast<float>(value) * factor, 0.0F, 255.0F));
    };
    return {channel(color.r), channel(color.g), channel(color.b), color.a};
}
/** Replace a color's alpha with an opacity from 0 to 1. */
Color alpha(Color color, float opacity) {
    return {color.r, color.g, color.b, static_cast<std::uint8_t>(255 * std::clamp(opacity, 0.0F, 1.0F))};
}
/** Rock color of a wall for a stage biome. */
Color wallRock(game::StageBiome biome) {
    switch (biome) {
    case game::StageBiome::Obsidian:
        return {96, 88, 118};
    case game::StageBiome::Tundra:
        return {122, 146, 164};
    case game::StageBiome::Lava:
        return {134, 84, 72};
    case game::StageBiome::Core:
        return {92, 128, 104};
    case game::StageBiome::Crystal:
        return {104, 96, 150};
    case game::StageBiome::Swamp:
        return {104, 112, 78};
    case game::StageBiome::Meadow:
        break;
    }
    return {150, 118, 90};
}
/** Picture key of a deposit's contents. */
const char *depositPicture(const game::Deposit &deposit) {
    switch (deposit.special) {
    case game::MineSpecial::Relic:
        return "chest";
    case game::MineSpecial::Fossil:
        return "fossil";
    case game::MineSpecial::Energy:
        return "energy";
    case game::MineSpecial::Unstable:
        return "cracked";
    case game::MineSpecial::None:
        break;
    }
    if (deposit.nest)
        return "nest";
    return deposit.ore == game::Ore::Iron     ? "ore-iron"
           : deposit.ore == game::Ore::Copper ? "ore-copper"
           : deposit.ore == game::Ore::Gold   ? "ore-gold"
                                              : "ore-crystal";
}
/** Accent color of a deposit's contents for glints, sockets and cues. */
Color depositColor(const game::Deposit &deposit) {
    if (deposit.special != game::MineSpecial::None)
        return mineSpecialColor(deposit.special);
    return deposit.nest ? Color{255, 96, 72} : mineOreColor(deposit.ore);
}
/** Draw a four-sided outline inside a rectangle. */
void outline(engine::Renderer &g, Rect box, float width, Color color) {
    g.rect({box.x, box.y, box.w, width}, color);
    g.rect({box.x, box.y + box.h - width, box.w, width}, color);
    g.rect({box.x, box.y + width, width, box.h - width * 2}, color);
    g.rect({box.x + box.w - width, box.y + width, width, box.h - width * 2}, color);
}
/** Draw text with a one-pixel dark offset so it stays legible on rock. */
void label(engine::Renderer &g, const std::string &value, ScreenPoint at, float size, Color color,
           float align = .5F) {
    const ScreenPoint rounded{std::round(at.x), std::round(at.y)};
    g.text(value, {rounded.x + 1, rounded.y + 1}, size,
           {12, 10, 12, static_cast<std::uint8_t>(color.a * .8F)}, align);
    g.text(value, rounded, size, color, align);
}
} // namespace

void View::excavation(engine::Renderer &g, engine::Ui &ui, const game::Run &run, const State &state) {
    const game::RunState &live = run.state();
    const bool still = state.app.preferences.reducedMotion;
    const float time = still ? 0.0F : state.play.visualTime;
    const float tile = layout.mineTile;
    const int columns = layout.mineColumns;
    const int layer = live.mineLayer;
    const Rect view = layout.mineViewport;
    const game::MineLayerDefinition &definition =
        layer == 0 ? run.catalog().mine : run.catalog().deeperMines[static_cast<std::size_t>(layer - 1)];
    const Color rock = shade(wallRock(definition.biome), 1.0F - .08F * static_cast<float>(layer));
    const game::StrikeTool tool = state.play.blast ? game::StrikeTool::Blast : game::StrikeTool::Pick;

    g.gradientRect(layout.world, {46, 38, 34}, {20, 20, 24}, 12);
    g.beginClip(view);
    g.rect(view, shade(rock, .28F));
    const Rect first = layout.mineCellBounds(0);
    const Rect board{first.x, first.y, tile * static_cast<float>(columns),
                     tile * static_cast<float>(layout.mineRows)};
    g.rect({board.x - 4, board.y - 4, board.w + 8, board.h + 8}, shade(rock, .18F), 6);

    const auto visible = [&](const Rect &box) { return box.y <= view.y + view.h && box.y + tile >= view.y; };
    std::vector<char> reach(live.mine.size(), 0);
    for (int index = 0; index < static_cast<int>(live.mine.size()); ++index)
        reach[static_cast<std::size_t>(index)] = run.reachable(index) ? 1 : 0;
    for (int index = 0; index < static_cast<int>(live.mine.size()); ++index) {
        const Rect box = layout.mineCellBounds(index);
        if (!visible(box))
            continue;
        const game::MineCell &cell = live.mine[static_cast<std::size_t>(index)];
        const game::Deposit *deposit =
            cell.deposit ? &live.deposits[static_cast<std::size_t>(cell.deposit - 1)] : nullptr;
        const ScreenPoint center{box.x + tile / 2, box.y + tile / 2};
        if (cell.cover == 0) {
            g.gradientRect({box.x + .5F, box.y + .5F, tile - 1, tile - 1}, {22, 18, 20}, {40, 32, 30});
            if (!deposit)
                continue;
            if (deposit->collected) {
                g.circle(center, tile * .1F, alpha(depositColor(*deposit), .28F));
                continue;
            }
            g.rect({box.x + tile * .06F, box.y + tile * .06F, tile * .88F, tile * .88F},
                   alpha(shade(depositColor(*deposit), .35F), .9F), tile * .2F);
            if (deposit->special == game::MineSpecial::Relic && deposit->cells.size() == 4) {
                // One chest spans the block; each exposed cell shows its own quarter.
                const Rect origin = layout.mineCellBounds(deposit->cells.front());
                g.beginClip(box);
                g.picture("chest", {origin.x + tile * .1F, origin.y + tile * .1F, tile * 1.8F, tile * 1.8F});
                g.endClip();
            } else
                g.picture(depositPicture(*deposit),
                          {box.x + tile * .1F, box.y + tile * .1F, tile * .8F, tile * .8F});
            continue;
        }
        const bool hard = cell.grade > 1;
        const bool locked = cell.grade > live.tool;
        const float depth = cell.cover >= 4 ? .5F : cell.cover == 3 ? .66F : cell.cover == 2 ? .84F : 1.04F;
        const Color face = hard ? shade(Color{118, 126, 140}, depth) : shade(rock, depth);
        g.rect({box.x + .5F, box.y + .5F, tile - 1, tile - 1}, shade(face, .62F), tile * .1F);
        g.rect({box.x + 1.5F, box.y + 1.5F, tile - 3, tile - 4.5F}, face, tile * .1F);
        g.rect({box.x + tile * .12F, box.y + tile * .08F, tile * .76F, std::max(1.5F, tile * .06F)},
               alpha(shade(face, 1.35F), .55F), 2);
        for (int speck = 0; speck < 2; ++speck)
            g.circle({box.x + tile * (.2F + grain(index, speck) * .6F),
                      box.y + tile * (.3F + grain(index, speck + 7) * .5F)},
                     tile * (.03F + grain(index, speck + 3) * .04F), alpha(shade(face, .7F), .8F));
        if (hard) {
            g.line({{box.x + tile * .12F, box.y + tile * .7F}, {box.x + tile * .7F, box.y + tile * .12F}},
                   std::max(1.5F, tile * .05F), alpha(shade(face, 1.5F), .5F));
            g.line({{box.x + tile * .32F, box.y + tile * .9F}, {box.x + tile * .9F, box.y + tile * .32F}},
                   std::max(1.5F, tile * .05F), alpha(shade(face, 1.5F), .5F));
        }
        if (cell.cover == 1 && !hard) {
            // Thin rock is cracked.
            const float sx = box.x + tile * (.25F + grain(index, 20) * .2F);
            g.line({{sx, box.y + tile * .18F},
                    {sx + tile * .14F, box.y + tile * .42F},
                    {sx + tile * .02F, box.y + tile * .6F},
                    {sx + tile * .2F, box.y + tile * .84F}},
                   std::max(1.0F, tile * .035F), alpha(shade(face, .45F), .9F));
            g.line({{sx + tile * .14F, box.y + tile * .42F}, {sx + tile * .38F, box.y + tile * .36F}},
                   std::max(1.0F, tile * .03F), alpha(shade(face, .45F), .9F));
        }
        // Thickness pips: one per remaining layer.
        for (int pip = 0; pip < cell.cover; ++pip)
            g.rect({box.x + tile * .1F + static_cast<float>(pip) * tile * .13F, box.y + tile * .8F,
                    tile * .09F, tile * .09F},
                   alpha({255, 244, 214, 255}, .7F), 1.5F);
        if (cell.info && deposit && !deposit->collected) {
            const Color accent = depositColor(*deposit);
            const float pulse = still ? .8F : .7F + .3F * std::sin(time * 3.0F + static_cast<float>(index));
            if (cell.cover == 1) {
                g.circle(center, tile * .3F, alpha(accent, .22F * pulse));
                g.picture(depositPicture(*deposit),
                          {box.x + tile * .22F, box.y + tile * .2F, tile * .56F, tile * .56F},
                          static_cast<std::uint8_t>(150 * pulse));
            } else {
                g.circle(center, tile * .2F, alpha(accent, .25F * pulse));
                g.polygon({{center.x, center.y - tile * .15F},
                           {center.x + tile * .11F, center.y},
                           {center.x, center.y + tile * .15F},
                           {center.x - tile * .11F, center.y}},
                          alpha(accent, .85F));
            }
        } else if (cell.info && !deposit && cell.cover >= 2) {
            // Scanned rock with nothing under it.
            const float r = tile * .1F;
            g.line({{center.x - r, center.y - r}, {center.x + r, center.y + r}}, std::max(1.0F, tile * .035F),
                   alpha({20, 16, 18, 255}, .45F));
            g.line({{center.x + r, center.y - r}, {center.x - r, center.y + r}}, std::max(1.0F, tile * .035F),
                   alpha({20, 16, 18, 255}, .45F));
        }
        // Rock that no strike can target yet stays in the dark.
        if (!reach[static_cast<std::size_t>(index)])
            g.rect({box.x + .5F, box.y + .5F, tile - 1, tile - 1}, {8, 8, 12, 96}, tile * .1F);
        if (locked) {
            const float size = std::max(9.0F, tile * .24F);
            g.rect({box.x + tile - size * 2.1F, box.y + tile * .08F, size * 1.9F, size * 1.25F},
                   {24, 30, 40, 220}, 3);
            g.text("T" + std::to_string(cell.grade), {box.x + tile - size * 1.15F, box.y + tile * .08F + 1},
                   size, {206, 222, 240}, .5F);
        }
    }

    // A partly exposed deposit shows how many of its cells are bare, at its first bare cell.
    for (const game::Deposit &deposit : live.deposits) {
        if (deposit.collected || deposit.cells.size() < 2)
            continue;
        int bare = 0, anchor = -1;
        for (int cell : deposit.cells)
            if (live.mine[static_cast<std::size_t>(cell)].cover == 0) {
                ++bare;
                if (anchor < 0)
                    anchor = cell;
            }
        if (anchor < 0 || !visible(layout.mineCellBounds(anchor)))
            continue;
        const Rect box = layout.mineCellBounds(anchor);
        const float size = std::max(9.0F, tile * .24F);
        const Rect badge{box.x + tile - size * 2.5F, box.y + tile - size * 1.35F, size * 2.4F, size * 1.25F};
        g.rect(badge, {16, 14, 18, 230}, size * .4F);
        g.text(std::to_string(bare) + "/" + std::to_string(deposit.cells.size()),
               {badge.x + badge.w / 2, badge.y + 1}, size, depositColor(deposit), .5F);
    }

    // In practice the vein under the guided cell is outlined with how much of it is bare, since a vein pays
    // only once every cell is exposed.
    const int guided = state.play.tutorial.active() ? state.play.tutorial.mineCell : -1;
    if (guided >= 0 && guided < static_cast<int>(live.mine.size()) && live.mine[guided].deposit) {
        const game::Deposit &vein = live.deposits[static_cast<std::size_t>(live.mine[guided].deposit - 1)];
        const auto inside = [&vein](int cell) {
            return std::find(vein.cells.begin(), vein.cells.end(), cell) != vein.cells.end();
        };
        const float pulse = still ? 1.0F : .7F + .3F * std::sin(time * 4.0F);
        const Color edge{255, 214, 110, static_cast<std::uint8_t>(255 * pulse)};
        int bare = 0;
        float top = 1e9F, left = 1e9F, bottom = -1e9F;
        for (int cell : vein.cells) {
            const Rect box = layout.mineCellBounds(cell);
            const int column = cell % columns, row = cell / columns;
            bare += live.mine[static_cast<std::size_t>(cell)].cover == 0 ? 1 : 0;
            top = std::min(top, box.y);
            bottom = std::max(bottom, box.y + tile);
            left = std::min(left, box.x);
            const float w = std::max(2.0F, tile * .07F);
            if (row == 0 || !inside(cell - columns))
                g.line({{box.x, box.y}, {box.x + tile, box.y}}, w, edge);
            if (row + 1 >= layout.mineRows || !inside(cell + columns))
                g.line({{box.x, box.y + tile}, {box.x + tile, box.y + tile}}, w, edge);
            if (column == 0 || !inside(cell - 1))
                g.line({{box.x, box.y}, {box.x, box.y + tile}}, w, edge);
            if (column + 1 >= columns || !inside(cell + 1))
                g.line({{box.x + tile, box.y}, {box.x + tile, box.y + tile}}, w, edge);
        }
        const std::string note = tr("excavation.vein", {{"bare", std::to_string(bare)},
                                                        {"total", std::to_string(vein.cells.size())}});
        const float size = std::max(10.0F, tile * .24F);
        const float width = labelWidth(note, size) + 14;
        const float noteX = std::clamp(left, view.x + 4, view.x + view.w - width - 4);
        // Above the vein unless that would cover the miner's ledge over the wall; then below it.
        const float above = top - size * 1.6F - 4;
        const float noteY = above < layout.mineCellBounds(0).y ? bottom + 4 : std::max(view.y + 4, above);
        g.rect({noteX, noteY, width, size * 1.6F}, {20, 16, 12, 230}, size * .5F);
        g.text(note, {noteX + 7, noteY + size * .25F}, size, {255, 224, 150});
    }

    wallMiner(g, run, state);

    // Strike or scan preview under the pointer.
    const engine::Widget *pointed = ui.hit(ui.pointerPosition);
    const int hovered =
        pointed && pointed->id == World && !ui.dragging ? layout.mineCell(ui.pointerPosition) : -1;
    const bool actionable = live.phase == game::Phase::Prep && !live.expansionPending && !live.rewardOffer;
    if (hovered >= 0 && actionable && state.play.scan && live.scans > 0) {
        const int reach = run.catalog().balance.scanReach;
        for (int index = 0; index < static_cast<int>(live.mine.size()); ++index)
            if (std::abs(index % columns - hovered % columns) <= reach &&
                std::abs(index / columns - hovered / columns) <= reach) {
                const Rect box = layout.mineCellBounds(index);
                g.rect({box.x + 1, box.y + 1, tile - 2, tile - 2}, alpha(theme::blue, .16F), 3);
                outline(g, {box.x + 1, box.y + 1, tile - 2, tile - 2}, 1.5F, alpha(theme::blue, .8F));
            }
    } else if (hovered >= 0 && actionable && !reach[static_cast<std::size_t>(hovered)]) {
        const Rect box = layout.mineCellBounds(hovered);
        outline(g, {box.x + 1, box.y + 1, tile - 2, tile - 2}, 2, alpha(theme::red, .85F));
    } else if (hovered >= 0 && actionable && live.digs >= run.strikeCost(tool)) {
        for (const game::StrikeHit &hit : run.strikePattern(hovered, tool)) {
            const Rect box = layout.mineCellBounds(hit.cell);
            const Color mark = hit.power > 0
                                   ? (tool == game::StrikeTool::Blast ? Color{255, 132, 84} : theme::gold)
                                   : Color{150, 150, 150};
            g.rect({box.x + 1, box.y + 1, tile - 2, tile - 2}, alpha(mark, hit.power > 0 ? .16F : .06F), 3);
            outline(g, {box.x + 1, box.y + 1, tile - 2, tile - 2}, hit.cell == hovered ? 2.5F : 1.5F,
                    alpha(mark, hit.power > 0 ? .9F : .4F));
            if (hit.power > 0 && tile >= 30)
                label(g, "-" + std::to_string(hit.power), {box.x + tile * .78F, box.y + tile * .56F},
                      std::max(10.0F, tile * .26F), {255, 244, 214});
        }
    }

    // Strike cues: dust for stripped rock, then gains and nest warnings above them.
    for (const MineCue &cue : state.play.feedback.mineCues) {
        if (cue.cell < 0 || cue.cell >= static_cast<int>(live.mine.size()))
            continue;
        const Rect box = layout.mineCellBounds(cue.cell);
        const ScreenPoint c{box.x + tile / 2, box.y + tile / 2};
        const float t = cue.age / cue.lifetime;
        if (cue.stripped > 0) {
            if (still)
                continue;
            for (int puff = 0; puff < 2 + cue.stripped; ++puff) {
                const float angle = grain(cue.cell, puff + 40) * 6.2831853F;
                const float reach = tile * (.15F + t * .45F);
                g.circle({c.x + std::cos(angle) * reach, c.y + std::sin(angle) * reach - t * tile * .15F},
                         tile * (.08F + (1 - t) * .08F), alpha({222, 208, 186, 255}, .55F * (1 - t)));
            }
            continue;
        }
        const float fade = std::clamp((1 - t) * 3.0F, 0.0F, 1.0F);
        const float rise = still ? 0.0F : std::min(t * 2.0F, 1.0F) * tile * .7F;
        const float size = std::max(12.0F, tile * .34F);
        const float x = std::clamp(c.x, view.x + size * 3.2F, view.x + view.w - size * 3.2F);
        float y = std::max(view.y + size, c.y - tile * .3F - rise);
        game::Deposit shown;
        shown.ore = cue.ore;
        shown.nest = cue.nest;
        shown.special = cue.special;
        const Color accent = depositColor(shown);
        if (!still && cue.collected && t < .35F)
            g.circle(c, tile * (.3F + t * 1.6F), alpha(accent, .3F * (1 - t / .35F)));
        if (cue.special == game::MineSpecial::Relic)
            label(g, tr("excavation.cue.relic", {{"relic", relicName(cue.relic)}}), {x, y}, size,
                  alpha(accent, fade));
        else if (cue.special == game::MineSpecial::Fossil)
            label(g, tr("excavation.cue.fossil", {{"card", cardName(cue.card)}}), {x, y}, size,
                  alpha(accent, fade));
        else if (cue.special == game::MineSpecial::Energy)
            label(g, tr("mine.next.energy", {{"amount", std::to_string(cue.amount)}}), {x, y}, size,
                  alpha(accent, fade));
        else if (cue.special == game::MineSpecial::Unstable)
            label(g, tr("mine.cue.collapse"), {x, y}, size, alpha(accent, fade));
        else if (cue.amount > 0)
            label(g, tr("mine.cue.ore", {{"amount", std::to_string(cue.amount)}, {"ore", oreName(cue.ore)}}),
                  {x, y}, size, alpha(mineOreColor(cue.ore), fade));
        if (cue.extra > 0)
            label(g, tr("excavation.cue.nest", {{"count", std::to_string(cue.extra)}}), {x, y + size * 1.25F},
                  size * .86F, alpha({255, 120, 96, 255}, fade));
    }
    // Tile information after a long press or a resting pointer: rock, tool needed and what shows through.
    if (hovered >= 0 && ui.inspected() == World) {
        const game::MineCell &cell = live.mine[static_cast<std::size_t>(hovered)];
        const game::Deposit *deposit =
            cell.deposit ? &live.deposits[static_cast<std::size_t>(cell.deposit - 1)] : nullptr;
        const bool known = cell.info || cell.cover == 0;
        std::string title = cell.cover == 0
                                ? tr("excavation.info.bare")
                                : tr("excavation.info.rock", {{"layers", std::to_string(cell.cover)}});
        if (cell.cover > 0 && cell.grade > 1)
            title = tr(cell.grade > live.tool ? "excavation.info.hard.tool" : "excavation.info.hard",
                       {{"title", title}});
        std::string detail;
        Color accent{226, 210, 180};
        if (!known)
            detail = tr("excavation.info.unknown");
        else if (!deposit || deposit->collected)
            detail = tr(deposit ? "excavation.info.mined" : "excavation.info.empty");
        else {
            accent = depositColor(*deposit);
            int bare = 0;
            for (int other : deposit->cells)
                bare += live.mine[static_cast<std::size_t>(other)].cover == 0 ? 1 : 0;
            bool progress = true;
            if (deposit->special == game::MineSpecial::Relic)
                detail = tr("excavation.info.relic", {{"relic", relicName(deposit->relic)}});
            else if (deposit->special == game::MineSpecial::Fossil)
                detail = tr("excavation.info.fossil", {{"card", cardName(deposit->card)}});
            else if (deposit->special == game::MineSpecial::Energy) {
                detail = tr("excavation.info.energy", {{"amount", std::to_string(deposit->amount)}});
                progress = false;
            } else if (deposit->special == game::MineSpecial::Unstable) {
                detail = tr("excavation.info.unstable");
                progress = false;
            } else if (deposit->nest)
                detail = tr("excavation.info.nest", {{"count", std::to_string(run.nestReinforcements())}});
            else
                detail = tr("excavation.info.ore", {{"ore", oreName(deposit->ore)}});
            if (progress && bare)
                detail = tr("excavation.info.progress", {{"detail", detail},
                                                         {"bare", std::to_string(bare)},
                                                         {"total", std::to_string(deposit->cells.size())}});
        }
        const float size = layout.compact ? 11.0F : 12.0F;
        const float width =
            std::min(view.w - 16, std::max(labelWidth(title, size + 1), labelWidth(detail, size)) + 20);
        const Rect box = layout.mineCellBounds(hovered);
        const float left = std::clamp(box.x + tile / 2 - width / 2, view.x + 8, view.x + view.w - width - 8);
        const float top = box.y > view.y + 60 ? box.y - 46 : box.y + tile + 4;
        g.rect({left, top, width, 42}, {16, 14, 20, 240}, 7);
        g.rect({left, top, 3, 42}, accent, 1);
        g.text(title, {left + 10, top + 5}, size + 1, accent, 0, width - 14);
        g.text(detail, {left + 10, top + 23}, size, {232, 222, 204}, 0, width - 14);
    }
    relicBanner(g, run, state);
    if (layout.mineScrollMax > 0) {
        if (layout.mineScroll > 1)
            g.gradientRect({view.x, view.y, view.w, tile * .5F}, {14, 12, 16, 200}, {14, 12, 16, 0});
        if (layout.mineScroll < layout.mineScrollMax - 1)
            g.gradientRect({view.x, view.y + view.h - tile * .6F, view.w, tile * .6F}, {14, 12, 16, 0},
                           {14, 12, 16, 220});
        const float trackH = view.h - 16;
        const float thumbH = std::max(24.0F, trackH * view.h / (view.h + layout.mineScrollMax));
        const float thumbY = view.y + 8 + (trackH - thumbH) * layout.mineScroll / layout.mineScrollMax;
        g.rect({view.x + view.w - 7, view.y + 8, 4, trackH}, {0, 0, 0, 90}, 2);
        g.rect({view.x + view.w - 7, thumbY, 4, thumbH}, {241, 192, 102, 200}, 2);
    }
    g.endClip();

    // Header: layer plate, the two tool toggles and the count of deposits still buried.
    const float headerY = layout.world.y + 8;
    const float limit = layout.world.x + layout.world.w - 122;
    const bool narrow = limit - layout.world.x < 250;
    const std::string code = "B" + std::to_string(layer + 1);
    const float nameW = narrow ? 34.0F : layout.compact ? 92.0F : 128.0F;
    g.rect({layout.world.x + 8, headerY, nameW, 24}, {22, 18, 18, 220}, 6);
    g.rect({layout.world.x + 8, headerY, 3, 24}, theme::gold, 1);
    g.text(narrow ? code : code + " · " + mineLayerName(definition), {layout.world.x + 17, headerY + 5},
           layout.compact ? 11 : 12, {240, 224, 190}, 0, nameW - 14);
    float x = layout.world.x + 14 + nameW;
    const float toolW = narrow ? 54.0F : 90.0F;
    for (const game::StrikeTool option : {game::StrikeTool::Pick, game::StrikeTool::Blast}) {
        const bool blast = option == game::StrikeTool::Blast;
        const bool selected = blast == state.play.blast && !state.play.scan;
        const Rect box{x, headerY - 1, toolW, 26};
        ui.add(blast ? ToolBlast : ToolPick, box, actionable);
        g.rect({box.x, box.y + 2, box.w, box.h}, {8, 10, 14, 200}, 7);
        g.rect(box, selected ? Color{248, 186, 68} : Color{45, 75, 86}, 7);
        g.rect({box.x + 1.5F, box.y + 1.5F, box.w - 3, box.h - 3},
               selected ? Color{92, 66, 30} : Color{30, 50, 58}, 6);
        g.picture(blast ? "tool-blast" : "tool-pick", {box.x + 4, box.y + 3, 20, 20});
        const std::string cost = "×" + std::to_string(run.strikeCost(option));
        g.text(narrow ? cost : tr(blast ? "excavation.tool.blast" : "excavation.tool.pick", {{"cost", cost}}),
               {box.x + 27, box.y + 6}, layout.compact ? 10 : 11,
               selected ? Color{255, 236, 190} : Color{196, 214, 212}, 0, box.w - 30);
        x += toolW + 5;
    }
    // Remaining digs stand out beside the tools, red once they run out.
    const std::string digs =
        tr(narrow ? "excavation.digs.short" : "excavation.digs", {{"count", std::to_string(live.digs)}});
    const float digsW = labelWidth(digs, 13) + 18;
    if (x + digsW <= limit) {
        const bool empty = live.digs <= 0;
        g.rect({x, headerY - 1, digsW, 26}, empty ? Color{70, 22, 18, 235} : Color{60, 44, 14, 235}, 7);
        g.rect({x, headerY + 23, digsW, 2}, empty ? theme::red : theme::gold, 1);
        g.text(digs, {x + digsW / 2, headerY + 4}, 13, empty ? Color{255, 150, 130} : Color{255, 222, 140},
               .5F);
        x += digsW + 5;
    }
    const std::string buried =
        narrow ? std::to_string(run.buriedDeposits())
               : tr("excavation.buried", {{"count", std::to_string(run.buriedDeposits())}});
    const float buriedW = labelWidth(buried, 11) + 16;
    if (x + buriedW <= limit) {
        g.rect({x, headerY, buriedW, 24}, {22, 18, 18, 220}, 6);
        g.text(buried, {x + buriedW / 2, headerY + 5}, 11, {226, 210, 180}, .5F);
    }
    // What the chosen tool does, along the bottom of the cutaway, unless a toast or the practice tips use it.
    const bool tips = state.play.tutorial.active() && layout.sidebar.h < 270;
    if (actionable && !tips && state.play.toastTime <= 0) {
        const game::RunBalance &balance = run.catalog().balance;
        const int pick = balance.pickPower + live.tool - 1, side = live.tool >= 3 ? 2 : 1;
        const int blast = balance.blastPower + (live.tool >= 2 ? 1 : 0);
        const std::string caption =
            state.play.scan    ? tr("excavation.caption.scan")
            : state.play.blast ? tr("excavation.caption.blast",
                                    {{"cost", std::to_string(run.strikeCost(game::StrikeTool::Blast))},
                                     {"layers", std::to_string(blast)}})
                               : tr("excavation.caption.pick",
                                    {{"center", std::to_string(pick)}, {"side", std::to_string(side)}});
        const float size = layout.compact ? 10.0F : 11.0F;
        const float width = std::min(view.w - 12, labelWidth(caption, size) + 18);
        const Rect plate{view.x + 6, view.y + view.h - size * 2 - 6, width, size * 2};
        g.rect(plate, {14, 18, 22, 215}, 6);
        g.text(caption, {plate.x + 9, plate.y + size * .45F}, size, {226, 214, 190}, 0, plate.w - 14);
    }
}
void View::wallMiner(engine::Renderer &g, const game::Run &run, const State &state) {
    const game::RunState &live = run.state();
    const bool still = state.app.preferences.reducedMotion;
    const float time = still ? 0.0F : state.play.visualTime;
    const float tile = layout.mineTile;
    const int columns = layout.mineColumns, rows = layout.mineRows;
    const int count = static_cast<int>(live.mine.size());
    const Rect view = layout.mineViewport;
    const Rect first = layout.mineCellBounds(0);
    const auto bare = [&](int column, int row) {
        return column >= 0 && column < columns && row >= 0 && row < rows &&
               live.mine[static_cast<std::size_t>(row * columns + column)].cover == 0;
    };
    const auto at = [&](game::Point cell) {
        return ScreenPoint{first.x + (cell.x + .5F) * tile,
                           cell.y < 0 ? first.y - tile * .3F : first.y + (cell.y + .5F) * tile};
    };

    // Props: timber posts rise and a cap beam settles in rock that was just opened, and a ladder links each
    // opening to the one above or below it.
    std::vector<float> fresh(static_cast<std::size_t>(count), 1.0F);
    for (const MineCue &cue : state.play.feedback.mineCues)
        if (cue.cell >= 0 && cue.cell < count && cue.stripped > 0)
            fresh[static_cast<std::size_t>(cue.cell)] =
                std::min(fresh[static_cast<std::size_t>(cue.cell)], cue.age / .6F);
    const Color wood{132, 90, 52}, grain{96, 64, 38};
    for (int index = 0; index < count; ++index) {
        const int column = index % columns, row = index / columns;
        if (!bare(column, row))
            continue;
        const game::MineCell &cell = live.mine[static_cast<std::size_t>(index)];
        if (cell.deposit && !live.deposits[static_cast<std::size_t>(cell.deposit - 1)].collected)
            continue;
        const Rect box = layout.mineCellBounds(index);
        if (box.y > view.y + view.h || box.y + tile < view.y)
            continue;
        const float grow = still ? 1.0F : std::clamp(fresh[static_cast<std::size_t>(index)], 0.0F, 1.0F);
        const float eased = 1 - (1 - grow) * (1 - grow);
        const float floor = box.y + tile - 1, height = tile * .84F * eased;
        g.rect({box.x + tile * .05F, floor - height, tile * .07F, height}, wood, 1);
        g.rect({box.x + tile * .88F, floor - height, tile * .07F, height}, wood, 1);
        if (grow > .6F) {
            const float settle = (grow - .6F) / .4F;
            g.rect({box.x + tile * .03F, box.y + tile * (.16F - .06F * settle), tile * .94F, tile * .08F},
                   alpha(grain, settle), 1);
        }
        const bool up = row == 0 || bare(column, row - 1), down = bare(column, row + 1);
        if (!up && !down)
            continue;
        const float cx = box.x + tile / 2;
        const float top = up ? box.y : box.y + tile * .4F;
        const float bottom = top + ((down ? box.y + tile : box.y + tile * .96F) - top) * eased;
        g.line({{cx - tile * .15F, top}, {cx - tile * .15F, bottom}}, std::max(1.0F, tile * .04F), wood);
        g.line({{cx + tile * .15F, top}, {cx + tile * .15F, bottom}}, std::max(1.0F, tile * .04F), wood);
        for (float rung = top + tile * .1F; rung < bottom; rung += tile * .2F)
            g.line({{cx - tile * .15F, rung}, {cx + tile * .15F, rung}}, std::max(1.0F, tile * .035F), grain);
    }

    // Walk: the miner keeps to the ledge above the wall and the opened cells, climbing the ladders, and
    // answers each strike from a spot beside the struck cell.
    const float now = state.play.visualTime;
    const float delta = wallClock_ < 0 ? 0.0F : std::clamp(now - wallClock_, 0.0F, .1F);
    wallClock_ = now;
    const auto standable = [&](int column, int row) {
        return column >= 0 && column < columns && (row == -1 || bare(column, row));
    };
    if (state.play.strikes != wallStrikes_) {
        wallStrikes_ = state.play.strikes;
        wallGoal_ = state.play.strikeCell;
        wallSwing_ = .8F;
        wallPath_.clear();
        if (wallGoal_ >= 0 && wallGoal_ < count) {
            const int goalColumn = wallGoal_ % columns, goalRow = wallGoal_ / columns;
            const int width = columns, height = rows + 1;
            const auto node = [width](int column, int row) { return (row + 1) * width + column; };
            int startColumn = std::clamp(static_cast<int>(std::lround(wallMiner_.x)), 0, columns - 1);
            int startRow = std::clamp(static_cast<int>(std::lround(wallMiner_.y)), -1, rows - 1);
            if (!standable(startColumn, startRow))
                startRow = -1;
            std::vector<int> previous(static_cast<std::size_t>(width * height), -2);
            std::vector<int> queue{node(startColumn, startRow)};
            previous[static_cast<std::size_t>(queue.front())] = -1;
            int found = -1;
            for (std::size_t head = 0; head < queue.size() && found < 0; ++head) {
                const int current = queue[head];
                const int column = current % width, row = current / width - 1;
                const bool beside = std::abs(column - goalColumn) + std::abs(row - goalRow) == 1 ||
                                    (column == goalColumn && row == goalRow);
                if (beside) {
                    found = current;
                    break;
                }
                const int steps[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (const auto &step : steps) {
                    const int nextColumn = column + step[0], nextRow = row + step[1];
                    if (!standable(nextColumn, nextRow))
                        continue;
                    const int next = node(nextColumn, nextRow);
                    if (previous[static_cast<std::size_t>(next)] != -2)
                        continue;
                    previous[static_cast<std::size_t>(next)] = current;
                    queue.push_back(next);
                }
            }
            if (found < 0)
                wallMiner_ = {static_cast<float>(goalColumn), -1};
            else
                for (int current = found; current >= 0; current = previous[static_cast<std::size_t>(current)])
                    wallPath_.insert(wallPath_.begin(), {static_cast<float>(current % width),
                                                         static_cast<float>(current / width - 1)});
            if (still && !wallPath_.empty()) {
                wallMiner_ = wallPath_.back();
                wallPath_.clear();
            }
        }
    }
    bool walking = false, climbing = false;
    float budget = delta * 6.0F;
    while (budget > 0 && !wallPath_.empty()) {
        const game::Point target = wallPath_.front();
        const float dx = target.x - wallMiner_.x, dy = target.y - wallMiner_.y;
        const float gap = std::sqrt(dx * dx + dy * dy);
        climbing = std::abs(dy) > std::abs(dx);
        walking = !climbing;
        if (gap <= budget) {
            wallMiner_ = target;
            wallPath_.erase(wallPath_.begin());
            budget -= gap;
        } else {
            wallMiner_ = {wallMiner_.x + dx / gap * budget, wallMiner_.y + dy / gap * budget};
            budget = 0;
        }
    }
    const bool swinging = wallPath_.empty() && wallSwing_ > 0;
    if (wallPath_.empty())
        wallSwing_ = std::max(0.0F, wallSwing_ - delta);

    // Figure: hard hat and lamp, a walking bob or a climbing reach, and a pickaxe swung toward the struck
    // cell.
    const ScreenPoint p = at(wallMiner_);
    if (p.y < view.y - tile || p.y > view.y + view.h + tile)
        return;
    float facing = 1.0F;
    bool downward = false, upward = false;
    if (wallGoal_ >= 0 && wallGoal_ < count) {
        const float goalColumn = static_cast<float>(wallGoal_ % columns);
        const float goalRow = static_cast<float>(wallGoal_ / columns);
        if (std::abs(goalColumn - wallMiner_.x) > .3F)
            facing = goalColumn < wallMiner_.x ? -1.0F : 1.0F;
        downward = std::abs(goalColumn - wallMiner_.x) < .3F && goalRow > wallMiner_.y;
        upward = std::abs(goalColumn - wallMiner_.x) < .3F && goalRow < wallMiner_.y;
    }
    const float scale = tile * .85F;
    const float bob = walking && !still ? std::abs(std::sin(time * 16.0F)) * scale * .06F : 0.0F;
    const ScreenPoint body{p.x, p.y - bob + scale * .05F};
    g.circle(p, scale * .9F, {255, 200, 120, 18});
    const float step = walking && !still ? std::sin(time * 16.0F) * scale * .05F : 0.0F;
    const float reach = climbing && !still ? std::sin(time * 14.0F) * scale * .06F : 0.0F;
    g.rect({body.x - scale * .14F + step, body.y + scale * .15F + reach, scale * .1F, scale * .2F},
           {40, 46, 52}, 2);
    g.rect({body.x + scale * .04F - step, body.y + scale * .15F - reach, scale * .1F, scale * .2F},
           {40, 46, 52}, 2);
    g.rect({body.x - scale * .19F, body.y - scale * .1F, scale * .38F, scale * .3F}, {58, 132, 112},
           scale * .07F);
    g.circle({body.x, body.y - scale * .2F}, scale * .14F, {245, 205, 160});
    g.circle({body.x + facing * scale * .06F, body.y - scale * .2F}, scale * .025F, {40, 30, 26});
    g.rect({body.x - scale * .17F, body.y - scale * .36F, scale * .34F, scale * .14F}, {241, 188, 66},
           scale * .07F);
    g.circle({body.x + facing * scale * .1F, body.y - scale * .3F}, scale * .045F, {255, 250, 220});
    if (climbing) {
        g.line({{body.x - scale * .17F, body.y - scale * .02F},
                {body.x - scale * .2F, body.y - scale * .3F - reach}},
               std::max(1.5F, scale * .06F), {245, 205, 160});
        g.line({{body.x + scale * .17F, body.y - scale * .02F},
                {body.x + scale * .2F, body.y - scale * .3F + reach}},
               std::max(1.5F, scale * .06F), {245, 205, 160});
        return;
    }
    float swing = facing > 0 ? -.6F : .6F + 3.1416F;
    if (swinging) {
        const float beat = still ? 0.0F : std::sin(time * 15.0F);
        swing = downward ? .9F + beat * .7F
                : upward ? -1.6F + beat * .5F
                         : (facing > 0 ? -.2F + beat * .9F : 3.34F - beat * .9F);
    }
    const ScreenPoint hand{body.x + facing * scale * .17F, body.y + scale * .02F};
    const float length = scale * .36F;
    const ScreenPoint tip{hand.x + std::cos(swing) * length, hand.y + std::sin(swing) * length};
    g.line({hand, tip}, std::max(1.5F, scale * .05F), {150, 101, 58});
    const float across = swing + 1.5708F;
    g.line({{tip.x - std::cos(across) * scale * .14F, tip.y - std::sin(across) * scale * .14F},
            {tip.x + std::cos(swing) * scale * .04F, tip.y + std::sin(swing) * scale * .04F},
            {tip.x + std::cos(across) * scale * .14F, tip.y + std::sin(across) * scale * .14F}},
           std::max(1.5F, scale * .06F), {206, 214, 220});
    g.circle(hand, scale * .05F, {245, 205, 160});
}
} // namespace thorium::presentation
