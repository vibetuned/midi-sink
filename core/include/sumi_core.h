#ifndef SUMI_CORE_H
#define SUMI_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Three-state export macro: shared-build export, shared-consume import, static no-op. */
#if defined(_WIN32)
  #if defined(SUMI_BUILD_SHARED)
    #define SUMI_API __declspec(dllexport)
  #elif defined(SUMI_USE_SHARED)
    #define SUMI_API __declspec(dllimport)
  #else
    #define SUMI_API
  #endif
#else
  #define SUMI_API __attribute__((visibility("default")))
#endif

typedef struct sumi_instance_t sumi_instance_t;

typedef enum {
    SUMI_BACKEND_AUTO  = 0,
    SUMI_BACKEND_METAL = 1,   /* native_surface_handle = CAMetalLayer*        */
    SUMI_BACKEND_D3D11 = 2,   /* native_surface_handle = HWND                 */
    SUMI_BACKEND_GL    = 3,   /* host-owned context; handle must be NULL      */
    SUMI_BACKEND_WEBGPU = 4   /* native_surface_handle = sumi_webgpu_surface_t* (below) */
} sumi_backend_t;

/* WebGPU host contract (Phase 5 §5, DECISIONS_4 #15). In a browser the
   adapter and device can only be created asynchronously, so the HOST (the JS
   page) creates them and hands the core the device — imported into the wasm
   as an emdawnwebgpu handle — plus the canvas to draw into and the canvas's
   preferred format (navigator.gpu.getPreferredCanvasFormat()). The core then
   owns the surface: it creates and configures it from the selector, acquires
   the frame texture, resizes it, and runs its readbacks (copy + async map)
   on that device. Pointed to by sumi_config_t.native_surface_handle for
   SUMI_BACKEND_WEBGPU; must outlive the instance. */
enum { SUMI_WEBGPU_FORMAT_BGRA8 = 0, SUMI_WEBGPU_FORMAT_RGBA8 = 1 };
typedef struct {
    const void* device;            /* WGPUDevice handle                       */
    const char* canvas_selector;   /* CSS selector of the <canvas>, e.g. "#sumi" */
    uint32_t    color_format;      /* SUMI_WEBGPU_FORMAT_*                    */
} sumi_webgpu_surface_t;

typedef enum {
    SUMI_INPUT_AUTO    = 0,
    SUMI_INPUT_MPE     = 1,
    SUMI_INPUT_CLASSIC = 2,
    SUMI_INPUT_WIND    = 3
} sumi_input_mode_t;

typedef enum {                 /* global control dimensions for CC routing */
    SUMI_CTL_VORTEX_STRENGTH = 0,
    SUMI_CTL_VORTEX_X        = 1,
    SUMI_CTL_VORTEX_Y        = 2,   /* v0.9: REVERSED at consumption — CC up
                                       moves the centre UP on screen (texture
                                       y is down; a raised hand should raise
                                       the stir, DECISIONS_4 #69)            */
    SUMI_CTL_VISCOSITY       = 3,
    SUMI_CTL_PAPER_ROUGHNESS = 4,
    SUMI_CTL_PALETTE_MORPH   = 5,
    SUMI_CTL_INK_FLOW        = 6,   /* breath aliases here in wind mode */
    SUMI_CTL_RIPPLE_AMP      = 7,   /* v0.4: sine ripple amplitude A          */
    SUMI_CTL_RIPPLE_FREQ     = 8,   /* v0.4: ripple wavenumber k              */
    /* v0.9 (DECISIONS_4 #69): the right hand's water — a Lamb-Oseen stir
       with its own centre (same reversed-Y convention), and two delta-driven
       pinches (0..1 value; each CHANGE emits +/-k toward the new value, like
       the CC 74 pinch — returning to rest nets out in exact math). */
    SUMI_CTL_SWIRL_STRENGTH  = 9,   /* Lamb-Oseen core rotation rate          */
    SUMI_CTL_SWIRL_X         = 10,
    SUMI_CTL_SWIRL_Y         = 11,  /* reversed like VORTEX_Y                 */
    SUMI_CTL_PINCH_SADDLE    = 12,  /* Hamiltonian saddle at the vortex centre */
    SUMI_CTL_PINCH_CROSS     = 13,  /* crossed tines at the swirl centre       */
    /* v0.10 (Phase 6 step 36, MEDIUM §2.1): the wave torsion's flavour
       controls, read by every vortex route whose profile is TORSION. */
    SUMI_CTL_TORSION_K       = 14,  /* wavenumber k: 0..1 -> 2π·4 .. 2π·40 per
                                       canvas height (rests at 0.5)           */
    SUMI_CTL_TORSION_PHASE   = 15,  /* φ: 0..1 -> 0..2π (rests at 0)           */
    /* v0.11 (Phase 6 step 37, MEDIUM §2.2): the Chladni cellular flow —
       A the stirring rate (0..1 of SUMI_CHLADNI_RATE, the cells' rotation in
       rad/s), B the balance between the two diagonal waves the flow splits
       into (0 = the full cellular flow, 0.5 = a single diagonal wave, 1 =
       the cells reversed). */
    SUMI_CTL_CHLADNI_A       = 16,
    SUMI_CTL_CHLADNI_B       = 17,
    /* v0.13 (Phase 6 step 39, MEDIUM §2.4): the spark shear's base
       wavenumber k: 0..1 -> 2π·2 .. 2π·24 per canvas height (the octaves
       stack above it) — a thick channel to fine streamers; rests at 0.5.
       Unmapped in the core; CC 74 lands here under slide_mode 2 (prepared
       for the Anod binding table, step 42). */
    SUMI_CTL_SPARK_K         = 18,
    /* v0.14 (Phase 6 step 40, MEDIUM §2.5): the Chirikov standard map's
       THROW — DELTA-driven: a change of δ in this control applies one step
       of the map with the kick and the drift both scaled by δ, so the
       step's chaos parameter is δ²·params.chirikov_kmax, capped per step
       by the core's erosion ceiling; a negative δ applies the exact
       inverse step. Unmapped in the core; the mod wheel / breath under the
       Anod binding table (step 42). */
    SUMI_CTL_CHIRIKOV_K      = 19,
    SUMI_CTL_COUNT           = 20
} sumi_ctl_t;

typedef enum {                       /* v0.4 vortex profiles, spec §4.3(3) */
    SUMI_VORTEX_EXPONENTIAL = 0,     /* Jaffer: diffuse, breath-like       */
    SUMI_VORTEX_RANKINE     = 1,     /* rigid core, crease ring at R       */
    SUMI_VORTEX_LAMB_OSEEN  = 2,     /* v0.6: the §4.3(7) swirl as a GESTURE —
                                        strength = Γ·Δt (signed), radius = r_c.
                                        The Marble-mode "pull back to stir"
                                        (DECISIONS_4 #30/#49); CC routing keeps
                                        profiles 0/1.                        */
    SUMI_VORTEX_TORSION     = 3      /* v0.10 (Phase 6, MEDIUM §2.1): WAVE
                                        TORSION — θ' = θ + A·sin(k·r − φ)·e^(−r/R),
                                        r' = r. A rotation by θ(r) like its
                                        siblings: EXACT at any amplitude
                                        (det J = 1; the ±A pair inverts
                                        analytically). strength = A (radians),
                                        radius = R (the e-fold decay length);
                                        k and φ come from SUMI_CTL_TORSION_K /
                                        _PHASE. Reachable through every vortex
                                        route: gesture, CC-routed, and the
                                        note-on sweep (torsion_sweep).        */
} sumi_vortex_profile_t;

typedef enum {                       /* v0.6: sumi_add_drop layer types      */
    SUMI_DROP_INK   = 0,             /* new ink band (counter-derived phase) */
    SUMI_DROP_CLEAR = 1,             /* clear water / surfactant: expands the
                                        field, interior un-inked            */
    SUMI_DROP_FEED  = 2,             /* GROW the ink already under the centre:
                                        the §3.4/§4.4 boundary growth as a
                                        gesture — the interior takes the
                                        centre texel's band, so a held press
                                        widens a band instead of laying rings.
                                        Radius = sqrt((R+ΔR)² − R²), the host
                                        tracks R (DECISIONS_4 #49).          */
    SUMI_DROP_NONE  = 3              /* v0.13: NO drop — sumi_add_drop returns,
                                        sumi_add_spark fires the burst and the
                                        shear episodes without the blast (the
                                        conservation gate's stream: a drop's
                                        exact expansion moves ink off the
                                        canvas, which no mass observable can
                                        tell from loss — DECISIONS_5 #39).   */
} sumi_drop_layer_t;

typedef void (*sumi_log_fn)(int level, const char* msg, void* user);

typedef struct {
    void*          native_surface_handle;
    sumi_backend_t backend;
    uint32_t       width;
    uint32_t       height;
    float          pixel_ratio;
    sumi_log_fn    log_cb;        /* optional, may be NULL */
    void*          log_user;
} sumi_config_t;

typedef enum {                   /* pitch -> position layouts, see spec 3.4 */
    SUMI_LAYOUT_FIFTHS      = 0, /* circle-of-fifths radial (default)         */
    SUMI_LAYOUT_CHROMA_GRID = 1, /* C1 top-left ... B7 bottom-right           */
    SUMI_LAYOUT_JANKO       = 2, /* staggered whole-tone Janko grid           */
    SUMI_LAYOUT_ROLL_H      = 3, /* horizontal piano roll, BPM-driven scroll  */
    SUMI_LAYOUT_ROLL_V      = 4, /* vertical piano roll, BPM-driven scroll    */
    SUMI_LAYOUT_PIANO_GRID  = 5, /* classical two-row piano grid, C1..B7      */
    SUMI_LAYOUT_ROLL_H_RIGHT = 6,/* v0.8: horizontal roll, now-line at the RIGHT,
                                    the sheet drifts left (DECISIONS_4 #64)   */
    SUMI_LAYOUT_ROLL_V_BOTTOM = 7,/* v0.8: vertical roll, now-line at the BOTTOM,
                                    the sheet rises                           */
    /* 1.0.0 (Phase 6 step 41): NAMED AND RESERVED for Phase 8's instrument
       layouts (INSTRUMENT_SPEC §2–§4). sumi_set_params clamps them to FIFTHS
       with a warning until each ships; the probe refuses them. */
    SUMI_LAYOUT_TRUMPET     = 8,  /* three valves + the harmonic series (stateful) */
    SUMI_LAYOUT_TROMBONE    = 9,  /* the slide (stateful, continuous)              */
    SUMI_LAYOUT_WICKI       = 10, /* Wicki–Hayden hexagonal isomorphic grid        */
    SUMI_LAYOUT_FRETS       = 11, /* a fretboard: strings × frets                  */
    SUMI_LAYOUT_THEREMIN    = 12  /* continuous pitch: the cell's CONTINUOUS flag  */
} sumi_layout_t;

/* 1.0.0 (Phase 6 step 41, MEDIUM §1): the MEDIUM — what the engine's field
   is READ as. The engine owns the field, the operators, the normalizer, the
   layouts and the budgets; a medium owns the composite, its palette family,
   its print styling and its default binding table. */
typedef enum {
    SUMI_MEDIUM_SUMI = 0,   /* suminagashi: ink on washi — the renderer of 0.x   */
    SUMI_MEDIUM_ANOD = 1    /* the electric medium (DECISIONS_5 #1): inert until
                               its composite lands (step 43) — renders as SUMI  */
} sumi_medium_t;

typedef struct {
    float    fluid_viscosity;    /* damping for continuous agitation            */
    float    expansion_rate;     /* pressure/breath-driven drop feed scale      */
    float    paper_roughness;    /* washi fiber composite strength              */
    float    smoothing_ms;       /* expressive-dimension smoothing time const   */
    uint32_t active_palette_id;  /* 0 sumi black, 1 indigo, 2 ochre, 3 = the
                                    custom palette of sumi_set_palette
                                    (SUMI_PALETTE_CUSTOM, 1.0.0)               */
    uint32_t pitch_layout;       /* sumi_layout_t value                         */
    float    sim_scale;          /* simulation res / output res, clamped (0,2].
                                    Host-chosen: 1.0 desktop/iPad-class GPUs,
                                    ~0.75 on typical Android phones for
                                    sustained thermals under continuous MPE
                                    streams. The core never detects devices —
                                    the host owns this default.               */
    float    bpm;                /* host-supplied tempo, roll layouts (dflt 120)*/
    float    roll_speed;         /* canvas-lengths per beat, rolls (dflt 0.0625:
                                    16 beats = 4 bars of 4/4 span the canvas)  */
    /* v0.4 */
    uint32_t slide_mode;         /* CC74 routing: 0 per-drop aux (v1 behavior),
                                    1 Hamiltonian pinch (delta-driven), 2 the
                                    spark shear's wavenumber (the latest
                                    voice's slide sets SUMI_CTL_SPARK_K);
                                    SUMI_MODE_MEDIUM_DEFAULT = the medium's
                                    table (1.1.0, MEDIUM §4: Sumi 0, Anod 2)  */
    uint32_t vortex_profile;     /* sumi_vortex_profile_t for CC-routed vortex */
    uint32_t ripple_bake;        /* 0 live (composite view), 1 bake (deform)   */
    float    ripple_angle;       /* ripple frame rotation, radians (dflt 0)    */
    uint32_t pinch_variant;      /* 0 Hamiltonian saddle (det = 1 exactly),
                                    1 crossed tines (the softer, lumpier look
                                    kept after the step-19 pick-by-eye pair —
                                    DECISIONS_3 #34). Honored by BOTH pinch
                                    routes: slide_mode = 1 and sumi_add_pinch. */
    uint32_t bend_mode;          /* PER-NOTE pitch-bend routing (§4.3(6),
                                    DECISIONS_3 #35 corrected): 0 = v1 glide
                                    (the bend drags the note's drop along the
                                    pitch axis), 1 = the note bend plays the
                                    sine ripple's amplitude and the drop
                                    holds position; 1.1.0: 2 = the bend plays
                                    the torsion's wavenumber (SUMI_CTL_
                                    TORSION_K, ±1.5 semitones = the range),
                                    3 = the spark's (SUMI_CTL_SPARK_K);
                                    SUMI_MODE_MEDIUM_DEFAULT = the medium's
                                    table (Sumi 0, Anod 2). Exactly ONE
                                    consumer owns the note bend; switchable
                                    live. Master bend keeps its shear tine. */
    uint32_t press_mode;         /* 0xD0 channel-pressure routing (§3.4 v0.4):
                                    0 = ink feed (v1 grow), 1 = the Lamb-Oseen
                                    swirl — hardware's door to the swirl
                                    voice; 1.1.0: 2 = the torsion sweep FEED
                                    (pressure spends torsion deltas around
                                    the note's drop, the sweep's emitter fed
                                    live); SUMI_MODE_MEDIUM_DEFAULT = the
                                    medium's table (Sumi 0, Anod 2). 0xA0 poly
                                    pressure -> the swirl dimension in every
                                    mode (the medium decides its consumer). */
    /* v0.7 (DECISIONS_4 #53) */
    uint32_t wake_profile;       /* sumi_add_wake's fluid: 0 = the inviscid
                                    potential doublet with a rigid tip (v0.4,
                                    exact, zero seam); 1 = the VISCOUS stroke —
                                    the 2-D unsteady Stokeslet displacement of
                                    an impulse spread over the tip radius,
                                    sub-stepped at <= a/4 like the doublet.    */
    float    wake_spread;        /* viscous profile only: l/a, the momentum's
                                    diffusion length after the impulse over the
                                    tip radius (l^2 = a^2 + 4 nu t). Clamped
                                    [1.5, 12]; default 3. Small = sharp, close
                                    to the tip; large = soft and far-reaching. */
    /* v0.10 (Phase 6 step 36, MEDIUM §2.1) */
    uint32_t torsion_sweep;      /* 0 = off (default). 1 = every note-on also
                                    fires the wave torsion's OUTWARD PHASE
                                    SWEEP at the voice's drop: per-frame
                                    torsion DELTAS (never absolutes) with φ
                                    advancing and a decaying amplitude, k
                                    from the TORSION_K control — the engine's
                                    first time-driven "episode". A stand-in
                                    until the medium's binding tables own the
                                    strike (step 42).                        */
    /* v0.11 (Phase 6 step 37, MEDIUM §2.2): the Chladni cellular flow —
       ψ = Ψ·cos(k_x(x−x0))·cos(k_y(y−y0)), the Taylor–Green vortex, an exact
       Navier–Stokes solution, on THE LAYOUT'S cell lattice: an eddy in every
       cell (adjacent cells counter-rotate), the cell boundaries its
       separatrices. Steadily driven it spins each note's drop in place and
       stretches the ink along the boundaries into the figure that outlines
       the grid. Bake only, into the field; nothing is live. */
    float    chladni_cell;       /* cell size, 0.5..1.5 (dflt 1): the eddy's disc
                                    as a fraction of the layout's display cell —
                                    the circle the shells draw for the key. 1
                                    fills the key, 0.5 leaves a ring of resting
                                    water round each eddy. Above 1 the discs
                                    would overlap: SUMI_CHLADNI_DISCS caps the
                                    size at 1 (exact), SUMI_CHLADNI_FIELD lets
                                    them grow to 1.5 and stir the water between
                                    the keys. Layouts without drawn cells (the
                                    fifths, the rolls) take the largest circle
                                    at each note that touches no neighbour's. */
    /* v0.12 (Phase 6 step 38, MEDIUM §2.3): the viscous multipole burst's
       AGE ENVELOPE (sumi_add_burst). The strike fires sharp at ℓ = a and the
       release grows the diffusion age ℓ (ℓ² = a² + 4νt): the lobes soften
       and reach further as the discharge dies. */
    float    burst_age;          /* the final age ℓ_end/a, 1.5..12 (dflt 4):
                                    1.5 leaves the lobes sharp and close to
                                    the core, 12 soft and far; D is the
                                    displacement over the burst's own age,
                                    so the age shapes the burst, it does not
                                    scale it.                                */
    float    burst_life;         /* seconds for the age to grow from a to
                                    burst_age·a, 0..4 (dflt 0.8); 0 = the whole
                                    burst at once, in the next update. ℓ²
                                    grows linearly in time, so most of the
                                    displacement lands early: the discharge
                                    blooms sharp and dies soft.              */
    uint32_t burst_order;        /* the multipole order m a strike takes when
                                    the gesture passes m = 0, 2..8 (dflt 2):
                                    2 the quadrupole — two lobes eject along
                                    the axis, two draw in across it — and m
                                    lobes eject for order m. 8 is the top the
                                    pass budget table covers (DECISIONS_5
                                    #29). The pitch-class → m table of the
                                    binding tables (step 42) will override
                                    it per note.                             */
    /* v0.13 (Phase 6 step 39, MEDIUM §2.4): the spark shear — the jagged
       streamers of the composed strike (sumi_add_spark). */
    uint32_t spark_stack;        /* octaves in the profile, 1..4 (dflt 3: k,
                                    2k, 4k with weights 1, ½, ¼) — the stack
                                    depth the spec asks to keep out of a magic
                                    constant.                                */
    uint32_t spark_profile;      /* 0 triangle waves (dflt), 1 piecewise-linear
                                    hash noise: either is exact (a shear
                                    inverts for any profile).               */
    float    spark_shear;        /* the episode's total kick A = B as a
                                    multiple of the strike radius, 0..2 (dflt
                                    0.6); 0 = no shear in the composition.    */
    float    spark_tau;          /* the decay time constant, seconds, 0.05..2
                                    (dflt 0.25): A, B ∝ e^(−t/τ); the episode
                                    is over after 4τ.                        */
    /* v0.14 (Phase 6 step 40, MEDIUM §2.5): the Chirikov standard map,
       y1 = y + A·sin(k(x−xc)+φ), x1 = x + ε·(y1−yc), K = A·k·ε. */
    float    chirikov_kmax;      /* K of a FULL throw of the CHIRIKOV_K control
                                    in one frame, 0..2 (dflt 1): Greene's
                                    threshold 0.9716 is where sheets give way
                                    to chaos; 0 disables the route. A throw
                                    eased over m frames is m steps at K/m²
                                    (the pendulum flow), a wheel THROWN is one
                                    hard kick — the depth into chaos is the
                                    wheel's speed. The per-step K is capped
                                    by the core's erosion ceiling.          */
    uint32_t chirikov_periods;   /* the kick's waves per canvas height along
                                    x, 1..8 (dflt 2): k = 2π·periods.         */
    float    chirikov_eps;       /* the drift's scale ε, 0.05..1 (dflt 0.5):
                                    the kick amplitude follows as A =
                                    K/(k·ε) — small ε means steep kicks,
                                    large ε a canvas-scale drift.            */
    /* 1.0.0 (Phase 6 step 41) — THE ONE ABI BREAK of the 2.0 arc. */
    uint32_t medium;             /* sumi_medium_t (dflt SUMI_MEDIUM_SUMI). Live-
                                    switchable: the field is medium-agnostic
                                    (coordinates, phase, aux), so switching
                                    re-reads the same deformation history.
                                    Values above ANOD clamp to SUMI. 1.1.0
                                    (step 42): the medium also owns the
                                    DEFAULT BINDING TABLE (MEDIUM §4) — what
                                    each MIDI dimension drives when a mode is
                                    SUMI_MODE_MEDIUM_DEFAULT, and, with no
                                    override at all, the strike (Sumi: the
                                    drop; Anod: the spark composition), the
                                    poly-pressure dimension (Lamb–Oseen
                                    swirl / Chladni stir) and the mod-wheel
                                    dimension (vortex / Chirikov throw). The
                                    CC map and explicit modes override.     */
    /* 1.1.0 (Phase 6 step 42, MEDIUM §3–§4): the Anod medium's knobs. */
    float    anod_glow;          /* the strain-glow scale, 0.2..5 (dflt 1): the
                                    glow is 1 − exp(−σ/anod_glow) with σ =
                                    |λ − 1/λ| the accumulated strain read
                                    from the field (‖J‖_F² − 2); smaller = a
                                    hotter, sooner glow.                     */
    uint32_t burst_order_by_class[12]; /* the Anod strike's multipole order per
                                    pitch class C..B, 2..8 (0 = burst_order);
                                    the table the author signs by eye (dflt:
                                    naturals 2, accidentals 3).              */
    float    anod_pitch;         /* the water grid's pitch at rest as a
                                    fraction of the canvas height, 1/256..1/8
                                    or 0 for no grid (dflt 1/144): Anod water
                                    draws the deformed grid — iso-lines of
                                    position + gain·displacement, one family
                                    per axis — where the displacement exceeds
                                    a texel; a larger pitch is fewer lines.   */
    /* 1.1.0 (Phase 6 step 43, MEDIUM §2.2): how the Chladni stir turns the
       layout's cells — an eddy in every display cell, the ring of each disc
       turning while its core and the water between the discs rest. */
    uint32_t chladni_mode;       /* SUMI_CHLADNI_DISCS (0, dflt): an exact
                                    rotation inside every disc, the discs kept
                                    disjoint (chladni_cell capped at 1).
                                    SUMI_CHLADNI_FIELD (1), the author's
                                    "inverse Chladni": the discs' rings summed
                                    into ONE divergence-free displacement field,
                                    so the discs may grow past their keys
                                    (chladni_cell to 1.5) and overlap, the
                                    water between the keys stirred by both
                                    neighbours; area-preserving to first order
                                    (the burst's class, sub-stepped).         */
} sumi_params_t;
#define SUMI_CHLADNI_DISCS 0u
#define SUMI_CHLADNI_FIELD 1u
/* 1.1.0: a mode set to this value takes the MEDIUM's default (MEDIUM §4). */
#define SUMI_MODE_MEDIUM_DEFAULT 255u

/* ---------------------------------------------------------------------------
   libsumi 1.0.0 — THE ONE ABI BREAK OF THE 2.0 ARC (Phase 6 step 41,
   DECISIONS_5 #45). Migrating a 0.x host, mechanically:
     1. sumi_layout_probe gained `const sumi_layout_state_t* state` after
        `aspect`: pass NULL (or zeros) for every layout that exists today.
     2. sumi_cell_info_t gained `flags` at its end (bit 0 = continuous, the
        theremin's; 0 today): read it or ignore it.
     3. sumi_params_t gained `medium` at its end (0 = SUMI_MEDIUM_SUMI, the
        renderer you know; 1 = SUMI_MEDIUM_ANOD, inert until its composite
        lands): zero-initialise params, or set 0.
     4. sumi_set_palette and SUMI_PALETTE_CUSTOM (active_palette_id 3): new
        and optional — the built-in palettes render as before.
     5. sumi_layout_t 8..12 are named and RESERVED for Phase 8;
        sumi_set_params clamps them to FIFTHS with a warning until then.
   Nothing else moved: medium 0 renders bitwise as 0.14.0 — the field gate
   proves the field, the composite gate (tests/fixtures/composite_512_
   metal.rgba) the pixels. From here on, additive growth only.
   --------------------------------------------------------------------------- */

/* Version & diagnostics */
SUMI_API uint32_t sumi_version(void);                       /* (maj<<16)|(min<<8)|patch */
SUMI_API uint32_t sumi_dropped_midi_count(sumi_instance_t*);/* queue overflow counter   */

/* Lifecycle — render thread only. sumi_create returns NULL on failure (see log_cb). */
SUMI_API sumi_instance_t* sumi_create (const sumi_config_t* config);
SUMI_API void             sumi_destroy(sumi_instance_t* inst);
SUMI_API void             sumi_resize (sumi_instance_t* inst, uint32_t w, uint32_t h, float pixel_ratio);

/* Frame loop — render thread only. */
SUMI_API void             sumi_update (sumi_instance_t* inst, double delta_time);
SUMI_API void             sumi_render (sumi_instance_t* inst);

/* MIDI ingest — exactly one producer thread, SPSC, wait-free. */
SUMI_API void             sumi_push_midi(sumi_instance_t* inst, uint8_t status, uint8_t data1, uint8_t data2);

/* Configuration — render thread only. */
SUMI_API void             sumi_set_params    (sumi_instance_t* inst, const sumi_params_t* params);
SUMI_API void             sumi_get_params    (sumi_instance_t* inst, sumi_params_t* out);
SUMI_API void             sumi_set_input_mode(sumi_instance_t* inst, sumi_input_mode_t mode);
SUMI_API void             sumi_map_cc        (sumi_instance_t* inst, uint8_t channel /*0xFF=any*/,
                                              uint8_t cc, sumi_ctl_t target);       /* Airwave routing */
SUMI_API void             sumi_clear_cc_map  (sumi_instance_t* inst);

/* 1.0.0 (Phase 6 step 41, QOL §1): a USER PALETTE — an N-stop gradient along
   the ink-depth axis plus the depth curve, a POD the shells own the editor
   and the persistence of. The composite applies it exactly like the
   built-ins (the same washi, the same soak: the medium keeps its rendering
   character, the user chooses the hues — the identity guardrail); the
   built-in palettes 0..2 are untouched and render bitwise as before. The
   Anod medium will read the same data its own way (a glow, not an ink).
   Selected by active_palette_id = SUMI_PALETTE_CUSTOM; until a palette is
   set, a sumi-like default stands in. Validated on the way in: stops
   clamped to 2..8 and to ascending positions in 0..1, RGB to 0..1, the curve
   to its ranges. */
#define SUMI_PALETTE_MAX_STOPS 8
#define SUMI_PALETTE_CUSTOM    3u
typedef struct {
    float    rgb[3];        /* LINEAR RGB, 0..1                                   */
    float    position;      /* 0..1 along the ink-depth axis (0 thin, 1 pooled)   */
} sumi_palette_stop_t;
typedef struct {
    uint32_t stop_count;    /* 2..8                                               */
    sumi_palette_stop_t stops[SUMI_PALETTE_MAX_STOPS];   /* ascending position   */
    float    depth_gamma;   /* the ink-depth curve: u = floor + (1−floor)·depth^γ, 0.25..4 (1 = linear) */
    float    depth_floor;   /* the thinnest visible ink's position, 0..1           */
    float    hue_drift;     /* per-drop variation: the aux selector shifts the
                               sampled position by ±drift/2, 0..1                  */
    float    clear_rgb[3];  /* the clear-water band tone, LINEAR RGB                */
    uint32_t reserved[4];
} sumi_palette_t;
SUMI_API void             sumi_set_palette  (sumi_instance_t* inst, const sumi_palette_t* palette);

/* Paper dip: freeze canvas, snapshot, reset UV to identity (rebases the drop
   counter, see spec 4.2). The print pipeline is double-buffered: the core keeps
   two print buffers and flips. v0.6 (DECISIONS_4 #51): when both hold an UNREAD
   print the older one is recycled (sumi_read_print copies synchronously, so no
   host ever holds a core buffer) — a dip is refused, with a warning log, only
   while a readback is still in flight (a few frames after the previous dip). */
SUMI_API void             sumi_trigger_paper_dip(sumi_instance_t* inst);
/* Synchronous readback of the last dipped print (RGBA8, tightly packed).
   Call with pixels=NULL to query size. Returns false if no print exists. */
SUMI_API bool             sumi_read_print(sumi_instance_t* inst, uint8_t* pixels, size_t capacity,
                                          uint32_t* out_w, uint32_t* out_h);

/* Layout geometry probe (v0.3, Phase 4) — pure read-only query for host-side
   play surfaces (hit-testing, bend scaling). See PROJECT_SPEC.md §8.2.

   Units: cell_center_* are normalized [0,1] canvas coordinates.
   cell_radius and semitone_step are DISTANCES in canvas-height units (the
   project's universal distance unit — same as gesture radii below);
   semitone_dx/dy is a unit vector in aspect-corrected space. A host measures
   touch deltas in the same metric by dividing pixel deltas by the view
   height; to convert a step along the axis back to normalized coordinates:
   dx_norm = step*semitone_dx/aspect, dy_norm = step*semitone_dy. */
typedef struct {
    uint8_t  note;            /* nominal MIDI note of the cell under (x, y)   */
    float    cell_center_x;   /* normalized canvas coords of the cell center  */
    float    cell_center_y;
    float    cell_radius;     /* half the smaller cell dimension (R_max),
                                 canvas-height units                          */
    float    semitone_dx;     /* unit vector: +1 semitone direction (glide    */
    float    semitone_dy;     /*   axis, DECISIONS_2 #7), aspect-corrected    */
    float    semitone_step;   /* distance of +1 semitone along it,
                                 canvas-height units (true lattice step,
                                 NOT the glide-rendering cap)                 */
    uint32_t flags;           /* 1.0.0: SUMI_CELL_* bits; 0 for every layout
                                 that exists today (the theremin's continuous
                                 pitch is the first bit, Phase 8)             */
} sumi_cell_info_t;
#define SUMI_CELL_CONTINUOUS 1u   /* the cell has no discrete note: pitch is continuous across it */

/* 1.0.0 (Phase 6 step 41, INSTRUMENT §1): the LAYOUT STATE a stateful layout
   (valves, a slide) needs to answer the probe — an explicit snapshot the
   shell keeps beside its params snapshot, so the probe stays a pure function
   callable from any thread. State changes travel as MIDI (Phase 8). Zeros or
   NULL = stateless, which every layout shipping today is. */
typedef struct {
    uint32_t buttons;      /* bitmask: valves, register keys (bit 0 = valve 1 …) */
    float    slider;       /* continuous control position 0..1 (trombone slide)  */
    uint32_t reserved[2];
} sumi_layout_state_t;

/* Pure, instance-free geometry query — a free function of the same inputs the
   internal layouts already consume. Callable from ANY thread (the caller
   supplies a params snapshot); no instance, no rendering, no MIDI, no state.
   This matters on Android, where touches arrive on the UI thread while the
   render thread owns the instance — an instance-bound probe would force a
   command-queue round-trip per touch-down, spending the play surface's
   latency budget on hit-testing. Returns false when Play mode is meaningless
   for the layout (FIFTHS, rolls) or (x, y) is outside the playable area. */
SUMI_API bool             sumi_layout_probe(uint32_t layout /* sumi_layout_t */,
                                            const sumi_params_t* params, float aspect,
                                            const sumi_layout_state_t* state,   /* 1.0.0: NULL = stateless */
                                            float norm_x, float norm_y,
                                            sumi_cell_info_t* out);

/* Manual touch / mouse gestures — render thread only, normalized [0,1] coords.
   layer_type: sumi_drop_layer_t (0 ink, 1 clear, 2 feed — v0.6; 3 none: no pass, v0.13). */
SUMI_API void             sumi_add_drop  (sumi_instance_t* inst, float x, float y, float radius, uint32_t layer_type);
SUMI_API void             sumi_add_tine  (sumi_instance_t* inst, float x0, float y0, float x1, float y1,
                                          float alpha /*sharpness*/, float magnitude);
SUMI_API void             sumi_add_vortex(sumi_instance_t* inst, float x, float y, float strength, float radius,
                                          uint32_t profile /* sumi_vortex_profile_t (v0.4) */);
/* v0.4: dipolar wake — the stylus stroke's fluid signature (spec §4.3(4)).
   v0.7: params.wake_profile selects the fluid — 0 the inviscid doublet below,
   1 the viscous 2-D Stokeslet stroke (DECISIONS_4 #53), same call, same units.
   NOT expressible as MIDI: a gesture-ABI-only deformation — a MIDI recording
   of a stylus performance replays notes but not wakes (documented invariant,
   PROJECT_SPEC.md §8.7). Magnitude is the tip displacement itself (wake strength IS
   pen speed, by physics); the core sub-steps internally (≤ a/4 per pass —
   a/2 is the fold threshold itself, DECISIONS_3 #32).
   tip_radius in canvas-height units, maps from stylus pressure. */
SUMI_API void             sumi_add_wake  (sumi_instance_t* inst, float x0, float y0, float x1, float y1,
                                          float tip_radius);
/* v0.4: Hamiltonian pinch (spec §4.3(5)) — localized area-preserving saddle
   at (x, y), fold axis `angle` radians, per-pass strength `k_delta` (always
   feed DELTAS of a smoothed controller — absolute values integrate into
   runaway strain; sign swaps which axis compresses). Gesture-ABI entry
   (DECISIONS_3 #32): the fold axis is host-side data (pen azimuth, drag
   angle) with no MIDI path; the MIDI route (slide_mode = 1) drives the same
   pass from per-voice CC74 deltas at the voice position. */
SUMI_API void             sumi_add_pinch (sumi_instance_t* inst, float x, float y,
                                          float k_delta, float angle);
/* v0.11 (Phase 6 step 37, MEDIUM §2.2): ONE step of the Chladni cellular
   flow as a gesture — the Taylor–Green stream function ψ = psi·cos(k_x(x−x0))
   ·cos(k_y(y−y0)) on a lattice with pitch (sx, sy) and a cell centre at
   (x0, y0) — NORMALIZED canvas units (k = π/pitch) — split into its two
   diagonal waves cos(u−v) and cos(u+v), each an EXACT shear, applied one
   after the other (the kick-drift splitting). `psi` is the step's stream-
   function amplitude (canvas-height² units; the displacement is ~psi·k);
   `balance` weights the second wave 1 − 2·balance (0 = the cellular flow).
   CLASS EXACT: det J = 1 at any amplitude. A NEGATIVE psi applies the step's
   EXACT INVERSE — both shears negated AND in reversed order — so (psi, b)
   then (−psi, b) is the identity; a sign flip alone would not be (crossed
   shears do not commute). */
SUMI_API void             sumi_add_chladni(sumi_instance_t* inst, float psi, float balance,
                                           float sx, float x0, float sy, float y0);
/* v0.12 (Phase 6 step 38, MEDIUM §2.3): the viscous multipole BURST — the
   strike operator. The time-integrated displacement of an impulsive viscous
   2-D multipole of order m (2 = the quadrupole: two ejection lobes along the
   axis theta0, two intake lobes across it; radians, canvas frame, y down),
   the impulse spread over a Gaussian core of radius `a` (canvas heights).
   Method after Jaffer's Lamb–Oseen paper (arXiv:1810.04646), extended to
   m ≥ 2, where the time integral is elementary: Ψ = A_m (a/r)^(m−2)
   sin(m(θ−θ0)) [Φ_m(r²/ℓ²) − Φ_m(r²/a²)], Φ_2 = χ = (1 − e^−S)/S, d = ∇⊥Ψ.
   `D` is the radial displacement at r = a on the ejection axis over the
   whole burst (canvas heights; D < 0 applies the FIRST-ORDER inverse).
   CLASS SUB-STEPPED (the wake's family): d is divergence-free as a field,
   the finite map area-preserving to first order; the core splits the burst
   into passes whose peak displacement stays within β_m × the current core
   (|∇d| ≤ 0.25, the wake's a/4 criterion). THE STRIKE HAS A LIFETIME: it
   fires sharp at ℓ = a and the release grows the age to params.burst_age·a
   over params.burst_life seconds, each frame's increment emitted by the
   mapper under the pass budget — the first increment lands in the next
   sumi_update. Near the core the quadrupole is pure hyperbolic strain (the
   pinch is its r → 0 limit); in the diffused zone a ≪ r ≪ ℓ it decays as
   cos(2θ)/r. `m` clamps to 2..8; m = 0 takes params.burst_order.
   Gesture-ABI only until the medium's binding tables route strikes
   (step 42). */
SUMI_API void             sumi_add_burst(sumi_instance_t* inst, float x, float y, float a, float D,
                                         float theta0, uint32_t m);
/* v0.13 (Phase 6 step 39, MEDIUM §2.4): ONE kick-drift step of the SPARK
   SHEAR as a gesture. In the frame rotated by theta0 about (x, y): the
   x-shear x1 = x + A·w(y)·f(y), then the y-shear y1 = y + B·w(x1)·f(x1),
   with f a stack of triangle waves (k, 2k, 4k, …, params.spark_stack deep;
   params.spark_profile = 1 swaps in piecewise-linear hash noise) and w a
   Gaussian window of half-width `band` ACROSS each shear (0 = the whole
   canvas). A, B in canvas heights (the peak kick), k in radians per canvas
   height, `phase` the base octave's φ. A zero amplitude skips its stage.
   CLASS EXACT for ANY profile: x' = x + g(y) inverts as x = x' − g(y)
   whatever g is — its kinks become creases, legally. The step's EXACT
   INVERSE is the step (0, −B) followed by the step (−A, 0): reversed order
   AND negated — a sign flip in the same order is not (crossed shears do not
   commute, DECISIONS_5 #17). */
SUMI_API void             sumi_add_spark_shear(sumi_instance_t* inst, float x, float y, float band,
                                               float A, float B, float k, float phase, float theta0);
/* v0.13 (Phase 6 step 39, MEDIUM §2.4): the composed SPARK STRIKE — one
   exact drop pass (the Joule blast: radial outflow cannot be divergence-
   free, so the engine's oldest operator does it; `layer_type` as
   sumi_add_drop's: 0 ink, 2 feed, 3 NO blast, else clear water), a viscous multipole
   burst of core r and lobe displacement D along theta0 (the pinch lobes;
   the order params.burst_order, the age params.burst_age/_life), and the
   spark shear as a DECAYING EPISODE: kicks A = B = params.spark_shear·r
   spent as e^(−t/τ) over 4·params.spark_tau seconds in kick-drift steps
   along and across theta0, windowed to twice the radius, k from
   SUMI_CTL_SPARK_K, φ drawn per strike. CLASS SUB-STEPPED BY INHERITANCE
   (the burst is the strictest member); the drop and the shears are exact.
   A strike-route candidate for the binding tables (step 42). */
SUMI_API void             sumi_add_spark(sumi_instance_t* inst, float x, float y, float r, float D,
                                         float theta0, uint32_t layer_type);
/* v0.14 (Phase 6 step 40, MEDIUM §2.5): ONE step of the scaled CHIRIKOV
   STANDARD MAP as a gesture — the kick y1 = y + A·sin(k(x−x)+phase), then
   the drift x1 = x + eps·(y1−y), about (x, y); k = 2π·periods per canvas
   height along x, A = K/(k·eps) so that K is the step's chaos parameter
   (Greene's threshold ≈ 0.9716: sheets below, filamentation and island
   chains above — the elliptic island sits half a kick period from the
   centre). CLASS EXACT (two shears, det J = 1 at any K). A NEGATIVE K
   applies the step's EXACT INVERSE — the drift undone first, then the
   kick. |K| clamps to 2, the resampling medium's own limit: iterating a
   step above the core's route ceiling shreds ink into filaments finer than
   a texel (DECISIONS_5, the erosion table). */
SUMI_API void             sumi_add_chirikov(sumi_instance_t* inst, float x, float y, float K,
                                            uint32_t periods, float eps, float phase);

#ifdef __cplusplus
}
#endif
#endif /* SUMI_CORE_H */
