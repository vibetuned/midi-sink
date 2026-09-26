// wav_io.h — a small RIFF/WAVE reader and writer for the desktop shell and the
// lab bench (Phase 7 step 49): the author's sample into Voxo, the glide bounce
// out to the spectral check. PCM 16/24/32-bit and IEEE float 32, 1 or 2
// channels, any rate; everything else is refused with a reason. Voxo's own
// decoding (dr_wav/dr_flac, step 50) is the sampler's business, not this.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct WavData {
    std::vector<float> frames;   // interleaved, -1..1
    uint32_t channels = 0;
    uint32_t rate = 0;
    uint32_t frame_count() const { return channels ? (uint32_t)(frames.size() / channels) : 0; }
};

// Reads the file; false with `why` filled on any refusal.
bool wav_read(const std::string& path, WavData* out, std::string* why);
// Writes IEEE float 32 interleaved frames.
bool wav_write(const std::string& path, const float* frames, uint32_t frame_count,
               uint32_t channels, uint32_t rate, std::string* why);
