// shock_probe.cpp — step 81: the trumpet under the brass shock. For breath 60 / 100 / 127 at C4 and G3, three builds
// of the bore — the shock with the limiter, the shock without (the red), no shock — the bore's peak pressure, the
// spectral tilt (the band 6–20 kHz against 0–3 kHz, dB), the ledger (k[9] held ≤ k[8] work, k[10] the shock's loss),
// and the pitch.
#include "voxo.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <initializer_list>
static const uint32_t BLOCK = 128;
static double band(const float* s, size_t n, double lo, double hi, double rate) {   // the power in [lo, hi] by a Goertzel comb every 20 Hz
    double e = 0.0;
    for (double hz = lo; hz < hi; hz += 20.0) {
        const double w = 2.0 * M_PI * hz / rate, cw = 2.0 * std::cos(w); double s0 = 0, s1 = 0, s2 = 0;
        for (size_t i = 0; i < n; i++) { s0 = (double)s[i] + cw * s1 - s2; s2 = s1; s1 = s0; }
        e += s1 * s1 + s2 * s2 - cw * s1 * s2;
    }
    return e;
}
static double peak_near(const float* s, size_t n, double hz, double rate, double span) {
    double best = -1, bf = hz;
    for (double c = -span; c <= span; c += 2.0) {
        const double f = hz * std::pow(2.0, c / 1200.0), w = 2.0 * M_PI * f / rate, cw = 2.0 * std::cos(w); double s0 = 0, s1 = 0, s2 = 0;
        for (size_t i = 0; i < n; i++) { const double win = 0.5 - 0.5 * std::cos(2.0 * M_PI * (double)i / (double)(n - 1)); s0 = (double)s[i] * win + cw * s1 - s2; s2 = s1; s1 = s0; }
        const double m = s1 * s1 + s2 * s2 - cw * s1 * s2; if (m > best) { best = m; bf = f; }
    }
    return bf;
}
int main(int argc, char** argv) {
    const uint32_t rate = 48000; const int kind = argc > 1 ? std::atoi(argv[1]) : 8;
    for (int note : { 60, 55 }) for (int breath : { 60, 100, 127 }) for (int mode : { 0, 1, 2 }) {   // 0 shock + limiter, 1 shock no limiter, 2 no shock
        voxo_config_t c{}; c.sample_rate = rate; c.block_frames = BLOCK; c.max_voices = 4; voxo_t* v = voxo_create(&c); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = (uint32_t)kind;
        sp.brass_shock = mode == 2 ? 0.0f : 1.0f; sp.shock_limiter = mode == 1 ? 0u : 1u;
        if (getenv("SHOCK")) sp.brass_shock = mode == 2 ? 0.0f : (float)atof(getenv("SHOCK"));
        if (getenv("BRASS")) sp.brass = (float)atof(getenv("BRASS"));
        voxo_set_suzu_params(v, &sp);
        voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15);
        voxo_push_midi(v, 0xB1, 2, (uint8_t)breath); voxo_push_midi(v, 0x91, (uint8_t)note, 100);
        const uint32_t frames = rate * 2; std::vector<float> out(2u * frames), s(frames); voxo_inspect_t ins; float pmax = 0; double held = 0, work = 0, loss = 0; bool led = true; bool fin = true;
        for (uint32_t f = 0; f < frames; f += BLOCK) {
            voxo_render(v, out.data() + 2u * f, BLOCK);
            if (voxo_suzu_inspect(v, &ins, 1)) { for (uint32_t i = 0; i < ins.n; i++) pmax = std::fmax(pmax, std::fabs(ins.a[i])); held = ins.k[9]; work = ins.k[8]; loss = ins.k[10]; if (f > rate / 10 && held > work * 1.0001 + 1e-9) led = false; }
        }
        for (uint32_t i = 0; i < frames; i++) { s[i] = out[2u * i]; if (!std::isfinite(s[i])) fin = false; }
        const float* w = s.data() + rate; const size_t n = rate;
        float pk = 0; for (size_t i = 0; i < n; i++) pk = std::fmax(pk, std::fabs(w[i]));
        const double f_nom = 440.0 * std::pow(2.0, (note - 69) / 12.0), f0 = fin ? peak_near(w, n, f_nom, rate, 150.0) : f_nom;
        const double tilt = fin ? 10.0 * std::log10((band(w, n, 6000.0, 20000.0, rate) + 1e-30) / (band(w, n, 20.0, 3000.0, rate) + 1e-30)) : 0.0;
        std::printf("note %d breath %3d %s: %s pitch %+5.1f c, peak %.3f, bore |p|max %.4f (%.1f P_REF), tilt 6–20k/0–3k %+6.1f dB, ledger held/work %.3g/%.3g %s, shock loss %.3g\n",
                    note, breath, mode == 0 ? "shock+limiter" : mode == 1 ? "shock, NO limiter" : "no shock     ", fin ? "finite" : "NON-FINITE", 1200.0 * std::log2(f0 / f_nom), pk, pmax, pmax / 0.005f, tilt, held, work, led ? "ok" : "CROSSED", loss);
        voxo_destroy(v);
    }
    return 0;
}
