#pragma once
#include "ActorAnimator.h"
#include "Feedback.h"
#include "Shell.h"
#include "TutorialGuide.h"
#include "engine/view/Camera2D.h"
#include <optional>
#include <string>

namespace thorium::presentation {
/** Modal identity is explicit, while numeric diagnostics remain stable. */
enum class Modal { None, Recruit, Replacement, Withdrawal, NewRun, MineDescent };
/** App-session preferences and navigation survive replacing the current run. */
struct AppState {
    bool campaign = false;
    bool tutorialCompleted = false;
    int selectedStage = 1;
    int highestUnlockedStage = 1;
    AnimationSets animationSets;
    std::uint32_t animationSeed = 0x54484f52U;
    /** Counts expeditions started in this session; mixed into each one's excavation wall seed. */
    std::uint32_t expeditions = 0;
    Navigation navigation;
    Preferences preferences;
    int settingsTab = 0;
    int guidePage = 0;
    float uiTime = 0;
};
/** Contextual one-time in-game tips for regular expedition situations. */
struct InGameHints {
    bool towerUpgrade = false;
    bool nestExcavated = false;
    bool scanMinesweeper = false;
    bool woundedUnit = false;
    bool firstGold = false;
    /** Shown once the wallet first covers clearing a boulder. */
    bool boulder = false;
};
/** Disposable run interaction and animation state; contains no gameplay authority. */
struct PlayState {
    engine::CameraPose camera;
    bool cameraFramed = false;
    TutorialGuide tutorial;
    Feedback feedback;
    ActorAnimations actors;
    InGameHints hints;
    int currentStage = 1;
    bool mine = false;
    /** Vertical cutaway scroll in tiles; the view clamps it to the active board. */
    float mineScroll = 0.0F;
    bool scan = false;
    /** Excavation strikes use the blast instead of the pick. */
    bool blast = false;
    bool paused = false;
    int speed = 1;
    game::CardId selectedCard = 0;
    game::EntityId selectedUnit = 0;
    game::EntityId selectedTower = 0;
    /** Boulder whose details and clear button the inspector shows. */
    std::optional<game::Point> selectedBoulder;
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
