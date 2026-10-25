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

// Step 88: the chorus and the phaser below are compiled WITHOUT fused multiply-add contraction, so their samples are
// the same bits in the Debug, the Release and the no-FMA reference builds (the roadmap's gate: bit-identical across
// the native builds at a fixed seed). The reverb and the delay above keep the compiler's default.
#if defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize ("fp-contract=off")
#elif defined(_MSC_VER)
#pragma fp_contract (off)
#endif

namespace {
inline float hermite4(float xm1, float x0, float x1, float x2, float t) {   // the §12.2 read: Catmull–Rom tangents
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}
}

void Chorus::clear() {
    std::memset(buf, 0, sizeof buf);
    idx = 0; lx = 0.0f; ly = 1.0f;
}

void Chorus::set(float mix_, float depth_, float rate_hz_) {
    mix = clampf(mix_, 0.0f, 1.0f); depth = clampf(depth_, 0.0f, 1.0f); rate_hz = clampf(rate_hz_, 0.0f, 10.0f);
}

void Chorus::process(float* lr, uint32_t frames, uint32_t rate) {
    const float eps = 2.0f * std::sin(3.14159265f * rate_hz / (float)rate);   // the magic circle's prewarp, once a block
    const float per_ms = (float)rate * 0.001f;
    const float d0 = CENTRE_MS * per_ms, dm = SWING_MS * per_ms * depth;
    const float wet = mix, dry = 1.0f - mix;
    for (uint32_t f = 0; f < frames; f++) {
        lx -= eps * ly; ly += eps * lx;                                         // the LFO: one leapfrog step, (sin, cos)
        const float x0 = lr[2 * f], x1 = lr[2 * f + 1];
        buf[0][idx] = x0; buf[1][idx] = x1;
        if (wet > 0.0f) {
            const float d[2] = {d0 + dm * lx, d0 + dm * ly};                    // samples back; ≥ 3 ms, ≤ 11 ms
            float out[2];
            for (int ch = 0; ch < 2; ch++) {
                const float r = (float)idx - d[ch] + (float)(2u * LEN);            // the read point, kept positive
                const uint32_t k = (uint32_t)r;                                    // floor
                const float t = r - (float)k;
                const float* b = buf[ch];
                out[ch] = hermite4(b[(k - 1u) % LEN], b[k % LEN], b[(k + 1u) % LEN], b[(k + 2u) % LEN], t);
            }
            lr[2 * f]     = dry * x0 + wet * out[0];
            lr[2 * f + 1] = dry * x1 + wet * out[1];
        }
        if (++idx >= LEN) idx = 0;
    }
}

void Phaser::clear() {
    std::memset(x1, 0, sizeof x1); std::memset(y1, 0, sizeof y1);
    last[0] = last[1] = 0.0f; lx = 0.0f; ly = 1.0f;
}

void Phaser::set(float mix_, float depth_, float rate_hz_, float center_hz_, float feedback_) {
    mix = clampf(mix_, 0.0f, 1.0f); depth = clampf(depth_, 0.0f, 1.0f); rate_hz = clampf(rate_hz_, 0.0f, 10.0f);
    center_hz = clampf(center_hz_, 20.0f, 22000.0f); feedback = clampf(feedback_, -0.95f, 0.95f);   // the format's ±1 is the loop's margin
}

void Phaser::process(float* lr, uint32_t frames, uint32_t rate) {
    if (mix <= 0.0f) return;
    const float eps = 2.0f * std::sin(3.14159265f * rate_hz / (float)rate);
    const float fmax = 0.45f * (float)rate;
    const float wet = mix, dry = 1.0f - mix, fb = feedback;
    float a[2] = {0.0f, 0.0f};
    for (uint32_t f = 0; f < frames; f++) {
        lx -= eps * ly; ly += eps * lx;
        if ((f & 15u) == 0u) {                                                  // the coefficient every sixteen samples: a tan a channel
            const float l[2] = {lx, ly};
            for (int ch = 0; ch < 2; ch++) {
                const float fc = clampf(center_hz * std::exp2(2.0f * depth * l[ch]), 20.0f, fmax);
                const float tn = std::tan(3.14159265f * fc / (float)rate);
                a[ch] = (tn - 1.0f) / (tn + 1.0f);
            }
        }
        for (int ch = 0; ch < 2; ch++) {
            const float x = lr[2 * f + ch];
            float s = x + fb * last[ch];
            for (int k = 0; k < STAGES; k++) {                                   // y = a·x + x[n−1] − a·y[n−1]: H = (a + z⁻¹)/(1 + a z⁻¹)
                const float y = a[ch] * s + x1[ch][k] - a[ch] * y1[ch][k];
                x1[ch][k] = s; y1[ch][k] = y; s = y;
            }
            last[ch] = undenormal(s);
            lr[2 * f + ch] = dry * x + wet * s;
        }
    }
}

} // namespace voxo_bus
