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
#include <stdlib.h>
#include <string.h>

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
    bool              field_dirty;       // any deform since identity/dip reset
    sg_sampler        sampler_linear;    // §4.2: u/v are safe to filter linearly

    sg_pipeline       pip_identity;      // deform.glsl identity pass
    sg_pipeline       pip_passthrough;   // deform.glsl passthrough pass
    sg_pipeline       pip_drop;          // deform.glsl §4.3.1
    sg_pipeline       pip_tine;          // deform.glsl §4.3.2
    sg_pipeline       pip_vortex;        // deform.glsl §4.3.3
    sg_pipeline       pip_scroll;        // deform.glsl §3.4 field motion
    sg_pipeline       pip_composite;     // composite.glsl -> swapchain (BGRA8)
    sg_pipeline       pip_composite_print;   // composite.glsl -> print target (RGBA8)

    sg_pass_action    clear_action;      // swapchain clear (deep indigo)
    sg_pass_action    field_action;      // offscreen: every texel overwritten

    // Paper-dip print (§5.3): offscreen RGBA8 snapshot + async readback into
    // one of TWO cpu buffers (double-buffered: a dip must never overwrite a
    // print the host is still consuming; a third dip is refused upstream).
    sg_image          print_img;
    sg_view           print_attach;
    uint32_t          print_w, print_h;      // target size (tracks output)
    uint8_t*          print_buf[2];
    uint32_t          buf_w[2], buf_h[2];
    int               buf_state[2];          // 0 free, 1 awaiting GPU, 2 ready
    uint64_t          buf_seq[2];            // ready order (newest = max)
    uint64_t          seq_counter;
    int               pending_idx;           // buffer awaiting the blit, or -1
    float             dip_fade;              // "lift the paper" flash
    sumi_render_visuals_t visuals;           // current frame's composite params
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

// (Re)creates the paper-dip print target at output resolution.
static bool create_print_target(sumi_renderer_t* r) {
    if (r->print_attach.id) { sg_destroy_view(r->print_attach); r->print_attach.id = 0; }
    if (r->print_img.id)    { sg_destroy_image(r->print_img);   r->print_img.id = 0; }

    r->print_w = r->out_width;
    r->print_h = r->out_height;
    sg_image_desc img = {};
    img.usage.color_attachment = true;
    img.width = (int)r->print_w;
    img.height = (int)r->print_h;
    img.pixel_format = SG_PIXELFORMAT_RGBA8;
    img.sample_count = 1;
    img.label = "print-target";
    r->print_img = sg_make_image(&img);

    sg_view_desc vd = {};
    vd.color_attachment.image = r->print_img;
    vd.label = "print-attach";
    r->print_attach = sg_make_view(&vd);

    // Preallocate both CPU-side print buffers now: a dip mid-performance must
    // not pay a 14 MB alloc inside the frame.
    for (int i = 0; i < 2; i++) {
        uint8_t* buf = (uint8_t*)realloc(r->print_buf[i], (size_t)r->print_w * r->print_h * 4);
        if (!buf) return false;
        r->print_buf[i] = buf;
    }
    return sg_query_image_state(r->print_img) == SG_RESOURCESTATE_VALID &&
           sg_query_view_state(r->print_attach) == SG_RESOURCESTATE_VALID;
}

// Composite the current field into a target (swapchain or print).
static void run_composite(sumi_renderer_t* r, sg_pipeline pip, float dip_fade) {
    composite_params_t cp = {};
    cp.aspect = (float)r->sim_width / (float)r->sim_height;
    cp.roughness = r->visuals.roughness;
    cp.palette_id = (float)r->visuals.palette_id;
    cp.palette_morph = r->visuals.palette_morph;
    cp.dip_fade = dip_fade;
    cp.texel_y = 1.0f / (float)(r->sim_height > 0 ? r->sim_height : 1);
    sg_apply_pipeline(pip);
    sg_bindings bind = {};
    bind.views[VIEW_tex_field] = r->field_tex[r->cur];
    bind.samplers[SMP_smp_field] = r->sampler_linear;
    sg_apply_bindings(&bind);
    sg_apply_uniforms(UB_composite_params, SG_RANGE(cp));
    sg_draw(0, 3, 1);
}

// §5.3 paper dip, snapshot half: composite the CURRENT field into the print
// target and schedule the async GPU->CPU blit. Runs inside the frame's pass
// stream — never blocks.
static void snapshot_print(sumi_renderer_t* r) {
    if (!r->print_img.id) return;
    int idx = -1;   // dip_ready() was checked upstream; find the free buffer
    if (r->buf_state[0] == 0) idx = 0;
    else if (r->buf_state[1] == 0) idx = 1;
    if (idx < 0 || r->pending_idx >= 0 || !r->print_buf[idx]) {
        r_log(r, SUMI_LOG_WARN, "renderer: print snapshot skipped (no free buffer)");
        return;
    }
    sg_pass pass = {};
    pass.action = r->field_action;
    pass.attachments.colors[0] = r->print_attach;
    pass.label = "print-snapshot";
    sg_begin_pass(&pass);
    run_composite(r, r->pip_composite_print, 0.0f);   // pre-dip look, no fade
    sg_end_pass();
    sg_commit();   // flush the snapshot pass before the copy is enqueued

    // Backend-neutral readback seam: the swapchain TU queries its own native
    // texture from the sg_image handle and orders the copy after the flushed
    // snapshot pass (Metal: blit on the renderer's queue; D3D11: CopyResource
    // on the immediate context).
    if (sumi_swapchain_readback_begin(r->swapchain, r->print_img,
                                      r->print_w, r->print_h, 4)) {
        r->buf_w[idx] = r->print_w;
        r->buf_h[idx] = r->print_h;
        r->buf_state[idx] = 1;
        r->pending_idx = idx;
    }
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

// (Re)creates both ping-pong targets at simulation resolution. A field that
// has been drawn on is CARRIED ACROSS the resize: the §4.2 texel payload
// (u, v, ink, aux) is normalized and resolution-independent, so one
// passthrough pass resamples the old current texture into the new target
// (stretched to the new aspect — the tray is the canvas). A pristine field
// re-runs the exact identity init instead, keeping the §4.6 field dump
// byte-stable (resampled identity differs from exact identity by half-float
// interpolation LSBs). Returns false on resource-creation failure.
static bool create_field_targets(sumi_renderer_t* r) {
    void* pool = sumi_swapchain_frame_pool_push(r->swapchain);

    // Detach the old set; the old current texture must stay alive until the
    // preserving resample below has run.
    sg_image old_img[2];
    sg_view  old_attach[2], old_tex[2];
    const int old_cur = r->cur;
    for (int i = 0; i < 2; i++) {
        old_img[i] = r->field_img[i];       r->field_img[i].id = 0;
        old_attach[i] = r->field_attach[i]; r->field_attach[i].id = 0;
        old_tex[i] = r->field_tex[i];       r->field_tex[i].id = 0;
    }
    const bool preserve = r->field_dirty && old_tex[old_cur].id != 0 &&
                          r->pip_passthrough.id != 0;

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
            for (int j = 0; j < 2; j++) {
                if (old_tex[j].id)    sg_destroy_view(old_tex[j]);
                if (old_attach[j].id) sg_destroy_view(old_attach[j]);
                if (old_img[j].id)    sg_destroy_image(old_img[j]);
            }
            sumi_swapchain_frame_pool_pop(r->swapchain, pool);
            return false;
        }
    }
    r->cur = 0;
    if (preserve) {
        sg_pass pass = {};
        pass.action = r->field_action;
        pass.attachments.colors[0] = r->field_attach[r->cur];
        pass.label = "field-resize-carry";
        sg_begin_pass(&pass);
        sg_apply_pipeline(r->pip_passthrough);
        sg_bindings bind = {};
        bind.views[VIEW_tex_current] = old_tex[old_cur];
        bind.samplers[SMP_smp_field] = r->sampler_linear;
        sg_apply_bindings(&bind);
        sg_draw(0, 3, 1);
        sg_end_pass();
    } else {
        identity_init(r);
        r->field_dirty = false;
    }
    for (int j = 0; j < 2; j++) {
        if (old_tex[j].id)    sg_destroy_view(old_tex[j]);
        if (old_attach[j].id) sg_destroy_view(old_attach[j]);
        if (old_img[j].id)    sg_destroy_image(old_img[j]);
    }
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

    sg_pipeline_desc pscroll = pd;
    pscroll.shader = sg_make_shader(deform_scroll_shader_desc(backend));
    pscroll.label = "deform-scroll";
    r->pip_scroll = sg_make_pipeline(&pscroll);

    // Composite: swapchain formats. The color format is left at default so it
    // inherits the environment default reported by the swapchain TU (BGRA8 on
    // Metal/D3D11, RGBA8 on GL) — the renderer stays backend-neutral.
    sg_pipeline_desc pc = {};
    pc.shader = sg_make_shader(composite_shader_desc(backend));
    pc.depth.pixel_format = SG_PIXELFORMAT_NONE;
    pc.sample_count = 1;
    pc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pc.cull_mode = SG_CULLMODE_NONE;
    pc.label = "composite";
    r->pip_composite = sg_make_pipeline(&pc);

    // Same composite into the RGBA8 print target (§5.3 readback wants RGBA8).
    // Distinct program: its VS is the offscreen-flipped variant (§4.6 — on GL
    // the print target must land top-left-origin like every offscreen pass;
    // on Metal/D3D11 both programs compile to identical code).
    sg_pipeline_desc pcp = pc;
    pcp.shader = sg_make_shader(composite_print_shader_desc(backend));
    pcp.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
    pcp.label = "composite-print";
    r->pip_composite_print = sg_make_pipeline(&pcp);

    if (sg_query_pipeline_state(r->pip_scroll) != SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(r->pip_composite_print) != SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(r->pip_identity) != SG_RESOURCESTATE_VALID ||
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

    r->visuals.palette_id = 0;
    r->visuals.roughness = 0.5f;
    r->pending_idx = -1;
    if (!create_pipelines(r) || !create_field_targets(r) || !create_print_target(r)) {
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
    free(r->print_buf[0]);
    free(r->print_buf[1]);
    delete r;
}

void sumi_renderer_resize(sumi_renderer_t* r, uint32_t w, uint32_t h, float pixel_ratio) {
    if (!r) return;
    sumi_swapchain_resize(r->swapchain, w, h, pixel_ratio);
    if (w == r->out_width && h == r->out_height) return;
    r->out_width = w;
    r->out_height = h;
    create_field_targets(r);   // recreate at the new simulation resolution
    create_print_target(r);    // print target tracks the output size
}

void sumi_renderer_set_sim_scale(sumi_renderer_t* r, float sim_scale) {
    if (!r || sim_scale <= 0.0f) return;
    if (sim_scale == r->sim_scale) return;
    r->sim_scale = sim_scale;
    create_field_targets(r);
}

void sumi_renderer_render(sumi_renderer_t* r, const sumi_deform_queue_t* deforms,
                          double dt, const sumi_render_visuals_t* visuals) {
    if (!r) return;
    if (visuals) r->visuals = *visuals;
    float fdt = (float)dt;
    if (fdt <= 0.0f || fdt > 0.1f) fdt = 1.0f / 120.0f;
    if (r->dip_fade > 0.0f) {
        r->dip_fade -= fdt * 2.5f;   // ~0.4 s "lift the paper" flash
        if (r->dip_fade < 0.0f) r->dip_fade = 0.0f;
    }
    // Poll the async paper-dip readback (§5.3): never blocks.
    if (r->pending_idx >= 0) {
        const int idx = r->pending_idx;
        const size_t bytes = (size_t)r->buf_w[idx] * r->buf_h[idx] * 4;
        const int st = sumi_swapchain_readback_poll(r->swapchain, r->print_buf[idx], bytes);
        if (st == 2) {
            r->buf_state[idx] = 2;
            r->buf_seq[idx] = ++r->seq_counter;
            r->pending_idx = -1;
            r_log(r, SUMI_LOG_INFO, "renderer: paper-dip print ready");
        }
    }

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

        // §5.3 paper dip: freeze & snapshot the canvas as it stands
        // (everything queued before the dip has applied), THEN reset.
        if (d->type == SUMI_DEFORM_RESET) {
            snapshot_print(r);
            r->dip_fade = 1.0f;
        }

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
            case SUMI_DEFORM_SCROLL: {
                sg_apply_pipeline(r->pip_scroll);
                scroll_params_t p = {};
                p.delta[0] = d->as.scroll.dx;
                p.delta[1] = d->as.scroll.dy;
                sg_apply_uniforms(UB_scroll_params, SG_RANGE(p));
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
        // Resize preservation tracking: a dip reset returns the field to
        // pristine identity; any expressive/motion pass marks it drawn-on.
        if (d->type == SUMI_DEFORM_RESET) r->field_dirty = false;
        else if (d->type != SUMI_DEFORM_PASSTHROUGH) r->field_dirty = true;
    }

    // Composite the current field to the swapchain (step 2: raw u/v as R/G).
    // A zero-width swapchain is the backend-neutral "no surface this frame"
    // signal (Metal: nextDrawable failed; D3D11: resize failed / zero-sized).
    sg_swapchain swapchain = sumi_swapchain_acquire(r->swapchain);
    if (swapchain.width <= 0 || swapchain.height <= 0) {
        sumi_swapchain_frame_pool_pop(r->swapchain, pool);
        return;
    }
    sg_pass pass = {};
    pass.action = r->clear_action;
    pass.swapchain = swapchain;
    pass.label = "composite";
    sg_begin_pass(&pass);
    run_composite(r, r->pip_composite, r->dip_fade);
    sg_end_pass();
    sg_commit();   // presents the drawable on Metal
    sumi_swapchain_frame_done(r->swapchain);   // presents on D3D11
    sumi_swapchain_frame_pool_pop(r->swapchain, pool);
}

// §4.6 cross-backend regression support (--field-dump): synchronous readback
// of the CURRENT field texture as raw RGBA16F. Test-only path — it may block
// (bounded), so it must never be called from the performance loop. The caller
// must have rendered (sg_commit flushed) before calling.
bool sumi_renderer_read_field(sumi_renderer_t* r, uint8_t* out_rgba16f, size_t capacity,
                              uint32_t* out_w, uint32_t* out_h) {
    if (!r) return false;
    if (out_w) *out_w = r->sim_width;
    if (out_h) *out_h = r->sim_height;
    if (!out_rgba16f) return true;   // size query
    const size_t bytes = (size_t)r->sim_width * r->sim_height * 8;
    if (capacity < bytes) return false;
    if (r->pending_idx >= 0) return false;   // print readback owns the machinery
    void* pool = sumi_swapchain_frame_pool_push(r->swapchain);
    bool ok = false;
    if (sumi_swapchain_readback_begin(r->swapchain, r->field_img[r->cur],
                                      r->sim_width, r->sim_height, 8)) {
        for (int i = 0; i < 5000; i++) {   // bounded ~5 s wait
            const int st = sumi_swapchain_readback_poll(r->swapchain, out_rgba16f, bytes);
            if (st == 2) { ok = true; break; }
            if (st == 0) break;
            sumi_swapchain_yield(r->swapchain);
        }
    }
    sumi_swapchain_frame_pool_pop(r->swapchain, pool);
    return ok;
}

bool sumi_renderer_dip_ready(const sumi_renderer_t* r) {
    if (!r) return false;
    return (r->buf_state[0] == 0 || r->buf_state[1] == 0) && r->pending_idx < 0;
}

bool sumi_renderer_read_print(sumi_renderer_t* r, uint8_t* pixels, size_t capacity,
                              uint32_t* out_w, uint32_t* out_h) {
    if (!r) return false;
    // Newest READY buffer (§5.3: "the last dipped print").
    int idx = -1;
    for (int i = 0; i < 2; i++) {
        if (r->buf_state[i] == 2 && (idx < 0 || r->buf_seq[i] > r->buf_seq[idx])) idx = i;
    }
    if (idx < 0 || !r->print_buf[idx]) return false;
    if (out_w) *out_w = r->buf_w[idx];
    if (out_h) *out_h = r->buf_h[idx];
    if (!pixels) return true;   // size query: does not consume
    const size_t bytes = (size_t)r->buf_w[idx] * r->buf_h[idx] * 4;
    if (capacity < bytes) return false;
    memcpy(pixels, r->print_buf[idx], bytes);
    r->buf_state[idx] = 0;   // consumed: the buffer is free for the next dip
    return true;
}

} // extern "C"
