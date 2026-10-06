/* sumi_trace.h — THE ORBIT TRACE as a host library (Phase 10 step 67's app
 * fixes, DECISIONS_9 #11): the bridge from Voxo to libsumi at frame rate that
 * step 59 built into the desktop shell (orbit_trace.h, the C++ class beside
 * this header), now shared by every native shell through a pure-C surface so
 * the tablets draw the synth's orbits the way the desktop does — one
 * implementation, as hostmpe and the presets are. It links the core and Voxo;
 * it touches no GPU and no device.
 *
 * Per frame, BEFORE sumi_update: poll Voxo's traced voices, place each at its
 * note's cell (the layout probe, under the engine's fingering state), and
 *   - on the INK route lay the orbit into the water as tine or wake segments
 *     through the gesture ABI (budgeted: at most 24 a frame over all voices,
 *     merged within a voice, never a voice dropped) — every segment reported
 *     to the gesture hook, so a recording carries the trace;
 *   - on the CANVAS route hand the polylines to libsumi's scope view
 *     (sumi_set_scope: over the water, or alone on the dark glass).
 * With both off the water is bit-identical to a world without the trace.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "sumi_core.h"
#include "voxo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sumi_trace_t sumi_trace_t;

typedef struct sumi_trace_config_t {
    uint32_t scope;      /* keep the polylines for a shell's own scope drawing (the desktop's miniature); 0/1 */
    uint32_t ink;        /* the gesture route: segments into the water at the voice's cell; 0/1              */
    uint32_t kinds;      /* the voice kinds traced, bit k; the desktop's default is the rotor (1 << 5), a
                            tablet's is every kind (0x1FF) — its patch picks the kind that sounds             */
    float    scale;      /* canvas heights per unit orbit amplitude (default 0.25)                          */
    uint32_t segments;   /* segments per voice per frame, 4..8 (default 6)                                  */
    uint32_t stroke;     /* 0 tine (exact), 1 wake (sub-stepped, the stylus doublet)                       */
    uint32_t canvas;     /* the scope on the canvas: 0 off, 1 over the water, 2 the scope alone              */
} sumi_trace_config_t;

/* Every ink segment, as the core's call: kind 5 (tine: x0 y0 x1 y1 alpha magnitude) or 6 (wake: x0 y0 x1 y1 tip) —
   the replay library's gesture kinds, so a shell forwards them to sumi_replay_rec_gesture while recording. */
typedef void (*sumi_trace_gesture_fn)(void* user, uint32_t kind, const float* args, uint32_t n);

void          sumi_trace_default_config(sumi_trace_config_t* out);
sumi_trace_t* sumi_trace_create(void);
void          sumi_trace_destroy(sumi_trace_t* t);
void          sumi_trace_configure(sumi_trace_t* t, const sumi_trace_config_t* config);
void          sumi_trace_set_gesture_hook(sumi_trace_t* t, sumi_trace_gesture_fn fn, void* user);
/* One frame. `inst` may be NULL (the polylines are kept, nothing is drawn); `aspect` is width over height. Call it
   before sumi_update so the segments land in this frame's passes; never while a replay plays (the recording carries
   its own). With canvas 0 it also clears the core's scope, so a shell that stops calling it should clear once. */
void          sumi_trace_frame(sumi_trace_t* t, voxo_t* voxo, sumi_instance_t* inst, const sumi_params_t* params, float aspect);
uint32_t      sumi_trace_voices(const sumi_trace_t* t);     /* traced voices this frame */
uint32_t      sumi_trace_emitted(const sumi_trace_t* t);    /* ink segments this frame */
uint64_t      sumi_trace_total_emitted(const sumi_trace_t* t);

#ifdef __cplusplus
}
#endif
