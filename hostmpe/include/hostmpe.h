/* hostmpe — shared HOST-side MPE generation library (PROJECT_SPEC.md §8.3–§8.5).
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
   Y is BIPOLAR (v0.4, PROJECT_SPEC.md §8.3): one radial soft-knee serves both
     halves of the CLAMPED joystick. UP (screen -y) → channel pressure 0xD0
     (the ink feed) — 0 at touch-down, 127 at full-radius straight up. DOWN
     → polyphonic key pressure 0xA0 on the voice's member channel, keyed by
     ITS NOTE (the Lamb-Oseen swirl). Center = both zeros; push away = feed,
     pull back = stir, both live with no mode flip. Glass has no force
     sensor; the axis is the sensor. FINGERS EMIT NO CC74 (the stylus owns
     timbre).
   Only changed byte values are emitted (identical repeats are noise, not
   information; this is NOT the outbound decimation — that is Step 17 and
   per-transport). `out` needs room for 3 messages; returns the count. */
uint32_t hostmpe_touch_update(hostmpe_t* h, int32_t voice,
                              float dx, float dy,
                              hostmpe_msg_t* out, uint32_t max);

/* Lift (§5.1 emit order): pressure 0 FIRST (release tails on synths gating
   on pressure), an engaged swirl half's 0xA0 0 next (v0.4 — no latched poly
   AT), then Note Off with the lift velocity (64 when unmeasured). The
   channel returns to the allocator as most-recently-released. `out` needs
   room for 3 messages. */
uint32_t hostmpe_touch_end(hostmpe_t* h, int32_t voice, double now, uint8_t lift,
                           hostmpe_msg_t* out, uint32_t max);

/* MIDI panic: release EVERY active touch voice (pressure 0 then Note Off,
   §5.1 emit order) and then silence the whole zone — CC 64 = 0 (sustain) and
   CC 123 = 0 (All Notes Off) on the master and every member channel. Use for
   an explicit panic control and before teardown. All output is exempt from
   rate limiting. `out` needs room for 3*15 + 2*16 = 77 messages; returns the
   count. Voices return to the allocator. */
uint32_t hostmpe_panic(hostmpe_t* h, double now, hostmpe_msg_t* out, uint32_t max);

/* Stateless zone silence: CC 64 = 0 + CC 123 = 0 on master and all member
   channels, WITHOUT touching the voice table. This is what a departing
   transport needs — a sink that stops receiving must not be left with hung
   notes, while voices still sounding on the other pipes keep playing.
   `out` needs room for 32 messages; returns the count. */
uint32_t hostmpe_silence_zone(hostmpe_msg_t* out, uint32_t max);

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

/* ---- echo suppression (#66) ----------------------------------------------- */

/* A transport that mirrors our own output back — a DAW with MIDI thru, a
   bridged virtual source, a network session looping — makes the shell
   consume its OWN bytes as if a device had played them. Measured on the iPad:
   99.5% of "external" input was our own output, median round trip 0.3 ms.
   The damage is real: hostmpe_observe_external then marks OUR channels
   externally held (the allocator can starve into silent drops), and the
   loopback paints every note twice.

   Contract: record every message the shell DELIVERS to a transport, and test
   every message arriving from a device. A match inside the window is
   CONSUMED, so a real device repeating the same bytes later is never
   swallowed — at most one echo is dropped per emission. */
#define HOSTMPE_ECHO_WINDOW_S 0.30
void     hostmpe_echo_record (hostmpe_t* h, double now,
                              uint8_t status, uint8_t d1, uint8_t d2);
bool     hostmpe_echo_is_ours(hostmpe_t* h, double now,
                              uint8_t status, uint8_t d1, uint8_t d2);
uint32_t hostmpe_echo_dropped(const hostmpe_t* h);   /* diagnostics */

/* ---- §7 stylus legato engine (Step 21) ------------------------------------ */

/* The pen abandons the joystick: ABSOLUTE-POSITION play. The SHELL owns the
   probe (cell under the pen, displacement along the anchor cell's semitone
   axis); hostmpe owns pitch state — bends, the ±47-semitone same-channel
   re-anchor, the piano-grid retune ramp — so the golden traces live here,
   headless, shared with Android later. Voices come from the same §5.1
   allocator; a pen voice ends through hostmpe_touch_end like any other. */

/* Pen-down: center bend then Note On (§5.1 emit order), velocity from real
   tip force. Marks the voice as a pen voice. `out` needs room for 2. */
int32_t  hostmpe_pen_begin(hostmpe_t* h, double now, uint8_t note, uint8_t velocity,
                           hostmpe_msg_t* out, uint32_t max, uint32_t* out_count);

/* LEGATO GLISSANDO (all playable layouts — #39, the stylus's reason to
   exist: the finger joystick already bends continuously, so the pen makes
   REAL NOTE CHANGES). The shell probes the cell under the pen each move and
   passes its note plus the pen's offset from that cell's center along the
   semitone axis (in semitones, ±~0.5 inside a cell). Same cell: the offset
   is the bend (change-only) — vibrato inside the cell. NEW cell: a
   same-channel legato retrigger — bend(offset) → Note On(new, velocity from
   the CURRENT force) → Note Off(old) — the classic legato overlap: mono/MPE
   synths glide, the DAW records real terminated notes, and the attack is in
   tune because the bend precedes the Note On. Pitch stays continuous across
   the crossing (offset flips sign as the reference cell changes). Dead
   zones: the shell simply makes no calls — the last pitch sustains.
   Boundary HYSTERESIS: a crossing only commits once the pen is
   ±HOSTMPE_PEN_HYST semitones past the CURRENT note — until then the true
   pitch offset keeps bending the current note, so vibrato near a cell edge
   bends instead of machine-gunning retriggers.
   `bend_scale` (#40) multiplies the EMITTED bend only — and 0 is legal and
   meaningful: the pen's bend is GESTURE-GATED (no azimuth swing -> scale 0 ->
   pure quantized legato, attacks exactly on the note; the swing brings the
   bend into existence up to ×3 and its decay settles pitch back to nominal).
   Note tracking and hysteresis stay on raw geometry, so cells commit where
   the pen physically is.
   `out` needs room for 3. */
#define HOSTMPE_PEN_HYST 0.65f
uint32_t hostmpe_pen_glide(hostmpe_t* h, int32_t voice, uint8_t cell_note,
                           float offset_semis, float bend_scale, uint8_t velocity,
                           hostmpe_msg_t* out, uint32_t max);

/* §3.3 stylus CC74: eff in [-1, 1] (the shell's knee-shaped Δy, up = +) ->
   cc = clamp(64 + round(eff·63), 0, 127), change-only. `out` room for 1. */
uint32_t hostmpe_pen_slide(hostmpe_t* h, int32_t voice, float eff,
                           hostmpe_msg_t* out, uint32_t max);

/* True tip force (0..1) -> channel pressure, change-only. `out` room for 1. */
uint32_t hostmpe_pen_pressure(hostmpe_t* h, int32_t voice, float force,
                              hostmpe_msg_t* out, uint32_t max);

/* ---- §5.3 outbound rate limiter (one instance PER TRANSPORT) ------------- */

/* The loopback is full-rate; every OUTBOUND transport gets change-only
   filtering plus its own policy (per-transport budgets are independent —
   DECISIONS_3 #3). Same external-serialization contract as hostmpe_t. */
typedef struct hostmpe_limiter_t hostmpe_limiter_t;

/* Per-voice, per-dimension latest-wins decimation to <= rate_hz — the
   virtual-source / Network-Session / MidiDeviceService class of links. */
hostmpe_limiter_t* hostmpe_limiter_create_rate(float rate_hz);

/* Global budget of ~msgs_per_s with round-robin per-voice fairness across
   dimensions (one wiggling finger cannot starve nine) — the BLE class.
   Exempt messages bypass the budget entirely (it applies to continuous
   dimensions only, §5.3). */
hostmpe_limiter_t* hostmpe_limiter_create_budget(float msgs_per_s);

void hostmpe_limiter_destroy(hostmpe_limiter_t* l);

/* Push one generated message. `exempt` marks what is never decimated or
   dropped (§5.3): Note On/Off, the initial center bend,
   pressure-0-before-Note-Off, session config — in practice, everything
   hostmpe_touch_begin/end and hostmpe_session_config emit; only
   hostmpe_touch_update output is non-exempt. Change-only filtering happens
   here (an identical PB/CC74/pressure per channel is never resent).
   Messages ready to send NOW are written to `out`; held messages surface
   from hostmpe_limiter_drain — call it regularly (each frame is plenty).
   Returns the count written. */
uint32_t hostmpe_limiter_push(hostmpe_limiter_t* l, double now,
                              hostmpe_msg_t msg, bool exempt,
                              hostmpe_msg_t* out, uint32_t max);
uint32_t hostmpe_limiter_drain(hostmpe_limiter_t* l, double now,
                               hostmpe_msg_t* out, uint32_t max);

/* ---- §8 performance control strip widget engines (Step 18) --------------- */

/* Value engines for the master-channel control strip: spring wheel (Pitch —
   master bend, ±2 by the MPE master default; the strip never sends RPN 0 on
   ch 1), latch wheels (Mod CC 1 + two assignable), momentary/toggle button
   (Sustain CC 64). EVERY message is on the master channel — the allocator's
   member channels are never touched (§8 channel discipline; unit-tested).
   All emission is change-only. Same external-serialization contract as
   hostmpe_t.

   Never-dropped class (§8): the shell passes BUTTON messages (CC 64) to the
   limiters with exempt=true — a decimated sustain-off is a stuck pedal.
   Wheels are ordinary continuous dimensions under each transport's policy:
   the limiter tracks every MASTER-channel CC as its own slot (DECISIONS_3
   #30 — before Step 18, generic CCs bypassed the policies entirely). */
typedef struct hostmpe_strip_t hostmpe_strip_t;

/* Latch wheel ids. */
#define HOSTMPE_STRIP_MOD       0   /* CC 1, fixed                            */
#define HOSTMPE_STRIP_ASSIGN_A  1   /* default CC 23 (loopback: viscosity)    */
#define HOSTMPE_STRIP_ASSIGN_B  2   /* default CC 24 (loopback: roughness)    */

hostmpe_strip_t* hostmpe_strip_create(void);
void             hostmpe_strip_destroy(hostmpe_strip_t* s);

/* Spring wheel (Pitch). While grabbed the shell feeds the CLAMPED joystick
   deflection v in [-1, 1] (§3.2 knee via hostmpe_joystick_eff with the
   widget's own r_max; up = positive bend): bend = 8192 + round(v * 8191).
   Grabbing cancels a return ramp in progress. Emits on change; returns the
   count (0 or 1; `out` needs room for 1). */
uint32_t hostmpe_strip_pitch_move(hostmpe_strip_t* s, float v,
                                  hostmpe_msg_t* out, uint32_t max);

/* Release starts the ~50 ms linear return-to-center ramp (§8: never a jump —
   a snap is a zipper on the outbound pipe). Ramp values surface from
   hostmpe_strip_tick — call it regularly (each frame is plenty). The ramp's
   FINAL message is exactly center (8192), guaranteed even under one sparse
   late tick. */
void     hostmpe_strip_pitch_release(hostmpe_strip_t* s, double now);
uint32_t hostmpe_strip_tick(hostmpe_strip_t* s, double now,
                            hostmpe_msg_t* out, uint32_t max);

/* Latch wheel: RELATIVE accumulation — regrasping at any position can never
   jump the value because no absolute-set entry point exists, and slow drags
   give arbitrarily fine control. `delta` is in CC units (float; sub-unit
   deltas accumulate; the shell scales drag distance to taste). The value
   clamps to [0, 127]; emits the assigned CC on a rounded change. `out` needs
   room for 1. */
uint32_t hostmpe_strip_latch_move(hostmpe_strip_t* s, int wheel, float delta,
                                  hostmpe_msg_t* out, uint32_t max);

/* Reassign an assignable wheel (ASSIGN_A/B only; MOD is fixed at CC 1). The
   wheel keeps its VALUE and emits nothing — the next change or announce
   speaks on the new CC. Refuses the protocol CCs (1, 6, 38, 64, 98..101,
   120..127): a strip-assigned CC 6 on the master would corrupt the DAW's RPN
   handshake state (DECISIONS_3 #30). Returns false when refused. */
bool     hostmpe_strip_assign(hostmpe_strip_t* s, int wheel, uint8_t cc);
uint8_t  hostmpe_strip_assigned_cc(const hostmpe_strip_t* s, int wheel);

/* Sustain button (CC 64). Momentary (default): press = 127, release = 0.
   Toggle: each press flips; release emits nothing. Changing the mode while
   sustain is ON emits the OFF (a mode switch must never strand a pedal).
   Each returns the count (0 or 1). */
uint32_t hostmpe_strip_sustain_press(hostmpe_strip_t* s, hostmpe_msg_t* out, uint32_t max);
uint32_t hostmpe_strip_sustain_release(hostmpe_strip_t* s, hostmpe_msg_t* out, uint32_t max);
uint32_t hostmpe_strip_sustain_mode(hostmpe_strip_t* s, bool toggle,
                                    hostmpe_msg_t* out, uint32_t max);

/* Re-announce (§8): the current latched values — spring bend, CC 1, both
   assignables, CC 64 — so a DAW that just received an MCM re-sync agrees
   with the strip. The shell calls this after every session-config send and
   passes the result EXEMPT (an announce repeats values by definition;
   change-only filtering would eat it). `out` needed room for 5; since step
   62 the fingering follows (the three valves, the slide): room for 9. */
uint32_t hostmpe_strip_announce(const hostmpe_strip_t* s, hostmpe_msg_t* out, uint32_t max);

/* UI getters (widget drawing): current spring value in [-1, 1] (tracks the
   return ramp as of the last tick), latched wheel value, sustain state. */
float    hostmpe_strip_pitch_value(const hostmpe_strip_t* s);
float    hostmpe_strip_latch_value(const hostmpe_strip_t* s, int wheel);
bool     hostmpe_strip_sustain_on(const hostmpe_strip_t* s);


/* ---- Phase 9 step 62 (INSTRUMENT §2–§4, QOL §2; DECISIONS_8 #9–#12): the fingering on the wire, the
   brass retune, the theremin surface, the small UX items ------------------------------------------ */

/* THE VALVE BUTTONS: three momentary widgets on the strip emitting the fingering CCs of libsumi 1.4.0 —
   CC 110 / 111 / 112 on the MASTER channel, 127 down, 0 up, change-only. Buttons: the shell passes them to
   the limiters EXEMPT (a decimated valve-up is a stuck valve). The bitmask is the shell's mirror of what it
   sent — the `buttons` of the sumi_layout_state_t it hands the probe (one source of truth, the bytes). */
uint32_t hostmpe_strip_valve_press  (hostmpe_strip_t* s, int valve /* 0..2 */, hostmpe_msg_t* out, uint32_t max);
uint32_t hostmpe_strip_valve_release(hostmpe_strip_t* s, int valve,            hostmpe_msg_t* out, uint32_t max);
uint32_t hostmpe_strip_valves(const hostmpe_strip_t* s);   /* bit 0 = valve 1 … */

/* THE POSITIONAL SLIDER (the trombone's slide): a new widget variant — POSITIONAL, not accumulating. A latch
   wheel accumulates deltas so a regrasp cannot jump it; the slide is the opposite case: the hand IS the
   slide, its position is the value, and a regrasp elsewhere is a new position. `position` 0..1 (the 1st
   to the 7th position; the seven detents are the shell's ticks at k/6, UI only — the value is continuous
   between them) → CC 113 = round(position · 127), master channel, change-only. A continuous dimension:
   the limiters police it as any master-channel CC. The shell mirrors the sent value into `slider`. */
#define HOSTMPE_SLIDE_POSITIONS 7
uint32_t hostmpe_strip_slide_set(hostmpe_strip_t* s, float position, hostmpe_msg_t* out, uint32_t max);
float    hostmpe_strip_slide_value(const hostmpe_strip_t* s);   /* 0..1 as sent (quantised to the CC) */
/* hostmpe_strip_announce (above) now restates the fingering too — the three valves then the slide, after
   the five it had: `out` needs room for 9. */

/* THE STRIP'S RESET — the panic's strip half (QOL §2, the panic as an action): sustain off, every valve
   up, the spring home at once (no ramp: a panic is the one jump that is right), emitting what changes;
   the latches and the slide keep their values (they are positions, not held states). The shell's panic
   is hostmpe_panic (every voice released, the zone silenced) then this. `out` needs room for 5. */
uint32_t hostmpe_strip_reset(hostmpe_strip_t* s, hostmpe_msg_t* out, uint32_t max);

/* LAYOUT QUICK-SWITCH (QOL §2): one strip action cycles a USER-CHOSEN subset of layouts, not all thirteen.
   The subset is sumi_layout_t values in the order they cycle; `next` answers the layout after `current`
   (wrapping), the subset's first when `current` is not in it, `current` itself when the subset is empty.
   Pure state: the shell applies the answer to its params and the session. */
#define HOSTMPE_QUICK_MAX 13
void     hostmpe_strip_quick_set  (hostmpe_strip_t* s, const uint32_t* layouts, uint32_t count);
uint32_t hostmpe_strip_quick_count(const hostmpe_strip_t* s);
uint32_t hostmpe_strip_quick_next (const hostmpe_strip_t* s, uint32_t current);

/* THE BRASS RETUNE (INSTRUMENT §2–§3). A held voice's sounding pitch moves with the fingering: a valve
   change shifts every held trumpet voice by the offsets' difference, the slide shifts every held trombone
   voice continuously. hostmpe keeps a PITCH OFFSET per voice (semitones, relative to the note on the
   wire) that adds to the joystick's bend; the shell moves it by the change the probe reports (the same
   cell re-probed under the new state: the new note minus the old — geometry has one source of truth).
   `ramp_s` > 0 ramps the move linearly over that long (the valves: HOSTMPE_RETUNE_S — the piano grid's
   20–40 ms legato machinery's number; a snap is a zipper on the outbound pipe), the ramp's messages
   surfacing from hostmpe_tick (call it each frame; the final message is exactly the target, whatever the
   tick cadence); `ramp_s` = 0 moves at once and emits the bend now (the slide: the hand is the ramp).
   `out` needs room for 1. A re-ramp while ramping continues from the current value. */
#define HOSTMPE_RETUNE_S 0.030f
uint32_t hostmpe_voice_retune(hostmpe_t* h, int32_t voice, double now, float delta_semis, float ramp_s,
                              hostmpe_msg_t* out, uint32_t max);
uint32_t hostmpe_tick(hostmpe_t* h, double now, hostmpe_msg_t* out, uint32_t max);   /* room for 15 */
float    hostmpe_voice_pitch_offset(const hostmpe_t* h, int32_t voice);               /* current, ramping or settled */

/* Touch-down WITH AN INITIAL OFFSET: the trombone between positions (the probe's note is the nearest
   semitone, the slide's exact pitch is the fraction) and the theremin (the hand lands between semitones).
   The first bend is the offset's, not centre, so the attack is in tune at the hand's pitch; the offset
   becomes the voice's pitch offset. Otherwise hostmpe_touch_begin. */
int32_t  hostmpe_touch_begin_offset(hostmpe_t* h, double now, uint8_t note, uint8_t velocity,
                                    float r_max, float grad_x, float grad_y, float offset_semis,
                                    hostmpe_msg_t* out, uint32_t max, uint32_t* out_count);

/* THE THEREMIN SURFACE (INSTRUMENT §4): pitch from X through the legato re-anchor machinery, Y the bipolar
   press axis. begin: the probe's note under the hand and its fraction (the cell's x minus the hand's,
   over the step), the velocity, R_max (the probe's half height). move: the probe's note and fraction
   under the hand NOW, and the raw vertical delta from touch-down in canvas heights — the pitch is set
   absolutely (the bend follows the hand, change-only) until the hand is more than HOSTMPE_THEREMIN_REANCHOR
   semitones from the anchor note, then the same-channel legato re-anchor the pen uses: bend(fraction) →
   Note On(the hand's note, the touch velocity) → Note Off(the old) — pitch continuous across it, the DAW
   records a terminated note; Y as the finger's (up → pressure, down → the swirl). No retrigger inside the
   range: a theremin never retriggers. `out` needs room for 5. */
#define HOSTMPE_THEREMIN_REANCHOR 47.0f
int32_t  hostmpe_theremin_begin(hostmpe_t* h, double now, uint8_t note, float offset_semis, uint8_t velocity,
                                float r_max, hostmpe_msg_t* out, uint32_t max, uint32_t* out_count);
uint32_t hostmpe_theremin_move (hostmpe_t* h, int32_t voice, uint8_t note, float offset_semis, float dy,
                                hostmpe_msg_t* out, uint32_t max);

/* LEFT-HANDED MIRRORING (QOL §2): the shell flips the surface and the strip — the lattice drawn mirrored,
   the touch's x mirrored before the probe, the strip at the other corner — and hostmpe flips the hand's
   horizontal delta so a rightward drag bends the way the mirrored lattice reads. */
void     hostmpe_set_mirror(hostmpe_t* h, bool on);
bool     hostmpe_mirror(const hostmpe_t* h);

/* PER-DEVICE PROFILES (QOL §2, DECISIONS_5 #7: OFFERED, never auto-applied). A known controller's MIDI
   device name → its family and the input mode it plays best in (sumi_input_mode_t's value: 0 auto, 1 MPE,
   2 classic, 3 wind); the shell OFFERS the preset — "use the Osmose settings?" — and applies on a yes. The
   match is a case-insensitive substring; an unknown name is HOSTMPE_DEVICE_NONE with an empty name. */
typedef enum {
    HOSTMPE_DEVICE_NONE = 0,
    HOSTMPE_DEVICE_ROLI,          /* Seaboard, LUMI, Blocks: MPE                       */
    HOSTMPE_DEVICE_AIRWAVE,       /* the gesture controller: its CC map, any mode      */
    HOSTMPE_DEVICE_OSMOSE,        /* Expressive E: MPE                                 */
    HOSTMPE_DEVICE_LINNSTRUMENT,  /* MPE                                               */
    HOSTMPE_DEVICE_CONTINUUM,     /* Haken: MPE                                        */
    HOSTMPE_DEVICE_BRISA,         /* the breath controller: wind                       */
    HOSTMPE_DEVICE_TRAVEL_SAX,    /* Odisei: wind                                      */
    HOSTMPE_DEVICE_EWI            /* AKAI: wind                                        */
} hostmpe_device_t;
typedef struct {
    hostmpe_device_t device;
    const char*      name;        /* the family's display name ("" for NONE), static storage */
    uint32_t         input_mode;  /* the recommended sumi_input_mode_t value */
} hostmpe_device_profile_t;
hostmpe_device_profile_t hostmpe_device_profile(const char* midi_device_name);

#ifdef __cplusplus
}
#endif
#endif /* HOSTMPE_H */
