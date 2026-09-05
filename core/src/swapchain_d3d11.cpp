// swapchain_d3d11.cpp — D3D11 device/DXGI swapchain management (§5.1: the
// host passes an HWND, the core creates the device + swapchain and handles
// resize). Mirrors the DECISIONS.md #1 hosting pattern: this translation unit
// hosts the sokol_gfx implementation for the D3D11 build and is the only TU
// allowed to include d3d11/dxgi headers. renderer.cpp sees declarations only.
//
// Orientation (§4.6): D3D11 shares Metal's top-left row origin — no flip
// anywhere in this file, by design.

// sokol_gfx.h's implementation section has no re-inclusion guard, so this TU
// must include it exactly once — swapchain.h pulls it in below.
#define SOKOL_IMPL
#define SOKOL_D3D11
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "swapchain.h"
#include "log_levels.h"

#include <d3d11.h>
#include <dxgi.h>

#include <new>
#include <string.h>

struct sumi_swapchain_t {
    HWND                    hwnd;
    ID3D11Device*           device;
    ID3D11DeviceContext*    context;
    IDXGISwapChain*         swapchain;
    ID3D11RenderTargetView* rtv;        // recreated on every resize
    uint32_t                width, height;
    bool                    frame_acquired;   // present only frames we rendered
    sumi_log_fn             log_cb;
    void*                   log_user;

    // Paper-dip / field readback (§5.3): CopyResource into a staging texture,
    // then non-blocking Map polling. Single render thread on this backend —
    // no cross-thread completion handler, so plain state is sufficient.
    ID3D11Texture2D*        staging;
    uint32_t                staging_w, staging_h;
    DXGI_FORMAT             staging_fmt;
    int                     readback_state;   // 0 idle, 1 in flight
    uint32_t                readback_w, readback_h, readback_bpp;
};

static void sc_log(const sumi_swapchain_t* sc, int level, const char* msg) {
    if (sc && sc->log_cb) sc->log_cb(level, msg, sc->log_user);
}

template <typename T>
static void sc_release(T** obj) {
    if (obj && *obj) {
        (*obj)->Release();
        *obj = nullptr;
    }
}

// (Re)acquires the backbuffer RTV. With the flip model the runtime always
// exposes buffer 0 as the writable backbuffer on D3D11, so one RTV per
// (re)size is enough.
static bool sc_create_rtv(sumi_swapchain_t* sc) {
    sc_release(&sc->rtv);
    ID3D11Texture2D* backbuf = nullptr;
    HRESULT hr = sc->swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuf);
    if (FAILED(hr) || !backbuf) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_d3d11: GetBuffer(0) failed");
        return false;
    }
    hr = sc->device->CreateRenderTargetView(backbuf, nullptr, &sc->rtv);
    backbuf->Release();
    if (FAILED(hr) || !sc->rtv) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_d3d11: CreateRenderTargetView failed");
        return false;
    }
    return true;
}

extern "C" {

sumi_swapchain_t* sumi_swapchain_create(const sumi_config_t* config) {
    sumi_swapchain_t* sc = new (std::nothrow) sumi_swapchain_t();
    if (!sc) return nullptr;
    sc->log_cb = config->log_cb;
    sc->log_user = config->log_user;

    // Backend validation lives here, next to the backend it validates: this
    // build's swapchain is D3D11-only (engine.cpp stays backend-agnostic).
    if (config->backend != SUMI_BACKEND_D3D11) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_d3d11: this build supports SUMI_BACKEND_D3D11 only");
        delete sc;
        return nullptr;
    }

    sc->hwnd = (HWND)config->native_surface_handle;
    if (!sc->hwnd || !IsWindow(sc->hwnd)) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_d3d11: native_surface_handle is not a valid HWND");
        delete sc;
        return nullptr;
    }
    sc->width = config->width;
    sc->height = config->height;

    // BGRA8 non-sRGB matches the Metal swapchain (DECISIONS.md #5: the
    // composite applies the sRGB encode manually, spec §4.5).
    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferDesc.Width = config->width;
    desc.BufferDesc.Height = config->height;
    desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.BufferDesc.RefreshRate.Numerator = 0;    // let DXGI pick
    desc.BufferDesc.RefreshRate.Denominator = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.OutputWindow = sc->hwnd;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(SUMI_D3D11_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    // Prefer 11.1 (full UAV slot count, silences sokol's feature-level warn),
    // fall back to 11.0. Old runtimes reject an array containing 11_1 with
    // E_INVALIDARG, hence the two-step retry rather than one two-entry array.
    const D3D_FEATURE_LEVEL want_11_1 = D3D_FEATURE_LEVEL_11_1;
    const D3D_FEATURE_LEVEL want_11_0 = D3D_FEATURE_LEVEL_11_0;
    D3D_FEATURE_LEVEL got = (D3D_FEATURE_LEVEL)0;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        &want_11_1, 1, D3D11_SDK_VERSION,
        &desc, &sc->swapchain, &sc->device, &got, &sc->context);
    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            &want_11_0, 1, D3D11_SDK_VERSION,
            &desc, &sc->swapchain, &sc->device, &got, &sc->context);
    }
    if (FAILED(hr)) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_d3d11: D3D11CreateDeviceAndSwapChain failed");
        delete sc;
        return nullptr;
    }
    if (!sc_create_rtv(sc)) {
        sumi_swapchain_destroy(sc);
        return nullptr;
    }
    return sc;
}

void sumi_swapchain_destroy(sumi_swapchain_t* sc) {
    if (!sc) return;
    // An issued CopyResource needs no wait: releasing the staging texture is
    // safe, the runtime keeps it alive until the GPU is done with it.
    if (sc->context) sc->context->ClearState();
    sc_release(&sc->staging);
    sc_release(&sc->rtv);
    sc_release(&sc->swapchain);
    sc_release(&sc->context);
    sc_release(&sc->device);
    delete sc;
}

sg_environment sumi_swapchain_environment(const sumi_swapchain_t* sc) {
    sg_environment env = {};
    env.defaults.color_format = SG_PIXELFORMAT_BGRA8;
    env.defaults.depth_format = SG_PIXELFORMAT_NONE;
    env.defaults.sample_count = 1;
    env.d3d11.device = (const void*)sc->device;
    env.d3d11.device_context = (const void*)sc->context;
    return env;
}

sg_swapchain sumi_swapchain_acquire(sumi_swapchain_t* sc) {
    sg_swapchain swapchain = {};
    if (!sc || !sc->rtv || sc->width == 0 || sc->height == 0) {
        return swapchain;   // zero width signals "skip this frame" upstream
    }
    swapchain.width = (int)sc->width;
    swapchain.height = (int)sc->height;
    swapchain.sample_count = 1;
    swapchain.color_format = SG_PIXELFORMAT_BGRA8;
    swapchain.depth_format = SG_PIXELFORMAT_NONE;
    swapchain.d3d11.render_view = (const void*)sc->rtv;
    sc->frame_acquired = true;
    return swapchain;
}

void sumi_swapchain_frame_done(sumi_swapchain_t* sc) {
    if (!sc || !sc->swapchain || !sc->frame_acquired) return;
    sc->frame_acquired = false;
    // Vsync interval 1: matches the CAMetalLayer's default display-linked
    // presentation on the Metal backend.
    const HRESULT hr = sc->swapchain->Present(1, 0);
    if (FAILED(hr)) {
        sc_log(sc, SUMI_LOG_WARN, "swapchain_d3d11: Present failed");
    }
}

// The autorelease-pool hooks are Metal-only plumbing (see swapchain.h);
// no-ops on D3D11.
void* sumi_swapchain_frame_pool_push(sumi_swapchain_t* sc) {
    (void)sc;
    return nullptr;
}

void sumi_swapchain_frame_pool_pop(sumi_swapchain_t* sc, void* pool) {
    (void)sc;
    (void)pool;
}

void sumi_swapchain_resize(sumi_swapchain_t* sc, uint32_t w, uint32_t h, float pixel_ratio) {
    (void)pixel_ratio;   // GLFW hands physical pixels; no scale plumbing on Win32
    if (!sc || !sc->swapchain || w == 0 || h == 0) return;
    if (w == sc->width && h == sc->height) return;
    // Every swapchain buffer reference must be released before ResizeBuffers.
    sc_release(&sc->rtv);
    sc->context->ClearState();
    const HRESULT hr = sc->swapchain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_d3d11: ResizeBuffers failed");
        sc->width = sc->height = 0;
        return;
    }
    sc->width = w;
    sc->height = h;
    if (!sc_create_rtv(sc)) {
        sc->width = sc->height = 0;
    }
}

bool sumi_swapchain_readback_begin(sumi_swapchain_t* sc, sg_image img,
                                   uint32_t w, uint32_t h, uint32_t bytes_per_pixel) {
    if (!sc || img.id == 0 || w == 0 || h == 0 ||
        (bytes_per_pixel != 4 && bytes_per_pixel != 8)) return false;
    if (sc->readback_state != 0) return false;   // one readback in flight at a time

    const sg_d3d11_image_info info = sg_d3d11_query_image_info(img);
    ID3D11Texture2D* src = (ID3D11Texture2D*)info.tex2d;
    if (!src) return false;

    const DXGI_FORMAT fmt = (bytes_per_pixel == 8) ? DXGI_FORMAT_R16G16B16A16_FLOAT
                                                   : DXGI_FORMAT_R8G8B8A8_UNORM;
    if (!sc->staging || sc->staging_w != w || sc->staging_h != h || sc->staging_fmt != fmt) {
        sc_release(&sc->staging);
        D3D11_TEXTURE2D_DESC sd = {};
        sd.Width = w;
        sd.Height = h;
        sd.MipLevels = 1;
        sd.ArraySize = 1;
        sd.Format = fmt;
        sd.SampleDesc.Count = 1;
        sd.Usage = D3D11_USAGE_STAGING;
        sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        const HRESULT hr = sc->device->CreateTexture2D(&sd, nullptr, &sc->staging);
        if (FAILED(hr) || !sc->staging) {
            sc_log(sc, SUMI_LOG_ERROR, "swapchain_d3d11: staging texture allocation failed");
            sc->staging = nullptr;
            return false;
        }
        sc->staging_w = w;
        sc->staging_h = h;
        sc->staging_fmt = fmt;
    }

    // Ordered after the already-submitted producing passes on the immediate
    // context; the copy itself returns without waiting for the GPU.
    sc->context->CopyResource(sc->staging, src);
    sc->readback_w = w;
    sc->readback_h = h;
    sc->readback_bpp = bytes_per_pixel;
    sc->readback_state = 1;
    return true;
}

int sumi_swapchain_readback_poll(sumi_swapchain_t* sc, uint8_t* dst, size_t dst_size) {
    if (!sc) return 0;
    if (sc->readback_state != 1) return sc->readback_state;

    // DO_NOT_WAIT keeps the §5.3 contract: while the GPU is still copying,
    // Map returns DXGI_ERROR_WAS_STILL_DRAWING and the render loop goes on.
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const HRESULT hr = sc->context->Map(sc->staging, 0, D3D11_MAP_READ,
                                        D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
    if (hr == DXGI_ERROR_WAS_STILL_DRAWING) return 1;
    if (FAILED(hr)) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_d3d11: staging Map failed");
        sc->readback_state = 0;
        return 0;
    }
    const size_t row_bytes = (size_t)sc->readback_w * sc->readback_bpp;
    const size_t bytes = row_bytes * sc->readback_h;
    if (dst && dst_size >= bytes) {
        // Tightly repack: staging RowPitch is driver-aligned. Row 0 = top —
        // D3D11 and Metal share the top-left origin (§4.6), no flip.
        const uint8_t* srcp = (const uint8_t*)mapped.pData;
        for (uint32_t y = 0; y < sc->readback_h; y++) {
            memcpy(dst + (size_t)y * row_bytes, srcp + (size_t)y * mapped.RowPitch, row_bytes);
        }
    }
    sc->context->Unmap(sc->staging, 0);
    sc->readback_state = 0;
    return 2;
}

void sumi_swapchain_yield(sumi_swapchain_t* sc) {
    (void)sc;
    Sleep(1);
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
