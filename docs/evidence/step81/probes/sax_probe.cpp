// sax_probe.cpp — step 81: the sax's sidebands. A3 (and C4) held under breath 50 / 70 / 100 for 2 s; the last second's
// spectrum: for each of the first four harmonics the largest Goertzel between 15 and 120 Hz off the harmonic, relative
// to the harmonic itself (dB) — a periodic tone reads under −45 dB (the Hann window's skirt); a quasi-periodic one shows
// its modulation sidebands. Also the pitch and the peak.
#include "voxo.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <initializer_list>
static const uint32_t BLOCK = 128;
static double goertzel(const float* s, size_t n, double hz, double rate) {
    const double w = 2.0 * M_PI * hz / rate, cw = 2.0 * std::cos(w); double s0 = 0, s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; i++) { const double win = 0.5 - 0.5 * std::cos(2.0 * M_PI * (double)i / (double)(n - 1)); s0 = (double)s[i] * win + cw * s1 - s2; s2 = s1; s1 = s0; }
    const double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w); return 4.0 * std::sqrt(re * re + im * im) / (double)n;
}
static double peak_near(const float* s, size_t n, double hz, double rate, double span) {
    double best = -1, bf = hz;
    for (double c = -span; c <= span; c += 1.0) { const double f = hz * std::pow(2.0, c / 1200.0); const double m = goertzel(s, n, f, rate); if (m > best) { best = m; bf = f; } }
    return bf;
}
int main(int argc, char** argv) {
    const uint32_t rate = 48000;
    const float inert = argc > 1 ? (float)std::atof(argv[1]) : -1.0f;
    for (int note : { 57, 60, 52 }) for (int breath : { 50, 70, 100 }) {
        voxo_config_t c{}; c.sample_rate = rate; c.block_frames = BLOCK; c.max_voices = 4; voxo_t* v = voxo_create(&c); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 7;
#ifdef HAVE_INERT
        if (inert >= 0.0f) sp.reed_inertance = inert;
#endif
        voxo_set_suzu_params(v, &sp);
        voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15);
        voxo_push_midi(v, 0xB1, 2, (uint8_t)breath); voxo_push_midi(v, 0x91, (uint8_t)note, 100);
        const uint32_t frames = rate * 2; std::vector<float> out(2u * frames), s(frames);
        for (uint32_t f = 0; f < frames; f += BLOCK) voxo_render(v, out.data() + 2u * f, BLOCK);
        for (uint32_t i = 0; i < frames; i++) s[i] = out[2u * i];
        const double f_nom = 440.0 * std::pow(2.0, (note - 69) / 12.0);
        const float* w = s.data() + rate; const size_t n = rate;
        const double f0 = peak_near(w, n, f_nom, rate, 120.0);
        float pk = 0; for (size_t i = 0; i < n; i++) pk = std::fmax(pk, std::fabs(w[i]));
        double worst = -200.0; int worst_k = 0; double worst_off = 0; std::string line;
        for (int k = 1; k <= 4; k++) {
            const double fk = k * f0, hk = goertzel(w, n, fk, rate);
            double sb = 0, sb_off = 0;
            for (double d = 15.0; d <= 120.0; d += 1.0) { for (double sgn : { -1.0, 1.0 }) { const double m = goertzel(w, n, fk + sgn * d, rate); if (m > sb) { sb = m; sb_off = sgn * d; } } }
            const double rel = 20.0 * std::log10((sb + 1e-12) / (hk + 1e-12));
            if (rel > worst) { worst = rel; worst_k = k; worst_off = sb_off; }
            char t[64]; std::snprintf(t, sizeof t, " h%d %+.1f dB @%+.0f Hz", k, rel, sb_off); line += t;
        }
        std::printf("note %d breath %3d: pitch %+6.1f cents, peak %.3f, sidebands worst %+.1f dB (h%d, %+.0f Hz):%s\n", note, breath, 1200.0 * std::log2(f0 / f_nom), pk, worst, worst_k, worst_off, line.c_str());
        voxo_destroy(v);
    }
    return 0;
}
