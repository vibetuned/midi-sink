// bore_hole_stab.cpp — the hole scheme's stability on a bare cylinder: 128 cells open–open at λ = 0.99·bound, one hole at
// cell 64 with 1/L_h = h·S, no loss (R 0): the energy after 0.5 s against the pulse's — growth = unstable.
#include "suzu.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
int main() {
    for (float lamf : { 0.99f, 0.97f, 0.95f, 0.90f }) for (float h : { 0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f }) {
        suzu::Bore* b = new suzu::Bore;
        b->setup(128, suzu::BORE_CYLINDER, 0.0f, 0.0f, suzu::END_OPEN, 0.0f, suzu::END_OPEN, 0.0f);
        const float lam = suzu::Bore::lambda_bound(b->mu_max()) * lamf; b->retune(128, lam, suzu::BORE_CYLINDER, 0.0f, 0.0f);
        int at[1] = { 64 }; float L[1] = { 1.0f / (h * b->sp[64]) }, R[1] = { 0.0f }; b->holes_set(1, at, L, R); b->holes_open(1u);
        double e0 = 0.0, e1 = 0.0; bool fin = true;
        for (long i = 0; i < 48000; i++) { const float pulse = i < 8 ? 0.02f * (1.0f - cosf(2.0f * suzu::PI * (float)i / 8.0f)) : 0.0f; b->step(0.0f, pulse, 1.0f); if (i == 200) e0 = b->energy() + b->holes_energy(); }
        e1 = b->energy() + b->holes_energy(); fin = std::isfinite(e1);
        std::printf("lam %.3f (%.2f of bound) h %.4f: E(200)=%.3g E(48000)=%s%.3g  %s\n", lam, lamf, h, e0, fin ? "" : "NON-FINITE ", e1, fin && e1 < 10 * e0 ? "stable" : "GROWS");
        delete b;
    }
    return 0;
}
