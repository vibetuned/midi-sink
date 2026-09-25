// voice_mapper.cpp — musical -> spatial mapping (PROJECT_SPEC.md §2.1, §2.4,
// §3.3, §3.4, §4.4).
// Classic mode: every note is its own voice with strike only; bend/mod act
// globally. MPE mode: one voice per member channel (newest note steals),
// per-voice press/glide/slide with exponential smoothing and per-frame
// coalescing, sustained pressure as incremental drop expansions (§4.4), and
// a global per-frame deformation budget with overflow merging (§3.4).
#include "voice_mapper.h"
#include "ink_phase.h"
#include "log_levels.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Classic-mode tuning (canvas-height units / radians).
static const float DROP_RADIUS_MIN   = 0.020f;   // strike -> radius: r = min + span*sqrt(strike)
static const float DROP_RADIUS_SPAN  = 0.075f;   // (§3.4: ink *area* tracks velocity)
static const float SHEAR_ALPHA       = 0.45f;    // broad tine = shear-like (§2.4)
static const float SHEAR_PER_SEMI    = 0.015f;   // tine magnitude per semitone of bend

// Global-control tuning (§2.2). The vortex is dt-scaled: a per-frame pass of
// strength*VORTEX_RATE*dt radians, damped by viscosity.
static const float VORTEX_RATE       = 6.0f;     // rad/s at ctl = 1, viscosity 0
static const float VORTEX_RADIUS     = 0.35f;
static const float VORTEX_MIN_EMIT   = 0.0006f;  // radians; below: skip the pass
static const float VISCOSITY_DAMP    = 0.85f;    // damping share at viscosity = 1
// v0.9 (DECISIONS_4 #69): the right hand's Lamb-Oseen stir — dt-scaled like
// the vortex, at the #49 gesture's full-pull rate; the far 1/r² field does
// the carrying, so the core stays drop-sized rather than VORTEX_RADIUS-sized.
static const float SWIRL_CTL_RATE    = 3.0f;     // rad/s core rotation at ctl = 1
// v0.10 (Phase 6 step 36): the note-on torsion sweep episode (MEDIUM §2.1).
// Total rotation amplitude integrates to RATE·τ ≈ 0.72 rad at the wave's
// crests; φ advances 1.5 cycles/s (the rings travel outward at ω/k); the
// episode is over after LIFE time constants; the reach is REACH× the strike
// radius (floored) — the discharge rings around the drop, not the whole sheet.
static const float TORSION_SWEEP_TAU      = 0.6f;
static const float TORSION_SWEEP_RATE     = 1.2f;        // rad/s at t = 0
static const float TORSION_SWEEP_OMEGA    = 9.4247780f;  // 2π · 1.5 rad/s
static const float TORSION_SWEEP_LIFE     = 4.0f;        // time constants until the episode ends
// 1.1.0 (Phase 6 step 42, MEDIUM §4): the Anod binding table's constants —
// the press FEED spends torsion at this rate (rad/s at full pressure) around
// the note's drop; the strike's burst displaces its lobes by this fraction of
// the charge (#88: the classic spark's D = 0.3 r, back after #71 took it out).
static const float TORSION_FEED_RATE   = 1.2f;
static const float ANOD_STRIKE_BURST_D = 0.3f;
// v0.12 (Phase 6 step 38): the burst episodes — an increment below the floor
// merges into the next frame's (the pending pattern, implicit in the age
// bookkeeping); one episode takes at most this many passes in a frame. The
// floor is the FIELD'S QUANTUM, not a texel: the coordinates are stored as
// half floats, whose spacing in the outer half of the canvas is 2^-11 canvas
// heights (4.9e-4), and a pass that moves a texel's source by less than half
// of that rounds back to where it was — the tail of a release, emitted
// frame by frame, would vanish pass by pass (measured: the lobe stalled at
// 73% of D). Merged until the peak reaches one quantum, it lands.
static const float BURST_MIN_EMIT     = 0.0005f;         // canvas heights: one half-float quantum of the stored coordinates
static const int   BURST_MAX_PER_FRAME = 24;
// v0.13 (Phase 6 step 39): the spark shear episodes — the window across each
// shear is twice the strike radius (the band the streamers run in), the
// episode ends after LIFE time constants, and a pending kick below the same
// quantum waits for the next frame.
static const float SPARK_BAND     = 2.0f;                // × the strike radius
static const float SPARK_LIFE     = 4.0f;                // time constants
static const float SPARK_MIN_EMIT = 0.0005f;             // canvas heights
// v0.14 (Phase 6 step 40): the Chirikov route — a throw smaller than this
// (of the full control range) accumulates until it is worth a step.
static const float CHIRIKOV_MIN_DELTA = 0.02f;
static const float TORSION_SWEEP_REACH    = 3.0f;
static const float TORSION_SWEEP_MIN_R    = 0.05f;
static const float TORSION_SWEEP_MIN_EMIT = 0.002f;      // rad; below it, increments merge into the next frame
static const float SWIRL_CTL_CORE_R  = 0.15f;    // r_c (canvas-height units)

// Wind-mode tuning (§2.3). The brush maintains a breath-proportional line
// WIDTH (relaxing toward it) rather than integrating flow without bound like
// MPE press — that is what draws a line instead of a blob (DECISIONS.md).
// #63: wind legato — the old drop is dragged to the new note as a rigid tip
// through the §4.3.4 wake (profile + spread from params), sub-stepped <= a/4.
static const float WIND_WAKE_TIP_MIN  = 0.006f;  // the tablets' lightest pen tip
static const uint32_t WIND_WAKE_MAX_STEPS = 256; // one legato's queue share

// §3.1 overflow safeguard: armed on the first ring overflow, never before.
static const double VOICE_TIMEOUT_S  = 10.0;     // silence per voice while traffic flows

// MPE per-voice tuning (§3.4, §4.4).
static const float FEED_RATE         = 0.055f;   // boundary growth/s at press=1, expansion_rate=1
static const float FEED_ONSET        = 0.02f;    // press rising across this = new feed episode
static const float FEED_RELEASE      = 0.008f;   // press below this ends the episode
static const float FEED_SEED_RADIUS  = 0.004f;   // a new episode's band grows from here
static const float FEED_MIN_GROW     = 0.0004f;  // accumulate below this (merge tiny steps)
static const float FEED_MAX_EMIT     = 0.050f;   // clamp one expansion pass (post-starvation)
static const float PRESS_DEADZONE    = 0.01f;
static const float GLIDE_TINE_ALPHA  = 0.030f;   // narrow: local wake, never a shear
static const float GLIDE_MIN_MOVE    = 0.0015f;
static const float SEMITONE_STEP_MAX = 0.030f;   // cap pitch-axis distance per semitone

// v0.4 Lamb-Oseen swirl (§4.3(7)): the drop IS the vortex core. Γ per pass is
// a delta rate like every continuous feed: the CORE'S angular velocity is
// SWIRL_OMEGA·amount·expansion_rate rad/s (Γ·Δt = θ0 · 2π·r_c², so the far
// field scales with the drop's own size). Tiny steps accumulate and merge
// like press growth; the emission threshold is in core-angle units.
static const float SWIRL_OMEGA      = 2.0f;      // rad/s at amount 1
static const float SWIRL_DEADZONE   = 0.01f;
static const float SWIRL_MIN_EMIT   = 0.0008f;   // radians of core rotation

// v0.4 pinch (§4.3(5), slide_mode = 1): per-voice CC74 DELTAS drive the pass.
static const float PINCH_K_SCALE     = 1.2f;     // full 0..1 slide sweep -> Σk ≈ 1.2
static const float PINCH_MIN_K       = 0.002f;   // accumulate below this
static const float PINCH_WINDOW_S    = 0.02f;    // S: streamline window (ac units²)

// v0.4 sine ripple (§4.3(6)) — ctl (0..1) -> physical mapping, shared with
// the engine's live-composite path via voice_mapper.h.

#define SUMI_DEFAULT_DEFORM_BUDGET 64u   // §3.4 deform passes per frame
#define SUMI_MAX_VOICES 16u              // one per MIDI channel (§2.1)

// MPE voice dynamics — indexed by member channel (voice_id == channel).
// §3.4 echo sets: a voice owns its full echo set for its lifetime; press,
// glide, slide and lift fan out to every echo.
struct sumi_mpe_voice_t {
    bool  active;
    uint32_t echo_count;                     // 1..SUMI_MAX_ECHOES
    float base_x[SUMI_MAX_ECHOES], base_y[SUMI_MAX_ECHOES];   // at note-on
    float cur_x[SUMI_MAX_ECHOES], cur_y[SUMI_MAX_ECHOES];     // dragged by glide
    float ax, ay;            // pitch axis: unit dir × one-semitone distance
    float phase_base;        // §4.2 band of this voice's ink
    float aux_base;          // raw drop counter at note-on
    float press_t, press_s;  // target / smoothed (§3.4 smoothing)
    float slide_t, slide_s;
    float glide_t, glide_s;  // semitones
    float swirl_t, swirl_s;  // v0.4 §4.3(7): the second pressure dimension
    float pending_swirl;     // merged core-rotation steps awaiting budget (rad)
    float slide_baked;       // slide value already realized as pinch (mode 1)
    bool  slide_primed;      // first CC74 snaps, never pinches (avoid the 0->rest jump)
    float nominal_radius;    // the drop's current boundary radius (grows with press)
    float pending_grow;      // merged §4.4 boundary-growth steps awaiting budget
    bool  feeding;           // inside a press episode (between onset/release)
    bool  fed_once;          // first episode continues the strike band; later
                             // episodes stamp a NEW band -> nested rings (§4.4)
    // v0.10 (Phase 6 step 36): the note-on torsion sweep — a time-driven
    // EPISODE emitting per-frame deltas, the pattern the burst's age envelope
    // and the spark's decay reuse. Outlives the note (a discharge dies on its
    // own clock); a new note in the slot restarts it.
    bool  sweep_on;
    float sweep_t;           // seconds since the strike
    float sweep_pending;     // merged rotation increments awaiting budget (rad)
    float sweep_radius;      // the sweep's e-fold reach, fixed at the strike
    float feed_t;            // 1.1.0: the press-fed torsion's phase clock (Anod's press feed)
};

// v0.12 (Phase 6 step 38): a burst EPISODE — sumi_add_burst's strike with its
// lifetime: the age l grows from a to l_end over `life` seconds (l² linear in
// t) and each frame's increment l_cur -> l(t) goes out as budgeted passes.
typedef struct {
    bool     on;
    float    x, y, a, amp, theta0;
    uint32_t m;
    float    l_cur, l_end;   // the age emitted so far; the final age
    float    life, t;        // seconds; life 0 = at once
} sumi_burst_slot_t;

// v0.13 (Phase 6 step 39): a spark shear EPISODE — the strike's jagged
// streamers: the kick decays as e^{−t/τ}, each frame's increment joining the
// pending kick until it is worth a kick-drift step.
typedef struct {
    bool     on;
    float    x, y, theta0, band;
    float    k, phase;
    float    a_tot, b_tot, tau, t;
    float    pend_a, pend_b;
    uint32_t stack, profile;
} sumi_spark_slot_t;

// Stage-1 note bookkeeping — which note owns each channel (steal detection).
struct sumi_note_slot_t {
    bool    active;
    uint8_t note;
};

struct sumi_voice_mapper_t {
    sumi_log_fn log_cb;
    void*       log_user;

    // Classic-mode global state.
    float bend_semis;        // last applied global bend

    // Stage-1 state.
    sumi_note_slot_t notes[SUMI_MAX_VOICES];
    // Per-update coalescing slots (§3.4: one value per dimension per voice).
    bool  has_press[SUMI_MAX_VOICES];
    float press_val[SUMI_MAX_VOICES];
    bool  has_slide[SUMI_MAX_VOICES];
    float slide_val[SUMI_MAX_VOICES];
    bool  has_glide[SUMI_MAX_VOICES];
    float glide_val[SUMI_MAX_VOICES];
    bool  has_swirl[SUMI_MAX_VOICES];
    float swirl_val[SUMI_MAX_VOICES];
    bool  have_master_bend;
    float master_bend;
    bool  have_ctl[SUMI_CTL_COUNT];      // per-update GlobalCtl coalescing
    float ctl_val[SUMI_CTL_COUNT];
    float pinch_ctl_baked[2];            // v0.9 #69: delta trackers, saddle/cross
    // v0.11 (Phase 6 step 37): the Chladni cellular flow — the current
    // layout's lattice (aspect-corrected x) and the last aspect normalize()
    // saw (the lattice lives in the layout's normalized space, the pass in
    // aspect-corrected space).

    float last_aspect;

    // CC routing (§2.2): -1 = unmapped; per-channel overrides any-channel.
    int8_t cc_map_any[128];
    int8_t cc_map_ch[16][128];

    int   last_mode;         // previous input mode (-1 = none yet)

    // §3.1 overflow safeguard state.
    bool     timeout_armed;
    uint32_t last_dropped;
    double   last_activity[SUMI_MAX_VOICES];   // last message touching each voice
    double   last_traffic;                     // last message of any kind

    // Stage-2 state.
    sumi_mpe_voice_t voices[SUMI_MAX_VOICES];
    float ctl_t[SUMI_CTL_COUNT];         // global field controls, target
    float ctl_s[SUMI_CTL_COUNT];         // smoothed (§3.4)
    float cells_pending;                 // step 43: the stir's rotation not yet emitted, rad
    float chladni_dir;                   // step 43: the stir's sense from the bend (+1 up, −1 down)
    bool  stir_by_gesture;               // #75: a long press's pull owns the stir's target until it lets go
    float gesture_torsion_t;             // #75: the press-fed torsion's phase clock (the rings travel while held)
    float gesture_torsion_pending;       // #75: its sub-floor increments, merged into the next frame's
    int   last_bend_eff;                 // #72: the effective bend mode last frame (a flip away from the stir stills it)
    bool  poly_on;                       // #72: the poly-pressure route holds the wavenumbers while pressed...
    float poly_hold_tk, poly_hold_sk;    //      ...and gives them back where they were at the release
    float cells_rmin;                    // step 43: the smallest disc, canvas heights (0 = unknown)
    float ripple_baked;      // total baked ripple amplitude (v0.4 §4.3(6))
    float ripple_phase;      // #36: drifts under bend-driven bake (permanence)
    int   last_bend_mode;    // #35: detect mode flips (1 -> 0 stills the amp)
    uint32_t budget;         // deform emissions per lower() call
    uint32_t frame_emitted;
    uint32_t merged_total;   // emissions deferred by budget exhaustion
    uint32_t merged_last_log;
    uint32_t frames;
    sumi_burst_slot_t bursts[SUMI_MAX_BURSTS];   // v0.12: the running burst episodes
    sumi_spark_slot_t sparks[SUMI_MAX_SPARKS];   // v0.13: the running spark shear episodes
    uint32_t          spark_seed;                // v0.13: the per-strike phase draw
    float             chirikov_baked;            // v0.14: the throw already applied (delta tracker)
    uint32_t          last_medium;               // 1.1.0: a medium switch re-baselines the Chirikov's source
};

// 1.1.0 (Phase 6 step 42, MEDIUM §4): the medium's DEFAULT BINDING TABLE. A
// mode left at SUMI_MODE_MEDIUM_DEFAULT resolves here; an explicit mode is
// the user's override, exactly as today. Sumi: glide / aux / ink feed. Anod:
// the bend plays the torsion's wavenumber, the slide the spark's, pressure
// feeds the torsion sweep.
static void eff_modes(const sumi_params_t* p, uint32_t* bend, uint32_t* slide, uint32_t* press) {
    const bool anod = p && p->medium == SUMI_MEDIUM_ANOD;
    uint32_t b = p ? p->bend_mode : 0u, s = p ? p->slide_mode : 0u, r = p ? p->press_mode : 0u;
    if (b == SUMI_MODE_MEDIUM_DEFAULT || b > 4u) b = anod ? 4u : 0u;   // step 43 (the author's table): Anod's bend plays the Chladni stir
    if (s == SUMI_MODE_MEDIUM_DEFAULT || s > 2u) s = anod ? 2u : 0u;
    if (r == SUMI_MODE_MEDIUM_DEFAULT || r > 2u) r = anod ? 2u : 0u;
    if (bend) *bend = b;
    if (slide) *slide = s;
    if (press) *press = r;
}
static bool ctl_is_mapped(const sumi_voice_mapper_t* vm, sumi_ctl_t dim) {
    for (int cc = 0; cc < 128; cc++) {
        if (vm->cc_map_any[cc] == (int8_t)dim) return true;
        for (int ch = 0; ch < 16; ch++) if (vm->cc_map_ch[ch][cc] == (int8_t)dim) return true;
    }
    return false;
}
static int8_t cc_lookup(const sumi_voice_mapper_t* vm, uint8_t ch, uint8_t cc) {
    const int8_t specific = vm->cc_map_ch[ch & 0x0F][cc & 0x7F];
    if (specific >= 0) return specific;
    return vm->cc_map_any[cc & 0x7F];
}

// Default bindings (documented in README.md): mod wheel and the Airwave
// dimensions. Every entry is remappable/erasable via sumi_map_cc /
// sumi_clear_cc_map (§2.2: Airwave CC numbers are user-configured device-side).
static void install_default_cc_map(sumi_voice_mapper_t* vm) {
    vm->cc_map_any[1]  = SUMI_CTL_VORTEX_STRENGTH;   // mod wheel (§2.4)
    vm->cc_map_any[2]  = SUMI_CTL_INK_FLOW;          // breath (§2.3)
    vm->cc_map_any[7]  = SUMI_CTL_INK_FLOW;          // volume = breath alias (wind)
    vm->cc_map_any[11] = SUMI_CTL_INK_FLOW;          // expression = breath alias
    // ROLI Airwave, as the device actually ships (measured, DECISIONS_4 #50:
    // Grasp 20/21, Slide 22/23, Glide 24/25, Raise 26/27, Tilt 28/29, Flex
    // 30/31, left/right pairs), laid out per the author's playing session
    // (DECISIONS_4 #69): each hand is a stirring hand — Raise the strength,
    // Glide the centre X, Slide the centre Y (reversed: hand up = centre up)
    // — the left an exponential/Rankine vortex, the right the Lamb-Oseen
    // swirl. Grasp is the pinch (saddle left, crossed tines right), Tilt the
    // ripple (wavelength left, amount right). Flex stays free: it cannot be
    // played without disturbing the other dimensions.
    vm->cc_map_any[26] = SUMI_CTL_VORTEX_STRENGTH;   // Raise L
    vm->cc_map_any[24] = SUMI_CTL_VORTEX_X;          // Glide L
    vm->cc_map_any[22] = SUMI_CTL_VORTEX_Y;          // Slide L (reversed at emit)
    vm->cc_map_any[27] = SUMI_CTL_SWIRL_STRENGTH;    // Raise R
    vm->cc_map_any[25] = SUMI_CTL_SWIRL_X;           // Glide R
    vm->cc_map_any[23] = SUMI_CTL_SWIRL_Y;           // Slide R (reversed at emit)
    vm->cc_map_any[20] = SUMI_CTL_PINCH_SADDLE;      // Grasp L
    vm->cc_map_any[21] = SUMI_CTL_PINCH_CROSS;       // Grasp R
    vm->cc_map_any[28] = SUMI_CTL_RIPPLE_FREQ;       // Tilt  L: wavelength
    vm->cc_map_any[29] = SUMI_CTL_RIPPLE_AMP;        // Tilt  R: amount
    // 30/31 Flex: free. Viscosity/roughness/palette have no Airwave route —
    // they stay settings-window sliders (remappable via the CC-map editor).
}

extern "C" {

// Local pitch axis at `note`, derived from the ACTIVE layout (§3.4): the
// direction of increasing pitch, one-semitone distance, capped so a ±48-
// semitone glide stays on canvas. Of the two neighbors (note±1) the SHORTER
// step wins — this keeps grid layouts on their row at octave wraps (B -> C
// jumps a row; B -> A# stays in it) and tames the circle-of-fifths chords.
static void pitch_axis(uint8_t note, uint32_t layout, const sumi_params_t* params,
                       float aspect, float* ax, float* ay) {
    // One derivation, two consumers (Phase 4): the shared shortest-neighbor
    // delta (layouts.cpp) is uncapped lattice truth. On the PLAYABLE lattices
    // (grid, Jankó) the glide uses the TRUE step, so a one-semitone bend
    // visibly lands the drop on the neighboring cell and a release ring
    // appears exactly where the performer let go (DECISIONS_3 #20 — the
    // Step 16 DONE demands "one-column drags read as clean semitone glides").
    // The rendering cap remains for fifths/rolls, whose neighbor steps can
    // span half the canvas.
    float dx = 0.0f, dy = 0.0f;
    if (!sumi_layout_semitone_delta(layout, note, params, aspect, &dx, &dy)) {
        *ax = SEMITONE_STEP_MAX; *ay = 0.0f;
        return;
    }
    const bool lattice = layout == SUMI_LAYOUT_CHROMA_GRID ||
                         layout == SUMI_LAYOUT_JANKO ||
                         layout == SUMI_LAYOUT_PIANO_GRID;
    const float len = sqrtf(dx * dx + dy * dy);
    const float cap = lattice ? len : SEMITONE_STEP_MAX;
    const float step = len > cap ? cap : len;
    *ax = dx / len * step;
    *ay = dy / len * step;
}

sumi_voice_mapper_t* sumi_voice_mapper_create(sumi_log_fn log_cb, void* log_user) {
    sumi_voice_mapper_t* vm = (sumi_voice_mapper_t*)calloc(1, sizeof(sumi_voice_mapper_t));
    if (!vm) return nullptr;
    vm->log_cb = log_cb;
    vm->log_user = log_user;
    vm->budget = SUMI_DEFAULT_DEFORM_BUDGET;
    vm->last_mode = -1;
    memset(vm->cc_map_any, -1, sizeof(vm->cc_map_any));
    memset(vm->cc_map_ch, -1, sizeof(vm->cc_map_ch));
    install_default_cc_map(vm);
    // Global control rest values: vortex and swirl centered, calm.
    vm->ctl_t[SUMI_CTL_VORTEX_X] = vm->ctl_s[SUMI_CTL_VORTEX_X] = 0.5f;
    vm->ctl_t[SUMI_CTL_VORTEX_Y] = vm->ctl_s[SUMI_CTL_VORTEX_Y] = 0.5f;
    vm->ctl_t[SUMI_CTL_SWIRL_X]  = vm->ctl_s[SUMI_CTL_SWIRL_X]  = 0.5f;
    vm->ctl_t[SUMI_CTL_SWIRL_Y]  = vm->ctl_s[SUMI_CTL_SWIRL_Y]  = 0.5f;
    // v0.4 (#35): the ripple wavelength rests mid-range — amplitude is the
    // gate (0 by default), so nothing shows until a bend or CC raises it.
    vm->ctl_t[SUMI_CTL_RIPPLE_FREQ] = vm->ctl_s[SUMI_CTL_RIPPLE_FREQ] = 0.5f;
    // v0.10: the torsion's wavenumber rests mid-range too; its phase at 0.
    vm->ctl_t[SUMI_CTL_TORSION_K] = vm->ctl_s[SUMI_CTL_TORSION_K] = 0.5f;
    // v0.13: the spark shear's wavenumber, mid-range as the others.
    vm->ctl_t[SUMI_CTL_SPARK_K] = vm->ctl_s[SUMI_CTL_SPARK_K] = 0.5f;
    vm->chladni_dir = 1.0f;
    vm->last_bend_eff = -1; vm->poly_on = false; vm->poly_hold_tk = vm->poly_hold_sk = 0.5f;
    vm->spark_seed = 0x9e3779b9u;
    return vm;
}

// step 43: the two waves of ψ = ½Ψ[cos(w1·P') + cos(w2·P')] whose eddies (the
// maxima AND the minima) form the lattice generated by (a1, a2): with b1 = a1 +
// a2 and b2 = a1 − a2, the maxima sit on the lattice of (b1, b2) and the minima
// on its centred points, so a1 and a2 each join a maximum to a minimum —
// neighbours counter-rotate along both. (w1, w2) is the dual basis, w_i·b_j =
// 2πδ_ij. For a1 = (s, 0), a2 = (0, s) this is Taylor–Green's cos(kx)cos(ky).
void sumi_chladni_waves(float a1x, float a1y, float a2x, float a2y, float* w1, float* w2) {
    const float b1x = a1x + a2x, b1y = a1y + a2y;
    const float b2x = a1x - a2x, b2y = a1y - a2y;
    const float p2x = -b2y, p2y = b2x, p1x = -b1y, p1y = b1x;          // perpendiculars
    const float d1 = b1x * p2x + b1y * p2y, d2 = b2x * p1x + b2y * p1y;
    const float TWO_PI = 6.28318530718f;
    if (fabsf(d1) < 1e-12f || fabsf(d2) < 1e-12f) { w1[0] = w1[1] = w2[0] = w2[1] = 0.0f; return; }
    w1[0] = TWO_PI * p2x / d1; w1[1] = TWO_PI * p2y / d1;
    w2[0] = TWO_PI * p1x / d2; w2[1] = TWO_PI * p1y / d2;
}

void sumi_voice_mapper_set_cells_rmin(sumi_voice_mapper_t* vm, float r_min) {
    if (!vm) return;
    vm->cells_rmin = r_min > 0.0f ? r_min : 0.0f;
    vm->cells_pending = 0.0f;
}

uint32_t sumi_chladni_emit_step(sumi_deform_queue_t* q, float psi, float balance,
                                float a1x, float a1y, float a2x, float a2y, float p0x, float p0y) {
    if (!q) return 0u;
    float w1[2], w2[2];
    sumi_chladni_waves(a1x, a1y, a2x, a2y, w1, w2);
    const bool inverse = psi < 0.0f;
    sumi_deform_t d;
    d.type = SUMI_DEFORM_CHLADNI;
    d.as.chladni.psi = psi;                          // signed: the inverse negates both waves
    d.as.chladni.p0x = p0x; d.as.chladni.p0y = p0y;
    for (int i = 0; i < 2; i++) {
        const uint32_t stage = inverse ? (uint32_t)(1 - i) : (uint32_t)i;   // the exact inverse: REVERSED order
        d.as.chladni.stage = stage;
        d.as.chladni.wx = stage == 0u ? w1[0] : w2[0];
        d.as.chladni.wy = stage == 0u ? w1[1] : w2[1];
        d.as.chladni.weight = stage == 0u ? 1.0f : 1.0f - 2.0f * balance;
        sumi_deform_queue_push(q, &d);
    }
    return 2u;
}

void sumi_voice_mapper_torsion_kphi(const sumi_voice_mapper_t* vm, uint32_t profile,
                                    float* k, float* phase) {
    if (!vm || profile != SUMI_VORTEX_TORSION) { if (k) *k = 0.0f; if (phase) *phase = 0.0f; return; }
    float kc = vm->ctl_s[SUMI_CTL_TORSION_K];
    kc = kc < 0.0f ? 0.0f : (kc > 1.0f ? 1.0f : kc);
    if (k) *k = SUMI_TORSION_K_MIN + kc * (SUMI_TORSION_K_MAX - SUMI_TORSION_K_MIN);
    if (phase) *phase = vm->ctl_s[SUMI_CTL_TORSION_PHASE] * 6.2831853f;
}

void sumi_voice_mapper_map_cc(sumi_voice_mapper_t* vm, uint8_t channel,
                              uint8_t cc, sumi_ctl_t target) {
    if (!vm || cc > 127 || target >= SUMI_CTL_COUNT) return;
    if (channel == 0xFF) vm->cc_map_any[cc] = (int8_t)target;
    else if (channel < 16) vm->cc_map_ch[channel][cc] = (int8_t)target;
}

void sumi_voice_mapper_clear_cc_map(sumi_voice_mapper_t* vm) {
    if (!vm) return;
    memset(vm->cc_map_any, -1, sizeof(vm->cc_map_any));
    memset(vm->cc_map_ch, -1, sizeof(vm->cc_map_ch));
}

void sumi_voice_mapper_destroy(sumi_voice_mapper_t* vm) {
    free(vm);
}

float sumi_voice_mapper_ctl(const sumi_voice_mapper_t* vm, sumi_ctl_t dim) {
    if (!vm || dim >= SUMI_CTL_COUNT) return 0.0f;
    return vm->ctl_s[dim];
}

bool sumi_voice_mapper_add_burst(sumi_voice_mapper_t* vm, float x, float y, float a, float D,
                                 float theta0, uint32_t m, const sumi_params_t* params) {
    if (!vm || !(a > 0.0f) || !(D != 0.0f) || !(theta0 == theta0)) return false;
    if (m == 0) m = params ? params->burst_order : SUMI_BURST_M_MIN;   // the gesture defers to the params' order
    if (m < SUMI_BURST_M_MIN) m = SUMI_BURST_M_MIN;
    if (m > SUMI_BURST_M_MAX) m = SUMI_BURST_M_MAX;
    float age = params ? params->burst_age : 4.0f;
    if (age < 1.5f) age = 1.5f;
    if (age > 12.0f) age = 12.0f;
    float life = params ? params->burst_life : 0.8f;
    if (life < 0.0f) life = 0.0f;
    if (life > 4.0f) life = 4.0f;
    // A free slot, else the episode nearest its end.
    sumi_burst_slot_t* b = nullptr;
    float best = 2.0f;
    for (uint32_t i = 0; i < SUMI_MAX_BURSTS; i++) {
        sumi_burst_slot_t* s = &vm->bursts[i];
        if (!s->on) { b = s; break; }
        const float frac = (s->l_end - s->l_cur) / (s->l_end - s->a);   // the share still to come
        if (frac < best) { best = frac; b = s; }
    }
    b->on = true;
    b->x = x; b->y = y; b->a = a; b->theta0 = theta0; b->m = m;
    b->l_end = a * age; b->l_cur = a;
    b->life = life; b->t = 0.0f;
    b->amp = (float)sumi_burst_amp(m, (double)D, (double)a, (double)b->l_end);
    return true;
}

// #75 (the author's gesture table): the long press's pull in Anod — the
// Chladni stir, reversed, at `amount` (0..1); 0 lets go of a stir the press
// set (a bend or a CC that wrote the target since keeps it).
void sumi_voice_mapper_gesture_stir(sumi_voice_mapper_t* vm, float amount) {
    if (!vm) return;
    if (amount > 0.002f) {
        vm->ctl_t[SUMI_CTL_CHLADNI_A] = amount > 1.0f ? 1.0f : amount;
        vm->chladni_dir = -1.0f;
        vm->stir_by_gesture = true;
    } else if (vm->stir_by_gesture) {
        vm->ctl_t[SUMI_CTL_CHLADNI_A] = 0.0f;
        vm->stir_by_gesture = false;
    }
}

// #75: the long press's hold / push in Anod — the torsion sweep FEED around
// (x, y), `dtheta` radians of ring amplitude this frame, the phase travelling
// at the sweep's ω on the press's own clock, k and φ from the ctls — the
// press-fed torsion of a held key (press_mode 2), as a gesture.
void sumi_voice_mapper_gesture_torsion(sumi_voice_mapper_t* vm, sumi_deform_queue_t* queue,
                                       float x, float y, float radius, float dtheta, float dt) {
    if (!vm || !queue || !(radius > 0.0f)) return;
    vm->gesture_torsion_t += dt;
    vm->gesture_torsion_pending += dtheta;
    if (vm->gesture_torsion_pending < TORSION_SWEEP_MIN_EMIT) return;
    float k = 0.0f, phase_ctl = 0.0f;
    sumi_voice_mapper_torsion_kphi(vm, SUMI_VORTEX_TORSION, &k, &phase_ctl);
    sumi_deform_t d;
    d.type = SUMI_DEFORM_VORTEX;
    d.as.vortex.x = x; d.as.vortex.y = y;
    d.as.vortex.strength = vm->gesture_torsion_pending;
    d.as.vortex.radius = radius < TORSION_SWEEP_MIN_R ? TORSION_SWEEP_MIN_R : radius;
    d.as.vortex.profile = SUMI_VORTEX_TORSION;
    d.as.vortex.k = k;
    d.as.vortex.phase = fmodf(phase_ctl + TORSION_SWEEP_OMEGA * vm->gesture_torsion_t, 6.2831853f);
    sumi_deform_queue_push(queue, &d);
    vm->gesture_torsion_pending = 0.0f;
}

uint32_t sumi_voice_mapper_burst_count(const sumi_voice_mapper_t* vm) {
    uint32_t n = 0;
    if (vm) for (uint32_t i = 0; i < SUMI_MAX_BURSTS; i++) n += vm->bursts[i].on ? 1u : 0u;
    return n;
}

bool sumi_voice_mapper_add_spark(sumi_voice_mapper_t* vm, float x, float y, float r, float theta0,
                                 const sumi_params_t* params) {
    if (!vm || !(r > 0.0f) || !(theta0 == theta0)) return false;
    float shear = params ? params->spark_shear : 0.6f;
    if (shear < 0.0f) shear = 0.0f;
    if (shear > 2.0f) shear = 2.0f;
    if (!(shear * r > 1e-6f)) return false;              // no shear in this composition
    float tau = params ? params->spark_tau : 0.25f;
    if (tau < 0.05f) tau = 0.05f;
    if (tau > 2.0f) tau = 2.0f;
    uint32_t stack = params ? params->spark_stack : 3u;
    if (stack < 1u) stack = 1u;
    if (stack > 4u) stack = 4u;
    // A free slot, else the episode nearest its end.
    sumi_spark_slot_t* s = nullptr;
    float best = -1.0f;
    for (uint32_t i = 0; i < SUMI_MAX_SPARKS; i++) {
        sumi_spark_slot_t* c = &vm->sparks[i];
        if (!c->on) { s = c; break; }
        const float frac = c->t / (SPARK_LIFE * c->tau);
        if (frac > best) { best = frac; s = c; }
    }
    s->on = true;
    s->x = x; s->y = y; s->theta0 = theta0; s->band = SPARK_BAND * r;
    s->k = SUMI_SPARK_K_MIN + vm->ctl_s[SUMI_CTL_SPARK_K] * (SUMI_SPARK_K_MAX - SUMI_SPARK_K_MIN);
    vm->spark_seed = vm->spark_seed * 1664525u + 1013904223u;   // the strike's own phase
    s->phase = (float)(vm->spark_seed >> 8) / 16777216.0f * 6.2831853f;
    s->a_tot = s->b_tot = shear * r;
    s->tau = tau; s->t = 0.0f;
    s->pend_a = s->pend_b = 0.0f;
    s->stack = stack;
    s->profile = params && params->spark_profile ? 1u : 0u;
    return true;
}

uint32_t sumi_voice_mapper_spark_count(const sumi_voice_mapper_t* vm) {
    uint32_t n = 0;
    if (vm) for (uint32_t i = 0; i < SUMI_MAX_SPARKS; i++) n += vm->sparks[i].on ? 1u : 0u;
    return n;
}

float sumi_voice_mapper_voice_radius(const sumi_voice_mapper_t* vm, uint32_t voice) {
    if (!vm || voice >= SUMI_MAX_VOICES || !vm->voices[voice].active) return 0.0f;
    return vm->voices[voice].nominal_radius;
}

void sumi_voice_mapper_set_budget(sumi_voice_mapper_t* vm, uint32_t budget) {
    if (vm && budget > 0) vm->budget = budget;
}

uint32_t sumi_voice_mapper_merged_count(const sumi_voice_mapper_t* vm) {
    return vm ? vm->merged_total : 0;
}

static uint32_t put(sumi_voice_event_t* out, uint32_t count, uint32_t max,
                    const sumi_voice_event_t* ev) {
    if (count < max) out[count] = *ev;
    return count + (count < max ? 1u : 0u);
}

/* ------------------------------------------------------------------ */
/* Stage 1: musical events -> §3.3 vocabulary                          */
/* ------------------------------------------------------------------ */

uint32_t sumi_voice_mapper_normalize(sumi_voice_mapper_t* vm,
                                     double now, uint32_t dropped_count,
                                     const sumi_midi_event_t* in, uint32_t in_count,
                                     sumi_input_mode_t mode, sumi_mpe_zone_t zone,
                                     const sumi_params_t* params, float aspect,
                                     sumi_voice_event_t* out, uint32_t max) {
    uint32_t press_eff = 0; eff_modes(params, nullptr, nullptr, &press_eff);   // 1.1.0: the medium's table
    if (vm && aspect > 0.0f) vm->last_aspect = aspect;   // v0.11: the Chladni lattice converts the layout's x through it
    if (!vm || !out || (in_count > 0 && !in)) return 0;
    // §3.1 overflow safeguard: arm the per-voice inactivity timeouts on the
    // first overflow (drop-oldest may have discarded a Note Off).
    if (dropped_count > vm->last_dropped) {
        vm->last_dropped = dropped_count;
        if (!vm->timeout_armed) {
            vm->timeout_armed = true;
            // The suspicious window starts AT the overflow (that is when a
            // Note Off may have vanished): refresh every activity clock so
            // held voices get a full grace period from this moment.
            for (uint32_t v2 = 0; v2 < SUMI_MAX_VOICES; v2++) {
                vm->last_activity[v2] = now;
            }
            if (vm->log_cb) {
                vm->log_cb(SUMI_LOG_WARN,
                           "mapper: ring overflow - arming voice inactivity timeouts",
                           vm->log_user);
            }
        }
    }
    if (in_count > 0) vm->last_traffic = now;
    const uint32_t layout = params ? params->pitch_layout : 0u;
    const bool mpe = (mode == SUMI_INPUT_MPE);
    const bool wind = (mode == SUMI_INPUT_WIND);

    uint32_t count = 0;
    vm->have_master_bend = false;
    for (uint32_t v = 0; v < SUMI_MAX_VOICES; v++) {
        vm->has_press[v] = vm->has_slide[v] = vm->has_glide[v] = false;
        vm->has_swirl[v] = false;
    }
    for (uint32_t c = 0; c < SUMI_CTL_COUNT; c++) vm->have_ctl[c] = false;

    // Input mode changed (§2.5 window handed the bath to another dialect):
    // end every voice tracked under the old dialect so nothing keeps feeding.
    if ((int)mode != vm->last_mode) {
        const int prev = vm->last_mode;
        vm->last_mode = (int)mode;
        for (uint32_t chn = 0; chn < SUMI_MAX_VOICES; chn++) {
            if (!vm->notes[chn].active) continue;
            vm->notes[chn].active = false;
            sumi_voice_event_t end = {};
            end.kind = SUMI_VEV_VOICE_END;
            end.voice_id = (prev == (int)SUMI_INPUT_WIND) ? 0 : chn;
            end.value = 0.0f;   // silent set, no lift ring
            count = put(out, count, max, &end);
        }
    }

    for (uint32_t i = 0; i < in_count; i++) {
        const sumi_midi_event_t* m = &in[i];
        const uint8_t ch = m->channel & 0x0F;
        // §3.1: any message touching a tracked voice counts as activity
        // (wind's single voice lives in slot 0 whatever its channel).
        const uint32_t act = wind ? 0u : ch;
        vm->last_activity[act] = now;
        // #60: the zone test is a property of the channel; MPE and WIND both
        // read the per-note layer on member channels (an IMU wind controller
        // in its MPE mode plays its one voice on a member channel).
        const bool in_zone = zone.member_count > 0 &&
                             ch >= zone.first_member &&
                             ch < (uint8_t)(zone.first_member + zone.member_count);
        const bool member = (mpe || wind) && in_zone;
        // #60: in MPE mode a note on a NON-member channel (the master, or a
        // plain keyboard sharing the bath) is a classic per-(channel, note)
        // voice — chords do not collapse onto one channel voice. This is what
        // lets MPE be the default input mode for every keyboard on channel 1.
        const bool classic_voice = !wind && !(mpe && in_zone);
        sumi_voice_event_t ev = {};
        switch (m->kind) {
            case SUMI_MEV_NOTE_ON: {
                if (wind) {
                    // §2.3 as of #63: wind is MPE with one voice (slot 0,
                    // whatever the channel) plus a WAKE between notes — a
                    // legato change drags the sounding drop to the new site
                    // as a rigid tip (the §4.3.4 operator; no wandering-brush
                    // special case), then the old voice ends silently and the
                    // new note strikes exactly as an MPE note would.
                    if (vm->notes[0].active) {
                        sumi_voice_event_t wk = {};
                        wk.kind = SUMI_VEV_VOICE_MIGRATE;
                        wk.voice_id = 0;
                        wk.echo_count = sumi_layout_position(layout, m->a, params, aspect,
                                                             wk.ex, wk.ey);
                        wk.x = wk.ex[0]; wk.y = wk.ey[0];
                        // Aspect-corrected displacement of echo 0 (the lowering
                        // has no aspect; every echo of a note moves alike).
                        float px[SUMI_MAX_ECHOES], py[SUMI_MAX_ECHOES];
                        sumi_layout_position(layout, vm->notes[0].note, params, aspect, px, py);
                        wk.ax = (wk.ex[0] - px[0]) * aspect;
                        wk.ay = wk.ey[0] - py[0];
                        count = put(out, count, max, &wk);
                        sumi_voice_event_t end = {};
                        end.kind = SUMI_VEV_VOICE_END;
                        end.voice_id = 0;
                        end.value = 0.0f;   // legato hand-over, not a lift
                        count = put(out, count, max, &end);
                    }
                    vm->notes[0].active = true;
                    vm->notes[0].note = m->a;
                    ev.kind = SUMI_VEV_VOICE_BEGIN;
                    ev.voice_id = 0;
                    ev.note = m->a;
                    ev.echo_count = sumi_layout_position(layout, m->a, params, aspect,
                                                         ev.ex, ev.ey);
                    ev.x = ev.ex[0]; ev.y = ev.ey[0];
                    pitch_axis(m->a, layout, params, aspect, &ev.ax, &ev.ay);
                    ev.value = (float)m->b / 127.0f;
                    count = put(out, count, max, &ev);
                    break;
                }
                // Voice identity (§2.1): MPE keys voices by member channel —
                // newest note steals the channel's voice. Classic (and #60:
                // non-member channels in MPE mode) keys by (channel, note).
                // Classic ids are offset past the MPE table range (0..15).
                const uint32_t vid = classic_voice ? (0x1000u | ((uint32_t)ch << 8) | m->a) : ch;
                if (!classic_voice) {
                    if (vm->notes[ch].active) {
                        sumi_voice_event_t steal = {};
                        steal.kind = SUMI_VEV_VOICE_END;
                        steal.voice_id = ch;
                        steal.value = 0.0f;   // stolen, not lifted
                        count = put(out, count, max, &steal);
                    }
                    vm->notes[ch].active = true;
                    vm->notes[ch].note = m->a;
                }
                ev.kind = SUMI_VEV_VOICE_BEGIN;
                ev.voice_id = vid;
                ev.note = m->a;
                ev.echo_count = sumi_layout_position(layout, m->a, params, aspect,
                                                     ev.ex, ev.ey);
                ev.x = ev.ex[0]; ev.y = ev.ey[0];
                pitch_axis(m->a, layout, params, aspect, &ev.ax, &ev.ay);
                ev.value = (float)m->b / 127.0f;   // strike
                count = put(out, count, max, &ev);
                break;
            }
            case SUMI_MEV_NOTE_OFF: {
                if (wind) {
                    // Only the current note ends the brush; offs of already-
                    // migrated-away notes (legato overlaps) are ignored.
                    if (!vm->notes[0].active || vm->notes[0].note != m->a) break;
                    vm->notes[0].active = false;
                    ev.voice_id = 0;
                } else if (!classic_voice) {
                    // Off for a stolen (no longer owning) note: ignore.
                    if (!vm->notes[ch].active || vm->notes[ch].note != m->a) break;
                    vm->notes[ch].active = false;
                    ev.voice_id = ch;
                } else {
                    ev.voice_id = 0x1000u | ((uint32_t)ch << 8) | m->a;
                }
                ev.kind = SUMI_VEV_VOICE_END;
                ev.value = (float)m->b / 127.0f;   // lift
                count = put(out, count, max, &ev);
                break;
            }
            case SUMI_MEV_BEND: {
                if (member && wind) {
                    // #60: an MPE wind controller's bend glides the brush
                    // (its one voice lives in slot 0 whatever the channel).
                    if (vm->notes[0].active) {
                        vm->has_glide[0] = true;
                        vm->glide_val[0] = m->f;
                    }
                } else if (member) {
                    // Per-note glide (§2.1) — coalesced, last value wins.
                    vm->has_glide[ch] = true;
                    vm->glide_val[ch] = m->f;
                } else {
                    // Master / classic: global shear (§2.4), coalesced.
                    vm->have_master_bend = true;
                    vm->master_bend = m->f;
                }
                break;
            }
            case SUMI_MEV_CHANNEL_PRESSURE: {
                if (wind) {
                    // #63: exactly as MPE, on the one voice — press_mode picks
                    // the feed or the swirl; breath CCs feed regardless.
                    if (vm->notes[0].active) {
                        if (press_eff == 1) {
                            vm->has_swirl[0] = true;
                            vm->swirl_val[0] = (float)m->b / 127.0f;
                        } else {
                            vm->has_press[0] = true;
                            vm->press_val[0] = (float)m->b / 127.0f;
                        }
                    }
                } else if (member && vm->notes[ch].active) {
                    // v0.4 press_mode (§3.4): ONE consumer owns 0xD0 — the
                    // ink feed (0, the v1 grow) or the Lamb-Oseen swirl (1,
                    // pressure-only hardware's door to the swirl voice).
                    if (press_eff == 1) {
                        vm->has_swirl[ch] = true;
                        vm->swirl_val[ch] = (float)m->b / 127.0f;
                    } else {
                        vm->has_press[ch] = true;
                        vm->press_val[ch] = (float)m->b / 127.0f;
                    }
                }
                // Classic keyboards' channel pressure: ignored (§2.4).
                break;
            }
            case SUMI_MEV_POLY_PRESSURE: {
                // v0.4 §2.1: 0xA0, keyed by the voice's note on its member
                // channel -> the swirl dimension, in EITHER press_mode.
                // #60: the wind brush takes it on any channel (one voice).
                if (wind) {
                    if (vm->notes[0].active && vm->notes[0].note == m->a) {
                        vm->has_swirl[0] = true;
                        vm->swirl_val[0] = (float)m->b / 127.0f;
                    }
                } else if (member && vm->notes[ch].active && vm->notes[ch].note == m->a) {
                    vm->has_swirl[ch] = true;
                    vm->swirl_val[ch] = (float)m->b / 127.0f;
                }
                break;
            }
            case SUMI_MEV_CC: {
                if (m->a == 74 && wind) {
                    // #60: the IMU / slide layer of a wind controller rides
                    // CC 74 on its single channel — the brush's slide.
                    if (vm->notes[0].active) {
                        vm->has_slide[0] = true;
                        vm->slide_val[0] = (float)m->b / 127.0f;
                    }
                    break;
                }
                if (m->a == 74 && member && vm->notes[ch].active) {
                    vm->has_slide[ch] = true;              // MPE slide wins over the map
                    vm->slide_val[ch] = (float)m->b / 127.0f;
                    break;
                }
                // #62: CC 64 is never a paper dip any more (it was §2.4's
                // classic-only mapping, DECISIONS_3 #67). The pedal belongs to
                // the synth downstream; the dip is a deliberate host action
                // (sumi_trigger_paper_dip). CC 64 falls through to the CC map
                // like any other controller, unmapped by default.
                // §2.2 CC routing table; coalesced per dimension per update.
                const int8_t target = cc_lookup(vm, ch, m->a);
                if (target >= 0) {
                    const float value = (float)m->b / 127.0f;
                    if (wind && target == SUMI_CTL_INK_FLOW) {
                        // §2.3: breath feeds the single active voice — the
                        // unbounded MPE press integration since #63.
                        if (vm->notes[0].active) {
                            vm->has_press[0] = true;
                            vm->press_val[0] = value;
                        }
                    } else {
                        vm->have_ctl[target] = true;
                        vm->ctl_val[target] = value;
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    // Emit the per-update coalesced continuous dimensions (§3.4).
    for (uint32_t v = 0; v < SUMI_MAX_VOICES; v++) {
        sumi_voice_event_t ev = {};
        ev.voice_id = v;
        if (vm->has_glide[v]) {
            ev.kind = SUMI_VEV_VOICE_GLIDE;
            ev.value = vm->glide_val[v];
            count = put(out, count, max, &ev);
        }
        if (vm->has_press[v]) {
            ev.kind = SUMI_VEV_VOICE_PRESS;
            ev.value = vm->press_val[v];
            count = put(out, count, max, &ev);
        }
        if (vm->has_slide[v]) {
            ev.kind = SUMI_VEV_VOICE_SLIDE;
            ev.value = vm->slide_val[v];
            count = put(out, count, max, &ev);
        }
        if (vm->has_swirl[v]) {
            ev.kind = SUMI_VEV_VOICE_SWIRL;
            ev.value = vm->swirl_val[v];
            count = put(out, count, max, &ev);
        }
    }
    // §3.1: sweep for stuck voices — only when armed by an overflow, only
    // when a voice was silent ~10 s while other traffic kept flowing.
    if (vm->timeout_armed) {
        for (uint32_t chn = 0; chn < SUMI_MAX_VOICES; chn++) {
            if (!vm->notes[chn].active) continue;
            if (now - vm->last_activity[chn] > VOICE_TIMEOUT_S &&
                now - vm->last_traffic < VOICE_TIMEOUT_S) {
                vm->notes[chn].active = false;
                sumi_voice_event_t end = {};
                end.kind = SUMI_VEV_VOICE_END;
                end.voice_id = (mode == SUMI_INPUT_WIND) ? 0 : chn;
                end.value = 0.0f;
                count = put(out, count, max, &end);
                if (vm->log_cb) {
                    char buf[96];
                    snprintf(buf, sizeof(buf),
                             "mapper: stuck voice %u expired (overflow safeguard)", chn);
                    vm->log_cb(SUMI_LOG_INFO, buf, vm->log_user);
                }
            }
        }
    }

    if (vm->have_master_bend) {
        sumi_voice_event_t ev = {};
        ev.kind = SUMI_VEV_GLOBAL_BEND;
        ev.value = vm->master_bend;
        count = put(out, count, max, &ev);
    }
    for (uint32_t c = 0; c < SUMI_CTL_COUNT; c++) {
        if (!vm->have_ctl[c]) continue;
        sumi_voice_event_t ev = {};
        ev.kind = SUMI_VEV_GLOBAL_CTL;
        ev.dimension = c;
        ev.value = vm->ctl_val[c];
        count = put(out, count, max, &ev);
    }
    return count;
}

/* ------------------------------------------------------------------ */
/* Stage 2: §3.3 vocabulary -> deformations (budgeted)                 */
/* ------------------------------------------------------------------ */

// Budgeted push. Returns false when the frame budget is exhausted — the
// caller must keep its accumulator so the emission merges into a later frame.
static bool budget_push(sumi_voice_mapper_t* vm, sumi_deform_queue_t* queue,
                        const sumi_deform_t* d) {
    if (vm->frame_emitted >= vm->budget) {
        vm->merged_total++;
        return false;
    }
    if (!sumi_deform_queue_push(queue, d)) return false;
    vm->frame_emitted++;
    return true;
}

// Echo-set atomic reservation (§3.4): an echo set's passes emit all-or-none
// this frame — merging happens WITHIN an echo across frames, never by culling
// one echo of a set while feeding another.
static bool budget_reserve(sumi_voice_mapper_t* vm, uint32_t n) {
    if (vm->frame_emitted + n > vm->budget) {
        vm->merged_total += n;
        return false;
    }
    return true;
}

// Discrete events (note strikes, lift rings, dips) are never budget-dropped —
// §3.4's budget caps *continuous* deformation streams (glide tines, press
// feeds, global shear/vortex). Discrete pushes still count against the frame
// budget so continuous streams yield to them.
static void discrete_push(sumi_voice_mapper_t* vm, sumi_deform_queue_t* q,
                          const sumi_deform_t* d) {
    if (sumi_deform_queue_push(q, d)) vm->frame_emitted++;
}

static void emit_ink_drop(sumi_voice_mapper_t* vm, sumi_deform_queue_t* q,
                          float x, float y, float radius, float phase_base, float aux) {
    sumi_deform_t d;
    d.type = SUMI_DEFORM_DROP;
    d.as.drop.x = x;
    d.as.drop.y = y;
    d.as.drop.radius = radius;
    d.as.drop.phase_base = phase_base;
    d.as.drop.aux = aux;
    discrete_push(vm, q, &d);
}

void sumi_voice_mapper_lower(sumi_voice_mapper_t* vm,
                             const sumi_voice_event_t* events, uint32_t count,
                             double dt, const sumi_params_t* params,
                             bool dip_allowed,
                             uint32_t* drop_counter,
                             sumi_deform_queue_t* queue) {
    if (!vm || !events || !queue || !drop_counter) return;
    vm->frame_emitted = 0;
    vm->frames++;
    // bend_mode flip 1 -> 0 (#35): the bend stops feeding the amplitude, so
    // the residual target goes home — no shimmer stuck behind the toggle.
    uint32_t bend_eff = 0, slide_eff = 0, press_eff = 0;
    eff_modes(params, &bend_eff, &slide_eff, &press_eff);   // 1.1.0: the medium's binding table (MEDIUM §4)
    const bool anod = params && params->medium == SUMI_MEDIUM_ANOD;
    {
        const int bm = (int)bend_eff;
        if (vm->last_bend_mode == 1 && bm != 1) vm->ctl_t[SUMI_CTL_RIPPLE_AMP] = 0.0f;
        vm->last_bend_mode = bm;
        // #72: the same for the stir — a flip away from mode 4 stills it (no eddy stuck behind the toggle)
        if (vm->last_bend_eff == 4 && bm != 4) vm->ctl_t[SUMI_CTL_CHLADNI_A] = 0.0f;
        vm->last_bend_eff = bm;
    }

    const float expansion_rate = params ? params->expansion_rate : 1.0f;
    float smoothing_ms = params ? params->smoothing_ms : 30.0f;
    if (smoothing_ms < 1.0f) smoothing_ms = 1.0f;
    float fdt = (float)dt;
    if (fdt <= 0.0f) fdt = 1.0f / 120.0f;
    if (fdt > 0.1f) fdt = 0.1f;
    // Exponential smoothing coefficient for this frame (§3.4).
    const float alpha = 1.0f - expf(-fdt * 1000.0f / smoothing_ms);

    for (uint32_t i = 0; i < count; i++) {
        const sumi_voice_event_t* ev = &events[i];
        switch (ev->kind) {
            case SUMI_VEV_VOICE_BEGIN: {
                // Strike -> initial drop, radius ∝ sqrt(velocity) (§3.4), in
                // every input mode (#63 retired the thin wind touch-down). The
                // drop counter ticks ONCE per VoiceBegin: every echo shares
                // band and aux (§3.4 echo-set rules).
                const float radius0 = DROP_RADIUS_MIN + DROP_RADIUS_SPAN * sqrtf(ev->value);
                // step 43 (the author's table, #71): in Anod the strike's CHARGE is a fraction of
                // the Sumi drop (params.anod_drop); the spark shear below keeps the Sumi radius
                const float radius = (anod && params) ? radius0 * params->anod_drop : radius0;
                const float aux = (float)*drop_counter;
                const float phase = sumi_next_ink_phase_base(drop_counter);
                const uint32_t n_echo = (ev->echo_count >= 1 && ev->echo_count <= SUMI_MAX_ECHOES)
                                            ? ev->echo_count : 1;
                for (uint32_t e = 0; e < n_echo; e++) {
                    emit_ink_drop(vm, queue, ev->ex[e], ev->ey[e], radius, phase, aux);
                }
                if (ev->voice_id < SUMI_MAX_VOICES) {
                    sumi_mpe_voice_t* v = &vm->voices[ev->voice_id];
                    v->active = true;
                    v->echo_count = n_echo;
                    for (uint32_t e = 0; e < n_echo; e++) {
                        v->base_x[e] = v->cur_x[e] = ev->ex[e];
                        v->base_y[e] = v->cur_y[e] = ev->ey[e];
                    }
                    v->ax = ev->ax;
                    v->ay = ev->ay;
                    v->phase_base = phase;
                    v->aux_base = aux;
                    v->press_t = v->press_s = 0.0f;
                    v->slide_t = v->slide_s = 0.0f;
                    v->slide_baked = 0.0f;
                    v->slide_primed = false;
                    v->glide_t = v->glide_s = 0.0f;
                    v->swirl_t = v->swirl_s = 0.0f;
                    v->pending_swirl = 0.0f;
                    v->nominal_radius = radius;
                    v->pending_grow = 0.0f;
                    v->feeding = false;
                    v->fed_once = false;
                    // v0.10: the torsion sweep episode, armed by the strike.
                    v->sweep_on = params && params->torsion_sweep == 1;
                    v->sweep_t = 0.0f;
                    v->sweep_pending = 0.0f;
                    v->sweep_radius = radius * TORSION_SWEEP_REACH < TORSION_SWEEP_MIN_R
                                          ? TORSION_SWEEP_MIN_R : radius * TORSION_SWEEP_REACH;
                    v->feed_t = 0.0f;
                }
                // 1.1.0 (MEDIUM §4, revised at step 43 — #71 — and after step 46 — #88):
                // in Anod the strike is THE CLASSIC SPARK on the charge, sumi_add_spark's
                // composition — the charge above (radius0 · anod_drop), a burst of core =
                // the charge with its lobes along the note's pitch axis (D = 0.3 · charge,
                // the order params.burst_order) and the spark shear episode with the
                // charge as its band and kick base: thick, short streamers, which a lossy
                // renderer keeps (#80) where #71's threads on the Sumi radius vanished.
                if (anod) {
                    const float th = atan2f(ev->ay, ev->ax);
                    for (uint32_t e = 0; e < n_echo; e++) {
                        sumi_voice_mapper_add_burst(vm, ev->ex[e], ev->ey[e], radius, ANOD_STRIKE_BURST_D * radius, th, 0u, params);
                        sumi_voice_mapper_add_spark(vm, ev->ex[e], ev->ey[e], radius, th, params);
                    }
                }
                break;
            }
            case SUMI_VEV_VOICE_END: {
                // bend_mode 1 (#35): the shimmer belongs to held notes' bends
                // — when the last voice releases, the amplitude target goes
                // home to zero and the water stills (smoothly, via the ctl
                // smoothing).
                if ((params && params->bend_mode == 1) || bend_eff == 4) {
                    bool any_other = false;
                    for (uint32_t v2 = 0; v2 < SUMI_MAX_VOICES; v2++) {
                        if (v2 != ev->voice_id && vm->voices[v2].active) {
                            any_other = true;
                            break;
                        }
                    }
                    // #72: the stir the same way — a note lifted while bent (the ROLI's slide-and-lift) must not stir on
                    if (!any_other) { if (params && params->bend_mode == 1) vm->ctl_t[SUMI_CTL_RIPPLE_AMP] = 0.0f; if (bend_eff == 4) vm->ctl_t[SUMI_CTL_CHLADNI_A] = 0.0f; }
                }
                if (ev->voice_id < SUMI_MAX_VOICES && vm->voices[ev->voice_id].active) {
                    // Lift -> the drop simply "sets": feeding stops, nothing
                    // is stamped. (#41 removes the §3.4 lift ring — the
                    // user: the clear drop at note-off "is really ruining
                    // the experience; the joystick's disappearance is
                    // feedback enough". Supersedes #21's ring placement.)
                    vm->voices[ev->voice_id].active = false;
                }
                break;
            }
            case SUMI_VEV_VOICE_GLIDE:
                // v0.4 bend_mode (§4.3(6), #35 corrected): mode 1 routes the
                // PER-NOTE bend to the sine ripple's AMPLITUDE, derived from
                // the bend's DISTANCE FROM CENTER — exactly like glide
                // displacement, which is the point: vibrato breathes the
                // shimmer and the water stills itself when the note returns
                // to center (the ripple group property — A back to zero
                // composes back). |±1.5| semitones saturate the amp ctl (a
                // ±0.5-semitone vibrato breathes a third — #66: four times
                // the v0.4 |±6| law, which read too weak on real MPE
                // vibrato); last writer wins
                // across voices; smoothed like any global control. The
                // wavelength k stays a flavor ctl (RIPPLE_FREQ, mid default).
                // The drop HOLDS (glide_t untouched — one consumer owns the
                // note bend); flipping back to glide lets glide_s catch up
                // smoothly. Master bend keeps its v1 shear tine regardless.
                if (bend_eff == 1) {
                    float a = ev->value >= 0.0f ? ev->value : -ev->value;
                    a /= 1.5f;   // #66
                    if (a > 1.0f) a = 1.0f;
                    vm->ctl_t[SUMI_CTL_RIPPLE_AMP] = a;
                    break;
                }
                if (bend_eff == 2 || bend_eff == 3) {
                    // 1.1.0 (MEDIUM §4): the note bend plays a WAVENUMBER — the
                    // torsion's (mode 2) or the spark's (mode 3): the rest
                    // position is mid-range, ±1.5 semitones span it (the ripple
                    // law's reach, #66); last writer wins across voices.
                    float t = 0.5f + ev->value / 3.0f;
                    if (t < 0.0f) t = 0.0f;
                    if (t > 1.0f) t = 1.0f;
                    vm->ctl_t[bend_eff == 2 ? SUMI_CTL_TORSION_K : SUMI_CTL_SPARK_K] = t;
                    break;
                }
                if (bend_eff == 4) {
                    // step 43 (the author's table, Anod's default): the note bend
                    // plays the CHLADNI STIR — its distance from centre the rate
                    // (±1.5 semitones saturate, the ripple law's reach), its sign
                    // the sense (a bend down stirs the other way: the exact
                    // inverse), so a vibrato stirs back and forth; last writer
                    // wins across voices AND against a CC on the dim (#72: the
                    // desktop maps CC 106 to the stir by default, and deferring
                    // to a mapped-but-silent CC had left this route dead).
                    {
                        float a = ev->value >= 0.0f ? ev->value : -ev->value;
                        a /= 1.5f;
                        if (a > 1.0f) a = 1.0f;
                        vm->ctl_t[SUMI_CTL_CHLADNI_A] = a;
                        if (ev->value > 0.02f) vm->chladni_dir = 1.0f; else if (ev->value < -0.02f) vm->chladni_dir = -1.0f;
                    }
                    break;
                }
                if (ev->voice_id < SUMI_MAX_VOICES) vm->voices[ev->voice_id].glide_t = ev->value;
                break;
            case SUMI_VEV_VOICE_PRESS:
                if (ev->voice_id < SUMI_MAX_VOICES) vm->voices[ev->voice_id].press_t = ev->value;
                break;
            case SUMI_VEV_VOICE_SWIRL:
                if (ev->voice_id < SUMI_MAX_VOICES) vm->voices[ev->voice_id].swirl_t = ev->value;
                break;
            case SUMI_VEV_VOICE_SLIDE:
                if (ev->voice_id < SUMI_MAX_VOICES) {
                    sumi_mpe_voice_t* v = &vm->voices[ev->voice_id];
                    if (!v->slide_primed) {
                        // First CC74 = the controller's rest position, not a
                        // gesture: snap, so neither aux nor a mode-1 pinch
                        // sees a spurious 0 -> rest sweep.
                        v->slide_s = v->slide_baked = ev->value;
                        v->slide_primed = true;
                    }
                    v->slide_t = ev->value;
                    // v0.13 (slide_mode 2, prepared for the Anod binding table,
                    // step 42): the slide is the spark shear's wavenumber — a
                    // GLOBAL flavour ctl, so the latest voice's slide wins.
                    if (slide_eff == 2) vm->ctl_t[SUMI_CTL_SPARK_K] = ev->value;
                }
                break;
            case SUMI_VEV_GLOBAL_BEND: {
                const float delta = ev->value - vm->bend_semis;
                vm->bend_semis = ev->value;
                if (delta > -1e-4f && delta < 1e-4f) break;
                sumi_deform_t d;
                d.type = SUMI_DEFORM_TINE;
                d.as.tine.y0 = 0.5f;
                d.as.tine.y1 = 0.5f;
                if (delta >= 0.0f) { d.as.tine.x0 = 0.0f; d.as.tine.x1 = 1.0f; }
                else               { d.as.tine.x0 = 1.0f; d.as.tine.x1 = 0.0f; }
                d.as.tine.alpha = SHEAR_ALPHA;
                d.as.tine.magnitude = (delta >= 0.0f ? delta : -delta) * SHEAR_PER_SEMI;
                budget_push(vm, queue, &d);
                break;
            }
            case SUMI_VEV_VOICE_MIGRATE: {
                // #63 wind legato: drag the sounding drop to the new note as a
                // rigid tip of its current radius — the §4.3.4 wake, profile
                // and spread from params, sub-stepped <= a/4 like the stylus
                // (engine.cpp sumi_add_wake, which this mirrors). ev->ax/ay
                // carry the aspect-corrected displacement (same for every
                // echo of a note). The voice itself ends right after (the
                // VOICE_END that follows in the same batch).
                if (ev->voice_id < SUMI_MAX_VOICES && vm->voices[ev->voice_id].active) {
                    const sumi_mpe_voice_t* v = &vm->voices[ev->voice_id];
                    const uint32_t n_echo = (ev->echo_count >= 1 && ev->echo_count <= SUMI_MAX_ECHOES)
                                                ? ev->echo_count : 1;
                    float tip = v->nominal_radius;
                    if (tip < WIND_WAKE_TIP_MIN) tip = WIND_WAKE_TIP_MIN;
                    const float len = sqrtf(ev->ax * ev->ax + ev->ay * ev->ay);
                    if (len > 1e-6f) {
                        uint32_t n = (uint32_t)ceilf(len / (tip * 0.25f));
                        if (n < 1) n = 1;
                        if (n > WIND_WAKE_MAX_STEPS) n = WIND_WAKE_MAX_STEPS;
                        const bool viscous = params && params->wake_profile == 1;
                        float spread = params ? params->wake_spread : 3.0f;
                        if (spread < 1.5f) spread = 1.5f;
                        if (spread > 12.0f) spread = 12.0f;
                        for (uint32_t e = 0; e < n_echo && e < v->echo_count; e++) {
                            const float x0 = v->cur_x[e], y0 = v->cur_y[e];
                            const float x1 = ev->ex[e], y1 = ev->ey[e];
                            for (uint32_t i = 1; i <= n; i++) {
                                const float t = (float)i / (float)n;
                                sumi_deform_t d;
                                if (viscous) {
                                    d.type = SUMI_DEFORM_STOKESLET;
                                    d.as.stokeslet.x = x0 + (x1 - x0) * t;
                                    d.as.stokeslet.y = y0 + (y1 - y0) * t;
                                    d.as.stokeslet.dx_ac = ev->ax / (float)n;
                                    d.as.stokeslet.dy_ac = ev->ay / (float)n;
                                    d.as.stokeslet.tip_radius = tip;
                                    d.as.stokeslet.spread = spread;
                                } else {
                                    d.type = SUMI_DEFORM_WAKE;
                                    d.as.wake.x = x0 + (x1 - x0) * t;   // tip AFTER this sub-step
                                    d.as.wake.y = y0 + (y1 - y0) * t;
                                    d.as.wake.dx_ac = ev->ax / (float)n;
                                    d.as.wake.dy_ac = ev->ay / (float)n;
                                    d.as.wake.tip_radius = tip;
                                }
                                discrete_push(vm, queue, &d);   // a note event, never dropped
                            }
                        }
                    }
                }
                break;
            }
            case SUMI_VEV_GLOBAL_CTL: {
                if (ev->dimension < SUMI_CTL_COUNT) {
                    vm->ctl_t[ev->dimension] = ev->value;   // smoothed in the tick
                }
                break;
            }
            case SUMI_VEV_PAPER_DIP: {
                if (!dip_allowed) {
                    // §5.3: both print buffers busy -> the dip is refused.
                    if (vm->log_cb) {
                        vm->log_cb(SUMI_LOG_WARN,
                                   "mapper: paper dip refused (both print buffers busy)",
                                   vm->log_user);
                    }
                    break;
                }
                *drop_counter = 0;   // §4.2: aux rebase — fresh sheet, fresh counter
                sumi_deform_t d = { SUMI_DEFORM_RESET, {} };
                discrete_push(vm, queue, &d);
                break;
            }
            default:
                break;
        }
    }

    // Per-frame voice tick (§3.4 smoothing + §4.4 continuous feeds). Runs
    // after event application so this frame's targets are already in place.

    float poly_max = 0.0f;   // 1.1.0: Anod's poly-pressure dimension -> the Chladni stir
    for (uint32_t vid = 0; vid < SUMI_MAX_VOICES; vid++) {
        sumi_mpe_voice_t* v = &vm->voices[vid];
        // v0.10 (Phase 6 step 36): the torsion SWEEP episode — φ advances at
        // ω, the amplitude decays with τ, and every frame emits the rotation
        // INCREMENT of that frame (rate · envelope · dt), never a total: two
        // strikes in the same place add up, and a merged frame carries its
        // increment over. Runs whether or not the note is still held.
        if (v->sweep_on) {
            v->sweep_t += fdt;
            if (v->sweep_t > TORSION_SWEEP_TAU * TORSION_SWEEP_LIFE) v->sweep_on = false;
            else v->sweep_pending += TORSION_SWEEP_RATE * expf(-v->sweep_t / TORSION_SWEEP_TAU) * fdt;
        }
        // 1.1.0 (MEDIUM §4, Anod): the press FEED spends torsion instead of ink —
        // pressure adds to the same pending rotation the note-on sweep spends,
        // at TORSION_FEED_RATE per second at full pressure, with its own phase
        // clock so the rings keep travelling while the key is held.
        if (press_eff == 2 && v->active && v->press_s > PRESS_DEADZONE) {
            v->sweep_pending += v->press_s * TORSION_FEED_RATE * fdt;
            v->feed_t += fdt;
        }
        {
            {
                if (v->sweep_pending >= TORSION_SWEEP_MIN_EMIT && budget_reserve(vm, v->echo_count)) {
                    float k = 0.0f, phase_ctl = 0.0f;
                    sumi_voice_mapper_torsion_kphi(vm, SUMI_VORTEX_TORSION, &k, &phase_ctl);
                    const float phi = fmodf(phase_ctl + TORSION_SWEEP_OMEGA * (v->sweep_t + v->feed_t), 6.2831853f);
                    for (uint32_t e = 0; e < v->echo_count; e++) {
                        sumi_deform_t d;
                        d.type = SUMI_DEFORM_VORTEX;
                        d.as.vortex.x = v->cur_x[e];
                        d.as.vortex.y = v->cur_y[e];
                        d.as.vortex.strength = v->sweep_pending;
                        d.as.vortex.radius = v->sweep_radius;
                        d.as.vortex.profile = SUMI_VORTEX_TORSION;
                        d.as.vortex.k = k;
                        d.as.vortex.phase = phi;
                        budget_push(vm, queue, &d);   // reserved: cannot fail on budget
                    }
                    v->sweep_pending = 0.0f;
                }
            }
        }
        if (!v->active) continue;

        v->press_s += (v->press_t - v->press_s) * alpha;
        v->slide_s += (v->slide_t - v->slide_s) * alpha;
        v->glide_s += (v->glide_t - v->glide_s) * alpha;
        v->swirl_s += (v->swirl_t - v->swirl_s) * alpha;

        // Glide -> drag the voice's center(s) along the pitch axis, emitting
        // a narrow tine along each echo's drag path (§3.4 — per-voice, never
        // global; the same lattice vector for every echo). Echo sets emit
        // all-or-none against the budget (merge within, never cull one).
        const float mdx0 = (v->base_x[0] + v->ax * v->glide_s) - v->cur_x[0];
        const float mdy0 = (v->base_y[0] + v->ay * v->glide_s) - v->cur_y[0];
        const float move = sqrtf(mdx0 * mdx0 + mdy0 * mdy0);
        if (move >= GLIDE_MIN_MOVE && budget_reserve(vm, v->echo_count)) {
            for (uint32_t e = 0; e < v->echo_count; e++) {
                const float tx = v->base_x[e] + v->ax * v->glide_s;
                const float ty = v->base_y[e] + v->ay * v->glide_s;
                sumi_deform_t d;
                d.type = SUMI_DEFORM_TINE;
                d.as.tine.x0 = v->cur_x[e];
                d.as.tine.y0 = v->cur_y[e];
                d.as.tine.x1 = tx;
                d.as.tine.y1 = ty;
                d.as.tine.alpha = GLIDE_TINE_ALPHA;
                d.as.tine.magnitude = move;
                budget_push(vm, queue, &d);   // reserved: cannot fail on budget
                v->cur_x[e] = tx;   // not updated on merge: motion carries over
                v->cur_y[e] = ty;
            }
        }

        // Press -> sustained ink feed: small incremental expansions re-emitted
        // per frame at the voice's current center (§4.4). The accumulated step
        // is BOUNDARY growth (§3.4: the drop expands at a rate ∝ pressure); a
        // center expansion of radius r moves an existing boundary R only to
        // sqrt(R² + r²), so the emitted pass radius converts via the area
        // relation r = sqrt((R+ΔR)² − R²) (see DECISIONS.md). Steps below the
        // threshold (or over budget) accumulate and merge.
        // §4.4 feed episodes: a press onset after a full release starts a NEW
        // ink band seeded small at the center — repeated pressure pulses stamp
        // nested rings instead of overwriting one interior. The first episode
        // continues the strike's band (the strike drop simply keeps growing).
        if (press_eff != 2 && !v->feeding && v->press_s > FEED_ONSET) {
            v->feeding = true;
            if (v->fed_once) {
                v->aux_base = (float)*drop_counter;
                v->phase_base = sumi_next_ink_phase_base(drop_counter);
                v->nominal_radius = FEED_SEED_RADIUS;
                v->pending_grow = 0.0f;
            }
            v->fed_once = true;
        } else if (v->feeding && v->press_s < FEED_RELEASE) {
            v->feeding = false;
        }

        if (press_eff != 2 && v->press_s > PRESS_DEADZONE) {
            // Unbounded in every mode since #63 (the wind brush's width clamp
            // made breath-fed drops too weak): the Osmose behaviour for breath too.
            const float g = v->press_s * fdt * expansion_rate * FEED_RATE;
            v->pending_grow += g;
        }
        if (v->pending_grow >= FEED_MIN_GROW && budget_reserve(vm, v->echo_count)) {
            const float R = v->nominal_radius;
            float grow = v->pending_grow;
            float r_emit = sqrtf((R + grow) * (R + grow) - R * R);
            if (r_emit > FEED_MAX_EMIT) {
                r_emit = FEED_MAX_EMIT;
                grow = sqrtf(R * R + r_emit * r_emit) - R;   // growth actually applied
            }
            // All echoes grow in lockstep: same pass radius, same band, same
            // aux; one shared nominal boundary (§3.4 echo-set rules).
            for (uint32_t e = 0; e < v->echo_count; e++) {
                sumi_deform_t d;
                d.type = SUMI_DEFORM_DROP;
                d.as.drop.x = v->cur_x[e];
                d.as.drop.y = v->cur_y[e];
                d.as.drop.radius = r_emit;
                d.as.drop.phase_base = v->phase_base;              // same band: the drop GROWS
                // slide -> aux modulation is the slide_mode = 0 routing; in
                // mode 1 the slide drives the pinch instead (v0.4, §3.4), in
                // mode 2 the spark's wavenumber (v0.13) — one consumer.
                d.as.drop.aux = v->aux_base + (slide_eff != 0 ? 0.0f : v->slide_s * 0.9f);
                budget_push(vm, queue, &d);   // reserved: cannot fail on budget
            }
            v->pending_grow -= grow;
            v->nominal_radius = R + grow;
        }

        // v0.4 §4.3(7): the swirl — a Lamb-Oseen vortex whose core IS the
        // voice's drop (r_c = nominal boundary R): its own rings rotate
        // near-rigidly and stay coherent while the far field stirs the
        // neighbors. Core-angle steps accumulate below the emission
        // threshold and merge (budget starvation carries over, like press
        // growth); the sign is the voice's band parity — adjacent notes
        // counter-rotate, zero configuration. Echo sets emit all-or-none.
        // 1.1.0 (MEDIUM §4): in Anod the poly-pressure dimension is the CHLADNI
        // STIR — the loudest voice's pressure sets the stir's target below
        // (unless a CC is mapped to it: the CC map overrides) — and the
        // Lamb–Oseen swirl stays quiet.
        if (anod) {
            if (v->swirl_s > poly_max) poly_max = v->swirl_s;
        } else if (v->swirl_s > SWIRL_DEADZONE) {
            v->pending_swirl += v->swirl_s * fdt * expansion_rate * SWIRL_OMEGA;
        }
        if (v->pending_swirl >= SWIRL_MIN_EMIT && budget_reserve(vm, v->echo_count)) {
            const float rc = v->nominal_radius > 1e-4f ? v->nominal_radius : 1e-4f;
            const float sign = (((long)v->phase_base) % 2 == 1) ? 1.0f : -1.0f;
            const float S = sign * v->pending_swirl * 6.2831853f * rc * rc;
            for (uint32_t e = 0; e < v->echo_count; e++) {
                sumi_deform_t d;
                d.type = SUMI_DEFORM_SWIRL;
                d.as.swirl.x = v->cur_x[e];
                d.as.swirl.y = v->cur_y[e];
                d.as.swirl.strength = S;
                d.as.swirl.core_r = rc;
                budget_push(vm, queue, &d);   // reserved: cannot fail on budget
            }
            v->pending_swirl = 0.0f;
        }

        // v0.4 (§4.3(5), slide_mode = 1): smoothed CC74 DELTAS drive the
        // Hamiltonian pinch at the voice's current position — k is always a
        // delta, never the absolute value (absolute feeding integrates into
        // runaway strain). Fold axis: the voice's lattice pitch axis — the
        // in-band default; the stylus path (Step 20) passes the pen azimuth
        // through sumi_add_pinch instead (DECISIONS_3 #32). Echo sets emit
        // all-or-none, like every other per-voice stream.
        if (slide_eff == 1) {
            const float dk = (v->slide_s - v->slide_baked) * PINCH_K_SCALE;
            const float adk = dk >= 0.0f ? dk : -dk;
            // Crossed-tine variant costs two passes per echo (#34).
            const uint32_t per_echo = params->pinch_variant == 1 ? 2u : 1u;
            if (adk >= PINCH_MIN_K && budget_reserve(vm, per_echo * v->echo_count)) {
                const float angle = atan2f(v->ay, v->ax);
                for (uint32_t e = 0; e < v->echo_count; e++) {
                    if (params->pinch_variant == 1) {
                        sumi_deform_t t[2];
                        sumi_deform_crossed_pinch(v->cur_x[e], v->cur_y[e],
                                                  v->ax, v->ay, dk, t);
                        budget_push(vm, queue, &t[0]);   // reserved: cannot fail
                        budget_push(vm, queue, &t[1]);
                        continue;
                    }
                    sumi_deform_t d;
                    d.type = SUMI_DEFORM_PINCH;
                    d.as.pinch.x = v->cur_x[e];
                    d.as.pinch.y = v->cur_y[e];
                    d.as.pinch.k = dk;
                    d.as.pinch.angle = angle;
                    d.as.pinch.window_s = PINCH_WINDOW_S;
                    budget_push(vm, queue, &d);   // reserved: cannot fail on budget
                }
                v->slide_baked = v->slide_s;
            }
        }
    }

    // Global field controls (§2.2): smooth, then run the per-frame agitation.
    for (uint32_t c = 0; c < SUMI_CTL_COUNT; c++) {
        vm->ctl_s[c] += (vm->ctl_t[c] - vm->ctl_s[c]) * alpha;
    }
    // v0.11 (Phase 6 step 37): the Chladni cellular flow, bake only — a
    // STEADY stir while the A control is up (the vortex's pattern: rate ×
    // dt each frame, never an absolute), one step = two exact diagonal shear
    // passes. Iterating the same area-preserving step is chaotic advection:
    // the eddies wrap their cells, the boundaries stretch the ink into
    // filaments — the figure forms by stretching, which is all Liouville
    // allows (a map with det J = 1 cannot change density). The lattice is
    // recomputed every frame (a few flops) and snaps on a layout change.
    // step 43 (the author's table): in Anod the poly-pressure dimension plays the WAVENUMBERS — the torsion's
    // and the spark's — from their rest at mid-range up (k = ½ + ½·pressure), under the one-consumer rule: a
    // wavenumber the bend (modes 2/3) or the slide (mode 2, spark k — the Anod default) owns is left to its
    // owner. #72: a CC on the dim SHARES it — pressure takes the wavenumber while any voice presses and gives
    // it back where it was at the release (a knob's setting survives a gesture); deferring to a mapped CC had
    // left the route dead on the desktop (CC 104/108 are its default handles). The stir is the bend's (mode 4).
    if (anod) {
        const bool tk_free = bend_eff != 2u, sk_free = bend_eff != 3u && slide_eff != 2u;
        if (poly_max > 0.004f) {   // half a MIDI step: the smoothed pressure's tail does not hold the dims
            if (!vm->poly_on) { vm->poly_on = true; vm->poly_hold_tk = vm->ctl_t[SUMI_CTL_TORSION_K]; vm->poly_hold_sk = vm->ctl_t[SUMI_CTL_SPARK_K]; }
            const float k = 0.5f + 0.5f * poly_max;
            if (tk_free) vm->ctl_t[SUMI_CTL_TORSION_K] = k;
            if (sk_free) vm->ctl_t[SUMI_CTL_SPARK_K] = k;
        } else if (vm->poly_on) {
            vm->poly_on = false;
            if (tk_free) vm->ctl_t[SUMI_CTL_TORSION_K] = vm->poly_hold_tk;
            if (sk_free) vm->ctl_t[SUMI_CTL_SPARK_K] = vm->poly_hold_sk;
        }
    }
    {
        // step 43 — AN EDDY IN EVERY CELL: a pass turns every display disc
        // about its centre, as a RING — A·rate·dt at ρ = 1/√2, zero at the core
        // and at the rim (the renderer holds the discs — layouts.cpp's cells —
        // and which texel lies in which); r is preserved inside each disc, the
        // core and the water between the discs rest: exact. B sets the odd
        // cells' sense: 0 neighbours counter-rotate, ½ every other cell rests,
        // 1 all turn the same way.
        // THE EMISSION FLOOR: a frame's rotation moves the fastest texel by 0.727·θ·R,
        // below the half-float quantum at any playable rate — a pass that small rounds
        // back to where it started on a fresh sheet and nothing accumulates (measured:
        // 0.07 rad after 150 frames of 0.0125). So the rotation is banked and emitted
        // as one pass whenever it carries at least SUMI_CELLS_MIN_EMIT of displacement
        // on the smallest disc; the stir stays exact, only its clock coarsens.
        const float A = vm->ctl_s[SUMI_CTL_CHLADNI_A];
        if (A > 0.002f) {
            vm->cells_pending += vm->chladni_dir * A * SUMI_CHLADNI_RATE * fdt;   // signed: the bend's sense
            const float theta_min = vm->cells_rmin > 0.0f ? SUMI_CELLS_MIN_EMIT / (SUMI_CELLS_PEAK * vm->cells_rmin) : 0.0f;
            const float pend = vm->cells_pending < 0.0f ? -vm->cells_pending : vm->cells_pending;
            if (pend >= theta_min && budget_reserve(vm, 1)) {
                float balance = vm->ctl_s[SUMI_CTL_CHLADNI_B];
                if (balance < 0.0f) balance = 0.0f;
                if (balance > 1.0f) balance = 1.0f;
                sumi_deform_t d;
                d.type = SUMI_DEFORM_CELLS;
                d.as.cells.theta = vm->cells_pending;
                d.as.cells.odd_weight = 2.0f * balance - 1.0f;
                d.as.cells.mode = params ? params->chladni_mode : SUMI_CHLADNI_DISCS;
                sumi_deform_queue_push(queue, &d);
                vm->frame_emitted += 1;
                vm->cells_pending = 0.0f;
            }
        } else {
            vm->cells_pending = 0.0f;                    // a sub-quantum remainder cannot be applied: it is let go
        }
    }
    // v0.12 (Phase 6 step 38): the burst episodes. Each frame the age
    // advances (l² linear in t: l² = a² + 4νt) and the increment since the
    // last emitted age goes out as passes, the greedy march keeping every
    // pass's peak displacement within β_m·l0 (displacement.h). An increment
    // below the floor waits for the next frame (it merges by construction);
    // what the pass budget refuses waits the same way. The last sliver of a
    // release that is below the floor closes the episode without a pass.
    for (uint32_t bi = 0; bi < SUMI_MAX_BURSTS; bi++) {
        sumi_burst_slot_t* b = &vm->bursts[bi];
        if (!b->on) continue;
        b->t += fdt;
        float l_target = b->l_end;
        if (b->life > 0.0f) {
            float f = b->t / b->life;
            if (f > 1.0f) f = 1.0f;
            l_target = sqrtf(b->a * b->a + (b->l_end * b->l_end - b->a * b->a) * f);
        }
        const bool final_piece = l_target >= b->l_end * (1.0f - 1e-6f);
        if (sumi_burst_peak(b->m, b->amp, b->a, b->l_cur, l_target) < BURST_MIN_EMIT) {
            if (final_piece) b->on = false;
            continue;
        }
        int emitted = 0;
        while (b->l_cur < l_target * (1.0f - 1e-6f) && emitted < BURST_MAX_PER_FRAME) {
            const double l_next = sumi_burst_step(b->m, b->amp, b->a, b->l_cur, l_target);
            if (!(l_next > (double)b->l_cur)) break;
            sumi_deform_t d;
            d.type = SUMI_DEFORM_BURST;
            d.as.burst.x = b->x;
            d.as.burst.y = b->y;
            d.as.burst.a = b->a;
            d.as.burst.amp = b->amp;
            d.as.burst.l0 = b->l_cur;
            d.as.burst.l1 = (float)l_next;
            d.as.burst.theta0 = b->theta0;
            d.as.burst.m = b->m;
            if (!budget_push(vm, queue, &d)) break;
            b->l_cur = (float)l_next;
            emitted++;
        }
        if (b->l_cur >= b->l_end * (1.0f - 1e-6f)) b->on = false;
    }
    // v0.13 (Phase 6 step 39): the spark shear episodes. The kick decays as
    // e^{−t/τ}: each frame the exact increment A_tot·(e^{−t0/τ} − e^{−t1/τ})
    // joins the pending kick, which goes out as one kick-drift step (two
    // exact passes) once it reaches the field's quantum; the last sliver
    // flushes when the episode ends at LIFE·τ. Successive steps do not
    // commute, so the emission granularity is part of the look — bounded
    // below by the quantum, above by the frame.
    for (uint32_t si = 0; si < SUMI_MAX_SPARKS; si++) {
        sumi_spark_slot_t* s = &vm->sparks[si];
        if (!s->on) continue;
        const float t0 = s->t;
        s->t += fdt;
        const float dec = expf(-t0 / s->tau) - expf(-s->t / s->tau);
        s->pend_a += s->a_tot * dec;
        s->pend_b += s->b_tot * dec;
        const bool over = s->t >= SPARK_LIFE * s->tau;
        const float pa = s->pend_a >= 0.0f ? s->pend_a : -s->pend_a;
        const float pb = s->pend_b >= 0.0f ? s->pend_b : -s->pend_b;
        const float pk = pa > pb ? pa : pb;
        if ((pk >= SPARK_MIN_EMIT || (over && pk > 0.0f)) && budget_reserve(vm, 2)) {
            vm->frame_emitted += sumi_spark_emit_step(queue, s->x, s->y, s->pend_a, s->pend_b, s->k, s->phase,
                                                      s->theta0, s->band, s->stack, s->profile);
            s->pend_a = s->pend_b = 0.0f;
        }
        if (over && s->pend_a == 0.0f && s->pend_b == 0.0f) s->on = false;
    }
    // v0.14 (Phase 6 step 40): the Chirikov standard map, DELTA-driven from
    // the CHIRIKOV_K control (the mod wheel / breath under the Anod table,
    // step 42). A throw of δ this frame applies ONE step of the map with the
    // kick and the drift both scaled by δ — the identity at δ = 0, the full
    // map at δ = 1 — so the step's chaos parameter is δ²·K_max: a wheel eased
    // over m frames is m steps at K_max/m² (the pendulum flow, integrable:
    // sheets), a wheel THROWN is one hard kick (chaos). The per-step K is
    // capped at SUMI_CHIRIKOV_K_CEIL (the erosion gate): the remainder of the
    // throw follows in the next frames. A negative δ applies the exact
    // inverse step — the wheel down retraces, step for step. The map is
    // centred where the vortex is (the same hand steers).
    {
        // 1.1.0 (MEDIUM §4): the throw's SOURCE is the medium's — the dedicated
        // CHIRIKOV_K control in Sumi, the mod-wheel dimension (VORTEX_STRENGTH)
        // in Anod, where the vortex stays quiet. A medium switch re-baselines
        // the delta tracker so the switch itself is not a throw.
        const uint32_t med = params ? params->medium : 0u;
        const sumi_ctl_t src = anod ? SUMI_CTL_VORTEX_STRENGTH : SUMI_CTL_CHIRIKOV_K;
        if (med != vm->last_medium) { vm->chirikov_baked = vm->ctl_s[src]; vm->last_medium = med; }
    }
    if (params && params->chirikov_kmax > 0.0f) {
        const float delta = vm->ctl_s[anod ? SUMI_CTL_VORTEX_STRENGTH : SUMI_CTL_CHIRIKOV_K] - vm->chirikov_baked;
        const float ad = delta >= 0.0f ? delta : -delta;
        if (ad >= CHIRIKOV_MIN_DELTA && budget_reserve(vm, 2)) {
            float kmax = params->chirikov_kmax;
            if (kmax > 2.0f) kmax = 2.0f;
            uint32_t periods = params->chirikov_periods;
            if (periods < 1u) periods = 1u;
            if (periods > 8u) periods = 8u;
            float eps = params->chirikov_eps;
            if (!(eps >= 0.05f)) eps = 0.05f;
            if (eps > 1.0f) eps = 1.0f;
            const float dcap = sqrtf(SUMI_CHIRIKOV_K_CEIL / kmax);          // δ² K_max ≤ the ceiling
            const float da = ad < dcap ? ad : dcap;
            const float k = 6.2831853f * (float)periods;
            vm->frame_emitted += sumi_chirikov_emit_step(queue,
                vm->ctl_s[SUMI_CTL_VORTEX_X], 1.0f - vm->ctl_s[SUMI_CTL_VORTEX_Y],
                da * kmax / (k * eps), k, 0.0f, da * eps, delta < 0.0f);
            vm->chirikov_baked += delta < 0.0f ? -da : da;
        }
    }
    {
        // Vortex: dt-scaled, damped by viscosity (§2.2 "fluid viscosity /
        // damping"). Roughness/palette-morph are smoothed here but only
        // consumed by the composite in a later step (DECISIONS.md).
        const float damping = 1.0f - VISCOSITY_DAMP * vm->ctl_s[SUMI_CTL_VISCOSITY];
        const float theta = vm->ctl_s[SUMI_CTL_VORTEX_STRENGTH] * VORTEX_RATE * fdt * damping;
        if (!anod && theta > VORTEX_MIN_EMIT) {   // 1.1.0: in Anod the mod-wheel dimension throws the Chirikov map instead (MEDIUM §4)
            sumi_deform_t d;
            d.type = SUMI_DEFORM_VORTEX;
            d.as.vortex.x = vm->ctl_s[SUMI_CTL_VORTEX_X];
            // v0.9 #69: Y reversed — CC up moves the centre UP on screen
            // (texture y grows downward; a raised hand should raise the stir).
            d.as.vortex.y = 1.0f - vm->ctl_s[SUMI_CTL_VORTEX_Y];
            d.as.vortex.strength = theta;
            d.as.vortex.radius = VORTEX_RADIUS;
            // v0.4: the CC-routed vortex takes its profile from params
            // (EXPONENTIAL default — diffuse, breath-like; RANKINE for twist
            // gestures and rotary deltas, §4.3(3)).
            d.as.vortex.profile = params ? params->vortex_profile : 0u;
            sumi_voice_mapper_torsion_kphi(vm, d.as.vortex.profile, &d.as.vortex.k, &d.as.vortex.phase);   // v0.10
            budget_push(vm, queue, &d);
        }
        // v0.9 #69: the right hand's Lamb-Oseen stir — the same dt-scaled
        // agitation with the swirl pass (rigid core, 1/r² far field), its own
        // centre (Y reversed like the vortex's).
        const float omega = vm->ctl_s[SUMI_CTL_SWIRL_STRENGTH] * SWIRL_CTL_RATE * fdt * damping;
        if (omega > SWIRL_MIN_EMIT) {
            sumi_deform_t d;
            d.type = SUMI_DEFORM_SWIRL;
            d.as.swirl.x = vm->ctl_s[SUMI_CTL_SWIRL_X];
            d.as.swirl.y = 1.0f - vm->ctl_s[SUMI_CTL_SWIRL_Y];
            // S = theta_core · 2π·r_c² (the swirl pass's core-rotation norm).
            d.as.swirl.strength = omega * 6.2831853f * SWIRL_CTL_CORE_R * SWIRL_CTL_CORE_R;
            d.as.swirl.core_r = SWIRL_CTL_CORE_R;
            budget_push(vm, queue, &d);
        }
        // v0.9 #69: Grasp = pinch, delta-driven like the CC 74 route (each
        // change emits ±k toward the new value; a squeeze-and-release nets
        // out in exact math, and what the release does not retrace bakes in
        // as marbling). The saddle folds at the LEFT hand's centre, the
        // crossed tines at the RIGHT hand's — each grasp works where its own
        // hand steers.
        for (int pi = 0; pi < 2; pi++) {
            const uint32_t dim = pi == 0 ? SUMI_CTL_PINCH_SADDLE : SUMI_CTL_PINCH_CROSS;
            const float dk = (vm->ctl_s[dim] - vm->pinch_ctl_baked[pi]) * PINCH_K_SCALE;
            const float adk = dk >= 0.0f ? dk : -dk;
            if (adk < PINCH_MIN_K) continue;
            if (pi == 0) {
                sumi_deform_t d;
                d.type = SUMI_DEFORM_PINCH;
                d.as.pinch.x = vm->ctl_s[SUMI_CTL_VORTEX_X];
                d.as.pinch.y = 1.0f - vm->ctl_s[SUMI_CTL_VORTEX_Y];
                d.as.pinch.k = dk;
                d.as.pinch.angle = 0.0f;   // horizontal fold axis
                d.as.pinch.window_s = PINCH_WINDOW_S;
                if (!budget_push(vm, queue, &d)) continue;
            } else {
                if (!budget_reserve(vm, 2)) continue;
                sumi_deform_t t[2];
                sumi_deform_crossed_pinch(vm->ctl_s[SUMI_CTL_SWIRL_X],
                                          1.0f - vm->ctl_s[SUMI_CTL_SWIRL_Y],
                                          1.0f, 0.0f, dk, t);
                budget_push(vm, queue, &t[0]);   // reserved: cannot fail
                budget_push(vm, queue, &t[1]);
            }
            vm->pinch_ctl_baked[pi] = vm->ctl_s[dim];
        }
    }
    // v0.4 sine ripple, BAKE insertion point (§4.3(6)): delta-driven like the
    // pinch — each pass applies ΔA toward the ctl target at the CURRENT k and
    // angle. At fixed (k, φ, angle) the passes compose additively (an LFO on A
    // returning to zero nets out in exact math); changing k or angle while
    // baked amplitude stands bakes residue in — that residue IS marbling (the
    // waved-comb feathering), deliberate and documented. Live mode (ripple_
    // bake = 0) emits nothing here: the engine routes the same ctl values to
    // the composite's view displacement instead.
    if (params && params->ripple_bake == 1) {
        const float target = vm->ctl_s[SUMI_CTL_RIPPLE_AMP] * SUMI_RIPPLE_AMP_MAX;
        const float dA = target - vm->ripple_baked;
        const float adA = dA >= 0.0f ? dA : -dA;
        if (adA > 0.0002f) {
            sumi_deform_t d;
            d.type = SUMI_DEFORM_RIPPLE;
            d.as.ripple.amp = dA;
            d.as.ripple.k = SUMI_RIPPLE_K_MIN +
                vm->ctl_s[SUMI_CTL_RIPPLE_FREQ] * (SUMI_RIPPLE_K_MAX - SUMI_RIPPLE_K_MIN);
            d.as.ripple.phase = vm->ripple_phase;
            d.as.ripple.angle = params->ripple_angle;
            if (budget_push(vm, queue, &d)) {
                vm->ripple_baked = target;
                // #36 (permanence, user request): under BEND-driven bake the
                // phase drifts a little per pass, so an excursion never
                // retraces exactly — each vibrato cycle lays a slightly
                // shifted comb and its feathered residue bakes in, the way a
                // glide leaves its tines (§4.3(6): changing φ between passes
                // IS the marbling). The dynamic still stills (#35: the amp
                // goes home); the mark stays. CC-driven bake (bend_mode 0)
                // keeps φ fixed — the pure composing-back group property.
                if (params->bend_mode == 1) {
                    vm->ripple_phase += adA / SUMI_RIPPLE_AMP_MAX * 1.5f;
                    if (vm->ripple_phase > 6.2831853f) vm->ripple_phase -= 6.2831853f;
                }
            }
        }
    }

    // Budget diagnostics (throttled): visible with a log callback attached.
    if (vm->frames % 300 == 0 && vm->merged_total != vm->merged_last_log) {
        if (vm->log_cb) {
            char buf[96];
            snprintf(buf, sizeof(buf), "mapper: deform budget merged %u emissions so far",
                     vm->merged_total);
            vm->log_cb(SUMI_LOG_INFO, buf, vm->log_user);
        }
        vm->merged_last_log = vm->merged_total;
    }
}

} // extern "C"
