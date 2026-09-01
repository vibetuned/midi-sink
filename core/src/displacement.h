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
    SUMI_DEFORM_VORTEX      = 3,   // §4.3.3 vortex agitation
    SUMI_DEFORM_RESET       = 4,   // UV reset to identity (paper dip)
    SUMI_DEFORM_SCROLL      = 5    // §3.4 field motion: uniform translation
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
    float strength;      // A, max angular deflection (radians)
    float radius;        // R, exponential decay length
} sumi_deform_vortex_t;

typedef struct {
    float dx, dy;        // this frame's translation, canvas units (y-down)
} sumi_deform_scroll_t;

typedef struct {
    sumi_deform_type_t type;
    union {
        sumi_deform_drop_t   drop;
        sumi_deform_tine_t   tine;
        sumi_deform_vortex_t vortex;
        sumi_deform_scroll_t scroll;
    } as;
} sumi_deform_t;

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
