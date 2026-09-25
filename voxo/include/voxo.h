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
 * Step 47 ships the skeleton: a sine per voice following MPE (note + bend,
 * velocity and pressure to level, sustain honoured), the miniaudio backend on
 * the platform's default output, block-size defaults per platform. The
 * sampler's voice interior arrives in steps 49–52. */
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
    char     device[64];      /* the output device's name, UTF-8, "" if none  */
} voxo_stats_t;
VOXO_API void     voxo_stats(const voxo_t* v, voxo_stats_t* out);

#ifdef __cplusplus
}
#endif
#endif /* VOXO_H */
