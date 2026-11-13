#pragma once
#include "Types.h"
#include <deque>
#include <optional>

namespace thorium::game {
/** Read-only outside Run; one owner commits every gameplay mutation. */
struct RunState {
    Phase phase = Phase::Ready;
    int wave = 0;
    int baseHp = 0;
    int energy = 0;
    int slots = 0;
    int gold = 0;
    int iron = 0;
    int copper = 0;
    int crystal = 0;
    int tool = 1;
    int digs = 0;
    int scans = 0;
    int nextExtra = 0;
    int spawned = 0;
    int defeated = 0;
    bool armor = false;
    bool coil = false;
    /** Relics in acquisition order; effects last until the run ends. */
    std::vector<Relic> relics;
    /** Energy added once to the start of the next battle by excavated energy veins and supply crates. */
    int bonusEnergy = 0;
    /** Miners hired in the current preparation; each adds one dig and raises the next price. */
    int hires = 0;
    /** Whether the current preparation's supply crate was bought. */
    bool supplied = false;
    std::optional<CardOffer> recruitmentOffer;
    bool expansionPending = false;
    std::optional<CardOffer> rewardOffer;
    int branch = 0;
    float battleTime = 0.0F;
    float energyTime = 0.0F;
    int minerCell = 2;
    int mineLayer = 0;
    MineJob job;
    std::array<Card, 8> deck{};
    std::array<int, 4> hand{};
    std::deque<int> queue;
    int mineWidth = 5;
    int mineHeight = 6;
    std::vector<MineCell> mine;
    /** Excavate: buried objects of the active wall, referenced by `MineCell::deposit`. */
    std::vector<Deposit> deposits;
    std::vector<Tower> towers;
    std::vector<Unit> units;
    std::vector<Enemy> enemies;
    std::vector<GroundField> fields;
    std::vector<Point> route;
    std::vector<Point> pads;
    std::vector<Point> unitCells;
    /** Building pads still blocked by a boulder; a cleared boulder moves to `pads`. */
    std::vector<Point> boulders;
};
} // namespace thorium::game
