// backend_miniaudio.cpp — Voxo's audio backend (Phase 7 step 47, SOUND §1;
// DECISIONS_6 #5): miniaudio over the platform's default output — CoreAudio
// here, WASAPI / ALSA-PulseAudio on the other desktops (exercised in step
// 55), AAudio / the iOS session in steps 48, 53, 54. This is the ONE
// translation unit that defines MINIAUDIO_IMPLEMENTATION. The callback does
// two things beside voxo_render: reads a monotonic clock (vDSO / commpage —
// no blocking call) and feeds the core's timing hook.
#include "voxo.h"
#include "voxo_internal.h"

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MA_NO_WAV
#define MA_NO_FLAC
#define MA_NO_MP3
#if defined(__APPLE__)
  #define MA_NO_RUNTIME_LINKING   // the frameworks are linked (voxo/CMakeLists.txt): no dlopen at start, and iOS forbids it
#endif
#define MINIAUDIO_IMPLEMENTATION
#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wunused-function"
  #pragma clang diagnostic ignored "-Wunused-variable"
#elif defined(__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-function"
  #pragma GCC diagnostic ignored "-Wunused-variable"
#endif
#include "miniaudio.h"
#if defined(__clang__)
  #pragma clang diagnostic pop
#elif defined(__GNUC__)
  #pragma GCC diagnostic pop
#endif

#include <chrono>
#include <cstring>
#include <new>
#if defined(__ANDROID__)
  #include <dlfcn.h>
  #include <time.h>
#endif

namespace {

#if defined(__ANDROID__)
// The AAudio calls miniaudio does not load, for the step-48 numbers (the
// granted performance mode, the burst, the buffer, the underruns, the
// presentation timestamp). Resolved once from libaaudio.so; absent = zeros.
struct AAudioExtra {
    bool tried = false;
    int32_t (*getPerformanceMode)(void*) = nullptr;
    int32_t (*getXRunCount)(void*) = nullptr;
    int32_t (*getBufferSizeInFrames)(void*) = nullptr;
    int32_t (*getFramesPerBurst)(void*) = nullptr;
    int64_t (*getFramesWritten)(void*) = nullptr;
    int32_t (*getTimestamp)(void*, clockid_t, int64_t*, int64_t*) = nullptr;
    int32_t (*setBufferSizeInFrames)(void*, int32_t) = nullptr;
    void load() {
        if (tried) return;
        tried = true;
        void* h = dlopen("libaaudio.so", RTLD_NOW);
        if (!h) return;
        getPerformanceMode    = (int32_t (*)(void*))dlsym(h, "AAudioStream_getPerformanceMode");
        getXRunCount          = (int32_t (*)(void*))dlsym(h, "AAudioStream_getXRunCount");
        getBufferSizeInFrames = (int32_t (*)(void*))dlsym(h, "AAudioStream_getBufferSizeInFrames");
        getFramesPerBurst     = (int32_t (*)(void*))dlsym(h, "AAudioStream_getFramesPerBurst");
        getFramesWritten      = (int64_t (*)(void*))dlsym(h, "AAudioStream_getFramesWritten");
        getTimestamp          = (int32_t (*)(void*, clockid_t, int64_t*, int64_t*))dlsym(h, "AAudioStream_getTimestamp");
        setBufferSizeInFrames = (int32_t (*)(void*, int32_t))dlsym(h, "AAudioStream_setBufferSizeInFrames");
    }
};
AAudioExtra g_aaudio;
#endif

struct Backend {
    ma_context context;
    ma_device device;
    voxo_t*   owner;
    double    last_callback_seconds;   // 0 = none yet
    uint32_t  rate;
    uint32_t  wanted_block;
};

inline double now_seconds() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

void data_callback(ma_device* device, void* output, const void* /*input*/, ma_uint32 frames) {
    Backend* b = (Backend*)device->pUserData;
    const double t0 = now_seconds();
    double late_by_frames = 0.0;
    if (b->last_callback_seconds > 0.0) {
        const double expected = (double)frames / (double)b->rate;
        const double gap = t0 - b->last_callback_seconds;
        late_by_frames = (gap - expected) * (double)b->rate;
        if (late_by_frames < 0.0) late_by_frames = 0.0;
    }
    b->last_callback_seconds = t0;
    voxo_core_block_start(b->owner, t0);
    voxo_render(b->owner, (float*)output, (uint32_t)frames);
    const double render_ms = (now_seconds() - t0) * 1000.0;
    voxo_core_callback_timing(b->owner, (uint32_t)frames, late_by_frames, render_ms);
}

} // namespace

double voxo_now_seconds(void) { return now_seconds(); }

void voxo_backend_query(voxo_t* v, voxo_stats_t* out) {
    Backend* b = (Backend*)*voxo_core_backend_slot(v);
    if (!b) return;
    const ma_device& d = b->device;
    out->frames_per_burst = d.playback.internalPeriodSizeInFrames;
    out->buffer_frames    = d.playback.internalPeriodSizeInFrames * d.playback.internalPeriods;
    out->low_latency      = (d.playback.internalPeriodSizeInFrames == b->wanted_block) ? 1u : 0u;
#if defined(__ANDROID__)
    if (d.pContext && d.pContext->backend == ma_backend_aaudio && d.aaudio.pStreamPlayback) {
        g_aaudio.load();
        void* stream = d.aaudio.pStreamPlayback;
        if (g_aaudio.getFramesPerBurst)     out->frames_per_burst = (uint32_t)g_aaudio.getFramesPerBurst(stream);
        if (g_aaudio.getBufferSizeInFrames) out->buffer_frames    = (uint32_t)g_aaudio.getBufferSizeInFrames(stream);
        if (g_aaudio.getXRunCount)          out->device_xruns     = (uint32_t)g_aaudio.getXRunCount(stream);
        if (g_aaudio.getPerformanceMode)    out->low_latency      = (g_aaudio.getPerformanceMode(stream) == 12 /* AAUDIO_PERFORMANCE_MODE_LOW_LATENCY */) ? 1u : 0u;
        if (g_aaudio.getTimestamp && g_aaudio.getFramesWritten) {
            // Oboe's formula: the newest frame written reaches the DAC at
            // (written - presented) / rate after the presentation timestamp.
            int64_t pos = 0, tns = 0;
            if (g_aaudio.getTimestamp(stream, CLOCK_MONOTONIC, &pos, &tns) == 0 && tns > 0) {
                struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
                const int64_t now_ns = (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
                const int64_t written = g_aaudio.getFramesWritten(stream);
                const double rate = (double)(d.sampleRate ? d.sampleRate : b->rate);
                const double ms = ((double)(written - pos) * 1e9 / rate + (double)(tns - now_ns)) / 1e6;
                // Before the stream has presented a frame the timestamp is not
                // meaningful (the first spike read -3e6 ms): report 0 = unknown.
                out->output_latency_ms = (ms > 0.0 && ms < 2000.0) ? (float)ms : 0.0f;
            }
        }
    }
#endif
}

bool voxo_backend_start(voxo_t* v, uint32_t want_rate, uint32_t want_block,
                        uint32_t* out_rate, uint32_t* out_block, char* name, size_t name_cap) {
    void** slot = voxo_core_backend_slot(v);
    if (*slot) return true;
    Backend* b = new (std::nothrow) Backend;
    if (!b) return false;
    std::memset(&b->context, 0, sizeof(b->context));
    std::memset(&b->device, 0, sizeof(b->device));
    b->owner = v; b->last_callback_seconds = 0.0; b->rate = want_rate; b->wanted_block = want_block;

    // The context: on iOS the SHELL owns the AVAudioSession (SOUND §4 — the
    // category, the preferred rate and IO buffer, interruptions), so
    // miniaudio leaves the category alone; elsewhere the fields are ignored.
    ma_context_config ctx = ma_context_config_init();
    ctx.coreaudio.sessionCategory = ma_ios_session_category_none;
    if (ma_context_init(nullptr, 0, &ctx, &b->context) != MA_SUCCESS) { delete b; return false; }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate        = want_rate;
    config.periodSizeInFrames = want_block;
    config.performanceProfile = ma_performance_profile_low_latency;
    config.dataCallback      = data_callback;
    config.pUserData         = b;
#if defined(__ANDROID__)
    // Step 48: the low-latency path wants AAudio's own burst as the callback
    // size (miniaudio leaves the capacity unset by default, which keeps the
    // MMAP path; the wanted block only names the ambition).
    config.aaudio.usage = ma_aaudio_usage_game;
#endif
    if (ma_device_init(&b->context, &config, &b->device) != MA_SUCCESS) { ma_context_uninit(&b->context); delete b; return false; }
    const uint32_t rate  = b->device.sampleRate ? b->device.sampleRate : want_rate;
    const uint32_t block = b->device.playback.internalPeriodSizeInFrames ? b->device.playback.internalPeriodSizeInFrames : want_block;
    b->rate = rate;
    voxo_core_device_opened(v, rate, block);
    if (name && name_cap) {
        std::strncpy(name, b->device.playback.name, name_cap - 1);
        name[name_cap - 1] = 0;
    }
#if defined(__ANDROID__)
    // Step 48: AAudio opens with a deep default buffer (eight bursts on the
    // Tab, 32 ms); the low-latency practice (Oboe's) is two bursts, grown on
    // underruns. Two here; the spike measures the underruns under load and
    // step 54 decides whether a tuner is needed.
    if (b->device.pContext && b->device.pContext->backend == ma_backend_aaudio && b->device.aaudio.pStreamPlayback) {
        g_aaudio.load();
        if (g_aaudio.setBufferSizeInFrames && g_aaudio.getFramesPerBurst) {
            const int32_t burst = g_aaudio.getFramesPerBurst(b->device.aaudio.pStreamPlayback);
            if (burst > 0) g_aaudio.setBufferSizeInFrames(b->device.aaudio.pStreamPlayback, 2 * burst);
        }
    }
#endif
    if (ma_device_start(&b->device) != MA_SUCCESS) { ma_device_uninit(&b->device); ma_context_uninit(&b->context); delete b; return false; }
    *slot = b;
    if (out_rate) *out_rate = rate;
    if (out_block) *out_block = block;
    return true;
}

void voxo_backend_stop(voxo_t* v) {
    void** slot = voxo_core_backend_slot(v);
    Backend* b = (Backend*)*slot;
    if (!b) return;
    ma_device_stop(&b->device);     // synchronous: no callback runs after it returns
    ma_device_uninit(&b->device);
    ma_context_uninit(&b->context);
    delete b;
    *slot = nullptr;
}
