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
 *
 * Step 75 (KESSHO §2.5): THE POINCARÉ MODE — the trace's second mode. Voxo's
 * reference phasor at the bass frequency (or a voice's own drive clock) strobes
 * every traced voice: at each wrap its pair is a RETURN (voxo_strobe_poll).
 * On the ink route each return stamps a micro-drop (Sumi), a charge speck
 * (Anod — the continuous trace lays them along the orbit there too, at the
 * same rate: only charge glows, the author's choice) or a micro-swirl (Kessho) at the voice's cell
 * through the gesture ABI, budgeted as the segments are; on the scope routes a
 * dust point. 1:1 one stationary point,
 * 3:2 a two-point orbit, incommensurate a dotted ring, a high-K rotor dust.
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
    uint32_t mode;       /* step 75: SUMI_TRACE_CONTINUOUS 0 (the segments above), SUMI_TRACE_POINCARE 1 (the
                            returns at the bass phasor's wraps), SUMI_TRACE_POINCARE_OWN 2 (each voice's own
                            drive clock where it has one — the Duffing's drive, the rotor's kick — else the bass) */
} sumi_trace_config_t;
#define SUMI_TRACE_CONTINUOUS   0u
#define SUMI_TRACE_POINCARE     1u
#define SUMI_TRACE_POINCARE_OWN 2u

/* Every ink segment, as the core's call: kind 5 (tine: x0 y0 x1 y1 alpha magnitude) or 6 (wake: x0 y0 x1 y1 tip) —
   the replay library's gesture kinds, so a shell forwards them to sumi_replay_rec_gesture while recording. Step 75: in the
   Poincaré mode every stamp too — kind 0 (tap: x y radius) under Sumi, kind 2 (twist: x y strength radius profile) under
   Kessho — and under Anod the charge specks of both modes, kind 7 (drop: x y radius layer). */
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
uint32_t      sumi_trace_emitted(const sumi_trace_t* t);    /* ink segments this frame (step 75: stamps, in the Poincaré mode) */
uint64_t      sumi_trace_total_emitted(const sumi_trace_t* t);

#ifdef __cplusplus
}
#endif
