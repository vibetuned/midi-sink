// bore_resp.cpp — the bare bore's response at the mouth: a pulse p_src at node 0, u[0] recorded 0.4 s, the spectrum's
// peaks (Goertzel 150..4000 Hz, 4 Hz steps) with their magnitudes and −3 dB widths. (a) the plain open–open bore cut for
// a note (λ derived, the radiation port, the leak); (b) the D4 bore (128 cells, λ 0.79) with holes 1..j open at the
// calibrated positions (corr from the engine's log), the vent closed, 1/L = h·S, R 0.3 through a one-pole at 3 kHz.
#include "suzu.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
static double goertzel(const float* s, size_t n, double hz, double rate) {
    const double w = 2.0 * M_PI * hz / rate, cw = 2.0 * std::cos(w); double s0 = 0, s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; i++) { const double win = 0.5 - 0.5 * std::cos(2.0 * M_PI * (double)i / (double)(n - 1)); s0 = (double)s[i] * win + cw * s1 - s2; s2 = s1; s1 = s0; }
    const double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w); return 4.0 * std::sqrt(re * re + im * im) / (double)n;
}
static void peaks(const std::vector<float>& rec, double rate) {
    std::vector<double> m; std::vector<double> fs;
    for (double f = 150; f <= 4000; f += 4) { fs.push_back(f); m.push_back(goertzel(rec.data(), rec.size(), f, rate)); }
    for (size_t i = 1; i + 1 < m.size(); i++) if (m[i] > m[i - 1] && m[i] >= m[i + 1] && m[i] > 1e-6) {
        size_t lo = i, hi = i; while (lo > 0 && m[lo] > m[i] * 0.707) lo--; while (hi + 1 < m.size() && m[hi] > m[i] * 0.707) hi++;
        std::printf("    %6.0f Hz  %8.3g  Q~%4.0f\n", fs[i], m[i], fs[i] / (fs[hi] - fs[lo] + 1e-9));
    }
}
int main(int argc, char** argv) {
    const float rate2 = 96000.0f; const int note = argc > 1 ? std::atoi(argv[1]) : 72; const float h = argc > 2 ? (float)std::atof(argv[2]) : 0.125f; const float R = argc > 3 ? (float)std::atof(argv[3]) : 0.3f;
    const float corr[12] = { 0, 62, 72, 84, 80, 90, 92, 102, 120, 108, 124, 248 };
    const float f = 440.0f * exp2f((float)(note - 69) / 12.0f), f_base = 293.665f;
    const int T = (int)(0.4f * rate2); std::vector<float> rec(T);
    auto run = [&](suzu::Bore* b, const char* what) {
        b->reset();
        double inj = 0.0, e100 = 0, e5 = 0, e10 = 0, e20 = 0;
        for (int i = 0; i < T; i++) { const float pulse = i < 16 ? 0.02f * (1.0f - cosf(2.0f * suzu::PI * (float)i / 16.0f)) : 0.0f; rec[i] = b->step(0.0f, pulse, 1.0f); if (i < 16) inj += (double)rec[i] * pulse;
            if (i == 100) e100 = b->energy() + b->holes_energy(); if (i == 4800) e5 = b->energy() + b->holes_energy(); if (i == 9600) e10 = b->energy() + b->holes_energy(); if (i == 19200) e20 = b->energy() + b->holes_energy(); }
        std::printf("%s: cells %d lam %.3f  injected %.3g  E(100 steps) %.3g  E(50ms) %.3g  E(100ms) %.3g  E(200ms) %.3g\n", what, b->n, b->lam, inj, e100, e5, e10, e20);
        for (int k = 1; k <= 3; k++) { double best = -1, bf = 0; for (double c = -150; c <= 150; c += 2) { const double ff = k * f * std::pow(2.0, c / 1200.0); const double m = goertzel(rec.data(), rec.size(), ff, rate2); if (m > best) { best = m; bf = ff; } } std::printf("    h%d near %6.0f Hz: %8.3g\n", k, bf, best); }
        peaks(rec, rate2);
    };
    // (a) plain
    { suzu::Bore* b = new suzu::Bore; b->setup(64, suzu::BORE_CYLINDER, 0, 0, suzu::END_OPEN, 0.3f, suzu::END_OPEN, 0.3f); b->radiation_corner(1500.0f, rate2);
      const float lam_max = suzu::Bore::lambda_bound(b->mu_max()) * 0.999f;
      const float ends = 2.0f * (suzu::Bore::end_correction(0.3f, b->rad_a, f, rate2) + suzu::Bore::leak_correction(0.3f, 40.0f, f, rate2));
      const int cells = suzu::Bore::cells_for(f, rate2, lam_max, false, 0, ends, 128);
      b->setup(cells, suzu::BORE_CYLINDER, 0, 0, suzu::END_OPEN, 0.3f, suzu::END_OPEN, 0.3f); b->retune(cells, suzu::Bore::lambda_for(f, rate2, cells, false, 0, ends), suzu::BORE_CYLINDER, 0, 0); b->radiation_corner(1500.0f, rate2); b->leak(0.3f, 40.0f, rate2);
      run(b, "plain"); delete b; }
    // (b) holes
    { suzu::Bore* b = new suzu::Bore; b->setup(64, suzu::BORE_CYLINDER, 0, 0, suzu::END_OPEN, 0.3f, suzu::END_OPEN, 0.3f); b->radiation_corner(1500.0f, rate2);
      const float lam_max = suzu::Bore::lambda_bound(b->mu_max()) * 0.999f * suzu::Bore::holes_room(h);
      const float ends = 2.0f * (suzu::Bore::end_correction(0.3f, b->rad_a, f_base, rate2) + suzu::Bore::leak_correction(0.3f, 40.0f, f_base, rate2));
      const int n = suzu::Bore::cells_for(f_base, rate2, lam_max, false, 0, ends, 128);
      b->setup(n, suzu::BORE_CYLINDER, 0, 0, suzu::END_OPEN, 0.3f, suzu::END_OPEN, 0.3f); b->retune(n, suzu::Bore::lambda_for(f_base, rate2, n, false, 0, ends), suzu::BORE_CYLINDER, 0, 0); b->radiation_corner(1500.0f, rate2); b->leak(0.3f, 40.0f, rate2);
      int at[12]; float L[12], Rr[12];
      for (int k = 0; k < 11; k++) { const float semis = (float)(k + 1) + corr[k + 1] * 0.01f; int c = (int)((float)n * exp2f(-semis / 12.0f) + 0.5f); if (c < 1) c = 1; if (c > n - 1) c = n - 1; at[k] = c; L[k] = 1.0f / (h * b->sp[c]); Rr[k] = R; }
      at[11] = n / 3; L[11] = 4.0f / (h * b->sp[n / 3]); Rr[11] = R;
      b->holes_set(12, at, L, Rr, 3000.0f, rate2);
      const int j = note - 62; uint32_t bits = 0; for (int k = 0; k < j; k++) bits |= 1u << k; b->holes_open(bits);
      std::printf("holes open 1..%d at cells:", j); for (int k = 0; k < j; k++) std::printf(" %d", at[k]); std::printf("\n");
      run(b, "holes"); delete b; }
    return 0;
}
