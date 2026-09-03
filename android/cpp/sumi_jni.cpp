// sumi_jni.cpp — the Android host shell (§5.4): SurfaceView lifecycle → EGL
// host-owned context (§5.1) on a dedicated render thread, AMidi →
// sumi_push_midi, touch/param marshaling, and the step-14 evidence hooks
// (field dump, stress feeder, per-second CSV).
//
// Threading (§5.2, the step's real difficulty): every sumi_* call except
// sumi_push_midi happens on the ONE render thread that owns the EGL context.
// Everything from Kotlin (touches, params, dip, surface sizes) is marshaled
// through a small command queue drained at the top of each render-thread
// frame. sumi_push_midi is called from exactly one producer at a time — the
// AMidi poller thread or the stress feeder — serialized by a producer mutex
// (DECISIONS #24 ported to the JNI layer).
//
// Teardown contract (§5.4, hard requirement): nativeSurfaceDestroyed BLOCKS
// the UI thread until the render thread has finished its in-flight frame,
// unbound the surface (eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
// ctx) — surfaceless-context is mandatory on Android's EGL since 7.0), and
// destroyed the EGL surface; only then is the ANativeWindow released and the
// call allowed to return. The EGL CONTEXT survives surface cycles — the
// field textures live in it, so DECISIONS_2 #28's resize preservation
// carries the drawing across rotations.
#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <amidi/AMidi.h>
#include <EGL/egl.h>

#include "sumi_core.h"
#include "sumi_debug.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define LOG_TAG "sumi-shell"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Every EGL call goes through here: the ×10 teardown-race evidence sweeps
// logcat for "EGLERR" — a clean run logs none.
static bool egl_check(const char* what, EGLBoolean ok) {
    if (ok == EGL_TRUE) return true;
    LOGE("EGLERR: %s failed, eglGetError=0x%04x", what, eglGetError());
    return false;
}

static void sumi_log_bridge(int level, const char* msg, void*) {
    const int prio = level <= 1 ? ANDROID_LOG_ERROR
                   : level == 2 ? ANDROID_LOG_WARN : ANDROID_LOG_INFO;
    __android_log_print(prio, "sumi", "%s", msg);
}

// IEEE half -> float for the field dump file (backend-independent float32
// rows, step-11 handoff format; same logic as the desktop harness).
static float half_to_float(uint16_t h) {
    const uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
    uint32_t exp = (h >> 10) & 0x1Fu;
    uint32_t man = h & 0x3FFu;
    uint32_t f;
    if (exp == 0) {
        if (man == 0) {
            f = sign;
        } else {
            exp = 127 - 15 + 1;
            while (!(man & 0x400u)) { man <<= 1; exp--; }
            man &= 0x3FFu;
            f = sign | (exp << 23) | (man << 13);
        }
    } else if (exp == 31) {
        f = sign | 0x7F800000u | (man << 13);
    } else {
        f = sign | ((exp - 15 + 127) << 23) | (man << 13);
    }
    float out;
    memcpy(&out, &f, sizeof(out));
    return out;
}

namespace {

using Clock = std::chrono::steady_clock;

struct Shell {
    // -- render thread & lifecycle -------------------------------------------
    std::thread render_thread;
    std::atomic<bool> running{false};

    std::mutex state_mu;
    std::condition_variable state_cv;
    ANativeWindow* pending_window = nullptr;   // handed off by surfaceCreated
    bool release_requested = false;            // surfaceDestroyed in progress
    bool surface_released = false;             // render thread ack

    // Render-thread-only state.
    EGLDisplay dpy = EGL_NO_DISPLAY;
    EGLContext ctx = EGL_NO_CONTEXT;
    EGLConfig  cfg = nullptr;
    EGLSurface surf = EGL_NO_SURFACE;
    ANativeWindow* window = nullptr;
    sumi_instance_t* inst = nullptr;
    double last_frame_t = 0.0;

    // -- command queue (anything that must run on the render thread) ---------
    std::mutex q_mu;
    std::deque<std::function<void()>> commands;

    // -- resize (latest wins; applied at frame top) ---------------------------
    std::atomic<uint32_t> want_w{0}, want_h{0};
    uint32_t applied_w = 0, applied_h = 0;   // render thread only
    std::atomic<uint32_t> density_x100{100};

    // -- MIDI producers (§5.2: exactly one at a time; DECISIONS #24 mutex) ----
    std::mutex push_mu;
    std::atomic<bool> device_midi_enabled{true};
    std::thread midi_thread;
    std::atomic<bool> midi_running{false};
    std::mutex ports_mu;
    struct OpenPort {
        AMidiDevice* dev;
        AMidiOutputPort* port;   // "output port" = data flowing OUT of the device
        uint8_t status = 0, d1 = 0;
        int have = 0;
        bool in_sysex = false;
    };
    std::vector<OpenPort> ports;

    std::thread stress_thread;
    std::atomic<bool> stress_running{false};

    // -- evidence CSV (t,fps,worst_frame_ms,thermal — iOS logger port) -------
    std::string files_dir;
    FILE* csv = nullptr;
    Clock::time_point session_start, second_start;
    int frames_this_second = 0;
    double worst_frame_ms = 0.0;
    std::atomic<int> thermal{0};
    std::atomic<long> egl_error_count{0};
};

Shell g;

double now_s() {
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

void post(std::function<void()> fn) {
    {
        std::lock_guard<std::mutex> lk(g.q_mu);
        g.commands.push_back(std::move(fn));
    }
    g.state_cv.notify_all();
}

// Post AND wait for completion (field dump, scripted evidence). Never call
// from the render thread.
void post_sync(const std::function<void()>& fn) {
    std::mutex done_mu;
    std::condition_variable done_cv;
    bool done = false;
    post([&] {
        fn();
        std::lock_guard<std::mutex> lk(done_mu);
        done = true;
        done_cv.notify_all();
    });
    std::unique_lock<std::mutex> lk(done_mu);
    done_cv.wait(lk, [&] { return done; });
}

void drain_commands() {
    for (;;) {
        std::function<void()> fn;
        {
            std::lock_guard<std::mutex> lk(g.q_mu);
            if (g.commands.empty()) return;
            fn = std::move(g.commands.front());
            g.commands.pop_front();
        }
        fn();
    }
}

const char* thermal_name(int t) {
    switch (t) {
        case 0: return "none";
        case 1: return "light";
        case 2: return "moderate";
        case 3: return "severe";
        case 4: return "critical";
        case 5: return "emergency";
        case 6: return "shutdown";
        default: return "unknown";
    }
}

void csv_line(const char* line) {   // render thread (or via post)
    if (!g.csv && !g.files_dir.empty()) {
        const std::string path = g.files_dir + "/session_log.csv";
        g.csv = fopen(path.c_str(), "w");
        if (g.csv) fputs("t_s,fps,worst_frame_ms,thermal\n", g.csv);
    }
    if (g.csv) {
        fputs(line, g.csv);
        fputc('\n', g.csv);
        fflush(g.csv);
    }
}

void csv_event(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    LOGI("%s", buf);
    csv_line(buf);
}

// -- EGL / surface handling (render thread only) -----------------------------

void detach_surface();

bool egl_init_once() {
    if (g.dpy != EGL_NO_DISPLAY) return true;
    g.dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g.dpy == EGL_NO_DISPLAY) {
        LOGE("EGLERR: eglGetDisplay returned EGL_NO_DISPLAY");
        return false;
    }
    if (!egl_check("eglInitialize", eglInitialize(g.dpy, nullptr, nullptr))) return false;
    const EGLint attrs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };
    EGLint n = 0;
    if (!egl_check("eglChooseConfig", eglChooseConfig(g.dpy, attrs, &g.cfg, 1, &n)) || n < 1) {
        LOGE("EGLERR: no ES3 RGBA8 window config");
        return false;
    }
    const EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    g.ctx = eglCreateContext(g.dpy, g.cfg, EGL_NO_CONTEXT, ctx_attrs);
    if (g.ctx == EGL_NO_CONTEXT) {
        LOGE("EGLERR: eglCreateContext failed, 0x%04x", eglGetError());
        return false;
    }
    LOGI("EGL context created (ES3)");
    return true;
}

void attach_surface(ANativeWindow* win) {
    if (!egl_init_once()) { ANativeWindow_release(win); return; }
    g.window = win;
    g.surf = eglCreateWindowSurface(g.dpy, g.cfg, win, nullptr);
    if (g.surf == EGL_NO_SURFACE) {
        LOGE("EGLERR: eglCreateWindowSurface failed, 0x%04x", eglGetError());
        ANativeWindow_release(win);
        g.window = nullptr;
        return;
    }
    if (!egl_check("eglMakeCurrent(window)", eglMakeCurrent(g.dpy, g.surf, g.surf, g.ctx))) return;
    egl_check("eglSwapInterval", eglSwapInterval(g.dpy, 1));   // vsync parity (DECISIONS_2 #21)

    EGLint w = 0, h = 0;
    eglQuerySurface(g.dpy, g.surf, EGL_WIDTH, &w);
    eglQuerySurface(g.dpy, g.surf, EGL_HEIGHT, &h);
    const float density = (float)g.density_x100.load() / 100.0f;

    if (!g.inst) {
        sumi_config_t config = {};
        config.native_surface_handle = nullptr;        // §5.1: host owns the context
        config.backend = SUMI_BACKEND_GL;
        config.width = (uint32_t)w;
        config.height = (uint32_t)h;
        config.pixel_ratio = density;
        config.log_cb = sumi_log_bridge;
        g.inst = sumi_create(&config);
        if (!g.inst) {
            LOGE("sumi_create failed — releasing the surface (no render loop)");
            detach_surface();
            return;
        }
        sumi_params_t p;
        sumi_get_params(g.inst, &p);
        p.sim_scale = 0.75f;   // host default for phone/tablet GPUs (§ params comment)
        sumi_set_params(g.inst, &p);
        g.session_start = g.second_start = Clock::now();
        LOGI("sumi %u.%u.%u ready, %dx%d @%.2fx",
             sumi_version() >> 16, (sumi_version() >> 8) & 0xFF, sumi_version() & 0xFF,
             w, h, (double)density);
    } else {
        sumi_resize(g.inst, (uint32_t)w, (uint32_t)h, density);
    }
    g.want_w = (uint32_t)w;
    g.want_h = (uint32_t)h;
    g.last_frame_t = 0.0;
    csv_event("# t=%.1f surface attached %dx%d",
              std::chrono::duration<double>(Clock::now() - g.session_start).count(), w, h);
}

// The §5.4 blocking-teardown back half. Runs on the render thread; the UI
// thread is parked in nativeSurfaceDestroyed until this signals.
void detach_surface() {
    if (g.surf != EGL_NO_SURFACE) {
        // The in-flight frame is already done (frame() returned). Unbind the
        // surface but KEEP the context current (surfaceless) so the field
        // textures survive the cycle.
        egl_check("eglMakeCurrent(surfaceless)",
                  eglMakeCurrent(g.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, g.ctx));
        egl_check("eglDestroySurface", eglDestroySurface(g.dpy, g.surf));
        g.surf = EGL_NO_SURFACE;
    }
    if (g.window) {
        ANativeWindow_release(g.window);
        g.window = nullptr;
    }
    csv_event("# t=%.1f surface released",
              std::chrono::duration<double>(Clock::now() - g.session_start).count());
}

void frame() {
    if (!g.inst || g.surf == EGL_NO_SURFACE) return;
    // Latest-wins resize (surfaceChanged is marshaled through atomics).
    const uint32_t ww = g.want_w.load(), wh = g.want_h.load();
    if (ww && wh && (ww != g.applied_w || wh != g.applied_h)) {
        sumi_resize(g.inst, ww, wh, (float)g.density_x100.load() / 100.0f);
        g.applied_w = ww;
        g.applied_h = wh;
    }
    const double t = now_s();
    double dt = (g.last_frame_t > 0.0) ? t - g.last_frame_t : 1.0 / 60.0;
    if (dt > 0.1) dt = 0.1;
    g.last_frame_t = t;

    const auto f0 = Clock::now();
    sumi_update(g.inst, dt);
    sumi_render(g.inst);
    if (!eglSwapBuffers(g.dpy, g.surf)) {
        LOGE("EGLERR: eglSwapBuffers failed, 0x%04x", eglGetError());
        g.egl_error_count++;
    }
    const double frame_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - f0).count();

    g.frames_this_second++;
    if (frame_ms > g.worst_frame_ms) g.worst_frame_ms = frame_ms;
    const double since = std::chrono::duration<double>(Clock::now() - g.second_start).count();
    if (since >= 1.0) {
        char line[128];
        snprintf(line, sizeof(line), "%.0f,%.1f,%.2f,%s",
                 std::chrono::duration<double>(Clock::now() - g.session_start).count(),
                 (double)g.frames_this_second / since, g.worst_frame_ms,
                 thermal_name(g.thermal.load()));
        csv_line(line);
        g.frames_this_second = 0;
        g.worst_frame_ms = 0.0;
        g.second_start = Clock::now();
    }
}

void render_loop() {
    LOGI("render thread up");
    for (;;) {
        // Wait for a surface, a command, or shutdown.
        {
            std::unique_lock<std::mutex> lk(g.state_mu);
            g.state_cv.wait(lk, [] {
                if (!g.running.load()) return true;
                if (g.pending_window) return true;
                std::lock_guard<std::mutex> qlk(g.q_mu);
                return !g.commands.empty();
            });
        }
        if (!g.running.load()) break;

        // Surfaceless commands (shutdown-time destroys, etc.).
        if (g.surf == EGL_NO_SURFACE) drain_commands();

        ANativeWindow* win = nullptr;
        {
            std::lock_guard<std::mutex> lk(g.state_mu);
            win = g.pending_window;
            g.pending_window = nullptr;
        }
        if (win) attach_surface(win);

        // Frame loop while the surface is attached.
        while (g.running.load() && g.surf != EGL_NO_SURFACE) {
            bool release;
            {
                std::lock_guard<std::mutex> lk(g.state_mu);
                release = g.release_requested;
            }
            if (release) break;
            drain_commands();
            if (g.surf == EGL_NO_SURFACE) break;   // a command may have torn down
            frame();
        }

        // Blocking teardown handshake (§5.4).
        {
            std::unique_lock<std::mutex> lk(g.state_mu);
            if (g.release_requested) {
                lk.unlock();
                drain_commands();   // finish anything queued before the destroy
                detach_surface();
                lk.lock();
                g.release_requested = false;
                g.surface_released = true;
                g.state_cv.notify_all();
            }
        }
    }
    // Shutdown: the context is still alive; destroy the instance under it.
    if (g.inst) {
        if (g.dpy != EGL_NO_DISPLAY && g.ctx != EGL_NO_CONTEXT) {
            eglMakeCurrent(g.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, g.ctx);
        }
        sumi_destroy(g.inst);
        g.inst = nullptr;
    }
    detach_surface();
    if (g.dpy != EGL_NO_DISPLAY) {
        eglMakeCurrent(g.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g.ctx != EGL_NO_CONTEXT) eglDestroyContext(g.dpy, g.ctx);
        eglTerminate(g.dpy);
        g.dpy = EGL_NO_DISPLAY;
        g.ctx = EGL_NO_CONTEXT;
    }
    if (g.csv) { fclose(g.csv); g.csv = nullptr; }
    LOGI("render thread down");
}

// -- MIDI ---------------------------------------------------------------------

// The single point every producer goes through (DECISIONS #24: the mutex
// keeps "exactly one producer thread" true across AMidi poller / stress
// feeder handoffs; the core stays lock-free).
void push_midi(uint8_t status, uint8_t d1, uint8_t d2) {
    std::lock_guard<std::mutex> lk(g.push_mu);
    if (g.inst) sumi_push_midi(g.inst, status, d1, d2);
}

// Byte-stream parser (AMidi hands raw MIDI 1.0 data; be robust to running
// status and interleaved realtime bytes).
void parse_midi_bytes(Shell::OpenPort& p, const uint8_t* bytes, size_t n) {
    for (size_t i = 0; i < n; i++) {
        const uint8_t b = bytes[i];
        if (b >= 0xF8) continue;              // realtime: ignore
        if (b == 0xF0) { p.in_sysex = true; continue; }
        if (b == 0xF7) { p.in_sysex = false; continue; }
        if (p.in_sysex) continue;
        if (b >= 0x80) {
            if (b >= 0xF0) { p.status = 0; continue; }   // other system: drop
            p.status = b;
            p.have = 0;
            continue;
        }
        if (!p.status) continue;              // data byte with no status: drop
        const uint8_t kind = p.status & 0xF0;
        const int need = (kind == 0xC0 || kind == 0xD0) ? 1 : 2;
        if (p.have == 0) {
            p.d1 = b;
            p.have = 1;
            if (need == 1) {
                if (g.device_midi_enabled.load()) push_midi(p.status, p.d1, 0);
                p.have = 0;   // running status stays armed
            }
        } else {
            if (g.device_midi_enabled.load()) push_midi(p.status, p.d1, b);
            p.have = 0;
        }
    }
}

// One poller thread for ALL ports: with a single consumer-side thread the
// "exactly one producer" contract holds naturally however many devices are
// open (BLE + USB + virtual all look the same here).
void midi_poll_loop() {
    uint8_t buf[512];
    while (g.midi_running.load()) {
        {
            std::lock_guard<std::mutex> lk(g.ports_mu);
            for (auto& p : g.ports) {
                for (;;) {
                    int32_t opcode = 0;
                    size_t got = 0;
                    int64_t ts = 0;
                    const ssize_t r = AMidiOutputPort_receive(
                        p.port, &opcode, buf, sizeof(buf), &got, &ts);
                    if (r <= 0) break;
                    if (opcode == AMIDI_OPCODE_DATA && got > 0) {
                        parse_midi_bytes(p, buf, got);
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// -- stress feeder (tests/mpe_stress_alsa.cpp schedule, §5 Osmose script) -----

uint8_t clamp7(double v) {
    if (v < 0.0) return 0;
    if (v > 127.0) return 127;
    return (uint8_t)v;
}

// One 30-second cycle of the canonical schedule (10 voices, MCM, 200 Hz
// press/voice + staggered bends and CC74 — byte-identical to the desktop
// feeders), looped until minutes elapse.
void stress_loop(int minutes) {
    static const uint8_t notes[10] = {48, 55, 60, 64, 67, 72, 76, 79, 84, 91};
    const auto session_end = Clock::now() + std::chrono::minutes(minutes);
    long cycles = 0, sent = 0;
    LOGI("stress feeder: %d minutes (device MIDI ingestion paused)", minutes);
    g.device_midi_enabled = false;
    while (g.stress_running.load() && Clock::now() < session_end) {
        push_midi(0xB0, 101, 0);   // MCM: lower zone, 15 members
        push_midi(0xB0, 100, 6);
        push_midi(0xB0, 6, 15);
        sent += 3;
        for (int i = 0; i < 10; i++) {
            push_midi((uint8_t)(0x90 | (1 + i)), notes[i], (uint8_t)(60 + i * 6));
            sent++;
        }
        long tick = 0;
        const auto start = Clock::now();
        auto next = start;
        while (g.stress_running.load()) {
            const double t = std::chrono::duration<double>(Clock::now() - start).count();
            if (t >= 30.0 || Clock::now() >= session_end) break;
            for (int v = 0; v < 10; v++) {
                const uint8_t ch = (uint8_t)(1 + v);
                push_midi((uint8_t)(0xD0 | ch),
                          clamp7(64.0 + 60.0 * sin(t * (1.1 + 0.13 * v) + v)), 0);
                sent++;
                if (tick % 10 == v) {
                    const int bend = (int)(8192.0 + 2000.0 * sin(t * 0.7 + v * 0.9));
                    push_midi((uint8_t)(0xE0 | ch), (uint8_t)(bend & 0x7F),
                              (uint8_t)((bend >> 7) & 0x7F));
                    sent++;
                }
                if (tick % 20 == 2 * v) {
                    push_midi((uint8_t)(0xB0 | ch), 74,
                              clamp7(64.0 + 60.0 * sin(t * 0.5 + v)));
                    sent++;
                }
            }
            tick++;
            next += std::chrono::milliseconds(5);
            std::this_thread::sleep_until(next);
        }
        for (int i = 0; i < 10; i++) {
            push_midi((uint8_t)(0x80 | (1 + i)), notes[i], 64);
            sent++;
        }
        cycles++;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    g.device_midi_enabled = true;
    LOGI("stress feeder done: %ld cycles, %ld messages", cycles, sent);
    post([sent] {
        csv_event("# stress feeder done, %ld messages, dropped=%u",
                  sent, g.inst ? sumi_dropped_midi_count(g.inst) : 0);
    });
}

} // namespace

// -- JNI entry points ---------------------------------------------------------

extern "C" {

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeInit(JNIEnv* env, jobject, jstring files_dir) {
    const char* s = env->GetStringUTFChars(files_dir, nullptr);
    g.files_dir = s ? s : "";
    env->ReleaseStringUTFChars(files_dir, s);
    if (!g.running.exchange(true)) {
        g.render_thread = std::thread(render_loop);
        g.midi_running = true;
        g.midi_thread = std::thread(midi_poll_loop);
    }
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSurfaceCreated(JNIEnv* env, jobject,
                                                              jobject surface, jfloat density) {
    ANativeWindow* win = ANativeWindow_fromSurface(env, surface);
    if (!win) {
        LOGE("ANativeWindow_fromSurface returned NULL");
        return;
    }
    g.density_x100 = (uint32_t)(density * 100.0f + 0.5f);
    {
        std::lock_guard<std::mutex> lk(g.state_mu);
        if (g.pending_window) ANativeWindow_release(g.pending_window);
        g.pending_window = win;
    }
    g.state_cv.notify_all();
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSurfaceChanged(JNIEnv*, jobject,
                                                              jint w, jint h, jfloat density) {
    g.density_x100 = (uint32_t)(density * 100.0f + 0.5f);
    g.want_w = (uint32_t)w;
    g.want_h = (uint32_t)h;
}

// §5.4 teardown contract: BLOCKS until the render thread has unbound and
// destroyed the EGL surface and released the ANativeWindow.
JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSurfaceDestroyed(JNIEnv*, jobject) {
    std::unique_lock<std::mutex> lk(g.state_mu);
    if (!g.running.load()) return;
    // If the surface never attached (pending window still queued), just drop it.
    if (g.pending_window) {
        ANativeWindow_release(g.pending_window);
        g.pending_window = nullptr;
        return;
    }
    g.surface_released = false;
    g.release_requested = true;
    g.state_cv.notify_all();
    g.state_cv.wait(lk, [] { return g.surface_released || !g.running.load(); });
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeShutdown(JNIEnv*, jobject) {
    g.stress_running = false;
    if (g.stress_thread.joinable()) g.stress_thread.join();
    g.midi_running = false;
    if (g.midi_thread.joinable()) g.midi_thread.join();
    {
        std::lock_guard<std::mutex> lk(g.ports_mu);
        for (auto& p : g.ports) {
            AMidiOutputPort_close(p.port);
            AMidiDevice_release(p.dev);
        }
        g.ports.clear();
    }
    g.running = false;
    g.state_cv.notify_all();
    if (g.render_thread.joinable()) g.render_thread.join();
}

// -- touch / params (marshaled to the render thread) -------------------------

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeAddDrop(JNIEnv*, jobject, jfloat x, jfloat y) {
    post([=] { if (g.inst) sumi_add_drop(g.inst, x, y, 0.06f, 0); });
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeAddTine(JNIEnv*, jobject, jfloat x0, jfloat y0,
                                                       jfloat x1, jfloat y1, jfloat magnitude) {
    post([=] { if (g.inst) sumi_add_tine(g.inst, x0, y0, x1, y1, 0.035f, magnitude); });
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeAddVortex(JNIEnv*, jobject, jfloat x, jfloat y,
                                                         jfloat strength) {
    post([=] { if (g.inst) sumi_add_vortex(g.inst, x, y, strength, 0.18f,
                                           SUMI_VORTEX_EXPONENTIAL); });
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeTriggerDip(JNIEnv*, jobject) {
    post([] { if (g.inst) sumi_trigger_paper_dip(g.inst); });
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSetSimScale(JNIEnv*, jobject, jfloat s, jint why_thermal) {
    post([=] {
        if (!g.inst) return;
        sumi_params_t p;
        sumi_get_params(g.inst, &p);
        if (p.sim_scale == s) return;
        p.sim_scale = s;
        sumi_set_params(g.inst, &p);
        csv_event("# t=%.1f sim_scale -> %.2f (thermal %s)",
                  std::chrono::duration<double>(Clock::now() - g.session_start).count(),
                  (double)s, thermal_name(why_thermal));
    });
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSetThermal(JNIEnv*, jobject, jint status) {
    g.thermal = status;
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSetLayout(JNIEnv*, jobject, jint layout) {
    post([=] {
        if (!g.inst || layout < 0 || layout > 5) return;   // 5 = piano grid
        sumi_params_t p;
        sumi_get_params(g.inst, &p);
        if (p.pitch_layout == (uint32_t)layout) return;
        p.pitch_layout = (uint32_t)layout;
        sumi_set_params(g.inst, &p);
        csv_event("# t=%.1f layout -> %d",
                  std::chrono::duration<double>(Clock::now() - g.session_start).count(),
                  layout);
    });
}

// v0.4 (§4.3(5), DECISIONS_3 #34): CC74 routing (0 hue/aux, 1 pinch) and the
// pinch look (0 Hamiltonian saddle, 1 crossed tines) — core params, so the
// MIDI route honors them identically to iOS.
JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSetSlidePinch(JNIEnv*, jobject,
                                                             jint slide_mode,
                                                             jint pinch_variant) {
    post([=] {
        if (!g.inst) return;
        sumi_params_t p;
        sumi_get_params(g.inst, &p);
        const uint32_t sm = slide_mode == 1 ? 1u : 0u;
        const uint32_t pv = pinch_variant == 1 ? 1u : 0u;
        if (p.slide_mode == sm && p.pinch_variant == pv) return;
        p.slide_mode = sm;
        p.pinch_variant = pv;
        sumi_set_params(g.inst, &p);
    });
}

// v0.4 bend_mode (§4.3(6), DECISIONS_3 #35 corrected): PER-NOTE bend routing
// — 0 = v1 glide (bend drags the note's drop), 1 = the note bend breathes
// the sine ripple's wavelength (subtle vibrato; drop holds). Mod wheel /
// vortex untouched. NOTE: an amplitude control (CC 102 -> RIPPLE_AMP, as on
// iOS/desktop) lands with the Step-21 parity pass — until then a mapped CC
// from a device is the only Android amp source.
JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSetBendMode(JNIEnv*, jobject, jint mode) {
    post([=] {
        if (!g.inst) return;
        sumi_params_t p;
        sumi_get_params(g.inst, &p);
        const uint32_t bm = mode == 1 ? 1u : 0u;
        if (p.bend_mode != bm || p.ripple_bake != bm) {
            p.bend_mode = bm;
            p.ripple_bake = bm;   // #36: ripple vibrato bakes in, like glide
            sumi_set_params(g.inst, &p);
        }
        sumi_map_cc(g.inst, 0xFF, 102, SUMI_CTL_RIPPLE_AMP);
        sumi_map_cc(g.inst, 0xFF, 103, SUMI_CTL_RIPPLE_FREQ);
    });
}

// -- MIDI devices -------------------------------------------------------------

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeAddMidiDevice(JNIEnv* env, jobject, jobject device) {
    AMidiDevice* dev = nullptr;
    if (AMidiDevice_fromJava(env, device, &dev) != AMEDIA_OK || !dev) {
        LOGE("AMidiDevice_fromJava failed");
        return;
    }
    const int nports = (int)AMidiDevice_getNumOutputPorts(dev);
    int opened = 0;
    std::lock_guard<std::mutex> lk(g.ports_mu);
    for (int i = 0; i < nports; i++) {
        AMidiOutputPort* port = nullptr;
        if (AMidiOutputPort_open(dev, i, &port) == AMEDIA_OK && port) {
            g.ports.push_back({dev, port});
            opened++;
        }
    }
    LOGI("MIDI device attached: %d/%d output ports opened", opened, nports);
    if (opened == 0) AMidiDevice_release(dev);
}

// -- evidence hooks -----------------------------------------------------------

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeStartStress(JNIEnv*, jobject, jint minutes) {
    if (g.stress_running.exchange(true)) return;
    if (g.stress_thread.joinable()) g.stress_thread.join();
    g.stress_thread = std::thread([minutes] {
        stress_loop(minutes);
        g.stress_running = false;
    });
}

// Runs the §4.6 canonical script at 512×512 and writes the cross-backend dump
// (w,h uint32 LE + float32 RGBA rows, row 0 = top). Blocks the CALLING thread
// (never the UI thread — Kotlin calls this from a worker) until the render
// thread has produced the file. Restores the on-screen size afterwards.
JNIEXPORT jboolean JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeFieldDump(JNIEnv* env, jobject, jstring jpath) {
    const char* cpath = env->GetStringUTFChars(jpath, nullptr);
    std::string path = cpath ? cpath : "";
    env->ReleaseStringUTFChars(jpath, cpath);
    bool ok = false;
    post_sync([&] {
        if (!g.inst) return;
        const uint32_t restore_w = g.want_w.load(), restore_h = g.want_h.load();
        const float density = (float)g.density_x100.load() / 100.0f;
        // The canonical dump is 512×512 at sim_scale 1.0 (the shell default
        // 0.75 would shrink the field to 384×384); restored below.
        sumi_params_t params;
        sumi_get_params(g.inst, &params);
        const float restore_scale = params.sim_scale;
        params.sim_scale = 1.0f;
        sumi_set_params(g.inst, &params);
        sumi_resize(g.inst, 512, 512, 1.0f);
        sumi_update(g.inst, 1.0 / 120.0);   // settle one identity frame
        sumi_render(g.inst);
        sumi_debug_run_field_script(g.inst);
        sumi_update(g.inst, 1.0 / 120.0);
        sumi_render(g.inst);                // drains the script's 7 passes
        uint32_t w = 0, h = 0;
        if (sumi_debug_read_field(g.inst, nullptr, 0, &w, &h) && w && h) {
            const size_t texels = (size_t)w * h;
            std::vector<uint16_t> halves(texels * 4);
            std::vector<float> floats(texels * 4);
            if (sumi_debug_read_field(g.inst, (uint8_t*)halves.data(), texels * 8, &w, &h)) {
                for (size_t i = 0; i < texels * 4; i++) floats[i] = half_to_float(halves[i]);
                FILE* f = fopen(path.c_str(), "wb");
                if (f) {
                    ok = fwrite(&w, sizeof(uint32_t), 1, f) == 1 &&
                         fwrite(&h, sizeof(uint32_t), 1, f) == 1 &&
                         fwrite(floats.data(), sizeof(float), texels * 4, f) == texels * 4;
                    fclose(f);
                }
            }
        }
        params.sim_scale = restore_scale;
        sumi_set_params(g.inst, &params);
        if (restore_w && restore_h) sumi_resize(g.inst, restore_w, restore_h, density);
        LOGI("field dump %s: %s", ok ? "written" : "FAILED", path.c_str());
    });
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeDroppedMidi(JNIEnv*, jobject) {
    return g.inst ? (jint)sumi_dropped_midi_count(g.inst) : -1;
}

} // extern "C"
