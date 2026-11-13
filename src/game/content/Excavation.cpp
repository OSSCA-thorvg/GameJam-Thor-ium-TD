#include "Excavation.h"
#include <algorithm>
#include <array>
#include <utility>

namespace thorium::game {
namespace {
/** Small reproducible generator; walls never depend on the platform's random library. */
struct Dice {
    std::uint32_t state;
    /** Return the next 32 mixed bits. */
    std::uint32_t next() {
        state += 0x9e3779b9U;
        std::uint32_t value = state;
        value = (value ^ (value >> 16)) * 0x85ebca6bU;
        value = (value ^ (value >> 13)) * 0xc2b2ae35U;
        return value ^ (value >> 16);
    }
    /** Return a value from zero up to but excluding `limit`. */
    int below(int limit) {
        return limit > 0 ? static_cast<int>(next() % static_cast<std::uint32_t>(limit)) : 0;
    }
};
using Offset = std::pair<int, int>;
using Shape = std::vector<Offset>;
/** Footprints a deposit of one to four cells may take, as column/row offsets from its top-left corner. */
const std::vector<Shape> &shapes(int cells) {
    static const std::array<std::vector<Shape>, 4> table{{
        {{{0, 0}}},
        {{{0, 0}, {1, 0}}, {{0, 0}, {0, 1}}},
        {{{0, 0}, {1, 0}, {2, 0}},
         {{0, 0}, {0, 1}, {0, 2}},
         {{0, 0}, {1, 0}, {0, 1}},
         {{0, 0}, {1, 0}, {1, 1}},
         {{0, 0}, {0, 1}, {1, 1}},
         {{1, 0}, {0, 1}, {1, 1}}},
        {{{0, 0}, {1, 0}, {0, 1}, {1, 1}},
         {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
         {{0, 0}, {1, 0}, {2, 0}, {1, 1}},
         {{1, 0}, {0, 1}, {1, 1}, {2, 1}},
         {{0, 0}, {0, 1}, {0, 2}, {1, 2}},
         {{0, 0}, {1, 0}, {2, 0}, {0, 1}},
         {{0, 0}, {1, 0}, {1, 1}, {2, 1}},
         {{1, 0}, {0, 1}, {1, 1}, {0, 2}}},
    }};
    return table[static_cast<std::size_t>(std::clamp(cells, 1, 4) - 1)];
}
/** A deposit waiting for a position, with the depth band it prefers as fractions of the wall height. */
struct Pending {
    Deposit deposit;
    int cells = 1;
    float top = 0;
    float bottom = 1;
    bool revealed = false;
};
/** Check that every cell of a shape at a position is inside the wall and free; with `apart`, also that no
 * cardinal neighbour belongs to another deposit. */
bool fits(const ExcavationWall &wall, const Shape &shape, int column, int row, bool apart) {
    for (const Offset &offset : shape) {
        const int x = column + offset.first, y = row + offset.second;
        if (x < 0 || y < 0 || x >= wall.width || y >= wall.height ||
            wall.cells[static_cast<std::size_t>(y * wall.width + x)].deposit)
            return false;
        if (!apart)
            continue;
        for (const Offset &side : {Offset{1, 0}, Offset{-1, 0}, Offset{0, 1}, Offset{0, -1}}) {
            const int nx = x + side.first, ny = y + side.second;
            if (nx >= 0 && ny >= 0 && nx < wall.width && ny < wall.height &&
                wall.cells[static_cast<std::size_t>(ny * wall.width + nx)].deposit)
                return false;
        }
    }
    return true;
}
/** Write a deposit into the wall at a position and mark its cells. */
void place(ExcavationWall &wall, Pending &item, const Shape &shape, int column, int row) {
    const int id = static_cast<int>(wall.deposits.size()) + 1;
    for (const Offset &offset : shape) {
        const int index = (row + offset.second) * wall.width + column + offset.first;
        ExcavationCell &cell = wall.cells[static_cast<std::size_t>(index)];
        cell.deposit = id;
        cell.grade = item.deposit.grade;
        cell.revealed = item.revealed;
        if (item.deposit.grade > 1)
            cell.cover = MaxCover;
        else if ((item.deposit.nest || item.deposit.ore == Ore::Gold) && index >= wall.width)
            cell.cover = std::min(MaxCover, cell.cover + 1);
        item.deposit.cells.push_back(index);
    }
    std::sort(item.deposit.cells.begin(), item.deposit.cells.end());
    wall.deposits.push_back(std::move(item.deposit));
}
/** Find a position for a deposit: random tries inside its depth band and apart from other deposits, then
 * anywhere apart, then the first free fit; a deposit that fits nowhere shrinks by one cell. */
bool bury(ExcavationWall &wall, Pending &item, Dice &dice) {
    for (int cells = item.cells; cells >= 1; --cells) {
        const std::vector<Shape> &options = shapes(cells);
        const int top =
            std::clamp(static_cast<int>(item.top * static_cast<float>(wall.height)), 0, wall.height - 1);
        const int bottom =
            std::clamp(static_cast<int>(item.bottom * static_cast<float>(wall.height)), top + 1, wall.height);
        for (int attempt = 0; attempt < 120; ++attempt) {
            const Shape &shape =
                options[static_cast<std::size_t>(dice.below(static_cast<int>(options.size())))];
            const bool banded = attempt < 80;
            const int column = dice.below(wall.width);
            const int row = banded ? top + dice.below(bottom - top) : dice.below(wall.height);
            if (fits(wall, shape, column, row, true)) {
                place(wall, item, shape, column, row);
                return true;
            }
        }
        for (const Shape &shape : options)
            for (int row = 0; row < wall.height; ++row)
                for (int column = 0; column < wall.width; ++column)
                    if (fits(wall, shape, column, row, false)) {
                        place(wall, item, shape, column, row);
                        return true;
                    }
    }
    return false;
}
} // namespace

ExcavationWall excavationWall(const MineLayerDefinition &layer, std::uint32_t seed) {
    ExcavationWall wall;
    wall.width = layer.width;
    wall.height = layer.height;
    wall.cells.assign(layer.cells.size(), ExcavationCell{});
    Dice dice{seed ^ 0x7f4a7c15U};
    for (const MineTileDefinition &tile : layer.cells)
        dice.state =
            (dice.state ^ static_cast<std::uint32_t>(static_cast<int>(tile.ore) * 131 + tile.amount * 17 +
                                                     static_cast<int>(tile.special) * 7 +
                                                     (tile.nest ? 3 : 0) + tile.grade)) *
            16777619U;
    // Rock thickens with depth from two layers to the maximum; a quarter of the cells carry one extra layer.
    // The top row is one layer thin and the second row two layers.
    for (int index = 0; index < static_cast<int>(wall.cells.size()); ++index) {
        const int row = index / wall.width;
        const int base = 2 + row * (MaxCover - 1) / std::max(1, wall.height);
        wall.cells[static_cast<std::size_t>(index)].cover =
            row == 0   ? 1
            : row == 1 ? 2
                       : std::min(MaxCover, base + (dice.below(4) == 0 ? 1 : 0));
    }
    std::vector<Pending> pending;
    int hardRock = 0;
    std::array<std::vector<const MineTileDefinition *>, 5> veins;
    for (const MineTileDefinition &tile : layer.cells) {
        if (tile.opened)
            continue;
        Pending item;
        item.deposit.grade = tile.grade;
        item.revealed = tile.revealed;
        if (tile.special != MineSpecial::None) {
            item.deposit.special = tile.special;
            item.deposit.relic = tile.relic;
            item.deposit.card = tile.card;
            item.deposit.amount = tile.amount;
            item.cells = tile.special == MineSpecial::Relic ? 4 : tile.special == MineSpecial::Fossil ? 3 : 1;
            item.top = tile.special == MineSpecial::Relic    ? .4F
                       : tile.special == MineSpecial::Fossil ? .3F
                                                             : .1F;
            pending.push_back(std::move(item));
        } else if (tile.nest) {
            item.deposit.nest = true;
            item.deposit.ore = tile.ore;
            item.deposit.amount = tile.amount;
            item.deposit.grade = 1;
            item.cells = 2;
            item.top = .35F;
            pending.push_back(std::move(item));
        } else if (tile.ore != Ore::Rock && tile.amount > 0)
            veins[static_cast<std::size_t>(tile.ore)].push_back(&tile);
        else if (tile.grade > 1)
            ++hardRock;
    }
    for (std::size_t ore = 0; ore < veins.size(); ++ore) {
        std::vector<const MineTileDefinition *> &tiles = veins[ore];
        std::stable_sort(
            tiles.begin(), tiles.end(),
            [](const MineTileDefinition *a, const MineTileDefinition *b) { return a->amount > b->amount; });
        for (std::size_t first = 0; first < tiles.size(); first += 3) {
            Pending item;
            item.deposit.ore = static_cast<Ore>(ore);
            const std::size_t last = std::min(tiles.size(), first + 3);
            for (std::size_t index = first; index < last; ++index) {
                item.deposit.amount += tiles[index]->amount;
                item.deposit.grade = std::max(item.deposit.grade, tiles[index]->grade);
                item.revealed = item.revealed || tiles[index]->revealed;
            }
            item.cells = static_cast<int>(last - first) + 1;
            const Ore kind = static_cast<Ore>(ore);
            item.top = kind == Ore::Gold || kind == Ore::Crystal ? .45F : 0;
            item.bottom = kind == Ore::Iron || kind == Ore::Copper ? .7F : 1;
            pending.push_back(std::move(item));
        }
    }
    // Large footprints first, while the wall still has room for them.
    std::stable_sort(pending.begin(), pending.end(),
                     [](const Pending &a, const Pending &b) { return a.cells > b.cells; });
    for (Pending &item : pending)
        bury(wall, item, dice);
    // Half of the layer's plain hard rock returns as hard cells below the top third.
    for (int count = hardRock / 2, attempt = 0; count > 0 && attempt < 400; ++attempt) {
        const int row = wall.height / 3 + dice.below(wall.height - wall.height / 3);
        ExcavationCell &cell =
            wall.cells[static_cast<std::size_t>(row * wall.width + dice.below(wall.width))];
        if (cell.deposit || cell.grade > 1)
            continue;
        cell.grade = 2;
        cell.cover = MaxCover;
        --count;
    }
    return wall;
}
} // namespace thorium::game
