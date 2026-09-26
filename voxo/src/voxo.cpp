// voxo.cpp — Voxo's core (Phase 7, SOUND §1–§3; DECISIONS_6 #2–#5, #10–#18):
// the second SPSC ring is the core's own MIDI normalizer compiled from source
// (one MPE decoder, two builds, zero runtime coupling); a fixed pool of
// PERFORMANCE voices keyed by (channel, note), each stacking the compiled
// instrument's zones as LAYERS — velocity layers with crossfades, round
// robins, release samples — every layer a sample player (the ratio
// 2^((note - root + bend)/12) recomputed per block and ramped per sample,
// 4-point Hermite, the zone's loop with an equal-power crossfade), an ADSR,
// a low-pass filter; the voice's MPE sources (pressure, timbre = CC 74, swirl
// = poly pressure) smoothed by the preset's rising/falling times and mapped
// through the preset's bindings or the defaults. With no instrument, a sine.
// voxo_render is the callback's whole body and honours the contract at the
// top of voxo.h: no allocation, no lock, no logging, no blocking call; every
// voice transition happens at block start, in event order, before a sample
// is written. The shell's settings — and the instrument itself — arrive
// through atomics read at block start.
#include "voxo.h"
#include "voxo_internal.h"
#include "midi_normalizer.h"   // core/src — compiled into this library, never linked from libsumi
#include "ds_preset.h"         // step 50: the Decent Sampler front end (shell-thread only)
#include "instrument.h"        // step 51: the compiled instrument the callback plays
#include "bus.h"               // step 52: the reverb and the delay after the sum

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#define VOXO_VERSION_MAJOR 0
#define VOXO_VERSION_MINOR 7
#define VOXO_VERSION_PATCH 0

using voxo_inst::Instrument;
using voxo_inst::Zone;
using voxo_inst::Group;
using voxo_inst::GroupState;
using voxo_inst::Target;

namespace {

constexpr uint32_t MAX_VOICES_CAP = 64;
constexpr uint32_t MAX_LAYERS     = 8;       // zones a performance voice stacks at once
constexpr uint32_t EVENTS_PER_BLOCK = 512;   // drained at block start; the ring keeps the rest for the next block
constexpr float    SINE_AMPLITUDE   = 0.18f; // one sine at full level; 16 of them soft-clip, never wrap
constexpr float    SAMPLE_AMPLITUDE = 0.40f; // one sample layer at full level
constexpr float    PI_F = 3.14159265358979f;

Instrument g_none;   // the sentinel "clear" request

enum : uint8_t { ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE, ENV_DONE };

struct Layer {
    bool     active;
    const Zone* zone;
    double   pos;              // the read position in the sample, frames
    uint8_t  stage;            // ENV_*
    float    env;              // 0..1
    float    attack_step;      // per sample, in attack
    float    decay_coef, release_coef;   // one-pole toward sustain / 0
    float    sustain;
    float    gain;             // zone gain × velocity gain × crossfade × SAMPLE_AMPLITUDE
    float    pan_l, pan_r;
    float    vel_cutoff_mul;   // the <velocity> binding's cutoff factor
    float    ic1l, ic2l, ic1r, ic2r;   // the SVF's state
    float    g_prev;           // the filter coefficient at the last block's end (for the ramp)
};

struct Voice {
    bool     active;
    bool     held;             // key down (note-off not yet received)
    bool     pedalled;         // note-off received while CC64 was down: releases with the pedal
    bool     releasing;        // the layers are in release (the sine: the level falls)
    uint8_t  channel, note, velocity;
    uint32_t serial;           // allocation order, for stealing the oldest
    double   phase;            // the sine's radians
    float    freq, freq_target;
    float    level, level_target;      // the sine's envelope
    float    vel_gain;                 // the sine's velocity^1.5
    float    pressure, pressure_target;
    float    timbre, timbre_target;    // CC 74, 0..1 (0.5 = centre)
    float    swirl, swirl_target;      // poly pressure, 0..1
    Layer    layers[MAX_LAYERS];
};

struct Channel {
    float bend_semitones;
    float pressure;        // last channel pressure 0..1
    float timbre;          // last CC 74 / 127
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
    std::atomic<Instrument*> pending_inst;   // the next instrument (or &g_none = clear); the callback takes it
    std::atomic<Instrument*> retired_inst;   // what the callback let go; the shell frees it
    // Callback -> shell.
    std::atomic<uint32_t> active_voices;
    std::atomic<uint32_t> active_layers;
    std::atomic<uint32_t> callbacks;
    std::atomic<uint32_t> xruns;
    std::atomic<float>    render_last_ms;
    std::atomic<float>    render_max_ms;
    std::atomic<uint32_t> device_rate;
    std::atomic<uint32_t> device_block;
    std::atomic<uint32_t> effective_mode;   // the dialect after the last block
    std::atomic<uint32_t> note_ons;         // the latency probe (step 48)
    std::atomic<double>   last_note_on_seconds;
    std::atomic<Instrument*> current_inst;  // written by the callback at the swap; read by voxo_stats
    // Callback-thread state (never touched by the shell while running).
    Voice    voices[MAX_VOICES_CAP];
    Channel  channels[16];
    // MPE's zone (DECISIONS_6 #25): the master channel's bend, sustain, pressure
    // and slide apply to every member; read from the normalizer at block start.
    sumi_mpe_zone_t zone;
    bool     zone_live;              // the dialect is MPE or wind: the zone's semantics apply
    float    master_bend;            // semitones, the master channel's, on top of a member's own
    uint32_t next_serial;
    double   clock_seconds;         // the normalizer's monotonic clock: rendered time
    double   block_start_seconds;   // set by the backend before each render, 0 without a device
    float    attack_coef, release_coef, press_coef;   // the sine's
    sumi_midi_event_t events[EVENTS_PER_BLOCK];
    // The preset's model (shell-thread only): the compiled instrument owns it once published.
    uint32_t preset_zones;
    bool     preset_loaded;
    uint64_t memory_budget;          // the shell's advice for the gate (step 52)
    // The shell's CC map into the bus (step 53, #27): double-buffered, the
    // shell writes the idle table and flips; the callback reads the live one.
    struct CcRoute { uint8_t channel, cc; uint32_t target; };
    CcRoute  cc_routes[2][32];
    uint32_t cc_route_count[2];
    std::atomic<uint32_t> cc_live;   // which table the callback reads (0 / 1)
    // The bus (step 52): buffers allocated when the rate is known (shell thread), owned by the callback while running.
    voxo_bus::Reverb* reverb;
    voxo_bus::Delay*  delay;
    uint32_t bus_rate;               // the rate the bus was allocated for
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
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

void inst_free(Instrument* i) { if (i && i != &g_none) delete i; }

inline bool is_member(const voxo_t* v, uint8_t ch) {
    return v->zone_live && v->zone.member_count > 0 && ch >= v->zone.first_member &&
           ch < (uint8_t)(v->zone.first_member + v->zone.member_count);
}
inline bool is_master(const voxo_t* v, uint8_t ch) { return v->zone_live && ch == v->zone.master; }

void voice_retune(voxo_t* v, Voice& vc) {
    const float zone_bend = is_member(v, vc.channel) ? v->master_bend : 0.0f;
    vc.freq_target = note_to_hz((float)vc.note + v->channels[vc.channel & 15].bend_semitones + zone_bend);
}

// A master-channel message under MPE reaches every member channel's state and voices.
template <typename F> void for_zone_channels(voxo_t* v, uint8_t ch, F fn) {
    if (is_master(v, ch)) {
        fn(ch);
        for (uint8_t c = v->zone.first_member; c < (uint8_t)(v->zone.first_member + v->zone.member_count) && c < 16; c++) fn(c);
    } else {
        fn(ch);
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
// One stereo read at `pos` (fractional frames) — Hermite or linear.
inline void read_frame(const Zone& z, double pos, bool linear, float* l, float* r) {
    const uint32_t i0 = (uint32_t)pos;
    const float t = (float)(pos - (double)i0);
    const uint32_t chn = z.channels;
    const float* p = z.data + (size_t)i0 * chn;
    if (linear) {
        *l = p[0] + (p[chn] - p[0]) * t;
        *r = chn == 2 ? p[1] + (p[3] - p[1]) * t : *l;
    } else {
        *l = hermite(p[-(int)chn], p[0], p[chn], p[2 * chn], t);
        *r = chn == 2 ? hermite(p[-1], p[1], p[3], p[5], t) : *l;
    }
}

uint32_t rng_next(uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }

// The envelope's shape (DECISIONS_6 #16): a linear attack (2 ms at least), a
// decay and a release as one-poles that land within 1% of their target in the
// preset's time (5 ms at least for the release, so a note-off never clicks).
void layer_envelope_coefs(Layer& L, const GroupState& st, uint32_t rate) {
    const float attack = st.attack < 0.002f ? 0.002f : st.attack;
    L.attack_step = 1.0f / (attack * (float)rate);
    L.decay_coef = st.decay <= 0.0f ? 1.0f : 1.0f - std::exp(-4.6f / (st.decay * (float)rate));
    const float release = st.release < 0.005f ? 0.005f : st.release;
    L.release_coef = 1.0f - std::exp(-4.6f / (release * (float)rate));
    L.sustain = st.sustain;
}

// Starts one zone as a layer of the voice: the velocity gain, the crossfade,
// the pan, the envelope — from the group's live state.
void layer_start(voxo_t* v, Voice& vc, Layer& L, const Zone& z, const Group& g, const GroupState& st, uint8_t velocity) {
    L.active = true;
    L.zone = &z;
    L.pos = (double)z.start;
    L.stage = ENV_ATTACK; L.env = 0.0f;
    layer_envelope_coefs(L, st, v->device_rate.load(std::memory_order_relaxed));
    const float vn = (float)velocity / 127.0f;
    float gain = z.gain * (1.0f - g.amp_vel_track + g.amp_vel_track * vn);
    // The crossfade at the zone's velocity edges (equal power).
    if (z.fade_lo > 0.0f && velocity < z.lo_vel + z.fade_lo) {
        const float t = ((float)velocity - (float)z.lo_vel + 0.5f) / z.fade_lo;
        gain *= std::sin(clampf(t, 0.0f, 1.0f) * PI_F * 0.5f);
    }
    if (z.fade_hi > 0.0f && velocity > z.hi_vel - z.fade_hi) {
        const float t = ((float)z.hi_vel - (float)velocity + 0.5f) / z.fade_hi;
        gain *= std::sin(clampf(t, 0.0f, 1.0f) * PI_F * 0.5f);
    }
    L.gain = SAMPLE_AMPLITUDE * gain;
    const float a = (z.pan + 1.0f) * PI_F * 0.25f;
    L.pan_l = std::cos(a); L.pan_r = std::sin(a);
    L.vel_cutoff_mul = g.velocity_to_cutoff.target == Target::FilterCutoff ? g.velocity_to_cutoff.at(vn) : 1.0f;
    L.ic1l = L.ic2l = L.ic1r = L.ic2r = 0.0f;
    L.g_prev = -1.0f;
    (void)vc;
}

Layer* layer_alloc(Voice& vc) {
    for (uint32_t i = 0; i < MAX_LAYERS; i++) if (!vc.layers[i].active) return &vc.layers[i];
    return nullptr;
}

// The dispatch (DECISIONS_6 #16): for every group, the zones matching the note
// and velocity; the sequence mode picks among their positions.
void voice_dispatch(voxo_t* v, Voice& vc, Instrument& inst, bool release_trigger, uint8_t velocity) {
    const uint32_t ngroups = (uint32_t)inst.groups.size();
    for (uint32_t gi = 0; gi < ngroups; gi++) {
        const Group& g = inst.groups[gi];
        GroupState& st = inst.state[gi];
        // The candidates' positions present for this note.
        uint32_t present_mask = 0;   // positions 1..32
        bool any = false;
        for (const Zone& z : inst.zones) {
            if (z.group != gi || z.release_trigger != release_trigger) continue;
            if (vc.note < z.lo_note || vc.note > z.hi_note || velocity < z.lo_vel || velocity > z.hi_vel) continue;
            any = true;
            if (z.seq_position >= 1 && z.seq_position <= 32) present_mask |= 1u << (z.seq_position - 1);
        }
        if (!any) continue;
        uint32_t chosen = 0;   // 0 = every candidate
        if (g.seq_mode == voxo_inst::SeqMode::RoundRobin && present_mask) {
            const uint32_t len = g.seq_length ? g.seq_length : 32;
            for (uint32_t tries = 0; tries < 32; tries++) {
                const uint32_t pos = ((st.rr_counter - 1) % len) + 1;
                st.rr_counter = pos + 1;
                if (pos <= 32 && (present_mask & (1u << (pos - 1)))) { chosen = pos; break; }
            }
        } else if (g.seq_mode == voxo_inst::SeqMode::Random && present_mask) {
            uint32_t count = 0;
            for (uint32_t b = 0; b < 32; b++) if (present_mask & (1u << b)) count++;
            uint32_t pick = rng_next(st.rng) % count;
            for (uint32_t b = 0; b < 32; b++) if (present_mask & (1u << b)) { if (pick-- == 0) { chosen = b + 1; break; } }
        }
        for (const Zone& z : inst.zones) {
            if (z.group != gi || z.release_trigger != release_trigger) continue;
            if (vc.note < z.lo_note || vc.note > z.hi_note || velocity < z.lo_vel || velocity > z.hi_vel) continue;
            if (chosen && z.seq_position != chosen) continue;
            Layer* L = layer_alloc(vc);
            if (!L) return;
            layer_start(v, vc, *L, z, g, st, velocity);
        }
    }
}

void voice_start(voxo_t* v, Voice& vc, Instrument* inst, uint8_t channel, uint8_t note, uint8_t velocity, bool fresh) {
    vc.active = true; vc.releasing = false; vc.held = true; vc.pedalled = false;
    vc.channel = channel; vc.note = note; vc.velocity = velocity;
    vc.serial = v->next_serial++;
    const float vn = (float)velocity / 127.0f;
    vc.vel_gain = vn * std::sqrt(vn);
    vc.level_target = 1.0f;
    const Channel& ch = v->channels[channel & 15];
    vc.pressure_target = ch.pressure;
    vc.timbre_target = ch.timbre;
    vc.swirl_target = 0.0f;
    if (fresh) { vc.phase = 0.0; vc.level = 0.0f; vc.pressure = vc.pressure_target; vc.timbre = vc.timbre_target; vc.swirl = 0.0f; }
    voice_retune(v, vc);
    if (fresh) vc.freq = vc.freq_target;   // a retrigger glides from where it was
    // A retrigger restarts the stack: the old layers release quickly, the new ones start.
    for (uint32_t i = 0; i < MAX_LAYERS; i++) if (vc.layers[i].active) { vc.layers[i].stage = ENV_RELEASE; vc.layers[i].release_coef = 0.02f; }
    if (inst) voice_dispatch(v, vc, *inst, false, velocity);
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
    if (best) for (uint32_t i = 0; i < MAX_LAYERS; i++) best->layers[i].active = false;
    return best;
}

void voice_release(voxo_t* v, Voice& vc, Instrument* inst) {
    vc.releasing = true; vc.held = false; vc.pedalled = false; vc.level_target = 0.0f;
    for (uint32_t i = 0; i < MAX_LAYERS; i++) {
        Layer& L = vc.layers[i];
        if (L.active && L.stage != ENV_RELEASE && !L.zone->release_trigger) L.stage = ENV_RELEASE;
    }
    if (inst) voice_dispatch(v, vc, *inst, true, vc.velocity);   // the release samples
}

void apply_cc_bindings(Instrument& inst, uint8_t cc, uint8_t value) {
    const float in = (float)value / 127.0f;
    for (const voxo_inst::CcBinding& cb : inst.cc_bindings) {
        if (cb.cc != cc) continue;
        const float out = cb.curve.at(in);
        if (cb.curve.target >= Target::ReverbWet) {   // the bus (step 52): the live copy the callback owns
            voxo_bus::Params& b = inst.bus;
            switch (cb.curve.target) {
                case Target::ReverbWet: b.reverb_wet = clampf(out, 0.0f, 1.0f); break;
                case Target::ReverbRoom: b.room_size = clampf(out, 0.0f, 1.0f); break;
                case Target::ReverbDamping: b.damping = clampf(out, 0.0f, 1.0f); break;
                case Target::DelayWet: b.delay_wet = clampf(out, 0.0f, 1.0f); break;
                case Target::DelayTime: b.delay_time = clampf(out, 0.001f, 2.0f); break;
                case Target::DelayFeedback: b.feedback = clampf(out, 0.0f, 0.95f); break;
                default: break;
            }
            continue;
        }
        const uint32_t lo = cb.curve.group < 0 ? 0 : (uint32_t)cb.curve.group;
        const uint32_t hi = cb.curve.group < 0 ? (uint32_t)inst.groups.size() : lo + 1;
        for (uint32_t gi = lo; gi < hi && gi < inst.groups.size(); gi++) {
            GroupState& st = inst.state[gi];
            switch (cb.curve.target) {
                case Target::AmpVolume: st.gain = clampf(out, 0.0f, 2.0f); break;
                case Target::FilterCutoff: st.cutoff = clampf(out, 20.0f, 22000.0f); break;
                case Target::FilterResonance: st.resonance = clampf(out - 0.7f, 0.0f, 1.0f); break;
                case Target::EnvAttack: st.attack = out < 0.0f ? 0.0f : out; break;
                case Target::EnvDecay: st.decay = out < 0.0f ? 0.0f : out; break;
                case Target::EnvSustain: st.sustain = clampf(out, 0.0f, 1.0f); break;
                case Target::EnvRelease: st.release = out < 0.0f ? 0.0f : out; break;
                default: break;
            }
        }
    }
}

// The shell's routes into the bus (step 53, #27): the live table, the CC's
// value scaled into the target's range, the effect switched on if it was off.
void apply_shell_routes(voxo_t* v, Instrument& inst, uint8_t channel, uint8_t cc, uint8_t value) {
    const uint32_t live = v->cc_live.load(std::memory_order_acquire);
    const uint32_t n = v->cc_route_count[live];
    const float in = (float)value / 127.0f;
    for (uint32_t i = 0; i < n; i++) {
        const voxo_t::CcRoute& r = v->cc_routes[live][i];
        if (r.cc != cc || (r.channel != 0xFF && r.channel != channel)) continue;
        voxo_bus::Params& b = inst.bus;
        switch (r.target) {
            case VOXO_CTL_REVERB_WET:     b.reverb_on = true; b.reverb_wet = in; break;
            case VOXO_CTL_REVERB_ROOM:    b.reverb_on = true; b.room_size = in; break;
            case VOXO_CTL_REVERB_DAMPING: b.reverb_on = true; b.damping = in; break;
            case VOXO_CTL_DELAY_WET:      b.delay_on = true; b.delay_wet = in; break;
            case VOXO_CTL_DELAY_TIME:     b.delay_on = true; b.delay_time = 0.05f + 0.95f * in; break;
            case VOXO_CTL_DELAY_FEEDBACK: b.delay_on = true; b.feedback = 0.9f * in; break;
            default: break;
        }
    }
}

void apply_event(voxo_t* v, Instrument* inst, const sumi_midi_event_t& e) {
    Channel& ch = v->channels[e.channel & 15];
    switch (e.kind) {
        case SUMI_MEV_NOTE_ON: {
            v->note_ons.fetch_add(1, std::memory_order_relaxed);
            v->last_note_on_seconds.store(v->block_start_seconds, std::memory_order_relaxed);
            if (Voice* vc = voice_find(v, e.channel, e.a)) { voice_start(v, *vc, inst, e.channel, e.a, e.b, false); break; }
            if (Voice* vc = voice_alloc(v)) voice_start(v, *vc, inst, e.channel, e.a, e.b, !vc->active);
            break;
        }
        case SUMI_MEV_NOTE_OFF: {
            if (Voice* vc = voice_find(v, e.channel, e.a)) {
                if (!vc->held) break;
                if (ch.sustain) { vc->held = false; vc->pedalled = true; }
                else voice_release(v, *vc, inst);
            }
            break;
        }
        case SUMI_MEV_BEND:
            if (is_master(v, e.channel)) {
                // The master's bend (the strip's wheel, a keyboard sharing the
                // zone): on top of every member's own; the master's own voices
                // take it as theirs.
                v->master_bend = e.f;
                ch.bend_semitones = e.f;
                for (uint32_t i = 0; i < v->max_voices; i++) if (v->voices[i].active) voice_retune(v, v->voices[i]);
            } else {
                ch.bend_semitones = e.f;
                for (uint32_t i = 0; i < v->max_voices; i++) {
                    Voice& vc = v->voices[i];
                    if (vc.active && vc.channel == e.channel) voice_retune(v, vc);
                }
            }
            break;
        case SUMI_MEV_CHANNEL_PRESSURE: {
            const float p = (float)e.b / 127.0f;
            for_zone_channels(v, e.channel, [&](uint8_t c) {
                v->channels[c & 15].pressure = p;
                for (uint32_t i = 0; i < v->max_voices; i++) {
                    Voice& vc = v->voices[i];
                    if (vc.active && vc.channel == c) vc.pressure_target = p;
                }
            });
            break;
        }
        case SUMI_MEV_POLY_PRESSURE:                 // the swirl (0xA0): a per-note source, no default target
            if (Voice* vc = voice_find(v, e.channel, e.a)) vc->swirl_target = (float)e.b / 127.0f;
            break;
        case SUMI_MEV_CC:
            if (e.a == 74) {                        // the timbre: MPE's slide, per channel (the master's reaches the zone)
                const float t = (float)e.b / 127.0f;
                for_zone_channels(v, e.channel, [&](uint8_t c) {
                    v->channels[c & 15].timbre = t;
                    for (uint32_t i = 0; i < v->max_voices; i++) {
                        Voice& vc = v->voices[i];
                        if (vc.active && vc.channel == c) vc.timbre_target = t;
                    }
                });
            }
            if (e.a == 64) {                        // sustain: the release logic (SOUND §2); the master's pedal holds the zone
                const bool down = e.b >= 64;
                for_zone_channels(v, e.channel, [&](uint8_t c) {
                    Channel& cc = v->channels[c & 15];
                    if (!down && cc.sustain) {
                        for (uint32_t i = 0; i < v->max_voices; i++) {
                            Voice& vc = v->voices[i];
                            if (vc.active && vc.channel == c && vc.pedalled) voice_release(v, vc, inst);
                        }
                    }
                    cc.sustain = down;
                });
            } else if (e.a == 122) {                // Local Control: tracked for the shell (#12)
                v->local_control.store(e.b >= 64 ? 1u : 0u, std::memory_order_relaxed);
            } else if (e.a == 123 || e.a == 120) {  // all notes off / all sound off: the panic
                for (uint32_t i = 0; i < v->max_voices; i++) {
                    Voice& vc = v->voices[i];
                    if (!vc.active || vc.channel != e.channel) continue;
                    if (e.a == 120) { vc.active = false; vc.level = 0.0f; for (uint32_t k = 0; k < MAX_LAYERS; k++) vc.layers[k].active = false; }
                    else voice_release(v, vc, nullptr);   // no release samples on a panic
                }
                ch.sustain = false;
            }
            if (inst) apply_cc_bindings(*inst, e.a, e.b);
            if (inst) apply_shell_routes(v, *inst, e.channel, e.a, e.b);
            break;
        default: break;
    }
}

// One layer's block: the envelope, the read with the loop, the filter, into the mix.
// Returns false when the layer ended.
bool render_layer(voxo_t* v, Voice& vc, Layer& L, const Group& g, const GroupState& st, float* out_lr, uint32_t frames,
                  float freq_from, float freq_step, float press_gain_from, float press_gain_step, float cutoff_from, float cutoff_to,
                  bool linear, uint32_t rate) {
    const Zone& z = *L.zone;
    const float inc_per_hz = (float)((double)z.rate / (double)rate / (double)z.root_hz);
    const double end = (double)z.end;
    const bool loop = z.loop;
    const double loop_len = loop ? (double)(z.loop_end - z.loop_start) : 0.0;
    const double xf_start = loop ? (double)(z.loop_end - z.loop_xfade) : 0.0;
    const float layer_gain = L.gain * st.gain;
    // The filter: the coefficient at the block's start and end (a tan each), ramped per sample.
    const float fc0 = clampf(cutoff_from * L.vel_cutoff_mul, 20.0f, 0.45f * (float)rate);
    const float fc1 = clampf(cutoff_to * L.vel_cutoff_mul, 20.0f, 0.45f * (float)rate);
    const bool filter = g.has_filter && (fc0 < 19000.0f || fc1 < 19000.0f || st.resonance > 0.01f);
    float gc = L.g_prev >= 0.0f ? L.g_prev : std::tan(PI_F * fc0 / (float)rate);
    const float g1 = std::tan(PI_F * fc1 / (float)rate);
    const float g_step = (g1 - gc) / (float)frames;
    const float k = 2.0f - 1.9f * clampf(st.resonance, 0.0f, 1.0f);
    float freq = freq_from, pg = press_gain_from;
    double pos = L.pos;
    float env = L.env;
    uint8_t stage = L.stage;
    float ic1l = L.ic1l, ic2l = L.ic2l, ic1r = L.ic1r, ic2r = L.ic2r;
    for (uint32_t f = 0; f < frames; f++) {
        freq += freq_step; pg += press_gain_step;
        // The envelope.
        switch (stage) {
            case ENV_ATTACK: env += L.attack_step; if (env >= 1.0f) { env = 1.0f; stage = L.decay_coef >= 1.0f ? ENV_SUSTAIN : ENV_DECAY; } break;
            case ENV_DECAY: env += (L.sustain - env) * L.decay_coef; if (env - L.sustain < 1e-4f) stage = ENV_SUSTAIN; break;
            case ENV_SUSTAIN: env = L.sustain; break;
            default: env += (0.0f - env) * L.release_coef; if (env < 1e-4f) { L.active = false; L.stage = ENV_DONE; return false; }
        }
        // The read, with the loop's crossfade.
        if (pos >= end && !loop) { L.active = false; L.stage = ENV_DONE; return false; }
        float sl, sr;
        if (loop && pos >= xf_start && z.loop_xfade > 0) {
            const float t = (float)((pos - xf_start) / (double)z.loop_xfade);
            float al, ar, bl, br;
            read_frame(z, pos, linear, &al, &ar);
            read_frame(z, pos - loop_len, linear, &bl, &br);
            const float wa = std::cos(t * PI_F * 0.5f), wb = std::sin(t * PI_F * 0.5f);
            sl = al * wa + bl * wb; sr = ar * wa + br * wb;
        } else {
            read_frame(z, pos, linear, &sl, &sr);
        }
        // The filter.
        if (filter) {
            gc += g_step;
            const float a1 = 1.0f / (1.0f + gc * (gc + k)), a2 = gc * a1, a3 = gc * a2;
            float v3 = sl - ic2l, v1 = a1 * ic1l + a2 * v3, v2 = ic2l + a2 * ic1l + a3 * v3;
            ic1l = 2.0f * v1 - ic1l; ic2l = 2.0f * v2 - ic2l; sl = v2;
            v3 = sr - ic2r; v1 = a1 * ic1r + a2 * v3; v2 = ic2r + a2 * ic1r + a3 * v3;
            ic1r = 2.0f * v1 - ic1r; ic2r = 2.0f * v2 - ic2r; sr = v2;
        }
        const float gg = layer_gain * env * pg;
        out_lr[2u * f]      += sl * gg * L.pan_l;
        out_lr[2u * f + 1u] += sr * gg * L.pan_r;
        pos += (double)(freq * inc_per_hz);
        if (loop && pos >= (double)z.loop_end) pos -= loop_len;
    }
    L.pos = pos; L.env = env; L.stage = stage;
    L.ic1l = ic1l; L.ic2l = ic2l; L.ic1r = ic1r; L.ic2r = ic2r;
    L.g_prev = filter ? g1 : -1.0f;
    (void)vc; (void)v;
    return true;
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
    new (&v->pending_inst) std::atomic<Instrument*>(nullptr);
    new (&v->retired_inst) std::atomic<Instrument*>(nullptr);
    new (&v->active_voices) std::atomic<uint32_t>(0);
    new (&v->active_layers) std::atomic<uint32_t>(0);
    new (&v->callbacks) std::atomic<uint32_t>(0);
    new (&v->xruns) std::atomic<uint32_t>(0);
    new (&v->render_last_ms) std::atomic<float>(0.0f);
    new (&v->render_max_ms) std::atomic<float>(0.0f);
    new (&v->device_rate) std::atomic<uint32_t>(v->sample_rate);
    new (&v->device_block) std::atomic<uint32_t>(v->block_frames);
    new (&v->effective_mode) std::atomic<uint32_t>((uint32_t)SUMI_INPUT_AUTO);
    new (&v->note_ons) std::atomic<uint32_t>(0);
    new (&v->last_note_on_seconds) std::atomic<double>(0.0);
    new (&v->current_inst) std::atomic<Instrument*>(nullptr);
    new (&v->cc_live) std::atomic<uint32_t>(0);
    for (int c2 = 0; c2 < 16; c2++) v->channels[c2].timbre = 0.5f;   // the slide at its centre until CC 74 says
    v->reverb = new (std::nothrow) voxo_bus::Reverb;
    v->delay = new (std::nothrow) voxo_bus::Delay;
    if (!v->reverb || !v->delay) { delete v->reverb; delete v->delay; sumi_normalizer_destroy(v->normalizer); std::free(v); return nullptr; }
    voxo_core_set_rate(v, v->sample_rate);
    return v;
}

void voxo_destroy(voxo_t* v) {
    if (!v) return;
    voxo_stop(v);
    // No callback runs now: every instrument still around is ours to free.
    inst_free(v->pending_inst.exchange(nullptr, std::memory_order_acq_rel));
    inst_free(v->retired_inst.exchange(nullptr, std::memory_order_acq_rel));
    inst_free(v->current_inst.exchange(nullptr, std::memory_order_acq_rel));
    delete v->reverb;
    delete v->delay;
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
    for (uint32_t i = 0; i < MAX_VOICES_CAP; i++) { v->voices[i].active = false; for (uint32_t k = 0; k < MAX_LAYERS; k++) v->voices[i].layers[k].active = false; }
    for (int c = 0; c < 16; c++) v->channels[c].sustain = false;
    v->master_bend = 0.0f;
    v->active_voices.store(0, std::memory_order_relaxed);
    v->active_layers.store(0, std::memory_order_relaxed);
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

namespace {
// Publishes an instrument to the callback (DECISIONS_6 #11's protocol).
void publish(voxo_t* v, Instrument* inst) {
    inst_free(v->retired_inst.exchange(nullptr, std::memory_order_acq_rel));   // the shell frees what the callback retired
    inst_free(v->pending_inst.exchange(inst, std::memory_order_acq_rel));      // a pending one the callback never saw is ours to drop
}
} // namespace

bool voxo_set_sample(voxo_t* v, const float* frames, uint32_t frame_count,
                     uint32_t channels, uint32_t sample_rate, float root_note) {
    if (!v) return false;
    if (!frames || frame_count == 0) { voxo_clear_sample(v); return true; }
    if ((channels != 1 && channels != 2) || sample_rate < 1000 || sample_rate > 384000) return false;
    if (!(root_note >= 0.0f && root_note <= 127.0f)) return false;
    Instrument* inst = voxo_inst::compile_sample(frames, frame_count, channels, sample_rate, root_note);
    if (!inst) return false;
    v->preset_loaded = false; v->preset_zones = 0;
    publish(v, inst);
    return true;
}

void voxo_clear_sample(voxo_t* v) {
    if (!v) return;
    v->preset_loaded = false; v->preset_zones = 0;
    publish(v, &g_none);
}

namespace {
void fill_report(const voxo_ds::Instrument& inst, voxo_report_t* r) {
    std::memset(r, 0, sizeof(*r));
    r->ok = 1;
    r->groups = (uint32_t)inst.groups.size();
    r->zones = inst.zone_count;
    r->samples = (uint32_t)inst.samples.size();
    r->samples_missing = inst.missing;
    r->memory_bytes = inst.memory_bytes > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)inst.memory_bytes;
    r->memory_estimate = inst.memory_estimate > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)inst.memory_estimate;
    r->memory_budget = inst.memory_budget > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)inst.memory_budget;
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
    voxo_ds::Instrument* model = new (std::nothrow) voxo_ds::Instrument;
    if (!model) { std::snprintf(r->text, sizeof(r->text), "%s", "out of memory"); return false; }
    std::string why;
    if (!voxo_ds::load(path, model, &why, v->memory_budget)) {
        std::snprintf(r->text, sizeof(r->text), "%s", why.c_str());
        delete model;
        return false;
    }
    fill_report(*model, r);
    Instrument* inst = voxo_inst::compile(model);   // owns the model from here
    v->preset_loaded = true; v->preset_zones = model->zone_count;
    publish(v, inst);
    if (v->log_cb) v->log_cb(3, r->text, v->log_user);
    return true;
}

void voxo_unload_preset(voxo_t* v) {
    if (!v) return;
    voxo_clear_sample(v);
}

const char* voxo_note_copy(uint32_t note) { return voxo_ds::note_copy(note); }

void voxo_set_interpolation(voxo_t* v, uint32_t mode) {
    if (v) v->interpolation.store(mode ? 1u : 0u, std::memory_order_relaxed);
}

void voxo_set_memory_budget(voxo_t* v, uint64_t bytes) {
    if (v) v->memory_budget = bytes;
}

bool voxo_covered_notes(const voxo_t* v, uint8_t mask[16]) {
    if (!v || !mask) return false;
    // The newest instrument the shell handed over: pending (not yet swapped) before current.
    const Instrument* i = v->pending_inst.load(std::memory_order_acquire);
    if (i == &g_none) return false;
    if (!i) i = v->current_inst.load(std::memory_order_acquire);
    if (!i || !i->model) return false;
    std::memcpy(mask, i->note_mask, 16);
    return true;
}

void voxo_map_cc(voxo_t* v, uint8_t channel, uint8_t cc, uint32_t target) {
    if (!v || cc > 127 || target < VOXO_CTL_REVERB_WET || target >= VOXO_CTL_REVERB_WET + VOXO_CTL_COUNT) return;
    const uint32_t live = v->cc_live.load(std::memory_order_acquire), idle = live ^ 1u;
    const uint32_t n = v->cc_route_count[live];
    if (n >= 32) return;
    std::memcpy(v->cc_routes[idle], v->cc_routes[live], sizeof(v->cc_routes[live]));
    v->cc_routes[idle][n] = voxo_t::CcRoute{channel, cc, target};
    v->cc_route_count[idle] = n + 1;
    v->cc_live.store(idle, std::memory_order_release);
}

void voxo_clear_cc_map(voxo_t* v) {
    if (!v) return;
    const uint32_t live = v->cc_live.load(std::memory_order_acquire), idle = live ^ 1u;
    v->cc_route_count[idle] = 0;
    v->cc_live.store(idle, std::memory_order_release);
}

void voxo_set_local_control(voxo_t* v, bool on) {
    if (v) v->local_control.store(on ? 1u : 0u, std::memory_order_relaxed);
}

/* The callback's whole body. */
void voxo_render(voxo_t* v, float* out_lr, uint32_t frames) {
    if (!out_lr || frames == 0) return;
    if (!v) { std::memset(out_lr, 0, sizeof(float) * 2u * frames); return; }

    // 1. The shell's settings, once per block — and the instrument swap.
    const uint32_t pending = v->pending_mode.exchange(UINT32_MAX, std::memory_order_acq_rel);
    if (pending != UINT32_MAX) sumi_normalizer_set_mode(v->normalizer, (sumi_input_mode_t)pending);
    const float gain = v->gain.load(std::memory_order_relaxed);
    const bool linear = v->interpolation.load(std::memory_order_relaxed) != 0;
    if (Instrument* next = v->pending_inst.exchange(nullptr, std::memory_order_acq_rel)) {
        Instrument* cur = v->current_inst.exchange(next == &g_none ? nullptr : next, std::memory_order_acq_rel);
        // The retired slot holds at most one: the shell empties it before it
        // publishes, so the callback only ever fills an empty slot.
        if (cur) { Instrument* stale = v->retired_inst.exchange(cur, std::memory_order_acq_rel); (void)stale; }
        for (uint32_t i = 0; i < v->max_voices; i++) { v->voices[i].active = false; for (uint32_t k = 0; k < MAX_LAYERS; k++) v->voices[i].layers[k].active = false; }   // voices on the old instrument end here
        v->reverb->clear(); v->delay->clear();   // the old instrument's tail goes with it
    }
    Instrument* inst = v->current_inst.load(std::memory_order_relaxed);

    // 2. Every voice transition, in order, before a sample is written — under
    //    the zone the normalizer holds and the dialect it resolved (#25).
    {
        const uint32_t m0 = (uint32_t)sumi_normalizer_mode(v->normalizer);
        v->zone = sumi_normalizer_zone(v->normalizer);
        v->zone_live = (m0 == SUMI_INPUT_MPE || m0 == SUMI_INPUT_WIND);
    }
    const uint32_t n = sumi_normalizer_drain(v->normalizer, v->clock_seconds, v->events, EVENTS_PER_BLOCK);
    for (uint32_t i = 0; i < n; i++) apply_event(v, inst, v->events[i]);
    const uint32_t mode = (uint32_t)sumi_normalizer_mode(v->normalizer);
    v->effective_mode.store(mode, std::memory_order_relaxed);
    // Pressure shapes the level only where the dialect carries it (MPE, wind).
    const bool pressure_live = mode != SUMI_INPUT_CLASSIC;

    // 3. The block.
    std::memset(out_lr, 0, sizeof(float) * 2u * frames);
    const double two_pi = 6.283185307179586;
    const uint32_t rate = v->device_rate.load(std::memory_order_relaxed);
    const double inv_rate = 1.0 / (double)rate;
    uint32_t active = 0, layers_active = 0;
    for (uint32_t vi = 0; vi < v->max_voices; vi++) {
        Voice& vc = v->voices[vi];
        if (!vc.active) continue;
        // The block's pitch: the ratio recomputed here from the events just
        // applied, reached by a straight line across the block (SOUND §2).
        const float freq_from = vc.freq;
        const float freq_step = (vc.freq_target - vc.freq) / (float)frames;
        if (inst) {
            // The MPE sources, smoothed with the group's rising/falling times
            // (taken from the first group carrying a layer, else the defaults) —
            // one value per block, ramped across it: the per-sample smoother.
            const Group* g0 = nullptr;
            for (uint32_t k = 0; k < MAX_LAYERS; k++) if (vc.layers[k].active) { g0 = &inst->groups[vc.layers[k].zone->group]; break; }
            const float pr_ms = g0 ? (vc.pressure_target > vc.pressure ? g0->pressure_rise_ms : g0->pressure_fall_ms) : 20.0f;
            const float tm_ms = g0 ? (vc.timbre_target > vc.timbre ? g0->timbre_rise_ms : g0->timbre_fall_ms) : 20.0f;
            const float block_s = (float)frames / (float)rate;
            const float pr_coef = pr_ms <= 0.0f ? 1.0f : 1.0f - std::exp(-block_s / (pr_ms * 0.001f));
            const float tm_coef = tm_ms <= 0.0f ? 1.0f : 1.0f - std::exp(-block_s / (tm_ms * 0.001f));
            const float pressure_prev = vc.pressure, timbre_prev = vc.timbre;
            vc.pressure += (vc.pressure_target - vc.pressure) * pr_coef;
            vc.timbre += (vc.timbre_target - vc.timbre) * tm_coef;
            vc.swirl += (vc.swirl_target - vc.swirl) * pr_coef;
            bool any = false;
            for (uint32_t k = 0; k < MAX_LAYERS; k++) {
                Layer& L = vc.layers[k];
                if (!L.active) continue;
                const Group& g = inst->groups[L.zone->group];
                const GroupState& st = inst->state[L.zone->group];
                // The pressure's gain and the timbre's cutoff at the block's start and end.
                const float pg0 = pressure_live ? g.pressure.at(pressure_prev) : 1.0f;
                const float pg1 = pressure_live ? g.pressure.at(vc.pressure) : 1.0f;
                float fc0, fc1;
                if (g.timbre_is_default) { fc0 = st.cutoff * g.timbre.at(timbre_prev); fc1 = st.cutoff * g.timbre.at(vc.timbre); }
                else { fc0 = g.timbre.at(timbre_prev); fc1 = g.timbre.at(vc.timbre); }
                if (render_layer(v, vc, L, g, st, out_lr, frames, freq_from, freq_step, pg0, (pg1 - pg0) / (float)frames, fc0, fc1, linear, rate)) {
                    any = true; layers_active++;
                }
            }
            vc.freq = vc.freq_target;
            if (!any) { vc.active = false; continue; }
        } else {
            // The sine (no instrument): the skeleton's voice.
            float freq = vc.freq, level = vc.level, press = vc.pressure;
            const float env_coef = vc.releasing ? v->release_coef : v->attack_coef;
            const float press_mix = pressure_live ? 0.65f : 0.0f;
            double phase = vc.phase;
            for (uint32_t f = 0; f < frames; f++) {
                freq  += freq_step;
                level += (vc.level_target - level) * env_coef;
                press += (vc.pressure_target - press) * v->press_coef;
                const float gg = SINE_AMPLITUDE * vc.vel_gain * level * (1.0f - press_mix + press_mix * press);
                const float s = (float)std::sin(phase) * gg;
                out_lr[2u * f]      += s;
                out_lr[2u * f + 1u] += s;
                phase += two_pi * (double)freq * inv_rate;
                if (phase >= two_pi) phase -= two_pi;
            }
            vc.phase = phase; vc.freq = vc.freq_target; vc.level = level; vc.pressure = press;
            if (vc.releasing && level < 1e-4f) { vc.active = false; continue; }
        }
        active++;
    }
    // The bus (step 52): the preset's reverb and delay after the sum, before the
    // master gain — their parameters re-read every block (a CC binding may have
    // moved them), the buffers the callback's own.
    if (inst) {
        if (inst->bus.reverb_on) { v->reverb->set(inst->bus); v->reverb->process(out_lr, frames); }
        if (inst->bus.delay_on) { v->delay->set(inst->bus, rate); v->delay->process(out_lr, frames); }
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
    v->active_layers.store(layers_active, std::memory_order_relaxed);
}

void voxo_stats(const voxo_t* v, voxo_stats_t* out) {
    if (!out) return;
    std::memset(out, 0, sizeof(*out));
    if (!v) return;
    out->sample_rate    = v->device_rate.load(std::memory_order_relaxed);
    out->block_frames   = v->device_block.load(std::memory_order_relaxed);
    out->active_voices  = v->active_voices.load(std::memory_order_relaxed);
    out->active_layers  = v->active_layers.load(std::memory_order_relaxed);
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
    if (const Instrument* i = v->current_inst.load(std::memory_order_acquire)) {
        if (!i->zones.empty()) {
            const Zone& z = i->zones[0];
            out->sample_frames = z.frames; out->sample_channels = z.channels;
            out->sample_rate_hz = z.rate; out->sample_root_note = 69.0f + 12.0f * std::log2(z.root_hz / 440.0f);
        }
        if (i->model) { out->preset_loaded = 1; out->preset_zones = i->zone_count(); }
    }
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
    // The bus's buffers for this rate (shell thread: the device is not running
    // when the rate changes — create, open, stop).
    if (v->bus_rate != rate) {
        v->reverb->allocate(rate);
        v->delay->allocate(rate);
        v->bus_rate = rate;
    }
    v->attack_coef  = one_pole_coef(0.003f, rate);   // the sine's 3 ms in
    v->release_coef = one_pole_coef(0.040f, rate);   // and 40 ms out
    v->press_coef   = one_pole_coef(0.020f, rate);   // the sine's pressure smoothing
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
