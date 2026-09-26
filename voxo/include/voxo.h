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
 * effect parameters — and the advisory memory gate. */
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
} voxo_stats_t;
VOXO_API void     voxo_stats(const voxo_t* v, voxo_stats_t* out);

#ifdef __cplusplus
}
#endif
#endif /* VOXO_H */
