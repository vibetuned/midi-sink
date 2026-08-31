// Internal renderer interface. Deliberately sokol-free: everything above
// renderer.cpp / swapchain_*.{mm,cpp} must stay ignorant of sokol (working rule 2).
#pragma once

#include "sumi_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sumi_renderer_t sumi_renderer_t;

sumi_renderer_t* sumi_renderer_create (const sumi_config_t* config);
void             sumi_renderer_destroy(sumi_renderer_t* r);
void             sumi_renderer_resize (sumi_renderer_t* r, uint32_t w, uint32_t h, float pixel_ratio);
void             sumi_renderer_render (sumi_renderer_t* r);

#ifdef __cplusplus
}
#endif
