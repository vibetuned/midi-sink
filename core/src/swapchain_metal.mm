// swapchain_metal.mm — CAMetalLayer device/drawable management (§5.1).
//
// The core owns the MTLDevice and drives nextDrawable/drawableSize for the
// CAMetalLayer* handed to sumi_create. The host must not touch the layer
// between sumi_create and sumi_destroy.
//
// This translation unit also hosts the sokol_gfx implementation: the Metal
// backend of sokol_gfx must be compiled as Objective-C++ with ARC, which a
// plain .cpp cannot provide (see DECISIONS.md). sokol stays confined to
// renderer.cpp / swapchain_* per the working rules.
#if !defined(__OBJC__) || !__has_feature(objc_arc)
#error "swapchain_metal.mm must be compiled as Objective-C++ with ARC (-fobjc-arc)"
#endif

// sokol_gfx.h's implementation section has no re-inclusion guard, so this TU
// must include it exactly once — swapchain.h pulls it in below.
#define SOKOL_IMPL
#define SOKOL_METAL
#include "swapchain.h"
#include "log_levels.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <new>

struct sumi_swapchain_t {
    CAMetalLayer*        layer;
    id<MTLDevice>        device;
    id<CAMetalDrawable>  current_drawable;   // retained from acquire until frame_done
    sumi_log_fn          log_cb;
    void*                log_user;
};

static void sc_log(const sumi_swapchain_t* sc, int level, const char* msg) {
    if (sc && sc->log_cb) sc->log_cb(level, msg, sc->log_user);
}

extern "C" {

sumi_swapchain_t* sumi_swapchain_create(const sumi_config_t* config) {
    sumi_swapchain_t* sc = new (std::nothrow) sumi_swapchain_t();
    if (!sc) return nullptr;
    sc->log_cb = config->log_cb;
    sc->log_user = config->log_user;

    sc->layer = (__bridge CAMetalLayer*)config->native_surface_handle;
    if (![sc->layer isKindOfClass:[CAMetalLayer class]]) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_metal: native_surface_handle is not a CAMetalLayer*");
        delete sc;
        return nullptr;
    }

    sc->device = MTLCreateSystemDefaultDevice();
    if (!sc->device) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_metal: MTLCreateSystemDefaultDevice failed");
        delete sc;
        return nullptr;
    }

    sc->layer.device = sc->device;
    sc->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    sc->layer.framebufferOnly = YES;
    sc->layer.drawableSize = CGSizeMake((CGFloat)config->width, (CGFloat)config->height);
    if (config->pixel_ratio > 0.0f) {
        sc->layer.contentsScale = (CGFloat)config->pixel_ratio;
    }
    return sc;
}

void sumi_swapchain_destroy(sumi_swapchain_t* sc) {
    if (!sc) return;
    sc->current_drawable = nil;
    sc->layer.device = nil;   // detach our device; the host destroys the layer itself
    sc->layer = nil;
    sc->device = nil;
    delete sc;
}

sg_environment sumi_swapchain_environment(const sumi_swapchain_t* sc) {
    sg_environment env = {};
    env.defaults.color_format = SG_PIXELFORMAT_BGRA8;
    env.defaults.depth_format = SG_PIXELFORMAT_NONE;
    env.defaults.sample_count = 1;
    env.metal.device = (__bridge const void*)sc->device;
    return env;
}

sg_swapchain sumi_swapchain_acquire(sumi_swapchain_t* sc) {
    sg_swapchain swapchain = {};
    if (!sc) return swapchain;
    sc->current_drawable = [sc->layer nextDrawable];
    if (!sc->current_drawable) {
        return swapchain;
    }
    // Report the drawable's actual size (not the requested one) so the pass
    // dimensions can never disagree with the texture during live resize.
    swapchain.width = (int)sc->current_drawable.texture.width;
    swapchain.height = (int)sc->current_drawable.texture.height;
    swapchain.sample_count = 1;
    swapchain.color_format = SG_PIXELFORMAT_BGRA8;
    swapchain.depth_format = SG_PIXELFORMAT_NONE;
    swapchain.metal.current_drawable = (__bridge const void*)sc->current_drawable;
    return swapchain;
}

void sumi_swapchain_frame_done(sumi_swapchain_t* sc) {
    if (!sc) return;
    sc->current_drawable = nil;
}

// objc_autoreleasePoolPush/Pop are the stable public runtime calls that
// @autoreleasepool compiles to; used directly so the pool can span the
// C++ renderer's frame scope.
extern void* objc_autoreleasePoolPush(void);
extern void  objc_autoreleasePoolPop(void* pool);

void* sumi_swapchain_frame_pool_push(sumi_swapchain_t* sc) {
    (void)sc;
    return objc_autoreleasePoolPush();
}

void sumi_swapchain_frame_pool_pop(sumi_swapchain_t* sc, void* pool) {
    (void)sc;
    if (pool) objc_autoreleasePoolPop(pool);
}

void sumi_swapchain_resize(sumi_swapchain_t* sc, uint32_t w, uint32_t h, float pixel_ratio) {
    if (!sc || w == 0 || h == 0) return;
    sc->layer.drawableSize = CGSizeMake((CGFloat)w, (CGFloat)h);
    if (pixel_ratio > 0.0f) {
        sc->layer.contentsScale = (CGFloat)pixel_ratio;
    }
}

} // extern "C"
