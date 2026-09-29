/* voxo.h — Voxo, the internal MPE sampler (Phase 7, SOUND §1): a SIBLING core
 * beside libsumi, C++20 behind this pure C ABI — the sumi_core.h rules
 * verbatim (no STL, no exceptions, no callbacks-into-C++ across it; a
 * three-state export macro; a version function; version-gated growth).
 * The rendering engine stays audio-free; a shell wires both, fanning its ONE
 * MIDI producer's bytes into libsumi (sumi_push_midi) and into Voxo
 * (voxo_push_midi). Voxo compiles the core's MIDI normalizer FROM SOURCE —
 * one MPE decoder, two builds, zero runtime coupling.
 *
 * THE CALLBACK-THREAD CONTRACT (the hardest real-time contract in the app —
 * SOUND §1; DECISIONS_6 #3). The audio backend calls voxo_render from its own
 * thread once per block. On that thread, Voxo:
 *   - allocates nothing, frees nothing, takes no lock, logs nothing, makes no
 *     blocking call (a test replaces the global allocator and asserts zero);
 *   - reads MIDI only from the lock-free single-producer ring the shell fills
 *     with voxo_push_midi (the normalizer's own wait-free ring), drained
 *     ONCE at block start; every voice-state transition happens there, in
 *     order, before a sample is written — never mid-block, never from
 *     another thread;
 *   - takes the shell's settings (gain, input mode) through atomics it reads
 *     at block start; the shell never touches a voice;
 *   - writes exactly `frames` interleaved stereo floats.
 * Everything else — voxo_create/destroy, voxo_start/stop, the setters, the
 * stats — is the shell's thread(s). voxo_push_midi is wait-free and belongs to
 * exactly ONE producer thread per instance (the shell's MIDI merge point).
 * voxo_render is also callable with no device running (tests, offline
 * bounces); it then follows the same contract on the calling thread.
 *
 * Step 47 shipped the skeleton: a sine per voice following MPE (note + bend,
 * velocity and pressure to level, sustain honoured), the miniaudio backend on
 * the platform's default output, block-size defaults per platform. Step 49
 * made the voice a sample player: ONE sample (voxo_set_sample) read at the
 * ratio 2^((note - root + bend)/12), the ratio ramped per sample, 4-point
 * Hermite interpolation (linear kept as the lab's comparison); the sine stays
 * the sound when no sample is loaded. Step 50 brought the format: a Decent
 * Sampler preset or library parsed and decoded to memory with its compat
 * report (voxo_load_preset). Step 51 gave the voice its interior: a
 * performance voice stacks the preset's zones (velocity layers with
 * crossfades, round robins, release samples), each with its ADSR, its loop
 * with crossfade, the low-pass filter, the MPE sources through the preset's
 * bindings or the defaults (pressure to expression, CC 74 to the cutoff),
 * smoothed by the preset's rising/falling times. Step 52 added the bus — a
 * Freeverb-class reverb and a feedback delay after the sum, from the preset's
 * effect parameters — and the advisory memory gate.
 *
 * Phase 8 step 56 (SYNTH §1–§2.4, §4): SUZU, the symplectic phase-space
 * synth, a SOURCE beside the sampler (voxo_set_source): the same callback,
 * the same normalizer-fed voice model, the same bus. A voice is a cell (x, y)
 * stepped by the magic-circle leapfrog with exact tuning ε = 2 sin(πf/fs) —
 * det = 1, amplitude confined by construction — re-based onto the same orbit
 * on every retune so a glide does not amplitude-modulate, a phase-space shear
 * for harmonics, the Chamberlin SVF (CC 74 → cutoff) with its resonance the
 * declared dissipation, all in a 2× oversampled section; the release is a
 * declared contraction. Every element declares its class (voxo/src/suzu.h's
 * table, SYNTH §1). FTZ/DAZ is set on the rendering thread at its first block.
 * Step 57 (§2.5–§2.6, §3): THE MODAL VOICE — a lattice of cells at the
 * preset's ratios (harmonic string, stiff bar, bell, glass, the plucked
 * string as Karplus–Strong in modal form) with declared decays per mode,
 * coupled along a chain by a shared-potential kick computed from the
 * pre-update positions (symplectic in the joint space; the swirl, 0xA0,
 * turns the coupling up), gated at patch load by the spectral radius of the
 * stiffness + coupling matrix; and THE BREATH BOW — an energy servo per mode
 * (signed damping toward the breath's target: a limit cycle, silence with no
 * breath) fed by CC 2 (11 its alias), the wind player's sustained tone. */
#ifndef VOXO_H
#define VOXO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Three-state export macro: shared-build export, shared-consume import, static no-op. */
#if defined(_WIN32)
  #if defined(VOXO_BUILD_SHARED)
    #define VOXO_API __declspec(dllexport)
  #elif defined(VOXO_USE_SHARED)
    #define VOXO_API __declspec(dllimport)
  #else
    #define VOXO_API
  #endif
#else
  #define VOXO_API __attribute__((visibility("default")))
#endif

typedef struct voxo_t voxo_t;

/* Levels as sumi_core.h's (0 panic, 1 error, 2 warn, 3 info). Never called
   from the audio thread. */
typedef void (*voxo_log_fn)(int level, const char* msg, void* user);

typedef struct {
    uint32_t    sample_rate;   /* Hz for offline rendering; a device may run another
                                  rate, which voxo_start adopts (voxo_stats says).
                                  0 = 48000.                                         */
    uint32_t    block_frames;  /* the period asked of the device; 0 = the platform
                                  default (voxo_default_block_frames).               */
    uint32_t    max_voices;    /* the voice pool, 1..64; 0 = 16 (15 MPE members and
                                  the master, or a classic keyboard's polyphony).    */
    voxo_log_fn log_cb;
    void*       log_user;
} voxo_config_t;

VOXO_API uint32_t voxo_version(void);                /* (maj<<16)|(min<<8)|patch */
/* Voxo's monotonic clock in seconds (steady_clock: CLOCK_MONOTONIC on Android
   and Linux, the mach clock on Apple, QPC on Windows) — the clock the latency
   probe below stamps with, so a shell compares its own marks against it. */
VOXO_API double   voxo_now_seconds(void);
/* The platform's block-size default (DECISIONS_6 #4): macOS 128, iOS 128,
   Android 192, Windows 256 (WASAPI shared), Linux 256. */
VOXO_API uint32_t voxo_default_block_frames(void);

VOXO_API voxo_t*  voxo_create (const voxo_config_t* config);   /* NULL config = defaults */
VOXO_API void     voxo_destroy(voxo_t* v);                     /* stops the device first */

/* The backend: the platform's default output device through miniaudio
   (CoreAudio / WASAPI / ALSA-PulseAudio / AAudio / ...). start opens it and the
   callback thread begins calling voxo_render; false = no device (the shell
   keeps running silent). stop is synchronous: no callback runs after it. */
VOXO_API bool     voxo_start  (voxo_t* v);
VOXO_API void     voxo_stop   (voxo_t* v);
VOXO_API bool     voxo_running(const voxo_t* v);

/* The shell's ONE producer thread. Wait-free; the oldest message is dropped
   on overflow (counted in voxo_stats). The same bytes it gives libsumi. */
VOXO_API void     voxo_push_midi(voxo_t* v, uint8_t status, uint8_t data1, uint8_t data2);

/* The input dialect, sumi_input_mode_t's values (0 auto, 1 MPE, 2 classic
   keyboard, 3 wind) — the shell mirrors what it set on libsumi. Applied at the
   next block start. */
VOXO_API void     voxo_set_input_mode(voxo_t* v, uint32_t mode);
/* Master gain 0..2 (1 = unity); applied at the next block start. */
VOXO_API void     voxo_set_gain(voxo_t* v, float gain);

/* THE SAMPLE (step 49, SOUND §2): interleaved float frames (1 or 2 channels)
   at `sample_rate`, sounding `root_note` (MIDI, fractional allowed) when read
   at ratio 1. Voxo COPIES the frames on the calling (shell) thread and hands
   the copy to the callback, which swaps it in at its next block start —
   voices on the previous sample end there. Plays once, no loop (step 51
   brings loops and layers). NULL frames or 0 frames = voxo_clear_sample. */
VOXO_API bool     voxo_set_sample(voxo_t* v, const float* frames, uint32_t frame_count,
                                  uint32_t channels, uint32_t sample_rate, float root_note);
VOXO_API void     voxo_clear_sample(voxo_t* v);   /* back to the sine */

/* THE PRESET (step 50, SOUND §3): a Decent Sampler .dspreset (its folder
   holds the samples) or a .dslibrary (the zip holds both). Parsed and decoded
   to memory on the calling (shell) thread — a large library takes seconds;
   the preload gate of step 52 puts that behind progress. Returns false with
   `report->text` saying why when the file is not a preset; true, with the
   compat report filled, when it loaded (missing samples and unsupported
   features are notes in the report, never refusals). Until step 51's layers,
   the zone under middle C at velocity 100 becomes THE sample of the player
   above, so a loaded preset sounds. */
enum {
    VOXO_NOTE_CHORUS          = 1u << 0,
    VOXO_NOTE_CONVOLUTION     = 1u << 1,
    VOXO_NOTE_MODULATORS      = 1u << 2,
    VOXO_NOTE_UI              = 1u << 3,
    VOXO_NOTE_STREAMING       = 1u << 4,
    VOXO_NOTE_SEQUENCES       = 1u << 5,
    VOXO_NOTE_OTHER_FILTERS   = 1u << 6,
    VOXO_NOTE_UNKNOWN_EFFECT  = 1u << 7,
    VOXO_NOTE_MISSING_SAMPLES = 1u << 8,
    VOXO_NOTE_UNKNOWN_BINDING = 1u << 9,
    VOXO_NOTE_MEMORY          = 1u << 10   /* step 52: larger than the advised budget; loaded anyway */
};
typedef struct {
    uint32_t ok;                 /* 1 = loaded (read the notes); 0 = refused, text says why */
    uint32_t groups, zones, samples;
    uint32_t samples_missing;    /* zones whose file could not be read: they stay silent */
    uint32_t memory_bytes;       /* decoded sample bytes now in memory */
    uint32_t memory_estimate;    /* step 52: the size read off the headers before decoding */
    uint32_t memory_budget;      /* the shell's advice in force for this load (0 = none) */
    uint32_t notes;              /* VOXO_NOTE_* bits */
    char     name[128];
    char     text[2048];         /* the report: the summary line, then one line per note (voxo/COMPAT_REPORT.md) */
} voxo_report_t;
VOXO_API bool     voxo_load_preset(voxo_t* v, const char* path, voxo_report_t* report);
/* THE INSTRUMENT'S REACH (step 53, DECISIONS_6 #27): the notes the loaded
   instrument sounds — the union of its attack zones' ranges — as 128 bits
   (bit n of mask[n / 8]). A play surface hides the cells it cannot sound.
   Returns false, mask untouched, when no preset is loaded (the sine and a
   raw sample answer every note). Reads the newest instrument the shell
   handed over, swapped in or not. */
VOXO_API bool     voxo_covered_notes(const voxo_t* v, uint8_t mask[16]);

/* THE SHELL'S CC MAP INTO THE BUS (step 53, #27): beside the preset's own
   bindings, the shell routes controllers to the reverb and the delay — the
   same (channel, cc) -> target table the core's sumi_map_cc keeps, with
   these targets, numbered from 1000 so a shell's one route table holds both
   namespaces (presets/SCHEMA.md). A mapped CC turns its effect on if the
   preset had none. Up to 32 routes; channel 0xFF = any. Applied at the next
   block start. */
enum {
    VOXO_CTL_REVERB_WET     = 1000,   /* 0..1                  */
    VOXO_CTL_REVERB_ROOM    = 1001,   /* 0..1                  */
    VOXO_CTL_REVERB_DAMPING = 1002,   /* 0..1                  */
    VOXO_CTL_DELAY_WET      = 1003,   /* 0..1                  */
    VOXO_CTL_DELAY_TIME     = 1004,   /* 0.05 .. 1.0 s         */
    VOXO_CTL_DELAY_FEEDBACK = 1005,   /* 0 .. 0.9              */
    VOXO_CTL_COUNT          = 6
};
VOXO_API void     voxo_map_cc(voxo_t* v, uint8_t channel /*0xFF=any*/, uint8_t cc, uint32_t target /*VOXO_CTL_*/);
VOXO_API void     voxo_clear_cc_map(voxo_t* v);

/* THE ADVISORY GATE (step 52, SOUND §3): the shell's memory advice in bytes —
   the desktop's free memory with headroom, iOS's os_proc_available_memory,
   Android's what the activity manager says. A preset whose decoded size (read
   off the sample headers before decoding) exceeds it gets VOXO_NOTE_MEMORY in
   its report and loads anyway — a warning, never a wall. 0 = no gate. */
VOXO_API void     voxo_set_memory_budget(voxo_t* v, uint64_t bytes);
VOXO_API void     voxo_unload_preset(voxo_t* v);   /* back to the single sample, or the sine */
/* The canonical sentence of one VOXO_NOTE_ bit — the documentation quotes these. */
VOXO_API const char* voxo_note_copy(uint32_t note);
/* The read interpolation: 0 = 4-point Hermite (the default, DECISIONS_6 #10),
   1 = linear — the lab's side-by-side, not a product setting. */
VOXO_API void     voxo_set_interpolation(voxo_t* v, uint32_t mode);
/* Local Control (CC 122) as MIDI defines it for a receiver with its own
   keyboard: OFF means the shell's own play surface must not sound here (its
   bytes still go out over MIDI), external input still does. Voxo TRACKS the
   state — a CC 122 in the stream sets it, this call sets it — and reports
   it in voxo_stats; the SHELL applies it, since only the shell knows which
   bytes are its own (it stops fanning them into voxo_push_midi). */
VOXO_API void     voxo_set_local_control(voxo_t* v, bool on);

/* Step 56 (SYNTH §1–§2.4): THE SOURCE — what a voice sounds with. The
   sampler (the sine, one sample, or the preset) or Suzu, the synth. Switching
   ends every voice (as an instrument swap does) — the next note is the new
   source's. Read at block start with the other settings. */
#define VOXO_SOURCE_SAMPLER 0u
#define VOXO_SOURCE_SUZU    1u
VOXO_API void     voxo_set_source(voxo_t* v, uint32_t source);

/* Suzu's patch (step 56: the cells; the modal voice and the bow follow in
   steps 57–58). POD, copied into the callback's idle slot and flipped —
   never read mid-block. voxo_suzu_default_params fills the defaults. */
typedef struct {
    float    level;        /* the strike's orbit amplitude at velocity 127, 0..1 (dflt 0.25)      */
    float    attack_s;     /* the output ramp after the strike, seconds (dflt 0.003)              */
    float    release_s;    /* the declared contraction: the T60 after note-off, seconds (dflt 0.4) */
    float    cutoff_hz;    /* the SVF's cutoff at CC 74 centre; >= 20000 = the filter bypassed
                              (dflt 20000); CC 74 scales it by 2^((t − 0.5)·6), as the sampler   */
    float    resonance;    /* 0..1: the SVF's damping q = 2 − 1.9·resonance — 0 is a gentle
                              slope, 1 rings (the declared dissipation you can hear; dflt 0)     */
    float    shear;        /* the phase-space shear's gain g, 0..1 (dflt 0: a pure orbit)        */
    uint32_t shear_kind;   /* 0 cubic (x += g·y³), 1 triangle fold (dflt 0)                      */
    uint32_t retune_mode;  /* THE LAB'S: 0 = the orbit re-based on retune (the shipped sound);
                              1 = the plain recurrence under the per-sample pitch ramp;
                              2 = the plain recurrence STEPPED once per block, no ramp — the
                              form SYNTH §2.1 feared (the glide ripples the decision prints)  */
    uint32_t update_mode;  /* THE LAB'S: 0 = the leapfrog (symplectic); 1 = the naive
                              simultaneous update, det 1 + ε² — the drift test's red control   */
    /* Step 57 (SYNTH §2.5–§2.6, §3): the modal voice and the breath bow. */
    uint32_t voice_kind;   /* 0 = one cell (step 56's); 1 = the modal lattice (dflt 1)          */
    uint32_t modal_preset; /* 0 harmonic string, 1 stiff bar, 2 bell, 3 glass, 4 plucked string
                              (Karplus–Strong in modal form) — dflt 0                          */
    uint32_t modes;        /* the lattice's cells, 1..16 (dflt 8)                                */
    float    coupling;     /* κ, 0..3.5: the chain's shared-potential coupling relative to the
                              LOWEST mode's stiffness (dflt 0.05); the swirl adds up to 0.5;
                              its detune is compensated exactly at patch load (the modes stay
                              in tune at any κ the gate admits)                                 */
    float    decay_s;      /* the fundamental's T60 while held, seconds (dflt 3; 0 = none: the
                              lab's energy ledger)                                              */
    float    decay_bright; /* β: the extra decay rate per (r_k² − 1), 1/s — highs die first
                              (dflt 0.3)                                                        */
    float    stiffness;    /* B, the string presets' inharmonicity (dflt 0: nylon)              */
    float    pluck;        /* the pluck position 0..0.5 for the plucked string (dflt 0.28)      */
    float    bow_onset_s;  /* the bow's time constant τ — an e-fold of amplitude from silence;
                              0 = no bow (dflt 0.15)                                            */
    float    bow_position; /* which partials the bow feeds: 0 the fundamental alone, 1 every
                              mode evenly, between sin(kπ·pos) (dflt 0.3)                        */
    uint32_t breath_cc;    /* the breath controller (dflt 2; 11 is read as its alias too)       */
    uint32_t lattice_gate; /* THE LAB'S: 1 = the load gate on (dflt); 0 = bypassed, the gate's
                              red control (an over-bound patch blows up in the harness)        */
} voxo_suzu_params_t;
VOXO_API void     voxo_suzu_default_params(voxo_suzu_params_t* out);
/* Returns false — and keeps the patch as it was — when the lattice load gate
   rejects it: λ_max of the stiffness + coupling matrix at the highest note
   (MIDI 108, the swirl's full addition to κ) reaches the joint leapfrog's
   bound of 4; the log callback says so. voxo_suzu_coupling_bound gives the κ
   at which that patch meets the bound (the UI's ceiling, the test's control). */
VOXO_API bool     voxo_set_suzu_params(voxo_t* v, const voxo_suzu_params_t* params);
VOXO_API float    voxo_suzu_coupling_bound(const voxo_t* v, const voxo_suzu_params_t* params);

/* One block: `frames` interleaved stereo float samples (L, R, L, R, ...).
   The callback's whole body — the contract above — and callable with no
   device (tests, offline bounces). */
VOXO_API void     voxo_render(voxo_t* v, float* out_lr, uint32_t frames);

typedef struct {
    uint32_t sample_rate;     /* the device's while running, else the config's */
    uint32_t block_frames;    /* the device's period while running             */
    uint32_t active_voices;   /* after the last block                          */
    uint32_t dropped_midi;    /* ring overflow, since create                   */
    uint32_t callbacks;       /* blocks the device asked for, since start      */
    uint32_t xruns;           /* callbacks that arrived late by more than half
                                 a period, or whose render outran the period —
                                 the audible-glitch proxy (#3); the first eight
                                 callbacks after start are not judged           */
    float    render_last_ms;  /* the last block's voxo_render time             */
    float    render_max_ms;   /* the worst since start                         */
    uint32_t input_mode;      /* the effective dialect after the last block
                                 (sumi_input_mode_t's values)                   */
    /* The latency probe (step 48): every note-on consumed is counted, and the
       voxo_now_seconds() at the START of the block that consumed the latest
       one is kept — a shell that marked its touch-down or its push on the same
       clock reads the input-to-callback latency off the difference. 0 while
       rendering with no device.                                               */
    uint32_t note_ons;
    double   last_note_on_seconds;
    /* The backend's own numbers, where the platform tells them; 0 elsewhere.  */
    uint32_t frames_per_burst;     /* the device's period as it runs (AAudio's burst) */
    uint32_t buffer_frames;        /* frames the device buffers ahead of the DAC       */
    uint32_t device_xruns;         /* the platform's own underrun count (AAudio)       */
    float    output_latency_ms;    /* callback-to-DAC as the platform reports it       */
    uint32_t low_latency;          /* 1 when the platform granted its low-latency path
                                      (AAudio's LOW_LATENCY performance mode; on the
                                      desktops, the period taken as asked)             */
    /* Step 49. */
    uint32_t local_control;        /* 1 = on (the default), 0 = CC 122 said off        */
    uint32_t interpolation;        /* 0 Hermite, 1 linear                              */
    uint32_t sample_frames;        /* the sample the callback plays; 0 = the sine      */
    uint32_t sample_channels;
    uint32_t sample_rate_hz;
    float    sample_root_note;
    /* Step 50. */
    uint32_t preset_loaded;        /* 1 while a preset is held                        */
    uint32_t preset_zones;
    /* Step 51. */
    uint32_t active_layers;        /* sample players inside the active voices (a voice
                                      stacks its zones: layers, release samples)     */
    char     device[64];      /* the output device's name, UTF-8, "" if none  */
    /* Step 56. */
    uint32_t source;               /* VOXO_SOURCE_* after the last block                */
    uint32_t suzu_modes;           /* step 57: the lattice's cells per voice (0 = one cell)    */
    uint32_t ftz;                  /* the rendering thread's flush-to-zero: 1 set, 2 the
                                      platform refused it, 0 no block rendered yet       */
} voxo_stats_t;
VOXO_API void     voxo_stats(const voxo_t* v, voxo_stats_t* out);

#ifdef __cplusplus
}
#endif
#endif /* VOXO_H */
