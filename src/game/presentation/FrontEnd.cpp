#include "Assets.h"
#include "Theme.h"
#include "View.h"
#include <algorithm>
#include <array>
#include <cmath>

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
const char *biomeName(game::StageBiome biome) {
    switch (biome) {
    case game::StageBiome::Obsidian:
        return "흑요석 지대";
    case game::StageBiome::Tundra:
        return "동토 갱도";
    case game::StageBiome::Lava:
        return "용암 심층";
    case game::StageBiome::Core:
        return "토르륨 핵";
    case game::StageBiome::Crystal:
        return "수정 공명굴";
    case game::StageBiome::Swamp:
        return "산성 늪지";
    case game::StageBiome::Meadow:
        break;
    }
    return "표층 초원";
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
    g.text("지상에서는 방어", {x + 19, y + 14}, layout.compact ? 10 : 12, theme::text);
    g.text("지하에서는 탐사", {x + 12, ground + 6}, layout.compact ? 10 : 12, {241, 218, 173});
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
            (run.state().phase == game::Phase::Ready ? std::string("출격 준비")
                                                     : "웨이브 " + std::to_string(run.state().wave)) +
            "  ·  " + game::stageMetadata(state.play.currentStage).name + " · 거점 " +
            std::to_string(run.state().baseHp) + " / " + std::to_string(run.catalog().balance.baseHp);
        const float time = state.app.preferences.reducedMotion ? 0 : state.app.uiTime;
        if (compact) {
            const float artY = g.height > 700 ? 120.0F : 90.0F;
            const float artHeight = std::clamp(g.height - (continuing ? 480.0F : 430.0F), 100.0F, 300.0F);
            g.text("THOR-IUM TD", {g.width / 2.0F, artY - 77}, g.height > 700 ? 33 : 29, theme::text, .5F);
            g.text("깊이 캐고, 끝까지 지켜라.", {g.width / 2.0F, artY - 31}, 13, theme::gold, .5F);
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
            const std::string stageTitle = "제 " + std::to_string(meta.id) + "구역 · " + meta.name;
            const std::string stageSub =
                std::string(biomeName(meta.biome)) + " · " + std::to_string(meta.wavesCount) + "웨이브 · " +
                std::to_string(game::stageCatalog(meta.id).additionalRoutes.size() + 1) + "입구 · " +
                std::to_string(state.app.highestUnlockedStage) + "/" + std::to_string(game::TotalStages) +
                " 해금";
            g.text(stageTitle, {g.width / 2.0F, y + 9}, 13, theme::gold, .5F, cardW - 100);
            g.text(stageSub, {g.width / 2.0F, y + 27}, 10.5F, theme::muted, .5F, cardW - 100);
            if (gimmickLine)
                g.text(meta.gimmick, {g.width / 2.0F, y + 44}, 10.5F, theme::green, .5F, cardW - 100);
            y += cardH + 10;

            if (continuing) {
                g.text(status, {g.width / 2.0F, y - 2}, 12, theme::muted, .5F);
                y += 20;
            }
            const float h = g.height > 700 ? 50.0F : 44.0F;
            button(g, ui, continuing ? HomeContinue : HomeNew, {16, y, g.width - 32.0F, h},
                   continuing ? "제 " + std::to_string(state.play.currentStage) + "구역 계속하기"
                              : "작전 개시",
                   true, true);
            y += h + 10;
            button(g, ui, BeginTutorial, {16, y, g.width - 32.0F, 34},
                   state.app.tutorialCompleted ? "연습 원정 다시하기" : "연습 원정 · 처음이라면 추천", true,
                   !state.app.tutorialCompleted);
            y += 42;
            if (continuing) {
                button(g, ui, HomeNew, {16, y, g.width - 32.0F, 34}, "선택 구역으로 새 원정 시작");
                y += 44;
            }
            const float half = (g.width - 42.0F) / 2;
            button(g, ui, OpenSettings, {16, y, half, 38}, "설정");
            button(g, ui, OpenGuide, {26 + half, y, half, 38}, "플레이 방법");
            y += 48;
            button(g, ui, OpenCredits, {16, y, g.width - 32.0F, 32}, "크레딧");
        } else {
            const float x = std::max(48.0F, g.width * .05F), w = std::min(390.0F, g.width * .36F);
            const float top = std::max(80.0F, (g.height - 670.0F) / 2);
            core(g, {x + 17, top - 28}, 17);
            g.text("THORIUM EXPEDITION", {x + 45, top - 37}, 11, theme::muted);
            g.text("THOR-IUM", {x, top + 20}, 54, theme::text, 0, w + 65);
            g.text("TD", {x, top + 84}, 54, theme::gold);
            g.text("깊이 캐고, 끝까지 지켜라.", {x, top + 167}, 20, theme::text);
            g.text("카드로 방어하고, 광물로 다음 공세를 준비하세요.", {x, top + 207}, 12, theme::muted, 0,
                   w + 35);
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
            const std::string stageTitle = "제 " + std::to_string(meta.id) + "구역 · " + meta.name;
            const std::string stageSub =
                std::string(biomeName(meta.biome)) + " · " + std::to_string(meta.wavesCount) + "웨이브 · " +
                std::to_string(game::stageCatalog(meta.id).additionalRoutes.size() + 1) + "입구 · " +
                std::to_string(state.app.highestUnlockedStage) + "/" + std::to_string(game::TotalStages) +
                " 해금";
            g.text(stageTitle, {x + w / 2.0F, y + 11}, 14, theme::gold, .5F);
            g.text(stageSub, {x + w / 2.0F, y + 31}, 11, theme::muted, .5F);
            if (!meta.gimmick.empty())
                g.text(meta.gimmick, {x + w / 2.0F, y + 50}, 11, theme::green, .5F, w - 96);
            y += cardH + 14;

            button(g, ui, continuing ? HomeContinue : HomeNew, {x, y, w, 52},
                   continuing ? "제 " + std::to_string(state.play.currentStage) + "구역 계속하기"
                              : "작전 개시",
                   true, true);
            y += 62;
            button(g, ui, BeginTutorial, {x, y, w, 36},
                   state.app.tutorialCompleted ? "연습 원정 다시하기" : "연습 원정 · 처음이라면 추천", true,
                   !state.app.tutorialCompleted);
            y += 46;
            if (continuing) {
                g.text(status, {x, y}, 12, theme::muted);
                y += 24;
                button(g, ui, HomeNew, {x, y, w, 38}, "선택 구역으로 새 원정 시작");
                y += 48;
            }
            const float half = (w - 12) / 2;
            button(g, ui, OpenSettings, {x, y, half, 40}, "설정");
            button(g, ui, OpenGuide, {x + half + 12, y, half, 40}, "플레이 방법");
            y += 50;
            button(g, ui, OpenCredits, {x, y, w, 34}, "크레딧");
            const float artX = x + w + 70;
            titleScene(g, run.catalog(),
                       {artX, std::max(80.0F, (g.height - 570) / 2.0F), g.width - artX - x,
                        std::min(570.0F, g.height - 150.0F)},
                       time, ambient);
        }
        g.text(std::to_string(game::TotalStages) + " STAGE CAMPAIGN  ·  OSSCA THORVG GAMEJAM",
               {g.width / 2.0F, g.height - 26.0F}, compact ? 8 : 10, theme::muted, .5F);
        if (!ui.focused)
            ui.focused = continuing ? HomeContinue : HomeNew;
        if (state.play.modal != presentation::Modal::None)
            overlay(g, ui, run, state, audio);
        return;
    }
    const float width = std::min(880.0F, g.width - (compact ? 24.0F : 96.0F));
    const float left = (g.width - width) / 2;
    const float top = compact ? 96.0F : 126.0F;
    const engine::Rect box{left, top, width, g.height - top - (compact ? 82.0F : 100.0F)};
    const char *title = screen == Screen::Settings ? "설정"
                        : screen == Screen::Guide  ? "플레이 방법"
                                                   : "크레딧";
    g.text(title, {left, compact ? 27.0F : 40.0F}, compact ? 28 : 36, theme::text);
    button(g, ui, GoHome, {left + width - 80, compact ? 27.0F : 40.0F, 80, 34}, "메인");
    g.text(screen == Screen::Settings ? (state.app.navigation.hasRun() ? "원정은 잠시 멈춰 있어요."
                                                                       : "소리와 전투 피드백을 내 취향대로.")
           : screen == Screen::Guide  ? "방어하고, 탐사하고, 더 강해지세요."
                                      : "작은 벡터로 만드는 광산 방어전.",
           {left, compact ? 67.0F : 88.0F}, compact ? 12 : 14, theme::muted);
    panel(g, box, true);
    const float x = box.x + 18, w = box.w - 36;
    if (screen == Screen::Settings) {
        const float half = (w - 10) / 2;
        const float third = (w - 16) / 3;
        button(g, ui, SettingsAudio, {x, box.y + 16, third, 34}, "오디오", true, state.app.settingsTab == 0);
        button(g, ui, SettingsVisual, {x + third + 8, box.y + 16, third, 34},
               compact ? "화면" : "화면 · 피드백", true, state.app.settingsTab == 1);
        button(g, ui, SettingsGame, {x + (third + 8) * 2, box.y + 16, third, 34}, "원정", true,
               state.app.settingsTab == 2);
        if (state.app.settingsTab == 2) {
            const bool excavation = state.app.preferences.excavation;
            g.text("채굴 방식", {x, box.y + 77}, compact ? 20 : 25, theme::gold);
            button(g, ui, ToggleMining, {x, box.y + 116, w, 40},
                   excavation ? "암벽 발굴  →  갱도 탐사로 바꾸기" : "갱도 탐사  →  암벽 발굴로 바꾸기", true,
                   excavation);
            g.text(excavation ? "암벽 발굴: 겹겹이 쌓인 암벽을 곡괭이와 발파로 깎아, 모양이 있는 매장물을 "
                                "통째로 드러내면 얻어요."
                              : "갱도 탐사: 열린 갱도 옆을 한 칸씩 파고, 숫자로 주변 둥지 수를 읽어요.",
                   {x, box.y + 172}, compact ? 10 : 12, theme::text, 0, w);
            g.text("새 원정부터 적용돼요. 연습 원정은 항상 암벽 발굴이에요.", {x, box.y + 214},
                   compact ? 10 : 12, theme::muted, 0, w);
        } else if (state.app.settingsTab == 0) {
            g.text("효과음", {x, box.y + 77}, compact ? 20 : 25, theme::gold);
            g.text("효과음 재생", {x, box.y + 122}, 14, theme::text);
            button(g, ui, AudioMute, {x + w - 110, box.y + 111, 110, 36}, audio.muted ? "켜기" : "끄기", true,
                   !audio.muted);
            g.text("전체 볼륨", {x, box.y + 170}, 14, theme::text);
            g.text(std::to_string(static_cast<int>(audio.volume * 100)) + "%", {x + w, box.y + 169}, 16,
                   theme::gold, 1);
            g.rect({x, box.y + 199, w, 7}, theme::inset, 3);
            g.rect({x, box.y + 199, w * audio.volume, 7}, audio.muted ? theme::muted : theme::gold, 3);
            button(g, ui, AudioDown, {x, box.y + 222, half, 38}, "-  작게", audio.volume > 0);
            button(g, ui, AudioUp, {x + half + 10, box.y + 222, half, 38}, "+  크게", audio.volume < 1);
            g.text("음악", {x, box.y + 280}, 14, theme::text);
            g.text(std::to_string(static_cast<int>(std::lround(audio.music * 100))) + "%",
                   {x + w, box.y + 279}, 16, theme::gold, 1);
            const float third = (w - 16) / 3;
            button(g, ui, MusicDown, {x, box.y + 306, third, 36}, "-", audio.music > 0);
            button(g, ui, MusicUp, {x + third + 8, box.y + 306, third, 36}, "+", audio.music < 1);
            button(g, ui, AudioTest, {x + (third + 8) * 2, box.y + 306, third, 36}, "미리 듣기",
                   !audio.muted && audio.volume > 0, true);
        } else {
            g.text("전투 피드백", {x, box.y + 77}, compact ? 20 : 25, theme::gold);
            g.text("움직임 줄이기", {x, box.y + 124}, compact ? 13 : 16, theme::text);
            button(g, ui, ToggleMotion, {x + w - 88, box.y + 112, 88, 36},
                   state.app.preferences.reducedMotion ? "켜짐" : "꺼짐", true,
                   state.app.preferences.reducedMotion);
            g.text("카드와 장식의 움직임을 간소화합니다.", {x, box.y + 165}, compact ? 10 : 12, theme::muted,
                   0, w);
            g.text("피해 숫자 표시", {x, box.y + 220}, compact ? 13 : 16, theme::text);
            button(g, ui, ToggleDamage, {x + w - 88, box.y + 208, 88, 36},
                   state.app.preferences.damageNumbers ? "표시" : "숨김", true,
                   state.app.preferences.damageNumbers);
            g.text("전장 위 피해량 숫자를 켜거나 끕니다.", {x, box.y + 261}, compact ? 10 : 12, theme::muted,
                   0, w);
            g.text("애니메이션 방식", {x, box.y + 281}, compact ? 10 : 12, theme::muted);
            button(g, ui, ToggleAnimationBackend, {x, box.y + 303, half, 38},
                   state.app.preferences.cachedAnimations ? "공유 프레임" : "벡터 원본", true,
                   state.app.preferences.cachedAnimations);
            button(g, ui, OpenGuide, {x + half + 10, box.y + 303, half, 38}, "플레이 방법");
        }
        if (box.h > 540) {
            g.text(state.app.settingsTab == 1 ? "화면 효과 미리보기" : "원정 미리보기", {x, box.y + 365}, 11,
                   theme::muted);
            const engine::Rect preview{x, box.y + 384, w, box.h - 402};
            titleScene(g, run.catalog(), preview, state.app.preferences.reducedMotion ? 0 : state.app.uiTime,
                       ambient);
            if (state.app.settingsTab == 1 && state.app.preferences.damageNumbers)
                g.text("12", {preview.x + preview.w * .4F, preview.y + preview.h * .18F}, 16, theme::text,
                       .5F);
        }
        button(g, ui, NavigateBack, {left, g.height - 64.0F, (width - 12) / 2, 42}, "←  돌아가기");
        button(g, ui, SettingsDefaults, {left + (width + 12) / 2, g.height - 64.0F, (width - 12) / 2, 42},
               "기본값 복원");
    } else if (screen == Screen::Guide) {
        static const std::array<const char *, 3> names{{"01  지상 방어", "02  지하 탐사", "03  제작과 성장"}};
        static const std::array<std::array<const char *, 4>, 3> lines{
            {{{"첫 웨이브 시작 후 카드를 배치하세요.", "건물은 패드, 유닛은 길 위 작은 칸에.",
               "카드를 다시 누르면 선택을 취소합니다.", "배치 대상을 눌러 상세·철수를 확인하세요."}},
             {{"정비마다 5칸을 굴착할 수 있습니다.", "타일을 선택하면 광부가 이동해 굴착합니다.",
               "숫자는 주변 8칸의 최초 둥지 수입니다.", "무료 스캔으로 경계 한 칸을 먼저 확인하세요."}},
             {{"철·구리·수정으로 도구와 장비를 만듭니다.", "금으로 슬롯을 늘리거나 유닛 카드를 모집하세요.",
               "생존 유닛·남은 체력·건물은 유지됩니다.", "마지막 웨이브까지 거점을 지키면 승리합니다."}}}};
        g.text(names[state.app.guidePage], {x, box.y + 22}, compact ? 21 : 28, theme::gold, 0, w);
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
        static const std::array<const char *, 4> wallLines{
            {"정비마다 굴착 5회. 곡괭이는 1회, 발파는 2회를 씁니다.",
             "맨 윗줄부터 암벽을 한 겹씩 깎아 내려갑니다.", "매장물은 모든 칸이 드러나야 얻습니다.",
             "발파가 닿은 둥지는 다음 웨이브에 적을 보냅니다."}};
        const bool wall = state.app.guidePage == 1 && state.app.preferences.excavation;
        for (int index = 0; index < 4; ++index) {
            const float y = box.y + 183 + index * 31;
            g.circle({x + 3, y + 6}, 2, theme::gold);
            g.text(wall ? wallLines[static_cast<std::size_t>(index)] : lines[state.app.guidePage][index],
                   {x + 15, y}, compact ? 10 : 14, theme::text, 0, w - 15);
        }
        if (!compact && box.h > 480) {
            g.text("키보드  1–4 카드 선택   Space 시작·정지   Esc 취소·뒤로", {x, box.y + 337}, 13,
                   theme::muted, 0, w);
            g.text("M 지상·지하   S 음소거   V 볼륨   Tab·Enter 버튼 조작", {x, box.y + 365}, 13,
                   theme::muted, 0, w);
        }
        const float half = (w - 10) / 2;
        button(g, ui, GuidePrevious, {x, box.y + box.h - 54, half, 36}, "이전", state.app.guidePage > 0);
        button(g, ui, GuideNext, {x + half + 10, box.y + box.h - 54, half, 36}, "다음",
               state.app.guidePage < 2, true);
        button(g, ui, NavigateBack, {left, g.height - 64.0F, (width - 10) / 2, 42}, "←  돌아가기");
        button(g, ui, BeginTutorial, {left + (width + 10) / 2, g.height - 64.0F, (width - 10) / 2, 42},
               "연습 원정", true, true);
    } else {
        core(g, {box.x + box.w / 2, box.y + 58}, 27);
        g.text("THOR-IUM TD", {box.x + box.w / 2, box.y + 102}, compact ? 25 : 34, theme::text, .5F);
        g.text("OSSCA · ThorVG GameJam", {box.x + box.w / 2, box.y + 145}, compact ? 12 : 16, theme::gold,
               .5F);
        const std::array<const char *, 7> credits{{"벡터 그래픽 · 스플래시  /  ThorVG",
                                                   "창 · 입력 · 오디오  /  SDL3", "폰트  /  Noto Sans KR",
                                                   "MP3 디코딩  /  dr_mp3", "배경음악  /  AI 생성 (Flow Music)",
                                                   "기획 · 개발  /  @hd1534", "소스 라이선스  /  MIT"}};
        for (int index = 0; index < 7; ++index) {
            const float y = box.y + 190 + index * (compact ? 26.0F : 33.0F);
            g.line({{x, y - 10}, {x + w, y - 10}}, 1, {86, 111, 113, 90});
            g.text(credits[index], {x, y}, compact ? 12 : 16, theme::muted, 0, w);
        }
        button(g, ui, NavigateBack, {left, g.height - 64.0F, width, 42}, "←  돌아가기");
    }
    if (!ui.focused)
        ui.focused = NavigateBack;
}
} // namespace thorium::presentation
