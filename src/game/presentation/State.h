#pragma once
#include "ActorAnimator.h"
#include "Feedback.h"
#include "Shell.h"
#include "TutorialGuide.h"
#include <string>

namespace thorium::presentation {
/** Modal identity is explicit, while numeric diagnostics remain stable. */
enum class Modal { None, Recruit, Replacement, Withdrawal, NewRun };
/** App-session preferences and navigation survive replacing the current run. */
struct AppState {
    bool tutorialCompleted = false;
    AnimationSets animationSets;
    std::uint32_t animationSeed = 0x54484f52U;
    Navigation navigation;
    Preferences preferences;
    int settingsTab = 0;
    int guidePage = 0;
    float uiTime = 0;
};
/** Disposable run interaction and animation state; contains no gameplay authority. */
struct PlayState {
    TutorialGuide tutorial;
    Feedback feedback;
    ActorAnimations actors;
    bool mine = false;
    bool scan = false;
    bool paused = false;
    int speed = 1;
    game::CardId selectedCard = 0;
    game::EntityId selectedUnit = 0;
    game::EntityId selectedTower = 0;
    bool movingUnit = false;
    Modal modal = Modal::None;
    game::OfferId selectedOffer = 0;
    game::CardKind candidate = game::CardKind::Guard;
    std::string toast = "카드를 끌어다 전장에 배치하세요";
    float toastTime = 6.0F;
    bool error = false;
    float visualTime = 0.0F;
    bool guideOpen = true;
};
/** Compose state with different lifetimes without copying preferences during reset. */
struct State {
    AppState app;
    PlayState play;
};
} // namespace thorium::presentation
