#include "Catalog.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

namespace thorium::game {
namespace {
constexpr int CampaignMineWidth = 7;
constexpr int CampaignMineHeight = 11;
constexpr int CampaignMineLayers = 3;
using MineMap = std::array<std::string_view, CampaignMineHeight>;

/** Authored contents of one campaign layer besides its tiles. */
struct LayerSpecials {
    std::string_view name;
    Relic relic;
    CardKind fossil;
};

/**
 * Tile legend for authored campaign maps:
 * `o` opened gallery, `.` soft rock, `#` hard rock,
 * `i`/`I` iron (soft/hard), `c`/`C` copper (soft/hard), `g`/`G` gold (soft/hard),
 * `x`/`X` crystal (always hard, small/large), `n` nest with gold, `m` nest with iron,
 * `1` revealed iron, `2` revealed copper, `3` revealed gold, `4` revealed crystal,
 * `$`/`%` relic chest (soft/hard), `&` revealed hard relic chest, `f`/`F` card fossil (soft/hard),
 * `e`/`E` energy vein (soft/hard), `u` unstable rock that collapses its four neighbours.
 */
MineTileDefinition tile(char symbol, int depth, const LayerSpecials &layer) {
    const int base = 2 + depth;
    const int rich = 4 + depth * 2;
    MineTileDefinition special;
    switch (symbol) {
    case 'o':
        return {Ore::Rock, 0, 1, false, true};
    case '#':
        return {Ore::Rock, 0, 2};
    case 'i':
        return {Ore::Iron, base};
    case 'I':
        return {Ore::Iron, rich, 2};
    case 'c':
        return {Ore::Copper, base};
    case 'C':
        return {Ore::Copper, rich, 2};
    case 'g':
        return {Ore::Gold, rich};
    case 'G':
        return {Ore::Gold, rich + 4, 2};
    case 'x':
        return {Ore::Crystal, base, 2};
    case 'X':
        return {Ore::Crystal, rich, 2};
    case 'n':
        return {Ore::Gold, rich + 2, 1, true};
    case 'm':
        return {Ore::Iron, rich, 1, true};
    case '1':
        return {Ore::Iron, base, 1, false, false, true};
    case '2':
        return {Ore::Copper, base, 1, false, false, true};
    case '3':
        return {Ore::Gold, rich, 1, false, false, true};
    case '4':
        return {Ore::Crystal, base, 2, false, false, true};
    case '$':
    case '%':
    case '&':
        special.special = MineSpecial::Relic;
        special.relic = layer.relic;
        special.grade = symbol == '$' ? 1 : 2;
        special.revealed = symbol == '&';
        return special;
    case 'f':
    case 'F':
        special.special = MineSpecial::Fossil;
        special.card = layer.fossil;
        special.grade = symbol == 'f' ? 1 : 2;
        return special;
    case 'e':
    case 'E':
        special.special = MineSpecial::Energy;
        special.amount = symbol == 'e' ? 2 : 3;
        special.grade = symbol == 'e' ? 1 : 2;
        return special;
    case 'u':
        special.special = MineSpecial::Unstable;
        return special;
    default:
        return {};
    }
}

/** Authored layers of one stage, ordered B1, B2, B3. */
struct StageMine {
    std::array<MineMap, CampaignMineLayers> maps;
    std::array<LayerSpecials, CampaignMineLayers> layers;
};

/** Every stage's cutaways: one chest per layer, fossils on B1 and B3, more nests and riches deeper. */
constexpr std::array<StageMine, TotalStages> stageMines{{
    // 표층 초원: soft iron/copper and hard gold by the entry, a hard band with a visible chest.
    {{{{{
           "..ooo..",
           ".i12cG.",
           "ci.e.ic",
           ".c.i.g.",
           "n.u.c.n",
           "##.#&.#",
           "#x.#n3#",
           "g.i.c.c",
           ".n.f.g.",
           "i.g.u.x",
           "m.G.c.x",
       }},
       {{
           "#.ooo.#",
           "c.1.3.i",
           "C#u#g#C",
           "n.gn.em",
           ".x#.#x.",
           "g.G.%.g",
           "#i#X#c#",
           "g.n.u.G",
           "X#c.i#X",
           ".m.E.n.",
           "G.x#u.g",
       }},
       {{
           "..ooo..",
           "x.4.4.x",
           "#X#u#X#",
           "i.n.c.g",
           "X#g#G#X",
           "n.e.m.x",
           "G#i#F#G",
           ".n.X.n.",
           "x#%#G#x",
           "X.m.u.n",
           "#G#E#G#",
       }}}},
     {{{"표층 갱도", Relic::Charm, CardKind::Flame},
       {"심층 광맥", Relic::Lens, CardKind::Flame},
       {"수정 동굴", Relic::Battery, CardKind::Lightning}}}},
    // 흑요석 지대: glassy hard ridges and crystal pockets, a narrow entry between black glass.
    {{{{{
           ".#ooo#.",
           "c.2.1.i",
           ".x#e#.c",
           "i.u.n.g",
           "n.c.x..",
           "#.##&#.",
           "c.n.3.i",
           ".g.i.n.",
           "#x#f#c#",
           "g.c.u.G",
           "m.X.g.n",
       }},
       {{
           "#.ooo.#",
           "x.3.1.x",
           "X#c#i#X",
           "n.u.e.n",
           ".g#m#g.",
           "C.x.x.C",
           "#n#%#n#",
           "g.G.G.g",
           "X#u.E#X",
           ".n.c.m.",
           "G.X#X.G",
       }},
       {{
           "..ooo..",
           "X.4.4.X",
           "#x#u#x#",
           "g.n.e.n",
           "X#G#G#X",
           "n.F.x.m",
           "G#i#c#G",
           ".m.X.n.",
           "x#G%G#x",
           "X.n.u.n",
           "#G#E#G#",
       }}}},
     {{{"유리 균열", Relic::Lens, CardKind::Tar},
       {"흑요석 맥", Relic::Ward, CardKind::Tar},
       {"화산 유리굴", Relic::Charm, CardKind::Flame}}}},
    // 동토 갱도: iron-rich frozen galleries with copper veins and ice-locked crystal.
    {{{{{
           "..ooo..",
           "i.1.1.c",
           ".c.e.i.",
           "n.i.c.n",
           ".u.g.u.",
           "##.&#.#",
           "#i.n.c#",
           "c.x.g.i",
           ".n.f.n.",
           "i.G.c.x",
           "m.g.X.n",
       }},
       {{
           "#.ooo.#",
           "c.2.3.i",
           "I#u#c#I",
           "n.g.e.m",
           ".x#n#x.",
           "g.C.%.g",
           "#i#X#u#",
           "g.n.c.G",
           "X#E.i#X",
           ".m.G.n.",
           "G.x#x.n",
       }},
       {{
           "..ooo..",
           "x.4.3.x",
           "#X#u#X#",
           "i.n.e.g",
           "X#G#F#X",
           "n.x.m.x",
           "G#%#c#G",
           ".n.X.n.",
           "x#G#E#x",
           "X.m.u.n",
           "#G#X#G#",
       }}}},
     {{{"서리 갱도", Relic::Ward, CardKind::Sniper},
       {"빙맥 회랑", Relic::Battery, CardKind::Sniper},
       {"얼음 수정굴", Relic::Lens, CardKind::Lightning}}}},
    // 용암 심층: basalt crust full of unstable rock, copper and gold seams and energy veins.
    {{{{{
           ".#ooo#.",
           "c.2.3.c",
           ".u.e.u.",
           "n.c.g.n",
           "g.#n#.i",
           "#.#&#.#",
           "c.e.u.g",
           ".n.G.n.",
           "#x#f#c#",
           "g.u.i.G",
           "m.X.g.n",
       }},
       {{
           "#.ooo.#",
           "C.3.2.C",
           "g#u#u#g",
           "n.E.e.n",
           ".G#m#G.",
           "c.u.%.c",
           "#n#X#n#",
           "G.g.g.G",
           "X#u.u#X",
           ".m.E.n.",
           "G.n#n.G",
       }},
       {{
           "..ooo..",
           "G.3.4.G",
           "#X#u#X#",
           "n.E.m.n",
           "X#G#G#X",
           "g.n.F.n",
           "G#u#u#G",
           ".n.%.m.",
           "x#G#G#x",
           "X.n.E.n",
           "#G#X#G#",
       }}}},
     {{{"현무암 갱도", Relic::Battery, CardKind::Frost},
       {"용암 균열", Relic::Charm, CardKind::Frost},
       {"마그마 공동", Relic::Ward, CardKind::Sniper}}}},
    // 토르륨 핵: luminous crystal, many energy veins and the densest nests.
    {{{{{
           "..ooo..",
           "x.4.1.c",
           ".e.u.e.",
           "n.i.c.n",
           ".x.n.g.",
           "#.#&#.#",
           "c.u.u.i",
           ".n.X.n.",
           "#g#f#g#",
           "x.E.u.X",
           "m.G.n.G",
       }},
       {{
           "#.ooo.#",
           "X.4.3.X",
           "C#e#e#C",
           "n.u.u.n",
           ".G#m#G.",
           "x.E.%.x",
           "#n#X#n#",
           "G.u.u.G",
           "X#E.E#X",
           ".m.G.n.",
           "G.n#n.G",
       }},
       {{
           "..ooo..",
           "X.4.4.X",
           "#E#u#E#",
           "n.G.m.n",
           "X#X#X#X",
           "G.n.F.n",
           "G#u#u#G",
           ".n.%.m.",
           "X#G#G#X",
           "E.n.E.n",
           "#G#X#G#",
       }}}},
     {{{"토르륨 외피", Relic::Charm, CardKind::Lightning},
       {"공명 광맥", Relic::Ward, CardKind::Lightning},
       {"토르륨 심핵", Relic::Battery, CardKind::Flame}}}},
    // 수정 공명굴: crystal clusters in every band, copper seams and few nests near the surface.
    {{{{{
           "..ooo..",
           "i.1.2.c",
           ".x.i.x.",
           "c.u.e.i",
           ".X.n.c.",
           "#.#&#.#",
           "i.c.u.c",
           ".n.X.g.",
           "#i#f#x#",
           "X.e.u.C",
           "m.G.n.X",
       }},
       {{
           "#.ooo.#",
           "X.4.2.X",
           "C#x#x#C",
           "n.u.e.n",
           ".X#m#X.",
           "c.E.%.c",
           "#n#X#n#",
           "G.x.x.G",
           "X#u.u#X",
           ".m.E.n.",
           "X.n#n.X",
       }},
       {{
           "..ooo..",
           "X.4.4.X",
           "#X#u#X#",
           "n.E.m.n",
           "X#C#C#X",
           "G.n.F.n",
           "X#u#u#X",
           ".n.%.m.",
           "X#G#X#X",
           "E.n.E.n",
           "#X#G#X#",
       }}}},
     {{{"수정 동굴 입구", Relic::Lens, CardKind::Lightning},
       {"공명 맥동층", Relic::Charm, CardKind::Sniper},
       {"수정 심장부", Relic::Battery, CardKind::Lightning}}}},
    // 산성 늪지: soft peat with iron and gold, unstable mud and nests packed under the surface.
    {{{{{
           "..ooo..",
           "i.1.2.c",
           ".u.c.u.",
           "n.g.i.n",
           ".c.n.x.",
           "#.#&#.#",
           "g.u.u.i",
           ".n.X.n.",
           "#c#f#g#",
           "x.E.u.G",
           "m.G.n.I",
       }},
       {{
           "#.ooo.#",
           "I.3.1.I",
           "g#u#u#g",
           "n.e.E.n",
           ".G#m#G.",
           "i.u.%.i",
           "#n#X#n#",
           "G.g.u.G",
           "I#E.u#X",
           ".m.G.n.",
           "G.n#n.G",
       }},
       {{
           "..ooo..",
           "G.3.4.G",
           "#I#u#I#",
           "n.E.m.n",
           "X#G#G#X",
           "G.n.F.n",
           "G#u#u#G",
           ".n.%.m.",
           "X#G#G#X",
           "E.n.E.n",
           "#G#X#G#",
       }}}},
     {{{"이탄 습지층", Relic::Ward, CardKind::Tar},
       {"부식 수렁", Relic::Charm, CardKind::Frost},
       {"산성 저수층", Relic::Battery, CardKind::Flame}}}},
}};
/** Wave after which B2 and B3 open; short early stages open their deeper layers sooner. */
constexpr std::array<std::array<int, 2>, TotalStages> layerGates{
    {{3, 5}, {3, 6}, {3, 7}, {3, 7}, {3, 7}, {3, 7}, {3, 7}}};
/** Cutaway palette per stage, matching the stage's battlefield biome. */
constexpr std::array<StageBiome, TotalStages> biomes{{StageBiome::Meadow, StageBiome::Obsidian,
                                                      StageBiome::Tundra, StageBiome::Lava, StageBiome::Core,
                                                      StageBiome::Crystal, StageBiome::Swamp}};
} // namespace

MineLayerDefinition campaignMine(int stage, int depth) {
    const std::size_t stageIndex = static_cast<std::size_t>(std::clamp(stage, 1, TotalStages) - 1);
    const std::size_t layerIndex = static_cast<std::size_t>(std::clamp(depth, 0, CampaignMineLayers - 1));
    const StageMine &mine = stageMines[stageIndex];
    const MineMap &map = mine.maps[layerIndex];
    const LayerSpecials &layer = mine.layers[layerIndex];
    MineLayerDefinition result;
    result.name = std::string(layer.name);
    result.opensAfterWave = layerIndex == 0 ? 0 : layerGates[stageIndex][layerIndex - 1];
    result.width = CampaignMineWidth;
    result.height = CampaignMineHeight;
    result.start = CampaignMineWidth / 2;
    result.biome = biomes[stageIndex];
    result.cells.reserve(static_cast<std::size_t>(CampaignMineWidth * CampaignMineHeight));
    for (int y = 0; y < CampaignMineHeight; ++y)
        for (int x = 0; x < CampaignMineWidth; ++x)
            result.cells.push_back(tile(map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)],
                                        static_cast<int>(layerIndex), layer));
    return result;
}
} // namespace thorium::game
