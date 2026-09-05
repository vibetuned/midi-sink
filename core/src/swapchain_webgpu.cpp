// swapchain_webgpu.cpp — the WebGPU "swapchain" (Phase 5 §5; the fourth
// swapchain TU, DECISIONS_4 #15/#16). Hosts the sokol_gfx implementation for
// the WebGPU backend (SOKOL_WGPU) exactly as the other three TUs host theirs.
//
// Ownership (§5.1, the web row): a browser can only create the adapter and
// device ASYNCHRONOUSLY, so the HOST page does that and hands the core the
// device (imported into the wasm as an emdawnwebgpu handle) plus the canvas
// selector and the canvas's preferred format — sumi_webgpu_surface_t. From
// there the core owns the surface: it creates it from the selector,
// configures it, acquires the frame texture, reconfigures on resize and
// releases it all at destroy. The browser presents; there is no
// wgpuSurfacePresent on Emscripten.
//
// Readback (§5.3 print, §4.6 field): copyTextureToBuffer + mapAsync, which
// completes only when control returns to the event loop — so poll() drives
// wgpuInstanceProcessEvents and the renderer's begin/poll split carries the
// field dump across frames. sokol never gives its textures CopySrc usage, so
// the two readback-bound targets are created HERE with the right usage and
// injected (sg_image_desc.wgpu_texture) — the prepare/release seam.
//
// Orientation (§4.6): WebGPU's texture origin is top-left like Metal and
// D3D11 — no flip anywhere in this file, the shaders' non-GL dialects apply.
#define SOKOL_IMPL
#define SOKOL_WGPU
#include "swapchain.h"
#include "log_levels.h"

#include <new>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static WGPUStringView sc_sv(const char* s) {
    WGPUStringView v;
    v.data = s;
    v.length = s ? WGPU_STRLEN : 0;
    return v;
}

struct sumi_swapchain_t {
    WGPUInstance      instance;
    WGPUDevice        device;          // imported by the host; one ref held here
    WGPUSurface       surface;
    WGPUTextureFormat surface_format;
    sg_pixel_format   sg_color_format;
    uint32_t          width, height;
    WGPUTexture       cur_tex;         // surface texture of the frame in flight
    WGPUTextureView   cur_view;
    sumi_log_fn       log_cb;
    void*             log_user;

    // Readback: one copy in flight at a time (§5.3 contract).
    WGPUBuffer        rb_buf;
    size_t            rb_cap;
    uint32_t          rb_w, rb_h, rb_bpp, rb_padded_row;
    int               rb_state;        // 0 idle, 1 in flight, 2 mapped, 3 failed
};

static void sc_log(const sumi_swapchain_t* sc, int level, const char* msg) {
    if (sc && sc->log_cb) sc->log_cb(level, msg, sc->log_user);
}

static void sc_configure(sumi_swapchain_t* sc) {
    WGPUSurfaceConfiguration conf = {};
    conf.device = sc->device;
    conf.format = sc->surface_format;
    conf.usage = WGPUTextureUsage_RenderAttachment;
    conf.width = sc->width;
    conf.height = sc->height;
    conf.alphaMode = WGPUCompositeAlphaMode_Opaque;
    conf.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(sc->surface, &conf);
}

static WGPUTextureFormat sc_wgpu_format(sg_pixel_format f) {
    switch (f) {
        case SG_PIXELFORMAT_RGBA16F: return WGPUTextureFormat_RGBA16Float;
        case SG_PIXELFORMAT_RGBA8:   return WGPUTextureFormat_RGBA8Unorm;
        case SG_PIXELFORMAT_BGRA8:   return WGPUTextureFormat_BGRA8Unorm;
        case SG_PIXELFORMAT_RGBA32F: return WGPUTextureFormat_RGBA32Float;
        default:                     return WGPUTextureFormat_Undefined;
    }
}

extern "C" {

sumi_swapchain_t* sumi_swapchain_create(const sumi_config_t* config) {
    sumi_swapchain_t* sc = new (std::nothrow) sumi_swapchain_t();
    if (!sc) return nullptr;
    sc->log_cb = config->log_cb;
    sc->log_user = config->log_user;

    if (config->backend != SUMI_BACKEND_WEBGPU) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_webgpu: this build supports SUMI_BACKEND_WEBGPU only");
        delete sc;
        return nullptr;
    }
    const sumi_webgpu_surface_t* surf = (const sumi_webgpu_surface_t*)config->native_surface_handle;
    if (!surf || !surf->device || !surf->canvas_selector || !surf->canvas_selector[0]) {
        sc_log(sc, SUMI_LOG_ERROR,
               "swapchain_webgpu: native_surface_handle must point to a sumi_webgpu_surface_t "
               "with a device and a canvas selector");
        delete sc;
        return nullptr;
    }
    sc->device = (WGPUDevice)surf->device;
    wgpuDeviceAddRef(sc->device);

    WGPUInstanceDescriptor idesc = {};
    sc->instance = wgpuCreateInstance(&idesc);
    if (!sc->instance) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_webgpu: wgpuCreateInstance failed");
        wgpuDeviceRelease(sc->device);
        delete sc;
        return nullptr;
    }

    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas = {};
    canvas.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    canvas.selector = sc_sv(surf->canvas_selector);
    WGPUSurfaceDescriptor sdesc = {};
    sdesc.nextInChain = &canvas.chain;
    sc->surface = wgpuInstanceCreateSurface(sc->instance, &sdesc);
    if (!sc->surface) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_webgpu: wgpuInstanceCreateSurface failed (bad canvas selector?)");
        wgpuInstanceRelease(sc->instance);
        wgpuDeviceRelease(sc->device);
        delete sc;
        return nullptr;
    }

    // The canvas's preferred format (navigator.gpu.getPreferredCanvasFormat),
    // chosen by the host: bgra8unorm on most platforms, rgba8unorm on Android.
    if (surf->color_format == SUMI_WEBGPU_FORMAT_RGBA8) {
        sc->surface_format = WGPUTextureFormat_RGBA8Unorm;
        sc->sg_color_format = SG_PIXELFORMAT_RGBA8;
    } else {
        sc->surface_format = WGPUTextureFormat_BGRA8Unorm;
        sc->sg_color_format = SG_PIXELFORMAT_BGRA8;
    }
    sc->width = config->width;
    sc->height = config->height;
    sc_configure(sc);

    char buf[192];
    snprintf(buf, sizeof(buf), "swapchain_webgpu: surface %s %ux%u (%s)", surf->canvas_selector,
             sc->width, sc->height, surf->color_format == SUMI_WEBGPU_FORMAT_RGBA8 ? "rgba8" : "bgra8");
    sc_log(sc, SUMI_LOG_INFO, buf);
    return sc;
}

void sumi_swapchain_destroy(sumi_swapchain_t* sc) {
    if (!sc) return;
    if (sc->cur_view) wgpuTextureViewRelease(sc->cur_view);
    if (sc->cur_tex) wgpuTextureRelease(sc->cur_tex);
    if (sc->rb_buf) {
        if (sc->rb_state == 2) wgpuBufferUnmap(sc->rb_buf);
        wgpuBufferRelease(sc->rb_buf);
    }
    if (sc->surface) {
        wgpuSurfaceUnconfigure(sc->surface);
        wgpuSurfaceRelease(sc->surface);
    }
    if (sc->device) wgpuDeviceRelease(sc->device);
    if (sc->instance) wgpuInstanceRelease(sc->instance);
    delete sc;
}

sg_environment sumi_swapchain_environment(const sumi_swapchain_t* sc) {
    sg_environment env = {};
    env.defaults.color_format = sc ? sc->sg_color_format : SG_PIXELFORMAT_BGRA8;
    env.defaults.depth_format = SG_PIXELFORMAT_NONE;
    env.defaults.sample_count = 1;
    env.wgpu.device = sc ? (const void*)sc->device : nullptr;
    return env;
}

sg_swapchain sumi_swapchain_acquire(sumi_swapchain_t* sc) {
    sg_swapchain swapchain = {};
    if (!sc || sc->width == 0 || sc->height == 0) return swapchain;
    WGPUSurfaceTexture st = {};
    wgpuSurfaceGetCurrentTexture(sc->surface, &st);
    switch (st.status) {
        case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
        case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
            break;
        default:
            // Outdated / lost / timeout: reconfigure and skip this frame.
            if (st.texture) wgpuTextureRelease(st.texture);
            sc_configure(sc);
            return swapchain;
    }
    sc->cur_tex = st.texture;
    sc->cur_view = wgpuTextureCreateView(sc->cur_tex, nullptr);
    if (!sc->cur_view) {
        wgpuTextureRelease(sc->cur_tex);
        sc->cur_tex = nullptr;
        return swapchain;
    }
    swapchain.width = (int)wgpuTextureGetWidth(sc->cur_tex);
    swapchain.height = (int)wgpuTextureGetHeight(sc->cur_tex);
    swapchain.sample_count = 1;
    swapchain.color_format = sc->sg_color_format;
    swapchain.depth_format = SG_PIXELFORMAT_NONE;
    swapchain.wgpu.render_view = (const void*)sc->cur_view;
    return swapchain;
}

void sumi_swapchain_frame_done(sumi_swapchain_t* sc) {
    if (!sc) return;
    // The browser presents the canvas texture at the end of the task; we only
    // drop our references (there is no wgpuSurfacePresent on Emscripten).
    if (sc->cur_view) { wgpuTextureViewRelease(sc->cur_view); sc->cur_view = nullptr; }
    if (sc->cur_tex) { wgpuTextureRelease(sc->cur_tex); sc->cur_tex = nullptr; }
}

// Pool hooks are Metal plumbing; on WebGPU the per-frame hook is where the
// instance's callback queue is drained (map completions land here or in poll).
void* sumi_swapchain_frame_pool_push(sumi_swapchain_t* sc) {
    if (sc && sc->instance) wgpuInstanceProcessEvents(sc->instance);
    return nullptr;
}

void sumi_swapchain_frame_pool_pop(sumi_swapchain_t* sc, void* pool) {
    (void)sc;
    (void)pool;
}

void sumi_swapchain_resize(sumi_swapchain_t* sc, uint32_t w, uint32_t h, float pixel_ratio) {
    (void)pixel_ratio;   // the host sizes the canvas in device pixels
    if (!sc || w == 0 || h == 0) return;
    if (w == sc->width && h == sc->height) return;
    sc->width = w;
    sc->height = h;
    sc_configure(sc);
}

// ---- readback-capable images (prepare/release seam) -------------------------

void sumi_swapchain_prepare_image(sumi_swapchain_t* sc, sg_image_desc* desc) {
    if (!sc || !desc) return;
    const WGPUTextureFormat fmt = sc_wgpu_format(desc->pixel_format);
    if (fmt == WGPUTextureFormat_Undefined) return;
    WGPUTextureDescriptor td = {};
    td.label = sc_sv(desc->label);
    td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding |
               WGPUTextureUsage_CopySrc;
    td.dimension = WGPUTextureDimension_2D;
    td.size.width = (uint32_t)desc->width;
    td.size.height = (uint32_t)desc->height;
    td.size.depthOrArrayLayers = 1;
    td.format = fmt;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    WGPUTexture tex = wgpuDeviceCreateTexture(sc->device, &td);
    if (!tex) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_webgpu: readback-capable texture creation failed");
        return;   // sokol creates its own (unreadable) texture; readback will then fail loudly
    }
    desc->wgpu_texture = (const void*)tex;   // sokol AddRefs on injection; we keep ours
}

void sumi_swapchain_release_image(sumi_swapchain_t* sc, sg_image img) {
    if (!sc || img.id == 0) return;
    const sg_wgpu_image_info info = sg_wgpu_query_image_info(img);
    if (info.tex) wgpuTextureRelease((WGPUTexture)info.tex);   // our creation ref
}

// ---- async readback ------------------------------------------------------------

static void sc_map_cb(WGPUMapAsyncStatus status, WGPUStringView msg, void* ud1, void* ud2) {
    (void)msg;
    (void)ud2;
    sumi_swapchain_t* sc = (sumi_swapchain_t*)ud1;
    if (!sc) return;
    sc->rb_state = (status == WGPUMapAsyncStatus_Success) ? 2 : 3;
    if (status != WGPUMapAsyncStatus_Success) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_webgpu: buffer map failed");
    }
}

bool sumi_swapchain_readback_begin(sumi_swapchain_t* sc, sg_image img,
                                   uint32_t w, uint32_t h, uint32_t bytes_per_pixel) {
    if (!sc || img.id == 0 || w == 0 || h == 0 ||
        (bytes_per_pixel != 4 && bytes_per_pixel != 8)) return false;
    if (sc->rb_state != 0) return false;   // one readback in flight at a time
    const sg_wgpu_image_info info = sg_wgpu_query_image_info(img);
    if (!info.tex) return false;

    // WebGPU requires bytesPerRow to be a multiple of 256 for buffer copies.
    const uint32_t row = w * bytes_per_pixel;
    const uint32_t padded = (row + 255u) & ~255u;
    const size_t size = (size_t)padded * h;
    if (!sc->rb_buf || sc->rb_cap < size) {
        if (sc->rb_buf) wgpuBufferRelease(sc->rb_buf);
        WGPUBufferDescriptor bd = {};
        bd.label = sc_sv("sumi-readback");
        bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
        bd.size = size;
        sc->rb_buf = wgpuDeviceCreateBuffer(sc->device, &bd);
        sc->rb_cap = sc->rb_buf ? size : 0;
        if (!sc->rb_buf) {
            sc_log(sc, SUMI_LOG_ERROR, "swapchain_webgpu: readback buffer allocation failed");
            return false;
        }
    }
    sc->rb_w = w;
    sc->rb_h = h;
    sc->rb_bpp = bytes_per_pixel;
    sc->rb_padded_row = padded;

    // The renderer flushed (sg_commit) the producing passes; WebGPU has one
    // queue, so a copy submitted now is ordered after them.
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(sc->device, nullptr);
    WGPUTexelCopyTextureInfo src = {};
    src.texture = (WGPUTexture)info.tex;
    src.mipLevel = 0;
    src.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo dst = {};
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = padded;
    dst.layout.rowsPerImage = h;
    dst.buffer = sc->rb_buf;
    WGPUExtent3D extent = {w, h, 1};
    wgpuCommandEncoderCopyTextureToBuffer(enc, &src, &dst, &extent);
    WGPUCommandBuffer cb = wgpuCommandEncoderFinish(enc, nullptr);
    WGPUQueue queue = (WGPUQueue)sg_wgpu_queue();
    wgpuQueueSubmit(queue, 1, &cb);
    wgpuCommandBufferRelease(cb);
    wgpuCommandEncoderRelease(enc);

    // Spontaneous: delivered straight from the browser's promise resolution.
    // (AllowProcessEvents would need the INSTANCE that owns the device's event
    // queue — the host-imported device's, not the one this TU created for the
    // surface — and never fired here.)
    WGPUBufferMapCallbackInfo mi = {};
    mi.mode = WGPUCallbackMode_AllowSpontaneous;
    mi.callback = sc_map_cb;
    mi.userdata1 = sc;
    sc->rb_state = 1;
    wgpuBufferMapAsync(sc->rb_buf, WGPUMapMode_Read, 0, size, mi);
    return true;
}

int sumi_swapchain_readback_poll(sumi_swapchain_t* sc, uint8_t* dst, size_t dst_size) {
    if (!sc) return 0;
    if (sc->rb_state == 0) return 0;
    wgpuInstanceProcessEvents(sc->instance);   // deliver a completed map, if any
    if (sc->rb_state == 1) return 1;
    if (sc->rb_state == 3) { sc->rb_state = 0; return 0; }
    const size_t bytes = (size_t)sc->rb_w * sc->rb_h * sc->rb_bpp;
    const size_t mapped_size = (size_t)sc->rb_padded_row * sc->rb_h;
    if (dst && dst_size >= bytes) {
        const uint8_t* m = (const uint8_t*)wgpuBufferGetConstMappedRange(sc->rb_buf, 0, mapped_size);
        if (m) {
            const size_t row = (size_t)sc->rb_w * sc->rb_bpp;
            for (uint32_t y = 0; y < sc->rb_h; y++) {   // de-pad: row 0 = top (§4.6)
                memcpy(dst + y * row, m + (size_t)y * sc->rb_padded_row, row);
            }
        } else {
            sc_log(sc, SUMI_LOG_ERROR, "swapchain_webgpu: mapped range unavailable");
        }
    }
    wgpuBufferUnmap(sc->rb_buf);
    sc->rb_state = 0;
    return 2;
}

void sumi_swapchain_yield(sumi_swapchain_t* sc) {
    // A browser cannot block on the GPU: no sleep, no event pumping from here
    // (that would need ASYNCIFY). The web host uses the begin/poll field read.
    (void)sc;
}

} // extern "C"
