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
    out[0].as.tine.profile = 0u;   // 1.7.0: the pinch's crossed tines keep Jaffer's profile (the cleavage profiles are the stroke's)
    out[1].type = SUMI_DEFORM_TINE;
    out[1].as.tine.x0 = x + sa * h;  out[1].as.tine.y0 = y - ca * h;
    out[1].as.tine.x1 = x - sa * h;  out[1].as.tine.y1 = y + ca * h;
    out[1].as.tine.alpha = 0.03f;
    out[1].as.tine.magnitude = mag;
    out[1].as.tine.profile = 0u;
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

// ---- 1.8.0 (Phase 11 step 72, KESSHO §2.2): the Arnol'd tongue lock ----------------------------------------
// THE MAP. In the action-angle frame about the drop (ρ = r²/2, θ) one iteration is M = T ∘ Kick:
//   the kick, from F₂(θ, ρ₁) = θρ₁ + (K/q)·W(ρ₁)·cos q(θ − φ):
//       ρ₁ = ρ + K·W(ρ₁)·sin q(θ − φ),   θ₁ = θ + (K/q)·W′(ρ₁)·cos q(θ − φ)      (det 1: a canonical transformation)
//   the twist, a rotation by Θ(r) depending on r alone (det 1 for any profile): the Lamb–Oseen angle with r_c = R,
//       scaled so Θ(R) = 2πΩ₀ — Ω₀ turns per iteration at the ring, the slope τ = dΩ/dρ·ρ ≈ −0.42·Ω₀ there — and
//       C²-windowed to zero between 1.75 R and 2.5 R so the rest of the sheet is left alone (a windowed rotation is
//       still a rotation by a function of r: exact).
// The iteration's lookup is M⁻¹: undo the twist (r is preserved, so ρ₁ is the texel's own), then the kick's inverse —
// θ from one scalar Newton (θ₁ known, ρ₁ known; FIVE iterations), ρ explicit. |K·W′| < 1 keeps the kick a bijection
// and the Newton monotone (g′ = 1 − K W′ sin ≥ 0.2 at K_max = 0.8/max|W′|). The spec's K_max = 0.8·q/max W′ carries a q the
// derivative does not have (DECISIONS_10 #9). The window W is 1 on [ρ_drop/2, ρ_drop] with C² quintic ramps to 0 at
// the centre and at 1.5·ρ_drop: W(0) = 0 is what keeps ρ = ρ₁ − K W sin ≥ 0 (the spec's W = 1 at the centre sends
// material to negative ρ). max|W′| = 1.875/(ρ_drop/2) = 3.75/ρ_drop.
static double arnold_s(double u) { return u * u * u * (10.0 + u * (-15.0 + 6.0 * u)); }      // the C² quintic step
static double arnold_ds(double u) { return 30.0 * u * u * (1.0 - u) * (1.0 - u); }
double sumi_arnold_window(double rho, double rho_drop, double* dW) {
    const double rin = 0.5 * rho_drop, half = 0.5 * rho_drop;
    double W = 0.0, d = 0.0;
    if (rho <= 0.0) { W = 0.0; d = 0.0; }
    else if (rho < rin) { const double u = rho / rin; W = arnold_s(u); d = arnold_ds(u) / rin; }
    else if (rho <= rho_drop) { W = 1.0; d = 0.0; }
    else if (rho < 1.5 * rho_drop) { const double v = (rho - rho_drop) / half; W = 1.0 - arnold_s(v); d = -arnold_ds(v) / half; }
    if (dW) *dW = d;
    return W;
}
float sumi_arnold_kmax(float rho_drop) {
    return rho_drop > 0.0f ? 0.8f * rho_drop / 3.75f : 0.0f;
}
float sumi_arnold_tongue(float omega, uint32_t* p_out, uint32_t* q_out) {
    // THE TONGUE (KESSHO §2.2, DECISIONS_10 #9): the nearest continued-fraction convergent p/q of Ω₀ with 2 ≤ q ≤ 6
    // inside its tongue |Ω₀ − p/q| < 1/(2q²); outside every tongue q = 6 with p the nearest. The trivial convergents
    // 0/1 and 1/1 are not tongues (they would swallow every interval): q = 1 is the integer ratio alone — within the
    // q = 6 tongue's half-width of an integer — the unison and the octave, which take a full turn plus the beat.
    if (!(omega == omega)) omega = 0.0f;
    omega -= floorf(omega);
    const float near_int = omega < 0.5f ? omega : 1.0f - omega;
    if (near_int < 0.5f / 36.0f) {
        if (p_out) *p_out = 1u; if (q_out) *q_out = 1u;
        return omega < 0.5f ? omega : omega - 1.0f;   // the signed detune about the integer
    }
    uint32_t bp = 0, bq = 0; float best = 1e9f;
    double x = (double)omega; long p0 = 0, q0 = 1, p1 = 1, q1 = 0;   // the convergents' recurrence
    for (int i = 0; i < 12; i++) {
        const double a = floor(x);
        const long p2 = (long)a * p1 + p0, q2 = (long)a * q1 + q0;
        if (q2 > (long)SUMI_ARNOLD_Q_MAX) break;
        if (q2 >= 2) {
            const float d = fabsf(omega - (float)p2 / (float)q2);
            if (d < 0.5f / ((float)q2 * (float)q2) && d < best) { best = d; bp = (uint32_t)p2; bq = (uint32_t)q2; }
        }
        const double frac = x - a;
        if (frac < 1e-9) break;
        x = 1.0 / frac;
        p0 = p1; q0 = q1; p1 = p2; q1 = q2;
    }
    if (bq == 0) { bq = SUMI_ARNOLD_Q_MAX; bp = (uint32_t)floorf(omega * (float)bq + 0.5f); }
    if (p_out) *p_out = bp;
    if (q_out) *q_out = bq;
    return omega - (float)bp / (float)bq;
}
// the twist angle at radius r (canvas heights) for a drop of radius R and Ω₀ turns at the ring, windowed
static double arnold_twist(double r, double R, double omega) {
    if (omega == 0.0 || R <= 0.0) return 0.0;
    const double ra = 1.75 * R, rb = 2.5 * R;
    if (r >= rb) return 0.0;
    const double x = (r * r) / (R * R);
    double f;
    if (x < 1e-3) f = (1.0 - 0.5 * x);                     // the series: the analytic limit at the centre
    else f = (1.0 - exp(-x)) / x;
    double w = 1.0;
    if (r > ra) w = 1.0 - arnold_s((r - ra) / (rb - ra));
    return 2.0 * 3.141592653589793 * omega * f / (1.0 - exp(-1.0)) * w;   // Θ(R) = 2πΩ₀
}
double sumi_arnold_eval(const sumi_deform_arnold_t* d, double px, double py, double aspect, bool forward,
                        double* sx, double* sy) {
    const double rho_d = d->rho_drop, K = d->kick, q = (double)(d->q < 1u ? 1u : d->q), phi = d->phase;
    const double R = sqrt(2.0 * rho_d);
    const double X = px * aspect, Y = py, CX = d->x * aspect, CY = d->y;
    const double rx = X - CX, ry = Y - CY, r = sqrt(rx * rx + ry * ry);
    double residual = 0.0;
    if (r >= 2.5 * R) { *sx = px; *sy = py; return 0.0; }   // beyond the twist's window: the identity, exactly
    if (!forward) {
        // the lookup M⁻¹: undo the twist, then the kick's inverse (Newton on θ)
        const double rho1 = 0.5 * r * r;
        double th1 = atan2(ry, rx) - arnold_twist(r, R, d->omega) - phi;
        double rho = rho1, th = th1;
        if (rho1 < 1.5 * rho_d) {
            double dW; const double W = sumi_arnold_window(rho1, rho_d, &dW);
            for (int i = 0; i < 5; i++) {   // five: at the 0.8 bound three leave 1e-3 (the basin's ratio c²/(2(1 − c)) = 1.6), five 1e-11 (DECISIONS_10 #9)
                const double g = th - th1 + (K / q) * dW * cos(q * th), gp = 1.0 - K * dW * sin(q * th);
                th -= g / gp;
            }
            residual = fabs(th - th1 + (K / q) * dW * cos(q * th));
            rho = rho1 - K * W * sin(q * th);
            if (rho < 0.0) rho = 0.0;
        }
        const double rs = sqrt(2.0 * rho), ts = th + phi;
        *sx = (CX + rs * cos(ts)) / aspect; *sy = CY + rs * sin(ts);
        return residual;
    }
    // the lookup M: the kick forward (Newton on ρ₁), then the twist at the new radius
    const double rho = 0.5 * r * r, th = atan2(ry, rx) - phi;
    double rho1 = rho, th1 = th;
    if (rho < 1.5 * rho_d) {
        const double sq = sin(q * th), cq = cos(q * th);
        double dW = 0.0, W = 0.0;
        for (int i = 0; i < 5; i++) {
            W = sumi_arnold_window(rho1, rho_d, &dW);
            const double h = rho1 - rho - K * W * sq, hp = 1.0 - K * dW * sq;
            rho1 -= h / hp;
        }
        if (rho1 < 0.0) rho1 = 0.0;
        W = sumi_arnold_window(rho1, rho_d, &dW);
        residual = fabs(rho1 - rho - K * W * sq);
        th1 = th + (K / q) * dW * cq;
    }
    const double r1 = sqrt(2.0 * rho1), t1 = th1 + phi + arnold_twist(r1, R, d->omega);
    *sx = (CX + r1 * cos(t1)) / aspect; *sy = CY + r1 * sin(t1);
    return residual;
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
