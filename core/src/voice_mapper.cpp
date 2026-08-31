// voice_mapper.cpp — musical -> spatial mapping (PROJECT_SPEC.md §2.1, §2.4,
// §3.3, §3.4, §4.4).
// Classic mode: every note is its own voice with strike only; bend/mod act
// globally. MPE mode: one voice per member channel (newest note steals),
// per-voice press/glide/slide with exponential smoothing and per-frame
// coalescing, sustained pressure as incremental drop expansions (§4.4), and
// a global per-frame deformation budget with overflow merging (§3.4).
#include "voice_mapper.h"
#include "ink_phase.h"
#include "log_levels.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Classic-mode tuning (canvas-height units / radians).
static const float DROP_RADIUS_MIN   = 0.020f;   // strike -> radius: r = min + span*sqrt(strike)
static const float DROP_RADIUS_SPAN  = 0.075f;   // (§3.4: ink *area* tracks velocity)
static const float SHEAR_ALPHA       = 0.45f;    // broad tine = shear-like (§2.4)
static const float SHEAR_PER_SEMI    = 0.015f;   // tine magnitude per semitone of bend

// Global-control tuning (§2.2). The vortex is dt-scaled: a per-frame pass of
// strength*VORTEX_RATE*dt radians, damped by viscosity.
static const float VORTEX_RATE       = 6.0f;     // rad/s at ctl = 1, viscosity 0
static const float VORTEX_RADIUS     = 0.35f;
static const float VORTEX_MIN_EMIT   = 0.0006f;  // radians; below: skip the pass
static const float VISCOSITY_DAMP    = 0.85f;    // damping share at viscosity = 1

// Wind-mode tuning (§2.3). The brush maintains a breath-proportional line
// WIDTH (relaxing toward it) rather than integrating flow without bound like
// MPE press — that is what draws a line instead of a blob (DECISIONS.md).
static const float MIGRATE_TINE_ALPHA = 0.035f;  // wake of the wandering brush
static const float WIND_WIDTH_MIN     = 0.006f;  // brush radius at breath ~0
static const float WIND_WIDTH_SPAN    = 0.050f;  // added radius at breath 1
static const float WIND_STRIKE_SPAN   = 0.020f;  // initial thin touch-down

// MPE per-voice tuning (§3.4, §4.4).
static const float FEED_RATE         = 0.055f;   // boundary growth/s at press=1, expansion_rate=1
static const float FEED_MIN_GROW     = 0.0004f;  // accumulate below this (merge tiny steps)
static const float FEED_MAX_EMIT     = 0.050f;   // clamp one expansion pass (post-starvation)
static const float PRESS_DEADZONE    = 0.01f;
static const float GLIDE_TINE_ALPHA  = 0.030f;   // narrow: local wake, never a shear
static const float GLIDE_MIN_MOVE    = 0.0015f;
static const float SEMITONE_STEP_MAX = 0.030f;   // cap pitch-axis distance per semitone
static const float LIFT_RING_BASE    = 0.006f;   // faint surfactant ring (§3.4 lift)
static const float LIFT_RING_SPAN    = 0.030f;

// Circle-of-fifths radial layout (§3.4).
static const float COF_R_OUTER = 0.42f;   // low notes outer
static const float COF_R_INNER = 0.10f;   // high notes inner

#define SUMI_DEFAULT_DEFORM_BUDGET 64u   // §3.4 deform passes per frame
#define SUMI_MAX_VOICES 16u              // one per MIDI channel (§2.1)

// MPE voice dynamics — indexed by member channel (voice_id == channel).
struct sumi_mpe_voice_t {
    bool  active;
    float base_x, base_y;    // position at note-on
    float cur_x, cur_y;      // current center (dragged by glide)
    float ax, ay;            // pitch axis: unit dir × one-semitone distance
    float phase_base;        // §4.2 band of this voice's ink
    float aux_base;          // raw drop counter at note-on
    float press_t, press_s;  // target / smoothed (§3.4 smoothing)
    float slide_t, slide_s;
    float glide_t, glide_s;  // semitones
    float nominal_radius;    // the drop's current boundary radius (grows with press)
    float pending_grow;      // merged §4.4 boundary-growth steps awaiting budget
    bool  wind_brush;        // §2.3 brush: radius relaxes toward width(breath)
};

// Stage-1 note bookkeeping — which note owns each channel (steal detection).
struct sumi_note_slot_t {
    bool    active;
    uint8_t note;
};

struct sumi_voice_mapper_t {
    sumi_log_fn log_cb;
    void*       log_user;

    // Classic-mode global state.
    float bend_semis;        // last applied global bend
    bool  sustain_down[16];  // CC64 edge detection per channel

    // Stage-1 state.
    sumi_note_slot_t notes[SUMI_MAX_VOICES];
    // Per-update coalescing slots (§3.4: one value per dimension per voice).
    bool  has_press[SUMI_MAX_VOICES];
    float press_val[SUMI_MAX_VOICES];
    bool  has_slide[SUMI_MAX_VOICES];
    float slide_val[SUMI_MAX_VOICES];
    bool  has_glide[SUMI_MAX_VOICES];
    float glide_val[SUMI_MAX_VOICES];
    bool  have_master_bend;
    float master_bend;
    bool  have_ctl[SUMI_CTL_COUNT];      // per-update GlobalCtl coalescing
    float ctl_val[SUMI_CTL_COUNT];

    // CC routing (§2.2): -1 = unmapped; per-channel overrides any-channel.
    int8_t cc_map_any[128];
    int8_t cc_map_ch[16][128];

    int   last_mode;         // previous input mode (-1 = none yet)

    // Stage-2 state.
    sumi_mpe_voice_t voices[SUMI_MAX_VOICES];
    float ctl_t[SUMI_CTL_COUNT];         // global field controls, target
    float ctl_s[SUMI_CTL_COUNT];         // smoothed (§3.4)
    uint32_t budget;         // deform emissions per lower() call
    uint32_t frame_emitted;
    uint32_t merged_total;   // emissions deferred by budget exhaustion
    uint32_t merged_last_log;
    uint32_t frames;
};

static int8_t cc_lookup(const sumi_voice_mapper_t* vm, uint8_t ch, uint8_t cc) {
    const int8_t specific = vm->cc_map_ch[ch & 0x0F][cc & 0x7F];
    if (specific >= 0) return specific;
    return vm->cc_map_any[cc & 0x7F];
}

// Default bindings (documented in README.md): mod wheel and the Airwave
// dimensions. Every entry is remappable/erasable via sumi_map_cc /
// sumi_clear_cc_map (§2.2: Airwave CC numbers are user-configured device-side).
static void install_default_cc_map(sumi_voice_mapper_t* vm) {
    vm->cc_map_any[1]  = SUMI_CTL_VORTEX_STRENGTH;   // mod wheel (§2.4)
    vm->cc_map_any[2]  = SUMI_CTL_INK_FLOW;          // breath (§2.3)
    vm->cc_map_any[7]  = SUMI_CTL_INK_FLOW;          // volume = breath alias (wind)
    vm->cc_map_any[11] = SUMI_CTL_INK_FLOW;          // expression = breath alias
    vm->cc_map_any[20] = SUMI_CTL_VORTEX_STRENGTH;   // Airwave L-Raise
    vm->cc_map_any[21] = SUMI_CTL_VORTEX_X;          // Airwave Glide
    vm->cc_map_any[22] = SUMI_CTL_VORTEX_Y;
    vm->cc_map_any[23] = SUMI_CTL_VISCOSITY;         // Airwave R-Tilt
    vm->cc_map_any[24] = SUMI_CTL_PAPER_ROUGHNESS;   // Airwave Flex
    vm->cc_map_any[25] = SUMI_CTL_PALETTE_MORPH;
}

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

// Local pitch axis at `note`: direction toward note+1, scaled to the distance
// of one semitone, capped so a ±48-semitone glide stays on canvas
// (DECISIONS.md — the circle-of-fifths layout has no continuous pitch axis).
static void pitch_axis(uint8_t note, uint32_t layout, float aspect, float* ax, float* ay) {
    float x0, y0, x1, y1;
    sumi_pitch_to_position(note, layout, aspect, &x0, &y0);
    sumi_pitch_to_position(note < 127 ? (uint8_t)(note + 1) : (uint8_t)(note - 1),
                           layout, aspect, &x1, &y1);
    float dx = x1 - x0, dy = y1 - y0;
    if (note >= 127) { dx = -dx; dy = -dy; }
    const float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f) { *ax = SEMITONE_STEP_MAX; *ay = 0.0f; return; }
    const float step = len > SEMITONE_STEP_MAX ? SEMITONE_STEP_MAX : len;
    *ax = dx / len * step;
    *ay = dy / len * step;
}

sumi_voice_mapper_t* sumi_voice_mapper_create(sumi_log_fn log_cb, void* log_user) {
    sumi_voice_mapper_t* vm = (sumi_voice_mapper_t*)calloc(1, sizeof(sumi_voice_mapper_t));
    if (!vm) return nullptr;
    vm->log_cb = log_cb;
    vm->log_user = log_user;
    vm->budget = SUMI_DEFAULT_DEFORM_BUDGET;
    vm->last_mode = -1;
    memset(vm->cc_map_any, -1, sizeof(vm->cc_map_any));
    memset(vm->cc_map_ch, -1, sizeof(vm->cc_map_ch));
    install_default_cc_map(vm);
    // Global control rest values: vortex centered, calm.
    vm->ctl_t[SUMI_CTL_VORTEX_X] = vm->ctl_s[SUMI_CTL_VORTEX_X] = 0.5f;
    vm->ctl_t[SUMI_CTL_VORTEX_Y] = vm->ctl_s[SUMI_CTL_VORTEX_Y] = 0.5f;
    return vm;
}

void sumi_voice_mapper_map_cc(sumi_voice_mapper_t* vm, uint8_t channel,
                              uint8_t cc, sumi_ctl_t target) {
    if (!vm || cc > 127 || target >= SUMI_CTL_COUNT) return;
    if (channel == 0xFF) vm->cc_map_any[cc] = (int8_t)target;
    else if (channel < 16) vm->cc_map_ch[channel][cc] = (int8_t)target;
}

void sumi_voice_mapper_clear_cc_map(sumi_voice_mapper_t* vm) {
    if (!vm) return;
    memset(vm->cc_map_any, -1, sizeof(vm->cc_map_any));
    memset(vm->cc_map_ch, -1, sizeof(vm->cc_map_ch));
}

void sumi_voice_mapper_destroy(sumi_voice_mapper_t* vm) {
    free(vm);
}

float sumi_voice_mapper_ctl(const sumi_voice_mapper_t* vm, sumi_ctl_t dim) {
    if (!vm || dim >= SUMI_CTL_COUNT) return 0.0f;
    return vm->ctl_s[dim];
}

void sumi_voice_mapper_set_budget(sumi_voice_mapper_t* vm, uint32_t budget) {
    if (vm && budget > 0) vm->budget = budget;
}

uint32_t sumi_voice_mapper_merged_count(const sumi_voice_mapper_t* vm) {
    return vm ? vm->merged_total : 0;
}

static uint32_t put(sumi_voice_event_t* out, uint32_t count, uint32_t max,
                    const sumi_voice_event_t* ev) {
    if (count < max) out[count] = *ev;
    return count + (count < max ? 1u : 0u);
}

/* ------------------------------------------------------------------ */
/* Stage 1: musical events -> §3.3 vocabulary                          */
/* ------------------------------------------------------------------ */

uint32_t sumi_voice_mapper_normalize(sumi_voice_mapper_t* vm,
                                     const sumi_midi_event_t* in, uint32_t in_count,
                                     sumi_input_mode_t mode, sumi_mpe_zone_t zone,
                                     const sumi_params_t* params, float aspect,
                                     sumi_voice_event_t* out, uint32_t max) {
    if (!vm || !out || (in_count > 0 && !in)) return 0;
    const uint32_t layout = params ? params->pitch_layout : 0u;
    const bool mpe = (mode == SUMI_INPUT_MPE);
    const bool wind = (mode == SUMI_INPUT_WIND);

    uint32_t count = 0;
    vm->have_master_bend = false;
    for (uint32_t v = 0; v < SUMI_MAX_VOICES; v++) {
        vm->has_press[v] = vm->has_slide[v] = vm->has_glide[v] = false;
    }
    for (uint32_t c = 0; c < SUMI_CTL_COUNT; c++) vm->have_ctl[c] = false;

    // Input mode changed (§2.5 window handed the bath to another dialect):
    // end every voice tracked under the old dialect so nothing keeps feeding.
    if ((int)mode != vm->last_mode) {
        const int prev = vm->last_mode;
        vm->last_mode = (int)mode;
        for (uint32_t chn = 0; chn < SUMI_MAX_VOICES; chn++) {
            if (!vm->notes[chn].active) continue;
            vm->notes[chn].active = false;
            sumi_voice_event_t end = {};
            end.kind = SUMI_VEV_VOICE_END;
            end.voice_id = (prev == (int)SUMI_INPUT_WIND) ? 0 : chn;
            end.value = 0.0f;   // silent set, no lift ring
            count = put(out, count, max, &end);
        }
    }

    for (uint32_t i = 0; i < in_count; i++) {
        const sumi_midi_event_t* m = &in[i];
        const uint8_t ch = m->channel & 0x0F;
        const bool member = mpe && zone.member_count > 0 &&
                            ch >= zone.first_member &&
                            ch < (uint8_t)(zone.first_member + zone.member_count);
        sumi_voice_event_t ev = {};
        switch (m->kind) {
            case SUMI_MEV_NOTE_ON: {
                if (wind) {
                    // §2.3: the single voice is a wandering ink brush. A
                    // legato note change MIGRATES the active drop's feed
                    // point instead of spawning a disconnected new drop.
                    if (vm->notes[0].active) {
                        vm->notes[0].note = m->a;
                        ev.kind = SUMI_VEV_VOICE_MIGRATE;
                        ev.voice_id = 0;
                        sumi_pitch_to_position(m->a, layout, aspect, &ev.x, &ev.y);
                        count = put(out, count, max, &ev);
                        break;
                    }
                    vm->notes[0].active = true;
                    vm->notes[0].note = m->a;
                    ev.kind = SUMI_VEV_VOICE_BEGIN;
                    ev.voice_id = 0;
                    ev.dimension = 1;   // marks the wind brush (see voice_mapper.h)
                    sumi_pitch_to_position(m->a, layout, aspect, &ev.x, &ev.y);
                    pitch_axis(m->a, layout, aspect, &ev.ax, &ev.ay);
                    ev.value = (float)m->b / 127.0f;
                    count = put(out, count, max, &ev);
                    break;
                }
                // Voice identity (§2.1): MPE keys voices by member channel —
                // newest note steals the channel's voice. Classic keys by
                // (channel, note).
                // Classic ids are offset past the MPE table range (0..15).
                const uint32_t vid = mpe ? ch : (0x1000u | ((uint32_t)ch << 8) | m->a);
                if (mpe) {
                    if (vm->notes[ch].active) {
                        sumi_voice_event_t steal = {};
                        steal.kind = SUMI_VEV_VOICE_END;
                        steal.voice_id = ch;
                        steal.value = 0.0f;   // stolen, not lifted
                        count = put(out, count, max, &steal);
                    }
                    vm->notes[ch].active = true;
                    vm->notes[ch].note = m->a;
                }
                ev.kind = SUMI_VEV_VOICE_BEGIN;
                ev.voice_id = vid;
                sumi_pitch_to_position(m->a, layout, aspect, &ev.x, &ev.y);
                pitch_axis(m->a, layout, aspect, &ev.ax, &ev.ay);
                ev.value = (float)m->b / 127.0f;   // strike
                count = put(out, count, max, &ev);
                break;
            }
            case SUMI_MEV_NOTE_OFF: {
                if (wind) {
                    // Only the current note ends the brush; offs of already-
                    // migrated-away notes (legato overlaps) are ignored.
                    if (!vm->notes[0].active || vm->notes[0].note != m->a) break;
                    vm->notes[0].active = false;
                    ev.voice_id = 0;
                } else if (mpe) {
                    // Off for a stolen (no longer owning) note: ignore.
                    if (!vm->notes[ch].active || vm->notes[ch].note != m->a) break;
                    vm->notes[ch].active = false;
                    ev.voice_id = ch;
                } else {
                    ev.voice_id = 0x1000u | ((uint32_t)ch << 8) | m->a;
                }
                ev.kind = SUMI_VEV_VOICE_END;
                ev.value = (float)m->b / 127.0f;   // lift
                count = put(out, count, max, &ev);
                break;
            }
            case SUMI_MEV_BEND: {
                if (member) {
                    // Per-note glide (§2.1) — coalesced, last value wins.
                    vm->has_glide[ch] = true;
                    vm->glide_val[ch] = m->f;
                } else {
                    // Master / classic: global shear (§2.4), coalesced.
                    vm->have_master_bend = true;
                    vm->master_bend = m->f;
                }
                break;
            }
            case SUMI_MEV_CHANNEL_PRESSURE: {
                if (wind) {
                    // §2.3: channel pressure aliases onto breath.
                    if (vm->notes[0].active) {
                        vm->has_press[0] = true;
                        vm->press_val[0] = (float)m->b / 127.0f;
                    }
                } else if (member && vm->notes[ch].active) {
                    vm->has_press[ch] = true;
                    vm->press_val[ch] = (float)m->b / 127.0f;
                }
                // Classic keyboards' channel pressure: ignored (§2.4).
                break;
            }
            case SUMI_MEV_CC: {
                if (m->a == 74 && member && vm->notes[ch].active) {
                    vm->has_slide[ch] = true;              // MPE slide wins over the map
                    vm->slide_val[ch] = (float)m->b / 127.0f;
                    break;
                }
                if (m->a == 64) {                          // sustain: reserved (paper dip)
                    const bool down = m->b >= 64;
                    if (down && !vm->sustain_down[ch]) {
                        ev.kind = SUMI_VEV_PAPER_DIP;
                        count = put(out, count, max, &ev);
                    }
                    vm->sustain_down[ch] = down;
                    break;
                }
                // §2.2 CC routing table; coalesced per dimension per update.
                const int8_t target = cc_lookup(vm, ch, m->a);
                if (target >= 0) {
                    const float value = (float)m->b / 127.0f;
                    if (wind && target == SUMI_CTL_INK_FLOW) {
                        // §2.3: breath feeds the single active voice.
                        if (vm->notes[0].active) {
                            vm->has_press[0] = true;
                            vm->press_val[0] = value;
                        }
                    } else {
                        vm->have_ctl[target] = true;
                        vm->ctl_val[target] = value;
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    // Emit the per-update coalesced continuous dimensions (§3.4).
    for (uint32_t v = 0; v < SUMI_MAX_VOICES; v++) {
        sumi_voice_event_t ev = {};
        ev.voice_id = v;
        if (vm->has_glide[v]) {
            ev.kind = SUMI_VEV_VOICE_GLIDE;
            ev.value = vm->glide_val[v];
            count = put(out, count, max, &ev);
        }
        if (vm->has_press[v]) {
            ev.kind = SUMI_VEV_VOICE_PRESS;
            ev.value = vm->press_val[v];
            count = put(out, count, max, &ev);
        }
        if (vm->has_slide[v]) {
            ev.kind = SUMI_VEV_VOICE_SLIDE;
            ev.value = vm->slide_val[v];
            count = put(out, count, max, &ev);
        }
    }
    if (vm->have_master_bend) {
        sumi_voice_event_t ev = {};
        ev.kind = SUMI_VEV_GLOBAL_BEND;
        ev.value = vm->master_bend;
        count = put(out, count, max, &ev);
    }
    for (uint32_t c = 0; c < SUMI_CTL_COUNT; c++) {
        if (!vm->have_ctl[c]) continue;
        sumi_voice_event_t ev = {};
        ev.kind = SUMI_VEV_GLOBAL_CTL;
        ev.dimension = c;
        ev.value = vm->ctl_val[c];
        count = put(out, count, max, &ev);
    }
    return count;
}

/* ------------------------------------------------------------------ */
/* Stage 2: §3.3 vocabulary -> deformations (budgeted)                 */
/* ------------------------------------------------------------------ */

// Budgeted push. Returns false when the frame budget is exhausted — the
// caller must keep its accumulator so the emission merges into a later frame.
static bool budget_push(sumi_voice_mapper_t* vm, sumi_deform_queue_t* queue,
                        const sumi_deform_t* d) {
    if (vm->frame_emitted >= vm->budget) {
        vm->merged_total++;
        return false;
    }
    if (!sumi_deform_queue_push(queue, d)) return false;
    vm->frame_emitted++;
    return true;
}

// Discrete events (note strikes, lift rings, dips) are never budget-dropped —
// §3.4's budget caps *continuous* deformation streams (glide tines, press
// feeds, global shear/vortex). Discrete pushes still count against the frame
// budget so continuous streams yield to them.
static void discrete_push(sumi_voice_mapper_t* vm, sumi_deform_queue_t* q,
                          const sumi_deform_t* d) {
    if (sumi_deform_queue_push(q, d)) vm->frame_emitted++;
}

static void emit_ink_drop(sumi_voice_mapper_t* vm, sumi_deform_queue_t* q,
                          float x, float y, float radius, float phase_base, float aux) {
    sumi_deform_t d;
    d.type = SUMI_DEFORM_DROP;
    d.as.drop.x = x;
    d.as.drop.y = y;
    d.as.drop.radius = radius;
    d.as.drop.phase_base = phase_base;
    d.as.drop.aux = aux;
    discrete_push(vm, q, &d);
}

void sumi_voice_mapper_lower(sumi_voice_mapper_t* vm,
                             const sumi_voice_event_t* events, uint32_t count,
                             double dt, const sumi_params_t* params,
                             uint32_t* drop_counter,
                             sumi_deform_queue_t* queue) {
    if (!vm || !events || !queue || !drop_counter) return;
    vm->frame_emitted = 0;
    vm->frames++;

    const float expansion_rate = params ? params->expansion_rate : 1.0f;
    float smoothing_ms = params ? params->smoothing_ms : 30.0f;
    if (smoothing_ms < 1.0f) smoothing_ms = 1.0f;
    float fdt = (float)dt;
    if (fdt <= 0.0f) fdt = 1.0f / 120.0f;
    if (fdt > 0.1f) fdt = 0.1f;
    // Exponential smoothing coefficient for this frame (§3.4).
    const float alpha = 1.0f - expf(-fdt * 1000.0f / smoothing_ms);

    for (uint32_t i = 0; i < count; i++) {
        const sumi_voice_event_t* ev = &events[i];
        switch (ev->kind) {
            case SUMI_VEV_VOICE_BEGIN: {
                // Strike -> initial drop, radius ∝ sqrt(velocity) (§3.4).
                // The wind brush touches down thin (§2.3).
                const bool wind_brush = (ev->dimension == 1);
                const float radius = wind_brush
                    ? WIND_WIDTH_MIN + WIND_STRIKE_SPAN * sqrtf(ev->value)
                    : DROP_RADIUS_MIN + DROP_RADIUS_SPAN * sqrtf(ev->value);
                const float aux = (float)*drop_counter;
                const float phase = sumi_next_ink_phase_base(drop_counter);
                emit_ink_drop(vm, queue, ev->x, ev->y, radius, phase, aux);
                if (ev->voice_id < SUMI_MAX_VOICES) {
                    sumi_mpe_voice_t* v = &vm->voices[ev->voice_id];
                    v->active = true;
                    v->base_x = v->cur_x = ev->x;
                    v->base_y = v->cur_y = ev->y;
                    v->ax = ev->ax;
                    v->ay = ev->ay;
                    v->phase_base = phase;
                    v->aux_base = aux;
                    v->press_t = v->press_s = 0.0f;
                    v->slide_t = v->slide_s = 0.0f;
                    v->glide_t = v->glide_s = 0.0f;
                    v->nominal_radius = radius;
                    v->pending_grow = 0.0f;
                    v->wind_brush = wind_brush;
                }
                break;
            }
            case SUMI_VEV_VOICE_END: {
                if (ev->voice_id < SUMI_MAX_VOICES && vm->voices[ev->voice_id].active) {
                    sumi_mpe_voice_t* v = &vm->voices[ev->voice_id];
                    v->active = false;
                    // Lift -> the drop "sets"; faint surfactant ring ∝ lift
                    // velocity (§3.4): a small clear-water expansion.
                    if (ev->value > 0.01f) {
                        sumi_deform_t d;
                        d.type = SUMI_DEFORM_DROP;
                        d.as.drop.x = v->cur_x;
                        d.as.drop.y = v->cur_y;
                        d.as.drop.radius = LIFT_RING_BASE + LIFT_RING_SPAN * ev->value;
                        d.as.drop.phase_base = 0.0f;   // clear surfactant
                        d.as.drop.aux = 0.0f;
                        discrete_push(vm, queue, &d);
                    }
                }
                break;
            }
            case SUMI_VEV_VOICE_GLIDE:
                if (ev->voice_id < SUMI_MAX_VOICES) vm->voices[ev->voice_id].glide_t = ev->value;
                break;
            case SUMI_VEV_VOICE_PRESS:
                if (ev->voice_id < SUMI_MAX_VOICES) vm->voices[ev->voice_id].press_t = ev->value;
                break;
            case SUMI_VEV_VOICE_SLIDE:
                if (ev->voice_id < SUMI_MAX_VOICES) vm->voices[ev->voice_id].slide_t = ev->value;
                break;
            case SUMI_VEV_GLOBAL_BEND: {
                const float delta = ev->value - vm->bend_semis;
                vm->bend_semis = ev->value;
                if (delta > -1e-4f && delta < 1e-4f) break;
                sumi_deform_t d;
                d.type = SUMI_DEFORM_TINE;
                d.as.tine.y0 = 0.5f;
                d.as.tine.y1 = 0.5f;
                if (delta >= 0.0f) { d.as.tine.x0 = 0.0f; d.as.tine.x1 = 1.0f; }
                else               { d.as.tine.x0 = 1.0f; d.as.tine.x1 = 0.0f; }
                d.as.tine.alpha = SHEAR_ALPHA;
                d.as.tine.magnitude = (delta >= 0.0f ? delta : -delta) * SHEAR_PER_SEMI;
                budget_push(vm, queue, &d);
                break;
            }
            case SUMI_VEV_VOICE_MIGRATE: {
                // §2.3: migrate the active drop's feed point, drawing a
                // tine-like wake from the old position to the new one.
                if (ev->voice_id < SUMI_MAX_VOICES && vm->voices[ev->voice_id].active) {
                    sumi_mpe_voice_t* v = &vm->voices[ev->voice_id];
                    const float dx = ev->x - v->cur_x, dy = ev->y - v->cur_y;
                    const float dist = sqrtf(dx * dx + dy * dy);
                    if (dist > 1e-4f) {
                        sumi_deform_t d;
                        d.type = SUMI_DEFORM_TINE;
                        d.as.tine.x0 = v->cur_x;
                        d.as.tine.y0 = v->cur_y;
                        d.as.tine.x1 = ev->x;
                        d.as.tine.y1 = ev->y;
                        d.as.tine.alpha = MIGRATE_TINE_ALPHA;
                        d.as.tine.magnitude = dist;
                        discrete_push(vm, queue, &d);   // a note event, never dropped
                    }
                    v->base_x = v->cur_x = ev->x;
                    v->base_y = v->cur_y = ev->y;
                    v->glide_t = v->glide_s = 0.0f;
                    // The new segment adopts the current breath width, so a
                    // quieter passage draws a thinner line again.
                    if (v->wind_brush) {
                        const float target = WIND_WIDTH_MIN + WIND_WIDTH_SPAN * v->press_s;
                        if (v->nominal_radius > target) v->nominal_radius = target;
                    }
                }
                break;
            }
            case SUMI_VEV_GLOBAL_CTL: {
                if (ev->dimension < SUMI_CTL_COUNT) {
                    vm->ctl_t[ev->dimension] = ev->value;   // smoothed in the tick
                }
                break;
            }
            case SUMI_VEV_PAPER_DIP: {
                sumi_deform_t d = { SUMI_DEFORM_RESET, {} };
                discrete_push(vm, queue, &d);
                break;
            }
            default:
                break;
        }
    }

    // Per-frame voice tick (§3.4 smoothing + §4.4 continuous feeds). Runs
    // after event application so this frame's targets are already in place.
    for (uint32_t vid = 0; vid < SUMI_MAX_VOICES; vid++) {
        sumi_mpe_voice_t* v = &vm->voices[vid];
        if (!v->active) continue;

        v->press_s += (v->press_t - v->press_s) * alpha;
        v->slide_s += (v->slide_t - v->slide_s) * alpha;
        v->glide_s += (v->glide_t - v->glide_s) * alpha;

        // Glide -> drag THIS drop's center along the pitch axis, emitting a
        // narrow tine along the drag path (§3.4 — per-voice, never global).
        const float tx = v->base_x + v->ax * v->glide_s;
        const float ty = v->base_y + v->ay * v->glide_s;
        const float mdx = tx - v->cur_x, mdy = ty - v->cur_y;
        const float move = sqrtf(mdx * mdx + mdy * mdy);
        if (move >= GLIDE_MIN_MOVE) {
            sumi_deform_t d;
            d.type = SUMI_DEFORM_TINE;
            d.as.tine.x0 = v->cur_x;
            d.as.tine.y0 = v->cur_y;
            d.as.tine.x1 = tx;
            d.as.tine.y1 = ty;
            d.as.tine.alpha = GLIDE_TINE_ALPHA;
            d.as.tine.magnitude = move;
            if (budget_push(vm, queue, &d)) {
                v->cur_x = tx;   // not updated on merge: motion carries over
                v->cur_y = ty;
            }
        }

        // Press -> sustained ink feed: small incremental expansions re-emitted
        // per frame at the voice's current center (§4.4). The accumulated step
        // is BOUNDARY growth (§3.4: the drop expands at a rate ∝ pressure); a
        // center expansion of radius r moves an existing boundary R only to
        // sqrt(R² + r²), so the emitted pass radius converts via the area
        // relation r = sqrt((R+ΔR)² − R²) (see DECISIONS.md). Steps below the
        // threshold (or over budget) accumulate and merge.
        if (v->press_s > PRESS_DEADZONE) {
            float g = v->press_s * fdt * expansion_rate * FEED_RATE;
            if (v->wind_brush) {
                // §2.3 brush: only grow toward the breath-proportional width.
                const float target = WIND_WIDTH_MIN + WIND_WIDTH_SPAN * v->press_s;
                const float room = target - (v->nominal_radius + v->pending_grow);
                if (room < g) g = room > 0.0f ? room : 0.0f;
            }
            v->pending_grow += g;
        }
        if (v->pending_grow >= FEED_MIN_GROW) {
            const float R = v->nominal_radius;
            float grow = v->pending_grow;
            float r_emit = sqrtf((R + grow) * (R + grow) - R * R);
            if (r_emit > FEED_MAX_EMIT) {
                r_emit = FEED_MAX_EMIT;
                grow = sqrtf(R * R + r_emit * r_emit) - R;   // growth actually applied
            }
            sumi_deform_t d;
            d.type = SUMI_DEFORM_DROP;
            d.as.drop.x = v->cur_x;
            d.as.drop.y = v->cur_y;
            d.as.drop.radius = r_emit;
            d.as.drop.phase_base = v->phase_base;              // same band: the drop GROWS
            d.as.drop.aux = v->aux_base + v->slide_s * 0.9f;   // slide -> aux modulation
            if (budget_push(vm, queue, &d)) {
                v->pending_grow -= grow;
                v->nominal_radius = R + grow;
            }
        }
    }

    // Global field controls (§2.2): smooth, then run the per-frame agitation.
    for (uint32_t c = 0; c < SUMI_CTL_COUNT; c++) {
        vm->ctl_s[c] += (vm->ctl_t[c] - vm->ctl_s[c]) * alpha;
    }
    {
        // Vortex: dt-scaled, damped by viscosity (§2.2 "fluid viscosity /
        // damping"). Roughness/palette-morph are smoothed here but only
        // consumed by the composite in a later step (DECISIONS.md).
        const float damping = 1.0f - VISCOSITY_DAMP * vm->ctl_s[SUMI_CTL_VISCOSITY];
        const float theta = vm->ctl_s[SUMI_CTL_VORTEX_STRENGTH] * VORTEX_RATE * fdt * damping;
        if (theta > VORTEX_MIN_EMIT) {
            sumi_deform_t d;
            d.type = SUMI_DEFORM_VORTEX;
            d.as.vortex.x = vm->ctl_s[SUMI_CTL_VORTEX_X];
            d.as.vortex.y = vm->ctl_s[SUMI_CTL_VORTEX_Y];
            d.as.vortex.strength = theta;
            d.as.vortex.radius = VORTEX_RADIUS;
            budget_push(vm, queue, &d);
        }
    }

    // Budget diagnostics (throttled): visible with a log callback attached.
    if (vm->frames % 300 == 0 && vm->merged_total != vm->merged_last_log) {
        if (vm->log_cb) {
            char buf[96];
            snprintf(buf, sizeof(buf), "mapper: deform budget merged %u emissions so far",
                     vm->merged_total);
            vm->log_cb(SUMI_LOG_INFO, buf, vm->log_user);
        }
        vm->merged_last_log = vm->merged_total;
    }
}

} // extern "C"
