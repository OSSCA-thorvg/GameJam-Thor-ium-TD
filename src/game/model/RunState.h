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
    std::optional<CardOffer> recruitmentOffer;
    bool expansionPending = false;
    std::optional<CardOffer> rewardOffer;
    int branch = 0;
    float battleTime = 0.0F;
    float energyTime = 0.0F;
    int minerCell = 2;
    MineJob job;
    std::array<Card, 8> deck{};
    std::array<int, 4> hand{};
    std::deque<int> queue;
    std::array<MineCell, 30> mine{};
    std::vector<Tower> towers;
    std::vector<Unit> units;
    std::vector<Enemy> enemies;
    std::vector<Point> route;
    std::vector<Point> pads;
    std::vector<Point> unitCells;
};
} // namespace thorium::game
