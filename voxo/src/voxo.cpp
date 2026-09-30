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
#include "suzu.h"              // step 56: Suzu, the symplectic synth — the source beside the sampler

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#define VOXO_VERSION_MAJOR 0
#define VOXO_VERSION_MINOR 13
#define VOXO_VERSION_PATCH 0

using voxo_inst::Instrument;
using voxo_inst::Zone;
using voxo_inst::Group;
using voxo_inst::GroupState;
using voxo_inst::Target;

namespace {

constexpr uint32_t MAX_VOICES_CAP = 64;
constexpr uint32_t TRACE_RING  = 1024;   // step 59: ~85 ms of orbit at the sub-rate's eighth (96 kHz / 8 = 12 kHz)
constexpr uint32_t TRACE_DECIM = 8;
constexpr uint32_t MAX_LAYERS     = 8;       // zones a performance voice stacks at once
constexpr uint32_t EVENTS_PER_BLOCK = 512;   // drained at block start; the ring keeps the rest for the next block
constexpr float    SINE_AMPLITUDE   = 0.18f; // one sine at full level; 16 of them soft-clip, never wrap
constexpr float    SAMPLE_AMPLITUDE = 0.40f; // one sample layer at full level
constexpr float    SUZU_END_AMP     = 1e-4f; // −80 dB: a released cell below this ends
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

// Step 56: the Suzu voice — one cell, its filter, the output ramp (suzu.h's classes).
// Step 57: the cell became a LATTICE of cells (the modes), each with its ratio,
// its declared decay and its share of the strike and of the bow; cell 0 alone
// is step 56's voice (voice_kind 0).
struct SuzuVoice {
    suzu::Cell cell;           // step 56's single cell = modes[0] when voice_kind is 0
    suzu::Cell modes[suzu::MODES_MAX];
    float      eps[suzu::MODES_MAX];        // each mode's ε for the current pitch (0 = muted, above the ceiling)
    float      inv_eps[suzu::MODES_MAX];    // 1/ε per mode (the coupling kick's, no division in the loop)
    float      decay[suzu::MODES_MAX];      // each mode's declared contraction per sub-step while held
    float      bow_t[suzu::MODES_MAX];      // each mode's bow target energy (breath · profile), 0 = no bow
    suzu::ModalTable table;                 // the preset's ratios, T60s, kick and bow profiles (copied at the strike)
    uint32_t   n;                           // modes alive (1 for the single cell)
    float      mix;                         // the sum's normalisation (1 / Σ kick weights)
    float      kappa;                       // the coupling at the strike (the patch's + the swirl's, clamped)
    suzu::Svf  svf;
    float      out_env;        // the attack ramp, 0..1 (a mixer stage, not a map on the cell)
    float      attack_step;    // per output sample
    float      contract;       // the release's per-sub-step factor (declared), 1 while held
    float      cutoff_prev;    // the SVF's cutoff at the last block's end, Hz (−1 = fresh)
    float      bow_g;          // the bow's per-sub-step onset rate 1/(τ·rate2), 0 = no bow
    // Step 58: the other voice kinds' bodies (one of them alive per voice; suzu.h's classes)
    suzu::VerletString string;
    suzu::HybridString hybrid;
    suzu::Duffing      duffing;
    suzu::Rotor        rotor;
    float      k_smooth;       // the rotor's K, delta-smoothed per block
    float      hybrid_freq;    // the pitch the hybrid was last tuned to (its retune is per block)
    // Step 58b: the flute
    suzu::Bore bore;
    suzu::Jet  jet;
    float      breath_s;       // the breath, smoothed per block
    float      bore_freq;      // the pitch the bore was last built for
    float      bore_lam_max;   // λ's bound for this bore (from μ_max at the strike)
    // Step 58c: the winds
    suzu::Valve valve;
    float      wind_dyn;       // 58c: the sax's breath-following level (the declared dynamics)
    float      dc_x1, dc_y1;   // the output's DC block (a closed reed end pushes a mean flow out of the bell)
    // Step 59: the orbit trace's ring — the rendering thread writes one point in TRACE_DECIM sub-steps when the
    // kind is traced; the poll reads from the other side (twrite is the only shared word; the reader keeps its own cursor)
    float      tx[TRACE_RING], ty[TRACE_RING];
    std::atomic<uint32_t> twrite;
    uint32_t   tdecim;         // the decimation counter
    float      tprev;          // the last traced output (the chains' derivative pair)
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
    SuzuVoice suzu;                    // step 56: alive when the block's source is Suzu
};

struct Channel {
    float bend_semitones;
    float pressure;        // last channel pressure 0..1
    float timbre;          // last CC 74 / 127
    float breath;          // step 57: the breath controller (CC 2, or 11 as its alias) 0..1
    float wheel;           // step 58: the mod wheel (CC 1) 0..1 — the rotor's K
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
    std::atomic<uint32_t> source;            // step 56: VOXO_SOURCE_*; read at block start
    std::atomic<uint32_t> trace_mask;        // step 59: the voice kinds whose orbit is traced (bit k), 0 = none
    uint32_t trace_read[MAX_VOICES_CAP];     //   the poll's cursor per slot (the reader's own)
    uint32_t trace_serial[MAX_VOICES_CAP];   //   the voice serial the cursor belongs to
    std::atomic<uint32_t> suzu_live;         // which suzu_params slot the callback reads
    voxo_suzu_params_t    suzu_params[2];    // the shell writes the idle slot and flips
    float                 shear_ratio[2][suzu::SHEAR_TABLE];   // step 56: the shears' measured detune per gain (cubic, triangle), filled at create
    float                 suzu_comp[2][suzu::COMP_POINTS][suzu::MODES_MAX];   // step 57: the coupling's detune compensation per κ (c²), beside its patch slot
    float                 suzu_comp_jinv[2][suzu::COMP_POINTS][suzu::MODES_MAX * suzu::MODES_MAX];   // and J⁻¹ per κ: the per-pitch correction's matrix
    uint64_t              suzu_comp_key[2];        // what the table depends on (the ratios: preset, modes, stiffness) — a knob that keeps them reuses it
    suzu::DoublePendulum  pendulum;                // step 58: the patch's chaotic modulator (control rate, the callback's thread)
    float                 mod_smooth;              // its output, smoothed per block
    struct WindCal { float peaks[8]; int npeaks; float offset[4]; uint64_t key; } suzu_wind[2];   // step 58c: per patch slot — the kind's intonation at C3, C4, C5, C6 (cents)
    std::atomic<uint32_t> ftz_set;           // the rendering thread set FTZ/DAZ (stats)
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
    uint32_t source_now;             // step 56: the block's source (the atomic, read once)
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

// The compensation table for a patch: c_k per κ grid point; past the feasible
// κ (a mode's own stiffness gone) the last feasible row is kept — the gate
// never admits such a patch, and the lab's bypass then runs under-compensated
// (detuned, bounded) rather than on a diverged solve.
static uint64_t suzu_comp_key_of(const voxo_suzu_params_t& p) {
    uint32_t sb; std::memcpy(&sb, &p.stiffness, sizeof sb);
    return ((uint64_t)p.modal_preset << 56) | ((uint64_t)p.modes << 48) | (uint64_t)sb | ((uint64_t)(p.voice_kind != 0) << 40);
}
static void suzu_comp_fill(const suzu::ModalTable& t, float (*out)[suzu::MODES_MAX], float (*jout)[suzu::MODES_MAX * suzu::MODES_MAX]) {
    int last_ok = -1; float warm[suzu::MODES_MAX]; bool have_warm = false;
    const int nn = t.n * t.n;
    for (int i = 0; i < suzu::COMP_POINTS; i++) {
        float* row = out[i]; float* jrow = jout[i];
        if (last_ok >= 0 && last_ok != i - 1) {                               // past the edge: the last feasible row, frozen (the gate never admits it)
            std::memcpy(row, out[last_ok], sizeof(float) * suzu::MODES_MAX); std::memcpy(jrow, jout[last_ok], sizeof(float) * (size_t)nn);
            continue;
        }
        const bool ok = suzu::lattice_compensation(t.ratio, t.n, (float)i * suzu::COMP_STEP, row, have_warm ? warm : nullptr, jrow);
        if (ok) { for (int k = 0; k < t.n; k++) warm[k] = row[k]; have_warm = true; last_ok = i; }
        for (int k = 0; k < t.n; k++) row[k] *= row[k];                       // stored squared (suzu::lattice_comp_at_pitch)
        for (int k = t.n; k < suzu::MODES_MAX; k++) row[k] = 1.0f;
        if (!ok && last_ok >= 0) { std::memcpy(row, out[last_ok], sizeof(float) * suzu::MODES_MAX); std::memcpy(jrow, jout[last_ok], sizeof(float) * (size_t)nn); }
    }
}
// Step 58c: THE WINDS' CALIBRATION at patch load. The trumpet's flare has no formula for its peaks, so they are
// measured (suzu::bore_peaks); and a valve pulls the played pitch off the bore's peak — an outward valve above it
// (+151 cents at the defaults), a reed's compliance below (−10) — so each wind is blown once at A3 exactly as the
// strike would build it and the offset measured; the strike then cuts the bore that many cents the other way.
static double suzu_goertzel(const float* s, size_t n, double mean, double hz, double rate) {
    const double w = 2.0 * suzu::PI * hz / rate, cw = 2.0 * cos(w); double s0 = 0.0, s1 = 0.0, s2 = 0.0;
    for (size_t i = 0; i < n; i++) { const double win = 0.5 - 0.5 * cos(2.0 * suzu::PI * (double)i / (double)(n - 1)); s0 = ((double)s[i] - mean) * win + cw * s1 - s2; s2 = s1; s1 = s0; }
    const double re = s1 - s2 * cos(w), im = s2 * sin(w); return sqrt(re * re + im * im);
}
// The period of a wind's tone (the intonation calibration): the autocorrelation's first lag that reaches its
// maximum within 0.08, over lags from rate/4000 to rate/60, then a 4-cent Goertzel scan about it. The period
// reads a first-register note whose fundamental sits under its harmonics as the note (the "lowest partial within
// 15 dB" rule read a weak fundamental as the octave and mis-cut the bore).
static float suzu_fundamental_of(const float* s, size_t n, float rate) {
    double mean = 0.0; for (size_t i = 0; i < n; i++) mean += s[i]; mean /= (double)n;
    const size_t lo = (size_t)(rate / 4000.0f), hi = (size_t)(rate / 60.0f) < n / 2 ? (size_t)(rate / 60.0f) : n / 2;
    if (hi <= lo + 2) return 0.0f;
    double e0 = 0.0; for (size_t i = 0; i < n - hi; i++) { const double a = (double)s[i] - mean; e0 += a * a; }
    double* rr = new double[hi]; double best = -2.0;
    for (size_t l = lo; l < hi; l++) {
        double c = 0.0, e1 = 0.0;
        for (size_t i = 0; i < n - hi; i++) { const double a = (double)s[i] - mean, b = (double)s[i + l] - mean; c += a * b; e1 += b * b; }
        rr[l] = c / sqrt((e0 > 0.0 ? e0 : 1e-30) * (e1 > 0.0 ? e1 : 1e-30)); if (rr[l] > best) best = rr[l];
    }
    double bl = (double)lo;
    for (size_t l = lo + 1; l + 1 < hi; l++) if (rr[l] >= best - 0.08 && rr[l] >= rr[l - 1] && rr[l] >= rr[l + 1]) { bl = (double)l; break; }   // 0.08: a jittery or period-doubled cycle still reads its period
    delete[] rr;
    const double f0 = (double)rate / bl; double bm = -1.0, bf = f0;
    for (double hz = f0 * pow(2.0, -60.0 / 1200.0); hz < f0 * pow(2.0, 60.0 / 1200.0); hz *= 1.0023131) { const double m = suzu_goertzel(s, n, mean, hz, rate); if (m > bm) { bm = m; bf = hz; } }
    return (float)bf;
}
static const float SUZU_P_REF = 0.005f;                     // the reference mouth pressure, in the bore's units at the blown end
// Build a wind's bore for a target frequency (the note, offset applied by the caller) and its valve; returns the cells (0 = none).
// THE SAX SCALES WITH THE NOTE (as the flute's embouchure does, DECISIONS_7 #19): its bell's radiation corner and its
// wall loss are declared at A3 and follow the pitch — corner·f/220, T60·220/f — so every note is the A3 cone in
// miniature, and the first register that holds there holds everywhere. With the corner fixed the high notes' peaks sat
// under the bell's cutoff and the reed took the octave; the trumpet keeps its fixed bell (its lips choose the register).
static inline float suzu_wind_corner(const voxo_suzu_params_t& sp, int kind, float hz) { return kind == 7 ? sp.bore_corner_hz * hz / 220.0f : sp.bore_corner_hz; }
static inline float suzu_wind_wall(const voxo_suzu_params_t& sp, int kind, float hz, float rate2) {
    if (sp.bore_wall_s <= 0.0f) return 1.0f;
    return suzu::contraction_for(kind == 7 ? sp.bore_wall_s * 220.0f / (hz > 1.0f ? hz : 1.0f) : sp.bore_wall_s, rate2);
}
static int suzu_wind_build(suzu::Bore& b, suzu::Valve& vv, const voxo_suzu_params_t& sp, int kind, float f_target, float lip_hz, float rate2, const float* peaks, int npeaks, bool fresh) {
    const bool sax = kind == 7;
    const int profile = sax ? suzu::BORE_CONE : suzu::BORE_TRUMPET;
    const float apex = sax ? sp.cone_apex : sp.bell_start, gamma = sax ? 0.0f : sp.bell_gamma;
    const float corner = suzu_wind_corner(sp, kind, f_target);
    if (fresh) { b.setup(64, profile, apex, gamma, suzu::END_CLOSED, 0.0f, suzu::END_OPEN, sp.bore_loss); vv.reset(); }
    b.radiation_corner(corner, rate2);
    const float lam_max = suzu::Bore::lambda_bound(b.mu_max()) * 0.99f;
    const float s_end = suzu::Bore::profile_s(profile, 1.0f, apex, gamma);
    const float ends = suzu::Bore::end_correction(sp.bore_loss, b.rad_a, f_target, rate2, s_end);
    int cells; float lam;
    if (sax) {
        cells = suzu::Bore::cells_for(f_target, rate2, lam_max, false, suzu::cone_extra(apex), ends, (int)sp.bore_nodes);
        lam = suzu::Bore::lambda_for(f_target, rate2, cells, false, suzu::cone_extra(apex), ends);
    } else {
        const int m = (int)(sp.partial < 1u ? 1u : sp.partial > 6u ? 6u : sp.partial);
        const float ratio = m <= npeaks ? peaks[m - 1] : (float)(2 * m - 1);
        const float fq = f_target / ratio;                                                  // the bore's quarter-wave fundamental
        cells = suzu::Bore::cells_for(fq, rate2, lam_max, true, 0.0f, ends, (int)sp.bore_nodes);
        lam = suzu::Bore::lambda_for(fq, rate2, cells, true, 0.0f, ends);
    }
    if (cells < 2) return 0;
    if (fresh) b.setup(cells, profile, apex, gamma, suzu::END_CLOSED, 0.0f, suzu::END_OPEN, sp.bore_loss);
    b.retune(cells, lam, profile, apex, gamma); b.radiation_corner(corner, rate2);
    if (sax) vv.setup(sp.reed_hz, rate2, sp.reed_q, sp.reed_open, sp.reed_close * SUZU_P_REF, sp.reed_area, 0, 2000.0f);   // the reed's turbulence low-passed at 2 kHz
    else     vv.setup(lip_hz, rate2, sp.lip_q, sp.lip_open, sp.lip_close * SUZU_P_REF, sp.lip_area, 1, 0.0f);            // the lips' white (the registers were tuned on it)
    return cells;
}
// One sub-step of a wind: the implicit junction (or the naive one), the bore, the valve. Returns the mouth end's velocity.
static inline float suzu_wind_step(suzu::Bore& b, suzu::Valve& vv, float Pm, float sigma, float wall, float shear, bool naive) {
    const float a = vv.A * vv.opening();
    const float swept = vv.swept();                                           // the valve's own volume flow into the bore, in the force's direction
    const float dp_prev = Pm - b.p[0];
    const float noise = sigma * Pm * vv.noise();
    const float Z = b.end_impedance();
    const float q = naive ? suzu::Valve::flow_naive(a, dp_prev + noise) : suzu::Valve::flow_implicit(a, Pm - b.end_pressure_ahead() - Z * swept + noise, Z);
    const float out = b.step(q + swept, 0.0f, wall, shear);
    vv.q = q + swept;                                                          // the mouth's flow: the aperture's and the swept
    vv.step(Pm - b.p[0]);
    return out;
}
// The intonation at a note: blown 0.45 s at 1.4 reference pressures (the breath map's middle), the pitch of the last
// 0.2 s against the note, in cents (0 if silent). The calibration takes it at C3, C4, C5 and C6 and lerps in octaves.
static float suzu_wind_offset(const voxo_suzu_params_t& sp, int kind, float rate2, const float* peaks, int npeaks, float hz) {
    suzu::Bore* b = new suzu::Bore; suzu::Valve vv;
    if (!suzu_wind_build(*b, vv, sp, kind, hz, hz * sp.lip_ratio, rate2, peaks, npeaks, true)) { delete b; return 0.0f; }
    const float Pm = 1.4f * SUZU_P_REF, wall = suzu_wind_wall(sp, kind, hz, rate2);
    const long N = (long)(0.45f * rate2), M = (long)(0.2f * rate2);
    float* rec = new float[M];
    for (long i = 0; i < N; i++) { const float o = suzu_wind_step(*b, vv, Pm, kind == 7 ? sp.reed_noise : 0.02f, wall, 0.0f, false); if (i >= N - M) rec[i - (N - M)] = o; }
    float pk = 0.0f; for (long i = 0; i < M; i++) pk = std::fmax(pk, std::fabs(rec[i]));
    float off = 0.0f;
    if (pk > 1e-4f && std::isfinite(pk)) { const float f = suzu_fundamental_of(rec, (size_t)M, rate2); if (f > 0.0f) off = 1200.0f * log2f(f / hz); }
    delete[] rec; delete b;
    return (off > -800.0f && off < 800.0f) ? off : 0.0f;                                   // a register away is not an intonation
}
static uint64_t suzu_wind_key_of(const voxo_suzu_params_t& p) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](float f) { uint32_t u; std::memcpy(&u, &f, sizeof u); h ^= u; h *= 1099511628211ull; };
    mix(p.reed_hz); mix(p.reed_q); mix(p.reed_open); mix(p.reed_close); mix(p.reed_area); mix(p.cone_apex); mix(p.lip_ratio); mix(p.lip_q); mix(p.lip_open);
    mix(p.lip_close); mix(p.lip_area); mix((float)p.partial); mix(p.bell_start); mix(p.bell_gamma); mix(p.bore_loss); mix(p.bore_corner_hz); mix(p.bore_wall_s); mix((float)p.bore_nodes);
    mix((float)p.voice_kind);
    return h;
}
static const float SUZU_CAL_HZ[4] = { 130.8128f, 261.6256f, 523.2511f, 1046.502f };   // C3, C4, C5, C6
static void suzu_wind_calibrate(voxo_t* v, const voxo_suzu_params_t& sp, voxo_t::WindCal& cal) {
    const float rate2 = 2.0f * (float)v->device_rate.load(std::memory_order_relaxed);
    cal.npeaks = sp.voice_kind == 8 ? suzu::bore_peaks(suzu::BORE_TRUMPET, sp.bell_start, sp.bell_gamma, sp.bore_loss, sp.bore_corner_hz, rate2, 8, cal.peaks) : 0;
    for (int k = 0; k < 4; k++) cal.offset[k] = suzu_wind_offset(sp, (int)sp.voice_kind, rate2, cal.peaks, cal.npeaks, SUZU_CAL_HZ[k]);
    cal.key = suzu_wind_key_of(sp);
    if (v->log_cb) {
        char msg[200];
        std::snprintf(msg, sizeof msg, "suzu: the %s calibrated — the embouchure's pull at C3 %+.0f, C4 %+.0f, C5 %+.0f, C6 %+.0f cents (the bore cut to cancel it, lerped between)",
                      sp.voice_kind == 7 ? "sax" : "trumpet", cal.offset[0], cal.offset[1], cal.offset[2], cal.offset[3]);
        v->log_cb(1, msg, v->log_user);
    }
}
// The calibrated offset at a pitch: the four points lerped in octaves from C3, held flat outside.
static inline float suzu_wind_offset_at(const voxo_t::WindCal& cal, float hz) {
    float x = log2f((hz > 1.0f ? hz : 1.0f) / SUZU_CAL_HZ[0]); if (x < 0.0f) x = 0.0f; if (x > 3.0f) x = 3.0f;
    const int i = x >= 3.0f ? 2 : (int)x; const float t = x - (float)i;
    return cal.offset[i] + (cal.offset[i + 1] - cal.offset[i]) * t;
}
// The voice's compensation at a κ and a pitch: the tables lerped at κ, the per-pitch correction (suzu.h).
static void suzu_comp_for(const voxo_t* v, uint32_t slot, float kappa, const suzu::ModalTable& t, float f0, float rate2, float* comp) {
    const int n = t.n, nn = n * n;
    float c2[suzu::MODES_MAX], jinv[suzu::MODES_MAX * suzu::MODES_MAX];
    if (kappa <= 0.0f) { for (int k = 0; k < n; k++) comp[k] = 1.0f; return; }
    float p = kappa / suzu::COMP_STEP; if (p > (float)(suzu::COMP_POINTS - 1)) p = (float)(suzu::COMP_POINTS - 1);
    int i = (int)p; if (i >= suzu::COMP_POINTS - 1) i = suzu::COMP_POINTS - 2;
    const float tt = p - (float)i;
    const float* lo = v->suzu_comp[slot][i]; const float* hi = v->suzu_comp[slot][i + 1];
    const float* jlo = v->suzu_comp_jinv[slot][i]; const float* jhi = v->suzu_comp_jinv[slot][i + 1];
    for (int k = 0; k < n; k++) c2[k] = suzu::table_lerp(lo, hi, tt, k);
    for (int k = 0; k < nn; k++) jinv[k] = suzu::table_lerp(jlo, jhi, tt, k);
    suzu::lattice_comp_at_pitch(c2, jinv, t.ratio, n, f0, rate2, comp);
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
    if (v->source_now != VOXO_SOURCE_SAMPLER) {
        vc.suzu.tdecim = 0; vc.suzu.tprev = 0.0f;                              // step 59: the trace's counters (its ring write index runs on across strikes)
        // Step 56: the strike is a DRIVE — the cell's orbit set at the velocity's
        // amplitude, the filter cleared, the output ramp from zero (a retrigger
        // re-kicks the same cell: the orbit restarts at the new amplitude).
        const voxo_suzu_params_t& sp = v->suzu_params[v->suzu_live.load(std::memory_order_acquire)];
        const uint32_t rate2 = 2u * v->device_rate.load(std::memory_order_relaxed);
        SuzuVoice& sz = vc.suzu;
        const float A = sp.level * vc.vel_gain;
        if (sp.mod_target != 0) v->pendulum.trigger(0.5 + 2.5 * (double)vc.vel_gain);   // step 58: the modulator's energy set at the trigger
        if (sp.voice_kind >= 2) {
            // Step 58: the strings and the chaos voices. decay[0] carries the kind's declared per-sub-step damping.
            sz.n = 1; sz.mix = 1.0f; sz.kappa = 0.0f;
            for (uint32_t k = 0; k < suzu::MODES_MAX; k++) { sz.bow_t[k] = 0.0f; sz.eps[k] = 0.0f; }
            sz.table.n = 1; sz.table.ratio[0] = 1.0f;
            if (sp.voice_kind == 2) {
                const int m = suzu::VerletString::nodes_for(vc.freq, (float)rate2, (int)sp.string_nodes);
                if (m >= 2) {
                    if (fresh || sz.string.m != m) sz.string.reset(m);            // a re-pluck of a ringing string ADDS its profile
                    const float sc = sp.string_cfl > 0.0f ? sqrtf(sp.string_cfl) : suzu::VerletString::s_for(vc.freq, (float)rate2, m);
                    sz.string.tune(sc);
                    sz.string.pluck(A, sp.pluck);
                } else sz.string.reset(2);                                        // above rate/6: not representable, silent
                sz.decay[0] = sp.string_decay_s > 0.0f ? suzu::contraction_for(sp.string_decay_s, (float)rate2) : 1.0f;
            } else if (sp.voice_kind == 3) {
                if (fresh) sz.hybrid.reset();
                float bhz[suzu::BRIDGE_MAX], bd[suzu::BRIDGE_MAX];
                const float bdamp = sp.bridge_decay_s > 0.0f ? suzu::contraction_for(sp.bridge_decay_s, (float)rate2) : 1.0f;
                for (int b = 0; b < suzu::BRIDGE_MAX; b++) { bhz[b] = sp.bridge_hz * (b == 0 ? 1.0f : b == 1 ? 1.618f : 2.618f); bd[b] = bdamp; }
                sz.hybrid.bridge_set((int)sp.bridge_cells, bhz, (float)rate2, sp.bridge_coupling, bd);
                sz.hybrid.bgain = sp.bridge_gain;
                sz.hybrid.gain = sp.string_decay_s > 0.0f ? expf(-6.907755f / (sp.string_decay_s * vc.freq)) : 1.0f;   // per round trip
                if (sz.hybrid.tune(vc.freq, (float)rate2, 0.5f * sp.loop_loss)) { sz.hybrid_freq = vc.freq; sz.hybrid.pluck(0.5f * A, sp.pluck); }
                sz.decay[0] = 1.0f;                                                // the hybrid's losses live inside it (declared)
            } else if (sp.voice_kind == 4) {
                sz.duffing.reset(); sz.duffing.beta = sp.duffing_beta;
                sz.duffing.c.kick(A, suzu::eps_for(vc.freq, (float)rate2));
                sz.duffing.dphi = 2.0f * suzu::PI * vc.freq * sp.drive_ratio / (float)rate2;
                sz.decay[0] = sp.decay_s > 0.0f ? suzu::contraction_for(sp.decay_s, (float)rate2) : 1.0f;
            } else if (sp.voice_kind == 6) {
                // the flute: an open–open cylinder with its radiation ports, the jet at node 0; the bore's cells from the
                // pitch under the CFL bound (μ_max by power iteration once, at the strike — the profile is fixed)
                if (fresh) { sz.bore.setup(64, suzu::BORE_CYLINDER, 0.0f, 0.0f, suzu::END_OPEN, sp.bore_loss, suzu::END_OPEN, sp.bore_loss); sz.jet.reset(); sz.breath_s = 0.0f; }
                sz.bore.radiation_corner(sp.bore_corner_hz, (float)rate2);
                sz.bore_lam_max = suzu::Bore::lambda_bound(sz.bore.mu_max()) * 0.999f;
                const float lam_max = sp.bore_cfl > 0.0f ? sz.bore_lam_max * sp.bore_cfl : sz.bore_lam_max;
                const float ends = 2.0f * suzu::Bore::end_correction(sp.bore_loss, sz.bore.rad_a, vc.freq, (float)rate2);
                const int cells = suzu::Bore::cells_for(vc.freq, (float)rate2, lam_max, false, 0.0f, ends, (int)sp.bore_nodes);
                if (cells >= 2) {
                    const float lam = sp.bore_cfl > 0.0f ? lam_max : suzu::Bore::lambda_for(vc.freq, (float)rate2, cells, false, 0.0f, ends);
                    if (fresh) sz.bore.setup(cells, suzu::BORE_CYLINDER, 0.0f, 0.0f, suzu::END_OPEN, sp.bore_loss, suzu::END_OPEN, sp.bore_loss);
                    sz.bore.retune(cells, lam, suzu::BORE_CYLINDER, 0.0f, 0.0f);
                    sz.bore.radiation_corner(sp.bore_corner_hz, (float)rate2);
                }
                sz.bore_freq = vc.freq;
                sz.jet.q_prev = 0.0f;
                sz.decay[0] = sp.bore_wall_s > 0.0f ? suzu::contraction_for(sp.bore_wall_s, (float)rate2) : 1.0f;   // the wall loss; the radiation is the ports'
            } else if (sp.voice_kind == 7 || sp.voice_kind == 8) {
                // the winds: the bore cut for the note less the embouchure's measured offset (the calibration), the valve set
                const voxo_t::WindCal& cal = v->suzu_wind[v->suzu_live.load(std::memory_order_acquire)];
                const float f_target = vc.freq * exp2f(-suzu_wind_offset_at(cal, vc.freq) / 1200.0f);
                suzu_wind_build(sz.bore, sz.valve, sp, (int)sp.voice_kind, f_target, vc.freq * sp.lip_ratio, (float)rate2, cal.peaks, cal.npeaks, true);   // every strike a new blow: the old bore's content seeded the wrong register
                sz.bore_lam_max = suzu::Bore::lambda_bound(sz.bore.mu_max()) * 0.99f;
                sz.bore_freq = vc.freq; sz.breath_s = fresh ? 0.0f : sz.breath_s;
                sz.dc_x1 = sz.dc_y1 = 0.0f; if (fresh) sz.wind_dyn = 0.32f;
                sz.decay[0] = suzu_wind_wall(sp, (int)sp.voice_kind, vc.freq, (float)rate2);
            } else {
                sz.rotor.strike(A, vc.freq, (float)rate2);
                sz.k_smooth = sp.rotor_k;
                sz.decay[0] = sp.decay_s > 0.0f ? suzu::contraction_for(sp.decay_s, (float)rate2) : 1.0f;
            }
        } else if (sp.voice_kind == 0) {
            sz.n = 1; sz.mix = 1.0f; sz.kappa = 0.0f;
            sz.table.n = 1; sz.table.ratio[0] = 1.0f; sz.table.t60[0] = 0.0f; sz.table.kick[0] = 1.0f; sz.table.bow[0] = 1.0f;
            sz.modes[0].kick(A, suzu::eps_for(vc.freq, (float)rate2));
            sz.eps[0] = sz.modes[0].eps; sz.inv_eps[0] = 1.0f / sz.eps[0]; sz.decay[0] = 1.0f;   // the single cell: no decay while held (step 56)
        } else {
            // Step 57: the modal lattice — the preset's table, the strike's kick profile
            // (a DRIVE per mode), the declared decays, the coupling with the swirl's share.
            suzu::modal_table(&sz.table, (int)sp.modal_preset, (int)sp.modes, sp.decay_s, sp.decay_bright, sp.stiffness, sp.pluck, sp.bow_position);
            sz.n = (uint32_t)sz.table.n;
            float wsum = 0.0f;
            const float ceiling = suzu::MODE_CEILING * (float)rate2;
            const float kappa0 = clampf(sp.coupling + 0.5f * vc.swirl_target, 0.0f, suzu::COMP_KAPPA_MAX);
            float comp[suzu::MODES_MAX];
            suzu_comp_for(v, v->suzu_live.load(std::memory_order_acquire), kappa0, sz.table, vc.freq, (float)rate2, comp);
            for (uint32_t k = 0; k < sz.n; k++) {
                const float fk = vc.freq * sz.table.ratio[k];
                if (fk >= ceiling) { sz.eps[k] = 0.0f; sz.inv_eps[k] = 0.0f; sz.modes[k].reset(); sz.decay[k] = 1.0f; continue; }   // muted above the ceiling: a wall
                sz.eps[k] = comp[k] * suzu::eps_for(fk, (float)rate2); sz.inv_eps[k] = 1.0f / sz.eps[k];
                sz.modes[k].kick(A * sz.table.kick[k], sz.eps[k]);
                sz.decay[k] = sz.table.t60[k] > 0.0f ? suzu::contraction_for(sz.table.t60[k], (float)rate2) : 1.0f;
                wsum += sz.table.kick[k];                                              // the ALIVE modes' weights: the strike peaks at A on every note, however many modes the ceiling left
            }
            sz.mix = wsum > 0.0f ? 1.0f / wsum : 1.0f;
            sz.kappa = clampf(sp.coupling + 0.5f * vc.swirl_target, 0.0f, suzu::COMP_KAPPA_MAX);
        }
        for (uint32_t k = 0; k < suzu::MODES_MAX; k++) sz.bow_t[k] = 0.0f;
        sz.bow_g = sp.bow_onset_s > 0.0f ? 1.0f / (sp.bow_onset_s * (float)rate2) : 0.0f;
        sz.svf.reset();
        sz.out_env = 0.0f;
        const float attack = sp.attack_s < 0.001f ? 0.001f : sp.attack_s;
        sz.attack_step = 1.0f / (attack * (float)(rate2 / 2u));
        sz.contract = 1.0f;
        sz.cutoff_prev = -1.0f;
        if (v->source_now == VOXO_SOURCE_SUZU) return;   // no layers: the source is the synth (layered: the sampler's strike follows)
    }
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
    if (v->source_now != VOXO_SOURCE_SAMPLER) {
        // Step 56: the release is the DECLARED contraction — a T60 in the patch,
        // applied per sub-step; nothing else takes energy from the cell.
        const voxo_suzu_params_t& sp = v->suzu_params[v->suzu_live.load(std::memory_order_acquire)];
        vc.suzu.contract = suzu::contraction_for(sp.release_s, 2.0f * (float)v->device_rate.load(std::memory_order_relaxed));
        if (sp.voice_kind == 3) {                                                  // step 58: the hybrid's release is its round-trip loss, tightened
            const float g = sp.release_s > 0.0f ? expf(-6.907755f / (sp.release_s * (vc.freq > 1.0f ? vc.freq : 1.0f))) : 0.0f;
            if (g < vc.suzu.hybrid.gain) vc.suzu.hybrid.gain = g;
        }
        if (v->source_now == VOXO_SOURCE_SUZU) return;
    }
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
            if (e.a == 1) {                         // step 58: the mod wheel → the rotor's K, per channel (the master's reaches the zone)
                const float b = (float)e.b / 127.0f;
                for_zone_channels(v, e.channel, [&](uint8_t c) { v->channels[c & 15].wheel = b; });
            }
            if (e.a == v->suzu_params[v->suzu_live.load(std::memory_order_acquire)].breath_cc || e.a == 11) {   // step 57: the breath (the patch's CC, dflt 2) and its alias (CC 11) → the bow's target, per channel (the master's reaches the zone)
                const float b = (float)e.b / 127.0f;
                for_zone_channels(v, e.channel, [&](uint8_t c) { v->channels[c & 15].breath = b; });
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

// Step 56: one Suzu voice's block (SYNTH §2.1–§2.4, the classes in suzu.h);
// step 57: the voice is a LATTICE (§2.5–§2.6). Per output sample, twice (the
// 2× section): the pitch's straight line retunes every mode (each re-based on
// its orbit), the coupling kick from the pre-update positions, each mode's
// rotation (with the shear, normalized per mode), the declared damping (the
// mode's T60 while held; the release's contraction after note-off) and the
// bow's servo where breath feeds it; the modes are summed (normalized by the
// strike profile), the pair averaged, the filter, the attack ramp, the
// pressure's level. Returns false when the voice ended.

// Step 58: the strings and the chaos voices — one body per voice, the common
// output stage (the SVF on CC 74, the attack ramp, the pressure gain) as the
// lattice's. decay[0] is the kind's declared per-sub-step damping; the
// release tightens it (the hybrid's is its round-trip loss, set at release).
// Step 59: one orbit point into the voice's trace ring, every TRACE_DECIM-th call (the rendering thread's side).
static inline void trace_push(SuzuVoice& sz, float x, float y) {
    if (++sz.tdecim < TRACE_DECIM) return;
    sz.tdecim = 0;
    const uint32_t w = sz.twrite.load(std::memory_order_relaxed);
    sz.tx[w % TRACE_RING] = x; sz.ty[w % TRACE_RING] = y;
    sz.twrite.store(w + 1, std::memory_order_release);
}
static bool render_suzu_other(voxo_t* v, Voice& vc, const voxo_suzu_params_t& sp, float* out_lr, uint32_t frames, float freq_from, float freq_step,
                              float rate2, float pg0, float pg_step, bool filter, float f_svf, float f_svf_step, float q) {
    SuzuVoice& sz = vc.suzu;
    const uint32_t kind = sp.voice_kind;
    const bool releasing = vc.releasing, gliding = freq_step != 0.0f;
    const float rr = releasing ? (sz.contract < sz.decay[0] ? sz.contract : sz.decay[0]) : sz.decay[0];
    const float block_s = (float)frames / (0.5f * rate2);
    const float coef = 1.0f - std::exp(-block_s / 0.020f);
    const float mod = sp.mod_target != 0 ? v->mod_smooth * sp.mod_depth : 0.0f;
    const float freq_end = freq_from + freq_step * (float)frames;
    const bool tracing = ((v->trace_mask.load(std::memory_order_relaxed) >> kind) & 1u) != 0;   // step 59
    const float trace_dscale = rate2 / (2.0f * suzu::PI * (freq_end > 1.0f ? freq_end : 1.0f));   // ṡ/ω: the derivative in the note's units
    float K = 0.0f, drive = 0.0f, body = 0.0f;
    // the flute's block values: the breath → the mouth pressure → the jet's speed, delay, band, flow
    float U0 = 0.0f, tau = 1.0f, band_f = 0.0f, kscale = 0.0f, leak = 1.0f, jet_g = 0.0f, mouth_q = 0.0f, Pm = 0.0f, jet_a = 0.0f;
    float wind_sigma = 0.0f, wind_shear = 0.0f, wind_scale = 1.0f; bool wind_naive = false;
    if (kind == 7 || kind == 8) {
        // the winds' breath → the mouth pressure, linear: the trumpet from half the reference to three; the sax from
        // just under its threshold (a third of the reed's closing pressure in theory, 1.15 P_ref with the losses —
        // the author heard nothing from a pedal that never reached the old map's threshold at half breath) to 0.6 of
        // the closing pressure (from 0.7 up some notes squeal to the fourth register). The press blows too when the
        // patch says so. The PRESSURE is what smooths, so a released voice's mouth goes to zero — a lingering
        // half-reference blow kept the tail sounding into the next note.
        float breath = vc.held ? v->channels[vc.channel & 15].breath : 0.0f;
        if (sp.press_blows != 0 && vc.held && vc.pressure > breath) breath = vc.pressure;
        const float r_lo = kind == 7 ? 1.1f : 0.5f, r_hi = kind == 7 ? 0.55f * sp.reed_close : 3.0f;
        const float Pm_t = !vc.held || breath <= 0.0f ? 0.0f : SUZU_P_REF * (r_lo + (r_hi - r_lo) * breath);
        // THE SAX'S DYNAMICS, declared: a beating reed's bore amplitude barely grows with the pressure (1.7 dB from the
        // threshold to full breath against the lips' 14), so the sax's level follows the breath itself, −10 dB at the
        // threshold to 0 at full — the player's crescendo, the brightening the physics gives on top
        if (kind == 7) sz.wind_dyn += ((0.32f + 0.68f * breath) - sz.wind_dyn) * coef;
        sz.breath_s += (Pm_t - sz.breath_s) * (1.0f - std::exp(-block_s / (kind == 7 ? 0.080f : 0.020f)));   // breath_s holds the mouth pressure for the winds (the sax's slower attack settles its first register — a hard blow on C3 seeded the third; the lips choose theirs at once)
        Pm = sz.breath_s;
        wind_sigma = kind == 7 ? sp.reed_noise : 0.02f;
        wind_shear = kind == 8 ? sp.brass : 0.0f;
        wind_scale = kind == 7 ? 1.0f * sz.wind_dyn : 4.0f;                      // the mouth end's velocity into the level's range (A3 near −24 dBFS at full breath)
        wind_naive = sp.valve_naive != 0;
        if (kind == 8) {                                                           // the embouchure: CC 74 bends the lips' resonance, ±lip_range octaves across its range
            const float lip_hz = freq_end * sp.lip_ratio * exp2f((vc.timbre - 0.5f) * 2.0f * sp.lip_range);
            sz.valve.setup(lip_hz, rate2, sp.lip_q, sp.lip_open, sp.lip_close * SUZU_P_REF, sp.lip_area, 1, 0.0f);
        }
        if (gliding && sz.bore_freq != freq_end) {                                 // the bore follows the pitch per block
            const voxo_t::WindCal& cal = v->suzu_wind[v->suzu_live.load(std::memory_order_acquire)];
            suzu_wind_build(sz.bore, sz.valve, sp, (int)kind, freq_end * exp2f(-suzu_wind_offset_at(cal, freq_end) / 1200.0f), freq_end * sp.lip_ratio, rate2, cal.peaks, cal.npeaks, false);
            sz.decay[0] = suzu_wind_wall(sp, (int)kind, freq_end, rate2);
            sz.bore_freq = freq_end;
        }
    }
    if (kind == 6) {
        float breath = vc.held ? v->channels[vc.channel & 15].breath : 0.0f;
        if (sp.press_blows != 0 && vc.held && vc.pressure > breath) breath = vc.pressure;   // the press blows too (an Osmose, aftertouch)
        sz.breath_s += (breath - sz.breath_s) * coef;
        const float P_ref = 0.005f;                                                // the reference mouth pressure, in the bore's units (U₀ 0.1)
        const float lo = 1.0f / sqrtf(sp.breath_range), hi = sqrtf(sp.breath_range);   // the pressure's range about the reference, soft to hard
        const float ratio = sz.breath_s <= 0.0f ? 0.0f : lo * powf(hi / lo, (sz.breath_s - sp.breath_ref) / (1.0f - sp.breath_ref) * 0.5f + 0.5f);
        Pm = P_ref * ratio;
        U0 = sqrtf(2.0f * Pm);
        const float T = rate2 / freq_end;
        tau = U0 > 1e-6f ? sp.jet_tau * T * sqrtf(P_ref / Pm) : (float)(suzu::JET_DELAY_MAX - 3);
        band_f = suzu::eps_for(0.5f * rate2 / tau, rate2);
        kscale = -sp.jet_drive;                                                    // the dipole's pressure per unit of the flow's rate, O(1)
        // THE EMBOUCHURE FOLLOWS THE NOTE (v1): the jet's delay is in periods (above), its gain rises with the pitch as the
        // bore's losses do, and its area — the flow — falls as the root of the pitch: the drive is a derivative, so its
        // saturated amplitude would climb 6 dB an octave otherwise (measured: C2 27 dB under A4; with these laws the
        // keyboard sits within 7 dB). The physical jet with one distance and one width, the player blowing harder for
        // height, is the [ITERATE].
        jet_g = sp.jet_gain * (freq_end / 440.0f);
        jet_a = sp.jet_area * sqrtf(440.0f / freq_end);
        leak = 1.0f - 2.0f * suzu::PI * 10.0f / rate2;
        mouth_q = 0.5f * jet_a * U0;                                              // the jet's mean flow: the mouth's work per sample is Pm·Q_in
        if (gliding && sz.bore_freq != freq_end) {                                 // the bore follows the pitch per block: its cells and λ
            const float ends = 2.0f * suzu::Bore::end_correction(sp.bore_loss, sz.bore.rad_a, freq_end, rate2);
            const int cells = suzu::Bore::cells_for(freq_end, rate2, sz.bore_lam_max, false, 0.0f, ends, (int)sp.bore_nodes);
            if (cells >= 2) sz.bore.retune(cells, suzu::Bore::lambda_for(freq_end, rate2, cells, false, 0.0f, ends), suzu::BORE_CYLINDER, 0.0f, 0.0f);
            sz.bore_freq = freq_end;
        }
    }
    if (kind == 5) {
        float kt = sp.rotor_k + (2.5f - sp.rotor_k) * v->channels[vc.channel & 15].wheel;
        if (sp.mod_target == 3) kt += mod;
        kt = clampf(kt, 0.0f, 2.5f);
        sz.k_smooth += (kt - sz.k_smooth) * coef;                              // delta-smoothed: the wheel's steps never land as jumps
        K = sz.k_smooth;
    } else if (kind == 4) {
        drive = sp.drive * vc.pressure * (sp.mod_target == 4 ? (1.0f + mod) : 1.0f);
        if (drive < 0.0f) drive = 0.0f;
        sz.duffing.dphi = 2.0f * suzu::PI * freq_end * sp.drive_ratio / rate2;
    } else if (kind == 3) {
        if (gliding && sz.hybrid_freq != freq_end) {                          // the hybrid retunes once per block (its junction solve is a 6×6 complex one)
            if (sz.hybrid.tune(freq_end, rate2, 0.5f * sp.loop_loss)) sz.hybrid_freq = freq_end;
        }
        body = sp.bridge_coupling > 0.0f ? 0.5f / sqrtf(2.0f * sp.bridge_coupling) : 0.0f;   // the bridge's velocity in the string's energy units, half the mix
    }
    float freq = freq_from, pg = pg0, env = sz.out_env;
    for (uint32_t f = 0; f < frames; f++) {
        freq += freq_step; pg += pg_step; f_svf += f_svf_step;
        if (gliding || f == 0) {
            if (kind == 2 && sp.string_cfl <= 0.0f) {                          // the CFL holds under any glide: the pitch saturates at the bound
                float sc = suzu::VerletString::s_for(freq, rate2, sz.string.m); if (sc > 1.0f) sc = 1.0f;
                sz.string.tune(sc);
            } else if (kind == 4) {
                const float e = suzu::eps_for(freq, rate2); if (e != sz.duffing.c.eps) sz.duffing.c.retune(e);
            } else if (kind == 5 && gliding) {
                sz.rotor.f0 = freq; sz.rotor.period = rate2 / freq;
                sz.rotor.c.retune(suzu::eps_for(freq * (1.0f + sz.rotor.p / (2.0f * suzu::PI)), rate2));
            }
        }
        float acc = 0.0f;
        for (int sub = 0; sub < 2; sub++) {
            float sum;
            if (kind == 2) { sz.string.step(rr); sum = sz.string.pickup(sp.pickup); }
            else if (kind == 3) { float vb; sz.hybrid.step(&vb); sum = 0.5f * sz.hybrid.tap(sp.pickup) + body * vb; }
            else if (kind == 4) { sz.duffing.step(drive); if (rr != 1.0f) sz.duffing.c.contract(rr); sum = sz.duffing.c.x; }
            else if (kind == 7 || kind == 8) {
                const float o = Pm > 1e-3f * SUZU_P_REF ? suzu_wind_step(sz.bore, sz.valve, Pm, wind_sigma, rr, wind_shear, wind_naive) : sz.bore.step(0.0f, 0.0f, rr, wind_shear);
                const float dcb = o - sz.dc_x1 + 0.9995f * sz.dc_y1; sz.dc_x1 = o; sz.dc_y1 = dcb;   // the DC block (declared: a one-pole at 8 Hz)
                sum = wind_scale * dcb;
            }
            else if (kind == 6) {
                const float q = U0 > 1e-6f ? sz.jet.step(sz.bore.u[0], U0, tau, jet_g, sp.jet_offset, jet_a, sp.jet_noise, leak, band_f, 1.0f / (sp.jet_q > 0.1f ? sp.jet_q : 0.1f)) : 0.0f;
                float p_src = kscale * (q - sz.jet.q_prev); sz.jet.q_prev = q;
                // THE POWER-LIMITED PORT (§1's self-excited row, the winds' mechanism): the dipole never does more work on the
                // bore in a sample than the mouth does on the jet, Pm·Q_in — a declared limiter, so the ledger holds by arithmetic
                const float work = p_src * sz.bore.u[0], budget = Pm * q;
                if (work > budget && work > 1e-20f) p_src *= budget / work;
                sum = 40.0f * sz.bore.step(0.0f, p_src, rr);                         // the mouth end's velocity, scaled into the level's range (A4 near the cell's −24 dBFS)
            }
            else { sz.rotor.step(K); if (rr != 1.0f) sz.rotor.c.contract(rr); sum = sz.rotor.c.x; }
            if (tracing) {                                                           // step 59: the orbit — the cell's own pair, or the chain's phase plane
                if (kind == 4) trace_push(sz, sz.duffing.c.x, sz.duffing.c.y);
                else if (kind == 5) trace_push(sz, sz.rotor.c.x, sz.rotor.c.y);
                else { trace_push(sz, sum, (sum - sz.tprev) * trace_dscale); sz.tprev = sum; }
            }
            acc += filter ? sz.svf.step(sum, f_svf, q) : sum;
        }
        if (env < 1.0f) { env += sz.attack_step; if (env > 1.0f) env = 1.0f; }
        const float s = 0.5f * acc * env * pg;
        out_lr[2u * f]      += s;
        out_lr[2u * f + 1u] += s;
    }
    sz.out_env = env;
    if (releasing) {
        const float amp = kind == 2 ? sqrtf(sz.string.energy()) : kind == 3 ? sqrtf(sz.hybrid.energy() / (float)(sz.hybrid.n > 0 ? sz.hybrid.n : 1)) : kind == 4 ? sz.duffing.c.amp : (kind == 6 || kind == 7 || kind == 8) ? 4.0f * sqrtf(fabsf(sz.bore.energy()) / (float)(sz.bore.n > 0 ? sz.bore.n : 1)) : sz.rotor.c.amp;
        if (amp < SUZU_END_AMP) return false;
    }
    return true;
}

bool render_suzu(voxo_t* v, Voice& vc, const voxo_suzu_params_t& sp, float* out_lr, uint32_t frames,
                 float freq_from, float freq_step, bool pressure_live, uint32_t rate) {
    SuzuVoice& sz = vc.suzu;
    const uint32_t n = sz.n;
    const float rate2 = 2.0f * (float)rate;
    // The MPE sources, one value per block, ramped across it (20 ms one-poles).
    const float block_s = (float)frames / (float)rate;
    const float coef = 1.0f - std::exp(-block_s / 0.020f);
    const float pressure_prev = vc.pressure, timbre_prev = vc.timbre;
    vc.pressure += (vc.pressure_target - vc.pressure) * coef;
    vc.timbre += (vc.timbre_target - vc.timbre) * coef;
    vc.swirl += (vc.swirl_target - vc.swirl) * coef;
    const float pg0 = pressure_live ? 0.35f + 0.65f * pressure_prev : 1.0f;
    const float pg1 = pressure_live ? 0.35f + 0.65f * vc.pressure : 1.0f;
    const float pg_step = (pg1 - pg0) / (float)frames;
    // The filter: the cutoff at the block's start and end, ramped; bypassed when open.
    const float fc_max = 0.16f * rate2;                        // the classic form's fs/6 ceiling, at the oversampled rate
    float fc0 = sp.cutoff_hz * std::exp2((timbre_prev - 0.5f) * 6.0f);
    float fc1 = sp.cutoff_hz * std::exp2((vc.timbre - 0.5f) * 6.0f);
    const bool filter = sp.cutoff_hz < 19999.0f || sp.resonance > 0.01f;
    fc0 = clampf(sz.cutoff_prev >= 0.0f ? sz.cutoff_prev : fc0, 20.0f, fc_max);
    fc1 = clampf(fc1, 20.0f, fc_max);
    sz.cutoff_prev = fc1;
    const float q = 2.0f - 1.9f * clampf(sp.resonance, 0.0f, 1.0f);
    float f_svf = suzu::svf_f_for(fc0, rate2, q);
    const float f_svf_step = (suzu::svf_f_for(fc1, rate2, q) - f_svf) / (float)frames;
    if (sp.voice_kind >= 2) return render_suzu_other(v, vc, sp, out_lr, frames, freq_from, freq_step, rate2, pg0, pg_step, filter, f_svf, f_svf_step, q);   // step 58
    const float g_shear = clampf(sp.shear, 0.0f, 1.0f);
    const bool cubic = sp.shear_kind == 0;
    // the shear's detune, compensated in ε from the cell's own calibration (suzu.h)
    const float eps_comp = g_shear > 0.0f ? 1.0f / suzu::shear_table_ratio(v->shear_ratio[cubic ? 0 : 1], g_shear) : 1.0f;
    const bool plain = sp.retune_mode != 0, stepped = sp.retune_mode == 2, naive = sp.update_mode != 0;
    // the coupling this block: the patch's κ plus the swirl's share (the gate allowed 0.5)
    const float kappa = sz.n > 1 ? clampf(sp.coupling + 0.5f * vc.swirl, 0.0f, suzu::COMP_KAPPA_MAX) : 0.0f;
    float comp[suzu::MODES_MAX];                                                  // the coupling's detune compensation at this block's κ and pitch
    if (n > 1) suzu_comp_for(v, v->suzu_live.load(std::memory_order_acquire), kappa, sz.table, sp.retune_mode == 2 ? vc.freq_target : freq_from + freq_step * (float)frames, rate2, comp);
    else comp[0] = 1.0f;
    // the bow: the breath's target energy per mode (the zone's breath on the voice's channel), off once released
    // the breath: the channel's breath CC, or the press when the patch lets it blow (a controller without a breath CC)
    float breath = vc.held ? v->channels[vc.channel & 15].breath : 0.0f;
    if (sp.press_blows != 0 && vc.held && vc.pressure > breath) breath = vc.pressure;
    const bool bowed = sz.bow_g > 0.0f && breath > 0.0f && n > 0;   // no breath (or released): no bow — the voice ends by amplitude
    if (bowed) {
        const float A = sp.level * (0.35f + 0.65f * breath) * breath;       // the target amplitude: the breath, twice — a soft breath sings softly
        for (uint32_t k = 0; k < n; k++) {
            // the bow's REACH: the servo's injection rate tops at 1/τ (u ≤ 1), so a mode whose declared decay
            // rate γ_k exceeds it cannot be held — it rings from the strike and dies, untouched (no re-seeding)
            const bool bowable = (1.0f - sz.decay[k]) < sz.bow_g;
            const float a = A * sz.table.bow[k]; sz.bow_t[k] = bowable ? a * a : 0.0f;
        }
    }
    const float ceiling = suzu::MODE_CEILING * rate2;
    const float r_low = sz.table.ratio[0];                                        // the coupling's reference: the lowest mode
    const float eps_c8 = suzu::eps_for(4186.009f * r_low, rate2);
    // The retune: every mode's ε from the pitch — a sinf per mode, so only when
    // the pitch MOVES (the block's first frame, then each frame of a glide; a
    // held note costs none). A mode that crosses the ceiling is muted there.
    // The lattice refreshes each mode's amplitude from its state first (the
    // coupling moved energy the cell's own `amp` did not see) so the re-base
    // keeps what the mode has, not what it was struck with.
    float e0 = suzu::eps_for((stepped ? vc.freq_target : freq_from) * r_low, rate2);
    float k_eps = 0.0f;
    auto retune_all = [&](float fr) {
        e0 = suzu::eps_for(fr * r_low, rate2);
        for (uint32_t k = 0; k < n; k++) {
            if (sz.eps[k] <= 0.0f) continue;
            const float fk = fr * sz.table.ratio[k];
            if (fk >= ceiling) { sz.eps[k] = 0.0f; sz.inv_eps[k] = 0.0f; sz.modes[k].reset(); continue; }   // crossed the ceiling: muted, a wall
            const float e = eps_comp * comp[k] * suzu::eps_for(fk, rate2);
            if (e == sz.modes[k].eps) continue;
            suzu::Cell& c = sz.modes[k];
            if (n > 1) { const float q = c.energy(); c.amp = q > 0.0f ? sqrtf(q / (1.0f - 0.25f * c.eps * c.eps)) : 0.0f; }
            if (plain) c.retune_plain(e); else c.retune(e);
            sz.eps[k] = e; sz.inv_eps[k] = 1.0f / e;
        }
        k_eps = n > 1 ? suzu::coupling_k(kappa, e0, eps_c8) : 0.0f;
    };
    if (stepped) {                                                                      // the feared form: once per block, no ramp
        for (uint32_t k = 0; k < n; k++) if (sz.eps[k] > 0.0f) { const float e = eps_comp * comp[k] * suzu::eps_for(vc.freq_target * sz.table.ratio[k], rate2); sz.modes[k].retune_plain(e); sz.eps[k] = e; sz.inv_eps[k] = 1.0f / e; }
        k_eps = n > 1 ? suzu::coupling_k(kappa, e0, eps_c8) : 0.0f;
    }
    float freq = freq_from, pg = pg0, env = sz.out_env;
    const bool releasing = vc.releasing;
    const float r_rel = sz.contract;
    const float mix = sz.mix;
    const bool gliding = freq_step != 0.0f;
    const bool tracing = ((v->trace_mask.load(std::memory_order_relaxed) >> sp.voice_kind) & 1u) != 0;   // step 59
    for (uint32_t f = 0; f < frames; f++) {
        freq += freq_step; pg += pg_step;
        f_svf += f_svf_step;
        if (!stepped && (gliding || f == 0)) retune_all(freq);
        float acc = 0.0f;
        for (int sub = 0; sub < 2; sub++) {
            if (n > 1 && k_eps > 0.0f) suzu::lattice_kick(sz.modes, sz.inv_eps, (int)n, k_eps);   // (1) the coupling, pre-update positions; muted modes are walls
            float sum = 0.0f, sumy = 0.0f;
            for (uint32_t k = 0; k < n; k++) {                                                      // (2) each cell's rotation
                if (sz.eps[k] <= 0.0f) continue;
                suzu::Cell& c = sz.modes[k];
                if (naive) c.step_naive();
                else if (g_shear > 0.0f) {
                    const float u = c.amp > 1e-9f ? c.y / c.amp : 0.0f;
                    c.step_sheared(g_shear * c.amp * (cubic ? suzu::shape_cubic(u) : suzu::shape_tri(u)));
                } else c.step();
                // (3) the declared damping: the mode's own while held, the release's after
                float rr = releasing ? (r_rel < sz.decay[k] ? r_rel : sz.decay[k]) : sz.decay[k];
                // (4) the bow's servo where the breath feeds this mode (self-excited, bounded)
                if (bowed && sz.bow_t[k] > 0.0f) {
                    const float E = c.x * c.x + c.y * c.y;
                    if (E < 1e-10f) c.kick(0.1f * std::sqrt(sz.bow_t[k]), c.eps);                   // the seed: the bow's first grip on a resting string (a DRIVE, declared: −20 dB of the target)
                    rr *= suzu::bow_factor(c.x * c.x + c.y * c.y, sz.bow_t[k], sz.bow_g);
                }
                if (rr != 1.0f) c.contract(rr);
                sum += c.x; sumy += c.y;
            }
            sum *= mix;
            if (tracing) trace_push(sz, sum, sumy * mix);                                          // step 59: the lattice's orbit (Σx, Σy)
            acc += filter ? sz.svf.step(sum, f_svf, q) : sum;
        }
        if (env < 1.0f) { env += sz.attack_step; if (env > 1.0f) env = 1.0f; }
        const float s = 0.5f * acc * env * pg;
        out_lr[2u * f]      += s;
        out_lr[2u * f + 1u] += s;
    }
    sz.out_env = env;
    if (releasing && !bowed) {
        float amax = 0.0f;
        for (uint32_t k = 0; k < n; k++) if (sz.eps[k] > 0.0f && sz.modes[k].amp > amax) amax = sz.modes[k].amp;
        if (amax < SUZU_END_AMP) return false;
    }
    return true;
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
    new (&v->source) std::atomic<uint32_t>(VOXO_SOURCE_SAMPLER);     // step 56
    new (&v->suzu_live) std::atomic<uint32_t>(0);
    new (&v->ftz_set) std::atomic<uint32_t>(0);
    voxo_suzu_default_params(&v->suzu_params[0]);
    {                                                    // step 57: the default patch's compensation table (both slots start from it)
        suzu::ModalTable t; const voxo_suzu_params_t& d = v->suzu_params[0];
        suzu::modal_table(&t, (int)d.modal_preset, (int)d.modes, d.decay_s, d.decay_bright, d.stiffness, d.pluck, d.bow_position);
        suzu_comp_fill(t, v->suzu_comp[0], v->suzu_comp_jinv[0]);
        std::memcpy(v->suzu_comp[1], v->suzu_comp[0], sizeof v->suzu_comp[0]);
        std::memcpy(v->suzu_comp_jinv[1], v->suzu_comp_jinv[0], sizeof v->suzu_comp_jinv[0]);
        v->suzu_comp_key[0] = v->suzu_comp_key[1] = suzu_comp_key_of(d);
    v->pendulum.reset(); v->mod_smooth = 0.0f;
    std::memset(v->suzu_wind, 0, sizeof v->suzu_wind);   // the winds calibrate when a wind patch is set
    }
    v->suzu_params[1] = v->suzu_params[0];
    suzu::shear_table_fill(true, v->shear_ratio[0]);    // the cell calibrates its shears (suzu.h)
    suzu::shear_table_fill(false, v->shear_ratio[1]);
    v->source_now = VOXO_SOURCE_SAMPLER;
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

void voxo_set_source(voxo_t* v, uint32_t source) {
    if (v) v->source.store(source == VOXO_SOURCE_SUZU ? VOXO_SOURCE_SUZU : source == VOXO_SOURCE_LAYERED ? VOXO_SOURCE_LAYERED : VOXO_SOURCE_SAMPLER, std::memory_order_relaxed);
}

void voxo_suzu_default_params(voxo_suzu_params_t* out) {
    if (!out) return;
    std::memset(out, 0, sizeof(*out));
    out->level = 0.25f; out->attack_s = 0.003f; out->release_s = 0.4f;
    out->cutoff_hz = 20000.0f; out->resonance = 0.0f;
    out->shear = 0.0f; out->shear_kind = 0; out->retune_mode = 0; out->update_mode = 0;
    out->voice_kind = 1; out->modal_preset = 0; out->modes = 8; out->coupling = 0.05f;
    out->decay_s = 3.0f; out->decay_bright = 0.3f; out->stiffness = 0.0f; out->pluck = 0.28f;
    out->bow_onset_s = 0.15f; out->bow_position = 0.3f; out->breath_cc = 2; out->lattice_gate = 1;
    out->string_nodes = 48; out->string_decay_s = 4.0f; out->pickup = 0.25f; out->cfl_gate = 1; out->string_cfl = 0.0f;
    out->bridge_hz = 220.0f; out->bridge_cells = 2; out->bridge_coupling = 0.002f; out->bridge_decay_s = 1.5f; out->bridge_gain = 1.0f;
    out->loop_loss = 0.5f; out->passivity_gate = 1;
    out->duffing_beta = 8.0f; out->drive = 0.0f; out->drive_ratio = 1.0f; out->rotor_k = 0.3f;
    out->mod_target = 0; out->mod_depth = 0.5f; out->mod_rate = 1.0f;
    out->bore_nodes = 128; out->bore_loss = 0.3f; out->bore_corner_hz = 1500.0f; out->bore_cfl_gate = 1; out->bore_cfl = 0.0f;
    out->jet_gain = 560.0f; out->jet_drive = 1.0f; out->jet_tau = 0.5f; out->jet_q = 1.0f; out->jet_noise = 0.02f; out->jet_area = 0.05f; out->jet_offset = 0.3f;
    out->breath_ref = 0.44f; out->breath_range = 12.0f; out->bore_wall_s = 1.0f; out->press_blows = 1;
    out->reed_hz = 12000.0f; out->reed_q = 0.7f; out->reed_open = 0.5f; out->reed_close = 3.0f; out->reed_area = 0.14f; out->reed_noise = 0.02f; out->cone_apex = 0.25f;
    out->lip_ratio = 0.95f; out->lip_q = 3.0f; out->lip_open = 0.05f; out->lip_close = 1.0f; out->lip_area = 0.5f; out->lip_range = 1.0f;
    out->partial = 3; out->bell_start = 0.6f; out->bell_gamma = 0.7f; out->brass = 0.5f; out->valve_naive = 0; out->valve_gate = 1;
}

// The lattice load gate (SYNTH §2.5, the CFL analog): λ_max of the stiffness +
// coupling matrix at the highest playable note, the swirl's share included.
// κ here is the TOTAL coupling (the patch's + the swirl's share, up to 0.5).
static float suzu_lambda_max_at(const voxo_t* v, const voxo_suzu_params_t* p, float kappa, int* worst_midi = nullptr) {
    const float rate2 = 2.0f * (float)v->device_rate.load(std::memory_order_relaxed);
    suzu::ModalTable t;
    suzu::modal_table(&t, (int)p->modal_preset, (int)p->modes, p->decay_s, p->decay_bright, p->stiffness, p->pluck, p->bow_position);
    float comp[suzu::MODES_MAX];
    if (!suzu::lattice_compensation(t.ratio, t.n, kappa, comp)) return 1e9f;   // infeasible counts as over the bound (monotone in κ as well)
    return suzu::lattice_lambda_max_over_pitch(t.ratio, t.n, rate2, kappa, worst_midi);
}
static const float SUZU_SWIRL_SHARE = 0.5f;                   // what the swirl (0xA0) may add to the patch's κ

// Step 58: the hybrid's load-time probe (suzu.h) at the device's oversampled rate, and its bound by bisection.
static const float SUZU_PASSIVE_TOL = 1.03f;                 // the conserving junction wobbles to 1.013 (the allpass's state); 1.005 already reads 1.07
static float suzu_hybrid_probe(const voxo_t* v, const voxo_suzu_params_t* p, float bgain) {
    const float rate2 = 2.0f * (float)v->device_rate.load(std::memory_order_relaxed);
    float bhz[suzu::BRIDGE_MAX];
    for (int b = 0; b < suzu::BRIDGE_MAX; b++) bhz[b] = p->bridge_hz * (b == 0 ? 1.0f : b == 1 ? 1.618f : 2.618f);
    return suzu::HybridString::probe_growth((int)p->bridge_cells, bhz, p->bridge_coupling, bgain, rate2);
}
// The passive point is the conserving junction's g = 1 exactly: a reflection scaled UNDER 1 creates energy
// too (the bridge still receives the full force 2s − v̄ while the string sees less of v̄ come back — the
// cross term 2s·v̄·(1 − g) is not a loss but a gain; a real lossy bridge would dissipate it in a resistor,
// which this junction has none of). So the bound is the largest g ABOVE 1 the probe admits.
float voxo_suzu_passive_bound(const voxo_t* v, const voxo_suzu_params_t* params) {
    if (!v || !params || params->voice_kind != 3) return 0.0f;
    float lo = 1.0f, hi = 2.0f;
    if (suzu_hybrid_probe(v, params, lo) > SUZU_PASSIVE_TOL) return 0.0f;
    if (suzu_hybrid_probe(v, params, hi) <= SUZU_PASSIVE_TOL) return hi;
    for (int i = 0; i < 14; i++) { const float mid = 0.5f * (lo + hi); if (suzu_hybrid_probe(v, params, mid) <= SUZU_PASSIVE_TOL) lo = mid; else hi = mid; }
    return lo;
}

float voxo_suzu_coupling_bound(const voxo_t* v, const voxo_suzu_params_t* params) {
    if (!v || !params || params->voice_kind != 1) return 0.0f;
    float lo = 0.0f, hi = 64.0f;                              // the total κ at which λ_max reaches the bound, by bisection (monotone)
    if (suzu_lambda_max_at(v, params, hi) < suzu::LATTICE_BOUND) return hi - SUZU_SWIRL_SHARE;
    for (int i = 0; i < 28; i++) { const float mid = 0.5f * (lo + hi); if (suzu_lambda_max_at(v, params, mid) < suzu::LATTICE_BOUND) lo = mid; else hi = mid; }
    const float b = lo - SUZU_SWIRL_SHARE;                    // the PATCH's bound: the swirl's share reserved
    return b > 0.0f ? b : 0.0f;
}

bool voxo_set_suzu_params(voxo_t* v, const voxo_suzu_params_t* params) {
    if (!v || !params) return false;
    voxo_suzu_params_t p = *params;
    p.level = clampf(params->level, 0.0f, 1.0f);
    p.modes = p.modes < 1u ? 1u : (p.modes > (uint32_t)suzu::MODES_MAX ? (uint32_t)suzu::MODES_MAX : p.modes);
    p.coupling = clampf(params->coupling, 0.0f, suzu::COMP_KAPPA_MAX - SUZU_SWIRL_SHARE);
    suzu::ModalTable table;
    suzu::modal_table(&table, (int)p.modal_preset, (int)p.modes, p.decay_s, p.decay_bright, p.stiffness, p.pluck, p.bow_position);
    if (p.voice_kind == 2 && p.cfl_gate != 0 && p.string_cfl > 1.0f) {     // step 58: the CFL gate (the derived k·dt² never exceeds 1; the forced one is the lab's)
        if (v->log_cb) {
            char msg[200];
            std::snprintf(msg, sizeof msg, "suzu: the patch is rejected — the chain's k·dt² = %.3f exceeds the CFL bound 1 (the explicit scheme is unstable past it: the top mode over Nyquist)", (double)p.string_cfl);
            v->log_cb(2, msg, v->log_user);
        }
        return false;
    }
    if (p.voice_kind == 6 && p.bore_cfl_gate != 0 && p.bore_cfl > 1.0f) {   // step 58b: the bore's CFL gate (the derived λ never exceeds its bound; the forced one is the lab's)
        if (v->log_cb) {
            char msg[200];
            std::snprintf(msg, sizeof msg, "suzu: the patch is rejected — the bore's Courant number is forced to %.3f× its CFL bound (λ²·μ_max < 4: past it the staggered scheme is unstable)", (double)p.bore_cfl);
            v->log_cb(2, msg, v->log_user);
        }
        return false;
    }
    if ((p.voice_kind == 7 || p.voice_kind == 8) && p.valve_gate != 0) {   // step 58c: the valve's load-time probe — the closed loop, damping zeroed, the mouth-power ledger asserted
        const float rate2 = 2.0f * (float)v->device_rate.load(std::memory_order_relaxed);
        suzu::Bore* b = new suzu::Bore; suzu::Valve vv;
        voxo_suzu_params_t q = p; q.bore_loss = 0.0f;                          // the bore's declared losses zeroed (the wall below); the valve keeps its damping — the lip on the reed is structural, and an undamped explicit valve step diverges even where the continuous loop is passive
        float peaks[8]; const int np = p.voice_kind == 8 ? suzu::bore_peaks(suzu::BORE_TRUMPET, p.bell_start, p.bell_gamma, p.bore_loss, p.bore_corner_hz, rate2, 8, peaks) : 0;
        bool grew = false; double mouth = 0.0, worst = 0.0;
        for (int blow = 0; blow < 2 && !grew; blow++) {                            // two blows, soft and hard, each with a retune halfway (a steady blow never troubles the naive form; its instability is kicked by a transient — the phrase's note change found it)
            if (!suzu_wind_build(*b, vv, q, (int)p.voice_kind, 220.0f, 220.0f * p.lip_ratio, rate2, peaks, np, true)) break;
            const float Pm = (blow == 0 ? 0.6f : 2.5f) * SUZU_P_REF; const long N = (long)(0.3f * rate2); mouth = 0.0;
            for (long i = 0; i < N; i++) {
                if (i == N / 2) suzu_wind_build(*b, vv, q, (int)p.voice_kind, 293.66f, 293.66f * p.lip_ratio, rate2, peaks, np, false);   // the transient: a fourth up, the state kept
                suzu_wind_step(*b, vv, Pm, 0.0f, 1.0f, 0.0f, p.valve_naive != 0);
                mouth += (double)Pm * vv.q;
                // past the transient (30 ms); 5 % over the mouth's work is the two staggered energy forms' wobble against a
                // per-step work sum (the reed beating hard read 1.02 with the losses zeroed) — the naive form reads 1e43
                if ((i & 63) == 0 && i > 2880) { const double E = (double)b->energy() + (double)vv.energy(); if (!std::isfinite(E)) { worst = 1e300; grew = true; break; } const double ratio = E / (mouth > 1e-12 ? mouth : 1e-12); if (ratio > worst) worst = ratio; if (ratio > 1.05) { grew = true; break; } }
            }
        }
        delete b;
        if (grew) {
            if (v->log_cb) {
                char msg[240];
                std::snprintf(msg, sizeof msg, "suzu: the patch is rejected — the %s's junction makes energy: with the bore's declared losses zeroed the bore + valve held %.3g× the mouth's work ∫P_mouth·Q at A3 within 300 ms (the ledger's bound is 1, the probe allows 5 %% for the discrete forms' wobble)%s",
                              p.voice_kind == 7 ? "reed" : "lips", worst, p.valve_naive ? " — the naive explicit junction" : "");
                v->log_cb(2, msg, v->log_user);
            }
            return false;
        }
    }
    if (p.voice_kind == 3 && p.passivity_gate != 0) {                        // step 58: the load-time passivity probe (no patch can dodge it)
        const float growth = suzu_hybrid_probe(v, &p, p.bridge_gain);
        if (growth > SUZU_PASSIVE_TOL) {
            if (v->log_cb) {
                char msg[240];
                std::snprintf(msg, sizeof msg, "suzu: the patch is rejected — the hybrid's junction gains: the closed loop's energy rose to %.3f× in 300 ms at C6 with every declared damping zeroed (bridge_gain %.3f; the passive bound is %.4f)",
                              (double)growth, (double)p.bridge_gain, (double)voxo_suzu_passive_bound(v, &p));
                v->log_cb(2, msg, v->log_user);
            }
            return false;
        }
    }
    if (p.voice_kind == 1 && p.lattice_gate != 0) {
        float comp[suzu::MODES_MAX];
        if (!suzu::lattice_compensation(table.ratio, table.n, p.coupling + SUZU_SWIRL_SHARE, comp)) {   // the swirl can add its share
            if (v->log_cb) {
                char msg[240];
                std::snprintf(msg, sizeof msg, "suzu: the patch is rejected — at coupling %.3f + the swirl's %.1f a mode's own stiffness would be gone (the spring between neighbours stiffer than the partial's own); the patch's bound is %.3f",
                              (double)p.coupling, (double)SUZU_SWIRL_SHARE, (double)voxo_suzu_coupling_bound(v, &p));
                v->log_cb(2, msg, v->log_user);
            }
            return false;
        }
        int worst = 0;
        const float lam = suzu_lambda_max_at(v, &p, p.coupling + SUZU_SWIRL_SHARE, &worst);
        if (lam >= suzu::LATTICE_BOUND) {
            if (v->log_cb) {
                char msg[240];
                std::snprintf(msg, sizeof msg, "suzu: the patch is rejected — its coupled lattice's spectral radius %.3f reaches the sampling bound %.0f at MIDI %d (coupling %.3f + the swirl's %.1f); the patch's bound is %.3f",
                              (double)lam, (double)suzu::LATTICE_BOUND, worst, (double)p.coupling, (double)SUZU_SWIRL_SHARE, (double)voxo_suzu_coupling_bound(v, &p));
                v->log_cb(2, msg, v->log_user);
            }
            return false;
        }
    }
    const uint32_t live = v->suzu_live.load(std::memory_order_acquire);
    const uint32_t idle = live ^ 1u;
    v->suzu_params[idle] = p;
    const uint64_t key = suzu_comp_key_of(p);                     // the compensation table beside the patch (the shell's thread; ms to tens of ms at 16 modes)
    if (key == v->suzu_comp_key[live]) {                          // the ratios unchanged: the live table serves
        std::memcpy(v->suzu_comp[idle], v->suzu_comp[live], sizeof v->suzu_comp[idle]);
        std::memcpy(v->suzu_comp_jinv[idle], v->suzu_comp_jinv[live], sizeof v->suzu_comp_jinv[idle]);
    } else suzu_comp_fill(table, v->suzu_comp[idle], v->suzu_comp_jinv[idle]);
    v->suzu_comp_key[idle] = key;
    if (p.voice_kind == 7 || p.voice_kind == 8) {                          // step 58c: the winds' calibration beside the patch (once per embouchure change; ~100 ms)
        const uint64_t wkey = suzu_wind_key_of(p);
        if (wkey == v->suzu_wind[live].key) v->suzu_wind[idle] = v->suzu_wind[live];
        else suzu_wind_calibrate(v, p, v->suzu_wind[idle]);
    }
    v->suzu_live.store(idle, std::memory_order_release);
    return true;
}

void voxo_set_local_control(voxo_t* v, bool on) {
    if (v) v->local_control.store(on ? 1u : 0u, std::memory_order_relaxed);
}

/* The callback's whole body. */
void voxo_render(voxo_t* v, float* out_lr, uint32_t frames) {
    if (!out_lr || frames == 0) return;
    if (!v) { std::memset(out_lr, 0, sizeof(float) * 2u * frames); return; }

    // 0. The rendering thread's floating-point mode (SYNTH §2.4, §4): flush-to-
    //    zero and denormals-are-zero, set once per thread at its first block —
    //    a decaying orbit's tail must not cost a subnormal cliff.
    {
        static thread_local uint32_t ftz_state = 0;   // 0 not yet, 1 on, 2 the platform refused
        if (ftz_state == 0) { suzu::ftz_enable(); ftz_state = suzu::ftz_enabled() ? 1u : 2u; }
        if (v->ftz_set.load(std::memory_order_relaxed) != ftz_state) v->ftz_set.store(ftz_state, std::memory_order_relaxed);
    }
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
    {   // step 56: the source — a switch ends every voice, as the swap does
        const uint32_t src = v->source.load(std::memory_order_relaxed);
        if (src != v->source_now) {
            v->source_now = src;
            for (uint32_t i = 0; i < v->max_voices; i++) { v->voices[i].active = false; for (uint32_t k = 0; k < MAX_LAYERS; k++) v->voices[i].layers[k].active = false; }
        }
    }
    const voxo_suzu_params_t& sp = v->suzu_params[v->suzu_live.load(std::memory_order_acquire)];
    if (sp.mod_target != 0 && v->source_now != VOXO_SOURCE_SAMPLER) {       // step 58: the chaotic modulator at control rate
        const float block_s = (float)frames / (float)v->device_rate.load(std::memory_order_relaxed);
        int n = (int)(block_s * 4000.0f * sp.mod_rate + 0.5f); if (n < 1) n = 1; if (n > 64) n = 64;   // a unit of pendulum time is 50 ms / mod_rate
        v->pendulum.step(n, true);
        const float coef = 1.0f - std::exp(-block_s / 0.020f);
        v->mod_smooth += ((float)v->pendulum.out() - v->mod_smooth) * coef;
    }

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
        bool suzu_alive = false;                                              // step 58: the layered source renders both
        if (v->source_now != VOXO_SOURCE_SAMPLER) {
            suzu_alive = render_suzu(v, vc, sp, out_lr, frames, freq_from, freq_step, pressure_live, rate);
            if (v->source_now == VOXO_SOURCE_SUZU) {
                if (!suzu_alive) { vc.active = false; continue; }
                vc.freq = vc.freq_target;
                active++;
                continue;
            }
        }
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
            if (!any && !suzu_alive) { vc.active = false; continue; }
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
            if (vc.releasing && level < 1e-4f && !suzu_alive) { vc.active = false; continue; }
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

// Step 59 (SYNTH §2.7): the orbit trace's mask and poll.
void voxo_set_trace(voxo_t* v, uint32_t kinds_mask) {
    if (v) v->trace_mask.store(kinds_mask, std::memory_order_relaxed);
}
// The curvature-weighted decimation: of n points keep at most K + 1 — the first, the last, and the points where the
// running weight (the turning angle at each point plus half the step length in orbit radii) crosses each K-th of its
// total, so the bends get the vertices and a straight run gets few. Returns the count kept.
static uint32_t trace_decimate(const float* x, const float* y, uint32_t n, float amp, uint32_t K, float* ox, float* oy) {
    if (n < 2) return 0;
    if (n <= K + 1) { for (uint32_t i = 0; i < n; i++) { ox[i] = x[i] / amp; oy[i] = y[i] / amp; } return n; }
    double total = 0.0; float w[TRACE_RING];
    for (uint32_t i = 0; i < n; i++) {
        float t = 0.0f;
        if (i > 0 && i + 1 < n) {
            const float ax = x[i] - x[i - 1], ay = y[i] - y[i - 1], bx = x[i + 1] - x[i], by = y[i + 1] - y[i];
            const float la = std::sqrt(ax * ax + ay * ay), lb = std::sqrt(bx * bx + by * by);
            if (la > 1e-12f && lb > 1e-12f) { float c = (ax * bx + ay * by) / (la * lb); c = c > 1.0f ? 1.0f : (c < -1.0f ? -1.0f : c); t = std::acos(c); }
            t += 0.5f * lb / amp;
        }
        w[i] = t; total += t;
    }
    uint32_t m = 0; ox[m] = x[0] / amp; oy[m] = y[0] / amp; m++;
    if (total > 1e-9) {
        double run = 0.0; uint32_t next = 1;
        for (uint32_t i = 1; i + 1 < n && m < K; i++) {
            run += w[i];
            if (run >= (double)next * total / (double)K) { ox[m] = x[i] / amp; oy[m] = y[i] / amp; m++; next++; }
        }
    }
    ox[m] = x[n - 1] / amp; oy[m] = y[n - 1] / amp; m++;
    return m;
}
uint32_t voxo_trace_poll(voxo_t* v, voxo_trace_t* out, uint32_t max_voices, uint32_t max_segments) {
    if (!v || !out || max_voices == 0) return 0;
    const uint32_t mask = v->trace_mask.load(std::memory_order_relaxed);
    if (mask == 0) return 0;
    uint32_t K = max_segments < 1 ? 1 : (max_segments > VOXO_TRACE_POINTS_MAX - 1 ? VOXO_TRACE_POINTS_MAX - 1 : max_segments);
    const uint32_t kind = v->suzu_params[v->suzu_live.load(std::memory_order_acquire)].voice_kind;
    if (((mask >> kind) & 1u) == 0) return 0;
    uint32_t count = 0;
    float bx[TRACE_RING], by[TRACE_RING];
    for (uint32_t i = 0; i < v->max_voices && count < max_voices; i++) {
        const Voice& vc = v->voices[i];
        if (!vc.active) continue;
        const SuzuVoice& sz = vc.suzu;
        const uint32_t w = sz.twrite.load(std::memory_order_acquire);
        if (v->trace_serial[i] != vc.serial) { v->trace_serial[i] = vc.serial; v->trace_read[i] = w > TRACE_RING / 4 ? w - TRACE_RING / 4 : 0; }   // a new voice: start a quarter ring back at most
        uint32_t rd = v->trace_read[i];
        if (w - rd > TRACE_RING - 1) rd = w - (TRACE_RING - 1);                  // fell behind: the oldest still in the ring
        const uint32_t n = w - rd;
        voxo_trace_t& t = out[count++];
        t.channel = vc.channel; t.note = vc.note; t.voice_kind = (uint8_t)kind; t.held = vc.held ? 1 : 0; t.serial = vc.serial;
        t.amplitude = 0.0f; t.count = 0;
        if (n < 2) continue;
        float amp = 0.0f;
        for (uint32_t k = 0; k < n; k++) { const uint32_t j = (rd + k) % TRACE_RING; bx[k] = sz.tx[j]; by[k] = sz.ty[j]; const float r = bx[k] * bx[k] + by[k] * by[k]; if (r > amp) amp = r; }
        v->trace_read[i] = w;
        amp = std::sqrt(amp);
        if (!(amp > 1e-7f) || !std::isfinite(amp)) continue;
        t.amplitude = amp;
        t.count = trace_decimate(bx, by, n, amp, K, t.x, t.y);
    }
    return count;
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
    out->source = v->source.load(std::memory_order_relaxed);
    out->ftz    = v->ftz_set.load(std::memory_order_relaxed);
    out->suzu_modes = v->suzu_params[v->suzu_live.load(std::memory_order_acquire)].voice_kind == 0 ? 0u : v->suzu_params[v->suzu_live.load(std::memory_order_acquire)].modes;
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

void voxo_core_callback_timing(voxo_t* v, uint32_t frames, double gap_frames, double render_ms) {
    v->callbacks.fetch_add(1, std::memory_order_relaxed);
    v->render_last_ms.store((float)render_ms, std::memory_order_relaxed);
    if ((float)render_ms > v->render_max_ms.load(std::memory_order_relaxed))
        v->render_max_ms.store((float)render_ms, std::memory_order_relaxed);
    if (v->warmup_callbacks > 0) { v->warmup_callbacks--; return; }
    // The XRun proxy (#3): a callback later than half a callback past its
    // expected time, or a render longer than the callback. The expected time
    // is the DEVICE's period when that exceeds the callback (#39): WASAPI
    // shared mode wakes the worker every 480 frames and miniaudio's fixed-size
    // callback then delivers 256 + 224 back to back — gaps of a period and of
    // nothing, both on time. Where callback and period agree (CoreAudio,
    // AAudio) this is the rule as it was.
    const uint32_t device_block = v->device_block.load(std::memory_order_relaxed);
    const double expected_frames = (double)(device_block > frames ? device_block : frames);
    const double late_by_frames = gap_frames - expected_frames;
    const double period_ms = 1000.0 * (double)frames / (double)v->device_rate.load(std::memory_order_relaxed);
    if (late_by_frames > 0.5 * (double)frames || render_ms > period_ms)
        v->xruns.fetch_add(1, std::memory_order_relaxed);
}

void** voxo_core_backend_slot(voxo_t* v) { return &v->backend; }
