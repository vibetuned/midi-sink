// Internal interface of the voice mapper (PROJECT_SPEC.md §2.1, §2.4, §3.3,
// §3.4, §4.4).
// Stage 1 (normalize): musical events -> the §3.3 normalized voice-event
// vocabulary — THE only vocabulary the simulator side ever sees. Continuous
// dimensions are coalesced to at most one event per voice per update (§3.4).
// Stage 2 (lower): §3.3 events -> deformation passes, with exponential
// smoothing (smoothing_ms), §4.4 incremental drop expansions for sustained
// pressure, and a per-frame deformation budget with overflow merging.
// Both stages are GPU-free and unit-testable headlessly.
#pragma once

#include "sumi_core.h"
#include "midi_normalizer.h"
#include "displacement.h"
#include "layouts.h"

#ifdef __cplusplus
extern "C" {
#endif

// v0.4 ripple ctl mapping (§4.3(6)) — one definition for BOTH consumers: the
// mapper's bake-pass emission and the engine's live-composite uniforms.
// ctl (0..1) -> A in canvas-height units; k in radians per canvas-height unit
// (2..16 waves across the canvas height).
#define SUMI_RIPPLE_AMP_MAX   0.025f
#define SUMI_RIPPLE_K_MIN     12.566371f   /* 2π · 2  */
#define SUMI_RIPPLE_K_MAX     100.530965f  /* 2π · 16 */

// §3.3 normalized event vocabulary. GlobalBend is a pragmatic extension for
// classic mode's global shear tine — §3.3 has no bend-shaped global control
// and sumi_ctl_t carries no bend dimension (see DECISIONS.md). VoiceBegin
// additionally carries the local pitch axis (ax, ay: direction × distance of
// one semitone) so glide can drag the drop along it without re-deriving
// layout state.
typedef enum {
    SUMI_VEV_VOICE_BEGIN = 0,   // voice_id, x, y, strike (0..1), ax/ay pitch axis
    SUMI_VEV_VOICE_GLIDE,       // voice_id, value = bend in semitones
    SUMI_VEV_VOICE_PRESS,       // voice_id, value = pressure (0..1)
    SUMI_VEV_VOICE_SLIDE,       // voice_id, value = timbre (0..1)
    SUMI_VEV_VOICE_MIGRATE,     // voice_id, x, y (wind mode, later step)
    SUMI_VEV_VOICE_END,         // voice_id, value = lift (0..1)
    SUMI_VEV_GLOBAL_CTL,        // dimension (sumi_ctl_t), value (0..1)
    SUMI_VEV_GLOBAL_BEND,       // value = bend in semitones (classic/master shear)
    SUMI_VEV_PAPER_DIP          // CC64 rising edge, or ABI call
} sumi_voice_event_kind_t;

typedef struct {
    sumi_voice_event_kind_t kind;
    uint32_t voice_id;
    uint32_t dimension;   // sumi_ctl_t for GLOBAL_CTL
    float    x, y;        // primary position (== echo 0)
    float    ax, ay;      // VOICE_BEGIN: pitch axis (unit dir × semitone step)
    float    value;       // strike / semitones / pressure / timbre / lift / ctl
    // §3.4 echo sets (VOICE_BEGIN / VOICE_MIGRATE): all canvas sites of the
    // note under the active layout. echo_count is 1..SUMI_MAX_ECHOES.
    float    ex[SUMI_MAX_ECHOES], ey[SUMI_MAX_ECHOES];
    uint32_t echo_count;
} sumi_voice_event_t;

typedef struct sumi_voice_mapper_t sumi_voice_mapper_t;

sumi_voice_mapper_t* sumi_voice_mapper_create(sumi_log_fn log_cb, void* log_user);
void sumi_voice_mapper_destroy(sumi_voice_mapper_t* vm);

// Stage 1: musical -> §3.3 vocabulary. `zone` classifies MPE member channels.
// `now` is the engine's monotonic clock; `dropped_count` is the ring's
// overflow counter (§3.1): the first increment arms per-voice inactivity
// timeouts (~10 s without any message for a voice while other traffic flows
// -> synthetic VoiceEnd, lift 0, logged). Never armed in normal operation.
uint32_t sumi_voice_mapper_normalize(sumi_voice_mapper_t* vm,
                                     double now, uint32_t dropped_count,
                                     const sumi_midi_event_t* in, uint32_t in_count,
                                     sumi_input_mode_t mode, sumi_mpe_zone_t zone,
                                     const sumi_params_t* params, float aspect,
                                     sumi_voice_event_t* out, uint32_t max);

// Stage 2: §3.3 vocabulary -> deformation queue, then the per-frame voice
// tick (smoothing, §4.4 press feeds, glide tines) under the deformation
// budget. Call exactly once per sumi_update with that frame's dt.
// `drop_counter` is the instance's global §4.2 drop counter.
// `dip_allowed` reflects the §5.3 double-buffer state (engine queries the
// renderer): a PaperDip event with dip_allowed == false is refused with a
// warning log — no reset, no counter rebase. An accepted dip pushes the RESET
// pass and REBASES the drop counter to 0 (§4.2 aux half-float headroom).
void sumi_voice_mapper_lower(sumi_voice_mapper_t* vm,
                             const sumi_voice_event_t* events, uint32_t count,
                             double dt, const sumi_params_t* params,
                             bool dip_allowed,
                             uint32_t* drop_counter,
                             sumi_deform_queue_t* queue);

// CC routing table (§2.2, §5.3): channel 0xFF = any channel; a channel-
// specific mapping overrides an any-channel one. Defaults documented in
// README.md; sumi_clear_cc_map removes everything including the defaults.
void sumi_voice_mapper_map_cc(sumi_voice_mapper_t* vm, uint8_t channel,
                              uint8_t cc, sumi_ctl_t target);
void sumi_voice_mapper_clear_cc_map(sumi_voice_mapper_t* vm);

// Smoothed global-control value (render thread; e.g. roughness/morph for the
// composite).
float sumi_voice_mapper_ctl(const sumi_voice_mapper_t* vm, sumi_ctl_t dim);

// Per-frame deformation budget (§3.4). Default 64; overridable for tests.
void sumi_voice_mapper_set_budget(sumi_voice_mapper_t* vm, uint32_t budget);
// Emissions merged into a later frame because the budget was exhausted.
uint32_t sumi_voice_mapper_merged_count(const sumi_voice_mapper_t* vm);

#ifdef __cplusplus
}
#endif
