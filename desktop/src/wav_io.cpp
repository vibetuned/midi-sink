// wav_io.cpp — see wav_io.h. Chunk-walking RIFF reader: "fmt " then "data",
// other chunks skipped (LIST, fact, cue, bext...), odd chunk sizes padded.
#include "wav_io.h"

#include <cstdio>
#include <cstring>

namespace {

uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
void wr32(std::vector<uint8_t>& o, uint32_t v) { o.push_back((uint8_t)v); o.push_back((uint8_t)(v >> 8)); o.push_back((uint8_t)(v >> 16)); o.push_back((uint8_t)(v >> 24)); }
void wr16(std::vector<uint8_t>& o, uint16_t v) { o.push_back((uint8_t)v); o.push_back((uint8_t)(v >> 8)); }

} // namespace

bool wav_read(const std::string& path, WavData* out, std::string* why) {
    auto fail = [&](const char* m) { if (why) *why = m; return false; };
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return fail("cannot open the file");
    std::vector<uint8_t> bytes;
    {
        std::fseek(f, 0, SEEK_END);
        const long n = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (n <= 12 || n > (256L << 20)) { std::fclose(f); return fail("not a WAV, or larger than 256 MB"); }
        bytes.resize((size_t)n);
        const size_t got = std::fread(bytes.data(), 1, (size_t)n, f);
        std::fclose(f);
        if (got != (size_t)n) return fail("short read");
    }
    const uint8_t* b = bytes.data();
    const size_t size = bytes.size();
    if (std::memcmp(b, "RIFF", 4) != 0 || std::memcmp(b + 8, "WAVE", 4) != 0) return fail("not a RIFF/WAVE file");
    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    const uint8_t* data = nullptr;
    uint32_t data_size = 0;
    size_t pos = 12;
    while (pos + 8 <= size) {
        const uint32_t csize = rd32(b + pos + 4);
        const uint8_t* body = b + pos + 8;
        const size_t avail = size - (pos + 8);
        if (std::memcmp(b + pos, "fmt ", 4) == 0) {
            if (csize < 16 || avail < 16) return fail("malformed fmt chunk");
            format = rd16(body); channels = rd16(body + 2); rate = rd32(body + 4); bits = rd16(body + 14);
            if (format == 0xFFFE && csize >= 40 && avail >= 40) format = rd16(body + 24);   // WAVE_FORMAT_EXTENSIBLE: the sub-format's first word
        } else if (std::memcmp(b + pos, "data", 4) == 0) {
            data = body;
            data_size = (uint32_t)(csize <= avail ? csize : avail);
            break;
        }
        pos += 8 + (size_t)csize + (csize & 1u);
    }
    if (!data) return fail("no data chunk");
    if (channels != 1 && channels != 2) return fail("only mono and stereo are read");
    if (rate < 8000 || rate > 192000) return fail("unsupported sample rate");
    const bool is_float = format == 3;
    if (!(format == 1 || is_float)) return fail("only PCM and IEEE float WAV are read");
    if (!((format == 1 && (bits == 16 || bits == 24 || bits == 32)) || (is_float && bits == 32))) return fail("unsupported bit depth");
    const uint32_t bps = bits / 8u;
    const uint32_t frames = data_size / (bps * channels);
    out->channels = channels;
    out->rate = rate;
    out->frames.resize((size_t)frames * channels);
    const uint8_t* p = data;
    for (size_t i = 0; i < out->frames.size(); i++, p += bps) {
        float v;
        if (is_float) { std::memcpy(&v, p, 4); }
        else if (bits == 16) { v = (float)(int16_t)rd16(p) / 32768.0f; }
        else if (bits == 24) { int32_t x = (int32_t)(((uint32_t)p[0] << 8) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 24)); v = (float)(x >> 8) / 8388608.0f; }
        else { v = (float)(int32_t)rd32(p) / 2147483648.0f; }
        out->frames[i] = v;
    }
    return true;
}

bool wav_write(const std::string& path, const float* frames, uint32_t frame_count,
               uint32_t channels, uint32_t rate, std::string* why) {
    auto fail = [&](const char* m) { if (why) *why = m; return false; };
    if (!frames || channels == 0 || channels > 2) return fail("bad arguments");
    std::vector<uint8_t> o;
    const uint32_t data_size = frame_count * channels * 4u;
    o.reserve(58 + data_size);
    o.insert(o.end(), {'R','I','F','F'}); wr32(o, 4 + 26 + 12 + 8 + data_size); o.insert(o.end(), {'W','A','V','E'});
    o.insert(o.end(), {'f','m','t',' '}); wr32(o, 18);
    wr16(o, 3); wr16(o, (uint16_t)channels); wr32(o, rate); wr32(o, rate * channels * 4u); wr16(o, (uint16_t)(channels * 4u)); wr16(o, 32); wr16(o, 0);
    o.insert(o.end(), {'f','a','c','t'}); wr32(o, 4); wr32(o, frame_count);
    o.insert(o.end(), {'d','a','t','a'}); wr32(o, data_size);
    const size_t at = o.size();
    o.resize(at + data_size);
    std::memcpy(o.data() + at, frames, data_size);
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return fail("cannot create the file");
    const size_t w = std::fwrite(o.data(), 1, o.size(), f);
    std::fclose(f);
    return w == o.size() ? true : fail("short write");
}
