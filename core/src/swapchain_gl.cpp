// swapchain_gl.cpp — OpenGL 4.1 core "swapchain" (§5.1 GL exception: the HOST
// owns the context — it creates it (GLFW/EGL), makes it current on the render
// thread BEFORE sumi_create, and presents (swap buffers) after sumi_render;
// native_surface_handle must be NULL). There is no device or swapchain to
// create here: this TU stays thin — it validates the contract, tracks the
// drawable size, exposes the default framebuffer to sokol, and hosts the
// async PBO print/field readback.
//
// Mirrors the DECISIONS.md #1 hosting pattern: this translation unit hosts
// the sokol_gfx implementation for the GL build and is the only TU touching
// GL entry points. renderer.cpp sees declarations only. sokol_gfx needs no
// separate GL loader on desktop Linux: it includes <GL/gl.h> with
// GL_GLEXT_PROTOTYPES and links libGL, and this TU uses the same prototypes.
//
// Orientation (§4.6): NO flip code in this file, by design. The offscreen
// vertex shaders carry the GLSL-only flip_vert_y option, so every offscreen
// target (field ping-pong, print) is top-left-row-origin in memory exactly
// like Metal/D3D11 — glReadPixels row 0 is texture memory row 0, i.e. the §4.6
// top row, and the PBO copy below is a straight memcpy. The one place GL
// diverges is the final on-screen composite, whose UNFLIPPED vertex shader
// lets the bottom-up default-framebuffer scanout provide the §4.6 flip.

// sokol_gfx.h's implementation section has no re-inclusion guard, so this TU
// must include it exactly once — swapchain.h pulls it in below.
//
// Backend selection lives in the swapchain TUs (DECISIONS_2 #16): this same
// file hosts desktop Linux (GL 4.1 core) and Android (GLES3). The §5.1
// host-owned-context contract, the §4.6 orientation story (flip_vert_y in
// the GLSL dialects, unflipped screen composite) and the PBO readback design
// are identical on both — GLES3 differences are confined to the
// SOKOL_GLES3 blocks below (RGBA16F renderability is an extension there, and
// half-float glReadPixels may need a float fallback).
#define SOKOL_IMPL
#if defined(__ANDROID__)
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif
#include "swapchain.h"
#include "log_levels.h"

#include <new>
#include <string.h>
#include <stdio.h>
#include <time.h>

struct sumi_swapchain_t {
    uint32_t    width, height;   // drawable size (host reports physical px)
    sumi_log_fn log_cb;
    void*       log_user;

    // Paper-dip / field readback (§5.3): glReadPixels into a PBO through a
    // private read-FBO, then a fence polled with glClientWaitSync(…, 0).
    // Single render thread on this backend — plain state is sufficient.
    GLuint      readback_fbo;
    GLuint      readback_pbo;
    size_t      pbo_capacity;
    GLsync      readback_fence;      // non-NULL while a readback is in flight
    uint32_t    readback_w, readback_h, readback_bpp;
    GLenum      readback_gl_type;    // HALF_FLOAT, or FLOAT on the ES3 fallback
};

// float32 -> float16 for the GLES3 RGBA/FLOAT readback fallback. The source
// texture IS half float, so every value glReadPixels widened to float32 is an
// exactly representable half — truncation here is lossless for that input
// (inf/NaN/overflow handled only for completeness).
static uint16_t f32_to_f16(float f) {
    uint32_t x;
    memcpy(&x, &f, sizeof(x));
    const uint32_t sign = (x >> 16) & 0x8000u;
    const int32_t  exp = (int32_t)((x >> 23) & 0xFFu) - 127 + 15;
    uint32_t man = x & 0x7FFFFFu;
    if (exp >= 31) return (uint16_t)(sign | 0x7C00u | ((x & 0x7F800000u) == 0x7F800000u && man ? 0x200u : 0));
    if (exp <= 0) {                       // subnormal half (or zero)
        if (exp < -10) return (uint16_t)sign;
        man |= 0x800000u;
        return (uint16_t)(sign | (man >> (uint32_t)(14 - exp)));
    }
    return (uint16_t)(sign | ((uint32_t)exp << 10) | (man >> 13));
}

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
    // build's swapchain is GL-only (engine.cpp stays backend-agnostic).
    if (config->backend != SUMI_BACKEND_GL) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_gl: this build supports SUMI_BACKEND_GL only");
        delete sc;
        return nullptr;
    }
    // §5.1: the host owns the context; a non-NULL handle means the host
    // expected device-creating ownership (Metal/D3D11 semantics) — refuse
    // loudly rather than silently ignoring it.
    if (config->native_surface_handle != nullptr) {
        sc_log(sc, SUMI_LOG_ERROR,
               "swapchain_gl: native_surface_handle must be NULL (the host owns the GL context, spec 5.1)");
        delete sc;
        return nullptr;
    }
    // The context the host made current is the closest thing to a device
    // handle we can validate; glGetString returns NULL without one.
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    if (!version) {
        sc_log(sc, SUMI_LOG_ERROR,
               "swapchain_gl: no current GL context (host must glfwMakeContextCurrent/eglMakeCurrent before sumi_create)");
        delete sc;
        return nullptr;
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "swapchain_gl: context GL %s on %s",
             (const char*)version, renderer ? (const char*)renderer : "?");
    sc_log(sc, SUMI_LOG_INFO, buf);

#if defined(SOKOL_GLES3)
    // RGBA16F color attachments are NOT core in any GLES3 version — the field
    // targets need an extension containing "_color_buffer_half_float", which
    // is exactly the gate sokol_gfx uses to mark RGBA16F renderable on GLES
    // (its "_color_buffer_float" promotion is WebGL2-only). Fail loudly here:
    // the alternative is a silent incomplete FBO deep inside sokol.
    bool has_half_rt = false;
    GLint ext_count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &ext_count);
    for (GLint i = 0; i < ext_count; i++) {
        const char* e = (const char*)glGetStringi(GL_EXTENSIONS, (GLuint)i);
        if (e && strstr(e, "_color_buffer_half_float")) {
            has_half_rt = true;
            break;
        }
    }
    if (!has_half_rt) {
        sc_log(sc, SUMI_LOG_ERROR,
               "swapchain_gl: this GLES3 driver lacks EXT_color_buffer_half_float — "
               "the RGBA16F field targets cannot be rendered on this device");
        delete sc;
        return nullptr;
    }
    sc_log(sc, SUMI_LOG_INFO, "swapchain_gl: EXT_color_buffer_half_float present");
#endif

    sc->width = config->width;
    sc->height = config->height;
    return sc;
}

void sumi_swapchain_destroy(sumi_swapchain_t* sc) {
    if (!sc) return;
    // GL defers actual deletion of in-use objects; no need to wait for an
    // in-flight readback. The host's context is still current here (it must
    // not destroy it before sumi_destroy returns).
    if (sc->readback_fence) glDeleteSync(sc->readback_fence);
    if (sc->readback_pbo) glDeleteBuffers(1, &sc->readback_pbo);
    if (sc->readback_fbo) glDeleteFramebuffers(1, &sc->readback_fbo);
    delete sc;
}

sg_environment sumi_swapchain_environment(const sumi_swapchain_t* sc) {
    (void)sc;
    sg_environment env = {};
    // RGBA8, not BGRA8: GL's default framebuffer has no client-visible
    // channel order — RGBA8 is the native tag for it (the composite pipeline
    // inherits this default, keeping renderer.cpp format-agnostic).
    env.defaults.color_format = SG_PIXELFORMAT_RGBA8;
    env.defaults.depth_format = SG_PIXELFORMAT_NONE;
    env.defaults.sample_count = 1;
    return env;
}

sg_swapchain sumi_swapchain_acquire(sumi_swapchain_t* sc) {
    sg_swapchain swapchain = {};
    if (!sc || sc->width == 0 || sc->height == 0) {
        return swapchain;   // zero width signals "skip this frame" upstream
    }
    swapchain.width = (int)sc->width;
    swapchain.height = (int)sc->height;
    swapchain.sample_count = 1;
    swapchain.color_format = SG_PIXELFORMAT_RGBA8;
    swapchain.depth_format = SG_PIXELFORMAT_NONE;
    swapchain.gl.framebuffer = 0;   // the host-owned default framebuffer
    return swapchain;
}

void sumi_swapchain_frame_done(sumi_swapchain_t* sc) {
    // §5.1: the HOST presents (glfwSwapBuffers / eglSwapBuffers) after
    // sumi_render returns; nothing to do here.
    (void)sc;
}

// The autorelease-pool hooks are Metal-only plumbing (see swapchain.h). On
// GL they double as the per-frame context-state re-assert: the renderer
// wraps every frame AND every target (re)creation — identity init included —
// in push/pop, so this runs before any pass touches the field.
//
// GL_DITHER: sokol's state reset calls glEnable(GL_DITHER) at sg_setup (the
// GL default), and never touches it again. Desktop drivers no-op dithering
// on fp16 targets, but mobile GPUs may actually apply it — per-texel ±ULP
// noise injected into every ping-pong pass, which breaks the §4.6
// cross-backend field regression's determinism budget. The chain must be
// bit-deterministic: dithering has no place on any target this engine
// renders (the composite does its own sRGB encode).
void* sumi_swapchain_frame_pool_push(sumi_swapchain_t* sc) {
    (void)sc;
    glDisable(GL_DITHER);
    return nullptr;
}

void sumi_swapchain_frame_pool_pop(sumi_swapchain_t* sc, void* pool) {
    (void)sc;
    (void)pool;
}

void sumi_swapchain_resize(sumi_swapchain_t* sc, uint32_t w, uint32_t h, float pixel_ratio) {
    (void)pixel_ratio;   // GLFW hands physical pixels; the window system owns scale
    if (!sc || w == 0 || h == 0) return;
    // The window system resizes the default framebuffer itself; just track
    // the size for acquire().
    sc->width = w;
    sc->height = h;
}

bool sumi_swapchain_readback_begin(sumi_swapchain_t* sc, sg_image img,
                                   uint32_t w, uint32_t h, uint32_t bytes_per_pixel) {
    if (!sc || img.id == 0 || w == 0 || h == 0 ||
        (bytes_per_pixel != 4 && bytes_per_pixel != 8)) return false;
    if (sc->readback_fence) return false;   // one readback in flight at a time

    const sg_gl_image_info info = sg_gl_query_image_info(img);
    const GLuint tex = info.tex[info.active_slot >= 0 ? info.active_slot : 0];
    if (!tex || info.tex_target != GL_TEXTURE_2D) return false;

    // Read through a private FBO instead of glGetTexImage: binding the source
    // texture here would silently desync sokol's texture-binding cache. The
    // READ_FRAMEBUFFER and PIXEL_PACK_BUFFER points are never touched by
    // sokol, and both are restored to 0 below.
    if (!sc->readback_fbo) glGenFramebuffers(1, &sc->readback_fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sc->readback_fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_gl: readback framebuffer incomplete");
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        return false;
    }

    // Desktop GL always accepts RGBA/HALF_FLOAT reads from a half-float
    // attachment. ES 3.0 only guarantees RGBA/FLOAT for float-type buffers —
    // RGBA/HALF_FLOAT works iff the driver reports it as the implementation
    // color-read pair, so query and fall back to FLOAT + CPU narrowing.
    GLenum type = (bytes_per_pixel == 8) ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE;
#if defined(SOKOL_GLES3)
    if (bytes_per_pixel == 8) {
        GLint impl_fmt = 0, impl_type = 0;
        glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &impl_fmt);
        glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &impl_type);
        if (!(impl_fmt == GL_RGBA && impl_type == GL_HALF_FLOAT)) {
            type = GL_FLOAT;   // the guaranteed pair; narrowed in poll()
        }
    }
#endif
    sc->readback_gl_type = type;
    const size_t pbo_bytes = (size_t)w * h * ((type == GL_FLOAT) ? 16 : bytes_per_pixel);
    if (!sc->readback_pbo) glGenBuffers(1, &sc->readback_pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, sc->readback_pbo);
    if (sc->pbo_capacity < pbo_bytes) {
        glBufferData(GL_PIXEL_PACK_BUFFER, (GLsizeiptr)pbo_bytes, nullptr, GL_STREAM_READ);
        sc->pbo_capacity = pbo_bytes;
    }

    // Rows are tightly packed: RGBA8/RGBA16F/RGBA32F rows are always 4-byte
    // aligned, so the default GL_PACK_ALIGNMENT of 4 never pads. In-context
    // command ordering places this after the flushed snapshot pass; with a
    // bound PIXEL_PACK_BUFFER glReadPixels returns immediately (§5.3: never
    // blocks).
    glReadPixels(0, 0, (GLsizei)w, (GLsizei)h, GL_RGBA, type, nullptr);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    sc->readback_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    if (!sc->readback_fence) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_gl: glFenceSync failed");
        return false;
    }
    // Make sure the fence (and the copy) actually reach the GPU: without a
    // flush, glClientWaitSync(…, 0) could report "in flight" forever.
    glFlush();

    sc->readback_w = w;
    sc->readback_h = h;
    sc->readback_bpp = bytes_per_pixel;
    return true;
}

int sumi_swapchain_readback_poll(sumi_swapchain_t* sc, uint8_t* dst, size_t dst_size) {
    if (!sc) return 0;
    if (!sc->readback_fence) return 0;

    // Timeout 0 keeps the §5.3 contract: while the GPU is still copying this
    // returns GL_TIMEOUT_EXPIRED and the render loop goes on.
    const GLenum wait = glClientWaitSync(sc->readback_fence, 0, 0);
    if (wait == GL_TIMEOUT_EXPIRED) return 1;
    glDeleteSync(sc->readback_fence);
    sc->readback_fence = nullptr;
    if (wait != GL_ALREADY_SIGNALED && wait != GL_CONDITION_SATISFIED) {
        sc_log(sc, SUMI_LOG_ERROR, "swapchain_gl: glClientWaitSync failed");
        return 0;
    }

    const size_t bytes = (size_t)sc->readback_w * sc->readback_h * sc->readback_bpp;
    if (dst && dst_size >= bytes) {
        const bool widen = (sc->readback_gl_type == GL_FLOAT);
        const size_t pbo_bytes = widen ? bytes * 2 : bytes;
        glBindBuffer(GL_PIXEL_PACK_BUFFER, sc->readback_pbo);
        const void* mapped = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, (GLsizeiptr)pbo_bytes,
                                              GL_MAP_READ_BIT);
        if (mapped) {
            if (widen) {
                // ES3 FLOAT fallback: narrow to the half floats the §5.3/§4.6
                // contract hands out. Lossless — the source texture is half,
                // so every widened value is an exactly representable half.
                const float* src = (const float*)mapped;
                uint16_t* out = (uint16_t*)dst;
                const size_t n = bytes / 2;   // number of half values
                for (size_t i = 0; i < n; i++) out[i] = f32_to_f16(src[i]);
            } else {
                // Straight copy: PBO row r = texture memory row r = §4.6 row r
                // (top-origin — the offscreen flip_vert_y already put it there).
                memcpy(dst, mapped, bytes);
            }
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        } else {
            sc_log(sc, SUMI_LOG_ERROR, "swapchain_gl: PBO map failed");
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }
    return 2;
}

void sumi_swapchain_yield(sumi_swapchain_t* sc) {
    (void)sc;
    struct timespec ts = {0, 1000000};   // 1 ms
    nanosleep(&ts, nullptr);
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
