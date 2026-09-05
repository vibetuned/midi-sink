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

#include <atomic>
#include <new>
#include <string.h>

struct sumi_swapchain_t {
    CAMetalLayer*        layer;
    id<MTLDevice>        device;
    id<CAMetalDrawable>  current_drawable;   // retained from acquire until frame_done
    sumi_log_fn          log_cb;
    void*                log_user;

    // Paper-dip readback (render thread starts/polls; GPU completes async).
    id<MTLCommandQueue>  blit_queue;
    id<MTLBuffer>        readback_buf;
    uint32_t             readback_w, readback_h, readback_bpp;
    std::atomic<int>     readback_state;     // 0 idle, 1 in flight, 2 gpu-done
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

    // Backend validation lives here, next to the backend it validates: this
    // build's swapchain is Metal-only (engine.cpp stays backend-agnostic).
    if (config->backend != SUMI_BACKEND_METAL) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_metal: this build supports SUMI_BACKEND_METAL only");
        delete sc;
        return nullptr;
    }

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

bool sumi_swapchain_readback_begin(sumi_swapchain_t* sc, sg_image img,
                                   uint32_t w, uint32_t h, uint32_t bytes_per_pixel) {
    if (!sc || img.id == 0 || w == 0 || h == 0 ||
        (bytes_per_pixel != 4 && bytes_per_pixel != 8)) return false;
    const sg_mtl_image_info info = sg_mtl_query_image_info(img);
    const void* mtl_texture = info.tex[info.active_slot >= 0 ? info.active_slot : 0];
    if (!mtl_texture) return false;
    int expected = 0;
    if (!sc->readback_state.compare_exchange_strong(expected, 1)) {
        return false;   // one readback in flight at a time
    }
    // Commit the blit on the RENDERER'S queue: command buffers on one queue
    // execute in commit order, so the blit is ordered after the snapshot
    // pass with no cross-queue synchronization.
    const void* mtl_queue = sg_mtl_command_queue();
    sc->blit_queue = mtl_queue ? (__bridge id<MTLCommandQueue>)mtl_queue
                               : (sc->blit_queue ?: [sc->device newCommandQueue]);
    const size_t bytes = (size_t)w * h * bytes_per_pixel;
    if (!sc->readback_buf || sc->readback_buf.length < bytes) {
        sc->readback_buf = [sc->device newBufferWithLength:bytes
                                                   options:MTLResourceStorageModeShared];
    }
    if (!sc->readback_buf) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_metal: readback buffer allocation failed");
        sc->readback_state.store(0);
        return false;
    }
    sc->readback_w = w;
    sc->readback_h = h;
    sc->readback_bpp = bytes_per_pixel;

    id<MTLTexture> tex = (__bridge id<MTLTexture>)mtl_texture;
    id<MTLCommandBuffer> cmd = [sc->blit_queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
    [blit copyFromTexture:tex
              sourceSlice:0
              sourceLevel:0
             sourceOrigin:MTLOriginMake(0, 0, 0)
               sourceSize:MTLSizeMake(w, h, 1)
                 toBuffer:sc->readback_buf
        destinationOffset:0
   destinationBytesPerRow:(NSUInteger)w * bytes_per_pixel
 destinationBytesPerImage:bytes];
    [blit endEncoding];
    std::atomic<int>* state = &sc->readback_state;
    [cmd addCompletedHandler:^(id<MTLCommandBuffer> cb) {
        (void)cb;
        state->store(2, std::memory_order_release);
    }];
    [cmd commit];
    return true;
}

int sumi_swapchain_readback_poll(sumi_swapchain_t* sc, uint8_t* dst, size_t dst_size) {
    if (!sc) return 0;
    const int state = sc->readback_state.load(std::memory_order_acquire);
    if (state != 2) return state;
    const size_t bytes = (size_t)sc->readback_w * sc->readback_h * sc->readback_bpp;
    if (dst && dst_size >= bytes) {
        memcpy(dst, sc->readback_buf.contents, bytes);
    }
    sc->readback_state.store(0);
    return 2;
}

void sumi_swapchain_yield(sumi_swapchain_t* sc) {
    (void)sc;
    struct timespec ts = {0, 1000000};   // 1 ms
    nanosleep(&ts, nullptr);
}

void sumi_swapchain_destroy(sumi_swapchain_t* sc) {
    if (!sc) return;
    // Let an in-flight blit finish before tearing down (bounded: one blit).
    for (int i = 0; i < 1000 && sc->readback_state.load() == 1; i++) {
        struct timespec ts = {0, 1000000};   // 1 ms
        nanosleep(&ts, nullptr);
    }
    sc->blit_queue = nil;
    sc->readback_buf = nil;
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

// Readback-capable images (swapchain.h): nothing to do on this backend — its
// textures are already readable by the copy path used above.
void sumi_swapchain_prepare_image(sumi_swapchain_t* sc, sg_image_desc* desc) {
    (void)sc;
    (void)desc;
}

void sumi_swapchain_release_image(sumi_swapchain_t* sc, sg_image img) {
    (void)sc;
    (void)img;
}

} // extern "C"
