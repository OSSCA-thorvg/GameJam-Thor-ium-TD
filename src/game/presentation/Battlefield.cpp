#include "TerrainArt.h"
#include "Theme.h"
#include "View.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace thorium::presentation {
using engine::Color;
using engine::Rect;
using engine::ScreenPoint;
namespace {
/** Ground colors and road materials for one stage biome. */
struct Palette {
    Color top;
    Color bottom;
    Color patchDark;
    Color patchLight;
    Color roadEdge;
    Color roadFill;
    Color roadIdleEdge;
    Color roadIdleFill;
    Color chevron;
};
/** Resolve the biome drawn for the current run; practice and tutorial runs use the meadow. */
game::StageBiome biomeFor(const State &state) {
    if (!state.app.campaign || state.play.tutorial.active())
        return game::StageBiome::Meadow;
    return game::stageMetadata(state.play.currentStage).biome;
}
/** Return the palette for one biome. */
Palette palette(game::StageBiome biome) {
    switch (biome) {
    case game::StageBiome::Obsidian:
        return {{78, 74, 94},    {108, 100, 118}, {52, 48, 66, 120}, {136, 124, 150, 70}, {38, 34, 46},
                {164, 150, 156}, {60, 56, 70},    {112, 106, 118},   {84, 70, 92, 170}};
    case game::StageBiome::Tundra:
        return {{188, 208, 218}, {226, 236, 241}, {156, 184, 202, 110}, {250, 252, 255, 140}, {104, 98, 96},
                {170, 156, 140}, {140, 150, 160}, {196, 204, 210},      {110, 92, 80, 160}};
    case game::StageBiome::Lava:
        return {{56, 42, 42},    {88, 62, 54}, {34, 26, 28, 140}, {122, 74, 52, 80}, {30, 22, 22},
                {152, 128, 112}, {52, 42, 40}, {98, 84, 78},      {90, 50, 36, 170}};
    case game::StageBiome::Core:
        return {{34, 48, 66},    {54, 72, 94}, {22, 32, 48, 140}, {70, 110, 120, 70}, {24, 34, 44},
                {132, 154, 164}, {44, 56, 68}, {88, 104, 114},    {52, 120, 104, 190}};
    case game::StageBiome::Crystal:
        return {{42, 38, 70},    {68, 62, 104}, {28, 24, 50, 130}, {120, 180, 230, 60}, {28, 26, 44},
                {150, 142, 176}, {52, 48, 70},  {102, 98, 126},    {120, 90, 180, 170}};
    case game::StageBiome::Swamp:
        return {{70, 88, 60},   {96, 112, 72}, {42, 62, 42, 140}, {150, 168, 92, 70}, {58, 46, 30},
                {148, 126, 92}, {64, 68, 50},  {116, 114, 88},    {92, 70, 40, 170}};
    case game::StageBiome::Meadow:
        break;
    }
    return {{158, 188, 142}, {196, 210, 162}, {128, 166, 116, 90}, {214, 222, 170, 90}, {150, 128, 95},
            {228, 209, 165}, {132, 151, 120}, {175, 188, 146},     {170, 140, 98, 170}};
}
/** Return a stable pseudo-random value for one tile and salt. */
unsigned noise(int x, int y, int salt) {
    unsigned value = static_cast<unsigned>(x) * 73856093U ^ static_cast<unsigned>(y) * 19349663U ^
                     static_cast<unsigned>(salt) * 83492791U;
    value ^= value >> 13;
    value *= 0x5bd1e995U;
    value ^= value >> 15;
    return value;
}
/** Scale a color's alpha without changing its hue. */
Color alpha(Color color, float amount) {
    return {color.r, color.g, color.b, static_cast<std::uint8_t>(std::clamp(color.a * amount, 0.0F, 255.0F))};
}
/** Append every tile covered by a cardinal polyline. */
void markRoute(std::vector<char> &grid, int width, int height, const std::vector<game::Point> &points,
               char value) {
    for (std::size_t index = 1; index < points.size(); ++index) {
        const game::Point a = points[index - 1], b = points[index];
        const int steps = static_cast<int>(std::lround(game::distance(a, b)));
        for (int step = 0; step <= steps; ++step) {
            const int x = static_cast<int>(std::lround(a.x + (b.x - a.x) * step / std::max(1, steps)));
            const int y = static_cast<int>(std::lround(a.y + (b.y - a.y) * step / std::max(1, steps)));
            if (x >= 0 && x < width && y >= 0 && y < height)
                grid[static_cast<std::size_t>(y * width + x)] = value;
        }
    }
}
/** Draw one direction chevron centred on a screen point. */
void chevron(engine::Renderer &g, ScreenPoint center, float dx, float dy, float size, float width,
             Color color) {
    const ScreenPoint tip{center.x + dx * size * .45F, center.y + dy * size * .45F};
    const ScreenPoint left{center.x - dx * size * .35F - dy * size * .5F,
                           center.y - dy * size * .35F + dx * size * .5F};
    const ScreenPoint right{center.x - dx * size * .35F + dy * size * .5F,
                            center.y - dy * size * .35F - dx * size * .5F};
    g.line({left, tip, right}, width, color);
}
/** Draw a ground-level feature beneath roads for one biome. */
void groundFeature(engine::Renderer &g, game::StageBiome biome, ScreenPoint p, float tile, unsigned seed,
                   float time) {
    const float jx = ((seed >> 4) % 100 / 100.0F - .5F) * tile * .5F;
    const float jy = ((seed >> 11) % 100 / 100.0F - .5F) * tile * .5F;
    const ScreenPoint c{p.x + jx, p.y + jy};
    switch (biome) {
    case game::StageBiome::Meadow:
        g.line({{c.x - tile * .12F, c.y + tile * .05F},
                {c.x - tile * .04F, c.y - tile * .12F},
                {c.x + tile * .02F, c.y + tile * .04F},
                {c.x + tile * .1F, c.y - tile * .08F}},
               std::max(1.0F, tile * .045F), {104, 146, 96, 150});
        if (seed % 5 == 0)
            g.circle({c.x + tile * .16F, c.y - tile * .1F}, tile * .05F, {246, 232, 160, 220});
        break;
    case game::StageBiome::Obsidian:
        g.line({{c.x - tile * .35F, c.y - tile * .1F},
                {c.x - tile * .05F, c.y + tile * .05F},
                {c.x + tile * .12F, c.y - tile * .15F},
                {c.x + tile * .38F, c.y + tile * .02F}},
               std::max(1.0F, tile * .05F), {40, 36, 52, 170});
        if (seed % 4 == 0)
            g.circle(c, tile * .05F, {190, 150, 236, 200});
        break;
    case game::StageBiome::Tundra:
        g.circle(c, tile * .32F, {214, 232, 242, 170});
        g.circle({c.x - tile * .08F, c.y - tile * .08F}, tile * .14F, {246, 252, 255, 200});
        if (seed % 3 == 0)
            g.line({{c.x - tile * .2F, c.y + tile * .1F}, {c.x + tile * .18F, c.y - tile * .06F}},
                   std::max(1.0F, tile * .03F), {160, 196, 216, 200});
        break;
    case game::StageBiome::Lava: {
        const float pulse = .65F + .35F * std::sin(time * 2.2F + static_cast<float>(seed % 7));
        const float angle = static_cast<float>(seed % 628) / 100.0F;
        const float ux = std::cos(angle), uy = std::sin(angle);
        std::vector<ScreenPoint> crack;
        for (int joint = 0; joint < 5; ++joint) {
            const float along = (joint - 2) * tile * .34F;
            const float side = (static_cast<int>((seed >> (joint * 3)) % 7) - 3) * tile * .06F;
            crack.push_back({c.x + ux * along - uy * side, c.y + uy * along + ux * side});
        }
        g.line(crack, tile * .18F, {255, 96, 40, static_cast<std::uint8_t>(45 * pulse)});
        g.line(crack, std::max(1.2F, tile * .06F), {255, 150, 60, static_cast<std::uint8_t>(220 * pulse)});
        if (seed % 3 == 0)
            g.line({crack[2],
                    {crack[2].x - uy * tile * .3F + ux * tile * .1F,
                     crack[2].y + ux * tile * .3F + uy * tile * .1F}},
                   std::max(1.0F, tile * .04F), {255, 150, 60, static_cast<std::uint8_t>(180 * pulse)});
        break;
    }
    case game::StageBiome::Core:
        g.line({{c.x - tile * .4F, c.y}, {c.x, c.y}, {c.x, c.y - tile * .35F}}, std::max(1.0F, tile * .04F),
               {84, 170, 150, 70});
        g.circle({c.x, c.y - tile * .35F}, tile * .05F, {130, 240, 190, 130});
        break;
    case game::StageBiome::Crystal:
        g.polygon({{c.x - tile * .08F, c.y + tile * .06F},
                   {c.x, c.y - tile * .2F},
                   {c.x + tile * .08F, c.y + tile * .06F}},
                  seed % 2 ? Color{150, 110, 220, 190} : Color{110, 190, 236, 190});
        if (seed % 3 == 0)
            g.circle({c.x + tile * .14F, c.y - tile * .02F}, tile * .04F, {236, 226, 255, 220});
        break;
    case game::StageBiome::Swamp:
        g.circle(c, tile * .3F, {52, 72, 48, 150});
        g.circle({c.x - tile * .05F, c.y - tile * .04F}, tile * .2F, {72, 96, 62, 170});
        if (seed % 3 == 0)
            g.circle({c.x + tile * .1F, c.y + tile * .04F}, tile * .08F, {120, 150, 76, 220});
        break;
    }
}
/** Draw one standing decoration that never overlaps a road, pad or placement cell. */
void decoration(engine::Renderer &g, game::StageBiome biome, ScreenPoint p, float tile, unsigned seed,
                float time) {
    const int variant = static_cast<int>(seed % 3);
    const float s = tile * (.8F + (seed >> 8) % 40 / 100.0F);
    const Color shade{20, 20, 26, 60};
    switch (biome) {
    case game::StageBiome::Meadow:
        if (variant == 0) {
            g.circle({p.x, p.y + s * .14F}, s * .4F, {48, 83, 61, 60});
            g.circle({p.x - s * .13F, p.y}, s * .27F, {87, 133, 94});
            g.circle({p.x + s * .13F, p.y - s * .12F}, s * .31F, {104, 150, 100});
            g.circle({p.x + s * .05F, p.y - s * .24F}, s * .16F, {128, 171, 111});
        } else if (variant == 1) {
            g.circle({p.x, p.y + s * .1F}, s * .22F, shade);
            g.polygon({{p.x - s * .22F, p.y + s * .08F},
                       {p.x - s * .1F, p.y - s * .14F},
                       {p.x + s * .14F, p.y - s * .12F},
                       {p.x + s * .24F, p.y + s * .08F}},
                      {150, 146, 132});
            g.polygon(
                {{p.x - s * .1F, p.y - s * .14F}, {p.x + s * .14F, p.y - s * .12F}, {p.x + s * .04F, p.y}},
                {182, 178, 160});
        } else {
            for (int index = 0; index < 3; ++index)
                g.circle({p.x + (index - 1) * s * .16F, p.y + (index % 2) * s * .08F}, s * .13F,
                         {118, 160, 106});
            g.circle({p.x, p.y - s * .04F}, s * .05F, {236, 128, 120});
        }
        break;
    case game::StageBiome::Obsidian:
        g.circle({p.x, p.y + s * .14F}, s * .3F, shade);
        if (variant == 2) {
            g.polygon({{p.x - s * .28F, p.y + s * .12F},
                       {p.x - s * .16F, p.y - s * .16F},
                       {p.x + s * .2F, p.y - s * .18F},
                       {p.x + s * .3F, p.y + s * .12F}},
                      {64, 58, 76});
            g.polygon(
                {{p.x - s * .16F, p.y - s * .16F}, {p.x + s * .2F, p.y - s * .18F}, {p.x + s * .02F, p.y}},
                {98, 90, 112});
        } else {
            g.polygon({{p.x - s * .22F, p.y + s * .14F},
                       {p.x - s * .08F, p.y - s * .42F},
                       {p.x + s * .04F, p.y + s * .14F}},
                      {30, 26, 38});
            g.polygon({{p.x - s * .02F, p.y + s * .14F},
                       {p.x + s * .12F, p.y - s * .3F},
                       {p.x + s * .24F, p.y + s * .14F}},
                      {44, 38, 56});
            g.line({{p.x - s * .1F, p.y - s * .3F}, {p.x - s * .05F, p.y + s * .05F}},
                   std::max(1.0F, s * .03F), {176, 140, 230, 200});
            if (variant == 1)
                g.circle({p.x + s * .12F, p.y - s * .32F}, s * .06F, {200, 160, 255, 210});
        }
        break;
    case game::StageBiome::Tundra:
        g.circle({p.x, p.y + s * .16F}, s * .3F, {120, 150, 170, 60});
        if (variant == 0) {
            g.rect({p.x - s * .04F, p.y + s * .02F, s * .08F, s * .16F}, {92, 70, 56});
            g.polygon(
                {{p.x - s * .3F, p.y + s * .06F}, {p.x, p.y - s * .26F}, {p.x + s * .3F, p.y + s * .06F}},
                {54, 96, 90});
            g.polygon(
                {{p.x - s * .22F, p.y - s * .1F}, {p.x, p.y - s * .42F}, {p.x + s * .22F, p.y - s * .1F}},
                {66, 112, 102});
            g.polygon({{p.x - s * .1F, p.y - s * .3F}, {p.x, p.y - s * .42F}, {p.x + s * .1F, p.y - s * .3F}},
                      {244, 250, 252});
        } else if (variant == 1) {
            g.polygon({{p.x - s * .26F, p.y + s * .12F},
                       {p.x - s * .14F, p.y - s * .2F},
                       {p.x + s * .08F, p.y - s * .26F},
                       {p.x + s * .26F, p.y + s * .12F}},
                      {168, 214, 236});
            g.polygon(
                {{p.x - s * .14F, p.y - s * .2F}, {p.x + s * .08F, p.y - s * .26F}, {p.x - s * .02F, p.y}},
                {226, 246, 255});
        } else {
            g.polygon({{p.x - s * .24F, p.y + s * .1F},
                       {p.x - s * .12F, p.y - s * .14F},
                       {p.x + s * .16F, p.y - s * .12F},
                       {p.x + s * .26F, p.y + s * .1F}},
                      {132, 130, 136});
            g.polygon({{p.x - s * .16F, p.y - s * .1F},
                       {p.x - s * .06F, p.y - s * .16F},
                       {p.x + s * .16F, p.y - s * .12F},
                       {p.x + s * .2F, p.y - s * .02F}},
                      {246, 250, 252});
        }
        break;
    case game::StageBiome::Lava:
        if (variant == 0) {
            const float pulse = .7F + .3F * std::sin(time * 1.7F + seed % 5);
            g.circle(p, s * .34F, {255, 90, 30, static_cast<std::uint8_t>(70 * pulse)});
            g.circle(p, s * .24F, {226, 92, 36});
            g.circle({p.x - s * .04F, p.y - s * .03F}, s * .14F,
                     {255, 176, 70, static_cast<std::uint8_t>(255 * pulse)});
        } else {
            g.circle({p.x, p.y + s * .14F}, s * .3F, shade);
            g.polygon({{p.x - s * .28F, p.y + s * .12F},
                       {p.x - s * .18F, p.y - s * .2F},
                       {p.x + s * .06F, p.y - s * .28F},
                       {p.x + s * .26F, p.y + s * .12F}},
                      {40, 32, 32});
            g.line({{p.x - s * .16F, p.y + s * .04F},
                    {p.x - s * .02F, p.y - s * .12F},
                    {p.x + s * .14F, p.y + s * .06F}},
                   std::max(1.0F, s * .04F), {255, 120, 50, 200});
        }
        break;
    case game::StageBiome::Core:
        if (variant == 2) {
            g.circle({p.x, p.y + s * .14F}, s * .3F, shade);
            g.rect({p.x - s * .2F, p.y - s * .3F, s * .16F, s * .44F}, {104, 112, 124}, s * .02F);
            g.rect({p.x + s * .04F, p.y - s * .12F, s * .18F, s * .26F}, {88, 96, 108}, s * .02F);
            g.rect({p.x - s * .24F, p.y - s * .34F, s * .24F, s * .07F}, {132, 140, 150}, s * .02F);
        } else {
            const float pulse = .75F + .25F * std::sin(time * 2.0F + seed % 9);
            g.circle(p, s * .36F, {90, 240, 170, static_cast<std::uint8_t>(40 * pulse)});
            g.polygon({{p.x - s * .1F, p.y + s * .12F},
                       {p.x - s * .02F, p.y - s * .36F},
                       {p.x + s * .08F, p.y + s * .12F}},
                      {104, 222, 168});
            g.polygon({{p.x + s * .02F, p.y + s * .12F},
                       {p.x + s * .18F, p.y - s * .2F},
                       {p.x + s * .22F, p.y + s * .12F}},
                      {70, 184, 140});
            g.polygon({{p.x - s * .26F, p.y + s * .12F},
                       {p.x - s * .2F, p.y - s * .14F},
                       {p.x - s * .08F, p.y + s * .12F}},
                      {80, 200, 150});
            g.line({{p.x - s * .02F, p.y - s * .3F}, {p.x - s * .02F, p.y + s * .05F}},
                   std::max(1.0F, s * .03F), {220, 255, 236, static_cast<std::uint8_t>(220 * pulse)});
        }
        break;
    case game::StageBiome::Crystal:
        if (variant == 2) {
            g.circle({p.x, p.y + s * .14F}, s * .3F, shade);
            g.polygon({{p.x - s * .26F, p.y + s * .12F},
                       {p.x - s * .14F, p.y - s * .14F},
                       {p.x + s * .16F, p.y - s * .16F},
                       {p.x + s * .28F, p.y + s * .12F}},
                      {60, 54, 82});
        } else {
            const float pulse = .75F + .25F * std::sin(time * 1.6F + seed % 7);
            g.circle(p, s * .34F, {150, 120, 255, static_cast<std::uint8_t>(34 * pulse)});
            drawCrystalCluster(g, {p.x, p.y + s * .05F}, s * (variant ? .55F : .7F));
        }
        break;
    case game::StageBiome::Swamp:
        g.circle({p.x, p.y + s * .14F}, s * .3F, shade);
        if (variant == 0) {
            g.rect({p.x - s * .12F, p.y - s * .2F, s * .24F, s * .36F}, {70, 56, 40}, s * .06F);
            g.rect({p.x - s * .16F, p.y - s * .24F, s * .32F, s * .08F}, {98, 80, 56}, s * .04F);
            g.line({{p.x + s * .1F, p.y - s * .1F}, {p.x + s * .3F, p.y - s * .32F}},
                   std::max(1.0F, s * .05F), {70, 56, 40});
        } else if (variant == 1) {
            for (int reed = 0; reed < 4; ++reed) {
                const float x = p.x + (reed - 1.5F) * s * .1F;
                g.line({{x, p.y + s * .12F},
                        {x + (reed % 2 ? .05F : -.04F) * s, p.y - s * (.26F + reed % 2 * .1F)}},
                       std::max(1.0F, s * .035F), {104, 128, 64});
            }
            g.rect({p.x - s * .03F, p.y - s * .34F, s * .06F, s * .14F}, {120, 82, 50}, s * .03F);
        } else {
            g.rect({p.x - s * .04F, p.y - s * .08F, s * .08F, s * .2F}, {210, 200, 170}, s * .03F);
            g.circle({p.x, p.y - s * .1F}, s * .16F, {176, 82, 70});
            g.circle({p.x - s * .05F, p.y - s * .14F}, s * .04F, {240, 220, 200});
        }
        break;
    }
}
/** Return a short lane letter. */
std::string laneLetter(std::uint32_t lane) {
    return std::string(1, static_cast<char>('A' + lane));
}
} // namespace

void View::frameOpening(const game::Run &run, const State &state) {
    // The first stage opens on the whole field on wide screens; phones zoom in only far enough to keep the
    // first entrance and the base on screen together.
    const bool firstStage = state.play.currentStage <= 1;
    if (!state.app.campaign || state.play.tutorial.active() || (firstStage && !layout.compact))
        return;
    const game::Point base = run.state().route.back();
    // Frame the base with the nearest entrance that sends enemies in the coming wave.
    game::Point gate = run.route(0).front();
    float nearest = 1e9F;
    for (const game::LaneForecast &lane : run.laneForecast()) {
        const game::Point entry = run.route(lane.route).front();
        if (lane.count > 0 && game::distance(entry, base) < nearest) {
            nearest = game::distance(entry, base);
            gate = entry;
        }
    }
    const engine::Rect world = layout.world;
    // Desktop may zoom out a little to show both; phones keep their tile size and only lean toward the gate.
    const float preferred = firstStage ? 20.0F : layout.compact ? 26.0F : 34.0F;
    const float floor = layout.compact ? 26.0F : 28.0F;
    const float fits =
        std::min(world.w / (std::abs(gate.x - base.x) + 3.2F), world.h / (std::abs(gate.y - base.y) + 3.2F));
    const float tile = firstStage ? std::min(fits, preferred) : std::clamp(fits, floor, preferred);
    // Center between both points, but keep the base clear of the frame and its edge labels.
    // On the first stage both points already fit, so the view simply centers between them.
    const float margin = layout.compact ? 3.0F : 1.5F;
    const float reachX = firstStage ? 1e6F : std::max(0.0F, world.w / tile / 2 - margin);
    const float reachY = firstStage ? 1e6F : std::max(0.0F, world.h / tile / 2 - margin);
    const engine::WorldPoint focus{std::clamp((base.x + gate.x) / 2, base.x - reachX, base.x + reachX),
                                   std::clamp((base.y + gate.y) / 2, base.y - reachY, base.y + reachY)};
    const engine::Camera2D fit(world, run.catalog().width, run.catalog().height);
    layout.camera =
        engine::Camera2D(world, run.catalog().width, run.catalog().height, fit.framed(tile, focus));
    layout.tile = layout.camera.tileSize();
    layout.mapOrigin = layout.camera.origin();
}

void View::fieldGround(engine::Renderer &g, const State &state) const {
    const Palette colors = palette(biomeFor(state));
    g.gradientRect(layout.world, colors.top, colors.bottom, 12);
}
int View::fieldKey(const State &state) const {
    return static_cast<int>(biomeFor(state));
}
void View::terrain(engine::Renderer &g, const game::Run &run, const State &state,
                   const std::vector<game::LaneForecast> &forecast) {
    const game::StageBiome biome = biomeFor(state);
    const Palette colors = palette(biome);
    const int width = run.catalog().width, height = run.catalog().height;
    const float tile = layout.tile;
    const float time = state.app.preferences.reducedMotion ? 0 : state.play.visualTime;
    std::vector<char> blocked(static_cast<std::size_t>(width * height), 0);
    for (std::size_t lane = 0; lane <= run.catalog().additionalRoutes.size(); ++lane)
        markRoute(blocked, width, height, run.route(static_cast<std::uint32_t>(lane)), 2);
    for (std::size_t fork = 0; fork < run.catalog().forks.size(); ++fork)
        markRoute(blocked, width, height, run.forkBranch(fork, !run.detoured(fork)), 2);
    std::vector<game::Point> ventTiles;
    for (const game::VentDefinition &vent : run.catalog().vents)
        ventTiles.push_back(vent.position);
    for (const game::TankDefinition &tank : run.catalog().tanks)
        ventTiles.push_back(tank.position);
    for (const game::RailDefinition &rail : run.catalog().rails)
        ventTiles.push_back(rail.lever);
    for (const std::vector<game::Point> *cells :
         {&run.state().pads, &run.state().unitCells, &run.state().boulders, &std::as_const(ventTiles)})
        for (game::Point point : *cells) {
            const int x = static_cast<int>(point.x), y = static_cast<int>(point.y);
            char &cell = blocked[static_cast<std::size_t>(std::clamp(y, 0, height - 1) * width +
                                                          std::clamp(x, 0, width - 1))];
            cell = std::max<char>(cell, 1);
        }
    const auto at = [&blocked, width, height](int x, int y) -> char {
        return x < 0 || x >= width || y < 0 || y >= height ? 0
                                                           : blocked[static_cast<std::size_t>(y * width + x)];
    };
    const Rect view{layout.world.x - tile, layout.world.y - tile, layout.world.w + tile * 2,
                    layout.world.h + tile * 2};
    const int salt = static_cast<int>(biome) * 31 + state.play.currentStage;
    // Lava cracks pulse with time, so only that biome keeps its ground features and roads out of the cache.
    const bool pulsingGround = biome == game::StageBiome::Lava && time != 0;
    const auto features = [&]() {
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const ScreenPoint p = screenPoint({static_cast<float>(x), static_cast<float>(y)});
                const unsigned seed = noise(x, y, salt);
                if (view.contains(p) && !at(x, y) && seed % 100 < 9)
                    groundFeature(g, biome, p, tile, seed, time);
            }
        roads(g, run, forecast, colors.roadEdge, colors.roadFill, colors.roadIdleEdge, colors.roadIdleFill);
        bogPools(g, run);
        railTracks(g, run);
        tunnelCovers(g, run, tunnelColors(biome));
        ventReaches(g, run);
    };
    const auto ground = [&]() {
        // Drawn unshaken; the retained image follows the whole-pixel shake offset instead.
        const ScreenPoint shake = shakeOffset;
        shakeOffset = {};
        for (int y = -1; y < height; y += 4)
            for (int x = -1; x < width; x += 4) {
                const unsigned seed = noise(x, y, salt + 7);
                if (seed % 10 < 3)
                    continue;
                const Color tone = alpha(seed % 2 ? colors.patchDark : colors.patchLight, .45F);
                for (int part = 0; part < 3; ++part) {
                    const unsigned local = noise(x + part, y - part, salt + 11);
                    const ScreenPoint p =
                        screenPoint({x + 1.5F + static_cast<float>(local % 30) / 10.0F - 1.5F,
                                     y + 1.5F + static_cast<float>((local >> 8) % 30) / 10.0F - 1.5F});
                    const float radius = tile * (1.0F + static_cast<float>((local >> 16) % 120) / 100.0F);
                    if (p.x + radius >= view.x && p.x - radius <= view.x + view.w && p.y + radius >= view.y &&
                        p.y - radius <= view.y + view.h)
                        g.circle(p, radius, tone);
                }
            }
        if (!pulsingGround)
            features();
        terrainFixtures(g, run);
        shakeOffset = shake;
    };
    // The ground depends only on the viewport, camera, stage layout, remaining boulders and which lanes carry
    // enemies; a margin beyond the frame covers the largest shake.
    engine::LayerKey key;
    key.add(layout.world).add(tile).add(layout.mapOrigin).add(salt).add(pulsingGround);
    for (const game::LaneForecast &lane : forecast) {
        key.add(lane.count > 0);
        for (game::Point point : run.route(lane.route))
            key.add(point);
    }
    for (const std::vector<game::Point> *cells : {&run.state().pads, &run.state().unitCells})
        for (game::Point point : *cells)
            key.add(point);
    key.add(run.state().boulders.size());
    for (game::Point boulder : run.state().boulders)
        key.add(boulder);
    for (const game::TraitPad &pad : run.catalog().padTraits)
        key.add(pad.position).add(pad.trait);
    for (const game::RouteSpan &span : run.tunnels())
        key.add(span.route).add(span.enter).add(span.exit);
    for (game::Point vent : ventTiles)
        key.add(vent);
    for (const game::RouteSpan &span : run.bogs())
        key.add(span.route).add(span.enter).add(span.exit);
    for (const game::RailDefinition &rail : run.catalog().rails)
        key.add(rail.from).add(rail.to);
    key.add(run.catalog().balance.ventRadius);
    const float margin = tile * .15F + 1;
    g.cachedLayer(TerrainLayer, key.value,
                  {layout.world.x - margin, layout.world.y - margin, layout.world.w + margin * 2,
                   layout.world.h + margin * 2},
                  ground, shakeOffset);
    if (pulsingGround)
        features();
    roadMarks(g, run, state, forecast, colors.chevron);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const unsigned seed = noise(x, y, salt + 3);
            if (seed % 100 >= 13 || at(x, y))
                continue;
            bool clear = true;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    clear = clear && at(x + dx, y + dy) != 2;
            const ScreenPoint p = screenPoint({static_cast<float>(x), static_cast<float>(y)});
            if (clear && view.contains(p))
                decoration(g, biome, p, tile, seed, time);
        }
}

void View::terrainFixtures(engine::Renderer &g, const game::Run &run) {
    const float tile = layout.tile;
    const float reach = run.catalog().balance.ventRadius * tile;
    for (const game::VentDefinition &vent : run.catalog().vents) {
        const ScreenPoint p = screenPoint(vent.position);
        if (!inWorld(p, reach + tile))
            continue;
        g.circle(p, reach, {120, 40, 16, 56});
        drawVentCrater(g, p, tile);
    }
    /** Check whether a tile is a live pad or a boulder, so traits of unbuilt expansion pads stay hidden. */
    const auto present = [&run](game::Point position) -> bool {
        for (const std::vector<game::Point> *cells : {&run.state().pads, &run.state().boulders})
            for (game::Point point : *cells)
                if (game::distance(point, position) < .01F)
                    return true;
        return false;
    };
    for (const game::TraitPad &pad : run.catalog().padTraits) {
        const ScreenPoint p = screenPoint(pad.position);
        if (inWorld(p, tile) && present(pad.position))
            drawPadTraitGround(g, pad.trait, p, tile);
    }
    for (game::Point boulder : run.state().boulders) {
        const ScreenPoint p = screenPoint(boulder);
        if (!inWorld(p, tile))
            continue;
        g.circle({p.x, p.y + tile * .06F}, tile * .42F, {148, 173, 144});
        g.circle(p, tile * .38F, {79, 106, 91});
        if (run.catalog().rails.empty())
            drawBoulder(g, {p.x, p.y - tile * .05F}, tile * .9F);
        else
            drawCrystalCluster(g, {p.x, p.y - tile * .02F}, tile * .9F);
    }
    for (const game::TankDefinition &tank : run.catalog().tanks) {
        const ScreenPoint p = screenPoint(tank.position);
        if (inWorld(p, tile * 2))
            g.circle(p, run.catalog().balance.tankRadius * tile, {200, 230, 90, 26});
    }
}

void View::bogPools(engine::Renderer &g, const game::Run &run) {
    const float tile = layout.tile;
    for (const game::RouteSpan &span : run.bogs()) {
        const std::vector<game::Point> &route = run.route(span.route);
        std::vector<ScreenPoint> water{screenPoint(run.routePoint(span.enter, span.route))};
        float travelled = 0;
        for (std::size_t index = 1; index + 1 < route.size(); ++index) {
            travelled += game::distance(route[index - 1], route[index]);
            if (travelled > span.enter && travelled < span.exit)
                water.push_back(screenPoint(route[index]));
        }
        water.push_back(screenPoint(run.routePoint(span.exit, span.route)));
        g.line(water, tile * 1.3F, {50, 74, 40, 150});
        g.line(water, tile * .9F, {84, 112, 52, 190});
        for (float along = span.enter + .5F; along < span.exit; along += 1.3F) {
            const ScreenPoint p = screenPoint(run.routePoint(along, span.route));
            const unsigned seed = noise(static_cast<int>(along * 10), static_cast<int>(span.route), 91);
            g.circle(
                {p.x + ((seed % 7) - 3.0F) * tile * .06F, p.y + (((seed >> 3) % 7) - 3.0F) * tile * .05F},
                tile * (.08F + (seed % 3) * .03F), {150, 180, 90, 200});
        }
    }
}

void View::railTracks(engine::Renderer &g, const game::Run &run) {
    for (const game::RailDefinition &rail : run.catalog().rails)
        drawRail(g, screenPoint(rail.from), screenPoint(rail.to), layout.tile);
}

void View::machines(engine::Renderer &g, const game::Run &run, const State &state) {
    const float tile = layout.tile;
    const game::RunBalance &balance = run.catalog().balance;
    const bool battle = run.state().phase == game::Phase::Battle;
    const float time = state.app.preferences.reducedMotion ? 0 : state.play.visualTime;
    // During battle an allied unit inside a ready gas tank's blast carries a red warning mark.
    if (battle)
        for (const game::Unit &unit : run.state().units)
            for (std::size_t index = 0; index < run.catalog().tanks.size(); ++index)
                if (run.tankReady(index) &&
                    game::distance(unit.position, run.catalog().tanks[index].position) <=
                        balance.tankRadius) {
                    const ScreenPoint p = screenPoint(unit.position);
                    const ScreenPoint mark{p.x + tile * .32F, p.y - tile * .42F};
                    g.circle(mark, tile * .15F, {200, 40, 30, 230});
                    g.text("!", {mark.x, mark.y - tile * .13F}, std::max(9.0F, tile * .24F), {255, 240, 220},
                           .5F);
                    break;
                }
    // Barricades: timber crossbars on the closed branch; between waves a pulse marks both branch mouths as
    // tappable.
    const bool planning = run.state().phase == game::Phase::Prep || run.state().phase == game::Phase::Ready;
    for (std::size_t fork = 0; fork < run.catalog().forks.size(); ++fork) {
        const game::Point spot = run.barricade(fork);
        const game::Point split = run.catalog().forks[fork].split;
        const ScreenPoint p = screenPoint(spot);
        const float dx = spot.x - split.x, dy = spot.y - split.y;
        const float across = tile * .42F;
        const ScreenPoint side{-dy * across, dx * across};
        g.circle(p, tile * .5F, {20, 16, 14, 90});
        const int durability = run.barricadeHp(fork);
        if (durability <= 0) {
            // A broken barricade leaves splintered planks on the closed branch.
            for (int plank = 0; plank < 3; ++plank) {
                const float angle = plank * 2.1F + .4F;
                const ScreenPoint at{p.x + std::cos(angle) * tile * .22F,
                                     p.y + std::sin(angle) * tile * .22F};
                g.line({{at.x - tile * .14F, at.y - tile * .04F}, {at.x + tile * .14F, at.y + tile * .04F}},
                       std::max(2.0F, tile * .09F), {110, 80, 50});
            }
            continue;
        }
        if (durability < balance.barricadeHp) {
            const float share = static_cast<float>(durability) / static_cast<float>(balance.barricadeHp);
            g.rect({p.x - tile * .32F, p.y + tile * .42F, tile * .64F, tile * .09F}, {40, 30, 30, 210}, 2);
            g.rect({p.x - tile * .32F, p.y + tile * .42F, tile * .64F * share, tile * .09F}, {240, 180, 90},
                   2);
        }
        for (float offset : {-.16F, .16F}) {
            const ScreenPoint shift{dx * tile * offset, dy * tile * offset};
            g.line({{p.x + shift.x - side.x, p.y + shift.y - side.y},
                    {p.x + shift.x + side.x, p.y + shift.y + side.y}},
                   std::max(2.0F, tile * .12F), {150, 104, 58});
        }
        g.line({{p.x - side.x * .9F - dx * tile * .2F, p.y - side.y * .9F - dy * tile * .2F},
                {p.x + side.x * .9F + dx * tile * .2F, p.y + side.y * .9F + dy * tile * .2F}},
               std::max(1.5F, tile * .07F), {214, 74, 54});
        if (planning) {
            const float pulse = time == 0 ? .6F : .5F + .5F * std::sin(time * 4.0F);
            std::vector<ScreenPoint> outline;
            const float radius = tile * (.55F + pulse * .12F);
            for (int step = 0; step <= 24; ++step)
                outline.push_back(
                    {p.x + std::cos(step * .2618F) * radius, p.y + std::sin(step * .2618F) * radius});
            g.line(outline, std::max(1.5F, tile * .06F),
                   {255, 214, 120, static_cast<std::uint8_t>(110 + 120 * pulse)});
        }
    }
    for (std::size_t index = 0; index < run.catalog().rails.size(); ++index) {
        const game::RailDefinition &rail = run.catalog().rails[index];
        const ScreenPoint from = screenPoint(rail.from), to = screenPoint(rail.to);
        const float length = std::max(1.0F, std::hypot(to.x - from.x, to.y - from.y));
        const float ux = (to.x - from.x) / length, uy = (to.y - from.y) / length;
        const float progress = run.cartProgress(index);
        const ScreenPoint cart = progress >= 0 ? ScreenPoint{from.x + (to.x - from.x) * progress,
                                                             from.y + (to.y - from.y) * progress}
                                               : from;
        if (inWorld(cart, tile)) {
            if (progress >= 0)
                g.circle(cart, balance.cartWidth * tile, {255, 214, 120, 60});
            drawCart(g, cart, ux, uy, tile);
        }
        const ScreenPoint lever = screenPoint(rail.lever);
        if (!inWorld(lever, tile))
            continue;
        const float countdown = run.cartCountdown(index);
        const bool ready = battle && countdown == 0;
        const float charge =
            !battle || progress >= 0 ? 1.0F : 1.0F - countdown / std::max(.01F, balance.cartCooldown);
        if (ready) {
            const float pulse = .5F + .5F * std::sin(time * 6);
            g.circle(lever, tile * (.58F + .08F * pulse),
                     {255, 214, 120, static_cast<std::uint8_t>(70 + 60 * pulse)});
        }
        drawLever(g, lever, tile, ready, std::clamp(charge, 0.0F, 1.0F));
        if (battle && countdown > 0)
            g.text(std::to_string(static_cast<int>(std::ceil(countdown))), {lever.x, lever.y - tile * .95F},
                   std::max(10.0F, tile * .36F), theme::text, .5F);
    }
    for (std::size_t index = 0; index < run.catalog().tanks.size(); ++index) {
        const ScreenPoint p = screenPoint(run.catalog().tanks[index].position);
        if (!inWorld(p, tile))
            continue;
        const bool spent = battle && !run.tankReady(index);
        if (battle && !spent) {
            const float pulse = .5F + .5F * std::sin(time * 5 + index);
            g.circle(p, tile * (.5F + .06F * pulse),
                     {220, 240, 110, static_cast<std::uint8_t>(50 + 50 * pulse)});
        }
        drawTank(g, p, tile, spent);
    }
}

void View::ventReaches(engine::Renderer &g, const game::Run &run) {
    const float radius = run.catalog().balance.ventRadius * layout.tile;
    for (const game::VentDefinition &vent : run.catalog().vents) {
        const ScreenPoint p = screenPoint(vent.position);
        if (inWorld(p, radius + layout.tile)) {
            g.glow(p, radius * .55F, {255, 120, 40, 46});
            drawDashedRing(g, p, radius, std::max(1.5F, layout.tile * .05F), {255, 138, 60, 150});
        }
    }
}

void View::tunnelCovers(engine::Renderer &g, const game::Run &run, const TunnelColors &colors) {
    const float tile = layout.tile;
    /** Mouth art and the round ends of the roof stroke reach this far past a span's ends, in tiles. */
    constexpr float Inset = .6F;
    for (const game::RouteSpan &span : run.tunnels()) {
        const std::vector<game::Point> &route = run.route(span.route);
        /** Unit direction on screen from one route position toward another. */
        const auto heading = [this, &run, &span](float from, float to) -> ScreenPoint {
            const ScreenPoint a = screenPoint(run.routePoint(from, span.route));
            const ScreenPoint b = screenPoint(run.routePoint(to, span.route));
            const float length = std::max(.01F, std::hypot(b.x - a.x, b.y - a.y));
            return {(b.x - a.x) / length, (b.y - a.y) / length};
        };
        const ScreenPoint entry = heading(span.enter, span.enter - .5F);
        const ScreenPoint leave = heading(span.exit, span.exit + .5F);
        drawTunnelMouth(g, screenPoint(run.routePoint(span.enter, span.route)), entry.x, entry.y, tile,
                        colors);
        drawTunnelMouth(g, screenPoint(run.routePoint(span.exit, span.route)), leave.x, leave.y, tile,
                        colors);
        if (span.exit - span.enter <= Inset * 2)
            continue;
        std::vector<ScreenPoint> roof{screenPoint(run.routePoint(span.enter + Inset, span.route))};
        float travelled = 0;
        for (std::size_t index = 1; index + 1 < route.size(); ++index) {
            travelled += game::distance(route[index - 1], route[index]);
            if (travelled > span.enter + Inset && travelled < span.exit - Inset)
                roof.push_back(screenPoint(route[index]));
        }
        roof.push_back(screenPoint(run.routePoint(span.exit - Inset, span.route)));
        drawTunnelCover(g, roof, tile, colors);
    }
}

void View::roads(engine::Renderer &g, const game::Run &run, const std::vector<game::LaneForecast> &forecast,
                 Color edge, Color fill, Color idleEdge, Color idleFill) {
    const float tile = layout.tile;
    // The closed branch of each fork stays a faded road behind its barricade.
    for (std::size_t fork = 0; fork < run.catalog().forks.size(); ++fork) {
        std::vector<ScreenPoint> closed;
        for (game::Point point : run.forkBranch(fork, !run.detoured(fork)))
            closed.push_back(screenPoint(point));
        g.line(closed, tile * .9F, alpha(idleEdge, .7F));
        g.line(closed, tile * .66F, alpha(idleFill, .6F));
    }
    std::vector<std::vector<ScreenPoint>> lanes;
    for (const game::LaneForecast &lane : forecast) {
        std::vector<ScreenPoint> route;
        for (game::Point point : run.route(lane.route))
            route.push_back(screenPoint(point));
        lanes.push_back(std::move(route));
    }
    for (std::size_t lane = 0; lane < lanes.size(); ++lane)
        g.line(lanes[lane], tile * 1.02F, forecast[lane].count ? edge : idleEdge);
    for (std::size_t lane = 0; lane < lanes.size(); ++lane)
        g.line(lanes[lane], tile * .8F, forecast[lane].count ? fill : idleFill);
}

void View::roadMarks(engine::Renderer &g, const game::Run &run, const State &state,
                     const std::vector<game::LaneForecast> &forecast, Color mark) {
    const float tile = layout.tile;
    const float time = state.app.preferences.reducedMotion ? 0 : state.play.visualTime;
    for (std::size_t lane = 0; lane < forecast.size(); ++lane) {
        const std::vector<game::Point> &points = run.route(forecast[lane].route);
        const bool active = forecast[lane].count > 0;
        float travelled = 0;
        const float shift = active ? std::fmod(time * 1.4F, 2.5F) : 0;
        for (std::size_t index = 1; index < points.size(); ++index) {
            const game::Point a = points[index - 1], b = points[index];
            const float length = game::distance(a, b);
            const float dx = (b.x - a.x) / length, dy = (b.y - a.y) / length;
            for (float along = std::fmod(2.5F - std::fmod(travelled, 2.5F) + shift, 2.5F); along < length;
                 along += 2.5F) {
                const float total = travelled + along;
                // A tunnel roof hides the road, so no chevron is drawn on it or on its mouths.
                if (run.tunnelAt(total - .6F, forecast[lane].route) ||
                    run.tunnelAt(total + .6F, forecast[lane].route))
                    continue;
                const bool near = total < 6.0F;
                const ScreenPoint c = screenPoint({a.x + dx * along, a.y + dy * along});
                const Color color = near && active ? Color{214, 82, 62, 230} : mark;
                chevron(g, c, dx, dy, tile * (near ? .42F : .3F), std::max(1.2F, tile * (near ? .09F : .06F)),
                        active || near ? color : alpha(mark, .5F));
            }
            travelled += length;
        }
        const ScreenPoint gate = screenPoint(points.front());
        g.circle(gate, tile * .62F, {16, 18, 22, 120});
        g.circle(gate, tile * .5F, active ? Color{154, 61, 48} : Color{60, 70, 66});
        g.circle(gate, tile * .38F, {24, 22, 26});
        g.circle({gate.x, gate.y + tile * .08F}, tile * .26F, active ? Color{90, 30, 26} : Color{38, 40, 42});
    }
}

void View::basePlaza(engine::Renderer &g, const game::Run &run, const State &state) {
    const float tile = layout.tile;
    const float time = state.app.preferences.reducedMotion ? 0 : state.play.visualTime;
    const ScreenPoint base = screenPoint(run.state().route.back());
    const float pulse = .5F + .5F * std::sin(time * 2.4F);
    g.circle({base.x, base.y + tile * .15F}, tile * 1.05F, {20, 26, 30, 70});
    g.circle(base, tile * .95F, {214, 206, 178});
    g.circle(base, tile * .82F, {176, 168, 142});
    g.circle(base, tile * (.95F + pulse * .2F), {241, 192, 102, static_cast<std::uint8_t>(60 * (1 - pulse))});
    g.rect({base.x - tile * .5F, base.y - tile * .62F, tile, tile * 1.02F}, {243, 237, 209}, tile * .18F);
    g.rect({base.x - tile * .34F, base.y - tile * .98F, tile * .68F, tile * .42F}, theme::green, tile * .1F);
    g.polygon({{base.x, base.y - tile * .38F},
               {base.x + tile * .26F, base.y},
               {base.x, base.y + tile * .24F},
               {base.x - tile * .26F, base.y}},
              theme::gold);
    if (biomeFor(state) == game::StageBiome::Core)
        g.circle({base.x, base.y - tile * .08F}, tile * (.14F + pulse * .05F), {150, 255, 200, 230});
}

void View::fieldEffects(engine::Renderer &g, const game::Run &run, const State &state) {
    const float tile = layout.tile;
    const float time = state.app.preferences.reducedMotion ? 0 : state.play.visualTime;
    for (const game::GroundField &field : run.state().fields) {
        const ScreenPoint p = screenPoint(field.position);
        const float remaining = field.until - run.state().battleTime;
        const float fade = std::clamp(remaining / 1.5F, 0.0F, 1.0F);
        const float radius = field.radius * tile;
        g.circle(p, radius, {28, 20, 16, static_cast<std::uint8_t>(150 * fade)});
        g.circle({p.x - radius * .1F, p.y + radius * .05F}, radius * .78F,
                 {46, 32, 24, static_cast<std::uint8_t>(200 * fade)});
        g.circle({p.x - radius * .32F, p.y - radius * .3F}, radius * .18F,
                 {120, 104, 92, static_cast<std::uint8_t>(110 * fade)});
        for (int bubble = 0; bubble < 3; ++bubble) {
            const float phase = std::fmod(time * .8F + bubble * .33F, 1.0F);
            g.circle(
                {p.x + std::cos(bubble * 2.1F) * radius * .45F, p.y + std::sin(bubble * 2.1F) * radius * .4F},
                radius * .08F * (1 - phase), {150, 128, 110, static_cast<std::uint8_t>(180 * fade)});
        }
    }
    machines(g, run, state);
    // Vents show their blast radius while a unit is being placed or moved, and a growing ring before each
    // eruption.
    const game::RunBalance &balance = run.catalog().balance;
    const game::Card *placing = run.card(placingCard_);
    const bool placingUnit = (placing && run.catalog().isUnit(placing->kind)) || state.play.movingUnit;
    for (std::size_t index = 0; index < run.catalog().vents.size(); ++index) {
        const ScreenPoint p = screenPoint(run.catalog().vents[index].position);
        const float radius = balance.ventRadius * tile;
        if (!inWorld(p, radius + tile))
            continue;
        if (placingUnit)
            drawRing(g, p, radius, std::max(2.0F, tile * .08F), {255, 170, 80, 235});
        const float countdown = run.ventCountdown(index);
        if (countdown < 0 || countdown > balance.ventWarning)
            continue;
        const float charge = 1 - countdown / std::max(.01F, balance.ventWarning);
        const float grown = state.app.preferences.reducedMotion ? radius : radius * (.2F + .8F * charge);
        g.circle(p, grown, {255, 104, 36, static_cast<std::uint8_t>(36 + 74 * charge)});
        drawRing(g, p, grown, std::max(2.0F, tile * .07F),
                 {255, 196, 96, static_cast<std::uint8_t>(120 + 135 * charge)});
        g.circle(p, tile * (.2F + .12F * charge), {255, 226, 150, 235});
    }
    for (const game::Tower &support : run.state().towers) {
        const game::CardDefinition &definition = run.catalog().card(support.kind);
        if (definition.aura <= 0)
            continue;
        const ScreenPoint from = screenPoint(support.position);
        const float pulse = std::fmod(time * .9F, 1.0F);
        const float range = run.buildingRange(support.kind, support.position);
        g.circle(from, range * tile * (.3F + pulse * .7F),
                 {241, 192, 102, static_cast<std::uint8_t>(36 * (1 - pulse))});
        for (const game::Tower &tower : run.state().towers) {
            if (tower.id == support.id || run.catalog().card(tower.kind).aura > 0 ||
                game::distance(tower.position, support.position) > range)
                continue;
            const ScreenPoint to = screenPoint(tower.position);
            g.line({from, to}, std::max(1.5F, tile * .05F), {241, 192, 102, 90});
            g.circle({from.x + (to.x - from.x) * pulse, from.y + (to.y - from.y) * pulse}, tile * .07F,
                     {255, 236, 160, 220});
        }
    }
}

std::vector<LaneMarker> View::laneMarkers(const game::Run &run) const {
    std::vector<LaneMarker> result;
    const Rect world = layout.world;
    const float tile = layout.tile;
    const int previewWave = run.state().wave + (run.state().phase == game::Phase::Battle ? 0 : 1);
    const ScreenPoint middle = world.center();
    const Rect inner{world.x + 40, world.y + 56, world.w - 80, world.h - 100};
    const Rect plate{world.x + 10, world.y + 9, layout.compact ? 116.0F : 184.0F, 25};
    const ScreenPoint home = screenPoint(run.state().route.back());
    const Rect base{home.x - tile, home.y - tile * 1.1F, tile * 2, tile * 2.1F};
    const auto overlaps = [](Rect a, Rect b) -> bool {
        return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
    };
    for (const game::LaneForecast &lane : run.laneForecast()) {
        LaneMarker marker;
        marker.lane = lane.route;
        const int opens = lane.route ? run.catalog().additionalRoutes[lane.route - 1].opensAtWave : 1;
        marker.closed = previewWave < opens;
        marker.active = lane.count > 0;
        marker.label = laneLetter(lane.route) + (marker.closed ? " · " + std::to_string(opens) + "W"
                                                 : lane.count  ? " +" + std::to_string(lane.count)
                                                               : " · 대기");
        const ScreenPoint entry = screenPoint(run.route(lane.route).front());
        const float inset = std::max(8.0F, tile * .3F);
        marker.visible = entry.x > world.x + inset && entry.x < world.x + world.w - inset &&
                         entry.y > world.y + inset && entry.y < world.y + world.h - inset;
        if (marker.visible) {
            // Badge beside its own gate: the first of above, below, right or left that stays inside the frame
            // and off the stage plate and the base.
            const float w = 58, h = 20, gap = tile * .62F + 4;
            const std::array<Rect, 4> options{{{entry.x - w / 2, entry.y - gap - h, w, h},
                                               {entry.x - w / 2, entry.y + gap, w, h},
                                               {entry.x + gap, entry.y - h / 2, w, h},
                                               {entry.x - gap - w, entry.y - h / 2, w, h}}};
            marker.box = {std::clamp(options[0].x, world.x + 4, world.x + world.w - w - 4),
                          std::clamp(options[0].y, world.y + 4, world.y + world.h - h - 4), w, h};
            for (const Rect &option : options)
                if (option.x >= world.x + 4 && option.y >= world.y + 4 &&
                    option.x + w <= world.x + world.w - 4 && option.y + h <= world.y + world.h - 4 &&
                    !overlaps(option, plate) && !overlaps(option, base)) {
                    marker.box = option;
                    break;
                }
            marker.anchor = entry;
        } else {
            const float dx = entry.x - middle.x, dy = entry.y - middle.y;
            const float scale = std::min(std::abs(dx) > .01F ? (inner.w / 2) / std::abs(dx) : 1e6F,
                                         std::abs(dy) > .01F ? (inner.h / 2) / std::abs(dy) : 1e6F);
            marker.anchor = {middle.x + dx * std::min(1.0F, scale), middle.y + dy * std::min(1.0F, scale)};
            const float length = std::max(.01F, std::hypot(dx, dy));
            marker.direction = {dx / length, dy / length};
            marker.box = {marker.anchor.x - 30, marker.anchor.y - 12, 60, 24};
            // Slide an arrow along its edge when it would sit on the base.
            if (overlaps(marker.box, base)) {
                const bool sideways = std::abs(dx) * inner.h > std::abs(dy) * inner.w;
                const float shift = sideways
                                        ? (marker.anchor.y < home.y ? base.y - marker.box.y - 26
                                                                    : base.y + base.h - marker.box.y + 2)
                                        : (marker.anchor.x < home.x ? base.x - marker.box.x - 62
                                                                    : base.x + base.w - marker.box.x + 2);
                (sideways ? marker.anchor.y : marker.anchor.x) += shift;
                (sideways ? marker.box.y : marker.box.x) += shift;
            }
        }
        result.push_back(marker);
    }
    return result;
}

void View::mapOverlays(engine::Renderer &g, engine::Ui &ui, const game::Run &run, const State &state) {
    layout.minimap = {};
    layout.minimapToggle = {};
    layout.entrances.clear();
    for (std::size_t lane = 0; lane <= run.catalog().additionalRoutes.size(); ++lane)
        layout.entrances.push_back(run.route(static_cast<std::uint32_t>(lane)).front());
    const Rect world = layout.world;
    const bool reduced = state.app.preferences.reducedMotion;
    const float wave = opening(run);
    const float beat = reduced ? 1.0F : .5F + .5F * std::sin(state.app.uiTime * 9.0F);
    const bool arrows =
        !placingCard_ && !state.play.movingUnit && !state.play.tutorial.active() && world.h >= 180;
    g.beginClip(world);
    for (const LaneMarker &marker : laneMarkers(run)) {
        const Color tone = marker.active ? Color{196, 70, 54, 240} : Color{40, 64, 56, 225};
        const float pulse = marker.active ? wave : 0.0F;
        if (marker.visible) {
            if (pulse > 0) {
                const float ring = reduced ? .6F : std::fmod(state.app.uiTime * 1.6F, 1.0F);
                g.circle(marker.anchor, layout.tile * (.7F + ring * 1.1F),
                         {255, 120, 90, static_cast<std::uint8_t>(110 * pulse * (1 - ring))});
            }
            const ScreenPoint tail{
                std::clamp(marker.anchor.x, marker.box.x + 8, marker.box.x + marker.box.w - 8),
                std::clamp(marker.anchor.y, marker.box.y + 4, marker.box.y + marker.box.h - 4)};
            g.line({tail, marker.anchor}, 2, alpha(tone, .8F));
            if (pulse > 0)
                g.rect({marker.box.x - 3, marker.box.y - 3, marker.box.w + 6, marker.box.h + 6},
                       {255, 214, 120, static_cast<std::uint8_t>(200 * pulse * beat)}, 8);
            g.rect(marker.box, tone, 6);
            g.text(marker.label, {marker.box.x + marker.box.w / 2, marker.box.y + 4}, 10, theme::text, .5F,
                   marker.box.w - 6);
            continue;
        }
        if (!arrows)
            continue;
        const float ux = marker.direction.x, uy = marker.direction.y;
        const ScreenPoint at = marker.anchor;
        if (pulse > 0) {
            // Light the world edge the wave enters from and ring the arrow while the wave opens.
            const bool sideways = std::abs(ux) * (world.h - 100) > std::abs(uy) * (world.w - 80);
            const std::uint8_t glow = static_cast<std::uint8_t>(150 * pulse * beat);
            if (sideways)
                g.rect({ux > 0 ? world.x + world.w - 6 : world.x, at.y - 70, 6, 140}, {255, 96, 70, glow}, 3);
            else
                g.rect({at.x - 70, uy > 0 ? world.y + world.h - 6 : world.y, 140, 6}, {255, 96, 70, glow}, 3);
            g.circle(at, 34 + 8 * beat, {255, 110, 80, static_cast<std::uint8_t>(70 * pulse)});
        }
        const float edge = std::min(30 / std::max(.01F, std::abs(ux)), 12 / std::max(.01F, std::abs(uy)));
        const float reach = 14 + (pulse > 0 ? 6 * beat : 0.0F);
        const ScreenPoint tip{at.x + ux * (edge + reach), at.y + uy * (edge + reach)};
        g.polygon({tip,
                   {at.x + ux * (edge + 1) - uy * 9, at.y + uy * (edge + 1) + ux * 9},
                   {at.x + ux * (edge + 1) + uy * 9, at.y + uy * (edge + 1) - ux * 9}},
                  tone);
        const Rect box = marker.box;
        if (pulse > 0)
            g.rect({box.x - 3, box.y - 3, box.w + 6, box.h + 6},
                   {255, 214, 120, static_cast<std::uint8_t>(220 * pulse * beat)}, 9);
        g.rect(box, tone, 7);
        g.text(marker.label, {at.x, at.y - 6}, 11, theme::text, .5F, 56);
        ui.add(LaneFocusBase + marker.lane, {box.x - 6, box.y - 6, box.w + 12, box.h + 12}, true, false,
               false);
    }
    g.endClip();
    if (state.play.tutorial.active())
        return;
    const engine::Rect visible = layout.camera.visibleWorld();
    const int width = run.catalog().width, height = run.catalog().height;
    if (visible.x <= .01F && visible.y <= .01F && visible.x + visible.w >= width - .01F &&
        visible.y + visible.h >= height - .01F)
        return;
    layout.fieldWidth = width;
    layout.fieldHeight = height;
    if (!layout.compact && run.state().phase != game::Phase::Prep) {
        // Desktop battles keep the overview in the command column, clear of the battlefield.
        const Rect side = layout.sidebar;
        const float mapW = side.w - 32, mapH = std::min(130.0F, mapW * height / width);
        const float fitW = mapH * width / height;
        layout.minimap = {side.x + 16 + (mapW - fitW) / 2, side.y + 58, fitW, mapH};
    } else if (arrows && state.app.minimapFolded) {
        layout.minimapToggle = {world.x + 10, world.y + world.h - 42, 64, 30};
    } else if (arrows) {
        const float mapW = std::clamp(world.w * (layout.compact ? .3F : .2F), 96.0F, 190.0F);
        const float mapH = mapW * height / width;
        layout.minimap = {world.x + 10, world.y + world.h - mapH - 12, mapW, mapH};
        layout.minimapToggle = {layout.minimap.x + mapW - 38, layout.minimap.y, 38, 22};
    }
}

float View::opening(const game::Run &run) const {
    if (run.state().phase != game::Phase::Battle)
        return 0.0F;
    return std::clamp(1.4F - run.state().battleTime / 3.0F, 0.0F, 1.0F);
}

void View::minimap(engine::Renderer &g, engine::Ui &ui, const game::Run &run, const State &state) {
    const Rect box = layout.minimap;
    const Rect toggle = layout.minimapToggle;
    const bool interactive = !placingCard_ && !state.play.movingUnit;
    if (box.w <= 0) {
        if (toggle.w <= 0)
            return;
        g.rect({toggle.x, toggle.y + 2, toggle.w, toggle.h}, {0, 0, 0, 90}, 6);
        g.rect(toggle, {12, 24, 30, 225}, 6);
        g.rect({toggle.x + 9, toggle.y + 9, 14, 12}, alpha(palette(biomeFor(state)).bottom, .8F), 2);
        g.circle({toggle.x + 19, toggle.y + 13}, 2, theme::gold);
        g.text("지도", {toggle.x + 28, toggle.y + 8}, 11, theme::text);
        if (interactive)
            ui.add(ToggleMinimap, toggle, true, false, false);
        return;
    }
    const int width = run.catalog().width, height = run.catalog().height;
    const std::vector<game::LaneForecast> forecast = run.laneForecast();
    const float wave = opening(run);
    const float ring = state.app.preferences.reducedMotion ? .6F : std::fmod(state.app.uiTime * 1.6F, 1.0F);
    g.rect({box.x - 4, box.y - 4, box.w + 8, box.h + 8}, {12, 24, 30, 225}, 6);
    g.rect(box, alpha(palette(biomeFor(state)).bottom, .55F), 3);
    g.beginClip(box);
    for (const game::LaneForecast &lane : forecast) {
        std::vector<ScreenPoint> route;
        for (game::Point point : run.route(lane.route))
            route.push_back(layout.minimapScreen(point));
        g.line(route, std::max(2.0F, box.w / width * .8F),
               lane.count ? Color{230, 170, 120} : Color{150, 160, 150, 170});
    }
    for (const game::Tower &tower : run.state().towers)
        g.circle(layout.minimapScreen(tower.position), std::max(1.6F, box.w / width * .5F), theme::green);
    // Enemy dots by role: flyers pale blue, a hidden stalker faint violet, the boss large, the rest red.
    const float dot = std::max(1.4F, box.w / width * .4F) * (wave > 0 ? 1.5F : 1.0F);
    for (const game::Enemy &enemy : run.state().enemies) {
        const game::EnemyDefinition &role = run.catalog().enemy(enemy.kind);
        const ScreenPoint at = layout.minimapScreen(enemy.position);
        if (enemy.kind == game::EnemyKind::Boss) {
            g.circle(at, dot * 2.4F, {60, 20, 16, 220});
            g.circle(at, dot * 1.8F, {255, 120, 60});
        } else if (role.flying)
            g.polygon(
                {{at.x, at.y - dot * 1.4F}, {at.x + dot * 1.3F, at.y + dot}, {at.x - dot * 1.3F, at.y + dot}},
                {150, 210, 255});
        else if (role.stealth > 0 && !run.revealed(enemy))
            g.circle(at, dot, {170, 130, 220, 120});
        else
            g.circle(at, dot, {255, 86, 70});
    }
    for (const game::LaneForecast &lane : forecast) {
        if (!lane.count)
            continue;
        const ScreenPoint gate = layout.minimapScreen(run.route(lane.route).front());
        if (wave > 0)
            g.circle(gate, 4 + ring * 10, {255, 120, 90, static_cast<std::uint8_t>(200 * wave * (1 - ring))});
        g.circle(gate, 3, {196, 70, 54});
    }
    g.circle(layout.minimapScreen(run.state().route.back()), std::max(3.0F, box.w / width), theme::gold);
    const engine::Rect visible = layout.camera.visibleWorld();
    const float sx = box.w / width, sy = box.h / height;
    const Rect frame{box.x + std::max(0.0F, visible.x) * sx, box.y + std::max(0.0F, visible.y) * sy,
                     std::min(static_cast<float>(width), visible.w) * sx,
                     std::min(static_cast<float>(height), visible.h) * sy};
    g.line({{frame.x, frame.y},
            {frame.x + frame.w, frame.y},
            {frame.x + frame.w, frame.y + frame.h},
            {frame.x, frame.y + frame.h},
            {frame.x, frame.y}},
           1.5F, {255, 248, 220, 230});
    g.endClip();
    if (toggle.w > 0) {
        g.rect(toggle, {12, 24, 30, 215}, 4);
        g.text("접기", {toggle.x + toggle.w / 2, toggle.y + 5}, 10, theme::text, .5F);
    }
    if (interactive) {
        ui.add(MinimapFocus, box, true, false, false);
        if (toggle.w > 0)
            ui.add(ToggleMinimap, toggle, true, false, false);
    }
}
} // namespace thorium::presentation
