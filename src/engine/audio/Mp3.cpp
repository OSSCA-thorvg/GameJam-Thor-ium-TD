#include "Audio.h"
#include <algorithm>
#include <array>
#include <fstream>
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

namespace thorium::engine {
AudioClip loadMp3(const std::string &path, std::string &error) {
    error.clear();
    AudioClip clip;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "MP3 file not found";
        return clip;
    }
    const std::streamoff size = file.tellg();
    if (size <= 0 || size > 4 * 1024 * 1024) {
        error = "MP3 must be between 1 byte and 4 MiB";
        return clip;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char *>(bytes.data()), size)) {
        error = "MP3 read failed";
        return clip;
    }
    drmp3 decoder{};
    if (!drmp3_init_memory(&decoder, bytes.data(), bytes.size(), nullptr)) {
        error = "Invalid MP3 data";
        return clip;
    }
    clip.sampleRate = static_cast<int>(decoder.sampleRate);
    clip.channels = static_cast<int>(decoder.channels);
    if (clip.sampleRate < 8000 || clip.sampleRate > 192000 || (clip.channels != 1 && clip.channels != 2)) {
        drmp3_uninit(&decoder);
        error = "Unsupported MP3 format";
        return {};
    }
    const std::size_t limit = static_cast<std::size_t>(clip.sampleRate) * clip.channels * 10;
    std::array<float, 8192> block{};
    for (;;) {
        const drmp3_uint64 read = drmp3_read_pcm_frames_f32(&decoder, 4096, block.data());
        if (!read)
            break;
        const std::size_t count = static_cast<std::size_t>(read) * clip.channels;
        if (clip.samples.size() + count > limit) {
            error = "MP3 SFX exceeds ten seconds";
            break;
        }
        clip.samples.insert(clip.samples.end(), block.begin(), block.begin() + count);
    }
    drmp3_uninit(&decoder);
    // Real-world MP3 reconstruction can exceed full scale slightly; normalize the decoder boundary.
    for (float &sample : clip.samples)
        sample = std::clamp(sample, -1.0F, 1.0F);
    if (!error.empty() || !clip.valid()) {
        if (error.empty())
            error = "MP3 contains no valid PCM";
        return {};
    }
    return clip;
}

struct MusicStream::Decoder {
    static constexpr std::size_t BlockFrames = 2048;
    std::vector<std::uint8_t> bytes;
    drmp3 mp3{};
    bool ready = false;
    int channels = 2;
    int sampleRate = 48000;
    std::vector<float> block;
    std::size_t frames = 0;
    double cursor = 0;
    std::uint64_t loops = 0;
    ~Decoder() {
        if (ready)
            drmp3_uninit(&mp3);
    }
    /** Decode the next block, seeking to the first frame once when the track ends. */
    bool fill() {
        for (int attempt = 0; attempt < 2; ++attempt) {
            const drmp3_uint64 read = drmp3_read_pcm_frames_f32(&mp3, BlockFrames, block.data());
            if (read) {
                frames = static_cast<std::size_t>(read);
                return true;
            }
            if (!drmp3_seek_to_pcm_frame(&mp3, 0))
                return false;
            ++loops;
        }
        return false;
    }
};
MusicStream::MusicStream() = default;
MusicStream::~MusicStream() = default;
bool MusicStream::open(const std::string &path, std::string &error) {
    error.clear();
    decoder_.reset();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "MP3 file not found";
        return false;
    }
    const std::streamoff size = file.tellg();
    if (size <= 0 || size > 16 * 1024 * 1024) {
        error = "Music MP3 must be between 1 byte and 16 MiB";
        return false;
    }
    std::unique_ptr<Decoder> decoder = std::make_unique<Decoder>();
    decoder->bytes.resize(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char *>(decoder->bytes.data()), size)) {
        error = "MP3 read failed";
        return false;
    }
    if (!drmp3_init_memory(&decoder->mp3, decoder->bytes.data(), decoder->bytes.size(), nullptr)) {
        error = "Invalid MP3 data";
        return false;
    }
    decoder->ready = true;
    decoder->channels = static_cast<int>(decoder->mp3.channels);
    decoder->sampleRate = static_cast<int>(decoder->mp3.sampleRate);
    if (decoder->sampleRate < 8000 || decoder->sampleRate > 192000 ||
        (decoder->channels != 1 && decoder->channels != 2)) {
        error = "Unsupported MP3 format";
        return false;
    }
    decoder->block.resize(Decoder::BlockFrames * decoder->channels);
    if (!decoder->fill()) {
        error = "MP3 contains no valid PCM";
        return false;
    }
    decoder->loops = 0;
    decoder_ = std::move(decoder);
    return true;
}
bool MusicStream::loaded() const {
    return decoder_ != nullptr;
}
std::uint64_t MusicStream::loops() const {
    return decoder_ ? decoder_->loops : 0;
}
void MusicStream::read(float *output, std::size_t frames, int outputRate) {
    if (!output)
        return;
    std::fill(output, output + frames * 2, 0.0F);
    if (!decoder_ || outputRate <= 0)
        return;
    Decoder &source = *decoder_;
    const double step = static_cast<double>(source.sampleRate) / outputRate;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        while (source.cursor >= source.frames) {
            source.cursor -= source.frames;
            if (!source.fill()) {
                decoder_.reset();
                return;
            }
        }
        const std::size_t a = static_cast<std::size_t>(source.cursor);
        const std::size_t b = std::min(a + 1, source.frames - 1);
        const float fraction = static_cast<float>(source.cursor - a);
        for (int channel = 0; channel < 2; ++channel) {
            const int from = source.channels == 1 ? 0 : channel;
            const float sample = source.block[a * source.channels + from] * (1 - fraction) +
                                 source.block[b * source.channels + from] * fraction;
            output[frame * 2 + channel] = std::clamp(sample, -1.0F, 1.0F);
        }
        source.cursor += step;
    }
}
} // namespace thorium::engine
