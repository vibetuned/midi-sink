// Internal deformation queue: the per-frame list of ping-pong passes to run
// (PROJECT_SPEC.md §4.1, §6). This layer is deliberately sokol-free — it only
// describes passes; renderer.cpp dispatches them (working rule 2).
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SUMI_DEFORM_PASSTHROUGH = 0,   // read-current/write-next (stress mode)
    SUMI_DEFORM_DROP        = 1,   // §4.3.1 circular drop expansion
    SUMI_DEFORM_TINE        = 2,   // §4.3.2 tine / comb stroke
    SUMI_DEFORM_VORTEX      = 3,   // §4.3.3 vortex agitation (two profiles, v0.4)
    SUMI_DEFORM_RESET       = 4,   // UV reset to identity (paper dip)
    SUMI_DEFORM_SCROLL      = 5,   // §3.4 field motion: uniform translation
    SUMI_DEFORM_WAKE        = 6,   // §4.3.4 dipolar wake (one ≤ a/2 sub-step)
    SUMI_DEFORM_PINCH       = 7,   // §4.3.5 Hamiltonian pinch (delta-driven)
    SUMI_DEFORM_RIPPLE      = 8,   // §4.3.6 sine ripple, bake pass (ΔA)
    SUMI_DEFORM_SWIRL       = 9,   // §4.3.7 Lamb-Oseen swirl (per-voice)
    SUMI_DEFORM_STOKESLET   = 10,  // v0.7 viscous stroke: 2-D unsteady Stokeslet
                                   //   displacement, one <= a/4 sub-step (DECISIONS_4 #53)
    SUMI_DEFORM_CHLADNI     = 11,  // v0.11 Chladni lattice: one kick-drift shear pair
                                   //   over the whole sheet (MEDIUM §2.2, Phase 6 step 37)
    SUMI_DEFORM_BURST       = 12,  // v0.12 viscous multipole burst: ONE age increment l0 -> l1
                                   //   of the impulse's displacement field (MEDIUM §2.3, step 38)
    SUMI_DEFORM_SPARK       = 13,  // v0.13 spark shear: ONE stage of the piecewise kick-drift
                                   //   (MEDIUM §2.4, step 39)
    SUMI_DEFORM_CHIRIKOV    = 14   // v0.14 Chirikov standard map (scaled): ONE stage of its
                                   //   kick-drift (MEDIUM §2.5, step 40)
} sumi_deform_type_t;

// All coordinates are normalized [0,1] canvas space (renderer converts to
// aspect-corrected space); lengths/radii are in units of canvas height.
typedef struct {
    float x, y;          // center C
    float radius;        // r
    float phase_base;    // parity-derived band base (1 or 2); 0 = clear water drop
    float aux;           // raw drop counter (per-drop selector, §4.2 aux channel)
} sumi_deform_drop_t;

typedef struct {
    float x0, y0, x1, y1;  // two points defining the line (L, D̂)
    float alpha;           // sharpness α
    float magnitude;       // z
} sumi_deform_tine_t;

typedef struct {
    float x, y;          // center V
    float strength;      // EXPONENTIAL: A, max deflection; RANKINE: ω (radians)
    float radius;        // EXPONENTIAL: decay length; RANKINE: rigid-core R
    uint32_t profile;    // sumi_vortex_profile_t (v0.4)
    float k, phase;      // v0.10 TORSION only: wavenumber (rad per canvas
                         //   height) and φ (rad); zero for the other profiles
} sumi_deform_vortex_t;

typedef struct {
    float dx, dy;        // this frame's translation, canvas units (y-down)
} sumi_deform_scroll_t;

typedef struct {         // §4.3.4 — ONE sub-step (displacement ≤ a/2, ensured
    float x, y;          //   upstream). Tip position AFTER this sub-step's
    float dx_ac, dy_ac;  //   motion; motion vector in aspect-corrected
    float tip_radius;    //   canvas-height units; tip radius a (same units).
} sumi_deform_wake_t;

typedef struct {         // §4.3.5 — k is ALWAYS a smoothed delta per pass.
    float x, y;          // pinch center
    float k;             // per-pass exponent (sign picks the compressed axis)
    float angle;         // fold-axis rotation, radians
    float window_s;      // S: streamline-value window scale
} sumi_deform_pinch_t;

typedef struct {         // §4.3.6 bake pass — amp is the per-pass ΔA; at fixed
    float amp;           //   (k, phase, angle) passes compose additively.
    float k;             // wavenumber, radians per canvas-height unit
    float phase;         // φ
    float angle;         // ripple frame rotation about the canvas center
} sumi_deform_ripple_t;

typedef struct {         // v0.7 — ONE sub-step of the viscous stroke: the impulse
    float x, y;          //   (Gaussian blob of radius a) that moved the tip by
    float dx_ac, dy_ac;  //   dvec, after momentum has diffused to l = spread·a.
    float tip_radius;    //   Displacement kernel in the stroke frame (x along d):
    float spread;        //   d_x = d/(2L)[Φ(S1)−Φ(S0)] − (d/L)(y²/r²)[χ(S1)−χ(S0)]
                         //   d_y = (d/L)(xy/r²)[χ(S1)−χ(S0)],  L = ln(l/a),
                         //   χ = (1−e^{−S})/S, Φ = χ + E1, S0 = r²/a², S1 = r²/l².
} sumi_deform_stokeslet_t;

typedef struct {         // §4.3.7 Lamb-Oseen: θ(r) = S/(2πr²)·(1−exp(−r²/r_c²))
    float x, y;          // center (the voice's position), normalized [0,1]
    float strength;      // S = Γ·Δt for this pass (SIGNED: band parity)
    float core_r;        // r_c = the voice's nominal boundary R
} sumi_deform_swirl_t;

typedef struct {         // v0.11 — ONE diagonal shear of the Taylor–Green splitting:
    float psi;           //   ψ = psi·cos(k_x(x−x0))·cos(k_y(y−y0)) = ½psi[cos(u−v) + cos(u+v)]
    float weight;        //   this wave's weight (1, or 1 − 2·balance for the second)
    float sx, x0;        //   lattice pitch and a cell centre along x, ASPECT-CORRECTED
    float sy, y0;        //   the same along y (canvas-height units); k = π/pitch
    uint32_t stage;      //   0 = the wave cos(u−v), sheared along (k_y, k_x); 1 = cos(u+v), along (−k_y, k_x)
} sumi_deform_chladni_t;

typedef struct {         // v0.12 — ONE age increment of the viscous multipole burst (MEDIUM §2.3):
    float x, y;          //   centre, normalized; the impulse of core a, aged from l0 to l1
    float a;             //   core radius (canvas heights) — the (a/r)^(m−2) scale of the orders above 2
    float amp;           //   A_m: Ψ = A_m (a/r)^(m−2) sin(m(θ−θ0)) [Φ_m(r²/l1²) − Φ_m(r²/l0²)], d = ∇⊥Ψ
    float l0, l1;        //   the increment's ages, canvas heights (a ≤ l0 < l1)
    float theta0;        //   the ejection axis, radians in the canvas frame (y down)
    uint32_t m;          //   the order, 2..8 (2 = the quadrupole)
} sumi_deform_burst_t;

typedef struct {         // v0.13 — ONE stage of the spark shear's kick-drift (MEDIUM §2.4):
    float x, y;          //   the strike, normalized — the shear frame's origin
    float amp;           //   A (stage 0) or B (stage 1), canvas heights, signed: the peak kick
    float k;             //   base wavenumber, rad per canvas height; the octaves 2k, 4k, … stack deep
    float phase;         //   φ of the base octave (the others offset from it)
    float theta0;        //   frame rotation, radians (canvas frame, y down)
    float band;          //   the window's half-width ACROSS the shear, canvas heights; 0 = the whole canvas
    uint32_t stack;      //   octaves, 1..4, weights 1, ½, ¼, ⅛ (normalised so |f| ≤ 1)
    uint32_t profile;    //   0 triangle, 1 piecewise-linear hash noise
    uint32_t stage;      //   0: lx += A·w(ly)·f(ly);  1: ly += B·w(lx)·f(lx)  (lx, ly in the frame)
} sumi_deform_spark_t;

typedef struct {         // v0.14 — ONE stage of the scaled Chirikov standard map (MEDIUM §2.5):
    float x, y;          //   the centre, normalized: the kick's phase origin and the drift's zero line
    float amp;           //   stage 0, the KICK: A, canvas heights, signed — y1 = y + A·sin(k(x−xc) + φ)
    float k;             //   the kick's wavenumber, rad per canvas height along x
    float phase;         //   φ
    float eps;           //   stage 1, the DRIFT: ε, signed — x1 = x + ε·(y1 − yc)
    uint32_t stage;      //   the step's chaos parameter is K = A·k·ε (Greene's threshold ≈ 0.9716)
} sumi_deform_chirikov_t;

typedef struct {
    sumi_deform_type_t type;
    union {
        sumi_deform_drop_t   drop;
        sumi_deform_tine_t   tine;
        sumi_deform_vortex_t vortex;
        sumi_deform_scroll_t scroll;
        sumi_deform_wake_t   wake;
        sumi_deform_pinch_t  pinch;
        sumi_deform_ripple_t ripple;
        sumi_deform_swirl_t  swirl;
        sumi_deform_stokeslet_t stokeslet;
        sumi_deform_chladni_t chladni;
        sumi_deform_burst_t  burst;
        sumi_deform_spark_t  spark;
        sumi_deform_chirikov_t chirikov;
    } as;
} sumi_deform_t;


// v0.12 (Phase 6 step 38): the burst's mathematics in double — shared by the
// mapper's episode, the tests and the harness; the shader (deform.glsl
// burst_fs) carries the same formulas in float. tools/multipole_verify.py is
// the derivation's paper trail.
//   γ_m(s) = 1 − e^{−s} Σ_{k<m} s^k/k!        the viscous cutoff (P(m, s))
//   Φ_m(S) = ∫_S^∞ γ_m(s)/s² ds = γ_m(S)/S + e^{−S} Σ_{k≤m−2} S^k/k! /(m−1)   (m ≥ 2; Φ_2 = χ)
#define SUMI_BURST_M_MIN 2u
#define SUMI_BURST_M_MAX 8u
double sumi_burst_phi (uint32_t m, double S);
double sumi_burst_dphi(uint32_t m, double S0, double S1);   // Φ_m(S1) − Φ_m(S0), S1 ≤ S0, stable at the core
double sumi_burst_gs  (uint32_t m, double S);               // γ_m(S)/S
// A_m from the API amplitude: D = the radial displacement at r = a on the
// ejection axis over the whole burst a -> l_end (D < 0: the first-order inverse).
double sumi_burst_amp (uint32_t m, double D, double a, double l_end);
// The peak |d| of the increment l0 -> l1 — it lies on the ejection axis.
double sumi_burst_peak(uint32_t m, double amp, double a, double l0, double l1);
// The per-pass budget β_m: a pass keeps |∇d| ≤ 0.25 (det ≥ 0.5, the wake's
// a/4 criterion) while its peak displacement ≤ β_m · l0 (multipole_verify §8).
double sumi_burst_budget(uint32_t m);
// The greedy march: the largest l' ≤ l1 whose increment l0 -> l' is within budget.
double sumi_burst_step(uint32_t m, double amp, double a, double l0, double l1);

// Crossed-tine pinch variant (v0.4, DECISIONS_3 #34): fills TWO tine passes
// reproducing the step-19 prototype — one along the fold axis, one along the
// perpendicular — through (x, y), in normalized coords. k's sign reverses
// both directions; magnitude maps as |k| * 0.2 (the calibration that matched
// the Hamiltonian pair's visual pace in the pick-by-eye demo).
void sumi_deform_crossed_pinch(float x, float y, float dir_x, float dir_y,
                               float k, sumi_deform_t out[2]);

typedef struct sumi_deform_queue_t sumi_deform_queue_t;

sumi_deform_queue_t* sumi_deform_queue_create(uint32_t capacity);
void     sumi_deform_queue_destroy(sumi_deform_queue_t* q);
// Returns false (and drops the pass) when the queue is full.
bool     sumi_deform_queue_push (sumi_deform_queue_t* q, const sumi_deform_t* deform);
uint32_t sumi_deform_queue_count(const sumi_deform_queue_t* q);
const sumi_deform_t* sumi_deform_queue_at(const sumi_deform_queue_t* q, uint32_t index);
void     sumi_deform_queue_clear(sumi_deform_queue_t* q);

// v0.13 (Phase 6 step 39): one kick-drift step of the spark shear onto a
// queue — the x-shear A (skipped when 0) then the y-shear B (skipped when 0),
// each an exact pass. The exact inverse of the step (A, B) is the step (0, −B)
// followed by the step (−A, 0): reversed order, negated. Returns the passes
// pushed (0..2) for the caller's budget accounting.
uint32_t sumi_spark_emit_step(sumi_deform_queue_t* q, float x, float y, float A, float B,
                              float k, float phase, float theta0, float band,
                              uint32_t stack, uint32_t profile);

// v0.14 (Phase 6 step 40): one step of the scaled Chirikov standard map onto
// a queue — the kick y1 = y + A·sin(k(x−xc) + φ) then the drift x1 = x +
// ε·(y1 − yc), two exact shears (K = A·k·ε). `inverse` applies the step's
// EXACT inverse: the drift undone first (−ε), then the kick (−A) — reversed
// order, negated. Returns the passes pushed (0..2).
uint32_t sumi_chirikov_emit_step(sumi_deform_queue_t* q, float x, float y, float A, float k,
                                 float phase, float eps, bool inverse);

#ifdef __cplusplus
}
#endif
