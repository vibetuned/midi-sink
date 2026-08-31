// renderer.cpp — sokol_gfx pass orchestration (PROJECT_SPEC.md §1, §4.1, §4.2).
// Step 2: two RGBA16F ping-pong displacement targets at simulation resolution
// (decoupled from the swapchain via sim_scale), identity init, per-frame
// deformation-queue drain (read tex_current -> write tex_next -> swap), and a
// temporary composite visualizing stored coordinates (red = u, green = v).
//
// Note: this file includes sokol_gfx.h declarations only; the sokol
// implementation (SOKOL_IMPL) lives in swapchain_metal.mm because the Metal
// backend must be compiled as Objective-C++ (see DECISIONS.md).
#include "renderer.h"
#include "swapchain.h"
#include "log_levels.h"

#include "deform.glsl.h"
#include "composite.glsl.h"

#include <new>
#include <stdio.h>

// Simulation-resolution guard rails (§4.1: resolution is a param; keep any
// sim_scale/window combination inside sane GPU limits).
static const uint32_t SUMI_SIM_MIN_DIM = 8;
static const uint32_t SUMI_SIM_MAX_DIM = 8192;

struct sumi_renderer_t {
    sumi_swapchain_t* swapchain;
    sumi_log_fn       log_cb;
    void*             log_user;

    uint32_t          out_width;     // swapchain pixel size
    uint32_t          out_height;
    float             sim_scale;
    uint32_t          sim_width;     // current target size
    uint32_t          sim_height;

    sg_image          field_img[2];      // RGBA16F (u, v, ink, aux), §4.2
    sg_view           field_attach[2];   // color-attachment views
    sg_view           field_tex[2];      // texture (sampling) views
    int               cur;               // index of tex_current
    sg_sampler        sampler_linear;    // §4.2: u/v are safe to filter linearly

    sg_pipeline       pip_identity;      // deform.glsl identity pass
    sg_pipeline       pip_passthrough;   // deform.glsl passthrough pass
    sg_pipeline       pip_drop;          // deform.glsl §4.3.1
    sg_pipeline       pip_tine;          // deform.glsl §4.3.2
    sg_pipeline       pip_vortex;        // deform.glsl §4.3.3
    sg_pipeline       pip_composite;     // composite.glsl -> swapchain

    sg_pass_action    clear_action;      // swapchain clear (deep indigo)
    sg_pass_action    field_action;      // offscreen: every texel overwritten
};

// Deep indigo clear color (see DECISIONS.md #6).
static const float SUMI_CLEAR_R = 0.055f;
static const float SUMI_CLEAR_G = 0.050f;
static const float SUMI_CLEAR_B = 0.220f;

static void r_log(const sumi_renderer_t* r, int level, const char* msg) {
    if (r && r->log_cb) r->log_cb(level, msg, r->log_user);
}

static void sokol_log_bridge(const char* tag, uint32_t log_level, uint32_t log_item_id,
                             const char* message_or_null, uint32_t line_nr,
                             const char* filename_or_null, void* user_data) {
    sumi_renderer_t* r = (sumi_renderer_t*)user_data;
    if (!r || !r->log_cb) return;
    char buf[512];
    snprintf(buf, sizeof(buf), "[%s:%u] item=%u line=%u file=%s: %s",
             tag ? tag : "sokol", log_level, log_item_id, line_nr,
             filename_or_null ? filename_or_null : "?",
             message_or_null ? message_or_null : "(no message)");
    r->log_cb((int)log_level, buf, r->log_user);
}

static uint32_t clamp_dim(uint32_t v) {
    if (v < SUMI_SIM_MIN_DIM) return SUMI_SIM_MIN_DIM;
    if (v > SUMI_SIM_MAX_DIM) return SUMI_SIM_MAX_DIM;
    return v;
}

static void destroy_field_targets(sumi_renderer_t* r) {
    for (int i = 0; i < 2; i++) {
        if (r->field_tex[i].id)    { sg_destroy_view(r->field_tex[i]);    r->field_tex[i].id = 0; }
        if (r->field_attach[i].id) { sg_destroy_view(r->field_attach[i]); r->field_attach[i].id = 0; }
        if (r->field_img[i].id)    { sg_destroy_image(r->field_img[i]);   r->field_img[i].id = 0; }
    }
}

// Runs the identity-init pass into tex_current (§4.1: u = x/W, v = y/H).
static void identity_init(sumi_renderer_t* r) {
    sg_pass pass = {};
    pass.action = r->field_action;
    pass.attachments.colors[0] = r->field_attach[r->cur];
    pass.label = "identity-init";
    sg_begin_pass(&pass);
    sg_apply_pipeline(r->pip_identity);
    sg_draw(0, 3, 1);
    sg_end_pass();
}

// (Re)creates both ping-pong targets at simulation resolution and re-runs
// identity init. Returns false on resource-creation failure.
static bool create_field_targets(sumi_renderer_t* r) {
    void* pool = sumi_swapchain_frame_pool_push(r->swapchain);
    destroy_field_targets(r);

    r->sim_width  = clamp_dim((uint32_t)((float)r->out_width  * r->sim_scale + 0.5f));
    r->sim_height = clamp_dim((uint32_t)((float)r->out_height * r->sim_scale + 0.5f));

    for (int i = 0; i < 2; i++) {
        sg_image_desc img_desc = {};
        img_desc.usage.color_attachment = true;
        img_desc.width  = (int)r->sim_width;
        img_desc.height = (int)r->sim_height;
        img_desc.pixel_format = SG_PIXELFORMAT_RGBA16F;   // §4.1 (RGBA32F = later quality flag)
        img_desc.sample_count = 1;
        img_desc.label = i ? "field-B" : "field-A";
        r->field_img[i] = sg_make_image(&img_desc);

        sg_view_desc attach_desc = {};
        attach_desc.color_attachment.image = r->field_img[i];
        attach_desc.label = i ? "field-B-attach" : "field-A-attach";
        r->field_attach[i] = sg_make_view(&attach_desc);

        sg_view_desc tex_desc = {};
        tex_desc.texture.image = r->field_img[i];
        tex_desc.label = i ? "field-B-tex" : "field-A-tex";
        r->field_tex[i] = sg_make_view(&tex_desc);

        if (sg_query_image_state(r->field_img[i]) != SG_RESOURCESTATE_VALID ||
            sg_query_view_state(r->field_attach[i]) != SG_RESOURCESTATE_VALID ||
            sg_query_view_state(r->field_tex[i]) != SG_RESOURCESTATE_VALID) {
            r_log(r, SUMI_LOG_ERROR, "renderer: failed to create simulation targets");
            sumi_swapchain_frame_pool_pop(r->swapchain, pool);
            return false;
        }
    }
    r->cur = 0;
    identity_init(r);
    sumi_swapchain_frame_pool_pop(r->swapchain, pool);

    char buf[128];
    snprintf(buf, sizeof(buf), "renderer: sim targets %ux%u (output %ux%u, sim_scale %.3f)",
             r->sim_width, r->sim_height, r->out_width, r->out_height, (double)r->sim_scale);
    r_log(r, SUMI_LOG_INFO, buf);
    return true;
}

static bool create_pipelines(sumi_renderer_t* r) {
    const sg_backend backend = sg_query_backend();

    // Offscreen deformation pipelines: fullscreen triangle, no vertex buffers,
    // RGBA16F color target, no depth.
    sg_pipeline_desc pd = {};
    pd.shader = sg_make_shader(deform_identity_shader_desc(backend));
    pd.colors[0].pixel_format = SG_PIXELFORMAT_RGBA16F;
    pd.depth.pixel_format = SG_PIXELFORMAT_NONE;
    pd.sample_count = 1;
    pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pd.cull_mode = SG_CULLMODE_NONE;
    pd.label = "deform-identity";
    r->pip_identity = sg_make_pipeline(&pd);

    sg_pipeline_desc pp = pd;
    pp.shader = sg_make_shader(deform_passthrough_shader_desc(backend));
    pp.label = "deform-passthrough";
    r->pip_passthrough = sg_make_pipeline(&pp);

    sg_pipeline_desc pdrop = pd;
    pdrop.shader = sg_make_shader(deform_drop_shader_desc(backend));
    pdrop.label = "deform-drop";
    r->pip_drop = sg_make_pipeline(&pdrop);

    sg_pipeline_desc ptine = pd;
    ptine.shader = sg_make_shader(deform_tine_shader_desc(backend));
    ptine.label = "deform-tine";
    r->pip_tine = sg_make_pipeline(&ptine);

    sg_pipeline_desc pvortex = pd;
    pvortex.shader = sg_make_shader(deform_vortex_shader_desc(backend));
    pvortex.label = "deform-vortex";
    r->pip_vortex = sg_make_pipeline(&pvortex);

    // Composite: swapchain formats.
    sg_pipeline_desc pc = {};
    pc.shader = sg_make_shader(composite_shader_desc(backend));
    pc.colors[0].pixel_format = SG_PIXELFORMAT_BGRA8;
    pc.depth.pixel_format = SG_PIXELFORMAT_NONE;
    pc.sample_count = 1;
    pc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pc.cull_mode = SG_CULLMODE_NONE;
    pc.label = "composite";
    r->pip_composite = sg_make_pipeline(&pc);

    if (sg_query_pipeline_state(r->pip_identity) != SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(r->pip_passthrough) != SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(r->pip_drop) != SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(r->pip_tine) != SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(r->pip_vortex) != SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(r->pip_composite) != SG_RESOURCESTATE_VALID) {
        r_log(r, SUMI_LOG_ERROR, "renderer: pipeline creation failed");
        return false;
    }

    sg_sampler_desc smp = {};
    smp.min_filter = SG_FILTER_LINEAR;
    smp.mag_filter = SG_FILTER_LINEAR;
    smp.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    smp.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    smp.label = "field-linear";
    r->sampler_linear = sg_make_sampler(&smp);
    return sg_query_sampler_state(r->sampler_linear) == SG_RESOURCESTATE_VALID;
}

extern "C" {

sumi_renderer_t* sumi_renderer_create(const sumi_config_t* config, float sim_scale) {
    sumi_renderer_t* r = new (std::nothrow) sumi_renderer_t();
    if (!r) return nullptr;
    r->log_cb = config->log_cb;
    r->log_user = config->log_user;
    r->out_width = config->width;
    r->out_height = config->height;
    r->sim_scale = sim_scale;

    r->swapchain = sumi_swapchain_create(config);
    if (!r->swapchain) {
        delete r;
        return nullptr;
    }

    sg_desc desc = {};
    desc.environment = sumi_swapchain_environment(r->swapchain);
    desc.logger.func = sokol_log_bridge;
    desc.logger.user_data = r;
    sg_setup(&desc);
    if (!sg_isvalid()) {
        r_log(r, SUMI_LOG_ERROR, "renderer: sg_setup failed");
        sumi_swapchain_destroy(r->swapchain);
        delete r;
        return nullptr;
    }

    r->clear_action = {};
    r->clear_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    r->clear_action.colors[0].store_action = SG_STOREACTION_STORE;
    r->clear_action.colors[0].clear_value = { SUMI_CLEAR_R, SUMI_CLEAR_G, SUMI_CLEAR_B, 1.0f };

    // Every deformation/init pass overwrites every texel of the target, so
    // the previous contents never need loading.
    r->field_action = {};
    r->field_action.colors[0].load_action = SG_LOADACTION_DONTCARE;
    r->field_action.colors[0].store_action = SG_STOREACTION_STORE;

    if (!create_pipelines(r) || !create_field_targets(r)) {
        sg_shutdown();
        sumi_swapchain_destroy(r->swapchain);
        delete r;
        return nullptr;
    }
    return r;
}

void sumi_renderer_destroy(sumi_renderer_t* r) {
    if (!r) return;
    sg_shutdown();   // releases all sokol resources, including the targets
    sumi_swapchain_destroy(r->swapchain);
    delete r;
}

void sumi_renderer_resize(sumi_renderer_t* r, uint32_t w, uint32_t h, float pixel_ratio) {
    if (!r) return;
    sumi_swapchain_resize(r->swapchain, w, h, pixel_ratio);
    if (w == r->out_width && h == r->out_height) return;
    r->out_width = w;
    r->out_height = h;
    create_field_targets(r);   // recreate at the new simulation resolution
}

void sumi_renderer_set_sim_scale(sumi_renderer_t* r, float sim_scale) {
    if (!r || sim_scale <= 0.0f) return;
    if (sim_scale == r->sim_scale) return;
    r->sim_scale = sim_scale;
    create_field_targets(r);
}

void sumi_renderer_render(sumi_renderer_t* r, const sumi_deform_queue_t* deforms) {
    if (!r) return;
    // Every autoreleased Metal object this frame creates (pass encoders,
    // drawables) is released here at end of frame — mandatory when the host
    // has no runloop-driven pool of its own (see swapchain.h).
    void* pool = sumi_swapchain_frame_pool_push(r->swapchain);

    // Drain the deformation queue: each pass reads tex_current, writes
    // tex_next, then the indices swap (§4.1).
    const uint32_t n = sumi_deform_queue_count(deforms);
    for (uint32_t i = 0; i < n; i++) {
        const sumi_deform_t* d = sumi_deform_queue_at(deforms, i);
        const int next = 1 - r->cur;

        // Deformation math runs in aspect-corrected space (§4.3): use the
        // actual field texture's aspect so clamped sim dims stay isotropic.
        const float aspect = (float)r->sim_width / (float)r->sim_height;

        sg_pass pass = {};
        pass.action = r->field_action;
        pass.attachments.colors[0] = r->field_attach[next];
        pass.label = "deform";
        sg_begin_pass(&pass);
        switch (d->type) {
            case SUMI_DEFORM_DROP: {
                sg_apply_pipeline(r->pip_drop);
                drop_params_t p = {};
                p.center[0] = d->as.drop.x;
                p.center[1] = d->as.drop.y;
                p.radius = d->as.drop.radius;
                p.aspect = aspect;
                p.phase_base = d->as.drop.phase_base;
                p.aux_value = d->as.drop.aux;
                sg_apply_uniforms(UB_drop_params, SG_RANGE(p));
                break;
            }
            case SUMI_DEFORM_TINE: {
                sg_apply_pipeline(r->pip_tine);
                tine_params_t p = {};
                p.p0[0] = d->as.tine.x0;
                p.p0[1] = d->as.tine.y0;
                p.p1[0] = d->as.tine.x1;
                p.p1[1] = d->as.tine.y1;
                p.alpha = d->as.tine.alpha;
                p.magnitude = d->as.tine.magnitude;
                p.aspect = aspect;
                sg_apply_uniforms(UB_tine_params, SG_RANGE(p));
                break;
            }
            case SUMI_DEFORM_VORTEX: {
                sg_apply_pipeline(r->pip_vortex);
                vortex_params_t p = {};
                p.center[0] = d->as.vortex.x;
                p.center[1] = d->as.vortex.y;
                p.strength = d->as.vortex.strength;
                p.vradius = d->as.vortex.radius;
                p.aspect = aspect;
                sg_apply_uniforms(UB_vortex_params, SG_RANGE(p));
                break;
            }
            case SUMI_DEFORM_RESET:
            case SUMI_DEFORM_PASSTHROUGH:
            default:
                sg_apply_pipeline(d->type == SUMI_DEFORM_RESET ? r->pip_identity
                                                               : r->pip_passthrough);
                break;
        }
        if (d->type != SUMI_DEFORM_RESET) {   // identity reads no field
            sg_bindings bind = {};
            bind.views[VIEW_tex_current] = r->field_tex[r->cur];
            bind.samplers[SMP_smp_field] = r->sampler_linear;
            sg_apply_bindings(&bind);
        }
        sg_draw(0, 3, 1);
        sg_end_pass();

        r->cur = next;
    }

    // Composite the current field to the swapchain (step 2: raw u/v as R/G).
    sg_swapchain swapchain = sumi_swapchain_acquire(r->swapchain);
    if (!swapchain.metal.current_drawable) {
        // zero-sized / occluded surface: skip presentation
        sumi_swapchain_frame_pool_pop(r->swapchain, pool);
        return;
    }
    sg_pass pass = {};
    pass.action = r->clear_action;
    pass.swapchain = swapchain;
    pass.label = "composite";
    sg_begin_pass(&pass);
    sg_apply_pipeline(r->pip_composite);
    sg_bindings bind = {};
    bind.views[VIEW_tex_field] = r->field_tex[r->cur];
    bind.samplers[SMP_smp_field] = r->sampler_linear;
    sg_apply_bindings(&bind);
    sg_draw(0, 3, 1);
    sg_end_pass();
    sg_commit();   // presents the drawable on Metal
    sumi_swapchain_frame_done(r->swapchain);
    sumi_swapchain_frame_pool_pop(r->swapchain, pool);
}

} // extern "C"
