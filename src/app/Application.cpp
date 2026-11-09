#include "Application.h"
#include "game/presentation/Assets.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace thorium {
namespace {
/** Encode diagnostic text as JSON without allowing asset names to break the snapshot. */
std::string jsonText(const std::string &value) {
    std::string result = "\"";
    constexpr const char *digits = "0123456789abcdef";
    for (unsigned char byte : value) {
        if (byte == '\\' || byte == '"') {
            result += '\\';
            result += static_cast<char>(byte);
        } else if (byte < 32) {
            result += "\\u00";
            result += digits[byte >> 4];
            result += digits[byte & 15];
        } else
            result += static_cast<char>(byte);
    }
    return result + "\"";
}
/** Serialize the actual rendered base-card data for read-only UI regression diagnostics. */
std::string jsonCardDetails(const presentation::CardDetails &card) {
    std::ostringstream output;
    output << "{\"kind\":" << static_cast<int>(card.kind) << ",\"name\":" << jsonText(card.name)
           << ",\"cost\":" << card.cost << ",\"description\":" << jsonText(card.description)
           << ",\"stats\":[";
    for (std::size_t index = 0; index < card.stats.size(); ++index) {
        if (index)
            output << ',';
        output << "{\"label\":" << jsonText(card.stats[index].label)
               << ",\"value\":" << jsonText(card.stats[index].value) << '}';
    }
    return output.str() + "]}";
}
} // namespace
Application::Application(game::Run initialRun)
    : run(std::move(initialRun)), graphics(presentation::assets::renderAssets()), sound(audio),
      feedback(run, state, sound), flow(run, state, ui, audio, sound, feedback),
      gameplay(run, state, ui, view.layout, sound, feedback), tutorial(run, state, ui, feedback, sound) {
    state.app.animationSets = presentation::animationSets(
        graphics.animationAssets(), presentation::assets::animationBindings(), run.catalog());
    if (graphics.error.empty() && !state.app.animationSets.error.empty())
        graphics.error = state.app.animationSets.error;
    feedback.reset();

    SaveData saved = Persistence::load();
    state.app.tutorialCompleted = saved.tutorialCompleted;
    state.app.highestUnlockedStage = std::max(state.app.highestUnlockedStage, saved.highestUnlockedStage);
    state.app.preferences.reducedMotion = saved.reducedMotion;
    state.app.preferences.damageNumbers = saved.damageNumbers;
    state.app.preferences.cachedAnimations = saved.cachedAnimations;
    audio.settings.volume = saved.volume;
    audio.settings.muted = saved.muted;
}
bool Application::frame(int width, int height, float density, float elapsed) {
    using engine::FrameClock;
    using engine::FrameStage;
    frameTimings_ = {};
    FrameClock::time_point stage = FrameClock::now();
    elapsed = std::isfinite(elapsed) ? std::clamp(elapsed, 0.0F, 0.1F) : 0.0F;
    state.play.toastTime = std::max(0.0F, state.play.toastTime - elapsed);
    const game::Phase before = run.state().phase;
    const bool stopAudio = !flow.allowsSimulation() || !tutorial.allowsSimulation();
    if (!state.app.preferences.reducedMotion)
        state.app.uiTime += elapsed;
    if (stopAudio && !audioPaused_) {
        audio.stop();
        sound.cancelPending();
    }
    audioPaused_ = stopAudio;
    sound.advance(elapsed);
    state.play.feedback.advance(elapsed, !stopAudio);
    frameTimings_.add(FrameStage::Presentation, engine::frameMilliseconds(stage));
    if (!stopAudio) {
        state.play.visualTime += elapsed;
        accumulator_ += elapsed * state.play.speed;
        while (accumulator_ >= 1.0F / 60.0F) {
            stage = FrameClock::now();
            state.play.actors.advance(1.0F / 60.0F);
            frameTimings_.add(FrameStage::Presentation, engine::frameMilliseconds(stage));
            stage = FrameClock::now();
            run.advance(1.0F / 60.0F);
            frameTimings_.add(FrameStage::Simulation, engine::frameMilliseconds(stage));
            ++frameTimings_.simulationSteps;
            stage = FrameClock::now();
            feedback.collect();
            frameTimings_.add(FrameStage::Presentation, engine::frameMilliseconds(stage));
            accumulator_ -= 1.0F / 60.0F;
        }
    } else
        accumulator_ = 0;
    stage = FrameClock::now();
    feedback.collect();
    gameplay.synchronize(before);
    tutorial.observe();
    frameTimings_.add(FrameStage::Presentation, engine::frameMilliseconds(stage));
    if (state.app.preferences.cachedAnimations && !animationCachePrepared_) {
        stage = FrameClock::now();
        for (const std::pair<const std::string, presentation::ActorAnimationSet> &entry :
             state.app.animationSets.profiles) {
            graphics.prewarmAnimation(entry.second.idle);
            graphics.prewarmAnimation(entry.second.move);
            graphics.prewarmAnimation(entry.second.attack);
            graphics.prewarmAnimation(entry.second.death);
        }
        animationCachePrepared_ = true;
        frameTimings_.add(FrameStage::CachePreparation, engine::frameMilliseconds(stage));
    }
    if (!state.play.actors.error().empty()) {
        graphics.error = state.play.actors.error();
        return false;
    }
    graphics.setAnimationBackend(state.app.preferences.cachedAnimations
                                     ? engine::AnimationBackend::SharedFrames
                                     : engine::AnimationBackend::Vector);
    stage = FrameClock::now();
    if (!graphics.begin(width, height, density))
        return false;
    frameTimings_.add(FrameStage::Raster, engine::frameMilliseconds(stage));
    stage = FrameClock::now();
    ui.advance(elapsed);
    view.draw(graphics, ui, run, state, audio.settings);
    frameTimings_.add(FrameStage::Scene, engine::frameMilliseconds(stage));
    stage = FrameClock::now();
    const bool rendered = graphics.finish();
    frameTimings_.add(FrameStage::Raster, engine::frameMilliseconds(stage));
    return rendered;
}
void Application::pointer(engine::Pointer event) {
    const std::optional<engine::Interaction> interaction = ui.pointer(event);
    if (interaction)
        act(*interaction);
}
void Application::applyFlow(FlowResult result) {
    if (result == FlowResult::Restarted) {
        accumulator_ = 0;
        audioPaused_ = false;
    }
}
void Application::act(engine::Interaction interaction) {
    const std::optional<GameInput> input = decodeInput(interaction);
    if (!input)
        return;
    if (input->kind == InputKind::Control) {
        using namespace presentation;
        if (input->action == BeginTutorial) {
            if (tutorial.active() || state.app.navigation.current() == Screen::Home ||
                state.app.navigation.current() == Screen::Guide) {
                if (tutorial.begin())
                    applyFlow(FlowResult::Restarted);
            }
            return;
        }
        if (tutorial.active()) {
            if (input->action == TutorialExit || input->action == GoHome) {
                tutorial.exit();
                applyFlow(FlowResult::Restarted);
                return;
            }
            if (input->action == TutorialRegular) {
                if (state.play.tutorial.lesson != Lesson::Complete)
                    return;
                tutorial.exit();
                applyFlow(flow.act(HomeNew));
                applyFlow(FlowResult::Restarted);
                return;
            }
            if (input->action == TutorialRetry || input->action == Reset || input->action == ConfirmReset) {
                tutorial.retry();
                applyFlow(FlowResult::Restarted);
                return;
            }
            if (input->action == TutorialNext) {
                tutorial.next();
                return;
            }
        } else if (input->action >= TutorialNext && input->action <= TutorialRegular)
            return;
    }
    if (state.app.navigation.gameplay() && !tutorial.allows(*input, view.layout)) {
        feedback.notify({false, "강조된 카드·칸부터 진행해 주세요. 다시하기로 이 단계를 복구할 수 있어요"});
        return;
    }
    const FlowResult result =
        input->kind == InputKind::Control ? flow.act(input->action) : FlowResult::Unhandled;
    applyFlow(result);
    if (result == FlowResult::Unhandled)
        gameplay.act(*input);
    tutorial.observe();
}
void Application::key(int key, bool shift) {
    if (key == 9) {
        ui.nextFocus(shift);
        return;
    }
    if (key == 13) {
        const std::optional<engine::Interaction> interaction = ui.activate();
        if (interaction)
            act(*interaction);
        return;
    }
    if (tutorial.active() && state.app.navigation.gameplay()) {
        using namespace presentation;
        if (key >= '1' && key <= '4') {
            act({engine::Interaction::Kind::Click,
                 CardBase + run.state().deck[run.state().hand[key - '1']].id,
                 {}});
            return;
        }
        if (key == ' ') {
            if (state.play.paused)
                act({engine::Interaction::Kind::Click, Resume, {}});
            else if (state.play.tutorial.lesson == Lesson::Briefing)
                act({engine::Interaction::Kind::Click, TutorialNext, {}});
            else if (state.play.tutorial.lesson == Lesson::Combat)
                act({engine::Interaction::Kind::Click, state.play.paused ? Resume : Pause, {}});
            return;
        }
        if (key == 'm')
            return;
        if (key == 27 && state.play.tutorial.lesson != Lesson::Combat && !state.play.selectedCard &&
            !state.play.scan && state.play.modal == Modal::None) {
            act({engine::Interaction::Kind::Click, TutorialExit, {}});
            return;
        }
    }
    const FlowResult result = flow.key(key);
    applyFlow(result);
    if (result == FlowResult::Unhandled)
        gameplay.key(key);
}
void Application::savePersistence() {
    SaveData data;
    data.tutorialCompleted = state.app.tutorialCompleted;
    data.highestUnlockedStage = state.app.highestUnlockedStage;
    data.reducedMotion = state.app.preferences.reducedMotion;
    data.damageNumbers = state.app.preferences.damageNumbers;
    data.cachedAnimations = state.app.preferences.cachedAnimations;
    data.volume = audio.settings.volume;
    data.muted = audio.settings.muted;
    Persistence::save(data);
}
void Application::suspend() {
    ui.cancel();
    audio.stop();
    sound.cancelPending();
    accumulator_ = 0;
    savePersistence();
    if (run.state().phase == game::Phase::Battle || run.state().job.active)
        state.play.paused = true;
}
std::string Application::debugState() const {
    std::string result = run.debugState();
    result.pop_back();
    result += ",\"paused\":" + std::string(state.play.paused ? "true" : "false") +
              ",\"screen\":" + std::to_string(static_cast<int>(state.app.navigation.current())) +
              ",\"hasRun\":" + (state.app.navigation.hasRun() ? "true" : "false") +
              ",\"reducedMotion\":" + (state.app.preferences.reducedMotion ? "true" : "false") +
              ",\"damageNumbers\":" + (state.app.preferences.damageNumbers ? "true" : "false") +
              ",\"settingsTab\":" + std::to_string(state.app.settingsTab) +
              ",\"guidePage\":" + std::to_string(state.app.guidePage) +
              ",\"mineView\":" + (state.play.mine ? "true" : "false") +
              ",\"guideOpen\":" + (state.play.guideOpen ? "true" : "false") +
              ",\"tutorial\":{\"active\":" + (tutorial.active() ? "true" : "false") +
              ",\"lesson\":" + std::to_string(static_cast<int>(state.play.tutorial.lesson)) +
              ",\"mineCell\":" + std::to_string(state.play.tutorial.mineCell) +
              ",\"completed\":" + (state.app.tutorialCompleted ? "true" : "false") + "}" +
              ",\"selectedCard\":" + std::to_string(state.play.selectedCard) +
              ",\"selectedUnit\":" + std::to_string(state.play.selectedUnit) +
              ",\"selectedTower\":" + std::to_string(state.play.selectedTower) +
              ",\"modal\":" + std::to_string(static_cast<int>(state.play.modal)) +
              ",\"width\":" + std::to_string(graphics.width) +
              ",\"height\":" + std::to_string(graphics.height);
    std::ostringstream feedback;
    feedback << ",\"placement\":";
    const std::optional<presentation::PlacementPreview> &placement = view.currentPlacement();
    if (placement) {
        feedback << "{\"card\":" << placement->card << ",\"x\":" << placement->target.x
                 << ",\"y\":" << placement->target.y
                 << ",\"allowed\":" << (placement->rule.allowed() ? "true" : "false")
                 << ",\"cost\":" << placement->rule.cost
                 << ",\"action\":" << static_cast<int>(placement->rule.action)
                 << ",\"range\":" << placement->range
                 << ",\"reason\":" << static_cast<int>(placement->rule.failure)
                 << ",\"message\":" << jsonText(placement->rule.message()) << '}';
    } else
        feedback << "null";
    feedback << ",\"cardInfo\":";
    const std::optional<presentation::CardInspection> &info = view.currentCardInspection();
    if (info)
        feedback << "{\"widget\":" << info->widget << ",\"details\":" << jsonCardDetails(info->details)
                 << ",\"x\":" << info->bounds.x << ",\"y\":" << info->bounds.y << ",\"w\":" << info->bounds.w
                 << ",\"h\":" << info->bounds.h << '}';
    else
        feedback << "null";
    feedback << ",\"handCharge\":[";
    bool firstCharge = true;
    for (const presentation::CardCharge &charge : view.currentHandCharge()) {
        if (!charge.card)
            continue;
        if (!firstCharge)
            feedback << ',';
        firstCharge = false;
        feedback << "{\"card\":" << charge.card << ",\"cost\":" << charge.cost
                 << ",\"mode\":" << static_cast<int>(charge.mode) << ",\"fraction\":" << charge.fraction
                 << '}';
    }
    feedback << ']';
    feedback << ",\"offeredCard\":";
    if (view.currentOfferCard())
        feedback << jsonCardDetails(*view.currentOfferCard());
    else
        feedback << "null";
    feedback << ",\"energyBar\":";
    if (state.app.navigation.gameplay() && run.state().phase != game::Phase::Prep) {
        const engine::Rect &bar = view.layout.energyBar;
        feedback << "{\"x\":" << bar.x << ",\"y\":" << bar.y << ",\"w\":" << bar.w << ",\"h\":" << bar.h
                 << '}';
    } else
        feedback << "null";
    feedback << ",\"handAvailability\":[";
    for (int index = 0; index < 4; ++index) {
        if (index)
            feedback << ',';
        const game::Card &card = run.state().deck[run.state().hand[index]];
        const game::PlayAvailability availability = run.playAvailability(card.id);
        feedback << "{\"card\":" << card.id << ",\"allowed\":" << (availability.allowed ? "true" : "false")
                 << ",\"minimumCost\":" << availability.minimumCost
                 << ",\"maximumCost\":" << availability.maximumCost
                 << ",\"reason\":" << static_cast<int>(availability.failure) << '}';
    }
    feedback << ']' << ",\"feedbackChanges\":" << state.play.feedback.handChanges
             << ",\"feedbackHits\":" << state.play.feedback.hits
             << ",\"rejectedCard\":" << state.play.feedback.rejectedCard
             << ",\"rejectionAge\":" << state.play.feedback.rejectionAge << ",\"cardMotion\":[";
    for (int index = 0; index < 4; ++index) {
        if (index)
            feedback << ',';
        const presentation::CardTransition &card = state.play.feedback.cards[index];
        feedback << "{\"old\":" << card.outgoing.id << ",\"next\":" << card.incoming.id
                 << ",\"oldKind\":" << static_cast<int>(card.outgoing.kind)
                 << ",\"nextKind\":" << static_cast<int>(card.incoming.kind) << ",\"age\":" << card.age
                 << ",\"busy\":" << (state.play.feedback.cardBusy(index) ? "true" : "false") << '}';
    }
    feedback << "],\"combatCues\":[";
    for (std::size_t index = 0; index < state.play.feedback.cues.size(); ++index) {
        if (index)
            feedback << ',';
        const presentation::CombatCue &cue = state.play.feedback.cues[index];
        feedback << "{\"type\":" << static_cast<int>(cue.event.type) << ",\"subject\":" << cue.event.subject
                 << ",\"age\":" << cue.age << '}';
    }
    const engine::AnimationRenderStats render = graphics.animationStats();
    feedback << "],\"rendering\":{\"error\":" << jsonText(graphics.error)
             << ",\"cached\":" << (state.app.preferences.cachedAnimations ? "true" : "false")
             << ",\"bytes\":" << render.cache.bytes << ",\"entries\":" << render.cache.entries
             << ",\"pinned\":" << render.cache.pinnedEntries << ",\"hits\":" << render.cache.hits
             << ",\"misses\":" << render.cache.misses << ",\"bakes\":" << render.cache.bakes
             << ",\"evictions\":" << render.cache.evictions << ",\"rejected\":" << render.cache.rejected
             << ",\"bakeMs\":" << render.cache.bakeMilliseconds << ",\"bakerSources\":" << render.bakerSources
             << ",\"sprites\":" << render.spriteInstances << ",\"vectors\":" << render.vectorInstances
             << ",\"cachedDraws\":" << render.cachedDraws << ",\"vectorDraws\":" << render.vectorDraws
             << ",\"poolHits\":" << render.poolHits << ",\"pooledVectors\":" << render.pooledVectors
             << ",\"fallbacks\":" << render.fallbacks << "},\"actors\":" << state.play.actors.debugState()
             << ",\"deathVisuals\":" << state.play.actors.deaths()
             << ",\"audio\":{\"muted\":" << (audio.settings.muted ? "true" : "false")
             << ",\"volume\":" << audio.settings.volume << ",\"voices\":" << audio.activeVoices()
             << ",\"played\":" << audio.played << ",\"suppressed\":" << audio.suppressed
             << ",\"beeps\":" << sound.beepClips << ",\"mp3\":" << sound.mp3Clips
             << ",\"assetErrors\":" << sound.assetErrors.size() << "}}";
    return result + feedback.str();
}
std::string Application::debugUi() const {
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < ui.widgets.size(); ++index) {
        if (index)
            output << ',';
        const engine::Widget &item = ui.widgets[index];
        output << "{\"id\":" << item.id << ",\"x\":" << item.bounds.x << ",\"y\":" << item.bounds.y
               << ",\"w\":" << item.bounds.w << ",\"h\":" << item.bounds.h
               << ",\"enabled\":" << (item.enabled ? "true" : "false")
               << ",\"focusable\":" << (item.focusable ? "true" : "false")
               << ",\"inspectable\":" << (item.inspectable ? "true" : "false") << '}';
    }
    output << ']';
    return output.str();
}
} // namespace thorium
