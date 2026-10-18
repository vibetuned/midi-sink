// gong_bar.cpp — step 79: where does the gong's grid hold? The 16 × 13 sheet at the (1,1) shape at the cap's stretching
// (E = E_max) with the top mode seeded at 1e-4, for swell 0.3 / 1 / 2 and the CFL number at E_max from 1.95 down,
// stepped 200 000 times (two seconds at 96 k): the plain energy's growth, or non-finite.
#include "suzu.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <algorithm>
int main() {
    using M = suzu::Membrane<suzu::MEMBRANE_MAX>;
    for (float share : { 0.0f, 0.005f, 0.01f }) for (float swell : { 0.3f, 1.0f, 2.0f }) {
        for (float lam : { 1.95f, 1.9f, 1.85f, 1.8f, 1.75f, 1.7f, 1.6f, 1.5f, 1.4f, 1.2f }) {
            for (float over : { 1.0f }) {
                M m; m.reset(16, 13);
                const float factor = M::cfl_factor(16, 13, share, swell);
                m.tune(lam / factor, share);
                const float emax = 1.0f; m.alpha = swell * m.c2 / emax;
                double phi2 = 0.0; for (int j = 1; j <= 13; j++) for (int i = 1; i <= 16; i++) { const double f = std::sin(M_PI * i / 17.0) * std::sin(M_PI * j / 14.0); phi2 += f * f; }
                const double a = std::sqrt(over * emax / (m.lambda11() * phi2));
                for (int j = 1; j <= 13; j++) for (int i = 1; i <= 16; i++) {
                    const double f = std::sin(M_PI * i / 17.0) * std::sin(M_PI * j / 14.0), top = std::sin(16.0 * M_PI * i / 17.0) * std::sin(13.0 * M_PI * j / 14.0);
                    m.u[j * M::S + i] = (float)(a * f + 1e-4 * a * top);
                }
                const double h0 = m.energy_plain(); double hmax = h0; long died = -1;
                for (long i = 0; i < 200000; i++) { m.step(); if (!std::isfinite(m.E)) { died = i; break; } if ((i & 1023) == 0) { const double h = m.energy_plain(); if (h > hmax) hmax = h; if (h > 1e6 * h0) { died = i; break; } } }
                const float eps11 = std::sqrt(m.c2 * m.lambda11());
                std::printf("share %.3f swell %.1f  λ_2D(E_max) %.2f  ε11 %.3f (ω_mod %.3f rad/step; π − θ_top %.2f): %s (max %.2f dB)\n", share, swell, lam, eps11, 2 * eps11, M_PI - 2 * std::asin(0.5 * lam),
                            died >= 0 ? "DIVERGED" : "held 200 000 steps", 10 * std::log10(hmax / h0));
            }
        }
    }
    // the engine's own regime: the gong patch's numbers at the notes the grid shrinks for — the grid the engine would cut,
    // the note's ε, swell 0.8, cap 1.5 full strikes, struck for real (a bump of momentum) and stepped 2 s
    std::printf("the engine's regime (a bump strike at the cap, swell 0.8):\n");
    for (int note = 60; note <= 108; note += 6) {
        const float f0 = 440.0f * std::pow(2.0f, (note - 69) / 12.0f), eps0 = suzu::eps_for(f0, 96000.0f), swell = 0.8f, share = 0.0f;
        int nx = 2, ny = 2; float factor = 1;
        for (int n = 16; n >= 2; n--) { nx = n; ny = (n * 13 + 8) / 16; if (ny < 2) ny = 2; factor = M::cfl_factor(nx, ny, share, swell); if (eps0 * factor <= 1.95f) break; }
        float eps = eps0; if (eps * factor > 1.95f) eps = 1.95f / factor;
        M m; m.reset(nx, ny); m.tune(eps, share);
        const float r = std::fmax(1.5f, 0.1f * std::min(nx, ny) + 1.0f);
        const float w2 = m.strike_weight2(0.42f, 0.42f, r), a_full = 0.3f * eps * 24.0f;
        const double hcap = 1.5 * 0.5 * a_full * a_full * w2; const float emax = (float)(2.0 * hcap / (m.c2 + m.k2 * m.lambda11()));
        m.alpha = swell * m.c2 / emax;
        m.strike(a_full, 0.42f, 0.42f, r); m.cap(hcap);
        const double h0 = m.energy_plain(); double hmax = h0; long died = -1;
        for (long i = 0; i < 192000; i++) { m.step(); if (!std::isfinite(m.E)) { died = i; break; } if ((i & 1023) == 0) { const double h = m.energy_plain(); if (h > hmax) hmax = h; if (h > 1e6 * h0) { died = i; break; } } }
        std::printf("  note %3d (%7.1f Hz) grid %2dx%2d λ_2D(E_max) %.3f: %s (max %+.2f dB)\n", note, f0, nx, ny, suzu::membrane_cfl(m.c2, m.k2, m.alpha, emax), died >= 0 ? "DIVERGED" : "held 2 s", 10 * std::log10(hmax / h0));
    }
    return 0;
}
