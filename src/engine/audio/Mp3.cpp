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
} // namespace thorium::engine
