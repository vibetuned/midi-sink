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
    SUMI_DEFORM_RIPPLE      = 8    // §4.3.6 sine ripple, bake pass (ΔA)
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
    } as;
} sumi_deform_t;

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

#ifdef __cplusplus
}
#endif
