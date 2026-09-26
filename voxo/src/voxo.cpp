// voxo.cpp — Voxo's core (Phase 7 step 47, SOUND §1–§2 skeleton; DECISIONS_6
// #2–#5): the second SPSC ring is the core's own MIDI normalizer compiled from
// source (one MPE decoder, two builds, zero runtime coupling); a fixed voice
// pool keyed by (channel, note); a sine per voice following note + bend, with
// velocity and pressure to level and the sustain pedal in the release logic.
// voxo_render is the callback's whole body and honours the contract at the top
// of voxo.h: no allocation, no lock, no logging, no blocking call; every voice
// transition happens at block start, in event order, before a sample is
// written. The shell's settings arrive through atomics read at block start.
#include "voxo.h"
#include "voxo_internal.h"
#include "midi_normalizer.h"   // core/src — compiled into this library, never linked from libsumi

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>

#define VOXO_VERSION_MAJOR 0
#define VOXO_VERSION_MINOR 2
#define VOXO_VERSION_PATCH 0

namespace {

constexpr uint32_t MAX_VOICES_CAP = 64;
constexpr uint32_t EVENTS_PER_BLOCK = 512;     // drained at block start; the ring keeps the rest for the next block
constexpr float    VOICE_AMPLITUDE  = 0.18f;   // one sine at full level; 16 of them soft-clip, never wrap

struct Voice {
    bool     active;       // in the pool (attacking, sustaining or releasing)
    bool     releasing;    // level falling toward silence
    bool     held;         // key down (note-off not yet received)
    bool     pedalled;     // note-off received while CC64 was down: releases with the pedal
    uint8_t  channel;
    uint8_t  note;
    uint32_t serial;       // allocation order, for stealing the oldest
    double   phase;        // radians, wrapped
    float    freq;         // Hz, ramping toward freq_target per sample
    float    freq_target;
    float    level;        // the envelope, 0..1
    float    level_target; // 1 while sounding, 0 in release
    float    vel_gain;     // velocity^1.5
    float    pressure;     // smoothed 0..1 (channel or poly pressure)
    float    pressure_target;
};

struct Channel {
    float bend_semitones;
    float pressure;        // last channel pressure 0..1
    bool  sustain;         // CC 64 >= 64
};

} // namespace

struct voxo_t {
    // Configuration (shell threads only).
    uint32_t sample_rate;
    uint32_t block_frames;
    uint32_t max_voices;
    voxo_log_fn log_cb;
    void*    log_user;
    // The shared decoder: its ring is the second SPSC of SOUND §1.
    sumi_normalizer_t* normalizer;
    // Shell -> callback, read at block start (DECISIONS_6 #3).
    std::atomic<uint32_t> pending_mode;      // UINT32_MAX = nothing pending
    std::atomic<float>    gain;
    // Callback -> shell.
    std::atomic<uint32_t> active_voices;
    std::atomic<uint32_t> callbacks;
    std::atomic<uint32_t> xruns;
    std::atomic<float>    render_last_ms;
    std::atomic<float>    render_max_ms;
    std::atomic<uint32_t> device_rate;
    std::atomic<uint32_t> device_block;
    // Callback-thread state (never touched by the shell while running).
    Voice    voices[MAX_VOICES_CAP];
    Channel  channels[16];
    uint32_t next_serial;
    double   clock_seconds;         // the normalizer's monotonic clock: rendered time
    float    attack_coef, release_coef, freq_coef, press_coef;
    std::atomic<uint32_t> effective_mode;   // callback -> shell: the dialect after the last block
    std::atomic<uint32_t> note_ons;         // the latency probe (step 48)
    std::atomic<double>   last_note_on_seconds;
    double   block_start_seconds;           // callback-thread state: set by the backend before each render, 0 without a device
    sumi_midi_event_t events[EVENTS_PER_BLOCK];
    // Backend state.
    bool     running;
    char     device_name[64];
    void*    backend;                // the miniaudio device (backend_miniaudio.cpp)
    uint32_t warmup_callbacks;       // the first callbacks are not judged for XRuns
};

namespace {

inline float note_to_hz(float note) { return 440.0f * std::exp2((note - 69.0f) / 12.0f); }
inline float one_pole_coef(float seconds, uint32_t rate) {
    return seconds <= 0.0f ? 1.0f : 1.0f - std::exp(-1.0f / (seconds * (float)rate));
}

void voice_retune(voxo_t* v, Voice& vc) {
    vc.freq_target = note_to_hz((float)vc.note + v->channels[vc.channel & 15].bend_semitones);
}

void voice_start(voxo_t* v, Voice& vc, uint8_t channel, uint8_t note, uint8_t velocity, bool fresh) {
    vc.active = true; vc.releasing = false; vc.held = true; vc.pedalled = false;
    vc.channel = channel; vc.note = note;
    vc.serial = v->next_serial++;
    const float vn = (float)velocity / 127.0f;
    vc.vel_gain = vn * std::sqrt(vn);
    vc.level_target = 1.0f;
    vc.pressure_target = v->channels[channel & 15].pressure;
    if (fresh) { vc.phase = 0.0; vc.level = 0.0f; vc.pressure = vc.pressure_target; }
    voice_retune(v, vc);
    if (fresh) vc.freq = vc.freq_target;   // a retrigger glides from where it was
}

Voice* voice_find(voxo_t* v, uint8_t channel, uint8_t note) {
    for (uint32_t i = 0; i < v->max_voices; i++) {
        Voice& vc = v->voices[i];
        if (vc.active && vc.channel == channel && vc.note == note) return &vc;
    }
    return nullptr;
}

Voice* voice_alloc(voxo_t* v) {
    for (uint32_t i = 0; i < v->max_voices; i++) if (!v->voices[i].active) return &v->voices[i];
    // Steal: the oldest releasing voice, else the oldest of all.
    Voice* best = nullptr;
    for (int pass = 0; pass < 2 && !best; pass++) {
        for (uint32_t i = 0; i < v->max_voices; i++) {
            Voice& vc = v->voices[i];
            if (pass == 0 && !vc.releasing) continue;
            if (!best || vc.serial < best->serial) best = &vc;
        }
    }
    return best;
}

void voice_release(Voice& vc) {
    vc.releasing = true; vc.held = false; vc.pedalled = false; vc.level_target = 0.0f;
}

void apply_event(voxo_t* v, const sumi_midi_event_t& e) {
    Channel& ch = v->channels[e.channel & 15];
    switch (e.kind) {
        case SUMI_MEV_NOTE_ON: {
            v->note_ons.fetch_add(1, std::memory_order_relaxed);
            v->last_note_on_seconds.store(v->block_start_seconds, std::memory_order_relaxed);
            if (Voice* vc = voice_find(v, e.channel, e.a)) { voice_start(v, *vc, e.channel, e.a, e.b, false); break; }
            if (Voice* vc = voice_alloc(v)) voice_start(v, *vc, e.channel, e.a, e.b, !vc->active);
            break;
        }
        case SUMI_MEV_NOTE_OFF: {
            if (Voice* vc = voice_find(v, e.channel, e.a)) {
                if (!vc->held) break;
                if (ch.sustain) { vc->held = false; vc->pedalled = true; }
                else voice_release(*vc);
            }
            break;
        }
        case SUMI_MEV_BEND:
            ch.bend_semitones = e.f;
            for (uint32_t i = 0; i < v->max_voices; i++) {
                Voice& vc = v->voices[i];
                if (vc.active && vc.channel == e.channel) voice_retune(v, vc);
            }
            break;
        case SUMI_MEV_CHANNEL_PRESSURE:
            ch.pressure = (float)e.b / 127.0f;
            for (uint32_t i = 0; i < v->max_voices; i++) {
                Voice& vc = v->voices[i];
                if (vc.active && vc.channel == e.channel) vc.pressure_target = ch.pressure;
            }
            break;
        case SUMI_MEV_POLY_PRESSURE:
            if (Voice* vc = voice_find(v, e.channel, e.a)) vc->pressure_target = (float)e.b / 127.0f;
            break;
        case SUMI_MEV_CC:
            if (e.a == 64) {                       // sustain: the release logic (SOUND §2)
                const bool down = e.b >= 64;
                if (!down && ch.sustain) {
                    for (uint32_t i = 0; i < v->max_voices; i++) {
                        Voice& vc = v->voices[i];
                        if (vc.active && vc.channel == e.channel && vc.pedalled) voice_release(vc);
                    }
                }
                ch.sustain = down;
            } else if (e.a == 123 || e.a == 120) { // all notes off / all sound off: the panic
                for (uint32_t i = 0; i < v->max_voices; i++) {
                    Voice& vc = v->voices[i];
                    if (!vc.active || vc.channel != e.channel) continue;
                    if (e.a == 120) { vc.active = false; vc.level = 0.0f; }
                    else voice_release(vc);
                }
                ch.sustain = false;
            }
            break;
        default: break;
    }
}

} // namespace

/* ------------------------------------------------------------------ */
/* The ABI                                                             */
/* ------------------------------------------------------------------ */

extern "C" {

uint32_t voxo_version(void) {
    return ((uint32_t)VOXO_VERSION_MAJOR << 16) | ((uint32_t)VOXO_VERSION_MINOR << 8) | (uint32_t)VOXO_VERSION_PATCH;
}

uint32_t voxo_default_block_frames(void) {
    // DECISIONS_6 #4: the table. Confirmed per platform in steps 48 (mobile)
    // and 55 (Windows / Linux); CoreAudio honours 128 as asked.
#if defined(__APPLE__)
    return 128;
#elif defined(__ANDROID__)
    return 192;
#else
    return 256;
#endif
}

voxo_t* voxo_create(const voxo_config_t* config) {
    voxo_config_t c;
    if (config) c = *config; else std::memset(&c, 0, sizeof(c));
    voxo_t* v = (voxo_t*)std::calloc(1, sizeof(voxo_t));
    if (!v) return nullptr;
    v->sample_rate  = c.sample_rate ? c.sample_rate : 48000u;
    v->block_frames = c.block_frames ? c.block_frames : voxo_default_block_frames();
    v->max_voices   = c.max_voices == 0 ? 16u : (c.max_voices > MAX_VOICES_CAP ? MAX_VOICES_CAP : c.max_voices);
    v->log_cb = c.log_cb; v->log_user = c.log_user;
    // The normalizer's log hook stays NULL: its mode-change lines would fire on
    // the callback thread. The shell reads the effective mode from voxo_stats.
    v->normalizer = sumi_normalizer_create(nullptr, nullptr);
    if (!v->normalizer) { std::free(v); return nullptr; }
    new (&v->pending_mode) std::atomic<uint32_t>(UINT32_MAX);
    new (&v->gain) std::atomic<float>(1.0f);
    new (&v->active_voices) std::atomic<uint32_t>(0);
    new (&v->callbacks) std::atomic<uint32_t>(0);
    new (&v->xruns) std::atomic<uint32_t>(0);
    new (&v->render_last_ms) std::atomic<float>(0.0f);
    new (&v->render_max_ms) std::atomic<float>(0.0f);
    new (&v->device_rate) std::atomic<uint32_t>(v->sample_rate);
    new (&v->device_block) std::atomic<uint32_t>(v->block_frames);
    new (&v->effective_mode) std::atomic<uint32_t>((uint32_t)SUMI_INPUT_AUTO);
    new (&v->note_ons) std::atomic<uint32_t>(0);
    new (&v->last_note_on_seconds) std::atomic<double>(0.0);
    voxo_core_set_rate(v, v->sample_rate);
    return v;
}

void voxo_destroy(voxo_t* v) {
    if (!v) return;
    voxo_stop(v);
    sumi_normalizer_destroy(v->normalizer);
    std::free(v);
}

bool voxo_start(voxo_t* v) {
    if (!v) return false;
    if (v->running) return true;
    uint32_t rate = 0, block = 0;
    if (!voxo_backend_start(v, v->sample_rate, v->block_frames, &rate, &block, v->device_name, sizeof(v->device_name))) {
        if (v->log_cb) v->log_cb(1, "voxo: no output device; running silent", v->log_user);
        return false;
    }
    v->running = true;
    return true;
}

void voxo_stop(voxo_t* v) {
    if (!v || !v->running) return;
    voxo_backend_stop(v);
    v->running = false;
    v->device_name[0] = 0;
    // Silence for the next start: the pool is the callback's, and no callback runs now.
    for (uint32_t i = 0; i < MAX_VOICES_CAP; i++) v->voices[i].active = false;
    for (int c = 0; c < 16; c++) v->channels[c].sustain = false;
    v->active_voices.store(0, std::memory_order_relaxed);
    v->block_start_seconds = 0.0;
    v->device_rate.store(v->sample_rate, std::memory_order_relaxed);
    v->device_block.store(v->block_frames, std::memory_order_relaxed);
    voxo_core_set_rate(v, v->sample_rate);
}

bool voxo_running(const voxo_t* v) { return v && v->running; }

void voxo_push_midi(voxo_t* v, uint8_t status, uint8_t data1, uint8_t data2) {
    if (v) sumi_normalizer_push(v->normalizer, status, data1, data2);
}

void voxo_set_input_mode(voxo_t* v, uint32_t mode) {
    if (v) v->pending_mode.store(mode > 3u ? 0u : mode, std::memory_order_release);
}

void voxo_set_gain(voxo_t* v, float gain) {
    if (!v) return;
    if (!(gain >= 0.0f)) gain = 0.0f;
    if (gain > 2.0f) gain = 2.0f;
    v->gain.store(gain, std::memory_order_relaxed);
}

/* The callback's whole body. */
void voxo_render(voxo_t* v, float* out_lr, uint32_t frames) {
    if (!out_lr || frames == 0) return;
    if (!v) { std::memset(out_lr, 0, sizeof(float) * 2u * frames); return; }

    // 1. The shell's settings, once per block.
    const uint32_t pending = v->pending_mode.exchange(UINT32_MAX, std::memory_order_acq_rel);
    if (pending != UINT32_MAX) sumi_normalizer_set_mode(v->normalizer, (sumi_input_mode_t)pending);
    const float gain = v->gain.load(std::memory_order_relaxed);

    // 2. Every voice transition, in order, before a sample is written.
    const uint32_t n = sumi_normalizer_drain(v->normalizer, v->clock_seconds, v->events, EVENTS_PER_BLOCK);
    for (uint32_t i = 0; i < n; i++) apply_event(v, v->events[i]);
    const uint32_t mode = (uint32_t)sumi_normalizer_mode(v->normalizer);
    v->effective_mode.store(mode, std::memory_order_relaxed);
    // Pressure shapes the level only where the dialect carries it (MPE, wind).
    const float press_mix = (mode == SUMI_INPUT_CLASSIC) ? 0.0f : 0.65f;

    // 3. The block.
    std::memset(out_lr, 0, sizeof(float) * 2u * frames);
    const double two_pi = 6.283185307179586;
    const double inv_rate = 1.0 / (double)v->device_rate.load(std::memory_order_relaxed);
    uint32_t active = 0;
    for (uint32_t vi = 0; vi < v->max_voices; vi++) {
        Voice& vc = v->voices[vi];
        if (!vc.active) continue;
        double phase = vc.phase;
        float freq = vc.freq, level = vc.level, press = vc.pressure;
        const float env_coef = vc.releasing ? v->release_coef : v->attack_coef;
        for (uint32_t f = 0; f < frames; f++) {
            freq  += (vc.freq_target - freq) * v->freq_coef;
            level += (vc.level_target - level) * env_coef;
            press += (vc.pressure_target - press) * v->press_coef;
            const float g = VOICE_AMPLITUDE * vc.vel_gain * level * (1.0f - press_mix + press_mix * press);
            const float s = (float)std::sin(phase) * g;
            out_lr[2u * f]      += s;
            out_lr[2u * f + 1u] += s;
            phase += two_pi * (double)freq * inv_rate;
            if (phase >= two_pi) phase -= two_pi;
        }
        vc.phase = phase; vc.freq = freq; vc.level = level; vc.pressure = press;
        if (vc.releasing && level < 1e-4f) { vc.active = false; continue; }
        active++;
    }
    // Master gain and a soft knee: linear below 0.5, then compressed toward
    // 1.0 — one voice passes untouched, the sum of sixteen never wraps.
    for (uint32_t i = 0; i < 2u * frames; i++) {
        const float x = out_lr[i] * gain;
        const float a = std::fabs(x);
        if (a > 0.5f) {
            const float y = 0.5f + 0.5f * (1.0f - std::exp(-(a - 0.5f) * 2.0f));
            out_lr[i] = x < 0.0f ? -y : y;
        } else {
            out_lr[i] = x;
        }
    }
    v->clock_seconds += (double)frames * inv_rate;
    v->active_voices.store(active, std::memory_order_relaxed);
}

void voxo_stats(const voxo_t* v, voxo_stats_t* out) {
    if (!out) return;
    std::memset(out, 0, sizeof(*out));
    if (!v) return;
    out->sample_rate    = v->device_rate.load(std::memory_order_relaxed);
    out->block_frames   = v->device_block.load(std::memory_order_relaxed);
    out->active_voices  = v->active_voices.load(std::memory_order_relaxed);
    out->dropped_midi   = sumi_normalizer_dropped(v->normalizer);
    out->callbacks      = v->callbacks.load(std::memory_order_relaxed);
    out->xruns          = v->xruns.load(std::memory_order_relaxed);
    out->render_last_ms = v->render_last_ms.load(std::memory_order_relaxed);
    out->render_max_ms  = v->render_max_ms.load(std::memory_order_relaxed);
    out->input_mode     = v->effective_mode.load(std::memory_order_relaxed);
    out->note_ons       = v->note_ons.load(std::memory_order_relaxed);
    out->last_note_on_seconds = v->last_note_on_seconds.load(std::memory_order_relaxed);
    std::memcpy(out->device, v->device_name, sizeof(out->device));
    out->device[sizeof(out->device) - 1] = 0;
    if (v->running) voxo_backend_query(const_cast<voxo_t*>(v), out);
}

} // extern "C"

/* ------------------------------------------------------------------ */
/* The backend's hooks (voxo_internal.h)                               */
/* ------------------------------------------------------------------ */

void voxo_core_set_rate(voxo_t* v, uint32_t rate) {
    if (rate == 0) rate = 48000u;
    v->device_rate.store(rate, std::memory_order_relaxed);
    v->attack_coef  = one_pole_coef(0.003f, rate);   // 3 ms in
    v->release_coef = one_pole_coef(0.040f, rate);   // 40 ms out
    v->freq_coef    = one_pole_coef(0.004f, rate);   // the per-sample glide toward the block's pitch
    v->press_coef   = one_pole_coef(0.020f, rate);   // pressure smoothing
}

void voxo_core_block_start(voxo_t* v, double seconds) { v->block_start_seconds = seconds; }

void voxo_core_device_opened(voxo_t* v, uint32_t rate, uint32_t block) {
    voxo_core_set_rate(v, rate);
    v->device_block.store(block, std::memory_order_relaxed);
    v->callbacks.store(0, std::memory_order_relaxed);
    v->xruns.store(0, std::memory_order_relaxed);
    v->render_max_ms.store(0.0f, std::memory_order_relaxed);
    v->warmup_callbacks = 8;
}

void voxo_core_callback_timing(voxo_t* v, uint32_t frames, double late_by_frames, double render_ms) {
    v->callbacks.fetch_add(1, std::memory_order_relaxed);
    v->render_last_ms.store((float)render_ms, std::memory_order_relaxed);
    if ((float)render_ms > v->render_max_ms.load(std::memory_order_relaxed))
        v->render_max_ms.store((float)render_ms, std::memory_order_relaxed);
    if (v->warmup_callbacks > 0) { v->warmup_callbacks--; return; }
    const double period_ms = 1000.0 * (double)frames / (double)v->device_rate.load(std::memory_order_relaxed);
    if (late_by_frames > 0.5 * (double)frames || render_ms > period_ms)
        v->xruns.fetch_add(1, std::memory_order_relaxed);
}

void** voxo_core_backend_slot(voxo_t* v) { return &v->backend; }
