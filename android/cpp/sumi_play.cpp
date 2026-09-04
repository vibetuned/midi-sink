// sumi_play.cpp — the Phase-4 host half of the Android shell (PHASE4_SPEC
// §3–§8): hostmpe (allocator, joystick, pen legato, strip engine) plus one
// rate limiter PER outbound transport, all living on the AMidi poller thread
// — the §5.2 single producer (DECISIONS_2 #33). The UI thread never touches
// hostmpe state and never calls sumi_push_midi: every touch/pen/strip event
// is marshalled here through a command queue (the iOS serial midiQueue's
// sibling, DECISIONS_3 #14). Touch-down is a SYNC hop — the overlay needs the
// voice id before it can track the touch (microseconds).
//
// Outbound wire writes go UP to Kotlin (NativeBridge.outboundWrite), which
// owns the MidiInputPort of the USB gadget port, the MidiDeviceService
// receiver (virtual device) and the BLE-MIDI GATT server. One generator, N
// sinks (§5.4), each behind its own hostmpe limiter (DECISIONS_3 #3): USB and
// the virtual device at ≤ 100 Hz per voice-dimension, BLE under a ~300 msg/s
// budget. The loopback never passes through a limiter.
#include "shell.h"

#include "hostmpe.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <unistd.h>
#include <unordered_set>
#include <vector>

// The headless suites (tests/hostmpe_tests.cpp, tests/normalizer_tests.cpp)
// are compiled into this library with their main() renamed — the Android
// build runs the FULL hostmpe suite on-device (Step 22 DONE), through a
// debug Intent, with stdout/stderr captured to a file.
int hostmpe_tests_main();
int normalizer_tests_main();

namespace {

using shell::now_s;

enum Sink { SINK_USB = 0, SINK_VIRTUAL = 1, SINK_BLE = 2, SINK_COUNT = 3 };
// Byte-log source tags (iOS midi_log.csv convention): 0 external device,
// 1 touch/pen, 2 session config, 3 control strip.
// Android adds 4 = stylus, so the byte-log asserts can tell finger voices
// (never CC74) from pen voices (CC74 by right) without guessing.
enum Src : uint8_t { SRC_EXT = 0, SRC_TOUCH = 1, SRC_CFG = 2, SRC_STRIP = 3, SRC_PEN = 4 };

struct LogEntry { double t; uint8_t s, d1, d2, src; };

struct Play {
    // -- command queue (UI -> MIDI thread) ----------------------------------
    std::mutex mu;
    std::condition_variable cv;
    std::deque<std::function<void()>> cmds;

    // -- engines: MIDI thread only ------------------------------------------
    hostmpe_t* mpe = nullptr;
    hostmpe_strip_t* strip = nullptr;
    hostmpe_limiter_t* lim[SINK_COUNT] = {nullptr, nullptr, nullptr};
    bool sink_on[SINK_COUNT] = {true, true, false};
    std::atomic<long> sent[SINK_COUNT] = {};   // MIDI thread writes, UI thread reads
    bool play_effective = false;
    double last_auto_resync = -1e9;
    double last_tick = 0.0;

    // -- byte log at the merge point (Step 16/18/22 evidence) --------------
    std::vector<LogEntry> log;
    static constexpr size_t LOG_CAP = 300000;

    // -- latency marks: touch-down (UI) -> pushed (MIDI) -> rendered ---------
    std::mutex lat_mu;
    std::vector<std::pair<double, double>> marks;     // (t_down, t_pushed)
    std::vector<std::pair<double, double>> samples;   // (to_render_ms, to_push_ms)
    std::atomic<double> last_latency_ms{-1.0};

    // -- storm test (Step 17 BLE saturation / Step 18 CC64 rider) ------------
    bool storm = false;
    double storm_t0 = 0, storm_seconds = 0, storm_next = 0;
    long storm_tick = 0;
    int32_t storm_voices[10];

    // Loopback queue-overflow counter, sampled on the render thread (the
    // DONE gate's "zero dropped loopback messages"); written into the log.
    std::atomic<uint32_t> dropped_at_flush{0};

    // -- JNI upcall to Kotlin: NativeBridge.outboundWrite(sink, bytes, len) --
    JavaVM* vm = nullptr;
    jclass bridge = nullptr;
    jmethodID out_mid = nullptr;
    JNIEnv* env = nullptr;   // the MIDI thread's env (attached for its lifetime)
    std::vector<uint8_t> wire_buf;
    std::atomic<bool> engines_ready{false};
};

Play P;

// ---- command queue -----------------------------------------------------------

void play_post(std::function<void()> fn) {
    {
        std::lock_guard<std::mutex> lk(P.mu);
        P.cmds.push_back(std::move(fn));
    }
    P.cv.notify_all();
}

// Sync hop: the caller (the UI thread — touch-down needs its voice id before
// the overlay can track the touch) waits until the MIDI thread ran fn.
//
// BOUNDED, deliberately: this blocks the UI thread, and the one thing worse
// than a dropped note is an ANR. The queue is drained every poll iteration
// (~1 ms) and each of these bodies is a handful of microseconds, so the
// timeout is four orders of magnitude of headroom — it can only fire if the
// MIDI thread is gone or wedged, and then the caller must proceed without an
// answer rather than hang the app. `fn` captures by reference at every call
// site, so a timed-out call leaves a lambda holding dangling references: the
// shared flag tells the queued lambda to do nothing if it ever runs.
struct SyncCall {
    std::mutex mu;
    std::condition_variable cv;
    bool done = false;
    bool abandoned = false;
};

bool play_post_sync(const std::function<void()>& fn, int timeout_ms = 250) {
    auto call = std::make_shared<SyncCall>();
    play_post([call, &fn] {
        {
            std::lock_guard<std::mutex> lk(call->mu);
            if (call->abandoned) return;   // the caller gave up; fn's captures are dead
        }
        fn();
        std::lock_guard<std::mutex> lk(call->mu);
        call->done = true;
        call->cv.notify_all();
    });
    std::unique_lock<std::mutex> lk(call->mu);
    if (call->cv.wait_for(lk, std::chrono::milliseconds(timeout_ms),
                          [&] { return call->done; })) {
        return true;
    }
    call->abandoned = true;
    LOGW("play_post_sync timed out after %d ms — MIDI thread wedged or gone", timeout_ms);
    return false;
}

// ---- merge point: log, loopback, outbound (MIDI thread only) -----------------

void log_byte(double t, uint8_t s, uint8_t d1, uint8_t d2, uint8_t src) {
    if (P.log.size() < Play::LOG_CAP) P.log.push_back({t, s, d1, d2, src});
}

// Raw MIDI 1.0 bytes for one message: 0xC0/0xD0 are TWO-byte messages — a
// third byte on the wire would be parsed as running-status data (a stray
// pressure 0 after every pressure message).
size_t wire_bytes(const hostmpe_msg_t& m, uint8_t* out) {
    out[0] = m.status;
    out[1] = m.data1;
    const uint8_t kind = m.status & 0xF0;
    if (kind == 0xC0 || kind == 0xD0) return 2;
    out[2] = m.data2;
    return 3;
}

// Hand a batch to Kotlin for one sink. Returns true when the sink had a live
// endpoint (counted as "sent").
void wire(int sink, const hostmpe_msg_t* m, uint32_t n) {
    if (n == 0 || !P.env || !P.bridge || !P.out_mid) return;
    P.wire_buf.clear();
    uint8_t tmp[3];
    for (uint32_t i = 0; i < n; i++) {
        const size_t k = wire_bytes(m[i], tmp);
        P.wire_buf.insert(P.wire_buf.end(), tmp, tmp + k);
    }
    jbyteArray arr = P.env->NewByteArray((jsize)P.wire_buf.size());
    if (!arr) { P.env->ExceptionClear(); return; }
    P.env->SetByteArrayRegion(arr, 0, (jsize)P.wire_buf.size(), (const jbyte*)P.wire_buf.data());
    const jboolean delivered = P.env->CallStaticBooleanMethod(P.bridge, P.out_mid, (jint)sink,
                                                              arr, (jint)P.wire_buf.size());
    if (P.env->ExceptionCheck()) { P.env->ExceptionDescribe(); P.env->ExceptionClear(); }
    P.env->DeleteLocalRef(arr);
    if (delivered) P.sent[sink] += (long)n;   // atomic
}

// Fan one generated message out to every enabled transport through its own
// limiter. `exempt` per §5.3: Note On/Off, initial center bend,
// pressure-0-before-Note-Off, session config, strip buttons.
void outbound(const hostmpe_msg_t& m, bool exempt, double now) {
    hostmpe_msg_t out[64];
    for (int s = 0; s < SINK_COUNT; s++) {
        if (!P.sink_on[s] || !P.lim[s]) continue;
        const uint32_t n = hostmpe_limiter_push(P.lim[s], now, m, exempt, out, 64);
        wire(s, out, n);
    }
}

// Both pipes: byte log, loopback (full rate), outbound (policed unless exempt).
void dispatch(const hostmpe_msg_t* m, uint32_t n, uint8_t src, bool exempt,
              bool loopback, double now) {
    for (uint32_t i = 0; i < n; i++) {
        log_byte(now, m[i].status, m[i].data1, m[i].data2, src);
        if (loopback) shell::push_midi(m[i].status, m[i].data1, m[i].data2);
        outbound(m[i], exempt, now);
    }
}

// §5.3 / working rule: MCM (RPN 6, lower zone, 15 members) + RPN 0 = 48 on
// the members into the LOOPBACK first (deterministic normalizer mode flip)
// and out every sink, then the strip re-announces its latched values (§8 —
// exempt: an announce repeats values by definition).
void send_session_config(double now) {
    if (!P.mpe) return;
    hostmpe_msg_t cfg[128];
    const uint32_t n = hostmpe_session_config(P.mpe, cfg, 128);
    dispatch(cfg, n, SRC_CFG, true, true, now);
    if (P.strip) {
        hostmpe_msg_t am[8];
        const uint32_t an = hostmpe_strip_announce(P.strip, am, 8);
        dispatch(am, an, SRC_STRIP, true, true, now);
    }
    LOGI("[cfg] session config sent (%u msgs) usb=%d virt=%d ble=%d",
         n, P.sink_on[0], P.sink_on[1], P.sink_on[2]);
}

// A sink appearing mid-session (USB mode flipped to MIDI, a DAW opening the
// virtual device, a BLE central subscribing) re-sends the handshake it
// missed — debounced to one per 2 s, Play mode only (DECISIONS_3 #28).
void auto_resync(double now) {
    if (!P.play_effective) return;
    if (now - P.last_auto_resync < 2.0) return;
    P.last_auto_resync = now;
    send_session_config(now);
}

void strip_dispatch(const hostmpe_msg_t* m, uint32_t n, bool exempt, double now) {
    dispatch(m, n, SRC_STRIP, exempt, true, now);
}

// ---- storm test (port of the iOS startStormTest, MIDI thread) ---------------
// 10 synthetic voices through the FULL pipeline (loopback + outbound
// limiters) at 125 Hz. A 1 Hz exempt marker (CC 118, master) rides along so a
// receiver can measure cumulative lag without clock sync, and a CC64
// transition every 2 s (exempt, §8 never-dropped class) lets the capture
// assert that sustain never sticks under the storm.

void storm_end_voice(int i, double now) {
    if (P.storm_voices[i] < 0) return;
    hostmpe_msg_t m[4];
    const uint32_t n = hostmpe_touch_end(P.mpe, P.storm_voices[i], now, 64, m, 4);
    dispatch(m, n, SRC_TOUCH, true, true, now);
    P.storm_voices[i] = -1;
}

void storm_tick(double now) {
    const double t = now - P.storm_t0;
    if (t >= P.storm_seconds) {
        for (int i = 0; i < 10; i++) storm_end_voice(i, now);
        P.storm = false;
        const long su = P.sent[0].load(), sv = P.sent[1].load(), sb = P.sent[2].load();
        LOGI("[storm] done: sent usb=%ld virtual=%ld ble=%ld over %.1fs (ble %.0f/s)",
             su, sv, sb, t, (double)sb / (t > 1 ? t : 1));
        return;
    }
    for (int i = 0; i < 10; i++) {
        const double phase = t + (double)i * 0.6;
        if (P.storm_voices[i] < 0 || (P.storm_tick % 750 == i * 75)) {
            storm_end_voice(i, now);
            hostmpe_msg_t m[4];
            uint32_t n = 0;
            P.storm_voices[i] = hostmpe_touch_begin(P.mpe, now, (uint8_t)(48 + i * 3), 96,
                                                    0.0571f, 1.0f / 0.1244f, 0.0f, m, 4, &n);
            dispatch(m, n, SRC_TOUCH, true, true, now);
        }
        if (P.storm_voices[i] < 0) continue;
        const float dx = (float)(0.12 * sin(phase * 2.1));
        const float dy = (float)(-0.05 * (0.5 + 0.5 * sin(phase * 3.3)));
        hostmpe_msg_t m[4];
        const uint32_t n = hostmpe_touch_update(P.mpe, P.storm_voices[i], dx, dy, m, 4);
        dispatch(m, n, SRC_TOUCH, false, true, now);
    }
    if (P.storm_tick % 125 == 0) {
        hostmpe_msg_t mk;
        mk.status = 0xB0; mk.data1 = 118; mk.data2 = (uint8_t)((P.storm_tick / 125) % 128);
        outbound(mk, true, now);
    }
    if (P.storm_tick % 250 == 125) {
        hostmpe_msg_t su;
        su.status = 0xB0; su.data1 = 64; su.data2 = (P.storm_tick / 250) % 2 == 0 ? 127 : 0;
        log_byte(now, su.status, su.data1, su.data2, SRC_STRIP);
        outbound(su, true, now);
    }
    P.storm_tick++;
}

// ---- logs --------------------------------------------------------------------

void flush_logs_now() {   // MIDI thread
    const std::string& dir = shell::files_dir();
    if (dir.empty()) return;
    if (!P.log.empty()) {
        FILE* f = fopen((dir + "/midi_log.csv").c_str(), "w");
        if (f) {
            fputs("t,status,d1,d2,src\n", f);
            for (const LogEntry& e : P.log) {
                fprintf(f, "%.4f,%d,%d,%d,%d\n", e.t, e.s, e.d1, e.d2, e.src);
            }
            // The log is capped, not a ring: a long session stops recording
            // partway and its tail (the releases) never lands. Say so in the
            // file, so the analyzer reports the balance instead of asserting
            // it (the wire capture is the complete record).
            if (P.log.size() >= Play::LOG_CAP) {
                fprintf(f, "# truncated: byte log hit the %zu-entry cap\n", Play::LOG_CAP);
            }
            fprintf(f, "# dropped_loopback_messages: %u\n", P.dropped_at_flush.load());
            fclose(f);
        }
    }
    std::vector<std::pair<double, double>> samples;
    {
        std::lock_guard<std::mutex> lk(P.lat_mu);
        samples = P.samples;
    }
    if (!samples.empty()) {
        FILE* f = fopen((dir + "/latency_log.csv").c_str(), "w");
        if (f) {
            fputs("touch_to_render_ms,touch_to_push_ms\n", f);
            for (const auto& s : samples) fprintf(f, "%.2f,%.2f\n", s.first, s.second);
            fclose(f);
        }
    }
    LOGI("[log] flushed %zu bytes-log entries, %zu latency samples", P.log.size(), samples.size());
}

} // namespace

// ---- shell.h play half -------------------------------------------------------

namespace shell {

void play_init(JNIEnv* env) {
    // launchMode="singleTask": the process outlives a back-out, so a relaunch
    // calls play_init again on state the previous session left behind. Stale
    // play_effective is the sharp edge — nativeSetPlayMode(true) would
    // early-return and the transports would never get their MCM/RPN0.
    P.play_effective = false;
    P.last_auto_resync = -1e9;
    P.last_tick = 0.0;
    P.storm = false;
    P.log.clear();
    for (int i = 0; i < SINK_COUNT; i++) P.sent[i] = 0;
    {
        std::lock_guard<std::mutex> lk(P.lat_mu);
        P.marks.clear();
        P.samples.clear();
    }
    P.last_latency_ms = -1.0;
    env->GetJavaVM(&P.vm);
    jclass local = env->FindClass("com/vibetuned/midisink/NativeBridge");
    if (!local) {
        LOGE("NativeBridge class not found — outbound transports disabled");
        env->ExceptionClear();
        return;
    }
    P.bridge = (jclass)env->NewGlobalRef(local);
    env->DeleteLocalRef(local);
    P.out_mid = env->GetStaticMethodID(P.bridge, "outboundWrite", "(I[BI)Z");
    if (!P.out_mid) {
        LOGE("NativeBridge.outboundWrite not found — outbound transports disabled");
        env->ExceptionClear();
    }
}

void play_shutdown() {
    // The MIDI thread has joined: the engines were destroyed in
    // play_thread_exit; only the JNI global ref remains.
    if (P.vm && P.bridge) {
        JNIEnv* env = nullptr;
        if (P.vm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_OK && env) {
            env->DeleteGlobalRef(P.bridge);
        }
        P.bridge = nullptr;
    }
}

void play_thread_enter() {
    if (P.vm) {
        JNIEnv* env = nullptr;
        if (P.vm->AttachCurrentThread(&env, nullptr) == JNI_OK) P.env = env;
        else LOGE("AttachCurrentThread failed on the MIDI thread");
    }
    P.mpe = hostmpe_create();
    P.strip = hostmpe_strip_create();
    P.lim[SINK_USB] = hostmpe_limiter_create_rate(100.0f);
    P.lim[SINK_VIRTUAL] = hostmpe_limiter_create_rate(100.0f);
    P.lim[SINK_BLE] = hostmpe_limiter_create_budget(300.0f);
    for (int i = 0; i < 10; i++) P.storm_voices[i] = -1;
    P.engines_ready = true;
    LOGI("MIDI thread up: hostmpe + strip + 3 limiters (usb/virtual ≤100 Hz, ble 300/s)");
}

void play_thread_exit() {
    P.engines_ready = false;
    // Flush BEFORE the panic (so a sink that stops draining cannot cost us
    // the whole log) and again after (so the panic's own messages are in it).
    flush_logs_now();
    if (P.mpe) {
        // Teardown never leaves a sink holding notes: panic through every pipe.
        hostmpe_msg_t m[128];
        const uint32_t n = hostmpe_panic(P.mpe, now_s(), m, 128);
        dispatch(m, n, SRC_TOUCH, true, false, now_s());
        flush_logs_now();
    }
    for (int s = 0; s < SINK_COUNT; s++) {
        if (P.lim[s]) hostmpe_limiter_destroy(P.lim[s]);
        P.lim[s] = nullptr;
    }
    if (P.strip) hostmpe_strip_destroy(P.strip);
    P.strip = nullptr;
    if (P.mpe) hostmpe_destroy(P.mpe);
    P.mpe = nullptr;
    if (P.vm && P.env) P.vm->DetachCurrentThread();
    P.env = nullptr;
    LOGI("MIDI thread down");
}

void play_drain(double now) {
    // 1. commands posted by the UI thread (touches, strip, transports…)
    for (;;) {
        std::function<void()> fn;
        {
            std::lock_guard<std::mutex> lk(P.mu);
            if (P.cmds.empty()) break;
            fn = std::move(P.cmds.front());
            P.cmds.pop_front();
        }
        fn();
    }
    // 2. limiter drains + strip spring ramp, every ~4 ms (each frame is
    //    plenty per the header contract; 4 ms keeps the ≤100 Hz slots fed).
    if (now - P.last_tick >= 0.004) {
        P.last_tick = now;
        hostmpe_msg_t out[64];
        for (int s = 0; s < SINK_COUNT; s++) {
            if (!P.sink_on[s] || !P.lim[s]) continue;
            wire(s, out, hostmpe_limiter_drain(P.lim[s], now, out, 64));
        }
        if (P.strip) {
            hostmpe_msg_t m[2];
            const uint32_t n = hostmpe_strip_tick(P.strip, now, m, 2);
            strip_dispatch(m, n, false, now);   // wheel: policed
        }
    }
    // 3. storm state machine at 8 ms cadence.
    if (P.storm && P.mpe) {
        while (P.storm && now >= P.storm_next) {
            storm_tick(P.storm_next);
            P.storm_next += 0.008;
        }
    }
}

void play_wait(int max_ms) {
    std::unique_lock<std::mutex> lk(P.mu);
    if (max_ms <= 0) { lk.unlock(); P.cv.notify_all(); return; }
    P.cv.wait_for(lk, std::chrono::milliseconds(max_ms), [] { return !P.cmds.empty(); });
}

// Device bytes at the merge point (MIDI thread): feed the external-occupancy
// mask (§5.1 — a ROLI chord must never be channel-stolen), log, loopback.
void play_ingest_external(double now, uint8_t status, uint8_t d1, uint8_t d2) {
    if (P.mpe) hostmpe_observe_external(P.mpe, now, status, d1, d2);
    log_byte(now, status, d1, d2, SRC_EXT);
    push_midi(status, d1, d2);
}

// The core instance came up (surface attached for the first time). If Play
// mode was already effective, the MCM/RPN0 pushed at mode entry never reached
// the loopback (push_midi drops without an instance) — send it again so the
// normalizer's mode flip stays deterministic on a cold start.
void play_instance_ready() {
    play_post([] {
        if (P.play_effective) {
            LOGI("[cfg] instance ready with Play effective -> loopback handshake");
            send_session_config(now_s());
        }
    });
}

void play_frame_rendered(double now) {
    P.dropped_at_flush = shell::stats().dropped;
    std::lock_guard<std::mutex> lk(P.lat_mu);
    if (P.marks.empty()) return;
    for (const auto& m : P.marks) {
        const double to_render = (now - m.first) * 1000.0;
        P.samples.emplace_back(to_render, (m.second - m.first) * 1000.0);
        P.last_latency_ms = to_render;
    }
    P.marks.clear();
}

} // namespace shell

// ---- JNI entry points --------------------------------------------------------

extern "C" {

#define NB(name) Java_com_vibetuned_midisink_NativeBridge_##name

// Play mode effective (playable layout AND the toggle): pushes MCM/RPN0 into
// the loopback and every sink FIRST (working rule), then the strip announce.
JNIEXPORT void JNICALL NB(nativeSetPlayMode)(JNIEnv*, jobject, jboolean effective) {
    play_post([=] {
        const bool on = effective == JNI_TRUE;
        if (on == P.play_effective) return;
        P.play_effective = on;
        LOGI("[mode] play effective=%d", on ? 1 : 0);
        if (on) send_session_config(now_s());
    });
}

// -- fingers (§3, §4 Android finger rows) -------------------------------------

JNIEXPORT jint JNICALL NB(nativeTouchBegin)(JNIEnv*, jobject, jdouble t_down, jint note,
                                            jint velocity, jfloat r_max, jfloat grad_x,
                                            jfloat grad_y) {
    if (!P.engines_ready.load()) return -1;
    int32_t voice = -1;
    play_post_sync([&] {
        if (!P.mpe) return;
        hostmpe_msg_t m[4];
        uint32_t n = 0;
        const double now = now_s();
        voice = hostmpe_touch_begin(P.mpe, now, (uint8_t)note, (uint8_t)velocity,
                                    r_max, grad_x, grad_y, m, 4, &n);
        dispatch(m, n, SRC_TOUCH, true, true, now);   // strike: never decimated
        if (voice >= 0) {
            std::lock_guard<std::mutex> lk(P.lat_mu);
            P.marks.emplace_back((double)t_down, now);
        }
    });
    return voice;
}

JNIEXPORT void JNICALL NB(nativeTouchUpdate)(JNIEnv*, jobject, jint voice, jfloat dx, jfloat dy) {
    play_post([=] {
        if (!P.mpe) return;
        hostmpe_msg_t m[4];
        const uint32_t n = hostmpe_touch_update(P.mpe, voice, dx, dy, m, 4);
        dispatch(m, n, SRC_TOUCH, false, true, now_s());   // continuous: policed
    });
}

JNIEXPORT void JNICALL NB(nativeTouchEnd)(JNIEnv*, jobject, jint voice, jint lift) {
    play_post([=] {
        if (!P.mpe) return;
        hostmpe_msg_t m[4];
        const double now = now_s();
        const uint32_t n = hostmpe_touch_end(P.mpe, voice, now, (uint8_t)lift, m, 4);
        dispatch(m, n, SRC_TOUCH, true, true, now);   // lift: never decimated
    });
}

// A pen voice ends through the same allocator call as a finger; the LOG tag
// differs so the byte-log asserts can read a stroke (and its release) as the
// stylus's (src 4).
JNIEXPORT void JNICALL NB(nativePenEnd)(JNIEnv*, jobject, jint voice, jint lift) {
    play_post([=] {
        if (!P.mpe) return;
        hostmpe_msg_t m[4];
        const double now = now_s();
        const uint32_t n = hostmpe_touch_end(P.mpe, voice, now, (uint8_t)lift, m, 4);
        dispatch(m, n, SRC_PEN, true, true, now);
    });
}

// -- stylus (§7, DECISIONS_3 #38–#41) -----------------------------------------

JNIEXPORT jint JNICALL NB(nativePenBegin)(JNIEnv*, jobject, jdouble t_down, jint note, jint velocity) {
    if (!P.engines_ready.load()) return -1;
    int32_t voice = -1;
    play_post_sync([&] {
        if (!P.mpe) return;
        hostmpe_msg_t m[4];
        uint32_t n = 0;
        const double now = now_s();
        voice = hostmpe_pen_begin(P.mpe, now, (uint8_t)note, (uint8_t)velocity, m, 4, &n);
        dispatch(m, n, SRC_PEN, true, true, now);
        if (voice >= 0) {
            std::lock_guard<std::mutex> lk(P.lat_mu);
            P.marks.emplace_back((double)t_down, now);
        }
    });
    return voice;
}

// Legato glissando (#39): a batch containing a retrigger (Note On) ships
// WHOLE as strike class — the bend→On→Off crossing must arrive intact on
// every transport; a bend-only batch is a policed continuous dimension.
JNIEXPORT void JNICALL NB(nativePenGlide)(JNIEnv*, jobject, jint voice, jint note, jfloat offset,
                                          jfloat scale, jint velocity) {
    play_post([=] {
        if (!P.mpe) return;
        hostmpe_msg_t m[4];
        const uint32_t n = hostmpe_pen_glide(P.mpe, voice, (uint8_t)note, offset, scale,
                                             (uint8_t)velocity, m, 4);
        bool has_on = false;
        for (uint32_t i = 0; i < n; i++) if ((m[i].status & 0xF0) == 0x90) has_on = true;
        dispatch(m, n, SRC_PEN, has_on, true, now_s());
    });
}

// §3.3 stylus CC74. With slide_mode = 1 the loopback is SKIPPED — the shell
// drives the azimuth pinch through the gesture ABI and a second (mapper)
// pinch from the same CC74 would double it; outbound still records the
// dimension (DECISIONS_3 #38).
JNIEXPORT void JNICALL NB(nativePenSlide)(JNIEnv*, jobject, jint voice, jfloat eff,
                                          jboolean outbound_only) {
    play_post([=] {
        if (!P.mpe) return;
        hostmpe_msg_t m[2];
        const uint32_t n = hostmpe_pen_slide(P.mpe, voice, eff, m, 2);
        dispatch(m, n, SRC_PEN, false, outbound_only == JNI_FALSE, now_s());
    });
}

JNIEXPORT void JNICALL NB(nativePenPressure)(JNIEnv*, jobject, jint voice, jfloat force) {
    play_post([=] {
        if (!P.mpe) return;
        hostmpe_msg_t m[2];
        const uint32_t n = hostmpe_pen_pressure(P.mpe, voice, force, m, 2);
        dispatch(m, n, SRC_PEN, false, true, now_s());
    });
}

// -- control strip (§8, Step 18) ----------------------------------------------

JNIEXPORT void JNICALL NB(nativeStripPitchMove)(JNIEnv*, jobject, jfloat v) {
    play_post([=] {
        if (!P.strip) return;
        hostmpe_msg_t m[2];
        strip_dispatch(m, hostmpe_strip_pitch_move(P.strip, v, m, 2), false, now_s());
    });
}

JNIEXPORT void JNICALL NB(nativeStripPitchRelease)(JNIEnv*, jobject) {
    play_post([] {
        if (P.strip) hostmpe_strip_pitch_release(P.strip, now_s());
        // Ramp messages surface from hostmpe_strip_tick in play_drain.
    });
}

JNIEXPORT void JNICALL NB(nativeStripLatchMove)(JNIEnv*, jobject, jint wheel, jfloat delta) {
    play_post([=] {
        if (!P.strip) return;
        hostmpe_msg_t m[2];
        strip_dispatch(m, hostmpe_strip_latch_move(P.strip, wheel, delta, m, 2), false, now_s());
    });
}

JNIEXPORT void JNICALL NB(nativeStripSustainDown)(JNIEnv*, jobject) {
    play_post([] {
        if (!P.strip) return;
        hostmpe_msg_t m[2];
        strip_dispatch(m, hostmpe_strip_sustain_press(P.strip, m, 2), true, now_s());
    });
}

JNIEXPORT void JNICALL NB(nativeStripSustainUp)(JNIEnv*, jobject) {
    play_post([] {
        if (!P.strip) return;
        hostmpe_msg_t m[2];
        strip_dispatch(m, hostmpe_strip_sustain_release(P.strip, m, 2), true, now_s());
    });
}

JNIEXPORT void JNICALL NB(nativeStripSustainMode)(JNIEnv*, jobject, jboolean toggle) {
    play_post([=] {
        if (!P.strip) return;
        hostmpe_msg_t m[2];
        // A mode switch while sustain is ON emits the OFF — never a stranded
        // pedal; button class, never dropped.
        strip_dispatch(m, hostmpe_strip_sustain_mode(P.strip, toggle == JNI_TRUE, m, 2),
                       true, now_s());
    });
}

// Returns the wheel's assigned CC after the request (unchanged when refused).
JNIEXPORT jint JNICALL NB(nativeStripAssign)(JNIEnv*, jobject, jint wheel, jint cc) {
    if (!P.engines_ready.load()) return -1;
    int result = -1;
    play_post_sync([&] {
        if (!P.strip) return;
        const bool ok = hostmpe_strip_assign(P.strip, wheel, (uint8_t)cc);
        result = ok ? (int)hostmpe_strip_assigned_cc(P.strip, wheel) : -1;
    });
    return result;
}

// [pitch, latch0, latch1, latch2, sustain, cc0, cc1, cc2] — the engine's
// state for the strip's display mirrors (values persist in the engine).
JNIEXPORT void JNICALL NB(nativeStripState)(JNIEnv* env, jobject, jfloatArray out) {
    float v[8] = {0, 0, 0, 0, 0, 1, 23, 24};
    if (P.engines_ready.load()) {
        play_post_sync([&] {
            if (!P.strip) return;
            v[0] = hostmpe_strip_pitch_value(P.strip);
            for (int w = 0; w < 3; w++) {
                v[1 + w] = hostmpe_strip_latch_value(P.strip, w);
                v[5 + w] = (float)hostmpe_strip_assigned_cc(P.strip, w);
            }
            v[4] = hostmpe_strip_sustain_on(P.strip) ? 1.0f : 0.0f;
        });
    }
    if (env->GetArrayLength(out) >= 8) env->SetFloatArrayRegion(out, 0, 8, v);
}

// -- transports (§5.4) ---------------------------------------------------------

JNIEXPORT void JNICALL NB(nativeSetTransports)(JNIEnv*, jobject, jboolean usb, jboolean virt,
                                               jboolean ble) {
    play_post([=] {
        const bool want[SINK_COUNT] = {usb == JNI_TRUE, virt == JNI_TRUE, ble == JNI_TRUE};
        // A transport being switched OFF gets a zone silence first (CC64 +
        // CC123 on master and every member): otherwise a synth on that sink
        // holds whatever was sounding, forever. Voices on the other pipes
        // are untouched (stateless, DECISIONS_3 #26).
        hostmpe_msg_t z[64];
        const uint32_t zn = hostmpe_silence_zone(z, 64);
        for (int s = 0; s < SINK_COUNT; s++) {
            if (P.sink_on[s] && !want[s]) {
                wire(s, z, zn);
                LOGI("[out] silenced departing sink %d", s);
            }
            P.sink_on[s] = want[s];
        }
        LOGI("[out] transports: usb=%d virtual=%d ble=%d", want[0], want[1], want[2]);
    });
}

// Kotlin saw a sink come up (USB gadget port opened, a DAW opened the
// virtual device, a BLE central subscribed): re-send the handshake it missed.
JNIEXPORT void JNICALL NB(nativeSinkAppeared)(JNIEnv*, jobject, jint sink) {
    play_post([=] {
        LOGI("[out] sink %d appeared -> handshake re-send (debounced)", sink);
        auto_resync(now_s());
    });
}

// "Re-sync DAW": resend MCM/RPN0 everywhere (the loopback tolerates it).
JNIEXPORT void JNICALL NB(nativeResyncSession)(JNIEnv*, jobject) {
    play_post([] { send_session_config(now_s()); });
}

// MIDI panic: release every held voice and silence the zone on the loopback
// AND every transport (exempt, never decimated).
JNIEXPORT void JNICALL NB(nativePanic)(JNIEnv*, jobject) {
    play_post([] {
        if (!P.mpe) return;
        hostmpe_msg_t m[128];
        const double now = now_s();
        const uint32_t n = hostmpe_panic(P.mpe, now, m, 128);
        dispatch(m, n, SRC_TOUCH, true, true, now);
        LOGI("[panic] released all voices, %u messages", n);
    });
}

JNIEXPORT void JNICALL NB(nativeStartStorm)(JNIEnv*, jobject, jint seconds) {
    play_post([=] {
        if (P.storm || !P.mpe) return;
        P.storm = true;
        P.storm_t0 = now_s();
        P.storm_next = P.storm_t0;
        P.storm_seconds = seconds > 0 ? (double)seconds : 60.0;
        P.storm_tick = 0;
        for (int i = 0; i < 10; i++) P.storm_voices[i] = -1;
        LOGI("[storm] started: 10 voices, %.0f s", P.storm_seconds);
    });
}

JNIEXPORT void JNICALL NB(nativeFlushLogs)(JNIEnv*, jobject) {
    play_post([] { flush_logs_now(); });
}

JNIEXPORT jstring JNICALL NB(nativeStatusLine)(JNIEnv* env, jobject) {
    const shell::Stats s = shell::stats();
    const double lat = P.last_latency_ms.load();
    char buf[256];
    const char* thermal[] = {"none", "light", "moderate", "severe", "critical", "emergency", "shutdown"};
    const char* th = (s.thermal >= 0 && s.thermal <= 6) ? thermal[s.thermal] : "unknown";
    const long su = P.sent[0].load(), sv = P.sent[1].load(), sb = P.sent[2].load();
    if (lat >= 0) {
        snprintf(buf, sizeof(buf),
                 "t=%.0fs  %.1f fps  worst %.2f ms  thermal %s  dropped %u  touch %.1f ms  out u%ld/v%ld/b%ld",
                 s.t, (double)s.fps, (double)s.worst_ms, th, s.dropped, lat, su, sv, sb);
    } else {
        snprintf(buf, sizeof(buf),
                 "t=%.0fs  %.1f fps  worst %.2f ms  thermal %s  dropped %u  out u%ld/v%ld/b%ld",
                 s.t, (double)s.fps, (double)s.worst_ms, th, s.dropped, su, sv, sb);
    }
    return env->NewStringUTF(buf);
}

// -- geometry (any thread — the probe is instance-free, PHASE4 §2) -----------

// out[7] = note, cx, cy, r, semitone_dx, semitone_dy, semitone_step.
JNIEXPORT jboolean JNICALL NB(nativeLayoutProbe)(JNIEnv* env, jobject, jfloat x, jfloat y,
                                                 jfloat aspect, jfloatArray out) {
    const sumi_params_t p = shell::params_snapshot();
    sumi_cell_info_t info;
    if (!sumi_layout_probe(p.pitch_layout, &p, aspect, x, y, &info)) return JNI_FALSE;
    const float v[7] = {(float)info.note, info.cell_center_x, info.cell_center_y,
                        info.cell_radius, info.semitone_dx, info.semitone_dy,
                        info.semitone_step};
    if (env->GetArrayLength(out) >= 7) env->SetFloatArrayRegion(out, 0, 7, v);
    return JNI_TRUE;
}

// The lattice is a probe SWEEP (DECISIONS_3 #9) — never shell-side geometry.
// Returns [note, cx, cy, r] per unique cell.
JNIEXPORT jfloatArray JNICALL NB(nativeLatticeSweep)(JNIEnv* env, jobject, jfloat aspect,
                                                     jint nx, jint ny) {
    const sumi_params_t p = shell::params_snapshot();
    std::vector<float> cells;
    std::unordered_set<uint64_t> seen;
    sumi_cell_info_t info;
    if (nx < 2) nx = 2;
    if (ny < 2) ny = 2;
    for (int iy = 0; iy < ny; iy++) {
        for (int ix = 0; ix < nx; ix++) {
            const float x = (float)ix / (float)(nx - 1);
            const float y = (float)iy / (float)(ny - 1);
            if (!sumi_layout_probe(p.pitch_layout, &p, aspect, x, y, &info)) continue;
            const uint64_t key = (uint64_t)info.note
                               | ((uint64_t)(uint32_t)(info.cell_center_x * 4096.0f) << 8)
                               | ((uint64_t)(uint32_t)(info.cell_center_y * 4096.0f) << 28);
            if (!seen.insert(key).second) continue;
            cells.push_back((float)info.note);
            cells.push_back(info.cell_center_x);
            cells.push_back(info.cell_center_y);
            cells.push_back(info.cell_radius);
        }
    }
    jfloatArray arr = env->NewFloatArray((jsize)cells.size());
    if (arr && !cells.empty()) env->SetFloatArrayRegion(arr, 0, (jsize)cells.size(), cells.data());
    return arr;
}

// §3.2 Δ_eff for the joystick indicator (pure math, any thread).
JNIEXPORT void JNICALL NB(nativeJoystickEff)(JNIEnv* env, jobject, jfloat dx, jfloat dy,
                                             jfloat r_max, jfloatArray out) {
    float ex = 0, ey = 0;
    hostmpe_joystick_eff(dx, dy, r_max, &ex, &ey);
    const float v[2] = {ex, ey};
    if (env->GetArrayLength(out) >= 2) env->SetFloatArrayRegion(out, 0, 2, v);
}

// -- on-device headless suites (Step 22 DONE: the Android build runs the full
//    hostmpe suite + the normalizer's loopback-conformance suite) -----------

// Runs both suites on the CALLING thread (a Kotlin worker) with stdout and
// stderr redirected into `path`. Returns a bit mask of failed suites
// (1 = hostmpe, 2 = normalizer); 0 = everything passed.
JNIEXPORT jint JNICALL NB(nativeRunSelfTests)(JNIEnv* env, jobject, jstring jpath) {
    const char* c = env->GetStringUTFChars(jpath, nullptr);
    std::string path = c ? c : "";
    env->ReleaseStringUTFChars(jpath, c);
    // ONCE per process: both suites keep file-static check/failure counters
    // (they are ordinary desktop test binaries), so a second run in the same
    // process reports doubled counts and can never report pass again once
    // anything has failed. It also redirects fds 1 and 2 process-wide, which
    // is only acceptable as a one-shot.
    static std::atomic<bool> already{false};
    if (already.exchange(true)) {
        LOGW("SELFTEST already run in this process — restart the app to run it again");
        return -2;
    }
    FILE* f = fopen(path.c_str(), "w");
    if (!f) { already = false; return -1; }
    fflush(stdout);
    fflush(stderr);
    const int so = dup(1), se = dup(2);
    dup2(fileno(f), 1);
    dup2(fileno(f), 2);
    printf("== hostmpe_tests (on-device, %s) ==\n", "arm64-v8a");
    fflush(stdout);
    const int r1 = hostmpe_tests_main();
    fflush(stdout);
    fflush(stderr);
    printf("== normalizer_tests (on-device) ==\n");
    fflush(stdout);
    const int r2 = normalizer_tests_main();
    fflush(stdout);
    fflush(stderr);
    dup2(so, 1);
    dup2(se, 2);
    close(so);
    close(se);
    fclose(f);
    LOGI("SELFTEST hostmpe=%s normalizer=%s -> %s", r1 ? "FAIL" : "pass",
         r2 ? "FAIL" : "pass", path.c_str());
    return (r1 ? 1 : 0) | (r2 ? 2 : 0);
}

// A hardware device disconnected: its held channels free immediately (§5.1
// occupancy clears on device disconnect).
JNIEXPORT void JNICALL NB(nativeExternalClear)(JNIEnv*, jobject) {
    play_post([] { if (P.mpe) hostmpe_external_clear(P.mpe); });
}

#undef NB

} // extern "C"
