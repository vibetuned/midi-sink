// shock_probe2.cpp — step 81: the unlimited shock. brass_shock 1 / 2 / 4, the limiter on / off, C4 and G3 at breath 127 (2 s):
// finite, the tilt 6–20k/0–3k, the roughness Σ|Δ²p|/Σ|Δp| of the bore's profile (the worst snapshot), the ledger, |p|max, the output slew.
#include "voxo.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <initializer_list>
static double band(const float* s, size_t n, double lo, double hi, double rate) {
    double e = 0.0;
    for (double hz = lo; hz < hi; hz += 20.0) { const double w = 2.0 * M_PI * hz / rate, cw = 2.0 * std::cos(w); double s0 = 0, s1 = 0, s2 = 0;
        for (size_t i = 0; i < n; i++) { s0 = (double)s[i] + cw * s1 - s2; s2 = s1; s1 = s0; } e += s1 * s1 + s2 * s2 - cw * s1 * s2; }
    return e;
}
int main() {
    const uint32_t rate = 48000;
    for (int note : { 60, 55 }) for (float shock : { 1.0f, 2.0f, 4.0f }) for (uint32_t lim : { 1u, 0u }) {
        voxo_config_t c{}; c.sample_rate = rate; c.block_frames = 128; c.max_voices = 4; voxo_t* v = voxo_create(&c); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 8; sp.brass_shock = shock; sp.shock_limiter = lim; voxo_set_suzu_params(v, &sp);
        voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15);
        voxo_push_midi(v, 0xB1, 2, 127); voxo_push_midi(v, 0x91, (uint8_t)note, 100);
        const uint32_t frames = rate * 2; std::vector<float> out(2u * frames), s(frames); voxo_inspect_t ins; float pmax = 0; double held = 0, work = 0, loss = 0, rough = 0; bool led = true, fin = true;
        for (uint32_t f = 0; f < frames; f += 128) {
            voxo_render(v, out.data() + 2u * f, 128);
            if (voxo_suzu_inspect(v, &ins, 1)) { double d1 = 0, d2 = 0; for (uint32_t i = 0; i < ins.n; i++) pmax = std::fmax(pmax, std::fabs(ins.a[i])); for (uint32_t i = 1; i < ins.n; i++) d1 += std::fabs(ins.a[i] - ins.a[i - 1]); for (uint32_t i = 1; i + 1 < ins.n; i++) d2 += std::fabs(ins.a[i + 1] - 2 * ins.a[i] + ins.a[i - 1]); if (f > rate / 2 && d1 > 0) rough = std::fmax(rough, d2 / d1);
                held = ins.k[9]; work = ins.k[8]; loss = ins.k[10]; if (f > rate / 10 && held > work * 1.0001 + 1e-9) led = false; }
        }
        float slew = 0, pk = 0; for (uint32_t i = 0; i < frames; i++) { s[i] = out[2u * i]; if (!std::isfinite(s[i])) fin = false; if (i > rate) { pk = std::fmax(pk, std::fabs(s[i])); slew = std::fmax(slew, std::fabs(s[i] - s[i - 1])); } }
        const double tilt = fin ? 10.0 * std::log10((band(s.data() + rate, rate, 6000, 20000, rate) + 1e-30) / (band(s.data() + rate, rate, 20, 3000, rate) + 1e-30)) : 0.0;
        std::printf("note %d shock %.0f limiter %s: %s tilt %+6.1f dB, roughness %.2f, |p|max %.1f P_REF, slew/peak %.2f, ledger held/work %.3g/%.3g %s, loss %.3g\n",
                    note, shock, lim ? "on " : "OFF", fin ? "finite" : "NON-FINITE", tilt, rough, pmax / 0.005f, pk > 0 ? slew / pk : 0.0, held, work, led ? "ok" : "CROSSED", loss);
        voxo_destroy(v);
    }
    return 0;
}
