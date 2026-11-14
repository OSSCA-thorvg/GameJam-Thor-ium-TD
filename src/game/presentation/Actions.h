#pragma once
#include "engine/ui/Ui.h"

namespace thorium::presentation {
/** Stable generic widget IDs interpreted outside the UI engine. */
enum Action : engine::WidgetId {
    World = 1,
    Start = 10,
    Pause,
    Speed,
    Surface,
    Mine,
    Scan,
    Exchange,
    Tool,
    Armor,
    Slots,
    Recruit,
    Coil,
    Heal,
    Dismiss,
    Reset,
    Close,
    ExpandLong,
    ExpandShort,
    RewardSkip,
    ConfirmDismiss,
    ConfirmReset,
    Resume,
    MoveUnit,
    ClearSelection,
    AudioMute,
    AudioVolume,
    HomeNew,
    HomeContinue,
    OpenSettings,
    OpenGuide,
    OpenCredits,
    NavigateBack,
    GoHome,
    ToggleMotion,
    ToggleDamage,
    AudioDown,
    AudioUp,
    AudioTest,
    SettingsDefaults,
    SettingsAudio,
    SettingsVisual,
    GuidePrevious,
    GuideNext,
    ToggleAnimationBackend,
    ToggleDrawer,
    CloseDrawer,
    AcknowledgeDrawer,
    BeginTutorial,
    TutorialNext,
    TutorialRetry,
    TutorialExit,
    TutorialRegular,
    SelectStagePrev,
    SelectStageNext,
    NextStageRun,
    ZoomOut,
    ZoomFit,
    ZoomIn,
    DescendMine,
    ConfirmDescend,
    /** Preparation row that selects an awakening candidate on the battlefield. */
    AwakenPick,
    /** Inspector command that awakens the selected building. */
    Awaken,
    Hire,
    Supply,
    /** Excavation tool toggles: the pick is the default and the blast stays selected until changed. */
    ToolPick,
    ToolBlast,
    /** Settings tab for expedition rules, and its switch between excavation and tile-by-tile mining. */
    SettingsGame,
    ToggleMining,
    /** Inspector command that clears the selected boulder. */
    ClearBoulder,
    /** Fold or unfold the battlefield introduction cards. */
    ToggleIntro,
    /** Fold or unfold the overview inset on the battlefield. */
    ToggleMinimap,
    /** Inspector command that repairs the selected building. */
    Repair,
    /** Settings steps of the music level. */
    MusicDown,
    MusicUp,
    /** Recruitment payment in gold or in battle shards. */
    PayGold,
    PayShards,
    /** Last control of the contiguous range decoded as a plain command. */
    LastControl = PayShards,
    GuideBackdrop = 901,
    CardBase = 1000,
    DeckBase = 2000,
    CandidateBase = 3000,
    LaneFocusBase = 4000,
    MinimapFocus = 4100
};
} // namespace thorium::presentation
