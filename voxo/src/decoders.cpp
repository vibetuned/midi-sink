// decoders.cpp — the sample decoders behind ds_preset.h (Phase 7 step 50,
// SOUND §3): dr_wav and dr_flac (their ONE implementation TU is this file),
// and a small AIFF / AIFF-C (PCM) reader of our own — the stock Decent
// Sampler piano ships AIFF, which answered the format's [ITERATE]
// (DECISIONS_6 #14). Everything decodes to interleaved float in memory.
#include "ds_preset.h"

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO
#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "dr_wav.h"
#include "dr_flac.h"
#if defined(__clang__)
  #pragma clang diagnostic pop
#elif defined(__GNUC__)
  #pragma GCC diagnostic pop
#endif

#include <cmath>
#include <cstring>

namespace voxo_ds {

namespace {

uint32_t be32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
uint16_t be16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }

// IEEE 754 80-bit extended, big-endian: the AIFF sample rate.
double read_extended(const uint8_t* p) {
    const int sign = p[0] & 0x80 ? -1 : 1;
    const int exp = ((p[0] & 0x7F) << 8) | p[1];
    uint64_t mant = 0;
    for (int i = 0; i < 8; i++) mant = (mant << 8) | p[2 + i];
    if (exp == 0 && mant == 0) return 0.0;
    return sign * std::ldexp((double)mant, exp - 16383 - 63);
}

bool decode_aiff(const uint8_t* b, size_t size, std::vector<float>* frames, uint32_t* channels, uint32_t* rate, std::string* why) {
    auto fail = [&](const char* m) { if (why) *why = m; return false; };
    if (size < 12 || std::memcmp(b, "FORM", 4) != 0) return fail("not an AIFF file");
    const bool aifc = std::memcmp(b + 8, "AIFC", 4) == 0;
    if (!aifc && std::memcmp(b + 8, "AIFF", 4) != 0) return fail("not an AIFF file");
    uint32_t ch = 0, bits = 0; uint64_t nframes = 0; double sr = 0.0;
    bool little = false, have_comm = false;
    const uint8_t* ssnd = nullptr; size_t ssnd_size = 0;
    size_t pos = 12;
    while (pos + 8 <= size) {
        const uint32_t csize = be32(b + pos + 4);
        const uint8_t* body = b + pos + 8;
        const size_t avail = size - (pos + 8);
        const size_t use = csize <= avail ? csize : avail;
        if (std::memcmp(b + pos, "COMM", 4) == 0 && use >= 18) {
            ch = be16(body); nframes = be32(body + 2); bits = be16(body + 6); sr = read_extended(body + 8);
            have_comm = true;
            if (aifc && use >= 22) {
                if (std::memcmp(body + 18, "sowt", 4) == 0) little = true;
                else if (std::memcmp(body + 18, "NONE", 4) != 0 && std::memcmp(body + 18, "twos", 4) != 0) return fail("compressed AIFF-C is not read");
            }
        } else if (std::memcmp(b + pos, "SSND", 4) == 0 && use >= 8) {
            const uint32_t offset = be32(body);
            if (offset > use - 8) return fail("malformed SSND chunk");
            ssnd = body + 8 + offset; ssnd_size = use - 8 - offset;
        }
        pos += 8 + (size_t)csize + (csize & 1u);
    }
    if (!have_comm || !ssnd) return fail("no COMM/SSND chunk");
    if (ch < 1 || ch > 2) return fail("only mono and stereo are read");
    if (bits != 8 && bits != 16 && bits != 24 && bits != 32) return fail("unsupported bit depth");
    if (sr < 1000.0 || sr > 384000.0) return fail("unsupported sample rate");
    const uint32_t bps = bits / 8;
    const uint64_t avail_frames = ssnd_size / (bps * ch);
    if (nframes > avail_frames) nframes = avail_frames;
    if (nframes == 0) return fail("no audio frames");
    if (nframes * ch > (1ull << 28)) return fail("the sample is too large");
    frames->resize((size_t)(nframes * ch));
    const uint8_t* p = ssnd;
    for (size_t i = 0; i < frames->size(); i++, p += bps) {
        int32_t v = 0;
        if (bits == 8) v = (int8_t)p[0] << 24;
        else if (bits == 16) v = little ? (int32_t)((int16_t)(p[0] | (p[1] << 8))) << 16 : (int32_t)((int16_t)be16(p)) << 16;
        else if (bits == 24) v = little ? (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24)
                                        : (int32_t)((uint32_t)p[2] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[0] << 24);
        else v = little ? (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24) : (int32_t)be32(p);
        (*frames)[i] = (float)v / 2147483648.0f;
    }
    *channels = ch; *rate = (uint32_t)sr;
    return true;
}

} // namespace

bool decode_audio(const std::string& name_hint, const uint8_t* bytes, size_t size,
                  std::vector<float>* frames, uint32_t* channels, uint32_t* rate, std::string* why) {
    auto fail = [&](const char* m) { if (why) *why = m; return false; };
    if (!bytes || size < 12) return fail("the file is too small to be audio");
    // Sniff the container rather than trust the extension.
    if (std::memcmp(bytes, "RIFF", 4) == 0 || std::memcmp(bytes, "RF64", 4) == 0 || std::memcmp(bytes, "riff", 4) == 0) {
        unsigned int ch = 0, sr = 0; drwav_uint64 n = 0;
        float* data = drwav_open_memory_and_read_pcm_frames_f32(bytes, size, &ch, &sr, &n, nullptr);
        if (!data) return fail("WAV could not be decoded");
        if (ch < 1 || ch > 2 || n == 0 || n * ch > (1ull << 28)) { drwav_free(data, nullptr); return fail(ch > 2 ? "only mono and stereo are read" : "no audio frames"); }
        frames->assign(data, data + n * ch);
        drwav_free(data, nullptr);
        *channels = ch; *rate = sr;
        return true;
    }
    if (std::memcmp(bytes, "fLaC", 4) == 0) {
        unsigned int ch = 0, sr = 0; drflac_uint64 n = 0;
        float* data = drflac_open_memory_and_read_pcm_frames_f32(bytes, size, &ch, &sr, &n, nullptr);
        if (!data) return fail("FLAC could not be decoded");
        if (ch < 1 || ch > 2 || n == 0 || n * ch > (1ull << 28)) { drflac_free(data, nullptr); return fail(ch > 2 ? "only mono and stereo are read" : "no audio frames"); }
        frames->assign(data, data + n * ch);
        drflac_free(data, nullptr);
        *channels = ch; *rate = sr;
        return true;
    }
    if (std::memcmp(bytes, "FORM", 4) == 0) return decode_aiff(bytes, size, frames, channels, rate, why);
    (void)name_hint;
    return fail("not a WAV, FLAC or AIFF file");
}

} // namespace voxo_ds
