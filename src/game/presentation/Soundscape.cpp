#include "Soundscape.h"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace thorium::presentation {
const std::array<SoundDefinition, static_cast<std::size_t>(Sound::Count)> &soundDefinitions() {
    static const std::array<SoundDefinition, static_cast<std::size_t>(Sound::Count)> bank{{
        {Sound::Select, "select", {650, 820, .035F}, {.42F, .035F, 5, 2}},
        {Sound::Confirm, "confirm", {760, 1040, .075F}, {.5F, .06F, 5, 2}},
        {Sound::Error, "error", {170, 95, .13F}, {.55F, .15F, 7, 1}},
        {Sound::CardUse, "card-use", {560, 340, .055F}, {.45F, .05F, 5, 2}},
        {Sound::CardDraw, "card-draw", {950, 1350, .065F}, {.4F, .08F, 5, 1}},
        {Sound::Placement, "placement", {300, 540, .095F}, {.5F, .06F, 4, 2}},
        {Sound::Shot, "shot", {230, 95, .035F, .2F}, {.25F, .055F, 1, 2}},
        {Sound::Cannon, "cannon", {120, 42, .16F, .45F}, {.55F, .14F, 3, 2}},
        {Sound::Lightning, "lightning", {1450, 500, .075F, .3F}, {.4F, .09F, 2, 2}},
        {Sound::Frost, "frost", {1250, 650, .18F}, {.5F, .12F, 4, 2}},
        {Sound::Blast, "blast", {140, 35, .2F, .6F}, {.6F, .12F, 4, 2}},
        {Sound::Hit, "hit", {300, 165, .026F, .15F}, {.16F, .06F, 0, 2}},
        {Sound::DefeatEnemy, "enemy-defeat", {480, 180, .07F, .15F}, {.35F, .075F, 2, 2}},
        {Sound::BaseHit, "base-hit", {180, 70, .2F}, {.6F, .14F, 6, 2}},
        {Sound::MineStart, "mine-start", {360, 620, .055F}, {.4F, .1F, 3, 1}},
        {Sound::MineComplete, "mine-complete", {800, 1150, .11F}, {.5F, .1F, 4, 1}},
        {Sound::Scan, "scan", {1100, 1800, .1F}, {.45F, .1F, 4, 1}},
        {Sound::Upgrade, "upgrade", {620, 1300, .17F}, {.55F, .12F, 6, 1}},
        {Sound::Reward, "reward", {850, 1650, .2F}, {.55F, .15F, 6, 1}},
        {Sound::WaveStart, "wave-start", {440, 950, .2F}, {.55F, .2F, 7, 1}},
        {Sound::WaveClear, "wave-clear", {740, 1480, .24F}, {.55F, .2F, 7, 1}},
        {Sound::Victory, "victory", {600, 1800, .45F}, {.6F, .4F, 8, 1}},
        {Sound::Defeat, "defeat", {300, 55, .45F}, {.6F, .4F, 8, 1}},
    }};
    return bank;
}
Soundscape::Soundscape(engine::AudioMixer &mixer, const std::string &directory) : mixer_(mixer) {
    for (const SoundDefinition &definition : soundDefinitions()) {
        const std::string path = directory + "/" + definition.key + ".mp3";
        std::string error;
        engine::AudioClip clip;
        if (std::ifstream(path, std::ios::binary).good()) {
            clip = engine::loadMp3(path, error);
            if (!clip.valid())
                assetErrors.push_back(path + ": " + error);
        }
        if (clip.valid())
            ++mp3Clips;
        else {
            clip = engine::synthesize(definition.beep);
            ++beepClips;
        }
        mixer_.setClip(static_cast<engine::SoundId>(definition.id), std::move(clip), definition.policy);
    }
}
bool Soundscape::play(Sound sound, float pan) {
    return sound != Sound::None && mixer_.play(static_cast<engine::SoundId>(sound), pan);
}
void Soundscape::advance(float seconds) {
    mixer_.advance(seconds);
    if (!std::isfinite(seconds) || seconds <= 0 || drawDelay_ < 0)
        return;
    drawDelay_ -= std::min(seconds, .1F);
    if (drawDelay_ <= 0) {
        drawDelay_ = -1;
        play(Sound::CardDraw);
    }
}
void Soundscape::observe(const game::Run &run) {
    if (!initialized_) {
        reset(run);
        return;
    }
    if (run.state().phase != phase_) {
        mixer_.stop();
        cancelPending();
        if (run.state().phase == game::Phase::Battle) {
            play(Sound::WaveStart);
            drawDelay_ = .16F;
        } else if (run.state().phase == game::Phase::Prep)
            play(Sound::WaveClear);
        else if (run.state().phase == game::Phase::Won)
            play(Sound::Victory);
        else if (run.state().phase == game::Phase::Lost)
            play(Sound::Defeat);
    } else if (run.state().phase == game::Phase::Battle) {
        bool changed = false;
        for (int index = 0; index < 4; ++index)
            changed |= hand_[index] != run.state().deck[run.state().hand[index]].id;
        if (changed) {
            play(Sound::CardUse);
            drawDelay_ = .16F;
        }
    }
    if (run.state().phase == game::Phase::Prep && run.state().job.active != mining_)
        play(run.state().job.active ? Sound::MineStart : Sound::MineComplete);
    for (int index = 0; index < 4; ++index)
        hand_[index] = run.state().deck[run.state().hand[index]].id;
    phase_ = run.state().phase;
    mining_ = run.state().job.active;
}
void Soundscape::consume(const std::vector<game::CombatEvent> &events) {
    for (const game::CombatEvent &event : events) {
        const float pan = std::clamp((event.to.x - 8.5F) / 17.0F, -.5F, .5F);
        if (event.type == game::CombatEventKind::Placement)
            play(Sound::Placement, pan);
        else if (event.type == game::CombatEventKind::Damage)
            play(Sound::Hit, pan);
        else if (event.type == game::CombatEventKind::Defeated)
            play(Sound::DefeatEnemy, pan);
        else if (event.type == game::CombatEventKind::Leak)
            play(Sound::BaseHit, pan);
        else if (event.type == game::CombatEventKind::Attack) {
            const Sound sound =
                event.card == game::CardKind::Cannon || event.card == game::CardKind::Demolition
                    ? Sound::Cannon
                : event.card == game::CardKind::Lightning ? Sound::Lightning
                : event.card == game::CardKind::Frost     ? Sound::Frost
                : event.card == game::CardKind::Blast     ? Sound::Blast
                                                          : Sound::Shot;
            play(sound, pan);
        }
    }
}
void Soundscape::reset(const game::Run &run) {
    mixer_.stop();
    cancelPending();
    for (int index = 0; index < 4; ++index)
        hand_[index] = run.state().deck[run.state().hand[index]].id;
    phase_ = run.state().phase;
    mining_ = run.state().job.active;
    initialized_ = true;
}
void Soundscape::cancelPending() {
    drawDelay_ = -1;
}
} // namespace thorium::presentation
