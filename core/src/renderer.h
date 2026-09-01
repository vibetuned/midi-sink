// Internal renderer interface. Deliberately sokol-free: everything above
// renderer.cpp / swapchain_*.{mm,cpp} must stay ignorant of sokol (working rule 2).
#pragma once

#include "sumi_core.h"
#include "displacement.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sumi_renderer_t sumi_renderer_t;

sumi_renderer_t* sumi_renderer_create (const sumi_config_t* config, float sim_scale);
void             sumi_renderer_destroy(sumi_renderer_t* r);
void             sumi_renderer_resize (sumi_renderer_t* r, uint32_t w, uint32_t h, float pixel_ratio);
// Recreates the simulation targets when the scale actually changed.
void             sumi_renderer_set_sim_scale(sumi_renderer_t* r, float sim_scale);

// Live composite parameters (§4.5), passed each frame.
typedef struct {
    uint32_t palette_id;      // 0 sumi, 1 indigo, 2 ochre
    float    roughness;       // washi fiber/grain strength 0..1
    float    palette_morph;   // 0..1 blend toward the next palette
} sumi_render_visuals_t;

// Drains the deformation queue as ping-pong passes, then composites the
// current field to the swapchain. A RESET pass first snapshots the composite
// into the print target and schedules the async readback (§5.3 paper dip).
// Does not clear the queue.
void             sumi_renderer_render (sumi_renderer_t* r, const sumi_deform_queue_t* deforms,
                                       double dt, const sumi_render_visuals_t* visuals);

// True when a paper dip can be accepted: a free print buffer exists and no
// readback is in flight (spec §5.3 double-buffer contract).
bool             sumi_renderer_dip_ready(const sumi_renderer_t* r);

// Newest completed paper-dip print (RGBA8, tightly packed, sRGB-encoded).
// pixels == NULL: query size only (does not consume). A successful data read
// consumes the buffer (frees it for the next dip). False if no print is ready.
bool             sumi_renderer_read_print(sumi_renderer_t* r, uint8_t* pixels, size_t capacity,
                                          uint32_t* out_w, uint32_t* out_h);

#ifdef __cplusplus
}
#endif
