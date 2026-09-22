// engine.cpp — instance state, lifecycle, param handling (PROJECT_SPEC.md §1, §5).
// This layer never touches sokol; all GPU work goes through renderer.h.
#include "sumi_core.h"
#include "sumi_debug.h"
#include "log_levels.h"
#include "renderer.h"
#include "displacement.h"
#include "midi_normalizer.h"
#include "voice_mapper.h"
#include "layouts.h"
#include "ink_phase.h"

#include <math.h>
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
    sumi_palette_t       palette;    // 1.0.0: the custom palette (SUMI_PALETTE_CUSTOM), validated
    sumi_midi_event_t*   mev_buf;    // SUMI_EVENT_BATCH entries
    sumi_voice_event_t*  vev_buf;    // SUMI_EVENT_BATCH entries
    uint32_t             stress_swaps;   // SUMI_STRESS_SWAPS test hook (DECISIONS.md)
    uint32_t             drop_counter;   // §4.2 global monotonic drop counter
    double               clock;          // monotonic time for §2.5 activity windows
    double               last_dt;        // update dt, consumed by render (fade timing)
    float                dbg_lattice;    // dev only (sumi_debug_set_chladni_overlay): the plate guide's strength
    // step 43: the layout's display cells as the Chladni stir uses them (radius scaled by chladni_cell,
    // capped at the display size so the discs never overlap), uploaded to the renderer when they change
    float                cells[320][4];
    uint32_t             cell_count;
    uint32_t             cells_key_layout, cells_key_w, cells_key_h;
    float                cells_key_scale;
    bool                 cells_valid;
};

static float cells_scale_of(const sumi_params_t* p) {
    float sc = p->chladni_cell;
    if (sc < 0.5f) sc = 0.5f;
    if (sc > 1.5f) sc = 1.5f;
    if (p->chladni_mode == SUMI_CHLADNI_DISCS && sc > 1.0f) sc = 1.0f;   // exact discs never overlap: the display disc is the ceiling
    return sc;
}
// the cells the stir turns: layouts.cpp's display cells, the radius scaled
static uint32_t engine_cells(const sumi_instance_t* inst, float* out, uint32_t max_cells) {
    const float aspect = (inst->config.height > 0) ? (float)inst->config.width / (float)inst->config.height : 1.0f;
    const uint32_t n = sumi_layout_cells(inst->params.pitch_layout, &inst->params, aspect, out, max_cells);
    const float sc = cells_scale_of(&inst->params);
    for (uint32_t i = 0; i < n; i++) out[4u * i + 2u] *= sc;
    return n;
}
static void engine_sync_cells(sumi_instance_t* inst) {
    const float sc = cells_scale_of(&inst->params);
    if (inst->cells_valid && inst->cells_key_layout == inst->params.pitch_layout && inst->cells_key_w == inst->config.width &&
        inst->cells_key_h == inst->config.height && inst->cells_key_scale == sc) return;
    inst->cell_count = engine_cells(inst, &inst->cells[0][0], 320u);
    inst->cells_key_layout = inst->params.pitch_layout; inst->cells_key_w = inst->config.width; inst->cells_key_h = inst->config.height;
    inst->cells_key_scale = sc; inst->cells_valid = true;
    sumi_renderer_set_cells(inst->renderer, &inst->cells[0][0], inst->cell_count);
    float r_min = 0.0f;
    for (uint32_t i = 0; i < inst->cell_count; i++) if (r_min <= 0.0f || inst->cells[i][2] < r_min) r_min = inst->cells[i][2];
    sumi_voice_mapper_set_cells_rmin(inst->mapper, r_min);   // the emission floor's clock
}

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
    p.pitch_layout      = SUMI_LAYOUT_FIFTHS;
    p.sim_scale         = 1.0f;
    p.bpm               = 120.0f;  // host-supplied tempo (roll layouts)
    p.roll_speed        = 0.0625f; // canvas-lengths per beat: 16 beats (4 bars
                                   // of 4/4) of history span the canvas (§3.4)
    // v0.4
    p.slide_mode        = 0;       // CC74 -> per-drop aux (the v1 behavior)
    p.vortex_profile    = SUMI_VORTEX_EXPONENTIAL;
    p.ripple_bake       = 0;       // live: composite view displacement
    p.ripple_angle      = 0.0f;
    p.pinch_variant     = 0;       // Hamiltonian saddle
    p.bend_mode         = 0;       // note bend -> v1 glide drag
    p.press_mode        = 0;       // 0xD0 -> v1 ink feed
    p.wake_profile      = 0;       // v0.7: inviscid doublet (v0.4 behaviour)
    p.wake_spread       = 3.0f;    // v0.7: l/a for the viscous stroke
    p.torsion_sweep     = 0;       // v0.10: the note-on torsion sweep is opt-in until step 42
    p.chladni_cell      = 1.0f;    // v0.11: an eddy in every cell of the layout
    p.burst_age         = 4.0f;    // v0.12: the burst diffuses to four cores (92% of its eventual displacement)
    p.burst_life        = 0.8f;    // v0.12: over 0.8 s — blooms sharp, dies soft
    p.burst_order       = 2;       // v0.12: the quadrupole, until the binding tables pick m per note
    p.spark_stack       = 3;       // v0.13: k, 2k, 4k
    p.spark_profile     = 0;       // v0.13: triangle waves
    p.spark_shear       = 0.6f;    // v0.13: the episode's kick, of the strike radius
    p.spark_tau         = 0.25f;   // v0.13: the decay time constant — over in a second
    p.chirikov_kmax     = 1.0f;    // v0.14: a full throw is one step at Greene's threshold, near enough
    p.chirikov_periods  = 2;       // v0.14: two kick waves per canvas height
    p.chirikov_eps      = 0.5f;    // v0.14: the drift's scale
    p.medium            = SUMI_MEDIUM_SUMI;   // 1.0.0: suminagashi — the renderer of 0.x
    p.anod_glow         = 1.0f;    // 1.1.0: the strain-glow scale
    p.anod_pitch        = 1.0f / 144.0f;   // 1.1.0: the water grid's pitch at rest, canvas heights (10 texels at 1440)
    p.chladni_mode      = SUMI_CHLADNI_DISCS;   // 1.1.0 (step 43): exact discs; 1 = the blended field
    // 1.1.0: the Anod strike's order by pitch class — naturals the quadrupole,
    // accidentals three lobes; the author signs it by eye (MEDIUM §4).
    { static const uint32_t cls[12] = {2, 3, 2, 3, 2, 2, 3, 2, 3, 2, 3, 2}; for (int i = 0; i < 12; i++) p.burst_order_by_class[i] = cls[i]; }
    // 1.1.0: the modes rest at the MEDIUM's default (MEDIUM §4) — Sumi's table is the 0.x behaviour.
    p.bend_mode         = SUMI_MODE_MEDIUM_DEFAULT;
    p.slide_mode        = SUMI_MODE_MEDIUM_DEFAULT;
    p.press_mode        = SUMI_MODE_MEDIUM_DEFAULT;
    return p;
}

extern "C" {

uint32_t sumi_version(void) {
    // 0.2.0: sumi_params_t grew (layout enum, bpm, roll_speed) — the struct
    // has no size field by design, so the version gates host compatibility.
    // 0.3.0: + sumi_layout_probe / sumi_cell_info_t (Phase 4 play surfaces).
    // 0.4.0: the deformation operator batch (§4.3(3-6)) — params grew again
    // (slide_mode, vortex_profile, ripple_bake, ripple_angle), sumi_add_vortex
    // gained the profile argument (breaking), + sumi_add_wake, sumi_add_pinch.
    // 0.5.0: + SUMI_BACKEND_WEBGPU and sumi_webgpu_surface_t (Phase 5 §5, the
    // WebGPU seam — additive; nothing existing moved).
    // 0.8.0: two roll layouts added to sumi_layout_t (#64); #60-#63, #66 behaviour.
    // 0.9.0: sumi_ctl_t grew (swirl trio, two pinches, #69); vortex/swirl centre Y reversed at emit.
    // 0.10.0 (Phase 6 step 36): + SUMI_VORTEX_TORSION, SUMI_CTL_TORSION_K/_PHASE
    // (COUNT 16), params.torsion_sweep — additive, the wave torsion (DECISIONS_5).
    // 0.11.0 (Phase 6 step 37): + sumi_add_chladni, SUMI_CTL_CHLADNI_A/_B (COUNT 18),
    // params.chladni_cell — additive, the Chladni lattice.
    // 0.12.0 (Phase 6 step 38): + sumi_add_burst, params.burst_age/_life/_order
    // — additive, the viscous multipole burst.
    // 0.13.0 (Phase 6 step 39): + sumi_add_spark_shear, sumi_add_spark,
    // SUMI_CTL_SPARK_K (COUNT 19), params.spark_* , slide_mode 2, SUMI_DROP_NONE — additive.
    // 0.14.0 (Phase 6 step 40): + sumi_add_chirikov, SUMI_CTL_CHIRIKOV_K (COUNT 20),
    // params.chirikov_* — additive, the Chirikov standard map.
    // 1.0.0 (Phase 6 step 41): THE ONE BREAK — sumi_layout_probe gained the
    // layout-state argument, sumi_cell_info_t its flags, sumi_params_t its
    // medium, + sumi_set_palette / SUMI_PALETTE_CUSTOM, layouts 8..12 reserved
    // (the header's migration note, DECISIONS_5 #45). Additive growth only from here.
    // 1.1.0 (Phase 6 step 42): + params.anod_glow, params.burst_order_by_class,
    // params.anod_pitch, params.chladni_mode (step 43), the mode values 2/3 and
    // SUMI_MODE_MEDIUM_DEFAULT — the Anod medium.
    return (1u << 16) | (1u << 8) | 0u;
}

sumi_instance_t* sumi_create(const sumi_config_t* config) {
    if (!config) {
        return NULL;   // no config, no log callback to report through
    }
    // Backend/handle validation is the swapchain TU's job (it knows which
    // backend this build carries); the engine stays backend-agnostic. §5.1:
    // Metal and D3D11 both require a native surface handle (GL will not).
    if (config->backend != SUMI_BACKEND_GL && !config->native_surface_handle) {
        log_msg(config, SUMI_LOG_ERROR, "sumi_create: this backend requires a native surface handle");
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
    // 1.0.0: a sumi-like stand-in until the host sets a palette — thin warm
    // gray to pooled black along the depth axis, the sumi clear-water tone.
    {
        sumi_palette_t pal = {};
        pal.stop_count = 2;
        pal.stops[0].rgb[0] = 0.55f; pal.stops[0].rgb[1] = 0.50f; pal.stops[0].rgb[2] = 0.42f; pal.stops[0].position = 0.0f;
        pal.stops[1].rgb[0] = 0.012f; pal.stops[1].rgb[1] = 0.011f; pal.stops[1].rgb[2] = 0.013f; pal.stops[1].position = 1.0f;
        for (uint32_t i = 2; i < SUMI_PALETTE_MAX_STOPS; i++) pal.stops[i] = pal.stops[1];
        pal.depth_gamma = 1.0f; pal.depth_floor = 0.0f; pal.hue_drift = 0.3f;
        pal.clear_rgb[0] = 0.830f; pal.clear_rgb[1] = 0.815f; pal.clear_rgb[2] = 0.760f;
        inst->palette = pal;
    }

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

    log_msg(config, SUMI_LOG_INFO, "sumi_create: instance ready");
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
    inst->last_dt = delta_time;
    // §3.4 field motion: the scroll pass is emitted ONCE per frame, FIRST in
    // the queue and outside the deformation budget — it is field motion, not
    // an expressive event. bpm/roll_speed changes apply next frame.
    {
        float sdx = 0.0f, sdy = 0.0f;
        if (sumi_layout_field_motion(inst->params.pitch_layout, &inst->params,
                                     delta_time > 0.0 && delta_time < 1.0 ? delta_time : 0.0,
                                     &sdx, &sdy)) {
            sumi_deform_t d;
            d.type = SUMI_DEFORM_SCROLL;
            d.as.scroll.dx = sdx;
            d.as.scroll.dy = sdy;
            sumi_deform_queue_push(inst->deforms, &d);
        }
    }

    const uint32_t n_midi = sumi_normalizer_drain(inst->normalizer, inst->clock,
                                                  inst->mev_buf, SUMI_EVENT_BATCH);
    const float aspect = (inst->config.height > 0)
        ? (float)inst->config.width / (float)inst->config.height : 1.0f;
    const uint32_t n_voice = sumi_voice_mapper_normalize(
        inst->mapper, inst->clock, sumi_normalizer_dropped(inst->normalizer),
        inst->mev_buf, n_midi,
        sumi_normalizer_mode(inst->normalizer),
        sumi_normalizer_zone(inst->normalizer),
        &inst->params, aspect, inst->vev_buf, SUMI_EVENT_BATCH);
    // Lower runs every frame even with no events: it owns the per-voice tick
    // (§3.4 smoothing, §4.4 sustained-pressure feeds). dip_allowed reflects
    // the §5.3 double-buffer state.
    sumi_voice_mapper_lower(inst->mapper, inst->vev_buf, n_voice, delta_time,
                            &inst->params, sumi_renderer_dip_ready(inst->renderer),
                            &inst->drop_counter, inst->deforms);

    for (uint32_t i = 0; i < inst->stress_swaps; i++) {
        sumi_deform_t d = { SUMI_DEFORM_PASSTHROUGH, {} };
        sumi_deform_queue_push(inst->deforms, &d);
    }
}

void sumi_render(sumi_instance_t* inst) {
    if (!inst) return;
    engine_sync_cells(inst);   // step 43: the stir's discs follow the layout, the size and the cell scale
    // Composite visuals (§4.5): params provide the base; the CC-routed global
    // controls (Airwave Flex etc., §2.2) add live modulation on top.
    sumi_render_visuals_t visuals;
    visuals.palette_id = inst->params.active_palette_id <= SUMI_PALETTE_CUSTOM ? inst->params.active_palette_id : 0u;   // 1.0.0: 3 = the custom palette
    visuals.medium = inst->params.medium;        // 1.1.0: the composite branches per medium
    visuals.anod_glow = inst->params.anod_glow;
    visuals.anod_pitch = inst->params.anod_pitch;
    // dev only: the plate — the display cells the stir turns, handed to the composite as a guide (0 = off)
    visuals.dbg_lattice = inst->dbg_lattice;
    visuals.dbg_cell_count = 0u;
    if (inst->dbg_lattice > 0.0f) {
        visuals.dbg_cell_count = inst->cell_count;
        for (uint32_t i = 0; i < inst->cell_count && i < 320u; i++)
            for (int k = 0; k < 4; k++) visuals.dbg_cells[i][k] = inst->cells[i][k];
    }
    {
        const sumi_palette_t* pal = &inst->palette;
        for (uint32_t i = 0; i < SUMI_PALETTE_MAX_STOPS; i++) {
            visuals.custom_stops[i][0] = pal->stops[i].rgb[0];
            visuals.custom_stops[i][1] = pal->stops[i].rgb[1];
            visuals.custom_stops[i][2] = pal->stops[i].rgb[2];
            visuals.custom_stops[i][3] = pal->stops[i].position;
        }
        visuals.custom_count = (float)pal->stop_count;
        visuals.custom_gamma = pal->depth_gamma;
        visuals.custom_floor = pal->depth_floor;
        visuals.custom_drift = pal->hue_drift;
        visuals.custom_clear[0] = pal->clear_rgb[0];
        visuals.custom_clear[1] = pal->clear_rgb[1];
        visuals.custom_clear[2] = pal->clear_rgb[2];
    }
    visuals.roughness = clamp01(inst->params.paper_roughness +
                                 sumi_voice_mapper_ctl(inst->mapper, SUMI_CTL_PAPER_ROUGHNESS));
    visuals.palette_morph = clamp01(sumi_voice_mapper_ctl(inst->mapper, SUMI_CTL_PALETTE_MORPH));
    // §4.5 live ripple (v0.4): the same smoothed ctl values the bake path
    // consumes, routed to the composite's view displacement instead. In bake
    // mode the live amp is 0 — the deform passes carry the ripple.
    const float ramp = clamp01(sumi_voice_mapper_ctl(inst->mapper, SUMI_CTL_RIPPLE_AMP));
    const float rfreq = clamp01(sumi_voice_mapper_ctl(inst->mapper, SUMI_CTL_RIPPLE_FREQ));
    visuals.ripple_amp = (inst->params.ripple_bake == 0) ? ramp * SUMI_RIPPLE_AMP_MAX : 0.0f;
    visuals.ripple_k = SUMI_RIPPLE_K_MIN + rfreq * (SUMI_RIPPLE_K_MAX - SUMI_RIPPLE_K_MIN);
    visuals.ripple_phase = 0.0f;
    visuals.ripple_angle = inst->params.ripple_angle;
    sumi_renderer_render(inst->renderer, inst->deforms, inst->last_dt, &visuals);
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
    // 1.0.0: the reserved layouts (8..12) clamp to FIFTHS until Phase 8 ships
    // them; an unknown medium is SUMI; the palette id stops at CUSTOM.
    if (inst->params.pitch_layout >= SUMI_LAYOUT_TRUMPET) {
        char msg[96];
        snprintf(msg, sizeof msg, "sumi_set_params: layout %u is reserved (Phase 8) - using FIFTHS", inst->params.pitch_layout);
        log_msg(&inst->config, SUMI_LOG_WARN, msg);
        inst->params.pitch_layout = SUMI_LAYOUT_FIFTHS;
    }
    if (inst->params.medium > SUMI_MEDIUM_ANOD) inst->params.medium = SUMI_MEDIUM_SUMI;
    if (inst->params.active_palette_id > SUMI_PALETTE_CUSTOM) inst->params.active_palette_id = 0u;
    // 1.1.0: the modes accept their values or the medium default; anything else is the default
    if (inst->params.bend_mode > 3u && inst->params.bend_mode != SUMI_MODE_MEDIUM_DEFAULT) inst->params.bend_mode = SUMI_MODE_MEDIUM_DEFAULT;
    if (inst->params.slide_mode > 2u && inst->params.slide_mode != SUMI_MODE_MEDIUM_DEFAULT) inst->params.slide_mode = SUMI_MODE_MEDIUM_DEFAULT;
    if (inst->params.press_mode > 2u && inst->params.press_mode != SUMI_MODE_MEDIUM_DEFAULT) inst->params.press_mode = SUMI_MODE_MEDIUM_DEFAULT;
    if (!(inst->params.anod_glow >= 0.2f)) inst->params.anod_glow = 0.2f;
    if (inst->params.anod_glow > 5.0f) inst->params.anod_glow = 5.0f;
    if (!(inst->params.anod_pitch > 0.0f)) inst->params.anod_pitch = 0.0f;                  // 0 = no grid; NaN and negatives land there
    else if (inst->params.anod_pitch < 1.0f / 256.0f) inst->params.anod_pitch = 1.0f / 256.0f;
    else if (inst->params.anod_pitch > 1.0f / 8.0f) inst->params.anod_pitch = 1.0f / 8.0f;
    if (inst->params.chladni_mode > SUMI_CHLADNI_FIELD) inst->params.chladni_mode = SUMI_CHLADNI_DISCS;
    for (int i = 0; i < 12; i++) {
        uint32_t m = inst->params.burst_order_by_class[i];
        if (m != 0u && m < 2u) m = 2u;
        if (m > 8u) m = 8u;
        inst->params.burst_order_by_class[i] = m;
    }
    sumi_renderer_set_sim_scale(inst->renderer, inst->params.sim_scale);
}

/* 1.0.0 (Phase 6 step 41, QOL §1): the custom palette, validated and stored;
 * the composite reads it when active_palette_id == SUMI_PALETTE_CUSTOM. */
void sumi_set_palette(sumi_instance_t* inst, const sumi_palette_t* palette) {
    if (!inst || !palette) return;
    sumi_palette_t p = *palette;
    if (p.stop_count < 2u) p.stop_count = 2u;
    if (p.stop_count > SUMI_PALETTE_MAX_STOPS) p.stop_count = SUMI_PALETTE_MAX_STOPS;
    float last = 0.0f;
    for (uint32_t i = 0; i < SUMI_PALETTE_MAX_STOPS; i++) {
        for (int c = 0; c < 3; c++) p.stops[i].rgb[c] = clamp01(p.stops[i].rgb[c] == p.stops[i].rgb[c] ? p.stops[i].rgb[c] : 0.0f);
        float pos = p.stops[i].position == p.stops[i].position ? clamp01(p.stops[i].position) : 1.0f;
        if (i < p.stop_count) { if (pos < last) pos = last; last = pos; }   // ascending
        p.stops[i].position = pos;
    }
    if (!(p.depth_gamma >= 0.25f)) p.depth_gamma = 0.25f;
    if (p.depth_gamma > 4.0f) p.depth_gamma = 4.0f;
    p.depth_floor = clamp01(p.depth_floor == p.depth_floor ? p.depth_floor : 0.0f);
    p.hue_drift = clamp01(p.hue_drift == p.hue_drift ? p.hue_drift : 0.0f);
    for (int c = 0; c < 3; c++) p.clear_rgb[c] = clamp01(p.clear_rgb[c] == p.clear_rgb[c] ? p.clear_rgb[c] : 0.8f);
    inst->palette = p;
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
    // §5.3: refuse (with a warning) while both print buffers are busy.
    if (!sumi_renderer_dip_ready(inst->renderer)) {
        log_msg(&inst->config, SUMI_LOG_WARN,
                "sumi_trigger_paper_dip: refused (print readback still in flight)");
        return;
    }
    inst->drop_counter = 0;   // §4.2: aux rebase on every dip
    sumi_deform_t d = { SUMI_DEFORM_RESET, {} };
    sumi_deform_queue_push(inst->deforms, &d);
}

bool sumi_read_print(sumi_instance_t* inst, uint8_t* pixels, size_t capacity,
                     uint32_t* out_w, uint32_t* out_h) {
    if (!inst) return false;
    return sumi_renderer_read_print(inst->renderer, pixels, capacity, out_w, out_h);
}

void sumi_add_drop(sumi_instance_t* inst, float x, float y, float radius, uint32_t layer_type) {
    if (!inst || radius <= 0.0f || layer_type == SUMI_DROP_NONE) return;   // v0.13: NONE lays nothing
    sumi_deform_t d;
    d.type = SUMI_DEFORM_DROP;
    d.as.drop.x = clamp01(x);
    d.as.drop.y = clamp01(y);
    d.as.drop.radius = radius;
    // layer_type (sumi_drop_layer_t): 0 = ink (counter-derived phase, §4.2);
    // 2 = FEED (v0.6): grow the band already under the centre — the shader
    // fills the interior with the centre texel, so nothing new is laid down
    // and the drop counter is untouched; anything else = clear water /
    // surfactant: expands the field but its interior stays un-inked.
    if (layer_type == SUMI_DROP_INK) {
        d.as.drop.aux = (float)inst->drop_counter;
        d.as.drop.phase_base = sumi_next_ink_phase_base(&inst->drop_counter);
    } else if (layer_type == SUMI_DROP_FEED) {
        d.as.drop.aux = 0.0f;
        d.as.drop.phase_base = -1.0f;   // "inherit the centre" marker, see drop_fs
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

/* Internal test hooks (sumi_debug.h): §4.6 cross-backend field regression.
 * Not SUMI_API — static-link only, never part of the DLL export surface. */

bool sumi_debug_read_field_begin(sumi_instance_t* inst) {
    if (!inst) return false;
    return sumi_renderer_read_field_begin(inst->renderer);
}

int sumi_debug_read_field_poll(sumi_instance_t* inst, uint8_t* out_rgba16f, size_t capacity,
                               uint32_t* out_w, uint32_t* out_h) {
    if (!inst) return 0;
    return sumi_renderer_read_field_poll(inst->renderer, out_rgba16f, capacity, out_w, out_h);
}

void sumi_debug_run_field_script(sumi_instance_t* inst) {
    if (!inst) return;
    sumi_deform_t d;

    d.type = SUMI_DEFORM_DROP;
    d.as.drop.x = 0.5f;  d.as.drop.y = 0.5f;  d.as.drop.radius = 0.20f;
    d.as.drop.phase_base = 1.0f;  d.as.drop.aux = 0.0f;
    sumi_deform_queue_push(inst->deforms, &d);

    d.type = SUMI_DEFORM_TINE;
    d.as.tine.x0 = 0.2f;  d.as.tine.y0 = 0.3f;
    d.as.tine.x1 = 0.8f;  d.as.tine.y1 = 0.7f;
    d.as.tine.alpha = 0.05f;  d.as.tine.magnitude = 0.10f;
    sumi_deform_queue_push(inst->deforms, &d);

    d.type = SUMI_DEFORM_VORTEX;
    d.as.vortex.x = 0.6f;  d.as.vortex.y = 0.4f;
    d.as.vortex.strength = 1.0f;  d.as.vortex.radius = 0.25f;
    d.as.vortex.profile = SUMI_VORTEX_EXPONENTIAL;   // fixture-stable (§4.6)
    d.as.vortex.k = 0.0f;  d.as.vortex.phase = 0.0f;  // v0.10 fields: unused by this profile
    sumi_deform_queue_push(inst->deforms, &d);

    d.type = SUMI_DEFORM_DROP;
    d.as.drop.x = 0.3f;  d.as.drop.y = 0.7f;  d.as.drop.radius = 0.10f;
    d.as.drop.phase_base = 2.0f;  d.as.drop.aux = 1.0f;
    sumi_deform_queue_push(inst->deforms, &d);

    d.type = SUMI_DEFORM_SCROLL;
    d.as.scroll.dx = 0.01f;  d.as.scroll.dy = 0.0f;
    for (int i = 0; i < 3; i++) {
        sumi_deform_queue_push(inst->deforms, &d);
    }
}

bool sumi_debug_read_field(sumi_instance_t* inst, uint8_t* out_rgba16f, size_t capacity,
                           uint32_t* out_w, uint32_t* out_h) {
    if (!inst) return false;
    return sumi_renderer_read_field(inst->renderer, out_rgba16f, capacity, out_w, out_h);
}

void sumi_add_vortex(sumi_instance_t* inst, float x, float y, float strength, float radius,
                     uint32_t profile) {
    if (!inst || radius <= 0.0f || strength == 0.0f) return;
    sumi_deform_t d;
    if (profile == SUMI_VORTEX_LAMB_OSEEN) {
        // v0.6 (DECISIONS_4 #49): the §4.3(7) swirl as a gesture — the same
        // pass the voice mapper emits for per-note pressure, with the host
        // supplying S = Γ·Δt (signed) and r_c. Marble mode's "pull to stir".
        d.type = SUMI_DEFORM_SWIRL;
        d.as.swirl.x = clamp01(x);
        d.as.swirl.y = clamp01(y);
        d.as.swirl.strength = strength;
        d.as.swirl.core_r = radius;
        sumi_deform_queue_push(inst->deforms, &d);
        return;
    }
    d.type = SUMI_DEFORM_VORTEX;
    d.as.vortex.x = clamp01(x);
    d.as.vortex.y = clamp01(y);
    d.as.vortex.strength = strength;
    d.as.vortex.radius = radius;
    d.as.vortex.profile = profile == SUMI_VORTEX_RANKINE ? SUMI_VORTEX_RANKINE
                        : profile == SUMI_VORTEX_TORSION ? SUMI_VORTEX_TORSION
                                                         : SUMI_VORTEX_EXPONENTIAL;
    // v0.10: the torsion's k and φ are flavour controls (like the ripple's
    // wavelength) — the gesture takes the mapper's current smoothed values.
    sumi_voice_mapper_torsion_kphi(inst->mapper, d.as.vortex.profile, &d.as.vortex.k, &d.as.vortex.phase);
    sumi_deform_queue_push(inst->deforms, &d);
}

/* v0.11 (Phase 6 step 37): the Chladni lattice as a gesture — one exact
 * kick-drift pass; a negative `a` applies the pair's exact inverse with
 * (−a, −b), reversed shear order (sumi_core.h). */
void sumi_add_chladni(sumi_instance_t* inst, float psi, float balance, float sx, float x0, float sy, float y0) {
    if (!inst || sx <= 0.0f || sy <= 0.0f || psi == 0.0f) return;
    const float aspect = (inst->config.height > 0)
        ? (float)inst->config.width / (float)inst->config.height : 1.0f;
    // the gesture's own rectangular lattice: basis (sx, 0), (0, sy) about (x0, y0), x through the aspect
    sumi_chladni_emit_step(inst->deforms, psi, balance, sx * aspect, 0.0f, 0.0f, sy, x0 * aspect, y0);
}

/* v0.12 (Phase 6 step 38): the viscous multipole burst as a gesture — the
 * strike with its lifetime. The mapper owns the episode (the age envelope
 * and the per-frame budgeted passes); the first increment lands in the next
 * sumi_update (sumi_core.h). */
void sumi_add_burst(sumi_instance_t* inst, float x, float y, float a, float D, float theta0, uint32_t m) {
    if (!inst) return;
    sumi_voice_mapper_add_burst(inst->mapper, clamp01(x), clamp01(y), a, D, theta0, m, &inst->params);
}

uint32_t sumi_debug_burst_count(sumi_instance_t* inst) {
    return inst ? sumi_voice_mapper_burst_count(inst->mapper) : 0u;
}

/* v0.13 (Phase 6 step 39): one exact kick-drift step of the spark shear
 * (sumi_core.h); the stack and the profile are the params'. */
void sumi_add_spark_shear(sumi_instance_t* inst, float x, float y, float band,
                          float A, float B, float k, float phase, float theta0) {
    if (!inst || !(k > 0.0f)) return;
    uint32_t stack = inst->params.spark_stack;
    if (stack < 1u) stack = 1u;
    if (stack > 4u) stack = 4u;
    sumi_spark_emit_step(inst->deforms, clamp01(x), clamp01(y), A, B, k, phase, theta0, band,
                         stack, inst->params.spark_profile ? 1u : 0u);
}

/* v0.13 (Phase 6 step 39): the composed spark strike — the drop now, the
 * burst and the shear as episodes the mapper emits from the next update on
 * (sumi_core.h). */
void sumi_add_spark(sumi_instance_t* inst, float x, float y, float r, float D, float theta0, uint32_t layer_type) {
    if (!inst || !(r > 0.0f)) return;
    sumi_add_drop(inst, x, y, r, layer_type);
    sumi_voice_mapper_add_burst(inst->mapper, clamp01(x), clamp01(y), r, D, theta0, 0u, &inst->params);
    sumi_voice_mapper_add_spark(inst->mapper, clamp01(x), clamp01(y), r, theta0, &inst->params);
}

uint32_t sumi_debug_spark_count(sumi_instance_t* inst) {
    return inst ? sumi_voice_mapper_spark_count(inst->mapper) : 0u;
}

/* v0.14 (Phase 6 step 40): one step of the scaled Chirikov standard map
 * (sumi_core.h); K < 0 is the exact inverse step. */
void sumi_add_chirikov(sumi_instance_t* inst, float x, float y, float K, uint32_t periods, float eps, float phase) {
    if (!inst || !(K == K) || K == 0.0f) return;
    if (periods < 1u) periods = 1u;
    if (periods > 8u) periods = 8u;
    if (!(eps >= 0.05f)) eps = 0.05f;
    if (eps > 1.0f) eps = 1.0f;
    float aK = K < 0.0f ? -K : K;
    if (aK > SUMI_CHIRIKOV_K_GESTURE_MAX) aK = SUMI_CHIRIKOV_K_GESTURE_MAX;
    const float k = 6.2831853f * (float)periods;
    sumi_chirikov_emit_step(inst->deforms, clamp01(x), clamp01(y), aK / (k * eps), k, phase, eps, K < 0.0f);
}

void sumi_debug_set_chladni_overlay(sumi_instance_t* inst, float strength) {
    if (!inst) return;
    if (!(strength > 0.0f)) strength = 0.0f;
    if (strength > 1.0f) strength = 1.0f;
    inst->dbg_lattice = strength;
}


void sumi_debug_add_cells_pass(sumi_instance_t* inst, float theta, float odd_weight, uint32_t mode) {
    if (!inst) return;
    engine_sync_cells(inst);                         // the renderer holds the discs before the pass reads them
    sumi_deform_t d;
    d.type = SUMI_DEFORM_CELLS;
    d.as.cells.theta = theta;
    d.as.cells.odd_weight = odd_weight;
    d.as.cells.mode = mode > SUMI_CHLADNI_FIELD ? SUMI_CHLADNI_DISCS : mode;
    sumi_deform_queue_push(inst->deforms, &d);
}

uint32_t sumi_debug_cells(sumi_instance_t* inst, float* out_xyrk, uint32_t max_cells) {
    if (!inst || !out_xyrk || max_cells == 0u) return 0u;
    return engine_cells(inst, out_xyrk, max_cells);   // as the stir uses them: the radius scaled by chladni_cell
}

/* §4.3(4): one stroke segment, internally subdivided so no single pass moves
 * the tip more than a/4. The spec's a/2 is the fold THRESHOLD, not a budget:
 * at d = a/2 the inverse-map Jacobian (1 − 2d/a at the rear stagnation point)
 * reaches exactly zero (DECISIONS_3 #32) — a/4 keeps det ≥ 0.5 everywhere.
 * The magnitude is the displacement itself — wake strength is pen speed. */
void sumi_add_wake(sumi_instance_t* inst, float x0, float y0, float x1, float y1,
                   float tip_radius) {
    if (!inst || tip_radius <= 0.0f) return;
    const float aspect = (inst->config.height > 0)
        ? (float)inst->config.width / (float)inst->config.height : 1.0f;
    const float dx_ac = (x1 - x0) * aspect;
    const float dy_ac = y1 - y0;
    const float len = sqrtf(dx_ac * dx_ac + dy_ac * dy_ac);
    if (len < 1e-6f) return;
    uint32_t n = (uint32_t)ceilf(len / (tip_radius * 0.25f));
    if (n < 1) n = 1;
    if (n > 256) n = 256;   // bound one segment's queue share (a stroke is many segments)
    // v0.7 (DECISIONS_4 #53): the viscous profile — the same <= a/4 sub-step
    // budget (measured: |grad d| <= 0.25 d/a for every l/a >= 1.5), the spread
    // clamped to the range the kernel is derived for.
    const bool viscous = inst->params.wake_profile == 1;
    float spread = inst->params.wake_spread;
    if (spread < 1.5f) spread = 1.5f;
    if (spread > 12.0f) spread = 12.0f;
    for (uint32_t i = 1; i <= n; i++) {
        const float t = (float)i / (float)n;
        sumi_deform_t d;
        if (viscous) {
            d.type = SUMI_DEFORM_STOKESLET;
            d.as.stokeslet.x = x0 + (x1 - x0) * t;
            d.as.stokeslet.y = y0 + (y1 - y0) * t;
            d.as.stokeslet.dx_ac = dx_ac / (float)n;
            d.as.stokeslet.dy_ac = dy_ac / (float)n;
            d.as.stokeslet.tip_radius = tip_radius;
            d.as.stokeslet.spread = spread;
        } else {
            d.type = SUMI_DEFORM_WAKE;
            d.as.wake.x = x0 + (x1 - x0) * t;        // tip AFTER this sub-step
            d.as.wake.y = y0 + (y1 - y0) * t;
            d.as.wake.dx_ac = dx_ac / (float)n;
            d.as.wake.dy_ac = dy_ac / (float)n;
            d.as.wake.tip_radius = tip_radius;
        }
        if (!sumi_deform_queue_push(inst->deforms, &d)) return;
    }
}

/* §4.3(5): Hamiltonian pinch — gesture-ABI entry (DECISIONS_3 #32): the fold
 * axis is host-side data (pen azimuth, drag angle) with no MIDI path. k MUST
 * be a smoothed delta per call. */
void sumi_add_pinch(sumi_instance_t* inst, float x, float y, float k_delta, float angle) {
    if (!inst || k_delta == 0.0f) return;
    // Variant switch (DECISIONS_3 #34): the crossed-tine look is a params
    // choice honored by both pinch routes; this one covers the gesture ABI.
    if (inst->params.pinch_variant == 1) {
        sumi_deform_t t[2];
        sumi_deform_crossed_pinch(clamp01(x), clamp01(y),
                                  cosf(angle), sinf(angle), k_delta, t);
        sumi_deform_queue_push(inst->deforms, &t[0]);
        sumi_deform_queue_push(inst->deforms, &t[1]);
        return;
    }
    sumi_deform_t d;
    d.type = SUMI_DEFORM_PINCH;
    d.as.pinch.x = clamp01(x);
    d.as.pinch.y = clamp01(y);
    d.as.pinch.k = k_delta;
    d.as.pinch.angle = angle;
    d.as.pinch.window_s = 0.02f;   // S: matches the mapper's PINCH_WINDOW_S
    sumi_deform_queue_push(inst->deforms, &d);
}

} // extern "C"
