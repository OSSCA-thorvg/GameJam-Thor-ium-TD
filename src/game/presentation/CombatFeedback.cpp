#include "Theme.h"
#include "View.h"
#include <algorithm>
#include <cmath>

namespace thorium::presentation {
namespace {
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
} // namespace

void View::combatFeedback(engine::Renderer &g, const State &state) {
    const float tile = layout.tile;
    g.beginClip(layout.world);
    for (const CombatCue &cue : state.play.feedback.cues) {
        const game::CombatEvent &event = cue.event;
        const float t = std::clamp(cue.age / cue.lifetime, 0.0F, 1.0F);
        const engine::ScreenPoint from = screenPoint(event.from), to = screenPoint(event.to);
        const engine::Color color = event.card == game::CardKind::Frost       ? theme::blue
                                    : event.card == game::CardKind::Lightning ? theme::purple
                                                                              : theme::gold;
        if (state.app.preferences.reducedMotion) {
            if (event.type == game::CombatEventKind::Attack)
                g.line({from, to}, 1.5F, fade(color, (1 - t) * .5F));
            else if (event.type == game::CombatEventKind::Damage && state.app.preferences.damageNumbers)
                g.text((event.enemy ? "" : "-") + std::to_string(event.amount), {to.x, to.y - tile * .55F},
                       layout.compact ? 12 : 17, fade(event.enemy ? theme::text : theme::red, 1 - t), .5F);
            else if (event.type == game::CombatEventKind::Placement ||
                     event.type == game::CombatEventKind::Defeated)
                g.circle(to, tile * .28F, fade(color, (1 - t) * .2F));
            continue;
        }
        if (event.type == game::CombatEventKind::Attack) {
            if (game::distance(event.from, event.to) > .01F) {
                if (event.card == game::CardKind::Guard) {
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
                    g.line(tracer, event.card == game::CardKind::Cannon ? 3.0F : 1.8F,
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
            } else if (cue.age < .16F)
                ring(g, to, tile * (.08F + t * .45F), 1.5F, fade(color, 1 - t));
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
            const float rise = tile * (.3F + .65F * (1 - std::pow(1 - t, 2.0F)));
            const float x = to.x + (static_cast<int>(event.subject % 3) - 1) * tile * .15F;
            const float size = std::clamp(tile * .38F, 12.0F, 20.0F) *
                               (1 + .25F * std::sin(std::min(1.0F, cue.age / .16F) * 3.14159F));
            const std::string label =
                event.enemy ? std::to_string(event.amount) : "-" + std::to_string(event.amount);
            const float opacity = std::min(1.0F, (1 - t) * 2.2F);
            g.text(label, {x + 1, to.y - rise + 1}, size, fade({20, 34, 40}, opacity), .5F);
            g.text(label, {x, to.y - rise}, size,
                   fade(event.enemy ? engine::Color{255, 245, 196} : theme::red, opacity), .5F);
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
    g.endClip();
}
} // namespace thorium::presentation
