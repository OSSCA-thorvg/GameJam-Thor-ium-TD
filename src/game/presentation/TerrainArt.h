#pragma once
#include "TerrainPresentation.h"
#include "engine/render/Renderer.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace thorium::presentation {
/** Return a pad trait's accent color for its glyph, pad tint and labels. */
inline engine::Color padTraitColor(game::PadTrait trait) {
    return trait == game::PadTrait::HighGround ? engine::Color{250, 214, 120} : engine::Color{132, 222, 255};
}
/** Rock tones of a tunnel's covered section and mouths. */
struct TunnelColors {
    engine::Color rim;
    engine::Color roof;
    engine::Color ridge;
};
/** Return the tunnel rock tones that match one stage biome's ground. */
inline TunnelColors tunnelColors(game::StageBiome biome) {
    switch (biome) {
    case game::StageBiome::Obsidian:
        return {{30, 26, 40}, {70, 62, 86}, {112, 100, 132}};
    case game::StageBiome::Tundra:
        return {{84, 94, 108}, {150, 166, 180}, {232, 240, 246}};
    case game::StageBiome::Lava:
        return {{24, 18, 18}, {66, 46, 42}, {118, 78, 60}};
    case game::StageBiome::Core:
        return {{18, 28, 38}, {52, 72, 88}, {96, 140, 140}};
    case game::StageBiome::Meadow:
        break;
    }
    return {{74, 84, 66}, {120, 136, 100}, {170, 182, 136}};
}
/** Draw a boulder about `size` wide whose base rests slightly below its center. */
inline void drawBoulder(engine::Renderer &g, engine::ScreenPoint p, float size) {
    const float s = size;
    g.circle({p.x, p.y + s * .26F}, s * .46F, {20, 24, 22, 70});
    g.polygon({{p.x - s * .5F, p.y + s * .22F},
               {p.x - s * .42F, p.y - s * .16F},
               {p.x - s * .14F, p.y - s * .42F},
               {p.x + s * .22F, p.y - s * .38F},
               {p.x + s * .48F, p.y - s * .08F},
               {p.x + s * .44F, p.y + s * .26F},
               {p.x - s * .1F, p.y + s * .36F}},
              {112, 106, 98});
    g.polygon({{p.x - s * .42F, p.y - s * .16F},
               {p.x - s * .14F, p.y - s * .42F},
               {p.x + s * .22F, p.y - s * .38F},
               {p.x + s * .06F, p.y - s * .1F},
               {p.x - s * .22F, p.y - s * .02F}},
              {168, 162, 148});
    g.polygon({{p.x + s * .22F, p.y - s * .38F},
               {p.x + s * .48F, p.y - s * .08F},
               {p.x + s * .2F, p.y + s * .04F},
               {p.x + s * .06F, p.y - s * .1F}},
              {140, 134, 122});
    g.line(
        {{p.x - s * .22F, p.y - s * .02F}, {p.x - s * .04F, p.y + s * .16F}, {p.x - s * .1F, p.y + s * .34F}},
        std::max(1.0F, s * .04F), {70, 66, 62});
    g.circle({p.x + s * .4F, p.y + s * .36F}, s * .09F, {132, 126, 116});
    g.circle({p.x - s * .46F, p.y + s * .34F}, s * .06F, {150, 144, 132});
}
/** Draw the small trait badge: an upward chevron for high ground, a faceted gem for crystal ground. */
inline void drawPadTraitGlyph(engine::Renderer &g, game::PadTrait trait, engine::ScreenPoint p, float size) {
    const engine::Color color = padTraitColor(trait);
    g.circle(p, size * .5F, {18, 26, 34, 235});
    if (trait == game::PadTrait::HighGround) {
        for (float lift : {.1F, -.14F})
            g.line({{p.x - size * .24F, p.y + size * (lift + .1F)},
                    {p.x, p.y + size * (lift - .12F)},
                    {p.x + size * .24F, p.y + size * (lift + .1F)}},
                   std::max(1.2F, size * .13F), color);
    } else {
        g.polygon({{p.x, p.y - size * .32F},
                   {p.x + size * .24F, p.y - size * .04F},
                   {p.x, p.y + size * .34F},
                   {p.x - size * .24F, p.y - size * .04F}},
                  color);
        g.polygon(
            {{p.x, p.y - size * .32F}, {p.x + size * .24F, p.y - size * .04F}, {p.x, p.y + size * .02F}},
            {236, 250, 255});
    }
}
/** Draw the ground around a trait pad, wider than the pad disc: a raised stone terrace for high ground or
 *  crystal shards for crystal ground, with the trait badge at the upper right. */
inline void drawPadTraitGround(engine::Renderer &g, game::PadTrait trait, engine::ScreenPoint p, float tile) {
    if (trait == game::PadTrait::HighGround) {
        std::vector<engine::ScreenPoint> terrace, face;
        for (int corner = 0; corner < 8; ++corner) {
            const float angle = (corner + .5F) * .7853982F;
            const engine::ScreenPoint point{p.x + std::cos(angle) * tile * .62F,
                                            p.y + std::sin(angle) * tile * .56F};
            terrace.push_back(point);
            face.push_back({point.x, point.y + tile * .12F});
        }
        g.polygon(face, {58, 52, 60});
        g.polygon(terrace, {158, 150, 142});
        g.line({terrace[4], terrace[5], terrace[6], terrace[7]}, std::max(1.0F, tile * .04F),
               {214, 206, 190});
    } else if (trait == game::PadTrait::Crystal) {
        g.circle(p, tile * .58F, {96, 190, 236, 60});
        for (int shard = 0; shard < 6; ++shard) {
            const float angle = shard * 1.0471976F + .4F;
            const float reach = tile * (shard % 2 ? .68F : .6F), width = tile * .1F;
            const engine::ScreenPoint root{p.x + std::cos(angle) * tile * .38F,
                                           p.y + std::sin(angle) * tile * .38F};
            const engine::ScreenPoint tip{p.x + std::cos(angle) * reach, p.y + std::sin(angle) * reach};
            g.polygon({{root.x - std::sin(angle) * width, root.y + std::cos(angle) * width},
                       tip,
                       {root.x + std::sin(angle) * width, root.y - std::cos(angle) * width}},
                      shard % 2 ? engine::Color{104, 196, 240} : engine::Color{178, 232, 255});
        }
    }
    if (trait != game::PadTrait::None)
        drawPadTraitGlyph(g, trait, {p.x + tile * .5F, p.y - tile * .42F}, tile * .4F);
}
/** Draw one tunnel mouth: a dark opening under a stone arch, facing along the unit direction (dx, dy) that
 *  points out of the tunnel. */
inline void drawTunnelMouth(engine::Renderer &g, engine::ScreenPoint p, float dx, float dy, float tile,
                            const TunnelColors &colors) {
    const engine::ScreenPoint inner{p.x - dx * tile * .12F, p.y - dy * tile * .12F};
    g.circle(inner, tile * .7F, colors.rim);
    g.circle(inner, tile * .62F, colors.roof);
    g.circle({p.x + dx * tile * .08F, p.y + dy * tile * .08F}, tile * .44F, {14, 12, 16});
    g.circle({p.x + dx * tile * .2F, p.y + dy * tile * .2F}, tile * .3F, {34, 28, 30});
    for (float side : {-1.0F, 1.0F}) {
        const engine::ScreenPoint post{p.x + dx * tile * .3F - dy * side * tile * .5F,
                                       p.y + dy * tile * .3F + dx * side * tile * .5F};
        g.circle(post, tile * .12F, {66, 46, 30});
        g.circle(post, tile * .07F, {150, 110, 70});
    }
}
/** Draw the covered section of a tunnel along screen points from mouth to mouth. */
inline void drawTunnelCover(engine::Renderer &g, const std::vector<engine::ScreenPoint> &path, float tile,
                            const TunnelColors &colors) {
    if (path.size() < 2)
        return;
    g.line(path, tile * 1.34F, colors.rim);
    g.line(path, tile * 1.18F, colors.roof);
    g.line(path, tile * .34F, colors.ridge);
    for (std::size_t index = 1; index < path.size(); ++index) {
        const engine::ScreenPoint a = path[index - 1], b = path[index];
        const float length = std::hypot(b.x - a.x, b.y - a.y);
        if (length < 1)
            continue;
        const float ux = (b.x - a.x) / length, uy = (b.y - a.y) / length;
        for (float along = tile * .5F; along < length - tile * .25F; along += tile) {
            const engine::ScreenPoint c{a.x + ux * along, a.y + uy * along};
            g.line({{c.x - uy * tile * .52F, c.y + ux * tile * .52F},
                    {c.x + uy * tile * .52F, c.y - ux * tile * .52F}},
                   std::max(1.0F, tile * .05F), colors.rim);
        }
    }
}
/** Draw a dormant lava vent: a stone-rimmed crater with cracks and a glowing core. */
inline void drawVentCrater(engine::Renderer &g, engine::ScreenPoint p, float tile) {
    for (int crack = 0; crack < 5; ++crack) {
        const float angle = crack * 1.2566371F + .3F;
        g.line({{p.x + std::cos(angle) * tile * .4F, p.y + std::sin(angle) * tile * .4F},
                {p.x + std::cos(angle + .25F) * tile * .62F, p.y + std::sin(angle + .25F) * tile * .62F},
                {p.x + std::cos(angle) * tile * .8F, p.y + std::sin(angle) * tile * .8F}},
               std::max(1.0F, tile * .05F), {255, 120, 50, 170});
    }
    g.circle({p.x, p.y + tile * .06F}, tile * .56F, {12, 10, 12, 130});
    g.circle(p, tile * .52F, {146, 124, 108});
    g.circle(p, tile * .43F, {40, 28, 28});
    for (int tooth = 0; tooth < 8; ++tooth) {
        const float angle = tooth * .7853982F;
        g.circle({p.x + std::cos(angle) * tile * .47F, p.y + std::sin(angle) * tile * .47F}, tile * .07F,
                 tooth % 2 ? engine::Color{176, 152, 132} : engine::Color{110, 92, 82});
    }
    g.circle(p, tile * .32F, {214, 80, 30});
    g.circle({p.x - tile * .04F, p.y - tile * .04F}, tile * .18F, {255, 176, 70});
    g.circle({p.x - tile * .06F, p.y - tile * .06F}, tile * .08F, {255, 236, 170});
}
/** Stroke one closed circle outline. */
inline void drawRing(engine::Renderer &g, engine::ScreenPoint p, float radius, float width,
                     engine::Color color) {
    std::vector<engine::ScreenPoint> points;
    points.reserve(41);
    for (int step = 0; step <= 40; ++step) {
        const float angle = step * 6.2831853F / 40;
        points.push_back({p.x + std::cos(angle) * radius, p.y + std::sin(angle) * radius});
    }
    g.line(points, width, color);
}
} // namespace thorium::presentation
