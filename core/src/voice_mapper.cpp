// voice_mapper.cpp — musical -> spatial mapping (PROJECT_SPEC.md §3.4, §2.4).
// Classic mode (step 4): every note is its own voice with strike only; pitch
// bend and mod wheel act globally; CC64 dips the paper. MPE/wind voice
// dynamics arrive in later steps.
#include "voice_mapper.h"
#include "ink_phase.h"
#include "log_levels.h"

#include <math.h>
#include <stdlib.h>

// Classic-mode tuning (canvas-height units / radians).
static const float DROP_RADIUS_MIN   = 0.020f;   // strike -> radius: r = min + span*sqrt(strike)
static const float DROP_RADIUS_SPAN  = 0.075f;   // (§3.4: ink *area* tracks velocity)
static const float SHEAR_ALPHA       = 0.45f;    // broad tine = shear-like (§2.4)
static const float SHEAR_PER_SEMI    = 0.015f;   // tine magnitude per semitone of bend
static const float MOD_VORTEX_MAX    = 0.12f;    // radians per update at mod = 127
static const float MOD_VORTEX_RADIUS = 0.35f;

// Circle-of-fifths radial layout (§3.4).
static const float COF_R_OUTER = 0.42f;   // low notes outer
static const float COF_R_INNER = 0.10f;   // high notes inner

struct sumi_voice_mapper_t {
    sumi_log_fn log_cb;
    void*       log_user;
    // Classic-mode global state.
    float bend_semis;        // last applied global bend
    bool  sustain_down[16];  // CC64 edge detection per channel
    // Per-update coalescing (§3.4: at most one value per dimension per update).
    bool  have_mod;
    float mod_value;
};

extern "C" {

void sumi_pitch_to_position(uint8_t note, uint32_t layout, float aspect,
                            float* out_x, float* out_y) {
    const int pc = note % 12;
    const int octave = note / 12;               // 0..10 for MIDI 0..127
    if (aspect <= 0.0f) aspect = 1.0f;
    if (layout == 1) {
        // 12-column semitone grid: pitch class = column, octave = row
        // (low notes at the bottom).
        *out_x = ((float)pc + 0.5f) / 12.0f;
        *out_y = 1.0f - ((float)octave + 0.5f) / 11.0f;
    } else {
        // Circle-of-fifths angular layout: pc index around the circle,
        // octave -> radius (low outer, high inner). C at 12 o'clock.
        const int fifths = (pc * 7) % 12;
        const float angle = ((float)fifths / 12.0f) * 6.28318530718f - 1.57079632679f;
        const float t = (float)octave / 10.0f;
        const float r = COF_R_OUTER + (COF_R_INNER - COF_R_OUTER) * t;
        // r is in canvas-height units; divide x by aspect so the ring of
        // pitches is a circle on screen, not an ellipse.
        *out_x = 0.5f + (r * cosf(angle)) / aspect;
        *out_y = 0.5f + r * sinf(angle);
    }
}

sumi_voice_mapper_t* sumi_voice_mapper_create(sumi_log_fn log_cb, void* log_user) {
    sumi_voice_mapper_t* vm = (sumi_voice_mapper_t*)calloc(1, sizeof(sumi_voice_mapper_t));
    if (!vm) return nullptr;
    vm->log_cb = log_cb;
    vm->log_user = log_user;
    return vm;
}

void sumi_voice_mapper_destroy(sumi_voice_mapper_t* vm) {
    free(vm);
}

static uint32_t put(sumi_voice_event_t* out, uint32_t count, uint32_t max,
                    const sumi_voice_event_t* ev) {
    if (count < max) out[count] = *ev;
    return count + (count < max ? 1u : 0u);
}

uint32_t sumi_voice_mapper_normalize(sumi_voice_mapper_t* vm,
                                     const sumi_midi_event_t* in, uint32_t in_count,
                                     sumi_input_mode_t mode,
                                     const sumi_params_t* params, float aspect,
                                     sumi_voice_event_t* out, uint32_t max) {
    if (!vm || !in || !out) return 0;
    (void)mode;   // step 4: classic mapping for every mode; MPE/wind in step 5+
    const uint32_t layout = params ? params->pitch_layout : 0u;

    uint32_t count = 0;
    vm->have_mod = false;

    for (uint32_t i = 0; i < in_count; i++) {
        const sumi_midi_event_t* m = &in[i];
        sumi_voice_event_t ev = {};
        switch (m->kind) {
            case SUMI_MEV_NOTE_ON: {
                ev.kind = SUMI_VEV_VOICE_BEGIN;
                ev.voice_id = ((uint32_t)m->channel << 8) | m->a;
                sumi_pitch_to_position(m->a, layout, aspect, &ev.x, &ev.y);
                ev.value = (float)m->b / 127.0f;   // strike
                count = put(out, count, max, &ev);
                break;
            }
            case SUMI_MEV_NOTE_OFF: {
                ev.kind = SUMI_VEV_VOICE_END;
                ev.voice_id = ((uint32_t)m->channel << 8) | m->a;
                ev.value = (float)m->b / 127.0f;   // lift
                count = put(out, count, max, &ev);
                break;
            }
            case SUMI_MEV_BEND: {
                // Classic: bend is global (§2.4). Coalesced naturally by
                // "last one wins" — the lowering stage acts on the delta.
                ev.kind = SUMI_VEV_GLOBAL_BEND;
                ev.value = m->f;
                count = put(out, count, max, &ev);
                break;
            }
            case SUMI_MEV_CHANNEL_PRESSURE:
                // Classic keyboards rarely send this; ignored until MPE (step 5).
                break;
            case SUMI_MEV_CC: {
                if (m->a == 1) {
                    // Mod wheel -> vortex strength (§2.4); coalesce to one
                    // GlobalCtl per update (§3.4 rate limiting).
                    vm->have_mod = true;
                    vm->mod_value = (float)m->b / 127.0f;
                } else if (m->a == 64) {
                    const bool down = m->b >= 64;
                    if (down && !vm->sustain_down[m->channel]) {
                        ev.kind = SUMI_VEV_PAPER_DIP;
                        count = put(out, count, max, &ev);
                    }
                    vm->sustain_down[m->channel] = down;
                }
                // Other CCs: routed via sumi_map_cc in a later step.
                break;
            }
            default:
                break;
        }
    }

    if (vm->have_mod) {
        sumi_voice_event_t ev = {};
        ev.kind = SUMI_VEV_GLOBAL_CTL;
        ev.dimension = SUMI_CTL_VORTEX_STRENGTH;
        ev.value = vm->mod_value;
        count = put(out, count, max, &ev);
    }
    return count;
}

void sumi_voice_mapper_lower(sumi_voice_mapper_t* vm,
                             const sumi_voice_event_t* events, uint32_t count,
                             uint32_t* drop_counter,
                             sumi_deform_queue_t* queue) {
    if (!vm || !events || !queue || !drop_counter) return;
    for (uint32_t i = 0; i < count; i++) {
        const sumi_voice_event_t* ev = &events[i];
        sumi_deform_t d;
        switch (ev->kind) {
            case SUMI_VEV_VOICE_BEGIN: {
                // Strike -> initial drop radius, perceptually scaled:
                // radius ∝ sqrt(velocity) so ink AREA tracks velocity (§3.4).
                d.type = SUMI_DEFORM_DROP;
                d.as.drop.x = ev->x;
                d.as.drop.y = ev->y;
                d.as.drop.radius = DROP_RADIUS_MIN + DROP_RADIUS_SPAN * sqrtf(ev->value);
                d.as.drop.aux = (float)*drop_counter;
                d.as.drop.phase_base = sumi_next_ink_phase_base(drop_counter);
                sumi_deform_queue_push(queue, &d);
                break;
            }
            case SUMI_VEV_GLOBAL_BEND: {
                const float delta = ev->value - vm->bend_semis;
                vm->bend_semis = ev->value;
                if (delta > -1e-4f && delta < 1e-4f) break;
                // Global shear tine (§2.4): broad horizontal comb through the
                // canvas center, magnitude ∝ bend delta.
                d.type = SUMI_DEFORM_TINE;
                d.as.tine.y0 = 0.5f;
                d.as.tine.y1 = 0.5f;
                if (delta >= 0.0f) { d.as.tine.x0 = 0.0f; d.as.tine.x1 = 1.0f; }
                else               { d.as.tine.x0 = 1.0f; d.as.tine.x1 = 0.0f; }
                d.as.tine.alpha = SHEAR_ALPHA;
                d.as.tine.magnitude = (delta >= 0.0f ? delta : -delta) * SHEAR_PER_SEMI;
                sumi_deform_queue_push(queue, &d);
                break;
            }
            case SUMI_VEV_GLOBAL_CTL: {
                if (ev->dimension == SUMI_CTL_VORTEX_STRENGTH && ev->value > 0.0f) {
                    d.type = SUMI_DEFORM_VORTEX;
                    d.as.vortex.x = 0.5f;
                    d.as.vortex.y = 0.5f;
                    d.as.vortex.strength = ev->value * MOD_VORTEX_MAX;
                    d.as.vortex.radius = MOD_VORTEX_RADIUS;
                    sumi_deform_queue_push(queue, &d);
                }
                break;
            }
            case SUMI_VEV_PAPER_DIP: {
                d.type = SUMI_DEFORM_RESET;   // step-4 stub: UV reset to identity
                sumi_deform_queue_push(queue, &d);
                break;
            }
            case SUMI_VEV_VOICE_END:
                // Classic: the drop simply "sets" (§3.4); lift ring is a
                // later-step refinement.
                break;
            default:
                break;
        }
    }
}

} // extern "C"
