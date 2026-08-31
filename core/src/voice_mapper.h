// Internal interface of the voice mapper (PROJECT_SPEC.md §3.3, §3.4, §2.4).
// Stage 1 (normalize): musical events -> the §3.3 normalized voice-event
// vocabulary — THE only vocabulary the simulator side ever sees.
// Stage 2 (lower): §3.3 events -> deformation passes in the displacement
// queue. Both stages are GPU-free and unit-testable headlessly.
#pragma once

#include "sumi_core.h"
#include "midi_normalizer.h"
#include "displacement.h"

#ifdef __cplusplus
extern "C" {
#endif

// §3.3 normalized event vocabulary. GlobalBend is a pragmatic extension for
// classic mode's global shear tine — §3.3 has no bend-shaped global control
// and sumi_ctl_t carries no bend dimension (see DECISIONS.md).
typedef enum {
    SUMI_VEV_VOICE_BEGIN = 0,   // voice_id, x, y, strike (0..1)
    SUMI_VEV_VOICE_GLIDE,       // voice_id, dx (semitone-scaled)
    SUMI_VEV_VOICE_PRESS,       // voice_id, pressure (0..1)
    SUMI_VEV_VOICE_SLIDE,       // voice_id, timbre (0..1)
    SUMI_VEV_VOICE_MIGRATE,     // voice_id, x, y
    SUMI_VEV_VOICE_END,         // voice_id, lift (0..1)
    SUMI_VEV_GLOBAL_CTL,        // dimension (sumi_ctl_t), value (0..1)
    SUMI_VEV_GLOBAL_BEND,       // value = bend in semitones (classic shear)
    SUMI_VEV_PAPER_DIP          // CC64 rising edge, or ABI call
} sumi_voice_event_kind_t;

typedef struct {
    sumi_voice_event_kind_t kind;
    uint32_t voice_id;
    uint32_t dimension;   // sumi_ctl_t for GLOBAL_CTL
    float    x, y;
    float    value;       // strike / pressure / timbre / lift / ctl value / bend
} sumi_voice_event_t;

typedef struct sumi_voice_mapper_t sumi_voice_mapper_t;

sumi_voice_mapper_t* sumi_voice_mapper_create(sumi_log_fn log_cb, void* log_user);
void sumi_voice_mapper_destroy(sumi_voice_mapper_t* vm);

// Stage 1: musical -> §3.3 vocabulary. `aspect` = field W/H (pitch layouts
// place drops on a *circle* on screen). Returns the number of events written.
uint32_t sumi_voice_mapper_normalize(sumi_voice_mapper_t* vm,
                                     const sumi_midi_event_t* in, uint32_t in_count,
                                     sumi_input_mode_t mode,
                                     const sumi_params_t* params, float aspect,
                                     sumi_voice_event_t* out, uint32_t max);

// Stage 2: §3.3 vocabulary -> deformation queue (classic-mode lowering).
// `drop_counter` is the instance's global §4.2 drop counter (shared with the
// sumi_add_drop gesture path so ring parity stays consistent).
void sumi_voice_mapper_lower(sumi_voice_mapper_t* vm,
                             const sumi_voice_event_t* events, uint32_t count,
                             uint32_t* drop_counter,
                             sumi_deform_queue_t* queue);

// Pitch -> position (§3.4), exposed for headless tests.
// layout 0: circle-of-fifths radial (low notes outer); 1: 12-column grid.
void sumi_pitch_to_position(uint8_t note, uint32_t layout, float aspect,
                            float* out_x, float* out_y);

#ifdef __cplusplus
}
#endif
