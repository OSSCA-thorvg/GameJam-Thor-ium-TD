#include "Assets.h"
#include "Text.h"
#include "Theme.h"
#include "View.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace thorium::presentation {
namespace {
/** Paint a small faceted core using the same original vector vocabulary as the game. */
void core(engine::Renderer &g, engine::ScreenPoint p, float size) {
    g.polygon({{p.x, p.y - size},
               {p.x + size * .7F, p.y - size * .25F},
               {p.x + size * .5F, p.y + size * .65F},
               {p.x, p.y + size},
               {p.x - size * .65F, p.y + size * .3F},
               {p.x - size * .6F, p.y - size * .35F}},
              theme::gold);
    g.polygon({{p.x, p.y - size}, {p.x + size * .7F, p.y - size * .25F}, {p.x, p.y + size * .1F}},
              {255, 241, 184});
    g.polygon({{p.x - size * .65F, p.y + size * .3F}, {p.x, p.y + size * .1F}, {p.x, p.y + size}},
              theme::copper);
}
/** Name the battlefield biome shown on the stage selector. */
std::string biomeName(game::StageBiome biome) {
    switch (biome) {
    case game::StageBiome::Obsidian:
        return tr("biome.obsidian");
    case game::StageBiome::Tundra:
        return tr("biome.tundra");
    case game::StageBiome::Lava:
        return tr("biome.lava");
    case game::StageBiome::Core:
        return tr("biome.core");
    case game::StageBiome::Crystal:
        return tr("biome.crystal");
    case game::StageBiome::Swamp:
        return tr("biome.swamp");
    case game::StageBiome::Meadow:
        break;
    }
    return tr("biome.meadow");
}
/** Title and detail lines of the stage selector card. */
std::pair<std::string, std::string> stageCard(const game::StageMetadata &meta, int unlocked) {
    const std::string id = std::to_string(meta.id);
    return {tr("home.stage.title", {{"id", id}, {"name", stageName(meta.id)}}),
            tr("home.stage.detail",
               {{"biome", biomeName(meta.biome)},
                {"waves", std::to_string(meta.wavesCount)},
                {"entrances", std::to_string(game::stageCatalog(meta.id).additionalRoutes.size() + 1)},
                {"unlocked", std::to_string(unlocked)},
                {"total", std::to_string(game::TotalStages)}})};
}
} // namespace

void View::titleScene(engine::Renderer &g, const game::Catalog &catalog, engine::Rect b, float time,
                      const AnimationSets &sets) {
    using engine::Color;
    using engine::ScreenPoint;
    panel(g, b, true);
    const float x = b.x + 7, y = b.y + 7, w = b.w - 14, h = b.h - 14;
    const float ground = y + h * .5F;
    g.beginClip({x, y, w, h});
    // Surface: two lanes from separate gates converge on the base.
    g.gradientRect({x, y, w, h * .5F}, {156, 184, 138}, {198, 210, 161});
    for (int i = 0; i < 6; ++i) {
        const float px = x + w * (.08F + i * .16F), py = y + h * (i % 2 ? .09F : .44F);
        g.circle({px, py}, h * .03F, {70, 112, 79, 130});
        g.circle({px + h * .012F, py - h * .012F}, h * .025F, {116, 160, 102});
    }
    const float lane = h * .052F;
    const std::vector<ScreenPoint> west{{x - lane, y + h * .17F},
                                        {x + w * .42F, y + h * .17F},
                                        {x + w * .42F, y + h * .34F},
                                        {x + w * .86F, y + h * .34F}};
    const std::vector<ScreenPoint> north{{x + w * .68F, y - lane}, {x + w * .68F, y + h * .34F}};
    for (const std::vector<ScreenPoint> *road : {&west, &north})
        g.line(*road, lane * 1.28F, {152, 137, 101});
    for (const std::vector<ScreenPoint> *road : {&west, &north})
        g.line(*road, lane, {235, 212, 167});
    for (const ScreenPoint gate :
         {ScreenPoint{x + w * .03F, y + h * .17F}, ScreenPoint{x + w * .68F, y + h * .04F}}) {
        g.circle(gate, lane * .75F, {154, 61, 48});
        g.circle(gate, lane * .52F, {24, 22, 26});
    }
    // The zone boss looms beyond the northern gate: a blocky stone silhouette with glowing eyes.
    const ScreenPoint boss{x + w * .86F, y + h * .1F};
    const float bulk = h * .11F;
    const Color stone{58, 52, 66, 215}, shade{44, 40, 52, 230};
    g.circle({boss.x, boss.y + bulk * .2F}, bulk * 1.5F, {196, 70, 54, 40});
    g.rect({boss.x - bulk * .75F, boss.y - bulk * .2F, bulk * 1.5F, bulk * 1.3F}, stone, bulk * .25F);
    for (const float side : {-1.0F, 1.0F}) {
        g.rect({boss.x + side * bulk * .95F - bulk * .3F, boss.y - bulk * .3F, bulk * .6F, bulk * .55F},
               shade, bulk * .2F);
        g.rect({boss.x + side * bulk * 1.05F - bulk * .2F, boss.y + bulk * .2F, bulk * .4F, bulk * .85F},
               stone, bulk * .15F);
    }
    g.rect({boss.x - bulk * .4F, boss.y - bulk * .75F, bulk * .8F, bulk * .62F}, shade, bulk * .18F);
    for (const float side : {-1.0F, 1.0F})
        g.circle({boss.x + side * bulk * .17F, boss.y - bulk * .45F}, bulk * .08F, {255, 108, 74});
    g.circle({boss.x, boss.y + bulk * .35F}, bulk * .16F, {150, 255, 200, 150});
    const std::array<std::pair<ScreenPoint, game::CardKind>, 2> towers{
        {{{x + w * .27F, y + h * .3F}, game::CardKind::Gun},
         {{x + w * .55F, y + h * .25F}, game::CardKind::Cannon}}};
    for (const auto &[point, kind] : towers) {
        g.circle(point, h * .05F, {59, 91, 75, 70});
        g.circle(point, h * .042F, {236, 231, 193});
        icon(g, catalog, kind, point, h * .1F);
    }
    icon(g, catalog, game::CardKind::Guard, {x + w * .6F, y + h * .335F}, h * .085F);
    // Three distinct enemy roles walk the two lanes on independent loops.
    const float actor = h * .085F;
    const auto walker = [&](const char *key, std::uint32_t id, ScreenPoint at, float scale) {
        const ActorAnimationSet *clips = sets.find(key);
        if (clips)
            g.animation(engine::loopingSample(clips->move, time), id,
                        {at.x - actor * scale / 2, at.y - actor * scale * .7F, actor * scale, actor * scale},
                        engine::AnimationBackend::Vector);
    };
    const float west1 = std::fmod(time * .07F, 1.0F), west2 = std::fmod(time * .07F + .45F, 1.0F);
    walker("oreling", 0, {x + w * (.04F + .36F * west1), y + h * .17F}, 1.0F);
    walker("ironback", 1, {x + w * (.04F + .36F * west2), y + h * .17F}, 1.25F);
    walker("dashling", 2, {x + w * .68F, y + h * (.05F + .24F * std::fmod(time * .11F, 1.0F))}, 1.0F);
    const ScreenPoint base{x + w * .87F, y + h * .33F};
    g.rect({base.x - h * .05F, base.y - h * .045F, h * .1F, h * .12F}, {241, 232, 197}, h * .02F);
    g.rect({base.x - h * .034F, base.y - h * .075F, h * .068F, h * .035F}, theme::green, h * .008F);
    core(g, base, h * .031F);

    // Cutaway: strata, a shaft from the surface and branching tunnels across a 7-wide board.
    const std::array<Color, 3> strata{{{150, 116, 82}, {118, 92, 74}, {88, 74, 92}}};
    const float band = h * .5F / 3;
    for (int row = 0; row < 3; ++row)
        g.rect({x, ground + band * row, w, band + 1}, strata[row], 0);
    g.gradientRect({x, ground, w, h * .03F}, {70, 56, 40, 160}, {70, 56, 40, 0});
    const int columns = 7;
    const float cell = std::min(w * .8F / columns, band * .86F);
    const float left = x + (w - cell * columns) / 2;
    const auto cellAt = [&](int col, int row) -> engine::Rect {
        return {left + col * cell, ground + band * row + (band - cell) / 2, cell, cell};
    };
    // Dug cells: the shaft in column 3, a long gallery on row 1 and a deeper pocket on row 2.
    const auto dug = [](int col, int row) -> bool {
        return col == 3 || (row == 1 && col >= 1 && col <= 5) || (row == 2 && col >= 4 && col <= 5);
    };
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < columns; ++col) {
            const engine::Rect r = cellAt(col, row);
            if (dug(col, row)) {
                g.rect({r.x - 1, r.y + 2, r.w + 2, r.h - 4}, {32, 34, 38}, 3);
                if (col + 1 < columns && dug(col + 1, row))
                    g.rect({r.x + r.w - 3, r.y + 2, 6, r.h - 4}, {32, 34, 38}, 0);
                continue;
            }
            g.rect({r.x + 2, r.y + 2, r.w - 4, r.h - 4}, {0, 0, 0, 28}, 4);
            g.line({{r.x + 6, r.y + 4}, {r.x + r.w * .5F, r.y + 4}}, 1, {255, 230, 190, 50});
            if ((row * 5 + col * 3) % 4 == 0) {
                const Color ore = row == 2 ? theme::purple : (col % 2 ? theme::gold : Color{173, 200, 207});
                const ScreenPoint p{r.x + r.w * .5F, r.y + r.h * .5F};
                g.polygon({{p.x, p.y - r.h * .2F},
                           {p.x + r.w * .17F, p.y},
                           {p.x, p.y + r.h * .2F},
                           {p.x - r.w * .13F, p.y}},
                          ore);
            }
        }
    const engine::Rect shaft = cellAt(3, 0);
    for (float rung = ground + 4; rung < shaft.y + band * 2; rung += cell * .3F)
        g.line({{shaft.x + shaft.w * .3F, rung}, {shaft.x + shaft.w * .7F, rung}}, 1.5F, {150, 120, 80});
    // The miner works the end of the gallery with a headlamp cone.
    const engine::Rect end = cellAt(5, 1);
    const ScreenPoint miner{end.x + end.w * .4F, end.y + end.h * .58F};
    g.polygon({{miner.x + cell * .1F, miner.y - cell * .22F},
               {end.x + end.w + cell * .55F, miner.y - cell * .5F},
               {end.x + end.w + cell * .55F, miner.y + cell * .15F}},
              {255, 230, 150, 60});
    icon(g, catalog, game::CardKind::Guard, miner, cell * .6F);
    g.rect({miner.x - cell * .22F, miner.y - cell * .28F, cell * .44F, cell * .13F}, theme::gold, 3);
    g.circle({miner.x + cell * .12F, miner.y - cell * .22F}, cell * .06F, theme::text);
    g.rect({x + 9, y + 8, w * .43F, 23}, {24, 53, 45, 220}, 5);
    g.text(tr("home.art.surface"), {x + 19, y + 14}, layout.compact ? 10 : 12, theme::text, 0, w * .43F - 14);
    g.text(tr("home.art.underground"), {x + 12, ground + 6}, layout.compact ? 10 : 12, {241, 218, 173});
    g.endClip();
}

void View::frontEnd(engine::Renderer &g, engine::Ui &ui, const game::Run &run, const State &state,
                    const engine::AudioSettings &audio) {
    layout.compact = g.width < 980 || g.height < 720;
    ui.beginFrame();
    g.gradientRect({0, 0, static_cast<float>(g.width), static_cast<float>(g.height)}, {28, 49, 58},
                   theme::background);
    for (int i = 0; i < 36; ++i) {
        const float x = static_cast<float>((i * 193 + 39) % g.width),
                    y = static_cast<float>((i * 113 + 17) % g.height);
        g.circle({x, y}, i % 3 ? 1.0F : 2.0F, {196, 174, 114, static_cast<std::uint8_t>(i % 3 ? 20 : 35)});
    }
    const bool compact = layout.compact;
    const Screen screen = state.app.navigation.current();
    const AnimationSets &ambient = state.app.animationSets;
    if (screen == Screen::Home) {
        const bool continuing = state.app.navigation.hasRun();
        const std::string status =
            tr("home.status",
               {{"phase", run.state().phase == game::Phase::Ready
                              ? tr("home.status.ready")
                              : tr("home.status.wave", {{"wave", std::to_string(run.state().wave)}})},
                {"stage", stageName(state.play.currentStage)},
                {"hp", std::to_string(run.state().baseHp)},
                {"max", std::to_string(run.catalog().balance.baseHp)}});
        const std::string continueLabel =
            continuing ? tr("home.continue", {{"id", std::to_string(state.play.currentStage)}})
                       : tr("home.start");
        const std::string practiceLabel =
            state.app.tutorialCompleted ? tr("home.practice.again") : tr("home.practice.first");
        const float time = state.app.preferences.reducedMotion ? 0 : state.app.uiTime;
        if (compact) {
            const float artY = g.height > 700 ? 120.0F : 90.0F;
            const float artHeight = std::clamp(g.height - (continuing ? 480.0F : 430.0F), 100.0F, 300.0F);
            g.text("THOR-IUM TD", {g.width / 2.0F, artY - 77}, g.height > 700 ? 33 : 29, theme::text, .5F);
            g.text(tr("home.tagline"), {g.width / 2.0F, artY - 31}, 13, theme::gold, .5F, g.width - 32.0F);
            titleScene(g, run.catalog(), {16, artY, g.width - 32.0F, artHeight}, time, ambient);
            float y = artY + artHeight + 14;

            // Stage Selector Card
            const auto &meta = game::stageMetadata(state.app.selectedStage);
            const float cardW = g.width - 32.0F;
            // Tall screens add the stage's gimmick line to the card.
            const bool gimmickLine = g.height > 700 && !meta.gimmick.empty();
            const float cardH = gimmickLine ? 64.0F : 48.0F;
            g.rect({16, y, cardW, cardH}, {18, 34, 42}, 6);
            g.rect({16, y, 4.0F, cardH}, theme::gold, 2);
            const bool canPrev = state.app.campaign && state.app.selectedStage > 1;
            const bool canNext =
                state.app.campaign &&
                state.app.selectedStage < std::min(game::TotalStages, state.app.highestUnlockedStage);
            button(g, ui, SelectStagePrev, {22, y + 8, 30, 32}, "<", canPrev);
            button(g, ui, SelectStageNext, {16 + cardW - 52, y + 8, 30, 32}, ">", canNext);
            const auto [stageTitle, stageSub] = stageCard(meta, state.app.highestUnlockedStage);
            g.text(stageTitle, {g.width / 2.0F, y + 9}, 13, theme::gold, .5F, cardW - 100);
            g.text(stageSub, {g.width / 2.0F, y + 27}, 10.5F, theme::muted, .5F, cardW - 100);
            if (gimmickLine)
                g.text(stageGimmick(meta.id), {g.width / 2.0F, y + 44}, 10.5F, theme::green, .5F,
                       cardW - 100);
            y += cardH + 10;

            if (continuing) {
                g.text(status, {g.width / 2.0F, y - 2}, 12, theme::muted, .5F);
                y += 20;
            }
            const float h = g.height > 700 ? 50.0F : 44.0F;
            button(g, ui, continuing ? HomeContinue : HomeNew, {16, y, g.width - 32.0F, h}, continueLabel,
                   true, continuing || state.app.tutorialCompleted);
            y += h + 10;
            button(g, ui, BeginTutorial, {16, y, g.width - 32.0F, 34}, practiceLabel, true,
                   !state.app.tutorialCompleted);
            y += 42;
            if (continuing) {
                button(g, ui, HomeNew, {16, y, g.width - 32.0F, 34}, tr("home.newRun"));
                y += 44;
            }
            const float half = (g.width - 42.0F) / 2;
            button(g, ui, OpenSettings, {16, y, half, 38}, tr("menu.settings"));
            button(g, ui, OpenGuide, {26 + half, y, half, 38}, tr("menu.guide"));
            y += 48;
            button(g, ui, OpenCredits, {16, y, g.width - 32.0F, 32}, tr("menu.credits"));
        } else {
            const float x = std::max(48.0F, g.width * .05F), w = std::min(390.0F, g.width * .36F);
            const float top = std::max(80.0F, (g.height - 670.0F) / 2);
            core(g, {x + 17, top - 28}, 17);
            g.text("THORIUM EXPEDITION", {x + 45, top - 37}, 11, theme::muted);
            g.text("THOR-IUM", {x, top + 20}, 54, theme::text, 0, w + 65);
            g.text("TD", {x, top + 84}, 54, theme::gold);
            g.text(tr("home.tagline"), {x, top + 167}, 20, theme::text, 0, w + 35);
            g.text(tr("home.subtitle"), {x, top + 207}, 12, theme::muted, 0, w + 35);
            float y = top + 248;

            // Stage Selector Card
            const auto &meta = game::stageMetadata(state.app.selectedStage);
            const float cardH = meta.gimmick.empty() ? 54.0F : 72.0F;
            g.rect({x, y, w, cardH}, {18, 34, 42}, 6);
            g.rect({x, y, 4.0F, cardH}, theme::gold, 2);
            const bool canPrev = state.app.campaign && state.app.selectedStage > 1;
            const bool canNext =
                state.app.campaign &&
                state.app.selectedStage < std::min(game::TotalStages, state.app.highestUnlockedStage);
            button(g, ui, SelectStagePrev, {x + 8, y + 10, 32, 34}, "<", canPrev);
            button(g, ui, SelectStageNext, {x + w - 40, y + 10, 32, 34}, ">", canNext);
            const auto [stageTitle, stageSub] = stageCard(meta, state.app.highestUnlockedStage);
            g.text(stageTitle, {x + w / 2.0F, y + 11}, 14, theme::gold, .5F, w - 96);
            g.text(stageSub, {x + w / 2.0F, y + 31}, 11, theme::muted, .5F, w - 96);
            if (!meta.gimmick.empty())
                g.text(stageGimmick(meta.id), {x + w / 2.0F, y + 50}, 11, theme::green, .5F, w - 96);
            y += cardH + 14;

            button(g, ui, continuing ? HomeContinue : HomeNew, {x, y, w, 52}, continueLabel, true,
                   continuing || state.app.tutorialCompleted);
            y += 62;
            button(g, ui, BeginTutorial, {x, y, w, 36}, practiceLabel, true, !state.app.tutorialCompleted);
            y += 46;
            if (continuing) {
                g.text(status, {x, y}, 12, theme::muted, 0, w);
                y += 24;
                button(g, ui, HomeNew, {x, y, w, 38}, tr("home.newRun"));
                y += 48;
            }
            const float half = (w - 12) / 2;
            button(g, ui, OpenSettings, {x, y, half, 40}, tr("menu.settings"));
            button(g, ui, OpenGuide, {x + half + 12, y, half, 40}, tr("menu.guide"));
            y += 50;
            button(g, ui, OpenCredits, {x, y, w, 34}, tr("menu.credits"));
            const float artX = x + w + 70;
            titleScene(g, run.catalog(),
                       {artX, std::max(80.0F, (g.height - 570) / 2.0F), g.width - artX - x,
                        std::min(570.0F, g.height - 150.0F)},
                       time, ambient);
        }
        g.text(tr("home.footer", {{"stages", std::to_string(game::TotalStages)}}),
               {g.width / 2.0F, g.height - 26.0F}, compact ? 8 : 10, theme::muted, .5F);
        const float globeWidth = compact ? 100.0F : 116.0F;
        languageButton(g, ui, {g.width - globeWidth - 16.0F, 16.0F, globeWidth, 34.0F});
        if (!ui.focused)
            ui.focused = continuing ? HomeContinue : state.app.tutorialCompleted ? HomeNew : BeginTutorial;
        if (state.play.modal != presentation::Modal::None)
            overlay(g, ui, run, state, audio);
        return;
    }
    const float width = std::min(880.0F, g.width - (compact ? 24.0F : 96.0F));
    const float left = (g.width - width) / 2;
    const float top = compact ? 96.0F : 126.0F;
    const engine::Rect box{left, top, width, g.height - top - (compact ? 82.0F : 100.0F)};
    const std::string title = tr(screen == Screen::Settings ? "menu.settings"
                                 : screen == Screen::Guide  ? "menu.guide"
                                                            : "menu.credits");
    g.text(title, {left, compact ? 27.0F : 40.0F}, compact ? 28 : 36, theme::text, 0, width - 96);
    button(g, ui, GoHome, {left + width - 80, compact ? 27.0F : 40.0F, 80, 34}, tr("menu.home"));
    g.text(tr(screen == Screen::Settings
                  ? (state.app.navigation.hasRun() ? "settings.subtitle.paused" : "settings.subtitle")
              : screen == Screen::Guide ? "guide.subtitle"
                                        : "credits.subtitle"),
           {left, compact ? 67.0F : 88.0F}, compact ? 12 : 14, theme::muted, 0, width);
    panel(g, box, true);
    const float x = box.x + 18, w = box.w - 36;
    if (screen == Screen::Settings) {
        const float half = (w - 10) / 2;
        const float quarter = (w - 24) / 4;
        const std::array<std::pair<Action, const char *>, 4> tabs{
            {{SettingsAudio, "settings.tab.audio"},
             {SettingsVisual, compact ? "settings.tab.visualShort" : "settings.tab.visual"},
             {SettingsGame, "settings.tab.game"},
             {SettingsLanguage, "settings.tab.language"}}};
        for (std::size_t index = 0; index < tabs.size(); ++index)
            button(g, ui, tabs[index].first, {x + (quarter + 8) * index, box.y + 16, quarter, 34},
                   tr(tabs[index].second), true, state.app.settingsTab == static_cast<int>(index));
        if (state.app.settingsTab == 3) {
            g.text(tr("settings.language.title"), {x, box.y + 77}, compact ? 20 : 25, theme::gold, 0, w);
            const auto &list = languages();
            for (std::size_t index = 0; index < list.size(); ++index)
                button(g, ui, LanguageBase + static_cast<int>(index), {x, box.y + 122 + index * 50.0F, w, 40},
                       list[index].name, true, language() == list[index].code);
            g.text(tr("settings.language.note"), {x, box.y + 132 + list.size() * 50.0F}, compact ? 10 : 12,
                   theme::muted, 0, w);
        } else if (state.app.settingsTab == 2) {
            const bool excavation = state.app.preferences.excavation;
            g.text(tr("settings.mining.title"), {x, box.y + 77}, compact ? 20 : 25, theme::gold);
            button(g, ui, ToggleMining, {x, box.y + 116, w, 40},
                   tr(excavation ? "settings.mining.toSweep" : "settings.mining.toWall"), true, excavation);
            g.text(tr(excavation ? "settings.mining.wall" : "settings.mining.sweep"), {x, box.y + 172},
                   compact ? 10 : 12, theme::text, 0, w);
            g.text(tr("settings.mining.note"), {x, box.y + 214}, compact ? 10 : 12, theme::muted, 0, w);
        } else if (state.app.settingsTab == 0) {
            g.text(tr("settings.audio.title"), {x, box.y + 77}, compact ? 20 : 25, theme::gold);
            g.text(tr("settings.audio.play"), {x, box.y + 122}, 14, theme::text, 0, w - 120);
            button(g, ui, AudioMute, {x + w - 110, box.y + 111, 110, 36},
                   tr(audio.muted ? "settings.on" : "settings.off"), true, !audio.muted);
            g.text(tr("settings.audio.volume"), {x, box.y + 170}, 14, theme::text);
            g.text(std::to_string(static_cast<int>(audio.volume * 100)) + "%", {x + w, box.y + 169}, 16,
                   theme::gold, 1);
            g.rect({x, box.y + 199, w, 7}, theme::inset, 3);
            g.rect({x, box.y + 199, w * audio.volume, 7}, audio.muted ? theme::muted : theme::gold, 3);
            button(g, ui, AudioDown, {x, box.y + 222, half, 38}, tr("settings.audio.down"), audio.volume > 0);
            button(g, ui, AudioUp, {x + half + 10, box.y + 222, half, 38}, tr("settings.audio.up"),
                   audio.volume < 1);
            g.text(tr("settings.audio.music"), {x, box.y + 280}, 14, theme::text);
            g.text(std::to_string(static_cast<int>(std::lround(audio.music * 100))) + "%",
                   {x + w, box.y + 279}, 16, theme::gold, 1);
            const float third = (w - 16) / 3;
            button(g, ui, MusicDown, {x, box.y + 306, third, 36}, "-", audio.music > 0);
            button(g, ui, MusicUp, {x + third + 8, box.y + 306, third, 36}, "+", audio.music < 1);
            button(g, ui, AudioTest, {x + (third + 8) * 2, box.y + 306, third, 36}, tr("settings.audio.test"),
                   !audio.muted && audio.volume > 0, true);
        } else {
            g.text(tr("settings.visual.title"), {x, box.y + 77}, compact ? 20 : 25, theme::gold);
            g.text(tr("settings.visual.motion"), {x, box.y + 124}, compact ? 13 : 16, theme::text, 0,
                   w - 100);
            button(g, ui, ToggleMotion, {x + w - 88, box.y + 112, 88, 36},
                   tr(state.app.preferences.reducedMotion ? "settings.enabled" : "settings.disabled"), true,
                   state.app.preferences.reducedMotion);
            g.text(tr("settings.visual.motionNote"), {x, box.y + 165}, compact ? 10 : 12, theme::muted, 0, w);
            g.text(tr("settings.visual.damage"), {x, box.y + 220}, compact ? 13 : 16, theme::text, 0,
                   w - 100);
            button(g, ui, ToggleDamage, {x + w - 88, box.y + 208, 88, 36},
                   tr(state.app.preferences.damageNumbers ? "settings.shown" : "settings.hidden"), true,
                   state.app.preferences.damageNumbers);
            g.text(tr("settings.visual.damageNote"), {x, box.y + 261}, compact ? 10 : 12, theme::muted, 0, w);
            g.text(tr("settings.visual.animation"), {x, box.y + 281}, compact ? 10 : 12, theme::muted);
            button(g, ui, ToggleAnimationBackend, {x, box.y + 303, half, 38},
                   tr(state.app.preferences.cachedAnimations ? "settings.visual.cached"
                                                             : "settings.visual.vector"),
                   true, state.app.preferences.cachedAnimations);
            button(g, ui, OpenGuide, {x + half + 10, box.y + 303, half, 38}, tr("menu.guide"));
        }
        if (box.h > 540) {
            g.text(tr(state.app.settingsTab == 1 ? "settings.preview.visual" : "settings.preview.game"),
                   {x, box.y + 365}, 11, theme::muted);
            const engine::Rect preview{x, box.y + 384, w, box.h - 402};
            titleScene(g, run.catalog(), preview, state.app.preferences.reducedMotion ? 0 : state.app.uiTime,
                       ambient);
            if (state.app.settingsTab == 1 && state.app.preferences.damageNumbers)
                g.text("12", {preview.x + preview.w * .4F, preview.y + preview.h * .18F}, 16, theme::text,
                       .5F);
        }
        button(g, ui, NavigateBack, {left, g.height - 64.0F, (width - 12) / 2, 42}, tr("menu.back"));
        button(g, ui, SettingsDefaults, {left + (width + 12) / 2, g.height - 64.0F, (width - 12) / 2, 42},
               tr("settings.defaults"));
    } else if (screen == Screen::Guide) {
        const std::string page = std::to_string(state.app.guidePage + 1);
        g.text(tr("guide.page." + page), {x, box.y + 22}, compact ? 21 : 28, theme::gold, 0, w);
        const engine::ScreenPoint p{box.x + box.w / 2, box.y + 114};
        if (state.app.guidePage == 0) {
            icon(g, run.catalog(), game::CardKind::Gun, {p.x - 38, p.y}, 60);
            icon(g, run.catalog(), game::CardKind::Guard, {p.x + 38, p.y}, 50);
        } else if (state.app.guidePage == 1)
            core(g, p, 35);
        else {
            icon(g, run.catalog(), game::CardKind::Lightning, {p.x - 38, p.y}, 60);
            core(g, {p.x + 38, p.y}, 24);
        }
        // The underground page follows the mining mode chosen for new expeditions.
        const bool wall = state.app.guidePage == 1 && state.app.preferences.excavation;
        const std::string lines = "guide." + (wall ? std::string("wall") : page) + ".";
        for (int index = 0; index < 4; ++index) {
            const float y = box.y + 183 + index * 31;
            g.circle({x + 3, y + 6}, 2, theme::gold);
            g.text(tr(lines + std::to_string(index + 1)), {x + 15, y}, compact ? 10 : 14, theme::text, 0,
                   w - 15);
        }
        if (!compact && box.h > 480) {
            g.text(tr("guide.keys.1"), {x, box.y + 337}, 13, theme::muted, 0, w);
            g.text(tr("guide.keys.2"), {x, box.y + 365}, 13, theme::muted, 0, w);
        }
        const float half = (w - 10) / 2;
        button(g, ui, GuidePrevious, {x, box.y + box.h - 54, half, 36}, tr("guide.previous"),
               state.app.guidePage > 0);
        button(g, ui, GuideNext, {x + half + 10, box.y + box.h - 54, half, 36}, tr("guide.next"),
               state.app.guidePage < 2, true);
        button(g, ui, NavigateBack, {left, g.height - 64.0F, (width - 10) / 2, 42}, tr("menu.back"));
        button(g, ui, BeginTutorial, {left + (width + 10) / 2, g.height - 64.0F, (width - 10) / 2, 42},
               tr("guide.practice"), true, true);
    } else {
        core(g, {box.x + box.w / 2, box.y + 58}, 27);
        g.text("THOR-IUM TD", {box.x + box.w / 2, box.y + 102}, compact ? 25 : 34, theme::text, .5F);
        g.text("OSSCA · ThorVG GameJam", {box.x + box.w / 2, box.y + 145}, compact ? 12 : 16, theme::gold,
               .5F);
        for (int index = 0; index < 7; ++index) {
            const float y = box.y + 190 + index * (compact ? 26.0F : 33.0F);
            g.line({{x, y - 10}, {x + w, y - 10}}, 1, {86, 111, 113, 90});
            g.text(tr("credits." + std::to_string(index + 1)), {x, y}, compact ? 12 : 16, theme::muted, 0, w);
        }
        // Live renderer counters followed by the ThorVG features in use; lines that would leave the panel
        // are skipped so small screens keep the credits list alone.
        const engine::AnimationRenderStats stats = g.animationStats();
        const std::array<std::string, 5> showcase{
            {tr("credits.thorvg"), tr("credits.thorvg.1"), tr("credits.thorvg.2"),
             tr("credits.thorvg.stats", {{"vector", std::to_string(stats.vectorDraws)},
                                         {"cached", std::to_string(stats.cachedDraws)},
                                         {"fallbacks", std::to_string(stats.fallbacks)}}),
             tr("credits.thorvg.layers", {{"reused", std::to_string(g.layerReuses())},
                                          {"drawn", std::to_string(g.layerRasterizations())}})}};
        const float showcaseTop = box.y + 190 + 7 * (compact ? 26.0F : 33.0F);
        const float step = 20.0F;
        for (int index = 0; index < 5; ++index) {
            const float y = showcaseTop + index * step;
            if (y + step > box.y + box.h)
                break;
            g.text(showcase[static_cast<std::size_t>(index)], {x, y}, index == 0 ? 15 : 13,
                   index == 0 ? theme::gold : theme::muted, 0, w);
        }
        button(g, ui, NavigateBack, {left, g.height - 64.0F, width, 42}, tr("menu.back"));
    }
    if (!ui.focused)
        ui.focused = NavigateBack;
}
} // namespace thorium::presentation
