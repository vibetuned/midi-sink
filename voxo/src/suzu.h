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
//   VerletString::step               SYMPLECTIC Euler (kick   §2.8: the 1D chain, the CFL bound
//                                    then drift) + conformal  k·dt² ≤ 1 enforced at patch load
//                                    per-node damping
//   HybridString: the delay + the    LOSSLESS TRANSPORT       §2.9: a ring + a Thiran allpass
//   Thiran allpass                   (measure-preserving)     for the fraction, |H| = 1
//   HybridString: the junction       LOSSLESS (a scattering   §2.9: the bridge's velocity re-
//                                    junction: the bridge's   radiates: −s + v_b, the kick
//                                    radiation is the         c·(2s − v_b) — the load-time probe
//                                    string's, not a loss)    and the soak gate it (§5)
//   HybridString: the loss filter,   DISSIPATIVE (declared:   §2.9: the KS averager as a one-
//   the round-trip gain              a one-zero ≤ 1, a factor) zero with |H| ≤ 1, a T60
//   Duffing::step                    SYMPLECTIC (the kick     §2.10: rotation + cubic hardening
//                                    reads x + βx³)           spring — the clang-and-settle
//   Duffing drive                    DRIVE (bounded per       §2.10: a sinusoid at a ratio of
//                                    sample)                  the note, the press its amplitude
//   Rotor::step                      SYMPLECTIC (the standard §2.3: the cell kicked once per
//                                    map on (θ, p); the cell  nominal cycle, p += K·sin θ, the
//                                    re-based on each kick)   momentum the pitch (mod 2π)
//   DoublePendulum::step             HAMILTONIAN at control   §2.10: the chaotic modulator,
//                                    rate (velocity Verlet;   energy set at trigger, bounded,
//                                    bounded by its energy)   routed to a smoothed parameter
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

// ---------------------------------------------------------------------------
// Step 58 — strings & chaos (SYNTH §2.3, §2.8–§2.10).
// ---------------------------------------------------------------------------
constexpr int   STRING_NODES_MAX = 80;
constexpr int   DELAY_MAX = 4096;                    // A0 at 96 kHz needs 3491 samples of loop
constexpr int   BRIDGE_MAX = 3;

// THE VERLET CHAIN (§2.8): M interior nodes between fixed ends, per sub-step
// and per node accel = k·(u[i+1] − 2u[i] + u[i−1]) from the PRE-update u,
// v = v·damping + accel·dt, u += v·dt — symplectic Euler (kick, then drift)
// with a conformal damping. In dimensionless units (dt = 1) the one number is
// s² = k·dt², and the m-th mode rotates by θ_m = 2·asin(s·sin(mπ/(2(M+1))))
// per step (the 1–2–1 Laplacian's eigenvalues 4 sin², the cell's ε² = λ):
// THE CFL BOUND s ≤ 1 keeps the top mode under Nyquist — over it, the
// scheme is unstable and the harness blows up (the red control). Tuning
// couples M, k and dt: for a note f₀ the chain needs s = sin(πf₀/rate) /
// sin(π/(2(M+1))) ≤ 1, so M is REDUCED per note to floor(rate/(2f₀) − 1);
// the pitch is exact by construction wherever M ≥ 2 (f₀ under rate/6).
// The pluck is a triangular profile ADDED to the state (re-plucking a
// ringing string) released from rest; the pickup reads u at a node.
struct VerletString {
    float u[STRING_NODES_MAX + 2], v[STRING_NODES_MAX + 2];   // [0] and [m+1] are the fixed ends
    int   m;                                                  // interior nodes
    float s2;                                                 // k·dt²
    void reset(int nodes) { m = nodes < 2 ? 2 : (nodes > STRING_NODES_MAX ? STRING_NODES_MAX : nodes); s2 = 0.0f; for (int i = 0; i < STRING_NODES_MAX + 2; i++) u[i] = v[i] = 0.0f; }
    static int   nodes_for(float hz, float rate, int wanted) { const float cap = rate / (2.0f * hz) - 1.0f; int c = (int)floorf(cap); if (c > wanted) c = wanted; return c; }   // < 2: not representable
    static float s_for(float hz, float rate, int nodes) { return sinf(PI * hz / rate) / sinf(PI / (2.0f * (float)(nodes + 1))); }
    static float hz_for(float s, float rate, int nodes) { const float a = s * sinf(PI / (2.0f * (float)(nodes + 1))); return rate / PI * asinf(a > 1.0f ? 1.0f : a); }
    void tune(float s) { s2 = s * s; }
    void pluck(float a, float pos) {                          // a triangle peaking at pos (0..1 of the length), from rest
        const float peak = pos * (float)(m + 1);
        for (int i = 1; i <= m; i++) { const float x = (float)i; u[i] += x <= peak ? a * x / peak : a * ((float)(m + 1) - x) / ((float)(m + 1) - peak); }
    }
    inline void step(float damp) {
        float lap[STRING_NODES_MAX + 2];
        for (int i = 1; i <= m; i++) lap[i] = u[i + 1] - 2.0f * u[i] + u[i - 1];   // every acceleration from the pre-update u
        for (int i = 1; i <= m; i++) { v[i] = v[i] * damp + s2 * lap[i]; u[i] += v[i]; }
    }
    float pickup(float pos) const { const float x = pos * (float)(m + 1); int i = (int)x; if (i > m) i = m; const float t = x - (float)i; return u[i] + (u[i + 1] - u[i]) * t; }
    float energy() const { double e = 0.0; for (int i = 1; i <= m; i++) e += (double)v[i] * v[i]; for (int i = 0; i <= m; i++) { const double d = (double)u[i + 1] - u[i]; e += (double)s2 * d * d; } return (float)e; }
};

// THE HYBRID STRING (§2.9): a lossless delay line (a ring of N samples plus a
// first-order Thiran allpass for the fraction, |H| = 1) carries the wave into
// a BRIDGE of magic-circle cells. The junction, per sample, is a scattering
// junction stated in the string's units: the sample s arriving at the bridge
// is the incident velocity wave; the bridge's velocity v_b = Σ y_m; each
// mode is kicked by the wave's force c_m·(2s − v_b) — the −v_b share is the
// bridge's own motion radiating BACK into the string, which is what keeps
// the junction lossless: incident power s² − reflected (v_b − s)² =
// (2s − v_b)·v_b, exactly the power the bridge receives. Discretized by the
// MIDPOINT rule — the force reads v̄ = (v_b + v_b')/2, solved in closed form,
// F = (2s − Σỹ)/(1 + Σc/2), with ỹ = y − ε·x/2 the cell's velocity at x's
// time level — the balance holds EXACTLY per sample against the cells'
// invariant (the explicit form pumped energy through its c·F² term, and the
// midpoint form on y itself through −ε·x·δ: both probes went to infinity);
// then the cells rotate (symplectic) and their declared damping applies. The reflected wave v̄ − s travels to the nut, inverts
// there (the fixed end) and returns, so the sample written back into the
// loop is s − g·v̄ (g = 1: the conserving junction; the lab's bridge_gain
// over 1 is the passivity gate's red control), after the declared losses
// (a one-zero |H| ≤ 1, a round-trip factor). The pluck is a triangle written
// into the loop; the pickup reads the loop at a fraction (both directions).
// Passivity is gated twice (§5): the load-time probe (a few ms, every
// declared damping zeroed, non-growth) and the ten-minute soak.
struct HybridString {
    float d[DELAY_MAX];               // the line's history (a ring); the loop reads N samples behind the write
    int   n, w;                       // the loop's integer length and the write index
    float ap_a, ap_x1, ap_y1;         // the Thiran allpass (the fraction δ ∈ [0.5, 1.5))
    float loss_a, loss_x1;            // the one-zero loss y = (1 − a)x + a·x[n−1], a ∈ [0, 0.5]
    float gain;                       // the declared round-trip loss as a per-sample factor (1 = none)
    Cell  bridge[BRIDGE_MAX]; float bc[BRIDGE_MAX], bdamp[BRIDGE_MAX]; int nb;
    float bgain;                      // the reflection's v̄ scale (1 = conserving)
    float phase_j;                    // the junction's reflection phase at the note (radians), from tune()
    void reset() { for (int i = 0; i < DELAY_MAX; i++) d[i] = 0.0f; n = 2; w = 0; ap_a = 0.0f; ap_x1 = ap_y1 = 0.0f; loss_a = 0.0f; loss_x1 = 0.0f; gain = 1.0f; nb = 0; bgain = 1.0f; phase_j = 0.0f; for (int b = 0; b < BRIDGE_MAX; b++) { bridge[b].reset(); bc[b] = 0.0f; bdamp[b] = 1.0f; } }
    void bridge_set(int count, const float* hz, float rate, float coupling, const float* damp) {
        nb = count < 0 ? 0 : (count > BRIDGE_MAX ? BRIDGE_MAX : count);
        for (int b = 0; b < nb; b++) { bridge[b].eps = eps_for(hz[b], rate); bc[b] = coupling; bdamp[b] = damp[b]; }
    }
    // THE JUNCTION'S PHASE at ω, in closed form: the per-sample map on the bridge's state (x_m, y_m) with the
    // wave s as input and the reflection r as output is linear — z' = A·z + B·s, r = Cᵣ·z + D·s — so its
    // steady-state response H(ω) = Cᵣ·(e^{iω}I − A)⁻¹·B + D is a 2·nb complex solve (nb ≤ 3). A and B are
    // read off the map itself by pushing unit vectors through one sample. The loop resonates where its whole
    // phase is 2π, so the reflection's phase is folded into the fractional delay: the fundamental stays on
    // the note while the partials keep the bridge's pull (the body; the wolf near a bridge resonance is
    // real and stays audible as the energy exchange).
    void junction_step_linear(float* z, float sIn, float* rOut) const {   // one sample of the junction on a state vector (no history)
        float Y = 0.0f, C = 0.0f;
        for (int b = 0; b < nb; b++) { Y += z[2 * b + 1] - 0.5f * bridge[b].eps * z[2 * b]; C += bc[b]; }
        const float F = (2.0f * sIn - Y) / (1.0f + 0.5f * C);
        const float vbar = Y + 0.5f * C * F;
        for (int b = 0; b < nb; b++) {
            float x = z[2 * b], y = z[2 * b + 1] + bc[b] * F; const float e = bridge[b].eps;
            x -= e * y; y += e * x; x *= bdamp[b]; y *= bdamp[b];
            z[2 * b] = x; z[2 * b + 1] = y;
        }
        *rOut = sIn - bgain * vbar;
    }
    float junction_phase(float omega) const {                   // arg H(ω); 0 with no bridge
        if (nb < 1) return 0.0f;
        const int m = 2 * nb;
        double Ar[BRIDGE_MAX * 2][BRIDGE_MAX * 2], Br[BRIDGE_MAX * 2], Cr[BRIDGE_MAX * 2], Dr;
        float z[BRIDGE_MAX * 2], r;
        for (int j = 0; j < m; j++) { for (int i = 0; i < m; i++) z[i] = i == j ? 1.0f : 0.0f; junction_step_linear(z, 0.0f, &r); for (int i = 0; i < m; i++) Ar[i][j] = z[i]; Cr[j] = r; }
        for (int i = 0; i < m; i++) z[i] = 0.0f; junction_step_linear(z, 1.0f, &r); for (int i = 0; i < m; i++) Br[i] = z[i]; Dr = r;
        // (e^{iω}I − A)·wv = B, complex Gaussian elimination with partial pivoting
        double Mr[BRIDGE_MAX * 2][BRIDGE_MAX * 2], Mi[BRIDGE_MAX * 2][BRIDGE_MAX * 2], br[BRIDGE_MAX * 2], bi[BRIDGE_MAX * 2];
        const double cw = cos(omega), sw = sin(omega);
        for (int i = 0; i < m; i++) { for (int j = 0; j < m; j++) { Mr[i][j] = (i == j ? cw : 0.0) - Ar[i][j]; Mi[i][j] = i == j ? sw : 0.0; } br[i] = Br[i]; bi[i] = 0.0; }
        for (int col = 0; col < m; col++) {
            int piv = col; double best = Mr[col][col] * Mr[col][col] + Mi[col][col] * Mi[col][col];
            for (int row = col + 1; row < m; row++) { const double v = Mr[row][col] * Mr[row][col] + Mi[row][col] * Mi[row][col]; if (v > best) { best = v; piv = row; } }
            if (piv != col) { for (int j = 0; j < m; j++) { double t = Mr[col][j]; Mr[col][j] = Mr[piv][j]; Mr[piv][j] = t; t = Mi[col][j]; Mi[col][j] = Mi[piv][j]; Mi[piv][j] = t; } double t = br[col]; br[col] = br[piv]; br[piv] = t; t = bi[col]; bi[col] = bi[piv]; bi[piv] = t; }
            const double pr = Mr[col][col], pi = Mi[col][col], pd = pr * pr + pi * pi; if (pd < 1e-300) continue;
            for (int row = col + 1; row < m; row++) {
                const double fr = (Mr[row][col] * pr + Mi[row][col] * pi) / pd, fi = (Mi[row][col] * pr - Mr[row][col] * pi) / pd;   // M[row][col] / pivot
                for (int j = col; j < m; j++) { Mr[row][j] -= fr * Mr[col][j] - fi * Mi[col][j]; Mi[row][j] -= fr * Mi[col][j] + fi * Mr[col][j]; }
                br[row] -= fr * br[col] - fi * bi[col]; bi[row] -= fr * bi[col] + fi * br[col];
            }
        }
        double wr[BRIDGE_MAX * 2], wi[BRIDGE_MAX * 2];
        for (int row = m - 1; row >= 0; row--) {
            double ar = br[row], ai = bi[row];
            for (int j = row + 1; j < m; j++) { ar -= Mr[row][j] * wr[j] - Mi[row][j] * wi[j]; ai -= Mr[row][j] * wi[j] + Mi[row][j] * wr[j]; }
            const double pr = Mr[row][row], pi = Mi[row][row], pd = pr * pr + pi * pi;
            if (pd < 1e-300) { wr[row] = wi[row] = 0.0; continue; }
            wr[row] = (ar * pr + ai * pi) / pd; wi[row] = (ai * pr - ar * pi) / pd;
        }
        double hr = Dr, hi = 0.0;
        for (int i = 0; i < m; i++) { hr += Cr[i] * wr[i]; hi += Cr[i] * wi[i]; }
        return (float)atan2(hi, hr);
    }
    // The loop's delay must total rate/hz around the whole loop: N + δ (the allpass) + the one-zero's phase
    // delay − the junction's phase over ω. Returns false where the note is not representable (N < 2).
    bool tune(float hz, float rate, float loss) {
        loss_a = loss < 0.0f ? 0.0f : (loss > 0.5f ? 0.5f : loss);
        const float omega = 2.0f * PI * hz / rate;
        const float d_loss = loss_a > 0.0f ? atan2f(loss_a * sinf(omega), 1.0f - loss_a + loss_a * cosf(omega)) / omega : 0.0f;
        phase_j = junction_phase(omega);
        const float L = rate / hz - d_loss + phase_j / omega;      // the samples the delay and the allpass must supply
        int N = (int)floorf(L - 0.5f); if (N < 2 || N > DELAY_MAX - 2) return false;
        const float delta = L - (float)N;                          // in [0.5, 1.5)
        n = N; ap_a = (1.0f - delta) / (1.0f + delta);
        return true;
    }
    void pluck(float a, float pos) {                          // the triangle written along the loop's last N samples, added
        const float peak = pos * (float)n;
        for (int i = 0; i < n; i++) { const float x = (float)i; const float t = x <= peak ? x / peak : ((float)n - x) / ((float)n - peak); d[(w - n + i + DELAY_MAX) % DELAY_MAX] += a * t; }
    }
    inline float step(float* vb_out) {
        const float s = d[(w - n + DELAY_MAX) % DELAY_MAX];      // the sample arriving at the bridge: N behind the write
        // The bridge's velocity at x's time level is y − ε·x/2 (the staggered scheme's y sits half a step
        // ahead): kicking y by δ then changes the cell's invariant by exactly (2ỹ + δ)·δ — the kinetic
        // energy's — so the balance below holds against the invariant (with y itself the junction grew).
        float Y = 0.0f, C = 0.0f; for (int b = 0; b < nb; b++) { Y += bridge[b].y - 0.5f * bridge[b].eps * bridge[b].x; C += bc[b]; }
        const float F = (2.0f * s - Y) / (1.0f + 0.5f * C);      // the midpoint force (§2.9 as built): exact balance
        const float vbar = Y + 0.5f * C * F;                     // the bridge's midpoint velocity
        for (int b = 0; b < nb; b++) { Cell& c = bridge[b]; c.y += bc[b] * F; c.step(); if (bdamp[b] != 1.0f) c.contract(bdamp[b]); }
        float r = s - bgain * vbar;                              // the reflection v̄ − s, inverted at the nut on its way back
        const float lo = (1.0f - loss_a) * r + loss_a * loss_x1; loss_x1 = r; r = lo;   // the declared one-zero loss
        const float ap = ap_a * r + ap_x1 - ap_a * ap_y1; ap_x1 = r; ap_y1 = ap; r = ap;  // the fraction, lossless
        d[w] = r * gain;
        if (++w >= DELAY_MAX) w = 0;
        if (vb_out) *vb_out = vbar;
        return s;
    }
    float tap(float pos) const {                               // the string at a fraction of its length: both travelling directions
        const int i1 = (w - n + (int)(pos * (float)n) + DELAY_MAX) % DELAY_MAX, i2 = (w - n + (int)((1.0f - pos) * (float)n) + DELAY_MAX) % DELAY_MAX;
        return d[i1] + d[i2];
    }
    // The loop's energy in the wave's units: the line's N samples squared, plus each bridge mode's invariant
    // over 2c_m (the mode's mass is 1/c_m in these units); the allpass's one sample of state.
    float energy() const { double e = 0.0; for (int i = 0; i < n; i++) e += (double)d[(w - n + i + DELAY_MAX) % DELAY_MAX] * d[(w - n + i + DELAY_MAX) % DELAY_MAX]; e += (double)ap_y1 * ap_y1; for (int b = 0; b < nb; b++) if (bc[b] > 0.0f) e += (double)bridge[b].energy() / (2.0 * (double)bc[b]); return (float)e; }
    // THE LOAD-TIME PROBE (§2.9, §5): the closed loop at a high note (many round trips), every declared
    // damping zeroed, plucked; returns the largest energy over `seconds` relative to the start (1 = held).
    // A patch whose junction gains (bridge_gain past the conserving 1) shows here — no patch can dodge it.
    static float probe_growth(int bridge_count, const float* bridge_hz, float coupling, float bgain_, float rate, float seconds = 0.3f, float hz = 1046.5f) {
        HybridString* h = new HybridString; h->reset();
        const float damp[BRIDGE_MAX] = { 1.0f, 1.0f, 1.0f };
        h->bridge_set(bridge_count, bridge_hz, rate, coupling, damp); h->bgain = bgain_;
        if (!h->tune(hz, rate, 0.0f)) { delete h; return 1.0f; }
        h->pluck(0.25f, 0.28f);
        const double e0 = h->energy(); double emax = e0;
        const long N = (long)(seconds * rate);
        for (long i = 0; i < N; i++) { h->step(nullptr); if ((i & 63) == 0) { const double e = h->energy(); if (!(e <= 1e30)) { delete h; return 1e30f; } if (e > emax) emax = e; } }
        delete h;
        return (float)(emax / e0);
    }
};

// THE DUFFING CELL (§2.10): the rotation with a cubic HARDENING spring in the
// kick — y += ε·(x + β·x³) — symplectic across any swing (a shear in one
// coordinate by a function of the other). The pitch depends on the
// amplitude: struck hard it clangs sharp (≈ 1 + ⅜·β·A² to first order) and
// settles flat onto the note as the declared decay shrinks the orbit; the
// settled pitch is exact with no compensation (the cubic vanishes). Driven
// by a sinusoid (the press its amplitude) it is the second chaos voice.
struct Duffing {
    Cell  c;
    float beta;
    float phase, dphi;                // the drive's phase and its advance per sub-step
    void reset() { c.reset(); beta = 0.0f; phase = 0.0f; dphi = 0.0f; }
    inline void step(float drive) {
        c.x -= c.eps * c.y;
        const float x = c.x;
        c.y += c.eps * (x + beta * x * x * x);
        if (drive != 0.0f) { phase += dphi; if (phase > 2.0f * PI) phase -= 2.0f * PI; c.y += c.eps * drive * sinf(phase); }
    }
};

// THE KICKED ROTOR (§2.3): the standard map as an oscillator. The cell
// carries the angle; once per NOMINAL cycle (a fixed clock at the note's
// frequency — the kick period of Chirikov's rotor) the momentum takes
// p += K·sin θ with sin θ read from the cell's quadrature, p is wrapped to
// (−π, π] (the map on its torus) and the cell is re-based onto the pitch
// f₀·(1 + p/2π) — the momentum IS the pitch, within the octave about the
// note. K = 0: the pure tone; small K: the momentum breathes slowly round an
// island (orderly sidebands); near K_c ≈ 0.9716 the last torus breaks; past
// it p diffuses across the band — pitched noise that remembers f₀. The kick
// is a re-based retune (phase- and amplitude-continuous), never a jump.
struct Rotor {
    Cell  c;
    float p;                          // the momentum, (−π, π]
    float acc, period;                // the kick clock, in sub-steps
    float f0, rate;
    void strike(float a, float hz, float rate_) { f0 = hz; rate = rate_; period = rate_ / hz; acc = 0.0f; p = 0.0f; c.kick(a, eps_for(hz, rate_)); }
    inline void step(float K) {
        c.step();
        acc += 1.0f;
        if (acc >= period) {
            acc -= period;
            const float sth = c.amp > 1e-12f ? c.y / c.amp : 0.0f;
            p += K * sth;
            while (p > PI) p -= 2.0f * PI;
            while (p <= -PI) p += 2.0f * PI;
            c.retune(eps_for(f0 * (1.0f + p / (2.0f * PI)), rate));
        }
    }
};

// THE CHAOTIC MODULATOR (§2.10): a double pendulum (unit masses, lengths and
// gravity) stepped at control rate — its energy set at the trigger from the
// note's velocity (a kick from rest at the bottom: below the flip energy it
// swings, above it it tumbles, chaotically) — and read as sin θ₂, bounded in
// [−1, 1] by construction. The spec says leapfrog; the double pendulum's
// Hamiltonian is not separable, so a leapfrog is not symplectic for it and
// the first probe drifted from E = −2 to +1.4 in ten minutes (and to NaN
// from a hard kick). Shipped: classical RK4 in double at a fixed sub-step,
// with the energy PROJECTED back onto the trigger's value after each control
// step (the velocities rescaled — a declared correction, the modulator's
// class table row): bounded by construction, the projection's size the
// suite's number. Routed to a smoothed parameter as the patch's mod source.
struct DoublePendulum {
    double th1, th2, w1, w2, dt, e_set;
    void reset() { th1 = th2 = w1 = w2 = 0.0; dt = 0.005; e_set = -3.0; }
    void trigger(double kick) { th1 = th2 = 0.0; w1 = kick; w2 = 0.0; e_set = energy(); }   // kick 0 … ~3: E = kick² − 3, the flip at E > −1
    static void deriv(const double* y, double* dy) {           // y = {th1, th2, w1, w2}
        const double dl = y[0] - y[1], sd = sin(dl), cd = cos(dl);
        const double den = 3.0 - cos(2.0 * dl);
        dy[0] = y[2]; dy[1] = y[3];
        dy[2] = (-3.0 * sin(y[0]) - sin(y[0] - 2.0 * y[1]) - 2.0 * sd * (y[3] * y[3] + y[2] * y[2] * cd)) / den;
        dy[3] = (2.0 * sd * (2.0 * y[2] * y[2] + 2.0 * cos(y[0]) + y[3] * y[3] * cd)) / den;
    }
    inline void substep() {
        double y[4] = { th1, th2, w1, w2 }, k1[4], k2[4], k3[4], k4[4], t[4];
        deriv(y, k1); for (int i = 0; i < 4; i++) t[i] = y[i] + 0.5 * dt * k1[i];
        deriv(t, k2);  for (int i = 0; i < 4; i++) t[i] = y[i] + 0.5 * dt * k2[i];
        deriv(t, k3);  for (int i = 0; i < 4; i++) t[i] = y[i] + dt * k3[i];
        deriv(t, k4);
        th1 = y[0] + dt / 6.0 * (k1[0] + 2.0 * k2[0] + 2.0 * k3[0] + k4[0]);
        th2 = y[1] + dt / 6.0 * (k1[1] + 2.0 * k2[1] + 2.0 * k3[1] + k4[1]);
        w1  = y[2] + dt / 6.0 * (k1[2] + 2.0 * k2[2] + 2.0 * k3[2] + k4[2]);
        w2  = y[3] + dt / 6.0 * (k1[3] + 2.0 * k2[3] + 2.0 * k3[3] + k4[3]);
        if (th1 > PI) th1 -= 2.0 * PI; else if (th1 < -PI) th1 += 2.0 * PI;
        if (th2 > PI) th2 -= 2.0 * PI; else if (th2 < -PI) th2 += 2.0 * PI;
    }
    // One control step of `n` sub-steps, then the projection: returns the velocity rescale applied (1 = none).
    inline double step(int n, bool project = true) {
        for (int i = 0; i < n; i++) substep();
        if (!project) return 1.0;
        const double T = kinetic(), V = potential(), want = e_set - V;
        if (T <= 1e-12 || want <= 0.0) return 1.0;                // at a turning point (or below the reachable energy): leave it
        const double r = sqrt(want / T);
        w1 *= r; w2 *= r;
        return r;
    }
    double kinetic() const { return w1 * w1 + 0.5 * w2 * w2 + w1 * w2 * cos(th1 - th2); }
    double potential() const { return -2.0 * cos(th1) - cos(th2); }
    double out() const { return sin(th2); }
    double energy() const { return kinetic() + potential(); }
};

// ---------------------------------------------------------------------------
// Step 58b — the bore & the jet (SYNTH §2.11, §2.13).
// ---------------------------------------------------------------------------
constexpr int BORE_NODES_MAX = 256;
constexpr int JET_DELAY_MAX = 4096;               // τ up to 43 ms at 96 kHz — A0's period and a half (a power of two: the ring's mask)

// THE ACOUSTIC BORE (§2.11): Webster's horn system on a staggered leapfrog
// (Yee) grid — the §2.8 chain wearing acoustic variables. p lives at the
// integer nodes 0..n, u (the volume velocity) at the half nodes 0..n−1, both
// in the units where the bore's characteristic impedance at its BLOWN end is 1
// (node 0: the reed's, the lips', the flute's embouchure; the cone's apex);
// S(x) is a per-node weight normalized there, so a mouth pressure is in the
// units the valve feels.
// Per sub-step: u_{i+½} −= λ·S_{i+½}·(p_{i+1} − p_i), then p_i −= λ·(u_{i+½}
// − u_{i−½})/S_i — symplectic Euler on the wave equation; λ = c·dt/dx is
// THE COURANT NUMBER and the CFL bound is λ²·μ_max(L_S) < 4 (μ_max of the
// weighted Laplacian (1/S_i)Δ(S_{i+½}Δ·), 4 on a uniform grid — λ ≤ 1 — a
// little over on a cone's apex cells: measured by power iteration and gated
// at patch load, the lab's forced λ its red control). PITCH IS BORE LENGTH:
// a note takes n = ⌊λ_max·rate/(2f₀)⌋ cells (open–open or the cone; half as
// many closed–open) and λ = 2f₀·n/rate ≤ λ_max absorbs the fraction (the
// KS-delay tuning move), so dispersion stays under a cell's worth. The ENDS
// are declared ports: closed (u = 0, the mirror), or open with a radiation
// loss — p = ∓z·u at the end node, z ≥ 0 the declared conformal boundary
// where the sound leaves (z = 0 the ideal pinned end). Profiles: cylinder,
// cone S ∝ (x₀ + x)² (closed at the truncated apex: the saxophone's full
// series), the Bessel flare S ∝ (1 − x/x₁)^{−γ} (trumpet, 58c).
enum BoreProfile { BORE_CYLINDER = 0, BORE_CONE = 1, BORE_BESSEL = 2, BORE_TRUMPET = 3 };
// The cone's mouthpiece as a fraction of the whole bore (apex/3 of the cone's length), and the cone's effective
// length beyond its cells as a fraction of them (the complete cone from the apex, less the mouthpiece it replaces):
// the series counts n·(1 + cone_extra) cells.
inline float cone_mouthpiece(float apex) { const float l = apex / 3.0f; return l / (1.0f + l); }
inline float cone_extra(float apex) { const float m = cone_mouthpiece(apex); return (1.0f - m) * (1.0f + apex) - 1.0f; }   // trumpet: a cylinder, then the flare from `apex` (its start, 0..1) with exponent `gamma`
enum BoreEnd { END_CLOSED = 0, END_OPEN = 1 };
struct Bore {
    float p[BORE_NODES_MAX + 1], u[BORE_NODES_MAX], pprev[BORE_NODES_MAX + 1];   // pprev: the pressures a step ago (the staggered energy)
    float sp[BORE_NODES_MAX + 1], su[BORE_NODES_MAX];   // S at the pressure nodes and at the velocity nodes
    float isp[BORE_NODES_MAX + 1];                      // 1/S at the pressure nodes (the step multiplies: a division per node was a third of the bore's cost)
    int   n;                                            // cells: n + 1 pressure nodes, n velocity nodes
    float lam;
    int   end0, end1; float z0, z1;                     // the ends and their radiation losses
    float rad_a, lp0, lp1;                              // the radiation's frequency dependence: a one-pole on u at each open end (positive-real)
    void reset() { for (int i = 0; i <= BORE_NODES_MAX; i++) p[i] = pprev[i] = 0.0f; for (int i = 0; i < BORE_NODES_MAX; i++) u[i] = 0.0f; lp0 = lp1 = 0.0f; }
    static float profile_s(int profile, float x /*0..1 from node 0 to the mouth*/, float apex, float gamma) {
        if (profile == BORE_CONE) {                                                   // the sax: a mouthpiece cavity, then the cone — normalized at the reed end
            // THE MOUTHPIECE replaces the missing apex: a cylinder of the truncation's area and a third of the cut cone's
            // length holds the same volume, and the series comes back to the complete cone's integers (the bare
            // truncation read +36 cents at the fourth peak; the mouth is ((1 + apex)/apex)² wider — a tenor's 10:1 at 0.1)
            const float m = cone_mouthpiece(apex);
            if (x <= m) return 1.0f;
            const float r = (apex + (x - m) / (1.0f - m)) / apex; return r * r;
        }
        if (profile == BORE_BESSEL) { const float x1 = 1.0f + apex; const float r0 = powf(1.0f - 1.0f / x1, gamma); return powf(1.0f - x / x1, -gamma) * r0; }   // normalized to 1 at the mouth
        if (profile == BORE_TRUMPET) {                                                // S = 1 along the tube, the flare S = (1 − (x − xb)/((1 − xb)(1 + ε)))^{−γ} to the bell's mouth
            const float xb = apex, eps = 0.1f;
            if (x <= xb || xb >= 1.0f) return 1.0f;
            const float t = (x - xb) / ((1.0f - xb) * (1.0f + eps));
            return powf(1.0f - t, -gamma);
        }
        return 1.0f;
    }
    void setup(int cells, int profile, float apex, float gamma, int e0, float rad0, int e1, float rad1) {
        n = cells < 2 ? 2 : (cells > BORE_NODES_MAX ? BORE_NODES_MAX : cells);
        for (int i = 0; i <= n; i++) sp[i] = profile_s(profile, (float)i / (float)n, apex, gamma);
        for (int i = 0; i < n; i++) su[i] = profile_s(profile, ((float)i + 0.5f) / (float)n, apex, gamma);
        for (int i = 0; i <= n; i++) isp[i] = 1.0f / sp[i];
        end0 = e0; end1 = e1; z0 = rad0; z1 = rad1; lam = 1.0f; rad_a = 0.0f; lp0 = lp1 = 0.0f;
        reset();
    }
    // The open ends' radiation resistance grows with frequency (an unflanged pipe's ∝ (ka)²): p_end = ∓z·(u − lp(u))
    // with lp a one-pole at `hz` — the response z·iω/(ω_c + iω) has a non-negative real part at every frequency, so
    // the port only ever absorbs; the fundamental sees little of z, the upper modes all of it (the bore's own selectivity).
    void radiation_corner(float hz, float rate) { rad_a = hz > 0.0f ? 1.0f - expf(-2.0f * PI * hz / rate) : 0.0f; }   // 0: the loss flat in frequency
    // A glide: the cell count and λ change, the wave keeps circulating (the cells past the new end are dropped, new ones start at rest).
    void retune(int cells, float lam_, int profile, float apex, float gamma) {
        const int m = cells < 2 ? 2 : (cells > BORE_NODES_MAX ? BORE_NODES_MAX : cells);
        if (m != n) {
            for (int i = n; i <= m; i++) { p[i] = 0.0f; pprev[i] = 0.0f; }
            for (int i = n; i < m; i++) u[i] = 0.0f;
            n = m;
            for (int i = 0; i <= n; i++) sp[i] = profile_s(profile, (float)i / (float)n, apex, gamma);
            for (int i = 0; i < n; i++) su[i] = profile_s(profile, ((float)i + 0.5f) / (float)n, apex, gamma);
            for (int i = 0; i <= n; i++) isp[i] = 1.0f / sp[i];
        }
        lam = lam_;
    }
    // μ_max of the weighted Laplacian on the pressure nodes, with the ends as set (the mirror doubles a closed end's cell).
    float mu_max() const {
        float v[BORE_NODES_MAX + 1], w[BORE_NODES_MAX + 1];
        for (int i = 0; i <= n; i++) v[i] = 1.0f + 0.01f * (float)(i % 7);
        float mu = 0.0f;
        for (int it = 0; it < 300; it++) {
            for (int i = 0; i <= n; i++) {
                float acc = 0.0f;
                if (i > 0) acc += su[i - 1] * (v[i] - v[i - 1]);
                if (i < n) acc += su[i] * (v[i] - v[i + 1]);
                if (i == 0 && end0 == END_CLOSED) acc *= 2.0f;                 // the mirror: a half cell
                if (i == n && end1 == END_CLOSED) acc *= 2.0f;
                if (i == 0 && end0 == END_OPEN) acc = 0.0f;                    // pinned: not a degree of freedom
                if (i == n && end1 == END_OPEN) acc = 0.0f;
                w[i] = acc / sp[i];
            }
            float norm = 0.0f; for (int i = 0; i <= n; i++) norm += w[i] * w[i];
            norm = sqrtf(norm); if (norm <= 0.0f) return 0.0f;
            mu = norm; for (int i = 0; i <= n; i++) v[i] = w[i] / norm;
        }
        return mu;
    }
    // λ²·μ < 4, and never past the interior scheme's own limit of 1: a pinned end removes a degree of freedom and the
    // power iteration then reads μ_max a hair UNDER 4 (3.9965 open–open), whose 2/√μ of 1.0004 let F#4's derived λ reach
    // 1.00026 — and the bore blew up in 439 sub-steps while every neighbour rang.
    static float lambda_bound(float mu) { const float b = mu > 0.0f ? 2.0f / sqrtf(mu) : 1.0f; return b < 1.0f ? b : 1.0f; }
    // The cells for a note at λ_max, and the λ that lands the pitch exactly: the round trip of the bore is 2n/λ
    // sub-steps for an open–open bore or a cone (f₀ = c/2L), 4n/λ for a closed–open cylinder (f₀ = c/4L).
    // THE END CORRECTION of an open end with the radiation port above: its reactance is an inertance
    // z·ω_c/(ω_c² + ω²) (ω in radians per sub-step) — in sub-steps of travel, so λ·that many cells of extra
    // bore: 2.8 cells per end at A4 with z 0.3 and a 1500 Hz corner (λ ≈ 1), which read 81 cents flat before it
    // was counted. Returned per unit λ; the tuning below carries the λ.
    static float end_correction(float z, float rad_a, float hz, float rate, float s_end = 1.0f) {   // s_end: the end's S (the port is z/S_end)
        if (z <= 0.0f || rad_a <= 0.0f) return 0.0f;
        const float wc = -logf(1.0f - rad_a), w = 2.0f * PI * hz / rate;
        return (z / (s_end > 1e-6f ? s_end : 1e-6f)) * wc / (wc * wc + w * w);
    }
    // `extra` is the bore's length beyond its cells as a fraction of them (a cone's truncated apex: its series
    // counts from the apex, so a cone of n cells and apex 0.05 sounds as 1.05·n); `k_ends` the end corrections
    // per unit λ (both open ends' sum). The bore's round trip is (2/λ)·(n·(1 + extra) + λ·k_ends) sub-steps
    // (open–open or the cone), twice that for a closed–open cylinder — so λ = c·f₀·n(1 + extra)/(rate − c·f₀·k).
    static int cells_for(float hz, float rate, float lam_max, bool quarter_wave, float extra, float k_ends, int cap) {
        const float c = lam_max * (rate / ((quarter_wave ? 4.0f : 2.0f) * hz) - k_ends) / (1.0f + extra);
        int m = (int)floorf(c); if (m > cap) m = cap; return m;
    }
    static float lambda_for(float hz, float rate, int cells, bool quarter_wave, float extra, float k_ends) {
        const float c = (quarter_wave ? 4.0f : 2.0f) * hz;
        return c * (float)cells * (1.0f + extra) / (rate - c * k_ends);
    }
    // One sub-step with the two ports at node 0: q_b the volume velocity THROUGH a closed end (the valve's aperture
    // flow, 58c: the mirror's half cell fills from it, p[0] −= 2λ(u_½ − q_b)/S₀) and p_src a pressure source at an
    // open end (the jet drive's dipole across the labium, ∝ dQ_in/dt — a flow alone does no work at a pinned open
    // end); damp the declared per-node contraction (1 = none); shear the bell's brassiness (58c), a phase-space
    // shear in the last cell's (p, u) — p −= shear·u³, det 1, amplitude-driven, declared. Returns the volume
    // velocity at the mouth end — the standing wave's amplitude there (the port's own pressure falls as f² toward
    // the bass; a far-field microphone would add a derivative).
    // The end pressure the valve will see after this step, before it is taken: p[0] − 2λ·u_½'/S₀ with u_½' the first
    // velocity node's update — so the implicit junction can be solved from the state before the step.
    inline float end_pressure_ahead() const { return end0 == END_CLOSED ? p[0] - 2.0f * lam * (u[0] - lam * su[0] * (p[1] - p[0])) / sp[0] : p[0]; }
    inline float end_impedance() const { return end0 == END_CLOSED ? 2.0f * lam / sp[0] : 0.0f; }   // ∂p[0]/∂q_b within the step
    inline float step(float q_b, float p_src, float damp, float shear = 0.0f) {
        // the two sweeps, each fused with its damping (the same operations in the same order as the four loops they
        // replace); the pressures a step ago are kept for the staggered energy
        if (damp != 1.0f) for (int i = 0; i < n; i++) u[i] = (u[i] - lam * su[i] * (p[i + 1] - p[i])) * damp;
        else              for (int i = 0; i < n; i++) u[i] -= lam * su[i] * (p[i + 1] - p[i]);
        for (int i = 1; i < n; i++) { const float pi = p[i]; pprev[i] = pi; p[i] = pi - lam * (u[i] - u[i - 1]) * isp[i]; }
        pprev[0] = p[0]; pprev[n] = p[n];
        if (shear != 0.0f) { const float w = u[n - 1]; p[n - 1] -= shear * w * w * w; }
        // the radiation ports in the END's own characteristic impedance, z/S_end — a bell five times the tube's area
        // has a fifth of its impedance, and a port scaled in the tube's units chattered there (the flare's true
        // Courant limit read 0.927 against the interior's 1 until this)
        if (end0 == END_CLOSED) p[0] -= 2.0f * lam * (u[0] - q_b) / sp[0]; else { lp0 += rad_a * (u[0] - lp0); p[0] = -(z0 / sp[0]) * (u[0] - lp0) + p_src; }
        if (end1 == END_CLOSED) p[n] += 2.0f * lam * u[n - 1] / sp[n]; else { lp1 += rad_a * (u[n - 1] - lp1); p[n] = (z1 / sp[n]) * (u[n - 1] - lp1); }
        if (damp != 1.0f) for (int i = 0; i <= n; i++) p[i] *= damp;
        return u[n - 1];
    }
    // The stored energy in the STAGGERED form the leapfrog conserves exactly: ½Σ S_i·p_n·p_{n+1} (the end nodes
    // half cells) + ½Σ u²_{n+½}/S_{i+½} — the pressures either side of u's time level (the symmetric p² form
    // wobbles by a quarter of a decibel at λ ≈ 1). In the units above, times dx/c.
    float energy() const {
        double e = 0.0;
        for (int i = 0; i <= n; i++) { const double wgt = (i == 0 || i == n) ? 0.5 : 1.0; e += 0.5 * wgt * (double)sp[i] * p[i] * pprev[i]; }
        for (int i = 0; i < n; i++) e += 0.5 * (double)u[i] * u[i] / su[i];
        return (float)e;
    }
};

// THE JET (§2.13): the flute's exciter, no moving parts. The acoustic
// velocity at the embouchure, delayed by the jet's travel time τ =
// d/(α·U₀) — a short transport, Hermite-interpolated since breath retunes τ
// every block — and amplified (e^{μd}, the jet's instability gain) is the
// jet's displacement at the labium; the labium partitions the jet's flow,
// Q_in = (Q₀/2)·(1 − tanh((η − y₀)/b)), and that flow is the bore's boundary
// port. Overblowing is not programmed: raising P_mouth raises U₀ and
// shortens τ until the phase condition breaks and the loop bifurcates to
// the next mode. Breath noise is a stochastic vector at the labium, the bore
// filters it into chiff. Class: SELF-EXCITED, power-limited — Q_in is
// bounded by Q₀ and the bore is passive, so the stored energy never
// exceeds the mouth's work ∫P_mouth·Q_in (§5's mouth-power ledger).
// THE JET'S RECEPTIVITY is a band: the sinuous instability grows fastest at
// one Strouhal number, f·d/U₀ — with the delay τ = d/(αU₀) that is f·τ, so
// the jet amplifies best around f_j = St₀/τ, and raising the breath moves
// the band up the bore's modes. A second-order bandpass (the Chamberlin
// core, Q from the patch) on the displacement, centred at ½/τ: at the
// reference breath the band sits on the fundamental; at four times it, on
// the octave — the register jump is the band crossing the modes, and a
// full-period delay (the very soft end) no longer locks every frequency at
// once (measured without it: a broadband saturation at τ = T).
struct Jet {
    float    ring[JET_DELAY_MAX];
    int      w;
    uint32_t rng;
    float    xi, q_prev;              // the acoustic displacement at the flue (a leaky integral of the velocity), the last flow
    float    eta_last;                // step 59c: the last η at the labium (the lab's trace channel; nothing reads it in the step)
    Svf      bp;                      // the receptivity band
    void reset() { for (int i = 0; i < JET_DELAY_MAX; i++) ring[i] = 0.0f; w = 0; rng = 0x9E3779B9u; xi = 0.0f; q_prev = 0.0f; eta_last = 0.0f; bp.reset(); }
    inline float noise() { rng = rng * 1664525u + 1013904223u; return ((float)(rng >> 8) * (1.0f / 8388608.0f)) - 1.0f; }   // white in [−1, 1)
    inline float read(float tau) const {                   // the sample τ behind the write, 4-point Hermite
        if (tau < 1.0f) tau = 1.0f; if (tau > (float)(JET_DELAY_MAX - 3)) tau = (float)(JET_DELAY_MAX - 3);
        const int it = (int)tau; const float t = tau - (float)it;
        const int i1 = (w - 1 - it + 2 * JET_DELAY_MAX) & (JET_DELAY_MAX - 1);
        const float y0 = ring[(i1 + 1) & (JET_DELAY_MAX - 1)], y1 = ring[i1], y2 = ring[(i1 - 1) & (JET_DELAY_MAX - 1)], y3 = ring[(i1 - 2) & (JET_DELAY_MAX - 1)];
        const float c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + c0;
    }
    // One sub-step: v_ac the bore's velocity at the embouchure (inward positive). The jet leaving the flue is
    // carried by the acoustic DISPLACEMENT there (the leaky integral of v_ac, DC-blocked at `leak` per step),
    // reaches the labium τ later, amplified: η > 0 is the jet OUTSIDE the labium (less flow in). Returns the
    // partitioned flow Q_in; the drive is its rate — the caller's pressure port, K·(Q − Q_prev)·T/2π.
    // `band_f` is the receptivity band's SVF coefficient (2·sin(π·f_j/rate) with f_j = ½/τ), `band_q` its damping (1 = Q 1).
    inline float step(float v_ac, float U0, float tau, float gain, float y0, float q_area, float sigma, float leak, float band_f, float band_q) {
        xi = xi * leak + v_ac;
        bp.step(xi, band_f, band_q);
        ring[w] = bp.band; w = (w + 1) & (JET_DELAY_MAX - 1);
        const float eta = -gain * read(tau) + sigma * U0 * noise();   // an inward displacement carries the jet IN (η < 0: more flow in)
        eta_last = eta;
        return 0.5f * q_area * U0 * (1.0f - tanhf(eta - y0));
    }
};

// ---------------------------------------------------------------------------
// Step 58c — the reed & the lips (SYNTH §2.12).
// ---------------------------------------------------------------------------
// THE VALVE: a 1-DOF mass–spring with conformal damping — the cell's kick
// and drift with a force, v −= ω²·y, v += f, y += v, v·d — in the sub-step's
// units (ω = 2πf_v/rate, stable under 2). y is the aperture's change from
// its rest opening h₀: an INWARD-striking reed closes with the mouth's
// pressure (f = −g·Δp), OUTWARD-striking lips open with it (f = +g·Δp); the
// compliance g is set by the pressure that closes the reed, p_M = h₀·ω²/g.
// The flow is Bernoulli's through the aperture, Q = A·[h]⁺·√(2|Δp|)·sgn(Δp),
// [h]⁺ the beating reed against its lay (the flow stops, the mass carries
// on). THE JUNCTION, the hard 20 % (§2.12): Δp = P_mouth − p_end, and the
// bore's end pressure answers the flow within the SAME sub-step, p_end =
// p_ahead + Z·Q (Z = 2λ/S₀, the closed end's half cell) — solved IMPLICITLY,
// a quadratic in Q: with a = A·h, Q = [−a²Z + √(a⁴Z² + 4a²|Δp_ahead|)]/2 in
// the sign of Δp_ahead. The valve's own motion sweeps volume into the bore
// too — ±S_r·v with S_r its area in the flow's units (a twentieth of the
// aperture's scale; the reed's mass is then S_r/g, g the force gain), the
// sign the valve's: a closing reed moves into the mouthpiece and pushes air
// in (−S_r·v), opening lips swing into the cup and do the same (+S_r·v) —
// and that flow rides along with Q: the bore receives p_end·(Q + swept), the
// valve (P_mouth − p_end)·swept, the aperture loses Δp·Q ≥ 0, and the sum is
// P_mouth·(Q + swept) − Δp·Q ≤ the mouth's work EXACTLY per sample (with the
// reed's swept share signed as the lips' the probe read 1.1× the mouth's
// work: the reed drew on the bore's pressure and the bore was never charged) — §5's
// mouth-power ledger is arithmetic (without the swept share the valve draws
// on the bore's pressure without the bore paying; with the swept share scaled
// by g instead of an area it pushed more air than the aperture passed and
// the probe's reed went to infinity in twenty milliseconds). The NAIVE variant reads p_end
// from the step before (no Z): its local loop gain Z·A·h/(2√Δp) passes 1 at
// soft pressures and the loop chatters or blows — the probe's red.
struct Valve {
    float y, v, y_prev;               // the aperture's change, its velocity per sub-step, the change a step ago (the staggered energy)
    float w, damp, g, h0, A;          // ω, the damping per sub-step, the force gain, the rest opening, the flow factor (A·√2 folded)
    float Sr;                         // the valve's area in the flow's units: its swept volume into the bore is ±Sr·v (the force's sign), its mass Sr/g
    inline float swept() const { return (outward ? Sr : -Sr) * v; }
    int   outward;                    // 0 the reed (closes with Δp), 1 the lips (open with it)
    float q;                          // the last flow
    uint32_t rng;                     // the breath's turbulence: the perturbation the static state grows from (a reed at rest is a fixed point)
    float nz, nk;                     // the turbulence low-passed (a one-pole at noise_hz; the reed's 2 kHz — white noise at the sub-rate seeded its upper registers; the lips' white)
    void reset() { y = v = y_prev = 0.0f; q = 0.0f; rng = 0x2545F491u; nz = 0.0f; }
    inline float noise_raw() { rng = rng * 1664525u + 1013904223u; return ((float)(rng >> 8) * (1.0f / 8388608.0f)) - 1.0f; }
    inline float noise() { nz += (noise_raw() - nz) * nk; return nz; }
    void setup(float hz, float rate, float Q, float rest_opening, float close_pressure, float flow_area, int outward_, float noise_hz) {
        w = 2.0f * PI * hz / rate; if (w > 1.9f) w = 1.9f;
        damp = expf(-w / (2.0f * (Q > 0.1f ? Q : 0.1f)));
        h0 = rest_opening; g = close_pressure > 1e-9f ? h0 * w * w / close_pressure : 0.0f;
        A = flow_area * 1.41421356f; outward = outward_; Sr = 0.05f;
        nk = noise_hz > 0.0f ? 1.0f - expf(-2.0f * PI * noise_hz / rate) : 1.0f;   // 0: white
    }
    inline float opening() const { const float h = h0 + y; return h > 0.0f ? h : 0.0f; }   // the force's sign sets the direction: the reed's y goes negative (closing), the lips' positive (opening)
    // The implicit Bernoulli flow: dp_ahead = P_mouth − the bore's end pressure before the flow, Z its impedance.
    static inline float flow_implicit(float a, float dp_ahead, float Z) {
        if (a <= 0.0f) return 0.0f;
        const float m = fabsf(dp_ahead); if (m < 1e-20f) return 0.0f;
        const float a2 = a * a;
        const float root = sqrtf(a2 * a2 * Z * Z + 4.0f * a2 * m);
        const float mag = 0.5f * (-a2 * Z + root);
        return dp_ahead > 0.0f ? mag : -mag;
    }
    static inline float flow_naive(float a, float dp) { return a <= 0.0f ? 0.0f : (dp >= 0.0f ? a * sqrtf(dp) : -a * sqrtf(-dp)); }
    // The mass–spring's sub-step under the pressure difference it feels after the flow.
    inline void step(float dp) {
        v -= w * w * y;
        v += (outward ? g : -g) * dp;
        y_prev = y;
        y += v;
        v *= damp;
        // THE STOP (the reed's): the reed meets the lay and rests closed at h = 0 instead of swinging on through
        // into a long closed phase clocked by its own resonance (which left the first register unstable above C4:
        // the note flipped to the octave or wandered). The collision is inelastic: the velocity dies, the potential
        // falls — a loss, so the ledger only gains margin. The lips keep their swing-through: they meet each other
        // softly, and the closed half-cycle is part of the lip oscillator the trumpet's registers were tuned on.
        if (!outward && y < -h0) { y = -h0; if (v < 0.0f) v = 0.0f; }
    }
    // The energy in the form the kick–drift conserves exactly, ½v² + ½ω²·y_n·y_{n+1} (a unit mass; the symmetric
    // form wobbles by ω at the reed's own frequency and tripped the probe's first check).
    float energy() const { return (g > 1e-12f ? Sr / g : 0.0f) * 0.5f * (v * v + w * w * y * y_prev); }   // the mass Sr/g
};

// THE BORE'S PEAKS, measured (58c): the trumpet plays a partial of its bore,
// and a cylinder with a flare and a radiating bell has no formula for where
// its impedance peaks sit — so at patch load a reference bore of the profile
// is blown with a pulse at its closed end and the peaks of its end pressure
// found (a Goertzel scan with parabolic tops), in units of the quarter-wave
// fundamental c/(4L): a closed–open cylinder reads 1, 3, 5 …; the flare pulls
// the low peaks up toward a harmonic series. The note's bore is then cut so
// its m-th peak sits on the note.
inline int bore_peaks(int profile, float apex, float gamma, float z, float corner_hz, float rate, int count, float* ratio_out) {
    Bore* b = new Bore;
    const int N = 200;
    b->setup(N, profile, apex, gamma, END_CLOSED, 0.0f, END_OPEN, z); b->radiation_corner(corner_hz, rate);
    b->lam = Bore::lambda_bound(b->mu_max()) * 0.999f;
    const float fq = b->lam * rate / (4.0f * (float)N);                   // the quarter-wave fundamental of this bore
    const long T = (long)(0.6f * rate);
    float* rec = new float[T];
    for (long i = 0; i < T; i++) { const float pulse = i < 8 ? 0.02f * (1.0f - cosf(2.0f * PI * (float)i / 8.0f)) : 0.0f; b->step(pulse, 0.0f, 1.0f); rec[i] = b->p[0]; }
    // the scan: 0.5·fq … (2·count + 2)·fq in 2 % steps (a parabola tops each peak), ~40 ms at patch load
    const float f_lo = 0.5f * fq, f_hi = (2.0f * (float)count + 2.0f) * fq;
    const int bins = (int)(logf(f_hi / f_lo) / logf(1.02f)) + 1;
    float* mag = new float[bins]; float* frq = new float[bins];
    float best = 0.0f;
    for (int k = 0; k < bins; k++) {
        const float f = f_lo * powf(1.02f, (float)k); frq[k] = f;
        const double wv = 2.0 * PI * f / rate, cw = 2.0 * cos(wv); double s0 = 0.0, s1 = 0.0, s2 = 0.0;
        for (long i = 0; i < T; i++) { const double win = 0.5 - 0.5 * cos(2.0 * PI * (double)i / (double)(T - 1)); s0 = rec[i] * win + cw * s1 - s2; s2 = s1; s1 = s0; }
        const double re = s1 - s2 * cos(wv), im = s2 * sin(wv);
        mag[k] = (float)sqrt(re * re + im * im); if (mag[k] > best) best = mag[k];
    }
    // the peaks: local maxima over a five-hundredth of the largest (the radiation loss shrinks the upper peaks
    // as 1/f²), each at least three times the valley on either side (a prominence, not a ripple)
    int found = 0;
    for (int k = 1; k + 1 < bins && found < count; k++) {
        if (!(mag[k] > 0.002f * best && mag[k] >= mag[k - 1] && mag[k] >= mag[k + 1])) continue;
        float vl = mag[k], vr = mag[k];
        for (int j = k - 1; j >= 0 && mag[j] <= mag[j + 1]; j--) vl = mag[j];
        for (int j = k + 1; j < bins && mag[j] <= mag[j - 1]; j++) vr = mag[j];
        if (mag[k] < 3.0f * vl || mag[k] < 3.0f * vr) continue;
        const float a = mag[k - 1], bm = mag[k], c = mag[k + 1], den = a - 2.0f * bm + c;
        const float off = den != 0.0f ? 0.5f * (a - c) / den : 0.0f;
        ratio_out[found++] = frq[k] * powf(1.02f, off) / fq;
    }
    delete[] mag; delete[] frq; delete[] rec; delete b;
    return found;
}

} // namespace suzu
