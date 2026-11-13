#include "MineArt.h"
#include "Theme.h"
#include "View.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace thorium::presentation {
using engine::Color;
using engine::Rect;
using engine::ScreenPoint;
namespace {
constexpr float Turn = 6.2831853F;

/** Return a deterministic value in [0, 1) for decorative variation of one cell. */
float noise(int index, int layer, int salt) {
    std::uint32_t value = static_cast<std::uint32_t>(index) * 2654435761U ^
                          static_cast<std::uint32_t>(layer + 1) * 40503U ^
                          static_cast<std::uint32_t>(salt) * 2246822519U;
    value ^= value >> 15;
    value *= 2246822519U;
    value ^= value >> 13;
    value *= 3266489917U;
    value ^= value >> 16;
    return static_cast<float>(value & 0xffffU) / 65536.0F;
}
/** Scale the RGB channels of a color, keeping its alpha. */
Color shade(Color color, float factor) {
    const auto channel = [factor](std::uint8_t value) -> std::uint8_t {
        return static_cast<std::uint8_t>(std::clamp(value * factor, 0.0F, 255.0F));
    };
    return {channel(color.r), channel(color.g), channel(color.b), color.a};
}
/** Blend two colors linearly; the result is opaque unless both inputs are translucent. */
Color mix(Color a, Color b, float t) {
    const auto channel = [t](std::uint8_t x, std::uint8_t y) -> std::uint8_t {
        return static_cast<std::uint8_t>(std::clamp(x + (y - x) * t, 0.0F, 255.0F));
    };
    return {channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), channel(a.a, b.a)};
}
/** Replace only the alpha channel of a color. */
Color alpha(Color color, float opacity) {
    return {color.r, color.g, color.b,
            static_cast<std::uint8_t>(std::clamp(opacity, 0.0F, 1.0F) * static_cast<float>(color.a))};
}
/** Rock, soil and vein colors of one biome's cutaway. */
struct MinePalette {
    std::array<Color, 3> top;
    std::array<Color, 3> bottom;
    Color soft;
    Color hard;
    /** Biome vein accent: glass glints, ice, lava or luminous crystal. */
    Color vein;
    Color skyTop;
    Color skyBottom;
    Color ground;
    Color groundLight;
    Color hills;
};
/** Resolve the authored palette of a stage biome. */
const MinePalette &palette(game::StageBiome biome) {
    static const std::array<MinePalette, 5> palettes{{
        {{{Color{132, 98, 66}, Color{92, 74, 70}, Color{64, 70, 92}}},
         {{Color{70, 54, 46}, Color{48, 42, 50}, Color{34, 38, 58}}},
         {158, 126, 94},
         {80, 86, 104},
         {226, 200, 150},
         {38, 58, 78},
         {96, 120, 118},
         {84, 132, 76},
         {132, 178, 104},
         {60, 92, 74, 180}},
        {{{Color{78, 68, 82}, Color{58, 50, 68}, Color{44, 38, 58}}},
         {{Color{38, 34, 44}, Color{28, 24, 36}, Color{20, 18, 32}}},
         {112, 98, 106},
         {44, 40, 56},
         {196, 170, 255},
         {34, 26, 48},
         {98, 72, 92},
         {46, 40, 52},
         {124, 112, 146},
         {24, 20, 30, 220}},
        {{{Color{142, 164, 182}, Color{104, 126, 152}, Color{78, 98, 128}}},
         {{Color{88, 102, 122}, Color{62, 76, 100}, Color{42, 54, 80}}},
         {178, 192, 204},
         {98, 122, 150},
         {206, 240, 255},
         {104, 136, 168},
         {196, 214, 228},
         {226, 236, 244},
         {255, 255, 255},
         {170, 190, 210, 200}},
        {{{Color{96, 60, 46}, Color{74, 42, 36}, Color{58, 30, 28}}},
         {{Color{50, 30, 28}, Color{38, 22, 22}, Color{30, 16, 18}}},
         {124, 84, 64},
         {66, 50, 50},
         {255, 128, 44},
         {36, 18, 20},
         {132, 52, 30},
         {52, 36, 32},
         {255, 128, 44},
         {28, 16, 16, 230}},
        {{{Color{62, 70, 64}, Color{46, 54, 58}, Color{36, 42, 52}}},
         {{Color{30, 36, 36}, Color{22, 28, 34}, Color{16, 20, 30}}},
         {98, 108, 94},
         {52, 64, 68},
         {176, 255, 128},
         {14, 22, 28},
         {38, 72, 60},
         {48, 60, 52},
         {170, 240, 120},
         {22, 40, 36, 220}},
    }};
    return palettes[static_cast<std::size_t>(std::clamp(static_cast<int>(biome), 0, 4))];
}
/** Soil color for one row; deeper rows and deeper layers become darker. */
Color stratum(const MinePalette &colors, int row, int rows, int layer) {
    const std::size_t depth = static_cast<std::size_t>(std::clamp(layer, 0, 2));
    const float t = rows > 1 ? static_cast<float>(row) / static_cast<float>(rows - 1) : 0.0F;
    return mix(colors.top[depth], colors.bottom[depth], std::clamp(t, 0.0F, 1.0F));
}
/** Sample a closed circle outline for rings without a stroked-ellipse primitive. */
std::vector<ScreenPoint> ring(ScreenPoint center, float radius, int segments = 28) {
    std::vector<ScreenPoint> points;
    points.reserve(static_cast<std::size_t>(segments + 1));
    for (int index = 0; index <= segments; ++index) {
        const float angle = static_cast<float>(index) * Turn / static_cast<float>(segments);
        points.push_back({center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius});
    }
    return points;
}
/** Draw a rectangle outline as one closed polyline. */
void outlineRect(engine::Renderer &g, Rect box, float width, Color color) {
    g.line({{box.x, box.y},
            {box.x + box.w, box.y},
            {box.x + box.w, box.y + box.h},
            {box.x, box.y + box.h},
            {box.x, box.y}},
           width, color);
}
/** Draw a small faceted shard used for ore glints and cost chips. */
void shard(engine::Renderer &g, ScreenPoint center, float size, Color color, float tilt = 0.0F) {
    const float c = std::cos(tilt), s = std::sin(tilt);
    const auto point = [&](float x, float y) -> ScreenPoint {
        return {center.x + (x * c - y * s) * size, center.y + (x * s + y * c) * size};
    };
    g.polygon({point(0, -1), point(.7F, -.15F), point(.45F, .9F), point(-.45F, .9F), point(-.7F, -.15F)},
              color);
    g.polygon({point(0, -1), point(.7F, -.15F), point(0, -.05F)}, {255, 255, 255, 120});
    g.polygon({point(-.45F, .9F), point(.45F, .9F), point(0, -.05F)}, {0, 0, 0, 55});
}
/** Draw a four-point twinkle whose size follows a 0..1 intensity. */
void twinkle(engine::Renderer &g, ScreenPoint center, float size, float intensity) {
    if (intensity <= 0.05F)
        return;
    const float r = size * intensity;
    const Color glow{255, 250, 225, static_cast<std::uint8_t>(230 * intensity)};
    g.polygon({{center.x, center.y - r},
               {center.x + r * .22F, center.y - r * .22F},
               {center.x + r, center.y},
               {center.x + r * .22F, center.y + r * .22F},
               {center.x, center.y + r},
               {center.x - r * .22F, center.y + r * .22F},
               {center.x - r, center.y},
               {center.x - r * .22F, center.y - r * .22F}},
              glow);
}
/** Draw text with a one-pixel dark offset so it stays legible on textured rock. */
void shadowText(engine::Renderer &g, const std::string &value, ScreenPoint at, float size, Color color,
                float align = .5F) {
    const ScreenPoint rounded{std::round(at.x), std::round(at.y)};
    g.text(value, {rounded.x + 1, rounded.y + 1}, size,
           {12, 10, 12, static_cast<std::uint8_t>(color.a * .8F)}, align);
    g.text(value, rounded, size, color, align);
}
} // namespace

Color mineOreColor(game::Ore ore) {
    switch (ore) {
    case game::Ore::Iron:
        return {181, 203, 206};
    case game::Ore::Copper:
        return {226, 151, 103};
    case game::Ore::Crystal:
        return theme::purple;
    case game::Ore::Gold:
        return theme::gold;
    default:
        return {176, 160, 140};
    }
}
const char *mineOreName(game::Ore ore) {
    switch (ore) {
    case game::Ore::Iron:
        return "철";
    case game::Ore::Copper:
        return "구리";
    case game::Ore::Crystal:
        return "수정";
    case game::Ore::Gold:
        return "금";
    default:
        return "암석";
    }
}
Color clueColor(int clue) {
    switch (clue) {
    case 1:
        return {120, 182, 255};
    case 2:
        return {124, 214, 128};
    case 3:
        return {255, 112, 98};
    case 4:
        return {196, 140, 255};
    case 5:
        return {255, 170, 76};
    default:
        return {255, 226, 120};
    }
}
void drawPickaxe(engine::Renderer &g, ScreenPoint center, float size, Color head, float angle) {
    const float c = std::cos(angle), s = std::sin(angle);
    const auto point = [&](float x, float y) -> ScreenPoint {
        return {center.x + (x * c - y * s) * size, center.y + (x * s + y * c) * size};
    };
    g.line({point(-.42F, .42F), point(.3F, -.3F)}, std::max(1.5F, size * .13F), {150, 101, 58});
    g.line({point(-.18F, -.62F), point(.3F, -.36F), point(.62F, .12F)}, std::max(1.5F, size * .15F), head);
}

const char *relicName(game::Relic relic) {
    switch (relic) {
    case game::Relic::Battery:
        return "고대 축전지";
    case game::Relic::Charm:
        return "광부의 부적";
    case game::Relic::Ward:
        return "둥지 봉인석";
    case game::Relic::Lens:
        return "심층 탐지경";
    }
    return "유물";
}
std::string relicEffect(game::Relic relic, const game::RunBalance &balance) {
    switch (relic) {
    case game::Relic::Battery:
        return "전투 시작 에너지 +" + std::to_string(balance.relicEnergy);
    case game::Relic::Charm:
        return "정비마다 굴착 +" + std::to_string(balance.relicDigs);
    case game::Relic::Ward:
        return "둥지 증원 -" + std::to_string(balance.relicNestReduction);
    case game::Relic::Lens:
        return "정비마다 무료 스캔 +" + std::to_string(balance.relicScans);
    }
    return {};
}
Color relicColor(game::Relic relic) {
    switch (relic) {
    case game::Relic::Battery:
        return {126, 232, 150};
    case game::Relic::Charm:
        return theme::gold;
    case game::Relic::Ward:
        return {236, 120, 104};
    case game::Relic::Lens:
        return {120, 196, 255};
    }
    return theme::gold;
}
void drawRelicIcon(engine::Renderer &g, game::Relic relic, ScreenPoint c, float size) {
    const Color tint = relicColor(relic);
    const float r = size * .5F;
    g.circle(c, r, {12, 14, 20, 235});
    g.circle(c, r * .88F, shade(tint, .32F));
    g.line(ring(c, r * .88F, 20), std::max(1.0F, size * .05F), alpha(tint, .8F));
    if (relic == game::Relic::Battery) {
        g.rect({c.x - r * .36F, c.y - r * .44F, r * .72F, r * .96F}, tint, r * .12F);
        g.rect({c.x - r * .14F, c.y - r * .58F, r * .28F, r * .16F}, tint, r * .04F);
        g.polygon({{c.x + r * .06F, c.y - r * .34F},
                   {c.x - r * .18F, c.y + r * .06F},
                   {c.x - r * .01F, c.y + r * .06F},
                   {c.x - r * .08F, c.y + r * .4F},
                   {c.x + r * .2F, c.y - r * .04F},
                   {c.x + r * .03F, c.y - r * .04F}},
                  {20, 40, 28});
    } else if (relic == game::Relic::Charm) {
        g.line(ring({c.x, c.y + r * .08F}, r * .42F, 18), std::max(1.2F, size * .08F), tint);
        g.line({{c.x - r * .2F, c.y - r * .52F}, {c.x, c.y - r * .32F}, {c.x + r * .2F, c.y - r * .52F}},
               std::max(1.0F, size * .05F), tint);
        drawPickaxe(g, {c.x, c.y + r * .08F}, r * .62F, {236, 240, 244}, 0);
    } else if (relic == game::Relic::Ward) {
        std::vector<ScreenPoint> stone;
        for (int corner = 0; corner < 6; ++corner) {
            const float angle = Turn * (static_cast<float>(corner) + .5F) / 6.0F;
            stone.push_back({c.x + std::cos(angle) * r * .6F, c.y + std::sin(angle) * r * .6F});
        }
        g.polygon(stone, {150, 136, 150});
        g.line({{c.x - r * .3F, c.y}, {c.x, c.y - r * .16F}, {c.x + r * .3F, c.y}},
               std::max(1.2F, size * .07F), tint);
        g.line({{c.x - r * .3F, c.y}, {c.x, c.y + r * .16F}, {c.x + r * .3F, c.y}},
               std::max(1.2F, size * .07F), tint);
        g.line({{c.x - r * .34F, c.y + r * .3F}, {c.x + r * .34F, c.y - r * .3F}},
               std::max(1.2F, size * .07F), {40, 20, 24});
    } else {
        const ScreenPoint lens{c.x - r * .1F, c.y - r * .1F};
        g.circle(lens, r * .36F, alpha(tint, .55F));
        g.line(ring(lens, r * .36F, 18), std::max(1.2F, size * .08F), {230, 236, 240});
        g.line({{lens.x + r * .26F, lens.y + r * .26F}, {c.x + r * .46F, c.y + r * .46F}},
               std::max(1.5F, size * .11F), {200, 170, 120});
        g.circle({lens.x - r * .12F, lens.y - r * .12F}, r * .08F, {255, 255, 255, 200});
    }
}
const char *mineSpecialName(game::MineSpecial special) {
    switch (special) {
    case game::MineSpecial::Relic:
        return "보물 상자";
    case game::MineSpecial::Fossil:
        return "카드 화석";
    case game::MineSpecial::Unstable:
        return "붕괴 암반";
    case game::MineSpecial::Energy:
        return "에너지 수정";
    case game::MineSpecial::None:
        break;
    }
    return "암석";
}
Color mineSpecialColor(game::MineSpecial special) {
    switch (special) {
    case game::MineSpecial::Relic:
        return {255, 214, 120};
    case game::MineSpecial::Fossil:
        return {232, 214, 178};
    case game::MineSpecial::Unstable:
        return {255, 150, 110};
    case game::MineSpecial::Energy:
        return {120, 240, 210};
    case game::MineSpecial::None:
        break;
    }
    return {206, 196, 182};
}
void drawSpecialGlyph(engine::Renderer &g, game::MineSpecial special, ScreenPoint c, float size) {
    const float s = size;
    if (special == game::MineSpecial::Relic) {
        const Rect body{c.x - s * .42F, c.y - s * .08F, s * .84F, s * .44F};
        g.rect({body.x, body.y + s * .04F, body.w, body.h}, {0, 0, 0, 90}, s * .06F);
        g.rect(body, {132, 82, 40}, s * .05F);
        g.rect({body.x, body.y - s * .26F, body.w, s * .3F}, {156, 98, 48}, s * .12F);
        g.rect({body.x, body.y - s * .03F, body.w, s * .07F}, {238, 190, 84});
        for (float side : {.12F, .88F})
            g.rect({body.x + body.w * side - s * .04F, body.y - s * .24F, s * .08F, body.h + s * .24F},
                   {238, 190, 84}, s * .02F);
        g.rect({c.x - s * .07F, body.y - s * .02F, s * .14F, s * .16F}, {255, 226, 130}, s * .03F);
        g.circle({c.x, body.y + s * .05F}, s * .025F, {60, 36, 20});
    } else if (special == game::MineSpecial::Fossil) {
        g.rect({c.x - s * .32F, c.y - s * .42F, s * .64F, s * .84F}, {214, 196, 160}, s * .1F);
        g.rect({c.x - s * .25F, c.y - s * .35F, s * .5F, s * .7F}, {176, 156, 120}, s * .07F);
        std::vector<ScreenPoint> spiral;
        for (int step = 0; step <= 22; ++step) {
            const float t = static_cast<float>(step) / 22.0F;
            const float angle = t * Turn * 1.6F;
            spiral.push_back({c.x + std::cos(angle) * s * (.04F + t * .17F),
                              c.y + std::sin(angle) * s * (.04F + t * .17F)});
        }
        g.line(spiral, std::max(1.0F, s * .06F), {96, 76, 52});
    } else if (special == game::MineSpecial::Energy) {
        shard(g, {c.x - s * .2F, c.y + s * .12F}, s * .26F, {90, 210, 190}, -.4F);
        shard(g, {c.x + s * .2F, c.y + s * .14F}, s * .24F, {90, 210, 190}, .45F);
        shard(g, {c.x, c.y - s * .06F}, s * .38F, {140, 255, 224});
        g.polygon({{c.x + s * .04F, c.y - s * .3F},
                   {c.x - s * .1F, c.y - s * .02F},
                   {c.x, c.y - s * .02F},
                   {c.x - s * .05F, c.y + s * .2F},
                   {c.x + s * .11F, c.y - s * .08F},
                   {c.x + s * .02F, c.y - s * .08F}},
                  {255, 255, 220});
    } else if (special == game::MineSpecial::Unstable) {
        const Color dark{40, 16, 12};
        g.line({{c.x - s * .4F, c.y - s * .34F},
                {c.x - s * .12F, c.y - s * .08F},
                {c.x - s * .2F, c.y + s * .1F},
                {c.x + s * .06F, c.y + s * .38F}},
               std::max(1.5F, s * .08F), dark);
        g.line({{c.x - s * .12F, c.y - s * .08F},
                {c.x + s * .2F, c.y - s * .2F},
                {c.x + s * .4F, c.y - s * .4F}},
               std::max(1.2F, s * .06F), dark);
        g.line({{c.x - s * .2F, c.y + s * .1F},
                {c.x + s * .24F, c.y + s * .12F},
                {c.x + s * .4F, c.y + s * .3F}},
               std::max(1.2F, s * .06F), dark);
        g.polygon({{c.x + s * .3F, c.y - s * .46F},
                   {c.x + s * .42F, c.y - s * .3F},
                   {c.x + s * .26F, c.y - s * .28F}},
                  {255, 150, 110});
    }
}
float labelWidth(const std::string &value, float size) {
    float width = 0;
    for (std::size_t index = 0; index < value.size();) {
        const unsigned char lead = static_cast<unsigned char>(value[index]);
        const std::size_t length = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
        width += lead < 0x80 ? size * .56F : size * .98F;
        index += length;
    }
    return width;
}

void View::underground(engine::Renderer &g, const engine::Ui &ui, const game::Run &run, const State &state) {
    const game::RunState &live = run.state();
    const bool still = state.app.preferences.reducedMotion;
    const float time = still ? 0.0F : state.play.visualTime;
    const float tile = layout.mineTile;
    const int columns = layout.mineColumns, rows = layout.mineRows;
    const int layer = live.mineLayer;
    const Rect view = layout.mineViewport;
    const game::MineLayerDefinition &definition =
        layer == 0 ? run.catalog().mine : run.catalog().deeperMines[static_cast<std::size_t>(layer - 1)];
    const MinePalette &colors = palette(definition.biome);
    const game::StageBiome biome = definition.biome;
    const float boardLeft = layout.mineOrigin.x, boardRight = boardLeft + tile * static_cast<float>(columns);

    g.gradientRect(layout.world, {46, 38, 34}, {20, 20, 24}, 12);
    g.beginClip(view);
    // Ground above the first row: sky and grass on the first layer, a rock ceiling deeper down.
    const float surface = layout.mineOrigin.y;
    if (surface > view.y) {
        if (layer == 0) {
            g.gradientRect({view.x, view.y, view.w, surface - view.y}, colors.skyTop, colors.skyBottom);
            if (biome == game::StageBiome::Lava || biome == game::StageBiome::Core) {
                const float glow = still ? .8F : .7F + .3F * std::sin(time * 1.6F);
                g.gradientRect({view.x, surface - tile * .9F, view.w, tile * .7F}, alpha(colors.vein, 0),
                               alpha(colors.vein, .22F * glow));
            }
            for (int index = 0; index < 9; ++index) {
                const float x = view.x + view.w * (static_cast<float>(index) + .5F) / 9.0F;
                const float h = tile * (.25F + noise(index, 9, 1) * .35F);
                const float base = surface - tile * .18F;
                if (biome == game::StageBiome::Obsidian || biome == game::StageBiome::Core) {
                    const float lean = (noise(index, 9, 2) - .5F) * tile * .3F;
                    g.polygon({{x - tile * .16F, base}, {x + lean, base - h * 1.3F}, {x + tile * .16F, base}},
                              colors.hills);
                    g.line({{x - tile * .05F, base - h * .2F}, {x + lean * .8F, base - h * 1.1F}},
                           std::max(1.0F, tile * .025F), alpha(colors.vein, .55F));
                } else {
                    g.polygon({{x - tile * .55F, base}, {x, base - h}, {x + tile * .55F, base}},
                              colors.hills);
                    if (biome == game::StageBiome::Tundra)
                        g.polygon({{x - tile * .18F, base - h * .67F},
                                   {x, base - h},
                                   {x + tile * .18F, base - h * .67F}},
                                  {250, 252, 255, 230});
                    if (biome == game::StageBiome::Lava && index % 3 == 1) {
                        g.circle({x, base - h}, tile * .1F, alpha(colors.vein, .7F));
                        g.line({{x, base - h}, {x - tile * .08F, base - h * .5F}},
                               std::max(1.0F, tile * .04F), alpha(colors.vein, .6F));
                    }
                }
            }
            g.rect({view.x, surface - tile * .2F, view.w, tile * .2F}, colors.ground);
            g.rect({view.x, surface - tile * .2F, view.w, tile * .05F}, colors.groundLight);
            const float shaft =
                layout.mineOrigin.x + (static_cast<float>(run.catalog().mine.start % columns) + .5F) * tile;
            const float peak = surface - tile * .56F;
            const Color timber{120, 80, 46};
            g.line({{shaft - tile * .42F, surface}, {shaft, peak}, {shaft + tile * .42F, surface}},
                   tile * .06F, timber);
            g.line(
                {{shaft - tile * .24F, surface - tile * .24F}, {shaft + tile * .24F, surface - tile * .24F}},
                tile * .045F, timber);
            g.circle({shaft, peak}, tile * .1F, {70, 50, 34});
            g.circle({shaft, peak}, tile * .05F, {190, 160, 110});
            g.line({{shaft + tile * .1F, peak}, {shaft + tile * .1F, surface + tile * .3F}}, 1,
                   {210, 190, 150, 160});
        } else {
            g.rect({view.x, view.y, view.w, surface - view.y}, shade(stratum(colors, 0, rows, layer), .55F));
            for (int index = 0; index < 12; ++index) {
                const float x =
                    view.x + view.w * (static_cast<float>(index) + noise(index, layer, 3)) / 12.0F;
                const float h = tile * (.12F + noise(index, layer, 4) * .22F);
                g.polygon({{x - tile * .08F, surface}, {x, surface + h}, {x + tile * .08F, surface}},
                          shade(stratum(colors, 0, rows, layer), .7F));
            }
        }
    }
    // Soil strata continue past the diggable columns, which are framed as darker bedrock.
    for (int row = 0; row < rows; ++row) {
        const float y = surface + static_cast<float>(row) * tile;
        if (y > view.y + view.h || y + tile < view.y)
            continue;
        const Color soil = stratum(colors, row, rows, layer);
        g.rect({view.x, y, view.w, tile + 1}, soil);
        std::vector<ScreenPoint> seam;
        for (int step = 0; step <= 12; ++step) {
            const float x = view.x + view.w * static_cast<float>(step) / 12.0F;
            seam.push_back({x, y + tile * (.15F + noise(step, row + layer * 31, 5) * .12F)});
        }
        g.line(seam, std::max(1.0F, tile * .03F), alpha(shade(soil, .72F), .7F));
        for (int index = 0; index < 6; ++index) {
            const float side = noise(index, row + layer * 17, 6);
            const float x = side < .5F ? view.x + (boardLeft - view.x) * side * 2
                                       : boardRight + (view.x + view.w - boardRight) * (side - .5F) * 2;
            g.circle({x, y + tile * (.2F + noise(index, row, 7) * .6F)},
                     tile * (.03F + noise(index, row, 8) * .05F), shade(soil, .78F));
        }
        // Biome veins in the bedrock margins: glass glints, ice, glowing lava or luminous crystal.
        if (biome != game::StageBiome::Meadow)
            for (int index = 0; index < 3; ++index) {
                const float side = noise(index, row + layer * 13, 90);
                const float x = side < .5F ? view.x + (boardLeft - view.x) * side * 2
                                           : boardRight + (view.x + view.w - boardRight) * (side - .5F) * 2;
                const float cy = y + tile * (.25F + noise(index, row, 91) * .5F);
                const float phase = still ? .8F : .65F + .35F * std::sin(time * 2.2F + row + index * 2.0F);
                if (biome == game::StageBiome::Obsidian)
                    shard(g, {x, cy}, tile * .14F, Color{30, 24, 40}, noise(index, row, 92) * 2 - 1);
                else if (biome == game::StageBiome::Tundra)
                    g.line({{x - tile * .3F, cy - tile * .1F},
                            {x, cy + tile * .04F},
                            {x + tile * .32F, cy - tile * .06F}},
                           std::max(1.2F, tile * .05F), alpha(colors.vein, .55F));
                else if (biome == game::StageBiome::Lava) {
                    g.line({{x - tile * .34F, cy},
                            {x - tile * .1F, cy + tile * .08F},
                            {x + tile * .3F, cy - tile * .04F}},
                           std::max(2.0F, tile * .09F), alpha(colors.vein, .18F * phase));
                    g.line({{x - tile * .34F, cy},
                            {x - tile * .1F, cy + tile * .08F},
                            {x + tile * .3F, cy - tile * .04F}},
                           std::max(1.0F, tile * .03F), alpha(colors.vein, .8F * phase));
                } else {
                    const Color lit = mix(colors.vein, Color{246, 222, 104}, noise(index, row, 93));
                    g.circle({x, cy}, tile * .3F, alpha(lit, .1F * phase));
                    g.circle({x, cy}, tile * .16F, alpha(lit, .2F * phase));
                    shard(g, {x - tile * .08F, cy + tile * .04F}, tile * .08F, lit, -.6F);
                    shard(g, {x + tile * .07F, cy + tile * .05F}, tile * .07F, lit, .5F);
                    shard(g, {x, cy}, tile * .13F, mix(lit, Color{255, 255, 230}, .3F * phase));
                }
            }
    }
    const float boardBottom = surface + tile * static_cast<float>(rows);
    for (const Rect &side :
         {Rect{view.x, surface, boardLeft - view.x, boardBottom - surface},
          Rect{boardRight, surface, view.x + view.w - boardRight, boardBottom - surface}}) {
        g.rect(side, {14, 10, 14, 120});
        const float bottomY = side.y + side.h;
        for (float hatch = side.y - side.w; hatch < bottomY; hatch += tile * .5F) {
            const float startX = side.x + std::max(0.0F, side.y - hatch);
            const float endX = side.x + std::min(side.w, bottomY - hatch);
            if (endX > startX)
                g.line({{startX, hatch + (startX - side.x)}, {endX, hatch + (endX - side.x)}}, 1,
                       {0, 0, 0, 40});
        }
    }
    if (boardBottom < view.y + view.h)
        g.rect({view.x, boardBottom, view.w, view.y + view.h - boardBottom}, {14, 12, 16});
    if (!layout.compact && boardLeft - view.x > 46)
        for (int row = 1; row < rows; row += 2) {
            const float y = surface + static_cast<float>(row) * tile + tile * .5F;
            g.text("-" + std::to_string((row + 1) * 5 + layer * 60) + "m",
                   {std::round(boardLeft - 10), std::round(y - 6)}, 10, {222, 200, 168, 110}, 1);
        }

    const auto dug = [&](int index) -> bool {
        return index >= 0 && index < static_cast<int>(live.mine.size()) && live.mine[index].dug;
    };
    const engine::Widget *pointed = ui.hit(ui.pointerPosition);
    const int hovered =
        pointed && pointed->id == World && !ui.dragging ? layout.mineCell(ui.pointerPosition) : -1;
    const bool jobActive = live.job.active;
    const float walkTime = jobActive
                               ? static_cast<float>(std::max<std::size_t>(live.job.path.size(), 1) - 1) *
                                     run.catalog().balance.minerStep
                               : 0.0F;
    const float digProgress =
        jobActive && live.job.duration > walkTime
            ? std::clamp((live.job.elapsed - walkTime) / (live.job.duration - walkTime), 0.0F, 1.0F)
            : 0.0F;
    const float pulse = .5F + .5F * std::sin(time * 4.0F);
    // Distance from the nearest open tunnel, used to fade rock that the lanterns do not reach.
    std::vector<int> distance(live.mine.size(), 99);
    std::vector<int> frontier;
    for (int index = 0; index < static_cast<int>(live.mine.size()); ++index)
        if (live.mine[index].dug) {
            distance[static_cast<std::size_t>(index)] = 0;
            frontier.push_back(index);
        }
    for (std::size_t head = 0; head < frontier.size(); ++head) {
        const int cell = frontier[head];
        const int cx = cell % columns, cy = cell / columns;
        for (const std::array<int, 2> &step :
             std::array<std::array<int, 2>, 4>{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}}) {
            const int nx = cx + step[0], ny = cy + step[1];
            if (nx < 0 || ny < 0 || nx >= columns || ny >= rows)
                continue;
            const std::size_t next = static_cast<std::size_t>(ny * columns + nx);
            if (distance[next] > distance[static_cast<std::size_t>(cell)] + 1) {
                distance[next] = distance[static_cast<std::size_t>(cell)] + 1;
                frontier.push_back(static_cast<int>(next));
            }
        }
    }

    for (int index = 0; index < static_cast<int>(live.mine.size()); ++index) {
        const Rect cellBox = layout.mineCellBounds(index);
        if (cellBox.y > view.y + view.h || cellBox.y + tile < view.y)
            continue;
        const game::MineCell &cell = live.mine[index];
        const int x = index % columns, y = index / columns;
        const Color soil = stratum(colors, y, rows, layer);
        if (cell.dug) {
            const bool left = x > 0 && dug(index - 1), right = x < columns - 1 && dug(index + 1);
            const bool up = y > 0 && dug(index - columns), down = y < rows - 1 && dug(index + columns);
            const float inset = tile * .09F;
            const Rect hole{cellBox.x + (left ? 0 : inset), cellBox.y + (up ? 0 : inset),
                            tile - (left ? 0 : inset) - (right ? 0 : inset),
                            tile - (up ? 0 : inset) - (down ? 0 : inset)};
            g.rect({hole.x - 1, hole.y - 1, hole.w + 2, hole.h + 2}, shade(soil, .45F), tile * .1F);
            g.gradientRect(hole, {24, 20, 22}, {48, 38, 32}, tile * .08F);
            if (!down)
                g.rect({hole.x, hole.y + hole.h - tile * .13F, hole.w, tile * .13F}, {86, 66, 48},
                       tile * .04F);
            const Color wood{146, 98, 54};
            const Color woodDark{98, 64, 36};
            if (!up) {
                g.rect({hole.x, hole.y, hole.w, tile * .085F}, wood, 2);
                g.rect({hole.x, hole.y + tile * .065F, hole.w, tile * .025F}, woodDark);
            }
            if (!left)
                g.rect({hole.x + tile * .03F, hole.y, tile * .07F, hole.h}, wood, 2);
            if (!right)
                g.rect({hole.x + hole.w - tile * .1F, hole.y, tile * .07F, hole.h}, wood, 2);
            if (up || down) {
                const float top = up ? cellBox.y : cellBox.y + tile * .5F;
                const float bottom = down ? cellBox.y + tile : cellBox.y + tile * .5F;
                const float cx = cellBox.x + tile * .5F;
                g.line({{cx - tile * .14F, top}, {cx - tile * .14F, bottom}}, tile * .04F, woodDark);
                g.line({{cx + tile * .14F, top}, {cx + tile * .14F, bottom}}, tile * .04F, woodDark);
                for (float rung = top + tile * .08F; rung < bottom; rung += tile * .2F)
                    g.line({{cx - tile * .14F, rung}, {cx + tile * .14F, rung}}, tile * .035F, wood);
            }
            if (!up && noise(index, layer, 11) < .45F) {
                const ScreenPoint lamp{cellBox.x + tile * (.3F + noise(index, layer, 12) * .4F),
                                       hole.y + tile * .26F};
                const float flicker = still ? 1.0F : .85F + .15F * std::sin(time * 9.0F + index);
                g.circle(lamp, tile * .42F * flicker, {255, 196, 110, 26});
                g.circle(lamp, tile * .24F * flicker, {255, 206, 120, 46});
                g.line({{lamp.x, hole.y + tile * .08F}, {lamp.x, lamp.y - tile * .05F}}, 1, {60, 46, 36});
                g.rect({lamp.x - tile * .045F, lamp.y - tile * .05F, tile * .09F, tile * .11F},
                       {255, 222, 150}, 2);
            }
            if (cell.rubble)
                for (int stone = 0; stone < 5; ++stone) {
                    const float px = hole.x + hole.w * (.15F + noise(index, stone, 100) * .7F);
                    const float size = tile * (.06F + noise(index, stone, 101) * .06F);
                    const float floor = hole.y + hole.h - tile * .1F;
                    g.polygon({{px - size, floor},
                               {px - size * .4F, floor - size * 1.1F},
                               {px + size * .7F, floor - size * .8F},
                               {px + size, floor}},
                              shade(soil, .75F + noise(index, stone, 102) * .3F));
                }
            if (cell.special == game::MineSpecial::Relic) {
                const ScreenPoint chest{cellBox.x + tile * .5F, hole.y + hole.h - tile * .24F};
                g.rect({chest.x - tile * .16F, chest.y - tile * .06F, tile * .32F, tile * .16F},
                       {110, 70, 36}, tile * .03F);
                g.polygon({{chest.x - tile * .16F, chest.y - tile * .06F},
                           {chest.x + tile * .16F, chest.y - tile * .06F},
                           {chest.x + tile * .2F, chest.y - tile * .22F},
                           {chest.x - tile * .12F, chest.y - tile * .22F}},
                          {140, 90, 44});
                g.rect({chest.x - tile * .16F, chest.y - tile * .07F, tile * .32F, tile * .03F},
                       {220, 176, 80});
            } else if (cell.special == game::MineSpecial::Energy)
                shard(g, {cellBox.x + tile * .5F, hole.y + hole.h - tile * .2F}, tile * .1F, {70, 120, 112});
            if (cell.nest) {
                g.circle({cellBox.x + tile * .5F, cellBox.y + tile * .62F}, tile * .2F, {96, 26, 30, 200});
                g.polygon({{cellBox.x + tile * .36F, cellBox.y + tile * .6F},
                           {cellBox.x + tile * .44F, cellBox.y + tile * .48F},
                           {cellBox.x + tile * .5F, cellBox.y + tile * .58F},
                           {cellBox.x + tile * .58F, cellBox.y + tile * .46F},
                           {cellBox.x + tile * .64F, cellBox.y + tile * .6F}},
                          {200, 186, 160, 170});
            }
            if (cell.clueKnown && cell.clue > 0) {
                const ScreenPoint center{cellBox.x + tile * .5F, cellBox.y + tile * .5F};
                g.circle(center, tile * .22F, {16, 14, 18, 190});
                shadowText(g, std::to_string(cell.clue), {center.x, center.y - tile * .2F}, tile * .4F,
                           clueColor(cell.clue));
            }
            continue;
        }
        // Undug rock: a continuous mass with seams, darker the farther it lies from open tunnels.
        const bool reachable = run.reachable(index);
        const bool hard = cell.grade >= 2;
        const bool target = jobActive && live.job.target == index;
        const bool surveyed = cell.clueKnown || cell.info;
        const float shake = target && digProgress > 0 && !still
                                ? std::sin(time * 46.0F) * tile * .025F * (1 + digProgress)
                                : 0.0F;
        const Rect box{cellBox.x + shake, cellBox.y, tile, tile};
        const int far = distance[static_cast<std::size_t>(index)];
        Color rock = hard ? mix(colors.hard, soil, .3F) : mix(colors.soft, soil, .5F);
        if (cell.info && cell.nest)
            rock = mix(rock, Color{120, 44, 40}, .55F);
        rock = shade(rock, (surveyed ? 1.05F : .97F) * (.95F + noise(index, layer, 1) * .1F));
        g.rect(box, rock);
        if (reachable)
            g.gradientRect({box.x + 2, box.y + 2, box.w - 4, box.h - 4}, shade(rock, 1.2F), shade(rock, .9F),
                           tile * .12F);
        if (hard) {
            const Color band = shade(rock, .68F);
            for (float offset : {.2F, .48F, .76F}) {
                std::vector<ScreenPoint> seam;
                for (int step = 0; step <= 3; ++step) {
                    const float px = box.x + box.w * static_cast<float>(step) / 3.0F;
                    const float wave =
                        std::sin((px - layout.mineOrigin.x) / tile * 2.1F + static_cast<float>(y));
                    seam.push_back({px, box.y + box.h * offset + tile * .05F * wave});
                }
                g.line(seam, std::max(1.5F, tile * .06F), band);
            }
            g.line({{box.x + tile * .1F, box.y + tile * .34F}, {box.x + tile * .4F, box.y + tile * .3F}},
                   std::max(1.0F, tile * .025F), alpha(shade(rock, 1.5F), .5F));
        } else {
            for (int pebble = 0; pebble < 3; ++pebble)
                g.circle({box.x + box.w * (.15F + noise(index, layer, 20 + pebble) * .7F),
                          box.y + box.h * (.15F + noise(index, layer, 30 + pebble) * .7F)},
                         tile * (.025F + noise(index, layer, 40 + pebble) * .03F), shade(rock, .74F));
            if (noise(index, layer, 45) < .35F) {
                const ScreenPoint stone{box.x + box.w * (.25F + noise(index, layer, 46) * .5F),
                                        box.y + box.h * (.3F + noise(index, layer, 47) * .45F)};
                g.circle(stone, tile * .09F, shade(rock, .8F));
                g.circle({stone.x - tile * .025F, stone.y - tile * .025F}, tile * .045F,
                         alpha(shade(rock, 1.35F), .7F));
            }
            if (noise(index, layer, 48) < .55F) {
                const float angle = noise(index, layer, 49) * Turn;
                const ScreenPoint from{box.x + box.w * (.25F + noise(index, layer, 50) * .5F),
                                       box.y + box.h * (.25F + noise(index, layer, 51) * .5F)};
                const float length = tile * (.12F + noise(index, layer, 52) * .12F);
                g.line({from,
                        {from.x + std::cos(angle) * length * .5F + tile * .03F,
                         from.y + std::sin(angle) * length * .5F},
                        {from.x + std::cos(angle) * length, from.y + std::sin(angle) * length}},
                       1, alpha(shade(rock, .55F), .75F));
            }
        }
        const float vein = noise(index, layer, 95);
        if (biome == game::StageBiome::Obsidian && vein < (hard ? .55F : .2F)) {
            const float gx = box.x + box.w * (.15F + noise(index, layer, 96) * .45F);
            const float gy = box.y + box.h * (.12F + noise(index, layer, 97) * .4F);
            const float span = tile * (.18F + vein * .3F);
            g.polygon({{gx, gy}, {gx + span, gy - span * .3F}, {gx + span * .35F, gy + span * .8F}},
                      alpha(colors.vein, hard ? .26F : .14F));
            g.line({{gx + span * .1F, gy + span * .05F}, {gx + span * .8F, gy - span * .2F}},
                   std::max(1.0F, tile * .02F), alpha({255, 255, 255, 255}, hard ? .45F : .25F));
        } else if (biome == game::StageBiome::Tundra && vein < .45F)
            g.line({{box.x, box.y + box.h * (.3F + vein)},
                    {box.x + box.w * .45F, box.y + box.h * (.45F + vein * .5F)},
                    {box.x + box.w, box.y + box.h * (.25F + vein)}},
                   std::max(1.2F, tile * .045F), alpha(colors.vein, hard ? .6F : .4F));
        else if (biome == game::StageBiome::Lava && vein < .35F) {
            const float glow = still ? .8F : .6F + .4F * std::sin(time * 2.6F + index * .9F);
            const std::vector<ScreenPoint> crack{{box.x + box.w * .1F, box.y + box.h * (.6F + vein * .5F)},
                                                 {box.x + box.w * .45F, box.y + box.h * (.5F + vein * .3F)},
                                                 {box.x + box.w * .9F, box.y + box.h * (.7F - vein * .4F)}};
            g.line(crack, std::max(2.5F, tile * .1F), alpha(colors.vein, .2F * glow));
            g.line(crack, std::max(1.0F, tile * .035F),
                   alpha(mix(colors.vein, Color{255, 230, 140}, .4F), glow));
        } else if (biome == game::StageBiome::Core && vein < .45F) {
            const float glow = still ? .8F : .6F + .4F * std::sin(time * 1.8F + index * 1.3F);
            const ScreenPoint speck{box.x + box.w * (.2F + noise(index, layer, 96) * .6F),
                                    box.y + box.h * (.25F + noise(index, layer, 97) * .45F)};
            const Color lit = mix(colors.vein, Color{246, 222, 104}, vein * 2);
            g.circle(speck, tile * .2F, alpha(lit, .14F * glow));
            g.circle(speck, tile * .11F, alpha(lit, .22F * glow));
            shard(g, {speck.x - tile * .05F, speck.y + tile * .02F}, tile * .06F, lit, -.5F);
            shard(g, speck, tile * .09F, mix(lit, Color{255, 255, 230}, .35F * glow));
        }
        if (cell.special == game::MineSpecial::Unstable && !cell.info && far <= 1) {
            const Color hair = alpha(shade(rock, .45F), .85F);
            g.line({{box.x + box.w * .2F, box.y + box.h * .18F},
                    {box.x + box.w * .42F, box.y + box.h * .4F},
                    {box.x + box.w * .36F, box.y + box.h * .62F},
                    {box.x + box.w * .6F, box.y + box.h * .84F}},
                   1, hair);
            g.line({{box.x + box.w * .42F, box.y + box.h * .4F}, {box.x + box.w * .74F, box.y + box.h * .3F}},
                   1, hair);
        }
        const Color seam = alpha(shade(rock, .58F), .55F);
        g.line({{box.x, box.y + box.h}, {box.x + box.w, box.y + box.h}}, 1, seam);
        g.line({{box.x + box.w, box.y}, {box.x + box.w, box.y + box.h}}, 1, seam);
        if (far > 1) {
            const float dark = std::min(.46F, .16F * static_cast<float>(far - 1)) * (cell.info ? .5F : 1.0F);
            g.rect(box, alpha({8, 6, 10, 255}, dark));
        }
        if (target && digProgress > 0) {
            const int cracks = 1 + static_cast<int>(digProgress * 4.0F);
            for (int crack = 0; crack < cracks; ++crack) {
                const float angle = noise(index, crack, 60) * Turn;
                const ScreenPoint c{box.x + box.w * .5F, box.y + box.h * .5F};
                const float length = tile * (.18F + .22F * digProgress);
                g.line({c,
                        {c.x + std::cos(angle) * length * .5F + tile * .04F,
                         c.y + std::sin(angle) * length * .5F},
                        {c.x + std::cos(angle) * length, c.y + std::sin(angle) * length}},
                       std::max(1.2F, tile * .03F), {30, 20, 16, 220});
            }
        }
        if (cell.info && cell.nest) {
            const ScreenPoint c{box.x + box.w * .5F, box.y + box.h * .52F};
            g.circle(c, tile * .24F, {34, 10, 12});
            g.circle(c, tile * .18F, {14, 4, 6});
            const float glow = still ? 1.0F : .7F + .3F * pulse;
            for (float side : {-1.0F, 1.0F}) {
                g.circle({c.x + side * tile * .075F, c.y - tile * .02F}, tile * .045F,
                         {255, 70, 50, static_cast<std::uint8_t>(255 * glow)});
                g.circle({c.x + side * tile * .075F, c.y - tile * .02F}, tile * .09F,
                         {255, 60, 40, static_cast<std::uint8_t>(60 * glow)});
            }
            for (float side : {-1.0F, 1.0F})
                g.line({{c.x + side * tile * .3F, box.y + box.h * .12F},
                        {c.x + side * tile * .22F, box.y + box.h * .24F},
                        {c.x + side * tile * .32F, box.y + box.h * .32F}},
                       std::max(1.2F, tile * .035F), {70, 18, 18});
            outlineRect(g, {box.x + 1, box.y + 1, box.w - 2, box.h - 2}, std::max(1.5F, tile * .05F),
                        {235, 80, 64, static_cast<std::uint8_t>(110 + 120 * glow * pulse)});
        } else if (cell.info && cell.special != game::MineSpecial::None) {
            const Color accent = mineSpecialColor(cell.special);
            const ScreenPoint c{box.x + box.w * .5F, box.y + box.h * .42F};
            const float glow = still ? .8F : .6F + .4F * std::sin(time * 3.2F + index);
            if (cell.special == game::MineSpecial::Unstable) {
                g.rect({box.x + 2, box.y + 2, box.w - 4, box.h - 4}, {120, 40, 24, 70}, tile * .1F);
                drawSpecialGlyph(g, cell.special, {c.x, box.y + box.h * .45F}, tile * .78F);
                for (int pebble = 0; pebble < 3; ++pebble) {
                    const float fall = still ? .3F : std::fmod(time * .8F + pebble * .37F, 1.0F);
                    g.circle({box.x + box.w * (.3F + pebble * .2F), box.y + box.h * (.1F + fall * .55F)},
                             tile * .03F, alpha(shade(rock, .6F), 1 - fall));
                }
            } else {
                g.circle(c, tile * .36F, alpha(accent, .16F * glow));
                g.circle(c, tile * .24F, alpha(accent, .2F * glow));
                drawSpecialGlyph(g, cell.special, c,
                                 tile * (cell.special == game::MineSpecial::Relic ? .66F : .56F));
                if (cell.special == game::MineSpecial::Relic)
                    twinkle(g, {c.x + tile * .2F, c.y - tile * .18F}, tile * .1F, glow);
            }
            const std::string label = cell.special == game::MineSpecial::Energy
                                          ? "에너지 +" + std::to_string(cell.amount)
                                      : cell.special == game::MineSpecial::Relic  ? std::string("유물")
                                      : cell.special == game::MineSpecial::Fossil ? std::string("화석")
                                                                                  : std::string("붕괴");
            const float pillW = std::max(tile * .5F, labelWidth(label, tile * .19F) + tile * .14F);
            const Rect pill{box.x + (box.w - pillW) / 2, box.y + box.h - tile * .3F, pillW, tile * .24F};
            g.rect(pill, {18, 14, 16, 215}, tile * .12F);
            g.rect({pill.x, pill.y, pill.w, pill.h}, alpha(accent, .25F), tile * .12F);
            g.text(label, {std::round(pill.x + pill.w / 2), std::round(pill.y + tile * .03F)}, tile * .19F,
                   accent, .5F, pill.w);
        } else if (cell.info && cell.ore != game::Ore::Rock) {
            const Color color = mineOreColor(cell.ore);
            const ScreenPoint c{box.x + box.w * .5F, box.y + box.h * .42F};
            const std::array<ScreenPoint, 3> places{{{c.x - tile * .17F, c.y + tile * .04F},
                                                     {c.x + tile * .02F, c.y - tile * .1F},
                                                     {c.x + tile * .18F, c.y + tile * .06F}}};
            for (std::size_t piece = 0; piece < places.size(); ++piece)
                shard(g, places[piece], tile * (piece == 1 ? .15F : .1F), color,
                      (noise(index, static_cast<int>(piece), 70) - .5F) * 1.2F);
            const float sparkle = still ? .6F : std::max(0.0F, std::sin(time * 3.0F + index * 1.7F));
            twinkle(g, {places[1].x + tile * .08F, places[1].y - tile * .1F}, tile * .1F, sparkle);
            const std::string label = std::string(mineOreName(cell.ore)) + " " + std::to_string(cell.amount);
            const float pillW = tile * .62F;
            const Rect pill{box.x + (box.w - pillW) / 2, box.y + box.h - tile * .3F, pillW, tile * .24F};
            g.rect(pill, {18, 14, 16, 200}, tile * .12F);
            g.text(label, {std::round(pill.x + pill.w / 2), std::round(pill.y + tile * .03F)}, tile * .19F,
                   color, .5F);
        } else if (cell.info) {
            g.circle({box.x + box.w * .5F, box.y + box.h * .5F}, tile * .06F,
                     alpha({240, 228, 206, 255}, .35F));
        }
        if (hard && far <= 2 && !(cell.info && cell.nest)) {
            const bool unlocked = live.tool >= cell.grade;
            const bool labelled =
                cell.info && (cell.ore != game::Ore::Rock || cell.special != game::MineSpecial::None);
            const Rect badge{box.x + box.w - tile * .44F,
                             labelled ? box.y + tile * .05F : box.y + box.h - tile * .27F, tile * .4F,
                             tile * .22F};
            g.rect(badge, unlocked ? Color{36, 74, 60, 230} : Color{60, 30, 28, 230}, tile * .11F);
            drawPickaxe(g, {badge.x + tile * .11F, badge.y + tile * .11F}, tile * .14F,
                        unlocked ? Color{170, 230, 200} : Color{232, 196, 160}, 0);
            g.text(std::to_string(cell.grade),
                   {std::round(badge.x + tile * .29F), std::round(badge.y + tile * .02F)}, tile * .18F,
                   unlocked ? Color{170, 230, 200} : Color{255, 196, 160}, .5F);
        }
        if (cell.clueKnown && cell.clue > 0 && !(cell.info && cell.nest)) {
            const bool corner =
                cell.info && (cell.ore != game::Ore::Rock || cell.special != game::MineSpecial::None);
            const ScreenPoint center = corner ? ScreenPoint{box.x + tile * .2F, box.y + tile * .2F}
                                              : ScreenPoint{box.x + box.w * .5F, box.y + box.h * .5F};
            const float radius = tile * (corner ? .15F : .18F);
            g.circle(center, radius, {16, 14, 18, 130});
            shadowText(g, std::to_string(cell.clue), {center.x, center.y - radius * .95F}, radius * 1.8F,
                       clueColor(cell.clue));
        }
        if (reachable && !jobActive) {
            const bool hover = hovered == index;
            const Color rim = state.play.scan ? (cell.info ? Color{90, 100, 110} : theme::blue) : theme::gold;
            const float strength = hover ? 1.0F : still ? .65F : .45F + .35F * pulse;
            outlineRect(g, {box.x + 1, box.y + 1, box.w - 2, box.h - 2}, hover ? 3.0F : 2.0F,
                        alpha(hover ? Color{255, 244, 206} : rim, strength));
            if (hover) {
                g.rect(box, {255, 250, 230, 34}, tile * .16F);
                if (state.play.scan)
                    for (float radius : {.12F, .22F})
                        g.line(ring({box.x + box.w * .5F, box.y + box.h * .5F}, tile * radius, 18), 1.5F,
                               {150, 214, 255, 220});
                else
                    drawPickaxe(g, {box.x + box.w * .78F, box.y + box.h * .22F}, tile * .2F, {230, 236, 240},
                                still ? -.4F : -.4F + std::sin(time * 8.0F) * .25F);
            }
        } else if (target) {
            outlineRect(g, {box.x + 1, box.y + 1, box.w - 2, box.h - 2}, 2.5F, {255, 236, 170, 220});
            if (digProgress > 0) {
                const Rect bar{box.x + tile * .12F, box.y + box.h + 1 - tile * .1F, box.w - tile * .24F,
                               tile * .06F};
                g.rect(bar, {12, 10, 12, 200}, 2);
                g.rect({bar.x, bar.y, bar.w * digProgress, bar.h}, theme::gold, 2);
            }
        }
    }

    for (float edge : {boardLeft, boardRight}) {
        g.line({{edge, std::max(view.y, surface)}, {edge, std::min(view.y + view.h, boardBottom)}}, 3,
               {12, 8, 10, 200});
        g.line({{edge + (edge == boardLeft ? -2.0F : 2.0F), std::max(view.y, surface)},
                {edge + (edge == boardLeft ? -2.0F : 2.0F), std::min(view.y + view.h, boardBottom)}},
               1, {255, 230, 190, 40});
    }
    // Miner: lantern light, walking bob, pickaxe swing toward the target while excavating.
    const game::Point miner = run.minerPosition();
    const ScreenPoint p{layout.mineOrigin.x + (miner.x + .5F) * tile,
                        layout.mineOrigin.y + (miner.y + .5F) * tile};
    const bool digging = jobActive && live.job.elapsed >= walkTime;
    const bool walking = jobActive && !digging;
    float facing = 1.0F;
    bool downward = false;
    if (jobActive && live.job.target >= 0) {
        const game::Point goal{static_cast<float>(live.job.target % columns),
                               static_cast<float>(live.job.target / columns)};
        if (std::abs(goal.x - miner.x) > .3F)
            facing = goal.x < miner.x ? -1.0F : 1.0F;
        downward = digging && std::abs(goal.x - miner.x) < .3F && goal.y > miner.y;
    }
    const float bob = walking && !still ? std::abs(std::sin(time * 16.0F)) * tile * .06F : 0.0F;
    const ScreenPoint body{p.x, p.y - bob + tile * .05F};
    const float flicker = still ? 1.0F : .92F + .08F * std::sin(time * 7.0F);
    g.circle(p, tile * 1.15F * flicker, {255, 200, 120, 16});
    g.circle(p, tile * .75F * flicker, {255, 206, 130, 22});
    g.rect({p.x - tile * .22F, p.y + tile * .3F, tile * .44F, tile * .09F}, {0, 0, 0, 80}, tile * .05F);
    const float step = walking && !still ? std::sin(time * 16.0F) * tile * .05F : 0.0F;
    g.rect({body.x - tile * .14F + step, body.y + tile * .15F, tile * .1F, tile * .2F}, {40, 46, 52}, 2);
    g.rect({body.x + tile * .04F - step, body.y + tile * .15F, tile * .1F, tile * .2F}, {40, 46, 52}, 2);
    g.rect({body.x - tile * .19F, body.y - tile * .1F, tile * .38F, tile * .3F}, {58, 132, 112}, tile * .07F);
    g.rect({body.x - tile * .13F, body.y - tile * .04F, tile * .26F, tile * .2F}, {44, 98, 132}, tile * .04F);
    g.circle({body.x, body.y - tile * .2F}, tile * .14F, {245, 205, 160});
    g.circle({body.x + facing * tile * .06F, body.y - tile * .2F}, tile * .025F, {40, 30, 26});
    g.rect({body.x - tile * .17F, body.y - tile * .36F, tile * .34F, tile * .14F}, {241, 188, 66},
           tile * .07F);
    g.rect({body.x - tile * .2F, body.y - tile * .25F, tile * .4F, tile * .04F}, {196, 144, 44}, 1);
    const ScreenPoint lamp{body.x + facing * tile * .1F, body.y - tile * .3F};
    g.polygon({lamp,
               {lamp.x + facing * tile * .9F, lamp.y - tile * .22F},
               {lamp.x + facing * tile * .9F, lamp.y + tile * .3F}},
              {255, 240, 190, 26});
    g.circle(lamp, tile * .045F, {255, 250, 220});
    float swing = facing > 0 ? -.6F : .6F + 3.1416F;
    if (digging && !still) {
        const float beat = std::sin(time * 15.0F);
        swing = downward ? .9F + beat * .7F : (facing > 0 ? -.2F + beat * .9F : 3.34F - beat * .9F);
    } else if (digging)
        swing = downward ? 1.1F : (facing > 0 ? .1F : 3.04F);
    const ScreenPoint hand{body.x + facing * tile * .17F, body.y + tile * .02F};
    const float length = tile * .36F;
    const ScreenPoint tip{hand.x + std::cos(swing) * length, hand.y + std::sin(swing) * length};
    g.line({hand, tip}, std::max(1.5F, tile * .05F), {150, 101, 58});
    const float across = swing + 1.5708F;
    g.line({{tip.x - std::cos(across) * tile * .14F, tip.y - std::sin(across) * tile * .14F},
            {tip.x + std::cos(swing) * tile * .04F, tip.y + std::sin(swing) * tile * .04F},
            {tip.x + std::cos(across) * tile * .14F, tip.y + std::sin(across) * tile * .14F}},
           std::max(1.5F, tile * .06F), {206, 214, 220});
    g.circle(hand, tile * .05F, {245, 205, 160});

    // Completed excavation cues: debris, floating resource text and nest warning pulses.
    std::vector<Rect> plates;
    for (const MineCue &cue : state.play.feedback.mineCues) {
        if (cue.cell < 0 || cue.cell >= static_cast<int>(live.mine.size()))
            continue;
        const Rect box = layout.mineCellBounds(cue.cell);
        const ScreenPoint c{box.x + tile * .5F, box.y + tile * .5F};
        const float t = cue.age;
        const float fade = std::clamp(1.0F - (t - cue.lifetime * .6F) / (cue.lifetime * .4F), 0.0F, 1.0F);
        if (!still && t < .6F) {
            const Color debris = cue.ore == game::Ore::Rock ? Color{176, 150, 118} : mineOreColor(cue.ore);
            for (int piece = 0; piece < 9; ++piece) {
                const float angle =
                    -Turn * (.08F + .84F * (static_cast<float>(piece) + noise(cue.cell, piece, 80)) / 9.0F);
                const float speed = tile * (1.6F + noise(cue.cell, piece, 81) * 1.4F);
                const ScreenPoint at{c.x + std::cos(angle) * speed * t,
                                     c.y + std::sin(angle) * speed * t + tile * 5.0F * t * t};
                const float size = tile * (.05F + noise(cue.cell, piece, 82) * .05F) * (1 - t / .6F);
                g.rect({at.x - size, at.y - size, size * 2, size * 2},
                       piece % 3 == 0 ? debris : Color{120, 98, 78}, size * .4F);
            }
            g.circle(c, tile * (.3F + t), {230, 210, 180, static_cast<std::uint8_t>(90 * (1 - t / .6F))});
        }
        if (cue.nest) {
            if (!still && t < 1.0F) {
                g.rect(view, {200, 40, 30, static_cast<std::uint8_t>(70 * (1 - t))});
                for (float lag : {0.0F, .25F})
                    if (t > lag)
                        g.line(ring(c, tile * (.4F + (t - lag) * 2.2F)), std::max(2.0F, tile * .07F),
                               {255, 80, 60, static_cast<std::uint8_t>(220 * std::max(0.0F, 1 - (t - lag)))});
            }
            const float rise = still ? 0.0F : std::min(t, .5F) * tile * .9F;
            const float size = std::max(13.0F, tile * .3F);
            const ScreenPoint at{c.x, c.y - tile * .95F - rise};
            const float plate = size * 7.6F;
            g.rect({at.x - plate / 2, at.y - size * .2F, plate, size * 1.45F}, alpha({60, 12, 12, 230}, fade),
                   size * .6F);
            shadowText(g, "다음 웨이브 +" + std::to_string(cue.extra), at, size,
                       alpha({255, 140, 120, 255}, fade));
        }
        if (cue.special != game::MineSpecial::None) {
            const Color accent = cue.special == game::MineSpecial::Relic ? relicColor(cue.relic)
                                                                         : mineSpecialColor(cue.special);
            if (!still && t < .9F)
                for (float lag : {0.0F, .2F})
                    if (t > lag)
                        g.line(ring(c, tile * (.35F + (t - lag) * 1.8F)), std::max(2.0F, tile * .06F),
                               alpha(accent, .85F * std::max(0.0F, 1 - (t - lag) / .9F)));
            std::string title, detail;
            if (cue.special == game::MineSpecial::Relic) {
                title = "보물 상자!";
                detail = relicName(cue.relic);
            } else if (cue.special == game::MineSpecial::Fossil) {
                title = "카드 화석";
                detail = run.catalog().card(cue.card).name + " 카드 발견";
            } else if (cue.special == game::MineSpecial::Energy) {
                title = "에너지 +" + std::to_string(cue.amount);
                detail = "다음 전투 시작 시";
            } else {
                title = "붕괴!";
                detail = cue.collapsed ? "주변 " + std::to_string(cue.collapsed) + "칸이 무너졌어요"
                                       : "무너질 칸이 없어요";
            }
            const float pop =
                still ? 1.0F : std::min(1.0F, .6F + t * 3.0F) * (t < .2F ? 1.0F + (.2F - t) : 1.0F);
            const float size = std::max(13.0F, tile * .32F) * pop;
            const float small = size * .72F;
            const float iconSize = size * 1.9F;
            const float plate =
                std::max(labelWidth(title, size), labelWidth(detail, small)) + iconSize + size * 1.4F;
            const float rise = still ? 0.0F : std::min(t, .45F) * tile * 1.1F;
            const float left = std::clamp(c.x - plate / 2, view.x + 4, view.x + view.w - plate - 4);
            const float top = std::max(view.y + 4, c.y - tile * 1.15F - rise - size);
            Rect box{left, top, plate, size * 2.6F};
            // Later special plates move below earlier overlapping ones instead of covering them.
            for (bool moved = true; moved;) {
                moved = false;
                for (const Rect &other : plates)
                    if (box.x < other.x + other.w && other.x < box.x + box.w && box.y < other.y + other.h &&
                        other.y < box.y + box.h) {
                        box.y = other.y + other.h + 4;
                        moved = true;
                    }
            }
            plates.push_back(box);
            g.rect({box.x, box.y + 3, box.w, box.h}, alpha({0, 0, 0, 255}, .35F * fade), size * .5F);
            g.rect(box, alpha({20, 16, 22, 240}, fade), size * .5F);
            g.rect({box.x, box.y, box.w, box.h}, alpha(accent, .18F * fade), size * .5F);
            outlineRect(g, {box.x + 1, box.y + 1, box.w - 2, box.h - 2}, 1.5F, alpha(accent, .7F * fade));
            const ScreenPoint mark{box.x + size * .5F + iconSize / 2, box.y + box.h / 2};
            if (fade > .05F) {
                if (cue.special == game::MineSpecial::Relic)
                    drawRelicIcon(g, cue.relic, mark, iconSize);
                else {
                    g.circle(mark, iconSize / 2,
                             cue.special == game::MineSpecial::Unstable ? Color{150, 120, 96}
                                                                        : Color{12, 14, 20, 235});
                    drawSpecialGlyph(g, cue.special, mark, iconSize * .62F);
                }
            }
            const float textX = mark.x + iconSize / 2 + size * .4F;
            shadowText(g, title, {textX, box.y + size * .25F}, size, alpha(accent, fade), 0);
            shadowText(g, detail, {textX, box.y + size * 1.35F}, small, alpha({236, 226, 206, 255}, fade), 0);
            continue;
        }
        if (cue.rubble) {
            if (!still && t < .8F)
                for (int puff = 0; puff < 4; ++puff)
                    g.circle({c.x + (noise(cue.cell, puff, 110) - .5F) * tile * .8F, c.y - t * tile * .4F},
                             tile * (.18F + t * .4F),
                             {200, 184, 160, static_cast<std::uint8_t>(80 * (1 - t / .8F))});
            continue;
        }
        const float rise = still ? 0.0F : t * tile * .9F;
        const float size = std::max(12.0F, tile * .32F);
        if (cue.amount > 0)
            shadowText(g, "+" + std::to_string(cue.amount) + " " + mineOreName(cue.ore),
                       {c.x, c.y - tile * .35F - rise}, size, alpha(mineOreColor(cue.ore), fade));
        else if (!cue.nest)
            shadowText(g, "빈 암석", {c.x, c.y - tile * .3F - rise}, size * .8F,
                       alpha({206, 196, 182, 255}, fade));
    }
    // Hover explanation of a revealed special tile.
    if (hovered >= 0 && hovered < static_cast<int>(live.mine.size()) && live.mine[hovered].info &&
        live.mine[hovered].special != game::MineSpecial::None && !live.mine[hovered].dug) {
        const game::MineCell &cell = live.mine[hovered];
        const std::string title = mineSpecialName(cell.special);
        const std::string detail =
            cell.special == game::MineSpecial::Relic
                ? std::string(relicName(cell.relic)) + " · " + relicEffect(cell.relic, run.catalog().balance)
            : cell.special == game::MineSpecial::Fossil
                ? run.catalog().card(cell.card).name + " 카드를 무료로 획득"
            : cell.special == game::MineSpecial::Energy
                ? "다음 전투 시작 에너지 +" + std::to_string(cell.amount)
                : std::string("주변 4칸이 무너져 열리지만 광물은 사라져요");
        const float size = layout.compact ? 11.0F : 12.0F;
        const float width =
            std::min(view.w - 16, std::max(labelWidth(title, size + 1), labelWidth(detail, size)) + 20);
        const Rect cellBox = layout.mineCellBounds(hovered);
        const float left =
            std::clamp(cellBox.x + tile / 2 - width / 2, view.x + 8, view.x + view.w - width - 8);
        const float top = cellBox.y > view.y + 60 ? cellBox.y - 46 : cellBox.y + tile + 4;
        const Color accent = mineSpecialColor(cell.special);
        g.rect({left, top, width, 42}, {16, 14, 20, 240}, 7);
        g.rect({left, top, 3, 42}, accent, 1);
        g.text(title, {left + 10, top + 5}, size + 1, accent, 0, width - 14);
        g.text(detail, {left + 10, top + 23}, size, {232, 222, 204}, 0, width - 14);
    }
    relicBanner(g, run, state);
    // Scroll affordances: fades at clipped edges and a slim position bar.
    if (layout.mineScrollMax > 0) {
        if (layout.mineScroll > 1)
            g.gradientRect({view.x, view.y, view.w, tile * .5F}, {14, 12, 16, 200}, {14, 12, 16, 0});
        if (layout.mineScroll < layout.mineScrollMax - 1) {
            g.gradientRect({view.x, view.y + view.h - tile * .6F, view.w, tile * .6F}, {14, 12, 16, 0},
                           {14, 12, 16, 220});
            const float cx = view.x + view.w / 2, by = view.y + view.h - 8;
            g.polygon({{cx - 8, by - 8}, {cx + 8, by - 8}, {cx, by}}, {241, 192, 102, 200});
        }
        const float trackH = view.h - 16;
        const float thumbH = std::max(24.0F, trackH * view.h / (view.h + layout.mineScrollMax));
        const float thumbY = view.y + 8 + (trackH - thumbH) * layout.mineScroll / layout.mineScrollMax;
        g.rect({view.x + view.w - 7, view.y + 8, 4, trackH}, {0, 0, 0, 90}, 2);
        g.rect({view.x + view.w - 7, thumbY, 4, thumbH}, {241, 192, 102, 200}, 2);
    }
    g.endClip();

    // Header: active layer name and a legend that explains the clue numbers.
    const float headerY = layout.world.y + 8;
    const std::string layerCode = "B" + std::to_string(layer + 1);
    float nameW = layout.compact ? 92.0F : 128.0F;
    const bool narrow = layout.world.w - 122 - 14 - nameW - 57 < 36;
    if (narrow)
        nameW = 36;
    const std::string layerName =
        narrow ? layerCode
               : layerCode + " · " +
                     (layer == 0 ? run.catalog().mine.name
                                 : run.catalog().deeperMines[static_cast<std::size_t>(layer - 1)].name);
    g.rect({layout.world.x + 8, headerY, nameW, 24}, {22, 18, 18, 220}, 6);
    g.rect({layout.world.x + 8, headerY, 3, 24}, theme::gold, 1);
    g.text(layerName, {layout.world.x + 17, headerY + 5}, layout.compact ? 11 : 12, {240, 224, 190}, 0,
           nameW - 14);
    float legendX = layout.world.x + 14 + nameW;
    const float legendLimit = layout.world.x + layout.world.w - 122;
    const float room = legendLimit - legendX - 57;
    const char *meaning = room >= 130  ? "= 주변 8칸의 둥지 수"
                          : room >= 64 ? "= 주변 둥지"
                          : room >= 36 ? "= 둥지"
                                       : nullptr;
    if (meaning) {
        for (int clue = 1; clue <= 3; ++clue) {
            g.circle({legendX + 9, headerY + 12}, 8.5F, {16, 14, 18, 220});
            g.text(std::to_string(clue), {legendX + 9, headerY + 4}, 13, clueColor(clue), .5F);
            legendX += 19;
        }
        g.text(meaning, {legendX + 3, headerY + 5}, layout.compact ? 10 : 11, {226, 210, 180}, 0,
               legendLimit - legendX - 3);
    }
}

void View::relicBanner(engine::Renderer &g, const game::Run &run, const State &state) {
    const bool still = state.app.preferences.reducedMotion;
    const Rect view = layout.mineViewport;
    const RelicBanner &banner = state.play.feedback.relicBanner;
    if (banner.active) {
        const float t = banner.age;
        const float enter = still ? 1.0F : std::clamp(t / .25F, 0.0F, 1.0F);
        const float fade = std::clamp((banner.lifetime - t) / .6F, 0.0F, 1.0F);
        const Color accent = relicColor(banner.relic);
        const std::string title = std::string("유물 획득 · ") + relicName(banner.relic);
        const std::string detail =
            relicEffect(banner.relic, run.catalog().balance) + " · 이번 원정 동안 유지";
        const float size = layout.compact ? 13.0F : 15.0F;
        const float width =
            std::min(view.w - 16, std::max(labelWidth(title, size), labelWidth(detail, size * .78F)) + 74);
        const Rect box{view.x + (view.w - width) / 2, view.y + view.h - 70 + (1 - enter) * 24, width, 52};
        g.beginClip(view, static_cast<std::uint8_t>(255 * fade * enter));
        g.rect({box.x, box.y + 4, box.w, box.h}, {0, 0, 0, 110}, 12);
        g.gradientRect(box, mix({28, 24, 30}, accent, .22F), {18, 16, 22, 245}, 12);
        outlineRect(g, {box.x + 1, box.y + 1, box.w - 2, box.h - 2}, 2, alpha(accent, .85F));
        drawRelicIcon(g, banner.relic, {box.x + 30, box.y + 26}, 38);
        if (!still)
            twinkle(g, {box.x + 44, box.y + 12}, 9, std::max(0.0F, std::sin(t * 5.0F)));
        g.text(title, {box.x + 56, box.y + 8}, size, accent, 0, box.w - 64);
        g.text(detail, {box.x + 56, box.y + 30}, size * .78F, {236, 226, 206}, 0, box.w - 64);
        g.endClip();
    }
}

void View::prepActions(engine::Renderer &g, engine::Ui &ui, const game::Run &run, const State &state,
                       Rect area) {
    const game::RunState &live = run.state();
    const game::RunBalance &balance = run.catalog().balance;
    // Status: remaining digs and scans as pips, tool tier and nest alarm.
    const bool tight = area.h < 300;
    float y = area.y;
    g.text("굴착", {area.x, y}, 11, theme::muted);
    const int digCapacity = std::max(run.prepDigBudget(), live.digs);
    const int scanCapacity = std::max({1, run.prepScanBudget(), live.scans});
    // Relic bonuses add pips; the spacing shrinks so the row keeps clear of the tool or nest badge.
    const float step = std::clamp((area.w - 66 - 82) / static_cast<float>(digCapacity + scanCapacity), 8.0F,
                                  tight ? 14.0F : 16.0F);
    const float pip = std::min(tight ? 9.0F : 11.0F, step - 4);
    for (int index = 0; index < digCapacity; ++index) {
        const ScreenPoint c{area.x + 36 + index * step, y + 8};
        g.circle(c, pip / 2 + 1, {10, 18, 24});
        g.circle(c, pip / 2 - .5F, index < live.digs ? theme::gold : Color{44, 58, 64});
    }
    // The free scan is a pill around its own pips, leaving the rows below to purchases.
    const float scanX = area.x + 36 + digCapacity * step + 14;
    const bool scanUsable = live.scans > 0 && !live.job.active;
    const Rect scanPill{scanX - 7, y - 4, 32 + scanCapacity * step + 4, 24};
    ui.add(Scan, scanPill, scanUsable);
    if (ui.focused == Scan)
        g.rect({scanPill.x - 3, scanPill.y - 3, scanPill.w + 6, scanPill.h + 6}, theme::blue, 14);
    const bool scanning = state.play.scan;
    g.rect(scanPill, scanning ? theme::blue : scanUsable ? Color{70, 130, 160} : Color{40, 52, 58}, 12);
    g.rect({scanPill.x + 1.5F, scanPill.y + 1.5F + (ui.captured == Scan ? 1.0F : 0.0F), scanPill.w - 3,
            scanPill.h - 3},
           scanning     ? Color{40, 92, 124}
           : scanUsable ? Color{24, 50, 64}
                        : Color{22, 32, 38},
           10.5F);
    g.text("스캔", {scanX, y}, 11, scanning ? theme::text : scanUsable ? theme::blue : theme::muted);
    for (int index = 0; index < scanCapacity; ++index) {
        const ScreenPoint c{scanX + 32 + index * step, y + 8};
        g.circle(c, pip / 2 + 1, {10, 18, 24});
        g.circle(c, pip / 2 - .5F, index < live.scans ? theme::blue : Color{44, 58, 64});
    }
    if (tight && live.nextExtra) {
        const std::string alarm = "둥지 +" + std::to_string(live.nextExtra);
        g.rect({area.x + area.w - 62, y - 1, 62, 19}, {120, 40, 34, 235}, 9);
        g.text(alarm, {area.x + area.w - 31, y + 2}, 11, {255, 206, 190}, .5F);
    } else {
        drawPickaxe(g, {area.x + area.w - 40, y + 8}, 9, live.tool >= 2 ? theme::green : Color{200, 206, 210},
                    0);
        g.text("T" + std::to_string(live.tool), {area.x + area.w, y + 1}, 12,
               live.tool >= 2 ? theme::green : theme::text, 1);
    }
    y += 22;
    // Relics owned this run and energy stored for the next battle.
    if (!live.relics.empty() || live.bonusEnergy > 0) {
        const float icon = tight ? 16.0F : 20.0F;
        const auto tag = [&](game::Relic relic) -> std::string {
            switch (relic) {
            case game::Relic::Battery:
                return "에너지+" + std::to_string(balance.relicEnergy);
            case game::Relic::Charm:
                return "굴착+" + std::to_string(balance.relicDigs);
            case game::Relic::Ward:
                return "둥지-" + std::to_string(balance.relicNestReduction);
            case game::Relic::Lens:
                return "스캔+" + std::to_string(balance.relicScans);
            }
            return {};
        };
        float x = area.x;
        int hovered = -1;
        for (std::size_t index = 0; index < live.relics.size(); ++index) {
            const Rect hit{x, y - 1, icon, icon};
            if (ui.pointerPosition.x >= hit.x && ui.pointerPosition.x <= hit.x + hit.w &&
                ui.pointerPosition.y >= hit.y && ui.pointerPosition.y <= hit.y + hit.h)
                hovered = static_cast<int>(index);
            drawRelicIcon(g, live.relics[index], {x + icon / 2, y - 1 + icon / 2}, icon);
            x += icon + 3;
        }
        float right = area.x + area.w;
        if (live.bonusEnergy > 0) {
            const std::string bonus = "다음 전투 에너지 +" + std::to_string(live.bonusEnergy);
            const float chipW = labelWidth(bonus, 10.5F) + 12;
            right -= chipW;
            g.rect({right, y - 1, chipW, icon}, {20, 60, 58, 235}, icon / 2);
            g.text(bonus, {right + chipW / 2, y + (icon - 12) / 2}, 10.5F, {150, 250, 220}, .5F, chipW - 6);
            right -= 6;
        }
        if (!live.relics.empty()) {
            std::string summary;
            if (hovered >= 0) {
                const game::Relic relic = live.relics[static_cast<std::size_t>(hovered)];
                summary = std::string(relicName(relic)) + " · " + relicEffect(relic, balance);
            } else if (live.relics.size() == 1)
                summary = relicEffect(live.relics.front(), balance);
            else
                for (game::Relic relic : live.relics)
                    summary += (summary.empty() ? "" : " ") + tag(relic);
            g.text(summary, {x + 3, y + (icon - 12) / 2}, 10.5F,
                   hovered >= 0 ? relicColor(live.relics[static_cast<std::size_t>(hovered)]) : theme::text, 0,
                   right - x - 5);
        }
        y += icon + 6;
    }
    const float fraction = live.job.active && live.job.duration > 0
                               ? std::clamp(live.job.elapsed / live.job.duration, 0.0F, 1.0F)
                               : 0;
    if (tight) {
        if (live.job.active) {
            g.rect({area.x, y - 4, area.w, 3}, theme::inset, 1);
            g.rect({area.x, y - 4, area.w * fraction, 3}, theme::gold, 1);
        }
    } else if (live.job.active) {
        g.rect({area.x, y + 3, area.w, 6}, theme::inset, 3);
        g.rect({area.x, y + 3, area.w * fraction, 6}, theme::gold, 3);
        y += 16;
    } else if (live.nextExtra) {
        g.rect({area.x, y, area.w, 20}, {96, 34, 30, 220}, 5);
        g.text("둥지 경보 · 다음 웨이브 +" + std::to_string(live.nextExtra), {area.x + 8, y + 3}, 11,
               {255, 196, 180}, 0, area.w - 16);
        y += 26;
    } else {
        const bool wall = run.catalog().mining == game::MiningMode::Excavate;
        g.text(state.play.scan ? (wall ? "속을 비춰 볼 암벽을 눌러 주세요"
                                       : "스캔할 경계 1칸을 눌러 주세요 · 정보만 공개")
               : wall          ? "매장물을 전부 드러내면 얻어요 · 길게 누르면 정보"
                               : "타일을 누르면 광부가 걸어가 굴착해요",
               {area.x, y}, 11, state.play.scan ? theme::blue : theme::muted, 0, area.w);
        y += 18;
    }

    /** Panel state of one purchase row; only Open rows accept presses. */
    enum class Status { Open, Owned, Done, Locked };
    struct Entry {
        engine::WidgetId id;
        const char *title;
        const char *shortTitle;
        std::string effect;
        game::ResourceCost cost;
        Status status;
        /** Rule text shown instead of the effect when the row is done for this preparation or locked. */
        std::string note;
        Color tint;
        int group;
    };
    /** Build a row from the same quote the purchase command checks before paying. */
    const auto entry = [](engine::WidgetId id, const char *title, const char *shortTitle, std::string effect,
                          const game::PurchaseQuote &price, Color tint, int group) -> Entry {
        const Status value = price.failure == game::PurchaseFailure::Owned       ? Status::Owned
                             : price.failure == game::PurchaseFailure::Exhausted ? Status::Done
                             : price.failure == game::PurchaseFailure::Requirement ||
                                     price.failure == game::PurchaseFailure::InvalidTarget
                                 ? Status::Locked
                                 : Status::Open;
        return {id,
                title,
                shortTitle,
                std::move(effect),
                price.cost,
                value,
                value == Status::Done || value == Status::Locked ? price.message : std::string(),
                tint,
                group};
    };
    const bool drill = live.tool >= 2;
    const int awakenBonus = static_cast<int>(std::lround(balance.levelBonus * 100));
    // The awakening row prices the candidate the panel would select: affordable first, then any open one.
    game::PurchaseQuote awakenPrice;
    awakenPrice.failure = game::PurchaseFailure::Requirement;
    awakenPrice.cost = balance.awaken.front();
    awakenPrice.message = "같은 카드로 Lv" + std::to_string(game::CardMaxLevel) + "까지 강화한 건물에 써요";
    int awakenable = 0;
    for (const game::Tower &tower : live.towers) {
        const game::PurchaseQuote price = run.awakenQuote(tower.id);
        if (!price.open())
            continue;
        if (!awakenable++ || (price.allowed() && !awakenPrice.allowed()))
            awakenPrice = price;
    }
    game::PurchaseQuote recruitPrice;
    recruitPrice.cost = live.recruitmentOffer ? live.recruitmentOffer->cost : balance.recruit;
    recruitPrice.failure =
        live.recruitmentOffer ? game::PurchaseFailure::None : game::PurchaseFailure::Exhausted;
    recruitPrice.message = "이번 정비의 모집을 마쳤어요";
    const std::array<Entry, 8> entries{{
        entry(Tool, drill ? "T3 드릴" : "T2 곡괭이", drill ? "T3" : "T2",
              drill ? "정비마다 굴착 +" + std::to_string(balance.drillDigs) + " · 산 정비에도 바로"
                    : "단단한 암반과 수정을 캘 수 있어요",
              run.quote(drill ? game::UpgradeKind::Drill : game::UpgradeKind::Tool),
              drill ? Color{240, 170, 96} : Color{200, 206, 210}, 0),
        entry(Armor, "경비병 장갑", "장갑",
              "경비병 최대 체력 " + std::to_string(run.catalog().card(game::CardKind::Guard).hp) + " → " +
                  std::to_string(balance.armoredGuardHp),
              run.quote(game::UpgradeKind::Armor), theme::blue, 0),
        entry(Coil, "전격 코일", "코일",
              "전격탑 피해 +" + std::to_string(static_cast<int>(std::lround((balance.coilBonus - 1) * 100))) +
                  "%",
              run.quote(game::UpgradeKind::Coil), theme::purple, 0),
        entry(AwakenPick, "건물 각성", "각성",
              (awakenable ? "각성 가능 " + std::to_string(awakenable) + "곳"
                          : "Lv" + std::to_string(game::CardMaxLevel) + " 건물을 Lv" +
                                std::to_string(run.awakenedLevel()) + "까지") +
                  " · 단계마다 피해 +" + std::to_string(awakenBonus) + "%",
              awakenPrice, {150, 120, 255}, 0),
        entry(Hire, "광부 고용", "고용",
              "이번 정비 굴착 +1 · 고용 " + std::to_string(live.hires) + "/" +
                  std::to_string(balance.hireLimit),
              run.quote(game::UpgradeKind::Hire), theme::gold, 1),
        entry(Supply, "보급 상자", "보급",
              "다음 전투 에너지 +" + std::to_string(balance.supplyEnergy) + " · 정비마다 1회",
              run.quote(game::UpgradeKind::Supply), {120, 220, 190}, 1),
        entry(Slots, "부대 슬롯", "슬롯",
              "출격 부대 " + std::to_string(balance.initialSlots) + " → " +
                  std::to_string(balance.upgradedSlots),
              run.quote(game::UpgradeKind::Slots), theme::green, 2),
        entry(Recruit, "유닛 카드 모집", "모집", "후보 3장 중 1장을 덱에 넣어요", recruitPrice, theme::gold,
              2),
    }};
    const auto missing = [&](const game::ResourceCost &cost) -> std::array<int, 4> {
        return {{std::max(0, cost.iron - live.iron), std::max(0, cost.copper - live.copper),
                 std::max(0, cost.crystal - live.crystal), std::max(0, cost.gold - live.gold)}};
    };
    const auto canPay = [&](const game::ResourceCost &cost) -> bool {
        const std::array<int, 4> lacking = missing(cost);
        return std::all_of(lacking.begin(), lacking.end(), [](int value) -> bool { return value == 0; });
    };
    const std::array<game::Ore, 4> ores{
        {game::Ore::Iron, game::Ore::Copper, game::Ore::Crystal, game::Ore::Gold}};
    /** Paint ore chips right-aligned at `right`, red when the wallet is short and `warn` is set; returns the
     * left edge. */
    const auto chips = [&](const game::ResourceCost &cost, float right, float top, float size,
                           bool warn = true) -> float {
        const std::array<int, 4> amounts{{cost.iron, cost.copper, cost.crystal, cost.gold}};
        const std::array<int, 4> lacking = warn ? missing(cost) : std::array<int, 4>{};
        float x = right;
        for (int index = 3; index >= 0; --index) {
            if (amounts[static_cast<std::size_t>(index)] <= 0)
                continue;
            const std::string value = std::to_string(amounts[static_cast<std::size_t>(index)]);
            const float width = size * (1.45F + .62F * static_cast<float>(value.size()));
            x -= width;
            const bool short_ = lacking[static_cast<std::size_t>(index)] > 0;
            const Rect chip{x, top, width - 3, size * 1.5F};
            if (short_)
                g.rect({chip.x - 1, chip.y - 1, chip.w + 2, chip.h + 2}, {214, 92, 76}, size * .55F);
            g.rect(chip, {12, 22, 28, 235}, size * .5F);
            shard(g, {x + size * .72F, top + size * .78F}, size * .52F,
                  mineOreColor(ores[static_cast<std::size_t>(index)]));
            g.text(value, {x + size * 1.38F, top + size * .22F}, size,
                   short_ ? Color{255, 150, 130} : theme::text);
        }
        return x;
    };
    /** Draw one purchase icon; ids map to the same icons in every layout density. */
    const auto symbol = [&](engine::WidgetId id, ScreenPoint c, float size, Color tint) {
        g.circle(c, size * .62F, {12, 22, 28});
        g.circle(c, size * .56F, shade(tint, .35F));
        if (id == Tool && drill) {
            g.polygon({{c.x - size * .3F, c.y - size * .14F},
                       {c.x + size * .06F, c.y - size * .14F},
                       {c.x + size * .36F, c.y},
                       {c.x + size * .06F, c.y + size * .14F},
                       {c.x - size * .3F, c.y + size * .14F}},
                      tint);
            for (float offset : {-.16F, -.02F})
                g.line({{c.x + size * offset, c.y - size * .14F},
                        {c.x + size * (offset + .1F), c.y + size * .14F}},
                       std::max(1.0F, size * .05F), shade(tint, .5F));
            g.rect({c.x - size * .38F, c.y - size * .2F, size * .1F, size * .4F}, tint, size * .03F);
        } else if (id == Tool)
            drawPickaxe(g, c, size * .62F, tint, 0);
        else if (id == Armor) {
            g.polygon({{c.x - size * .3F, c.y - size * .32F},
                       {c.x + size * .3F, c.y - size * .32F},
                       {c.x + size * .3F, c.y + size * .02F},
                       {c.x, c.y + size * .36F},
                       {c.x - size * .3F, c.y + size * .02F}},
                      tint);
            g.polygon({{c.x - size * .2F, c.y - size * .24F},
                       {c.x, c.y - size * .24F},
                       {c.x, c.y + size * .22F},
                       {c.x - size * .2F, c.y}},
                      {255, 255, 255, 90});
        } else if (id == Coil)
            g.polygon({{c.x + size * .06F, c.y - size * .38F},
                       {c.x - size * .2F, c.y + size * .04F},
                       {c.x - size * .01F, c.y + size * .04F},
                       {c.x - size * .08F, c.y + size * .38F},
                       {c.x + size * .22F, c.y - size * .08F},
                       {c.x + size * .03F, c.y - size * .08F}},
                      tint);
        else if (id == AwakenPick) {
            // A crystal over a rising chevron: the building climbs past its card levels.
            shard(g, {c.x, c.y - size * .08F}, size * .26F, tint);
            g.line({{c.x - size * .26F, c.y + size * .32F},
                    {c.x, c.y + size * .14F},
                    {c.x + size * .26F, c.y + size * .32F}},
                   std::max(1.2F, size * .08F), tint);
        } else if (id == Hire) {
            g.circle({c.x, c.y - size * .14F}, size * .13F, tint);
            g.rect({c.x - size * .2F, c.y + size * .02F, size * .4F, size * .24F}, tint, size * .1F);
            g.rect({c.x - size * .22F, c.y - size * .3F, size * .44F, size * .08F}, shade(tint, .7F),
                   size * .04F);
        } else if (id == Supply) {
            g.rect({c.x - size * .28F, c.y - size * .2F, size * .56F, size * .44F}, tint, size * .05F);
            g.line({{c.x - size * .28F, c.y - size * .02F}, {c.x + size * .28F, c.y - size * .02F}},
                   std::max(1.0F, size * .05F), shade(tint, .45F));
            g.polygon({{c.x + size * .04F, c.y - size * .16F},
                       {c.x - size * .08F, c.y + size * .04F},
                       {c.x + size * .01F, c.y + size * .04F},
                       {c.x - size * .04F, c.y + size * .2F},
                       {c.x + size * .1F, c.y - size * .02F},
                       {c.x + size * .02F, c.y - size * .02F}},
                      shade(tint, .3F));
        } else if (id == Slots) {
            for (float offset : {-.17F, .17F}) {
                g.circle({c.x + size * offset, c.y - size * .12F}, size * .1F, tint);
                g.rect({c.x + size * offset - size * .13F, c.y, size * .26F, size * .22F}, tint, size * .08F);
            }
        } else if (id == Recruit) {
            g.rect({c.x - size * .2F, c.y - size * .28F, size * .4F, size * .56F}, tint, size * .06F);
            g.rect({c.x - size * .14F, c.y - size * .22F, size * .28F, size * .44F}, shade(tint, .55F),
                   size * .04F);
            g.line({{c.x - size * .1F, c.y}, {c.x + size * .1F, c.y}}, size * .07F, tint);
            g.line({{c.x, c.y - size * .1F}, {c.x, c.y + size * .1F}}, size * .07F, tint);
        }
    };
    const auto lackingLabel = [&](const game::ResourceCost &cost) -> std::string {
        const std::array<int, 4> lacking = missing(cost);
        std::string text;
        for (std::size_t index = 0; index < 4; ++index)
            if (lacking[index] > 0)
                text += (text.empty() ? "" : " ") + std::string(mineOreName(ores[index])) +
                        std::to_string(lacking[index]);
        return text.empty() ? text : text + " 부족";
    };
    /** Paint one row or tile frame in its open, owned, done or locked state. */
    const auto frame = [&](const Entry &item, Rect face, float radius) {
        const bool open = item.status == Status::Open;
        const Color border = item.status == Status::Owned ? Color{58, 118, 92}
                             : !open                      ? Color{40, 52, 58}
                             : canPay(item.cost)          ? Color{214, 160, 70}
                                                          : Color{70, 84, 90};
        g.rect(face, border, radius);
        g.gradientRect({face.x + 1.5F, face.y + 1.5F, face.w - 3, face.h - 3},
                       item.status == Status::Owned ? Color{30, 62, 52}
                       : open                       ? Color{40, 66, 76}
                                                    : Color{30, 42, 48},
                       item.status == Status::Owned ? Color{22, 46, 40}
                       : open                       ? Color{28, 48, 58}
                                                    : Color{24, 34, 40},
                       radius - 1);
    };
    const float bottom = area.y + area.h;
    const std::array<const char *, 3> groups{{"도구 · 장비", "금 보급", "부대 · 모집"}};
    const float headerHeight = 18;
    const float rowStep = std::min(50.0F, (bottom - y - static_cast<float>(groups.size()) * headerHeight) /
                                              static_cast<float>(entries.size()));
    if (!layout.compact && rowStep >= 36) {
        // Roomy list: grouped rows with icon, effect line and ore chips, shrinking to fit the column.
        const float height = rowStep - 6;
        const bool big = height >= 40;
        int group = -1;
        for (const Entry &item : entries) {
            if (item.group != group) {
                group = item.group;
                g.text(groups[static_cast<std::size_t>(group)], {area.x, y}, 11, theme::gold);
                g.line({{area.x + 70, y + 7}, {area.x + area.w, y + 7}}, 1, {52, 76, 84});
                y += headerHeight;
            }
            const bool open = item.status == Status::Open;
            const bool shown = open || item.status == Status::Owned;
            const Rect row{area.x, y, area.w, height};
            ui.add(item.id, row, open);
            const bool pressed = ui.captured == item.id;
            const Rect face{row.x, row.y + (pressed ? 2.0F : 0.0F), row.w, row.h};
            if (ui.focused == item.id)
                g.rect({row.x - 3, row.y - 3, row.w + 6, row.h + 6}, theme::blue, 10);
            g.rect({face.x, face.y + 3, face.w, face.h}, {8, 16, 22, 200}, 8);
            frame(item, face, 8);
            const float iconSize = std::min(30.0F, height - 8);
            symbol(item.id, {face.x + 8 + iconSize * .5F, face.y + height / 2}, iconSize,
                   shown ? item.tint : Color{110, 124, 128});
            const float textX = face.x + 16 + iconSize;
            const Color titleColor = shown ? theme::text : Color{130, 148, 150};
            float chipsLeft = face.x + face.w - 8;
            const float badgeY = face.y + (height - 20) / 2;
            if (item.status == Status::Owned || item.status == Status::Done) {
                const bool owned = item.status == Status::Owned;
                g.rect({face.x + face.w - 54, badgeY, 46, 20}, owned ? Color{50, 120, 90} : Color{48, 60, 66},
                       10);
                g.text(owned ? "보유" : "완료", {face.x + face.w - 31, badgeY + 3}, 11,
                       owned ? Color{220, 255, 236} : theme::muted, .5F);
                chipsLeft = face.x + face.w - 58;
            } else
                chipsLeft = chips(item.cost, face.x + face.w - 6, face.y + (big ? 6.0F : 4.0F),
                                  big ? 11.0F : 10.0F, open);
            g.text(item.title, {textX, face.y + (big ? 6.0F : 3.0F)}, big ? 13.0F : 12.0F, titleColor, 0,
                   chipsLeft - textX - 4);
            const bool short_ = open && !canPay(item.cost);
            const std::string line = !item.note.empty() ? item.note
                                     : short_           ? lackingLabel(item.cost)
                                                        : item.effect;
            const Color lineColor = item.status == Status::Locked ? Color{214, 176, 120}
                                    : short_                      ? Color{255, 150, 130}
                                                                  : theme::muted;
            g.text(line, {textX, face.y + height - (big ? 18.0F : 15.0F)}, big ? 10.5F : 10.0F, lineColor, 0,
                   (item.status == Status::Done ? chipsLeft : face.x + face.w - 6) - textX);
            y += rowStep;
        }
        return;
    }
    // Compact grid: small tiles with icon, short title and chips; narrow tiles stack the chips underneath.
    const int count = static_cast<int>(entries.size());
    const float gap = 6;
    const int perRow = (area.w - 3 * gap) / 4 >= 60 ? 4 : 3;
    const int rows = (count + perRow - 1) / perRow;
    const float tileW = (area.w - gap * static_cast<float>(perRow - 1)) / static_cast<float>(perRow);
    const float tileH = std::clamp(
        (bottom - y - gap * static_cast<float>(rows - 1)) / static_cast<float>(rows), 28.0F, 46.0F);
    const bool narrow = tileW < 96;
    for (int index = 0; index < count; ++index) {
        const Entry &item = entries[static_cast<std::size_t>(index)];
        const Rect cell{area.x + static_cast<float>(index % perRow) * (tileW + gap),
                        y + static_cast<float>(index / perRow) * (tileH + gap), tileW, tileH};
        const bool open = item.status == Status::Open;
        const bool shown = open || item.status == Status::Owned;
        ui.add(item.id, cell, open);
        const bool pressed = ui.captured == item.id;
        const Rect face{cell.x, cell.y + (pressed ? 2.0F : 0.0F), cell.w, cell.h};
        if (ui.focused == item.id)
            g.rect({cell.x - 3, cell.y - 3, cell.w + 6, cell.h + 6}, theme::blue, 9);
        g.rect({face.x, face.y + 3, face.w, face.h}, {8, 16, 22, 200}, 7);
        frame(item, face, 7);
        const Color tint = shown ? item.tint : Color{110, 124, 128};
        const Color titleColor = shown ? theme::text : Color{130, 148, 150};
        const char *badge = item.status == Status::Owned    ? "보유"
                            : item.status == Status::Done   ? "완료"
                            : item.status == Status::Locked ? "잠김"
                                                            : nullptr;
        const Color stateColor = item.status == Status::Owned ? theme::green : theme::muted;
        if (narrow) {
            // Icon and title share the upper line; the price or state sits right-aligned on the lower line.
            const float iconSize = std::min(18.0F, tileH * .5F);
            symbol(item.id, {face.x + 4 + iconSize * .62F, face.y + 3 + iconSize * .62F}, iconSize, tint);
            const float textX = face.x + 7 + iconSize * 1.24F;
            g.text(item.shortTitle, {textX, face.y + 3}, 10.5F, titleColor, 0, face.x + face.w - textX - 3);
            const float lower = face.y + face.h - 15;
            if (badge)
                g.text(badge, {face.x + face.w - 5, lower + 1}, 10, stateColor, 1);
            else
                chips(item.cost, face.x + face.w - 3, lower, 9);
            continue;
        }
        const float iconSize = std::min(26.0F, tileH - 12);
        symbol(item.id, {face.x + 6 + iconSize * .62F, face.y + face.h / 2}, iconSize, tint);
        const float textX = face.x + 10 + iconSize * 1.2F;
        const bool roomy = tileW > 120;
        g.text(roomy ? item.title : item.shortTitle, {textX, face.y + (tileH > 40 ? 6.0F : 3.0F)},
               roomy ? 12.0F : 11.0F, titleColor, 0, face.x + face.w - textX - 4);
        const float lower = face.y + face.h - (tileH > 40 ? 20.0F : 17.0F);
        if (badge)
            g.text(badge, {textX, lower + 2}, 10, stateColor);
        else
            chips(item.cost, face.x + face.w - 4, lower, 10);
    }
}
} // namespace thorium::presentation
