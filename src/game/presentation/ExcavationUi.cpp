#include "Actions.h"
#include "MineArt.h"
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
            label(g, std::string("유물 · ") + relicName(cue.relic), {x, y}, size, alpha(accent, fade));
        else if (cue.special == game::MineSpecial::Fossil)
            label(g, run.catalog().card(cue.card).name + " 카드 화석", {x, y}, size, alpha(accent, fade));
        else if (cue.special == game::MineSpecial::Energy)
            label(g, "다음 전투 에너지 +" + std::to_string(cue.amount), {x, y}, size, alpha(accent, fade));
        else if (cue.special == game::MineSpecial::Unstable)
            label(g, "붕괴!", {x, y}, size, alpha(accent, fade));
        else if (cue.amount > 0)
            label(g, "+" + std::to_string(cue.amount) + " " + mineOreName(cue.ore), {x, y}, size,
                  alpha(mineOreColor(cue.ore), fade));
        if (cue.extra > 0)
            label(g, "둥지 각성 · 다음 웨이브 +" + std::to_string(cue.extra), {x, y + size * 1.25F},
                  size * .86F, alpha({255, 120, 96, 255}, fade));
    }
    // Tile information after a long press or a resting pointer: rock, tool needed and what shows through.
    if (hovered >= 0 && ui.inspected() == World) {
        const game::MineCell &cell = live.mine[static_cast<std::size_t>(hovered)];
        const game::Deposit *deposit =
            cell.deposit ? &live.deposits[static_cast<std::size_t>(cell.deposit - 1)] : nullptr;
        const bool known = cell.info || cell.cover == 0;
        std::string title = cell.cover == 0 ? "드러난 곳" : "암석 " + std::to_string(cell.cover) + "겹";
        if (cell.cover > 0 && cell.grade > 1)
            title += cell.grade > live.tool ? " · 단단함 (T2 곡괭이 필요)" : " · 단단함";
        std::string detail;
        Color accent{226, 210, 180};
        if (!known)
            detail = "속이 보이지 않아요. 한 겹만 남기거나 스캔하면 보여요";
        else if (!deposit || deposit->collected)
            detail = deposit ? "이미 캐낸 자리예요" : "아무것도 묻혀 있지 않아요";
        else {
            accent = depositColor(*deposit);
            int bare = 0;
            for (int other : deposit->cells)
                bare += live.mine[static_cast<std::size_t>(other)].cover == 0 ? 1 : 0;
            const std::string progress = bare ? " · " + std::to_string(bare) + "/" +
                                                    std::to_string(deposit->cells.size()) + "칸 드러남"
                                              : "";
            if (deposit->special == game::MineSpecial::Relic)
                detail = std::string("보물 상자 · ") + relicName(deposit->relic) + progress;
            else if (deposit->special == game::MineSpecial::Fossil)
                detail = "카드 화석 · " + run.catalog().card(deposit->card).name + progress;
            else if (deposit->special == game::MineSpecial::Energy)
                detail = "에너지 수정 · 다음 전투 에너지 +" + std::to_string(deposit->amount);
            else if (deposit->special == game::MineSpecial::Unstable)
                detail = "붕괴 암반 · 드러나면 주변 8칸이 한 겹씩 무너져요";
            else if (deposit->nest)
                detail = std::string("둥지 · 전부 드러나거나 발파가 닿으면 다음 웨이브 +") +
                         std::to_string(run.nestReinforcements()) + progress;
            else
                detail = std::string(mineOreName(deposit->ore)) + " 광맥 · 전부 드러내면 획득" + progress;
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
    g.text(narrow ? code : code + " · " + definition.name, {layout.world.x + 17, headerY + 5},
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
        g.text(narrow ? cost : std::string(blast ? "발파 " : "곡괭이 ") + cost, {box.x + 27, box.y + 6},
               layout.compact ? 10 : 11, selected ? Color{255, 236, 190} : Color{196, 214, 212}, 0,
               box.w - 30);
        x += toolW + 5;
    }
    const std::string buried = (narrow ? "" : "매장물 ") + std::to_string(run.buriedDeposits());
    const float buriedW = labelWidth(buried, 11) + 16;
    if (x + buriedW <= limit) {
        g.rect({x, headerY, buriedW, 24}, {22, 18, 18, 220}, 6);
        g.text(buried, {x + buriedW / 2, headerY + 5}, 11, {226, 210, 180}, .5F);
    }
}
} // namespace thorium::presentation
