#include "engine/audio/Audio.h"
#include "game/presentation/Soundscape.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
using namespace thorium::engine;
using namespace thorium::presentation;
int checks = 0;
/** Keep release-build assertions active and report a concrete broken audio contract. */
void require(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
/** Require finite, bounded audible PCM without depending on an audio device. */
bool audible(const std::vector<float> &samples) {
    return std::all_of(samples.begin(), samples.end(),
                       [](float value) { return std::isfinite(value) && std::abs(value) <= 1; }) &&
           std::any_of(samples.begin(), samples.end(), [](float value) { return std::abs(value) > .0001F; });
}
/** Exercise synthesis envelopes, validation, channel placement, mute and limiter behavior. */
void mixerContracts() {
    const AudioClip beep = synthesize({440, 880, .1F});
    require(beep.valid() && beep.samples.front() == 0 && beep.samples.back() == 0,
            "procedural fallback has valid click-free endpoints");
    require(!synthesize({0, 880, .1F}).valid() &&
                !synthesize({440, 880, std::numeric_limits<float>::quiet_NaN()}).valid(),
            "invalid synthesis parameters reject");
    AudioMixer mixer;
    require(mixer.setClip(1, beep, {.5F, .05F, 1, 2}), "register valid fallback PCM");
    require(!mixer.play(99), "unknown sound cannot create a voice");
    require(mixer.play(1, -1), "play left-panned sound");
    require(!mixer.play(1), "same-frame duplicate sound is rate-limited");
    std::vector<float> buffer(1024);
    mixer.render(buffer.data(), 512);
    require(audible(buffer), "mixed output is nonzero, finite and bounded");
    bool rightSilent = true;
    for (std::size_t i = 1; i < buffer.size(); i += 2)
        rightSilent &= buffer[i] == 0;
    require(rightSilent, "full left pan preserves a silent right channel");
    mixer.settings.muted = true;
    mixer.render(buffer.data(), 512);
    require(std::all_of(buffer.begin(), buffer.end(), [](float sample) { return sample == 0; }) &&
                mixer.activeVoices() == 0,
            "muting clears voices and produces exact silence");
    require(!mixer.play(1), "muted requests are discarded rather than replayed later");
    mixer.settings.muted = false;
    mixer.settings.volume = 0;
    require(!mixer.play(1), "zero volume does not retain pending effects");
    mixer.settings.volume = .5F;
    mixer.advance(.1F);
    require(mixer.play(1), "cooldown expires on the real-time clock");
    const std::uint64_t generation = mixer.generation;
    mixer.stop();
    require(!mixer.activeVoices() && mixer.generation == generation + 1, "stop invalidates pending sink PCM");
    AudioClip invalid = beep;
    invalid.samples[3] = std::numeric_limits<float>::infinity();
    require(!mixer.setClip(2, std::move(invalid)), "non-finite decoded data cannot reach output");
    AudioMixer busy;
    busy.setClip(1, beep, {1, 0, 2, 16});
    busy.setClip(2, beep, {1, 0, 0, 16});
    busy.setClip(3, beep, {1, 0, 8, 16});
    for (std::size_t i = 0; i < AudioMixer::MaxVoices; ++i)
        require(busy.play(1), "fill bounded simultaneous voices");
    require(!busy.play(2), "lower-priority sound cannot evict more important voices");
    require(busy.play(3) && busy.activeVoices() == AudioMixer::MaxVoices,
            "important UI sound replaces a low-priority voice without growing the pool");
    busy.settings.volume = 1;
    busy.render(buffer.data(), 512);
    require(audible(buffer), "many simultaneous sounds remain finite and peak-limited");
    std::vector<float> tail(16000);
    busy.render(tail.data(), 8000);
    require(busy.activeVoices() == 0, "completed voices are reclaimed");
}
/** Decode a real original MP3 and check direct file override plus corrupt-file fallback. */
void assetContracts(const std::string &fixtures) {
    std::string error;
    AudioClip mp3 = loadMp3(fixtures + "/card-use.mp3", error);
    require(error.empty() && mp3.valid() && mp3.channels == 2 && mp3.sampleRate == 44100,
            "real stereo MP3 decodes at its native sample rate");
    AudioMixer mixer;
    mixer.setClip(5, std::move(mp3));
    require(mixer.play(5), "decoded file uses the same sound identity API");
    std::vector<float> buffer(16000);
    mixer.render(buffer.data(), 8000);
    require(audible(buffer) && mixer.activeVoices() == 0,
            "44100 Hz MP3 resamples and finishes in the 48000 Hz output mixer");
    require(!loadMp3(fixtures + "/error.mp3", error).valid() && !error.empty(),
            "malformed MP3 reports a decoder error");
    require(!loadMp3(fixtures + "/missing.mp3", error).valid(), "missing optional MP3 returns an empty clip");
    Soundscape sound(mixer, fixtures);
    require(sound.mp3Clips == 1 && sound.beepClips == static_cast<int>(Sound::Count) - 1,
            "per-key MP3 override replaces only that sound, others synthesize");
    require(sound.assetErrors.size() == 1, "existing invalid file records one fallback diagnostic");
    require(sound.play(Sound::Error), "corrupt optional asset still has a playable beep fallback");
}
/** Loop a short real MP3 as music under the voices, through mute and stop. */
void musicContracts(const std::string &fixtures) {
    std::string error;
    MusicStream missing;
    require(!missing.open(fixtures + "/missing.mp3", error) && !missing.loaded() && !error.empty(),
            "a missing music file leaves the stream unloaded");
    require(!missing.open(fixtures + "/error.mp3", error) && !missing.loaded(),
            "malformed music MP3 is rejected");
    AudioMixer empty;
    require(!empty.setMusic(std::make_unique<MusicStream>(), .4F) && !empty.music(),
            "the mixer rejects an unloaded music stream");

    const AudioClip clip = loadMp3(fixtures + "/card-use.mp3", error);
    const std::size_t length = clip.samples.size() / clip.channels;
    std::unique_ptr<MusicStream> track = std::make_unique<MusicStream>();
    require(track->open(fixtures + "/card-use.mp3", error) && track->loaded() && track->loops() == 0,
            "a real stereo MP3 opens as music");
    AudioMixer mixer;
    require(mixer.setMusic(std::move(track), .4F) && mixer.music(), "install looping music");
    const std::size_t frames = length * AudioMixer::SampleRate / clip.sampleRate * 3 + 512;
    std::vector<float> pcm(frames * AudioMixer::Channels);
    mixer.render(pcm.data(), frames);
    require(mixer.music()->loops() >= 2, "music restarts from its first frame after each pass");
    const std::vector<float> tail(pcm.end() - 1024, pcm.end());
    require(audible(pcm) && audible(tail) && mixer.activeVoices() == 0,
            "music stays audible past its own length without any voice");

    const std::uint64_t loops = mixer.music()->loops();
    mixer.stop();
    std::vector<float> after(1024);
    mixer.render(after.data(), 512);
    require(audible(after) && mixer.music()->loops() == loops, "stopping voices keeps the music playing");
    mixer.settings.muted = true;
    mixer.render(after.data(), 512);
    require(std::all_of(after.begin(), after.end(), [](float value) { return value == 0; }),
            "muted output carries no music");

    Soundscape none(mixer, fixtures);
    require(!none.music && !none.battleMusic, "a directory without bgm.mp3 adds no music");

    AudioMixer scenes;
    std::unique_ptr<MusicStream> calm = std::make_unique<MusicStream>();
    std::unique_ptr<MusicStream> battle = std::make_unique<MusicStream>();
    require(calm->open(fixtures + "/card-use.mp3", error) &&
                battle->open(fixtures + "/card-use.mp3", error) && scenes.setMusic(std::move(calm), .4F) &&
                scenes.setBattleMusic(std::move(battle)),
            "a battle track installs beside the main music");
    std::vector<float> block(1024);
    scenes.setBattle(true);
    for (int step = 0; step < 120; ++step)
        scenes.render(block.data(), 512);
    require(scenes.battleBlend() == 1, "a battle fades fully into the battle track");
    scenes.setBattle(false);
    scenes.render(block.data(), 512);
    require(scenes.battleBlend() > 0 && scenes.battleBlend() < 1, "leaving battle fades back gradually");
    scenes.settings.music = 0;
    scenes.render(block.data(), 512);
    require(std::all_of(block.begin(), block.end(), [](float value) { return value == 0; }),
            "a zero music level silences both tracks");
}
/** Check one-shot game routing, card arrival, pause cancellation and mass-hit suppression. */
void routingContracts() {
    AudioMixer mixer;
    Soundscape sound(mixer, "/thorium-no-optional-audio");
    thorium::game::Run run;
    sound.reset(run);
    require(mixer.played == 0 && sound.beepClips == static_cast<int>(Sound::Count),
            "fresh run is quiet and all procedural sounds are registered");
    run.startWave();
    sound.observe(run);
    const std::uint64_t start = mixer.played;
    sound.observe(run);
    require(start == 1 && mixer.played == start,
            "starting a wave emits one cue, repeated observation does not replay it");
    run.play(1, {6, 4});
    sound.observe(run);
    sound.consume(run.takeCombatEvents());
    require(mixer.played == start + 2, "card cycle and placement are independent factual sound requests");
    sound.advance(.1F);
    sound.advance(.1F);
    require(mixer.played == start + 3, "new card arrival sounds once after the visual exit interval");
    sound.advance(.1F);
    require(mixer.played == start + 3, "settled card cannot replay arrival sound");
    run.exchange(3);
    sound.observe(run);
    sound.cancelPending();
    mixer.stop();
    const std::uint64_t canceled = mixer.played;
    sound.advance(.1F);
    sound.advance(.1F);
    require(mixer.played == canceled, "pause/focus cancellation drops delayed card sound");
    const std::uint64_t before = mixer.played;
    std::vector<thorium::game::CombatEvent> hits(100, {thorium::game::CombatEventKind::Damage, 99, {}, {}});
    sound.consume(hits);
    require(mixer.played == before + 1 && mixer.suppressed >= 99,
            "a hundred simultaneous hits do not create a sound storm");
    const std::string state = run.debugState();
    mixer.stop();
    sound.reset(run);
    require(run.debugState() == state, "resetting audio never changes run state");
}
/** Expose PCM consumption without granting a device mutable mixer configuration or scheduling. */
void outputContracts() {
    thorium::engine::AudioMixer mixer;
    const thorium::engine::AudioOutput output = mixer.output();
    require(mixer.setClip(70, thorium::engine::synthesize({440, 660, .1F})), "register output fixture");
    require(mixer.play(70), "play the output fixture through the owning mixer");
    std::vector<float> pcm(1024);
    output.render(pcm.data(), 512);
    require(std::any_of(pcm.begin(), pcm.end(), [](float value) -> bool { return value != 0; }),
            "output-only handle consumes actual PCM from the original mixer");
    const std::uint64_t before = output.generation();
    mixer.stop();
    mixer.settings.muted = true;
    require(output.generation() != before && output.muted(),
            "queue invalidation and mute metadata remain live without exposing mutable fields");
    output.render(pcm.data(), 512);
    require(std::all_of(pcm.begin(), pcm.end(), [](float value) -> bool { return value == 0; }),
            "the output capability honors application-owned stop and mute");
}
/** Keep the center of a larger campaign field centered in stereo after a stage reset. */
void campaignStereoContracts() {
    thorium::game::RunCreation created = thorium::game::Run::create(thorium::game::stageCatalog(3));
    require(static_cast<bool>(created.run), "wide-field audio fixture uses validated campaign content");
    AudioMixer mixer;
    Soundscape sound(mixer, "/thorium-no-optional-audio");
    sound.reset(*created.run);
    const float center = (created.run->catalog().width - 1) / 2.0F;
    sound.consume(
        {{thorium::game::CombatEventKind::Damage, 1, {}, {center, 10}, thorium::game::CardKind::Gun, 12}});
    std::array<float, 1024> pcm{};
    mixer.output().render(pcm.data(), 512);
    float energy = 0;
    float difference = 0;
    for (std::size_t index = 0; index < pcm.size(); index += 2) {
        energy += std::abs(pcm[index]);
        difference += std::abs(pcm[index] - pcm[index + 1]);
    }
    require(energy > 0 && difference < .001F,
            "wide-field center events have audible, balanced left/right PCM");
}
} // namespace

/** Run portable audio and game-routing tests with one real MP3 fixture directory. */
int main(int argc, char **argv) {
    require(argc == 2, "MP3 fixture path supplied by Meson");
    mixerContracts();
    outputContracts();
    assetContracts(argv[1]);
    musicContracts(argv[1]);
    routingContracts();
    campaignStereoContracts();
    std::cout << checks << " audio checks passed\n";
}
