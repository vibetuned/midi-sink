// engine.cpp — instance state, lifecycle, param handling (PROJECT_SPEC.md §1, §5).
// This layer never touches sokol; all GPU work goes through renderer.h.
#include "sumi_core.h"
#include "log_levels.h"
#include "renderer.h"
#include "displacement.h"
#include "midi_normalizer.h"
#include "voice_mapper.h"
#include "ink_phase.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Per-frame deformation-pass capacity. §3.4's per-frame deform budget (later
// step) is far below this; the headroom exists for the step-2 stress mode.
#define SUMI_DEFORM_QUEUE_CAPACITY 4096u
// Per-update MIDI/voice event batch size (matches the SPSC ring capacity, so
// one update can always fully drain a worst-case backlog).
#define SUMI_EVENT_BATCH 4096u

struct sumi_instance_t {
    sumi_config_t        config;   // log_cb/log_user kept for the instance lifetime
    sumi_params_t        params;
    sumi_renderer_t*     renderer;
    sumi_deform_queue_t* deforms;
    sumi_normalizer_t*   normalizer;
    sumi_voice_mapper_t* mapper;
    sumi_midi_event_t*   mev_buf;    // SUMI_EVENT_BATCH entries
    sumi_voice_event_t*  vev_buf;    // SUMI_EVENT_BATCH entries
    uint32_t             stress_swaps;   // SUMI_STRESS_SWAPS test hook (DECISIONS.md)
    uint32_t             drop_counter;   // §4.2 global monotonic drop counter
    double               clock;          // monotonic time for §2.5 activity windows
};

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static void log_msg(const sumi_config_t* cfg, int level, const char* msg) {
    if (cfg && cfg->log_cb) {
        cfg->log_cb(level, msg, cfg->log_user);
    }
}

static sumi_params_t default_params(void) {
    sumi_params_t p;
    p.fluid_viscosity   = 0.5f;
    p.expansion_rate    = 1.0f;
    p.paper_roughness   = 0.5f;
    p.smoothing_ms      = 30.0f;   // §3.4 default smoothing time constant
    p.active_palette_id = 0;       // sumi black
    p.pitch_layout      = 0;       // circle-of-fifths radial
    p.sim_scale         = 1.0f;
    return p;
}

extern "C" {

uint32_t sumi_version(void) {
    return (0u << 16) | (1u << 8) | 0u;   // 0.1.0
}

sumi_instance_t* sumi_create(const sumi_config_t* config) {
    if (!config) {
        return NULL;   // no config, no log callback to report through
    }
    if (config->backend != SUMI_BACKEND_METAL) {
        log_msg(config, SUMI_LOG_ERROR, "sumi_create: only SUMI_BACKEND_METAL is supported in phase 1");
        return NULL;
    }
    if (!config->native_surface_handle) {
        log_msg(config, SUMI_LOG_ERROR, "sumi_create: METAL backend requires a CAMetalLayer* surface handle");
        return NULL;
    }
    if (config->width == 0 || config->height == 0) {
        log_msg(config, SUMI_LOG_ERROR, "sumi_create: width/height must be non-zero");
        return NULL;
    }

    sumi_instance_t* inst = (sumi_instance_t*)calloc(1, sizeof(sumi_instance_t));
    if (!inst) {
        log_msg(config, SUMI_LOG_ERROR, "sumi_create: out of memory");
        return NULL;
    }
    inst->config = *config;
    inst->params = default_params();

    inst->deforms = sumi_deform_queue_create(SUMI_DEFORM_QUEUE_CAPACITY);
    inst->normalizer = sumi_normalizer_create(config->log_cb, config->log_user);
    inst->mapper = sumi_voice_mapper_create(config->log_cb, config->log_user);
    inst->mev_buf = (sumi_midi_event_t*)calloc(SUMI_EVENT_BATCH, sizeof(sumi_midi_event_t));
    inst->vev_buf = (sumi_voice_event_t*)calloc(SUMI_EVENT_BATCH, sizeof(sumi_voice_event_t));
    if (!inst->deforms || !inst->normalizer || !inst->mapper || !inst->mev_buf || !inst->vev_buf) {
        log_msg(config, SUMI_LOG_ERROR, "sumi_create: subsystem allocation failed");
        sumi_voice_mapper_destroy(inst->mapper);
        sumi_normalizer_destroy(inst->normalizer);
        sumi_deform_queue_destroy(inst->deforms);
        free(inst->mev_buf);
        free(inst->vev_buf);
        free(inst);
        return NULL;
    }

    inst->renderer = sumi_renderer_create(&inst->config, inst->params.sim_scale);
    if (!inst->renderer) {
        log_msg(config, SUMI_LOG_ERROR, "sumi_create: renderer initialization failed");
        sumi_voice_mapper_destroy(inst->mapper);
        sumi_normalizer_destroy(inst->normalizer);
        sumi_deform_queue_destroy(inst->deforms);
        free(inst->mev_buf);
        free(inst->vev_buf);
        free(inst);
        return NULL;
    }

    // Test hook (never set in production): force N ping-pong swaps per frame
    // to stress the machinery (step-2 DONE check; see DECISIONS.md).
    const char* stress = getenv("SUMI_STRESS_SWAPS");
    if (stress) {
        long n = strtol(stress, NULL, 10);
        if (n > 0) {
            inst->stress_swaps = (uint32_t)n;
            char buf[96];
            snprintf(buf, sizeof(buf), "sumi_create: SUMI_STRESS_SWAPS=%u active", inst->stress_swaps);
            log_msg(config, SUMI_LOG_WARN, buf);
        }
    }

    log_msg(config, SUMI_LOG_INFO, "sumi_create: instance ready (Metal)");
    return inst;
}

void sumi_destroy(sumi_instance_t* inst) {
    if (!inst) return;
    sumi_renderer_destroy(inst->renderer);
    sumi_voice_mapper_destroy(inst->mapper);
    sumi_normalizer_destroy(inst->normalizer);
    sumi_deform_queue_destroy(inst->deforms);
    free(inst->mev_buf);
    free(inst->vev_buf);
    log_msg(&inst->config, SUMI_LOG_INFO, "sumi_destroy: instance destroyed");
    free(inst);
}

void sumi_resize(sumi_instance_t* inst, uint32_t w, uint32_t h, float pixel_ratio) {
    if (!inst || w == 0 || h == 0) return;
    inst->config.width = w;
    inst->config.height = h;
    inst->config.pixel_ratio = pixel_ratio;
    sumi_renderer_resize(inst->renderer, w, h, pixel_ratio);
}

void sumi_update(sumi_instance_t* inst, double delta_time) {
    if (!inst) return;

    // §3.1/§5.2: drain the SPSC ring on the render thread, decode statefully,
    // map to the §3.3 vocabulary, lower to deformation passes.
    if (delta_time > 0.0 && delta_time < 1.0) inst->clock += delta_time;
    else inst->clock += 1.0 / 120.0;
    const uint32_t n_midi = sumi_normalizer_drain(inst->normalizer, inst->clock,
                                                  inst->mev_buf, SUMI_EVENT_BATCH);
    const float aspect = (inst->config.height > 0)
        ? (float)inst->config.width / (float)inst->config.height : 1.0f;
    const uint32_t n_voice = sumi_voice_mapper_normalize(
        inst->mapper, inst->mev_buf, n_midi,
        sumi_normalizer_mode(inst->normalizer),
        sumi_normalizer_zone(inst->normalizer),
        &inst->params, aspect, inst->vev_buf, SUMI_EVENT_BATCH);
    // Lower runs every frame even with no events: it owns the per-voice tick
    // (§3.4 smoothing, §4.4 sustained-pressure feeds).
    sumi_voice_mapper_lower(inst->mapper, inst->vev_buf, n_voice, delta_time,
                            &inst->params, &inst->drop_counter, inst->deforms);

    for (uint32_t i = 0; i < inst->stress_swaps; i++) {
        sumi_deform_t d = { SUMI_DEFORM_PASSTHROUGH, {} };
        sumi_deform_queue_push(inst->deforms, &d);
    }
}

void sumi_render(sumi_instance_t* inst) {
    if (!inst) return;
    sumi_renderer_render(inst->renderer, inst->deforms);
    sumi_deform_queue_clear(inst->deforms);
}

/* ------------------------------------------------------------------ */
/* Stubs — exported from day one so the shared library carries the     */
/* full ABI contract (roadmap step 1); implemented in later steps.     */
/* ------------------------------------------------------------------ */

uint32_t sumi_dropped_midi_count(sumi_instance_t* inst) {
    return inst ? sumi_normalizer_dropped(inst->normalizer) : 0;
}

/* §5.2: exactly one producer thread, wait-free, concurrent with the render
 * thread. */
void sumi_push_midi(sumi_instance_t* inst, uint8_t status, uint8_t data1, uint8_t data2) {
    if (!inst) return;
    sumi_normalizer_push(inst->normalizer, status, data1, data2);
}

void sumi_set_params(sumi_instance_t* inst, const sumi_params_t* params) {
    if (!inst || !params) return;
    inst->params = *params;
    // §4.1: keep sim_scale inside (0, 2].
    if (inst->params.sim_scale <= 0.0f) inst->params.sim_scale = 1.0f;
    if (inst->params.sim_scale > 2.0f)  inst->params.sim_scale = 2.0f;
    sumi_renderer_set_sim_scale(inst->renderer, inst->params.sim_scale);
}

void sumi_get_params(sumi_instance_t* inst, sumi_params_t* out) {
    if (!inst || !out) return;
    *out = inst->params;
}

void sumi_set_input_mode(sumi_instance_t* inst, sumi_input_mode_t mode) {
    if (!inst) return;
    sumi_normalizer_set_mode(inst->normalizer, mode);
}

void sumi_map_cc(sumi_instance_t* inst, uint8_t channel, uint8_t cc, sumi_ctl_t target) {
    if (!inst) return;
    sumi_voice_mapper_map_cc(inst->mapper, channel, cc, target);
}

void sumi_clear_cc_map(sumi_instance_t* inst) {
    if (!inst) return;
    sumi_voice_mapper_clear_cc_map(inst->mapper);
}

void sumi_trigger_paper_dip(sumi_instance_t* inst) {
    if (!inst) return;
    // Step-4 stub: UV reset to identity. Snapshot/readback lands with
    // sumi_read_print in a later step.
    sumi_deform_t d = { SUMI_DEFORM_RESET, {} };
    sumi_deform_queue_push(inst->deforms, &d);
}

bool sumi_read_print(sumi_instance_t* inst, uint8_t* pixels, size_t capacity,
                     uint32_t* out_w, uint32_t* out_h) {
    (void)inst; (void)pixels; (void)capacity; (void)out_w; (void)out_h;
    return false;   // no print exists yet
}

void sumi_add_drop(sumi_instance_t* inst, float x, float y, float radius, uint32_t layer_type) {
    if (!inst || radius <= 0.0f) return;
    sumi_deform_t d;
    d.type = SUMI_DEFORM_DROP;
    d.as.drop.x = clamp01(x);
    d.as.drop.y = clamp01(y);
    d.as.drop.radius = radius;
    // layer_type 0 = ink (counter-derived phase, §4.2); anything else = clear
    // water/surfactant: expands the field but its interior stays un-inked.
    if (layer_type == 0) {
        d.as.drop.aux = (float)inst->drop_counter;
        d.as.drop.phase_base = sumi_next_ink_phase_base(&inst->drop_counter);
    } else {
        d.as.drop.aux = 0.0f;
        d.as.drop.phase_base = 0.0f;
    }
    sumi_deform_queue_push(inst->deforms, &d);
}

void sumi_add_tine(sumi_instance_t* inst, float x0, float y0, float x1, float y1,
                   float alpha, float magnitude) {
    if (!inst || alpha <= 0.0f || magnitude == 0.0f) return;
    const float dx = x1 - x0, dy = y1 - y0;
    if (dx * dx + dy * dy < 1e-12f) return;   // degenerate direction
    sumi_deform_t d;
    d.type = SUMI_DEFORM_TINE;
    d.as.tine.x0 = x0; d.as.tine.y0 = y0;
    d.as.tine.x1 = x1; d.as.tine.y1 = y1;
    d.as.tine.alpha = alpha;
    d.as.tine.magnitude = magnitude;
    sumi_deform_queue_push(inst->deforms, &d);
}

void sumi_add_vortex(sumi_instance_t* inst, float x, float y, float strength, float radius) {
    if (!inst || radius <= 0.0f || strength == 0.0f) return;
    sumi_deform_t d;
    d.type = SUMI_DEFORM_VORTEX;
    d.as.vortex.x = clamp01(x);
    d.as.vortex.y = clamp01(y);
    d.as.vortex.strength = strength;
    d.as.vortex.radius = radius;
    sumi_deform_queue_push(inst->deforms, &d);
}

} // extern "C"
