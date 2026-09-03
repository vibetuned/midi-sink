// Internal interface of the MIDI normalizer (PROJECT_SPEC.md §3.1, §3.2, §2.5).
// Owns the lock-free SPSC ring fed by sumi_push_midi (producer: the host MIDI
// thread) and the stateful decoder that runs on the render thread inside
// sumi_update. Emits device-agnostic *musical* events; voice_mapper turns
// them into the §3.3 vocabulary and then into deformations.
#pragma once

#include "sumi_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SUMI_MEV_NOTE_ON = 0,        // a = note, b = velocity (1..127)
    SUMI_MEV_NOTE_OFF,           // a = note, b = release velocity
    SUMI_MEV_BEND,               // f = bend in semitones (RPN 0 range applied)
    SUMI_MEV_CC,                 // a = controller, b = value
    SUMI_MEV_CHANNEL_PRESSURE,   // b = pressure
    SUMI_MEV_POLY_PRESSURE       // a = note, b = value (0xA0, v0.4 swirl §2.1)
} sumi_midi_event_kind_t;

typedef struct {
    sumi_midi_event_kind_t kind;
    uint8_t channel;             // 0-based
    uint8_t a;
    uint8_t b;
    float   f;
} sumi_midi_event_t;

typedef struct sumi_normalizer_t sumi_normalizer_t;

sumi_normalizer_t* sumi_normalizer_create(sumi_log_fn log_cb, void* log_user);
void sumi_normalizer_destroy(sumi_normalizer_t* n);

// Producer side — wait-free, exactly one thread, callable concurrently with
// the render thread (§5.2). Drops the OLDEST message on overflow (§3.1).
void sumi_normalizer_push(sumi_normalizer_t* n, uint8_t status, uint8_t data1, uint8_t data2);

// Consumer side — render thread. Drains the ring, decodes statefully, writes
// up to `max` events; returns the count. `now_seconds` is a monotonic clock
// (any origin): §2.5 detection only weighs activity inside a sliding window,
// so a quiet instrument stops dominating the mode after a few seconds.
uint32_t sumi_normalizer_drain(sumi_normalizer_t* n, double now_seconds,
                               sumi_midi_event_t* out, uint32_t max);

uint32_t sumi_normalizer_dropped(const sumi_normalizer_t* n);

// Mode override (SUMI_INPUT_AUTO = use the §2.5 heuristic).
void sumi_normalizer_set_mode(sumi_normalizer_t* n, sumi_input_mode_t mode);
// Effective mode after override/heuristic resolution.
sumi_input_mode_t sumi_normalizer_mode(const sumi_normalizer_t* n);

// MPE zone (§2.1). Default when no MCM was received: Lower Zone, master =
// ch 1 (index 0), members = ch 2..16 (index 1..15). v1 supports one zone.
typedef struct {
    uint8_t master;         // master channel index (0-based)
    uint8_t first_member;   // first member channel index
    uint8_t member_count;   // 0 = zone disabled
} sumi_mpe_zone_t;

sumi_mpe_zone_t sumi_normalizer_zone(const sumi_normalizer_t* n);

#ifdef __cplusplus
}
#endif
