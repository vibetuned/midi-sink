// shell.h — shared plumbing between the two halves of the Android JNI shell:
// sumi_jni.cpp (SurfaceView → EGL render thread, AMidi ingest, evidence
// hooks) and sumi_play.cpp (Phase 4: the hostmpe host living on the AMidi
// poller thread, outbound transports, byte log). Both compile into the one
// libsumi-shell.so the Compose app loads; Kotlin sees only NativeBridge.
#pragma once
#include <jni.h>
#include <android/log.h>

#include <cstdint>
#include <functional>
#include <string>

#include "sumi_core.h"

#define LOG_TAG "sumi-shell"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace shell {

// Monotonic seconds (steady_clock == CLOCK_MONOTONIC on Android, the same
// clock Kotlin's System.nanoTime() reads) — the one clock for hostmpe
// timestamps, the limiters, the byte log and the latency marks.
double now_s();

// Render-thread command queue (DECISIONS_2 #32): every sumi_* call except
// sumi_push_midi runs on the render thread that owns the EGL context.
void post(std::function<void()> fn);
void post_sync(const std::function<void()>& fn);   // never from the render thread

// The §5.2 loopback push, under the DECISIONS #24 producer mutex. Called from
// the AMidi poller thread (the single producer) — and by the in-process
// stress feeder, which takes the same mutex while it runs.
void push_midi(uint8_t status, uint8_t d1, uint8_t d2);

const std::string& files_dir();
void csv_event(const char* fmt, ...);

// Host-owned params snapshot (PHASE4 §2 / DECISIONS_3 #2): the shell owns
// every params write, so the UI thread's copy IS the probe's ground truth —
// hit-testing never round-trips through the render thread. modify() edits
// the snapshot immediately (caller's thread) and posts the apply.
sumi_params_t params_snapshot();
void params_modify(const std::function<void(sumi_params_t&)>& fn);

// Per-second render stats for the settings status line.
struct Stats {
    double   t;
    float    fps;
    float    worst_ms;
    int      thermal;
    uint32_t dropped;
};
Stats stats();

// ---- play half (sumi_play.cpp) ---------------------------------------------
void play_init(JNIEnv* env);       // UI thread, from nativeInit
void play_shutdown();              // after the MIDI thread has joined
void play_thread_enter();          // MIDI thread start (JNI attach, engines)
void play_thread_exit();
void play_drain(double now);       // MIDI thread: commands, limiter drains, strip tick, storm
void play_wait(int max_ms);        // MIDI thread: block until a command or timeout
// Device bytes at the merge point: occupancy mask + byte log + loopback push.
void play_ingest_external(double now, uint8_t status, uint8_t d1, uint8_t d2);
void play_frame_rendered(double now);   // render thread, right after sumi_render
void play_instance_ready();             // render thread, after sumi_create: loopback exists now

} // namespace shell
