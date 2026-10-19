// lips_probe.cpp — step 81: the two-mode lips. For the shear's share 0 / 0.3 / 0.6 (ratio 1.5): the trumpet at C4 and A3
// with CC 74 at 0 / 64 / 127 — the pitch in cents against the note (the registers: below −500, on, above +500), the peak,
// finite; and the bore's cells and λ.
#include "voxo.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <initializer_list>
static const uint32_t BLOCK = 128;
static double period_hz(const float* s, size_t n, double rate) {   // the autocorrelation's first strong lag, then a Goertzel scan
    const size_t lo = (size_t)(rate / 4000.0), hi = (size_t)(rate / 60.0);
    double e0 = 0.0; for (size_t i = 0; i < n - hi; i++) e0 += (double)s[i] * s[i];
    std::vector<double> rr(hi, -2.0); double best = -2.0;
    for (size_t l = lo; l < hi; l++) { double c = 0.0, e1 = 0.0; for (size_t i = 0; i < n - hi; i++) { c += (double)s[i] * s[i + l]; e1 += (double)s[i + l] * s[i + l]; } rr[l] = c / std::sqrt((e0 > 0 ? e0 : 1e-30) * (e1 > 0 ? e1 : 1e-30)); if (rr[l] > best) best = rr[l]; }
    double bl = (double)lo; for (size_t l = lo + 1; l + 1 < hi; l++) if (rr[l] >= best - 0.08 && rr[l] >= rr[l - 1] && rr[l] >= rr[l + 1]) { bl = (double)l; break; }
    return rate / bl;
}
int main() {
    const uint32_t rate = 48000;
    for (float shear : { 0.0f, 0.3f, 0.6f }) for (float ratio : { 1.5f }) for (int note : { 60, 57 }) {
        std::printf("shear %.1f ratio %.1f note %d:", shear, ratio, note);
        for (int c74 : { 0, 64, 127 }) {
            voxo_config_t c{}; c.sample_rate = rate; c.block_frames = BLOCK; c.max_voices = 4; voxo_t* v = voxo_create(&c); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
            voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 8; sp.lip_shear = shear; sp.lip_shear_ratio = ratio; voxo_set_suzu_params(v, &sp);
            voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15);
            voxo_push_midi(v, 0xB1, 74, (uint8_t)c74); voxo_push_midi(v, 0xB1, 2, 70); voxo_push_midi(v, 0x91, (uint8_t)note, 100);
            const uint32_t frames = rate; std::vector<float> out(2u * frames), s(frames);
            for (uint32_t f = 0; f < frames; f += BLOCK) voxo_render(v, out.data() + 2u * f, BLOCK);
            bool fin = true; float pk = 0; for (uint32_t i = 0; i < frames; i++) { s[i] = out[2u * i]; if (!std::isfinite(s[i])) fin = false; if (i > rate / 2) pk = std::fmax(pk, std::fabs(s[i])); }
            const double f_nom = 440.0 * std::pow(2.0, (note - 69) / 12.0);
            const double f = fin && pk > 1e-3 ? period_hz(s.data() + rate / 2, rate / 2, rate) : 0.0;
            std::printf("  cc74 %3d: %s%+7.0f c (pk %.3f)", c74, fin ? "" : "NON-FINITE ", f > 0 ? 1200.0 * std::log2(f / f_nom) : -9999.0, pk);
            voxo_destroy(v);
        }
        std::printf("\n");
    }
    return 0;
}
