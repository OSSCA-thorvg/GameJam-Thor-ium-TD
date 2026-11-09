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
        if (event.type == game::CombatEventKind::Damage) {
            ++hits;
            const std::vector<CombatCue>::iterator simultaneous =
                std::find_if(cues.begin(), cues.end(), [&event](const CombatCue &cue) {
                    return cue.age == 0 && cue.event.type == game::CombatEventKind::Damage &&
                           cue.event.subject == event.subject && cue.event.enemy == event.enemy;
                });
            if (simultaneous != cues.end()) {
                simultaneous->event.amount += event.amount;
                continue;
            }
        }
        if (cues.size() >= 128)
            cues.erase(cues.begin());
        const float lifetime = event.type == game::CombatEventKind::Damage   ? .65F
                               : event.type == game::CombatEventKind::Attack ? .32F
                                                                             : .55F;
        cues.push_back({event, 0, lifetime});
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
    if (!combatActive)
        return;
    for (CombatCue &cue : cues)
        cue.age += delta;
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
