#include "TutorialDirector.h"
#include "game/content/TutorialCatalog.h"
#include <algorithm>
#include <cmath>
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
    completeHold_ = -1;
    sound.reset(run);
    ui.cancel();
    ui.focused = 0;
}
void TutorialDirector::enter(Lesson lesson) {
    completeHold_ = -1;
    state.play.tutorial.lesson = lesson;
    state.play.tutorial.mineCell = mineTarget();
    state.play.selectedCard = 0;
    state.play.selectedTower = 0;
    state.play.selectedUnit = 0;
    state.play.scan = false;
    if (run.state().phase == game::Phase::Prep)
        state.play.mine = lesson != Lesson::Fork;
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
    const Lesson lesson = state.play.tutorial.lesson;
    if (lesson == Lesson::Scan)
        return 6;
    // Strike order of the authored practice wall: iron, then copper, then the gold under thick rock.
    const auto next = [this](std::initializer_list<int> cells) {
        for (int cell : cells)
            if (run.state().mine[static_cast<std::size_t>(cell)].cover > 0)
                return cell;
        return -1;
    };
    if (lesson == Lesson::FirstDig)
        return next({0, 5});
    if (lesson == Lesson::Crafting)
        return next({3});
    return lesson == Lesson::Investment && !completing() ? next({8, 7}) : -1;
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
        enter(Lesson::Fork);
    else if (lesson == Lesson::Fork && run.detoured(0))
        enter(Lesson::Scan);
    else if (lesson == Lesson::Scan && live.scans == 0 && live.mine[6].info)
        enter(Lesson::FirstDig);
    else if (lesson == Lesson::FirstDig && live.iron >= 2)
        enter(Lesson::Crafting);
    else if (lesson == Lesson::Crafting && live.tool == 2)
        enter(Lesson::Investment);
    else if (lesson == Lesson::Investment && !completing() && live.gold >= 4) {
        completeHold_ = CompleteHold;
        state.app.tutorialCompleted = true;
    }
    state.play.tutorial.mineCell = mineTarget();
}
void TutorialDirector::advance(float seconds) {
    if (!active() || !completing() || !std::isfinite(seconds) || seconds <= 0)
        return;
    completeHold_ -= std::min(seconds, .1F);
    if (completeHold_ <= 0)
        enter(Lesson::Complete);
}
bool TutorialDirector::allowsSimulation() const {
    if (!active() || completing())
        return true;
    const Lesson lesson = state.play.tutorial.lesson;
    return lesson == Lesson::Combat || lesson == Lesson::FirstDig || lesson == Lesson::Crafting ||
           lesson == Lesson::Investment;
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
        case ToggleIntro:
        case ToggleMinimap:
        case AudioMute:
        case AudioVolume:
        case ToggleLanguage:
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
            if (completing())
                return false;
            if (lesson == Lesson::Fork)
                return true;
            if (lesson == Lesson::Scan || lesson == Lesson::FirstDig || lesson == Lesson::Crafting ||
                lesson == Lesson::Investment)
                return layout.mineCell(input.position) == mineTarget() &&
                       (lesson == Lesson::Scan) == state.play.scan;
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
