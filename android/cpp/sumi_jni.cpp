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
#include "sumi_preset.h"   // step 45b: the one session, through the one serializer (DECISIONS_5 #73/#79)
#include "voxo.h"          // Phase 7 step 48: the internal sound on AAudio (the latency spike)

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <map>
#include <string>
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
    // -- step 45b: THE SESSION (DECISIONS_5 #79, the iPad's #73 model) ---------
    // One sumi_preset_t: the params, the custom palette, the CC map, the input
    // dialect, the routed controls, the strip's wheel CCs. Kotlin reads it as
    // the serializer's JSON and changes it with JSON patches read OVER it (the
    // schema rule: keys present overwrite, missing keys keep); the render
    // thread applies what changed. Seeded from the core's defaults right after
    // sumi_create; before that the texts Kotlin handed in wait here.
    sumi_preset_t sess{};                 // under params_mu
    bool sess_ready = false;              // under params_mu
    std::string sess_text, sess_legacy;   // under params_mu: the last session / a 0.x migration patch
    float host_sim_scale = 0.75f;         // under params_mu: the thermal listener owns sim_scale (DECISIONS #31)
    // render thread only: what the core has now
    bool applied_valid = false;
    sumi_params_t applied_params{};
    sumi_palette_t applied_palette{};
    std::vector<uint32_t> applied_cc;      // (channel, cc, target) triples
    uint32_t applied_input = 0;
    std::map<uint32_t, uint8_t> controls_sent;

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

    // -- Phase 7 step 48: Voxo beside the core (SOUND §1: the one producer
    // fans the same bytes into a second ring). Created in nativeInit, fed
    // under push_mu, started only by the spike intent until step 54 wires
    // the setting and the lifecycle. --------------------------------------
    voxo_t* voxo = nullptr;                       // under push_mu for the fan-out; Kotlin's Sound owns start/stop (step 54)
    bool local_control = true;                    // under push_mu (step 54, #12)
    std::atomic<double> last_touch_down{0.0};     // the play surface's mark (shell::mark_touch_down)
    std::atomic<bool> spike_running{false};

    // -- evidence CSV (t,fps,worst_frame_ms,thermal — iOS logger port) -------
    std::string files_dir;
    FILE* csv = nullptr;
    Clock::time_point session_start, second_start;
    int frames_this_second = 0;
    double worst_frame_ms = 0.0;
    std::atomic<int> thermal{0};
    std::atomic<long> egl_error_count{0};

    // -- step 45b: THE PRINT LEDGER (QOL §4, the iPad's PrintLedger.swift) ---
    // Each dip keeps its field (sumi_read_field), params and palette; the
    // thumbnail when the print lands; any entry re-exports at any size. A
    // clear's print is read and dropped on arrival. Six entries or 256 MB.
    struct LedgerEntry {
        uint32_t id = 0;
        int64_t when = 0;                 // unix seconds
        std::vector<uint8_t> field;       // RGBA16F
        uint32_t fw = 0, fh = 0;
        sumi_params_t params{};
        sumi_palette_t palette{};
        std::vector<uint8_t> thumb;       // RGBA8
        uint32_t tw = 0, th = 0;
        std::vector<uint8_t> print;       // RGBA8, the newest entry only
        uint32_t pw = 0, ph = 0;
        bool print_seen = false;
    };
    std::mutex ledger_mu;
    std::vector<LedgerEntry> ledger;      // newest last; under ledger_mu
    uint32_t ledger_next_id = 1;
    std::string ledger_status;            // under ledger_mu
    int  print_expect = 0;                // render thread: 1 keep (the newest entry), 2 drop (a clear)
    int  print_frames = 0;                // render thread
    bool export_pending = false;          // render thread
    uint32_t export_w = 0, export_h = 0;  // render thread
    std::vector<uint8_t> export_px;       // under ledger_mu: the finished export (RGBA8)
    uint32_t export_rw = 0, export_rh = 0;
    bool export_ready = false;            // under ledger_mu
    // gestures (#75): the long press's boundary radius, render thread
    bool press_active = false;
    float press_x = 0, press_y = 0, press_R = 0;
    std::mutex stats_mu;
    shell::Stats stats{};
};

Shell g;

void apply_session();          // render thread

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
void push_midi(uint8_t status, uint8_t d1, uint8_t d2, bool local) {
    std::lock_guard<std::mutex> lk(g.push_mu);
    if (g.inst) sumi_push_midi(g.inst, status, d1, d2);
    if (g.voxo && (!local || g.local_control)) voxo_push_midi(g.voxo, status, d1, d2);   // step 48: the second ring, same bytes, same producer; step 54: Local Control
}

void mark_touch_down(double t_down) { g.last_touch_down.store(t_down, std::memory_order_relaxed); }

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
    post([] { apply_session(); });
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

size_t ledger_bytes_locked() {
    size_t b = 0;
    for (const auto& e : g.ledger) b += e.field.size() + e.thumb.size() + e.print.size();
    return b;
}
void ledger_evict_locked() {
    while (!g.ledger.empty() && (g.ledger.size() > 6 || ledger_bytes_locked() > (256u << 20)))
        g.ledger.erase(g.ledger.begin());
}

// Render thread, every frame: the dip's print lands (kept on the newest entry
// with its thumbnail, or dropped for a clear); an export in flight is polled.
void service_ledger() {
    if (!g.inst) return;
    if (g.print_expect) {
        uint32_t w = 0, h = 0;
        if (sumi_read_print(g.inst, nullptr, 0, &w, &h) && w && h) {
            std::vector<uint8_t> px((size_t)w * h * 4);
            if (sumi_read_print(g.inst, px.data(), px.size(), &w, &h)) {
                if (g.print_expect == 1) {
                    // a thumbnail, box-averaged, at most 240 wide / 150 tall
                    uint32_t sx = (w + 239) / 240, sy = (h + 149) / 150, sc = sx > sy ? sx : sy; if (sc < 1) sc = 1;
                    const uint32_t tw = w / sc, th = h / sc;
                    std::vector<uint8_t> t((size_t)tw * th * 4);
                    for (uint32_t y = 0; y < th; y++) for (uint32_t x = 0; x < tw; x++) {
                        uint32_t acc[4] = {0, 0, 0, 0};
                        for (uint32_t yy = 0; yy < sc; yy++) for (uint32_t xx = 0; xx < sc; xx++) {
                            const uint8_t* q = &px[(((size_t)(y * sc + yy)) * w + (x * sc + xx)) * 4];
                            for (int c = 0; c < 4; c++) acc[c] += q[c];
                        }
                        for (int c = 0; c < 4; c++) t[((size_t)y * tw + x) * 4 + c] = (uint8_t)(acc[c] / (sc * sc));
                    }
                    std::lock_guard<std::mutex> lk(g.ledger_mu);
                    if (!g.ledger.empty() && !g.ledger.back().print_seen) {
                        auto& e = g.ledger.back();
                        for (auto& o : g.ledger) { o.print.clear(); o.print.shrink_to_fit(); }   // only the newest keeps its print
                        e.print.swap(px); e.pw = w; e.ph = h; e.thumb.swap(t); e.tw = tw; e.th = th; e.print_seen = true;
                        ledger_evict_locked();
                    }
                    LOGI("[dip] print %ux%u kept in the ledger", w, h);
                } else {
                    LOGI("[dip] fresh sheet, print %ux%u dropped", w, h);
                }
            }
            g.print_expect = 0;
        } else if (++g.print_frames > 240) {
            LOGI("[dip] no print after %d frames", g.print_frames);
            g.print_expect = 0;
        }
    }
    if (g.export_pending) {
        // The full poll is what advances the readback (the size query never
        // does): poll with the buffer every frame the export is in flight, as
        // the desktop ledger does; the buffer is kept across frames.
        static std::vector<uint8_t> buf;
        const size_t need = (size_t)g.export_w * g.export_h * 4;
        if (buf.size() != need) buf.assign(need, 0);
        uint32_t w = 0, h = 0;
        const int st = sumi_export_poll(g.inst, buf.data(), buf.size(), &w, &h);
        if (st == 0) {
            g.export_pending = false; buf.clear(); buf.shrink_to_fit();
            LOGI("[print] export failed in flight");
            std::lock_guard<std::mutex> lk(g.ledger_mu); g.ledger_status = "Export failed";
        } else if (st == 2) {
            g.export_pending = false;
            std::lock_guard<std::mutex> lk(g.ledger_mu);
            g.export_px.swap(buf); buf.clear(); buf.shrink_to_fit();
            g.export_rw = w; g.export_rh = h; g.export_ready = true;
            char b[96]; snprintf(b, sizeof b, "Exported %ux%u", w, h); g.ledger_status = b;
            LOGI("[print] export %ux%u done", w, h);
        }
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

// Step 45b (DECISIONS_5 #79): the session onto the core, piecewise, only what
// changed — render thread. sim_scale is the host's (the thermal listener), not
// the session's: the session keeps its own value so a preset round-trips.
void apply_session() {
    if (!g.inst) return;
    sumi_preset_t s;
    float host_sim;
    {
        std::lock_guard<std::mutex> lk(g.params_mu);
        if (!g.sess_ready) return;
        s = g.sess;
        host_sim = g.host_sim_scale;
    }
    sumi_params_t want = s.params;
    want.sim_scale = host_sim;
    if (!g.applied_valid || memcmp(&want, &g.applied_params, sizeof want) != 0) {
        const double t = std::chrono::duration<double>(Clock::now() - g.session_start).count();
        if (!g.applied_valid || want.pitch_layout != g.applied_params.pitch_layout) csv_event("# t=%.1f layout -> %u", t, want.pitch_layout);
        if (!g.applied_valid || want.sim_scale != g.applied_params.sim_scale)
            csv_event("# t=%.1f sim_scale -> %.2f (thermal %s)", t, (double)want.sim_scale, thermal_name(g.thermal.load()));
        if (!g.applied_valid || want.medium != g.applied_params.medium) csv_event("# t=%.1f medium -> %u", t, want.medium);
        sumi_set_params(g.inst, &want);
        g.applied_params = want;
        sumi_params_t clamped;
        sumi_get_params(g.inst, &clamped);
        std::lock_guard<std::mutex> lk(g.params_mu);
        g.snapshot = clamped;   // the probe's ground truth (§8.2) is what the core holds
    }
    if (!g.applied_valid || memcmp(&s.palette, &g.applied_palette, sizeof s.palette) != 0) {
        sumi_set_palette(g.inst, &s.palette);
        g.applied_palette = s.palette;
    }
    std::vector<uint32_t> cc;
    for (uint32_t i = 0; i < s.cc_count && i < SUMI_PRESET_MAX_CC; i++) {
        cc.push_back(s.cc[i].channel); cc.push_back(s.cc[i].cc); cc.push_back(s.cc[i].target);
    }
    if (!g.applied_valid || cc != g.applied_cc) {
        sumi_clear_cc_map(g.inst);
        if (g.voxo) voxo_clear_cc_map(g.voxo);   // step 54 (#27): the bus routes of the one map
        for (size_t i = 0; i + 2 < cc.size(); i += 3) {
            if (cc[i + 1] > 127) continue;
            if (cc[i + 2] >= 1000u) { if (g.voxo) voxo_map_cc(g.voxo, (uint8_t)(cc[i] == 0xFF ? 0xFF : (cc[i] & 0x0F)), (uint8_t)cc[i + 1], cc[i + 2]); continue; }   // Voxo's bus, before the core's range check
            if (cc[i + 2] >= SUMI_CTL_COUNT) continue;
            sumi_map_cc(g.inst, (uint8_t)(cc[i] == 0xFF ? 0xFF : (cc[i] & 0x0F)), (uint8_t)cc[i + 1], (sumi_ctl_t)cc[i + 2]);
        }
        g.applied_cc = cc;
        g.controls_sent.clear();   // the handles may have moved: send every control again
    }
    const uint32_t im = (s.input_mode >= 1 && s.input_mode <= 3) ? s.input_mode : 1u;
    if (!g.applied_valid || im != g.applied_input) {
        sumi_set_input_mode(g.inst, (sumi_input_mode_t)im);   // #60: a setting, never a detection
        if (g.voxo) voxo_set_input_mode(g.voxo, im);          // step 54 (#25): Voxo speaks the session's dialect too
        g.applied_input = im;
    }
    g.applied_valid = true;
    // The routed controls: each changed value travels as its routed CC through
    // the sole MIDI producer, the route a controller would use (#56, #73).
    for (uint32_t i = 0; i < s.control_count && i < SUMI_PRESET_MAX_CONTROLS; i++) {
        const uint32_t ctl = s.controls[i].ctl;
        const uint8_t v = s.controls[i].value > 127 ? 127 : s.controls[i].value;
        auto it = g.controls_sent.find(ctl);
        if (it != g.controls_sent.end() && it->second == v) continue;
        for (size_t k = 0; k + 2 < cc.size(); k += 3) {
            if (cc[k + 2] == ctl) { shell::play_send_cc((uint8_t)cc[k + 1], v); break; }
        }
        g.controls_sent[ctl] = v;
    }
}

// The session's defaults: the core's params and palette as sumi_create left
// them (the values a preset's missing keys fall back to), the host's
// sim_scale, the desktop's default CC map (app_settings_default_routes, with
// the Phase-6 handles 104-109), the controls at rest, MPE.
void session_defaults(sumi_preset_t& p) {
    sumi_params_t d; sumi_palette_t pd;
    sumi_get_params(g.inst, &d);
    sumi_get_palette(g.inst, &pd);
    d.sim_scale = 0.75f;
    sumi_preset_init(&p, &d, &pd);
    p.input_mode = 1;
    static const uint8_t routes[][2] = {
        {1, SUMI_CTL_VORTEX_STRENGTH}, {2, SUMI_CTL_INK_FLOW}, {7, SUMI_CTL_INK_FLOW}, {11, SUMI_CTL_INK_FLOW},
        {26, SUMI_CTL_VORTEX_STRENGTH}, {24, SUMI_CTL_VORTEX_X}, {22, SUMI_CTL_VORTEX_Y},
        {27, SUMI_CTL_SWIRL_STRENGTH}, {25, SUMI_CTL_SWIRL_X}, {23, SUMI_CTL_SWIRL_Y},
        {20, SUMI_CTL_PINCH_SADDLE}, {21, SUMI_CTL_PINCH_CROSS}, {28, SUMI_CTL_RIPPLE_FREQ}, {29, SUMI_CTL_RIPPLE_AMP},
        {102, SUMI_CTL_RIPPLE_AMP}, {103, SUMI_CTL_RIPPLE_FREQ},
        {104, 14}, {105, 15}, {106, 16}, {107, 17}, {108, 18}, {109, 19}};
    p.cc_count = 0;
    for (const auto& r : routes) { p.cc[p.cc_count].channel = 0xFF; p.cc[p.cc_count].cc = r[0]; p.cc[p.cc_count].target = r[1]; p.cc_count++; }
    static const uint8_t ctls[][2] = {{7, 0}, {8, 32}, {16, 0}, {17, 0}, {18, 64}, {19, 0}};
    p.control_count = 0;
    for (const auto& c : ctls) { p.controls[p.control_count].ctl = c[0]; p.controls[p.control_count].value = c[1]; p.control_count++; }
}

// Render thread, right after sumi_create: the session from the last one (or,
// the first time, the 0.x rows Kotlin read), over the core's defaults.
void session_attach() {
    sumi_preset_t p;
    session_defaults(p);
    std::string text, legacy;
    {
        std::lock_guard<std::mutex> lk(g.params_mu);
        text = g.sess_text; legacy = g.sess_legacy;
    }
    const char* from = "defaults";
    if (!text.empty() && sumi_preset_read(text.c_str(), 0, &p)) from = "last session";
    else if (!legacy.empty() && sumi_preset_read(legacy.c_str(), 0, &p)) from = "the 0.x settings (migrated once)";
    {
        std::lock_guard<std::mutex> lk(g.params_mu);
        g.sess = p;
        g.sess_ready = true;
    }
    g.applied_valid = false;
    g.controls_sent.clear();
    LOGI("[session] ready from %s: medium %u, layout %u, %u CC routes", from, p.params.medium, p.params.pitch_layout, p.cc_count);
    apply_session();
}

std::string session_json_locked(const char* name) {
    sumi_preset_t p = g.sess;
    if (name) snprintf(p.name, sizeof p.name, "%s", name);
    const size_t need = sumi_preset_write(&p, sumi_version(), nullptr, 0);
    std::string out(need + 1, '\0');
    sumi_preset_write(&p, sumi_version(), &out[0], need + 1);
    out.resize(need);
    return out;
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
        // Step 45b (DECISIONS_5 #79): the session — the last one, a migration,
        // or the core's defaults — onto the fresh instance; its controls go out
        // as their routed CCs through the sole producer.
        session_attach();
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
    service_ledger();
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
        {
            voxo_config_t vc{};
            vc.sample_rate = 48000; vc.block_frames = 0; vc.max_voices = 16;   // block 0 = the table's 192 (DECISIONS_6 #4)
            vc.log_cb = [](int, const char* msg, void*) { LOGI("[voxo] %s", msg); };
            std::lock_guard<std::mutex> lk(g.push_mu);
            g.voxo = voxo_create(&vc);
            if (!g.voxo) LOGE("[voxo] create failed; the shell runs without sound");
        }
        g.render_thread = std::thread(render_loop);
        g.midi_running = true;
        g.midi_thread = std::thread(midi_poll_loop);
    }
}

// Phase 7 step 54 — Kotlin's Sound (the product side of Voxo on the Tab;
// DECISIONS_6 #28). Start/stop are the activity's (foreground only, under
// audio focus); the load blocks its (worker) caller; the stats line also
// paces the AAudio buffer tuner (#7).
JNIEXPORT jboolean JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeVoxoSetEnabled(JNIEnv*, jobject, jboolean on) {
    if (!g.voxo) return JNI_FALSE;
    if (on) { const bool ok = voxo_start(g.voxo); if (ok) { voxo_stats_t st; voxo_stats(g.voxo, &st); LOGI("[voxo] started: %u Hz, %u frames per burst, buffer %u", st.sample_rate, st.frames_per_burst, st.buffer_frames); } return ok ? JNI_TRUE : JNI_FALSE; }
    voxo_stop(g.voxo);
    LOGI("[voxo] stopped");
    return JNI_TRUE;
}
JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeVoxoSetGain(JNIEnv*, jobject, jfloat gain) { if (g.voxo) voxo_set_gain(g.voxo, gain); }
JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeVoxoSetLocalControl(JNIEnv*, jobject, jboolean on) {
    std::lock_guard<std::mutex> lk(g.push_mu);
    g.local_control = on;
    if (g.voxo) voxo_set_local_control(g.voxo, on);
}
JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeVoxoSetBudget(JNIEnv*, jobject, jlong bytes) { if (g.voxo) voxo_set_memory_budget(g.voxo, bytes > 0 ? (uint64_t)bytes : 0u); }
JNIEXPORT jstring JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeVoxoLoad(JNIEnv* env, jobject, jstring jpath) {
    if (!g.voxo) return env->NewStringUTF("ERR\nno sound core");
    const char* c = env->GetStringUTFChars(jpath, nullptr);
    std::string path = c ? c : "";
    env->ReleaseStringUTFChars(jpath, c);
    voxo_report_t rep{};
    const bool ok = voxo_load_preset(g.voxo, path.c_str(), &rep);
    std::string out = (ok ? "OK\n" : "ERR\n") + std::string(rep.text);
    return env->NewStringUTF(out.c_str());
}
JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeVoxoUnload(JNIEnv*, jobject) { if (g.voxo) voxo_unload_preset(g.voxo); }
JNIEXPORT jstring JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeVoxoStatus(JNIEnv* env, jobject) {
    if (!g.voxo) return env->NewStringUTF("");
    voxo_stats_t st; voxo_stats(g.voxo, &st);
    char buf[512];
    if (!voxo_running(g.voxo)) { std::snprintf(buf, sizeof(buf), "stopped|0"); }
    else std::snprintf(buf, sizeof(buf), "%u Hz, burst %u, buffer %u%s · %u voices (%u layers) · render %.2f ms (max %.2f) · %u underruns (%u late) in %u callbacks · %u dropped|1",
                       st.sample_rate, st.frames_per_burst, st.buffer_frames, st.low_latency ? ", low latency" : "",
                       st.active_voices, st.active_layers, (double)st.render_last_ms, (double)st.render_max_ms,
                       st.device_xruns, st.xruns, st.callbacks, st.dropped_midi);
    return env->NewStringUTF(buf);
}
JNIEXPORT jbyteArray JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeVoxoCoveredNotes(JNIEnv* env, jobject) {
    uint8_t mask[16];
    if (!g.voxo || !voxo_covered_notes(g.voxo, mask)) return nullptr;
    jbyteArray a = env->NewByteArray(16);
    env->SetByteArrayRegion(a, 0, 16, (const jbyte*)mask);
    return a;
}

// Phase 7 step 48 — the latency spike. Voxo on the platform's default output
// (miniaudio's AAudio path, low-latency profile); the numbers the roadmap asks
// for, on the one clock (shell::now_s == voxo_now_seconds, CLOCK_MONOTONIC):
//   push -> callback: 40 note-ons pushed from this thread through the ONE
//     producer, each paired with the start of the Voxo block that consumed it;
//   touch -> callback: every play-surface touch-down during the window (the
//     Kotlin mark at the touch callback, as the Phase-4 latency marks), paired
//     the same way — `adb shell input swipe` supplies them from the Mac;
//   the stream: the granted performance mode, the burst, the buffer, AAudio's
//     own underrun count, the timestamp-derived output latency, Voxo's XRun
//     proxy and render time — read at the end, after whatever load ran.
// Writes files/voxo_spike.csv and logs VOXO_SPIKE_DONE. Blocks its (worker)
// caller for `seconds`.
JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeVoxoSpike(JNIEnv*, jobject, jint seconds) {
    if (!g.voxo || g.spike_running.exchange(true)) return;
    const std::string path = g.files_dir + "/voxo_spike.csv";
    FILE* f = fopen(path.c_str(), "w");
    auto both = [&](const char* fmt, ...) {
        char buf[512];
        va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
        LOGI("[voxo spike] %s", buf);
        if (f) { fputs(buf, f); fputc('\n', f); fflush(f); }
    };
    voxo_set_input_mode(g.voxo, 1);
    voxo_set_gain(g.voxo, 0.8f);
    if (!voxo_start(g.voxo)) {
        both("# FAIL: no output device");
        if (f) fclose(f);
        g.spike_running = false;
        return;
    }
    voxo_stats_t st{};
    voxo_stats(g.voxo, &st);
    both("# voxo %u.%u.%u spike; device \"%s\"; %u Hz; burst %u frames; buffer %u frames; low_latency %u; output_latency_ms %.2f",
         voxo_version() >> 16, (voxo_version() >> 8) & 0xFF, voxo_version() & 0xFF, st.device, st.sample_rate,
         st.frames_per_burst, st.buffer_frames, st.low_latency, (double)st.output_latency_ms);
    both("kind,index,t_ref_s,t_callback_s,delta_ms");
    // Phase A: push -> callback, 40 note-ons a member channel each, 150 ms apart.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));   // the stream's warm-up
    uint32_t seen = st.note_ons;
    for (int i = 0; i < 40; i++) {
        const uint8_t note = (uint8_t)(48 + (i * 5) % 24);
        const double t_ref = voxo_now_seconds();
        shell::push_midi((uint8_t)(0x90 | (1 + i % 15)), note, 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        voxo_stats(g.voxo, &st);
        if (st.note_ons > seen) {
            both("push,%d,%.6f,%.6f,%.3f", i, t_ref, st.last_note_on_seconds, (st.last_note_on_seconds - t_ref) * 1000.0);
            seen = st.note_ons;
        } else {
            both("push,%d,%.6f,0,nan", i, t_ref);
        }
        shell::push_midi((uint8_t)(0x80 | (1 + i % 15)), note, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(90));
    }
    // Phase B: the touch window — pair every consumed note-on with the latest touch mark.
    const double t_end = shell::now_s() + (double)(seconds > 0 ? seconds : 30);
    int touches = 0;
    double last_mark_paired = 0.0;
    while (shell::now_s() < t_end) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        voxo_stats(g.voxo, &st);
        if (st.note_ons > seen) {
            seen = st.note_ons;
            const double mark = g.last_touch_down.load(std::memory_order_relaxed);
            if (mark > 0.0 && mark != last_mark_paired && st.last_note_on_seconds >= mark) {
                last_mark_paired = mark;
                both("touch,%d,%.6f,%.6f,%.3f", touches++, mark, st.last_note_on_seconds, (st.last_note_on_seconds - mark) * 1000.0);
            }
        }
    }
    voxo_stats(g.voxo, &st);
    both("# end: %u callbacks, %u voxo_xruns (proxy), %u device_xruns (AAudio), render max %.3f ms, burst %u, buffer %u, low_latency %u, output_latency_ms %.2f, dropped %u, note_ons %u, touches %d",
         st.callbacks, st.xruns, st.device_xruns, (double)st.render_max_ms, st.frames_per_burst, st.buffer_frames,
         st.low_latency, (double)st.output_latency_ms, st.dropped_midi, st.note_ons, touches);
    voxo_stop(g.voxo);
    if (f) fclose(f);
    LOGI("VOXO_SPIKE_DONE %s", path.c_str());
    g.spike_running = false;
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
    {   // step 48: the producers are stopped; the second ring goes after them
        voxo_t* v = nullptr;
        { std::lock_guard<std::mutex> lk(g.push_mu); v = g.voxo; g.voxo = nullptr; }
        if (v) voxo_destroy(v);
    }
    g.running = false;
    g.state_cv.notify_all();
    if (g.render_thread.joinable()) g.render_thread.join();
}

// -- touch / params (marshaled to the render thread) -------------------------

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeAddTine(JNIEnv*, jobject, jfloat x0, jfloat y0,
                                                       jfloat x1, jfloat y1, jfloat magnitude) {
    shell::post([=] { if (g.inst) sumi_add_tine(g.inst, x0, y0, x1, y1, 0.035f, magnitude); });
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
Java_com_vibetuned_midisink_NativeBridge_nativeTriggerDip(JNIEnv*, jobject) {
    shell::post([] { if (g.inst) sumi_trigger_paper_dip(g.inst); });
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSetSimScale(JNIEnv*, jobject, jfloat sc, jint) {
    { std::lock_guard<std::mutex> lk(g.params_mu); g.host_sim_scale = sc; }
    shell::post([] { apply_session(); });
}

JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSetThermal(JNIEnv*, jobject, jint status) {
    g.thermal = status;
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

// The engine's own version (the ABI, DECISIONS_4 #3 — a different number
// from the app version by design), for the About line beside it.
JNIEXPORT jint JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeCoreVersion(JNIEnv*, jobject) {
    return (jint)sumi_version();
}

JNIEXPORT jint JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeDroppedMidi(JNIEnv*, jobject) {
    // Under the producer mutex: g.inst is destroyed under it too (contract 2
    // otherwise puts this call on the wrong thread).
    std::lock_guard<std::mutex> lk(g.push_mu);
    return g.inst ? (jint)sumi_dropped_midi_count(g.inst) : -1;
}


// ---- step 45b (DECISIONS_5 #79): THE SESSION -------------------------------------
static std::string jstr(JNIEnv* env, jstring js) {
    if (!js) return std::string();
    const char* c = env->GetStringUTFChars(js, nullptr);
    std::string out = c ? c : "";
    if (c) env->ReleaseStringUTFChars(js, c);
    return out;
}

// Before the instance exists: the last session's text (or empty) and, when
// there is none, a patch Kotlin built from the 0.x SharedPreferences rows.
extern "C" JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSessionInit(JNIEnv* env, jobject, jstring session, jstring legacy) {
    std::lock_guard<std::mutex> lk(g.params_mu);
    g.sess_text = jstr(env, session);
    g.sess_legacy = jstr(env, legacy);
}

// The session as the serializer writes it; "" until the instance has seeded it.
extern "C" JNIEXPORT jstring JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSessionJson(JNIEnv* env, jobject, jstring name) {
    std::string out;
    {
        std::lock_guard<std::mutex> lk(g.params_mu);
        if (g.sess_ready) {
            const std::string n = jstr(env, name);
            out = session_json_locked(name ? n.c_str() : nullptr);
        }
    }
    return env->NewStringUTF(out.c_str());
}

// A JSON patch read OVER the session (keys present overwrite, missing keep —
// a whole preset file is a patch too). Returns the session after it, or ""
// when the text is not a JSON object or the session is not ready.
extern "C" JNIEXPORT jstring JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeSessionPatch(JNIEnv* env, jobject, jstring patch) {
    const std::string t = jstr(env, patch);
    std::string out;
    {
        std::lock_guard<std::mutex> lk(g.params_mu);
        if (g.sess_ready && sumi_preset_read(t.c_str(), 0, &g.sess)) out = session_json_locked(nullptr);
    }
    if (!out.empty()) shell::post([] { apply_session(); });
    return env->NewStringUTF(out.c_str());
}

// The palette library: entry `index` of `medium` as a preset whose name is the
// palette's name and whose palette is it (Kotlin reads the two keys); "" past the end.
extern "C" JNIEXPORT jstring JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativePalettePreset(JNIEnv* env, jobject, jint medium, jint index) {
    sumi_palette_t pal; const char* nm = nullptr;
    if (index < 0 || !sumi_palette_preset((uint32_t)medium, (uint32_t)index, &pal, &nm)) return env->NewStringUTF("");
    static sumi_preset_t p;
    sumi_preset_init(&p, nullptr, &pal);
    snprintf(p.name, sizeof p.name, "%s", nm ? nm : "");
    const size_t need = sumi_preset_write(&p, sumi_version(), nullptr, 0);
    std::string out(need + 1, '\0');
    sumi_preset_write(&p, sumi_version(), &out[0], need + 1);
    out.resize(need);
    return env->NewStringUTF(out.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativePalettePresetCount(JNIEnv*, jobject, jint medium) {
    return (jint)sumi_palette_preset_count((uint32_t)medium);
}

// ---- step 45b: THE MARBLE GESTURES THROUGH THE CORE (#75) --------------------------
static constexpr float kDropRadius = 0.06f, kVortexRadius = 0.18f;

extern "C" JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeGestureTap(JNIEnv*, jobject, jfloat x, jfloat y) {
    shell::post([=] { if (g.inst) sumi_gesture_tap(g.inst, x, y, kDropRadius); });
}
// span = the finger distance in canvas heights (a pen passes 2 × the vortex radius)
extern "C" JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeGesturePinch(JNIEnv*, jobject, jfloat x, jfloat y, jfloat k,
                                                             jfloat angle, jfloat span) {
    shell::post([=] { if (g.inst) sumi_gesture_pinch(g.inst, x, y, k, angle, span > 0.0f ? span : 2.0f * kVortexRadius); });
}
extern "C" JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeGestureTwist(JNIEnv*, jobject, jfloat x, jfloat y, jfloat strength) {
    const uint32_t profile = shell::params_snapshot().vortex_profile;   // the settings' profile, as the desktop's right drag
    shell::post([=] { if (g.inst) sumi_gesture_twist(g.inst, x, y, strength, kVortexRadius, profile); });
}
// The long press: its first touch is a tap; then one frame per vsync with the
// push (up) and pull (down) 0..1; the core tracks the boundary radius R.
extern "C" JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeGesturePressBegin(JNIEnv*, jobject, jfloat x, jfloat y) {
    shell::post([=] {
        if (!g.inst) return;
        sumi_gesture_tap(g.inst, x, y, kDropRadius);
        g.press_active = true; g.press_x = x; g.press_y = y; g.press_R = kDropRadius;
    });
}
extern "C" JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeGesturePressFrame(JNIEnv*, jobject, jfloat up, jfloat down, jfloat dt) {
    shell::post([=] {
        if (!g.inst || !g.press_active) return;
        g.press_R = sumi_gesture_press(g.inst, g.press_x, g.press_y, g.press_R, up, down, (double)dt);
    });
}
extern "C" JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeGesturePressEnd(JNIEnv*, jobject) {
    shell::post([] {
        if (!g.inst || !g.press_active) return;
        sumi_gesture_press_end(g.inst);   // lets go of a stir the press set
        g.press_active = false;
    });
}

// ---- step 45b: THE PRINT LEDGER ----------------------------------------------------
// keep = "Dip the paper — keep the print": the field, the look, then the dip;
// otherwise "Clear the canvas — discard": the dip, its print dropped on arrival.
extern "C" JNIEXPORT void JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeLedgerDip(JNIEnv*, jobject, jboolean keep) {
    shell::post([keep] {
        if (!g.inst) return;
        if (keep == JNI_TRUE) {
            Shell::LedgerEntry e;
            uint32_t w = 0, h = 0;
            if (sumi_read_field(g.inst, nullptr, 0, &w, &h) && w && h) {
                e.field.resize((size_t)w * h * 8);
                if (sumi_read_field(g.inst, e.field.data(), e.field.size(), &w, &h)) {
                    e.fw = w; e.fh = h;
                    sumi_get_params(g.inst, &e.params);
                    sumi_get_palette(g.inst, &e.palette);
                    e.when = (int64_t)time(nullptr);
                    std::lock_guard<std::mutex> lk(g.ledger_mu);
                    e.id = g.ledger_next_id++;
                    g.ledger.push_back(std::move(e));
                    ledger_evict_locked();
                    g.ledger_status = "Dipped: the sheet is kept in the ledger";
                }
            }
            g.print_expect = 1;
        } else {
            g.print_expect = 2;
            std::lock_guard<std::mutex> lk(g.ledger_mu);
            g.ledger_status = "Cleared: a fresh sheet, nothing kept";
        }
        g.print_frames = 0;
        sumi_trigger_paper_dip(g.inst);
    });
}

// [count, then per entry: id, fw, fh, medium, printSeen, tw, th, when(low 31 bits), pw, ph] newest last.
extern "C" JNIEXPORT jintArray JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeLedgerList(JNIEnv* env, jobject) {
    std::vector<jint> v;
    {
        std::lock_guard<std::mutex> lk(g.ledger_mu);
        v.push_back((jint)g.ledger.size());
        for (const auto& e : g.ledger) {
            v.push_back((jint)e.id); v.push_back((jint)e.fw); v.push_back((jint)e.fh); v.push_back((jint)e.params.medium);
            v.push_back(e.print_seen ? 1 : 0); v.push_back((jint)e.tw); v.push_back((jint)e.th); v.push_back((jint)(e.when & 0x7FFFFFFF)); v.push_back((jint)e.pw); v.push_back((jint)e.ph);
        }
    }
    jintArray a = env->NewIntArray((jsize)v.size());
    if (a) env->SetIntArrayRegion(a, 0, (jsize)v.size(), v.data());
    return a;
}

// RGBA8 bytes of an entry's thumbnail (which = 0) or of the newest print (which = 1); null if none.
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeLedgerPixels(JNIEnv* env, jobject, jint id, jint which) {
    std::lock_guard<std::mutex> lk(g.ledger_mu);
    for (const auto& e : g.ledger) {
        if ((jint)e.id != id) continue;
        const std::vector<uint8_t>& px = which == 1 ? e.print : e.thumb;
        if (px.empty()) return nullptr;
        jbyteArray a = env->NewByteArray((jsize)px.size());
        if (a) env->SetByteArrayRegion(a, 0, (jsize)px.size(), (const jbyte*)px.data());
        return a;
    }
    return nullptr;
}

// Re-export entry `id` at w × h: the entry's look round the render (begin renders),
// the look as it stands after. alpha = Anod over alpha. False while one is in flight.
extern "C" JNIEXPORT jboolean JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeLedgerExport(JNIEnv*, jobject, jint id, jint w, jint h, jboolean alpha) {
    if (w <= 0 || h <= 0) return JNI_FALSE;
    {
        std::lock_guard<std::mutex> lk(g.ledger_mu);
        g.export_ready = false; g.export_px.clear(); g.export_px.shrink_to_fit();
        g.ledger_status = "Exporting…";
    }
    shell::post([=] {
        if (!g.inst || g.export_pending) return;
        const Shell::LedgerEntry* e = nullptr;
        std::unique_lock<std::mutex> lk(g.ledger_mu);
        for (const auto& x : g.ledger) if ((jint)x.id == id) e = &x;
        if (!e) { g.ledger_status = "Export failed: the entry left the ledger"; LOGI("[print] export: entry %d not in the ledger", (int)id); return; }
        sumi_params_t cur; sumi_palette_t curp;
        sumi_get_params(g.inst, &cur); sumi_get_palette(g.inst, &curp);
        sumi_set_params(g.inst, &e->params); sumi_set_palette(g.inst, &e->palette);
        const uint32_t flags = (alpha == JNI_TRUE && e->params.medium == SUMI_MEDIUM_ANOD) ? SUMI_EXPORT_ANOD_ALPHA : 0u;
        const bool ok = sumi_export_begin(g.inst, e->field.data(), e->fw, e->fh, (uint32_t)w, (uint32_t)h, flags);
        sumi_set_params(g.inst, &cur); sumi_set_palette(g.inst, &curp);
        if (!ok) { g.ledger_status = "Export could not start (a readback is in flight, or the size is out of range)"; LOGI("[print] export %dx%d could not start", (int)w, (int)h); return; }
        LOGI("[print] export %dx%d started%s", (int)w, (int)h, flags ? " (Anod over alpha)" : "");
        g.export_pending = true; g.export_w = (uint32_t)w; g.export_h = (uint32_t)h;
    });
    return JNI_TRUE;
}

// The finished export as [w, h] + RGBA8 via the two calls; null while none is ready.
extern "C" JNIEXPORT jintArray JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeLedgerExportSize(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lk(g.ledger_mu);
    if (!g.export_ready) return nullptr;
    jintArray a = env->NewIntArray(2);
    const jint wh[2] = {(jint)g.export_rw, (jint)g.export_rh};
    if (a) env->SetIntArrayRegion(a, 0, 2, wh);
    return a;
}
// Copies the finished export into `dst` (a direct ByteBuffer of w*h*4) and frees it.
extern "C" JNIEXPORT jboolean JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeLedgerExportTake(JNIEnv* env, jobject, jobject dst) {
    std::lock_guard<std::mutex> lk(g.ledger_mu);
    if (!g.export_ready || !dst) return JNI_FALSE;
    void* p = env->GetDirectBufferAddress(dst);
    const jlong cap = env->GetDirectBufferCapacity(dst);
    if (!p || cap < (jlong)g.export_px.size()) return JNI_FALSE;
    memcpy(p, g.export_px.data(), g.export_px.size());
    g.export_ready = false; g.export_px.clear(); g.export_px.shrink_to_fit();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_vibetuned_midisink_NativeBridge_nativeLedgerStatus(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lk(g.ledger_mu);
    return env->NewStringUTF(g.ledger_status.c_str());
}
} // extern "C"
