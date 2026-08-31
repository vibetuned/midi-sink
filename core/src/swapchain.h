// Internal swapchain contract between renderer.cpp and the per-backend
// swapchain_*.{mm,cpp} files. This header may use sokol types — it is only
// included below the renderer layer (working rule 2).
//
// Per-backend surface ownership contract: see PROJECT_SPEC.md §5.1. For Metal
// the core owns MTLDevice/queue and drives the CAMetalLayer it was handed.
#pragma once

#include "sumi_core.h"
#include "sokol_gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sumi_swapchain_t sumi_swapchain_t;

// Takes the native surface handle from the config (CAMetalLayer* on Metal),
// creates the GPU device, and configures the layer. Returns NULL on failure
// (reported through the config's log callback).
sumi_swapchain_t* sumi_swapchain_create(const sumi_config_t* config);
void              sumi_swapchain_destroy(sumi_swapchain_t* sc);

// sg_environment for sg_setup() (device pointer + default pixel formats).
sg_environment    sumi_swapchain_environment(const sumi_swapchain_t* sc);

// Acquire the frame's drawable. On failure (e.g. zero-sized layer) the
// returned struct has a NULL metal.current_drawable — skip the frame.
// The drawable stays retained until sumi_swapchain_frame_done().
sg_swapchain      sumi_swapchain_acquire(sumi_swapchain_t* sc);
void              sumi_swapchain_frame_done(sumi_swapchain_t* sc);

void              sumi_swapchain_resize(sumi_swapchain_t* sc, uint32_t w, uint32_t h, float pixel_ratio);

#ifdef __cplusplus
}
#endif
