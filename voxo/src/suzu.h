// suzu.h — Suzu, the symplectic phase-space synth inside Voxo (Phase 8 step
// 56, SYNTH §1–§2.4, §4). A SOURCE beside the sampler: the same callback, the
// same normalizer-fed voice model, the same bus. Every element here declares
// its class per SYNTH §1's DSP table — the audio twin of the canvas's operator
// classes:
//
//   element                          class                    where
//   ------------------------------   ----------------------   ---------------------------
//   Cell::step (the leapfrog)        SYMPLECTIC (det = 1)     the magic circle, §2.1
//   Cell::retune (amplitude-kept)    SYMPLECTIC (a rescale    the Gordon–Smith recurrence
//                                    to the same orbit)       re-based on the invariant
//   Cell::step_naive                 det = 1 + ε² > 1 —       the NEGATIVE CONTROL of the
//                                    BANNED, test only        drift test (§5)
//   Cell::contract                   DISSIPATIVE (conformal,  the release: a declared
//                                    declared factor)         per-sample factor
//   Cell::kick                       DRIVE (injection)        the strike sets the orbit
//   shear_cubic / shear_tri          SYMPLECTIC (x += g(y))   waveshaping, §2.4
//   Svf::step (Chamberlin)           the (low, band) core is  §2.2: q is the declared
//                                    the magic circle; q > 0  dissipation, the input the
//                                    DISSIPATIVE, declared    drive
//   lattice_kick (the coupling)      SYMPLECTIC (a gradient   §2.5: the shared-potential
//                                    kick from pre-update     kick along the chain, det = 1
//                                    positions)               in the joint space
//   lattice_lambda_max (load gate)   — the CFL analog —       §2.5: λ_max of the stiffness +
//                                                             coupling matrix < 4, power
//                                                             iteration at patch load
//   bow_factor (the servo)           SELF-EXCITED, bounded    §2.6: signed damping
//                                    by construction          μ·(E_target − E), a limit cycle
//   bow seed                         DRIVE (declared)         §2.6: the bow's first grip on a
//                                                             resting string
//
// Nothing here allocates, locks or logs: every struct is POD and lives inside
// a Voice (voxo.cpp); the callback contract (voxo.h) holds. The nonlinear
// stages and the filter run inside a 2× oversampled section (§2.2, §2.4):
// the voice steps its cells twice per output sample and averages the pair.
#pragma once

#include <stdint.h>
#include <math.h>

namespace suzu {

constexpr float PI = 3.14159265358979f;

// Exact tuning (§2.1): the leapfrog rotates by θ = 2π f / fs per step when
// ε = 2 sin(θ / 2) — cents-true across the keyboard, not the small-angle ε ≈ θ.
inline float eps_for(float hz, float rate) {
    float x = PI * hz / rate;
    if (x > 1.5f) x = 1.5f;   // past Nyquist the recurrence is meaningless; keep it bounded
    return 2.0f * sinf(x);
}

// THE CELL: (x, y) on the orbit x² + y² − ε·x·y = E, the invariant of the
// staggered update below (x first, then y with the UPDATED x — the
// evaluated-at-the-displaced-value subtlety a simultaneous update breaks).
// On that orbit x peaks at A = sqrt(E / (1 − ε²/4)), the amplitude the voice
// keeps as `amp`.
struct Cell {
    float x, y;     // the state
    float eps;      // 2 sin(θ/2) for the current pitch
    float amp;      // the orbit's amplitude (x's peak) the state is held at

    void reset() { x = y = 0.0f; eps = 0.0f; amp = 0.0f; }

    // DRIVE: the strike sets the orbit at amplitude a (x at its peak, y at the
    // staggered zero — the point of the orbit where x = a).
    void kick(float a, float e) {
        eps = e; amp = a;
        x = a; y = 0.5f * e * a;   // y = εx/2 is where x peaks on the invariant's ellipse
    }

    // SYMPLECTIC: one leapfrog step (the magic circle).
    inline void step() {
        x -= eps * y;
        y += eps * x;
    }
    // SYMPLECTIC: the step with a phase-space shear (§2.4) — x' = x − ε·(y + s(y)),
    // then y' = y + ε·x'. The shear is a FORCE, so it scales with the step's
    // rotation ε (a fixed kick per step dwarfs a low note's tiny ε and the
    // orbit escapes — measured: NaN below C3 at g = 0.3 unscaled); with the
    // linear term's sign it HARDENS the spring — the potential grows faster
    // than the quadratic, so every orbit stays bounded (the confinement check).
    inline void step_sheared(float s_of_y) {
        x -= eps * (y + s_of_y);
        y += eps * x;
    }

    // The NEGATIVE CONTROL (test only): the "simultaneous" update, det 1 + ε².
    inline void step_naive() {
        const float x0 = x;
        x -= eps * y;
        y += eps * x0;
    }

    // SYMPLECTIC re-base (§2.1's MPE correction): a new ε with the state kept
    // as it is would put the state on an orbit of a different amplitude —
    // a ±48-semitone glide audibly amplitude-modulates (the "plain" form).
    // Re-based, the state is rescaled onto the orbit of the same amplitude
    // under the new ε: one rsqrt per retuned sample, none while ε holds.
    inline void retune(float e) {
        eps = e;
        const float q = x * x + y * y - e * x * y;                 // the invariant under the new ε
        const float a2 = q / (1.0f - 0.25f * e * e);               // its x-amplitude, squared
        if (a2 > 1e-30f) {
            const float s = amp / sqrtf(a2);
            x *= s; y *= s;
        }
    }
    // The plain retune (the printed justification, never the shipped sound).
    inline void retune_plain(float e) { eps = e; }

    // DISSIPATIVE (conformal): the declared contraction — every decay traces to
    // this factor and nothing else.
    inline void contract(float r) { x *= r; y *= r; amp *= r; }

    float energy() const { return x * x + y * y - eps * x * y; }
};

// SYMPLECTIC shears (§2.4): the s(y) of Cell::step_sheared — any s is legal,
// det = 1 for all of them; they alias, so they run in the oversampled section.
// Each shape acts on the orbit NORMALIZED by its amplitude, u = y / A in
// [−1, 1], and is scaled back by A: s = g · A · shape(u) — so the shear does
// the same thing to a soft note and a loud one, and the same at every level
// (a fixed-scale shape either never reaches its fold at A = 0.25 or swamps a
// soft orbit). The cubic is the Duffing spring (§2.10: a hardening bell); the
// triangle fold has hard edges, legally. A shear changes the restoring force,
// so it DETUNES: to first order by its describing function — the fundamental
// gain of shape(sin φ) — which the voice compensates in ε (the note stays in
// tune at any gain; the residual, second order, is printed by the suite).
inline float shape_cubic(float u) { return u * u * u; }
inline float shape_tri(float u) {                     // a triangle fold of u: 2u to the fold at ±½, back to 0 at ±1
    if (u > 0.5f) return 2.0f - 2.0f * u;
    if (u < -0.5f) return -2.0f - 2.0f * u;
    return 2.0f * u;
}
// A shear changes the restoring force, so it DETUNES — the cubic hardens the
// spring by sqrt(1 + ¾g) to first order, the fold is not sine-like at all —
// and a first-order formula left 110 and 555 cents at full gain (measured).
// So the cell CALIBRATES ITSELF: at create (the shell's thread), the sheared
// cell at unit amplitude runs 64 cycles at a small reference ε for each gain
// of a 17-point table per shape and its period is measured against the bare
// cell's; the voice scales ε by the table's ratio (interpolated) and the note
// stays in tune at any gain — the residual is the reference ε's second order
// at high pitch, printed by the suite.
constexpr int SHEAR_TABLE = 17;
inline float shear_period_ratio(bool cubic, float g) {
    if (g <= 0.0f) return 1.0f;
    const float e = 2.0f * sinf(PI / 256.0f);        // 256 steps per bare cycle
    Cell c; c.reset(); c.kick(1.0f, e);
    double first = -1.0, last = -1.0; long cross = 0; float px = c.x;
    const long steps = 256L * 80L;
    for (long i = 1; i < steps; i++) {
        const float u = c.y;                          // amp = 1: the orbit is already normalized
        c.step_sheared(g * (cubic ? shape_cubic(u) : shape_tri(u)));
        if (px < 0.0f && c.x >= 0.0f) {
            const double t = (double)(i - 1) + (double)(-px) / (double)(c.x - px);
            if (first < 0.0) first = t; else last = t;
            cross++;
        }
        px = c.x;
    }
    if (cross < 3) return 1.0f;
    const double period = (last - first) / (double)(cross - 1);
    return (float)(256.0 / period);                   // the sheared frequency over the bare one
}
inline void shear_table_fill(bool cubic, float* table /*SHEAR_TABLE*/) {
    for (int i = 0; i < SHEAR_TABLE; i++) table[i] = shear_period_ratio(cubic, (float)i / (float)(SHEAR_TABLE - 1));
}
inline float shear_table_ratio(const float* table, float g) {
    if (g <= 0.0f) return 1.0f;
    if (g >= 1.0f) return table[SHEAR_TABLE - 1];
    const float p = g * (float)(SHEAR_TABLE - 1);
    const int i = (int)p; const float t = p - (float)i;
    return table[i] + (table[i + 1] - table[i]) * t;
}

// THE CHAMBERLIN SVF (§2.2): low += f·band; high = in − low − q·band;
// band += f·high — the (low, band) pair is the magic circle, driven by `in`
// and damped by q (the declared dissipation: 0 rings forever, 2 is dead).
// Exact tuning f = 2 sin(π fc / rate). Run at the oversampled rate, where the
// classic form's fs/6 ceiling sits at fs/3 of the output rate.
struct Svf {
    float low, band;
    void reset() { low = band = 0.0f; }
    inline float step(float in, float f, float q) {
        low += f * band;
        const float high = in - low - q * band;
        band += f * high;
        return low;
    }
};

// The SVF's coefficient for a RINGING frequency (§2.2's "exact tuning",
// finished): the (low, band) core with q = 0 rings at exactly fc when
// f = 2 sin(π fc / rate); with damping the update is the matrix
// [[1, f], [−f, 1 − fq − f²]], whose rotation per step obeys
//   cos θ = (2 − fq − f²) / (2 sqrt(1 − fq)),
// so the ring sits SHARP of fc by an amount that grows with f and 1/q — 3
// cents at C8 for Q = 50 at 96 kHz, over the 2-cent gate. Solved for f from
// the wanted θ and q by two Newton steps from the undamped value — the
// thematic form kept, the pitch cents-true at any damping (the trapezoidal
// escape hatch stays unused).
inline float svf_f_for(float hz, float rate, float q) {
    const float f0 = eps_for(hz, rate);
    if (q <= 0.0f) return f0;
    // The stable form: the same equation with 1 − cos θ = 2 sin²(θ/2) and
    // 1 − sqrt(1 − fq) = fq / (1 + sqrt(1 − fq)) — a difference of numbers
    // near 2 would cancel to θ² and drown at low pitch in float. In double:
    //   f² + f·q·k = 4 sin²(θ/2),  k = (4 sin²(θ/2) − u) / (1 + s),
    //   s = sqrt(1 − fq), u = fq / (1 + s) — k depends on f only through u,
    // so two passes of the quadratic settle it.
    double th = 2.0 * (double)PI * (double)hz / (double)rate; if (th > 3.0) th = 3.0;
    const double sh = sin(0.5 * th), four_s2 = 4.0 * sh * sh;
    double f = (double)f0;
    for (int i = 0; i < 3; i++) {
        const double fq = f * (double)q;
        const double sq = sqrt(fq < 1.0 ? 1.0 - fq : 0.0);
        const double u = fq / (1.0 + sq);
        const double k = (four_s2 - u) / (1.0 + sq);
        const double b = (double)q * k;
        f = 0.5 * (-b + sqrt(b * b + 4.0 * four_s2));
    }
    return (float)f;
}

// The declared contraction for a release: the amplitude falls 60 dB over
// `seconds` at `rate` steps per second (a T60, as a room's).
inline float contraction_for(float seconds, float rate) {
    if (seconds <= 0.0f) return 0.0f;
    return expf(-6.907755f / (seconds * rate));   // ln(1000) = 6.9078
}

// Denormals (§2.4, §4): decaying orbits glide into subnormal floats and the
// callback's cost explodes on x86 — flush-to-zero / denormals-are-zero is set
// on the rendering thread at its first block (suzu.cpp) and gated by the
// suite's decay-tail stress.
void ftz_enable();           // this thread: FTZ + DAZ on (x86: MXCSR; arm64: FPCR.FZ)
bool ftz_enabled();          // reads the thread's mode back
void ftz_disable();          // the test's negative control only

// ---------------------------------------------------------------------------
// Step 57 — the modal lattice and the breath bow (SYNTH §2.5–§2.6, §3).
// ---------------------------------------------------------------------------
constexpr int MODES_MAX = 16;

// THE COUPLING (§2.5), stated as the map it is. In each cell x is the
// position and y = −v/ω the scaled velocity (the leapfrog's convention:
// x −= ε·y; y += ε·x is ẍ = −ω²x). A spring potential between neighbours,
// U = ½·κ_c·(x_k − x_{k+1})², adds v̇_k = −κ_c·(x_k − x_{k+1}), which in the
// cell's units is y_k += (κ_c·dt/ω_k)·(x_k − x_{k+1}). With κ_c = κ·ω₀² —
// κ relative to the FUNDAMENTAL's stiffness — that is
//     y_k += (κ·ε₀² / ε_k) · (L·x)_k,   L the chain's Laplacian,
// a gradient kick in the coordinates (x_k, ε_k·y_k): symmetric, hence
// SYMPLECTIC in the joint space, computed from the PRE-update positions
// (read before write), then each cell rotates, then the declared damping.
// THE CEILING (the CFL analog): the joint leapfrog is stable iff every joint
// mode's per-step stiffness stays under 4 — λ_max(S) < 4 for
//     S = diag(ε_k²) + κ·ε₀²·L   (in ε² units),
// which patch load checks by power iteration over the playable range.
// (The order — the coupling kick, then each cell's drift and own kick — is
// the standard drift–kick leapfrog with the FULL S up to a half-step shift:
// two kicks in a row commute and merge, so the bound λ_max(S) < 4 is exact.)
// THE CEILING of a mode: MODE_CEILING of the oversampled rate = the OUTPUT
// Nyquist — a mode above it would fold back through the decimator; it is
// muted at the strike and, during a glide, the moment it crosses.
constexpr float MODE_CEILING = 0.25f;
// A MUTED mode (inv_eps 0: reset, above the ceiling) is a WALL: it is not
// kicked and stays at x = 0, so its alive neighbour keeps both springs.
inline void lattice_kick(Cell* c, const float* inv_eps, int n, float k_eps0sq) {
    if (n < 2 || k_eps0sq <= 0.0f) return;
    float lx[MODES_MAX];
    for (int k = 0; k < n; k++) {                 // (L·x)_k from the pre-update positions
        float v = 0.0f;
        if (k > 0)     v += c[k].x - c[k - 1].x;
        if (k < n - 1) v += c[k].x - c[k + 1].x;
        lx[k] = v;
    }
    for (int k = 0; k < n; k++) c[k].y += (k_eps0sq * inv_eps[k]) * lx[k];
}
// λ_max of S by power iteration (K ≤ 16: microseconds). Symmetric positive
// definite, so the iterate converges to the top eigenvalue from any start.
inline float lattice_lambda_max(const float* eps, int n, float k_eps0sq, bool walled = false) {   // walled: a muted mode past the last, a fixed end
    float v[MODES_MAX], w[MODES_MAX];
    for (int k = 0; k < n; k++) v[k] = 1.0f + 0.1f * (float)k;
    float lam = 0.0f;
    for (int it = 0; it < 200; it++) {
        for (int k = 0; k < n; k++) {
            float s = eps[k] * eps[k] * v[k];
            if (k > 0)     s += k_eps0sq * (v[k] - v[k - 1]);
            if (k < n - 1) s += k_eps0sq * (v[k] - v[k + 1]);
            else if (walled) s += k_eps0sq * v[k];
            w[k] = s;
        }
        float norm = 0.0f; for (int k = 0; k < n; k++) norm += w[k] * w[k];
        norm = sqrtf(norm); if (norm <= 0.0f) return 0.0f;
        lam = norm;                                   // ‖S v‖ with ‖v‖ = 1 → the top eigenvalue
        for (int k = 0; k < n; k++) v[k] = w[k] / norm;
    }
    return lam;
}
constexpr float LATTICE_BOUND = 4.0f;               // the joint leapfrog's stability bound on λ_max(S)
// The coupling's stiffness κ·ε₀² is CAPPED at C8's ε₀: above C8 (a bend can
// reach MIDI 156) the coupling stops growing with the pitch, so a bent note
// cannot stiffen the chain past what the gate saw. eps_c8 = eps_for(C8, rate2).
inline float coupling_k(float kappa, float e0, float eps_c8) {
    const float e = e0 < eps_c8 ? e0 : eps_c8;
    return kappa * e * e;
}
// THE COUPLING'S DETUNE, COMPENSATED (§2.5): a spring between neighbours
// stiffens each mode it touches — a harmonic chain's fundamental goes sharp
// by κ/4 to first order (measured 149 cents through the ABI at the default
// κ with the swirl's share) — so κ would be a pitch control before a timbre
// control. The voice compensates it EXACTLY: at patch load the normal modes
// of M(κ) = diag(ρ_k²) + κ·L (ρ_k = r_k / r_0, in units of the LOWEST mode's
// ε² — the coupling's reference stiffness, so the bell's hum keeps a spring
// of its own) are solved (cyclic Jacobi, n ≤ 16, double) and each mode's own
// stiffness d_k is moved by fixed point until the k-th normal mode lands on
// ρ_k²; the voice runs ε_k·c_k with c_k = sqrt(d_k)/ρ_k, tabled over κ and
// interpolated per block as the swirl moves. Pitch-independent while ε ≈ θ
// (every fundamental; the top modes near the ceiling drift a little,
// printed by the suite). A patch whose coupling would take a mode's whole
// stiffness (d_k ≤ 1 % of its own) is rejected by the load gate: a coupled
// system with no spring of its own under one of its partials.
constexpr int   COMP_POINTS = 129;                    // κ = 0 … COMP_KAPPA_MAX in steps of COMP_STEP (1/32: the lerp's
                                                      // error near the feasibility edge is 0.2 cent; a 1/8 grid missed by 3.6)
constexpr float COMP_KAPPA_MAX = 4.0f;
constexpr float COMP_STEP = COMP_KAPPA_MAX / (float)(COMP_POINTS - 1);
inline void sym_eigen(double* a, int n, double* w, double* V) {   // a: n×n symmetric, destroyed; w ascending; V's columns their vectors (V[i*n+k])
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) V[i * n + j] = i == j ? 1.0 : 0.0;
    for (int sweep = 0; sweep < 60; sweep++) {
        double off = 0.0;
        for (int p = 0; p < n; p++) for (int q = p + 1; q < n; q++) off += a[p * n + q] * a[p * n + q];
        if (off < 1e-22) break;                       // the eigenvalues are second order in the off-diagonal residue
        for (int p = 0; p < n; p++) for (int q = p + 1; q < n; q++) {
            const double apq = a[p * n + q];
            if (fabs(apq) < 1e-300) continue;
            const double theta = (a[q * n + q] - a[p * n + p]) / (2.0 * apq);
            const double t = (theta >= 0.0 ? 1.0 : -1.0) / (fabs(theta) + sqrt(theta * theta + 1.0));
            const double c = 1.0 / sqrt(t * t + 1.0), s = t * c;
            for (int k = 0; k < n; k++) { const double akp = a[k * n + p], akq = a[k * n + q]; a[k * n + p] = c * akp - s * akq; a[k * n + q] = s * akp + c * akq; }
            for (int k = 0; k < n; k++) { const double apk = a[p * n + k], aqk = a[q * n + k]; a[p * n + k] = c * apk - s * aqk; a[q * n + k] = s * apk + c * aqk; }
            for (int k = 0; k < n; k++) { const double vkp = V[k * n + p], vkq = V[k * n + q]; V[k * n + p] = c * vkp - s * vkq; V[k * n + q] = s * vkp + c * vkq; }
        }
    }
    int order[MODES_MAX]; for (int k = 0; k < n; k++) order[k] = k;
    for (int i = 1; i < n; i++) { const int x = order[i]; int j = i; while (j > 0 && a[order[j - 1] * n + order[j - 1]] > a[x * n + x]) { order[j] = order[j - 1]; j--; } order[j] = x; }
    double Vs[MODES_MAX * MODES_MAX];
    for (int k = 0; k < n; k++) { w[k] = a[order[k] * n + order[k]]; for (int i = 0; i < n; i++) Vs[i * n + k] = V[i * n + order[k]]; }
    for (int i = 0; i < n * n; i++) V[i] = Vs[i];
}
inline void sym_eigenvalues(double* a, int n, double* w) { double V[MODES_MAX * MODES_MAX]; sym_eigen(a, n, w, V); }
// comp[k] = c_k for this κ; false when a mode's own stiffness would be gone.
// Newton on d: the k-th eigenvalue's derivative to the j-th diagonal is the
// j-th component of the k-th eigenvector, squared (J = V∘V, doubly
// stochastic) — a plain fixed point stalls near the feasibility edge, where
// that component of the fundamental's vector goes small.
// jinv (optional, n×n row-major): the inverse of the eigen-derived Jacobian at
// the solution — δd = J⁻¹·δs, the runtime's first-order correction (below).
inline bool lattice_compensation(const float* ratio, int n, float kappa, float* comp, const float* warm = nullptr, float* jinv = nullptr) {   // warm: a nearby κ's comp, the Newton's start
    for (int k = 0; k < n; k++) comp[k] = 1.0f;
    if (jinv) for (int i = 0; i < n * n; i++) jinv[i] = (i % (n + 1)) == 0 ? 1.0f : 0.0f;
    if (n < 2 || kappa <= 0.0f) return true;
    double rho2[MODES_MAX], d[MODES_MAX], a[MODES_MAX * MODES_MAX], w[MODES_MAX], V[MODES_MAX * MODES_MAX];
    for (int k = 0; k < n; k++) { const double r = (double)ratio[k] / (double)ratio[0]; rho2[k] = r * r; d[k] = warm ? (double)warm[k] * warm[k] * rho2[k] : rho2[k]; }
    for (int it = 0; it < 40; it++) {
        for (int i = 0; i < n * n; i++) a[i] = 0.0;
        for (int k = 0; k < n; k++) {
            a[k * n + k] = d[k] + (double)kappa * ((k > 0 ? 1.0 : 0.0) + (k < n - 1 ? 1.0 : 0.0));
            if (k > 0) a[k * n + k - 1] = a[(k - 1) * n + k] = -(double)kappa;
        }
        sym_eigen(a, n, w, V);
        double J[MODES_MAX * MODES_MAX], r[MODES_MAX], err = 0.0;
        for (int k = 0; k < n; k++) { r[k] = rho2[k] - w[k]; const double e = fabs(r[k]) / rho2[k]; if (e > err) err = e; for (int j = 0; j < n; j++) J[k * n + j] = V[j * n + k] * V[j * n + k]; }
        if (err < 1e-12) {
            if (jinv) {                                        // J⁻¹ at the solution, by Gauss–Jordan on [J | I]
                double A[MODES_MAX * MODES_MAX], B[MODES_MAX * MODES_MAX];
                for (int i = 0; i < n * n; i++) { A[i] = J[i]; B[i] = (i % (n + 1)) == 0 ? 1.0 : 0.0; }
                for (int col = 0; col < n; col++) {
                    int piv = col; for (int row = col + 1; row < n; row++) if (fabs(A[row * n + col]) > fabs(A[piv * n + col])) piv = row;
                    if (piv != col) for (int j = 0; j < n; j++) { double t = A[col * n + j]; A[col * n + j] = A[piv * n + j]; A[piv * n + j] = t; t = B[col * n + j]; B[col * n + j] = B[piv * n + j]; B[piv * n + j] = t; }
                    const double pv = A[col * n + col]; if (fabs(pv) < 1e-300) continue;
                    for (int j = 0; j < n; j++) { A[col * n + j] /= pv; B[col * n + j] /= pv; }
                    for (int row = 0; row < n; row++) { if (row == col) continue; const double f = A[row * n + col]; if (f == 0.0) continue; for (int j = 0; j < n; j++) { A[row * n + j] -= f * A[col * n + j]; B[row * n + j] -= f * B[col * n + j]; } }
                }
                for (int i = 0; i < n * n; i++) jinv[i] = (float)B[i];
            }
            break;
        }
        for (int col = 0; col < n; col++) {                    // J·Δ = r by Gaussian elimination, partial pivoting
            int piv = col; for (int row = col + 1; row < n; row++) if (fabs(J[row * n + col]) > fabs(J[piv * n + col])) piv = row;
            if (piv != col) { for (int j = 0; j < n; j++) { const double t = J[col * n + j]; J[col * n + j] = J[piv * n + j]; J[piv * n + j] = t; } const double t = r[col]; r[col] = r[piv]; r[piv] = t; }
            const double pv = J[col * n + col]; if (fabs(pv) < 1e-300) continue;
            for (int row = col + 1; row < n; row++) { const double f = J[row * n + col] / pv; if (f == 0.0) continue; for (int j = col; j < n; j++) J[row * n + j] -= f * J[col * n + j]; r[row] -= f * r[col]; }
        }
        double delta[MODES_MAX];
        for (int row = n - 1; row >= 0; row--) { double acc = r[row]; for (int j = row + 1; j < n; j++) acc -= J[row * n + j] * delta[j]; const double pv = J[row * n + row]; delta[row] = fabs(pv) > 1e-300 ? acc / pv : 0.0; }
        for (int k = 0; k < n; k++) { d[k] += delta[k]; if (d[k] < 1e-4 * rho2[k]) d[k] = 1e-4 * rho2[k]; }   // infeasible: held on positive ground, reported below
    }
    bool ok = true;
    for (int k = 0; k < n; k++) {
        if (d[k] <= 0.01 * rho2[k]) ok = false;
        comp[k] = (float)sqrt(d[k] / rho2[k]);
    }
    return ok;
}
// The table holds c_k² (= d_k/ρ_k², smooth and nearly linear in κ) and J⁻¹
// per κ point; a linear lerp of c_k itself missed by 2 cents near the edge.
inline float table_lerp(const float* lo, const float* hi, float t, int i) { return lo[i] + (hi[i] - lo[i]) * t; }
// THE RUNTIME'S COMPENSATION at a pitch (per block, per voice — n² MACs):
// the table is solved for the LINEAR ratios ρ_k = r_k/r_0, but a mode's ε
// bends below linear toward the ceiling (ε = 2 sin(θ/2): the third partial
// of C8 at 12.5 kHz sits 5 % under, and read 5 cents off with the swirl at
// full). So the true ratios at this pitch, ρ_k' = ε_k/ε₀, correct the
// solved stiffness to first order, δd = J⁻¹·(ρ'² − ρ²) — the residual is
// second order in the bend (a tenth of a cent). Muted modes are walls:
// their δ is zero and their comp unused. comp[k] multiplies eps_for(f·r_k).
inline void lattice_comp_at_pitch(const float* c2, const float* jinv, const float* ratio, int n, float f0, float rate2, float* comp) {
    const float e0 = eps_for(f0 * ratio[0], rate2);
    float rho2[MODES_MAX], drho2[MODES_MAX], eps[MODES_MAX];
    for (int k = 0; k < n; k++) {
        const float r = ratio[k] / ratio[0]; rho2[k] = r * r;
        const float fk = f0 * ratio[k];
        if (fk >= MODE_CEILING * rate2) { eps[k] = 0.0f; drho2[k] = 0.0f; continue; }
        eps[k] = eps_for(fk, rate2);
        const float rt = eps[k] / e0; drho2[k] = rt * rt - rho2[k];
    }
    for (int k = 0; k < n; k++) {
        if (eps[k] <= 0.0f) { comp[k] = 1.0f; continue; }
        float d = c2[k] * rho2[k];
        for (int j = 0; j < n; j++) d += jinv[k * n + j] * drho2[j];
        const float rt2 = rho2[k] + drho2[k];
        if (d < 1e-4f * rt2) d = 1e-4f * rt2;
        comp[k] = sqrtf(d) * e0 / eps[k];                          // ε_eff = sqrt(d)·ε₀ = comp·ε_k
    }
}
// The gate's sweep (§2.5, §5): λ_max(S) of the chain AS THE VOICE RUNS IT at
// every semitone of the playable range — MIDI 21 up to 108 + the 48-semitone
// bend — the compensation from the exact solve at this κ, corrected per pitch
// as above, the modes past the ceiling muted into walls; the maximum decides.
// Monotone in κ, so a bisection on it is sound. The coupling's reference is
// the lowest mode's ε, capped at its C8 value.
inline float lattice_lambda_max_over_pitch(const float* ratio, int n, float rate2, float kappa, int* worst_midi = nullptr) {
    float worst = 0.0f; int at = 0;
    float c[MODES_MAX], c2[MODES_MAX], jinv[MODES_MAX * MODES_MAX];
    lattice_compensation(ratio, n, kappa, c, nullptr, jinv);
    for (int k = 0; k < n; k++) c2[k] = c[k] * c[k];
    const float eps_c8 = eps_for(4186.009f * ratio[0], rate2);
    for (int midi = 21; midi <= 156; midi++) {
        const float f0 = 440.0f * exp2f((float)(midi - 69) / 12.0f);
        float comp[MODES_MAX], eps[MODES_MAX]; int alive = 0;
        lattice_comp_at_pitch(c2, jinv, ratio, n, f0, rate2, comp);
        for (int k = 0; k < n; k++) {
            const float fk = f0 * ratio[k];
            if (fk >= MODE_CEILING * rate2) break;       // the muted modes: walls past the alive prefix
            eps[alive++] = comp[k] * eps_for(fk, rate2);
        }
        if (alive == 0) break;                            // and above that every mode is silent
        const float lam = lattice_lambda_max(eps, alive, coupling_k(kappa, eps_for(f0 * ratio[0], rate2), eps_c8), alive < n);
        if (lam > worst) { worst = lam; at = midi; }
    }
    if (worst_midi) *worst_midi = at;
    return worst;
}

// THE BOW (§2.6): the servo's per-sub-step factor on a cell's state. E is
// the cell's energy x² + y², E_t the breath's target; g = 1/(τ·rate) is the
// onset rate. Below the target the cell is anti-damped (the bow grips: an
// e-fold of amplitude in τ from silence), above it damped (the bow slips,
// three times as fast from twice the target); the factor is clamped so no
// single step gives more than an e-fold's share — a limit cycle by
// construction, never free growth. Zero breath ⇒ E_t = 0 ⇒ contraction only.
inline float bow_factor(float E, float E_t, float g) {
    const float E_ref = E_t > 1e-12f ? E_t : 1e-12f;
    float u = (E_t - E) / E_ref;                    // +1 at silence … 0 at the target … −3 at 2× the amplitude
    if (u > 1.0f) u = 1.0f;
    if (u < -4.0f) u = -4.0f;
    return 1.0f + g * u;
}
// The NEGATIVE CONTROL (test only): the servo with its extraction clamped off —
// it gives and never takes — grows without bound.
inline float bow_factor_give_only(float E, float E_t, float g) {
    const float E_ref = E_t > 1e-12f ? E_t : 1e-12f;
    float u = (E_t - E) / E_ref; if (u < 0.0f) u = -u; if (u > 1.0f) u = 1.0f;
    return 1.0f + g * u;
}

// THE PRESETS (§2.5): the tables that make the instruments. Ratios r_k,
// declared decays as T60s per mode from γ_k = α + β·(k² − 1) (α = ln(1000)/
// decay_s, the fundamental's; highs die first — the KS law), the strike's
// kick profile w_k and the bow's b_k. The plucked string is Karplus–Strong in
// modal form: f_k = k·f₀·√(1 + B·k²), w_k ∝ sin(kπp)/k² (the triangular
// pluck's spectrum — pluck near the bridge and the kicks brighten).
enum ModalPreset { PRESET_HARMONIC = 0, PRESET_BAR = 1, PRESET_BELL = 2, PRESET_GLASS = 3, PRESET_PLUCKED = 4 };
struct ModalTable {
    int   n;
    float ratio[MODES_MAX];
    float t60[MODES_MAX];       // seconds, while held
    float kick[MODES_MAX];      // the strike's profile, max 1
    float bow[MODES_MAX];       // the bow's profile, max 1
};
inline void modal_table(ModalTable* t, int preset, int n, float decay_s, float decay_bright, float stiffness, float pluck, float bow_position) {
    if (n < 1) n = 1; if (n > MODES_MAX) n = MODES_MAX;
    t->n = n;
    static const float bell_r[MODES_MAX] = {0.5f, 1.0f, 1.2f, 1.5f, 2.0f, 2.5f, 2.667f, 3.0f, 3.5f, 4.0f, 4.5f, 5.0f, 5.333f, 6.0f, 6.667f, 7.0f};
    static const float bell_w[MODES_MAX] = {0.6f, 1.0f, 0.8f, 0.5f, 0.9f, 0.4f, 0.3f, 0.35f, 0.2f, 0.25f, 0.15f, 0.12f, 0.1f, 0.08f, 0.06f, 0.05f};
    static const float glass_r[MODES_MAX] = {1.0f, 2.32f, 4.25f, 6.63f, 9.38f, 12.5f, 16.1f, 20.1f, 24.5f, 29.3f, 34.5f, 40.1f, 46.1f, 52.5f, 59.3f, 66.5f};
    const float alpha = decay_s > 0.0f ? 6.907755f / decay_s : 0.0f;                          // decay_s 0: NO decay while held (the lab's ledger)
    float wsum_max = 0.0f;
    for (int k = 0; k < n; k++) {
        const float m = (float)(k + 1);
        float r, w;
        switch (preset) {
            case PRESET_BAR:     r = (2.0f * m + 1.0f) * (2.0f * m + 1.0f) / 9.0f; w = 1.0f / m; break;       // the free bar: 1, 2.78, 5.44, 9 …
            case PRESET_BELL:    r = bell_r[k]; w = bell_w[k]; break;
            case PRESET_GLASS:   r = glass_r[k]; w = 1.0f / (m * m); break;
            case PRESET_PLUCKED: r = m * sqrtf(1.0f + stiffness * m * m); w = fabsf(sinf(m * PI * pluck)) / (m * m); break;
            default:             r = m * sqrtf(1.0f + stiffness * m * m); w = 1.0f / m; break;                 // the harmonic string
        }
        t->ratio[k] = r;
        t->kick[k] = w;
        const float gamma = alpha + decay_bright * (r * r - 1.0f);                        // γ_k = α + β·(r_k² − 1): highs die first
        t->t60[k] = gamma > 1e-9f ? 6.907755f / gamma : 0.0f;                               // 0 = none declared
        // the bow's profile: position 0 feeds the fundamental alone, 1 every mode evenly; between, sin(kπ·pos) like a bow's point
        const float b = bow_position <= 0.0f ? (k == 0 ? 1.0f : 0.0f) : (bow_position >= 1.0f ? 1.0f : fabsf(sinf(m * PI * bow_position)));
        t->bow[k] = b;
    }
    for (int k = 0; k < n; k++) wsum_max = fmaxf(wsum_max, t->kick[k]);
    if (wsum_max > 0.0f) for (int k = 0; k < n; k++) t->kick[k] /= wsum_max;
    if (preset == PRESET_BELL) { t->kick[1] = 1.0f; }                                        // the prime is the loudest, as cast
}

} // namespace suzu
