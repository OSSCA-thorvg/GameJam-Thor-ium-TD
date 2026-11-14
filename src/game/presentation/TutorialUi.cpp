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
        return {"1/8 · 거점과 에너지", "카드 방어 → 지하 채굴 → 다음 공세 준비",
                "최종 공세까지 거점을 사수하는 것이 목표입니다 (시작 에너지 6)"};
    case Lesson::Building:
        return {"2/8 · 첫 건물 배치", "연사탑을 초록색 원형 패드에 배치하세요.", "카드를 선택하거나 끌면 사거리가 표시됩니다."};
    case Lesson::Unit:
        return {"3/8 · 비용과 유닛", "경비병을 길 위의 초록색 칸에 배치하세요.",
                "남은 에너지 3을 소모하여 적의 진격을 저지합니다."};
    case Lesson::Combat:
        return {"4/8 · 전투와 카드 순환", "사용한 카드는 덱 맨 뒤로 이동하며,", "대기열의 다음 카드가 손패로 들어옵니다."};
    case Lesson::Fork:
        return {"5/8 · 갈림길", "깜빡이는 바리케이드를 눌러 적의 이동 경로를 바꾸세요.",
                "정비 중에만 전환 가능하며 적은 열린 경로로 이동합니다."};
    case Lesson::Scan:
        return {"6/8 · 무료 스캔", "스캔을 켜고 빛나는 칸을 선택하세요.",
                "암석 내부를 먼저 확인한 뒤 안전하게 채굴할 수 있습니다."};
    case Lesson::FirstDig:
        return {"6/8 · 첫 채굴", "빛나는 칸을 타격해 암석을 걷어내세요.", "광맥 전체가 드러나면 철을 획득합니다."};
    case Lesson::Crafting:
        return {"7/8 · 도구 제작", "구리 광맥을 타격해 완전히 드러내세요.", "철 4, 구리 4를 모아 T2 곡괭이를 제작합니다."};
    case Lesson::Investment:
        return {"8/8 · 다음 투자", "T2 곡괭이로 단단한 암석 속의 금을 채굴하세요.",
                "금으로 카드를 모집하거나 부대 슬롯을 확장할 수 있습니다."};
    case Lesson::Complete:
        return {"연습 원정 완료", "방어와 채굴, 장비 강화의 기본 흐름을 모두 익혔습니다.",
                "정규 원정은 기본 덱과 초기 자원으로 새롭게 시작합니다."};
    case Lesson::Failed:
        return {"연습 다시 도전", "거점이 무너졌습니다.", "다시하기를 누르면 공세 시작 시점부터 재도전합니다."};
    default:
        return {"", "", ""};
    }
}
/** Detail lines of the mining lessons: how veins pay, what the tools do, what hides in the rock and how the
 *  pick is upgraded. Numbers come from the catalog. */
std::vector<std::string> lessonDetails(Lesson lesson, const game::Run &run) {
    const game::RunBalance &balance = run.catalog().balance;
    const std::string pick = std::to_string(balance.pickPower), blast = std::to_string(balance.blastDigs);
    switch (lesson) {
    case Lesson::Scan:
        return {"· 스캔은 3×3 영역의 암석 내부를 투시합니다", "· 암석이 1겹만 남아도 내부가 비쳐 보입니다",
                "· 유물, 카드 화석, 에너지 수정이 묻혀 있습니다", "· 둥지를 건드리면 다음 웨이브의 적이 늘어납니다"};
    case Lesson::FirstDig:
        return {"· 곡괭이(굴착 1회): 타격 칸 " + pick + "겹, 상하좌우 4칸 1겹 제거",
                "· 광맥은 테두리 안의 칸이 모두 드러나야 획득합니다", "· 맨 윗줄이나 이미 드러난 칸 인접부만 타격 가능",
                "· 남은 굴착 횟수는 채굴 화면 상단에 표시됩니다"};
    case Lesson::Crafting:
        return {"· 철 4, 구리 4를 모아 'T2 제작'을 누르면 곡괭이 강화",
                "· T2 곡괭이는 타격 칸을 " + std::to_string(balance.pickPower + 1) + "겹씩 깎아냅니다",
                "· 'T2' 표시가 붙은 단단한 암석도 파쇄 가능",
                "· 정규 원정에서는 정비 패널의 도구 버튼으로 강화"};
    case Lesson::Investment:
        return {"· 금은 카드 모집, 부대 슬롯 확장, 건물 각성에 사용",
                "· 발파(굴착 " + blast + "회): 3×3 영역을 1겹씩 제거 (T2부터 2겹)",
                "· 발파는 넓은 범위를 깎지만 닿은 둥지를 즉시 깨웁니다", "· 굴착 횟수는 매 정비마다 다시 충전됩니다"};
    default:
        return {};
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
    const std::vector<std::string> details = lessonDetails(lesson, run);
    if (!dense && area.h > 270) {
        float y = area.y + 100;
        if (details.empty()) {
            g.text("연습 원정 중에도 기존 원정 기록은 보존됩니다", {x, y + 8}, 11, theme::green, 0, width);
            g.text("배치를 잘못했다면 현재 단계 다시하기", {x, y + 32}, 11, theme::muted, 0, width);
        }
        for (const std::string &line : details) {
            g.text(line, {x, y}, 11, theme::text, 0, width);
            y += 22;
        }
        if (lesson == Lesson::Crafting)
            g.text("현재 철 " + std::to_string(run.state().iron) + " · 구리 " +
                       std::to_string(run.state().copper),
                   {x, y + 8}, 12, theme::gold, 0, width);
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
void View::tutorialTips(engine::Renderer &g, engine::Ui &ui, const game::Run &run, const State &state) {
    // A short sidebar has no room for the lesson details, so they sit over the cutaway and fold on tap.
    const std::vector<std::string> details = lessonDetails(state.play.tutorial.lesson, run);
    if (details.empty() || !state.play.mine || layout.sidebar.h >= 270 || state.play.paused)
        return;
    const engine::Rect view = layout.mineViewport;
    const float size = 10, line = 15;
    const bool folded = state.app.introFolded;
    const engine::Rect card = folded ? engine::Rect{view.x + 6, view.y + view.h - 28, 96, 22}
                                     : engine::Rect{view.x + 6, view.y + view.h - 12 - line * details.size(),
                                                    view.w - 12, 8 + line * details.size()};
    g.rect(card, {14, 24, 30, 235}, 6);
    g.rect({card.x, card.y, 3, card.h}, theme::gold, 1);
    if (folded)
        g.text("채굴 도움말 · 펼치기", {card.x + 9, card.y + 5}, size, theme::gold);
    else {
        for (std::size_t index = 0; index < details.size(); ++index)
            g.text(details[index], {card.x + 9, card.y + 5 + line * index}, size, theme::text, 0,
                   card.w - 40);
        g.text("접기", {card.x + card.w - 8, card.y + 5}, size, theme::muted, 1);
    }
    ui.add(ToggleIntro, card, true, false, false);
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
    } else if (lesson == Lesson::Fork && !run.catalog().forks.empty()) {
        const engine::ScreenPoint center = layout.screenPoint(run.barricade(0));
        const float radius = std::max(9.0F, layout.tile * .6F);
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
        const engine::Rect box = layout.mineCellBounds(cell);
        outline(g, {box.x + 2, box.y + 2, box.w - 4, box.h - 4});
    }
}
} // namespace thorium::presentation
