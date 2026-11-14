#include "Application.h"
#include "game/autoplay/Autoplayer.h"
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
Application::Application() : Application(game::Run{}) {
    game::RunCreation initial = game::Run::create(expeditionContent(1, state.app));
    if (!initial.run) {
        graphics.error = initial.validation.summary();
        return;
    }
    run = std::move(*initial.run);
    state.app.campaign = true;
    feedback.reset();
    restoreRun();
}
void Application::restoreRun() {
    const std::string text = Persistence::loadRun();
    if (text.empty())
        return;
    // The first line names the stage, mining mode and wall seed the expedition was created with.
    std::istringstream header(text.substr(0, text.find('\n')));
    std::string tag;
    int stage = 0, mining = -1;
    long long seed = -1;
    header >> tag >> stage >> mining >> seed;
    const bool known = tag == "stage" && stage >= 1 &&
                       stage <= std::min(game::TotalStages, state.app.highestUnlockedStage) &&
                       (mining == 0 || mining == 1) && seed >= 0 && seed <= 0xffffffffLL &&
                       text.find('\n') != std::string::npos;
    if (known) {
        game::Catalog content = game::stageCatalog(stage);
        content.mining = static_cast<game::MiningMode>(mining);
        content.mineSeed = static_cast<std::uint32_t>(seed);
        game::RunCreation created = game::Run::create(std::move(content));
        if (created.run) {
            const game::Result restored = created.run->restore(text.substr(text.find('\n') + 1));
            if (restored.ok) {
                run = std::move(*created.run);
                state.app.selectedStage = stage;
                state.play = presentation::PlayState{};
                state.play.currentStage = stage;
                state.play.toast = restored.message;
                state.app.navigation.resume();
                feedback.reset();
                savedRun_ = text;
                savedPhase_ = run.state().phase;
                return;
            }
        }
    }
    Persistence::clearRun();
}
void Application::syncRunSave(float elapsed, bool force) {
    if (benchmark_ || !state.app.campaign || tutorial.active() || !state.app.navigation.hasRun())
        return;
    const game::Phase phase = run.state().phase;
    if (phase == game::Phase::Won || phase == game::Phase::Lost) {
        if (!savedRun_.empty()) {
            Persistence::clearRun();
            savedRun_.clear();
        }
        return;
    }
    runSaveTimer_ += std::isfinite(elapsed) ? std::max(0.0F, elapsed) : 0.0F;
    if (!force && phase == savedPhase_ && runSaveTimer_ < .5F)
        return;
    runSaveTimer_ = 0;
    savedPhase_ = phase;
    const std::string body = run.snapshot();
    if (body.empty())
        return;
    const std::string text = "stage " + std::to_string(state.play.currentStage) + " " +
                             std::to_string(static_cast<int>(run.catalog().mining)) + " " +
                             std::to_string(run.catalog().mineSeed) + "\n" + body;
    if (text == savedRun_)
        return;
    Persistence::saveRun(text);
    savedRun_ = text;
}
Application::Application(game::Run initialRun)
    : run(std::move(initialRun)), graphics(presentation::assets::renderAssets()), sound(audio),
      feedback(run, state, sound), flow(run, state, ui, audio, sound, feedback),
      gameplay(run, state, ui, view.layout, sound, feedback), tutorial(run, state, ui, feedback, sound),
      camera(state, ui, view.layout), mineScroll(run, state, ui, view.layout) {
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
    state.app.preferences.excavation = saved.excavation;
    audio.settings.volume = saved.volume;
    audio.settings.music = saved.music;
    audio.settings.muted = saved.muted;
}
bool Application::swallowSplashInput() {
    constexpr float SkippableAfter = 3.0F;
    if (!splashActive_)
        return false;
    if (splashTime_ >= SkippableAfter)
        splashActive_ = false;
    return true;
}
void Application::startSplash() {
    splashActive_ = graphics.animationAssets().count(presentation::assets::ThorvgSplash) > 0;
    splashTime_ = 0;
}
bool Application::splashFrame(int width, int height, float density, float elapsed) {
    const engine::AnimationInfo &info = graphics.animationAssets().at(presentation::assets::ThorvgSplash);
    splashTime_ += elapsed;
    if (splashTime_ >= info.duration) {
        splashActive_ = false;
        return true;
    }
    if (!graphics.begin(width, height, std::min(density, engine::Renderer::MaxDensity)))
        return false;
    graphics.rect({0, 0, static_cast<float>(width), static_cast<float>(height)}, {255, 255, 255});
    const float scale = std::min(width / info.width, height / info.height);
    const engine::Rect bounds{(width - info.width * scale) / 2, (height - info.height * scale) / 2,
                              info.width * scale, info.height * scale};
    const float frame = std::min(splashTime_ / info.duration * info.frames, info.frames - 1.0F);
    graphics.animation({presentation::assets::ThorvgSplash, frame, {0, info.frames}}, 0, bounds,
                       engine::AnimationBackend::Vector);
    return graphics.finish();
}
bool Application::frame(int width, int height, float density, float elapsed) {
    using engine::FrameClock;
    using engine::FrameStage;
    frameTimings_ = {};
    FrameClock::time_point stage = FrameClock::now();
    elapsed = std::isfinite(elapsed) ? std::clamp(elapsed, 0.0F, 0.1F) : 0.0F;
    if (splashActive_)
        return splashFrame(width, height, density, elapsed);
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
    audio.setBattle(run.state().phase == game::Phase::Battle);
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
            if (benchmark_ && run.state().phase == game::Phase::Battle && benchmarkSteps_++ % 30 == 0)
                game::autoplay::playCard(run);
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
    tutorial.advance(elapsed);
    tutorial.observe();
    if (flow.synchronizeProgress())
        savePersistence();
    syncRunSave(elapsed);
    frameTimings_.add(FrameStage::Presentation, engine::frameMilliseconds(stage));
    if (state.app.preferences.cachedAnimations && !animationCachePrepared_) {
        stage = FrameClock::now();
        // Only the common enemy's frames are prepared; rarer roles bake on first use within the budget.
        if (const presentation::ActorAnimationSet *common =
                state.app.animationSets.find(run.catalog().enemy(game::EnemyKind::Normal).animation)) {
            graphics.prewarmAnimation(common->idle);
            graphics.prewarmAnimation(common->move);
            graphics.prewarmAnimation(common->attack);
            graphics.prewarmAnimation(common->death);
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
    // The scale applies to the density the renderer would use, so it lowers pixels on dense displays too.
    if (!graphics.begin(width, height,
                        std::min(density, engine::Renderer::MaxDensity) * renderScale_.value()))
        return false;
    frameTimings_.add(FrameStage::Raster, engine::frameMilliseconds(stage));
    stage = FrameClock::now();
    ui.advance(elapsed);
    if (!stopAudio)
        mineScroll.follow(elapsed);
    view.draw(graphics, ui, run, state, audio.settings);
    if (state.app.navigation.gameplay() && !state.play.cameraFramed && !state.play.mine) {
        state.play.camera = view.layout.camera.pose();
        state.play.cameraFramed = true;
    }
    frameTimings_.add(FrameStage::Scene, engine::frameMilliseconds(stage));
    stage = FrameClock::now();
    const bool rendered = graphics.finish();
    frameTimings_.add(FrameStage::Raster, engine::frameMilliseconds(stage));
    renderScale_.update(frameTimings_.measuredMs(), elapsed);
    return rendered;
}
void Application::pointer(engine::Pointer event) {
    if (swallowSplashInput())
        return;
    const std::optional<engine::Interaction> interaction = ui.pointer(event);
    const bool cameraDrag = camera.pointer(event);
    const bool mineDrag = mineScroll.pointer(event);
    if (interaction && !cameraDrag && !mineDrag)
        act(*interaction);
}
void Application::wheel(engine::ScreenPoint point, float steps) {
    if (splashActive_)
        return;
    if (!mineScroll.wheel(steps))
        camera.wheel(point, steps);
}
void Application::applyFlow(FlowResult result) {
    if (result == FlowResult::Restarted) {
        camera.cancel();
        mineScroll.cancel();
        state.play.mineScroll = 0;
        accumulator_ = 0;
        audioPaused_ = false;
    }
}
void Application::act(engine::Interaction interaction) {
    if (splashActive_)
        return;
    const std::optional<GameInput> input = decodeInput(interaction);
    if (!input)
        return;
    if (input->kind == InputKind::Control && camera.act(input->action, input->position))
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
    if (result != FlowResult::Unhandled)
        camera.cancel();
    if (result == FlowResult::Unhandled)
        gameplay.act(*input);
    tutorial.observe();
}
void Application::key(int key, bool shift) {
    if (swallowSplashInput())
        return;
    if (key == '+' || key == '-' || key == '0') {
        act({engine::Interaction::Kind::Click,
             key == '+'   ? presentation::ZoomIn
             : key == '-' ? presentation::ZoomOut
                          : presentation::ZoomFit,
             {}});
        return;
    }
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
    if (benchmark_)
        return;
    SaveData data;
    data.tutorialCompleted = state.app.tutorialCompleted;
    data.highestUnlockedStage = state.app.highestUnlockedStage;
    data.reducedMotion = state.app.preferences.reducedMotion;
    data.damageNumbers = state.app.preferences.damageNumbers;
    data.cachedAnimations = state.app.preferences.cachedAnimations;
    data.excavation = state.app.preferences.excavation;
    data.volume = audio.settings.volume;
    data.music = audio.settings.music;
    data.muted = audio.settings.muted;
    Persistence::save(data);
}
bool Application::startBenchmark(const BenchRequest &request) {
    BenchScene scene = prepareBenchScene(request);
    if (!scene.run) {
        graphics.error = "Bench scene: " + scene.error;
        return false;
    }
    benchmark_ = request;
    benchmarkSteps_ = 0;
    run = std::move(*scene.run);
    state.app.campaign = true;
    state.app.selectedStage = request.stage;
    state.app.preferences = presentation::Preferences{};
    if (request.cachedAnimations)
        state.app.preferences.cachedAnimations = *request.cachedAnimations;
    state.play = presentation::PlayState{};
    state.play.currentStage = request.stage;
    state.play.guideOpen = false;
    state.play.hints = {true, true, true, true, true};
    state.app.navigation.enterGame();
    feedback.reset();
    accumulator_ = 0;
    return run.startWave().ok;
}
void Application::suspend() {
    camera.cancel();
    mineScroll.cancel();
    ui.cancel();
    audio.stop();
    sound.cancelPending();
    accumulator_ = 0;
    savePersistence();
    syncRunSave(0, true);
    if (run.state().phase == game::Phase::Battle || run.state().job.active)
        state.play.paused = true;
}
std::string Application::debugState() const {
    std::string result = run.debugState();
    result.pop_back();
    result += ",\"paused\":" + std::string(state.play.paused ? "true" : "false") +
              ",\"splash\":" + (splashActive_ ? "true" : "false") +
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
    const engine::CameraPose pose = view.layout.camera.pose();
    feedback << ",\"campaign\":" << (state.app.campaign ? "true" : "false")
             << ",\"selectedStage\":" << state.app.selectedStage
             << ",\"currentStage\":" << state.play.currentStage << ",\"benchmark\":";
    if (benchmark_)
        feedback << "{\"stage\":" << benchmark_->stage << ",\"wave\":" << benchmark_->wave
                 << ",\"crowd\":" << benchmark_->crowd << '}';
    else
        feedback << "null";
    feedback << ",\"highestUnlockedStage\":" << state.app.highestUnlockedStage
             << ",\"totalWaves\":" << run.catalog().waves.size() << ",\"mineLayer\":" << run.state().mineLayer
             << ",\"mineLayers\":" << run.catalog().deeperMines.size() + 1
             << ",\"canDescend\":" << (run.descentAvailability().ok ? "true" : "false")
             << ",\"mine\":{\"x\":" << view.layout.mineOrigin.x << ",\"y\":" << view.layout.mineOrigin.y
             << ",\"tile\":" << view.layout.mineTile << ",\"columns\":" << run.state().mineWidth
             << ",\"rows\":" << run.state().mineHeight << ",\"scroll\":" << view.layout.mineScroll
             << ",\"scrollMax\":" << view.layout.mineScrollMax
             << ",\"viewport\":{\"x\":" << view.layout.mineViewport.x
             << ",\"y\":" << view.layout.mineViewport.y << ",\"w\":" << view.layout.mineViewport.w
             << ",\"h\":" << view.layout.mineViewport.h << "}}"
             << ",\"field\":{\"width\":" << run.catalog().width << ",\"height\":" << run.catalog().height
             << ",\"originX\":" << view.layout.mapOrigin.x << ",\"originY\":" << view.layout.mapOrigin.y
             << ",\"tile\":" << view.layout.tile << ",\"zoom\":" << pose.zoom << ",\"panX\":" << pose.panX
             << ",\"panY\":" << pose.panY << "},\"lanes\":[";
    bool firstLane = true;
    for (const game::LaneForecast &lane : run.laneForecast()) {
        if (!firstLane)
            feedback << ',';
        firstLane = false;
        const game::Point entry = run.route(lane.route).front();
        feedback << "{\"route\":" << lane.route << ",\"count\":" << lane.count
                 << ",\"runners\":" << lane.runners << ",\"armored\":" << lane.armored << ",\"x\":" << entry.x
                 << ",\"y\":" << entry.y << ",\"opensAtWave\":"
                 << (lane.route ? run.catalog().additionalRoutes[lane.route - 1].opensAtWave : 1)
                 << ",\"length\":" << run.routeLength(lane.route) << ",\"kinds\":[";
        for (std::size_t kind = 0; kind < lane.kinds.size(); ++kind)
            feedback << (kind ? "," : "") << lane.kinds[kind];
        feedback << "]}";
    }
    feedback << ']';
    feedback << ",\"pads\":[";
    for (std::size_t index = 0; index < run.state().pads.size(); ++index) {
        const game::Point pad = run.state().pads[index];
        const bool occupied = std::any_of(
            run.state().towers.begin(), run.state().towers.end(),
            [pad](const game::Tower &tower) -> bool { return game::distance(tower.position, pad) < .1F; });
        feedback << (index ? "," : "") << "{\"x\":" << pad.x << ",\"y\":" << pad.y
                 << ",\"occupied\":" << (occupied ? "true" : "false") << '}';
    }
    feedback << ']';
    feedback << ",\"boulders\":[";
    for (std::size_t index = 0; index < run.state().boulders.size(); ++index) {
        const game::Point boulder = run.state().boulders[index];
        const bool selected =
            state.play.selectedBoulder && game::distance(*state.play.selectedBoulder, boulder) < .1F;
        feedback << (index ? "," : "") << "{\"x\":" << boulder.x << ",\"y\":" << boulder.y
                 << ",\"selected\":" << (selected ? "true" : "false") << '}';
    }
    feedback << ']';
    feedback << ",\"rails\":[";
    for (std::size_t index = 0; index < run.catalog().rails.size(); ++index) {
        const game::RailDefinition &rail = run.catalog().rails[index];
        feedback << (index ? "," : "") << "{\"x\":" << rail.lever.x << ",\"y\":" << rail.lever.y
                 << ",\"countdown\":" << run.cartCountdown(index)
                 << ",\"progress\":" << run.cartProgress(index) << '}';
    }
    feedback << "],\"tanks\":[";
    for (std::size_t index = 0; index < run.catalog().tanks.size(); ++index) {
        const game::Point tank = run.catalog().tanks[index].position;
        feedback << (index ? "," : "") << "{\"x\":" << tank.x << ",\"y\":" << tank.y
                 << ",\"ready\":" << (run.tankReady(index) ? "true" : "false") << '}';
    }
    feedback << "],\"forks\":[";
    for (std::size_t index = 0; index < run.catalog().forks.size(); ++index) {
        const game::Point spot = run.barricade(index);
        feedback << (index ? "," : "") << "{\"x\":" << spot.x << ",\"y\":" << spot.y
                 << ",\"detour\":" << (run.detoured(index) ? "true" : "false")
                 << ",\"length\":" << run.routeLength(run.catalog().forks[index].route) << '}';
    }
    feedback << ']';
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
             << ",\"scale\":" << renderScale_.value() << ",\"density\":" << graphics.density()
             << ",\"cached\":" << (state.app.preferences.cachedAnimations ? "true" : "false")
             << ",\"bytes\":" << render.cache.bytes << ",\"entries\":" << render.cache.entries
             << ",\"pinned\":" << render.cache.pinnedEntries << ",\"hits\":" << render.cache.hits
             << ",\"misses\":" << render.cache.misses << ",\"bakes\":" << render.cache.bakes
             << ",\"evictions\":" << render.cache.evictions << ",\"rejected\":" << render.cache.rejected
             << ",\"bakeMs\":" << render.cache.bakeMilliseconds << ",\"bakerSources\":" << render.bakerSources
             << ",\"sprites\":" << render.spriteInstances << ",\"vectors\":" << render.vectorInstances
             << ",\"cachedDraws\":" << render.cachedDraws << ",\"vectorDraws\":" << render.vectorDraws
             << ",\"poolHits\":" << render.poolHits << ",\"pooledVectors\":" << render.pooledVectors
             << ",\"fallbacks\":" << render.fallbacks << ",\"layers\":" << graphics.layerRasterizations()
             << ",\"layerReuses\":" << graphics.layerReuses()
             << "},\"actors\":" << state.play.actors.debugState()
             << ",\"deathVisuals\":" << state.play.actors.deaths()
             << ",\"audio\":{\"muted\":" << (audio.settings.muted ? "true" : "false")
             << ",\"volume\":" << audio.settings.volume << ",\"voices\":" << audio.activeVoices()
             << ",\"played\":" << audio.played << ",\"suppressed\":" << audio.suppressed
             << ",\"beeps\":" << sound.beepClips << ",\"mp3\":" << sound.mp3Clips
             << ",\"music\":" << (sound.music ? "true" : "false")
             << ",\"musicLoops\":" << (audio.music() ? audio.music()->loops() : 0)
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
