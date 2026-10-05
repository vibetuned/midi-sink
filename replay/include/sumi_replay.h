/* sumi_replay.h — Phase 9 step 65 (QOL §1, SOUND §5; DECISIONS_8 #22–#24):
 * SESSION REPLAY — the byte stream is the recording.
 *
 * Everything musical the core sees is MIDI (the fingering included), the
 * marble gestures are a handful of calls, and the look is the session
 * preset — so a RECORDING is: the session at the start, then per FRAME the
 * frame's dt and wall time, the bytes the core drained in it, the gesture
 * calls made before its update, the state changes (the session as applied),
 * the resizes and the dips. Platform-neutral text, version-stamped, the
 * source device named; the same file replays on every shell.
 *
 * THE FRAME BOUNDARY IS THE DRAIN POINT (DECISIONS_5 #8). The engine
 * coalesces continuous dimensions per sumi_update, so a performance replays
 * identically only when every byte lands in the frame that drained it and
 * every update runs with the dt it ran with. The recorder makes that exact
 * by construction: while recording, the shell's MIDI producer STAGES its
 * bytes here (sumi_replay_rec_midi, wait-free, one producer) instead of
 * pushing them to the core, and the render thread hands them to the core
 * itself at the start of each frame (sumi_replay_rec_frame), stamped with
 * that frame — the core sees them in the same update it would have
 * (it drains only at update), the recording knows which one. Playback
 * drives the scripted clock through the recorded boundaries: one
 * sumi_update(dt_recorded) + sumi_render per frame, however the host's
 * display paces. Re-bucketing the bytes by wall time on another cadence
 * (sumi_replay_rebucket) is the documented anti-pattern and the negative
 * test.
 *
 * Host-side, C11, libc only (malloc for the growing arrays; the staging
 * ring is a fixed SPSC ring on compiler atomics). The core stays stateless
 * about files: a shell owns storage and hands text in and out. The one
 * translation unit that links the core (sumi_replay_apply.c: feeding a
 * frame into an instance, the state through the preset serializer) is
 * separate, as the presets' apply is — a consumer that never calls it never
 * pulls the core. Format: replay/FORMAT.md. */
#ifndef SUMI_REPLAY_H
#define SUMI_REPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sumi_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SUMI_REPLAY_SCHEMA   1u
#define SUMI_REPLAY_EXT      ".sumireplay"

/* The header: who recorded, on what, at which size. Strings are UTF-8,
   NUL-terminated, truncated to fit. */
typedef struct {
    char     platform[16];   /* "macos" "linux" "windows" "ios" "android" "web" */
    char     backend[16];    /* "metal" "gl" "d3d11" "gles" "webgpu"            */
    char     device[64];     /* the machine or model name                       */
    char     app[32];        /* the app version string                          */
    uint32_t sumi_version;   /* sumi_version() of the writer                    */
    char     recorded[32];   /* ISO 8601, UTC, when the recording started       */
    uint32_t width, height;  /* the instance's size at the start (sumi_resize)  */
    float    pixel_ratio;
} sumi_replay_info_t;

/* The gesture calls recorded, with their arguments in order. */
typedef enum {
    SUMI_REPLAY_G_TAP = 0,    /* x y r                        sumi_gesture_tap       */
    SUMI_REPLAY_G_PINCH,      /* x y k angle span             sumi_gesture_pinch     */
    SUMI_REPLAY_G_TWIST,      /* x y strength radius profile  sumi_gesture_twist     */
    SUMI_REPLAY_G_PRESS,      /* x y R up down dt             sumi_gesture_press     */
    SUMI_REPLAY_G_PRESS_END,  /* -                            sumi_gesture_press_end */
    SUMI_REPLAY_G_TINE,       /* x0 y0 x1 y1 alpha magnitude  sumi_add_tine          */
    SUMI_REPLAY_G_WAKE,       /* x0 y0 x1 y1 tip              sumi_add_wake          */
    SUMI_REPLAY_G_DROP,       /* x y radius layer             sumi_add_drop          */
    SUMI_REPLAY_G_VORTEX,     /* x y strength radius profile  sumi_add_vortex        */
    SUMI_REPLAY_G_PINCH_RAW,  /* x y k angle                  sumi_add_pinch         */
    SUMI_REPLAY_G_COUNT
} sumi_replay_gesture_t;
#define SUMI_REPLAY_G_ARGS 6u

/* Byte sources, as the tablets' byte logs tag them: 0 an external device,
   1 the shell's own (touch, pen, strip), 2 session config. Informational. */
#define SUMI_REPLAY_SRC_EXTERNAL 0u
#define SUMI_REPLAY_SRC_SHELL    1u
#define SUMI_REPLAY_SRC_CONFIG   2u

/* The shell's push: what a byte does on the render thread (sumi_push_midi
   and the sound's ring, the shell's business). */
typedef void (*sumi_replay_push_fn)(void* user, uint8_t status, uint8_t d1, uint8_t d2, uint8_t src);

/* ---- the recorder ------------------------------------------------------- */

typedef struct sumi_replay_rec_t sumi_replay_rec_t;

/* `session_json` is the session as the one serializer writes it (the params
   AS THE CORE HOLDS THEM — sim_scale included — the input mode, the CC map,
   the custom palette). `len` 0 = NUL-terminated. */
sumi_replay_rec_t* sumi_replay_rec_create(const sumi_replay_info_t* info, const char* session_json, size_t len);
void               sumi_replay_rec_destroy(sumi_replay_rec_t* r);

/* THE PRODUCER'S SIDE — wait-free, exactly one thread (the shell's MIDI
   producer, however it serialises itself). `t` is the host's monotonic
   seconds. Returns false when the stage is full (4096 bytes in flight — a
   render thread that stalled a second); the byte is counted dropped. */
bool     sumi_replay_rec_midi(sumi_replay_rec_t* r, double t, uint8_t status, uint8_t d1, uint8_t d2, uint8_t src);

/* THE RENDER THREAD, right before sumi_update(inst, dt): hands every staged
   byte to `push` in order (the core's sole producer for the frame), records
   them under this frame, then closes the frame with its dt and wall time.
   Returns the bytes pushed. The gestures and state calls below belong to
   the frame whose sumi_replay_rec_frame comes next. */
uint32_t sumi_replay_rec_frame(sumi_replay_rec_t* r, double t, double dt, sumi_replay_push_fn push, void* user);
void     sumi_replay_rec_gesture(sumi_replay_rec_t* r, uint32_t kind, const float* args, uint32_t n);
/* A state change: the session as applied (the same text sumi_replay_rec_create took). A repeat of the last is skipped. */
void     sumi_replay_rec_state(sumi_replay_rec_t* r, const char* session_json, size_t len);
void     sumi_replay_rec_resize(sumi_replay_rec_t* r, uint32_t w, uint32_t h, float pixel_ratio);
void     sumi_replay_rec_dip(sumi_replay_rec_t* r);

/* Stopping: hands any staged bytes to `push` WITHOUT recording them (they
   belong to the frames after the recording). Returns the count. */
uint32_t sumi_replay_rec_flush(sumi_replay_rec_t* r, sumi_replay_push_fn push, void* user);

uint32_t sumi_replay_rec_frames(const sumi_replay_rec_t* r);
double   sumi_replay_rec_seconds(const sumi_replay_rec_t* r);   /* Σ dt */
uint32_t sumi_replay_rec_events(const sumi_replay_rec_t* r);
uint32_t sumi_replay_rec_dropped(const sumi_replay_rec_t* r);   /* stage full, or the recording's caps */
bool     sumi_replay_rec_full(const sumi_replay_rec_t* r);      /* a cap reached: the recording stops growing */

/* The file's text. NULL/0 = size query (the length, NOT counting the NUL);
   otherwise writes up to cap, always NUL-terminated, returns the full length. */
size_t   sumi_replay_rec_write(const sumi_replay_rec_t* r, char* out, size_t cap);
bool     sumi_replay_rec_save(const sumi_replay_rec_t* r, const char* path);

/* Now, as the header wants it: ISO 8601 UTC ("2026-10-05T19:21:04Z"). */
void     sumi_replay_timestamp(char* out, size_t cap);

/* ---- the player --------------------------------------------------------- */

typedef struct sumi_replay_t sumi_replay_t;

/* Parses the text (len 0 = NUL-terminated). NULL when it is not a replay
   file, its schema is newer than this library's, or it is truncated. Lines
   it does not know are skipped (the schema rule, as the presets'). */
sumi_replay_t*  sumi_replay_open(const char* text, size_t len);
sumi_replay_t*  sumi_replay_load(const char* path);
void            sumi_replay_close(sumi_replay_t* r);

const sumi_replay_info_t* sumi_replay_info(const sumi_replay_t* r);
const char*     sumi_replay_session(const sumi_replay_t* r, size_t* len);
uint32_t        sumi_replay_frame_count(const sumi_replay_t* r);
uint32_t        sumi_replay_event_count(const sumi_replay_t* r);
double          sumi_replay_duration(const sumi_replay_t* r);    /* Σ dt */
uint32_t        sumi_replay_position(const sumi_replay_t* r);    /* the next frame's index */
double          sumi_replay_elapsed(const sumi_replay_t* r);     /* Σ dt of the frames fed */
double          sumi_replay_peek_dt(const sumi_replay_t* r);     /* the next frame's dt; 0 at the end (for pacing) */
void            sumi_replay_rewind(sumi_replay_t* r);

/* What a frame's events do. Any callback may be NULL. */
typedef struct {
    void (*midi)(void* user, uint8_t status, uint8_t d1, uint8_t d2, uint8_t src);
    void (*gesture)(void* user, uint32_t kind, const float* args, uint32_t n);
    void (*state)(void* user, const char* session_json, size_t len);
    void (*resize)(void* user, uint32_t w, uint32_t h, float pixel_ratio);
    void (*dip)(void* user);
} sumi_replay_sink_t;

/* Feeds the next frame's events through the sink, in the recorded order,
   and returns its dt and wall time: the caller then runs
   sumi_update(inst, dt) and sumi_render. False at the end. */
bool            sumi_replay_next(sumi_replay_t* r, const sumi_replay_sink_t* sink, void* user, double* dt_out, double* t_out);

/* THE NEGATIVE TEST: regroups every event by its wall time into frames of a
   fixed dt (frame k holds [k·dt, (k+1)·dt)), as a host re-bucketing bytes by
   wall time on its own cadence would. Rewinds. The field must diverge. */
bool            sumi_replay_rebucket(sumi_replay_t* r, double fixed_dt);

/* ---- the apply (sumi_replay_apply.c — links the core and the presets) --- */

#define SUMI_REPLAY_APPLY_SIZE     1u   /* sumi_resize to the recording's size, and its resize events */
#define SUMI_REPLAY_APPLY_PALETTE  2u   /* the recording's custom palette slot too (else the viewer's stays) */

/* Rewinds and applies the header's session to the instance: the params as
   recorded (sim_scale included), the input dialect, the CC map (the core's
   routes; the sound's bus routes are the shell's), the palette and the size
   under their flags. The paper is NOT dipped here: a recording starts with
   its own dip (the shells dip when they start recording). */
bool            sumi_replay_begin(sumi_replay_t* r, sumi_instance_t* inst, uint32_t flags);
/* One frame into the instance: the bytes through `push` (the caller's
   sumi_push_midi and its sound ring), the gestures on the core, the state
   through the serializer, resizes under the flag, dips. False at the end;
   the caller runs sumi_update(inst, *dt_out) and sumi_render after true. */
bool            sumi_replay_step(sumi_replay_t* r, sumi_instance_t* inst, uint32_t flags,
                                 sumi_replay_push_fn push, void* user, double* dt_out);
/* The gesture table, for a shell that drives the sink itself. */
void            sumi_replay_apply_gesture(sumi_instance_t* inst, uint32_t kind, const float* args, uint32_t n);
/* A state event onto an instance (the serializer; palette under the flag). */
bool            sumi_replay_apply_state(sumi_instance_t* inst, const char* session_json, size_t len, uint32_t flags);

#ifdef __cplusplus
}
#endif
#endif /* SUMI_REPLAY_H */
