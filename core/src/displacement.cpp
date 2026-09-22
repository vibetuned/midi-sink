// displacement.cpp — deformation queue -> shader pass dispatch descriptions
// (PROJECT_SPEC.md §6). Fixed-capacity array, no allocation after create.
#include "displacement.h"

#include <math.h>
#include <stdlib.h>

struct sumi_deform_queue_t {
    sumi_deform_t* items;
    uint32_t       capacity;
    uint32_t       count;
};

extern "C" {

// Crossed-tine pinch (v0.4 variant, DECISIONS_3 #34): the step-19 prototype
// verbatim — one tine along the fold axis, one along the perpendicular,
// through (x, y). Tines are infinite lines; the endpoints only fix direction.
void sumi_deform_crossed_pinch(float x, float y, float dir_x, float dir_y,
                               float k, sumi_deform_t out[2]) {
    const float len = sqrtf(dir_x * dir_x + dir_y * dir_y);
    float ca = 1.0f, sa = 0.0f;
    if (len > 1e-9f) { ca = dir_x / len; sa = dir_y / len; }
    if (k < 0.0f) { ca = -ca; sa = -sa; }        // sign reverses both drags
    const float mag = (k >= 0.0f ? k : -k) * 0.2f;
    const float h = 0.1f;
    out[0].type = SUMI_DEFORM_TINE;
    out[0].as.tine.x0 = x - ca * h;  out[0].as.tine.y0 = y - sa * h;
    out[0].as.tine.x1 = x + ca * h;  out[0].as.tine.y1 = y + sa * h;
    out[0].as.tine.alpha = 0.03f;
    out[0].as.tine.magnitude = mag;
    out[1].type = SUMI_DEFORM_TINE;
    out[1].as.tine.x0 = x + sa * h;  out[1].as.tine.y0 = y - ca * h;
    out[1].as.tine.x1 = x - sa * h;  out[1].as.tine.y1 = y + ca * h;
    out[1].as.tine.alpha = 0.03f;
    out[1].as.tine.magnitude = mag;
}

/* ------------------------------------------------------------------ */
/* v0.12 (Phase 6 step 38): the viscous multipole burst's mathematics  */
/* ------------------------------------------------------------------ */
// Method after Jaffer (arXiv:1810.04646): the m-th viscous multipole's stream
// function ψ_m ∝ γ_m(s) sin(mθ)/r^m, s = r²/4ντ (the Stokes-approximation
// multipole of the vorticity diffusion equation), integrated in time:
// ∫ψ dτ ∝ r^{2−m} Φ_m(S). For m = 1 the remainder is E1 (the Stokeslet's
// kernel, DECISIONS_4 #53); for m ≥ 2 it is an exponential polynomial and Φ_m
// is elementary. Near the core Φ_m sits at the plateau 1/(m−1): the small-S
// form is the DEFICIT series, so differences never subtract two plateaus.

static double burst_fact(uint32_t n) { double f = 1.0; for (uint32_t k = 2; k <= n; k++) f *= (double)k; return f; }

// 1/(m−1) − Φ_m(S) = (1/(m−1)!) Σ_n (−1)^n S^{m+n−1} / (n! (m+n)(m+n−1)),  S < 1
static double burst_deficit(uint32_t m, double S) {
    double term = pow(S, (double)(m - 1)), acc = 0.0;
    for (uint32_t n = 0; n < 18; n++) {
        acc += term / ((double)(m + n) * (double)(m + n - 1));
        term *= -S / (double)(n + 1);
    }
    return acc / burst_fact(m - 1);
}
static double burst_phi_closed(uint32_t m, double S) {       // S ≥ 1
    const double e = exp(-S);
    double term = 1.0, sum_m = 0.0, sum_m1 = 0.0;
    for (uint32_t k = 0; k < m; k++) {
        sum_m += term;
        if (k + 2 <= m) sum_m1 += term;
        term *= S / (double)(k + 1);
    }
    return (1.0 - e * sum_m) / S + e * sum_m1 / (double)(m - 1);
}
double sumi_burst_phi(uint32_t m, double S) {
    if (S <= 0.0) return 1.0 / (double)(m - 1);
    return S < 1.0 ? 1.0 / (double)(m - 1) - burst_deficit(m, S) : burst_phi_closed(m, S);
}
double sumi_burst_dphi(uint32_t m, double S0, double S1) {
    if (S0 < 1.0) return burst_deficit(m, S0) - burst_deficit(m, S1);
    return sumi_burst_phi(m, S1) - burst_phi_closed(m, S0);
}
double sumi_burst_gs(uint32_t m, double S) {                 // γ_m(S)/S
    if (S <= 0.0) return 0.0;
    const double e = exp(-S);
    if (S < 1.0) {                                           // e^{−S} Σ_{k≥m} S^{k−1}/k!
        double term = pow(S, (double)(m - 1)) / burst_fact(m), acc = 0.0;
        for (uint32_t k = 0; k < 20; k++) { acc += term; term *= S / (double)(m + k + 1); }
        return e * acc;
    }
    double term = 1.0, s = 0.0;
    for (uint32_t k = 0; k < m; k++) { s += term; term *= S / (double)(k + 1); }
    return (1.0 - e * s) / S;
}
// The radial displacement on the ejection axis: d_r(r) = (m A/r)(a/r)^{m−2} ΔΦ.
static double burst_axis_dr(uint32_t m, double amp, double a, double l0, double l1, double r) {
    const double r2 = r * r;
    return (double)m * amp / r * pow(a / r, (double)(m - 2)) * sumi_burst_dphi(m, r2 / (l0 * l0), r2 / (l1 * l1));
}
double sumi_burst_amp(uint32_t m, double D, double a, double l_end) {
    if (m < SUMI_BURST_M_MIN) m = SUMI_BURST_M_MIN;
    if (m > SUMI_BURST_M_MAX) m = SUMI_BURST_M_MAX;
    const double dphi = sumi_burst_dphi(m, 1.0, (a * a) / (l_end * l_end));
    return dphi > 0.0 ? D * a / ((double)m * dphi) : 0.0;
}
double sumi_burst_peak(uint32_t m, double amp, double a, double l0, double l1) {
    // |d_r| along the axis is unimodal (∝ r^{m−1} at the core, ∝ r^{−m−1}
    // beyond l1): a log-spaced scan, then a golden-section refinement.
    const double r_lo = 0.25 * l0, r_hi = 4.0 * l1;
    const int N = 28;
    double best = 0.0; int bi = 0;
    for (int i = 0; i < N; i++) {
        const double r = r_lo * pow(r_hi / r_lo, (double)i / (double)(N - 1));
        const double v = fabs(burst_axis_dr(m, amp, a, l0, l1, r));
        if (v > best) { best = v; bi = i; }
    }
    double lo = r_lo * pow(r_hi / r_lo, (double)(bi > 0 ? bi - 1 : 0) / (double)(N - 1));
    double hi = r_lo * pow(r_hi / r_lo, (double)(bi + 1 < N ? bi + 1 : N - 1) / (double)(N - 1));
    const double g = 0.6180339887498949;
    double x1 = hi - g * (hi - lo), x2 = lo + g * (hi - lo);
    double f1 = fabs(burst_axis_dr(m, amp, a, l0, l1, x1)), f2 = fabs(burst_axis_dr(m, amp, a, l0, l1, x2));
    for (int it = 0; it < 14; it++) {
        if (f1 < f2) { lo = x1; x1 = x2; f1 = f2; x2 = lo + g * (hi - lo); f2 = fabs(burst_axis_dr(m, amp, a, l0, l1, x2)); }
        else         { hi = x2; x2 = x1; f2 = f1; x1 = hi - g * (hi - lo); f1 = fabs(burst_axis_dr(m, amp, a, l0, l1, x1)); }
    }
    const double refined = f1 > f2 ? f1 : f2;
    return refined > best ? refined : best;
}
double sumi_burst_budget(uint32_t m) {
    // tools/multipole_verify.py §8: max|∂d| per (d_max/l0) over ages a..12a,
    // per order — 1.83, 2.29, 2.39, 3.04, 3.15, 3.58, 3.27 — so |∇d| ≤ 0.25
    // needs d_max ≤ 0.137, 0.109, 0.105, 0.082, 0.079, 0.070, 0.076 · l0.
    static const double beta[7] = {0.13, 0.10, 0.10, 0.08, 0.075, 0.068, 0.072};
    if (m < SUMI_BURST_M_MIN) m = SUMI_BURST_M_MIN;
    if (m > SUMI_BURST_M_MAX) m = SUMI_BURST_M_MAX;
    return beta[m - SUMI_BURST_M_MIN];
}
double sumi_burst_step(uint32_t m, double amp, double a, double l0, double l1) {
    const double cap = sumi_burst_budget(m) * l0;
    if (sumi_burst_peak(m, amp, a, l0, l1) <= cap) return l1;
    // Bisection in l² (the time variable): the increment's peak grows
    // monotonically with l' (Φ_m falls with S, so ΔΦ grows at every r).
    double lo2 = l0 * l0, hi2 = l1 * l1;
    for (int it = 0; it < 18; it++) {
        const double mid2 = 0.5 * (lo2 + hi2), mid = sqrt(mid2);
        if (sumi_burst_peak(m, amp, a, l0, mid) <= cap) lo2 = mid2; else hi2 = mid2;
    }
    return sqrt(lo2);
}

/* ------------------------------------------------------------------ */
/* v0.13 (Phase 6 step 39): the spark shear's kick-drift step            */
/* ------------------------------------------------------------------ */
uint32_t sumi_spark_emit_step(sumi_deform_queue_t* q, float x, float y, float A, float B,
                              float k, float phase, float theta0, float band,
                              uint32_t stack, uint32_t profile) {
    if (!q || !(k > 0.0f)) return 0;
    if (stack < 1u) stack = 1u;
    if (stack > 4u) stack = 4u;
    uint32_t n = 0;
    for (uint32_t stage = 0; stage < 2; stage++) {
        const float amp = stage == 0 ? A : B;
        if (amp == 0.0f || !(amp == amp)) continue;
        sumi_deform_t d;
        d.type = SUMI_DEFORM_SPARK;
        d.as.spark.x = x;
        d.as.spark.y = y;
        d.as.spark.amp = amp;
        d.as.spark.k = k;
        d.as.spark.phase = phase;
        d.as.spark.theta0 = theta0;
        d.as.spark.band = band > 0.0f ? band : 0.0f;
        d.as.spark.stack = stack;
        d.as.spark.profile = profile ? 1u : 0u;
        d.as.spark.stage = stage;
        if (!sumi_deform_queue_push(q, &d)) break;
        n++;
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* v0.14 (Phase 6 step 40): the Chirikov standard map's kick-drift step  */
/* ------------------------------------------------------------------ */
uint32_t sumi_chirikov_emit_step(sumi_deform_queue_t* q, float x, float y, float A, float k,
                                 float phase, float eps, bool inverse) {
    if (!q || !(k > 0.0f) || !(A == A) || !(eps == eps)) return 0;
    uint32_t n = 0;
    for (int i = 0; i < 2; i++) {
        const uint32_t stage = inverse ? (uint32_t)(1 - i) : (uint32_t)i;   // the inverse undoes the drift first
        sumi_deform_t d;
        d.type = SUMI_DEFORM_CHIRIKOV;
        d.as.chirikov.x = x;
        d.as.chirikov.y = y;
        d.as.chirikov.amp = inverse ? -A : A;
        d.as.chirikov.k = k;
        d.as.chirikov.phase = phase;
        d.as.chirikov.eps = inverse ? -eps : eps;
        d.as.chirikov.stage = stage;
        if (!sumi_deform_queue_push(q, &d)) break;
        n++;
    }
    return n;
}

sumi_deform_queue_t* sumi_deform_queue_create(uint32_t capacity) {
    if (capacity == 0) return nullptr;
    sumi_deform_queue_t* q = (sumi_deform_queue_t*)calloc(1, sizeof(sumi_deform_queue_t));
    if (!q) return nullptr;
    q->items = (sumi_deform_t*)calloc(capacity, sizeof(sumi_deform_t));
    if (!q->items) {
        free(q);
        return nullptr;
    }
    q->capacity = capacity;
    return q;
}

void sumi_deform_queue_destroy(sumi_deform_queue_t* q) {
    if (!q) return;
    free(q->items);
    free(q);
}

bool sumi_deform_queue_push(sumi_deform_queue_t* q, const sumi_deform_t* deform) {
    if (!q || !deform || q->count >= q->capacity) return false;
    q->items[q->count++] = *deform;
    return true;
}

uint32_t sumi_deform_queue_count(const sumi_deform_queue_t* q) {
    return q ? q->count : 0;
}

const sumi_deform_t* sumi_deform_queue_at(const sumi_deform_queue_t* q, uint32_t index) {
    if (!q || index >= q->count) return nullptr;
    return &q->items[index];
}

void sumi_deform_queue_clear(sumi_deform_queue_t* q) {
    if (q) q->count = 0;
}

} // extern "C"
