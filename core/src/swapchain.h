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

// Frame-scoped autorelease pool. On Metal every pass encoder / drawable is an
// autoreleased ObjC object; without a per-frame pool they accumulate without
// bound in a plain C render loop (macOS and iOS alike). The renderer wraps
// each frame in push/pop; non-ObjC backends implement these as no-ops.
void*             sumi_swapchain_frame_pool_push(sumi_swapchain_t* sc);
void              sumi_swapchain_frame_pool_pop (sumi_swapchain_t* sc, void* pool);

// Async GPU->CPU readback for the paper-dip print (§5.3) and the §4.6 field
// dump. Backend-neutral seam: the image is passed as its sg_image handle and
// each swapchain TU performs its own backend query (sg_mtl_query_image_info /
// sg_d3d11_query_image_info) and copy scheduling (Metal: blit into a shared
// buffer on the renderer's queue; D3D11: CopyResource into a staging
// texture). Never blocks the render loop; the caller must have flushed the
// producing passes (sg_commit) so the copy is ordered after them.
// `bytes_per_pixel`: 4 for the RGBA8 print, 8 for the RGBA16F field.
// Returns false if a readback is already in flight.
bool              sumi_swapchain_readback_begin(sumi_swapchain_t* sc,
                                                sg_image img,
                                                uint32_t w, uint32_t h,
                                                uint32_t bytes_per_pixel);
// Poll from the render thread: 0 = idle, 1 = in flight, 2 = completed and
// copied into dst (dst_size must be >= w*h*bytes_per_pixel from
// readback_begin; rows are tightly packed, row 0 = top per §4.6).
int               sumi_swapchain_readback_poll(sumi_swapchain_t* sc,
                                               uint8_t* dst, size_t dst_size);
// ~1 ms sleep for bounded readback waits outside the render loop (the
// --field-dump test path). Implemented per TU (nanosleep / Sleep).
void              sumi_swapchain_yield(sumi_swapchain_t* sc);

#ifdef __cplusplus
}
#endif
