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
    SUMI_CTL_VORTEX_Y        = 2,
    SUMI_CTL_VISCOSITY       = 3,
    SUMI_CTL_PAPER_ROUGHNESS = 4,
    SUMI_CTL_PALETTE_MORPH   = 5,
    SUMI_CTL_INK_FLOW        = 6,   /* breath aliases here in wind mode */
    SUMI_CTL_RIPPLE_AMP      = 7,   /* v0.4: sine ripple amplitude A          */
    SUMI_CTL_RIPPLE_FREQ     = 8,   /* v0.4: ripple wavenumber k              */
    SUMI_CTL_COUNT           = 9
} sumi_ctl_t;

typedef enum {                       /* v0.4 vortex profiles, spec §4.3(3) */
    SUMI_VORTEX_EXPONENTIAL = 0,     /* Jaffer: diffuse, breath-like       */
    SUMI_VORTEX_RANKINE     = 1,     /* rigid core, crease ring at R       */
    SUMI_VORTEX_LAMB_OSEEN  = 2      /* v0.6: the §4.3(7) swirl as a GESTURE —
                                        strength = Γ·Δt (signed), radius = r_c.
                                        The Marble-mode "pull back to stir"
                                        (DECISIONS_4 #30/#49); CC routing keeps
                                        profiles 0/1.                        */
} sumi_vortex_profile_t;

typedef enum {                       /* v0.6: sumi_add_drop layer types      */
    SUMI_DROP_INK   = 0,             /* new ink band (counter-derived phase) */
    SUMI_DROP_CLEAR = 1,             /* clear water / surfactant: expands the
                                        field, interior un-inked            */
    SUMI_DROP_FEED  = 2              /* GROW the ink already under the centre:
                                        the §3.4/§4.4 boundary growth as a
                                        gesture — the interior takes the
                                        centre texel's band, so a held press
                                        widens a band instead of laying rings.
                                        Radius = sqrt((R+ΔR)² − R²), the host
                                        tracks R (DECISIONS_4 #49).          */
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
    SUMI_LAYOUT_PIANO_GRID  = 5  /* classical two-row piano grid, C1..B7      */
} sumi_layout_t;

typedef struct {
    float    fluid_viscosity;    /* damping for continuous agitation            */
    float    expansion_rate;     /* pressure/breath-driven drop feed scale      */
    float    paper_roughness;    /* washi fiber composite strength              */
    float    smoothing_ms;       /* expressive-dimension smoothing time const   */
    uint32_t active_palette_id;  /* 0 sumi black, 1 indigo, 2 ochre             */
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
                                    1 Hamiltonian pinch (delta-driven)         */
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
                                    holds position. Exactly ONE consumer owns
                                    the note bend; switchable live. Master
                                    bend keeps its v1 shear tine regardless. */
    uint32_t press_mode;         /* 0xD0 channel-pressure routing (§3.4 v0.4):
                                    0 = ink feed (v1 grow, default), 1 = the
                                    Lamb-Oseen swirl — hardware's door to the
                                    swirl voice. 0xA0 poly pressure -> swirl
                                    in either mode.                          */
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
} sumi_params_t;

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
} sumi_cell_info_t;

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
                                            float norm_x, float norm_y,
                                            sumi_cell_info_t* out);

/* Manual touch / mouse gestures — render thread only, normalized [0,1] coords.
   layer_type: sumi_drop_layer_t (0 ink, 1 clear, 2 feed — v0.6). */
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

#ifdef __cplusplus
}
#endif
#endif /* SUMI_CORE_H */
