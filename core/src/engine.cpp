// engine.cpp — instance state, lifecycle, param handling (PROJECT_SPEC.md §1, §5).
// This layer never touches sokol; all GPU work goes through renderer.h.
#include "sumi_core.h"
#include "log_levels.h"
#include "renderer.h"
#include "displacement.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Per-frame deformation-pass capacity. §3.4's per-frame deform budget (later
// step) is far below this; the headroom exists for the step-2 stress mode.
#define SUMI_DEFORM_QUEUE_CAPACITY 4096u

struct sumi_instance_t {
    sumi_config_t        config;   // log_cb/log_user kept for the instance lifetime
    sumi_params_t        params;
    sumi_renderer_t*     renderer;
    sumi_deform_queue_t* deforms;
    uint32_t             stress_swaps;   // SUMI_STRESS_SWAPS test hook (DECISIONS.md)
};

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
    if (!inst->deforms) {
        log_msg(config, SUMI_LOG_ERROR, "sumi_create: deformation queue allocation failed");
        free(inst);
        return NULL;
    }

    inst->renderer = sumi_renderer_create(&inst->config, inst->params.sim_scale);
    if (!inst->renderer) {
        log_msg(config, SUMI_LOG_ERROR, "sumi_create: renderer initialization failed");
        sumi_deform_queue_destroy(inst->deforms);
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
    sumi_deform_queue_destroy(inst->deforms);
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
    // MIDI drain and the normalizer arrive in later steps; today the queue is
    // only fed by the stress hook (and stays empty otherwise).
    (void)delta_time;
    if (!inst) return;
    for (uint32_t i = 0; i < inst->stress_swaps; i++) {
        sumi_deform_t d = { SUMI_DEFORM_PASSTHROUGH };
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
    (void)inst;
    return 0;
}

void sumi_push_midi(sumi_instance_t* inst, uint8_t status, uint8_t data1, uint8_t data2) {
    (void)inst; (void)status; (void)data1; (void)data2;
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
    (void)inst; (void)mode;
}

void sumi_map_cc(sumi_instance_t* inst, uint8_t channel, uint8_t cc, sumi_ctl_t target) {
    (void)inst; (void)channel; (void)cc; (void)target;
}

void sumi_clear_cc_map(sumi_instance_t* inst) {
    (void)inst;
}

void sumi_trigger_paper_dip(sumi_instance_t* inst) {
    (void)inst;
}

bool sumi_read_print(sumi_instance_t* inst, uint8_t* pixels, size_t capacity,
                     uint32_t* out_w, uint32_t* out_h) {
    (void)inst; (void)pixels; (void)capacity; (void)out_w; (void)out_h;
    return false;   // no print exists yet
}

void sumi_add_drop(sumi_instance_t* inst, float x, float y, float radius, uint32_t layer_type) {
    (void)inst; (void)x; (void)y; (void)radius; (void)layer_type;
}

void sumi_add_tine(sumi_instance_t* inst, float x0, float y0, float x1, float y1,
                   float alpha, float magnitude) {
    (void)inst; (void)x0; (void)y0; (void)x1; (void)y1; (void)alpha; (void)magnitude;
}

void sumi_add_vortex(sumi_instance_t* inst, float x, float y, float strength, float radius) {
    (void)inst; (void)x; (void)y; (void)strength; (void)radius;
}

} // extern "C"
