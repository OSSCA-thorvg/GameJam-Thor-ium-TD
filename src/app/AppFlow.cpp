#include "AppFlow.h"
#include "game/presentation/CommandPolicy.h"
#include <algorithm>
#include <cmath>
namespace thorium {
AppFlow::AppFlow(game::Run &run, presentation::State &state, engine::Ui &ui, engine::AudioMixer &audio,
                 presentation::Soundscape &sound, FeedbackChannel &feedback)
    : run(run), state(state), ui(ui), audio(audio), sound(sound), feedback(feedback) {}
void AppFlow::restart() {
    ui.cancel();
    ui.focused = 0;
    run.reset();
    state.play = presentation::PlayState{};
    state.app.navigation.enterGame();
    feedback.reset();
}
FlowResult AppFlow::act(presentation::Action action) {
    using namespace presentation;
    switch (action) {
    case HomeNew:
        if (state.app.navigation.current() != Screen::Home)
            return FlowResult::Handled;
        if (state.app.navigation.hasRun())
            state.play.modal = presentation::Modal::NewRun;
        else {
            restart();
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
    case AudioTest:
        sound.play(Sound::Reward);
        break;
    case SettingsDefaults:
        audio.settings = engine::AudioSettings{};
        state.app.preferences = Preferences{};
        audio.stop();
        sound.play(Sound::Confirm);
        break;
    case SettingsAudio:
    case SettingsVisual:
        state.app.settingsTab = action == SettingsAudio ? 0 : 1;
        ui.cancel();
        sound.play(Sound::Select);
        break;
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
        restart();
        return FlowResult::Restarted;
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
           state.play.modal != presentation::Modal::NewRun;
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
