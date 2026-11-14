#include "Feedback.h"
#include <algorithm>
#include <cmath>

namespace thorium::presentation {
void Feedback::observeHand(const game::Run &run) {
    for (int index = 0; index < 4; ++index) {
        const game::Card current = run.state().deck[run.state().hand[index]];
        CardTransition &slot = cards[index];
        if (!initialized_) {
            slot.outgoing = current;
            slot.incoming = current;
            slot.age = CardDuration;
        } else if (slot.incoming.id != current.id) {
            slot.outgoing = slot.incoming;
            slot.incoming = current;
            slot.age = 0;
            ++handChanges;
        }
    }
    if (initialized_ && run.state().energy < previousEnergy_) {
        energySpent = previousEnergy_ - run.state().energy;
        energyAge = 0;
    }
    previousEnergy_ = run.state().energy;
    initialized_ = true;
}
void Feedback::revealHand(const game::Run &run) {
    observeHand(run);
    for (CardTransition &slot : cards) {
        slot.outgoing = {};
        slot.age = CardExit;
    }
}
void Feedback::consume(const std::vector<game::CombatEvent> &events) {
    for (const game::CombatEvent &event : events) {
        if (event.type == game::CombatEventKind::Leak) {
            screenShake = 0.35F;
        } else if (event.type == game::CombatEventKind::Stomp ||
                   event.type == game::CombatEventKind::Disable ||
                   event.type == game::CombatEventKind::Wreck ||
                   event.type == game::CombatEventKind::Breach ||
                   (event.type == game::CombatEventKind::Defeated && event.enemy &&
                    event.enemyKind == game::EnemyKind::Boss)) {
            screenShake = std::max(screenShake, 0.3F);
        } else if (event.type == game::CombatEventKind::Attack &&
                   (event.card == game::CardKind::Cannon || event.card == game::CardKind::Blast)) {
            screenShake = std::max(screenShake, 0.16F);
        }
        if (event.type == game::CombatEventKind::Damage) {
            ++hits;
            const std::vector<CombatCue>::iterator recent =
                std::find_if(cues.begin(), cues.end(), [&event](const CombatCue &cue) {
                    return cue.age <= DamageMergeGap && cue.span <= DamageMergeSpan &&
                           cue.event.type == game::CombatEventKind::Damage &&
                           cue.event.subject == event.subject && cue.event.enemy == event.enemy;
                });
            if (recent != cues.end()) {
                recent->event.amount += event.amount;
                recent->event.to = event.to;
                recent->age = 0;
                continue;
            }
        }
        if (cues.size() >= 128)
            cues.erase(cues.begin());
        const bool bossDefeat = event.type == game::CombatEventKind::Defeated && event.enemy &&
                                event.enemyKind == game::EnemyKind::Boss;
        const float lifetime =
            event.type == game::CombatEventKind::Damage   ? .65F
            : event.type == game::CombatEventKind::Attack ? .32F
            : event.type == game::CombatEventKind::Stomp || event.type == game::CombatEventKind::Disable ? .8F
            : event.type == game::CombatEventKind::Eruption                                              ? .7F
            : event.type == game::CombatEventKind::Detonation                                            ? .8F
            : event.type == game::CombatEventKind::Wreck    ? 1.0F
            : event.type == game::CombatEventKind::Airdrop  ? 1.0F
            : event.type == game::CombatEventKind::Breach   ? 1.2F
            : event.type == game::CombatEventKind::Loot     ? 1.1F
            : event.type == game::CombatEventKind::Cart     ? 1.0F
            : event.type == game::CombatEventKind::Sabotage ? 1.0F
            : bossDefeat                                    ? 1.2F
                                                            : .55F;
        cues.push_back({event, 0, lifetime});
    }
}
void Feedback::observeMine(const game::Run &run) {
    const game::RunState &live = run.state();
    if (live.relics.size() > previousRelics_ && !live.relics.empty())
        relicBanner = {live.relics.back(), 0.0F, 3.4F, true};
    else if (live.relics.size() < previousRelics_)
        relicBanner = {};
    previousRelics_ = live.relics.size();
    if (run.catalog().mining == game::MiningMode::Excavate) {
        observeWall(run);
        return;
    }
    if (live.mineLayer != previousLayer_ || previousDug_.size() != live.mine.size()) {
        previousLayer_ = live.mineLayer;
        previousDug_.assign(live.mine.size(), false);
        for (std::size_t index = 0; index < live.mine.size(); ++index)
            previousDug_[index] = live.mine[index].dug;
        mineCues.clear();
        return;
    }
    int collapsed = 0;
    for (std::size_t index = 0; index < live.mine.size(); ++index)
        collapsed += live.mine[index].dug && !previousDug_[index] && live.mine[index].rubble ? 1 : 0;
    for (std::size_t index = 0; index < live.mine.size(); ++index) {
        const game::MineCell &cell = live.mine[index];
        if (cell.dug && !previousDug_[index]) {
            if (mineCues.size() >= 16)
                mineCues.erase(mineCues.begin());
            MineCue cue{static_cast<int>(index),
                        cell.ore,
                        cell.amount,
                        cell.nest,
                        cell.nest ? run.nestReinforcements() : 0,
                        0.0F,
                        cell.nest ? 2.2F : 1.4F};
            cue.special = cell.special;
            cue.relic = cell.relic;
            cue.card = cell.card;
            cue.rubble = cell.rubble;
            if (cell.special == game::MineSpecial::Unstable)
                cue.collapsed = collapsed;
            if (cell.special != game::MineSpecial::None)
                cue.lifetime = 2.2F;
            mineCues.push_back(cue);
        }
        previousDug_[index] = cell.dug;
    }
}
void Feedback::observeWall(const game::Run &run) {
    const game::RunState &live = run.state();
    if (live.mineLayer != previousLayer_ || previousCover_.size() != live.mine.size() ||
        previousCollected_.size() != live.deposits.size()) {
        previousLayer_ = live.mineLayer;
        previousCover_.resize(live.mine.size());
        for (std::size_t index = 0; index < live.mine.size(); ++index)
            previousCover_[index] = live.mine[index].cover;
        previousCollected_.resize(live.deposits.size());
        previousWoken_.resize(live.deposits.size());
        for (std::size_t index = 0; index < live.deposits.size(); ++index) {
            previousCollected_[index] = live.deposits[index].collected;
            previousWoken_[index] = live.deposits[index].woken;
        }
        mineCues.clear();
        return;
    }
    const auto push = [this](const MineCue &cue) {
        if (mineCues.size() >= 40)
            mineCues.erase(mineCues.begin());
        mineCues.push_back(cue);
    };
    for (std::size_t index = 0; index < live.mine.size(); ++index) {
        const int cover = live.mine[index].cover;
        if (cover < previousCover_[index]) {
            MineCue dust;
            dust.cell = static_cast<int>(index);
            dust.stripped = previousCover_[index] - cover;
            dust.lifetime = .55F;
            push(dust);
        }
        previousCover_[index] = cover;
    }
    for (std::size_t index = 0; index < live.deposits.size(); ++index) {
        const game::Deposit &deposit = live.deposits[index];
        const bool woke = deposit.woken && !previousWoken_[index];
        if ((deposit.collected && !previousCollected_[index]) || woke) {
            MineCue cue;
            cue.cell = deposit.cells.empty() ? -1 : deposit.cells[deposit.cells.size() / 2];
            cue.collected = deposit.collected;
            cue.ore = deposit.ore;
            cue.amount = deposit.collected ? deposit.amount : 0;
            cue.nest = deposit.nest;
            cue.extra = woke ? run.nestReinforcements() : 0;
            cue.special = deposit.special;
            cue.relic = deposit.relic;
            cue.card = deposit.card;
            cue.lifetime = deposit.nest || deposit.special != game::MineSpecial::None ? 2.2F : 1.6F;
            push(cue);
        }
        previousCollected_[index] = deposit.collected;
        previousWoken_[index] = deposit.woken;
    }
}
void Feedback::settleCards() {
    for (CardTransition &slot : cards)
        slot.age = CardDuration;
}
void Feedback::advance(float seconds, bool combatActive) {
    if (!std::isfinite(seconds) || seconds <= 0)
        return;
    const float delta = std::min(seconds, .1F);
    for (CardTransition &slot : cards)
        slot.age = std::min(CardDuration, slot.age + delta);
    rejectionAge = std::min(1.0F, rejectionAge + delta);
    energyAge = std::min(1.0F, energyAge + delta);
    screenShake = std::max(0.0F, screenShake - delta);
    if (!combatActive)
        return;
    for (MineCue &cue : mineCues)
        cue.age += delta;
    if (relicBanner.active) {
        relicBanner.age += delta;
        relicBanner.active = relicBanner.age < relicBanner.lifetime;
    }
    mineCues.erase(std::remove_if(mineCues.begin(), mineCues.end(),
                                  [](const MineCue &cue) { return cue.age >= cue.lifetime; }),
                   mineCues.end());
    for (CombatCue &cue : cues) {
        cue.age += delta;
        cue.span += delta;
    }
    cues.erase(std::remove_if(cues.begin(), cues.end(),
                              [](const CombatCue &cue) { return cue.age >= cue.lifetime; }),
               cues.end());
}
void Feedback::reject(game::CardId card) {
    if (!card)
        return;
    rejectedCard = card;
    rejectionAge = 0;
}
bool Feedback::cardBusy(int slot) const {
    return slot >= 0 && slot < 4 && cards[slot].age < CardArrival;
}
float Feedback::hitIntensity(game::EntityId id, bool enemy) const {
    float result = 0;
    for (const CombatCue &cue : cues)
        if (cue.event.type == game::CombatEventKind::Damage && cue.event.subject == id &&
            cue.event.enemy == enemy)
            result = std::max(result, std::max(0.0F, 1 - cue.age / .18F));
    return result;
}
game::Point Feedback::recoil(game::EntityId id) const {
    for (std::vector<CombatCue>::const_reverse_iterator it = cues.rbegin(); it != cues.rend(); ++it)
        if (it->event.type == game::CombatEventKind::Attack && it->event.subject == id && it->age < .15F) {
            const float length = game::distance(it->event.from, it->event.to);
            if (length < .001F)
                return {};
            const float amount = .12F * (1 - it->age / .15F) / length;
            return {(it->event.from.x - it->event.to.x) * amount,
                    (it->event.from.y - it->event.to.y) * amount};
        }
    return {};
}
} // namespace thorium::presentation
