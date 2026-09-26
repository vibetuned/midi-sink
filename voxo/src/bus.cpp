// bus.cpp — see bus.h.
#include "bus.h"

#include <cmath>
#include <cstring>

namespace voxo_bus {

namespace {
constexpr int COMB_LEN[Reverb::COMBS] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
constexpr int ALLPASS_LEN[Reverb::ALLPASSES] = {556, 441, 341, 225};
constexpr int STEREO_SPREAD = 23;
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
inline float undenormal(float x) { return std::fabs(x) < 1e-18f ? 0.0f : x; }
}

void Reverb::allocate(uint32_t rate) {
    const double scale = (double)rate / 44100.0;
    for (int ch = 0; ch < 2; ch++) {
        for (int i = 0; i < COMBS; i++) {
            const uint32_t n = (uint32_t)std::lround((COMB_LEN[i] + (ch ? STEREO_SPREAD : 0)) * scale);
            comb[ch][i].buf.assign(n ? n : 1, 0.0f); comb[ch][i].idx = 0; comb[ch][i].filterstore = 0.0f;
        }
        for (int i = 0; i < ALLPASSES; i++) {
            const uint32_t n = (uint32_t)std::lround((ALLPASS_LEN[i] + (ch ? STEREO_SPREAD : 0)) * scale);
            allpass[ch][i].buf.assign(n ? n : 1, 0.0f); allpass[ch][i].idx = 0;
        }
    }
}

void Reverb::clear() {
    for (int ch = 0; ch < 2; ch++) {
        for (int i = 0; i < COMBS; i++) { std::memset(comb[ch][i].buf.data(), 0, comb[ch][i].buf.size() * sizeof(float)); comb[ch][i].filterstore = 0.0f; }
        for (int i = 0; i < ALLPASSES; i++) std::memset(allpass[ch][i].buf.data(), 0, allpass[ch][i].buf.size() * sizeof(float));
    }
}

void Reverb::set(const Params& p) {
    // Freeverb's scaling: room 0..1 -> feedback 0.7..0.98, damping 0..1 -> 0..0.4.
    feedback = 0.7f + 0.28f * clampf(p.room_size, 0.0f, 1.0f);
    damp1 = 0.4f * clampf(p.damping, 0.0f, 1.0f);
    damp2 = 1.0f - damp1;
    wet = clampf(p.reverb_wet, 0.0f, 1.0f);
}

void Reverb::process(float* lr, uint32_t frames) {
    if (wet <= 0.0f) return;
    const float gain = 0.015f * 3.0f;   // Freeverb's fixed input gain, times a wet scale that lands 100% wet near unity
    for (uint32_t f = 0; f < frames; f++) {
        const float in = (lr[2 * f] + lr[2 * f + 1]) * gain;
        float out[2] = {0.0f, 0.0f};
        for (int ch = 0; ch < 2; ch++) {
            for (int i = 0; i < COMBS; i++) {
                Comb& c = comb[ch][i];
                const float y = c.buf[c.idx];
                c.filterstore = undenormal(y * damp2 + c.filterstore * damp1);
                c.buf[c.idx] = in + c.filterstore * feedback;
                if (++c.idx >= c.buf.size()) c.idx = 0;
                out[ch] += y;
            }
            for (int i = 0; i < ALLPASSES; i++) {
                Allpass& a = allpass[ch][i];
                const float bufout = a.buf[a.idx];
                const float y = -out[ch] + bufout;
                a.buf[a.idx] = undenormal(out[ch] + bufout * 0.5f);
                if (++a.idx >= a.buf.size()) a.idx = 0;
                out[ch] = y;
            }
        }
        lr[2 * f]     += out[0] * wet;
        lr[2 * f + 1] += out[1] * wet;
    }
}

void Delay::allocate(uint32_t rate) {
    len = rate * 2 + 1;
    buf[0].assign(len, 0.0f); buf[1].assign(len, 0.0f);
    idx = 0;
}

void Delay::clear() {
    std::memset(buf[0].data(), 0, len * sizeof(float));
    std::memset(buf[1].data(), 0, len * sizeof(float));
}

void Delay::set(const Params& p, uint32_t rate) {
    const float t = clampf(p.delay_time, 0.001f, 2.0f);
    const float off = clampf(p.stereo_offset, -0.5f, 0.5f);
    time_l = (uint32_t)(t * (float)rate);
    time_r = (uint32_t)(clampf(t + off, 0.001f, 2.0f) * (float)rate);
    if (time_l >= len) time_l = len - 1;
    if (time_r >= len) time_r = len - 1;
    if (time_l == 0) time_l = 1;
    if (time_r == 0) time_r = 1;
    feedback = clampf(p.feedback, 0.0f, 0.95f);
    wet = clampf(p.delay_wet, 0.0f, 1.0f);
}

void Delay::process(float* lr, uint32_t frames) {
    if (wet <= 0.0f) return;
    for (uint32_t f = 0; f < frames; f++) {
        const uint32_t rl = (idx + len - time_l) % len, rr = (idx + len - time_r) % len;
        const float dl = buf[0][rl], dr = buf[1][rr];
        buf[0][idx] = undenormal(lr[2 * f] + dl * feedback);
        buf[1][idx] = undenormal(lr[2 * f + 1] + dr * feedback);
        if (++idx >= len) idx = 0;
        lr[2 * f]     += dl * wet;
        lr[2 * f + 1] += dr * wet;
    }
}

} // namespace voxo_bus
