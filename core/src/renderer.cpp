// renderer.cpp — sokol_gfx pass orchestration (PROJECT_SPEC.md §1).
// Phase 1: a single swapchain pass clearing to deep indigo. Ping-pong
// displacement targets and composite arrive in later steps.
//
// Note: this file includes sokol_gfx.h declarations only; the sokol
// implementation (SOKOL_IMPL) lives in swapchain_metal.mm because the Metal
// backend must be compiled as Objective-C++ (see DECISIONS.md).
#include "renderer.h"
#include "swapchain.h"
#include "log_levels.h"

#include <new>
#include <stdio.h>

// Deep indigo clear color (linear-ish sRGB values written to a non-sRGB
// BGRA8 swapchain in phase 1; color-space handling is a composite-step
// concern, see DECISIONS.md).
static const float SUMI_CLEAR_R = 0.055f;
static const float SUMI_CLEAR_G = 0.050f;
static const float SUMI_CLEAR_B = 0.220f;

struct sumi_renderer_t {
    sumi_swapchain_t* swapchain;
    sumi_log_fn       log_cb;
    void*             log_user;
    sg_pass_action    clear_action;
};

// Bridge sokol's logger to the host's sumi_log_fn.
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

extern "C" {

sumi_renderer_t* sumi_renderer_create(const sumi_config_t* config) {
    sumi_renderer_t* r = new (std::nothrow) sumi_renderer_t();
    if (!r) return nullptr;
    r->log_cb = config->log_cb;
    r->log_user = config->log_user;

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
        if (r->log_cb) r->log_cb(SUMI_LOG_ERROR, "renderer: sg_setup failed", r->log_user);
        sumi_swapchain_destroy(r->swapchain);
        delete r;
        return nullptr;
    }

    r->clear_action = {};
    r->clear_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    r->clear_action.colors[0].store_action = SG_STOREACTION_STORE;
    r->clear_action.colors[0].clear_value = { SUMI_CLEAR_R, SUMI_CLEAR_G, SUMI_CLEAR_B, 1.0f };
    return r;
}

void sumi_renderer_destroy(sumi_renderer_t* r) {
    if (!r) return;
    sg_shutdown();
    sumi_swapchain_destroy(r->swapchain);
    delete r;
}

void sumi_renderer_resize(sumi_renderer_t* r, uint32_t w, uint32_t h, float pixel_ratio) {
    if (!r) return;
    sumi_swapchain_resize(r->swapchain, w, h, pixel_ratio);
}

void sumi_renderer_render(sumi_renderer_t* r) {
    if (!r) return;
    sg_swapchain swapchain = sumi_swapchain_acquire(r->swapchain);
    if (!swapchain.metal.current_drawable) {
        return;   // zero-sized / occluded surface: skip the frame
    }
    sg_pass pass = {};
    pass.action = r->clear_action;
    pass.swapchain = swapchain;
    sg_begin_pass(&pass);
    sg_end_pass();
    sg_commit();   // presents the drawable on Metal
    sumi_swapchain_frame_done(r->swapchain);
}

} // extern "C"
