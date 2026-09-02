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
#define SOKOL_IMPL
#define SOKOL_GLCORE
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

// The autorelease-pool hooks are Metal-only plumbing (see swapchain.h);
// no-ops on GL.
void* sumi_swapchain_frame_pool_push(sumi_swapchain_t* sc) {
    (void)sc;
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

    const size_t bytes = (size_t)w * h * bytes_per_pixel;
    if (!sc->readback_pbo) glGenBuffers(1, &sc->readback_pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, sc->readback_pbo);
    if (sc->pbo_capacity < bytes) {
        glBufferData(GL_PIXEL_PACK_BUFFER, (GLsizeiptr)bytes, nullptr, GL_STREAM_READ);
        sc->pbo_capacity = bytes;
    }

    // Rows are tightly packed: RGBA8/RGBA16F rows are always 4-byte aligned,
    // so the default GL_PACK_ALIGNMENT of 4 never pads. In-context command
    // ordering places this after the flushed snapshot pass; with a bound
    // PIXEL_PACK_BUFFER glReadPixels returns immediately (§5.3: never blocks).
    const GLenum type = (bytes_per_pixel == 8) ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE;
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
        glBindBuffer(GL_PIXEL_PACK_BUFFER, sc->readback_pbo);
        const void* mapped = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, (GLsizeiptr)bytes,
                                              GL_MAP_READ_BIT);
        if (mapped) {
            // Straight copy: PBO row r = texture memory row r = §4.6 row r
            // (top-origin — the offscreen flip_vert_y already put it there).
            memcpy(dst, mapped, bytes);
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

} // extern "C"
