// sumi_jni.cpp — the Android host shell (§5.4): SurfaceView lifecycle → EGL
// host-owned context (§5.1) on a dedicated render thread, AMidi →
// sumi_push_midi, touch/param marshaling, and the step-14 evidence hooks
// (field dump, stress feeder, per-second CSV). The Phase-4 play surface
// (hostmpe host, transports, byte log) is the sibling TU sumi_play.cpp; the
// plumbing shared between them is declared in shell.h.
//
// Threading (§5.2, the step's real difficulty): every sumi_* call except
// sumi_push_midi happens on the ONE render thread that owns the EGL context.
// Everything from Kotlin (touches, params, dip, surface sizes) is marshaled
// through a small command queue drained at the top of each render-thread
// frame. sumi_push_midi is called from exactly one producer at a time — the
// AMidi poller thread (which also hosts hostmpe, DECISIONS_2 #33) or the
// stress feeder — serialized by a producer mutex (DECISIONS #24 ported to
// the JNI layer).
//
// Teardown contract (§5.4, hard requirement): nativeSurfaceDestroyed BLOCKS
// the UI thread until the render thread has finished its in-flight frame,
// unbound the surface (eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
// ctx) — surfaceless-context is mandatory on Android's EGL since 7.0), and
// destroyed the EGL surface; only then is the ANativeWindow released and the
// call allowed to return. The EGL CONTEXT survives surface cycles — the
// field textures live in it, so DECISIONS_2 #28's resize preservation
// carries the drawing across rotations.
#include "shell.h"

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <amidi/AMidi.h>
#include <EGL/egl.h>

#include "sumi_debug.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

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

    // -- host-owned params snapshot (PROJECT_SPEC.md §8.2; probe ground truth) ----------
    std::mutex params_mu;
    sumi_params_t snapshot{};
    bool snapshot_seeded = false;   // non-host fields copied from the core once

    // -- MIDI producers (§5.2: exactly one at a time; DECISIONS #24 mutex) ----
    std::mutex push_mu;
    std::atomic<bool> device_midi_enabled{true};
    std::thread midi_thread;
    std::atomic<bool> midi_running{false};
    std::mutex ports_mu;
    struct OpenPort {
        AMidiDevice* dev;
        AMidiOutputPort* port;   // "output port" = data flowing OUT of the device
        int32_t device_id;       // MidiDeviceInfo.getId(), for removal
        bool owns_device;        // exactly ONE port per device releases it
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
    std::mutex stats_mu;
    shell::Stats stats{};
};

Shell g;

void apply_params_snapshot();   // render thread

} // namespace

// ---- shell.h plumbing -------------------------------------------------------

namespace shell {

double now_s() {
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

void post(std::function<void()> fn) {
    {
        std::lock_guard<std::mutex> lk(g.q_mu);
        g.commands.push_back(std::move(fn));
    }
    // The waiter evaluates its predicate holding state_mu, so the signal must
    // be taken under state_mu too: otherwise a notify landing between
    // "predicate false" and wait() is lost, and a posted command sits until
    // some unrelated event wakes the render thread (post_sync would hang).
    {
        std::lock_guard<std::mutex> lk(g.state_mu);
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

// The single point every producer goes through (DECISIONS #24: the mutex
// keeps "exactly one producer thread" true across AMidi poller / stress
// feeder handoffs; the core stays lock-free).
void push_midi(uint8_t status, uint8_t d1, uint8_t d2) {
    std::lock_guard<std::mutex> lk(g.push_mu);
    if (g.inst) sumi_push_midi(g.inst, status, d1, d2);
}

const std::string& files_dir() { return g.files_dir; }

sumi_params_t params_snapshot() {
    std::lock_guard<std::mutex> lk(g.params_mu);
    return g.snapshot;
}

void params_modify(const std::function<void(sumi_params_t&)>& fn) {
    {
        std::lock_guard<std::mutex> lk(g.params_mu);
        fn(g.snapshot);
    }
    post([] { apply_params_snapshot(); });
}

Stats stats() {
    std::lock_guard<std::mutex> lk(g.stats_mu);
    return g.stats;
}

} // namespace shell

namespace {

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

} // namespace

void shell::csv_event(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    LOGI("%s", buf);
    csv_line(buf);
}

namespace {

using shell::csv_event;
using shell::now_s;

// Host-owned fields (the ones the shell writes): everything else is the
// core's default. Applied on the render thread from the UI-owned snapshot.
void apply_params_snapshot() {
    if (!g.inst) return;
    sumi_params_t cur;
    sumi_get_params(g.inst, &cur);
    sumi_params_t want;
    {
        std::lock_guard<std::mutex> lk(g.params_mu);
        if (!g.snapshot_seeded) {
            // First apply: adopt the core's defaults for every non-host field
            // so the snapshot is a complete params struct from here on.
            const sumi_params_t host = g.snapshot;
            g.snapshot = cur;
            g.snapshot.sim_scale = host.sim_scale;
            g.snapshot.pitch_layout = host.pitch_layout;
            g.snapshot.slide_mode = host.slide_mode;
            g.snapshot.pinch_variant = host.pinch_variant;
            g.snapshot.bend_mode = host.bend_mode;
            g.snapshot.ripple_bake = host.ripple_bake;
            g.snapshot.press_mode = host.press_mode;
            g.snapshot_seeded = true;
        }
        want = g.snapshot;
    }
    const bool changed =
        cur.sim_scale != want.sim_scale || cur.pitch_layout != want.pitch_layout ||
        cur.slide_mode != want.slide_mode || cur.pinch_variant != want.pinch_variant ||
        cur.bend_mode != want.bend_mode || cur.ripple_bake != want.ripple_bake ||
        cur.press_mode != want.press_mode;
    if (!changed) return;
    if (cur.pitch_layout != want.pitch_layout) {
        csv_event("# t=%.1f layout -> %u",
                  std::chrono::duration<double>(Clock::now() - g.session_start).count(),
                  want.pitch_layout);
    }
    if (cur.sim_scale != want.sim_scale) {
        csv_event("# t=%.1f sim_scale -> %.2f (thermal %s)",
                  std::chrono::duration<double>(Clock::now() - g.session_start).count(),
                  (double)want.sim_scale, thermal_name(g.thermal.load()));
    }
    cur.sim_scale = want.sim_scale;
    cur.pitch_layout = want.pitch_layout;
    cur.slide_mode = want.slide_mode;
    cur.pinch_variant = want.pinch_variant;
    cur.bend_mode = want.bend_mode;
    cur.ripple_bake = want.ripple_bake;
    cur.press_mode = want.press_mode;
    sumi_set_params(g.inst, &cur);
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
    if (!egl_check("eglMakeCurrent(window)", eglMakeCurrent(g.dpy, g.surf, g.surf, g.ctx))) {
        // Leaving g.surf set here would let the frame loop render into a
        // surface with no current context.
        eglDestroySurface(g.dpy, g.surf);
        g.surf = EGL_NO_SURFACE;
        ANativeWindow_release(win);
        g.window = nullptr;
        return;
    }
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
        g.session_start = g.second_start = Clock::now();
        // Host-owned params (sim_scale 0.75 default for phone/tablet GPUs,
        // layout, v0.4 routing) come from the UI-owned snapshot.
        apply_params_snapshot();
        // v0.4 ripple ctls (DECISIONS_3 #32/#35): CC 102/103 are the shell's
        // local handles for amplitude/wavelength — the strip's assignable
        // wheels and external devices ride them (the default map ships the
        // dims unmapped).
        sumi_map_cc(g.inst, 0xFF, 102, SUMI_CTL_RIPPLE_AMP);
        sumi_map_cc(g.inst, 0xFF, 103, SUMI_CTL_RIPPLE_FREQ);
        // Play mode may already be effective (persisted setting, cold start):
        // the loopback handshake sent before the instance existed went
        // nowhere — the play half re-sends it now that there is a consumer.
        shell::play_instance_ready();
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
    // Touch-down -> this render is the first that can show the drop (PROJECT_SPEC.md
    // §8.6 latency budget): resolve the marks the MIDI thread left.
    shell::play_frame_rendered(now_s());
    const double frame_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - f0).count();

    g.frames_this_second++;
    if (frame_ms > g.worst_frame_ms) g.worst_frame_ms = frame_ms;
    const double since = std::chrono::duration<double>(Clock::now() - g.second_start).count();
    if (since >= 1.0) {
        const double session_t =
            std::chrono::duration<double>(Clock::now() - g.session_start).count();
        char line[128];
        snprintf(line, sizeof(line), "%.0f,%.1f,%.2f,%s",
                 session_t, (double)g.frames_this_second / since, g.worst_frame_ms,
                 thermal_name(g.thermal.load()));
        csv_line(line);
        {
            std::lock_guard<std::mutex> lk(g.stats_mu);
            g.stats.t = session_t;
            g.stats.fps = (float)((double)g.frames_this_second / since);
            g.stats.worst_ms = (float)g.worst_frame_ms;
            g.stats.thermal = g.thermal.load();
            g.stats.dropped = sumi_dropped_midi_count(g.inst);
        }
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
                // §5.4: a destroy must ALWAYS be answered. Without this the
                // UI thread waits forever whenever the render thread parked
                // with no surface — which is exactly what happens when
                // attach_surface failed (no ES3 config, eglCreateWindowSurface
                // or sumi_create failure), and the ANR needs a force-stop.
                if (g.release_requested) return true;
                std::lock_guard<std::mutex> qlk(g.q_mu);
                return !g.commands.empty();
            });
            // Nothing is attached: acknowledge the destroy right here.
            if (g.release_requested && g.surf == EGL_NO_SURFACE && !g.pending_window) {
                g.release_requested = false;
                g.surface_released = true;
                g.state_cv.notify_all();
            }
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
        {
            // No producer may push into an instance being destroyed.
            std::lock_guard<std::mutex> lk(g.push_mu);
            sumi_destroy(g.inst);
            g.inst = nullptr;
        }
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

// Byte-stream parser (AMidi hands raw MIDI 1.0 data; be robust to running
// status and interleaved realtime bytes). Complete messages go to the merge
// point (sumi_play.cpp): external-occupancy mask, byte log, loopback push.
void parse_midi_bytes(Shell::OpenPort& p, const uint8_t* bytes, size_t n) {
    const double now = now_s();
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
                if (g.device_midi_enabled.load()) shell::play_ingest_external(now, p.status, p.d1, 0);
                p.have = 0;   // running status stays armed
            }
        } else {
            if (g.device_midi_enabled.load()) shell::play_ingest_external(now, p.status, p.d1, b);
            p.have = 0;
        }
    }
}

// One poller thread for ALL ports: with a single consumer-side thread the
// "exactly one producer" contract holds naturally however many devices are
// open (BLE + USB + virtual all look the same here). Phase 4 (DECISIONS_2
// #33 / PROJECT_SPEC.md §8.5): this thread ALSO hosts hostmpe and the outbound
// limiters — touch bytes are handed to it through the play command queue,
// drained at the top of each iteration, and the 1 ms cadence becomes a
// condvar wait so a posted command wakes it immediately.
void midi_poll_loop() {
    shell::play_thread_enter();
    uint8_t buf[512];
    while (g.midi_running.load()) {
        shell::play_drain(now_s());
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
        shell::play_wait(1);
    }
    shell::play_thread_exit();
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
    using shell::push_midi;
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
    shell::post([sent] {
        csv_event("# stress feeder done, %ld messages, dropped=%u",
                  sent, g.inst ? sumi_dropped_midi_count(g.inst) : 0);
    });
}

} // namespace

// -- JNI entry points ---------------------------------------------------------

extern "C" {

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeInit(JNIEnv* env, jobject, jstring files_dir) {
    if (!g.running.exchange(true)) {
        // Inside the guard: the MIDI thread reads files_dir by const
        // reference, so a second nativeInit in a live process (launchMode
        // singleTask survives a back-out) must not reassign the string under
        // it.
        const char* s = env->GetStringUTFChars(files_dir, nullptr);
        g.files_dir = s ? s : "";
        env->ReleaseStringUTFChars(files_dir, s);
        {
            std::lock_guard<std::mutex> lk(g.params_mu);
            memset(&g.snapshot, 0, sizeof(g.snapshot));
            g.snapshot.sim_scale = 0.75f;   // host default for phone/tablet GPUs
        }
        shell::play_init(env);
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
    shell::play_wait(0);   // no-op wake so the poller sees midi_running = false
    if (g.midi_thread.joinable()) g.midi_thread.join();
    {
        std::lock_guard<std::mutex> lk(g.ports_mu);
        for (auto& p : g.ports) {
            AMidiOutputPort_close(p.port);
            if (p.owns_device) AMidiDevice_release(p.dev);   // once per DEVICE
        }
        g.ports.clear();
    }
    shell::play_shutdown();
    g.running = false;
    g.state_cv.notify_all();
    if (g.render_thread.joinable()) g.render_thread.join();
}

// -- touch / params (marshaled to the render thread) -------------------------

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeAddDrop(JNIEnv*, jobject, jfloat x, jfloat y) {
    shell::post([=] { if (g.inst) sumi_add_drop(g.inst, x, y, 0.06f, 0); });
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeAddTine(JNIEnv*, jobject, jfloat x0, jfloat y0,
                                                       jfloat x1, jfloat y1, jfloat magnitude) {
    shell::post([=] { if (g.inst) sumi_add_tine(g.inst, x0, y0, x1, y1, 0.035f, magnitude); });
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeAddVortex(JNIEnv*, jobject, jfloat x, jfloat y,
                                                         jfloat strength) {
    shell::post([=] { if (g.inst) sumi_add_vortex(g.inst, x, y, strength, 0.18f,
                                                  SUMI_VORTEX_EXPONENTIAL); });
}

// v0.4 gesture-ABI passes (PROJECT_SPEC.md §8.7, DECISIONS_3 #32/#41): the pen's
// dipolar wake (physical, never MIDI) and the pinch (fold axis is host-side
// data — pen azimuth or the two-finger line). Render thread via post.
JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeAddWake(JNIEnv*, jobject, jfloat x0, jfloat y0,
                                                       jfloat x1, jfloat y1, jfloat tip) {
    shell::post([=] { if (g.inst) sumi_add_wake(g.inst, x0, y0, x1, y1, tip); });
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeAddPinch(JNIEnv*, jobject, jfloat x, jfloat y,
                                                        jfloat k, jfloat angle) {
    shell::post([=] { if (g.inst) sumi_add_pinch(g.inst, x, y, k, angle); });
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeTriggerDip(JNIEnv*, jobject) {
    shell::post([] { if (g.inst) sumi_trigger_paper_dip(g.inst); });
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSetSimScale(JNIEnv*, jobject, jfloat s, jint) {
    shell::params_modify([=](sumi_params_t& p) { p.sim_scale = s; });
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSetThermal(JNIEnv*, jobject, jint status) {
    g.thermal = status;
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSetLayout(JNIEnv*, jobject, jint layout) {
    if (layout < 0 || layout > 5) return;   // 5 = piano grid
    shell::params_modify([=](sumi_params_t& p) { p.pitch_layout = (uint32_t)layout; });
}

// v0.4 (§4.3(5), DECISIONS_3 #34): CC74 routing (0 hue/aux, 1 pinch) and the
// pinch look (0 Hamiltonian saddle, 1 crossed tines) — core params, so the
// MIDI route honors them identically to iOS.
JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSetSlidePinch(JNIEnv*, jobject,
                                                             jint slide_mode,
                                                             jint pinch_variant) {
    shell::params_modify([=](sumi_params_t& p) {
        p.slide_mode = slide_mode == 1 ? 1u : 0u;
        p.pinch_variant = pinch_variant == 1 ? 1u : 0u;
    });
}

// v0.4 bend_mode (§4.3(6), DECISIONS_3 #35 corrected): PER-NOTE bend routing
// — 0 = v1 glide (bend drags the note's drop), 1 = the note bend breathes
// the sine ripple (#36: bakes in, like glide). Mod wheel / vortex untouched.
JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSetBendMode(JNIEnv*, jobject, jint mode) {
    shell::params_modify([=](sumi_params_t& p) {
        p.bend_mode = mode == 1 ? 1u : 0u;
        p.ripple_bake = p.bend_mode;
    });
}

// v0.4 press_mode (§3.4, step 20): 0xD0 hardware routing — 0 = ink feed (v1
// grow), 1 = the Lamb–Oseen swirl. The surface's own down-pull emits 0xA0,
// which swirls in EITHER mode.
JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSetPressMode(JNIEnv*, jobject, jint mode) {
    shell::params_modify([=](sumi_params_t& p) { p.press_mode = mode == 1 ? 1u : 0u; });
}

// -- MIDI devices -------------------------------------------------------------

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeAddMidiDevice(JNIEnv* env, jobject,
                                                             jobject device, jint device_id) {
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
            // fromJava hands out ONE reference for the device however many
            // ports it has: exactly one entry owns the release, or teardown
            // double-frees a multi-port device.
            g.ports.push_back({dev, port, (int32_t)device_id, opened == 0});
            opened++;
        }
    }
    LOGI("MIDI device %d attached: %d/%d output ports opened", (int)device_id, opened, nports);
    if (opened == 0) AMidiDevice_release(dev);
}

// A device left (unplugged, BLE dropped): its ports must leave the poller —
// otherwise AMidiOutputPort_receive keeps being called on a dead port every
// millisecond, a replug appends a second set, and teardown closes ports whose
// Java device is already closed.
JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeRemoveMidiDevice(JNIEnv*, jobject, jint device_id) {
    std::lock_guard<std::mutex> lk(g.ports_mu);
    int closed = 0;
    AMidiDevice* release_me = nullptr;
    for (auto it = g.ports.begin(); it != g.ports.end();) {
        if (it->device_id == (int32_t)device_id) {
            AMidiOutputPort_close(it->port);
            if (it->owns_device) release_me = it->dev;
            it = g.ports.erase(it);
            closed++;
        } else {
            ++it;
        }
    }
    if (release_me) AMidiDevice_release(release_me);
    if (closed) LOGI("MIDI device %d removed: %d port(s) closed", (int)device_id, closed);
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
    shell::post_sync([&] {
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
    // Under the producer mutex: g.inst is destroyed under it too (contract 2
    // otherwise puts this call on the wrong thread).
    std::lock_guard<std::mutex> lk(g.push_mu);
    return g.inst ? (jint)sumi_dropped_midi_count(g.inst) : -1;
}

} // extern "C"
