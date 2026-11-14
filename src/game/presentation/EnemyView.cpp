#include "Theme.h"
#include "View.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace thorium::presentation {
using engine::Color;
using engine::Rect;
using engine::ScreenPoint;
namespace {
/** First renderer instance ID reserved for HUD enemy portraits, far above simulation entity IDs. */
constexpr std::uint32_t PortraitIds = 0xFFFF0000U;
/** Drawn body size of each role in tiles, indexed by EnemyKind. */
constexpr std::array<float, game::EnemyKindCount> BodyScale{
    {.85F, .8F, 1.0F, .58F, .9F, .86F, .95F, .88F, .84F, .72F, .86F, .9F, .96F, .84F, .82F, 1.9F}};
/** Height in tiles a flying role hovers above its ground shadow, bobbing gently. */
float hover(const game::EnemyDefinition &role, float time, game::EntityId id) {
    return role.flying ? .5F + .08F * std::sin(time * 5 + static_cast<float>(id)) : 0.0F;
}
/** Return one role's drawn body size in tiles. */
float bodyScale(game::EnemyKind kind) {
    return BodyScale.at(static_cast<std::size_t>(kind));
}
/** Apply a bounded opacity without changing the palette's base alpha. */
Color fade(Color color, float opacity) {
    color.a = static_cast<std::uint8_t>(color.a * std::clamp(opacity, 0.0F, 1.0F));
    return color;
}
/** Fill an axis-aligned ellipse with a polygon approximation. */
void ellipse(engine::Renderer &g, ScreenPoint center, float rx, float ry, Color color) {
    std::vector<ScreenPoint> points;
    points.reserve(20);
    for (int index = 0; index < 20; ++index) {
        const float angle = index * 6.2831853F / 20;
        points.push_back({center.x + std::cos(angle) * rx, center.y + std::sin(angle) * ry});
    }
    g.polygon(points, color);
}
/** Stroke a closed axis-aligned ellipse outline. */
void ellipseOutline(engine::Renderer &g, ScreenPoint center, float rx, float ry, float width, Color color) {
    std::vector<ScreenPoint> points;
    points.reserve(33);
    for (int index = 0; index <= 32; ++index) {
        const float angle = index * 6.2831853F / 32;
        points.push_back({center.x + std::cos(angle) * rx, center.y + std::sin(angle) * ry});
    }
    g.line(points, width, color);
}
/** Paint a kite shield whose face dims and cracks as its absorption pool empties. */
void shieldPlate(engine::Renderer &g, ScreenPoint center, float size, float fraction) {
    const float w = size * .2F, h = size * .27F;
    const std::vector<ScreenPoint> outer{{center.x - w, center.y - h},
                                         {center.x + w, center.y - h},
                                         {center.x + w * 1.05F, center.y},
                                         {center.x, center.y + h * 1.15F},
                                         {center.x - w * 1.05F, center.y}};
    g.polygon(outer, {36, 52, 66});
    std::vector<ScreenPoint> inner;
    for (const ScreenPoint &point : outer)
        inner.push_back({center.x + (point.x - center.x) * .78F, center.y + (point.y - center.y) * .78F});
    g.polygon(inner, fraction > .5F ? Color{170, 206, 226} : Color{132, 160, 178});
    g.rect({center.x - w * .12F, center.y - h * .6F, w * .24F, h * 1.3F}, {241, 192, 102}, 1);
    if (fraction <= .5F)
        g.line({{center.x - w * .6F, center.y - h * .5F},
                {center.x - w * .1F, center.y - h * .05F},
                {center.x - w * .4F, center.y + h * .3F}},
               std::max(1.0F, size * .03F), {36, 52, 66});
}
/** Paint the mound above an underground burrower, with its drill tip breaking the soil. */
void dirtMound(engine::Renderer &g, ScreenPoint p, float size, float time) {
    const float bob = std::sin(time * 14) * size * .04F;
    ellipse(g, {p.x, p.y + size * .16F}, size * .42F, size * .17F, {92, 64, 38, 200});
    ellipse(g, {p.x, p.y + size * .1F}, size * .34F, size * .14F, {139, 99, 60});
    ellipse(g, {p.x - size * .08F, p.y + size * .05F}, size * .16F, size * .06F, {176, 133, 84});
    g.polygon({{p.x - size * .1F, p.y + size * .04F + bob},
               {p.x + size * .1F, p.y + size * .04F + bob},
               {p.x, p.y - size * .2F + bob}},
              {190, 196, 206});
    for (int index = 0; index < 3; ++index) {
        const float angle = time * 5 + index * 2.1F;
        g.circle({p.x + std::cos(angle) * size * .36F, p.y + size * .14F + std::sin(angle) * size * .07F},
                 size * .045F, {116, 84, 50});
    }
}
/** Paint a three-point crown whose base is centred on a point, with a dark rim for contrast. */
void crown(engine::Renderer &g, ScreenPoint base, float width, Color color) {
    const float w = width / 2, h = width * .62F;
    const std::vector<ScreenPoint> outline{{base.x - w, base.y},
                                           {base.x - w * 1.05F, base.y - h},
                                           {base.x - w * .45F, base.y - h * .5F},
                                           {base.x, base.y - h * 1.15F},
                                           {base.x + w * .45F, base.y - h * .5F},
                                           {base.x + w * 1.05F, base.y - h},
                                           {base.x + w, base.y}};
    std::vector<ScreenPoint> rim;
    for (const ScreenPoint &point : outline)
        rim.push_back({base.x + (point.x - base.x) * 1.25F, base.y + 1.5F + (point.y - base.y) * 1.2F});
    g.polygon(rim, {24, 20, 28, 230});
    g.polygon(outline, color);
    g.rect({base.x - w, base.y - h * .28F, w * 2, h * .28F}, {255, 236, 170}, 1);
}
/** Stroke a circle outline as alternating dashes. */
void dashedRing(engine::Renderer &g, ScreenPoint center, float radius, float width, Color color,
                float phase) {
    constexpr int Dashes = 16;
    for (int index = 0; index < Dashes; ++index) {
        const float from = (index + phase) * 6.2831853F / Dashes;
        const float to = from + 6.2831853F / Dashes * .55F;
        g.line(
            {{center.x + std::cos(from) * radius, center.y + std::sin(from) * radius},
             {center.x + std::cos((from + to) / 2) * radius, center.y + std::sin((from + to) / 2) * radius},
             {center.x + std::cos(to) * radius, center.y + std::sin(to) * radius}},
            width, color);
    }
}
/** Stroke the part of a circle from twelve o'clock clockwise covering a fraction of the turn. */
void arc(engine::Renderer &g, ScreenPoint center, float radius, float fraction, float width, Color color) {
    const int steps = std::max(2, static_cast<int>(40 * fraction));
    std::vector<ScreenPoint> points;
    for (int index = 0; index <= steps; ++index) {
        const float angle = -1.5707963F + 6.2831853F * fraction * index / steps;
        points.push_back({center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius});
    }
    g.line(points, width, color);
}
/** Map remaining health to a green, amber or red fill. */
Color healthColor(float fraction) {
    return fraction > .5F ? Color{118, 201, 92} : fraction > .25F ? Color{240, 190, 80} : Color{232, 88, 68};
}
/** Format seconds with at most one decimal place. */
std::string seconds(float value) {
    const int tenths = static_cast<int>(std::lround(value * 10));
    return std::to_string(tenths / 10) + (tenths % 10 ? "." + std::to_string(tenths % 10) : "");
}
/** Explain one role's distinguishing rule from its authored numbers. */
std::string roleSummary(const game::Catalog &catalog, game::EnemyKind kind) {
    const game::EnemyDefinition &role = catalog.enemy(kind);
    switch (kind) {
    case game::EnemyKind::Runner:
        return "빠르지만 체력이 낮아요";
    case game::EnemyKind::Armored:
        return "방어력 " + std::to_string(role.armor) + " · 전격 피해는 방어를 무시해요";
    case game::EnemyKind::Swarm:
        return "작고 빠른 무리 · 범위 공격으로 쓸어내세요";
    case game::EnemyKind::Shielded:
        return "방패는 한 번에 " + std::to_string(role.shieldCap) + " 피해만 흡수 · 연사로 깨세요";
    case game::EnemyKind::Healer:
        return seconds(role.abilityInterval) + "초마다 주변 적 +" + std::to_string(role.abilityAmount) +
               " 회복 · 우선 처치하세요";
    case game::EnemyKind::Splitter:
        return "처치하면 " + catalog.enemy(role.minionKind).name + " " + std::to_string(role.splitCount) +
               "마리로 갈라져요";
    case game::EnemyKind::Burrower:
        return "땅속에 숨어 저지·공격을 피하며 빠르게 전진";
    case game::EnemyKind::Sapper:
        return "타워 옆 " + seconds(role.abilityWindup) + "초 폭약 설치 → " + seconds(role.abilityDuration) +
               "초 마비 · 설치 중 처치·저지";
    case game::EnemyKind::Flyer:
        return "날아다녀요 · 저지·포격·폭파병·타르·분출에 안 맞음 · 연사·저격·전격으로";
    case game::EnemyKind::Stalker:
        return seconds(role.stealth) + "칸 안에 와야 조준돼요 · 맞으면 " +
               seconds(catalog.balance.revealTime) + "초 드러남 · 범위 공격으로";
    case game::EnemyKind::Leaper:
        return "처음 막히면 " + seconds(role.abilityWindup) + "초 뒤 " + seconds(role.abilityRadius) +
               "칸 뛰어넘어요 · 저지는 두 겹으로";
    case game::EnemyKind::Wrecker:
        return seconds(role.abilityRadius) + "칸 안 건물 앞에 멈춰 " + std::to_string(role.abilityAmount) +
               "씩 부숴요 · 유닛으로 저지";
    case game::EnemyKind::Slinger:
        return seconds(role.abilityRadius) + "칸 밖에서 건물에 돌을 던져요 · 사거리 밖이면 마법·저격으로";
    case game::EnemyKind::Carrier:
        return "날아와 길 중간에 " + catalog.enemy(role.minionKind).name + " " +
               std::to_string(role.abilityAmount) + "마리를 떨궈요 · 대공으로 먼저";
    case game::EnemyKind::Boss:
        return (role.shield > 0 ? "보호막 " + std::to_string(role.shield) + " · " : std::string()) +
               (role.armor > 3 ? "방어 " + std::to_string(role.armor) + " · " : std::string()) + "타워 " +
               seconds(role.abilityDuration) + "초 기절 · " + catalog.enemy(role.minionKind).name + " " +
               std::to_string(role.abilityAmount) + " 소환 · 기지 피해 " + std::to_string(role.leak);
    default:
        return "기본 적";
    }
}
/** Explain one elite modifier from its authored numbers. */
std::string eliteSummary(const game::Catalog &catalog, game::EliteAffix affix) {
    const game::EliteDefinition &elite = catalog.elite(affix);
    const auto percent = [](float value) -> std::string {
        return std::to_string(static_cast<int>(std::lround(value * 100))) + "%";
    };
    switch (affix) {
    case game::EliteAffix::Steel:
        return "방어력 +" + std::to_string(elite.armor) + " · 전격·포격·저격으로 뚫으세요";
    case game::EliteAffix::Swift:
        return "주변 " + seconds(elite.auraRadius) + "칸 적 이동 속도 +" + percent(elite.auraSpeed - 1) +
               " · 먼저 처치하세요";
    case game::EliteAffix::Regen:
        return "매초 최대 체력 " + percent(elite.regen) + " 회복 · 화력을 모으세요";
    case game::EliteAffix::Enrage:
        return "체력 " + percent(elite.enrageBelow) + " 이하에서 속도 ×" + seconds(elite.enrageSpeed) +
               " · 감속·저지로 붙잡으세요";
    case game::EliteAffix::Leader:
        return "주변 " + seconds(elite.auraRadius) + "칸 적이 받는 피해 -" + percent(elite.auraGuard) +
               " · 우두머리부터 노리세요";
    default:
        return "";
    }
}
/** Drawn size multiplier of an elite carrier. */
float eliteScale(game::EliteAffix affix) {
    return affix == game::EliteAffix::Leader ? 1.35F : affix == game::EliteAffix::None ? 1.0F : 1.1F;
}
/** Battlefield gimmicks a stage introduces with a card before its first wave; Mixed stands for a stage
 *  that combines more than two of them. */
enum class Gimmick { None, Boulder, TraitPad, Tunnel, Vent, Bog, Rail, Tank, Fork, Mixed };
/** List the gimmicks authored in a catalog, in card order. */
std::vector<Gimmick> stageGimmicks(const game::Catalog &catalog) {
    std::vector<Gimmick> result;
    if (!catalog.boulders.empty())
        result.push_back(Gimmick::Boulder);
    if (!catalog.padTraits.empty())
        result.push_back(Gimmick::TraitPad);
    if (!catalog.tunnels.empty())
        result.push_back(Gimmick::Tunnel);
    if (!catalog.vents.empty())
        result.push_back(Gimmick::Vent);
    if (!catalog.bogs.empty())
        result.push_back(Gimmick::Bog);
    if (!catalog.rails.empty())
        result.push_back(Gimmick::Rail);
    if (!catalog.tanks.empty())
        result.push_back(Gimmick::Tank);
    if (!catalog.forks.empty())
        result.push_back(Gimmick::Fork);
    return result;
}
/** Return one gimmick's localized name. */
const char *gimmickName(Gimmick gimmick, const game::Catalog &catalog) {
    return gimmick == Gimmick::Boulder    ? (catalog.rails.empty() ? "낙석 바위" : "수정 군집")
           : gimmick == Gimmick::TraitPad ? "특성 패드"
           : gimmick == Gimmick::Tunnel   ? "갱도 터널"
           : gimmick == Gimmick::Vent     ? "용암 분출구"
           : gimmick == Gimmick::Bog      ? "늪"
           : gimmick == Gimmick::Rail     ? "광차 선로"
           : gimmick == Gimmick::Tank     ? "가스통"
           : gimmick == Gimmick::Fork     ? "갈림길"
                                          : "복합 지형";
}
/** Explain one gimmick's rule from the catalog's authored numbers. */
std::string gimmickSummary(const game::Catalog &catalog, Gimmick gimmick) {
    const game::RunBalance &balance = catalog.balance;
    switch (gimmick) {
    case Gimmick::Boulder:
        return std::string(catalog.rails.empty() ? "패드를 막은 바위" : "패드를 막은 수정") +
               " · 정비 때 눌러서 치워요 · " + game::costLabel(balance.boulder);
    case Gimmick::TraitPad:
        return std::string(padTraitName(game::PadTrait::HighGround)) + " " +
               padTraitEffect(game::PadTrait::HighGround, balance) + " · " +
               padTraitName(game::PadTrait::Crystal) + " " + padTraitEffect(game::PadTrait::Crystal, balance);
    case Gimmick::Tunnel:
        return "안에 든 적은 공격·저지할 수 없고 이동 ×" + seconds(balance.tunnelSpeed);
    case Gimmick::Vent:
        return seconds(balance.ventPeriod) + "초마다 분출 · 반경 안 적과 아군 피해 " +
               std::to_string(balance.ventDamage);
    case Gimmick::Bog:
        return "늪을 건너는 지상 적은 이동 ×" + seconds(balance.bogSpeed) + " · 비행 적은 그대로";
    case Gimmick::Rail:
        return "전투 중 레버를 누르면 광차가 선로 위 지상 적에게 " + std::to_string(balance.cartDamage) +
               " · 재사용 " + seconds(balance.cartCooldown) + "초";
    case Gimmick::Tank:
        return "전투 중 누르면 폭발 · 반경 안 적과 아군 " + std::to_string(balance.tankDamage) +
               " 피해 · 웨이브마다 다시 채워짐";
    case Gimmick::Fork:
        return "정비 중 바리케이드를 눌러 막을 갈래를 골라요 · 우회로는 더 길어요";
    case Gimmick::Mixed: {
        std::string names = "함께 나와요";
        for (Gimmick part : stageGimmicks(catalog))
            if (part != Gimmick::Fork)
                names += " · " + std::string(gimmickName(part, catalog));
        return names;
    }
    case Gimmick::None:
        break;
    }
    return "";
}
/** Draw one gimmick's emblem centred in a square of side `size`. */
void gimmickIcon(engine::Renderer &g, Gimmick gimmick, ScreenPoint centre, float size,
                 game::StageBiome biome) {
    if (gimmick == Gimmick::Boulder && biome == game::StageBiome::Crystal)
        drawCrystalCluster(g, centre, size * .8F);
    else if (gimmick == Gimmick::Boulder)
        drawBoulder(g, centre, size * .8F);
    else if (gimmick == Gimmick::TraitPad) {
        drawPadTraitGlyph(g, game::PadTrait::HighGround, {centre.x - size * .2F, centre.y - size * .08F},
                          size * .46F);
        drawPadTraitGlyph(g, game::PadTrait::Crystal, {centre.x + size * .2F, centre.y + size * .1F},
                          size * .46F);
    } else if (gimmick == Gimmick::Tunnel)
        drawTunnelMouth(g, {centre.x, centre.y - size * .04F}, 0, 1, size * .56F, tunnelColors(biome));
    else if (gimmick == Gimmick::Vent)
        drawVentCrater(g, centre, size * .78F);
    else if (gimmick == Gimmick::Bog) {
        g.circle(centre, size * .38F, {50, 74, 40});
        g.circle({centre.x - size * .04F, centre.y - size * .03F}, size * .28F, {84, 112, 52});
        g.circle({centre.x + size * .12F, centre.y + size * .06F}, size * .07F, {150, 180, 90});
        g.circle({centre.x - size * .14F, centre.y - size * .1F}, size * .05F, {150, 180, 90});
    } else if (gimmick == Gimmick::Rail) {
        drawRail(g, {centre.x - size * .4F, centre.y + size * .14F},
                 {centre.x + size * .4F, centre.y + size * .14F}, size * .4F);
        drawCart(g, {centre.x, centre.y + size * .02F}, 1, 0, size * .62F);
    } else if (gimmick == Gimmick::Tank)
        drawTank(g, centre, size * .72F, false);
    else if (gimmick == Gimmick::Fork) {
        const Color road{196, 170, 120};
        g.line({{centre.x, centre.y + size * .4F},
                {centre.x, centre.y},
                {centre.x - size * .3F, centre.y - size * .36F}},
               size * .14F, road);
        g.line({{centre.x, centre.y}, {centre.x + size * .3F, centre.y - size * .36F}}, size * .14F, road);
        g.line({{centre.x + size * .14F, centre.y - size * .1F},
                {centre.x + size * .4F, centre.y - size * .26F}},
               size * .08F, {214, 74, 54});
    } else if (gimmick == Gimmick::Mixed) {
        drawTunnelMouth(g, {centre.x - size * .14F, centre.y - size * .12F}, 0, 1, size * .4F,
                        tunnelColors(biome));
        drawVentCrater(g, {centre.x + size * .2F, centre.y + size * .16F}, size * .46F);
    }
}
} // namespace

namespace {
/** Enemy visuals back to front: ordinary bodies by screen row, then pack leaders, then the boss. */
std::vector<const ActorVisual *> paintOrder(const State &state) {
    std::vector<const ActorVisual *> order;
    for (const std::pair<const game::EntityId, ActorVisual> &entry : state.play.actors.actors())
        order.push_back(&entry.second);
    std::stable_sort(order.begin(), order.end(), [](const ActorVisual *a, const ActorVisual *b) {
        const auto layer = [](const ActorVisual *actor) -> int {
            return actor->kind == game::EnemyKind::Boss       ? 2
                   : actor->affix == game::EliteAffix::Leader ? 1
                                                              : 0;
        };
        return layer(a) != layer(b) ? layer(a) < layer(b) : a->position.y < b->position.y;
    });
    return order;
}
} // namespace
void View::enemies(engine::Renderer &g, const game::Run &run, const State &state) {
    const float tile = layout.tile;
    const bool still = state.app.preferences.reducedMotion;
    const float time = still ? 0.0F : state.play.visualTime;
    std::unordered_map<game::EntityId, const game::Enemy *> live;
    for (const game::Enemy &enemy : run.state().enemies)
        live.emplace(enemy.id, &enemy);
    const std::vector<const ActorVisual *> order = paintOrder(state);
    for (const ActorVisual *actor : order) {
        const bool dying = actor->animator.motion() == ActorMotion::Death;
        if (dying && still)
            continue;
        const std::unordered_map<game::EntityId, const game::Enemy *>::const_iterator found =
            live.find(actor->id);
        const game::Enemy *enemy = dying || found == live.end() ? nullptr : found->second;
        const game::EnemyDefinition &role = run.catalog().enemy(actor->kind);
        ScreenPoint p = screenPoint(actor->position);
        const float size = tile * bodyScale(actor->kind) * eliteScale(actor->affix);
        const bool elite = actor->affix != game::EliteAffix::None;
        const Color eliteColor = theme::elite(actor->affix);
        // Everything below stays within the body, its aura and the boss stomp ring; the frame clips the rest.
        const float reach =
            std::max({size, elite ? run.catalog().elite(actor->affix).auraRadius * tile : 0.0F,
                      actor->kind == game::EnemyKind::Boss ? role.abilityRadius * tile : 0.0F}) +
            tile;
        if (!inWorld(p, reach))
            continue;
        if (enemy && run.tunneled(*enemy)) {
            // Inside a tunnel only a faint shadow on the roof shows how far the enemy has come.
            ellipse(g, p, size * .3F, size * .2F, {14, 12, 18, 64});
            continue;
        }
        if (enemy && run.burrowed(*enemy)) {
            dirtMound(g, p, size, time);
            continue;
        }
        if (!dying)
            ellipse(g, {p.x, p.y + size * .3F}, size * (role.flying ? .26F : .34F), size * .1F,
                    {24, 44, 30, static_cast<std::uint8_t>(role.flying ? 60 : 80)});
        p.y -= hover(role, time, actor->id) * tile;
        if (enemy && actor->kind == game::EnemyKind::Boss) {
            const float pulse = .5F + .5F * std::sin(time * 3);
            // The zone's golem glows in its biome's colour.
            const game::StageBiome biome = state.app.campaign
                                               ? game::stageMetadata(state.play.currentStage).biome
                                               : game::StageBiome::Meadow;
            const Color glow = biome == game::StageBiome::Obsidian  ? Color{150, 110, 220}
                               : biome == game::StageBiome::Tundra  ? Color{150, 220, 255}
                               : biome == game::StageBiome::Lava    ? Color{255, 120, 50}
                               : biome == game::StageBiome::Core    ? Color{255, 220, 90}
                               : biome == game::StageBiome::Crystal ? Color{120, 240, 230}
                               : biome == game::StageBiome::Swamp   ? Color{150, 210, 90}
                                                                    : Color{200, 170, 120};
            g.circle({p.x, p.y - size * .1F}, size * .62F, fade(glow, .12F + .08F * pulse));
            ellipseOutline(g, {p.x, p.y + size * .28F}, size * .52F, size * .18F, std::max(2.0F, tile * .06F),
                           fade({214, 72, 52}, .45F + .35F * pulse));
            if (role.ability == game::EnemyAbility::Stomp && enemy->ability < 1.2F) {
                const float charge = std::clamp(1 - enemy->ability / 1.2F, 0.0F, 1.0F);
                const float radius = role.abilityRadius * tile;
                g.circle(p, radius * charge, fade({214, 72, 52}, .16F));
                std::vector<ScreenPoint> ring;
                for (int index = 0; index <= 40; ++index) {
                    const float angle = index * 6.2831853F / 40;
                    ring.push_back({p.x + std::cos(angle) * radius, p.y + std::sin(angle) * radius});
                }
                g.line(ring, std::max(1.5F, tile * .05F), fade({235, 96, 70}, .3F + .6F * charge));
            }
        }
        if (enemy && actor->kind == game::EnemyKind::Healer) {
            const float pulse = .5F + .5F * std::sin(time * 4 + actor->id);
            g.circle({p.x, p.y - size * .12F}, size * (.5F + .06F * pulse), fade({126, 230, 150}, .22F));
        }
        if (enemy && elite) {
            const game::EliteDefinition &affix = run.catalog().elite(actor->affix);
            if (affix.auraRadius > 0) {
                const float radius = affix.auraRadius * tile;
                dashedRing(g, p, radius, std::max(1.2F, tile * .035F), fade(eliteColor, .55F), time * .6F);
            }
            if (enemy->enraged) {
                const float pulse = .5F + .5F * std::sin(time * 12 + actor->id);
                g.circle({p.x, p.y - size * .12F}, size * (.52F + .08F * pulse), fade(eliteColor, .3F));
            }
            ellipse(g, {p.x, p.y + size * .3F}, size * .44F, size * .14F, fade(eliteColor, .28F));
            ellipseOutline(g, {p.x, p.y + size * .3F}, size * .46F, size * .15F, std::max(2.0F, tile * .07F),
                           fade(eliteColor, .95F));
        }
        if (!dying && actor->slowed)
            g.circle(p, tile * .48F * std::max(1.0F, bodyScale(actor->kind)), {155, 219, 234, 180});
        const float hit = still || dying ? 0.0F : state.play.feedback.hitIntensity(actor->id, true);
        const float width = size * (1 + hit * .18F), height = size * (1 - hit * .15F);
        g.animation(actor->animator.sample(still), actor->id,
                    {p.x - width / 2, p.y - height * .62F, width, height});
        if (hit > 0)
            g.circle({p.x, p.y - size * .1F}, size * .3F,
                     {255, 249, 221, static_cast<std::uint8_t>(170 * hit)});
        if (!enemy)
            continue;
        if (role.stealth > 0) {
            // A hidden stalker sits under a smoky veil with its detection radius traced faintly.
            if (!run.revealed(*enemy)) {
                ellipse(g, {p.x, p.y - size * .14F}, size * .42F, size * .46F, {28, 22, 46, 150});
                dashedRing(g, p, role.stealth * tile, std::max(1.0F, tile * .025F), {150, 120, 220, 70},
                           time * .4F);
            } else
                g.circle({p.x + size * .12F, p.y - size * .32F}, std::max(1.5F, size * .06F), {255, 90, 80});
        }
        if (role.ability == game::EnemyAbility::Leap && enemy->blocker && enemy->charges > 0) {
            const float charge =
                std::clamp(1 - enemy->ability / std::max(.01F, role.abilityWindup), 0.0F, 1.0F);
            ellipseOutline(g, {p.x, p.y + size * .3F}, size * (.5F - .2F * charge),
                           size * (.17F - .06F * charge), std::max(1.5F, tile * .05F),
                           fade({255, 214, 120}, .5F + .5F * charge));
        }
        if (actor->kind == game::EnemyKind::Healer)
            ellipseOutline(g, {p.x, p.y - size * .68F}, size * .2F, size * .06F, std::max(1.5F, tile * .045F),
                           {176, 246, 160});
        const int maxShield = run.catalog().shield(*enemy);
        if (enemy->shield > 0 && maxShield > 0)
            shieldPlate(g, {p.x - size * .34F, p.y + size * .04F}, size,
                        static_cast<float>(enemy->shield) / maxShield);
        const float health =
            std::clamp(static_cast<float>(enemy->hp) / std::max(1, run.catalog().health(*enemy)), 0.0F, 1.0F);
        const bool shieldDamaged = maxShield > 0 && enemy->shield < maxShield;
        const float barW = std::max(tile * .5F, size * .72F), barH = std::max(4.0F, tile * .08F);
        const Rect bar{p.x - barW / 2, p.y - size * .74F - barH, barW, barH};
        if (actor->kind == game::EnemyKind::Boss || (!elite && health >= 1 && !shieldDamaged))
            continue;
        g.rect({bar.x - 1, bar.y - 1, bar.w + 2, bar.h + 2}, {24, 22, 26, 210}, 2);
        g.rect({bar.x, bar.y, bar.w * health, bar.h}, healthColor(health), 1);
        if (maxShield > 0 && enemy->shield > 0)
            g.rect({bar.x, bar.y - barH * .7F, bar.w * enemy->shield / maxShield, barH * .6F},
                   {130, 198, 236}, 1);
    }
}
void View::enemyMarkers(engine::Renderer &g, const game::Run &run, const State &state) {
    const float tile = layout.tile;
    const bool still = state.app.preferences.reducedMotion;
    const float time = still ? 0.0F : state.play.visualTime;
    std::unordered_map<game::EntityId, const game::Enemy *> live;
    for (const game::Enemy &enemy : run.state().enemies)
        live.emplace(enemy.id, &enemy);
    for (const ActorVisual *actor : paintOrder(state)) {
        const std::unordered_map<game::EntityId, const game::Enemy *>::const_iterator found =
            live.find(actor->id);
        if (actor->animator.motion() == ActorMotion::Death || found == live.end() ||
            run.underground(*found->second))
            continue;
        const game::Enemy *enemy = found->second;
        const game::EnemyDefinition &role = run.catalog().enemy(actor->kind);
        ScreenPoint p = screenPoint(actor->position);
        p.y -= hover(role, time, actor->id) * tile;
        const float size = tile * bodyScale(actor->kind) * eliteScale(actor->affix);
        const bool elite = actor->affix != game::EliteAffix::None;
        const Color eliteColor = theme::elite(actor->affix);
        const int maxShield = run.catalog().shield(*enemy);
        const bool shieldDamaged = maxShield > 0 && enemy->shield < maxShield;
        const float barH = std::max(4.0F, tile * .08F);
        const float barTop = p.y - size * .74F - barH;
        const game::Tower *target = enemy->sabotage ? run.tower(enemy->sabotage) : nullptr;
        // A fuse spans from the sapper to its tower, so either end on screen keeps the marker.
        if (!inWorld(p, size + tile * 2) && (!target || !inWorld(screenPoint(target->position), tile * 3)))
            continue;
        if (enemy->sabotage) {
            if (target) {
                const ScreenPoint at = screenPoint(target->position);
                const float charge =
                    std::clamp(1 - enemy->ability / std::max(.01F, role.abilityWindup), 0.0F, 1.0F);
                const float blink = still ? 1.0F : .55F + .45F * std::sin(time * (8 + charge * 14));
                const Color fuse{255, 136, 48};
                std::vector<ScreenPoint> wire;
                for (int step = 0; step <= 8; ++step) {
                    const float part = step / 8.0F;
                    const float sag = std::sin(part * 3.14159F) * tile * .18F;
                    wire.push_back({p.x + (at.x - p.x) * part, p.y + (at.y - p.y) * part + sag});
                }
                g.line(wire, std::max(1.5F, tile * .05F), fade({40, 26, 20}, .8F));
                g.line(wire, std::max(1.0F, tile * .03F), fade(fuse, blink));
                g.circle(at, tile * .5F, fade({230, 70, 40}, .12F + .2F * charge));
                arc(g, at, tile * .52F, 1, std::max(2.0F, tile * .06F), {40, 20, 16, 180});
                arc(g, at, tile * .52F, charge, std::max(2.0F, tile * .06F), fade(fuse, .6F + .4F * blink));
                const float label = std::clamp(tile * .36F, 11.0F, 16.0F);
                const std::string countdown = "폭파 " + seconds(std::max(0.0F, enemy->ability));
                g.rect({at.x - label * 2.1F, at.y - tile * .62F - label * 1.25F, label * 4.2F, label * 1.2F},
                       {58, 20, 16, 220}, 4);
                g.text(countdown, {at.x, at.y - tile * .62F - label * 1.2F}, label, {255, 214, 150}, .5F);
                g.circle({p.x + size * .2F, p.y - size * .3F}, size * .1F * (.7F + .5F * blink),
                         fade({255, 230, 120}, blink));
            }
        }
        if (elite) {
            const float crownW = std::max(9.0F, size * .34F);
            crown(g, {p.x, barTop - (shieldDamaged ? barH : 0) - 3}, crownW, eliteColor);
            const std::string &name = run.catalog().elite(actor->affix).name;
            const float label = std::clamp(tile * .38F, 12.0F, 16.0F);
            const float tagW = label * (static_cast<float>(name.size()) / 3 * 1.05F + .8F);
            const Rect tag{p.x - tagW / 2, p.y + size * .38F, tagW, label * 1.3F};
            g.rect(tag, {22, 18, 26, 215}, label * .4F);
            g.rect({tag.x, tag.y + tag.h - 2, tag.w, 2}, eliteColor, 1);
            g.text(name, {p.x, tag.y + label * .12F}, label, eliteColor, .5F);
        }
    }
}

void View::enemyHud(engine::Renderer &g, const game::Run &run, const State &state) {
    introToggle_ = {};
    const game::Catalog &catalog = run.catalog();
    const bool battle = run.state().phase == game::Phase::Battle;
    const bool planning = run.state().phase == game::Phase::Ready || run.state().phase == game::Phase::Prep;
    if (!battle && !planning)
        return;
    const int wave = run.state().wave + (battle ? 0 : 1);
    if (wave < 1 || wave > static_cast<int>(catalog.waves.size()))
        return;
    std::array<int, game::EnemyKindCount> totals{};
    for (const game::LaneForecast &lane : run.laneForecast())
        for (std::size_t kind = 0; kind < totals.size(); ++kind)
            totals[kind] += lane.kinds[kind];
    const Rect world = layout.world;
    const bool compact = layout.compact;
    const std::vector<LaneMarker> markers = laneMarkers(run);
    const float flash =
        state.app.preferences.reducedMotion ? 1.0F : .65F + .35F * std::sin(state.app.uiTime * 7);
    const std::size_t bossKind = static_cast<std::size_t>(game::EnemyKind::Boss);
    const game::EnemyDefinition &bossRole = catalog.enemies[bossKind];
    const auto portrait = [&](game::EnemyKind kind, Rect bounds, std::uint32_t slot) {
        const ActorAnimationSet *clips = state.app.animationSets.find(catalog.enemy(kind).animation);
        if (clips)
            g.animation(engine::loopingSample(clips->idle, 0), PortraitIds + slot, bounds,
                        engine::AnimationBackend::Vector);
    };
    // Heavy-threat banner: the zone boss, otherwise the mid-stage pack leader; live health while it fights,
    // a countdown before it arrives.
    const game::Enemy *boss = nullptr;
    const game::Enemy *leader = nullptr;
    for (const game::Enemy &enemy : run.state().enemies) {
        if (enemy.kind == game::EnemyKind::Boss && (!boss || enemy.hp > boss->hp))
            boss = &enemy;
        if (enemy.affix == game::EliteAffix::Leader && (!leader || enemy.hp > leader->hp))
            leader = &enemy;
    }
    std::array<int, game::EliteAffixCount> elites{};
    for (const game::LaneForecast &lane : run.laneForecast())
        for (std::size_t affix = 0; affix < elites.size(); ++affix)
            elites[affix] += lane.elites[affix];
    const float bossArrival = run.nextArrival(game::EnemyKind::Boss);
    const float leaderArrival = run.nextElite(game::EliteAffix::Leader);
    const bool bossShown = boss || (totals[bossKind] > 0 && (planning || bossArrival >= 0));
    const bool leaderShown =
        !bossShown && (leader || (elites[static_cast<std::size_t>(game::EliteAffix::Leader)] > 0 &&
                                  (planning || leaderArrival >= 0)));
    float bannerBottom = world.y;
    if (bossShown || leaderShown) {
        const game::Enemy *live = bossShown ? boss : leader;
        const game::EnemyKind kind = bossShown ? game::EnemyKind::Boss
                                     : live    ? live->kind
                                               : game::EnemyKind::Armored;
        const game::EnemyDefinition &role = bossShown ? bossRole : catalog.enemy(kind);
        const std::string title =
            bossShown ? "구역 보스 · " + bossRole.name
                      : catalog.elite(game::EliteAffix::Leader).name + " · " + catalog.enemy(kind).name;
        const int leak = bossShown ? bossRole.leak : role.leak + catalog.elite(game::EliteAffix::Leader).leak;
        const float arrival = bossShown ? bossArrival : leaderArrival;
        const Color accent = bossShown ? theme::gold : theme::elite(game::EliteAffix::Leader);
        const float w = std::min(compact ? world.w - 24 : 380.0F, world.w - 24);
        Rect banner{world.x + (world.w - w) / 2, world.y + (compact ? 40.0F : 8.0F), w,
                    compact ? 34.0F : 40.0F};
        // A live threat or an entrance label under the top banner moves it above the bottom message bar.
        bool covered = false;
        if (live) {
            const ScreenPoint at = screenPoint(live->position);
            const float reach = layout.tile * bodyScale(live->kind) * eliteScale(live->affix);
            covered = at.x + reach > banner.x && at.x - reach < banner.x + banner.w &&
                      at.y - reach < banner.y + banner.h + 8;
            for (const LaneMarker &marker : markers)
                covered |= marker.box.x + marker.box.w > banner.x && marker.box.x < banner.x + banner.w &&
                           marker.box.y < banner.y + banner.h && marker.box.y + marker.box.h > banner.y;
        }
        if (covered)
            banner.y = world.y + world.h - banner.h - 48;
        else
            bannerBottom = banner.y + banner.h;
        g.rect({banner.x, banner.y + 3, banner.w, banner.h}, {0, 0, 0, 120}, 8);
        const Color fill = bossShown ? Color{58, 22, 22, 235} : Color{40, 24, 58, 235};
        const Color warning = bossShown ? Color{120, 30, 26, 235} : Color{78, 40, 116, 235};
        g.rect(banner, live ? fill : fade(warning, flash), 8);
        g.line({{banner.x + 8, banner.y + 1}, {banner.x + banner.w - 8, banner.y + 1}}, 2, fade(accent, .8F));
        portrait(kind, {banner.x + 4, banner.y - 6, banner.h + 4, banner.h + 4}, 0);
        if (!bossShown)
            crown(g, {banner.x + 4 + (banner.h + 4) / 2, banner.y + 1}, banner.h * .32F, accent);
        const float left = banner.x + banner.h + 12;
        if (live) {
            const int maximum = catalog.health(*live);
            const float health = std::clamp(static_cast<float>(live->hp) / std::max(1, maximum), 0.0F, 1.0F);
            g.text(title, {left, banner.y + 5}, compact ? 10 : 12, accent);
            g.text(std::to_string(std::max(0, live->hp)) + " / " + std::to_string(maximum),
                   {banner.x + banner.w - 10, banner.y + 5}, compact ? 10 : 12, theme::text, 1);
            const Rect bar{left, banner.y + banner.h - (compact ? 12.0F : 15.0F),
                           banner.x + banner.w - 10 - left, compact ? 7.0F : 9.0F};
            g.rect({bar.x - 1, bar.y - 1, bar.w + 2, bar.h + 2}, {20, 10, 10}, 3);
            g.rect({bar.x, bar.y, bar.w * health, bar.h},
                   !bossShown     ? theme::elite(game::EliteAffix::Leader)
                   : health > .3F ? Color{226, 76, 58}
                                  : Color{250, 140, 60},
                   2);
            for (int tick = 1; tick < 4; ++tick)
                g.line({{bar.x + bar.w * tick / 4, bar.y}, {bar.x + bar.w * tick / 4, bar.y + bar.h}}, 1,
                       {20, 10, 10, 160});
        } else {
            const std::string threat = bossShown ? "구역 보스" : catalog.elite(game::EliteAffix::Leader).name;
            g.text(planning ? "경고 · 다음 공세에 " + threat + " 출현" : "경고 · " + threat + " 접근 중",
                   {left, banner.y + 5}, compact ? 10 : 13, {255, 226, 190});
            const std::string name = bossShown ? bossRole.name : title;
            g.text(planning ? name + " · 기지 피해 " + std::to_string(leak)
                            : name + " · " + std::to_string(static_cast<int>(std::ceil(arrival))) + "초 후",
                   {left, banner.y + (compact ? 19.0F : 22.0F)}, compact ? 9 : 11, accent);
        }
    }
    // Composition chips for the current or next wave, with a NEW tag on roles and elites debuting now.
    int kinds = 0, eliteKinds = 0;
    for (int total : totals)
        kinds += total > 0 ? 1 : 0;
    for (std::size_t affix = 1; affix < elites.size(); ++affix)
        eliteKinds += elites[affix] > 0 ? 1 : 0;
    if (!kinds || (battle && run.state().battleTime > 9))
        return;
    const float header = compact ? 0.0F : 58.0F;
    const float chip = std::min(compact ? 28.0F : 40.0F, (world.w - 24 - header) / (kinds + eliteKinds) - 4);
    const float stripW = (kinds + eliteKinds) * (chip + 4) + header;
    const float stripX = world.x + world.w - stripW - 8;
    float stripY = compact ? std::max(bannerBottom + 6, world.y + 42) : world.y + 54;
    // Keep the entrance labels readable by moving below any label that shares the strip's columns.
    for (const LaneMarker &marker : markers) {
        // A visible gate counts together with its badge, so the strip never hides an entrance.
        Rect label = marker.box;
        if (marker.visible) {
            const float reach = layout.tile * .7F;
            const float left = std::min(label.x, marker.anchor.x - reach);
            const float top = std::min(label.y, marker.anchor.y - reach);
            label = {left, top, std::max(label.x + label.w, marker.anchor.x + reach) - left,
                     std::max(label.y + label.h, marker.anchor.y + reach) - top};
        }
        if (label.x + label.w > stripX - 4 && label.x < stripX + stripW + 4 &&
            label.y + label.h > stripY - 4 && label.y < stripY + chip + 4)
            stripY = label.y + label.h + 8;
    }
    g.rect({stripX - 4, stripY - 4, stripW + 8, chip + 8}, {16, 30, 38, 200}, 7);
    float x = stripX;
    if (!compact) {
        g.text(battle ? "이번" : "다음", {x + 26, stripY + 4}, 11, theme::muted, .5F);
        g.text("공세", {x + 26, stripY + 20}, 11, theme::muted, .5F);
        x += 58;
    }
    /** One introduction card: a debuting role, the pack leader, a debuting elite modifier or a battlefield
     *  gimmick of the stage. */
    struct Intro {
        game::EnemyKind kind;
        game::EliteAffix affix;
        Gimmick gimmick = Gimmick::None;
    };
    std::vector<Intro> intros;
    for (std::size_t kind = 0; kind < totals.size(); ++kind) {
        if (!totals[kind])
            continue;
        const game::EnemyKind role = static_cast<game::EnemyKind>(kind);
        const bool fresh = catalog.enemyDebuts[kind] == wave;
        if (fresh)
            intros.insert(role == game::EnemyKind::Boss ? intros.begin() : intros.end(),
                          {role, game::EliteAffix::None});
        g.rect({x, stripY, chip, chip}, fresh ? fade({120, 84, 24, 240}, flash) : Color{34, 54, 62, 230}, 6);
        if (fresh)
            g.rect({x, stripY + chip - 3, chip, 3}, theme::gold, 1);
        portrait(role, {x + chip * .1F, stripY + chip * .02F, chip * .8F, chip * .8F},
                 static_cast<std::uint32_t>(kind + 1));
        g.text(std::to_string(totals[kind]), {x + chip - 3, stripY + chip - (compact ? 12.0F : 15.0F)},
               compact ? 9 : 11, theme::text, 1);
        if (fresh)
            g.text("NEW", {x + chip / 2, stripY - (compact ? 9.0F : 11.0F)}, compact ? 8 : 10, theme::gold,
                   .5F);
        x += chip + 4;
    }
    if (elites[static_cast<std::size_t>(game::EliteAffix::Leader)] > 0)
        intros.push_back({game::EnemyKind::Armored, game::EliteAffix::Leader});
    for (std::size_t index = 1; index < elites.size(); ++index) {
        if (!elites[index])
            continue;
        const game::EliteAffix affix = static_cast<game::EliteAffix>(index);
        const Color color = theme::elite(affix);
        const bool fresh = catalog.eliteDebuts[index] == wave;
        if (fresh && affix != game::EliteAffix::Leader)
            intros.push_back({game::EnemyKind::Normal, affix});
        g.rect({x, stripY, chip, chip}, fresh ? fade({70, 44, 86, 240}, flash) : Color{40, 34, 52, 235}, 6);
        g.rect({x, stripY + chip - 3, chip, 3}, color, 1);
        crown(g, {x + chip / 2, stripY + chip * .42F}, chip * .42F, color);
        const std::string &name = catalog.elite(affix).name;
        const bool wide = name.size() > 6;
        g.text(name, {x + 2, stripY + chip * .5F}, wide ? (compact ? 6.5F : 9.0F) : (compact ? 10.0F : 12.0F),
               color);
        g.text(std::to_string(elites[index]), {x + chip - 3, stripY + chip - (compact ? 12.0F : 15.0F)},
               compact ? 9 : 11, theme::text, 1);
        if (fresh)
            g.text("NEW", {x + chip / 2, stripY - (compact ? 9.0F : 11.0F)}, compact ? 8 : 10, color, .5F);
        x += chip + 4;
    }
    // The stage's battlefield gimmicks are introduced before its first wave, ahead of any enemy card; more
    // than two share one card.
    if (wave == 1) {
        std::vector<Gimmick> terrain = stageGimmicks(catalog);
        const bool fork = !catalog.forks.empty();
        if (terrain.size() > 2)
            terrain = fork ? std::vector<Gimmick>{Gimmick::Fork, Gimmick::Mixed}
                           : std::vector<Gimmick>{Gimmick::Mixed};
        for (std::size_t index = terrain.size(); index > 0; --index)
            intros.insert(intros.begin(),
                          {game::EnemyKind::Normal, game::EliteAffix::None, terrain[index - 1]});
    }
    // Short introduction cards while planning and during the opening seconds of the wave.
    if (intros.empty() || layout.denseMine || (compact && battle))
        return;
    const float cardW = compact ? world.w - 16 : 340.0F;
    // A narrow screen's card sits above the overview inset placed on the previous frame, when there is one.
    const float compactBottom = layout.minimap.w > 0 ? layout.minimap.y - 6 : world.y + world.h - 42;
    if (state.app.introFolded) {
        const float foldW = 96;
        const Rect fold = compact ? Rect{world.x + 8, compactBottom - 30, foldW, 30}
                                  : Rect{world.x + world.w - foldW - 8, stripY + chip + 10, foldW, 30};
        g.rect({fold.x, fold.y + 2, fold.w, fold.h}, {0, 0, 0, 90}, 6);
        g.rect(fold, {22, 38, 46, 235}, 6);
        g.rect({fold.x, fold.y, 4, fold.h}, theme::gold, 2);
        g.text("안내 " + std::to_string(intros.size()) + " · 펼치기", {fold.x + 12, fold.y + 8}, 11,
               theme::gold);
        introToggle_ = fold;
        return;
    }
    const std::size_t shown = std::min<std::size_t>(intros.size(), compact ? 1 : 2);
    for (std::size_t index = 0; index < shown; ++index) {
        const Intro intro = intros[index];
        const float offset = static_cast<float>(index) * 64;
        const Rect full = compact
                              ? Rect{world.x + 8, compactBottom - 46, cardW, 46}
                              : Rect{world.x + world.w - cardW - 8, stripY + chip + 10 + offset, cardW, 58};
        const Rect card{full.x, full.y, full.w - 34, full.h};
        introToggle_ = index == 0 ? full
                                  : Rect{introToggle_.x, introToggle_.y, introToggle_.w,
                                         full.y + full.h - introToggle_.y};
        g.rect({card.x + card.w, full.y + 3, 34, full.h}, {0, 0, 0, 110}, 8);
        g.rect({card.x + card.w, full.y, 34, full.h}, {14, 26, 32, 240}, 8);
        g.text("접기", {card.x + card.w + 17, full.y + full.h / 2 - 6}, 10, theme::muted, .5F);
        const bool elite = intro.affix != game::EliteAffix::None;
        const bool ground = intro.gimmick != Gimmick::None;
        const Color accent = ground                                ? theme::green
                             : elite                               ? theme::elite(intro.affix)
                             : intro.kind == game::EnemyKind::Boss ? theme::red
                                                                   : theme::gold;
        g.rect({card.x, card.y + 3, card.w, card.h}, {0, 0, 0, 110}, 8);
        g.rect(card, elite ? Color{34, 28, 46, 240} : Color{22, 38, 46, 240}, 8);
        g.rect({card.x, card.y, 4, card.h}, accent, 2);
        if (ground) {
            gimmickIcon(g, intro.gimmick, {card.x + 4 + card.h / 2, card.y + card.h / 2}, card.h - 10,
                        state.app.campaign ? game::stageMetadata(state.play.currentStage).biome
                                           : game::StageBiome::Meadow);
            const float textX = card.x + card.h + 6;
            g.text(std::string("전장 지형 · ") + gimmickName(intro.gimmick, catalog),
                   {textX, card.y + (compact ? 6.0F : 9.0F)}, compact ? 11 : 13, accent);
            g.text(gimmickSummary(catalog, intro.gimmick), {textX, card.y + (compact ? 24.0F : 31.0F)},
                   compact ? 9 : 11, theme::text, 0, card.x + card.w - textX - 8);
            continue;
        }
        if (elite && intro.affix != game::EliteAffix::Leader) {
            const ScreenPoint centre{card.x + 4 + card.h / 2, card.y + card.h / 2};
            g.circle(centre, card.h * .36F, fade(accent, .22F));
            crown(g, {centre.x, centre.y + card.h * .14F}, card.h * .44F, accent);
        } else {
            portrait(intro.kind, {card.x + 8, card.y + 4, card.h - 8, card.h - 8},
                     20 + static_cast<std::uint32_t>(index));
            if (elite)
                crown(g, {card.x + 4 + card.h / 2, card.y + 10}, card.h * .3F, accent);
        }
        const float textX = card.x + card.h + 6;
        const std::string title =
            intro.affix == game::EliteAffix::Leader
                ? "정예 우두머리 · " + catalog.enemy(intro.kind).name + " 체력 ×" +
                      seconds(catalog.elite(intro.affix).health)
            : elite ? "정예 등장 · " + catalog.elite(intro.affix).name
                    : (intro.kind == game::EnemyKind::Boss ? "구역 보스 · " : "새로운 적 · ") +
                          catalog.enemy(intro.kind).name;
        g.text(title, {textX, card.y + (compact ? 6.0F : 9.0F)}, compact ? 11 : 13, accent);
        g.text(elite ? eliteSummary(catalog, intro.affix) : roleSummary(catalog, intro.kind),
               {textX, card.y + (compact ? 24.0F : 31.0F)}, compact ? 9 : 11, theme::text, 0,
               card.x + card.w - textX - 8);
    }
}
} // namespace thorium::presentation
