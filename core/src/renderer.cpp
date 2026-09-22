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

#include <math.h>
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
    sg_sampler        sampler_nearest;   // step 43: the cells' index map is read as written
    // step 43: the display cells and their index map (which disc a field texel lies in)
    float             cells[320][4];
    uint32_t          cell_count;
    sg_image          cells_img;
    sg_view           cells_tex;
    uint32_t          cells_map_w, cells_map_h;   // the size the map was built at (0 = not built)

    sg_pipeline       pip_identity;      // deform.glsl identity pass
    sg_pipeline       pip_passthrough;   // deform.glsl passthrough pass
    sg_pipeline       pip_drop;          // deform.glsl §4.3.1
    sg_pipeline       pip_tine;          // deform.glsl §4.3.2
    sg_pipeline       pip_vortex;        // deform.glsl §4.3.3
    sg_pipeline       pip_scroll;        // deform.glsl §3.4 field motion
    sg_pipeline       pip_wake;          // deform.glsl §4.3.4 (v0.4)
    sg_pipeline       pip_pinch;         // deform.glsl §4.3.5 (v0.4)
    sg_pipeline       pip_ripple;        // deform.glsl §4.3.6 bake (v0.4)
    sg_pipeline       pip_swirl;         // deform.glsl §4.3.7 (v0.4)
    sg_pipeline       pip_chladni;       // deform.glsl v0.11 Chladni lattice (Phase 6 step 37)
    sg_pipeline       pip_cells;         // deform.glsl step 43: an eddy in every display cell
    sg_pipeline       pip_burst;         // deform.glsl v0.12 viscous multipole burst (Phase 6 step 38)
    sg_pipeline       pip_spark;         // deform.glsl v0.13 spark shear (Phase 6 step 39)
    sg_pipeline       pip_chirikov;      // deform.glsl v0.14 Chirikov standard map (Phase 6 step 40)
    sg_pipeline       pip_stokeslet;     // deform.glsl viscous stroke (v0.7)
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
    // step 43 (QOL §4): an export in flight — its target, the snapshot's field (when exporting from data)
    sg_image          export_img;
    sg_view           export_attach;
    sg_image          export_field_img;
    sg_view           export_field_tex;
    uint32_t          export_w, export_h;
    bool              export_pending;
};

static void destroy_export(sumi_renderer_t* r);   // step 43 (QOL §4), defined with the export below

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
    if (r->print_img.id) {
        sumi_swapchain_release_image(r->swapchain, r->print_img);
        sg_destroy_image(r->print_img);
        r->print_img.id = 0;
    }

    r->print_w = r->out_width;
    r->print_h = r->out_height;
    sg_image_desc img = {};
    img.usage.color_attachment = true;
    img.width = (int)r->print_w;
    img.height = (int)r->print_h;
    img.pixel_format = SG_PIXELFORMAT_RGBA8;
    img.sample_count = 1;
    img.label = "print-target";
    sumi_swapchain_prepare_image(r->swapchain, &img);   // readback-capable (WebGPU)
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

// Composite the current field into a target (swapchain or print). The print
// path passes live_ripple = false: the dip always samples the UN-rippled
// field (§4.5 — the print is what touches the water; the shimmer is surface
// motion, not ink position).
static void run_composite_ex(sumi_renderer_t* r, sg_pipeline pip, float dip_fade, bool live_ripple,
                             sg_view field_view, uint32_t fw, uint32_t fh, uint32_t flags) {
    composite_params_t cp = {};
    cp.aspect = (float)fw / (float)fh;
    cp.roughness = r->visuals.roughness;

    cp.palette_morph = r->visuals.palette_morph;
    cp.alpha_out = (flags & SUMI_EXPORT_ANOD_ALPHA) ? 1.0f : 0.0f;   // step 43: an Anod export over alpha
    cp.dip_fade = dip_fade;
    cp.texel_y = 1.0f / (float)(fh > 0 ? fh : 1);
    cp.ripple_amp = live_ripple ? r->visuals.ripple_amp : 0.0f;
    cp.ripple_k = r->visuals.ripple_k;
    cp.ripple_phase = r->visuals.ripple_phase;
    cp.ripple_ca = cosf(r->visuals.ripple_angle);
    cp.ripple_sa = sinf(r->visuals.ripple_angle);
    // 1.0.0: the custom palette rides along; the shader reads it only when palette_id == 3
    for (int i = 0; i < 8; i++) for (int c = 0; c < 4; c++) { cp.pa_stops[i][c] = r->visuals.pal_a_stops[i][c]; cp.pb_stops[i][c] = r->visuals.pal_b_stops[i][c]; }
    for (int c = 0; c < 4; c++) {
        cp.pa_params[c] = r->visuals.pal_a_params[c]; cp.pa_accent[c] = r->visuals.pal_a_accent[c]; cp.pa_clear[c] = r->visuals.pal_a_clear[c];
        cp.pb_params[c] = r->visuals.pal_b_params[c]; cp.pb_accent[c] = r->visuals.pal_b_accent[c]; cp.pb_clear[c] = r->visuals.pal_b_clear[c];
    }
    cp.medium = (float)r->visuals.medium;        // 1.1.0
    cp.anod_glow = r->visuals.anod_glow > 0.0f ? r->visuals.anod_glow : 1.0f;
    cp.anod_pitch = r->visuals.anod_pitch > 0.0f ? r->visuals.anod_pitch : 0.0f;   // 0 = no grid
    for (int c = 0; c < 4; c++) cp.paper_tint[c] = r->visuals.paper_tint[c];       // step 43: the substrate
    cp.fiber_scale = r->visuals.fiber_scale > 0.0f ? r->visuals.fiber_scale : 1.0f;
    cp.anod_dark = r->visuals.anod_dark;
    cp.anod_grain = r->visuals.anod_grain;
    cp.dbg_lattice = r->visuals.dbg_lattice;                                      // dev only: 0 on every shipped path
    cp.dbg_cell_count = (float)r->visuals.dbg_cell_count;
    for (uint32_t i = 0; i < r->visuals.dbg_cell_count && i < 320u; i++)
        for (int k = 0; k < 4; k++) cp.dbg_cells[i][k] = r->visuals.dbg_cells[i][k];
    sg_apply_pipeline(pip);
    sg_bindings bind = {};
    bind.views[VIEW_tex_field] = field_view;
    bind.samplers[SMP_smp_field] = r->sampler_linear;
    sg_apply_bindings(&bind);
    sg_apply_uniforms(UB_composite_params, SG_RANGE(cp));
    sg_draw(0, 3, 1);
}
static void run_composite(sumi_renderer_t* r, sg_pipeline pip, float dip_fade, bool live_ripple) {
    run_composite_ex(r, pip, dip_fade, live_ripple, r->field_tex[r->cur], r->sim_width, r->sim_height, 0u);
}

// §5.3 paper dip, snapshot half: composite the CURRENT field into the print
// target and schedule the async GPU->CPU blit. Runs inside the frame's pass
// stream — never blocks.
static void snapshot_print(sumi_renderer_t* r) {
    if (!r->print_img.id) return;
    int idx = -1;   // dip_ready() was checked upstream; find the free buffer
    if (r->buf_state[0] == 0) idx = 0;
    else if (r->buf_state[1] == 0) idx = 1;
    if (idx < 0 && r->pending_idx < 0) {
        // v0.6 (DECISIONS_4 #51): both prints READY and unread — recycle the
        // OLDER one. sumi_read_print is a synchronous copy, so no host holds a
        // core buffer between calls; the only unsafe overwrite is a readback
        // still in flight (pending_idx >= 0), which dip_ready() refuses. A dip
        // that nobody reads must never silently stop working.
        idx = (r->buf_seq[0] <= r->buf_seq[1]) ? 0 : 1;
        r_log(r, SUMI_LOG_INFO, "renderer: recycling the oldest unread print");
    }
    if (idx < 0 || r->pending_idx >= 0 || r->export_pending || !r->print_buf[idx]) {
        r_log(r, SUMI_LOG_WARN, "renderer: print snapshot skipped (readback in flight)");
        return;
    }
    sg_pass pass = {};
    pass.action = r->field_action;
    pass.attachments.colors[0] = r->print_attach;
    pass.label = "print-snapshot";
    sg_begin_pass(&pass);
    run_composite(r, r->pip_composite_print, 0.0f, false);   // pre-dip, UN-rippled
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
        if (r->field_img[i].id) {
            sumi_swapchain_release_image(r->swapchain, r->field_img[i]);
            sg_destroy_image(r->field_img[i]);
            r->field_img[i].id = 0;
        }
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
    r->cells_map_w = r->cells_map_h = 0;   // step 43: the cells' index map follows the field's size (rebuilt lazily)
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
        sumi_swapchain_prepare_image(r->swapchain, &img_desc);   // readback-capable (WebGPU)
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
                if (old_img[j].id) {
                    sumi_swapchain_release_image(r->swapchain, old_img[j]);
                    sg_destroy_image(old_img[j]);
                }
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
        if (old_img[j].id) {
            sumi_swapchain_release_image(r->swapchain, old_img[j]);
            sg_destroy_image(old_img[j]);
        }
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

    sg_pipeline_desc pwake = pd;
    pwake.shader = sg_make_shader(deform_wake_shader_desc(backend));
    pwake.label = "deform-wake";
    r->pip_wake = sg_make_pipeline(&pwake);

    sg_pipeline_desc ppinch = pd;
    ppinch.shader = sg_make_shader(deform_pinch_shader_desc(backend));
    ppinch.label = "deform-pinch";
    r->pip_pinch = sg_make_pipeline(&ppinch);

    sg_pipeline_desc pripple = pd;
    pripple.shader = sg_make_shader(deform_ripple_shader_desc(backend));
    pripple.label = "deform-ripple";
    r->pip_ripple = sg_make_pipeline(&pripple);

    sg_pipeline_desc pswirl = pd;
    pswirl.shader = sg_make_shader(deform_swirl_shader_desc(backend));
    pswirl.label = "deform-swirl";
    r->pip_swirl = sg_make_pipeline(&pswirl);

    sg_pipeline_desc pchladni = pd;
    pchladni.shader = sg_make_shader(deform_chladni_shader_desc(backend));
    pchladni.label = "deform-chladni";
    r->pip_chladni = sg_make_pipeline(&pchladni);
    sg_pipeline_desc pcells = pd;
    pcells.shader = sg_make_shader(deform_cells_shader_desc(backend));
    pcells.label = "deform-cells";
    r->pip_cells = sg_make_pipeline(&pcells);
    sg_pipeline_desc pburst = pd;
    pburst.shader = sg_make_shader(deform_burst_shader_desc(backend));
    pburst.label = "deform-burst";
    r->pip_burst = sg_make_pipeline(&pburst);
    sg_pipeline_desc pspark = pd;
    pspark.shader = sg_make_shader(deform_spark_shader_desc(backend));
    pspark.label = "deform-spark";
    r->pip_spark = sg_make_pipeline(&pspark);
    sg_pipeline_desc pchir = pd;
    pchir.shader = sg_make_shader(deform_chirikov_shader_desc(backend));
    pchir.label = "deform-chirikov";
    r->pip_chirikov = sg_make_pipeline(&pchir);
    sg_pipeline_desc pstok = pd;
    pstok.shader = sg_make_shader(deform_stokeslet_shader_desc(backend));
    pstok.label = "deform-stokeslet";
    r->pip_stokeslet = sg_make_pipeline(&pstok);

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
        sg_query_pipeline_state(r->pip_wake) != SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(r->pip_pinch) != SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(r->pip_ripple) != SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(r->pip_swirl) != SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(r->pip_chladni) != SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(r->pip_cells) != SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(r->pip_burst) != SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(r->pip_spark) != SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(r->pip_chirikov) != SG_RESOURCESTATE_VALID ||
        sg_query_pipeline_state(r->pip_stokeslet) != SG_RESOURCESTATE_VALID ||
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
    sg_sampler_desc smpn = smp;
    smpn.min_filter = SG_FILTER_NEAREST;
    smpn.mag_filter = SG_FILTER_NEAREST;
    smpn.label = "cells-nearest";
    r->sampler_nearest = sg_make_sampler(&smpn);
    return sg_query_sampler_state(r->sampler_linear) == SG_RESOURCESTATE_VALID &&
           sg_query_sampler_state(r->sampler_nearest) == SG_RESOURCESTATE_VALID;
}

// step 43: the cells' INDEX MAP — an RGBA16F texture at the field's
// resolution holding, per texel, the indices of up to four display discs it
// lies in (−1 for none), rasterized on the CPU from the cell table. Exact
// discs are disjoint (the largest circle that touches no neighbour's), so a
// texel has one owner in the first slot; the FIELD mode lets discs grow to 1.5
// of the key, where a texel at a corner lies in four. Rebuilt when the cells
// change or the field resizes.
static uint16_t half_of_small_int(int v) {          // exact for 0..2048; −1 → 0xBC00
    if (v < 0) return 0xBC00u;
    if (v == 0) return 0u;
    int e = 0; while ((1 << (e + 1)) <= v) e++;
    const int mant = ((v - (1 << e)) * 1024) >> e;
    return (uint16_t)(((e + 15) << 10) | mant);
}
static void destroy_cells_map(sumi_renderer_t* r) {
    if (r->cells_tex.id) { sg_destroy_view(r->cells_tex); r->cells_tex.id = 0; }
    if (r->cells_img.id) { sg_destroy_image(r->cells_img); r->cells_img.id = 0; }
    r->cells_map_w = r->cells_map_h = 0;
}
static bool ensure_cells_map(sumi_renderer_t* r) {
    if (r->cell_count == 0) return false;
    if (r->cells_tex.id && r->cells_map_w == r->sim_width && r->cells_map_h == r->sim_height) return true;
    destroy_cells_map(r);
    const uint32_t W = r->sim_width, H = r->sim_height;
    uint16_t* map = (uint16_t*)malloc((size_t)W * H * 4u * sizeof(uint16_t));
    if (!map) return false;
    for (size_t i = 0; i < (size_t)W * H * 4u; i++) map[i] = 0xBC00u;                  // −1: no disc, in every slot
    for (uint32_t c = 0; c < r->cell_count; c++) {
        const float cx = r->cells[c][0] * (float)W, cy = r->cells[c][1] * (float)H, rp = r->cells[c][2] * (float)H;
        if (rp <= 0.0f) continue;
        int x0 = (int)floorf(cx - rp) - 1, x1 = (int)ceilf(cx + rp) + 1, y0 = (int)floorf(cy - rp) - 1, y1 = (int)ceilf(cy + rp) + 1;
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > (int)W - 1) x1 = (int)W - 1; if (y1 > (int)H - 1) y1 = (int)H - 1;
        const uint16_t h = half_of_small_int((int)c);
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            const float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy;
            if (dx * dx + dy * dy <= rp * rp) {
                uint16_t* slot = map + ((size_t)y * W + x) * 4u;
                for (int k = 0; k < 4; k++) if (slot[k] == 0xBC00u) { slot[k] = h; break; }   // the first free slot (a fifth owner is dropped)
            }
        }
    }
    sg_image_desc img = {};
    img.width = (int)W; img.height = (int)H;
    img.pixel_format = SG_PIXELFORMAT_RGBA16F;
    img.data.mip_levels[0].ptr = map;
    img.data.mip_levels[0].size = (size_t)W * H * 4u * sizeof(uint16_t);
    img.label = "cells-map";
    r->cells_img = sg_make_image(&img);
    sg_view_desc vd = {};
    vd.texture.image = r->cells_img;
    vd.label = "cells-map-tex";
    r->cells_tex = sg_make_view(&vd);
    free(map);
    if (sg_query_image_state(r->cells_img) != SG_RESOURCESTATE_VALID || sg_query_view_state(r->cells_tex) != SG_RESOURCESTATE_VALID) {
        r_log(r, SUMI_LOG_ERROR, "renderer: failed to create the cells' index map");
        destroy_cells_map(r);
        return false;
    }
    r->cells_map_w = W; r->cells_map_h = H;
    return true;
}

void sumi_renderer_set_cells(sumi_renderer_t* r, const float* cells_xyrk, uint32_t count) {
    if (!r) return;
    if (!cells_xyrk) count = 0u;
    if (count > 320u) count = 320u;
    r->cell_count = count;
    for (uint32_t i = 0; i < count; i++) for (int k = 0; k < 4; k++) r->cells[i][k] = cells_xyrk[i * 4u + (uint32_t)k];
    destroy_cells_map(r);                              // rebuilt lazily by the next pass
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

    r->visuals.roughness = 0.5f;
    r->visuals.paper_tint[0] = 0.900f; r->visuals.paper_tint[1] = 0.868f; r->visuals.paper_tint[2] = 0.790f;
    r->visuals.fiber_scale = 1.0f; r->visuals.anod_dark = 0.5f; r->visuals.anod_grain = 0.5f;
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
    destroy_export(r);
    destroy_cells_map(r);
    if (r->sampler_nearest.id) { sg_destroy_sampler(r->sampler_nearest); r->sampler_nearest.id = 0; }
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
        bool bind_cells = false;
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
                p.profile = (float)d->as.vortex.profile;
                p.k = d->as.vortex.k;           // v0.10: torsion only
                p.phase = d->as.vortex.phase;
                sg_apply_uniforms(UB_vortex_params, SG_RANGE(p));
                break;
            }
            case SUMI_DEFORM_WAKE: {
                sg_apply_pipeline(r->pip_wake);
                wake_params_t p = {};
                p.tip[0] = d->as.wake.x;
                p.tip[1] = d->as.wake.y;
                p.dvec[0] = d->as.wake.dx_ac;
                p.dvec[1] = d->as.wake.dy_ac;
                p.tip_radius = d->as.wake.tip_radius;
                p.aspect = aspect;
                sg_apply_uniforms(UB_wake_params, SG_RANGE(p));
                break;
            }
            case SUMI_DEFORM_STOKESLET: {
                sg_apply_pipeline(r->pip_stokeslet);
                stokeslet_params_t p = {};
                p.tip[0] = d->as.stokeslet.x;
                p.tip[1] = d->as.stokeslet.y;
                p.dvec[0] = d->as.stokeslet.dx_ac;
                p.dvec[1] = d->as.stokeslet.dy_ac;
                p.tip_radius = d->as.stokeslet.tip_radius;
                p.spread = d->as.stokeslet.spread;
                p.aspect = aspect;
                sg_apply_uniforms(UB_stokeslet_params, SG_RANGE(p));
                break;
            }
            case SUMI_DEFORM_PINCH: {
                sg_apply_pipeline(r->pip_pinch);
                pinch_params_t p = {};
                p.center[0] = d->as.pinch.x;
                p.center[1] = d->as.pinch.y;
                p.k = d->as.pinch.k;
                p.ca = cosf(d->as.pinch.angle);
                p.sa = sinf(d->as.pinch.angle);
                p.window_s = d->as.pinch.window_s;
                p.aspect = aspect;
                sg_apply_uniforms(UB_pinch_params, SG_RANGE(p));
                break;
            }
            case SUMI_DEFORM_SWIRL: {
                sg_apply_pipeline(r->pip_swirl);
                swirl_params_t p = {};
                p.center[0] = d->as.swirl.x;
                p.center[1] = d->as.swirl.y;
                p.strength = d->as.swirl.strength;
                p.core_r = d->as.swirl.core_r;
                p.aspect = aspect;
                sg_apply_uniforms(UB_swirl_params, SG_RANGE(p));
                break;
            }
            case SUMI_DEFORM_CHLADNI: {   // v0.11 → step 43: one wave, any wavevector
                sg_apply_pipeline(r->pip_chladni);
                chladni_params_t p = {};
                p.psi = d->as.chladni.psi;
                p.weight = d->as.chladni.weight;
                p.wx = d->as.chladni.wx;
                p.wy = d->as.chladni.wy;
                p.p0x = d->as.chladni.p0x;
                p.p0y = d->as.chladni.p0y;
                p.aspect = aspect;
                sg_apply_uniforms(UB_chladni_params, SG_RANGE(p));
                break;
            }
            case SUMI_DEFORM_CELLS: {     // step 43: an eddy in every display cell (exact)
                if (!ensure_cells_map(r)) { sg_apply_pipeline(r->pip_passthrough); break; }
                sg_apply_pipeline(r->pip_cells);
                bind_cells = true;
                static cells_params_t p;   // 5 KB: kept off the stack
                memset(&p, 0, sizeof(p));
                p.theta = d->as.cells.theta;
                p.odd_weight = d->as.cells.odd_weight;
                p.mode = (float)d->as.cells.mode;
                p.count = (float)r->cell_count;
                p.aspect = aspect;
                for (uint32_t i = 0; i < r->cell_count; i++) for (int k = 0; k < 4; k++) p.cells[i][k] = r->cells[i][k];
                sg_apply_uniforms(UB_cells_params, SG_RANGE(p));
                break;
            }
            case SUMI_DEFORM_BURST: {     // v0.12
                sg_apply_pipeline(r->pip_burst);
                burst_params_t p = {};
                p.centre[0] = d->as.burst.x;
                p.centre[1] = d->as.burst.y;
                p.a = d->as.burst.a;
                p.amp = d->as.burst.amp;
                p.l0 = d->as.burst.l0;
                p.l1 = d->as.burst.l1;
                p.theta0 = d->as.burst.theta0;
                p.order = (float)d->as.burst.m;
                p.aspect = aspect;
                sg_apply_uniforms(UB_burst_params, SG_RANGE(p));
                break;
            }
            case SUMI_DEFORM_SPARK: {     // v0.13
                sg_apply_pipeline(r->pip_spark);
                spark_params_t p = {};
                p.centre[0] = d->as.spark.x;
                p.centre[1] = d->as.spark.y;
                p.amp = d->as.spark.amp;
                p.rk = d->as.spark.k;
                p.phase = d->as.spark.phase;
                p.rca = cosf(d->as.spark.theta0);
                p.rsa = sinf(d->as.spark.theta0);
                p.band = d->as.spark.band;
                p.stack = (float)d->as.spark.stack;
                p.profile = (float)d->as.spark.profile;
                p.stage = (float)d->as.spark.stage;
                p.aspect = aspect;
                sg_apply_uniforms(UB_spark_params, SG_RANGE(p));
                break;
            }
            case SUMI_DEFORM_CHIRIKOV: {  // v0.14
                sg_apply_pipeline(r->pip_chirikov);
                chirikov_params_t p = {};
                p.centre[0] = d->as.chirikov.x;
                p.centre[1] = d->as.chirikov.y;
                p.amp = d->as.chirikov.amp;
                p.rk = d->as.chirikov.k;
                p.phase = d->as.chirikov.phase;
                p.eps = d->as.chirikov.eps;
                p.stage = (float)d->as.chirikov.stage;
                p.aspect = aspect;
                sg_apply_uniforms(UB_chirikov_params, SG_RANGE(p));
                break;
            }
            case SUMI_DEFORM_RIPPLE: {
                sg_apply_pipeline(r->pip_ripple);
                ripple_params_t p = {};
                p.amp = d->as.ripple.amp;
                p.rk = d->as.ripple.k;
                p.phase = d->as.ripple.phase;
                p.rca = cosf(d->as.ripple.angle);
                p.rsa = sinf(d->as.ripple.angle);
                p.aspect = aspect;
                sg_apply_uniforms(UB_ripple_params, SG_RANGE(p));
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
            if (bind_cells) {                  // step 43: the cells pass reads its index map too
                bind.views[VIEW_tex_cells] = r->cells_tex;
                bind.samplers[SMP_smp_cells] = r->sampler_nearest;
            }
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
    run_composite(r, r->pip_composite, r->dip_fade, true);
    sg_end_pass();
    sg_commit();   // presents the drawable on Metal
    sumi_swapchain_frame_done(r->swapchain);   // presents on D3D11
    sumi_swapchain_frame_pool_pop(r->swapchain, pool);
}

// §4.6 cross-backend regression support (--field-dump): synchronous readback
// of the CURRENT field texture as raw RGBA16F. Test-only path — it may block
// (bounded), so it must never be called from the performance loop. The caller
// must have rendered (sg_commit flushed) before calling.
// Non-blocking halves (Phase 5 §5): the web host cannot block on a GPU map —
// mapAsync completes only when control returns to the browser's event loop —
// so the read is split into begin + poll-across-frames. The synchronous
// form below is these two plus a bounded yield loop (unchanged behaviour).
bool sumi_renderer_read_field_begin(sumi_renderer_t* r) {
    if (!r) return false;
    if (r->pending_idx >= 0 || r->export_pending) return false;   // the print / an export owns the machinery
    void* pool = sumi_swapchain_frame_pool_push(r->swapchain);
    const bool ok = sumi_swapchain_readback_begin(r->swapchain, r->field_img[r->cur],
                                                  r->sim_width, r->sim_height, 8);
    sumi_swapchain_frame_pool_pop(r->swapchain, pool);
    return ok;
}

int sumi_renderer_read_field_poll(sumi_renderer_t* r, uint8_t* out_rgba16f, size_t capacity,
                                  uint32_t* out_w, uint32_t* out_h) {
    if (!r) return 0;
    if (out_w) *out_w = r->sim_width;
    if (out_h) *out_h = r->sim_height;
    const size_t bytes = (size_t)r->sim_width * r->sim_height * 8;
    if (!out_rgba16f) return 1;              // size query: never consumes
    if (capacity < bytes) return 0;
    void* pool = sumi_swapchain_frame_pool_push(r->swapchain);
    const int st = sumi_swapchain_readback_poll(r->swapchain, out_rgba16f, bytes);
    sumi_swapchain_frame_pool_pop(r->swapchain, pool);
    return st;
}

bool sumi_renderer_read_field(sumi_renderer_t* r, uint8_t* out_rgba16f, size_t capacity,
                              uint32_t* out_w, uint32_t* out_h) {
    if (!r) return false;
    if (out_w) *out_w = r->sim_width;
    if (out_h) *out_h = r->sim_height;
    if (!out_rgba16f) return true;   // size query
    const size_t bytes = (size_t)r->sim_width * r->sim_height * 8;
    if (capacity < bytes) return false;
    if (!sumi_renderer_read_field_begin(r)) return false;
    for (int i = 0; i < 5000; i++) {   // bounded ~5 s wait
        const int st = sumi_renderer_read_field_poll(r, out_rgba16f, capacity, nullptr, nullptr);
        if (st == 2) return true;
        if (st == 0) return false;
        sumi_swapchain_yield(r->swapchain);
    }
    return false;
}

bool sumi_renderer_dip_ready(const sumi_renderer_t* r) {
    if (r && r->export_pending) return false;   // step 43: an export owns the readback slot
    if (!r) return false;
    // v0.6: only a readback in flight blocks a dip; two unread prints recycle
    // the older one (snapshot_print). Refusal is therefore a few frames long.
    return r->pending_idx < 0;
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

// ---- step 43 (QOL §4): prints at any size -------------------------------------
static void destroy_export(sumi_renderer_t* r) {
    if (r->export_attach.id)    { sg_destroy_view(r->export_attach); r->export_attach.id = 0; }
    if (r->export_img.id)       { sumi_swapchain_release_image(r->swapchain, r->export_img); sg_destroy_image(r->export_img); r->export_img.id = 0; }
    if (r->export_field_tex.id) { sg_destroy_view(r->export_field_tex); r->export_field_tex.id = 0; }
    if (r->export_field_img.id) { sg_destroy_image(r->export_field_img); r->export_field_img.id = 0; }
    r->export_pending = false;
}

void sumi_renderer_set_visuals(sumi_renderer_t* r, const sumi_render_visuals_t* visuals) {
    if (r && visuals) r->visuals = *visuals;
}

bool sumi_renderer_export_begin(sumi_renderer_t* r, const uint8_t* field, uint32_t fw, uint32_t fh,
                                uint32_t w, uint32_t h, uint32_t flags) {
    if (!r || w == 0 || h == 0 || w > SUMI_EXPORT_MAX_DIM || h > SUMI_EXPORT_MAX_DIM) return false;
    if (r->pending_idx >= 0 || r->export_pending) return false;      // one readback at a time
    if (field && (fw == 0 || fh == 0 || fw > SUMI_SIM_MAX_DIM || fh > SUMI_SIM_MAX_DIM)) return false;
    destroy_export(r);
    void* pool = sumi_swapchain_frame_pool_push(r->swapchain);
    sg_view field_view = r->field_tex[r->cur];
    uint32_t vw = r->sim_width, vh = r->sim_height;
    bool ok = true;
    if (field) {                                                        // a snapshot's field, uploaded for the pass
        sg_image_desc fd = {};
        fd.width = (int)fw; fd.height = (int)fh;
        fd.pixel_format = SG_PIXELFORMAT_RGBA16F;
        fd.data.mip_levels[0].ptr = field;
        fd.data.mip_levels[0].size = (size_t)fw * fh * 8u;
        fd.label = "export-field";
        r->export_field_img = sg_make_image(&fd);
        sg_view_desc vd = {};
        vd.texture.image = r->export_field_img;
        vd.label = "export-field-tex";
        r->export_field_tex = sg_make_view(&vd);
        ok = sg_query_image_state(r->export_field_img) == SG_RESOURCESTATE_VALID && sg_query_view_state(r->export_field_tex) == SG_RESOURCESTATE_VALID;
        field_view = r->export_field_tex; vw = fw; vh = fh;
    }
    if (ok) {
        sg_image_desc img = {};
        img.usage.color_attachment = true;
        img.width = (int)w; img.height = (int)h;
        img.pixel_format = SG_PIXELFORMAT_RGBA8;
        img.sample_count = 1;
        img.label = "export-target";
        sumi_swapchain_prepare_image(r->swapchain, &img);              // readback-capable (WebGPU)
        r->export_img = sg_make_image(&img);
        sg_view_desc ad = {};
        ad.color_attachment.image = r->export_img;
        ad.label = "export-attach";
        r->export_attach = sg_make_view(&ad);
        ok = sg_query_image_state(r->export_img) == SG_RESOURCESTATE_VALID && sg_query_view_state(r->export_attach) == SG_RESOURCESTATE_VALID;
    }
    if (ok) {
        sg_pass pass = {};
        pass.action = r->field_action;
        pass.attachments.colors[0] = r->export_attach;
        pass.label = "export";
        sg_begin_pass(&pass);
        run_composite_ex(r, r->pip_composite_print, 0.0f, false, field_view, vw, vh, flags);   // un-rippled, like a dip
        sg_end_pass();
        sg_commit();
        ok = sumi_swapchain_readback_begin(r->swapchain, r->export_img, w, h, 4);
    }
    if (ok) { r->export_w = w; r->export_h = h; r->export_pending = true; }
    else { r_log(r, SUMI_LOG_WARN, "renderer: export could not start"); destroy_export(r); }
    sumi_swapchain_frame_pool_pop(r->swapchain, pool);
    return ok;
}

int sumi_renderer_export_poll(sumi_renderer_t* r, uint8_t* out_rgba8, size_t capacity, uint32_t* out_w, uint32_t* out_h) {
    if (!r || !r->export_pending) return 0;
    if (out_w) *out_w = r->export_w;
    if (out_h) *out_h = r->export_h;
    if (!out_rgba8) return 1;                                           // size query: never consumes
    const size_t bytes = (size_t)r->export_w * r->export_h * 4u;
    if (capacity < bytes) return 1;
    void* pool = sumi_swapchain_frame_pool_push(r->swapchain);
    const int st = sumi_swapchain_readback_poll(r->swapchain, out_rgba8, bytes);
    sumi_swapchain_frame_pool_pop(r->swapchain, pool);
    if (st == 2 || st == 0) destroy_export(r);                          // done, or failed: the slot is free again
    return st;
}

} // extern "C"
