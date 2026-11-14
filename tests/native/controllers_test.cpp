#include "Scenario.h"
#include "app/AppFlow.h"
#include "app/GameplayController.h"
#include "game/presentation/AssetKeys.h"
#include "game/presentation/CardPresentation.h"
#include "game/presentation/CommandPolicy.h"
#include "game/presentation/TerrainPresentation.h"
#include "game/presentation/Text.h"
#include "game/presentation/Theme.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <type_traits>
#include <utility>

using namespace thorium;
using namespace thorium::game;
using namespace thorium::presentation;
namespace {
int checks = 0;
/** Fail independently of release-build assertion settings. */
void require(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
static_assert(std::is_same_v<decltype(std::declval<Run &>().state()), const RunState &>);
static_assert(std::is_same_v<decltype(std::declval<Run &>().catalog()), const Catalog &>);

/** Compose real controllers with no SDL, ThorVG or graphics device. */
struct Session {
    Run run;
    State state;
    engine::Ui ui;
    engine::AudioMixer audio;
    Soundscape sound{audio, "/thorium-no-optional-audio"};
    FeedbackChannel feedback{run, state, sound};
    Layout layout;
    AppFlow flow{run, state, ui, audio, sound, feedback};
    GameplayController gameplay{run, state, ui, layout, sound, feedback};
    /** Seed observable hand identities and simple, invertible test geometry. */
    explicit Session(Catalog content = defaultCatalog()) : run(makeRun(std::move(content))) {
        const engine::AnimationInfo info{
            75, 2.5F, {{"idle", {0, 1}}, {"move", {0, 45}}, {"attack", {45, 57}}, {"death", {57, 75}}}};
        AnimationBindings bindings;
        for (const char *creature : assets::Creatures)
            bindings.push_back({creature, "oreling"});
        state.app.animationSets = animationSets({{"oreling", info}}, bindings, run.catalog());
        feedback.reset();
        layout.world = {0, 0, 400, 300};
        layout.tile = 10;
        layout.mineTile = 20;
    }
    /** Route the same decoded control boundary used by Application. */
    void click(engine::WidgetId id, engine::ScreenPoint position = {}) {
        const std::optional<GameInput> input = decodeInput({engine::Interaction::Kind::Click, id, position});
        require(input.has_value(), "fixture control has a supported identity");
        if (input->kind != InputKind::Control || flow.act(input->action) == FlowResult::Unhandled)
            gameplay.act(*input);
    }
};

/** Verify content snapshots, authored schedules, costs and attacks without changing rule code. */
void contentContracts() {
    const Catalog &defaults = defaultCatalog();
    require(defaults.schedule(1).size() == 16 && defaults.schedule(2).size() == 28 &&
                defaults.schedule(3).size() == 44,
            "authored schedules preserve all three wave totals");
    const std::vector<Spawn> schedule = defaults.schedule(3);
    require(std::is_sorted(schedule.begin(), schedule.end(),
                           [](const Spawn &a, const Spawn &b) { return a.time < b.time; }),
            "spawn groups expand in stable chronological order");
    Catalog custom = defaults;
    custom.cards[0].cost = 1;
    custom.cards[0].damage = 21;
    custom.cards[0].range = 8;
    custom.balance.tool = {2, 1};
    Run run = makeRun(custom);
    custom.cards[0].cost = 9;
    require(run.catalog().card(CardKind::Gun).cost == 1 && defaults.card(CardKind::Gun).cost == 3,
            "a run owns its immutable content snapshot independently of the source and other runs");
    run.startWave();
    require(run.play(1, {6, 4}).ok && run.state().energy == 5, "card commands charge the run catalog price");
    scenario(run).enemies.push_back({900, EnemyKind::Normal, {4, 3}, 0, 100});
    run.advance(1.0F / 60.0F);
    require(run.state().enemies.front().hp == 79, "combat reads catalog damage and targeting range");
    scenario(run).phase = Phase::Prep;
    scenario(run).iron = 2;
    require(!run.upgrade(UpgradeKind::Tool).ok && run.state().iron == 2,
            "recipe failure preserves every inventory entry");
    scenario(run).copper = 1;
    require(costText(run.catalog().balance.tool) == oreName(Ore::Iron) + "2 " + oreName(Ore::Copper) + "1",
            "content prices share one UI and error label");
    require(run.upgrade(UpgradeKind::Tool).ok && run.state().iron == 0 && run.state().copper == 0,
            "recipe validation and payment use the same catalog definition");
    Catalog empty = defaults;
    empty.waves = {{}};
    Run quiet = makeRun(empty);
    require(quiet.startWave().ok, "empty authored schedule has a valid start");
    quiet.advance(1.0F / 60.0F);
    require(quiet.state().phase == Phase::Won, "empty final wave completes without reading a missing spawn");
}

/** Protect navigation, pause, confirmation and distinct session/run state lifetimes. */
void flowContracts() {
    Session session;
    require(!session.flow.allowsSimulation(), "cold title gates simulation");
    session.click(HomeNew);
    require(session.state.app.navigation.gameplay() && session.flow.allowsSimulation(),
            "new expedition enters playable screen");
    session.click(Start);
    session.run.advance(.1F);
    session.click(Pause);
    const std::string before = session.run.debugState();
    session.click(OpenSettings);
    session.click(OpenGuide);
    session.click(NavigateBack);
    require(session.state.app.navigation.current() == Screen::Settings, "nested Back restores actual caller");
    session.click(NavigateBack);
    require(session.state.play.paused && !session.flow.allowsSimulation() &&
                session.run.debugState() == before,
            "utility navigation preserves a manually paused run exactly");
    session.click(CardBase + 1);
    session.click(World, session.layout.screenPoint({6, 4}));
    require(session.run.state().towers.empty() && session.run.state().energy == 6,
            "stale gameplay input cannot spend resources under a pause screen");
    session.click(Resume);
    require(session.flow.allowsSimulation(), "resume reopens the simulation gate");
    session.click(ToggleMotion);
    session.click(ToggleDamage);
    session.click(ToggleAnimationBackend);
    session.click(AudioMute);
    session.audio.settings.volume = .15F;
    session.click(Reset);
    require(session.state.play.modal == Modal::NewRun && !session.flow.allowsSimulation(),
            "reset confirmation gates the simulation before replacement");
    session.click(Close);
    require(session.run.debugState() == before && session.flow.allowsSimulation(),
            "canceling reset leaves the current run untouched");
    session.click(GoHome);
    session.click(HomeNew);
    require(session.state.play.modal == Modal::NewRun && session.run.debugState() == before,
            "title new-run action requires confirmation while a run exists");
    session.click(Close);
    session.click(HomeContinue);
    require(session.run.debugState() == before, "continue retains the same run and elapsed simulation time");
    scenario(session.run).enemies.push_back({880, EnemyKind::Normal, {4, 3}, 0, 36});
    session.feedback.collect();
    require(session.state.play.actors.find(880), "active run owns an independent actor animation instance");
    session.state.play.selectedUnit = 888;
    session.state.play.selectedTower = 999;
    session.state.play.feedback.consume({{CombatEventKind::Damage, 999, {}, {}}});
    session.click(Reset);
    session.click(ConfirmReset);
    require(session.run.state().phase == Phase::Ready && session.run.state().wave == 0 &&
                session.state.play.selectedUnit == 0 && session.state.play.selectedTower == 0 &&
                session.state.play.feedback.cues.empty(),
            "new run drops stale identities and visual effects");
    require(session.state.play.actors.actors().empty() && session.state.app.animationSets.find("oreling"),
            "new run clears actor playback while retaining session animation bindings");
    require(session.state.app.preferences.cachedAnimations, "rendering choice survives new-run reset");
    require(session.state.app.preferences.reducedMotion && !session.state.app.preferences.damageNumbers &&
                session.audio.settings.muted && session.audio.settings.volume == .15F,
            "new run preserves visual and audio preferences by lifetime rather than manual copying");
    require(session.flow.act(ConfirmReset) == FlowResult::Handled,
            "a stale confirmation cannot reset a different run");
    const std::string resetState = session.run.debugState();
    session.click(SettingsDefaults);
    require(!session.state.app.preferences.cachedAnimations && session.run.debugState() == resetState,
            "default settings restore the measured vector path without resetting gameplay");
}

/** Keep result controls, explicit confirmations and stale input governed by the same eligibility query. */
void restartPolicyContracts() {
    /** Describe meaningful screen/phase/modal combinations independently of the policy implementation. */
    struct Case {
        Screen screen;
        Phase phase;
        Modal modal;
        bool allowed;
    };
    const std::array<Case, 11> cases{{
        {Screen::Game, Phase::Won, Modal::None, true},
        {Screen::Game, Phase::Lost, Modal::None, true},
        {Screen::Game, Phase::Battle, Modal::NewRun, true},
        {Screen::Home, Phase::Battle, Modal::NewRun, true},
        {Screen::Game, Phase::Ready, Modal::None, false},
        {Screen::Game, Phase::Battle, Modal::None, false},
        {Screen::Game, Phase::Prep, Modal::None, false},
        {Screen::Home, Phase::Won, Modal::None, false},
        {Screen::Game, Phase::Prep, Modal::Recruit, false},
        {Screen::Settings, Phase::Won, Modal::None, false},
        {Screen::Guide, Phase::Battle, Modal::NewRun, false},
    }};
    Session session;
    for (const Case &item : cases) {
        session.state.app.navigation.home();
        if (item.screen == Screen::Game)
            session.state.app.navigation.enterGame();
        else if (item.screen != Screen::Home)
            session.state.app.navigation.open(item.screen);
        scenario(session.run).phase = item.phase;
        session.state.play.modal = item.modal;
        require(canConfirmNewRun(item.phase, session.state) == item.allowed,
                "restart eligibility matches result, explicit confirmation and stale-input contracts");
        const std::string before = session.run.debugState();
        const FlowResult result = session.flow.act(ConfirmReset);
        if (item.allowed)
            require(result == FlowResult::Restarted && session.run.state().phase == Phase::Ready &&
                        session.state.play.modal == Modal::None && session.state.app.navigation.gameplay(),
                    "execution honors the same eligible restart as the visible control");
        else
            require(result == FlowResult::Handled && session.run.debugState() == before,
                    "ineligible confirmation is consumed without replacing the current run");
    }
}

/** Exercise controller placement and feedback through the same geometry as the view. */
void gameplayContracts() {
    Session session;
    session.click(HomeNew);
    session.click(Start);
    session.click(CardBase + 1);
    session.click(World, session.layout.screenPoint({6, 4}));
    require(session.run.state().towers.size() == 1 && session.run.state().energy == 3 &&
                session.state.play.selectedCard == 0 && session.state.play.feedback.handChanges == 1,
            "selection and world input commit one card effect and one replacement transition");
    session.click(World, session.layout.screenPoint({6, 4}));
    require(session.state.play.selectedTower == session.run.state().towers.front().id,
            "world selection resolves stable placed-entity identity");
    session.click(ConfirmDismiss);
    require(session.run.state().towers.size() == 1, "withdrawal requires its active confirmation");
    session.click(Dismiss);
    require(!session.flow.allowsSimulation(), "withdrawal confirmation uses the shared simulation gate");
    session.state.play.paused = true;
    session.click(ConfirmDismiss);
    require(session.run.state().towers.empty() && session.state.play.selectedTower == 0,
            "withdrawal clears both authoritative entity and disposable selection");
    require(session.state.play.paused, "confirmation after focus-loss pause preserves the manual pause");
    session.click(Resume);
    const Phase before = session.run.state().phase;
    enterPreparation(session.run);
    session.state.play.selectedCard = 2;
    session.gameplay.synchronize(before);
    require(session.state.play.mine && session.state.play.selectedCard == 0,
            "phase transition enters cutaway view and drops old placement intent");
    require(session.layout.mineCell({-1, 2}) == -1 && session.layout.mineCell({100, 40}) == -1,
            "shared mine geometry rejects margins without clamping to a tile");
    session.click(Recruit);
    session.click(CandidateBase + static_cast<int>(CardKind::Gun));
    require(session.state.play.modal == Modal::Recruit, "recruitment rejects a stale non-unit candidate");
    session.click(CandidateBase + static_cast<int>(CardKind::Shooter));
    require(session.state.play.modal == Modal::Replacement,
            "valid recruitment enters typed replacement modal");
    session.click(Close);
    const Point original{8, 4};
    require(distance(original, session.layout.surfacePoint(session.layout.screenPoint(original), true)) == 0,
            "drawing and input use inverse surface transforms");
    require(!decodeInput({engine::Interaction::Kind::Click, CandidateBase + 500, {}}) &&
                !decodeInput({engine::Interaction::Kind::Click, 9, {}}),
            "unknown widget controls cannot become unchecked card enums");
}
/** A rejected drop releases its card, while a click-selected card stays armed for another target. */
void rejectedDropContracts() {
    Session session;
    session.click(HomeNew);
    session.click(Start);
    scenario(session.run).energy = 0;
    const engine::ScreenPoint pad = session.layout.screenPoint({6, 4});
    const std::string before = session.run.debugState();
    session.gameplay.act({InputKind::Hand, World, 1, engine::Interaction::Kind::Drop, pad});
    require(session.run.debugState() == before && session.state.play.selectedCard == 0 &&
                session.state.play.error && session.state.play.feedback.rejectedCard == 1,
            "an unaffordable drop changes nothing, explains itself and leaves no card selected");
    session.click(CardBase + 1);
    session.click(World, pad);
    require(session.run.debugState() == before && session.state.play.selectedCard == 1,
            "a click-selected card survives a rejected target so its reason and exchange stay available");
}
/** A boulder is explained, selected and cleared through world taps and the inspector's control. */
void boulderContracts() {
    Catalog content = defaultCatalog();
    content.boulders = {{11, 5}};
    content.padTraits = {{{9, 4}, PadTrait::Crystal}};
    Session session(content);
    session.click(HomeNew);
    const engine::ScreenPoint rock = session.layout.screenPoint({11, 5});
    session.click(World, rock);
    require(!session.state.play.selectedBoulder &&
                session.state.play.toast.find(tr("app.obstacle.boulder")) != std::string::npos &&
                !session.state.play.error,
            "a boulder tapped outside preparation explains itself without a selection");
    session.click(World, session.layout.screenPoint({9, 4}));
    require(session.state.play.toast.find(std::string(padTraitName(PadTrait::Crystal))) !=
                    std::string::npos &&
                session.state.play.toast.find("+20%") != std::string::npos,
            "an empty trait pad names its bonus");
    const Phase before = session.run.state().phase;
    enterPreparation(session.run);
    session.gameplay.synchronize(before);
    session.click(Surface);
    session.click(World, rock);
    require(session.state.play.selectedBoulder &&
                distance(*session.state.play.selectedBoulder, {11, 5}) == 0 &&
                !session.state.play.selectedTower && !session.state.play.selectedUnit,
            "a preparation tap selects the boulder");
    session.click(ClearBoulder);
    require(session.run.state().boulders.size() == 1 && session.state.play.error &&
                session.state.play.selectedBoulder,
            "an unaffordable clear explains itself and keeps the boulder selected");
    scenario(session.run).iron = 3;
    session.click(ClearBoulder);
    require(session.run.state().boulders.empty() && session.run.state().iron == 0 &&
                !session.state.play.selectedBoulder && !session.state.play.error,
            "the clear control pays once and drops the selection");
    scenario(session.run).iron = 3;
    session.click(ClearBoulder);
    require(session.run.state().iron == 3, "a clear command without a selected boulder does nothing");
    session.click(World, rock);
    require(!session.state.play.selectedBoulder, "the cleared tile is an ordinary pad");
}
/** Keep custom card categories authoritative through pointer transforms and visual vocabulary. */
void customCategoryContracts() {
    Catalog content = defaultCatalog();
    content.cards[0].category = CardCategory::Spell;
    content.cards[0].range = .1F;
    content.cards[0].damage = 100;
    for (const engine::Interaction::Kind intent :
         {engine::Interaction::Kind::Click, engine::Interaction::Kind::Drop}) {
        Session session(content);
        session.click(HomeNew);
        session.click(Start);
        const Point target{6.25F, 4.25F};
        scenario(session.run).enemies.push_back({900, EnemyKind::Normal, target, 0, 36});
        const engine::ScreenPoint position = session.layout.screenPoint(target);
        if (intent == engine::Interaction::Kind::Click) {
            session.click(CardBase + 1);
            session.click(World, position);
        } else {
            session.gameplay.act({InputKind::Hand, World, 1, intent, position});
        }
        require(session.run.state().defeated == 1 && session.run.state().enemies.empty(),
                "click and drag both use the run catalog rather than rounding a reclassified spell");
        const CardCategory category = session.run.catalog().card(CardKind::Gun).category;
        require(category == CardCategory::Spell &&
                    std::string(cardCategoryName(category)) ==
                        std::string(cardCategoryName(CardCategory::Spell)) &&
                    std::string(cardInstruction(category)) ==
                        std::string(cardInstruction(CardCategory::Spell)) &&
                    std::string(cardInstruction(category)) !=
                        std::string(cardInstruction(CardCategory::Building)) &&
                    theme::accent(category).r == theme::purple.r,
                "category labels, instructions and accents share the authoritative definition");
    }
}
/** Keep dialog selection disposable while the run owns the candidate list and payment lifetime. */
void offerDialogContracts() {
    Catalog content = defaultCatalog();
    content.cards[0].category = CardCategory::Unit;
    content.cards[0].hp = 50;
    content.recruitmentCandidates = {CardKind::Gun, CardKind::Guard, CardKind::Shooter};
    Session session(content);
    session.click(HomeNew);
    enterPreparation(session.run);
    scenario(session.run).gold = 4;
    const CardOffer offered = *session.run.state().recruitmentOffer;
    session.click(Recruit);
    session.click(CandidateBase + static_cast<int>(CardKind::Demolition));
    require(session.state.play.modal == Modal::Recruit,
            "controller rejects a supported unit outside the actual authored offer");
    session.click(CandidateBase + static_cast<int>(CardKind::Gun));
    require(session.state.play.modal == Modal::Replacement && session.state.play.selectedOffer == offered.id,
            "a newly authored candidate is selected by offer membership rather than a fixed UI pool");
    session.click(Close);
    require(session.run.offer(offered.id) && session.run.state().gold == 4 &&
                session.state.play.selectedOffer == 0,
            "closing replacement discards UI selection without consuming the domain offer");
    session.click(Recruit);
    require(session.state.play.selectedOffer == offered.id &&
                session.run.offer(offered.id)->candidates == offered.candidates,
            "reopening the dialog binds the same existing candidates and offer identity");
    session.click(CandidateBase + static_cast<int>(CardKind::Gun));
    session.click(DeckBase + session.run.state().deck[7].id);
    require(!session.run.state().recruitmentOffer && session.run.state().gold == 0 &&
                session.state.play.modal == Modal::None,
            "only the valid replacement confirmation consumes the offer and charges once");
}
} // namespace

/** Run architecture boundary and controller contracts with portable concrete implementations. */
int main() {
    contentContracts();
    customCategoryContracts();
    offerDialogContracts();
    flowContracts();
    restartPolicyContracts();
    gameplayContracts();
    rejectedDropContracts();
    boulderContracts();
    std::cout << checks << " controller/content checks passed\n";
}
