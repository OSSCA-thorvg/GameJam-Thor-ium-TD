#include "TutorialDirector.h"
#include "game/content/TutorialCatalog.h"
#include <utility>

namespace thorium {
using presentation::Lesson;
TutorialDirector::TutorialDirector(game::Run &run, presentation::State &state, engine::Ui &ui,
                                   FeedbackChannel &feedback, presentation::Soundscape &sound)
    : run(run), state(state), ui(ui), feedback(feedback), sound(sound) {}
bool TutorialDirector::begin() {
    game::RunCreation created = game::Run::create(game::tutorialCatalog());
    if (!created.run) {
        feedback.notify({false, "연습 데이터를 불러오지 못했어요"});
        return false;
    }
    if (!active()) {
        regularRun_ = run;
        regularPlay_ = state.play;
        regularNavigation_ = state.app.navigation;
    }
    run = std::move(*created.run);
    run.startWave();
    state.play = presentation::PlayState{};
    state.play.guideOpen = false;
    state.play.toastTime = 0;
    state.app.navigation.enterGame();
    feedback.reset();
    enter(Lesson::Briefing);
    return true;
}
void TutorialDirector::exit() {
    if (!regularRun_)
        return;
    run = std::move(*regularRun_);
    state.play = std::move(*regularPlay_);
    state.app.navigation = regularNavigation_;
    state.app.navigation.home();
    regularRun_.reset();
    regularPlay_.reset();
    checkpoint_.reset();
    checkpointLesson_ = Lesson::None;
    sound.reset(run);
    ui.cancel();
    ui.focused = 0;
}
void TutorialDirector::enter(Lesson lesson) {
    state.play.tutorial.lesson = lesson;
    state.play.tutorial.mineCell = mineTarget();
    state.play.selectedCard = 0;
    state.play.selectedTower = 0;
    state.play.selectedUnit = 0;
    state.play.scan = false;
    if (run.state().phase == game::Phase::Prep)
        state.play.mine = true;
    state.play.toastTime = 0;
    ui.cancel();
    if (lesson != Lesson::Complete && lesson != Lesson::Failed) {
        checkpoint_ = run;
        checkpointLesson_ = lesson;
    }
    if (lesson == Lesson::Complete)
        state.app.tutorialCompleted = true;
}
void TutorialDirector::retry() {
    if (!active() || !checkpoint_)
        return;
    run = *checkpoint_;
    state.play = presentation::PlayState{};
    state.play.guideOpen = false;
    state.play.mine = run.state().phase == game::Phase::Prep;
    feedback.reset();
    enter(checkpointLesson_);
}
void TutorialDirector::next() {
    if (state.play.tutorial.lesson == Lesson::Briefing)
        enter(Lesson::Building);
}
int TutorialDirector::mineTarget() const {
    if (state.play.tutorial.lesson == Lesson::Scan || state.play.tutorial.lesson == Lesson::FirstDig)
        return 6;
    if (state.play.tutorial.lesson == Lesson::Crafting) {
        for (int cell : {7, 11, 12})
            if (!run.state().mine[cell].dug)
                return cell;
    }
    return state.play.tutorial.lesson == Lesson::Investment ? 13 : -1;
}
void TutorialDirector::observe() {
    if (!active())
        return;
    const game::RunState &live = run.state();
    const Lesson lesson = state.play.tutorial.lesson;
    if (live.phase == game::Phase::Lost && lesson != Lesson::Failed) {
        enter(Lesson::Failed);
        return;
    }
    if (lesson == Lesson::Building) {
        for (const game::Tower &tower : live.towers)
            if (tower.kind == game::CardKind::Gun && game::distance(tower.position, {6, 4}) < .1F) {
                enter(Lesson::Unit);
                return;
            }
        if (!live.towers.empty()) {
            retry();
            feedback.notify({false, "강조된 발판에 놓아 주세요. 이 단계를 복구했어요"});
        }
    } else if (lesson == Lesson::Unit) {
        for (const game::Unit &unit : live.units)
            if (unit.kind == game::CardKind::Guard && game::distance(unit.position, {8, 4}) < .1F) {
                enter(Lesson::Combat);
                return;
            }
        if (!live.units.empty()) {
            retry();
            feedback.notify({false, "강조된 길 위에 놓아 주세요. 이 단계를 복구했어요"});
        }
    } else if (lesson == Lesson::Combat && live.phase == game::Phase::Prep)
        enter(Lesson::Scan);
    else if (lesson == Lesson::Scan && live.scans == 0 && live.mine[6].info)
        enter(Lesson::FirstDig);
    else if (lesson == Lesson::FirstDig && !live.job.active && live.mine[6].dug && live.iron >= 2)
        enter(Lesson::Crafting);
    else if (lesson == Lesson::Crafting && live.tool == 2)
        enter(Lesson::Investment);
    else if (lesson == Lesson::Investment && !live.job.active && live.mine[13].dug && live.gold >= 4)
        enter(Lesson::Complete);
    state.play.tutorial.mineCell = mineTarget();
}
bool TutorialDirector::allowsSimulation() const {
    if (!active())
        return true;
    const Lesson lesson = state.play.tutorial.lesson;
    return lesson == Lesson::Combat ||
           ((lesson == Lesson::FirstDig || lesson == Lesson::Crafting || lesson == Lesson::Investment) &&
            run.state().job.active);
}
bool TutorialDirector::allows(const GameInput &input, const presentation::Layout &layout) const {
    using namespace presentation;
    if (!active())
        return true;
    const Lesson lesson = state.play.tutorial.lesson;
    if (input.kind == InputKind::Control) {
        switch (input.action) {
        case OpenSettings:
        case OpenGuide:
        case AudioMute:
        case AudioVolume:
        case GoHome:
        case Close:
        case ClearSelection:
        case NavigateBack:
            return true;
        case Resume:
            return true;
        case Pause:
        case Speed:
            return lesson == Lesson::Combat;
        case Scan:
            return lesson == Lesson::Scan;
        case Tool:
            return lesson == Lesson::Crafting;
        case World:
            if (lesson == Lesson::Scan || lesson == Lesson::FirstDig || lesson == Lesson::Crafting ||
                lesson == Lesson::Investment)
                return !run.state().job.active && layout.mineCell(input.position) == mineTarget() &&
                       (lesson != Lesson::Scan || state.play.scan);
            return lesson == Lesson::Building || lesson == Lesson::Unit || lesson == Lesson::Combat;
        default:
            return lesson == Lesson::Combat && input.action != Start && input.action != Reset;
        }
    }
    if (input.kind == InputKind::Hand) {
        const game::Card *card = run.card(input.payload);
        return card && (lesson == Lesson::Combat ||
                        (lesson == Lesson::Building && card->kind == game::CardKind::Gun) ||
                        (lesson == Lesson::Unit && card->kind == game::CardKind::Guard));
    }
    return false;
}
} // namespace thorium
