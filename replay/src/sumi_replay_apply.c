/* sumi_replay_apply.c — Phase 9 step 65 (DECISIONS_8 #23): a frame into an
 * instance. The one translation unit of the replay library that links the
 * core and the preset serializer: the state events are session JSON (the
 * presets' language), the gestures the core's calls. A consumer that drives
 * the sink itself never pulls this. */
#include "sumi_replay.h"
#include "sumi_preset.h"

#include <string.h>

void sumi_replay_apply_gesture(sumi_instance_t* inst, uint32_t kind, const float* a, uint32_t n) {
    if (!inst || !a) return;
    float v[SUMI_REPLAY_G_ARGS] = {0, 0, 0, 0, 0, 0};
    if (n > SUMI_REPLAY_G_ARGS) n = SUMI_REPLAY_G_ARGS;
    for (uint32_t i = 0; i < n; i++) v[i] = a[i];
    switch (kind) {
    case SUMI_REPLAY_G_TAP:       sumi_gesture_tap(inst, v[0], v[1], v[2]); break;
    case SUMI_REPLAY_G_PINCH:     sumi_gesture_pinch(inst, v[0], v[1], v[2], v[3], v[4]); break;
    case SUMI_REPLAY_G_TWIST:     sumi_gesture_twist(inst, v[0], v[1], v[2], v[3], (uint32_t)(v[4] + 0.5f)); break;
    case SUMI_REPLAY_G_PRESS:     (void)sumi_gesture_press(inst, v[0], v[1], v[2], v[3], v[4], (double)v[5]); break;
    case SUMI_REPLAY_G_PRESS_END: sumi_gesture_press_end(inst); break;
    case SUMI_REPLAY_G_TINE:      sumi_add_tine(inst, v[0], v[1], v[2], v[3], v[4], v[5]); break;
    case SUMI_REPLAY_G_WAKE:      sumi_add_wake(inst, v[0], v[1], v[2], v[3], v[4]); break;
    case SUMI_REPLAY_G_DROP:      sumi_add_drop(inst, v[0], v[1], v[2], (uint32_t)(v[3] + 0.5f)); break;
    case SUMI_REPLAY_G_VORTEX:    sumi_add_vortex(inst, v[0], v[1], v[2], v[3], (uint32_t)(v[4] + 0.5f)); break;
    case SUMI_REPLAY_G_PINCH_RAW: sumi_add_pinch(inst, v[0], v[1], v[2], v[3]); break;
    default: break;
    }
}

bool sumi_replay_apply_state(sumi_instance_t* inst, const char* json, size_t len, uint32_t flags) {
    if (!inst || !json) return false;
    sumi_params_t base; sumi_get_params(inst, &base);
    sumi_palette_t pal; sumi_get_palette(inst, &pal);
    sumi_preset_t p;
    sumi_preset_init(&p, &base, &pal);
    if (!sumi_preset_read(json, len, &p)) return false;
    sumi_set_params(inst, &p.params);                       /* as recorded, sim_scale included: the field's size and the physics */
    if (flags & SUMI_REPLAY_APPLY_PALETTE) sumi_set_palette(inst, &p.palette);
    sumi_set_input_mode(inst, (sumi_input_mode_t)(p.input_mode >= 1u && p.input_mode <= 3u ? p.input_mode : 1u));
    sumi_clear_cc_map(inst);
    for (uint32_t i = 0; i < p.cc_count && i < SUMI_PRESET_MAX_CC; i++) {
        if (p.cc[i].cc > 127u || p.cc[i].target >= (uint32_t)SUMI_CTL_COUNT) continue;   /* the sound's bus routes (>= 1000) are the shell's */
        sumi_map_cc(inst, p.cc[i].channel, p.cc[i].cc, (sumi_ctl_t)p.cc[i].target);
    }
    return true;
}

bool sumi_replay_begin(sumi_replay_t* r, sumi_instance_t* inst, uint32_t flags) {
    if (!r || !inst) return false;
    sumi_replay_rewind(r);
    size_t n = 0;
    const char* s = sumi_replay_session(r, &n);
    bool ok = true;
    if (s && n) ok = sumi_replay_apply_state(inst, s, n, flags);
    const sumi_replay_info_t* i = sumi_replay_info(r);
    if ((flags & SUMI_REPLAY_APPLY_SIZE) && i && i->width && i->height) sumi_resize(inst, i->width, i->height, i->pixel_ratio > 0.0f ? i->pixel_ratio : 1.0f);
    return ok;
}

typedef struct { sumi_instance_t* inst; uint32_t flags; sumi_replay_push_fn push; void* user; } step_ctx_t;

static void step_midi(void* u, uint8_t s, uint8_t d1, uint8_t d2, uint8_t src) {
    step_ctx_t* c = (step_ctx_t*)u;
    if (c->push) c->push(c->user, s, d1, d2, src);
}
static void step_gesture(void* u, uint32_t kind, const float* a, uint32_t n) {
    step_ctx_t* c = (step_ctx_t*)u;
    sumi_replay_apply_gesture(c->inst, kind, a, n);
}
static void step_state(void* u, const char* json, size_t len) {
    step_ctx_t* c = (step_ctx_t*)u;
    (void)sumi_replay_apply_state(c->inst, json, len, c->flags);
}
static void step_resize(void* u, uint32_t w, uint32_t h, float pr) {
    step_ctx_t* c = (step_ctx_t*)u;
    if ((c->flags & SUMI_REPLAY_APPLY_SIZE) && w && h) sumi_resize(c->inst, w, h, pr);
}
static void step_dip(void* u) {
    step_ctx_t* c = (step_ctx_t*)u;
    sumi_trigger_paper_dip(c->inst);
}

bool sumi_replay_step(sumi_replay_t* r, sumi_instance_t* inst, uint32_t flags, sumi_replay_push_fn push, void* user, double* dt_out) {
    if (!r || !inst) return false;
    step_ctx_t c = { inst, flags, push, user };
    const sumi_replay_sink_t sink = { step_midi, step_gesture, step_state, step_resize, step_dip };
    return sumi_replay_next(r, &sink, &c, dt_out, NULL);
}
