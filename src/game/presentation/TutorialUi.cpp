#include "Theme.h"
#include "View.h"
#include <algorithm>

namespace thorium::presentation {
namespace {
/** Short copy that fits both a desktop sidebar and a 320px lesson panel. */
struct LessonCopy {
    const char *title;
    const char *first;
    const char *second;
};
/** Describe each observed lesson without deciding when gameplay advances. */
LessonCopy lessonCopy(Lesson lesson) {
    switch (lesson) {
    case Lesson::Briefing:
        return {"1/7 · 거점과 에너지", "카드 방어 → 채굴 → 다음 공세 준비",
                "마지막 공세 생존이 목표 · 에너지 6"};
    case Lesson::Building:
        return {"2/7 · 첫 건물 배치", "연사탑을 초록 발판에 놓으세요.", "선택하거나 끌면 사거리가 보여요."};
    case Lesson::Unit:
        return {"3/7 · 비용과 유닛", "경비병을 길 위 초록 칸에 놓으세요.",
                "남은 에너지 3 사용 · 적의 진격 저지"};
    case Lesson::Combat:
        return {"4/7 · 전투와 카드 순환", "사용한 카드는 덱 뒤로 돌아가요.", "다음 카드가 손패에 들어와요."};
    case Lesson::Scan:
        return {"5/7 · 무료 스캔", "스캔을 켜고 빛나는 칸을 누르세요.",
                "정보만 공개 · 굴착과 광물은 별도예요."};
    case Lesson::FirstDig:
        return {"5/7 · 첫 채굴", "공개된 칸을 눌러 굴착하세요.", "광부가 도착해야 철을 얻어요."};
    case Lesson::Crafting:
        return {"6/7 · 도구 제작", "빛나는 칸을 따라 재료를 모으세요.",
                "철 4 · 구리 4로 T2 곡괭이를 만들어요."};
    case Lesson::Investment:
        return {"7/7 · 다음 투자", "T2 지층을 캐서 금을 얻으세요.", "금으로 카드 모집 · 부대 슬롯 강화"};
    case Lesson::Complete:
        return {"연습 원정 완료", "방어하고, 채굴하고, 더 강해졌어요.",
                "정규 원정은 기본 덱과 자원으로 시작해요."};
    case Lesson::Failed:
        return {"연습 다시 도전", "거점이 무너졌지만 괜찮아요.", "다시하기로 공세 시작 시점에 돌아가요."};
    default:
        return {"", "", ""};
    }
}
/** Draw a highlight without claiming pointer input or exposing hidden tile contents. */
void outline(engine::Renderer &graphics, engine::Rect bounds) {
    graphics.rect({bounds.x - 3, bounds.y - 3, bounds.w + 6, bounds.h + 6}, {97, 225, 169, 70}, 7);
    graphics.line({{bounds.x, bounds.y + bounds.h},
                   {bounds.x, bounds.y},
                   {bounds.x + bounds.w, bounds.y},
                   {bounds.x + bounds.w, bounds.y + bounds.h},
                   {bounds.x, bounds.y + bounds.h}},
                  2.5F, theme::green);
}
} // namespace
void View::tutorialPanel(engine::Renderer &g, engine::Ui &ui, const game::Run &run, const State &state) {
    const engine::Rect area = layout.sidebar;
    panel(g, area, true);
    const Lesson lesson = state.play.tutorial.lesson;
    const LessonCopy copy = lessonCopy(lesson);
    const float x = area.x + 12, width = area.w - 24;
    const bool dense = area.h < 180;
    g.text(copy.title, {x, area.y + 10}, dense ? 13 : 16, theme::gold, 0, width);
    g.text(copy.first, {x, area.y + (dense ? 34 : 47)}, dense ? 10 : 12, theme::text, 0, width);
    g.text(copy.second, {x, area.y + (dense ? 51 : 69)}, dense ? 10 : 12, theme::muted, 0, width);
    if (!dense && area.h > 270) {
        g.text("연습 원정 · 정규 원정은 보존돼요", {x, area.y + 108}, 11, theme::green, 0, width);
        g.text("잘못 놓았다면 현재 단계 다시하기", {x, area.y + 132}, 11, theme::muted, 0, width);
        if (lesson == Lesson::Crafting)
            g.text("현재 철 " + std::to_string(run.state().iron) + " · 구리 " +
                       std::to_string(run.state().copper),
                   {x, area.y + 163}, 12, theme::gold, 0, width);
    }
    const float gap = 6, buttonWidth = (width - 2 * gap) / 3, y = area.y + area.h - 42;
    button(g, ui, TutorialRetry, {x, y, buttonWidth, 32}, "다시하기");
    button(g, ui, TutorialExit, {x + buttonWidth + gap, y, buttonWidth, 32}, "건너뛰기");
    const engine::Rect action{x + 2 * (buttonWidth + gap), y, buttonWidth, 32};
    if (state.play.paused)
        button(g, ui, Resume, action, "계속", true, true);
    else if (lesson == Lesson::Briefing)
        button(g, ui, TutorialNext, action, "시작", true, true);
    else if (lesson == Lesson::Scan)
        button(g, ui, Scan, action, state.play.scan ? "스캔 ON" : "스캔", true, state.play.scan);
    else if (lesson == Lesson::Crafting)
        button(g, ui, Tool, action, "T2 제작", run.state().iron >= 4 && run.state().copper >= 4, true);
    else if (lesson == Lesson::Combat)
        button(g, ui, Pause, action, "일시정지");
}
void View::tutorialOverlay(engine::Renderer &g, engine::Ui &ui, const State &state) {
    const Lesson lesson = state.play.tutorial.lesson;
    if (lesson != Lesson::Complete && lesson != Lesson::Failed)
        return;
    const LessonCopy copy = lessonCopy(lesson);
    ui.beginFrame();
    ui.add(902, {0, 0, static_cast<float>(g.width), static_cast<float>(g.height)}, false, false, false);
    g.rect({0, 0, static_cast<float>(g.width), static_cast<float>(g.height)}, {6, 16, 24, 225});
    const float modalWidth = std::min(460.0F, g.width - 28.0F);
    const engine::Rect box{(g.width - modalWidth) / 2, (g.height - 300.0F) / 2, modalWidth, 300};
    panel(g, box, true);
    const float left = box.x + 16, w = box.w - 32;
    g.text(copy.title, {left, box.y + 22}, 21, theme::gold, 0, w);
    g.text(copy.first, {left, box.y + 65}, 12, theme::text, 0, w);
    g.text(copy.second, {left, box.y + 90}, 11, theme::muted, 0, w);
    if (lesson == Lesson::Complete) {
        g.text("금 4 → 유닛 카드 3택1 또는 슬롯 6 → 8", {left, box.y + 122}, 11, theme::gold, 0, w);
        button(g, ui, TutorialRegular, {left, box.y + 154, w, 38}, "정규 원정 시작", true, true);
        button(g, ui, BeginTutorial, {left, box.y + 200, w, 36}, "처음부터 다시 연습");
    } else
        button(g, ui, TutorialRetry, {left, box.y + 174, w, 40}, "공세 시작부터 다시하기", true, true);
    button(g, ui, TutorialExit, {left, box.y + 246, w, 36}, "메인 화면으로");
}
void View::tutorialHighlights(engine::Renderer &g, const engine::Ui &ui, const game::Run &run,
                              const State &state) {
    const Lesson lesson = state.play.tutorial.lesson;
    if (lesson == Lesson::None || lesson == Lesson::Complete || lesson == Lesson::Failed || state.play.paused)
        return;
    engine::WidgetId widget = 0;
    if (lesson == Lesson::Briefing || lesson == Lesson::Unit)
        outline(g, layout.energyBar);
    if (lesson == Lesson::Building || lesson == Lesson::Unit) {
        const game::CardKind kind = lesson == Lesson::Building ? game::CardKind::Gun : game::CardKind::Guard;
        for (int index : run.state().hand)
            if (run.state().deck[index].kind == kind) {
                widget = CardBase + run.state().deck[index].id;
                break;
            }
        const engine::ScreenPoint center =
            layout.screenPoint(lesson == Lesson::Building ? game::Point{6, 4} : game::Point{8, 4});
        const float radius = std::max(7.0F, layout.tile * .5F);
        g.circle(center, radius + 4, {97, 225, 169, 65});
        rangeRing(g, center, radius, theme::green, false);
    } else if (lesson == Lesson::Scan)
        widget = Scan;
    else if (lesson == Lesson::Crafting && state.play.tutorial.mineCell < 0)
        widget = Tool;
    for (const engine::Widget &item : ui.widgets)
        if (item.id == widget)
            outline(g, item.bounds);
    const int cell = state.play.tutorial.mineCell;
    if (cell >= 0 && state.play.mine) {
        const float size = layout.mineTile;
        outline(g, {layout.mineOrigin.x + (cell % 5) * size + 2, layout.mineOrigin.y + (cell / 5) * size + 2,
                    size - 4, size - 4});
    }
}
} // namespace thorium::presentation
