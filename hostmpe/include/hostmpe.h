/* hostmpe — shared HOST-side MPE generation library (PHASE4_SPEC.md §3–§5).
   Lives OUTSIDE core/: the core never generates MIDI. One implementation
   consumed by both tablet shells — Swift via module.modulemap, Kotlin via
   the JNI layer — so the joystick math, allocator, and rate limiter cannot
   drift between platforms.

   PURE C HEADER, exactly like sumi_core.h (working rule): no STL, no C++
   types cross this boundary — the C++ lives behind it. Enforced by
   tests/hostmpe_c_compile.c.

   Step 16 scope: the §3 joystick→MIDI mapping, the §5.1 voice allocator
   with external-occupancy masking, and the session MCM/RPN0 config. The
   outbound rate limiter lands in Step 17. */
#ifndef HOSTMPE_H
#define HOSTMPE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* §3.2 radial deadband with a soft knee (no zipper):
     d = ‖Δ‖ / R_max, clamped to 1
     g = 0                    if d <= 0.03
     g = (d − 0.03) / 0.97    otherwise      (smooth from exactly 0)
   Returns g in [0, 1]. A hard threshold would make the first vibrato wiggle
   jump from 0 to 3% — audible zipper on the outbound pipe. */
float hostmpe_soft_knee(float d);

/* Δ_eff = Δ̂ · g(‖Δ‖ / r_max): the joystick's effective deflection, a
   direction-preserving vector of magnitude g ∈ [0, 1]. Δ and r_max must be
   in ONE metric (the shells use canvas-height units, matching the probe's
   cell_radius). Zero-length Δ and r_max <= 0 yield (0, 0). */
void hostmpe_joystick_eff(float dx, float dy, float r_max,
                          float* out_x, float* out_y);

/* BEND deflection (DECISIONS_3 #10): the knee is a DEADBAND, not a travel
   limit — inside the circle it equals the soft knee; beyond it, identity
   (continuous at d = 1, where both equal 1). This is what makes a one-cell
   drag exactly one semitone regardless of the cell_radius/semitone_step
   ratio: far from the origin the bend tracks the finger absolutely. The
   CLAMPED knee stays the law for the bounded axes (CC74, the indicator). */
float hostmpe_bend_deflection(float d);

/* §3.3 semitone-exact 14-bit bend, pb_range = 48 (what the MCM declares):
   pb = 8192 + round(semitones / 48 * 8192), clamped to [0, 16383].
   One semitone = ±171 counts (8192/48 = 170.67). */
uint16_t hostmpe_bend14(float semitones);

/* ---- §5.1 voice allocator + §3 touch → MPE byte generation -------------- */

/* One MPE lower zone: master channel 0 (MIDI ch 1), members 1..15. All calls
   must be externally serialized (the shells' single-producer MIDI queue —
   §5.2 — is exactly that context). Timestamps are seconds, any monotonic
   clock. */
typedef struct hostmpe_t hostmpe_t;

typedef struct {
    uint8_t status;   /* complete 3-byte MIDI message (status | channel)     */
    uint8_t data1;
    uint8_t data2;
} hostmpe_msg_t;

#define HOSTMPE_MEMBERS        15
#define HOSTMPE_BEND_RANGE     48   /* semitones, declared by the session MCM */
#define HOSTMPE_EXT_TIMEOUT_S  30.0 /* stuck external note: occupancy clears  */

/* Absolute deadband floor, canvas-height units (DECISIONS_3 #16): finger
   jitter is an absolute quantity (a few points of wobble), but §3.2's
   deadband is 3% OF R_MAX — sub-pixel on small-celled layouts (Jankó R_max
   ≈ 0.018 -> 3% ≈ half a pixel), so every micro-wobble bent pitch. The
   working deadband is max(0.03 · r_max, this floor); the knee stays smooth
   and still reaches 1 exactly at the circle. Applies inside
   hostmpe_joystick_eff and hostmpe_touch_update (both know r_max); the bare
   reference forms hostmpe_soft_knee / hostmpe_bend_deflection keep the
   normalized 3% knee. */
#define HOSTMPE_KNEE_FLOOR_CH  0.006f

hostmpe_t* hostmpe_create(void);
void       hostmpe_destroy(hostmpe_t* h);

/* MPE configuration for session open / "Re-sync" (§5.3): MCM (RPN 6, lower
   zone, 15 members) on the master, then RPN 0 = 48 on every member channel
   (RPN null terminators included). Also pushed into the LOOPBACK when Play
   mode activates, making the normalizer's mode flip deterministic (roadmap
   working rule). Needs room for ~85 messages; returns the count written. */
uint32_t hostmpe_session_config(hostmpe_t* h, hostmpe_msg_t* out, uint32_t max);

/* Touch-down (§5.1 emit order): CENTER BEND first, then Note On, on the
   least-recently-released free member channel that is not externally
   occupied. Returns the voice id (the member channel, 1..15), or -1 on
   saturation — silent drop, no messages (the host blinks a HUD instead;
   never steal). r_max comes from the probe at the touch cell; grad_x/grad_y
   is the LOCAL PITCH GRADIENT of the lattice at that cell (semitones per
   canvas-height unit toward +x / +y screen space, DECISIONS_3 #17): the
   shell derives it from the probed neighbor cells, so bend follows the
   lattice's own 2D pitch geometry — grid rows are octaves, Jankó columns
   are whole tones and its rows the alternating ±1. A pure #7-axis surface
   passes (1/semitone_step, 0). Stored for the voice's lifetime. `out` needs
   room for 2 messages. */
int32_t hostmpe_touch_begin(hostmpe_t* h, double now, uint8_t note, uint8_t velocity,
                            float r_max, float grad_x, float grad_y,
                            hostmpe_msg_t* out, uint32_t max, uint32_t* out_count);

/* Joystick update (§3.3 rev, finger rows). dx/dy are the RAW touch delta
   from the touch-down origin, SCREEN-oriented (y grows down), in the same
   metric as r_max.
   Bend → 14-bit: semitones = bend_deflection(d)/d · (grad_x·dx + grad_y·dy)
     — the deadband is radial; beyond the joystick circle the bend tracks the
     finger's lattice-pitch displacement absolutely (#10).
   Y → CHANNEL PRESSURE, upward only (§3.3 rev / DECISIONS_3 #19): pressure
     is the upward component of the CLAMPED joystick — 0 at touch-down, 0 for
     any downward Δy, monotonic through the soft knee, 127 at full-radius
     straight up. Glass has no force sensor; pushing INTO the lattice
     upward is the growth gesture. FINGERS EMIT NO CC74 (the stylus matrix
     in Step 18 owns timbre).
   Only changed byte values are emitted (identical repeats are noise, not
   information; this is NOT the outbound decimation — that is Step 17 and
   per-transport). `out` needs room for 2 messages; returns the count. */
uint32_t hostmpe_touch_update(hostmpe_t* h, int32_t voice,
                              float dx, float dy,
                              hostmpe_msg_t* out, uint32_t max);

/* Lift (§5.1 emit order): pressure 0 FIRST (release tails on synths gating
   on pressure), then Note Off with the lift velocity (64 when unmeasured).
   The channel returns to the allocator as most-recently-released. `out`
   needs room for 2 messages. */
uint32_t hostmpe_touch_end(hostmpe_t* h, int32_t voice, double now, uint8_t lift,
                           hostmpe_msg_t* out, uint32_t max);

/* External-occupancy masking (§5.1): feed EVERY byte from hardware devices
   at the merge point. A member channel with an active external note is
   unavailable to the allocator; occupancy clears on the external Note Off,
   on hostmpe_external_clear (device disconnect), or after
   HOSTMPE_EXT_TIMEOUT_S with no traffic on that channel (any message on the
   channel refreshes the clock — a held ROLI note streams pressure
   continuously, so real holds never time out). */
void hostmpe_observe_external(hostmpe_t* h, double now,
                              uint8_t status, uint8_t data1, uint8_t data2);
void hostmpe_external_clear(hostmpe_t* h);

/* Diagnostics / tests. */
uint32_t hostmpe_active_voices(const hostmpe_t* h);

#ifdef __cplusplus
}
#endif
#endif /* HOSTMPE_H */
