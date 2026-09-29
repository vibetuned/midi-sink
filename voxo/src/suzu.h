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

} // namespace suzu
