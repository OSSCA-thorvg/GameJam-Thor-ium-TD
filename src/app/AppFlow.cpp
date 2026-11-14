#include "AppFlow.h"
#include "game/presentation/CommandPolicy.h"
#include "game/presentation/Text.h"
#include <algorithm>
#include <cmath>
namespace thorium {
game::Catalog expeditionContent(int stage, presentation::AppState &app) {
    game::Catalog content = game::stageCatalog(stage);
    content.mining = app.preferences.excavation ? game::MiningMode::Excavate : game::MiningMode::Sweep;
    content.mineSeed =
        (app.animationSeed ^ static_cast<std::uint32_t>(stage)) + ++app.expeditions * 2654435761U;
    return content;
}
AppFlow::AppFlow(game::Run &run, presentation::State &state, engine::Ui &ui, engine::AudioMixer &audio,
                 presentation::Soundscape &sound, FeedbackChannel &feedback)
    : run(run), state(state), ui(ui), audio(audio), sound(sound), feedback(feedback) {}
bool AppFlow::restart(int stageIndex) {
    const int stage = stageIndex ? stageIndex : state.app.selectedStage;
    if (state.app.campaign) {
        game::RunCreation created = game::Run::create(expeditionContent(stage, state.app));
        if (!created.run) {
            feedback.notify({false, game::Message("app.content.invalid", {{"detail", created.validation.summary()}})});
            return false;
        }
        run = std::move(*created.run);
        state.app.selectedStage = stage;
    } else {
        run.reset();
    }
    ui.cancel();
    ui.focused = 0;
    state.play = presentation::PlayState{};
    state.play.currentStage = stage;
    state.app.navigation.enterGame();
    feedback.reset();
    return true;
}
bool AppFlow::synchronizeProgress() {
    if (!state.app.campaign || state.play.tutorial.active() || run.state().phase != game::Phase::Won)
        return false;
    const int unlocked = std::min(game::TotalStages, state.play.currentStage + 1);
    if (unlocked <= state.app.highestUnlockedStage)
        return false;
    state.app.highestUnlockedStage = unlocked;
    return true;
}
FlowResult AppFlow::act(presentation::Action action) {
    using namespace presentation;
    if (action >= LanguageBase && action < LanguageBase + static_cast<int>(languages().size())) {
        std::string error;
        setLanguage(languages()[static_cast<std::size_t>(action - LanguageBase)].code, error);
        state.app.preferences.language = language();
        sound.play(Sound::Select);
        return FlowResult::Handled;
    }
    switch (action) {
    case HomeNew:
        if (state.app.navigation.current() != Screen::Home)
            return FlowResult::Handled;
        if (state.app.navigation.hasRun())
            state.play.modal = presentation::Modal::NewRun;
        else {
            if (!restart())
                return FlowResult::Handled;
            sound.play(Sound::Confirm);
            return FlowResult::Restarted;
        }
        ui.cancel();
        ui.focused = 0;
        break;
    case HomeContinue:
        if (state.app.navigation.current() != Screen::Home || !state.app.navigation.hasRun())
            return FlowResult::Handled;
        state.app.navigation.enterGame();
        state.play.paused = false;
        state.play.modal = presentation::Modal::None;
        state.play.selectedOffer = 0;
        ui.cancel();
        ui.focused = 0;
        sound.play(Sound::Select);
        break;
    case OpenSettings:
    case OpenGuide:
    case OpenCredits:
        state.app.navigation.open(action == OpenSettings ? Screen::Settings
                                  : action == OpenGuide  ? Screen::Guide
                                                         : Screen::Credits);
        audio.stop();
        sound.cancelPending();
        ui.cancel();
        ui.focused = 0;
        sound.play(Sound::Select);
        break;
    case NavigateBack:
        state.app.navigation.back();
        ui.cancel();
        ui.focused = 0;
        sound.play(Sound::Select);
        break;
    case GoHome:
        state.app.navigation.home(run.state().phase != game::Phase::Won &&
                                  run.state().phase != game::Phase::Lost);
        state.play.modal = presentation::Modal::None;
        state.play.selectedOffer = 0;
        audio.stop();
        sound.cancelPending();
        ui.cancel();
        ui.focused = 0;
        break;
    case ToggleMotion:
        state.app.preferences.reducedMotion = !state.app.preferences.reducedMotion;
        if (state.app.preferences.reducedMotion)
            state.play.feedback.settleCards();
        sound.play(Sound::Select);
        break;
    case ToggleAnimationBackend:
        state.app.preferences.cachedAnimations = !state.app.preferences.cachedAnimations;
        sound.play(Sound::Select);
        break;
    case ToggleDamage:
        state.app.preferences.damageNumbers = !state.app.preferences.damageNumbers;
        sound.play(Sound::Select);
        break;
    case AudioDown:
    case AudioUp:
        audio.settings.volume = std::clamp(
            (std::round(audio.settings.volume * 20) + (action == AudioUp ? 1 : -1)) / 20, 0.0F, 1.0F);
        break;
    case MusicDown:
    case MusicUp:
        audio.settings.music = std::clamp(
            (std::round(audio.settings.music * 10) + (action == MusicUp ? 1 : -1)) / 10, 0.0F, 1.0F);
        break;
    case AudioTest:
        sound.play(Sound::Reward);
        break;
    case SettingsDefaults:
        audio.settings = engine::AudioSettings{};
        state.app.preferences = Preferences{};
        state.app.preferences.language = presentation::language();
        audio.stop();
        sound.play(Sound::Confirm);
        break;
    case SettingsAudio:
    case SettingsVisual:
    case SettingsGame:
    case SettingsLanguage:
        state.app.settingsTab = action == SettingsAudio    ? 0
                                : action == SettingsVisual ? 1
                                : action == SettingsGame   ? 2
                                                           : 3;
        ui.cancel();
        sound.play(Sound::Select);
        break;
    case ToggleMining:
        state.app.preferences.excavation = !state.app.preferences.excavation;
        sound.play(Sound::Select);
        break;
    case ToggleLanguage: {
        std::string error;
        presentation::setLanguage(presentation::nextLanguage(), error);
        state.app.preferences.language = presentation::language();
        sound.play(Sound::Select);
        break;
    }
    case GuidePrevious:
    case GuideNext:
        state.app.guidePage = std::clamp(state.app.guidePage + (action == GuideNext ? 1 : -1), 0, 2);
        ui.cancel();
        sound.play(Sound::Select);
        break;
    case Reset:
        if (!state.app.navigation.gameplay())
            return FlowResult::Handled;
        state.play.modal = presentation::Modal::NewRun;
        ui.cancel();
        break;
    case ConfirmReset:
        if (!canConfirmNewRun(run.state().phase, state))
            return FlowResult::Handled;
        return restart() ? FlowResult::Restarted : FlowResult::Handled;
    case AudioMute:
        audio.settings.muted = !audio.settings.muted;
        audio.stop();
        sound.cancelPending();
        if (!audio.settings.muted)
            sound.play(Sound::Select);
        break;
    case AudioVolume:
        audio.settings.volume = audio.settings.volume >= .99F
                                    ? .25F
                                    : std::min(1.0F, (std::floor(audio.settings.volume * 4) + 1) / 4);
        audio.settings.muted = false;
        sound.play(Sound::Select);
        break;
    case SelectStagePrev:
    case SelectStageNext: {
        if (!state.app.campaign || state.app.navigation.current() != Screen::Home ||
            state.play.modal != Modal::None)
            return FlowResult::Handled;
        const int next = state.app.selectedStage + (action == SelectStageNext ? 1 : -1);
        if (next >= 1 && next <= std::min(game::TotalStages, state.app.highestUnlockedStage)) {
            state.app.selectedStage = next;
            sound.play(Sound::Select);
        }
        ui.cancel();
        break;
    }
    case NextStageRun:
        if (state.app.campaign && state.app.navigation.gameplay() && run.state().phase == game::Phase::Won &&
            !state.play.tutorial.active() && state.play.currentStage < game::TotalStages) {
            synchronizeProgress();
            if (!restart(state.play.currentStage + 1))
                return FlowResult::Handled;
            sound.play(Sound::Confirm);
            return FlowResult::Restarted;
        }
        break;
    case Close:
        state.play.modal = presentation::Modal::None;
        state.play.selectedOffer = 0;
        ui.cancel();
        break;
    default:
        return FlowResult::Unhandled;
    }
    return FlowResult::Handled;
}
bool AppFlow::allowsSimulation() const {
    return state.app.navigation.gameplay() && !state.play.paused &&
           state.play.modal != presentation::Modal::Withdrawal &&
           state.play.modal != presentation::Modal::NewRun &&
           state.play.modal != presentation::Modal::MineDescent;
}
FlowResult AppFlow::key(int key) {
    using namespace presentation;
    if (key == 's' || key == 'v')
        return act(key == 's' ? AudioMute : AudioVolume);
    if (key == 27 && state.play.modal != Modal::None)
        return act(Close);
    if (!state.app.navigation.gameplay()) {
        if (key == 27 && state.app.navigation.current() != Screen::Home)
            return act(NavigateBack);
        if (key == ' ' && state.app.navigation.current() == Screen::Home && state.play.modal == Modal::None)
            return act(state.app.navigation.hasRun() ? HomeContinue : HomeNew);
        return FlowResult::Handled;
    }
    return FlowResult::Unhandled;
}
} // namespace thorium
