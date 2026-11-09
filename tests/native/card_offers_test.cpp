#include "Scenario.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>

using namespace thorium::game;
namespace {
int checks = 0;
/** Assert atomic offer transitions in release builds. */
void require(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Complete authored empty waves through the actual simulation transition. */
void nextPreparation(Run &run) {
    require(run.startWave().ok, "start the next authored wave");
    run.advance(1.0F / 60.0F);
    require(run.state().phase == Phase::Prep && run.state().recruitmentOffer,
            "wave completion authors a persistent recruitment offer");
}
/** Keep candidates, price, invalid selections and payment under one authoritative owner. */
void recruitmentContracts() {
    Catalog data = defaultCatalog();
    data.waves = {{}, {}, {}};
    data.balance.recruit = {0, 2, 0, 3};
    data.recruitmentCandidates = {CardKind::Demolition, CardKind::Guard, CardKind::Shooter};
    data.cards[5].category = CardCategory::Unit;
    data.cards[5].hp = 50;
    data.expansions[0].reward = CardKind::Frost;
    Run run = makeRun(data);
    nextPreparation(run);
    const CardOffer offered = *run.state().recruitmentOffer;
    require(offered.candidates == std::vector<CardKind>(data.recruitmentCandidates.begin(),
                                                        data.recruitmentCandidates.end()) &&
                offered.cost.gold == 3 && offered.cost.copper == 2,
            "offer captures authored candidate order and its full resource price");
    scenario(run).gold = 3;
    scenario(run).copper = 2;
    const CardId old = run.state().deck[7].id;
    require(!run.acceptOffer(offered.id, CardKind::Lightning, old).ok && run.state().gold == 3 &&
                run.offer(offered.id),
            "a valid unit that was not offered cannot be acquired");
    require(!run.acceptOffer(offered.id, CardKind::Guard, 99999).ok && run.state().copper == 2 &&
                run.offer(offered.id),
            "invalid replacement does not consume resources or the offer");
    scenario(run).copper = 1;
    require(!run.acceptOffer(offered.id, CardKind::Guard, old).ok && run.state().gold == 3 &&
                run.state().copper == 1,
            "payment validates every resource before committing any deduction");
    require(!run.skipOffer(offered.id).ok && run.offer(offered.id),
            "closing recruitment must not skip or consume its offer");
    scenario(run).copper = 2;
    require(run.acceptOffer(offered.id, CardKind::Shooter, old).ok && !run.offer(offered.id) &&
                run.state().gold == 0 && run.state().copper == 0,
            "valid selection atomically replaces the card, pays and consumes the offer");
    const CardId acquired = run.state().deck[7].id;
    require(!run.acceptOffer(offered.id, CardKind::Guard, acquired).ok && run.state().deck[7].id == acquired,
            "duplicate confirmation cannot pay or replace a second time");
    nextPreparation(run);
    const OfferId later = run.state().recruitmentOffer->id;
    require(later != offered.id && !run.acceptOffer(offered.id, CardKind::Guard, acquired).ok,
            "an old offer cannot be reused during the following preparation");
    require(run.state().expansionPending && run.expand(1).ok && run.state().rewardOffer,
            "map selection authors its own expansion reward offer");
    const CardOffer reward = *run.state().rewardOffer;
    require(reward.source == OfferSource::Expansion &&
                reward.candidates == std::vector<CardKind>{CardKind::Frost},
            "reward comes from the chosen branch definition rather than a hardcoded lightning card");
    scenario(run).gold = 3;
    scenario(run).copper = 2;
    require(!run.acceptOffer(later, CardKind::Guard, acquired).ok && run.offer(later),
            "recruitment waits until the mandatory reward choice has been resolved");
    require(!run.acceptOffer(reward.id, CardKind::Shooter, acquired).ok && run.offer(reward.id),
            "reward validation rejects non-offered candidates too");
    require(run.skipOffer(reward.id).ok && !run.state().rewardOffer && run.offer(later) &&
                run.state().gold == 3,
            "skipping a reward preserves the independent recruitment offer and wallet");
    require(run.acceptOffer(later, CardKind::Guard, acquired).ok,
            "recruitment remains usable after reward skip");
}
/** Preserve eight active cards, latest-card priority and a no-refund one-shot reward commit. */
void rewardContracts() {
    Catalog data = defaultCatalog();
    data.waves = {{}, {}, {}};
    data.expansions[1].reward = CardKind::Demolition;
    Run run = makeRun(data);
    nextPreparation(run);
    const OfferId unused = run.state().recruitmentOffer->id;
    nextPreparation(run);
    require(!run.offer(unused), "starting another wave expires an unconsumed recruitment offer");
    require(run.expand(2).ok, "select an independently authored reward branch");
    const OfferId reward = run.state().rewardOffer->id;
    const CardId old = run.state().deck[0].id;
    require(run.acceptOffer(reward, CardKind::Demolition, old).ok && run.state().deck.size() == 8 &&
                run.state().gold == 0 && !run.state().rewardOffer,
            "free expansion reward replaces one card without expanding the deck or charging gold");
    const CardId acquired = run.state().deck[0].id;
    require(!run.skipOffer(reward).ok && !run.acceptOffer(reward, CardKind::Demolition, acquired).ok,
            "a claimed reward cannot later be skipped or claimed twice");
    require(run.startWave().ok && std::any_of(run.state().hand.begin(), run.state().hand.end(),
                                              [&run, acquired](int index) -> bool {
                                                  return run.state().deck[index].id == acquired;
                                              }),
            "the acquired instance is prioritized in the next wave's hand");
}
} // namespace
/** Run portable offer-authority and transactional payment regression scenarios. */
int main() {
    recruitmentContracts();
    rewardContracts();
    std::cout << checks << " card offer checks passed\n";
}
