#include "CardPresentation.h"
#include "CommandPolicy.h"
#include "Theme.h"
#include "View.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace thorium::presentation {
using engine::Color;
using engine::Rect;
using engine::ScreenPoint;
namespace {
/** Format a logical range without a long floating-point representation. */
std::string decimal(float value) {
    std::ostringstream result;
    result << std::fixed << std::setprecision(1) << value;
    return result.str();
}
/** Paint a clean, modern faceted ore/gem token. */
void gem(engine::Renderer &g, ScreenPoint center, float size, Color color) {
    const float hw = size * 0.85F;
    const float hh = size;
    g.polygon({{center.x, center.y - hh},
               {center.x + hw, center.y},
               {center.x, center.y + hh},
               {center.x - hw, center.y}},
              color);
    g.polygon({{center.x, center.y - hh},
               {center.x + hw, center.y},
               {center.x, center.y - hh * 0.15F}},
              {255, 255, 255, 110});
    g.polygon({{center.x - hw, center.y},
               {center.x, center.y + hh},
               {center.x, center.y - hh * 0.15F}},
              {0, 0, 0, 45});
}
/** Draw a segmented instrument bar with explicit minimum/maximum clamping. */
void meter(engine::Renderer &g, Rect bounds, float fraction, Color color, int segments = 10) {
    g.rect(bounds, theme::inset, 3);
    const float gap = 2.0F;
    const float width = (bounds.w - (segments - 1) * gap) / segments;
    const float fill = std::clamp(fraction, 0.0F, 1.0F) * segments;
    for (int index = 0; index < segments; ++index) {
        const float part = std::clamp(fill - index, 0.0F, 1.0F);
        if (part > 0)
            g.rect({bounds.x + index * (width + gap), bounds.y, width * part, bounds.h}, color, 1);
    }
}
/** Sample a clockwise arc starting at twelve o'clock, using logical screen coordinates. */
std::vector<ScreenPoint> clockwiseArc(ScreenPoint center, float radius, float from, float to) {
    constexpr float turn = 6.283185307F;
    const int segments = std::max(1, static_cast<int>(std::ceil((to - from) * 64)));
    std::vector<ScreenPoint> points;
    points.reserve(static_cast<std::size_t>(segments + 1));
    for (int step = 0; step <= segments; ++step) {
        const float angle = -turn / 4 + (from + (to - from) * step / segments) * turn;
        points.push_back({center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius});
    }
    return points;
}
/** Reveal the illustration clockwise while keeping the remaining energy coverage visibly dimmed. */
void chargeMask(engine::Renderer &g, Rect art, float fraction) {
    const ScreenPoint center = art.center();
    const float radius = std::hypot(art.w, art.h);
    std::vector<ScreenPoint> sector = clockwiseArc(center, radius, fraction, 1);
    sector.insert(sector.begin(), center);
    g.beginClip(art);
    g.polygon(sector, {16, 30, 40, 155});
    g.endClip();
}

} // namespace

/** Draw an arcade-style game heart for core base health. */
void drawHeart(engine::Renderer &g, ScreenPoint center, float size, Color color) {
    const float r = size * 0.52F;
    g.circle({center.x - r * 0.5F, center.y - r * 0.35F}, r, color);
    g.circle({center.x + r * 0.5F, center.y - r * 0.35F}, r, color);
    g.polygon({{center.x - r * 1.02F, center.y - r * 0.12F},
               {center.x + r * 1.02F, center.y - r * 0.12F},
               {center.x, center.y + size * 0.88F}},
              color);
    g.circle({center.x - r * 0.55F, center.y - r * 0.55F}, r * 0.35F, {255, 255, 255, 140});
}
/** Draw a tactical squadron shield badge. */
void drawShield(engine::Renderer &g, ScreenPoint center, float size, Color color) {
    const float hw = size * 0.72F;
    const float top = center.y - size * 0.65F;
    const float bottom = center.y + size * 0.85F;
    g.polygon({{center.x - hw, top},
               {center.x + hw, top},
               {center.x + hw, center.y + size * 0.1F},
               {center.x, bottom},
               {center.x - hw, center.y + size * 0.1F}},
              color);
    g.polygon({{center.x - hw * 0.65F, top + size * 0.12F},
               {center.x, top + size * 0.12F},
               {center.x, bottom - size * 0.28F},
               {center.x - hw * 0.65F, center.y + size * 0.05F}},
              {255, 255, 255, 90});
}
/** Draw a classic swallowtail war ribbon banner. */
void drawRibbon(engine::Renderer &g, Rect bounds, const std::string &title, Color ribbonColor, Color textColor) {
    const float tailW = 22.0F;
    const float tailH = bounds.h;
    const float depth = 8.0F;
    const Color darkTail{static_cast<std::uint8_t>(ribbonColor.r * 0.65F),
                         static_cast<std::uint8_t>(ribbonColor.g * 0.65F),
                         static_cast<std::uint8_t>(ribbonColor.b * 0.65F)};
    // Left swallowtail fold
    g.polygon({{bounds.x - tailW, bounds.y + 4.0F},
               {bounds.x, bounds.y + 4.0F},
               {bounds.x, bounds.y + tailH + 4.0F},
               {bounds.x - tailW, bounds.y + tailH + 4.0F},
               {bounds.x - tailW + depth, bounds.y + tailH / 2 + 4.0F}},
              darkTail);
    // Right swallowtail fold
    g.polygon({{bounds.x + bounds.w + tailW, bounds.y + 4.0F},
               {bounds.x + bounds.w, bounds.y + 4.0F},
               {bounds.x + bounds.w, bounds.y + tailH + 4.0F},
               {bounds.x + bounds.w + tailW, bounds.y + tailH + 4.0F},
               {bounds.x + bounds.w + tailW - depth, bounds.y + tailH / 2 + 4.0F}},
              darkTail);
    // Ribbon shadow & body
    g.rect({bounds.x, bounds.y + 3.0F, bounds.w, bounds.h}, {10, 20, 26, 140}, 5);
    g.rect(bounds, ribbonColor, 5);
    // Lustrous top highlight
    g.rect({bounds.x + 2.0F, bounds.y + 2.0F, bounds.w - 4.0F, bounds.h * 0.38F}, {255, 255, 255, 75}, 3);
    // Chunky ribbon text
    g.text(title, {bounds.x + bounds.w / 2, bounds.y + (bounds.h - 17) / 2}, 16, textColor, .5F);
}

void View::panel(engine::Renderer &g, Rect bounds, bool brass) {
    g.rect({bounds.x, bounds.y + 4, bounds.w, bounds.h}, theme::shadow, 12);
    g.rect(bounds, brass ? theme::copper : theme::edge, 10);
    g.gradientRect({bounds.x + 2, bounds.y + 2, bounds.w - 4, bounds.h - 4}, theme::top, theme::bottom, 8);
}
void View::button(engine::Renderer &g, engine::Ui &ui, engine::WidgetId id, Rect bounds,
                  const std::string &title, bool enabled, bool primary, bool danger) {
    ui.add(id, bounds, enabled);
    if (ui.focused == id)
        g.rect({bounds.x - 3, bounds.y - 3, bounds.w + 6, bounds.h + 6}, theme::blue, 11);
    const bool pressed = ui.captured == id;
    const float depth = pressed ? 1.5F : 4.5F;
    const Color baseShadow = !enabled ? Color{25, 34, 38}
                             : danger  ? Color{98, 32, 28}
                             : primary ? Color{155, 96, 28}
                                       : Color{22, 42, 50};
    g.rect({bounds.x, bounds.y + depth, bounds.w, bounds.h}, baseShadow, 9);
    const Color inkBorder = !enabled ? Color{42, 54, 60}
                            : danger  ? Color{165, 52, 44}
                            : primary ? Color{215, 142, 42}
                                      : Color{45, 75, 86};
    const float topY = bounds.y + (pressed ? 3.0F : 0.0F);
    g.rect({bounds.x, topY, bounds.w, bounds.h}, inkBorder, 9);
    const Color face = !enabled ? Color{52, 66, 72}
                       : danger  ? Color{205, 78, 65}
                       : primary ? Color{248, 186, 68}
                                 : (pressed ? Color{42, 78, 88} : Color{56, 96, 108});
    g.rect({bounds.x + 1.5F, topY + 1.5F, bounds.w - 3.0F, bounds.h - 3.0F}, face, 8);
    if (enabled) {
        const Color highlight = primary ? Color{255, 235, 165, 140}
                                : danger ? Color{255, 185, 175, 90}
                                         : Color{145, 205, 218, 90};
        g.rect({bounds.x + 4.0F, topY + 2.0F, bounds.w - 8.0F, (bounds.h - 4.0F) * 0.35F}, highlight, 6);
    }
    g.text(title, {bounds.x + bounds.w / 2, topY + (bounds.h - 17) / 2},
           layout.compact ? 13 : 14,
           !enabled  ? Color{130, 150, 153}
           : primary ? Color{58, 36, 12}
                     : theme::text,
           .5F, bounds.w - 18);
}

void View::hud(engine::Renderer &g, engine::Ui &ui, const game::Run &run, const State &state,
               const engine::AudioSettings &audio) {
    // Break the box: subtle ambient vignette instead of a heavy rectangular wall
    g.gradientRect({0, 0, static_cast<float>(g.width), 110.0F}, {10, 20, 26, 175}, {10, 20, 26, 0});

    if (layout.compact) {
        const Rect wave{g.width - 114.0F, 17.0F, 100.0F, 30.0F};
        const Rect settings{wave.x - 76, 17, 30, 30};
        ui.add(OpenSettings, settings);
        g.rect(settings, ui.focused == OpenSettings ? theme::blue : theme::inset, 6);
        const ScreenPoint gear = settings.center();
        for (int i = 0; i < 4; ++i) {
            const float a = i * .785398F;
            g.line({{gear.x - std::cos(a) * 9, gear.y - std::sin(a) * 9},
                    {gear.x + std::cos(a) * 9, gear.y + std::sin(a) * 9}},
                   2.5F, theme::muted);
        }
        g.circle(gear, 5.5F, theme::muted);
        g.circle(gear, 2.5F, theme::inset);

        const Rect sound{wave.x - 38, 17, 30, 30};
        ui.add(AudioMute, sound);
        g.rect(sound, ui.focused == AudioMute ? theme::blue : theme::inset, 6);
        const ScreenPoint c = sound.center();
        const Color color = audio.muted ? theme::muted : theme::gold;
        g.polygon({{c.x - 8, c.y - 3},
                   {c.x - 4, c.y - 3},
                   {c.x + 1, c.y - 8},
                   {c.x + 1, c.y + 8},
                   {c.x - 4, c.y + 3},
                   {c.x - 8, c.y + 3}},
                  color);
        if (audio.muted)
            g.line({{c.x + 4, c.y - 5}, {c.x + 10, c.y + 5}}, 1.5F, theme::red);
        else
            g.line({{c.x + 5, c.y - 4}, {c.x + 8, c.y}, {c.x + 5, c.y + 4}}, 1.5F, color);

        g.rect(wave, theme::inset, 6);
        g.rect({wave.x, wave.y, 3, wave.h}, theme::gold, 1);
        g.text(state.play.tutorial.active() ? "연습" : "WAVE", {wave.x + 10, wave.y + 9}, 9, theme::muted);
        g.text(state.play.tutorial.active() ? "1 / 1"
                                            : std::to_string(std::max(1, run.state().wave)) + " / 3",
               {wave.x + wave.w - 12, wave.y + 5}, 17, theme::gold, 1);

        const float step = (g.width - 48.0F) / 2.0F;
        const float width = step - 14.0F;
        const std::array<std::string, 2> labels{
            {"거점 " + std::to_string(run.state().baseHp) + " / " + std::to_string(run.catalog().balance.baseHp),
             "부대 " + std::to_string(run.state().units.size()) + " / " + std::to_string(run.state().slots)}};
        const std::array<Color, 2> colors{{theme::green, theme::gold}};
        const std::array<float, 2> values{{static_cast<float>(run.state().baseHp) / run.catalog().balance.baseHp,
                                           static_cast<float>(run.state().units.size()) / run.state().slots}};
        for (int index = 0; index < 2; ++index) {
            g.text(labels[index], {24.0F + index * step, 57.0F}, 11, colors[index]);
            meter(g, {24.0F + index * step, 74.0F, width, 5.0F}, values[index], colors[index], index == 0 ? 10 : run.state().slots);
        }

        const float oreWidth = (g.width - 44.0F - 18) / 4;
        const std::array<int, 4> amounts{
            {run.state().iron, run.state().copper, run.state().crystal, run.state().gold}};
        const std::array<const char *, 4> names{{"철", "구리", "수정", "금"}};
        const std::array<Color, 4> oreColors{
            {Color{181, 203, 206}, Color{226, 151, 103}, theme::purple, theme::gold}};
        for (int index = 0; index < 4; ++index) {
            const float left = 22.0F + index * (oreWidth + 6);
            g.rect({left, 92.0F, oreWidth, 23.0F}, {17, 33, 42}, 5);
            gem(g, {left + 12, 103.0F}, 5.5F, oreColors[index]);
            g.text(std::string(names[index]) + " " + std::to_string(amounts[index]), {left + 22, 96.0F},
                   10, theme::text);
        }
    } else {
        // [LEFT] Tangible Game Core Heart & Squadron Shield
        const float baseSectionX = 28.0F;
        // Big juicy heart
        drawHeart(g, {baseSectionX + 18, 38}, 14.0F, theme::red);
        g.text("거점 라이프", {baseSectionX + 40, 21}, 11, theme::muted);
        g.text(std::to_string(run.state().baseHp) + " / " + std::to_string(run.catalog().balance.baseHp),
               {baseSectionX + 40, 36}, 18, theme::text);
        meter(g, {baseSectionX, 60, 160.0F, 7.0F},
              static_cast<float>(run.state().baseHp) / run.catalog().balance.baseHp, theme::green, 10);

        const float unitSectionX = baseSectionX + 180.0F;
        // Bold squadron shield
        drawShield(g, {unitSectionX + 16, 38}, 14.0F, theme::blue);
        g.text("출격 부대", {unitSectionX + 38, 21}, 11, theme::muted);
        g.text(std::to_string(run.state().units.size()) + " / " + std::to_string(run.state().slots) + " 슬롯",
               {unitSectionX + 38, 36}, 18, theme::text);
        meter(g, {unitSectionX, 60, 150.0F, 7.0F},
              static_cast<float>(run.state().units.size()) / run.state().slots, theme::gold, run.state().slots);

        // Chunky arcade system buttons
        button(g, ui, AudioMute, {baseSectionX, 79, 78, 26}, audio.muted ? "소리 OFF" : "소리 ON");
        button(g, ui, AudioVolume, {baseSectionX + 84, 79, 88, 26},
               "볼륨 " + std::to_string(static_cast<int>(audio.volume * 100)) + "%");
        button(g, ui, OpenSettings, {baseSectionX + 178, 79, 62, 26}, "설정");

        // [CENTER] Classic War Ribbon Banner
        const float ribbonW = 180.0F;
        const float ribbonX = (g.width - ribbonW) / 2.0F;
        const std::string waveTitle =
            state.play.tutorial.active()
                ? "연습 원정 · 공세 1/1"
                : "★ WAVE " + std::to_string(std::max(1, run.state().wave)) + " / 3 ★";
        drawRibbon(g, {ribbonX, 22.0F, ribbonW, 34.0F}, waveTitle, theme::gold, Color{48, 30, 10});

        const std::string statusText =
            state.play.tutorial.lesson == Lesson::Briefing ||
                    state.play.tutorial.lesson == Lesson::Building ||
                    state.play.tutorial.lesson == Lesson::Unit
                ? "배치 연습 · 적은 대기 중"
            : run.state().phase == game::Phase::Prep
                ? "굴착 " + std::to_string(run.state().digs) + " · 스캔 " + std::to_string(run.state().scans)
            : run.state().phase == game::Phase::Ready
                ? "작전 준비 · 방어선을 구축하세요"
                : "공세 격퇴 중 (남은 적 " + std::to_string(run.state().enemies.size()) + ")";
        g.text(statusText, {ribbonX + ribbonW / 2, 65.0F}, 12, theme::text, .5F);
        g.text(run.state().phase == game::Phase::Battle ? "출현 " + std::to_string(run.waveSpawned()) +
                                                              " / " + std::to_string(run.waveSpawnCount())
                                                        : "표층 01 구역",
               {ribbonX + ribbonW / 2, 84.0F}, 11, theme::muted, .5F);

        // [RIGHT] Embossed Coin Badges (No web stat boxes!)
        const float oreCardWidth = 96.0F;
        const float oreStartX = g.width - 418.0F;
        const std::array<int, 4> amounts{
            {run.state().iron, run.state().copper, run.state().crystal, run.state().gold}};
        const std::array<const char *, 4> names{{"철", "구리", "수정", "금"}};
        const std::array<Color, 4> oreColors{
            {Color{181, 203, 206}, Color{226, 151, 103}, theme::purple, theme::gold}};

        for (int index = 0; index < 4; ++index) {
            const float left = oreStartX + index * oreCardWidth;
            // Chunky pill token
            g.rect({left, 30.0F, 86.0F, 48.0F}, {16, 28, 36, 210}, 12);
            g.rect({left, 30.0F, 86.0F, 48.0F}, {45, 72, 82}, 12);
            // Thick embossed coin badge
            const ScreenPoint coin{left + 22.0F, 54.0F};
            g.circle(coin, 13.0F, {10, 20, 26});
            g.circle(coin, 11.0F, oreColors[index]);
            gem(g, coin, 6.5F, {255, 255, 240});
            // Bold, high-contrast game font numbers
            g.text(names[index], {left + 42.0F, 36.0F}, 11, theme::muted);
            g.text(std::to_string(amounts[index]), {left + 42.0F, 50.0F}, 18, theme::text);
        }
    }
    static_cast<void>(state);
}

void View::inspector(engine::Renderer &g, engine::Ui &ui, const game::Run &run, const State &state,
                     Rect area) {
    const game::Tower *tower = run.tower(state.play.selectedTower);
    const game::Unit *unit = run.unit(state.play.selectedUnit);
    if (!tower && !unit)
        return;
    const game::CardKind kind = tower ? tower->kind : unit->kind;
    const bool prep = run.state().phase == game::Phase::Prep;
    const bool compact = layout.compact || area.h < 350;
    const bool dense = compact && prep && area.h < 170;
    g.rect(area, {16, 32, 40, 180}, 9);
    const float x = area.x + 12, w = area.w - 24;
    const float titleY = area.y + 12;
    icon(g, run.catalog(), kind, {compact ? x + 20 : x + w / 2, compact ? titleY + 17 : titleY + 71},
         compact ? 32 : 76);
    g.text(run.catalog().card(kind).name, {compact ? x + 49 : x, titleY}, compact ? 17 : 20, theme::text);
    if (tower) {
        g.text("Lv. " + std::to_string(tower->level) + " / 3", {area.x + area.w - 12, titleY + 3}, 12,
               theme::gold, 1);
        if (!dense) {
            g.text("공격 범위 " + decimal(run.catalog().card(kind).range), {x, area.y + (compact ? 54 : 130)},
                   12, theme::muted);
            g.text(kind == game::CardKind::Cannon      ? "범위 포격"
                   : kind == game::CardKind::Lightning ? "전격 연쇄"
                                                       : "단일 표적 사격",
                   {x, area.y + (compact ? 75 : 154)}, 12, theme::gold);
        }
    } else {
        g.text(std::to_string(unit->hp) + " / " + std::to_string(unit->maxHp),
               {area.x + area.w - 12, titleY + 4}, 12, theme::green, 1);
        if (!dense) {
            meter(g, {x, area.y + (compact ? 53 : 129), w, 7}, static_cast<float>(unit->hp) / unit->maxHp,
                  theme::green, 10);
            g.text("범위 " + decimal(run.catalog().card(kind).range) + "   ·   점유 1슬롯",
                   {x, area.y + (compact ? 72 : 151)}, 12, theme::muted);
        }
    }
    float y = area.y + (dense ? 47.0F : compact ? 94.0F : 191.0F);
    const float buttonHeight = dense ? 32.0F : 40.0F;
    if (prep && unit) {
        const float half = (w - 8) / 2;
        button(g, ui, MoveUnit, {x, y, half, buttonHeight},
               state.play.movingUnit ? "이동 칸 선택 중" : "위치 이동", true, state.play.movingUnit);
        button(g, ui, Heal, {x + half + 8, y, half, buttonHeight}, "치료 · 철1 구리1");
        y += buttonHeight + 8;
    }
    const float half = (w - 8) / 2;
    button(g, ui, Dismiss, {x, y, half, buttonHeight}, tower ? "건물 철거" : "부대 퇴각", true, false, true);
    button(g, ui, ClearSelection, {x + half + 8, y, half, buttonHeight}, "선택 해제");
    if (!compact) {
        g.text("철수 비용 환급 없음", {x, y + 62}, 11, theme::muted);
        g.text("다시 배치할 때 카드 비용 사용", {x, y + 82}, 11, theme::muted);
    }
}

void View::commandPanel(engine::Renderer &g, engine::Ui &ui, const game::Run &run, const State &state) {
    if (state.play.tutorial.active()) {
        tutorialPanel(g, ui, run, state);
        return;
    }
    const Rect side = layout.sidebar;
    const bool prep = run.state().phase == game::Phase::Prep;
    const bool selected = run.tower(state.play.selectedTower) || run.unit(state.play.selectedUnit);

    // -------------------------------------------------------------
    // Mobile Compact View: Clean Telemetry + Popup Tactical Guide
    // -------------------------------------------------------------
    if (layout.compact && !prep) {
        panel(g, side);
        const float x = side.x + 14, w = side.w - 28;
        const bool dense = side.h < 180;
        const float controlY = side.y + side.h - (dense ? 44 : 46);

        const game::Tower *tower = run.tower(state.play.selectedTower);
        const game::Unit *unit = run.unit(state.play.selectedUnit);
        const game::Card *card = run.card(state.play.selectedCard);
        const bool guideVisible = state.play.guideOpen && !selected && !card && !placingCard_;
        const auto &meta = game::stageMetadata(state.play.currentStage);
        std::string title = tower  ? run.catalog().card(tower->kind).name
                            : unit ? run.catalog().card(unit->kind).name
                            : card ? run.catalog().card(card->kind).name
                            : run.state().phase == game::Phase::Ready
                                ? meta.name + " · 대기 중"
                                : meta.name + " · " + std::to_string(run.state().wave) + "/" +
                                      std::to_string(meta.wavesCount) + "W";
        if (card) {
            const game::PlayAvailability availability = run.playAvailability(card->id);
            title += placement_ && placement_->card == card->id
                         ? " · " + std::string(playActionName(placement_->rule.action)) + " " +
                               std::to_string(placement_->rule.cost)
                         : " · " + std::to_string(availability.minimumCost) +
                               (availability.variableCost() ? "+" : "");
        }

        // Header plaque
        const float headerY = side.y + (dense ? 6 : 10);
        const float headerH = dense ? 28.0F : 32.0F;
        g.rect({x, headerY, w, headerH}, {18, 34, 42}, 6);
        g.rect({x, headerY, 4.0F, headerH}, theme::gold, 2);
        g.text(title, {x + 12, headerY + 6}, 14, theme::text, 0, w - 48);

        // Tactile Popup Guide button in upper right
        const float guideBtnW = 34.0F;
        const Rect guideBtnRect{x + w - guideBtnW, headerY + 1, guideBtnW, dense ? 26.0F : 30.0F};
        button(g, ui, ToggleDrawer, guideBtnRect, "?", !guideVisible, state.play.guideOpen);

        const float half = (w - 8) / 2;

        if (selected) {
            // Selected Tower/Unit Controls
            const Rect selBox{x, side.y + (dense ? 40 : 48), w, controlY - (side.y + (dense ? 46 : 54))};
            g.rect(selBox, {20, 36, 46, 230}, 8);
            g.rect(selBox, theme::edge, 1);
            if (!dense) {
                g.text(tower ? "배치된 방어 건물" : "배치된 방어 유닛", {x + 12, selBox.y + 12}, 12,
                       theme::gold);
                g.text(tower ? "사거리 내 침투하는 적을 자동 요격합니다"
                             : "지정된 경로에서 적의 진격을 저지합니다",
                       {x + 12, selBox.y + 32}, 11, theme::muted);
            }
            const float btnY = dense ? selBox.y + 1 : selBox.y + selBox.h - 38;
            button(g, ui, Dismiss, {x + 8, btnY, half - 4, 32}, tower ? "건물 철거" : "부대 퇴각", true,
                   false, true);
            button(g, ui, ClearSelection, {x + half + 4, btnY, half - 4, 32}, "선택 해제");
        } else if (card) {
            // Selected Card Controls
            const Rect cardBox{x, side.y + (dense ? 40 : 48), w, controlY - (side.y + (dense ? 46 : 54))};
            g.rect(cardBox, {20, 36, 46, 230}, 8);
            g.rect(cardBox, theme::edge, 1);
            if (!dense) {
                g.text(std::string(cardCategoryName(run.catalog().card(card->kind).category)) + " 카드 작전",
                       {x + 12, cardBox.y + 12}, 12, theme::gold);
                g.text(cardInstruction(run.catalog().card(card->kind).category), {x + 12, cardBox.y + 32}, 11,
                       theme::text);
            }
            const float btnY = dense ? cardBox.y + 1 : cardBox.y + cardBox.h - 38;
            button(g, ui, Exchange, {x + 8, btnY, half - 4, 32}, "카드 교체 (1)",
                   run.state().phase == game::Phase::Battle && run.state().energy >= 1);
            button(g, ui, ClearSelection, {x + half + 4, btnY, half - 4, 32}, "선택 해제");
        } else {
            // Live Telemetry Status Hub
            const Rect hubBox{x, side.y + (dense ? 40 : 48), w, controlY - (side.y + (dense ? 46 : 54))};
            g.rect(hubBox, {18, 32, 42, 190}, 8);
            g.rect(hubBox, theme::edge, 1);

            if (dense) {
                g.text(run.state().phase == game::Phase::Ready
                           ? "웨이브 시작 후 카드를 배치하세요"
                           : "침투 " + std::to_string(run.state().enemies.size()) + " · 처치 " +
                                 std::to_string(run.state().defeated),
                       {x + 12, hubBox.y + 10}, 11, theme::muted, 0, w - 24);
            } else if (run.state().phase == game::Phase::Ready) {
                // Ready phase status hub
                g.text("기지 현황", {x + 14, hubBox.y + 12}, 12, theme::gold);
                g.text("기지 내구도", {x + 14, hubBox.y + 36}, 11, theme::muted);
                g.text(std::to_string(run.state().baseHp) + " / " +
                           std::to_string(run.catalog().balance.baseHp),
                       {x + w - 14, hubBox.y + 36}, 11, theme::green, 1);
                g.text("출격 부대", {x + 14, hubBox.y + 56}, 11, theme::muted);
                g.text(std::to_string(run.state().units.size()) + " / " + std::to_string(run.state().slots),
                       {x + w - 14, hubBox.y + 56}, 11, theme::text, 1);
                g.line({{x + 12, hubBox.y + 76}, {x + w - 12, hubBox.y + 76}}, 1, {36, 56, 68});
                g.text("카드를 끌어다 전장에 배치하세요", {x + w / 2, hubBox.y + 86}, 11, theme::text, 0.5F);
            } else {
                // Battle phase live wave telemetry
                g.text("웨이브 진행", {x + 14, hubBox.y + 12}, 11, theme::muted);
                g.text(std::to_string(run.waveSpawned()) + " / " + std::to_string(run.waveSpawnCount()),
                       {x + w - 14, hubBox.y + 12}, 11, theme::gold, 1);
                meter(g, {x + 14, hubBox.y + 32, w - 28, 8},
                      run.waveSpawnCount() ? static_cast<float>(run.waveSpawned()) / run.waveSpawnCount() : 0,
                      theme::gold, 16);
                g.line({{x + 12, hubBox.y + 52}, {x + w - 12, hubBox.y + 52}}, 1, {36, 56, 68});
                g.text("침투한 적", {x + 14, hubBox.y + 64}, 11, theme::muted);
                g.text(std::to_string(run.state().enemies.size()) + "마리", {x + w - 14, hubBox.y + 64}, 11,
                       theme::red, 1);
                g.text("처치한 적", {x + 14, hubBox.y + 84}, 11, theme::muted);
                g.text(std::to_string(run.state().defeated) + "마리", {x + w - 14, hubBox.y + 84}, 11,
                       theme::text, 1);
            }
        }

        // Action buttons
        if (run.state().phase == game::Phase::Ready) {
            button(g, ui, Start, {x, controlY, w - 66, 38}, "웨이브 시작", !guideVisible, true);
            button(g, ui, Speed, {x + w - 58, controlY, 58, 38}, state.play.speed == 1 ? "1x" : "2x",
                   !guideVisible);
        } else {
            button(g, ui, Pause, {x, controlY, half, 38}, "일시정지", !guideVisible);
            button(g, ui, Speed, {x + half + 8, controlY, half, 38},
                   state.play.speed == 1 ? "1배속" : "2배속", !guideVisible);
        }

        // -------------------------------------------------------------
        // Floating Popup Dialog: Appears on initial deploy or via [?]
        // -------------------------------------------------------------
        if (guideVisible) {
            // Backdrop dimming
            const Rect dimArea{side.x - 4, layout.world.y + 12.0F, side.w + 8,
                               layout.hand.y - layout.world.y - 14.0F};
            g.rect(dimArea, {6, 14, 22, 195}, 10);

            // Floating Popup Card
            const float popupY = layout.world.y + 12;
            const Rect pop{side.x + 8, popupY, side.w - 16, std::min(268.0F, layout.hand.y - popupY - 12)};
            ui.add(GuideBackdrop, pop, false, false, false);
            g.rect({pop.x, pop.y + 6, pop.w, pop.h}, {0, 0, 0, 220}, 14);
            g.rect(pop, theme::gold, 14);
            g.rect({pop.x + 2.5F, pop.y + 2.5F, pop.w - 5.0F, pop.h - 5.0F}, {18, 32, 44}, 12);

            // Popup Header
            g.rect({pop.x + 10, pop.y + 10, pop.w - 20, 32}, {26, 44, 58}, 6);
            g.rect({pop.x + 10, pop.y + 10, 4.0F, 32}, theme::gold, 2);
            g.text("방어 작전 수칙", {pop.x + 22, pop.y + 17}, 14, theme::gold);
            button(g, ui, CloseDrawer, {pop.x + pop.w - 42, pop.y + 10, 32, 32}, "X", true, false, true);

            // Popup Content
            float py = pop.y + 54;
            const std::array<std::pair<const char *, Color>, 3> tips{
                {{"타워 · 원형 발판에 건설해 자동 공격", theme::green},
                 {"병력 · 이동 경로에 배치해 적 저지", theme::blue},
                 {"마법 · 전장에 즉시 시전하는 광역 공격", theme::purple}}};
            for (const auto &tip : tips) {
                gem(g, {pop.x + 22, py + 7}, 7, tip.second);
                g.text(tip.first, {pop.x + 36, py}, 12, {245, 248, 250}, 0, pop.w - 50);
                py += 26;
            }
            py += 4;
            g.line({{pop.x + 16, py}, {pop.x + pop.w - 16, py}}, 1, {42, 68, 84});
            py += 10;
            g.text("아군 선택 → 퇴각 · 철거", {pop.x + 18, py}, 11.5F, theme::muted, 0, pop.w - 36);

            // Confirm & Close Button
            button(g, ui, AcknowledgeDrawer, {pop.x + 16, pop.y + pop.h - 48, pop.w - 32, 38},
                   "확인 · 안내 닫기", true, true);
        }
        return;
    }

    // -------------------------------------------------------------
    // Desktop View: Sidebar + Side Wing Panel (누르면 옆에 날개 전개)
    // -------------------------------------------------------------
    panel(g, side);
    const float x = side.x + 16, w = side.w - 32;
    const float bottom = side.y + side.h - 56;

    // Tactical commander plaque
    g.rect({x, side.y + 13, w, 32.0F}, {18, 34, 42}, 6);
    g.rect({x, side.y + 13, 4.0F, 32.0F}, theme::gold, 2);
    const auto &desktopMeta = game::stageMetadata(state.play.currentStage);
    g.text(prep                                      ? "탐사 · 정비 지휘소"
           : run.state().phase == game::Phase::Ready ? desktopMeta.name + " 작전"
                                                     : desktopMeta.name + " 지휘소",
           {x + 12, side.y + 18}, layout.compact ? 16 : 18, theme::text);

    // Desktop Side Wing Tab Button (사이드바 좌측으로 살짝 튀어나온 날개 탭)
    if (!prep) {
        const float wingTabW = 36.0F, wingTabH = 34.0F;
        const Rect wingTabRect{side.x - wingTabW, side.y + 12.0F, wingTabW, wingTabH};
        button(g, ui, ToggleDrawer, wingTabRect, "", true, state.play.guideOpen);
        // Draw wing arrow indicator
        const float tabCx = wingTabRect.x + wingTabRect.w / 2.0F;
        const float tabCy = wingTabRect.y + wingTabRect.h / 2.0F;
        if (state.play.guideOpen) {
            // Arrow pointing right towards sidebar to close
            g.polygon({{tabCx + 4, tabCy}, {tabCx - 4, tabCy - 5}, {tabCx - 4, tabCy + 5}}, theme::gold);
        } else {
            // Arrow pointing left towards battlefield to open wing
            g.polygon({{tabCx - 4, tabCy}, {tabCx + 4, tabCy - 5}, {tabCx + 4, tabCy + 5}}, theme::gold);
        }

        // Tactical Side Wing Panel (누르면 옆에 날개가 펼쳐지는 느낌)
        if (state.play.guideOpen && !placingCard_) {
            const Rect wingBox{side.x - 248.0F, side.y + 48.0F, 242.0F, 268.0F};
            ui.add(GuideBackdrop, wingBox, false, false, false);
            g.rect({wingBox.x - 4, wingBox.y + 5, wingBox.w + 4, wingBox.h}, {0, 0, 0, 180}, 12);
            g.rect(wingBox, theme::gold, 12);
            g.rect({wingBox.x + 2.0F, wingBox.y + 2.0F, wingBox.w - 4.0F, wingBox.h - 4.0F}, {20, 36, 48},
                   10);

            // Wing Header
            g.rect({wingBox.x + 8, wingBox.y + 8, wingBox.w - 16, 32}, {28, 48, 62}, 6);
            g.rect({wingBox.x + 8, wingBox.y + 8, 4.0F, 32}, theme::gold, 2);
            g.text("작전 브리핑", {wingBox.x + 18, wingBox.y + 15}, 14, theme::gold);
            button(g, ui, CloseDrawer, {wingBox.x + wingBox.w - 36, wingBox.y + 9, 28, 30}, "X", true, false,
                   true);

            // Wing Tips
            float wy = wingBox.y + 50;
            const std::array<std::pair<const char *, Color>, 3> tips{
                {{"타워 · 원형 발판에 건설해 자동 공격", theme::green},
                 {"병력 · 이동 경로에 배치해 적 저지", theme::blue},
                 {"마법 · 전장에 즉시 시전하는 광역 공격", theme::purple}}};
            for (const auto &tip : tips) {
                gem(g, {wingBox.x + 20, wy + 7}, 6, tip.second);
                g.text(tip.first, {wingBox.x + 32, wy}, 11.5F, {240, 245, 248}, 0, wingBox.w - 44);
                wy += 25;
            }
            wy += 6;
            g.line({{wingBox.x + 12, wy}, {wingBox.x + wingBox.w - 12, wy}}, 1, {42, 68, 84});
            wy += 10;
            g.text("아군 선택 → 퇴각 · 철거", {wingBox.x + 14, wy}, 11, theme::muted, 0, wingBox.w - 28);
            wy += 20;
            g.text("적의 경로와 약점을 고려해 방어하세요", {wingBox.x + 14, wy}, 10.5F, theme::muted, 0,
                   wingBox.w - 28);
        }
    }

    if (prep) {

        const float half = (w - 8) / 2;
        button(g, ui, Surface, {x, side.y + 52, half, 36}, "지상 · 병력", true, !state.play.mine);
        button(g, ui, Mine, {x + half + 8, side.y + 52, half, 36}, "지하 · 채굴", true, state.play.mine);
        if (selected) {
            inspector(g, ui, run, state, {x, side.y + 97, w, side.h - 163});
        } else if (layout.compact || side.h < 570) {
            g.text("굴착 " + std::to_string(run.state().digs) + " / " +
                       std::to_string(run.catalog().balance.prepDigs) + "   스캔 " +
                       std::to_string(run.state().scans) + "   곡괭이 T" + std::to_string(run.state().tool),
                   {x, side.y + 99}, 12, theme::muted);
            if (run.state().nextExtra)
                g.text("+" + std::to_string(run.state().nextExtra), {x + w, side.y + 99}, 12, theme::red, 1);
            const float y = side.y + 124;
            if (side.h < 320) {
                const float third = (w - 12) / 3;
                button(g, ui, Scan, {x, y, third, 38}, state.play.scan ? "스캔 선택 중" : "무료 스캔",
                       run.state().scans > 0 && !run.state().job.active, state.play.scan);
                button(g, ui, Tool, {x + third + 6, y, third, 38},
                       run.state().tool >= 2 ? "T2 완료"
                                             : "T2 " + game::costLabel(run.catalog().balance.tool, true),
                       run.state().tool < 2);
                button(g, ui, Armor, {x + (third + 6) * 2, y, third, 38},
                       run.state().armor ? "장갑 완료"
                                         : "장갑 " + game::costLabel(run.catalog().balance.armor, true),
                       !run.state().armor);
                button(g, ui, Slots, {x, y + 46, third, 38},
                       run.state().slots >= run.catalog().balance.upgradedSlots
                           ? std::to_string(run.catalog().balance.upgradedSlots) + "슬롯 완료"
                           : "슬롯 " + game::costLabel(run.catalog().balance.slots, true),
                       run.state().slots < run.catalog().balance.upgradedSlots);
                button(g, ui, Recruit, {x + third + 6, y + 46, third, 38},
                       "모집 " + game::costLabel(run.state().recruitmentOffer
                                                     ? run.state().recruitmentOffer->cost
                                                     : run.catalog().balance.recruit,
                                                 true),
                       run.state().recruitmentOffer.has_value());
                button(g, ui, Coil, {x + (third + 6) * 2, y + 46, third, 38},
                       run.state().coil ? "코일 완료" : game::costLabel(run.catalog().balance.coil, true),
                       !run.state().coil);
            } else {
                button(g, ui, Scan, {x, y, half, 40}, state.play.scan ? "스캔 칸 선택 중" : "스캔 · 무료 1회",
                       run.state().scans > 0 && !run.state().job.active, state.play.scan);
                button(g, ui, Tool, {x + half + 8, y, half, 40},
                       run.state().tool >= 2 ? "T2 제작 완료"
                                             : "T2 · " + game::costLabel(run.catalog().balance.tool),
                       run.state().tool < 2);
                button(g, ui, Armor, {x, y + 48, half, 40},
                       run.state().armor ? "장갑 제작 완료"
                                         : "장갑 · " + game::costLabel(run.catalog().balance.armor),
                       !run.state().armor);
                button(g, ui, Slots, {x + half + 8, y + 48, half, 40},
                       run.state().slots >= run.catalog().balance.upgradedSlots
                           ? std::to_string(run.catalog().balance.upgradedSlots) + "슬롯 완료"
                           : "슬롯 " + std::to_string(run.catalog().balance.upgradedSlots) + " · " +
                                 game::costLabel(run.catalog().balance.slots),
                       run.state().slots < run.catalog().balance.upgradedSlots);
                button(g, ui, Recruit, {x, y + 96, half, 40},
                       "유닛 모집 · " + game::costLabel(run.state().recruitmentOffer
                                                            ? run.state().recruitmentOffer->cost
                                                            : run.catalog().balance.recruit),
                       run.state().recruitmentOffer.has_value());
                button(g, ui, Coil, {x + half + 8, y + 96, half, 40},
                       run.state().coil ? "코일 제작 완료"
                                        : "코일 · " + game::costLabel(run.catalog().balance.coil),
                       !run.state().coil);
            }
        } else {
            g.text("굴착 " + std::to_string(run.state().digs) + " / " +
                       std::to_string(run.catalog().balance.prepDigs) + "   스캔 " +
                       std::to_string(run.state().scans) + "   곡괭이 T" + std::to_string(run.state().tool),
                   {x, side.y + 110}, 12, theme::muted);
            if (run.state().job.active)
                meter(g, {x, side.y + 135, w, 5}, run.state().job.elapsed / run.state().job.duration,
                      theme::gold, 12);
            else if (run.state().nextExtra)
                g.text("둥지 경보 · 다음 공세 +" + std::to_string(run.state().nextExtra), {x, side.y + 134},
                       12, theme::red);
            float y = side.y + 161;
            button(g, ui, Scan, {x, y, w, 42},
                   state.play.scan ? "스캔할 경계 1칸을 선택" : "무료 스캔 · 정보만 공개",
                   run.state().scans > 0 && !run.state().job.active, state.play.scan);
            y += 52;
            button(g, ui, Tool, {x, y, w, 42},
                   run.state().tool >= 2 ? "T2 곡괭이 · 제작 완료"
                                         : "T2 곡괭이 · " + game::costLabel(run.catalog().balance.tool),
                   run.state().tool < 2);
            y += 52;
            button(g, ui, Armor, {x, y, w, 42},
                   run.state().armor ? "경비병 장갑 · 제작 완료"
                                     : "경비병 장갑 · " + game::costLabel(run.catalog().balance.armor),
                   !run.state().armor);
            y += 52;
            button(g, ui, Slots, {x, y, w, 42},
                   run.state().slots >= run.catalog().balance.upgradedSlots
                       ? std::to_string(run.catalog().balance.upgradedSlots) + "슬롯 · 강화 완료"
                       : "부대 슬롯 " + std::to_string(run.catalog().balance.upgradedSlots) + " · " +
                             game::costLabel(run.catalog().balance.slots),
                   run.state().slots < run.catalog().balance.upgradedSlots);
            y += 52;
            button(g, ui, Recruit, {x, y, w, 42},
                   "유닛 카드 모집 · " + game::costLabel(run.state().recruitmentOffer
                                                             ? run.state().recruitmentOffer->cost
                                                             : run.catalog().balance.recruit),
                   run.state().recruitmentOffer.has_value());
            y += 52;
            button(g, ui, Coil, {x, y, w, 42},
                   run.state().coil ? "전격 코일 · 제작 완료"
                                    : "전격 코일 · " + game::costLabel(run.catalog().balance.coil),
                   !run.state().coil);
        }
        button(g, ui, Start, {x, bottom, w, 44},
               run.state().job.active ? "광부 작업 중…" : "정비 완료 · 다음 웨이브", !run.state().job.active,
               true);
        return;
    }
    if (selected)
        inspector(g, ui, run, state, {x, side.y + 56, w, side.h - 122});
    else {
        const game::Card *card = run.card(state.play.selectedCard);
        float y = side.y + 59;
        if (card) {
            g.text(run.catalog().card(card->kind).name, {x, y}, layout.compact ? 18 : 22, theme::gold);
            y += 31;
            if (!layout.compact) {
                icon(g, run.catalog(), card->kind, {x + w / 2, y + 52}, 87);
                y += 118;
            }
            const game::PlayAvailability availability = run.playAvailability(card->id);
            const bool targeted = placement_ && placement_->card == card->id;
            const std::string cost =
                targeted
                    ? std::to_string(placement_->rule.cost)
                    : std::to_string(availability.minimumCost) +
                          (availability.variableCost() ? "~" + std::to_string(availability.maximumCost) : "");
            const std::string operation = targeted
                                              ? playActionName(placement_->rule.action)
                                              : cardCategoryName(run.catalog().card(card->kind).category);
            g.text(operation + "  ·  에너지 " + cost, {x, y}, 13, theme::text, 0, w);
            y += 28;
            g.text(targeted && !placement_->rule.allowed()
                       ? placement_->rule.message()
                       : cardInstruction(run.catalog().card(card->kind).category),
                   {x, y}, layout.compact ? 11 : 12,
                   targeted && !placement_->rule.allowed() ? theme::red : theme::muted, 0, w);
            y += 32;
            const float exchangeY = layout.compact ? std::min(y, bottom - 50) : y + 15;
            button(g, ui, Exchange, {x, exchangeY, w, 40}, "카드 교체 (에너지 1)",
                   run.state().phase == game::Phase::Battle && run.state().energy >= 1);
        } else {
            if (run.state().phase == game::Phase::Ready) {
                g.text("방어 병력을 배치하세요", {x, y}, layout.compact ? 13 : 15, theme::gold);
                y += 32;
                const std::array<const char *, 3> tips{
                    {"건물  →  둥근 설치 패드", "유닛  →  길 위 배치 칸", "마법  →  원하는 전장 위치"}};
                for (int index = 0; index < 3; ++index) {
                    g.circle({x + 7, y + 6}, 4,
                             index == 0   ? theme::green
                             : index == 1 ? theme::blue
                                          : theme::purple);
                    g.text(tips[index], {x + 20, y}, layout.compact ? 11 : 13, theme::text);
                    y += layout.compact ? 23 : 34;
                }
                if (!layout.compact) {
                    g.line({{x, y + 10}, {x + w, y + 10}}, 1, theme::edge);
                    g.text("배치한 건물·유닛을 누르면", {x, y + 37}, 12, theme::muted);
                    g.text("상세 확인과 철수가 가능해요", {x, y + 61}, 12, theme::muted);
                }
            } else {
                g.text("웨이브 진행", {x, y}, 13, theme::muted);
                g.text(std::to_string(run.waveSpawned()) + " / " + std::to_string(run.waveSpawnCount()),
                       {x + w, y}, 13, theme::gold, 1);
                y += 26;
                meter(g, {x, y, w, 8},
                      run.waveSpawnCount() ? static_cast<float>(run.waveSpawned()) / run.waveSpawnCount() : 0,
                      theme::gold, 16);
                y += 29;
                g.text("침투한 적 " + std::to_string(run.state().enemies.size()) + "   ·   누적 격파 " +
                           std::to_string(run.state().defeated),
                       {x, y}, 12, theme::text);
                y += 33;
                if (!layout.compact) {
                    g.text("카드를 선택해 방어선을 구축하세요", {x, y}, 12, theme::muted, 0, w);
                    g.text("배치된 대상을 누르면 퇴각/철거 가능", {x, y + 25}, 12, theme::muted, 0, w);
                }
            }
        }
    }
    if (run.state().phase == game::Phase::Ready) {
        button(g, ui, Start, {x, bottom, w - 62, 44}, "웨이브 시작", true, true);
        button(g, ui, Speed, {x + w - 54, bottom, 54, 44}, state.play.speed == 1 ? "1x" : "2x");
    } else {
        const float half = (w - 8) / 2;
        button(g, ui, Pause, {x, bottom, half, 44}, "일시정지");
        button(g, ui, Speed, {x + half + 8, bottom, half, 44}, state.play.speed == 1 ? "1배속" : "2배속");
    }
    if (!layout.compact && side.h > 590)
        button(g, ui, Reset, {x, bottom - 49, w, 34}, "재도전");
}

void View::cardFace(engine::Renderer &g, const game::Catalog &catalog, const game::Card &card, Rect box,
                    bool affordable, int displayedCost, bool variableCost, const CardCharge *charge) {
    const Color accent = theme::accent(catalog.card(card.kind).category);
    // Tactile card drop shadow
    g.rect({box.x, box.y + 5, box.w, box.h}, {8, 18, 24, 180}, 9);
    // Chunky category border (verified at box.y + 1 by application tests)
    g.rect(box, affordable ? accent : Color{96, 111, 110}, 9);
    // Warm white card face
    g.gradientRect({box.x + 3, box.y + 3, box.w - 6, box.h - 6}, {252, 250, 245}, {242, 237, 226}, 7);
    const Rect art{box.x + 6, box.y + 6, box.w - 12, box.h - (layout.compact ? 47.0F : 51.0F)};
    // Illustration window
    g.gradientRect(art, {42, 64, 76}, {26, 42, 52}, 6);
    g.circle({art.x + art.w / 2, art.y + art.h * .54F}, std::min(art.w * .38F, art.h * .46F),
             {accent.r, accent.g, accent.b, 70});
    icon(g, catalog, card.kind, {art.x + art.w / 2, art.y + art.h * .53F},
         std::min(layout.compact ? 42.0F : 58.0F, art.h * .9F));

    // Card Nameplate ribbon plaque
    const float plateY = art.y + art.h + 3.0F;
    const float plateH = layout.compact ? 18.0F : 21.0F;
    g.rect({box.x + 7, plateY, box.w - 14, plateH}, {235, 229, 216}, 4);
    g.text(catalog.card(card.kind).name, {box.x + box.w / 2, plateY + (layout.compact ? 2.5F : 3.0F)},
           layout.compact ? 12 : 14, theme::cardInk, .5F, box.w - 16);

    // Cute category pill badge
    const float badgeW = layout.compact ? 38.0F : 46.0F;
    const float badgeH = layout.compact ? 13.0F : 15.0F;
    const float badgeY = box.y + box.h - (layout.compact ? 16.0F : 18.0F);
    g.rect({box.x + (box.w - badgeW) / 2, badgeY, badgeW, badgeH},
           {accent.r, accent.g, accent.b, 55}, 6);
    g.text(cardCategoryName(catalog.card(card.kind).category),
           {box.x + box.w / 2, badgeY + (layout.compact ? 1.0F : 1.5F)},
           layout.compact ? 9 : 10, {80, 68, 50}, .5F);

    if (charge && charge->mode == CardChargeMode::Charging)
        chargeMask(g, art, charge->fraction);
    else if (!affordable)
        g.rect(art, {16, 30, 40, 110}, 6);

    // Overhanging chunky energy coin badge
    const ScreenPoint badge{box.x + 17, box.y + 17};
    gem(g, badge, 13, affordable ? theme::blue : Color{103, 121, 125});
    if (charge && charge->mode != CardChargeMode::Ready) {
        g.line(clockwiseArc(badge, 15, 0, 1), 2.5F,
               charge->mode == CardChargeMode::Blocked ? theme::red : Color{48, 70, 80});
        if (charge->mode == CardChargeMode::Charging && charge->fraction > 0)
            g.line(clockwiseArc(badge, 15, 0, charge->fraction), 2.5F, theme::blue);
    }
    g.text(std::to_string(displayedCost >= 0 ? displayedCost : catalog.card(card.kind).cost) +
               (variableCost ? "+" : ""),
           {box.x + 17, box.y + 9}, 12, {255, 255, 240}, .5F);
}

void View::hand(engine::Renderer &g, engine::Ui &ui, const game::Run &run, const State &state) {
    const Rect area = layout.hand;
    // Integrated card dock tray with elevated tabs
    const float trayTop = area.y + 14.0F;
    g.gradientRect({area.x, trayTop, area.w, area.h - 14.0F}, {16, 30, 38, 175}, {10, 20, 26, 225}, 10);
    g.rect({area.x, trayTop, area.w, 2.0F}, theme::edge, 1);

    // Elevated Deck Tab (clean game nameplate)
    const float tabH = layout.compact ? 20.0F : 22.0F;
    const float tabW = layout.compact ? 78.0F : 88.0F;
    const Rect tabRect{area.x + 12.0F, area.y + 3.0F, tabW, tabH};
    g.rect({tabRect.x, tabRect.y + 1.0F, tabRect.w, tabRect.h}, {8, 16, 22}, 5);
    g.rect(tabRect, {36, 58, 72}, 5);
    g.rect({tabRect.x + 1.5F, tabRect.y + 1.5F, tabRect.w - 3.0F, tabRect.h - 3.0F}, {20, 36, 46}, 4);
    g.rect({tabRect.x + 5.0F, tabRect.y + 4.5F, 3.0F, tabRect.h - 9.0F}, theme::gold, 1.5F);
    g.text("전투 덱",
           {tabRect.x + 14.0F, tabRect.y + (layout.compact ? 3.0F : 3.5F)}, layout.compact ? 11 : 12, theme::gold);

    // Elevated Next Card Tab
    const game::Card &next = run.state().deck[run.state().queue.front()];
    const std::string preview = std::string("다음 ") + run.catalog().card(next.kind).name + " (" +
                                std::to_string(run.catalog().card(next.kind).cost) + ")";
    const float nextW = layout.compact ? 104.0F : 118.0F;
    const Rect nextRect{area.x + area.w - nextW - 12.0F, area.y + 3.0F, nextW, tabH};
    g.rect({nextRect.x, nextRect.y + 1.0F, nextRect.w, nextRect.h}, {8, 16, 22}, 5);
    g.rect(nextRect, {32, 52, 64}, 5);
    g.rect({nextRect.x + 1.5F, nextRect.y + 1.5F, nextRect.w - 3.0F, nextRect.h - 3.0F}, {18, 30, 38}, 4);
    g.text(preview, {nextRect.x + nextRect.w / 2, nextRect.y + (layout.compact ? 3.0F : 3.5F)},
           layout.compact ? 10 : 11, theme::text, 0.5F, nextW - 12);
    const float gap = 9, cardWidth = (area.w - 30 - 3 * gap) / 4;
    for (int index = 0; index < 4; ++index) {
        const game::Card &card = run.state().deck[run.state().hand[index]];
        const CardTransition &transition = state.play.feedback.cards[index];
        const bool changing = transition.age < Feedback::CardDuration;
        const bool busy = state.play.feedback.cardBusy(index);
        const bool selected = card.id == state.play.selectedCard;
        const game::PlayAvailability availability = run.playAvailability(card.id);
        const bool targeted = placement_ && placement_->card == card.id;
        const bool affordable = run.state().phase != game::Phase::Battle ||
                                (targeted ? placement_->rule.allowed() : availability.allowed);
        const int cost = targeted ? placement_->rule.cost : availability.minimumCost;
        const bool variable = !targeted && availability.variableCost();
        handCharge_[index] =
            cardCharge(run, card.id, cost, targeted ? placement_->rule.failure : availability.failure);
        Rect box{area.x + 15 + index * (cardWidth + gap), area.y + 29 - (selected ? 6.0F : 0.0F), cardWidth,
                 area.h - 71};
        ui.add(CardBase + card.id, box, !busy, true, true, true);
        const bool rejected =
            state.play.feedback.rejectedCard == card.id && state.play.feedback.rejectionAge < .32F;
        if (rejected && !state.app.preferences.reducedMotion)
            box.x += std::sin(state.play.feedback.rejectionAge * 65) * 4 *
                     (1 - state.play.feedback.rejectionAge / .32F);
        if (selected || ui.focused == CardBase + card.id || changing || rejected)
            g.rect({box.x - 3, box.y - 3, box.w + 6, box.h + 6},
                   rejected               ? theme::red
                   : selected || changing ? theme::gold
                                          : theme::blue,
                   10);
        g.rect(box, theme::inset, 9);
        const Rect clip{box.x - 4, area.y + 22, box.w + 8, area.h - 62};
        if (changing && transition.age < Feedback::CardExit && transition.outgoing.id) {
            const float t = transition.age / Feedback::CardExit;
            Rect outgoing = box;
            outgoing.y -= t * box.h * .45F;
            g.beginClip(clip, static_cast<std::uint8_t>(255 * (1 - t)));
            cardFace(g, run.catalog(), transition.outgoing, outgoing, true);
            g.endClip();
        }
        if (!changing || transition.age >= Feedback::CardExit) {
            const float t = changing ? std::clamp((transition.age - Feedback::CardExit) /
                                                      (Feedback::CardArrival - Feedback::CardExit),
                                                  0.0F, 1.0F)
                                     : 1.0F;
            const float ease = 1 - std::pow(1 - t, 3.0F);
            Rect incoming = box;
            incoming.y += (1 - ease) * box.h * .7F;
            g.beginClip(clip, static_cast<std::uint8_t>(255 * std::min(1.0F, t * 2)));
            cardFace(g, run.catalog(), card, incoming, affordable, cost, variable, &handCharge_[index]);
            g.endClip();
        }
        if (changing) {
            const float glow = std::clamp(1 - (transition.age - Feedback::CardArrival) / .47F, 0.0F, 1.0F);
            g.rect({box.x + box.w - std::min(44.0F, box.w - 10), box.y + 7, std::min(38.0F, box.w - 16), 16},
                   {241, 192, 102, static_cast<std::uint8_t>(235 * glow)}, 4);
            g.text(transition.age < Feedback::CardExit ? "순환" : "DRAW", {box.x + box.w - 10, box.y + 10}, 8,
                   {43, 53, 54, static_cast<std::uint8_t>(255 * glow)}, 1);
        }
        if (!layout.compact)
            g.text(std::to_string(index + 1), {box.x + box.w - 12, box.y + box.h - 17}, 10, {120, 105, 77},
                   1);
    }
    handEnergy(g, run, state);
}

void View::handEnergy(engine::Renderer &g, const game::Run &run, const State &state) {
    const Rect bar = layout.energyBar;
    const int energy = run.state().energy;
    const int cap = run.catalog().balance.energyCap;
    const float next = energy < cap ? run.state().energyTime / run.catalog().balance.energyInterval : 0;
    g.rect(bar, theme::inset, 6);
    gem(g, {bar.x + 15, bar.y + 13}, 10, theme::blue);
    g.text(std::to_string(energy) + " / " + std::to_string(cap), {bar.x + 31, bar.y + 5}, 14, theme::text);
    meter(g, layout.energyMeter, (energy + next) / cap, theme::blue, 10);
    if (state.play.feedback.energyAge < .65F) {
        const float age = state.play.feedback.energyAge;
        g.text("-" + std::to_string(state.play.feedback.energySpent),
               {bar.x + bar.w - 6, bar.y + 7 - age * 4}, 12,
               {255, 217, 137, static_cast<std::uint8_t>(255 * (1 - age / .65F))}, 1);
    }
}

void View::draw(engine::Renderer &g, engine::Ui &ui, const game::Run &run, const State &state,
                const engine::AudioSettings &audio) {
    placement_.reset();
    placingCard_ = 0;
    inspection_.reset();
    offerCard_.reset();
    offerDetailsBounds_ = {};
    handCharge_ = {};
    if (!state.app.navigation.gameplay()) {
        frontEnd(g, ui, run, state, audio);
        return;
    }
    const bool prep = run.state().phase == game::Phase::Prep;
    const bool mineView = prep && state.play.mine;
    measure(g.width, g.height, prep, mineView, state.play.tutorial.active());
    preparePlacement(run, ui, state, mineView);
    ui.beginFrame();
    g.gradientRect({0, 0, static_cast<float>(g.width), static_cast<float>(g.height)}, {28, 46, 55},
                   theme::background);
    for (int index = 0; index < g.width / 76 + 1; ++index)
        g.line({{index * 76.0F, -20}, {index * 76.0F + 60, static_cast<float>(g.height)}}, 1,
               {98, 128, 125, 12});
    hud(g, ui, run, state, audio);
    // Crisp tactical map shadow and frame instead of nested box-in-box
    g.rect({layout.world.x, layout.world.y + 4.0F, layout.world.w, layout.world.h}, {6, 15, 20, 150}, 12);
    g.rect(layout.world, {75, 102, 98}, 10);
    ui.add(World, layout.world, true, false, false);
    if (mineView)
        underground(g, run, state);
    else
        surface(g, run, state);
    if (state.play.toastTime > 0 && !state.play.toast.empty()) {
        const Rect toast{layout.world.x + 8, layout.world.y + layout.world.h - 38, layout.world.w - 16, 29};
        g.rect(toast, {15, 31, 38, 235}, 6);
        g.rect({toast.x, toast.y, 3, toast.h}, state.play.error ? theme::red : theme::gold, 1);
        g.text(state.play.toast, {toast.x + toast.w / 2, toast.y + 7}, layout.compact ? 10 : 12, theme::text,
               .5F, toast.w - 16);
    }
    commandPanel(g, ui, run, state);
    if (!prep)
        hand(g, ui, run, state);
    tutorialHighlights(g, ui, run, state);
    if (ui.dragging && ui.captured >= CardBase && ui.captured < DeckBase) {
        const game::Card *card = run.card(ui.captured - CardBase);
        if (card) {
            const ScreenPoint p = ui.pointerPosition;
            g.circle(p, 30, {12, 29, 36, 210});
            g.circle(p, 27, {theme::gold.r, theme::gold.g, theme::gold.b, 80});
            icon(g, run.catalog(), card->kind, p, 43);
        }
    }
    placementHint(g);
    overlay(g, ui, run, state, audio);
    if (state.play.error && state.play.toastTime > 0 &&
        (state.play.modal != presentation::Modal::None || run.state().expansionPending ||
         run.state().rewardOffer)) {
        const float width = std::min(640.0F, g.width - 24.0F);
        const Rect error{(g.width - width) / 2, g.height - 48.0F, width, 36};
        g.rect(error, {137, 57, 47}, 8);
        g.text(state.play.toast, {error.x + width / 2, error.y + 9}, 12, theme::text, .5F, width - 20);
    }
    cardInspection(g, ui, run);
    tutorialOverlay(g, ui, state);
}

void View::overlay(engine::Renderer &g, engine::Ui &ui, const game::Run &run, const State &state,
                   const engine::AudioSettings &audio) {
    const bool terminal = run.state().phase == game::Phase::Won || run.state().phase == game::Phase::Lost;
    if (state.play.modal == presentation::Modal::None && !run.state().expansionPending &&
        !run.state().rewardOffer && !state.play.paused && !terminal)
        return;
    ui.beginFrame();
    ui.add(900, {0, 0, static_cast<float>(g.width), static_cast<float>(g.height)}, false);
    g.rect({0, 0, static_cast<float>(g.width), static_cast<float>(g.height)}, {4, 13, 20, 200});
    const bool replacement = run.state().rewardOffer || state.play.modal == presentation::Modal::Replacement;
    const float width = std::min(660.0F, g.width - 28.0F);
    const float height =
        replacement                                           ? std::min(566.0F, g.height - 32.0F)
        : state.play.modal == presentation::Modal::Recruit    ? std::min(440.0F, g.height - 32.0F)
        : state.play.modal == presentation::Modal::Withdrawal ? 360.0F
        : state.play.paused && state.play.modal == presentation::Modal::None && !terminal ? 390.0F
                                                                                          : 330.0F;
    const Rect box{(g.width - width) / 2, (g.height - height) / 2, width, height};
    panel(g, box, true);
    const float x = box.x + 22, y = box.y + 25, w = box.w - 44;
    g.rect({x, y, w, 3}, theme::gold, 1);
    if (terminal) {
        const bool won = run.state().phase == game::Phase::Won;
        const auto &meta = game::stageMetadata(state.play.currentStage);
        g.text(won ? (meta.name + " 방어 성공!") : "원정대 철수",
               {box.x + width / 2, y + 23}, layout.compact ? 22 : 28, won ? theme::gold : theme::red, .5F);
        gem(g, {box.x + width / 2, y + 109}, 35, won ? theme::gold : theme::red);
        g.text("도달 웨이브 " + std::to_string(run.state().wave) + " / " + std::to_string(meta.wavesCount) +
                   "   ·   누적 격파 " + std::to_string(run.state().defeated),
               {box.x + width / 2, y + 170}, 14, theme::text, .5F);
        const bool hasNextStage = won && state.play.currentStage < game::TotalStages;
        if (hasNextStage) {
            const float third = (w - 16) / 3;
            button(g, ui, NextStageRun, {x, box.y + height - 72, third * 1.35F, 45}, "다음 구역 출격", true, true);
            button(g, ui, ConfirmReset, {x + third * 1.35F + 8, box.y + height - 72, third * 0.8F, 45}, "재도전");
            button(g, ui, GoHome, {x + third * 2.15F + 16, box.y + height - 72, third * 0.85F, 45}, "메인");
        } else {
            const float half = (w - 10) / 2;
            button(g, ui, ConfirmReset, {x, box.y + height - 72, half, 45}, won ? "다시 도전" : "새 원정 시작",
                   canConfirmNewRun(run.state().phase, state), true);
            button(g, ui, GoHome, {x + half + 10, box.y + height - 72, half, 45}, "메인 화면");
        }
        return;
    }
    if (run.state().expansionPending) {
        g.text("전선을 확장하세요", {x, y + 22}, layout.compact ? 23 : 28, theme::text);
        g.text("다음 공세의 진입점이 바뀝니다", {x, y + 66}, 13, theme::muted);
        button(g, ui, ExpandLong, {x, y + 112, w, 49}, "우회 협곡 · 긴 방어 경로", true, true);
        button(g, ui, ExpandShort, {x, y + 177, w, 49}, "노천 방어지 · 추가 패드 4개");
        g.text("기존 건물과 생존 병력은 유지됩니다", {x, y + 247}, 11, theme::muted, 0, w);
        return;
    }
    if (replacement || state.play.modal == presentation::Modal::Recruit) {
        cardOfferOverlay(g, ui, run, state, box, replacement);
        return;
    }
    if (state.play.modal == presentation::Modal::Withdrawal) {
        const game::Tower *tower = run.tower(state.play.selectedTower);
        const game::Unit *unit = run.unit(state.play.selectedUnit);
        const game::CardKind kind = tower ? tower->kind : unit ? unit->kind : game::CardKind::Guard;
        g.text(tower ? "건물을 철거할까요?" : "유닛을 철수할까요?", {x, y + 22}, layout.compact ? 22 : 27,
               theme::text);
        icon(g, run.catalog(), kind, {box.x + width / 2, y + 104}, 64);
        g.text(run.catalog().card(kind).name +
                   (tower ? "  Lv. " + std::to_string(tower->level) : std::string("")),
               {box.x + width / 2, y + 144}, 15, theme::gold, .5F);
        g.text(tower ? "건물과 강화 레벨이 사라집니다" : "전장 슬롯 1개가 비워집니다",
               {box.x + width / 2, y + 172}, layout.compact ? 12 : 14, theme::muted, .5F, w);
        g.text("사용한 에너지·광물은 반환되지 않아요", {box.x + width / 2, y + 198}, layout.compact ? 11 : 12,
               theme::red, .5F, w);
        button(g, ui, ConfirmDismiss, {x, box.y + height - 111, w, 44},
               tower ? "건물 철거 확정" : "유닛 철수 확정", true, false, true);
        button(g, ui, Close, {x, box.y + height - 57, w, 38}, "취소 · 배치 유지");
        return;
    }
    if (state.play.modal == presentation::Modal::NewRun) {
        g.text("원정을 다시 시작할까요?", {x, y + 25}, layout.compact ? 21 : 27, theme::text);
        g.text("현재 판의 채굴·카드·배치가 초기화돼요", {x, y + 85}, 12, theme::muted, 0, w);
        button(g, ui, ConfirmReset, {x, y + 151, w, 44}, "새 원정 시작",
               canConfirmNewRun(run.state().phase, state), true);
        button(g, ui, Close, {x, y + 207, w, 40}, "원정 계속하기");
        return;
    }
    g.text("작전 일시 정지", {x, y + 17}, 27, theme::gold);
    g.text("계속하기 전까지 전투·굴착 시간이 멈춥니다", {x, y + 66}, layout.compact ? 11 : 13, theme::muted,
           0, w);
    const float half = (w - 10) / 2;
    button(g, ui, Resume, {x, y + 108, w, 46}, "작전 계속하기", true, true);
    button(g, ui, OpenSettings, {x, y + 167, half, 40}, "설정");
    button(g, ui, OpenGuide, {x + half + 10, y + 167, half, 40}, "플레이 방법");
    button(g, ui, GoHome, {x, y + 219, half, 40}, "메인 화면");
    button(g, ui, Reset, {x + half + 10, y + 219, half, 40}, "재도전");
    button(g, ui, AudioMute, {x, y + 276, half, 36}, audio.muted ? "효과음 켜기" : "효과음 끄기");
    button(g, ui, AudioVolume, {x + half + 10, y + 276, half, 36},
           "볼륨 " + std::to_string(static_cast<int>(audio.volume * 100)) + "%");
}
} // namespace thorium::presentation
