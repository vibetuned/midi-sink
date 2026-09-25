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

namespace {

struct Backend {
    ma_device device;
    voxo_t*   owner;
    double    last_callback_seconds;   // 0 = none yet
    uint32_t  rate;
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
    voxo_render(b->owner, (float*)output, (uint32_t)frames);
    const double render_ms = (now_seconds() - t0) * 1000.0;
    voxo_core_callback_timing(b->owner, (uint32_t)frames, late_by_frames, render_ms);
}

} // namespace

bool voxo_backend_start(voxo_t* v, uint32_t want_rate, uint32_t want_block,
                        uint32_t* out_rate, uint32_t* out_block, char* name, size_t name_cap) {
    void** slot = voxo_core_backend_slot(v);
    if (*slot) return true;
    Backend* b = new (std::nothrow) Backend;
    if (!b) return false;
    std::memset(&b->device, 0, sizeof(b->device));
    b->owner = v; b->last_callback_seconds = 0.0; b->rate = want_rate;

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate        = want_rate;
    config.periodSizeInFrames = want_block;
    config.performanceProfile = ma_performance_profile_low_latency;
    config.dataCallback      = data_callback;
    config.pUserData         = b;
    if (ma_device_init(nullptr, &config, &b->device) != MA_SUCCESS) { delete b; return false; }
    const uint32_t rate  = b->device.sampleRate ? b->device.sampleRate : want_rate;
    const uint32_t block = b->device.playback.internalPeriodSizeInFrames ? b->device.playback.internalPeriodSizeInFrames : want_block;
    b->rate = rate;
    voxo_core_device_opened(v, rate, block);
    if (name && name_cap) {
        std::strncpy(name, b->device.playback.name, name_cap - 1);
        name[name_cap - 1] = 0;
    }
    if (ma_device_start(&b->device) != MA_SUCCESS) { ma_device_uninit(&b->device); delete b; return false; }
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
    delete b;
    *slot = nullptr;
}
