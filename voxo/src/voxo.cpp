// voxo.cpp — Voxo's core (Phase 7, SOUND §1–§2; DECISIONS_6 #2–#5, #10–#12):
// the second SPSC ring is the core's own MIDI normalizer compiled from source
// (one MPE decoder, two builds, zero runtime coupling); a fixed voice pool
// keyed by (channel, note); each voice reads THE sample at the ratio
// 2^((note - root + bend)/12) — the ratio recomputed at every pitch event and
// ramped per sample, 4-point Hermite interpolation (linear as the lab's
// comparison) — or, with no sample loaded, a sine; velocity and pressure to
// level, the sustain pedal in the release logic, Local Control tracked.
// voxo_render is the callback's whole body and honours the contract at the
// top of voxo.h: no allocation, no lock, no logging, no blocking call; every
// voice transition happens at block start, in event order, before a sample
// is written. The shell's settings — and the sample itself — arrive through
// atomics read at block start.
#include "voxo.h"
#include "voxo_internal.h"
#include "midi_normalizer.h"   // core/src — compiled into this library, never linked from libsumi
#include "ds_preset.h"         // step 50: the Decent Sampler front end (shell-thread only)

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#define VOXO_VERSION_MAJOR 0
#define VOXO_VERSION_MINOR 4
#define VOXO_VERSION_PATCH 0

namespace {

constexpr uint32_t MAX_VOICES_CAP = 64;
constexpr uint32_t EVENTS_PER_BLOCK = 512;     // drained at block start; the ring keeps the rest for the next block
constexpr float    SINE_AMPLITUDE   = 0.18f;   // one sine at full level; 16 of them soft-clip, never wrap
constexpr float    SAMPLE_AMPLITUDE = 0.40f;   // one sample voice at full level
constexpr uint32_t PAD_BEFORE = 2, PAD_AFTER = 4;   // the 4-point read never bounds-checks

// The sample: immutable once published to the callback (DECISIONS_6 #11).
struct Sample {
    float*   data;       // interleaved, PAD_BEFORE zero frames before, PAD_AFTER after
    uint32_t frames;
    uint32_t channels;   // 1 or 2
    uint32_t rate;
    float    root_note;
    float    root_hz;
};
Sample g_no_sample;      // the sentinel "clear" request

struct Voice {
    bool     active;       // in the pool (attacking, sustaining or releasing)
    bool     releasing;    // level falling toward silence
    bool     held;         // key down (note-off not yet received)
    bool     pedalled;     // note-off received while CC64 was down: releases with the pedal
    uint8_t  channel;
    uint8_t  note;
    uint32_t serial;       // allocation order, for stealing the oldest
    double   phase;        // the sine's radians, wrapped
    double   pos;          // the sample read position, frames (fractional)
    float    freq;         // Hz as rendered; the ratio is freq / root. Recomputed per block from
    float    freq_target;  // freq_target (note + bend) and ramped LINEARLY per sample across the block
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
    std::atomic<uint32_t> interpolation;     // 0 Hermite, 1 linear
    std::atomic<uint32_t> local_control;     // tracked; the shell applies it
    std::atomic<Sample*>  pending_sample;    // the next sample (or &g_no_sample = clear); the callback takes it
    std::atomic<Sample*>  retired_sample;    // what the callback let go; the shell frees it
    // Callback -> shell.
    std::atomic<uint32_t> active_voices;
    std::atomic<uint32_t> callbacks;
    std::atomic<uint32_t> xruns;
    std::atomic<float>    render_last_ms;
    std::atomic<float>    render_max_ms;
    std::atomic<uint32_t> device_rate;
    std::atomic<uint32_t> device_block;
    std::atomic<uint32_t> effective_mode;   // the dialect after the last block
    std::atomic<uint32_t> note_ons;         // the latency probe (step 48)
    std::atomic<double>   last_note_on_seconds;
    std::atomic<Sample*>  current_sample;   // written by the callback at the swap; read by voxo_stats
    // Callback-thread state (never touched by the shell while running).
    Voice    voices[MAX_VOICES_CAP];
    Channel  channels[16];
    uint32_t next_serial;
    double   clock_seconds;         // the normalizer's monotonic clock: rendered time
    double   block_start_seconds;   // set by the backend before each render, 0 without a device
    float    attack_coef, release_coef, press_coef;
    sumi_midi_event_t events[EVENTS_PER_BLOCK];
    // The preset (step 50): the shell's, never the callback's.
    voxo_ds::Instrument* instrument;
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

void sample_free(Sample* s) {
    if (s && s != &g_no_sample) std::free(s);   // one block: the struct and its frames
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
    vc.pos = 0.0;                          // a retrigger restarts the sample from its head
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
            } else if (e.a == 122) {               // Local Control: tracked for the shell (#12)
                v->local_control.store(e.b >= 64 ? 1u : 0u, std::memory_order_relaxed);
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

// 4-point, 3rd-order Hermite (Catmull-Rom tangents): the read between x0 and
// x1 at fraction t, with its neighbours xm1 and x2 shaping the curve.
inline float hermite(float xm1, float x0, float x1, float x2, float t) {
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
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
    // DECISIONS_6 #4 / #9: the table. macOS and iOS take 128 as asked; Android
    // runs AAudio's burst (192 on the Tab); Windows / Linux measured in 55.
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
    new (&v->interpolation) std::atomic<uint32_t>(0);
    new (&v->local_control) std::atomic<uint32_t>(1);
    new (&v->pending_sample) std::atomic<Sample*>(nullptr);
    new (&v->retired_sample) std::atomic<Sample*>(nullptr);
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
    new (&v->current_sample) std::atomic<Sample*>(nullptr);
    voxo_core_set_rate(v, v->sample_rate);
    return v;
}

void voxo_destroy(voxo_t* v) {
    if (!v) return;
    voxo_stop(v);
    // No callback runs now: every sample still around is ours to free.
    sample_free(v->pending_sample.exchange(nullptr, std::memory_order_acq_rel));
    sample_free(v->retired_sample.exchange(nullptr, std::memory_order_acq_rel));
    sample_free(v->current_sample.exchange(nullptr, std::memory_order_acq_rel));
    delete v->instrument;
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

bool voxo_set_sample(voxo_t* v, const float* frames, uint32_t frame_count,
                     uint32_t channels, uint32_t sample_rate, float root_note) {
    if (!v) return false;
    if (!frames || frame_count == 0) { voxo_clear_sample(v); return true; }
    if ((channels != 1 && channels != 2) || sample_rate < 1000 || sample_rate > 384000) return false;
    if (!(root_note >= 0.0f && root_note <= 127.0f)) return false;
    // The shell's thread frees what the callback retired last time.
    sample_free(v->retired_sample.exchange(nullptr, std::memory_order_acq_rel));
    const size_t padded = (size_t)(frame_count + PAD_BEFORE + PAD_AFTER) * channels;
    Sample* s = (Sample*)std::calloc(1, sizeof(Sample) + padded * sizeof(float));
    if (!s) return false;
    s->data = (float*)(s + 1);
    s->frames = frame_count; s->channels = channels; s->rate = sample_rate;
    s->root_note = root_note; s->root_hz = note_to_hz(root_note);
    std::memcpy(s->data + (size_t)PAD_BEFORE * channels, frames, (size_t)frame_count * channels * sizeof(float));
    // A pending sample the callback never saw is ours to drop.
    sample_free(v->pending_sample.exchange(s, std::memory_order_acq_rel));
    return true;
}

void voxo_clear_sample(voxo_t* v) {
    if (!v) return;
    sample_free(v->retired_sample.exchange(nullptr, std::memory_order_acq_rel));
    sample_free(v->pending_sample.exchange(&g_no_sample, std::memory_order_acq_rel));
}

namespace {
// Step 50's bridge to the step-49 player: the zone under middle C at velocity
// 100 (else the zone nearest to it) becomes THE sample, at its effective root
// (rootNote - tuning). Step 51 replaces this with the full dispatch.
bool publish_bridge_zone(voxo_t* v, const voxo_ds::Instrument& inst) {
    const voxo_ds::Zone* best = nullptr;
    int best_dist = 1 << 30;
    for (const voxo_ds::Group& g : inst.groups) {
        if (g.trigger == voxo_ds::Trigger::Release) continue;
        for (const voxo_ds::Zone& z : g.zones) {
            if (z.sample < 0 || z.trigger == voxo_ds::Trigger::Release) continue;
            const bool in_note = 60 >= z.lo_note && 60 <= z.hi_note;
            const bool in_vel = 100 >= z.lo_vel && 100 <= z.hi_vel;
            const int dist = (in_note ? 0 : (60 < z.lo_note ? z.lo_note - 60 : 60 - z.hi_note) * 4) + (in_vel ? 0 : 1);
            if (dist < best_dist) { best = &z; best_dist = dist; }
        }
    }
    if (!best) return false;
    const voxo_ds::SampleData& sd = inst.samples[(size_t)best->sample];
    return voxo_set_sample(v, sd.frames.data(), (uint32_t)(sd.frames.size() / sd.channels), sd.channels, sd.rate,
                           (float)best->root_note - best->tuning);
}
void fill_report(const voxo_ds::Instrument& inst, voxo_report_t* r) {
    std::memset(r, 0, sizeof(*r));
    r->ok = 1;
    r->groups = (uint32_t)inst.groups.size();
    r->zones = inst.zone_count;
    r->samples = (uint32_t)inst.samples.size();
    r->samples_missing = inst.missing;
    r->memory_bytes = inst.memory_bytes > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)inst.memory_bytes;
    r->notes = inst.notes;
    std::snprintf(r->name, sizeof(r->name), "%s", inst.name.c_str());
    std::snprintf(r->text, sizeof(r->text), "%s", inst.report.c_str());
}
} // namespace

bool voxo_load_preset(voxo_t* v, const char* path, voxo_report_t* report) {
    voxo_report_t local;
    voxo_report_t* r = report ? report : &local;
    std::memset(r, 0, sizeof(*r));
    if (!v || !path || !*path) { std::snprintf(r->text, sizeof(r->text), "%s", "no path"); return false; }
    voxo_ds::Instrument* inst = new (std::nothrow) voxo_ds::Instrument;
    if (!inst) { std::snprintf(r->text, sizeof(r->text), "%s", "out of memory"); return false; }
    std::string why;
    if (!voxo_ds::load(path, inst, &why)) {
        std::snprintf(r->text, sizeof(r->text), "%s", why.c_str());
        delete inst;
        return false;
    }
    delete v->instrument;
    v->instrument = inst;
    fill_report(*inst, r);
    if (!publish_bridge_zone(v, *inst)) voxo_clear_sample(v);   // every zone silent: the sine, and the report says why
    if (v->log_cb) v->log_cb(3, r->text, v->log_user);
    return true;
}

void voxo_unload_preset(voxo_t* v) {
    if (!v) return;
    delete v->instrument;
    v->instrument = nullptr;
    voxo_clear_sample(v);
}

const char* voxo_note_copy(uint32_t note) { return voxo_ds::note_copy(note); }

void voxo_set_interpolation(voxo_t* v, uint32_t mode) {
    if (v) v->interpolation.store(mode ? 1u : 0u, std::memory_order_relaxed);
}

void voxo_set_local_control(voxo_t* v, bool on) {
    if (v) v->local_control.store(on ? 1u : 0u, std::memory_order_relaxed);
}

/* The callback's whole body. */
void voxo_render(voxo_t* v, float* out_lr, uint32_t frames) {
    if (!out_lr || frames == 0) return;
    if (!v) { std::memset(out_lr, 0, sizeof(float) * 2u * frames); return; }

    // 1. The shell's settings, once per block — and the sample swap.
    const uint32_t pending = v->pending_mode.exchange(UINT32_MAX, std::memory_order_acq_rel);
    if (pending != UINT32_MAX) sumi_normalizer_set_mode(v->normalizer, (sumi_input_mode_t)pending);
    const float gain = v->gain.load(std::memory_order_relaxed);
    const bool linear = v->interpolation.load(std::memory_order_relaxed) != 0;
    if (Sample* next = v->pending_sample.exchange(nullptr, std::memory_order_acq_rel)) {
        Sample* cur = v->current_sample.exchange(next == &g_no_sample ? nullptr : next, std::memory_order_acq_rel);
        // The retired slot holds at most one: a second swap before the shell
        // freed the first would leak it, so voxo_set_sample frees the slot
        // first on the shell's thread and the callback only fills an empty one.
        if (cur) {
            Sample* stale = v->retired_sample.exchange(cur, std::memory_order_acq_rel);
            (void)stale;   // never non-null by the protocol above
        }
        for (uint32_t i = 0; i < v->max_voices; i++) v->voices[i].active = false;   // voices on the old sample end here
    }
    Sample* smp = v->current_sample.load(std::memory_order_relaxed);

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
    const uint32_t rate = v->device_rate.load(std::memory_order_relaxed);
    const double inv_rate = 1.0 / (double)rate;
    // The sample's read increment per output frame at 1 Hz of pitch: ratio = freq / root_hz * sample_rate / device_rate.
    const float inc_per_hz = smp ? (float)((double)smp->rate * inv_rate / (double)smp->root_hz) : 0.0f;
    uint32_t active = 0;
    for (uint32_t vi = 0; vi < v->max_voices; vi++) {
        Voice& vc = v->voices[vi];
        if (!vc.active) continue;
        float freq = vc.freq, level = vc.level, press = vc.pressure;
        const float env_coef = vc.releasing ? v->release_coef : v->attack_coef;
        // The block's pitch: the ratio recomputed here from the events just
        // applied, reached by a straight line across the block (SOUND §2) —
        // no step at the block boundary, so a bend sweep is a continuous
        // piecewise-linear trajectory (the one-pole toward a stepped target
        // left a ripple at the block rate: DECISIONS_6 #10's measurement).
        const float freq_step = (vc.freq_target - freq) / (float)frames;
        bool ended = false;
        if (smp) {
            double pos = vc.pos;
            const float* d = smp->data + (size_t)PAD_BEFORE * smp->channels;   // frame 0
            const uint32_t chn = smp->channels;
            const double end = (double)smp->frames;
            for (uint32_t f = 0; f < frames; f++) {
                freq  += freq_step;
                level += (vc.level_target - level) * env_coef;
                press += (vc.pressure_target - press) * v->press_coef;
                if (pos >= end) { ended = true; break; }
                const float g = SAMPLE_AMPLITUDE * vc.vel_gain * level * (1.0f - press_mix + press_mix * press);
                const uint32_t i0 = (uint32_t)pos;
                const float t = (float)(pos - (double)i0);
                const float* p = d + (size_t)i0 * chn;
                float l, r;
                if (linear) {
                    l = p[0] + (p[chn] - p[0]) * t;
                    r = chn == 2 ? p[1] + (p[3] - p[1]) * t : l;
                } else {
                    l = hermite(p[-(int)chn], p[0], p[chn], p[2 * chn], t);
                    r = chn == 2 ? hermite(p[-1], p[1], p[3], p[5], t) : l;
                }
                out_lr[2u * f]      += l * g;
                out_lr[2u * f + 1u] += r * g;
                pos += (double)(freq * inc_per_hz);
            }
            vc.pos = pos;
        } else {
            double phase = vc.phase;
            for (uint32_t f = 0; f < frames; f++) {
                freq  += freq_step;
                level += (vc.level_target - level) * env_coef;
                press += (vc.pressure_target - press) * v->press_coef;
                const float g = SINE_AMPLITUDE * vc.vel_gain * level * (1.0f - press_mix + press_mix * press);
                const float s = (float)std::sin(phase) * g;
                out_lr[2u * f]      += s;
                out_lr[2u * f + 1u] += s;
                phase += two_pi * (double)freq * inv_rate;
                if (phase >= two_pi) phase -= two_pi;
            }
            vc.phase = phase;
        }
        vc.freq = vc.freq_target; vc.level = level; vc.pressure = press;   // the line lands exactly on the target
        if (ended || (vc.releasing && level < 1e-4f)) { vc.active = false; continue; }
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
    out->local_control  = v->local_control.load(std::memory_order_relaxed);
    out->interpolation  = v->interpolation.load(std::memory_order_relaxed);
    if (const Sample* s = v->current_sample.load(std::memory_order_acquire)) {
        out->sample_frames = s->frames; out->sample_channels = s->channels;
        out->sample_rate_hz = s->rate; out->sample_root_note = s->root_note;
    }
    if (v->instrument) { out->preset_loaded = 1; out->preset_zones = v->instrument->zone_count; }
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
