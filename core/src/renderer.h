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
// Drains the deformation queue as ping-pong passes, then composites the
// current field to the swapchain. Does not clear the queue.
void             sumi_renderer_render (sumi_renderer_t* r, const sumi_deform_queue_t* deforms);

#ifdef __cplusplus
}
#endif
