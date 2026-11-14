#include "Theme.h"
#include "View.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace thorium::presentation {
namespace {
constexpr engine::Color healGreen{126, 232, 140};
/** Apply one bounded animation opacity without changing the palette's base alpha. */
engine::Color fade(engine::Color color, float opacity) {
    color.a = static_cast<std::uint8_t>(color.a * std::clamp(opacity, 0.0F, 1.0F));
    return color;
}
/** Draw an expanding circular outline rather than obscuring entities with a solid disk. */
void ring(engine::Renderer &g, engine::ScreenPoint center, float radius, float width, engine::Color color) {
    std::vector<engine::ScreenPoint> points;
    points.reserve(33);
    for (int i = 0; i <= 32; ++i) {
        const float angle = i * 6.2831853F / 32;
        points.push_back({center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius});
    }
    g.line(points, width, color);
}
/** One floating damage or heal number waiting for its place among the others drawn this frame. */
struct FloatingNumber {
    std::string label;
    engine::ScreenPoint at;
    float size = 12;
    engine::Color color;
    float opacity = 1;
    int amount = 0;
    bool shadow = false;
};
/** Draw the largest numbers first and skip any whose box would overlap one already drawn, up to a limit, so
 *  a crowd shows a few readable totals instead of a stacked column. */
void drawNumbers(engine::Renderer &g, std::vector<FloatingNumber> &numbers, std::size_t limit) {
    std::stable_sort(numbers.begin(), numbers.end(),
                     [](const FloatingNumber &a, const FloatingNumber &b) { return a.amount > b.amount; });
    std::vector<engine::Rect> placed;
    for (const FloatingNumber &number : numbers) {
        if (placed.size() >= limit)
            break;
        const float width = number.size * (.3F + .58F * static_cast<float>(number.label.size()));
        const engine::Rect box{number.at.x - width / 2, number.at.y, width, number.size * 1.05F};
        const bool overlaps = std::any_of(placed.begin(), placed.end(), [&box](const engine::Rect &other) {
            return box.x < other.x + other.w && other.x < box.x + box.w && box.y < other.y + other.h &&
                   other.y < box.y + box.h;
        });
        if (overlaps)
            continue;
        placed.push_back(box);
        if (number.shadow)
            g.text(number.label, {number.at.x + 1, number.at.y + 1}, number.size,
                   fade({20, 34, 40}, number.opacity), .5F);
        g.text(number.label, number.at, number.size, fade(number.color, number.opacity), .5F);
    }
}
} // namespace

void View::combatFeedback(engine::Renderer &g, const State &state) {
    const float tile = layout.tile;
    g.beginClip(layout.world);
    std::vector<FloatingNumber> numbers;
    for (const CombatCue &cue : state.play.feedback.cues) {
        const game::CombatEvent &event = cue.event;
        const float t = std::clamp(cue.age / cue.lifetime, 0.0F, 1.0F);
        const engine::ScreenPoint from = screenPoint(event.from), to = screenPoint(event.to);
        // One circle around the cue's segment covers its tracer, area, sparks and floating text.
        if (!inWorld({(from.x + to.x) / 2, (from.y + to.y) / 2},
                     std::hypot(to.x - from.x, to.y - from.y) / 2 + tile * (event.radius + 1.5F)))
            continue;
        const engine::Color color = event.card == game::CardKind::Frost       ? theme::blue
                                    : event.card == game::CardKind::Lightning ? theme::purple
                                    : event.card == game::CardKind::Flame     ? engine::Color{255, 128, 52}
                                    : event.card == game::CardKind::Tar       ? engine::Color{120, 92, 70}
                                                                              : theme::gold;
        if (state.app.preferences.reducedMotion) {
            if (event.type == game::CombatEventKind::Attack)
                g.line({from, to}, 1.5F, fade(color, (1 - t) * .5F));
            else if (event.type == game::CombatEventKind::Damage && state.app.preferences.damageNumbers)
                numbers.push_back({(event.enemy ? "" : "-") + std::to_string(event.amount),
                                   {to.x, to.y - tile * .55F},
                                   layout.compact ? 12.0F : 17.0F,
                                   event.enemy ? theme::text : theme::red,
                                   1 - t,
                                   event.amount});
            else if (event.type == game::CombatEventKind::Placement ||
                     event.type == game::CombatEventKind::Defeated)
                g.circle(to, tile * .28F, fade(color, (1 - t) * .2F));
            else if (event.type == game::CombatEventKind::Heal && event.amount > 0 &&
                     state.app.preferences.damageNumbers)
                numbers.push_back({"+" + std::to_string(event.amount),
                                   {to.x, to.y - tile * .55F},
                                   layout.compact ? 12.0F : 16.0F,
                                   healGreen,
                                   1 - t,
                                   event.amount});
            else if (event.type == game::CombatEventKind::Stomp || event.type == game::CombatEventKind::Heal)
                ring(g, to, tile * std::max(.4F, event.radius), 2,
                     fade(event.type == game::CombatEventKind::Heal ? healGreen : theme::red, (1 - t) * .6F));
            else if (event.type == game::CombatEventKind::Disable ||
                     event.type == game::CombatEventKind::Enrage)
                g.text(event.type == game::CombatEventKind::Disable ? "타워 마비!" : "폭주!",
                       {to.x, to.y - tile * .8F}, layout.compact ? 12 : 15, fade({255, 196, 140}, 1 - t),
                       .5F);
            else if (event.type == game::CombatEventKind::Eruption) {
                g.circle(to, tile * event.radius, fade({255, 110, 40}, (1 - t) * .3F));
                ring(g, to, tile * event.radius, 2, fade({255, 196, 96}, 1 - t));
            } else if (event.type == game::CombatEventKind::Detonation) {
                g.circle(to, tile * event.radius, fade({190, 220, 80}, (1 - t) * .3F));
                ring(g, to, tile * event.radius, 2, fade({240, 250, 150}, 1 - t));
            } else if (event.type == game::CombatEventKind::Leap)
                g.line({from, to}, 2, fade({255, 214, 150}, (1 - t) * .7F));
            continue;
        }
        if (event.type == game::CombatEventKind::Attack) {
            if (game::distance(event.from, event.to) > .01F) {
                if (event.card == game::CardKind::Flame) {
                    const float dx = to.x - from.x, dy = to.y - from.y;
                    const float length = std::max(1.0F, std::hypot(dx, dy));
                    const float reach = length + tile * .5F, ux = dx / length, uy = dy / length;
                    const float spread = reach * .62F;
                    const engine::ScreenPoint tip{from.x + ux * reach, from.y + uy * reach};
                    g.polygon({from,
                               {tip.x - uy * spread, tip.y + ux * spread},
                               {tip.x + uy * spread, tip.y - ux * spread}},
                              fade(color, (1 - t) * .35F));
                    for (int puff = 1; puff <= 3; ++puff) {
                        const float part = std::fmod(puff / 3.0F + cue.age * 3, 1.0F);
                        g.circle({from.x + ux * reach * part, from.y + uy * reach * part},
                                 tile * (.1F + part * .22F),
                                 fade({255, 206, 96}, (1 - t) * (1 - part) * .8F));
                    }
                } else if (event.card == game::CardKind::Guard) {
                    g.line(
                        {{to.x - tile * .24F, to.y + tile * .17F}, {to.x + tile * .22F, to.y - tile * .22F}},
                        std::max(2.0F, tile * .07F), fade(theme::text, 1 - t));
                } else {
                    std::vector<engine::ScreenPoint> tracer{from};
                    if (event.card == game::CardKind::Lightning) {
                        for (int i = 1; i < 5; ++i) {
                            const float part = i / 5.0F;
                            const float offset = (i % 2 ? 1.0F : -1.0F) * tile * .13F;
                            tracer.push_back({from.x + (to.x - from.x) * part + offset,
                                              from.y + (to.y - from.y) * part - offset});
                        }
                    }
                    tracer.push_back(to);
                    g.line(tracer, 5, fade(color, (1 - t) * .25F));
                    g.line(tracer,
                           event.card == game::CardKind::Cannon   ? 3.0F
                           : event.card == game::CardKind::Sniper ? 2.6F
                                                                  : 1.8F,
                           fade(theme::text, (1 - t) * .9F));
                    if (cue.age < .1F) {
                        g.circle(from, tile * .18F * (1 + cue.age * 3), fade(color, 1 - cue.age * 10));
                        g.circle(from, tile * .07F, fade(theme::text, 1 - cue.age * 10));
                    }
                }
            }
            if (event.radius > 0) {
                const float radius = tile * event.radius * (.16F + .84F * (1 - std::pow(1 - t, 2.0F)));
                g.circle(to, radius, fade(color, (1 - t) * .12F));
                ring(g, to, radius, std::max(1.5F, 3 * (1 - t)), fade(color, 1 - t));

                // Cannon & Blast: Ground dust puffs and explosion debris sparks
                if (event.card == game::CardKind::Cannon || event.card == game::CardKind::Blast) {
                    if (cue.age < .30F) {
                        const float dt = cue.age / .30F;
                        for (int d = 0; d < 4; ++d) {
                            const float da = d * 1.570796F + 0.35F;
                            const float dist = tile * (.18F + dt * .42F);
                            const engine::ScreenPoint dp{to.x + std::cos(da) * dist,
                                                         to.y + std::sin(da) * dist};
                            g.circle(dp, tile * (.16F + dt * .14F), fade({155, 128, 98}, (1 - dt) * .32F));
                        }
                        for (int s = 0; s < 6; ++s) {
                            const float sa = s * 1.047197F + event.subject * .4F;
                            const float sr = tile * (.12F + dt * .7F);
                            const engine::ScreenPoint sp{to.x + std::cos(sa) * sr, to.y + std::sin(sa) * sr};
                            g.line({to, sp}, std::max(1.2F, tile * .035F), fade(theme::gold, 1 - dt));
                        }
                    }
                }
            } else if (cue.age < .16F)
                ring(g, to, tile * (.08F + t * .45F), 1.5F, fade(color, 1 - t));

            // Lightning discharge arcs
            if (event.card == game::CardKind::Lightning && cue.age < .22F) {
                const float lt = cue.age / .22F;
                for (int e = 0; e < 4; ++e) {
                    const float ea = e * 1.570796F + event.subject * .6F;
                    const float er = tile * (.09F + lt * .48F);
                    const engine::ScreenPoint ep{to.x + std::cos(ea) * er + (e % 2 ? 3.0F : -3.0F),
                                                 to.y + std::sin(ea) * er - (e % 2 ? -2.0F : 2.0F)};
                    g.line({to, ep}, std::max(1.5F, tile * .04F), fade({215, 185, 255}, 1 - lt));
                }
            }
            // Frost ice crystal shards
            if (event.card == game::CardKind::Frost && cue.age < .32F) {
                const float ft = cue.age / .32F;
                for (int c = 0; c < 6; ++c) {
                    const float ca = c * 1.047197F;
                    const float cr = tile * (.1F + ft * .75F);
                    const engine::ScreenPoint cp{to.x + std::cos(ca) * cr, to.y + std::sin(ca) * cr};
                    g.circle(cp, tile * .055F * (1 - ft), fade({190, 238, 255}, 1 - ft));
                    g.line({to, cp}, 1.2F, fade({140, 215, 245}, (1 - ft) * .55F));
                }
            }
        } else if (event.type == game::CombatEventKind::Damage) {
            if (cue.age < .18F) {
                for (int ray = 0; ray < 5; ++ray) {
                    const float angle = ray * 1.256637F + event.subject * .37F;
                    const float radius = tile * (.08F + t * .6F);
                    const engine::ScreenPoint end{to.x + std::cos(angle) * radius,
                                                  to.y + std::sin(angle) * radius};
                    g.line({to, end}, std::max(1.0F, tile * .035F),
                           fade(event.enemy ? theme::text : theme::red, 1 - cue.age / .18F));
                }
            }
            if (!state.app.preferences.damageNumbers)
                continue;
            // The number rises from the first hit it holds; each later hit it absorbs pops it again.
            const float climb = std::min(1.0F, cue.span / .65F);
            const float rise = tile * (.3F + .65F * (1 - std::pow(1 - climb, 2.0F)));
            const float x = to.x + (static_cast<int>(event.subject % 3) - 1) * tile * .15F;
            const float size = std::clamp(tile * .38F, 12.0F, 20.0F) *
                               (1 + .25F * std::sin(std::min(1.0F, cue.age / .16F) * 3.14159F));
            numbers.push_back(
                {event.enemy ? std::to_string(event.amount) : "-" + std::to_string(event.amount),
                 {x, to.y - rise},
                 size,
                 event.enemy ? engine::Color{255, 245, 196} : theme::red,
                 std::min(1.0F, (1 - t) * 2.2F),
                 event.amount,
                 true});
        } else if (event.type == game::CombatEventKind::Heal) {
            if (event.radius > 0) {
                const float radius = tile * event.radius * (.2F + .8F * (1 - std::pow(1 - t, 2.0F)));
                g.circle(to, radius, fade(healGreen, (1 - t) * .14F));
                ring(g, to, radius, std::max(1.5F, 3 * (1 - t)), fade(healGreen, 1 - t));
            } else {
                for (int spark = 0; spark < 3; ++spark) {
                    const float sx = to.x + (spark - 1) * tile * .16F;
                    const float sy = to.y - tile * (.2F + t * .5F + spark % 2 * .1F);
                    const float arm = tile * .06F;
                    g.line({{sx - arm, sy}, {sx + arm, sy}}, std::max(1.5F, tile * .035F),
                           fade(healGreen, 1 - t));
                    g.line({{sx, sy - arm}, {sx, sy + arm}}, std::max(1.5F, tile * .035F),
                           fade(healGreen, 1 - t));
                }
                if (state.app.preferences.damageNumbers)
                    numbers.push_back({"+" + std::to_string(event.amount),
                                       {to.x, to.y - tile * (.5F + t * .5F)},
                                       std::clamp(tile * .32F, 11.0F, 17.0F),
                                       healGreen,
                                       std::min(1.0F, (1 - t) * 2),
                                       event.amount});
            }
        } else if (event.type == game::CombatEventKind::Split) {
            for (int shard = 0; shard < 6; ++shard) {
                const float angle = shard * 1.0471975F + event.subject * .3F;
                const float radius = tile * (.1F + t * .75F);
                const engine::ScreenPoint p{to.x + std::cos(angle) * radius, to.y + std::sin(angle) * radius};
                const float s = tile * .09F * (1 - t);
                g.polygon({{p.x, p.y - s}, {p.x + s, p.y}, {p.x, p.y + s}, {p.x - s, p.y}},
                          fade({236, 120, 196}, 1 - t));
            }
            ring(g, to, tile * (.15F + t * .5F), 2, fade({236, 120, 196}, 1 - t));
            g.text("분열!", {to.x, to.y - tile * (.7F + t * .3F)}, layout.compact ? 11 : 14,
                   fade({255, 196, 232}, 1 - t), .5F);
        } else if (event.type == game::CombatEventKind::Burrow) {
            for (int puff = 0; puff < 5; ++puff) {
                const float angle = 3.14159F + puff * .785398F;
                const float radius = tile * (.15F + t * .4F);
                g.circle(
                    {to.x + std::cos(angle) * radius, to.y + tile * .15F + std::sin(angle) * radius * .4F},
                    tile * (.1F + t * .08F), fade({150, 112, 72}, (1 - t) * .7F));
            }
        } else if (event.type == game::CombatEventKind::Stomp) {
            const float radius = tile * event.radius * (.15F + .85F * (1 - std::pow(1 - t, 3.0F)));
            g.circle(to, radius, fade(theme::red, (1 - t) * .12F));
            ring(g, to, radius, std::max(2.0F, 5 * (1 - t)), fade({235, 96, 70}, 1 - t));
            ring(g, to, radius * .7F, std::max(1.5F, 3 * (1 - t)), fade({255, 214, 150}, (1 - t) * .8F));
            for (int crack = 0; crack < 8; ++crack) {
                const float angle = crack * .785398F + .2F;
                const float reach = tile * event.radius * std::min(1.0F, t * 2.5F) * .8F;
                g.line({{to.x + std::cos(angle) * tile * .3F, to.y + std::sin(angle) * tile * .3F},
                        {to.x + std::cos(angle + .12F) * reach * .55F,
                         to.y + std::sin(angle + .12F) * reach * .55F},
                        {to.x + std::cos(angle) * reach, to.y + std::sin(angle) * reach}},
                       std::max(1.5F, tile * .05F), fade({92, 64, 40}, (1 - t) * .8F));
            }
            g.text("타워 기절!", {to.x, to.y - tile * (1.3F + t * .4F)}, layout.compact ? 12 : 16,
                   fade({255, 214, 150}, 1 - t), .5F);
        } else if (event.type == game::CombatEventKind::Batter) {
            g.line({from, to}, std::max(2.0F, tile * .07F), fade({255, 214, 150}, (1 - t) * .6F));
            for (int shard = 0; shard < 4; ++shard) {
                const float angle = shard * 1.5708F + .4F + event.subject * .7F;
                const float reach = tile * (.15F + t * .35F);
                g.circle({to.x + std::cos(angle) * reach, to.y + std::sin(angle) * reach},
                         std::max(1.5F, tile * .05F), fade({150, 120, 96}, 1 - t));
            }
            g.text("-" + std::to_string(event.amount), {to.x, to.y - tile * (.7F + t * .35F)},
                   layout.compact ? 11 : 14, fade({255, 170, 120}, 1 - t), .5F);
        } else if (event.type == game::CombatEventKind::Airdrop) {
            for (int drop = 0; drop < event.amount; ++drop) {
                const float spread = (static_cast<float>(drop) - (event.amount - 1) / 2.0F) * tile * .35F;
                const float fall = tile * 1.2F * (1 - std::min(1.0F, t * 2));
                g.line(
                    {{from.x + spread, from.y - tile * .9F - fall}, {from.x + spread, from.y - fall * .2F}},
                    1, fade({220, 220, 230}, (1 - t) * .7F));
                g.circle({from.x + spread, from.y - fall * .2F}, tile * .1F, fade({200, 90, 70}, 1 - t));
            }
            ring(g, from, tile * (.3F + t * .8F), 2, fade({255, 196, 140}, 1 - t));
            g.text("강습!", {from.x, from.y - tile * (1.2F + t * .3F)}, layout.compact ? 12 : 15,
                   fade({255, 196, 140}, 1 - t), .5F);
        } else if (event.type == game::CombatEventKind::Wreck) {
            const float radius = tile * (.3F + .7F * (1 - std::pow(1 - t, 3.0F)));
            g.circle(to, radius, fade({120, 96, 80}, (1 - t) * .4F));
            for (int shard = 0; shard < 10; ++shard) {
                const float angle = shard * .6283F + event.subject * .3F;
                const float reach = tile * (.2F + t * 1.1F);
                g.rect({to.x + std::cos(angle) * reach - tile * .06F,
                        to.y + std::sin(angle) * reach - tile * .06F, tile * .12F, tile * .12F},
                       fade({110, 96, 84}, 1 - t), 1);
            }
            g.text("건물 파괴!", {to.x, to.y - tile * (1.0F + t * .4F)}, layout.compact ? 12 : 16,
                   fade({255, 150, 120}, 1 - t), .5F);
        } else if (event.type == game::CombatEventKind::Sabotage) {
            ring(g, to, tile * (.9F - .4F * t), std::max(1.5F, 3 * (1 - t)), fade({255, 136, 48}, 1 - t));
            g.text("폭약 설치!", {from.x, from.y - tile * (.8F + t * .3F)}, layout.compact ? 11 : 14,
                   fade({255, 196, 140}, 1 - t), .5F);
        } else if (event.type == game::CombatEventKind::Disable) {
            const float radius = tile * (.2F + .8F * (1 - std::pow(1 - t, 3.0F)));
            g.circle(to, radius, fade({255, 150, 60}, (1 - t) * .35F));
            ring(g, to, radius, std::max(2.0F, 4 * (1 - t)), fade({255, 214, 150}, 1 - t));
            for (int shard = 0; shard < 8; ++shard) {
                const float angle = shard * .785398F + event.subject * .3F;
                const float reach = tile * (.2F + t * .9F);
                g.line({{to.x + std::cos(angle) * reach * .4F, to.y + std::sin(angle) * reach * .4F},
                        {to.x + std::cos(angle) * reach, to.y + std::sin(angle) * reach}},
                       std::max(1.5F, tile * .05F), fade({90, 70, 60}, 1 - t));
            }
            g.text("타워 마비!", {to.x, to.y - tile * (1.0F + t * .4F)}, layout.compact ? 12 : 16,
                   fade({255, 214, 150}, 1 - t), .5F);
        } else if (event.type == game::CombatEventKind::Eruption) {
            const float radius = tile * event.radius * (.3F + .7F * (1 - std::pow(1 - t, 3.0F)));
            g.circle(to, radius, fade({255, 104, 36}, (1 - t) * .38F));
            ring(g, to, radius, std::max(2.0F, 5 * (1 - t)), fade({255, 196, 96}, 1 - t));
            for (int blob = 0; blob < 7; ++blob) {
                const float angle = blob * .897598F + event.subject * .7F;
                const float reach = tile * event.radius * (.2F + .7F * t);
                g.circle({to.x + std::cos(angle) * reach,
                          to.y + std::sin(angle) * reach - tile * .5F * std::sin(t * 3.14159F)},
                         tile * .11F * (1 - t),
                         fade(blob % 2 ? engine::Color{255, 150, 60} : theme::gold, 1 - t));
            }
            g.circle(to, tile * .3F * (1 - t), fade({255, 236, 170}, 1 - t));
            if (event.amount > 0)
                g.text("분출!", {to.x, to.y - tile * (.9F + t * .4F)}, layout.compact ? 12 : 15,
                       fade({255, 206, 140}, 1 - t), .5F);
        } else if (event.type == game::CombatEventKind::Detonation) {
            const float radius = tile * event.radius * (.25F + .75F * (1 - std::pow(1 - t, 3.0F)));
            g.circle(to, radius, fade({168, 210, 70}, (1 - t) * .4F));
            g.circle(to, radius * .6F, fade({255, 226, 120}, (1 - t) * .5F));
            ring(g, to, radius, std::max(2.0F, 6 * (1 - t)), fade({240, 250, 160}, 1 - t));
            for (int shard = 0; shard < 9; ++shard) {
                const float angle = shard * .698132F + event.subject * .5F;
                const float reach = tile * event.radius * (.3F + .8F * t);
                g.line({{to.x + std::cos(angle) * reach * .5F, to.y + std::sin(angle) * reach * .5F},
                        {to.x + std::cos(angle) * reach, to.y + std::sin(angle) * reach}},
                       std::max(1.5F, tile * .06F),
                       fade(shard % 2 ? engine::Color{96, 104, 70} : theme::gold, 1 - t));
            }
            g.text("폭발!", {to.x, to.y - tile * (1.0F + t * .5F)}, layout.compact ? 13 : 17,
                   fade({240, 250, 170}, 1 - t), .5F);
        } else if (event.type == game::CombatEventKind::Leap) {
            const float dx = to.x - from.x, dy = to.y - from.y;
            std::vector<engine::ScreenPoint> arc;
            for (int step = 0; step <= 10; ++step) {
                const float part = step / 10.0F;
                arc.push_back(
                    {from.x + dx * part, from.y + dy * part - tile * .9F * std::sin(part * 3.14159F)});
            }
            g.line(arc, std::max(1.5F, tile * .07F), fade({255, 214, 150}, (1 - t) * .8F));
            g.circle(from, tile * (.2F + t * .4F), fade({150, 130, 100}, (1 - t) * .5F));
            g.text("도약!", {to.x, to.y - tile * (.9F + t * .3F)}, layout.compact ? 12 : 15,
                   fade({255, 214, 150}, 1 - t), .5F);
        } else if (event.type == game::CombatEventKind::Cart) {
            g.text("광차 출발!", {from.x, from.y - tile * (.8F + t * .3F)}, layout.compact ? 12 : 15,
                   fade({255, 226, 150}, 1 - t), .5F);
        } else if (event.type == game::CombatEventKind::Enrage) {
            ring(g, to, tile * (.3F + t * .7F), std::max(1.5F, 3 * (1 - t)),
                 fade(theme::elite(game::EliteAffix::Enrage), 1 - t));
            g.text("폭주!", {to.x, to.y - tile * (.8F + t * .3F)}, layout.compact ? 12 : 15,
                   fade(theme::elite(game::EliteAffix::Enrage), 1 - t), .5F);
        } else if (event.type == game::CombatEventKind::ShieldBreak) {
            for (int shard = 0; shard < 7; ++shard) {
                const float angle = shard * .897598F + event.subject * .5F;
                const float radius = tile * (.12F + t * .6F);
                const engine::ScreenPoint p{to.x - tile * .25F + std::cos(angle) * radius,
                                            to.y + std::sin(angle) * radius + tile * .3F * t * t};
                g.line({p, {p.x + std::cos(angle + 1) * tile * .1F, p.y + std::sin(angle + 1) * tile * .1F}},
                       std::max(1.5F, tile * .05F), fade({176, 212, 232}, 1 - t));
            }
            g.text("방패 파괴!", {to.x, to.y - tile * (.75F + t * .3F)}, layout.compact ? 11 : 14,
                   fade({190, 226, 246}, 1 - t), .5F);
        } else if (event.type == game::CombatEventKind::Defeated && event.enemy &&
                   event.enemyKind == game::EnemyKind::Boss) {
            for (int wave = 0; wave < 3; ++wave)
                ring(g, to, tile * (.4F + (t + wave * .15F) * 2.2F), std::max(2.0F, 5 * (1 - t)),
                     fade(wave % 2 ? theme::gold : engine::Color{255, 240, 200}, 1 - t));
            for (int ray = 0; ray < 14; ++ray) {
                const float angle = ray * .448799F;
                const float radius = tile * (.3F + t * 1.8F);
                g.circle({to.x + std::cos(angle) * radius,
                          to.y + std::sin(angle) * radius - tile * .4F * std::sin(t * 3.14159F)},
                         tile * .14F * (1 - t),
                         fade(ray % 2 ? engine::Color{120, 214, 236} : theme::gold, 1 - t));
            }
            g.text("보스 격파!", {to.x, to.y - tile * (1.4F + t * .6F)}, layout.compact ? 15 : 22,
                   fade(theme::gold, std::min(1.0F, (1 - t) * 2)), .5F);
        } else if (event.type == game::CombatEventKind::Defeated) {
            const engine::Color debris = event.enemy ? engine::Color{248, 210, 127} : theme::blue;
            for (int ray = 0; ray < 7; ++ray) {
                const float angle = ray * .897598F + event.subject;
                const float radius = tile * (.12F + t * .7F);
                const engine::ScreenPoint p{to.x + std::cos(angle) * radius,
                                            to.y + std::sin(angle) * radius -
                                                tile * .15F * std::sin(t * 3.14159F)};
                g.circle(p, tile * .08F * (1 - t), fade(debris, 1 - t));
            }
            ring(g, to, tile * (.1F + t * .6F), 2, fade(debris, (1 - t) * .8F));
        } else if (event.type == game::CombatEventKind::Placement) {
            ring(g, to, tile * (.3F + t * .6F), 2, fade(theme::blue, 1 - t));
            if (cue.age < .15F)
                g.circle(to, tile * .45F, fade(theme::text, .35F * (1 - cue.age / .15F)));
            g.text(event.amount > 1 ? "강화!" : "배치!", {to.x, to.y - tile * (.65F + t * .3F)},
                   layout.compact ? 11 : 14, fade(theme::text, 1 - t), .5F);
        } else if (event.type == game::CombatEventKind::Leak) {
            const engine::Rect area = layout.world;
            g.line({{area.x + 3, area.y + 3},
                    {area.x + area.w - 3, area.y + 3},
                    {area.x + area.w - 3, area.y + area.h - 3},
                    {area.x + 3, area.y + area.h - 3},
                    {area.x + 3, area.y + 3}},
                   5, fade(theme::red, (1 - t) * .8F));
            if (state.app.preferences.damageNumbers)
                g.text("-" + std::to_string(event.amount), {to.x, to.y - tile * (.7F + t * .7F)},
                       layout.compact ? 16 : 24, fade(theme::red, 1 - t), .5F);
        }
    }
    drawNumbers(g, numbers, layout.compact ? 10 : 18);
    g.endClip();
}
} // namespace thorium::presentation
