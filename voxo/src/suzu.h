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
//   Membrane::step (step 79)         SYMPLECTIC (kick then     TO_PROJECT_SPEC §13.9: the 2D
//                                    drift, every force from   leapfrog grid, the biharmonic
//                                    the pre-update u; the     through the simply-supported
//                                    gong's shear reads u      ring; the CFL gate λ_2D ≤ 1.95
//                                    alone) + the DECLARED     evaluated at the cap's E_max
//                                    contraction γ₀ + γ₂Δ
//   Membrane::strike / inject        DRIVE (a bump of          §13.9: the strike capped at H_cap
//                                    momentum; a sample into   (the plain energy never exceeds
//                                    the bus plate's node)     it — E_max follows)
//   StickSlip::solve (step 80)       SELF-EXCITED, bounded by  TO_PROJECT_SPEC §13.10: the hyperbolic
//                                    construction (the force   friction curve solved implicitly
//                                    never carries the string  against the point's mobility; the
//                                    past the bow: the gain    energy the string takes ≤ the bow's
//                                    f·v_string ≤ f·v_bow)     work f·v_bow, by arithmetic
//   StickSlip::solve_naive           the Newton steps taken    the ledger's red control (the
//                                    raw, no bound — TEST ONLY string overshoots the bow)
//
// Nothing here allocates, locks or logs: every struct is POD and lives inside
// a Voice (voxo.cpp); the callback contract (voxo.h) holds. The nonlinear
// stages and the filter run inside a 2× oversampled section (§2.2, §2.4):
// the voice steps its cells twice per output sample and averages the pair.
#pragma once

#include <stdint.h>
#include <string.h>
#include <math.h>
#include "simd.h"       // step 78: four float lanes behind one header
#include "halfband.h"   // step 78: the ×4 oversampler's half-band (generated by tools/halfband_design.py)

namespace suzu {

constexpr float PI = 3.14159265358979f;

// Exact tuning (§2.1): the leapfrog rotates by θ = 2π f / fs per step when
// ε = 2 sin(θ / 2) — cents-true across the keyboard, not the small-angle ε ≈ θ.
inline float eps_for(float hz, float rate) {
    float x = PI * hz / rate;
    if (x > 1.5f) x = 1.5f;   // past Nyquist the recurrence is meaningless; keep it bounded
    return 2.0f * sinf(x);
}
// Step 78 (TO_PROJECT_SPEC §13.8): THE PREWARP as a shared helper — the magic circle's ε = 2 sin(ω·dt/2) for an angular
// frequency and a step, the form the Boris gyroscope's spring and field take (step 84: B_num = (2/dt)·tan(ω_B·dt/2) is the
// companion). eps_for keeps its own arithmetic (π·hz/rate) so every shipped ε stays bit for bit; the two agree to float
// rounding (the suite's gate).
inline float prewarp(float omega, float dt) {
    float x = 0.5f * omega * dt;
    if (x > 1.5f) x = 1.5f;
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
    // Step 86 (found: a cutoff under the ceiling at zero resonance blew every voice since step 56): the compensation asks for
    // a ringing frequency, and an overdamped filter has none — the solve then runs past stability (q 2 at 4 kHz, 96 kHz:
    // f 3.6 against the plain 0.26; nan in the SVF, the breaker). The ring exists while the update's eigenvalues are complex,
    // |cos θ| < 1 with fq < 1; outside it the cutoff is the cutoff: the undamped f.
    const double fq = f * (double)q;
    if (!(fq < 1.0) || !(f < 1.9)) return f0;
    const double c = (2.0 - fq - f * f) / (2.0 * sqrt(1.0 - fq));
    if (!(c > -1.0 && c < 1.0)) return f0;
    return (float)f;
}

// The declared contraction for a release: the amplitude falls 60 dB over
// `seconds` at `rate` steps per second (a T60, as a room's).
inline float contraction_for(float seconds, float rate) {
    if (seconds <= 0.0f) return 0.0f;
    return expf(-6.907755f / (seconds * rate));   // ln(1000) = 6.9078
}

// Step 78 (TO_PROJECT_SPEC §13.8): THE HALF-BAND DECIMATOR, 2:1 — one stage of the ×4 oversampler (two in cascade bring
// the cell family's section from four sub-steps a sample to one). A Kaiser-windowed half-band FIR (halfband.h: 47 taps,
// the centre at delay 2·SIDE on an even-phase sample, the odd taps around it; every other even tap zero, so an output costs
// SIDE multiplies and 2·SIDE adds); push one input a call, out() after every second push is the decimated sample. The
// stopband [0.30, 0.50] of the input rate lies 64 dB down, so the input's top band folds under the output's 0.20 at that
// attenuation — the alias gate's band. The ×2 section keeps its two-tap average (the shipped sound, bit for bit).
struct HalfBand {
    float z[64]; int w;
    void reset() { for (int i = 0; i < 64; i++) z[i] = 0.0f; w = 0; }
    inline void push(float x) { z[w & 63] = x; w++; }
    inline float out() const {
        const int c = w - 1 - 2 * SUZU_HALFBAND_SIDE;
        float y = SUZU_HALFBAND_CENTRE * z[c & 63];
        for (int k = 0; k < SUZU_HALFBAND_SIDE; k++) { const int d = 2 * k + 1; y += SUZU_HALFBAND_ODD[k] * (z[(c - d) & 63] + z[(c + d) & 63]); }
        return y;
    }
};

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
// Step 78 (TO_PROJECT_SPEC §13.8, DECISIONS_12 #1): THE LATTICE IN FOUR LANES — the scalar sub-step's operations in the
// scalar's order per mode: (1) the coupling kick from the pre-update positions (lx_k = (x_k − x_{k−1}) + (x_k − x_{k+1}),
// each term present as the scalar had it, y += (κε₀²/ε_k)·lx), (2) the rotation, sheared or plain (x −= ε(y + s),
// y += εx; s = (g·A)·shape(y/A)), four cells at a time through de-interleaved loads of the (x, y, ε, A) structs; the
// contraction (3) is the second pass once the caller has the per-mode factors (the bow's share is computed between the
// two, as the scalar did), and the sums are the caller's, in scalar order. A muted mode (ε = 0, the state 0) comes out
// as it went in, bit for bit, as the scalar's skip left it. The fused products follow the scalar compiler's contraction
// (simd.h); the reference build's are plain.
inline void lattice_rotate_simd(Cell* c, const float* inv_eps, int n, float k_eps0sq, float g_shear, bool cubic) {
    float lx[MODES_MAX + 4];
    const bool kick = n > 1 && k_eps0sq > 0.0f;
    if (kick) {
        float xs[MODES_MAX];
        for (int k = 0; k < n; k++) xs[k] = c[k].x;
        for (int k = 0; k < n; k++) {
            float v = 0.0f;
            if (k > 0)     v += xs[k] - xs[k - 1];
            if (k < n - 1) v += xs[k] - xs[k + 1];
            lx[k] = v;
        }
        for (int k = n; k < MODES_MAX + 4; k++) lx[k] = 0.0f;
    }
    const f32x4 g = f4_set1(g_shear), tiny = f4_set1(1e-9f), zero = f4_set1(0.0f), half = f4_set1(0.5f), two = f4_set1(2.0f), neg_half = f4_set1(-0.5f), neg_two = f4_set1(-2.0f), ktot = f4_set1(k_eps0sq);
    for (int k0 = 0; k0 < n; k0 += 4) {
        f32x4x4 q = f4_load_cells(&c[k0].x);
        if (kick) { const f32x4 coef = f4_mul(ktot, f4_load(&inv_eps[k0])); q.y = f4_mul_add(coef, f4_load(&lx[k0]), q.y); }
        f32x4 t = q.y;
        if (g_shear > 0.0f) {
            const f32x4 u = f4_select(f4_gt(q.amp, tiny), f4_div(q.y, q.amp), zero);
            f32x4 sh;
            if (cubic) sh = f4_mul(f4_mul(u, u), u);
            else sh = f4_select(f4_gt(u, half), f4_mul_sub(two, two, u), f4_select(f4_gt(neg_half, u), f4_mul_sub(neg_two, two, u), f4_mul(two, u)));
            t = f4_add(q.y, f4_mul(f4_mul(g, q.amp), sh));
        }
        q.x = f4_mul_sub(q.x, q.eps, t);
        q.y = f4_mul_add(q.eps, q.x, q.y);
        f4_store_cells(&c[k0].x, q);
    }
}
inline void lattice_contract_simd(Cell* c, int n, const float* rr /* MODES_MAX, 1 beyond n */) {
    for (int k0 = 0; k0 < n; k0 += 4) {
        f32x4x4 q = f4_load_cells(&c[k0].x); const f32x4 r = f4_load(&rr[k0]);
        q.x = f4_mul(q.x, r); q.y = f4_mul(q.y, r); q.amp = f4_mul(q.amp, r);
        f4_store_cells(&c[k0].x, q);
    }
}
// the scalar twin of the two above for the suite's comparison (the sub-step as step 57 wrote it, the bow aside)
inline void lattice_step_scalar(Cell* c, const float* eps, const float* inv_eps, int n, float k_eps0sq, float g_shear, bool cubic, const float* rr) {
    if (n > 1 && k_eps0sq > 0.0f) lattice_kick(c, inv_eps, n, k_eps0sq);
    for (int k = 0; k < n; k++) {
        if (eps[k] <= 0.0f) continue;
        Cell& x = c[k];
        if (g_shear > 0.0f) { const float u = x.amp > 1e-9f ? x.y / x.amp : 0.0f; x.step_sheared(g_shear * x.amp * (cubic ? shape_cubic(u) : shape_tri(u))); }
        else x.step();
        if (rr[k] != 1.0f) x.contract(rr[k]);
    }
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
inline float lattice_lambda_max_over_pitch(const float* ratio, int n, float rate2, float kappa, int* worst_midi = nullptr, bool compensated = true) {   // step 82: the raw lattice's bound (no solve: a unison pair has no separate targets)
    float worst = 0.0f; int at = 0;
    float c[MODES_MAX], c2[MODES_MAX], jinv[MODES_MAX * MODES_MAX];
    if (compensated) lattice_compensation(ratio, n, kappa, c, nullptr, jinv);
    else { for (int k = 0; k < n; k++) c[k] = 1.0f; for (int i = 0; i < n * n; i++) jinv[i] = (i % (n + 1)) == 0 ? 1.0f : 0.0f; }
    for (int k = 0; k < n; k++) c2[k] = c[k] * c[k];
    const float eps_c8 = eps_for(4186.009f * ratio[0], rate2);
    for (int midi = 21; midi <= 156; midi++) {
        const float f0 = 440.0f * exp2f((float)(midi - 69) / 12.0f);
        float comp[MODES_MAX], eps[MODES_MAX]; int alive = 0;
        if (compensated) lattice_comp_at_pitch(c2, jinv, ratio, n, f0, rate2, comp); else for (int k = 0; k < n; k++) comp[k] = 1.0f;
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
enum ModalPreset { PRESET_HARMONIC = 0, PRESET_BAR = 1, PRESET_BELL = 2, PRESET_GLASS = 3, PRESET_PLUCKED = 4, PRESET_TABLE = 5 };   // step 82: 5 the patch's own table
struct ModalTable {
    int   n;
    float ratio[MODES_MAX];
    float t60[MODES_MAX];       // seconds, while held
    float kick[MODES_MAX];      // the strike's profile, max 1
    float bow[MODES_MAX];       // the bow's profile, max 1
};
inline void modal_table(ModalTable* t, int preset, int n, float decay_s, float decay_bright, float stiffness, float pluck, float bow_position,
                        const float* ratio_in = nullptr, const float* t60_in = nullptr, const float* weight_in = nullptr) {   // step 82: the table's arrays under PRESET_TABLE
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
            case PRESET_TABLE:   r = ratio_in && ratio_in[k] > 0.0f ? ratio_in[k] : m; w = weight_in && weight_in[k] > 0.0f ? weight_in[k] : 1.0f / m; break;   // step 82: as the patch states them
            default:             r = m * sqrtf(1.0f + stiffness * m * m); w = 1.0f / m; break;                 // the harmonic string
        }
        t->ratio[k] = r;
        t->kick[k] = w;
        const float gamma = alpha + decay_bright * (r * r - 1.0f);                        // γ_k = α + β·(r_k² − 1): highs die first
        t->t60[k] = gamma > 1e-9f ? 6.907755f / gamma : 0.0f;                               // 0 = none declared
        if (preset == PRESET_TABLE && t60_in && t60_in[k] > 0.0f) t->t60[k] = t60_in[k];     // step 82: the table's own decay, where it states one
        // the bow's profile: position 0 feeds the fundamental alone, 1 every mode evenly; between, sin(kπ·pos) like a bow's point
        const float b = bow_position <= 0.0f ? (k == 0 ? 1.0f : 0.0f) : (bow_position >= 1.0f ? 1.0f : fabsf(sinf(m * PI * bow_position)));
        t->bow[k] = b;
    }
    for (int k = 0; k < n; k++) wsum_max = fmaxf(wsum_max, t->kick[k]);
    if (wsum_max > 0.0f) for (int k = 0; k < n; k++) t->kick[k] /= wsum_max;
    if (preset == PRESET_BELL) { t->kick[1] = 1.0f; }                                        // the prime is the loudest, as cast
}


// ---------------------------------------------------------------------------
// Step 83 (TO_PROJECT_SPEC §13.13, DECISIONS_13 #1): THE TODA LATTICE SOLITON RING — voice kind 10.
// ---------------------------------------------------------------------------
// N nodes on a closed ring under H = Σ p²/2 + (a/b)·Σ(e^{−b r_i} + b r_i − 1), r_i = q_{i+1} − q_i (q_N ≡ q_0): the force on
// node i is a·(e^{−b r_{i−1}} − e^{−b r_i}) — for small b·r the linear chain with k = a·b (the ring's modes ω_m = 2√k·sin(mπ/N),
// the lattice's dispersion, a marimba's inharmonicity), for a compression (r < 0) exponentially stiffer: a pulse self-focuses
// into solitons that circulate faster than the sound speed √k and keep their shape. Symplectic Verlet (kick, drift; dt = 1 a
// sub-step at ×4): det J = 1, a modified H preserved — NOT the Lax invariants (the discrete ring is near-integrable: the shape
// gate, not an integrability claim). THE CAP: e^{−b r} ≤ kcap — the exponential's ceiling, a modified potential (linear in r
// past the cap, still conservative) chosen under the leapfrog's bound (the local stiffness k·e^{−b r} < 1); without it a
// fortissimo at a high note runs the local stiffness past the bound in one sub-step (the red). The ring conserves Σp
// (the forces sum to zero around it): a uniform momentum is a Galilean boost — the solitons ride it past the pickup faster
// or slower (the swirl's impulse), a rigid rotation the strain does not see.
constexpr int TODA_NODES_MAX = 64;
// THE RING'S EXPONENTIAL: e^x in double by range reduction (Cody–Waite, x = k·ln2 + f, |f| ≤ ln2/2) and a degree-11 Taylor
// on f (its remainder f¹²/12! < 10⁻¹⁴ relative: double's own precision), the 2^k by the exponent field — plain arithmetic
// the compiler vectorizes across the nodes, the same bits on every platform (libm's exp was the Tab's whole budget:
// a voice a millisecond a burst there, measured at the step; and a different last bit on every libm). The force is then
// the exact gradient of a potential within 10⁻¹⁴ of Toda's, and the leapfrog's modified H stays conserved.
inline double toda_exp(double x) {
    if (x > 700.0) x = 700.0; else if (x < -700.0) x = -700.0;
    const double kd = floor(x * 1.4426950408889634 + 0.5);
    const double f = (x - kd * 0.693147180369123816490) - kd * 1.90821492927058770002e-10;   // ln2 split hi + lo
    double p = 1.0 / 39916800.0;                                                              // 1/11!
    p = p * f + 1.0 / 3628800.0; p = p * f + 1.0 / 362880.0; p = p * f + 1.0 / 40320.0; p = p * f + 1.0 / 5040.0;
    p = p * f + 1.0 / 720.0; p = p * f + 1.0 / 120.0; p = p * f + 1.0 / 24.0; p = p * f + 1.0 / 6.0;
    p = p * f + 0.5; p = p * f + 1.0; p = p * f + 1.0;
    const int64_t bits = ((int64_t)kd + 1023) << 52;
    double scale; memcpy(&scale, &bits, sizeof scale);
    return p * scale;
}
struct TodaRing {
    double q[TODA_NODES_MAX], p[TODA_NODES_MAX];              // THE STATE IN DOUBLE: the drift gate asks a thousandth of a dB over ten minutes of 192 kHz steps, and
    int    n;                                                //   float's rounding of q += p alone drifts a hundredth of a dB a minute (measured at the step)
    double a, b, kcap;                                       // the exponential's scale a (k = a·b), the nonlinearity b, THE SOFT CAP on e^{−b r}: E = e up to
                                                             //   cap/2, then cap − cap²/(4e) — C¹, saturating at cap; the stiffness k·E'/(−b)… peaks at k·cap/2
                                                             //   at the knee (a hard min() left a kink the leapfrog fed on at the margin, measured)
    double pick_x, pick_xi;                                  // the pickup's set position (nodes) and its lattice coordinate ξ: the pickup stands still in the lab
                                                             //   against the ring's MEAN drift (ξ runs back at Σp/N a step) — the swirl's boost reaches it as a
                                                             //   Doppler — and reads the compression in the lattice's own coordinates (a fully Eulerian reading,
                                                             //   ξ + q(ξ) = x, sees the density spike of a hard soliton, the lattice near-touching: −43 dB of
                                                             //   top-octave content, measured; the lattice reading keeps the half-bands' floor)
    void reset(int nodes) { n = nodes < 4 ? 4 : (nodes > TODA_NODES_MAX ? TODA_NODES_MAX : nodes); for (int i = 0; i < TODA_NODES_MAX; i++) q[i] = p[i] = 0.0; a = 0.0; b = 1e-6; kcap = 1e9; pick_x = pick_xi = 0.0; }
    void setup(float k, float bb, float cap) { b = bb > 1e-6f ? (double)bb : 1e-6; a = (double)k / b; kcap = cap > 1.0f ? (double)cap : 1.0; }
    void set_pickup(float pos) { const double x = (double)pos * (double)n; if (x != pick_x) { pick_x = x; pick_xi = x; } }
    // the linear ring's first mode on the pitch: 2√k·sin(π/N) = 2·sin(ω/2) (the leapfrog's exact frequency), ω = 2π·hz/rate
    static float k_for(float hz, float rate, int nodes) { const float s = sinf(PI * hz / rate) / sinf(PI / (float)nodes); return s * s; }
    // the nodes a pitch allows under the bound: the top mode 2√k < 2·0.95 → sin(π/N) ≥ sin(ω/2)/0.95; < 4: not representable
    static int nodes_for(float hz, float rate, int wanted) {
        const float s = sinf(PI * hz / rate) / 0.95f; if (s >= 1.0f) return 0;
        int c = (int)floorf(PI / asinf(s)); if (c > wanted) c = wanted; return c < 4 ? 0 : c;
    }
    inline double r_at(int i) const { return q[i + 1 < n ? i + 1 : 0] - q[i]; }
    inline void step(float damp) {                           // the kick from the pre-update q, then the drift; damp the declared contraction on p
        double E[TODA_NODES_MAX];
        double x[TODA_NODES_MAX];
        for (int i = 0; i < n; i++) x[i] = -b * r_at(i);
        for (int i = 0; i < n; i++) E[i] = toda_exp(x[i]);
        for (int i = 0; i < n; i++) if (E[i] > 0.5 * kcap) E[i] = kcap - 0.25 * kcap * kcap / E[i];
        double sum_p = 0.0;
        for (int i = 0; i < n; i++) { const double f = a * (E[i > 0 ? i - 1 : n - 1] - E[i]); p[i] = (p[i] + f) * (double)damp; sum_p += p[i]; }
        for (int i = 0; i < n; i++) q[i] += p[i];
        pick_xi -= sum_p / (double)n;                        // the pickup stands still against the mean drift
        if (pick_xi >= (double)n) pick_xi -= (double)n; else if (pick_xi < 0.0) pick_xi += (double)n;
    }
    // H with the capped potential: V = (a/b)(e^{−br} + br − 1) down to the knee r_c (e^{−b r_c} = cap/2); past it, with
    // E = cap − cap²/(4e): V = V(r_c) + a[(r − r_c)(1 − cap) + (cap²/4b)(e^{br} − e^{b r_c})] (V' continuous: a(1 − cap/2))
    double energy() const {
        double e = 0.0; const double rc = -log(0.5 * kcap) / b, vc = (a / b) * (0.5 * kcap + b * rc - 1.0), erc = exp(b * rc);
        for (int i = 0; i < n; i++) {
            const double r = r_at(i); e += 0.5 * p[i] * p[i];
            e += r >= rc ? (a / b) * (exp(-b * r) + b * r - 1.0) : vc + a * ((r - rc) * (1.0 - kcap) + (0.25 * kcap * kcap / b) * (exp(b * r) - erc));
        }
        return e;
    }
    // the ring's rate changes (the inner steps a sub-step): p is per step — scaled by the old step over the new; H in per-step
    // units scales by its square, the physics invariant (k is re-set by the caller at the new rate)
    void rescale(double ratio) { for (int i = 0; i < n; i++) p[i] *= ratio; }
    float momentum() const { double s = 0.0; for (int i = 0; i < n; i++) s += p[i]; return (float)s; }
    float strain_peak() const { double m = 0.0; for (int i = 0; i < n; i++) { const double c = -r_at(i); if (c > m) m = c; } return (float)m; }   // the compression's peak
    // the strike: a Gaussian compression of amplitude `amp` (strain units) and width `w` (nodes) about pos·N, mean-free around
    // the ring (its length is fixed), launched to the right with p = −√k·r (exact for the linear wave; the soliton outruns it
    // and the rest radiates); a re-strike adds its pulse
    void strike(float amp, float pos, float w, float launch) {
        double r[TODA_NODES_MAX]; double mean = 0.0; const double c = (double)pos * (double)n, ww = w > 0.1f ? (double)w : 0.1;
        for (int i = 0; i < n; i++) { double x = (double)i - c; if (x > 0.5 * (double)n) x -= (double)n; if (x < -0.5 * (double)n) x += (double)n; r[i] = -(double)amp * exp(-0.5 * x * x / (ww * ww)); mean += r[i]; }
        mean /= (double)n;
        double acc = 0.0;
        for (int i = 0; i < n; i++) { r[i] -= mean; q[i] += acc; acc += r[i]; p[i] += -(double)launch * r[i]; }
    }
    void kick_all(float dp) { for (int i = 0; i < n; i++) p[i] += (double)dp; }   // the swirl's impulse: a uniform momentum, the boost
    // the compression at ξ: the bonds sit at i + ½, the four around ξ through a Catmull–Rom cubic (C¹ across the bonds, and
    // ξ moves slowly — the kinks enter at second order in the drift)
    inline float pickup() const {
        double u = pick_xi - 0.5; if (u < 0.0) u += (double)n; int i = (int)u; if (i >= n) i = n - 1; const double f = u - (double)i;
        const double c0 = -r_at(i > 0 ? i - 1 : n - 1), c1 = -r_at(i), c2 = -r_at(i + 1 < n ? i + 1 : 0), c3 = -r_at(i + 2 < n ? i + 2 : i + 2 - n);
        return (float)(c1 + 0.5 * f * ((c2 - c0) + f * ((2.0 * c0 - 5.0 * c1 + 4.0 * c2 - c3) + f * (3.0 * (c1 - c2) + c3 - c0))));
    }
};

// ---------------------------------------------------------------------------
// Step 84 — THE BORIS–LEAPFROG MAGNETIC GYROSCOPE (TO_PROJECT_SPEC §13.13, DECISIONS_13 #2): a charged particle on a
// harmonic surface under a perpendicular field.
// ---------------------------------------------------------------------------
// The state (x₁, x₂) and (y₁, y₂), y the velocity in the cell's units (y = ẋ/ω₀: a free orbit is a circle of radius A),
// H = (x₁² + x₂² + y₁² + y₂²)/2. One sub-step: the half kick y −= (ε/2)·x, THE CAYLEY ROTATION of y by θ_B — three
// shears, t = tan(θ/2) then s = sin θ then t again: exactly [[cos θ, −sin θ], [sin θ, cos θ]], and in floating point a
// round-off walk rather than a bias (a cos/sin matrix whose c² + s² misses 1 by a unit in the last place scales the
// energy by that EVERY step: e^{±1.7} over ten minutes at 96 kHz) — then the half kick, then the drift x += ε·y. The
// Lorentz force does no work: the rotation leaves |y| as it was. ε = 2·sin(ω₀·dt/2) (the magic circle's prewarp) and
// θ = ω_B·dt — the Boris angle 2·atan(B_num·dt/2) with B_num = (2/dt)·tan(ω_B·dt/2), the field prewarped. On z = x₁ + i·x₂,
// w = y₁ + i·y₂ the map is complex-linear with trace (1 + e^{iθ})(1 − ε²/2) and determinant e^{iθ}, so its angles are
// θ/2 ± ψ with cos ψ = cos(θ/2)·(1 − ε²/2): the Zeeman doublet ω_B/2 ± sqrt(ω₀² + ω_B²/4) to (ω_B·dt/2)²/6 — a
// thousandth of a cent at 50 Hz. Volume-preserving (det 1: every factor a shear); the energy breathes with the kick's
// splitting and does not drift. The state is double (the ten-minute gate). (x₁, x₂) are the voice's (L, R).
struct Gyro {
    double x1, x2, y1, y2;
    double eps, t, s;      // the spring's step, the rotation's shears
    double theta;          // the rotation a step (the own-clock strobe accumulates it)
    double phase;          // the cyclotron phase, [0, 2π)
    void reset() { x1 = x2 = y1 = y2 = 0.0; eps = 0.0; t = s = 0.0; theta = 0.0; phase = 0.0; }
    void setup(float e, float th) { eps = (double)e; theta = (double)th; t = tan(0.5 * (double)th); s = sin((double)th); }
    // the strike: a displacement of a along the pan's direction — the equal-power law (√(1−u), √u): u ½ the diagonal, both
    // channels THE SAME BITS (a mono planar string until the field turns it); 0 the left alone, 1 the right; a re-strike adds
    void strike(float a, float pan) { const double u = (double)pan; x1 += (double)a * sqrt(1.0 - u); x2 += (double)a * sqrt(u); }
    inline void step() {
        const double h = 0.5 * eps;
        y1 -= h * x1; y2 -= h * x2;
        y1 -= t * y2; y2 += s * y1; y1 -= t * y2;
        y1 -= h * x1; y2 -= h * x2;
        x1 += eps * y1; x2 += eps * y2;
        phase += theta;
    }
    inline void contract(float r) { x1 *= r; x2 *= r; y1 *= r; y2 *= r; }   // the declared decay, on the whole state
    double energy() const { return 0.5 * (x1 * x1 + x2 * x2 + y1 * y1 + y2 * y2); }
    // the step's two angles (radians a step): θ/2 ± ψ — the doublet the scheme sounds
    static void angles(double eps, double theta, double* plus, double* minus) {
        double c = cos(0.5 * theta) * (1.0 - 0.5 * eps * eps); if (c > 1.0) c = 1.0; if (c < -1.0) c = -1.0;
        const double psi = acos(c); *plus = 0.5 * theta + psi; *minus = psi - 0.5 * theta;
    }
};

// ---------------------------------------------------------------------------
// Step 85 — THE BILLIARD (TO_PROJECT_SPEC §13.13, DECISIONS_13 #3): free flight under the note's spring inside a convex
// wall, with the exact specular reflection.
// ---------------------------------------------------------------------------
// The state (x₁, x₂) and the velocity (y₁, y₂) in the cell's units (y = ẋ/ω₀): the flight between the walls is the
// harmonic oscillator's exact rotation by θ = ω₀·dt in each plane (three shears, as the gyroscope's), so the pitch is the
// note's by construction and the energy ½(x² + y²) is the orbit's. The wall is a stadium — two half-ellipses (the half-axes
// a along x, b along y; the aspect a/b from CC 74) joined by straight walls of half-length hl (the mod wheel's L) — and a
// step that crosses it is cut at the crossing: the fraction of the rotation found by bisection on the angle, the velocity
// reflected off the wall's normal (v' = v − 2(v·n̂)n̂, |v| as it was), the rest of the rotation taken. Every factor is a
// rotation or a reflection: det J = +1 on the 4D state (Liouville — the proposal's −1 was the 2D impact oscillator's, a
// different voice). The amplitude is the wall's: a kick past it folds the orbit (a wavefolder with the circle's integrable
// orbits; the stadium's are ergodic — the dust in the strobe). THE KINK: a reflection jumps the velocity, so the positions
// have a slope discontinuity the 192 kHz sampling would alias; each collision adds the two-point polyBLAMP residual of its
// slope jump Δ = θ·(y' − y) at its fraction f — Δ(1 − f)³/6 to the sample before, Δ·f³/6 to the sample after — through a
// one-sample output delay. `exact` 0 is the lab's red: the reflection at the step's end, no BLAMP.
struct Billiard {
    double x1, x2, y1, y2;
    double t, s, theta;              // the step's rotation shears and its angle
    double a, b, hl;                 // the wall
    double d1, d2;                   // the one-sample output delay a channel (the BLAMP's reach back)
    uint32_t hits;                   // the collisions since the strike
    bool exact;
    double blamp;                    // the BLAMP residual's gain (1; the probe's 0 and −1)
    void reset() { x1 = x2 = y1 = y2 = 0.0; t = s = theta = 0.0; a = b = 1.0; hl = 0.0; d1 = d2 = 0.0; hits = 0; exact = true; blamp = 1.0; }
    void setup(float th, float aa, float bb, float L) { theta = (double)th; t = tan(0.5 * (double)th); s = sin((double)th); a = (double)aa; b = (double)bb; hl = 0.5 * (double)L; }
    void strike(float v, float pan) { const double u = (double)pan; y1 += (double)v * sqrt(1.0 - u); y2 += (double)v * sqrt(u); }   // a kick along the pan's direction
    inline void contract(float r) { x1 *= r; x2 *= r; y1 *= r; y2 *= r; }
    double energy() const { return 0.5 * (x1 * x1 + x2 * x2 + y1 * y1 + y2 * y2); }
    inline double g(double x, double y) const {                                   // the wall: < 0 inside
        const double ax = fabs(x) - hl; const double yy = y * y / (b * b);
        return ax <= 0.0 ? yy - 1.0 : ax * ax / (a * a) + yy - 1.0;
    }
    inline void normal(double x, double y, double* nx, double* ny) const {
        const double ax = fabs(x) - hl;
        if (ax <= 0.0) { *nx = 0.0; *ny = y >= 0.0 ? 1.0 : -1.0; return; }
        const double gx = 2.0 * ax / (a * a) * (x >= 0.0 ? 1.0 : -1.0), gy = 2.0 * y / (b * b), nn = sqrt(gx * gx + gy * gy);
        *nx = gx / nn; *ny = gy / nn;
    }
    static inline void rot(double& x, double& y, double tt, double ss) { x += tt * y; y -= ss * x; x += tt * y; }   // the oscillator's rotation (clockwise in (x, y))
    // one step; o1/o2 receive the delayed outputs (the positions, a sample late)
    inline void step(float damp, float* o1, float* o2) {
        double px1 = x1, px2 = x2, py1 = y1, py2 = y2;   // the state at the start of the part of the step not yet taken
        double f0 = 0.0;                                 // the fraction of the step that part starts at
        double c1 = 0.0, c2 = 0.0;                       // the BLAMP's corrections to this step's output sample
        rot(x1, y1, t, s); rot(x2, y2, t, s);
        if (exact) {
            for (int hit = 0; hit < 4 && g(x1, x2) > 0.0; hit++) {
                double lo = f0, hi = 1.0;                                          // the crossing lies in (f0, 1]
                double tx1 = 0, tx2 = 0, ty1 = 0, ty2 = 0;
                for (int it = 0; it < 16; it++) {
                    const double mid = 0.5 * (lo + hi), ang = (mid - f0) * theta, tm = tan(0.5 * ang), sm = sin(ang);
                    tx1 = px1; ty1 = py1; tx2 = px2; ty2 = py2; rot(tx1, ty1, tm, sm); rot(tx2, ty2, tm, sm);
                    if (g(tx1, tx2) > 0.0) hi = mid; else lo = mid;
                }
                const double f = lo, ang = (f - f0) * theta, tm = tan(0.5 * ang), sm = sin(ang);
                tx1 = px1; ty1 = py1; tx2 = px2; ty2 = py2; rot(tx1, ty1, tm, sm); rot(tx2, ty2, tm, sm);   // just inside, at the crossing
                double nx, ny; normal(tx1, tx2, &nx, &ny);
                const double vn = ty1 * nx + ty2 * ny;
                if (vn > 0.0) {                                                    // heading out: reflect
                    const double ny1 = ty1 - 2.0 * vn * nx, ny2 = ty2 - 2.0 * vn * ny;
                    const double dl1 = blamp * theta * (ny1 - ty1), dl2 = blamp * theta * (ny2 - ty2);   // the slope jump in the position, per step
                    const double w = 1.0 - f;
                    d1 += dl1 * w * w * w / 6.0; d2 += dl2 * w * w * w / 6.0;      // the sample before
                    c1 += dl1 * f * f * f / 6.0; c2 += dl2 * f * f * f / 6.0;      // the sample after (this step's)
                    ty1 = ny1; ty2 = ny2; hits++;
                }
                px1 = tx1; py1 = ty1; px2 = tx2; py2 = ty2; f0 = f;
                const double rem = (1.0 - f0) * theta, tr = tan(0.5 * rem), sr = sin(rem);
                x1 = px1; y1 = py1; x2 = px2; y2 = py2; rot(x1, y1, tr, sr); rot(x2, y2, tr, sr);
            }
        } else if (g(x1, x2) > 0.0) {                                               // the red: the reflection at the step's end, no cut, no BLAMP
            double nx, ny; normal(x1, x2, &nx, &ny); const double vn = y1 * nx + y2 * ny;
            if (vn > 0.0) { y1 -= 2.0 * vn * nx; y2 -= 2.0 * vn * ny; hits++; }
        }
        if (damp != 1.0f) contract(damp);
        *o1 = (float)d1; *o2 = (float)d2;
        d1 = x1 + c1; d2 = x2 + c2;
    }
};

// ---------------------------------------------------------------------------
// Step 85 — THE PORT-HAMILTONIAN DIRAC NETWORK (TO_PROJECT_SPEC §13.13, DECISIONS_13 #3): ẋ = (J − R)∇H, H = ½xᵀQx.
// ---------------------------------------------------------------------------
// A network of n states under a quadratic energy (Q diagonal: the springs' stiffnesses, the masses' inverses), a Dirac
// structure J (skew: which port's effort drives which flow) and a dissipation R (diagonal, ≥ 0), stepped by the Cayley
// transform — the implicit midpoint — M = (I − h/2·A)⁻¹(I + h/2·A), A = (J − R)Q, built once by Gauss–Jordan. With R = 0
// the step is Q-ORTHOGONAL (an isometry in the energy norm, xᵀQx conserved exactly; not orthogonal — the spec's gate
// delta b), with R ⪰ 0 strictly passive; the suite's probe reads the Q-norm gain of M ≤ 1 for arbitrary skew J. The
// prepared network the patch ships: a chain of masses (unit mass) on springs k with both ends held — the states the
// springs' EXTENSIONS and the masses' momenta, so H is diagonal and J the chain's incidence — its last spring on a bridge
// mass (m_b, its own spring k_b to the wall; its velocity the voice's output), the stick-slip bow on a mass as a port
// (the force a kick between Cayley steps, its work in the ledger), and THE RATTLE STOP on the bridge's displacement — a
// unilateral wall at −d, the bridge's momentum reflected with the declared loss, as the reed's lay stops the reed. The
// chain's lowest mode is set on the note by inverse iteration on the chain-and-bridge stiffness (the bridge loads the
// end), the target prewarped for the midpoint rule (ω' = (2/h)·tan(ω₀h/2)). The state and M are double: a matrix that
// is Q-orthogonal to float's last place is a gain or a loss of that every step.
constexpr int DIRAC_MAX = 36;
struct Dirac {
    int    n;
    double x[DIRAC_MAX];
    double q[DIRAC_MAX];                      // Q's diagonal
    double M[DIRAC_MAX][DIRAC_MAX];
    int    masses, ib_e, ib_p;                // the chain's masses; the bridge's extension and momentum indices
    double m_b, k, k_b, stop, stop_loss;      // the bridge's mass, the springs, the stop's distance and loss
    double gain_q;                            // the Q-norm gain of M (power iteration at the build): ≤ 1 is the passivity
    uint32_t stops;                           // the stop's hits since the strike
    double loss_acc;                          // the energy the stop took since the strike
    void reset() { n = 0; for (int i = 0; i < DIRAC_MAX; i++) x[i] = 0.0; stops = 0; loss_acc = 0.0; }
    int idx_e(int j) const { return j; }                          // the extensions 0..masses (masses + 1 of them)
    int idx_p(int i) const { return masses + 1 + (i - 1); }       // the momenta of masses 1..masses
    // THE CAYLEY STEP from A = (J − R)Q (n×n, row-major), h the step: M = (I − h/2·A)⁻¹(I + h/2·A); false if singular
    bool build(int nn, const double* A, double h) {
        n = nn; if (n < 1 || n > DIRAC_MAX) return false;
        static thread_local double W[DIRAC_MAX][2 * DIRAC_MAX];
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) { const double a = A[i * n + j], d = i == j ? 1.0 : 0.0; W[i][j] = d - 0.5 * h * a; W[i][n + j] = d + 0.5 * h * a; }
        for (int c = 0; c < n; c++) {                              // Gauss–Jordan with partial pivoting on [I − h/2 A | I + h/2 A]
            int piv = c; for (int r = c + 1; r < n; r++) if (fabs(W[r][c]) > fabs(W[piv][c])) piv = r;
            if (fabs(W[piv][c]) < 1e-300) return false;
            if (piv != c) for (int j = 0; j < 2 * n; j++) { const double tmp = W[c][j]; W[c][j] = W[piv][j]; W[piv][j] = tmp; }
            const double inv = 1.0 / W[c][c]; for (int j = 0; j < 2 * n; j++) W[c][j] *= inv;
            for (int r = 0; r < n; r++) if (r != c && W[r][c] != 0.0) { const double f = W[r][c]; for (int j = 0; j < 2 * n; j++) W[r][j] -= f * W[c][j]; }
        }
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) M[i][j] = W[i][n + j];
        // the Q-norm gain: the largest singular value of Q^½ M Q^−½, by power iteration on its Gram matrix
        double v[DIRAC_MAX], w[DIRAC_MAX]; for (int i = 0; i < n; i++) v[i] = 1.0 / sqrt((double)n + i);
        double lam = 0.0;
        for (int it = 0; it < 120; it++) {
            for (int i = 0; i < n; i++) { double acc = 0.0; for (int j = 0; j < n; j++) acc += M[i][j] * v[j] / sqrt(q[j]); w[i] = acc * sqrt(q[i]); }   // B v
            for (int j = 0; j < n; j++) { double acc = 0.0; for (int i = 0; i < n; i++) acc += M[i][j] * sqrt(q[i]) * w[i]; v[j] = acc / sqrt(q[j]); }   // Bᵀ(B v)
            double nrm = 0.0; for (int i = 0; i < n; i++) nrm += v[i] * v[i]; nrm = sqrt(nrm); if (nrm <= 0.0) break;
            lam = nrm; for (int i = 0; i < n; i++) v[i] /= nrm;
        }
        gain_q = sqrt(lam);
        return true;
    }
    inline void step() {
        double y[DIRAC_MAX];
        for (int i = 0; i < n; i++) { const double* r = M[i]; double acc = 0.0; for (int j = 0; j < n; j++) acc += r[j] * x[j]; y[i] = acc; }
        for (int i = 0; i < n; i++) x[i] = y[i];
    }
    double energy() const { double e = 0.0; for (int i = 0; i < n; i++) e += 0.5 * q[i] * x[i] * x[i]; return e; }
    // THE PREPARED NETWORK: `masses` on springs k (set below), the bridge (m_b, k_b) on the last spring, the damping on the
    // momenta (the amplitude's T60 = decay_s; 0 = none), the step h; the chain's lowest mode put on `hz` (prewarped)
    bool chain(int masses_, float hz, float bridge_hz, float bridge_mass, float decay_s, float bridge_decay_s, double h) {
        masses = masses_ < 2 ? 2 : (masses_ > 16 ? 16 : masses_);
        const int ne = masses + 1, nn = ne + masses + 2; ib_e = ne + masses; ib_p = ib_e + 1;
        m_b = bridge_mass > 0.01f ? (double)bridge_mass : 0.01;
        const double w_t = (2.0 / h) * tan(0.5 * 2.0 * M_PI * (double)hz * h), wb_t = (2.0 / h) * tan(0.5 * 2.0 * M_PI * (double)bridge_hz * h);
        k_b = m_b * wb_t * wb_t;
        k = 0.25 * w_t * w_t / (sin(0.5 * M_PI / (double)(masses + 1)) * sin(0.5 * M_PI / (double)(masses + 1)));   // the fixed–fixed chain's lowest mode
        for (int it = 0; it < 6; it++) {                                           // the bridge loads the end: inverse iteration on the chain-and-bridge stiffness
            const int N = masses + 1; double dg[DIRAC_MAX], od[DIRAC_MAX], mm[DIRAC_MAX], u[DIRAC_MAX], r[DIRAC_MAX];
            for (int i = 0; i < N; i++) { dg[i] = i < masses ? 2.0 * k : k + k_b; od[i] = -k; mm[i] = i < masses ? 1.0 : m_b; u[i] = 1.0; }
            double lam = 0.0;
            for (int pw = 0; pw < 60; pw++) {
                for (int i = 0; i < N; i++) r[i] = mm[i] * u[i];
                double cp[DIRAC_MAX], dp[DIRAC_MAX];                                  // Thomas on the tridiagonal K
                cp[0] = od[0] / dg[0]; dp[0] = r[0] / dg[0];
                for (int i = 1; i < N; i++) { const double den = dg[i] - od[i - 1] * cp[i - 1]; cp[i] = od[i] / den; dp[i] = (r[i] - od[i - 1] * dp[i - 1]) / den; }
                u[N - 1] = dp[N - 1]; for (int i = N - 2; i >= 0; i--) u[i] = dp[i] - cp[i] * u[i + 1];
                double num = 0.0, den = 0.0;                                           // the Rayleigh quotient uᵀKu / uᵀMu
                for (int i = 0; i < N; i++) { double ku = dg[i] * u[i] + (i > 0 ? od[i - 1] * u[i - 1] : 0.0) + (i + 1 < N ? od[i] * u[i + 1] : 0.0); num += u[i] * ku; den += u[i] * mm[i] * u[i]; }
                lam = num / den; const double nrm = sqrt(den); for (int i = 0; i < N; i++) u[i] /= nrm;
            }
            const double w1 = sqrt(lam > 0.0 ? lam : 1e-12); k *= (w_t / w1) * (w_t / w1);
        }
        for (int i = 0; i < nn; i++) q[i] = 1.0;
        for (int j = 0; j < ne; j++) q[idx_e(j)] = k;                                // the springs' energy ½k e²
        for (int i = 1; i <= masses; i++) q[idx_p(i)] = 1.0;                         // the masses' ½p² (unit mass)
        q[ib_e] = k_b; q[ib_p] = 1.0 / m_b;
        static thread_local double A[DIRAC_MAX * DIRAC_MAX];
        for (int i = 0; i < nn * nn; i++) A[i] = 0.0;
        auto J = [&](int r, int c, double v) { A[r * nn + c] += v; };              // J's entries (the chain's incidence); A = (J − R)Q applied below
        for (int j = 0; j < ne; j++) {                                               // ė_j = ∂H/∂p_{j+1} − ∂H/∂p_j (p_0 = the wall; p_{masses+1} = the bridge's)
            if (j + 1 <= masses) J(idx_e(j), idx_p(j + 1), 1.0); else J(idx_e(j), ib_p, 1.0);
            if (j >= 1) J(idx_e(j), idx_p(j), -1.0);
        }
        for (int i = 1; i <= masses; i++) { J(idx_p(i), idx_e(i), 1.0); J(idx_p(i), idx_e(i - 1), -1.0); }   // ṗ_i = ∂H/∂e_i − ∂H/∂e_{i−1}
        J(ib_e, ib_p, 1.0);                                                           // ė_b = ∂H/∂p_b
        J(ib_p, idx_e(ne - 1), -1.0); J(ib_p, ib_e, -1.0);                           // ṗ_b = −∂H/∂e_last − ∂H/∂e_b
        const double gamma = decay_s > 0.0f ? 2.0 * log(1000.0) / (double)decay_s : 0.0;   // R on the momenta: ṗ −= γ·∂H/∂p (the amplitude's T60)
        for (int i = 1; i <= masses; i++) A[idx_p(i) * nn + idx_p(i)] -= gamma;
        const double gamma_b = bridge_decay_s > 0.0f ? 2.0 * log(1000.0) / (double)bridge_decay_s : 0.0;   // the bridge's own loss — the radiation: a body's mode is broad
        A[ib_p * nn + ib_p] -= (gamma + gamma_b) * m_b;
        for (int r = 0; r < nn; r++) for (int c = 0; c < nn; c++) A[r * nn + c] *= q[c];   // (J − R)Q
        return build(nn, A, h);
    }
    // the ports between the Cayley steps: the rattle stop on the bridge (returns the energy it took)
    inline double rattle() {
        if (stop <= 0.0 || x[ib_e] >= -stop) return 0.0;
        const double e_before = 0.5 * k_b * x[ib_e] * x[ib_e] + 0.5 * x[ib_p] * x[ib_p] / m_b;
        x[ib_e] = -stop;
        if (x[ib_p] < 0.0) x[ib_p] = -(1.0 - stop_loss) * x[ib_p];
        const double e_after = 0.5 * k_b * x[ib_e] * x[ib_e] + 0.5 * x[ib_p] * x[ib_p] / m_b;
        stops++; const double took = e_before - e_after; loss_acc += took; return took;
    }
};

// ---------------------------------------------------------------------------
// Step 80 — THE STICK-SLIP BOW (TO_PROJECT_SPEC §13.10, DECISIONS_12 #4): Helmholtz motion from a friction curve.
// ---------------------------------------------------------------------------
// The bow moves at v_b past a point of the string moving at v; the friction is F·μ(v_rel), v_rel = v_b − v, with the
// hyperbolic curve μ(v_rel) = μ_d + (μ_s − μ_d)·v₀²/(v₀² + v_rel²) — μ_s at rest (the grip), μ_d far from it (the slip),
// v₀ the width where one becomes the other — and the sign of v_rel, smoothed over a hundredth of v₀ so Newton has a
// derivative. The force acts on the point through its mobility G (a velocity per unit force per sub-step: 1 for the
// chain's node, whose mass is 1; 1 for the lattice's point, by the modal masses' choice in voxo.cpp), IMPLICITLY — the
// new velocity solves v = v_pre + G·f(v_b − v), three Newton iterations from v_pre with the slope floored (the curve's
// falling flank makes the equation fold: the stick-to-slip jump) — and then THE BOUND: the string never crosses the
// bow's velocity in a step (it sticks there instead) and never takes more than the grip can give, |Δv| ≤ G·F·μ_s. With
// the bound the sign of f is the sign of v_rel before and after, so the work the bow does, f·v_b, is at least what the
// string gains, f·v: the energy a bowed string holds never exceeds the strike's plus ∫f·v_b — the winds' power-limited
// port discipline, by arithmetic, which the suite's ledger asserts. The explicit form (the force from the previous
// sub-step's v_rel, no bound) is the red control: the string overshoots the bow and the ledger's line is crossed.
struct StickSlip {
    static constexpr float MU_S = 0.8f, MU_D = 0.3f;   // the grip and the slip (rosin on a string: 0.8 / 0.3 by the usual measurements)
    static inline float mu(float vrel, float v0) { return MU_D + (MU_S - MU_D) * v0 * v0 / (v0 * v0 + vrel * vrel); }
    // f(v_rel) = F·μ(v_rel)·sgn(v_rel), the sign smoothed: v_rel / sqrt(v_rel² + (v₀/100)²)
    static inline float friction(float vrel, float F, float v0) {
        const float d = 0.01f * v0, s = vrel / sqrtf(vrel * vrel + d * d);
        return F * mu(vrel, v0) * s;
    }
    static inline float dfriction(float vrel, float F, float v0) {   // df/dv_rel
        const float d = 0.01f * v0, q = vrel * vrel + d * d, r = sqrtf(q), s = vrel / r;
        const float dsdv = (d * d) / (q * r);
        const float m = mu(vrel, v0), dmdv = -(MU_S - MU_D) * v0 * v0 * 2.0f * vrel / ((v0 * v0 + vrel * vrel) * (v0 * v0 + vrel * vrel));
        return F * (dmdv * s + m * dsdv);
    }
    // THE IMPLICIT SOLVE with the bound and THE HYSTERESIS (McIntyre–Woodhouse): the curve against the point's load line
    // can meet three times, and the string stays on the branch it is on — stuck, it stays stuck while the force the bow
    // must give to hold it (|v_b − v_pre|/G) is within the grip F·μ_s; slipping, Newton finds the slip root from the free
    // velocity, and the capture is the bound: a step that would carry the point past the bow sticks it there. Returns the
    // new velocity, *f_out the force taken (per sub-step, velocity units); *stuck is the voice's branch.
    static inline float solve(float v_pre, float v_b, float F, float v0, float G, float* f_out, bool* stuck) {
        const float cap = G * F * MU_S;
        if (*stuck && fabsf(v_b - v_pre) <= cap) {                     // sticking continues: the bow holds the point at its speed
            if (f_out) *f_out = G > 0.0f ? (v_b - v_pre) / G : 0.0f;
            return v_b;
        }
        float v = v_pre;
        for (int it = 0; it < 3; it++) {
            const float vrel = v_b - v;
            const float h = v - v_pre - G * friction(vrel, F, v0);
            float hp = 1.0f + G * dfriction(vrel, F, v0);              // d/dv of (v − v_pre − G·f(v_b − v))
            if (hp < 0.25f) hp = 0.25f;                                // the falling flank folds the equation: a damped step
            v -= h / hp;
        }
        // the bound: the string never crosses the bow in a step (the capture: it sticks), and never takes more than the grip
        // can give
        bool st = false;
        if ((v_b - v_pre) * (v_b - v) <= 0.0f) { v = v_b; st = true; }
        if (v - v_pre > cap) { v = v_pre + cap; st = false; } else if (v_pre - v > cap) { v = v_pre - cap; st = false; }
        *stuck = st;
        const float f = G > 0.0f ? (v - v_pre) / G : 0.0f;
        if (f_out) *f_out = f;
        return v;
    }
    // THE RED CONTROL (test only): the three Newton steps taken raw — no bound, no branch. A converged root never crosses
    // the bow (the force has the sign of the relative velocity it is evaluated at), but three steps on the folding flank
    // do, and then the string takes more than the bow gives: f·v > f·v_b for that step — the ledger's line crossed.
    static inline float solve_naive(float v_pre, float v_b, float F, float v0, float G, float* f_out) {
        float v = v_pre;
        for (int it = 0; it < 3; it++) {
            const float vrel = v_b - v;
            const float h = v - v_pre - G * friction(vrel, F, v0);
            float hp = 1.0f + G * dfriction(vrel, F, v0);
            if (hp < 0.25f) hp = 0.25f;
            v -= h / hp;
        }
        const float f = G > 0.0f ? (v - v_pre) / G : 0.0f;
        if (f_out) *f_out = f;
        return v;
    }
};


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
    // step 58's sub-step as written: the suite's scalar reference for the four-lane step below
    inline void step_scalar(float damp) {
        float lap[STRING_NODES_MAX + 2];
        for (int i = 1; i <= m; i++) lap[i] = u[i + 1] - 2.0f * u[i] + u[i - 1];   // every acceleration from the pre-update u
        for (int i = 1; i <= m; i++) { v[i] = v[i] * damp + s2 * lap[i]; u[i] += v[i]; }
    }
    // Step 78 (TO_PROJECT_SPEC §13.8, DECISIONS_12 #1): THE CHAIN IN FOUR LANES — the same two passes, four nodes at a time:
    // the Laplacian from the pre-update u in the scalar's order ((u[i+1] − 2u[i]) + u[i−1], the product fused where the
    // scalar's compiler fuses it), then v = v·damp + s2·lap (the first product fused likewise) and u += v; the tail scalar.
    // Per node the operations are the scalar's, so the two agree bit for bit under one contraction rule.
    inline void step(float damp) {
        float lap[STRING_NODES_MAX + 6];
        const f32x4 two = f4_set1(2.0f), vd = f4_set1(damp), vs = f4_set1(s2);
        int i = 1;
        for (; i + 3 <= m; i += 4) {
            const f32x4 um = f4_load(&u[i - 1]), u0 = f4_load(&u[i]), up = f4_load(&u[i + 1]);
            f4_store(&lap[i], f4_add(f4_mul_sub(up, two, u0), um));
        }
        for (; i <= m; i++) lap[i] = u[i + 1] - 2.0f * u[i] + u[i - 1];
        i = 1;
        for (; i + 3 <= m; i += 4) {
            const f32x4 vv = f4_mul_add(f4_load(&v[i]), vd, f4_mul(vs, f4_load(&lap[i])));
            f4_store(&v[i], vv); f4_store(&u[i], f4_add(f4_load(&u[i]), vv));
        }
        for (; i <= m; i++) { v[i] = v[i] * damp + s2 * lap[i]; u[i] += v[i]; }
    }
    // Step 80: the step in two halves for the stick-slip bow — the kick (the same arithmetic as above, the drift held back),
    // the bow on its node BETWEEN them (a bowed node must move at the bow's speed in the drift that follows: applied after
    // the drift, a strong bow pinned the node into a static deflection — the string's kick cancelled it before it moved),
    // then the drift. Unbowed, kick() then drift() is step() bit for bit.
    inline void kick(float damp) {
        float lap[STRING_NODES_MAX + 6];
        const f32x4 two = f4_set1(2.0f), vd = f4_set1(damp), vs = f4_set1(s2);
        int i = 1;
        for (; i + 3 <= m; i += 4) {
            const f32x4 um = f4_load(&u[i - 1]), u0 = f4_load(&u[i]), up = f4_load(&u[i + 1]);
            f4_store(&lap[i], f4_add(f4_mul_sub(up, two, u0), um));
        }
        for (; i <= m; i++) lap[i] = u[i + 1] - 2.0f * u[i] + u[i - 1];
        i = 1;
        for (; i + 3 <= m; i += 4) f4_store(&v[i], f4_mul_add(f4_load(&v[i]), vd, f4_mul(vs, f4_load(&lap[i]))));
        for (; i <= m; i++) v[i] = v[i] * damp + s2 * lap[i];
    }
    inline void drift() {
        int i = 1;
        for (; i + 3 <= m; i += 4) f4_store(&u[i], f4_add(f4_load(&u[i]), f4_load(&v[i])));
        for (; i <= m; i++) u[i] += v[i];
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
    // Step 80 (TO_PROJECT_SPEC §13.10): THE BOW ON THE LOOP — two taps. The ring holds the right-going wave as it will arrive at
    // the bridge, τ samples from now, the nut's inversion already folded in; the bow at the fraction β of the length sees the
    // right-going wave about to pass it, R(L_b) with L_b = (1 − β)·n/2, and the left-going wave from the bridge side, which is
    // −R(L_b + β·n) (it has the nut still ahead of it). The junction: v = v_i⁺ + v_i⁻ + f/(2Z) with Z = 1 in the loop's
    // units (the mobility 1/2), the friction solved implicitly and bounded (StickSlip), and each outgoing wave takes f/2:
    // R(L_b) += f/2, R(L_b + βn) −= f/2. The loop gains exactly f·v a sub-step (the two squares' change), the bow works
    // f·v_b ≥ f·v: the ledger by arithmetic. The delay line is dispersionless, so the Helmholtz corner stays sharp — the
    // chain's lattice dispersion smears it (measured, DECISIONS_12 #4).
    inline void bow(float beta, float vb, float F, float v0, bool naive, float* f_out, bool* stuck, float* v_out = nullptr) {
        int t1 = (int)((1.0f - beta) * 0.5f * (float)n + 0.5f), t2 = t1 + (int)(beta * (float)n + 0.5f);
        if (t1 < 1) t1 = 1; if (t2 <= t1) t2 = t1 + 1;
        if (t2 >= n) { if (f_out) *f_out = 0.0f; if (v_out) *v_out = 0.0f; return; }
        const int i1 = (w - n + t1 + DELAY_MAX) % DELAY_MAX, i2 = (w - n + t2 + DELAY_MAX) % DELAY_MAX;
        const float vh = d[i1] - d[i2];
        float f = 0.0f;
        const float v = naive ? StickSlip::solve_naive(vh, vb, F, v0, 0.5f, &f) : StickSlip::solve(vh, vb, F, v0, 0.5f, &f, stuck);
        d[i1] += 0.5f * f; d[i2] -= 0.5f * f;
        if (f_out) *f_out = f; if (v_out) *v_out = v;
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
    float kappa, cq, pcap;                              // step 81: THE SHOCK — the convective steepening's coefficient (the local speed c(1 + κ·p)), the
                                                        // pressure it saturates at (the Courant number stays under its bound: the base λ leaves room for it) and the
                                                        // Lapidus viscosity's (the limiter; 0 = off, the red); shock_loss the energy it took since the strike
    double shock_loss;
    // Step 81: THE TONEHOLES — branch ports at pressure nodes: the hole's air is an inertance L_h (its volume velocity q_h
    // a momentum kicked by the node's pressure) radiating through a resistance R_h; open, q_h += (λ/L_h)·(p_i − R_h·q_h) and
    // the node's continuity takes q_h beside its neighbours' u; closed, q_h = 0 (the pad). Both are declared: the inertance
    // a symplectic mass, the resistance a conformal loss; a closed hole is a wall. Up to HOLES_MAX holes, each at a node.
    static constexpr int HOLES_MAX = 24;                 // eleven holes and the vent, each over two nodes (a fractional place)
    int   nh; int hnode[HOLES_MAX]; float hinv_l[HOLES_MAX], hr[HOLES_MAX], hq[HOLES_MAX], hlp[HOLES_MAX]; float hla; uint32_t hopen;   // hopen: bit k = hole k open; hlp/hla the holes' radiation one-pole (as the ends')
    float leak_a, zleak, lk0, lk1;                      // step 81: THE LOW-FREQUENCY LEAK at each open end — a resistance z_leak on the one-pole
                                                        // low-passed u (the corner `leak_hz`): the bore's slow modes see it, the harmonics do not
    void reset() { for (int i = 0; i <= BORE_NODES_MAX; i++) p[i] = pprev[i] = 0.0f; for (int i = 0; i < BORE_NODES_MAX; i++) u[i] = 0.0f; lp0 = lp1 = 0.0f; lk0 = lk1 = 0.0f; shock_loss = 0.0; for (int k = 0; k < HOLES_MAX; k++) hq[k] = hlp[k] = 0.0f; }
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
        end0 = e0; end1 = e1; z0 = rad0; z1 = rad1; lam = 1.0f; rad_a = 0.0f; lp0 = lp1 = 0.0f; leak_a = 0.0f; zleak = 0.0f; lk0 = lk1 = 0.0f; kappa = 0.0f; cq = 0.0f; pcap = 0.0f; shock_loss = 0.0; nh = 0; hopen = 0u; hla = 0.0f;
        reset();
    }
    // the holes: `at` the node of each (1 … n−1), `inertance` L_h and `resistance` R_h per hole in the bore's units at that node;
    // the resistance acts through a one-pole at `corner_hz` as the ends' does (∝ f² below it: a hole radiates little at the
    // fundamental — a flat R_h ate the flute's), 0 = flat
    void holes_set(int count, const int* at, const float* inertance, const float* resistance, float corner_hz = 0.0f, float rate = 1.0f) {
        nh = count < 0 ? 0 : (count > HOLES_MAX ? HOLES_MAX : count);
        for (int k = 0; k < nh; k++) { int i = at[k]; if (i < 1) i = 1; if (i > n - 1) i = n - 1; hnode[k] = i; hinv_l[k] = inertance[k] > 1e-9f ? 1.0f / inertance[k] : 0.0f; hr[k] = resistance[k] > 0.0f ? resistance[k] : 0.0f; hq[k] = 0.0f; hlp[k] = 0.0f; }
        hla = corner_hz > 0.0f ? 1.0f - expf(-2.0f * PI * corner_hz / rate) : 0.0f;
    }
    // the hole mode's room under the CFL bound: a hole adds 1/(L_h·S) = h to its node's row of the Laplacian (Gershgorin:
    // μ ≤ 4 + h), so λ ≤ λ_bound/√(1 + h/4) — measured on the bare bore (h 1/8 at 0.999 of the bound grew, 0.99 held)
    static float holes_room(float h) { return 1.0f / sqrtf(1.0f + (h > 0.0f ? h : 0.0f) * 0.25f); }
    void holes_open(uint32_t bits) { hopen = bits; for (int k = 0; k < nh; k++) if (((bits >> k) & 1u) == 0u) hq[k] = hlp[k] = 0.0f; }   // a pad closing stops the hole's air
    float holes_energy() const { double e = 0.0; for (int k = 0; k < nh; k++) if ((hopen >> k) & 1u) e += 0.5 * (double)hq[k] * hq[k] / (hinv_l[k] > 0.0f ? hinv_l[k] : 1.0f); return (float)e; }   // ½L·q² per open hole
    // Step 81 (TO_PROJECT_SPEC §13.11): THE BRASS SHOCK. The wave steepens as it travels — the local speed is c·(1 + ((γ+1)/2)·p/ρc²),
    // the crests outrunning the troughs — so the velocity update's Courant number is λ·(1 + κ·p̄) at each half node, p̄ the
    // two pressures' mean (a simple-wave form of the convective term: the brassy brightening of a loud note, the shock
    // front at fff). THE LIMITER, DECLARED DISSIPATIVE: a Burgers shock must lose energy (the entropy condition) and an
    // energy-conserving scheme rings at the front (Gibbs), so after the sweeps a Lapidus viscosity — a nonlinear diffusion
    // of the pressure in flux form, the flux c_q·κ·|Δp|·Δp·S between neighbours — takes energy only where the gradient is
    // steep; its loss is exact on the quadratic form ½ΣS·p² and accumulates in shock_loss for the mouth-power ledger.
    // THE SATURATION: the crest's speed grows no further past |p| = p_cap — the local Courant number λ(1 + κ·p_cap) is the
    // bound the tuning leaves room for (λ_max / (1 + κ·p_cap)); without it a crest at the grid's own limit ran the scheme
    // past its CFL and the front rang whatever the limiter did (measured).
    void shock(float steepening, float limiter, float p_cap) { kappa = steepening > 0.0f ? steepening : 0.0f; cq = limiter > 0.0f ? limiter : 0.0f; pcap = p_cap > 0.0f ? p_cap : 0.0f; }
    static float shock_room(float steepening, float p_cap) { return 1.0f + (steepening > 0.0f ? steepening : 0.0f) * (p_cap > 0.0f ? p_cap : 0.0f); }
    // Step 81 (TO_PROJECT_SPEC §13.11): THE LOW-FREQUENCY LEAK — the radiation port's one-pole leaves the bore's slow modes
    // (the cavity against the bell's inertance, a few tens of hertz) with no loss at all, and under a strong reed one of
    // them self-oscillates and modulates the tone (the sax's quasi-periodic A3, DECISIONS_7 #33: a 16 Hz sideband pair
    // at every harmonic; measured here). Real bores lose those modes to the walls and the pads. A resistance z_leak on
    // the one-pole low-passed end velocity (the corner leak_hz) is positive-real — the port only absorbs — and the
    // harmonics above the corner see almost none of it (the one-pole's gain falls as f_c/f).
    void leak(float z, float hz, float rate) { zleak = z > 0.0f ? z : 0.0f; leak_a = hz > 0.0f && z > 0.0f ? 1.0f - expf(-2.0f * PI * hz / rate) : 0.0f; }
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
    // Step 81: THE LEAK'S REACTANCE — its one-pole z_l·ω_l/(ω_l + iω) is a compliance above the corner (the imaginary part
    // −z_l·ω_l·ω/(ω_l² + ω²)), a NEGATIVE end correction: the flute read +40 cents sharp before it was counted (D4, z_l 0.3
    // at 40 Hz: −2.1 cells an end against the radiation's +2.9). Per unit λ, as end_correction's; the callers add the two.
    static float leak_correction(float z_leak, float leak_hz, float hz, float rate, float s_end = 1.0f) {
        if (z_leak <= 0.0f || leak_hz <= 0.0f) return 0.0f;
        const float wl = 2.0f * PI * leak_hz / rate, w = 2.0f * PI * hz / rate;
        return -(z_leak / (s_end > 1e-6f ? s_end : 1e-6f)) * wl / (wl * wl + w * w);
    }
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
    // velocity at the far (bell) end, plus the open holes' (step 81) — the standing wave's amplitude there (the port's own pressure falls as f² toward
    // the bass; a far-field microphone would add a derivative).
    // The end pressure the valve will see after this step, before it is taken: p[0] − 2λ·u_½'/S₀ with u_½' the first
    // velocity node's update — so the implicit junction can be solved from the state before the step.
    inline float end_pressure_ahead() const { return end0 == END_CLOSED ? p[0] - 2.0f * lam * (u[0] - lam * su[0] * (p[1] - p[0])) / sp[0] : p[0]; }
    inline float end_impedance() const { return end0 == END_CLOSED ? 2.0f * lam / sp[0] : 0.0f; }   // ∂p[0]/∂q_b within the step
    inline float step(float q_b, float p_src, float damp, float shear = 0.0f) {
        // the two sweeps, each fused with its damping (the same operations in the same order as the four loops they
        // replace); the pressures a step ago are kept for the staggered energy
        if (kappa != 0.0f) {                                                   // step 81: the steepening — the local Courant number λ(1 + κ·p̄), p̄ saturated at ±p_cap (branchless: the same values)
            const float lk = lam * kappa, pc = pcap;
            if (damp != 1.0f) for (int i = 0; i < n; i++) { const float pb = fminf(fmaxf(0.5f * (p[i] + p[i + 1]), -pc), pc); u[i] = (u[i] - (lam + lk * pb) * su[i] * (p[i + 1] - p[i])) * damp; }
            else              for (int i = 0; i < n; i++) { const float pb = fminf(fmaxf(0.5f * (p[i] + p[i + 1]), -pc), pc); u[i] -= (lam + lk * pb) * su[i] * (p[i + 1] - p[i]); }
        } else if (damp != 1.0f) for (int i = 0; i < n; i++) u[i] = (u[i] - lam * su[i] * (p[i + 1] - p[i])) * damp;
        else              for (int i = 0; i < n; i++) u[i] -= lam * su[i] * (p[i + 1] - p[i]);
        if (kappa != 0.0f) {                                                   // the continuity carries the local speed too (the simple wave's c(1 + κ·p) on both sweeps)
            const float lk = lam * kappa, pc = pcap;
            for (int i = 1; i < n; i++) { const float pi = p[i]; pprev[i] = pi; const float pb = fminf(fmaxf(pi, -pc), pc); p[i] = pi - (lam + lk * pb) * (u[i] - u[i - 1]) * isp[i]; }
        } else for (int i = 1; i < n; i++) { const float pi = p[i]; pprev[i] = pi; p[i] = pi - lam * (u[i] - u[i - 1]) * isp[i]; }
        pprev[0] = p[0]; pprev[n] = p[n];
        if (nh > 0 && hopen != 0u) {                                           // step 81: the open holes — the branch's kick from the node's pressure, then its share of the continuity
            for (int k = 0; k < nh; k++) {
                if (((hopen >> k) & 1u) == 0u) continue;
                const int i = hnode[k];
                hlp[k] += hla * (hq[k] - hlp[k]);                                   // the hole's radiation one-pole (hla 0: the resistance flat)
                hq[k] += lam * hinv_l[k] * (pprev[i] - hr[k] * (hq[k] - hlp[k]));   // the hole's air kicked by the pressure before this step's continuity (the staggering as u's)
                p[i] -= lam * hq[k] * isp[i];                                  // the continuity: what leaves through the hole
            }
        }
        if (kappa != 0.0f && cq != 0.0f) {                                     // the limiter: the Lapidus diffusion in flux form, its loss logged
            // The loss Σ½S(p_old² − p_new²) over the updated nodes in closed form (summation by parts): Σ_i flux_i·d_i − p_n·flux_{n−1}
            // + p_0·flux_0 − ½Σ S·dp². The per-cell terms are written beside the updates (the two passes stay vectorised) and
            // summed after with eight independent accumulators: a running sum in the passes serialised them and cost the trumpet
            // a third of its render (the storm's XRuns at 13 voices); the lanes' order is the loss's rounding only.
            float flux[BORE_NODES_MAX + 1], lc[BORE_NODES_MAX + 1], ls[BORE_NODES_MAX + 1]; const float ck = cq * kappa;
            for (int i = 0; i < n; i++) { const float d = p[i + 1] - p[i]; const float fl = ck * fabsf(d) * d * su[i]; flux[i] = fl; lc[i] = fl * d; }
            ls[0] = 0.0f;
            for (int i = 1; i < n; i++) { const float dp = (flux[i] - flux[i - 1]) * isp[i]; p[i] += dp; ls[i] = sp[i] * dp * dp; }
            float a0 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, a4 = 0.0f, a5 = 0.0f, a6 = 0.0f, a7 = 0.0f; int i = 0;
            for (; i + 7 < n; i += 8) { a0 += lc[i] - 0.5f * ls[i]; a1 += lc[i + 1] - 0.5f * ls[i + 1]; a2 += lc[i + 2] - 0.5f * ls[i + 2]; a3 += lc[i + 3] - 0.5f * ls[i + 3]; a4 += lc[i + 4] - 0.5f * ls[i + 4]; a5 += lc[i + 5] - 0.5f * ls[i + 5]; a6 += lc[i + 6] - 0.5f * ls[i + 6]; a7 += lc[i + 7] - 0.5f * ls[i + 7]; }
            for (; i < n; i++) a0 += lc[i] - 0.5f * ls[i];
            const double loss = (double)(((a0 + a1) + (a2 + a3)) + ((a4 + a5) + (a6 + a7)) - p[n] * flux[n - 1] + p[0] * flux[0]);
            shock_loss += loss;
        }
        if (shear != 0.0f) { const float w = u[n - 1]; p[n - 1] -= shear * w * w * w; }
        // the radiation ports in the END's own characteristic impedance, z/S_end — a bell five times the tube's area
        // has a fifth of its impedance, and a port scaled in the tube's units chattered there (the flare's true
        // Courant limit read 0.927 against the interior's 1 until this)
        if (end0 == END_CLOSED) p[0] -= 2.0f * lam * (u[0] - q_b) / sp[0]; else { lp0 += rad_a * (u[0] - lp0); p[0] = -(z0 / sp[0]) * (u[0] - lp0) + p_src; if (leak_a > 0.0f) { lk0 += leak_a * (u[0] - lk0); p[0] -= (zleak / sp[0]) * lk0; } }
        if (end1 == END_CLOSED) p[n] += 2.0f * lam * u[n - 1] / sp[n]; else { lp1 += rad_a * (u[n - 1] - lp1); p[n] = (z1 / sp[n]) * (u[n - 1] - lp1); if (leak_a > 0.0f) { lk1 += leak_a * (u[n - 1] - lk1); p[n] += (zleak / sp[n]) * lk1; } }
        if (damp != 1.0f) for (int i = 0; i <= n; i++) p[i] *= damp;
        float out = u[n - 1];
        if (nh > 0 && hopen != 0u) for (int k = 0; k < nh; k++) if ((hopen >> k) & 1u) out += hq[k];   // step 81: what leaves through the open holes radiates too — the far end alone sits past the lattice, where the fundamental is evanescent (read there, a flute's holes left it −40 dB under its fifth partial)
        return out;
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
    float y2, v2, y2_prev;            // step 81: THE SECOND MODE — the lips' shearing (inward-striking) mode beside the swinging (outward) one
    float w2, damp2, g2;              //   its ω, damping and force gain; g2 = 0 is the one-mode valve as shipped, bit for bit
    float w, damp, g, h0, A;          // ω, the damping per sub-step, the force gain, the rest opening, the flow factor (A·√2 folded)
    float Sr;                         // the valve's area in the flow's units: its swept volume into the bore is ±Sr·v (the force's sign), its mass Sr/g
    inline float swept() const { return (outward ? Sr : -Sr) * v; }
    int   outward;                    // 0 the reed (closes with Δp), 1 the lips (open with it)
    float q;                          // the last flow
    float q_ap;                       // step 81: the aperture's flow alone a step ago (the channel's inertance reads it)
    uint32_t rng;                     // the breath's turbulence: the perturbation the static state grows from (a reed at rest is a fixed point)
    float nz, nk;                     // the turbulence low-passed (a one-pole at noise_hz; the reed's 2 kHz — white noise at the sub-rate seeded its upper registers; the lips' white)
    void reset() { y = v = y_prev = 0.0f; y2 = v2 = y2_prev = 0.0f; q = 0.0f; q_ap = 0.0f; rng = 0x2545F491u; nz = 0.0f; }
    inline float noise_raw() { rng = rng * 1664525u + 1013904223u; return ((float)(rng >> 8) * (1.0f / 8388608.0f)) - 1.0f; }
    inline float noise() { nz += (noise_raw() - nz) * nk; return nz; }
    void setup(float hz, float rate, float Q, float rest_opening, float close_pressure, float flow_area, int outward_, float noise_hz) {
        w = 2.0f * PI * hz / rate; if (w > 1.9f) w = 1.9f;
        damp = expf(-w / (2.0f * (Q > 0.1f ? Q : 0.1f)));
        h0 = rest_opening; g = close_pressure > 1e-9f ? h0 * w * w / close_pressure : 0.0f;
        A = flow_area * 1.41421356f; outward = outward_; Sr = 0.05f;
        nk = noise_hz > 0.0f ? 1.0f - expf(-2.0f * PI * noise_hz / rate) : 1.0f;   // 0: white
        w2 = w; damp2 = damp; g2 = 0.0f;
    }
    // Step 81 (TO_PROJECT_SPEC §13.11): THE TWO-MODE LIPS. A lip is not a door on a hinge: beside the swinging mode that
    // opens with the mouth's pressure (outward-striking, the valve as shipped) it carries a shearing mode of its surface
    // that closes with it (inward-striking), at its own resonance ratio·f and Q, with the share `shear` of the compliance
    // — the mucosal wave's two degrees of freedom. The aperture is h₀ + y + y2 (the shear's closing counted negative), the
    // swept volume the swing's alone; the shear's mass is Sr/g2 in the energy. With share 0 the second mode never moves.
    void setup_shear(float share, float ratio, float Q, float rate) {
        const float sh = share < 0.0f ? 0.0f : (share > 1.0f ? 1.0f : share);
        w2 = w * (ratio > 0.1f ? ratio : 0.1f); if (w2 > 1.9f) w2 = 1.9f;
        damp2 = expf(-w2 / (2.0f * (Q > 0.1f ? Q : 0.1f)));
        g2 = sh * g; (void)rate;
    }
    inline float opening() const { const float h = h0 + y + y2; return h > 0.0f ? h : 0.0f; }   // the force's sign sets the direction: the reed's y goes negative (closing), the lips' positive (opening); the shear closes (step 81)
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
        if (g2 > 0.0f) {                                                      // step 81: the shearing mode, inward-striking, its own spring and damping
            v2 -= w2 * w2 * y2;
            v2 -= g2 * dp;
            y2_prev = y2;
            y2 += v2;
            v2 *= damp2;
        }
    }
    // The energy in the form the kick–drift conserves exactly, ½v² + ½ω²·y_n·y_{n+1} (a unit mass; the symmetric
    // form wobbles by ω at the reed's own frequency and tripped the probe's first check).
    float energy() const { return (g > 1e-12f ? Sr / g : 0.0f) * 0.5f * (v * v + w * w * y * y_prev) + (g2 > 1e-12f ? Sr / g2 : 0.0f) * 0.5f * (v2 * v2 + w2 * w2 * y2 * y2_prev); }   // the masses Sr/g, Sr/g2
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

// ---------------------------------------------------------------------------
// Step 79 — the 2D membrane, the plate and the gong (TO_PROJECT_SPEC §13.9, DECISIONS_12 #3).
// ---------------------------------------------------------------------------
// THE MEMBRANE: nx × ny interior nodes (displacement u, momentum p) inside a ring of zeros — the SIMPLY SUPPORTED
// boundary, declared: u = 0 and Δ_h u = 0 on the edge, so the biharmonic is the five-point Laplacian applied twice through
// the same zero ring and the mode table is λ_mn = 4 sin²(mπ/(2(nx+1))) + 4 sin²(nπ/(2(ny+1))), ω²_mn = c²λ + κ²λ², in grid
// units (h = 1, Δt = 1 at the section's rate). The step: the KICK p += c²Δu − κ²Δ²u with every force read from the
// pre-update u (det J = 1 exactly), the DECLARED contraction p ← γ₀p + γ₂Δp (the mode at λ contracts by γ₀ − γ₂λ: the
// fundamental's T60, the brighter modes' faster — the air and the thermoelastic loss), then the DRIFT u += p. THE GONG
// (Berger / von Kármán): the mid-plane stretching E = Σ|∇u|² = −Σ u·Δu is one scalar read off the Laplacian pass and the
// kick's c² becomes c² + α·E — an exact momentum shear (the force depends on u alone), one multiply-add a node. THE CFL:
// the leapfrog's bound ε < 2 on the top mode, λ_2D = sqrt(8(c² + αE) + 64κ²) ≤ 1.95 at E_max, with the continuum's 8 and
// 64 (the grid's own λ_max is a hair under: the safe side). Four lanes a row (simd.h: the neighbours as unaligned loads,
// the tail scalar), the stretching summed by lane then in order — so the scalar fallback is the same arithmetic in the
// same order and the web gate's reference is bit for bit (DECISIONS_12 #1). Nothing here allocates.
constexpr int   MEMBRANE_MAX = 16;    // the per-voice mini-membrane's cap, nodes a side
constexpr int   PLATE_MAX = 64;       // the bus plate's cap (the mobile tier takes 32)
constexpr float MEMBRANE_CFL_GATE = 1.95f;        // the linear grid's bar (the leapfrog's 2, with 5 % of stiffness in hand)
constexpr float MEMBRANE_CFL_GATE_GONG = 1.80f;   // the gong's bar (measured: a pure membrane at swell 1 displaced to E_max with the top mode
                                                  // seeded diverged at exactly 1.95 — the (1,1)'s stretching modulates the top mode's stiffness
                                                  // by the whole swell at twice its own rate, non-adiabatic that close to the edge — and held at
                                                  // 1.90 for swells to 2; 1.80 is the bar with a margin, and the κ share lowers the depth further)
inline float membrane_lambda(int m, int n, int nx, int ny) {   // the Laplacian's eigenvalue λ_mn with the zero ring
    const float a = sinf(0.5f * PI * (float)m / (float)(nx + 1)), b = sinf(0.5f * PI * (float)n / (float)(ny + 1));
    return 4.0f * a * a + 4.0f * b * b;
}
inline float membrane_cfl(float c2, float k2, float alpha, float E) { return sqrtf(8.0f * (c2 + alpha * E) + 64.0f * k2); }

template <int CAP> struct Membrane {
    static constexpr int S = CAP + 2;              // the row stride: the interior and its ring
    float u[S * S], p[S * S];
    mutable float lap[S * S];                      // the step's scratch (Δu, then Δp); the diagnostics reuse it
    int   nx, ny;
    float c2, k2, alpha;                           // c², κ² and the gong's α, grid units
    float g0, g2;                                  // the contraction's (γ₀, γ₂); (1, 0) = undamped
    float E;                                       // E_stretch read off the last step's Laplacian pass
    float l11, lm;                                 // the grid's λ₁₁ and λ_max (cached at reset: the glide's retune calls no libm)

    void reset(int x, int y) {
        nx = x < 2 ? 2 : (x > CAP ? CAP : x); ny = y < 2 ? 2 : (y > CAP ? CAP : y);
        clear(); c2 = 0.0f; k2 = 0.0f; alpha = 0.0f; g0 = 1.0f; g2 = 0.0f;
        l11 = membrane_lambda(1, 1, nx, ny); lm = membrane_lambda(nx, ny, nx, ny);
    }
    void clear() { for (int i = 0; i < S * S; i++) u[i] = p[i] = lap[i] = 0.0f; E = 0.0f; }
    float lambda11() const { return l11; }
    float lambda_max() const { return lm; }
    // THE TUNING: from the fundamental's ε (= 2 sin(πf/rate), the magic circle's) and the biharmonic SHARE s of its
    // stiffness — c²λ₁₁ = (1 − s)ε², κ²λ₁₁² = s·ε² — the (1,1) mode lands on the note at any share: 0 a membrane whose
    // partials go as √λ, 1 a plate whose partials go as λ. The CFL at that tuning is ε times a factor of the grid alone.
    void tune(float eps, float share) {
        const float l = lambda11();
        c2 = (1.0f - share) * eps * eps / l; k2 = share * eps * eps / (l * l);
    }
    static float cfl_factor(int x, int y, float share, float swell) {   // λ_2D / ε at the cap's energy (c²_eff = c²(1 + swell))
        const float l = membrane_lambda(1, 1, x, y);
        return sqrtf(8.0f * (1.0f - share) * (1.0f + swell) / l + 64.0f * share / (l * l));
    }
    // THE DECLARED CONTRACTION from the fundamental's T60 and the extra decay rate per (λ/λ₁₁ − 1) of the brighter
    // modes (1/s, the lattice's law), at `rate` steps a second. The factor acts on the momentum, whose rate is twice
    // the amplitude's for a light damping — so an amplitude T60 of `t60` is the momentum's factor at twice the rate.
    void damp(float t60, float bright, float rate) {
        if (t60 <= 0.0f && bright <= 0.0f) { g0 = 1.0f; g2 = 0.0f; return; }
        const float l1 = lambda11(), lm = lambda_max();
        const float r1 = t60 > 0.0f ? 2.0f * 6.907755f / t60 : 0.0f;
        const float rm = r1 + 2.0f * (bright > 0.0f ? bright : 0.0f) * (lm / l1 - 1.0f);
        const float y1 = expf(-r1 / rate), ym = expf(-rm / rate);
        g2 = (y1 - ym) / (lm - l1); g0 = y1 + g2 * l1;
    }
    // DRIVE: a raised-cosine bump of momentum of radius r nodes centred at the fractions (fx, fy) of the sheet; and the
    // bump's Σw² (the cap's reference: a full strike's kinetic energy is ½a²Σw²)
    void strike(float a, float fx, float fy, float r) {
        const float cx = fx * (float)(nx + 1), cy = fy * (float)(ny + 1);
        for (int j = 1; j <= ny; j++) for (int i = 1; i <= nx; i++) {
            const float dx = (float)i - cx, dy = (float)j - cy, d = sqrtf(dx * dx + dy * dy);
            if (d < r) p[j * S + i] += a * 0.5f * (1.0f + cosf(PI * d / r));
        }
    }
    float strike_weight2(float fx, float fy, float r) const {
        const float cx = fx * (float)(nx + 1), cy = fy * (float)(ny + 1); float s = 0.0f;
        for (int j = 1; j <= ny; j++) for (int i = 1; i <= nx; i++) {
            const float dx = (float)i - cx, dy = (float)j - cy, d = sqrtf(dx * dx + dy * dy);
            if (d < r) { const float w = 0.5f * (1.0f + cosf(PI * d / r)); s += w * w; }
        }
        return s;
    }
    // DRIVE: one sample of momentum into one node (the bus plate's injection point of a voice)
    inline void inject(int node, float x) { p[node] += x; }
    int node_at(float fx, float fy) const {
        int i = (int)(fx * (float)(nx + 1) + 0.5f), j = (int)(fy * (float)(ny + 1) + 0.5f);
        if (i < 1) i = 1; if (i > nx) i = nx; if (j < 1) j = 1; if (j > ny) j = ny;
        return j * S + i;
    }
    // the pickup: u read bilinearly at the fractions (the ring reads zero)
    float pickup(float fx, float fy) const {
        const float x = fx * (float)(nx + 1), y = fy * (float)(ny + 1);
        int i = (int)x, j = (int)y; if (i < 0) i = 0; if (i > nx) i = nx; if (j < 0) j = 0; if (j > ny) j = ny;
        const float tx = x - (float)i, ty = y - (float)j;
        const float* r0 = u + j * S; const float* r1 = r0 + S;
        const float a = r0[i] + (r0[i + 1] - r0[i]) * tx, b = r1[i] + (r1[i + 1] - r1[i]) * tx;
        return a + (b - a) * ty;
    }
    // SYMPLECTIC + the declared contraction: one sub-step.
    inline void step() {
        const f32x4 four = f4_set1(4.0f);
        // (1) lap = Δu over the interior from the pre-update u, E = −Σ u·Δu: four lanes a row, the tail scalar, the
        //     lane sums added in order after the rows (((l0 + l1) + l2) + l3) + tail)
        f32x4 eacc = f4_set1(0.0f); float etail = 0.0f;
        for (int j = 1; j <= ny; j++) {
            const float* r = u + j * S; float* l = lap + j * S;
            int i = 1;
            for (; i + 3 <= nx; i += 4) {
                const f32x4 c = f4_load(r + i);
                const f32x4 L = f4_mul_sub(f4_add(f4_add(f4_load(r + i - 1), f4_load(r + i + 1)), f4_add(f4_load(r + i - S), f4_load(r + i + S))), four, c);
                f4_store(l + i, L);
                eacc = f4_mul_sub(eacc, c, L);
            }
            for (; i <= nx; i++) { const float c = r[i]; const float L = ((r[i - 1] + r[i + 1]) + (r[i - S] + r[i + S])) - 4.0f * c; l[i] = L; etail = etail - c * L; }
        }
        E = (((f4_lane(eacc, 0) + f4_lane(eacc, 1)) + f4_lane(eacc, 2)) + f4_lane(eacc, 3)) + etail;
        // (2) the kick from the pre-update u: p += (c² + αE)·Δu − κ²·Δ²u, the biharmonic the Laplacian of lap through the
        //     ring (zero: simply supported); with a uniform contraction (γ₂ = 0) the contraction and the drift ride the
        //     same pass
        const float ceff = c2 + alpha * E;
        const f32x4 vce = f4_set1(ceff), vk2 = f4_set1(k2), vg0 = f4_set1(g0), vg2 = f4_set1(g2);
        const bool biharm = k2 != 0.0f, graded = g2 != 0.0f, uniform = g0 != 1.0f;
        for (int j = 1; j <= ny; j++) {
            const float* l = lap + j * S; float* pp = p + j * S; float* uu = u + j * S;
            int i = 1;
            for (; i + 3 <= nx; i += 4) {
                const f32x4 L = f4_load(l + i);
                f32x4 P = f4_mul_add(vce, L, f4_load(pp + i));
                if (biharm) P = f4_mul_sub(P, vk2, f4_mul_sub(f4_add(f4_add(f4_load(l + i - 1), f4_load(l + i + 1)), f4_add(f4_load(l + i - S), f4_load(l + i + S))), four, L));
                if (!graded) { if (uniform) P = f4_mul(P, vg0); f4_store(pp + i, P); f4_store(uu + i, f4_add(f4_load(uu + i), P)); }
                else f4_store(pp + i, P);
            }
            for (; i <= nx; i++) {
                const float L = l[i];
                float P = pp[i] + ceff * L;
                if (biharm) P = P - k2 * (((l[i - 1] + l[i + 1]) + (l[i - S] + l[i + S])) - 4.0f * L);
                if (!graded) { if (uniform) P = P * g0; pp[i] = P; uu[i] = uu[i] + P; }
                else pp[i] = P;
            }
        }
        if (!graded) return;
        // (3) the graded contraction: lap = Δp from the kicked p (its ring zero), then p ← γ₀p + γ₂Δp and the drift
        for (int j = 1; j <= ny; j++) {
            const float* r = p + j * S; float* l = lap + j * S;
            int i = 1;
            for (; i + 3 <= nx; i += 4)
                f4_store(l + i, f4_mul_sub(f4_add(f4_add(f4_load(r + i - 1), f4_load(r + i + 1)), f4_add(f4_load(r + i - S), f4_load(r + i + S))), four, f4_load(r + i)));
            for (; i <= nx; i++) l[i] = ((r[i - 1] + r[i + 1]) + (r[i - S] + r[i + S])) - 4.0f * r[i];
        }
        for (int j = 1; j <= ny; j++) {
            const float* l = lap + j * S; float* pp = p + j * S; float* uu = u + j * S;
            int i = 1;
            for (; i + 3 <= nx; i += 4) {
                const f32x4 P = f4_mul_add(vg0, f4_load(pp + i), f4_mul(vg2, f4_load(l + i)));
                f4_store(pp + i, P); f4_store(uu + i, f4_add(f4_load(uu + i), P));
            }
            for (; i <= nx; i++) { const float P = g0 * pp[i] + g2 * l[i]; pp[i] = P; uu[i] = uu[i] + P; }
        }
    }
    // THE DIAGNOSTICS (never in the sub-step): the stretching at the current u, the plain energy
    // H = ½Σp² + ½c²E + ½κ²Σ(Δu)² (+ the gong's (α/4)E²), and the linear leapfrog's EXACT invariant
    // H̃ = ½(Σp² + c²E + κ²Σ(Δu)² + Σ p·(c²Δu − κ²Δ²u)) on the staggered pair after a step — the drift gate's reading.
    double stretch() const { laplacian_of_u(); double e = 0.0; for (int j = 1; j <= ny; j++) for (int i = 1; i <= nx; i++) e -= (double)u[j * S + i] * lap[j * S + i]; return e; }
    double kinetic() const { double k = 0.0; for (int j = 1; j <= ny; j++) for (int i = 1; i <= nx; i++) k += 0.5 * (double)p[j * S + i] * p[j * S + i]; return k; }
    double energy_plain() const {
        laplacian_of_u(); double e = 0.0, l2 = 0.0;
        for (int j = 1; j <= ny; j++) for (int i = 1; i <= nx; i++) { const int n = j * S + i; e -= (double)u[n] * lap[n]; l2 += (double)lap[n] * lap[n]; }
        return kinetic() + 0.5 * c2 * e + 0.5 * k2 * l2 + 0.25 * alpha * e * e;
    }
    double energy() const {
        laplacian_of_u(); double e = 0.0, l2 = 0.0, pf = 0.0;
        for (int j = 1; j <= ny; j++) for (int i = 1; i <= nx; i++) {
            const int n = j * S + i; const float L = lap[n];
            const float L2 = ((lap[n - 1] + lap[n + 1]) + (lap[n - S] + lap[n + S])) - 4.0f * L;
            e -= (double)u[n] * L; l2 += (double)L * L; pf += (double)p[n] * ((double)c2 * L - (double)k2 * L2);
        }
        return kinetic() + 0.5 * (c2 * e + k2 * l2 + pf);
    }
    // THE CAP: the plain energy (the gong's term included) never above H_cap after a strike — the momentum scaled down
    // to the remainder (the positions are the strike's own, and were under the cap before it)
    void cap(double h_cap) {
        const double h = energy_plain(); if (h <= h_cap) return;
        const double k = kinetic(), v = h - k;
        const double s = v >= h_cap || k <= 0.0 ? 0.0 : sqrt((h_cap - v) / k);
        for (int j = 1; j <= ny; j++) for (int i = 1; i <= nx; i++) p[j * S + i] *= (float)s;
    }
private:
    void laplacian_of_u() const {
        for (int j = 1; j <= ny; j++) for (int i = 1; i <= nx; i++) { const int n = j * S + i; lap[n] = ((u[n - 1] + u[n + 1]) + (u[n - S] + u[n + S])) - 4.0f * u[n]; }
    }
};

} // namespace suzu
