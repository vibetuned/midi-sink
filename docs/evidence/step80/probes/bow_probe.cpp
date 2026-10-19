// bow_probe.cpp — step 80's calibration: the stick-slip bow on the chain and on the lattice. For a grid of bow_force
// (the force scale) at the default speed: the pitch, the output peak, the stick fraction at the bowed node (the chain:
// |v − v_b| < 0.25 v_b), the brightness (the spectral centroid of the held tone), the ledger (E_held against the strike's
// energy plus the bow's work), finite or not.
#include "voxo.h"
#include "suzu.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <initializer_list>
static const uint32_t BLOCK = 128;
static voxo_t* make(uint32_t rate) {
    voxo_config_t c{}; c.sample_rate = rate; c.block_frames = BLOCK; c.max_voices = 4;
    voxo_t* v = voxo_create(&c); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU); return v;
}
static void mcm(voxo_t* v) { voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15); }
static double goertzel(const float* s, size_t n, double hz, double rate) {
    const double w = 2.0 * M_PI * hz / rate, cw = 2.0 * std::cos(w); double s0 = 0, s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; i++) { const double win = 0.5 - 0.5 * std::cos(2.0 * M_PI * (double)i / (double)(n - 1)); s0 = (double)s[i] * win + cw * s1 - s2; s2 = s1; s1 = s0; }
    const double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w); return 4.0 * std::sqrt(re * re + im * im) / (double)n;
}
static double peak_near(const float* s, size_t n, double hz, double rate, double span = 200.0) {
    double best = -1, bf = hz;
    for (double c = -span; c <= span; c += 2.0) { const double f = hz * std::pow(2.0, c / 1200.0); const double m = goertzel(s, n, f, rate); if (m > best) { best = m; bf = f; } }
    return bf;
}
static double centroid(const float* s, size_t n, double f0, double rate) {   // over the harmonics 1..40: Σ k·a_k / Σ a_k
    double num = 0, den = 0;
    for (int k = 1; k <= 40; k++) { const double f = k * f0; if (f > 0.45 * rate) break; const double a = goertzel(s, n, f, rate); num += k * a; den += a; }
    return den > 0 ? num / den : 0.0;
}
int main(int argc, char** argv) {
    const uint32_t rate = 48000; const int kind = argc > 1 ? std::atoi(argv[1]) : 2; const int note = argc > 2 ? std::atoi(argv[2]) : 57;
    const double f0 = 440.0 * std::pow(2.0, (note - 69) / 12.0);
    std::printf("[bow probe] kind %d note %d (%.1f Hz)\n", kind, note, f0);
    for (float speed : { 1.0f }) for (float force : { 0.25f, 0.5f, 1.0f, 2.0f }) {
        voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
        sp.voice_kind = (uint32_t)kind; sp.bow_mode = 1; sp.bow_position = 0.12f; sp.bow_force = force; sp.bow_speed = speed; sp.string_nodes = 48; sp.cutoff_hz = 20000.0f;
        sp.decay_s = 3.0f; sp.string_decay_s = 4.0f; sp.level = 0.25f;
        voxo_set_suzu_params(v, &sp); mcm(v);
        voxo_push_midi(v, 0xB1, 2, 127); voxo_push_midi(v, 0x91, (uint8_t)note, 100);
        const uint32_t frames = rate * 2; std::vector<float> out(2u * frames), s(frames);
        bool fin = true; double stuck = 0, total = 0; double e_over = 0; float pk = 0;
        voxo_inspect_t ins;
        for (uint32_t f = 0; f < frames; f += BLOCK) {
            voxo_render(v, out.data() + 2u * f, BLOCK);
            if (f > rate && voxo_suzu_inspect(v, &ins, 1) && kind == 2) {
                const int ib = (int)ins.k[4]; const float vb = ins.k[2];
                if (vb > 0 && ib > 0) { total++; if (std::fabs(ins.b[ib] - vb) < 0.25f * vb) stuck++; }
            }
        }
        for (uint32_t i = 0; i < frames; i++) { s[i] = out[2u * i]; if (!std::isfinite(s[i])) fin = false; if (i > rate / 2) pk = std::fmax(pk, std::fabs(s[i])); }
        const double pf = fin ? peak_near(s.data() + rate, rate, f0, rate) : 0.0, cen = fin ? centroid(s.data() + rate, rate, f0, rate) : 0.0;
        (void)e_over;
        std::printf("  force %.2f speed %.2f: %s peak %.3f, pitch %+.1f cents, centroid %.2f harmonics, stuck %.0f %% (f %.3g, vb %.3g, work %.3g)\n",
                    force, speed, fin ? "finite" : "NON-FINITE", pk, fin ? 1200.0 * std::log2(pf / f0) : 0.0, cen, total > 0 ? 100.0 * stuck / total : -1.0,
                    kind == 2 ? ins.k[1] : kind == 3 ? ins.k[8] : ins.k[2], kind == 2 ? ins.k[2] : kind == 3 ? ins.k[9] : ins.k[3], kind == 2 ? ins.k[3] : kind == 3 ? ins.k[10] : ins.k[4]);
        voxo_destroy(v);
    }
    return 0;
}
