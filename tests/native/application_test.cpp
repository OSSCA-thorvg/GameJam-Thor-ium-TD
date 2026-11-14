#include "ApplicationScenario.h"
#include "app/Application.h"
#include "app/Persistence.h"
#include "game/autoplay/Autoplayer.h"
#include "game/presentation/Text.h"
#include "game/presentation/Theme.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <type_traits>
#include <unistd.h>
#include <utility>

using namespace thorium;
namespace {
int checks = 0;
/** Detect an accidental public state owner without requiring a particular member layout. */
template <typename T, typename = void> struct ExposesRun : std::false_type {};
template <typename T>
struct ExposesRun<T, std::void_t<decltype(std::declval<T &>().run)>> : std::true_type {};
/** Protect presentation state ownership from being reopened for platform convenience. */
template <typename T, typename = void> struct ExposesState : std::false_type {};
template <typename T>
struct ExposesState<T, std::void_t<decltype(std::declval<T &>().state)>> : std::true_type {};
static_assert(!ExposesRun<Application>::value && !ExposesState<Application>::value);
static_assert(std::is_same_v<decltype(std::declval<Application &>().audioOutput()), engine::AudioOutput>);
static_assert(!std::is_constructible_v<game::Run, game::Catalog>);
static_assert(!std::is_constructible_v<Application, game::Catalog>);
static_assert(std::is_same_v<decltype(std::declval<Application &>().game()), const game::Run &>);
static_assert(
    std::is_same_v<decltype(std::declval<Application &>().renderOutput()), const engine::Renderer &>);
static_assert(presentation::AudioMute == 34 && presentation::HomeNew == 36 &&
              presentation::OpenSettings == 38 && presentation::ToggleAnimationBackend == 53);
static_assert(presentation::ToggleDrawer == 54 && presentation::CloseDrawer == 55 &&
              presentation::AcknowledgeDrawer == 56);
/** Fail a real-render integration contract independently of assertion compiler flags. */
void require(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Sample the actual software framebuffer in logical coordinates. */
engine::Color pixel(const engine::Renderer &renderer, engine::ScreenPoint position) {
    const int x = static_cast<int>(position.x * renderer.pixelWidth / renderer.width);
    const int y = static_cast<int>(position.y * renderer.pixelHeight / renderer.height);
    const std::uint32_t value = renderer.pixels[static_cast<std::size_t>(y * renderer.pixelWidth + x)];
    return {static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8),
            static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 24)};
}
/** Keep initial binding validation and real card appearance aligned with injected content. */
void applicationContracts() {
    game::Catalog invalid = game::defaultCatalog();
    invalid.enemies[0].animation = "unregistered-monster";
    {
        Application app(game::makeRun(invalid));
        require(app.error().find("unregistered-monster") != std::string::npos,
                "composition rejects missing catalog animation profiles before the first frame");
        require(!app.frame(1200, 800, 1, 0), "invalid content cannot launch invisible combatants");
    }
    game::Catalog custom = game::defaultCatalog();
    custom.cards[0].category = game::CardCategory::Spell;
    Application app(game::makeRun(custom));
    require(app.error().empty(), "valid custom content initializes the real renderer");
    app.act({engine::Interaction::Kind::Click, presentation::HomeNew, {}});
    require(app.frame(1200, 800, 1, 0), "actual UI renders with the injected catalog");
    const engine::Widget *card = nullptr;
    for (const engine::Widget &widget : app.widgets())
        if (widget.id == presentation::CardBase + 1)
            card = &widget;
    require(card != nullptr, "the first authored card is visible in the real hand");
    // Sample the category border, outside the inset art and text, rather than a screenshot hash.
    const engine::Color border =
        pixel(app.renderOutput(), {card->bounds.x + card->bounds.w / 2, card->bounds.y + 1});
    require(border.r == presentation::theme::purple.r && border.g == presentation::theme::purple.g,
            "real card pixels use the changed spell category rather than the default building color");
    require(app.frameTimings().simulationSteps == 0 &&
                app.frameTimings().stage(engine::FrameStage::Scene) > 0 &&
                app.frameTimings().stage(engine::FrameStage::Raster) > 0,
            "app profiling separates scene construction from CPU rasterization");
    app.act({engine::Interaction::Kind::Click, presentation::Start, {}});
    require(app.frame(1200, 800, 1, 1.0F / 60.0F) && app.frameTimings().simulationSteps == 1,
            "normal fixed steps are counted without using profiling time as game time");
    app.act({engine::Interaction::Kind::Click, presentation::Pause, {}});
    require(app.frame(1200, 800, 1, .1F) && app.frameTimings().simulationSteps == 0 &&
                app.frameTimings().stage(engine::FrameStage::Simulation) == 0,
            "paused frames cannot inherit stale simulation timings");
}
/** Render candidate controls from run-owned data, including a kind outside the old hardcoded pool. */
void offerUiContracts() {
    game::Catalog content = game::defaultCatalog();
    content.cards[0].category = game::CardCategory::Unit;
    content.cards[0].hp = 50;
    content.recruitmentCandidates = {game::CardKind::Gun, game::CardKind::Guard, game::CardKind::Shooter};
    Application app(game::makeRun(content));
    app.act({engine::Interaction::Kind::Click, presentation::HomeNew, {}});
    game::enterPreparation(ApplicationTestAccess::run(app));
    app.act({engine::Interaction::Kind::Click, presentation::Recruit, {}});
    require(app.frame(1200, 800, 1, 0), "custom recruitment offer renders through the real view");
    std::vector<engine::WidgetId> candidates;
    for (const engine::Widget &widget : app.widgets())
        if (widget.id >= presentation::CandidateBase && widget.id < presentation::CandidateBase + 8)
            candidates.push_back(widget.id);
    require(candidates == std::vector<engine::WidgetId>{3000, 3002, 3006},
            "actual candidate hit regions follow the authored offer rather than Guard/Shooter/Demolition "
            "literals");
}
/** Resolve a currently rendered hit rectangle without coupling tests to sidebar pixel constants. */
engine::Rect bounds(const Application &app, engine::WidgetId id) {
    for (const engine::Widget &widget : app.widgets())
        if (widget.id == id)
            return widget.bounds;
    std::cerr << "missing widget: " << id << "; state=" << app.debugState() << '\n';
    require(false, "expected widget exists");
    return {};
}
/** Reach both terminal phases through public commands and click the actual registered restart control. */
void terminalRestartContracts() {
    for (const game::Phase terminal : {game::Phase::Won, game::Phase::Lost}) {
        game::Catalog content = game::defaultCatalog();
        content.waves = {{}};
        if (terminal == game::Phase::Lost) {
            content.balance.baseHp = 1;
            content.enemies[0].speed = 100;
            content.waves = {{{game::EnemyKind::Normal, 1, 0, 0}}};
        }
        Application app(game::makeRun(content));
        app.act({engine::Interaction::Kind::Click, presentation::HomeNew, {}});
        app.act({engine::Interaction::Kind::Click, presentation::ToggleMotion, {}});
        app.act({engine::Interaction::Kind::Click, presentation::AudioMute, {}});
        app.act({engine::Interaction::Kind::Click, presentation::Start, {}});
        for (int frame = 0; frame < 20 && app.game().state().phase == game::Phase::Battle; ++frame)
            require(app.frame(640, 640, 1, .1F), "advance and render a natural terminal transition");
        require(app.game().state().phase == terminal, "authored public-command scenario reaches its result");
        const engine::ScreenPoint restart = bounds(app, presentation::ConfirmReset).center();
        app.pointer({engine::PointerPhase::Down, 0, restart});
        app.pointer({engine::PointerPhase::Up, 0, restart});
        require(app.game().state().phase == game::Phase::Ready && app.game().state().wave == 0 &&
                    app.game().state().baseHp == content.balance.baseHp,
                "victory and defeat restart controls each replace the terminal run immediately");
        require(app.debugState().find("\"reducedMotion\":true") != std::string::npos &&
                    app.audioOutput().muted(),
                "result-screen restart preserves session preferences");
        app.act({engine::Interaction::Kind::Click, presentation::Start, {}});
        const std::string started = app.game().debugState();
        app.pointer({engine::PointerPhase::Up, 0, restart});
        app.act({engine::Interaction::Kind::Click, presentation::ConfirmReset, {}});
        require(app.game().debugState() == started,
                "duplicate release and stale confirmation cannot reset the subsequent active battle");
    }
}
/** Exercise the real query-to-UI path, including affordable upgrades, rejection tint and app input gates. */
void placementUiContracts() {
    game::Catalog content = game::defaultCatalog();
    content.balance.initialEnergy = 2;
    Application app(game::makeRun(content));
    app.act({engine::Interaction::Kind::Click, presentation::HomeNew, {}});
    app.act({engine::Interaction::Kind::Click, presentation::Start, {}});
    game::Run &run = ApplicationTestAccess::run(app);
    game::scenario(run).towers.push_back({700, game::CardKind::Gun, {6, 4}});
    require(app.frame(1200, 800, 1, 0), "render upgrade-only affordability scenario");
    const engine::Rect card = bounds(app, presentation::CardBase + 1);
    const engine::ScreenPoint borderPoint{card.x + card.w / 2, card.y + 1};
    const engine::Color available = pixel(app.renderOutput(), borderPoint);
    require(available.r == presentation::theme::green.r && available.g == presentation::theme::green.g,
            "real hand card remains bright when two energy affords an upgrade but not a new three-energy "
            "building");
    game::scenario(run).energy = 1;
    require(app.frame(1200, 800, 1, 0), "render insufficient energy");
    const engine::Color dimmed = pixel(app.renderOutput(), borderPoint);
    require(dimmed.r == 96 && dimmed.g == 111,
            "card becomes dim only after every legal action is unaffordable");
    game::scenario(run).energy = 2;
    app.act({engine::Interaction::Kind::Click, presentation::CardBase + 1, {}});
    const presentation::View &view = ApplicationTestAccess::view(app);
    const engine::ScreenPoint target = view.layout.screenPoint({6, 4});
    app.pointer({engine::PointerPhase::Move, 0, {0, 0}});
    require(app.frame(1200, 800, 1, 0), "render selected-card affordances without a cursor overlay");
    const engine::Color originalCenter = pixel(app.renderOutput(), target);
    app.pointer({engine::PointerPhase::Move, 0, target});
    require(app.frame(1200, 800, 1, 0) && view.currentPlacement() &&
                view.currentPlacement()->rule.allowed() &&
                view.currentPlacement()->rule.action == game::PlayAction::Upgrade &&
                view.currentPlacement()->rule.cost == 2,
            "rendered cursor intent contains the actual upgrade action and price");
    const engine::ScreenPoint ring{target.x + view.layout.tile * .42F, target.y};
    const engine::Color green = pixel(app.renderOutput(), ring);
    require(green.r == presentation::theme::green.r && green.g == presentation::theme::green.g,
            "actual marker pixels are green for the currently valid action");
    const engine::Color markedCenter = pixel(app.renderOutput(), target);
    require(markedCenter.r == originalCenter.r && markedCenter.g == originalCenter.g &&
                markedCenter.b == originalCenter.b,
            "the placement ring preserves the building art instead of painting over its center");
    game::scenario(run).energy = 1;
    require(app.frame(1200, 800, 1, 0) &&
                view.currentPlacement()->rule.failure == game::PlayFailure::EnergyShortage,
            "rendered intent updates when energy changes without another pointer event");
    const engine::Color red = pixel(app.renderOutput(), ring);
    require(red.r == presentation::theme::red.r && red.g == presentation::theme::red.g,
            "actual marker pixels turn red for a rejected action");
    app.act({engine::Interaction::Kind::Click, presentation::World, target});
    require(run.state().energy == 1 && run.state().towers.front().level == 1 &&
                run.state().deck[run.state().hand[0]].id == 1,
            "a previously green cursor cannot commit after its energy becomes insufficient");
    game::scenario(run).energy = 2;
    app.act({engine::Interaction::Kind::Click, presentation::Pause, {}});
    require(app.frame(1200, 800, 1, 0) && !view.currentPlacement() && run.previewPlay(1, {6, 4}).allowed(),
            "app pause hides actionable hints while the domain query remains independent of screen state");
    app.act({engine::Interaction::Kind::Click, presentation::Resume, {}});
    app.act({engine::Interaction::Kind::Click, presentation::OpenSettings, {}});
    require(app.frame(1200, 800, 1, 0) && !view.currentPlacement(),
            "utility screens cannot retain old battlefield hints");
    app.act({engine::Interaction::Kind::Click, presentation::NavigateBack, {}});
    app.viewportChanged();
    require(app.frame(320, 568, 1, 0), "compact battlefield remeasures its placement geometry");
    app.pointer({engine::PointerPhase::Move, 0, view.layout.screenPoint({6, 4})});
    require(app.frame(320, 568, 1, 0) && view.currentPlacement() && view.currentPlacement()->rule.cost == 2,
            "320px layout retains effective target cost and renders its bounded hint");
}
/** Keep energy beside every card and draw actual attack/effect radii before any card is committed. */
void energyAndRangeUiContracts() {
    Application app;
    app.act({engine::Interaction::Kind::Click, presentation::HomeNew, {}});
    app.act({engine::Interaction::Kind::Click, presentation::Start, {}});
    const presentation::View &view = ApplicationTestAccess::view(app);
    for (const std::array<int, 2> size :
         {std::array<int, 2>{1200, 900}, {980, 739}, {390, 844}, {320, 568}}) {
        app.viewportChanged();
        require(app.frame(size[0], size[1], 1, 0), "render responsive cards and adjacent energy");
        const engine::Rect bar = view.layout.energyBar;
        require(bar.x >= 0 && bar.y + bar.h <= size[1] && bar.x + bar.w <= size[0] &&
                    view.layout.energyMeter.w > 0,
                "energy status and regeneration meter fit the viewport down to 320px");
        for (game::CardId id = 1; id <= 4; ++id) {
            const engine::Rect card = bounds(app, presentation::CardBase + id);
            require(card.y + card.h <= bar.y && bar.y - (card.y + card.h) <= 16,
                    "all four cards sit immediately above energy without intercepting the meter");
        }
    }
    game::Run &run = ApplicationTestAccess::run(app);
    game::scenario(run).energy = 2;
    require(app.frame(320, 568, 1, 0), "render the two-energy balance below the hand");
    const engine::Rect meter = view.layout.energyMeter;
    const engine::ScreenPoint fill{meter.x + meter.w * .15F, meter.y + meter.h / 2};
    const engine::Color charged = pixel(app.renderOutput(), fill);
    require(charged.r == presentation::theme::blue.r && charged.g == presentation::theme::blue.g,
            "the actual lower meter displays available energy");
    game::scenario(run).energy = 1;
    require(app.frame(320, 568, 1, 0), "update the lower meter after spending energy");
    const engine::Color spent = pixel(app.renderOutput(), fill);
    require(spent.r != charged.r && spent.g != charged.g, "spent energy disappears from the adjacent meter");

    game::scenario(run).energy = 6;
    require(app.frame(1200, 900, 1, 0), "render a stable battlefield for real drag preview");
    const engine::ScreenPoint target = view.layout.screenPoint({6, 4});
    const float radius = run.catalog().card(game::CardKind::Gun).range;
    const engine::ScreenPoint edge{target.x, target.y + radius * view.layout.tile};
    const engine::Color before = pixel(app.renderOutput(), edge);
    const engine::ScreenPoint card = bounds(app, presentation::CardBase + 1).center();
    app.pointer({engine::PointerPhase::Down, 0, card});
    app.pointer({engine::PointerPhase::Move, 0, target});
    require(app.frame(1200, 900, 1, 0) && view.currentPlacement() &&
                view.currentPlacement()->range == radius && run.state().towers.empty() &&
                run.state().energy == 6,
            "a captured card drag previews its authored range before spending or placing");
    const engine::Color preview = pixel(app.renderOutput(), edge);
    require(preview.r != before.r || preview.g != before.g || preview.b != before.b,
            "real range outline pixels are visible before pointer release");
    app.pointer({engine::PointerPhase::Up, 0, target});
    require(app.frame(1200, 900, 1, 0) && !view.currentPlacement() && run.state().towers.size() == 1,
            "drop commits once and retires the pre-placement range");
    app.act({engine::Interaction::Kind::Click, presentation::World, target});
    require(app.frame(1200, 900, 1, 0), "render the placed tower selection");
    const engine::Color selected = pixel(app.renderOutput(), edge);
    require(selected.r > selected.g, "the same radius is shown by the placed tower selection");

    for (const game::CardId id : {2U, 4U}) {
        const game::Point position = id == 2 ? game::Point{8, 4} : game::Point{6.25F, 4.25F};
        const engine::ScreenPoint destination = view.layout.screenPoint(position);
        app.pointer({engine::PointerPhase::Down, 0, bounds(app, presentation::CardBase + id).center()});
        app.pointer({engine::PointerPhase::Move, 0, destination});
        require(app.frame(1200, 900, 1, 0) && view.currentPlacement() &&
                    view.currentPlacement()->range == run.catalog().card(run.card(id)->kind).range &&
                    game::distance(view.currentPlacement()->target, position) < .0001F,
                "unit and fractional spell drags show their own attack/effect radius at the actual target");
        app.pointer({engine::PointerPhase::Cancel, 0, destination});
        require(app.frame(1200, 900, 1, 0) && !view.currentPlacement(),
                "cancelled drag removes its range overlay");
    }
}
/** Read one displayed stat by its label's string key rather than relying on a particular grid cell. */
std::string statValue(const presentation::CardDetails &details, const std::string &key) {
    const std::string label = presentation::tr(key);
    for (const presentation::CardStat &stat : details.stats)
        if (stat.label == label)
            return stat.value;
    return {};
}
/** Format one number with a stat value unit template. */
std::string valueText(const char *key, const std::string &value) {
    return presentation::tr(key, {{"value", value}});
}
/** Hold a real current control with touch input while the app advances its independent UI clock. */
void holdCard(Application &app, engine::WidgetId id, int width, int height) {
    const engine::ScreenPoint point = bounds(app, id).center();
    app.pointer({engine::PointerPhase::Down, 42, point, engine::PointerDevice::Touch});
    for (int tick = 0; tick < 6; ++tick)
        require(app.frame(width, height, 1, .1F), "advance a stationary card inspection hold");
    app.pointer({engine::PointerPhase::Up, 42, point, engine::PointerDevice::Touch});
    require(app.frame(width, height, 1, 0), "render the inspect-only release");
}
/** Click or dismiss using actual normalized pointer events rather than bypassing the UI. */
void tap(Application &app, engine::ScreenPoint point) {
    app.pointer({engine::PointerPhase::Down, 42, point, engine::PointerDevice::Touch});
    app.pointer({engine::PointerPhase::Up, 42, point, engine::PointerDevice::Touch});
}
/** Check unique registrations and nonoverlapping actionable controls, excluding the canvas surface. */
void requireControlLayout(const Application &app, int width, int height) {
    const std::vector<engine::Widget> &widgets = app.widgets();
    for (std::size_t index = 0; index < widgets.size(); ++index) {
        const engine::Widget &a = widgets[index];
        for (std::size_t next = index + 1; next < widgets.size(); ++next) {
            const engine::Widget &b = widgets[next];
            require(a.id != b.id, "each rendered control has a unique identity for pointer release");
            if (a.enabled && a.focusable && b.enabled && b.focusable) {
                const bool separate = a.bounds.x + a.bounds.w <= b.bounds.x + .5F ||
                                      b.bounds.x + b.bounds.w <= a.bounds.x + .5F ||
                                      a.bounds.y + a.bounds.h <= b.bounds.y + .5F ||
                                      b.bounds.y + b.bounds.h <= a.bounds.y + .5F;
                if (!separate)
                    std::cerr << "overlapping controls: " << a.id << '/' << b.id << '\n';
                require(separate, "responsive action buttons do not intercept one another");
            }
        }
        if (a.enabled && a.focusable)
            require(a.bounds.x >= 0 && a.bounds.y >= 0 && a.bounds.x + a.bounds.w <= width + 1 &&
                        a.bounds.y + a.bounds.h <= height + 1,
                    "responsive controls fit the logical viewport");
    }
}
/** Preserve redesigned briefing controls, input occlusion and dense mobile selection layouts. */
void designIntegrationContracts() {
    for (const std::array<int, 2> size : {std::array<int, 2>{1200, 900}, {390, 844}, {320, 568}}) {
        Application app;
        app.act({engine::Interaction::Kind::Click, presentation::HomeNew, {}});
        require(app.frame(size[0], size[1], 1, 0), "render the redesigned initial briefing");
        requireControlLayout(app, size[0], size[1]);
        const engine::Rect briefing = bounds(app, presentation::GuideBackdrop);
        require(briefing.y + briefing.h <= ApplicationTestAccess::view(app).layout.hand.y - 8,
                "the briefing leaves the elevated deck tabs unobstructed on short screens");
        const std::string initial = app.game().debugState();
        if (size[0] < 980) {
            const engine::Rect start = bounds(app, presentation::Start);
            tap(app, {start.x + 4, start.y + start.h / 2});
            require(app.frame(size[0], size[1], 1, 0) && app.game().debugState() == initial,
                    "briefing content blocks clicks on the underlying wave-start control");
        }
        tap(app, bounds(app, presentation::CloseDrawer).center());
        require(app.frame(size[0], size[1], 1, 0) &&
                    app.debugState().find("\"guideOpen\":false") != std::string::npos &&
                    app.game().debugState() == initial,
                "the briefing X closes without changing the run");
        tap(app, bounds(app, presentation::ToggleDrawer).center());
        require(app.frame(size[0], size[1], 1, 0) &&
                    app.debugState().find("\"guideOpen\":true") != std::string::npos,
                "the briefing tab reopens through actual pointer input");
        const presentation::Action close =
            size[0] < 980 ? presentation::AcknowledgeDrawer : presentation::CloseDrawer;
        tap(app, bounds(app, close).center());
        require(app.frame(size[0], size[1], 1, 0) && app.game().debugState() == initial,
                "acknowledging the briefing only dismisses its panel");
        tap(app, bounds(app, presentation::Start).center());
        require(app.frame(size[0], size[1], 1, 0) && app.game().state().phase == game::Phase::Battle,
                "wave start remains actionable after briefing dismissal");
        tap(app, bounds(app, presentation::CardBase + 1).center());
        require(app.frame(size[0], size[1], 1, 0), "render selected card controls");
        requireControlLayout(app, size[0], size[1]);
        const engine::ScreenPoint pad = ApplicationTestAccess::view(app).layout.screenPoint({6, 4});
        tap(app, pad);
        require(app.frame(size[0], size[1], 1, 0) && app.game().state().towers.size() == 1,
                "card selection and deployment remain playable after the visual redesign");
        tap(app, pad);
        require(app.frame(size[0], size[1], 1, 0), "render deployed building controls");
        requireControlLayout(app, size[0], size[1]);
        require(bounds(app, presentation::Dismiss).h > 0,
                "the deployed building retains a usable withdrawal control");
    }
}
/** Keep tutorial setup interactive while preserving simulation, regular-run and utility-screen lifetimes. */
void tutorialApplicationContracts() {
    Application app;
    const std::string regular = app.game().debugState();
    app.act({engine::Interaction::Kind::Click, presentation::BeginTutorial, {}});
    for (const std::array<int, 2> size :
         {std::array<int, 2>{1200, 900}, {980, 739}, {390, 844}, {320, 568}}) {
        app.viewportChanged();
        require(app.frame(size[0], size[1], 1, .1F), "render initial practice at responsive sizes");
        requireControlLayout(app, size[0], size[1]);
        require(app.game().state().battleTime == 0 && app.game().state().energy == 6 &&
                    app.frameTimings().simulationSteps == 0,
                "the lesson freezes simulation without using manual pause");
    }
    app.key(' ', false);
    require(app.frame(320, 568, 1, .1F), "acknowledge practice with the keyboard");
    app.act({engine::Interaction::Kind::Click, presentation::TutorialRegular, {}});
    require(app.game().state().phase == game::Phase::Battle &&
                app.debugState().find("\"tutorial\":{\"active\":true") != std::string::npos,
            "a stale completion action cannot exit an unfinished lesson");
    const engine::ScreenPoint card = bounds(app, presentation::CardBase + 1).center();
    const engine::ScreenPoint target = ApplicationTestAccess::view(app).layout.screenPoint({6, 4});
    app.pointer({engine::PointerPhase::Down, 7, card});
    app.pointer({engine::PointerPhase::Move, 7, target});
    require(app.frame(320, 568, 1, .1F) && ApplicationTestAccess::view(app).currentPlacement() &&
                app.game().state().battleTime == 0,
            "actual drag and range preview work while practice simulation is frozen");
    app.pointer({engine::PointerPhase::Up, 7, target});
    require(app.frame(320, 568, 1, 0) && app.game().state().towers.size() == 1 &&
                app.game().state().energy == 3,
            "dropping the practice card commits exactly one real cost");
    app.act({engine::Interaction::Kind::Click, presentation::Start, {}});
    require(app.game().state().wave == 1, "the normal start command cannot bypass a practice lesson");
    app.act({engine::Interaction::Kind::Click, presentation::OpenSettings, {}});
    app.act({engine::Interaction::Kind::Click, presentation::ToggleMotion, {}});
    require(app.frame(320, 568, 1, .1F) && app.game().state().battleTime == 0,
            "settings preserves the current lesson and frozen battle clock");
    app.act({engine::Interaction::Kind::Click, presentation::NavigateBack, {}});
    app.suspend();
    require(app.frame(320, 568, 1, 0), "practice resumes after focus loss");
    tap(app, bounds(app, presentation::Resume).center());
    require(app.frame(320, 568, 1, 0), "resume control is available even outside combat lessons");
    app.act({engine::Interaction::Kind::Click, presentation::TutorialRetry, {}});
    require(app.game().state().towers.size() == 1 && app.game().state().energy == 3,
            "retrying the unit lesson retains its completed building checkpoint");
    app.act({engine::Interaction::Kind::Click, presentation::GoHome, {}});
    require(app.game().debugState() == regular &&
                app.debugState().find("\"tutorial\":{\"active\":false") != std::string::npos,
            "leaving practice restores the untouched regular run instead of carrying practice rewards");
    app.act({engine::Interaction::Kind::Click, presentation::HomeNew, {}});
    require(app.game().state().phase == game::Phase::Ready && app.game().state().towers.empty() &&
                app.game().catalog().schedule(1).size() == 16,
            "a normal expedition still starts from the ordinary deck and three-wave catalog");
}
/** Share real card data across hover, touch, recruitment and expansion without accidental payment. */
void cardInformationContracts() {
    game::Catalog custom = game::defaultCatalog();
    custom.cards[0].category = game::CardCategory::Spell;
    custom.cards[0].cost = 7;
    custom.cards[0].range = 2.75F;
    custom.cards[0].damage = 47;
    Application app(game::makeRun(custom));
    app.act({engine::Interaction::Kind::Click, presentation::HomeNew, {}});
    require(app.frame(1200, 900, 1, 0), "render a custom hand for inspection");
    const presentation::View &view = ApplicationTestAccess::view(app);
    app.pointer({engine::PointerPhase::Move, 0, bounds(app, presentation::CardBase + 1).center()});
    const std::string before = app.game().debugState();
    for (int tick = 0; tick < 5; ++tick)
        require(app.frame(1200, 900, 1, .1F), "advance mouse hover time");
    require(view.currentCardInspection() && view.currentCardInspection()->details.cost == 7 &&
                view.currentCardInspection()->details.category == game::CardCategory::Spell &&
                statValue(view.currentCardInspection()->details, "carddetail.stat.area") ==
                    valueText("carddetail.value.tiles", "2.75") &&
                statValue(view.currentCardInspection()->details, "carddetail.stat.damage") == "47" &&
                statValue(view.currentCardInspection()->details, "carddetail.stat.interval").empty() &&
                app.game().debugState() == before,
            "hover shows the actual catalog category and stats without changing the run");
    for (const std::array<int, 2> size : {std::array<int, 2>{390, 844}, {320, 568}}) {
        app.viewportChanged();
        require(app.frame(size[0], size[1], 1, 0), "rebuild card information at a compact viewport");
        holdCard(app, presentation::CardBase + 2, size[0], size[1]);
        require(
            view.currentCardInspection() &&
                statValue(view.currentCardInspection()->details, "carddetail.stat.hp") == "120" &&
                statValue(view.currentCardInspection()->details, "carddetail.stat.block") ==
                    valueText("carddetail.value.count", "2") &&
                view.currentCardInspection()->bounds.x >= 0 && view.currentCardInspection()->bounds.y >= 0 &&
                view.currentCardInspection()->bounds.y + view.currentCardInspection()->bounds.h <= size[1] &&
                app.game().debugState() == before &&
                app.debugState().find("\"selectedCard\":0") != std::string::npos,
            "touch details show unit stats within the viewport and release never selects or spends");
        tap(app, {1, 1});
        require(app.frame(size[0], size[1], 1, 0) && !view.currentCardInspection(),
                "an outside tap dismisses pinned inspection");
    }

    game::Run &run = ApplicationTestAccess::run(app);
    game::enterPreparation(run);
    game::scenario(run).gold = 4;
    app.act({engine::Interaction::Kind::Click, presentation::Recruit, {}});
    require(app.frame(320, 568, 1, 0), "render recruitment candidates with compact base statistics");
    holdCard(app, presentation::CandidateBase + static_cast<int>(game::CardKind::Shooter), 320, 568);
    require(view.currentCardInspection() &&
                view.currentCardInspection()->details.kind == game::CardKind::Shooter &&
                !view.currentOfferCard() && run.state().gold == 4,
            "holding a recruitment candidate inspects without accepting it");
    tap(app, {1, 1});
    tap(app, bounds(app, presentation::CandidateBase + static_cast<int>(game::CardKind::Shooter)).center());
    require(app.frame(320, 568, 1, 0) && view.currentOfferCard() &&
                view.currentOfferCard()->kind == game::CardKind::Shooter &&
                statValue(*view.currentOfferCard(), "carddetail.stat.attack") == "12" &&
                run.state().gold == 4,
            "replacement always displays the incoming portrait data and performance before charging");
    const std::string offerBefore = run.debugState();
    holdCard(app, presentation::DeckBase + 1, 320, 568);
    require(view.currentCardInspection() && view.currentCardInspection()->details.cost == 7 &&
                run.debugState() == offerBefore && view.currentOfferCard(),
            "holding an existing card compares its stats while preserving incoming card and payment");
    tap(app, {1, 1});
    tap(app, bounds(app, presentation::DeckBase + 1).center());
    require(app.frame(320, 568, 1, 0) && !view.currentOfferCard() && !view.currentCardInspection() &&
                run.state().gold == 0 && run.state().deck[0].kind == game::CardKind::Shooter,
            "only a deliberate replacement tap consumes the offer and its price");
    game::enterPreparation(run, 2);
    require(run.expand(1).ok && app.frame(320, 568, 1, 0) && view.currentOfferCard() &&
                view.currentOfferCard()->kind == game::CardKind::Lightning &&
                statValue(*view.currentOfferCard(), "carddetail.stat.chains") ==
                    valueText("carddetail.value.count", "3"),
            "expansion reward shows its actual card art and chaining performance before deck replacement");
    const presentation::CardDetails frost = presentation::cardDetails(run.catalog(), game::CardKind::Frost);
    require(statValue(frost, "carddetail.stat.slow") == "40%" &&
                statValue(frost, "carddetail.stat.duration") == valueText("carddetail.value.seconds", "3") &&
                statValue(frost, "carddetail.stat.attack").empty(),
            "control spells describe their real effect instead of pretending to be attack towers");
}
/** Verify continuous cost coverage, clockwise pixels and the same prices used by actual placement rules. */
void cardChargeContracts() {
    game::Catalog content = game::defaultCatalog();
    content.balance.initialEnergy = 0;
    Application app(game::makeRun(content));
    app.act({engine::Interaction::Kind::Click, presentation::HomeNew, {}});
    app.act({engine::Interaction::Kind::Click, presentation::Start, {}});
    game::Run &run = ApplicationTestAccess::run(app);
    const presentation::View &view = ApplicationTestAccess::view(app);
    require(app.frame(1200, 900, 1, 0) &&
                view.currentHandCharge()[0].mode == presentation::CardChargeMode::Charging &&
                view.currentHandCharge()[0].fraction == 0,
            "an unaffordable current hand card starts empty without an independent countdown");
    const engine::Rect card = bounds(app, presentation::CardBase + 1);
    const engine::ScreenPoint center{card.x + 17, card.y + 17};
    game::scenario(run).energyTime = run.catalog().balance.energyInterval * .75F;
    require(app.frame(1200, 900, 1, 0) && std::abs(view.currentHandCharge()[0].fraction - .25F) < .0001F,
            "three-cost card coverage includes fractional regeneration before the first whole energy");
    const float diagonal = 15 / std::sqrt(2.0F);
    const engine::Color clockwise = pixel(app.renderOutput(), {center.x + diagonal, center.y - diagonal});
    const engine::Color counterclockwise =
        pixel(app.renderOutput(), {center.x - diagonal, center.y - diagonal});
    require(clockwise.r == presentation::theme::blue.r && clockwise.g == presentation::theme::blue.g &&
                counterclockwise.r != presentation::theme::blue.r,
            "actual cost-badge pixels fill the northeast quadrant clockwise from twelve o'clock");
    game::scenario(run).energy = 1;
    game::scenario(run).energyTime = run.catalog().balance.energyInterval * .5F;
    require(app.frame(1200, 900, 1, 0) && std::abs(view.currentHandCharge()[0].fraction - .5F) < .0001F,
            "coverage continues across whole-energy increments rather than restarting each tick");
    for (const std::array<int, 2> size : {std::array<int, 2>{390, 844}, {320, 568}}) {
        app.viewportChanged();
        require(app.frame(size[0], size[1], 1, 0) &&
                    std::abs(view.currentHandCharge()[0].fraction - .5F) < .0001F,
                "compact cost progress uses the same authoritative energy fraction");
    }
    game::scenario(run).energy = 3;
    game::scenario(run).energyTime = 0;
    require(app.frame(320, 568, 1, 0) &&
                view.currentHandCharge()[0].mode == presentation::CardChargeMode::Ready &&
                view.currentHandCharge()[0].fraction == 1,
            "the charge indicator becomes ready exactly when actual energy covers its cost");

    game::scenario(run).towers.push_back({700, game::CardKind::Gun, {6, 4}});
    game::scenario(run).energy = 1;
    game::scenario(run).energyTime = run.catalog().balance.energyInterval * .5F;
    require(app.frame(1200, 900, 1, 0) && view.currentHandCharge()[0].cost == 2 &&
                std::abs(view.currentHandCharge()[0].fraction - .75F) < .0001F,
            "hand progress uses the cheaper available tower-upgrade price");
    app.act({engine::Interaction::Kind::Click, presentation::CardBase + 1, {}});
    app.pointer({engine::PointerPhase::Move, 0, view.layout.screenPoint({9, 4})});
    require(app.frame(1200, 900, 1, 0) && view.currentHandCharge()[0].cost == 3 &&
                std::abs(view.currentHandCharge()[0].fraction - .5F) < .0001F,
            "hovering an empty pad switches the same card to its actual installation cost");
    app.pointer({engine::PointerPhase::Move, 0, view.layout.screenPoint({5, 5})});
    require(app.frame(1200, 900, 1, 0) &&
                view.currentHandCharge()[0].mode == presentation::CardChargeMode::Blocked,
            "an invalid target is not presented as a renewable energy wait");
    app.pointer({engine::PointerPhase::Move, 0, {0, 0}});
    app.act({engine::Interaction::Kind::Click, presentation::Pause, {}});
    require(app.frame(1200, 900, 1, .1F) && std::abs(view.currentHandCharge()[0].fraction - .75F) < .0001F,
            "manual pause freezes regeneration coverage instead of letting a UI timer advance");
    app.act({engine::Interaction::Kind::Click, presentation::Resume, {}});
    app.act({engine::Interaction::Kind::Click, presentation::ToggleMotion, {}});
    require(app.frame(1200, 900, 1, .1F) && view.currentHandCharge()[0].fraction > .75F,
            "reduced decorative motion does not hide actual energy regeneration progress");

    game::Catalog capped = content;
    capped.balance.energyCap = 2;
    game::Run unreachable = game::makeRun(capped);
    unreachable.startWave();
    require(presentation::cardCharge(unreachable, 1, 3, unreachable.playAvailability(1).failure).mode ==
                presentation::CardChargeMode::Blocked,
            "a card above the energy cap never promises a countdown that can finish");
    game::Catalog free = content;
    free.cards[0].cost = 0;
    game::Run zero = game::makeRun(free);
    zero.startWave();
    require(presentation::cardCharge(zero, 1, 0, zero.playAvailability(1).failure).fraction == 1,
            "free cards are ready without dividing by zero");
    game::Catalog smallSquad = game::defaultCatalog();
    smallSquad.balance.initialSlots = 1;
    game::Run full = game::makeRun(smallSquad);
    full.startWave();
    require(full.play(2, {8, 4}).ok && full.exchange(1).ok, "fill the one-unit squad");
    game::scenario(full).units.front().level = game::CardMaxLevel;
    require(presentation::cardCharge(full, 6, 3, full.playAvailability(6).failure).mode ==
                presentation::CardChargeMode::Blocked,
            "full unit slots remain a placement block even when energy is also insufficient");
}
/** Exercise descent confirmation and scaled placement through the actual responsive application view. */
void campaignUiContracts() {
    Application app(game::makeRun(game::stageCatalog(3)));
    app.act({engine::Interaction::Kind::Click, presentation::HomeNew, {}});
    game::Run &run = ApplicationTestAccess::run(app);
    game::enterPreparation(run, 3);
    game::scenario(run).tool = 2;
    game::scenario(run).digs = 2;
    game::scenario(run).scans = 0;
    app.act({engine::Interaction::Kind::Click, presentation::Mine, {}});
    require(app.frame(320, 568, 1, 0), "deep mine renders at the minimum touch viewport");
    require(ApplicationTestAccess::view(app).layout.mineTile >= 24,
            "small mobile cutaways retain tappable tiles instead of compressing below ten pixels");
    for (const engine::Widget &a : app.widgets()) {
        if (!a.enabled || !a.focusable)
            continue;
        for (const engine::Widget &b : app.widgets()) {
            if (a.id >= b.id || !b.enabled || !b.focusable)
                continue;
            require(a.bounds.x + a.bounds.w <= b.bounds.x + .5F ||
                        b.bounds.x + b.bounds.w <= a.bounds.x + .5F ||
                        a.bounds.y + a.bounds.h <= b.bounds.y + .5F ||
                        b.bounds.y + b.bounds.h <= a.bounds.y + .5F,
                    "compressed mine controls stay separate from each other");
        }
    }
    const engine::Rect entry = bounds(app, presentation::DescendMine);
    require(entry.w > 0 && entry.x >= 0 && entry.x + entry.w <= 320,
            "descent entry stays within the mine header");
    app.act({engine::Interaction::Kind::Click, presentation::DescendMine, {}});
    require(app.frame(320, 568, 1, 0) && bounds(app, presentation::ConfirmDescend).w > 0,
            "descent requires a reviewable one-way confirmation");
    app.act({engine::Interaction::Kind::Click, presentation::Close, {}});
    require(run.state().mineLayer == 0 && run.state().digs == 2,
            "canceling descent leaves the current layer and budget intact");
    app.act({engine::Interaction::Kind::Click, presentation::DescendMine, {}});
    require(app.frame(320, 568, 1, 0), "confirmed descent controls render before input");
    const engine::ScreenPoint confirm = bounds(app, presentation::ConfirmDescend).center();
    app.pointer({engine::PointerPhase::Down, 17, confirm, engine::PointerDevice::Touch});
    app.pointer({engine::PointerPhase::Up, 17, confirm, engine::PointerDevice::Touch});
    require(run.state().mineLayer == 1 && run.state().digs == 2 && run.state().scans == 0,
            "actual touch confirmation switches layers without refilling actions");
    app.act({engine::Interaction::Kind::Click, presentation::Surface, {}});
    require(app.frame(1200, 900, 1, 0), "large authored battlefield renders after returning to the surface");
    app.act({engine::Interaction::Kind::Click, presentation::ZoomIn, {}});
    require(app.frame(1200, 900, 1, 0), "zoom updates the shared projection before placement");
    const presentation::Layout &layout = ApplicationTestAccess::view(app).layout;
    require(layout.camera.pose().zoom > 1 && layout.tile > 1 &&
                layout.surfacePoint(layout.screenPoint({20, 14}), true).x == 20,
            "enlarged field input and rendering use the same tile centers");
}
/** Store a campaign expedition between waves, continue it after a restart, and drop it once it ends or when
 * the stored text does not match. */
void runSaveContracts() {
    Persistence::clearRun();
    std::string played;
    {
        Application app;
        require(app.debugState().find("\"hasRun\":false") != std::string::npos &&
                    Persistence::loadRun().empty(),
                "a first launch has nothing to continue");
        app.act({engine::Interaction::Kind::Click, presentation::HomeNew, {}});
        // Frames clamp their elapsed time to 0.1 s, so the half-second check needs six of them.
        for (int frame = 0; frame < 6; ++frame)
            require(app.frame(1200, 900, 1, .1F), "render a new expedition");
        require(Persistence::loadRun().rfind("stage 1 ", 0) == 0,
                "a new expedition is stored before its first wave");
        game::Run &run = ApplicationTestAccess::run(app);
        require(run.startWave().ok && app.frame(1200, 900, 1, 0), "start the first wave");
        const std::string ready = Persistence::loadRun();
        for (int step = 0; step < 6000 && run.state().phase == game::Phase::Battle; ++step) {
            if (step % 5 == 0)
                game::autoplay::playCard(run);
            run.advance(.1F);
        }
        require(run.state().phase == game::Phase::Prep, "the first wave ends in preparation");
        require(Persistence::loadRun() == ready, "a battle keeps the stored state from before the wave");
        require(app.frame(1200, 900, 1, 0), "render preparation");
        require(Persistence::loadRun() != ready && Persistence::loadRun().find("\n") != std::string::npos,
                "entering preparation stores the expedition at once");
        played = run.debugState();
    }
    {
        Application app;
        require(ApplicationTestAccess::run(app).debugState() == played &&
                    app.debugState().find("\"hasRun\":true") != std::string::npos &&
                    app.debugState().find("\"screen\":0") != std::string::npos,
                "a restart opens the title with the stored expedition ready to continue");
        app.act({engine::Interaction::Kind::Click, presentation::HomeContinue, {}});
        require(app.frame(1200, 900, 1, 0) && app.debugState().find("\"screen\":1") != std::string::npos &&
                    ApplicationTestAccess::run(app).state().phase == game::Phase::Prep,
                "continue enters the restored preparation");
        game::scenario(ApplicationTestAccess::run(app)).phase = game::Phase::Won;
        require(app.frame(1200, 900, 1, 0) && Persistence::loadRun().empty(),
                "an ended expedition is removed");
    }
    for (const char *stored :
         {"stage 1 1 5\nTHORIUM-RUN 1 1 5 8 0 broken", "stage 4 1 5\nTHORIUM-RUN", "garbage"}) {
        Persistence::saveRun(stored);
        Application app;
        require(app.debugState().find("\"hasRun\":false") != std::string::npos &&
                    Persistence::loadRun().empty(),
                "a damaged, locked or unknown stored expedition is discarded");
    }
}
} // namespace
/** Exercise composition, actual ThorVG UI pixels and timing boundaries without SDL. */
int main() {
    char home[] = "/tmp/thorium-app-test-XXXXXX";
    require(mkdtemp(home) != nullptr && setenv("HOME", home, 1) == 0, "isolated save directory per run");
    require(SaveData{}.volume == engine::AudioSettings{}.volume &&
                SaveData{}.muted == engine::AudioSettings{}.muted &&
                SaveData{}.reducedMotion == presentation::Preferences{}.reducedMotion &&
                SaveData{}.damageNumbers == presentation::Preferences{}.damageNumbers &&
                SaveData{}.cachedAnimations == presentation::Preferences{}.cachedAnimations &&
                SaveData{}.language == presentation::Preferences{}.language,
            "fresh saved settings match the live settings reset contract");
    applicationContracts();
    campaignUiContracts();
    offerUiContracts();
    terminalRestartContracts();
    placementUiContracts();
    energyAndRangeUiContracts();
    cardInformationContracts();
    cardChargeContracts();
    designIntegrationContracts();
    tutorialApplicationContracts();
    runSaveContracts();
    Persistence::clearRun();
    std::cout << checks << " application integration checks passed\n";
}
