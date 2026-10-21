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
#define VOXO_VERSION_MINOR 24
#define VOXO_VERSION_PATCH 0   // 0.24.0: the Boris–leapfrog magnetic gyroscope (step 84, DECISIONS_13 #2); 0.23.0: the Toda lattice soliton ring (step 83, DECISIONS_13 #1); 0.22.0: the editable modal table, the Scala tuning and the unison pair (step 82, DECISIONS_12 #6); 0.21.0: the acoustic bore v2 — the toneholes, the two-mode lips, the brass shock and its limiter, the leak and the reed's inertance (step 81, DECISIONS_12 #5); 0.20.0: the stick-slip bow and the strike position from the cell (step 80, DECISIONS_12 #4); 0.19.0: the membrane, the bus plate and the gong (step 79, DECISIONS_12 #3); 0.18.0: the four-lane engine and the ×4 oversampler (step 78, DECISIONS_12 #2); 0.17.0: the Poincaré strobe (step 75); 0.16.1: the winds' press is the mouth alone (DECISIONS_9 #16)

using voxo_inst::Instrument;
using voxo_inst::Zone;
using voxo_inst::Group;
using voxo_inst::GroupState;
using voxo_inst::Target;

namespace {

constexpr uint32_t MAX_VOICES_CAP = 64;
constexpr uint32_t TRACE_RING  = 1024;   // step 59: ~85 ms of orbit at the sub-rate's eighth (96 kHz / 8 = 12 kHz)
constexpr uint32_t TRACE_DECIM = 8;      //   the default density; voxo_set_trace_decimation sets it (59c: the lab's portraits at 1 or 2)
constexpr uint32_t STROBE_RING = 256;    // step 75: the strobe ring per voice — the returns at the reference phasor's wraps (~2 s at 110 Hz)
constexpr uint32_t STROBE_BLOCK_MAX = 64;//   the wraps precomputed per block (a bass above 24 kHz at 128 frames would overflow it: nonsense anyway)
constexpr uint32_t MAX_LAYERS     = 8;       // zones a performance voice stacks at once
constexpr uint32_t EVENTS_PER_BLOCK = 512;   // drained at block start; the ring keeps the rest for the next block
constexpr float    SINE_AMPLITUDE   = 0.18f; // one sine at full level; 16 of them soft-clip, never wrap
constexpr float    SAMPLE_AMPLITUDE = 0.40f; // one sample layer at full level
constexpr float    SUZU_END_AMP     = 1e-4f; // −80 dB: a released cell below this ends
// Step 79 (TO_PROJECT_SPEC §13.9): the membrane's and the bus plate's numbers.
constexpr uint32_t PLATE_SRC_MAX    = 1024;  // the voices' blocks kept for the plate's injection (a larger block skips the plate)
constexpr float    SUZU_DRUM_STRIKE = 24.0f; // the strike's momentum per unit of level × ε₁₁ — the pickup's peak reads the level within a few dB across the keyboard (measured: 0.25…0.5 at level 0.3)
constexpr float    PLATE_DRIVE      = 1.0f;  // a voice's sample into its node at 64², scaled by (n/64)² (measured: the response goes as 1/n²; 32² reads within 1 dB of 64²)
constexpr float    PLATE_NORM       = 0.045f;// the plate's pickups into the mix at plate_gain 1: 10 dB under the dry harp chord (measured: +17 dB over it at 1)
constexpr float    PLATE_PICK_LX = 0.33f, PLATE_PICK_LY = 0.62f, PLATE_PICK_RX = 0.67f, PLATE_PICK_RY = 0.38f;   // the two pickups, mirrored
// Step 80 (TO_PROJECT_SPEC §13.10): the stick-slip bow's scales — the speed per unit of level (the Helmholtz amplitude reads the
// level at full mouth), the force per unit of speed (bow_force 0.5 inside the Helmholtz range), the curve's width v₀ as a
// share of the speed; measured on the chain and on the lattice (the probe of DECISIONS_12 #4)
constexpr float    SUZU_BOW_KV_CHAIN = 2.3f, SUZU_BOW_KF_CHAIN = 6.0f;
constexpr float    SUZU_BOW_KV_LAT   = 1.0f, SUZU_BOW_KF_LAT   = 1.0f;
constexpr float    SUZU_BOW_KV_HYB   = 0.45f, SUZU_BOW_KF_HYB  = 15.0f;   // the waveguide: the loop's velocity waves, Z = 1
constexpr float    SUZU_BOW_V0       = 0.1f;
// Step 83: the Toda ring's scales — the strike's strain amplitude per unit level (the output divides it back), the
// nonlinearity at full press (toda_b 1), the swirl's impulse in units of the sound speed √k.
constexpr float    SUZU_TODA_STRAIN  = 4.0f;
constexpr float    SUZU_TODA_BMAX    = 4.0f;
constexpr float    SUZU_TODA_SWIRL_J = 0.25f;
// THE RESOLUTION RULE (step 83, measured on the ring alone): the leapfrog resolves the exponential along the trajectory only
// while a step's displacement over the exponential's length, √k·ĉ·b (ĉ the compression's peak), stays small — the ring makes
// energy past 0.5 uncapped, past 0.22 with the cap engaged (C7, b 2.4: +2 dB a second at 0.24, +37 dB at 0.32), whatever
// the cap's shape; the ring's inner steps a sub-step rise (√k ∝ 1/m) to hold it under SUZU_TODA_RES, and where m_max cannot
// reach it the nonlinearity is CAPPED FOR THE NOTE at the strike (the pulse self-focuses to ~1.3× its struck peak: the
// allowance) — a cap, not a feedback: a nonlinearity that followed the live peak re-shaped the potential under the pulse and
// pumped it (C7 at level 1 blew up under the rule).
static constexpr float SUZU_TODA_RES = 0.15f;
static constexpr float SUZU_TODA_FOCUS = 1.3f;
// Step 84: the gyroscope's field at most (Hz) — the doublet's split; past it the lower line falls out of the note's reach.
static constexpr float SUZU_BORIS_BMAX = 50.0f;
#ifdef VOXO_MOBILE
static constexpr int   SUZU_TODA_M_MAX = 4;   // the tablets' tier: the inner steps a 2× sub-step at most 4 (×8) — the note's ceiling on b does the rest there
#else
static constexpr int   SUZU_TODA_M_MAX = 8;   // the inner steps a 2× sub-step, even: 2 (the ring at ×4) to 8 (×16)
#endif
constexpr float    SUZU_LEAK_ROOM    = 0.96f;   // step 81: a leaky bell's reactance sits outside the Laplacian the bound is read from — the sax at G7 (13 cells) blew up at λ 0.989 under bore_leak 0.4; 0.96 of the bound holds the range (measured 94..108 at 0.3 / 0.4 / 0.5)
constexpr float    BORE_LEAK_HZ      = 40.0f;   // step 81: the sax's bell's low-frequency leak's corner (the flute and the trumpet take none: measured, the leak took 22 dB off the flute's C2 and 7 off the trumpet's — their bass is the radiation's near-lossless)
constexpr float    SUZU_SHOCK_KAPPA  = 1.7f;    // step 81: (γ+1)/2ρc² in the bore's units — 1 kPa ≈ P_REF 0.005: a 5 kPa crest runs 4 % faster
constexpr float    SUZU_SHOCK_CQ     = 0.1f;   //   the Lapidus viscosity's coefficient (measured against the front's roughness and the tilt)
// Step 81: A HOLE'S INERTANCE against a bore cell's — a flute's hole (10 mm across, 8 mm effective height) in a 19 mm bore at
// the grid's 3.6 mm cells is 8 cells' worth: 1/L_h = HOLE_H0·S at the node per unit hole_size; the scheme holds at λ 0.99 of
// the bound up to 4× that (measured on the bare bore: 1/L_h = S/2 stable, S not) and the mode takes Bore::holes_room besides.
// The vent is a quarter of a hole.
constexpr float    HOLE_H0          = 0.125f;
constexpr float    HOLE_VENT_H0     = 0.011f;   // the register key: a 2 mm vent in a 15 mm neck, 5 mm deep — 90 cells' inertance, at an eighth of the length
constexpr float    SUZU_SHOCK_PCAP   = 0.1f;    //   the steepening saturates at 20 reference pressures (the trumpet's bore at fff reads 16–18)
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
    suzu::TodaRing ring;           // step 83: the Toda lattice soliton ring
    float toda_k, toda_b, toda_swirl;   //   its linear stiffness at the note (per inner step), the nonlinearity this block, the swirl the last impulse saw
    int   toda_m;                      //   the inner steps a 2× sub-step (the resolution rule)
    float toda_b_want, toda_b_cap;     //   the nonlinearity the press asked (k[7]; k[1] what the rule allowed) and the note's ceiling on it
    suzu::Gyro gyro;                   // step 84: the Boris gyroscope
    suzu::Svf  svf2;                   //   its right channel's filter (the voice is stereo)
    float boris_hz, boris_rate2;       //   its field this block, Hz; the sub-step rate it was fitted at
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
    float      bore_mu;        // step 81: the flute's μ_max, taken once per fresh bore (the power iteration cost every strike 40 µs: a burst of fifteen missed the callback)
    // Step 58c: the winds
    suzu::Valve valve;
    float      wind_dyn;       // 58c: the sax's breath-following level (the declared dynamics)
    int        hole_j, hole_reg;   // step 81: the toneholes — the note's semitone above the register's base and the register (−1 = the v1 cut, no holes)
    uint32_t   hole_bits;      //   the keys at the strike
    float      dc_x1, dc_y1;   // the output's DC block (a closed reed end pushes a mean flow out of the bell)
    suzu::HalfBand hb[2];      // step 78: the ×4 oversampler's two 2:1 stages (the cell family at oversample 4)
    // Step 79: the membrane (voice_kind 9) — the per-voice mini-membrane and the strike's numbers (the cap, E_max, the CFL
    // number the gate read, the ε the grid admits, the held contraction the release tightens) — and every kind's point on
    // the bus plate
    suzu::Membrane<suzu::MEMBRANE_MAX> drum;
    float      drum_cap;       // H_cap: the plain energy the strike is admitted under (drum_cap × one full strike's)
    float      drum_emax;      // E_max = 2H_cap/(c² + κ²λ₁₁)
    float      drum_cfl;       // λ_2D at E_max, as the gate read it
    float      drum_eps_max;   // the ε the grid admits under the gate (a glide saturates there)
    float      drum_eps;       // ε₁₁ as tuned
    float      drum_g0, drum_g2;   // the held contraction; the release multiplies it per block
    float      plate_x, plate_y;   // the bus plate's injection point for this voice (the shell's, or the default map's)
    // Step 80: the stick-slip bow's reading (the force this sub-step, the speed, the work ∫f·v_b since the strike — the
    // ledger's right side; nothing in the render reads them) and the strike position the cell gave (else the patch's pluck)
    float      bow_f, bow_vb;
    double     bow_work, bow_gain;   // ∫f·v_b the bow's work; ∫f·v what the string took (v at the midpoint of the force's step) — gain ≤ work is the ledger
    int        bow_node;
    bool       bow_stuck;      // the friction's branch (the hysteresis of StickSlip::solve)
    float      strike_pos;
    // Step 59: the orbit trace's ring — the rendering thread writes one point in TRACE_DECIM sub-steps when the
    // kind is traced; the poll reads from the other side (twrite is the only shared word; the reader keeps its own cursor)
    float      tx[TRACE_RING], ty[TRACE_RING];
    float      tu[TRACE_RING], tw[TRACE_RING];   // step 59c: the kind's two aux channels (voxo.h: the valve, the rotor's momentum, the drive's phase …)
    std::atomic<uint32_t> twrite;
    uint32_t   tdecim;         // the decimation counter
    float      tprev;          // the last traced output (the chains' derivative pair)
    // Step 59b: the last block's flute values the inspection reports (written per block; nothing reads them in the render)
    float      ins_pm, ins_u0, ins_tau, ins_gain, ins_area;
    double     work;           // step 59c: the winds' mouth work ∫P_mouth·Q since the strike (the ledger's left side; nothing in the render reads it)
    // Step 75 (KESSHO §2.5): the strobe ring — the voice's pair (the trace's, with the kind's aux pair) at each wrap of the
    // reference phasor, or of the voice's own drive clock, interpolated between the two sub-steps the wrap falls between;
    // swrite is the shared word (the poll keeps its own cursor), sreturns the count since the strike; nothing allocates
    float      sx[STROBE_RING], sy[STROBE_RING], su[STROBE_RING], sw[STROBE_RING];
    std::atomic<uint32_t> swrite;
    uint32_t   sreturns;
    float      sprev[4];       // the last sub-step's pair and aux: the interpolation's left end
    float      strobe_hz;      // the clock the voice was strobed by over the last block (the poll reports it)
};

// Step 83: THE RING'S FIT at the note, every block and at the strike — the inner steps a 2× sub-step m (even, 2..M_MAX) from
// the resolution rule √k_m·ĉ·b ≤ RES, the nonlinearity limited where m_max cannot reach it, the cap under the stiffness bound
// k·cap ≤ 0.9 (the soft cap's stiffness peaks at k·cap/2: 2√0.45 of the leapfrog's 2). A change of m rescales the ring's
// momenta (exact). toda_gate 0: the ring as written — m 2, the cap and b as asked — the red control.
static void suzu_toda_fit(SuzuVoice& sz, const voxo_suzu_params_t& sp, float freq, float rate2, float b_want, bool struck) {
    const int n = sz.ring.n; const float pk = sz.ring.strain_peak(), peak = pk > 1e-3f ? pk : 1e-3f;
    int m = 2; float k = suzu::TodaRing::k_for(freq, (float)m * rate2, n); float b = b_want;
    if (sp.toda_gate != 0u) {
        const float k8 = suzu::TodaRing::k_for(freq, (float)SUZU_TODA_M_MAX * rate2, n);
        if (struck) sz.toda_b_cap = SUZU_TODA_RES / (sqrtf(k8) * SUZU_TODA_FOCUS * peak);            // the note's ceiling, from the struck pulse
        else if (sqrtf(k8) * peak * (b_want < sz.toda_b_cap ? b_want : sz.toda_b_cap) > SUZU_TODA_RES) sz.toda_b_cap = SUZU_TODA_RES / (sqrtf(k8) * peak);   // the pulse outgrew the allowance: lowered, never raised
        if (b > sz.toda_b_cap) b = sz.toda_b_cap;
        while (m < SUZU_TODA_M_MAX && sqrtf(k) * peak * b > SUZU_TODA_RES) { m += 2; k = suzu::TodaRing::k_for(freq, (float)m * rate2, n); }
    }
    float cap = expf(sp.toda_cap); if (sp.toda_gate != 0u && cap > 0.9f / k) cap = 0.9f / k;
    if (m != sz.toda_m) { sz.ring.rescale((double)sz.toda_m / (double)m); sz.toda_m = m; }
    sz.toda_k = k; sz.toda_b = b; sz.toda_b_want = b_want;
    sz.ring.setup(k, b, cap);
}

// Step 84: THE GYROSCOPE'S FIT at the block — the field from the poly pressure, the spring's step and the rotation's angle
// prewarped (or raw, the lab's red), the cyclotron frequency for the own-clock strobe.
static void suzu_boris_fit(SuzuVoice& sz, const voxo_suzu_params_t& sp, float freq, float rate2, float swirl) {
    const float dt = 1.0f / rate2;
    const float sw = swirl < 0.0f ? 0.0f : swirl > 1.0f ? 1.0f : swirl;
    float fb = sp.boris_b_rest + (sp.boris_b - sp.boris_b_rest) * sw; if (fb < 0.0f) fb = 0.0f; if (fb > SUZU_BORIS_BMAX) fb = SUZU_BORIS_BMAX;
    const float w0 = 2.0f * suzu::PI * freq, wb = 2.0f * suzu::PI * fb;
    float eps, theta;
    if (sp.boris_prewarp != 0u) { eps = suzu::prewarp(w0, dt); theta = wb * dt; }
    else { eps = w0 * dt > 1.9f ? 1.9f : w0 * dt; theta = 2.0f * atanf(0.5f * wb * dt); }   // the raw coefficients: the leapfrog flat, the Boris angle short
    sz.boris_hz = fb; sz.boris_rate2 = rate2; sz.gyro.setup(eps, theta);
}

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
    std::atomic<uint32_t> trace_decim;       // step 59c: one ring point in this many sub-steps (1..64; 0 at create = TRACE_DECIM)
    uint32_t trace_read[MAX_VOICES_CAP];     //   the poll's cursor per slot (the reader's own)
    uint32_t trace_serial[MAX_VOICES_CAP];   //   the voice serial the cursor belongs to
    std::atomic<uint32_t> strobe_mode;       // step 75: the strobe's clock — 0 off, 1 the bass phasor, 2 each voice's own drive where it has one
    std::atomic<float>    strobe_hz_out;     //   the phasor's frequency over the last block (voxo_strobe_hz)
    uint32_t strobe_read[MAX_VOICES_CAP];    //   the strobe poll's cursor per slot, and the serial it belongs to
    uint32_t strobe_serial[MAX_VOICES_CAP];
    std::atomic<uint32_t> suzu_live;         // which suzu_params slot the callback reads
    voxo_suzu_params_t    suzu_params[2];    // the shell writes the idle slot and flips
    float                 shear_ratio[2][suzu::SHEAR_TABLE];   // step 56: the shears' measured detune per gain (cubic, triangle), filled at create
    float                 suzu_comp[2][suzu::COMP_POINTS][suzu::MODES_MAX];   // step 57: the coupling's detune compensation per κ (c²), beside its patch slot
    float                 suzu_comp_jinv[2][suzu::COMP_POINTS][suzu::MODES_MAX * suzu::MODES_MAX];   // and J⁻¹ per κ: the per-pitch correction's matrix
    uint64_t              suzu_comp_key[2];        // what the table depends on (the ratios: preset, modes, stiffness) — a knob that keeps them reuses it
    suzu::DoublePendulum  pendulum;                // step 58: the patch's chaotic modulator (control rate, the callback's thread)
    float                 mod_smooth;              // its output, smoothed per block
    struct WindCal { float peaks[8]; int npeaks; float offset[4]; float mu64; uint64_t key; } suzu_wind[2];   // step 81: mu64 the profile's μ_max on the fresh 64-cell bore, once per patch (two power iterations a strike had cost the trumpet's storm its XRuns)   // step 58c: per patch slot — the kind's intonation at C3, C4, C5, C6 (cents)
    struct HoleCal { float corr[2][12]; float vent[2]; uint64_t key; } suzu_hole[2];   // step 81: per patch slot — each hole's placement correction in cents per register (the woodwind maker's), the vent's detune per register
    uint32_t keys;                   // step 81: the fingering keys as the CCs last said (the core's nine bits), the callback's
    float    flute_mu64;             // step 81: the open–open cylinder's μ_max on the 64-cell bore every flute strike builds from, once per engine
    float    tuning_hz[2][128];      // step 82: THE TUNING — a note's frequency per slot (the live slot read at every retune; the set fills the idle one and flips)
    std::atomic<uint32_t> tuning_live, tuning_count;   // tuning_count 0: equal temperament (the table unread)
    char     tuning_name[96];        // the scale's description line
    std::atomic<uint32_t> ftz_set;           // the rendering thread set FTZ/DAZ (stats)
    std::atomic<Instrument*> pending_inst;   // the next instrument (or &g_none = clear); the callback takes it
    std::atomic<Instrument*> retired_inst;   // what the callback let go; the shell frees it
    // Callback -> shell.
    std::atomic<uint32_t> active_voices;
    std::atomic<uint32_t> active_layers;
    std::atomic<uint32_t> callbacks;
    std::atomic<uint32_t> xruns;
    std::atomic<uint32_t> breaker_trips;    // step 75: non-finite blocks the breaker cut (voxo_stats)
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
    uint32_t suzu_kind_now;          // step 75: the voice kind the sounding voices were built for (a patch of another kind ends them)
    double   strobe_phase;           // step 75: the reference phasor, turns in [0, 1)
    float    strobe_hz;              //   its frequency this block (0 = no bass: nothing sounds)
    uint32_t strobe_n;               //   this block's wraps: the sub-step each completes in and the fraction into it
    uint32_t strobe_s[STROBE_BLOCK_MAX];
    float    strobe_f[STROBE_BLOCK_MAX];
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
    // Step 79: THE BUS PLATE — one membrane on the bus at the output rate, every Suzu voice injecting its sample at its
    // point; allocated at create (52 KB), stepped by the callback; the voices' mono blocks beside it for the injection
    suzu::Membrane<suzu::PLATE_MAX>* plate;
    float*   plate_src;              // max_voices × PLATE_SRC_MAX: each voice's block, read after the voice loop
    bool     plate_live;             // the plate holds state (cleared when it turns on, or its size changes)
    uint32_t plate_n;                // the size it was set up at
    float    plate_hz_set, plate_kappa_set, plate_decay_set, plate_bright_set;   // the tuning it was set up at (recomputed on a change)
    float    strike_x[16][128], strike_y[16][128];   // voxo_set_strike_point: the shell's point per (channel, note); < 0 = the default map
    float    strike_pluck[16][128];                  // step 80, voxo_set_strike_pluck: the strike position per (channel, note); < 0 = the patch's
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
    const float bend = v->channels[vc.channel & 15].bend_semitones + zone_bend;
    if (v->tuning_count.load(std::memory_order_acquire) != 0u)                                 // step 82: the scale's frequency for the note, the bend an equal-tempered offset on it
        vc.freq_target = v->tuning_hz[v->tuning_live.load(std::memory_order_acquire)][vc.note & 127] * std::exp2(bend / 12.0f);
    else vc.freq_target = note_to_hz((float)vc.note + bend);
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
    if (p.modal_preset == 5u) {                                             // step 82: the table's ratios in the key (the stiffness is unread under it)
        uint32_t h = 2166136261u;
        for (int k = 0; k < VOXO_MODAL_MAX; k++) { uint32_t u; std::memcpy(&u, &p.modal_ratio[k], 4); h ^= u; h *= 16777619u; }
        sb = h ^ (p.modal_comp << 31);
    }
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
static int suzu_wind_build(suzu::Bore& b, suzu::Valve& vv, const voxo_suzu_params_t& sp, int kind, float f_target, float lip_hz, float rate2, const float* peaks, int npeaks, bool fresh, float mu64 = 0.0f) {
    const bool sax = kind == 7;
    const int profile = sax ? suzu::BORE_CONE : suzu::BORE_TRUMPET;
    const float apex = sax ? sp.cone_apex : sp.bell_start, gamma = sax ? 0.0f : sp.bell_gamma;
    const float corner = suzu_wind_corner(sp, kind, f_target);
    if (fresh) { b.setup(64, profile, apex, gamma, suzu::END_CLOSED, 0.0f, suzu::END_OPEN, sp.bore_loss); vv.reset(); }
    b.radiation_corner(corner, rate2);
    const float shock = sax ? 0.0f : sp.brass_shock;                                                                                   // step 81: the shock is the brass's (the sax's cone keeps the v1 bore: measured, the shock cost it 80 % of its render for no claim of the spec's)
    const float leak = sax ? sp.bore_leak : 0.0f;                                                                                        // step 81: the leak is the sax's bell's
    const float lam_max = suzu::Bore::lambda_bound(mu64 > 0.0f ? mu64 : b.mu_max()) * 0.99f / suzu::Bore::shock_room(shock * SUZU_SHOCK_KAPPA, SUZU_SHOCK_PCAP)   // step 81: room for the crests' speed; μ cached per patch
                        * (sax && sp.tonehole_mode == 1u ? suzu::Bore::holes_room(HOLE_H0 * sp.hole_size) : 1.0f)                                  //   and for the holes
                        * (leak > 0.0f ? SUZU_LEAK_ROOM : 1.0f);                                                                                     //   and for the leak's compliance at the bell (G7 at 13 cells blew up at 0.999 of the bound under it)
    const float s_end = suzu::Bore::profile_s(profile, 1.0f, apex, gamma);
    const float ends = suzu::Bore::end_correction(sp.bore_loss, b.rad_a, f_target, rate2, s_end) + suzu::Bore::leak_correction(leak, BORE_LEAK_HZ, f_target, rate2, s_end);   // step 81: the leak's reactance
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
    b.retune(cells, lam, profile, apex, gamma); b.radiation_corner(corner, rate2); b.leak(leak, BORE_LEAK_HZ, rate2);   // step 81
    b.shock(shock * SUZU_SHOCK_KAPPA, sp.shock_limiter != 0 ? SUZU_SHOCK_CQ : 0.0f, SUZU_SHOCK_PCAP);                           // step 81: the trumpet steepens
    if (sax) vv.setup(sp.reed_hz, rate2, sp.reed_q, sp.reed_open, sp.reed_close * SUZU_P_REF, sp.reed_area, 0, 2000.0f);   // the reed's turbulence low-passed at 2 kHz
    else   { vv.setup(lip_hz, rate2, sp.lip_q, sp.lip_open, sp.lip_close * SUZU_P_REF, sp.lip_area, 1, 0.0f); vv.setup_shear(sp.lip_shear, sp.lip_shear_ratio, sp.lip_q, rate2); }   // the lips' white (the registers were tuned on it); step 81: the shearing mode
    return cells;
}
// One sub-step of a wind: the implicit junction (or the naive one), the bore, the valve. Returns the mouth end's velocity.
static inline float suzu_wind_step(suzu::Bore& b, suzu::Valve& vv, float Pm, float sigma, float wall, float shear, bool naive, float Lr = 0.0f) {
    const float a = vv.A * vv.opening();
    const float swept = vv.swept();                                           // the valve's own volume flow into the bore, in the force's direction
    const float dp_prev = Pm - b.p[0];
    const float noise = sigma * Pm * vv.noise();
    const float Z = b.end_impedance();
    // step 81: the reed channel's acoustic mass — Δp = q²/a² + Z·q + L_r·(q − q_prev): the same quadratic with Z + L_r and
    // the previous flow's L_r·q_prev on the pressure's side (the air in the channel carries momentum)
    const float q = naive ? suzu::Valve::flow_naive(a, dp_prev + noise) : suzu::Valve::flow_implicit(a, Pm - b.end_pressure_ahead() - Z * swept + noise + Lr * vv.q_ap, Z + Lr);
    const float out = b.step(q + swept, 0.0f, wall, shear);
    vv.q_ap = q;
    vv.q = q + swept;                                                          // the mouth's flow: the aperture's and the swept
    vv.step(Pm - b.p[0]);
    return out;
}
// The intonation at a note: blown 0.45 s at 1.4 reference pressures (the breath map's middle), the pitch of the last
// 0.2 s against the note, in cents (0 if silent). The calibration takes it at C3, C4, C5 and C6 and lerps in octaves.
static float suzu_wind_offset(const voxo_suzu_params_t& sp, int kind, float rate2, const float* peaks, int npeaks, float hz, float mu64) {
    suzu::Bore* b = new suzu::Bore; suzu::Valve vv;
    if (!suzu_wind_build(*b, vv, sp, kind, hz, hz * sp.lip_ratio, rate2, peaks, npeaks, true, mu64)) { delete b; return 0.0f; }
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
// Step 81: the blown pitch of a bore as set (the holes placed), in cents against `hz` — 0.4 s at the breath map's middle, the
// last 0.15 s read; 0 if silent or a register away. The hole calibration's second pass.
static float suzu_blow_offset(suzu::Bore& b, suzu::Valve& vv, const voxo_suzu_params_t& sp, int kind, float rate2, float hz) {
    const float Pm = 1.4f * SUZU_P_REF, wall = suzu_wind_wall(sp, kind, hz, rate2);
    const long N = (long)(0.4f * rate2), M = (long)(0.15f * rate2);                // the sax's upper register takes a third of a second to settle
    float* rec = new float[M]; b.reset(); vv.reset();
    for (long i = 0; i < N; i++) { const float o = suzu_wind_step(b, vv, Pm, kind == 7 ? sp.reed_noise : 0.02f, wall, 0.0f, false, kind == 7 ? sp.reed_inertance : 0.0f); if (i >= N - M) rec[i - (N - M)] = o; }
    float pk = 0.0f; for (long i = 0; i < M; i++) pk = std::fmax(pk, std::fabs(rec[i]));
    float off = 0.0f;
    if (pk > 1e-4f && std::isfinite(pk)) {                                     // the strongest line within ±300 cents of the note (coarse, then fine): the note itself, or none
        double best = -1.0; float bc = 0.0f;
        for (float c = -300.0f; c <= 300.0f; c += 6.0f) { const double m = suzu_goertzel(rec, (size_t)M, 0.0, hz * exp2f(c / 1200.0f), rate2); if (m > best) { best = m; bc = c; } }
        for (float c = bc - 6.0f; c <= bc + 6.0f; c += 1.0f) { const double m = suzu_goertzel(rec, (size_t)M, 0.0, hz * exp2f(c / 1200.0f), rate2); if (m > best) { best = m; bc = c; } }
        off = bc;
    }
    delete[] rec;
    return off;
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
    { suzu::Bore* b = new suzu::Bore; const bool sax = sp.voice_kind == 7;   // step 81: the profile's μ_max once, on the fresh 64-cell bore every strike builds from
      b->setup(64, sax ? suzu::BORE_CONE : suzu::BORE_TRUMPET, sax ? sp.cone_apex : sp.bell_start, sax ? 0.0f : sp.bell_gamma, suzu::END_CLOSED, 0.0f, suzu::END_OPEN, sp.bore_loss); cal.mu64 = b->mu_max(); delete b; }
    for (int k = 0; k < 4; k++) cal.offset[k] = suzu_wind_offset(sp, (int)sp.voice_kind, rate2, cal.peaks, cal.npeaks, SUZU_CAL_HZ[k], cal.mu64);
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

// Step 81 (TO_PROJECT_SPEC §13.11): THE TONEHOLES. The chart is the core's Boehm rule (sumi_layout_boehm_note, copied: Voxo
// never links libsumi): the highest open hole sounds, D4..C♯5 from the nine bits; the registers add 12 and 24. In hole mode
// the bore is cut for the register's base (62, 74, 86) and hole j (1..11) sits where the base's j-th semitone would end the
// bore — n_full·2^{−(j + corr_j/100)/12} cells, corr_j the calibration's cents — the vent (hole 12) at a third of the length
// for the upper registers; for the note's semitone j the holes 1..j are open (the lower ones), j+1..11 closed, the vent open
// from the second register. A hole's air is an inertance 1/(hole_size·S) at its node radiating through hole_loss.
static uint8_t boehm_note_of(uint32_t b) {
    const bool L1 = (b & 1u) != 0u, L2 = (b & 2u) != 0u, L3 = (b & 4u) != 0u, T = (b & 8u) != 0u, GS = (b & 16u) != 0u;
    const bool R1 = (b & 32u) != 0u, R2 = (b & 64u) != 0u, R3 = (b & 128u) != 0u, DS = (b & 256u) != 0u;
    if (!L1) return 73; if (!T) return 72; if (!(L2 || R1)) return 71; if (!L2) return 70; if (!L3) return 69; if (GS) return 68;
    if (!R1) return (R3 && !R2) ? 66 : 67; if (!R2) return 65; if (!R3) return 64; return DS ? 63 : 62;
}
static const int HOLE_BASE = 62, HOLE_VENT = 11;
// A hole at the fractional cell x sits on the two nodes about it, its conductance split (1 − f) : f — two inertances in
// parallel are one at the weighted place to first order; the top register's semitone is a cell and a half, and a glide moves
// the holes smoothly. Hole h is the pair 2h, 2h + 1; the vent the pair 22, 23.
static void suzu_holes_place(suzu::Bore& b, int n_full, float extra, const float* corr, float hole_size, float hole_loss, float corner_hz, float rate2, int j, int reg) {
    int at[24]; float L[24], R[24];
    const float a = extra * (float)n_full;                                  // a cone's series counts from the apex: the cut at c sounds as (c + a)
    auto pair = [&](int k, float x, float hinv_unit) {
        if (x < 1.0f) x = 1.0f; if (x > (float)(n_full - 1)) x = (float)(n_full - 1);
        int c0 = (int)x; float f = x - (float)c0; if (c0 >= n_full - 1) { c0 = n_full - 1; f = 0.0f; }
        const int c1 = c0 + 1 <= n_full - 1 ? c0 + 1 : c0;
        const float g0 = hinv_unit * b.sp[c0] * (1.0f - f), g1 = hinv_unit * b.sp[c1] * f;
        at[2 * k] = c0; L[2 * k] = g0 > 1e-9f ? 1.0f / g0 : 1e12f; R[2 * k] = hole_loss;
        at[2 * k + 1] = c1; L[2 * k + 1] = g1 > 1e-9f ? 1.0f / g1 : 1e12f; R[2 * k + 1] = hole_loss;
    };
    for (int h = 0; h < 11; h++) {
        const float semis = (float)(h + 1) + (corr ? corr[h + 1] : 0.0f) * 0.01f;
        pair(h, ((float)n_full + a) * exp2f(-semis / 12.0f) - a, hole_size * HOLE_H0);
    }
    pair(HOLE_VENT, (float)n_full / 8.0f, HOLE_VENT_H0);
    b.holes_set(24, at, L, R, corner_hz, rate2);   // the holes' corner above the bore's: half the radius
    uint32_t bits = 0u; for (int h = 0; h < j && h < 11; h++) bits |= 3u << (2 * h);
    if (reg >= 1) bits |= 3u << (2 * HOLE_VENT);
    b.holes_open(bits);
}
// the open holes as twelve bits (the pairs folded), for the inspection
static inline uint32_t suzu_holes_bits(const suzu::Bore& b) { uint32_t o = 0u; for (int h = 0; h < 12; h++) if ((b.hopen >> (2 * h)) & 1u) o |= 1u << h; return o; }
// the bore's first peak with its holes as set: a pulse at the blown end, the end velocity's Goertzel scan about f_guess
static float suzu_bore_first_peak(const suzu::Bore& src, float rate2, float f_guess, bool closed_end) {
    suzu::Bore* b = new suzu::Bore; *b = src; b->reset();
    const long T = (long)(0.12f * rate2); float* rec = new float[T];
    for (long i = 0; i < T; i++) { const float pulse = i < 8 ? 0.02f * (1.0f - cosf(2.0f * suzu::PI * (float)i / 8.0f)) : 0.0f; rec[i] = closed_end ? b->step(pulse, 0.0f, 1.0f) : b->step(0.0f, pulse, 1.0f); }
    double best = -1.0; float bc = 0.0f;
    for (float c = -400.0f; c <= 400.0f; c += 8.0f) { const double m = suzu_goertzel(rec, (size_t)T, 0.0, f_guess * exp2f(c / 1200.0f), rate2); if (m > best) { best = m; bc = c; } }   // coarse, then fine about it
    for (float c = bc - 8.0f; c <= bc + 8.0f; c += 1.0f) { const double m = suzu_goertzel(rec, (size_t)T, 0.0, f_guess * exp2f(c / 1200.0f), rate2); if (m > best) { best = m; bc = c; } }
    const float bf = f_guess * exp2f(bc / 1200.0f);
    delete[] rec; delete b;
    return bf;
}
static uint64_t suzu_hole_key_of(const voxo_suzu_params_t& p) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](float x) { uint32_t u; std::memcpy(&u, &x, 4); h ^= u; h *= 1099511628211ull; };
    mix((float)p.voice_kind); mix((float)p.tonehole_mode); mix(p.hole_size); mix(p.hole_loss); mix((float)p.bore_nodes); mix(p.bore_loss); mix(p.bore_corner_hz); mix(p.cone_apex); mix(p.bore_leak);
    return h;
}
// THE CALIBRATION at patch load: the base bore for D4 as the strike builds it, each of the twelve fingerings' holes placed,
// the bore pulsed and its first peak read against the chart's note — the hole moved by the difference, twice over.
static void suzu_hole_calibrate(voxo_t* v, const voxo_suzu_params_t& sp, const voxo_t::WindCal& wcal, voxo_t::HoleCal& cal) {
    const float rate2 = 2.0f * (float)v->device_rate.load(std::memory_order_relaxed);
    const bool sax = sp.voice_kind == 7;
    const float extra = sax ? suzu::cone_extra(sp.cone_apex) : 0.0f, corner = 2.0f * sp.bore_corner_hz;
    for (int r = 0; r < 2; r++) { cal.vent[r] = 0.0f; for (int j = 0; j < 12; j++) cal.corr[r][j] = 0.0f; }
    suzu::Bore* b = new suzu::Bore; suzu::Valve vv;
    int n_log[2] = { 0, 0 };
    // the register's base bore as the strike cuts it (the sax's through the wind build — the embouchure's pull is the
    // blown pitch's, not the passive peak's, so the targets here are the bore's own: f_cut)
    auto cut = [&](float f_cut) -> int {
        if (sax) return suzu_wind_build(*b, vv, sp, 7, f_cut, f_cut, rate2, nullptr, 0, true, wcal.mu64);
        b->setup(64, suzu::BORE_CYLINDER, 0.0f, 0.0f, suzu::END_OPEN, sp.bore_loss, suzu::END_OPEN, sp.bore_loss); b->radiation_corner(sp.bore_corner_hz, rate2);
        const float lam_max = suzu::Bore::lambda_bound(b->mu_max()) * 0.999f * suzu::Bore::holes_room(HOLE_H0 * sp.hole_size);
        const float ends = 2.0f * suzu::Bore::end_correction(sp.bore_loss, b->rad_a, f_cut, rate2);   // the flute takes no leak
        const int n = suzu::Bore::cells_for(f_cut, rate2, lam_max, false, 0.0f, ends, (int)sp.bore_nodes);
        if (n >= 2) { b->setup(n, suzu::BORE_CYLINDER, 0.0f, 0.0f, suzu::END_OPEN, sp.bore_loss, suzu::END_OPEN, sp.bore_loss); b->retune(n, suzu::Bore::lambda_for(f_cut, rate2, n, false, 0.0f, ends), suzu::BORE_CYLINDER, 0.0f, 0.0f); b->radiation_corner(sp.bore_corner_hz, rate2); }
        return n;
    };
    for (int r = 0; r < 2; r++) {
        const float f_r = 440.0f * exp2f((float)(HOLE_BASE + 12 * r - 69) / 12.0f);
        int n_r = cut(f_r); if (n_r < 4) continue;
        if (r >= 1) {                                                           // the vent's detune: the base bore pulsed with the vent closed and open
            suzu_holes_place(*b, n_r, extra, cal.corr[r], sp.hole_size, sp.hole_loss, corner, rate2, 0, 0); const float f0 = suzu_bore_first_peak(*b, rate2, f_r, sax);
            suzu_holes_place(*b, n_r, extra, cal.corr[r], sp.hole_size, sp.hole_loss, corner, rate2, 0, r); const float f1 = suzu_bore_first_peak(*b, rate2, f_r, sax);
            float c = f0 > 1.0f && f1 > 1.0f ? 1200.0f * log2f(f1 / f0) : 0.0f; if (c > 150.0f) c = 150.0f; if (c < -150.0f) c = -150.0f;
            cal.vent[r] = c;
            n_r = cut(f_r * exp2f(-c / 1200.0f)); if (n_r < 4) continue;      // the bore as the strike will cut it, the vent's detune out
        }
        n_log[r] = n_r;
        for (int pass = 0; pass < (sax ? 1 : 2); pass++) for (int j = 1; j < 12; j++) {   // each hole moved by what its note misses, twice over (the sax's second pass is blown)
            suzu_holes_place(*b, n_r, extra, cal.corr[r], sp.hole_size, sp.hole_loss, corner, rate2, j, r);
            const float f_t = f_r * exp2f((float)j / 12.0f), f_p = suzu_bore_first_peak(*b, rate2, f_t, sax);
            float c = 1200.0f * log2f(f_t / (f_p > 1.0f ? f_p : 1.0f)); if (c > 150.0f) c = 150.0f; if (c < -150.0f) c = -150.0f;
            cal.corr[r][j] += c;
        }
    }
    if (sax) for (int pass = 0; pass < 2; pass++) {                             // the second pass by blowing, twice over: the reed's note on each hole bore (the strike's cut)
        for (int r = 0; r < 2; r++) {
            const float f_r = 440.0f * exp2f((float)(HOLE_BASE + 12 * r - 69) / 12.0f);
            for (int j = (r == 0 ? 1 : 0); j < 12; j++) {
                const float f_cut = f_r * exp2f(-cal.vent[r] / 1200.0f), f_t = f_r * exp2f((float)j / 12.0f);
                const int n_r = suzu_wind_build(*b, vv, sp, 7, f_cut * exp2f(-suzu_wind_offset_at(wcal, f_cut) / 1200.0f), f_cut * sp.lip_ratio, rate2, wcal.peaks, wcal.npeaks, true, wcal.mu64);
                if (n_r < 4) continue;
                suzu_holes_place(*b, n_r, extra, cal.corr[r], sp.hole_size, sp.hole_loss, corner, rate2, j, r);
                float off = suzu_blow_offset(*b, vv, sp, 7, rate2, f_t); if (off > 150.0f) off = 150.0f; if (off < -150.0f) off = -150.0f;
                if (j == 0) cal.vent[r] += off; else cal.corr[r][j] -= off;
            }
        }
    }
    delete b;
    cal.key = suzu_hole_key_of(sp);
    if (v->log_cb) {
        char msg[512]; int o = std::snprintf(msg, sizeof msg, "suzu: the %s's toneholes calibrated (the registers' bores %d / %d cells, the vent's detune %+.0f); the holes' corrections in cents:", sax ? "sax" : "flute", n_log[0], n_log[1], (double)cal.vent[1]);
        for (int r = 0; r < 2 && o < (int)sizeof msg - 80; r++) { o += std::snprintf(msg + o, sizeof msg - (size_t)o, " |"); for (int j = 1; j < 12 && o < (int)sizeof msg - 12; j++) o += std::snprintf(msg + o, sizeof msg - (size_t)o, " %+.0f", (double)cal.corr[r][j]); }
        v->log_cb(1, msg, v->log_user);
    }
}
// the strike's choice: the note's register and semitone when the keys agree with the chart (−1: the v1 cut)
static int suzu_hole_j_of(const voxo_t* v, const voxo_suzu_params_t& sp, uint8_t note, int* reg_out) {
    if (sp.tonehole_mode != 1u || (sp.voice_kind != 6 && sp.voice_kind != 7)) return -1;
    const int rel = (int)note - HOLE_BASE; if (rel < 0 || rel >= 24) return -1;   // two registers: above them a hole's impedance is the bore's own (the lattice's cutoff) and the v1 cut sounds the note
    const int reg = rel / 12, j = rel % 12;
    if ((int)boehm_note_of(v->keys) != HOLE_BASE + j) return -1;
    *reg_out = reg; return j;
}

// Step 79 (TO_PROJECT_SPEC §13.9): the membrane's grid — the rows 13/16 of the columns (a rectangle: the degenerate pairs
// split), the strike's bump a tenth of the sheet, the plate's point from the note when the shell gave none (the note
// along x as a soundboard's bridge, a golden-ratio scatter along y so neighbours share no null), and the plate's tuning
// under the gate at the output rate (the share clamped first, then the pitch).
static inline int suzu_drum_ny(int nx) { const int ny = (nx * 13 + 8) / 16; return ny < 2 ? 2 : ny; }
static inline float suzu_drum_radius(int nx, int ny) { const float r = 0.1f * (float)(nx < ny ? nx : ny) + 1.0f; return r < 1.5f ? 1.5f : r; }
static void plate_default_point(uint8_t note, float* x, float* y) {
    const float t = ((float)note - 21.0f) / 87.0f; *x = 0.1f + 0.8f * clampf(t, 0.0f, 1.0f);
    const float g = (float)note * 0.6180340f; *y = 0.2f + 0.6f * (g - floorf(g));
}
static bool suzu_plate_tuning(const voxo_suzu_params_t& sp, float rate, int n, float* eps_out, float* share_out) {   // true when the gate clamped it
    const float G = suzu::MEMBRANE_CFL_GATE, l = suzu::membrane_lambda(1, 1, n, n);
    float eps = suzu::eps_for(sp.plate_hz, rate), share = clampf(sp.plate_kappa, 0.0f, 1.0f); bool clamped = false;
    const float a = 8.0f / l, b = 64.0f / (l * l), target = G * G / (eps * eps);
    if (target < a) { share = 0.0f; eps = G / sqrtf(a); clamped = true; }                     // the pitch saturates on this grid
    else { const float smax = (target - a) / (b - a); if (share > smax) { share = smax; clamped = true; } }
    *eps_out = eps; *share_out = share; return clamped;
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
        vc.suzu.sreturns = 0; vc.suzu.strobe_hz = 0.0f;                        // step 75: the strobe's count and left end (its ring runs on too)
        vc.suzu.sprev[0] = vc.suzu.sprev[1] = vc.suzu.sprev[2] = vc.suzu.sprev[3] = 0.0f;
        // Step 56: the strike is a DRIVE — the cell's orbit set at the velocity's
        // amplitude, the filter cleared, the output ramp from zero (a retrigger
        // re-kicks the same cell: the orbit restarts at the new amplitude).
        const voxo_suzu_params_t& sp = v->suzu_params[v->suzu_live.load(std::memory_order_acquire)];
        const uint32_t rate2 = 2u * v->device_rate.load(std::memory_order_relaxed);
        const float rateC = (sp.voice_kind <= 1u && sp.oversample == 4u) ? 2.0f * (float)rate2 : (float)rate2;   // step 78: the cell family's section rate (the ×4 oversampler)
        SuzuVoice& sz = vc.suzu;
        const float A = sp.level * vc.vel_gain;
        {   // step 79: the voice's point on the bus plate — the shell's (voxo_set_strike_point), else the default map's
            const float sx = v->strike_x[channel & 15][note & 127], sy = v->strike_y[channel & 15][note & 127];
            if (sx >= 0.0f && sy >= 0.0f) { sz.plate_x = clampf(sx, 0.0f, 1.0f); sz.plate_y = clampf(sy, 0.0f, 1.0f); } else plate_default_point(note, &sz.plate_x, &sz.plate_y);
            const float sp_pos = v->strike_pluck[channel & 15][note & 127];   // step 80: the strike position from the cell, else the patch's pluck
            sz.strike_pos = sp_pos >= 0.0f ? clampf(sp_pos, 0.01f, 0.5f) : sp.pluck;
            sz.bow_f = 0.0f; sz.bow_vb = 0.0f; sz.bow_work = 0.0; sz.bow_gain = 0.0; sz.bow_node = 0; sz.bow_stuck = false;
            sz.hole_j = -1; sz.hole_reg = 0; sz.hole_bits = 0u;   // step 81
        }
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
                    sz.string.pluck(A, sz.strike_pos);                                   // step 80: the position the cell gave, else the patch's
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
            } else if (sp.voice_kind == 10) {
                // step 83: the Toda ring — the nodes the pitch allows, k for the linear first mode on the note (×4), the compression
                // pulse at the cell's position launched to the right, the swirl's momentum as an impulse, the cap under the bound
                const float rate4 = 2.0f * (float)rate2;
                const int m = suzu::TodaRing::nodes_for(vc.freq, rate4, (int)sp.toda_nodes);
                if (m >= 4) {
                    if (fresh || sz.ring.n != m) { sz.ring.reset(m); sz.toda_m = 2; }
                    const float nonlin = clampf(vc.pressure + 2.0f * (vc.timbre - 0.5f), 0.0f, 1.0f);
                    const float b_want = sp.toda_b * SUZU_TODA_BMAX * (0.05f + 0.95f * nonlin);
                    const float k_now = suzu::TodaRing::k_for(vc.freq, (float)sz.toda_m * (float)rate2, m);   // the ring's present rate (a re-strike adds to a running ring)
                    sz.ring.set_pickup(sp.pickup);
                    sz.ring.strike(A * SUZU_TODA_STRAIN, sz.strike_pos, sp.toda_width, sqrtf(k_now));
                    suzu_toda_fit(sz, sp, vc.freq, (float)rate2, b_want, true);                               // the rate, the nonlinearity and the cap the pulse allows
                    sz.toda_swirl = vc.swirl_target; sz.ring.kick_all(SUZU_TODA_SWIRL_J * vc.swirl_target * sqrtf(sz.toda_k));
                } else { sz.ring.reset(4); sz.toda_m = 2; }                        // above the bound at four nodes: silent
                sz.hb[0].reset(); sz.hb[1].reset();
                sz.decay[0] = sp.decay_s > 0.0f ? suzu::contraction_for(sp.decay_s, (float)rate2) : 1.0f;
            } else if (sp.voice_kind == 11) {
                // step 84: the gyroscope — a displacement along the cell's position (the diagonal at 0.5: mono), the field from
                // the poly pressure, the cyclotron phase from zero
                if (fresh) { sz.gyro.reset(); sz.svf2.reset(); }
                suzu_boris_fit(sz, sp, vc.freq, (float)rate2, vc.swirl_target);
                sz.gyro.strike(A, clampf(sz.strike_pos, 0.0f, 1.0f));
                sz.decay[0] = sp.decay_s > 0.0f ? suzu::contraction_for(sp.decay_s, (float)rate2) : 1.0f;
            } else if (sp.voice_kind == 6) {
                // the flute: an open–open cylinder with its radiation ports, the jet at node 0; the bore's cells from the
                // pitch under the CFL bound (μ_max by power iteration once, at the strike — the profile is fixed)
                if (fresh) { sz.bore.setup(64, suzu::BORE_CYLINDER, 0.0f, 0.0f, suzu::END_OPEN, sp.bore_loss, suzu::END_OPEN, sp.bore_loss); sz.jet.reset(); sz.breath_s = 0.0f; }
                sz.bore.radiation_corner(sp.bore_corner_hz, (float)rate2);
                int hreg = 0; const int hj = suzu_hole_j_of(v, sp, note, &hreg); sz.hole_j = hj; sz.hole_reg = hreg; sz.hole_bits = v->keys;   // step 81
                if (fresh || !(sz.bore_mu > 0.0f)) sz.bore_mu = v->flute_mu64 > 0.0f ? v->flute_mu64 : sz.bore.mu_max();                       // step 81: the engine's once-computed μ (the cylinder's is 3.9965 at any length)
                sz.bore_lam_max = suzu::Bore::lambda_bound(sz.bore_mu) * 0.999f * (hj >= 0 ? suzu::Bore::holes_room(HOLE_H0 * sp.hole_size) : 1.0f);   // step 81: the holes' room
                const float lam_max = sp.bore_cfl > 0.0f ? sz.bore_lam_max * sp.bore_cfl : sz.bore_lam_max;
                const voxo_t::HoleCal& hcal = v->suzu_hole[v->suzu_live.load(std::memory_order_acquire)];
                const float f_cut = hj >= 0 ? vc.freq * exp2f(-(float)hj / 12.0f - hcal.vent[hreg] / 1200.0f) : vc.freq;                  //   the register's base in hole mode, the vent's detune cut out
                const float ends = 2.0f * suzu::Bore::end_correction(sp.bore_loss, sz.bore.rad_a, f_cut, (float)rate2);   // step 81: the flute takes no leak (its bass is the radiation's near-lossless)
                const int cells = suzu::Bore::cells_for(f_cut, (float)rate2, lam_max, false, 0.0f, ends, (int)sp.bore_nodes);
                if (cells >= 2) {
                    const float lam = sp.bore_cfl > 0.0f ? lam_max : suzu::Bore::lambda_for(f_cut, (float)rate2, cells, false, 0.0f, ends);
                    if (fresh) sz.bore.setup(cells, suzu::BORE_CYLINDER, 0.0f, 0.0f, suzu::END_OPEN, sp.bore_loss, suzu::END_OPEN, sp.bore_loss);
                    sz.bore.retune(cells, lam, suzu::BORE_CYLINDER, 0.0f, 0.0f);
                    sz.bore.radiation_corner(sp.bore_corner_hz, (float)rate2);
                    if (hj >= 0) suzu_holes_place(sz.bore, cells, 0.0f, hcal.corr[hreg], sp.hole_size, sp.hole_loss, 2.0f * sp.bore_corner_hz, (float)rate2, hj, hreg); else sz.bore.holes_set(0, nullptr, nullptr, nullptr);
                }
                sz.bore_freq = vc.freq;
                sz.jet.q_prev = 0.0f;
                sz.decay[0] = sp.bore_wall_s > 0.0f ? suzu::contraction_for(sp.bore_wall_s, (float)rate2) : 1.0f;   // the wall loss; the radiation is the ports'
            } else if (sp.voice_kind == 7 || sp.voice_kind == 8) {
                // the winds: the bore cut for the note less the embouchure's measured offset (the calibration), the valve set
                const voxo_t::WindCal& cal = v->suzu_wind[v->suzu_live.load(std::memory_order_acquire)];
                int hreg = 0; const int hj = suzu_hole_j_of(v, sp, note, &hreg); sz.hole_j = hj; sz.hole_reg = hreg; sz.hole_bits = v->keys;   // step 81: the sax's holes
                const voxo_t::HoleCal& hcal = v->suzu_hole[v->suzu_live.load(std::memory_order_acquire)];
                const float f_cut = hj >= 0 ? vc.freq * exp2f(-(float)hj / 12.0f - hcal.vent[hreg] / 1200.0f) : vc.freq;
                const float f_target = f_cut * exp2f(-suzu_wind_offset_at(cal, f_cut) / 1200.0f);
                const int wcells = suzu_wind_build(sz.bore, sz.valve, sp, (int)sp.voice_kind, f_target, vc.freq * sp.lip_ratio, (float)rate2, cal.peaks, cal.npeaks, true, cal.mu64);   // every strike a new blow: the old bore's content seeded the wrong register
                if (hj >= 0 && wcells >= 4) suzu_holes_place(sz.bore, wcells, suzu::cone_extra(sp.cone_apex), hcal.corr[hreg], sp.hole_size, sp.hole_loss, 2.0f * sp.bore_corner_hz, (float)rate2, hj, hreg); else sz.bore.holes_set(0, nullptr, nullptr, nullptr);
                sz.bore_lam_max = suzu::Bore::lambda_bound(cal.mu64) * 0.99f;   // step 81: the cached μ (the flute's glide reads this; the winds' build has its own)
                sz.bore_freq = vc.freq; sz.breath_s = fresh ? 0.0f : sz.breath_s;
                sz.dc_x1 = sz.dc_y1 = 0.0f; if (fresh) sz.wind_dyn = 0.32f;
                sz.work = 0.0;                                                           // 59c: the ledger restarts with each strike (the bore is new)
                sz.decay[0] = suzu_wind_wall(sp, (int)sp.voice_kind, vc.freq, (float)rate2);
            } else if (sp.voice_kind == 9) {
                // step 79: THE MEMBRANE — the grid cut for the note: the largest the gate admits at the cap's stretching
                // (λ_2D is ε times a factor of the grid, the share and the swell alone; past the gate on the 2 × 2 grid the
                // pitch saturates, as the chain's does at its bound); the strike a bump of momentum scaled by ε₁₁ so the
                // pickup's displacement reads the level across the keyboard; the cap enforced on the plain energy, so a
                // re-strike on a ringing drum never exceeds it; the gong's α from the swell at that cap
                const float share = clampf(sp.drum_kappa, 0.0f, 1.0f), swell = sp.drum_alpha > 0.0f ? sp.drum_alpha : 0.0f;
                const float gate = swell > 0.0f ? suzu::MEMBRANE_CFL_GATE_GONG : suzu::MEMBRANE_CFL_GATE;   // the gong's bar sits lower (suzu.h: measured)
                float eps = suzu::eps_for(vc.freq, (float)rate2);
                int nx = 2, ny = 2; float factor = 1.0f;
                for (int n2 = (int)sp.drum_size; n2 >= 2; n2--) {
                    nx = n2; ny = suzu_drum_ny(n2);
                    factor = suzu::Membrane<suzu::MEMBRANE_MAX>::cfl_factor(nx, ny, share, swell);
                    if (eps * factor <= gate) break;
                }
                sz.drum_eps_max = gate / factor;
                if (sp.drum_cfl > 0.0f) { sz.drum_eps_max *= sp.drum_cfl; eps = sz.drum_eps_max; }   // the lab's forced Courant number
                else if (eps > sz.drum_eps_max) eps = sz.drum_eps_max;
                if (fresh || sz.drum.nx != nx || sz.drum.ny != ny) sz.drum.reset(nx, ny);              // a re-strike of a ringing drum adds its bump
                sz.drum.tune(eps, share); sz.drum_eps = eps;
                const float r = suzu_drum_radius(nx, ny);
                const float w2 = sz.drum.strike_weight2(sz.strike_pos, sz.strike_pos, r);   // step 80: the strike position from the cell
                const float a_full = sp.level * eps * SUZU_DRUM_STRIKE;
                sz.drum_cap = (sp.drum_cap > 0.0f ? sp.drum_cap : 1.0f) * 0.5f * a_full * a_full * w2;
                const float l11 = sz.drum.lambda11();
                sz.drum_emax = 2.0f * sz.drum_cap / (sz.drum.c2 + sz.drum.k2 * l11);
                sz.drum.alpha = sz.drum_emax > 0.0f ? swell * sz.drum.c2 / sz.drum_emax : 0.0f;
                sz.drum_cfl = suzu::membrane_cfl(sz.drum.c2, sz.drum.k2, sz.drum.alpha, sz.drum_emax);
                sz.drum.strike(a_full * vc.vel_gain, sz.strike_pos, sz.strike_pos, r);
                if (sp.drum_cfl_gate != 0) sz.drum.cap((double)sz.drum_cap);
                sz.drum.damp(sp.decay_s, sp.decay_bright, (float)rate2);
                sz.drum_g0 = sz.drum.g0; sz.drum_g2 = sz.drum.g2;
                sz.decay[0] = 1.0f;                                                               // the membrane's damping lives in its grid
            } else {
                sz.rotor.strike(A, vc.freq, (float)rate2);
                sz.k_smooth = sp.rotor_k;
                sz.decay[0] = sp.decay_s > 0.0f ? suzu::contraction_for(sp.decay_s, (float)rate2) : 1.0f;
            }
        } else if (sp.voice_kind == 0) {
            sz.n = 1; sz.mix = 1.0f; sz.kappa = 0.0f;
            sz.table.n = 1; sz.table.ratio[0] = 1.0f; sz.table.t60[0] = 0.0f; sz.table.kick[0] = 1.0f; sz.table.bow[0] = 1.0f;
            sz.modes[0].kick(A, suzu::eps_for(vc.freq, rateC));   // step 78: at the cell family's own rate
            sz.eps[0] = sz.modes[0].eps; sz.inv_eps[0] = 1.0f / sz.eps[0]; sz.decay[0] = 1.0f;   // the single cell: no decay while held (step 56)
        } else {
            // Step 57: the modal lattice — the preset's table, the strike's kick profile
            // (a DRIVE per mode), the declared decays, the coupling with the swirl's share.
            suzu::modal_table(&sz.table, (int)sp.modal_preset, (int)sp.modes, sp.decay_s, sp.decay_bright, sp.stiffness, sz.strike_pos, sp.bow_position, sp.modal_ratio, sp.modal_t60, sp.modal_weight);   // step 80: the plucked profile at the cell's position
            sz.n = (uint32_t)sz.table.n;
            float wsum = 0.0f;
            const float ceiling = 0.5f * (float)(rate2 / 2u);                                   // the OUTPUT Nyquist whatever the section's rate (step 78)
            const float kappa0 = clampf(sp.coupling + (sp.modal_comp != 0u ? 0.5f * vc.swirl_target : 0.0f), 0.0f, suzu::COMP_KAPPA_MAX);   // step 82: the raw lattice's coupling is the patch's alone (the swirl would detune every mode)
            float comp[suzu::MODES_MAX];
            if (sp.modal_comp != 0u) suzu_comp_for(v, v->suzu_live.load(std::memory_order_acquire), kappa0, sz.table, vc.freq, rateC, comp);
            else for (uint32_t k = 0; k < sz.n; k++) comp[k] = 1.0f;                                   // step 82: the raw lattice — the coupling's splitting is the sound
            for (uint32_t k = 0; k < sz.n; k++) {
                const float fk = vc.freq * sz.table.ratio[k];
                if (fk >= ceiling) { sz.eps[k] = 0.0f; sz.inv_eps[k] = 0.0f; sz.modes[k].reset(); sz.decay[k] = 1.0f; continue; }   // muted above the ceiling: a wall
                sz.eps[k] = comp[k] * suzu::eps_for(fk, rateC); sz.inv_eps[k] = 1.0f / sz.eps[k];
                sz.modes[k].kick(A * sz.table.kick[k], sz.eps[k]);
                sz.decay[k] = sz.table.t60[k] > 0.0f ? suzu::contraction_for(sz.table.t60[k], rateC) : 1.0f;
                wsum += sz.table.kick[k];                                              // the ALIVE modes' weights: the strike peaks at A on every note, however many modes the ceiling left
            }
            sz.mix = wsum > 0.0f ? 1.0f / wsum : 1.0f;
            sz.kappa = clampf(sp.coupling + 0.5f * vc.swirl_target, 0.0f, suzu::COMP_KAPPA_MAX);
        }
        for (uint32_t k = 0; k < suzu::MODES_MAX; k++) sz.bow_t[k] = 0.0f;
        sz.bow_g = sp.bow_onset_s > 0.0f ? 1.0f / (sp.bow_onset_s * rateC) : 0.0f;
        sz.svf.reset();
        sz.hb[0].reset(); sz.hb[1].reset();   // step 78
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
        vc.suzu.contract = suzu::contraction_for(sp.release_s, (sp.voice_kind <= 1u && sp.oversample == 4u ? 4.0f : 2.0f) * (float)v->device_rate.load(std::memory_order_relaxed));   // per sub-step of the voice's own section (step 78)
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
            if ((e.a >= 110 && e.a <= 112) || (e.a >= 114 && e.a <= 119)) {   // step 81: the fingering keys — the valves' CCs 110–112 → bits 0..2, the Boehm keys 114–119 → bits 3..8 (the core's rule: the master channel under MPE, any channel otherwise)
                if (!v->zone_live || is_master(v, e.channel)) { const uint32_t bit = e.a <= 112 ? (uint32_t)(e.a - 110) : 3u + (uint32_t)(e.a - 114); if (e.b >= 64) v->keys |= 1u << bit; else v->keys &= ~(1u << bit); }
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
// Step 59: one orbit point into the voice's trace ring, every `decim`-th call (the rendering thread's side); 59c: with
// the kind's two aux channels.
static inline void trace_push(SuzuVoice& sz, uint32_t decim, float x, float y, float u, float w2) {
    if (++sz.tdecim < decim) return;
    sz.tdecim = 0;
    const uint32_t w = sz.twrite.load(std::memory_order_relaxed);
    const uint32_t j = w % TRACE_RING;
    sz.tx[j] = x; sz.ty[j] = y; sz.tu[j] = u; sz.tw[j] = w2;
    sz.twrite.store(w + 1, std::memory_order_release);
}
static inline uint32_t trace_decim_of(const voxo_t* v) { const uint32_t d = v->trace_decim.load(std::memory_order_relaxed); return d == 0 ? TRACE_DECIM : d; }
// Step 75: one return into the voice's strobe ring — the pair interpolated at `frac` between the last sub-step's (sprev) and
// this one's; the aux pair this sub-step's own (the rotor's p after its kick: the section's p_{k+1}).
static inline void strobe_push(SuzuVoice& sz, float frac, float x, float y, float u, float w2) {
    const uint32_t w = sz.swrite.load(std::memory_order_relaxed);
    const uint32_t j = w % STROBE_RING;
    sz.sx[j] = sz.sprev[0] + frac * (x - sz.sprev[0]); sz.sy[j] = sz.sprev[1] + frac * (y - sz.sprev[1]);
    sz.su[j] = u; sz.sw[j] = w2;
    sz.sreturns++;
    sz.swrite.store(w + 1, std::memory_order_release);
}
static bool render_suzu_other(voxo_t* v, Voice& vc, const voxo_suzu_params_t& sp, float* out_lr, uint32_t frames, float freq_from, float freq_step,
                              float rate2, float pg0, float pg_step, bool filter, float f_svf, float f_svf_step, float q, float* src) {
    SuzuVoice& sz = vc.suzu;
    const uint32_t kind = sp.voice_kind;
    const bool releasing = vc.releasing, gliding = freq_step != 0.0f;
    const float rr = releasing ? (sz.contract < sz.decay[0] ? sz.contract : sz.decay[0]) : sz.decay[0];
    if (kind == 10 && sz.ring.n >= 4) {                                                            // step 83: the fit at the block's pitch (the glide rides it), the nonlinearity from the press and the slide, the swirl's impulse on a change
        const float nonlin = clampf(vc.pressure + 2.0f * (vc.timbre - 0.5f), 0.0f, 1.0f);
        suzu_toda_fit(sz, sp, freq_from, rate2, sp.toda_b * SUZU_TODA_BMAX * (0.05f + 0.95f * nonlin), false); sz.ring.set_pickup(sp.pickup);
        if (vc.swirl != sz.toda_swirl) { sz.ring.kick_all(SUZU_TODA_SWIRL_J * (vc.swirl - sz.toda_swirl) * sqrtf(sz.toda_k)); sz.toda_swirl = vc.swirl; }
    }
    const float rr_m = kind == 10 ? powf(rr, 1.0f / (float)(sz.toda_m > 0 ? sz.toda_m : 2)) : 1.0f;   // step 83: the contraction per inner step
    const float block_s = (float)frames / (0.5f * rate2);
    const float coef = 1.0f - std::exp(-block_s / 0.020f);
    const float mod = sp.mod_target != 0 ? v->mod_smooth * sp.mod_depth : 0.0f;
    const float freq_end = freq_from + freq_step * (float)frames;
    const bool tracing = ((v->trace_mask.load(std::memory_order_relaxed) >> kind) & 1u) != 0;   // step 59
    const uint32_t tdec = trace_decim_of(v);
    const uint32_t smode = tracing ? v->strobe_mode.load(std::memory_order_relaxed) : 0u;        // step 75: the strobe rides the trace
    const bool strobing = smode != 0;
    uint32_t sk = 0;                                                                             //   the block's next wrap
    const float trace_dscale = rate2 / (2.0f * suzu::PI * (freq_end > 1.0f ? freq_end : 1.0f));   // ṡ/ω: the derivative in the note's units
    float K = 0.0f, drive = 0.0f, body = 0.0f;
    // the flute's block values: the breath → the mouth pressure → the jet's speed, delay, band, flow
    float U0 = 0.0f, tau = 1.0f, band_f = 0.0f, kscale = 0.0f, leak = 1.0f, jet_g = 0.0f, mouth_q = 0.0f, Pm = 0.0f, jet_a = 0.0f;
    float wind_sigma = 0.0f, wind_shear = 0.0f, wind_scale = 1.0f; bool wind_naive = false;
    bool own_clock = false;                                                                      // step 75: set once the block's drive is known
    const float drum_share = clampf(sp.drum_kappa, 0.0f, 1.0f), drum_swell = sp.drum_alpha > 0.0f ? sp.drum_alpha : 0.0f;   // step 79
    const float pk_x = sp.pickup, pk_y = 1.0f - sp.pickup;
    // Step 80: THE STICK-SLIP BOW on the chain's node at bow_position of the length — the mouth (breath, or the press under
    // press_blows) the speed, the press and the velocity the force; the node's velocity solved against the friction curve
    // after the chain's own step (its mass 1: the mobility 1), bounded; the chain takes no bow under bow_mode 0
    bool slipping = false; float bow_vb = 0.0f, bow_F = 0.0f, bow_v0 = 1.0f; int bow_ib = 1; const bool bow_naive = sp.bow_naive != 0;
    sz.bow_vb = 0.0f;
    float bow_beta = 0.12f;
    if ((kind == 2 || kind == 3) && sp.bow_mode == 1u && vc.held && (kind == 3 || sz.string.m >= 2)) {
        float mouth = v->channels[vc.channel & 15].breath;
        if (sp.press_blows != 0 && vc.pressure > mouth) mouth = vc.pressure;
        if (mouth > 0.0f) {
            slipping = true;
            bow_beta = clampf(sp.bow_position, 0.02f, 0.5f);
            float v_ref;
            if (kind == 2) {
                int ib = (int)(bow_beta * (float)(sz.string.m + 1) + 0.5f); if (ib < 1) ib = 1; if (ib > sz.string.m) ib = sz.string.m;
                bow_ib = ib; sz.bow_node = ib;
                v_ref = sp.level * (2.0f * freq_end / rate2) / (1.0f - bow_beta) * SUZU_BOW_KV_CHAIN;   // the Helmholtz amplitude ≈ the level at full mouth
            } else v_ref = sp.level * SUZU_BOW_KV_HYB;                                                // the loop's velocity waves, the level's units
            bow_vb = sp.bow_speed * mouth * v_ref;
            bow_v0 = SUZU_BOW_V0 * bow_vb;
            bow_F = sp.bow_force * bow_vb * (kind == 2 ? SUZU_BOW_KF_CHAIN : SUZU_BOW_KF_HYB) * (0.5f + 0.5f * (float)vc.velocity / 127.0f) * (1.0f + vc.pressure);
            sz.bow_vb = bow_vb;
        }
    }
    if (kind == 7 || kind == 8) {
        // the winds' breath → the mouth pressure, linear: the trumpet from half the reference to three; the sax from
        // just under its threshold (a third of the reed's closing pressure in theory, 1.15 P_ref with the losses —
        // the author heard nothing from a pedal that never reached the old map's threshold at half breath) to 0.6 of
        // the closing pressure (from 0.7 up some notes squeal to the fourth register). The press blows too when the
        // patch says so. The PRESSURE is what smooths, so a released voice's mouth goes to zero — a lingering
        // half-reference blow kept the tail sounding into the next note.
        const float breath_cc = vc.held ? v->channels[vc.channel & 15].breath : 0.0f;
        float breath = breath_cc;
        if (sp.press_blows != 0 && vc.held && vc.pressure > breath_cc) {
            // The press blows. The 2.0 release round (DECISIONS_9 #16): when the press IS the mouth it is the mouth
            // alone — the output gain holds at the breath controller's (0.35 with none) instead of following the press
            // too. Compounded, the lips' +14 dB and the gain's +9 dB put one trumpet voice at −3.7 dBFS and three at
            // +5.8 dBFS under a hard finger on the Tab (pressure 127), deep in the master knee — the saturation heard.
            breath = vc.pressure;
            pg0 = 0.35f + 0.65f * breath_cc; pg_step = 0.0f;
        }
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
            sz.valve.setup(lip_hz, rate2, sp.lip_q, sp.lip_open, sp.lip_close * SUZU_P_REF, sp.lip_area, 1, 0.0f); sz.valve.setup_shear(sp.lip_shear, sp.lip_shear_ratio, sp.lip_q, rate2);   // step 81
        }
        if (gliding && sz.bore_freq != freq_end) {                                 // the bore follows the pitch per block
            const voxo_t::WindCal& cal = v->suzu_wind[v->suzu_live.load(std::memory_order_acquire)];
            const voxo_t::HoleCal& hcal = v->suzu_hole[v->suzu_live.load(std::memory_order_acquire)];
            const float f_cut = sz.hole_j >= 0 ? freq_end * exp2f(-(float)sz.hole_j / 12.0f - hcal.vent[sz.hole_reg] / 1200.0f) : freq_end;   // step 81: the base glides, the holes ride
            const int wcells = suzu_wind_build(sz.bore, sz.valve, sp, (int)kind, f_cut * exp2f(-suzu_wind_offset_at(cal, f_cut) / 1200.0f), freq_end * sp.lip_ratio, rate2, cal.peaks, cal.npeaks, false, cal.mu64);
            if (sz.hole_j >= 0 && wcells >= 4) suzu_holes_place(sz.bore, wcells, suzu::cone_extra(sp.cone_apex), hcal.corr[sz.hole_reg], sp.hole_size, sp.hole_loss, 2.0f * sp.bore_corner_hz, rate2, sz.hole_j, sz.hole_reg);
            sz.decay[0] = suzu_wind_wall(sp, (int)kind, freq_end, rate2);
            sz.bore_freq = freq_end;
        }
    }
    if (kind == 6) {
        const float breath_cc = vc.held ? v->channels[vc.channel & 15].breath : 0.0f;
        float breath = breath_cc;
        if (sp.press_blows != 0 && vc.held && vc.pressure > breath_cc) {   // the press blows too (an Osmose, aftertouch) — the mouth alone (#16, as the reed and the lips)
            breath = vc.pressure;
            pg0 = 0.35f + 0.65f * breath_cc; pg_step = 0.0f;
        }
        sz.breath_s += (breath - sz.breath_s) * coef;
        const float P_ref = 0.005f;                                                // the reference mouth pressure, in the bore's units (U₀ 0.1)
        const float lo = 1.0f / sqrtf(sp.breath_range), hi = sqrtf(sp.breath_range);   // the pressure's range about the reference, soft to hard
        const float ratio = sz.breath_s <= 0.0f ? 0.0f : lo * powf(hi / lo, (sz.breath_s - sp.breath_ref) / (1.0f - sp.breath_ref) * 0.5f + 0.5f);
        Pm = P_ref * ratio;
        U0 = sqrtf(2.0f * Pm);
        // Step 75 (found by the strobe fuzz): THE MODEL'S CEILING — the jet's receptivity band sits at half the sub-rate over
        // its delay, and a note bent to 17 kHz (a +47 semitone bend on B5) put that band past the SVF's stable range and the
        // bore at four cells: NaN in the block. The jet and the bore are built for the pitch up to rate2/16 (6 kHz at 48 kHz,
        // above any flute's top); past it the model saturates there, as the chain's pitch does at its CFL bound.
        const float f_model = freq_end < rate2 * (1.0f / 16.0f) ? freq_end : rate2 * (1.0f / 16.0f);
        const float T = rate2 / f_model;
        tau = U0 > 1e-6f ? sp.jet_tau * T * sqrtf(P_ref / Pm) : (float)(suzu::JET_DELAY_MAX - 3);
        band_f = suzu::eps_for(0.5f * rate2 / tau, rate2);
        kscale = -sp.jet_drive;                                                    // the dipole's pressure per unit of the flow's rate, O(1)
        // THE EMBOUCHURE FOLLOWS THE NOTE (v1): the jet's delay is in periods (above), its gain rises with the pitch as the
        // bore's losses do, and its area — the flow — falls as the root of the pitch: the drive is a derivative, so its
        // saturated amplitude would climb 6 dB an octave otherwise (measured: C2 27 dB under A4; with these laws the
        // keyboard sits within 7 dB). The physical jet with one distance and one width, the player blowing harder for
        // height, is the [ITERATE].
        jet_g = sp.jet_gain * (f_model / 440.0f);
        jet_a = sp.jet_area * sqrtf(440.0f / f_model);
        leak = 1.0f - 2.0f * suzu::PI * 10.0f / rate2;
        mouth_q = 0.5f * jet_a * U0;                                              // the jet's mean flow: the mouth's work per sample is Pm·Q_in
        sz.ins_pm = Pm; sz.ins_u0 = U0; sz.ins_tau = tau; sz.ins_gain = jet_g; sz.ins_area = jet_a;   // step 59b: for the inspection
        if (gliding && sz.bore_freq != f_model) {                                  // the bore follows the pitch per block: its cells and λ (to the ceiling)
            const voxo_t::HoleCal& hcal = v->suzu_hole[v->suzu_live.load(std::memory_order_acquire)];
            const float f_cut = sz.hole_j >= 0 ? f_model * exp2f(-(float)sz.hole_j / 12.0f - hcal.vent[sz.hole_reg] / 1200.0f) : f_model;   // step 81: the base glides, the holes ride
            const float ends = 2.0f * suzu::Bore::end_correction(sp.bore_loss, sz.bore.rad_a, f_cut, rate2);
            const int cells = suzu::Bore::cells_for(f_cut, rate2, sz.bore_lam_max, false, 0.0f, ends, (int)sp.bore_nodes);
            if (cells >= 2) { sz.bore.retune(cells, suzu::Bore::lambda_for(f_cut, rate2, cells, false, 0.0f, ends), suzu::BORE_CYLINDER, 0.0f, 0.0f); if (sz.hole_j >= 0) suzu_holes_place(sz.bore, cells, 0.0f, hcal.corr[sz.hole_reg], sp.hole_size, sp.hole_loss, 2.0f * sp.bore_corner_hz, rate2, sz.hole_j, sz.hole_reg); }
            sz.bore_freq = f_model;
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
    } else if (kind == 9) {
        // step 79: the release tightens the grid's contraction — the momentum's factor is the amplitude's squared; a
        // glide retunes the grid per frame below, saturating at the ε the gate admits
        const float r2 = rr * rr;
        sz.drum.g0 = sz.drum_g0 * r2; sz.drum.g2 = sz.drum_g2 * r2;
    } else if (kind == 11) {
        suzu_boris_fit(sz, sp, freq_end, rate2, vc.swirl);                     // step 84: the field follows the poly pressure, the spring the glide
    }
    float freq = freq_from, pg = pg0, env = sz.out_env;
    if (strobing) {                                                                              // step 75: the voice's clock this block
        own_clock = smode == 2 && (kind == 5 || (kind == 4 && drive != 0.0f && sz.duffing.dphi > 0.0f) || (kind == 11 && sz.gyro.theta > 0.0));   // step 84: the cyclotron period
        sz.strobe_hz = own_clock ? (kind == 5 ? (sz.rotor.period > 0.0f ? rate2 / sz.rotor.period : 0.0f) : kind == 11 ? sz.boris_hz : sz.duffing.dphi * rate2 / (2.0f * suzu::PI)) : v->strobe_hz;
    }
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
            else if (kind == 9 && gliding && sp.drum_cfl <= 0.0f) {                          // step 79: the grid follows the pitch, to the gate's ε
                float e = suzu::eps_for(freq, rate2); if (e > sz.drum_eps_max) e = sz.drum_eps_max;
                if (e != sz.drum_eps) {
                    sz.drum.tune(e, drum_share); sz.drum_eps = e;
                    sz.drum_emax = 2.0f * sz.drum_cap / (sz.drum.c2 + sz.drum.k2 * sz.drum.l11);
                    sz.drum.alpha = sz.drum_emax > 0.0f ? drum_swell * sz.drum.c2 / sz.drum_emax : 0.0f;
                }
            }
        }
        float acc = 0.0f, accr = 0.0f;                                                   // step 84: the gyroscope's right channel
        for (int sub = 0; sub < 2; sub++) {
            float sum, sum_r = 0.0f;
            float own_frac = -1.0f;                                                      // step 75: the own clock's wrap this sub-step, if any
            if (kind == 2) {
                if (slipping) {                                                          // step 80: the bow on its node, between the kick and the drift
                    const float v_before = sz.string.v[bow_ib];                          // the half-step velocity entering the kick
                    sz.string.kick(rr);
                    float f = 0.0f; const float vp = sz.string.v[bow_ib];
                    sz.string.v[bow_ib] = bow_naive ? suzu::StickSlip::solve_naive(vp, bow_vb, bow_F, bow_v0, 1.0f, &f) : suzu::StickSlip::solve(vp, bow_vb, bow_F, bow_v0, 1.0f, &f, &sz.bow_stuck);
                    // the string's gain is the leapfrog invariant's change: f at the SYNCHRONIZED velocity, the mean of the half-step
                    // velocities around the force (the staggered energy's own accounting; the probe of DECISIONS_12 #4 read it exact)
                    sz.bow_f = f; sz.bow_work += (double)f * (double)bow_vb; sz.bow_gain += (double)f * 0.5 * ((double)v_before + (double)sz.string.v[bow_ib]);
                    sz.string.drift();
                } else sz.string.step(rr);
                sum = sz.string.pickup(sp.pickup);
            }
            else if (kind == 3) {
                if (slipping) {                                                          // step 80: the bow on the loop
                    float f = 0.0f, vpost = 0.0f; sz.hybrid.bow(bow_beta, bow_vb, bow_F, bow_v0, bow_naive, &f, &sz.bow_stuck, &vpost);
                    sz.bow_f = f; sz.bow_work += (double)f * (double)bow_vb; sz.bow_gain += (double)f * (double)vpost;
                }
                float vb; sz.hybrid.step(&vb); sum = 0.5f * sz.hybrid.tap(sp.pickup) + body * vb;
            }
            else if (kind == 4) {
                const float ph_before = sz.duffing.phase;                            // step 75: the drive's phase before the step (the own-clock strobe)
                sz.duffing.step(drive); if (rr != 1.0f) sz.duffing.c.contract(rr); sum = sz.duffing.c.x;
                if (own_clock && sz.duffing.phase < ph_before) own_frac = (2.0f * suzu::PI - ph_before) / sz.duffing.dphi;   // the wrap fell in this sub-step
            }
            else if (kind == 7 || kind == 8) {
                float o;
                if (Pm > 1e-3f * SUZU_P_REF) { o = suzu_wind_step(sz.bore, sz.valve, Pm, wind_sigma, rr, wind_shear, wind_naive, kind == 7 ? sp.reed_inertance : 0.0f); sz.work += (double)Pm * (double)sz.valve.q; }   // 59c: the ledger's mouth work
                else o = sz.bore.step(0.0f, 0.0f, rr, wind_shear);
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
            else if (kind == 9) { sz.drum.step(); sum = sz.drum.pickup(pk_x, pk_y); }         // step 79: the membrane
            else if (kind == 11) {                                                               // step 84: the gyroscope — (x₁, x₂) the two channels, the cyclotron wrap the own clock
                const double phb = sz.gyro.phase;
                sz.gyro.step(); if (rr != 1.0f) sz.gyro.contract(rr);
                if (sz.gyro.phase >= 2.0 * (double)suzu::PI) { sz.gyro.phase -= 2.0 * (double)suzu::PI; if (own_clock) own_frac = (float)((2.0 * (double)suzu::PI - phb) / sz.gyro.theta); }
                sum = (float)sz.gyro.x1; sum_r = (float)sz.gyro.x2;
            }
            else if (kind == 10) {                                                               // step 83: the ring m times a sub-step, read twice (at ×4), the half-band down to this sub-step
                const int mm = sz.toda_m, half = mm / 2;
                for (int j = 1; j <= mm; j++) { sz.ring.step(rr_m); if (j == half || j == mm) sz.hb[0].push(sz.ring.pickup() / SUZU_TODA_STRAIN); }
                sum = sz.hb[0].out();
            }
            else {
                const float acc_before = sz.rotor.acc;                               // step 75: the kick clock's reading before the step (the own-clock strobe)
                sz.rotor.step(K); if (rr != 1.0f) sz.rotor.c.contract(rr); sum = sz.rotor.c.x;
                if (own_clock && sz.rotor.acc < acc_before) own_frac = sz.rotor.period - acc_before;   // the kick fell in this sub-step
            }
            if (tracing) {                                                           // step 59: the orbit — the cell's own pair, or the chain's phase plane
                float px, py, au = 0.0f, aw = 0.0f;                                 // 59c: the kind's aux pair
                if (kind == 4) { px = sz.duffing.c.x; py = sz.duffing.c.y; au = sz.duffing.phase; aw = drive; }
                else if (kind == 5) { px = sz.rotor.c.x; py = sz.rotor.c.y; au = sz.rotor.p; aw = K; }
                else if (kind == 11) { px = sum; py = sum_r; au = (float)sz.gyro.energy(); aw = sz.boris_hz; }   // step 84: the spirograph — (x₁, x₂) as drawn; the energy and the field
                else {
                    if (kind == 3 && sz.hybrid.nb > 0) { au = sz.hybrid.bridge[0].x; aw = sz.hybrid.bridge[0].y; }
                    else if (kind == 6) { au = sz.jet.eta_last; aw = sz.jet.q_prev; }
                    else if (kind == 7 || kind == 8) { au = sz.valve.y; aw = sz.valve.v; }
                    else if (kind == 9) { au = sz.drum.E; aw = sz.drum.c2 + sz.drum.alpha * sz.drum.E; }   // step 79: the stretching, the effective c²
                    else if (kind == 10) { au = sz.ring.strain_peak(); aw = sz.ring.momentum(); }           // step 83: the biggest soliton, the drift
                    px = sum; py = (sum - sz.tprev) * trace_dscale; sz.tprev = sum;
                }
                trace_push(sz, tdec, px, py, au, aw);
                if (strobing) {                                                      // step 75: a return where a wrap fell in this sub-step
                    const uint32_t ss = 2u * f + (uint32_t)sub;
                    float frac = -1.0f;
                    if (own_clock) frac = own_frac;
                    else if (sk < v->strobe_n && v->strobe_s[sk] == ss) { frac = v->strobe_f[sk]; sk++; }
                    if (frac >= 0.0f) strobe_push(sz, frac < 1.0f ? frac : 1.0f, px, py, au, aw);
                    sz.sprev[0] = px; sz.sprev[1] = py; sz.sprev[2] = au; sz.sprev[3] = aw;
                }
            }
            if (kind == 10) sz.hb[1].push(filter ? sz.svf.step(sum, f_svf, q) : sum);           // step 83: the second half-band stage to the output rate
            else acc += filter ? sz.svf.step(sum, f_svf, q) : sum;
            if (kind == 11) accr += filter ? sz.svf2.step(sum_r, f_svf, q) : sum_r;             // step 84: the right channel through its own filter
        }
        if (env < 1.0f) { env += sz.attack_step; if (env > 1.0f) env = 1.0f; }
        const float s = (kind == 10 ? sz.hb[1].out() : 0.5f * acc) * env * pg;
        if (kind == 11) {                                                                        // step 84: (x₁, x₂) straight to (L, R)
            const float sr = 0.5f * accr * env * pg;
            out_lr[2u * f]      += s;
            out_lr[2u * f + 1u] += sr;
            if (src) src[f] = 0.5f * (s + sr);
        } else {
            out_lr[2u * f]      += s;
            out_lr[2u * f + 1u] += s;
            if (src) src[f] = s;                                                                 // step 79: the plate's injection reads it
        }
    }
    sz.out_env = env;
    if (releasing) {
        const float amp = kind == 11 ? (float)sqrt(2.0 * sz.gyro.energy()) : kind == 10 ? sz.ring.strain_peak() / SUZU_TODA_STRAIN : kind == 9 ? sqrtf(2.0f * (float)sz.drum.energy_plain() / (float)(sz.drum.nx * sz.drum.ny)) / (sz.drum_eps > 1e-9f ? sz.drum_eps : 1e-9f) : kind == 2 ? sqrtf(sz.string.energy()) : kind == 3 ? sqrtf(sz.hybrid.energy() / (float)(sz.hybrid.n > 0 ? sz.hybrid.n : 1)) : kind == 4 ? sz.duffing.c.amp : (kind == 6 || kind == 7 || kind == 8) ? 4.0f * sqrtf(fabsf(sz.bore.energy()) / (float)(sz.bore.n > 0 ? sz.bore.n : 1)) : sz.rotor.c.amp;
        if (amp < SUZU_END_AMP) return false;
    }
    return true;
}

bool render_suzu(voxo_t* v, Voice& vc, const voxo_suzu_params_t& sp, float* out_lr, uint32_t frames,
                 float freq_from, float freq_step, bool pressure_live, uint32_t rate, float* src) {
    SuzuVoice& sz = vc.suzu;
    const uint32_t n = sz.n;
    const float rate2 = 2.0f * (float)rate;
    const float rateC = (sp.voice_kind <= 1u && sp.oversample == 4u) ? 4.0f * (float)rate : rate2;   // step 78: the cell family's section rate
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
    const float fc_max = 0.16f * rateC;                        // the classic form's fs/6 ceiling, at the oversampled rate
    float fc0 = sp.cutoff_hz * std::exp2((timbre_prev - 0.5f) * 6.0f);
    float fc1 = sp.cutoff_hz * std::exp2((vc.timbre - 0.5f) * 6.0f);
    const bool filter = sp.cutoff_hz < 19999.0f || sp.resonance > 0.01f;
    fc0 = clampf(sz.cutoff_prev >= 0.0f ? sz.cutoff_prev : fc0, 20.0f, fc_max);
    fc1 = clampf(fc1, 20.0f, fc_max);
    sz.cutoff_prev = fc1;
    const float q = 2.0f - 1.9f * clampf(sp.resonance, 0.0f, 1.0f);
    float f_svf = suzu::svf_f_for(fc0, rateC, q);
    const float f_svf_step = (suzu::svf_f_for(fc1, rateC, q) - f_svf) / (float)frames;
    if (sp.voice_kind >= 2) return render_suzu_other(v, vc, sp, out_lr, frames, freq_from, freq_step, rate2, pg0, pg_step, filter, f_svf, f_svf_step, q, src);   // step 58
    const float g_shear = clampf(sp.shear, 0.0f, 1.0f);
    const bool cubic = sp.shear_kind == 0;
    // the shear's detune, compensated in ε from the cell's own calibration (suzu.h)
    const float eps_comp = g_shear > 0.0f ? 1.0f / suzu::shear_table_ratio(v->shear_ratio[cubic ? 0 : 1], g_shear) : 1.0f;
    const bool plain = sp.retune_mode != 0, stepped = sp.retune_mode == 2, naive = sp.update_mode != 0;
    // the coupling this block: the patch's κ plus the swirl's share (the gate allowed 0.5)
    const float kappa = sz.n > 1 ? clampf(sp.coupling + (sp.modal_comp != 0u ? 0.5f * vc.swirl : 0.0f), 0.0f, suzu::COMP_KAPPA_MAX) : 0.0f;   // step 82: no swirl share on the raw lattice
    float comp[suzu::MODES_MAX];                                                  // the coupling's detune compensation at this block's κ and pitch
    if (n > 1 && sp.modal_comp != 0u) suzu_comp_for(v, v->suzu_live.load(std::memory_order_acquire), kappa, sz.table, sp.retune_mode == 2 ? vc.freq_target : freq_from + freq_step * (float)frames, rateC, comp);
    else for (uint32_t k = 0; k < n; k++) comp[k] = 1.0f;                                     // step 82: modal_comp 0 — the raw lattice
    // the bow: the breath's target energy per mode (the zone's breath on the voice's channel), off once released
    // the breath: the channel's breath CC, or the press when the patch lets it blow (a controller without a breath CC)
    float breath = vc.held ? v->channels[vc.channel & 15].breath : 0.0f;
    if (sp.press_blows != 0 && vc.held && vc.pressure > breath) breath = vc.pressure;
    const bool bowed = sp.bow_mode == 0u && sz.bow_g > 0.0f && breath > 0.0f && n > 0;   // no breath (or released): no bow — the voice ends by amplitude
    // Step 80 (TO_PROJECT_SPEC §13.10): THE STICK-SLIP BOW on the lattice's bowing point — the point's velocity Σ b_k·w_k
    // (w_k = −ε_k·y_k, the cell's velocity of x per sub-step; b_k the profile sin(kπ·pos)) is driven by the friction curve
    // against the bow's speed, implicitly and bounded (suzu::StickSlip); the force is shared out to the modes by b_k over
    // Σb_k² (the modal masses chosen so the point's mobility is 1 and the work f·v_s is the modes' gain). The mouth is
    // the speed, the press and the velocity the force; no mouth lifts the bow. The servo (bow_mode 0) is untouched.
    const bool slipping = sp.bow_mode == 1u && breath > 0.0f && vc.held && n > 0;
    float bow_vb = 0.0f, bow_F = 0.0f, bow_v0 = 1.0f, bow_invB = 0.0f;
    if (slipping) {
        const float e1 = suzu::eps_for(freq_from + freq_step * (float)frames, rateC);
        bow_vb = sp.bow_speed * breath * sp.level * e1 * SUZU_BOW_KV_LAT;
        bow_v0 = SUZU_BOW_V0 * bow_vb;
        bow_F = sp.bow_force * bow_vb * SUZU_BOW_KF_LAT * (0.5f + 0.5f * (float)vc.velocity / 127.0f) * (1.0f + vc.pressure);
        float B = 0.0f; for (uint32_t k = 0; k < n; k++) if (sz.eps[k] > 0.0f) B += sz.table.bow[k] * sz.table.bow[k];
        bow_invB = B > 1e-12f ? 1.0f / B : 0.0f;
        sz.bow_vb = bow_vb;
    } else sz.bow_vb = 0.0f;
    const bool bow_naive = sp.bow_naive != 0;
    if (bowed) {
        const float A = sp.level * (0.35f + 0.65f * breath) * breath;       // the target amplitude: the breath, twice — a soft breath sings softly
        for (uint32_t k = 0; k < n; k++) {
            // the bow's REACH: the servo's injection rate tops at 1/τ (u ≤ 1), so a mode whose declared decay
            // rate γ_k exceeds it cannot be held — it rings from the strike and dies, untouched (no re-seeding)
            const bool bowable = (1.0f - sz.decay[k]) < sz.bow_g;
            const float a = A * sz.table.bow[k]; sz.bow_t[k] = bowable ? a * a : 0.0f;
        }
    }
    const float ceiling = 0.5f * (float)rate;                                        // the OUTPUT Nyquist whatever the section's rate (step 78)
    const float r_low = sz.table.ratio[0];                                        // the coupling's reference: the lowest mode
    const float eps_c8 = suzu::eps_for(4186.009f * r_low, rateC);
    // The retune: every mode's ε from the pitch — a sinf per mode, so only when
    // the pitch MOVES (the block's first frame, then each frame of a glide; a
    // held note costs none). A mode that crosses the ceiling is muted there.
    // The lattice refreshes each mode's amplitude from its state first (the
    // coupling moved energy the cell's own `amp` did not see) so the re-base
    // keeps what the mode has, not what it was struck with.
    float e0 = suzu::eps_for((stepped ? vc.freq_target : freq_from) * r_low, rateC);
    float k_eps = 0.0f;
    auto retune_all = [&](float fr) {
        e0 = suzu::eps_for(fr * r_low, rateC);
        for (uint32_t k = 0; k < n; k++) {
            if (sz.eps[k] <= 0.0f) continue;
            const float fk = fr * sz.table.ratio[k];
            if (fk >= ceiling) { sz.eps[k] = 0.0f; sz.inv_eps[k] = 0.0f; sz.modes[k].reset(); continue; }   // crossed the ceiling: muted, a wall
            const float e = eps_comp * comp[k] * suzu::eps_for(fk, rateC);
            if (e == sz.modes[k].eps) continue;
            suzu::Cell& c = sz.modes[k];
            if (n > 1) { const float q = c.energy(); c.amp = q > 0.0f ? sqrtf(q / (1.0f - 0.25f * c.eps * c.eps)) : 0.0f; }
            if (plain) c.retune_plain(e); else c.retune(e);
            sz.eps[k] = e; sz.inv_eps[k] = 1.0f / e;
        }
        k_eps = n > 1 ? suzu::coupling_k(kappa, e0, eps_c8) : 0.0f;
    };
    if (stepped) {                                                                      // the feared form: once per block, no ramp
        for (uint32_t k = 0; k < n; k++) if (sz.eps[k] > 0.0f) { const float e = eps_comp * comp[k] * suzu::eps_for(vc.freq_target * sz.table.ratio[k], rateC); sz.modes[k].retune_plain(e); sz.eps[k] = e; sz.inv_eps[k] = 1.0f / e; }
        k_eps = n > 1 ? suzu::coupling_k(kappa, e0, eps_c8) : 0.0f;
    }
    float freq = freq_from, pg = pg0, env = sz.out_env;
    const bool releasing = vc.releasing;
    const float r_rel = sz.contract;
    const float mix = sz.mix;
    const bool gliding = freq_step != 0.0f;
    const bool tracing = ((v->trace_mask.load(std::memory_order_relaxed) >> sp.voice_kind) & 1u) != 0;   // step 59
    const uint32_t tdec = trace_decim_of(v);
    const bool strobing = tracing && v->strobe_mode.load(std::memory_order_relaxed) != 0;         // step 75: the cells and the lattice keep the bass phasor
    uint32_t sk = 0;
    if (strobing) sz.strobe_hz = v->strobe_hz;
    // Step 78: the cell family's section — N sub-steps an output sample (2: the shipped two-tap average, bit for bit; 4: the
    // half-band oversampler, two 2:1 stages); the lattice in four lanes (suzu.h) unless the lab's naive update asks for the
    // scalar loop; the strobe's sub-steps stay the 2× section's (the bus precomputes the phasor's wraps at that rate): at ×4
    // a wrap at fraction φ of a 2× sub-step falls in its first or second quarter, with the fraction rescaled.
    const int N = sp.oversample == 4u ? 4 : 2, halfN = N / 2;
    float rr_k[suzu::MODES_MAX]; for (uint32_t k = 0; k < suzu::MODES_MAX; k++) rr_k[k] = 1.0f;
    for (uint32_t f = 0; f < frames; f++) {
        freq += freq_step; pg += pg_step;
        f_svf += f_svf_step;
        if (!stepped && (gliding || f == 0)) retune_all(freq);
        float acc = 0.0f;
        for (int sub = 0; sub < N; sub++) {
            float sum = 0.0f, sumy = 0.0f;
            if (naive) {                                                                             // the lab's negative control: the scalar loop as step 57 wrote it
                if (n > 1 && k_eps > 0.0f) suzu::lattice_kick(sz.modes, sz.inv_eps, (int)n, k_eps);
                for (uint32_t k = 0; k < n; k++) {
                    if (sz.eps[k] <= 0.0f) continue;
                    suzu::Cell& c = sz.modes[k];
                    c.step_naive();
                    float rr = releasing ? (r_rel < sz.decay[k] ? r_rel : sz.decay[k]) : sz.decay[k];
                    if (bowed && sz.bow_t[k] > 0.0f) {
                        const float E = c.x * c.x + c.y * c.y;
                        if (E < 1e-10f) c.kick(0.1f * std::sqrt(sz.bow_t[k]), c.eps);
                        rr *= suzu::bow_factor(c.x * c.x + c.y * c.y, sz.bow_t[k], sz.bow_g);
                    }
                    if (rr != 1.0f) c.contract(rr);
                    sum += c.x; sumy += c.y;
                }
            } else {
                // (1) the coupling kick from the pre-update positions and (2) each cell's rotation, four lanes at a time;
                // (3) the declared damping per mode — the mode's own while held, the release's after — with (4) the bow's
                // servo where the breath feeds the mode (self-excited, bounded; the seed a declared drive); the contraction in
                // lanes; the sums in scalar order over the alive modes
                suzu::lattice_rotate_simd(sz.modes, sz.inv_eps, (int)n, k_eps, g_shear, cubic);
                for (uint32_t k = 0; k < n; k++) {
                    float rr = releasing ? (r_rel < sz.decay[k] ? r_rel : sz.decay[k]) : sz.decay[k];
                    if (bowed && sz.bow_t[k] > 0.0f && sz.eps[k] > 0.0f) {
                        suzu::Cell& c = sz.modes[k];
                        const float E = c.x * c.x + c.y * c.y;
                        if (E < 1e-10f) c.kick(0.1f * std::sqrt(sz.bow_t[k]), c.eps);
                        rr *= suzu::bow_factor(c.x * c.x + c.y * c.y, sz.bow_t[k], sz.bow_g);
                    }
                    rr_k[k] = rr;
                }
                suzu::lattice_contract_simd(sz.modes, (int)n, rr_k);
                if (slipping && bow_invB > 0.0f) {                                                  // step 80: the bow on the point, after the modes' own step
                    float vs = 0.0f, vsx = 0.0f;
                    for (uint32_t k = 0; k < n; k++) if (sz.eps[k] > 0.0f) { vs += sz.table.bow[k] * (-sz.eps[k] * sz.modes[k].y); vsx += sz.table.bow[k] * sz.eps[k] * sz.eps[k] * sz.modes[k].x; }
                    float f = 0.0f;
                    const float vpost = bow_naive ? suzu::StickSlip::solve_naive(vs, bow_vb, bow_F, bow_v0, 1.0f, &f) : suzu::StickSlip::solve(vs, bow_vb, bow_F, bow_v0, 1.0f, &f, &sz.bow_stuck);
                    for (uint32_t k = 0; k < n; k++) if (sz.eps[k] > 0.0f) sz.modes[k].y -= f * sz.table.bow[k] * bow_invB * sz.inv_eps[k];
                    // the modes' invariants' change: f at the synchronized point velocity (the rotation's own kick −ε²x halved back in)
                    sz.bow_f = f; sz.bow_work += (double)f * (double)bow_vb; sz.bow_gain += (double)f * ((double)vpost - 0.5 * (double)f + 0.5 * (double)vsx);
                }
                for (uint32_t k = 0; k < n; k++) { if (sz.eps[k] <= 0.0f) continue; sum += sz.modes[k].x; sumy += sz.modes[k].y; }
            }
            sum *= mix;
            if (tracing) {
                trace_push(sz, tdec, sum, sumy * mix, sz.modes[0].x, sz.modes[0].y);                 // step 59: the lattice's orbit (Σx, Σy); 59c: mode 0 beside
                if (strobing) {                                                                      // step 75: a return where the phasor wrapped in this sub-step
                    const uint32_t ss = 2u * f + (uint32_t)(sub / halfN);
                    if (sk < v->strobe_n && v->strobe_s[sk] == ss) {
                        const float fr = v->strobe_f[sk] * (float)halfN - (float)(sub % halfN);     // the wrap's fraction within this sub-step (at ×2 the fraction itself)
                        if (fr >= 0.0f && (fr < 1.0f || sub % halfN == halfN - 1)) { strobe_push(sz, fr < 1.0f ? fr : 1.0f, sum, sumy * mix, sz.modes[0].x, sz.modes[0].y); sk++; }
                    }
                    sz.sprev[0] = sum; sz.sprev[1] = sumy * mix; sz.sprev[2] = sz.modes[0].x; sz.sprev[3] = sz.modes[0].y;
                }
            }
            const float o = filter ? sz.svf.step(sum, f_svf, q) : sum;
            if (N == 2) acc += o;
            else { sz.hb[0].push(o); if (sub & 1) sz.hb[1].push(sz.hb[0].out()); }
        }
        if (env < 1.0f) { env += sz.attack_step; if (env > 1.0f) env = 1.0f; }
        const float s = (N == 2 ? 0.5f * acc : sz.hb[1].out()) * env * pg;
        out_lr[2u * f]      += s;
        out_lr[2u * f + 1u] += s;
        if (src) src[f] = s;                                                                             // step 79: the plate's injection reads it
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
    new (&v->breaker_trips) std::atomic<uint32_t>(0);   // step 75
    { suzu::Bore* b = new suzu::Bore; b->setup(64, suzu::BORE_CYLINDER, 0.0f, 0.0f, suzu::END_OPEN, 0.3f, suzu::END_OPEN, 0.3f); v->flute_mu64 = b->mu_max(); delete b; }   // step 81: the flute's μ once (the open ends are pinned whatever their loss)
    new (&v->tuning_live) std::atomic<uint32_t>(0); new (&v->tuning_count) std::atomic<uint32_t>(0);   // step 82: equal temperament until a scale is set
    for (int i = 0; i < 128; i++) v->tuning_hz[0][i] = v->tuning_hz[1][i] = note_to_hz((float)i);
    v->tuning_name[0] = 0;
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
    new (&v->strobe_mode) std::atomic<uint32_t>(0);      // step 75
    new (&v->strobe_hz_out) std::atomic<float>(0.0f);
    voxo_suzu_default_params(&v->suzu_params[0]);
    {                                                    // step 57: the default patch's compensation table (both slots start from it)
        suzu::ModalTable t; const voxo_suzu_params_t& d = v->suzu_params[0];
        suzu::modal_table(&t, (int)d.modal_preset, (int)d.modes, d.decay_s, d.decay_bright, d.stiffness, d.pluck, d.bow_position, d.modal_ratio, d.modal_t60, d.modal_weight);
        suzu_comp_fill(t, v->suzu_comp[0], v->suzu_comp_jinv[0]);
        std::memcpy(v->suzu_comp[1], v->suzu_comp[0], sizeof v->suzu_comp[0]);
        std::memcpy(v->suzu_comp_jinv[1], v->suzu_comp_jinv[0], sizeof v->suzu_comp_jinv[0]);
        v->suzu_comp_key[0] = v->suzu_comp_key[1] = suzu_comp_key_of(d);
    v->pendulum.reset(); v->mod_smooth = 0.0f;
    v->suzu_kind_now = v->suzu_params[0].voice_kind;   // step 75
    std::memset(v->suzu_wind, 0, sizeof v->suzu_wind);   // the winds calibrate when a wind patch is set
    std::memset(v->suzu_hole, 0, sizeof v->suzu_hole); v->keys = 0u;   // step 81
    }
    v->suzu_params[1] = v->suzu_params[0];
    suzu::shear_table_fill(true, v->shear_ratio[0]);    // the cell calibrates its shears (suzu.h)
    suzu::shear_table_fill(false, v->shear_ratio[1]);
    v->source_now = VOXO_SOURCE_SAMPLER;
    for (int c2 = 0; c2 < 16; c2++) v->channels[c2].timbre = 0.5f;   // the slide at its centre until CC 74 says
    v->reverb = new (std::nothrow) voxo_bus::Reverb;
    v->delay = new (std::nothrow) voxo_bus::Delay;
    if (!v->reverb || !v->delay) { delete v->reverb; delete v->delay; sumi_normalizer_destroy(v->normalizer); std::free(v); return nullptr; }
    v->plate = new (std::nothrow) suzu::Membrane<suzu::PLATE_MAX>;                      // step 79: the bus plate and the voices' blocks for its injection
    v->plate_src = new (std::nothrow) float[(size_t)v->max_voices * PLATE_SRC_MAX];
    if (!v->plate || !v->plate_src) { delete v->plate; delete[] v->plate_src; delete v->reverb; delete v->delay; sumi_normalizer_destroy(v->normalizer); std::free(v); return nullptr; }
    v->plate->reset(suzu::PLATE_MAX, suzu::PLATE_MAX); v->plate_live = false; v->plate_n = 0;
    for (int c2 = 0; c2 < 16; c2++) for (int n2 = 0; n2 < 128; n2++) { v->strike_x[c2][n2] = v->strike_y[c2][n2] = -1.0f; v->strike_pluck[c2][n2] = -1.0f; }
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
    delete v->plate; delete[] v->plate_src;   // step 79
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

void voxo_set_strike_point(voxo_t* v, uint8_t channel, uint8_t note, float x, float y) {   // step 79
    if (!v || note > 127) return;
    v->strike_x[channel & 15][note] = x; v->strike_y[channel & 15][note] = y;
}
void voxo_set_strike_pluck(voxo_t* v, uint8_t channel, uint8_t note, float position) {   // step 80
    if (!v || note > 127) return;
    v->strike_pluck[channel & 15][note] = position;
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
    out->oversample = 2;   // step 78: the shipped 2× section; 4 the half-band oversampler
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
    out->drum_size = 16; out->drum_kappa = 0.02f; out->drum_alpha = 0.0f; out->drum_cap = 1.0f; out->drum_cfl_gate = 1; out->drum_cfl = 0.0f;   // step 79
    out->plate_on = 0; out->plate_size = 64; out->plate_hz = 110.0f; out->plate_kappa = 0.003f; out->plate_decay_s = 2.0f; out->plate_bright = 0.02f; out->plate_gain = 1.0f;
    out->bow_mode = 0; out->bow_force = 0.5f; out->bow_speed = 1.0f; out->bow_naive = 0;   // step 80
    out->reed_inertance = 0.0f; out->bore_leak = 0.4f; out->brass_shock = 1.0f; out->shock_limiter = 1; out->lip_shear = 0.0f; out->lip_shear_ratio = 1.5f;
    out->tonehole_mode = 0; out->hole_size = 1.0f; out->hole_loss = 0.3f;   // step 81
    for (int k = 0; k < VOXO_MODAL_MAX; k++) { out->modal_ratio[k] = (float)(k + 1); out->modal_t60[k] = 0.0f; out->modal_weight[k] = 0.0f; }   // step 82: the harmonic series, the formula's decays and weights
    out->modal_comp = 1;
    out->toda_nodes = 16; out->toda_b = 0.6f; out->toda_cap = 6.0f; out->toda_width = 1.5f; out->toda_gate = 1;   // step 83
    out->boris_b = 24.0f; out->boris_b_rest = 2.0f; out->boris_prewarp = 1;   // step 84
}

// The lattice load gate (SYNTH §2.5, the CFL analog): λ_max of the stiffness +
// coupling matrix at the highest playable note, the swirl's share included.
// κ here is the TOTAL coupling (the patch's + the swirl's share, up to 0.5).
static float suzu_lambda_max_at(const voxo_t* v, const voxo_suzu_params_t* p, float kappa, int* worst_midi = nullptr) {
    const float rate2 = 2.0f * (float)v->device_rate.load(std::memory_order_relaxed);
    suzu::ModalTable t;
    suzu::modal_table(&t, (int)p->modal_preset, (int)p->modes, p->decay_s, p->decay_bright, p->stiffness, p->pluck, p->bow_position, p->modal_ratio, p->modal_t60, p->modal_weight);
    float comp[suzu::MODES_MAX];
    if (p->modal_comp != 0u && !suzu::lattice_compensation(t.ratio, t.n, kappa, comp)) return 1e9f;   // infeasible counts as over the bound (monotone in κ as well); step 82: the raw lattice has no solve to fail
    return suzu::lattice_lambda_max_over_pitch(t.ratio, t.n, rate2, kappa, worst_midi, p->modal_comp != 0u);
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
    p.oversample = params->oversample == 4u ? 4u : 2u;   // step 78
    p.drum_size = params->drum_size < 4u ? 4u : params->drum_size > (uint32_t)suzu::MEMBRANE_MAX ? (uint32_t)suzu::MEMBRANE_MAX : params->drum_size;   // step 79
    p.drum_kappa = clampf(params->drum_kappa, 0.0f, 1.0f); p.drum_alpha = clampf(params->drum_alpha, 0.0f, 4.0f); p.drum_cap = params->drum_cap > 0.0f ? params->drum_cap : 1.0f;
    p.plate_size = params->plate_size < 16u ? 16u : params->plate_size > (uint32_t)suzu::PLATE_MAX ? (uint32_t)suzu::PLATE_MAX : params->plate_size;
#if defined(VOXO_MOBILE)
    if (p.plate_size > 32u) p.plate_size = 32u;                                   // the mobile tier (the biharmonic CFL scales as h²)
#endif
    p.plate_hz = clampf(params->plate_hz, 20.0f, 2000.0f); p.plate_kappa = clampf(params->plate_kappa, 0.0f, 1.0f); p.plate_gain = clampf(params->plate_gain, 0.0f, 4.0f);
    p.plate_decay_s = params->plate_decay_s > 0.0f ? params->plate_decay_s : 0.0f; p.plate_bright = params->plate_bright > 0.0f ? params->plate_bright : 0.0f;
    p.bow_mode = params->bow_mode == 1u ? 1u : 0u; p.bow_force = clampf(params->bow_force, 0.0f, 4.0f); p.bow_speed = clampf(params->bow_speed, 0.25f, 4.0f);   // step 80
    p.reed_inertance = params->reed_inertance > 0.0f ? params->reed_inertance : 0.0f; p.bore_leak = clampf(params->bore_leak, 0.0f, 1.0f); p.brass_shock = clampf(params->brass_shock, 0.0f, 4.0f);   // step 81
    p.lip_shear = clampf(params->lip_shear, 0.0f, 1.0f); p.lip_shear_ratio = clampf(params->lip_shear_ratio, 0.25f, 4.0f);
    p.tonehole_mode = params->tonehole_mode == 1u ? 1u : 0u; p.hole_size = clampf(params->hole_size, 0.1f, 4.0f); p.hole_loss = clampf(params->hole_loss, 0.0f, 2.0f);
    p.modal_preset = params->modal_preset > 5u ? 0u : params->modal_preset; p.modal_comp = params->modal_comp != 0u ? 1u : 0u;   // step 82
    p.toda_nodes = params->toda_nodes < 4u ? 4u : params->toda_nodes > (uint32_t)suzu::TODA_NODES_MAX ? (uint32_t)suzu::TODA_NODES_MAX : params->toda_nodes;   // step 83
#if defined(VOXO_MOBILE)
    if (p.toda_nodes > 32u) {                                                     // step 83: the plate's kind of cap — the ring's nodes at most 32 on the tablets' tier
        if (v->log_cb) { char msg[128]; std::snprintf(msg, sizeof msg, "suzu: the Toda ring's %u nodes capped at 32 on this device (the tablets' tier)", p.toda_nodes); v->log_cb(1, msg, v->log_user); }
        p.toda_nodes = 32u;
    }
#endif
    p.toda_b = clampf(params->toda_b, 0.0f, 1.0f); p.toda_cap = clampf(params->toda_cap, 1.0f, 12.0f); p.toda_width = clampf(params->toda_width, 0.5f, 4.0f); p.toda_gate = params->toda_gate != 0u ? 1u : 0u;
    p.boris_b = clampf(params->boris_b, 0.0f, SUZU_BORIS_BMAX); p.boris_b_rest = clampf(params->boris_b_rest, 0.0f, SUZU_BORIS_BMAX); p.boris_prewarp = params->boris_prewarp != 0u ? 1u : 0u;   // step 84
    for (int k = 0; k < VOXO_MODAL_MAX; k++) {
        p.modal_ratio[k] = params->modal_ratio[k] > 0.0f ? clampf(params->modal_ratio[k], 0.05f, 64.0f) : 0.0f;
        p.modal_t60[k] = params->modal_t60[k] > 0.0f ? clampf(params->modal_t60[k], 0.01f, 120.0f) : 0.0f;
        p.modal_weight[k] = params->modal_weight[k] > 0.0f ? clampf(params->modal_weight[k], 0.0f, 8.0f) : 0.0f;
    }
    suzu::ModalTable table;
    suzu::modal_table(&table, (int)p.modal_preset, (int)p.modes, p.decay_s, p.decay_bright, p.stiffness, p.pluck, p.bow_position, p.modal_ratio, p.modal_t60, p.modal_weight);
    if (p.voice_kind == 2 && p.cfl_gate != 0 && p.string_cfl > 1.0f) {     // step 58: the CFL gate (the derived k·dt² never exceeds 1; the forced one is the lab's)
        if (v->log_cb) {
            char msg[200];
            std::snprintf(msg, sizeof msg, "suzu: the patch is rejected — the chain's k·dt² = %.3f exceeds the CFL bound 1 (the explicit scheme is unstable past it: the top mode over Nyquist)", (double)p.string_cfl);
            v->log_cb(2, msg, v->log_user);
        }
        return false;
    }
    if (p.voice_kind == 9 && p.drum_cfl_gate != 0 && p.drum_cfl > 1.0f) {   // step 79: the membrane's CFL gate (the derived λ_2D never exceeds 1.95 at E_max; the forced one is the lab's)
        if (v->log_cb) {
            char msg[240];
            std::snprintf(msg, sizeof msg, "suzu: the patch is rejected — the membrane's Courant number is forced to %.3f× its gate (λ_2D = sqrt(8(c² + αE_max) + 64κ²) ≤ %.2f: past 2 the top mode's leapfrog is unstable)", (double)p.drum_cfl, (double)(p.drum_alpha > 0.0f ? suzu::MEMBRANE_CFL_GATE_GONG : suzu::MEMBRANE_CFL_GATE));
            v->log_cb(2, msg, v->log_user);
        }
        return false;
    }
    if (p.plate_on != 0 && v->log_cb) {                                      // step 79: the plate's tuning under the gate at the output rate — clamped, not rejected (a bus element), the line says so
        float eps, share; const float rate = (float)v->device_rate.load(std::memory_order_relaxed);
        if (suzu_plate_tuning(p, rate, (int)p.plate_size, &eps, &share)) {
            char msg[240];
            const float hz = rate / suzu::PI * asinf(0.5f * eps);
            std::snprintf(msg, sizeof msg, "suzu: the bus plate at %u² takes the biharmonic share %.5f at %.1f Hz (asked %.5f at %.1f Hz): the gate λ_2D ≤ 1.95 at the output rate", p.plate_size, (double)share, (double)hz, (double)p.plate_kappa, (double)p.plate_hz);
            v->log_cb(1, msg, v->log_user);
        }
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
                char msg[320];   // the message below runs past 240 bytes (its UTF-8 glyphs) — clang's -Wformat-truncation on the Tab
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
        if (p.modal_comp != 0u && !suzu::lattice_compensation(table.ratio, table.n, p.coupling + (p.modal_comp != 0u ? SUZU_SWIRL_SHARE : 0.0f), comp)) {   // the swirl can add its share; step 82: the raw lattice skips the solve
            if (v->log_cb) {
                char msg[240];
                std::snprintf(msg, sizeof msg, "suzu: the patch is rejected — at coupling %.3f + the swirl's %.1f a mode's own stiffness would be gone (the spring between neighbours stiffer than the partial's own); the patch's bound is %.3f",
                              (double)p.coupling, (double)(p.modal_comp != 0u ? SUZU_SWIRL_SHARE : 0.0f), (double)voxo_suzu_coupling_bound(v, &p));
                v->log_cb(2, msg, v->log_user);
            }
            return false;
        }
        int worst = 0;
        const float lam = suzu_lambda_max_at(v, &p, p.coupling + (p.modal_comp != 0u ? SUZU_SWIRL_SHARE : 0.0f), &worst);
        if (lam >= suzu::LATTICE_BOUND) {
            if (v->log_cb) {
                char msg[240];
                std::snprintf(msg, sizeof msg, "suzu: the patch is rejected — its coupled lattice's spectral radius %.3f reaches the sampling bound %.0f at MIDI %d (coupling %.3f + the swirl's %.1f); the patch's bound is %.3f",
                              (double)lam, (double)suzu::LATTICE_BOUND, worst, (double)p.coupling, (double)(p.modal_comp != 0u ? SUZU_SWIRL_SHARE : 0.0f), (double)voxo_suzu_coupling_bound(v, &p));
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
    } else if (p.modal_comp != 0u) suzu_comp_fill(table, v->suzu_comp[idle], v->suzu_comp_jinv[idle]);   // step 82: unread under modal_comp 0
    v->suzu_comp_key[idle] = key;
    if (p.voice_kind == 7 || p.voice_kind == 8) {                          // step 58c: the winds' calibration beside the patch (once per embouchure change; ~100 ms)
        const uint64_t wkey = suzu_wind_key_of(p);
        if (wkey == v->suzu_wind[live].key) v->suzu_wind[idle] = v->suzu_wind[live];
        else suzu_wind_calibrate(v, p, v->suzu_wind[idle]);
    }
    if (p.tonehole_mode == 1u && (p.voice_kind == 6 || p.voice_kind == 7)) {   // step 81: the toneholes' calibration beside the patch (the base bore pulsed per fingering)
        const uint64_t hkey = suzu_hole_key_of(p);
        if (hkey == v->suzu_hole[live].key) v->suzu_hole[idle] = v->suzu_hole[live];
        else suzu_hole_calibrate(v, p, v->suzu_wind[idle], v->suzu_hole[idle]);
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
    if (sp.voice_kind != v->suzu_kind_now) {   // step 75 (found by the strobe fuzz): a patch of ANOTHER VOICE KIND ends the sounding voices —
        v->suzu_kind_now = sp.voice_kind;      // their bodies were built for the kind they were struck as (the trumpet's bore over a cell's
        if (v->source_now != VOXO_SOURCE_SAMPLER)   // state was NaN in two blocks, and a NaN never leaves the bus); the same kind's knobs stay live
            for (uint32_t i = 0; i < v->max_voices; i++) { v->voices[i].active = false; for (uint32_t k = 0; k < MAX_LAYERS; k++) v->voices[i].layers[k].active = false; }
    }
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
    // Step 75 (KESSHO §2.5): THE REFERENCE PHASOR — the bass's frequency this block (the lowest held voice's, bend included;
    // the lowest sounding one when none is held), advanced per sub-step in double along the block's glide line; its wraps
    // precomputed (the sub-step each completes in and the fraction into it) so every voice's loop reads the same instants.
    // Not a zero-crossing detector on the rendered signal (ill-defined for a chain or a bore). Nothing of it touches the output.
    v->strobe_n = 0; v->strobe_hz = 0.0f;
    if (v->strobe_mode.load(std::memory_order_relaxed) != 0 && v->source_now != VOXO_SOURCE_SAMPLER && v->trace_mask.load(std::memory_order_relaxed) != 0) {
        float f0 = 0.0f, f1 = 0.0f; bool held_any = false;
        for (uint32_t vi = 0; vi < v->max_voices; vi++) {
            const Voice& vc = v->voices[vi];
            if (!vc.active) continue;
            if (vc.held && !held_any) { held_any = true; f1 = 0.0f; }                 // a held voice outranks every released one
            if (held_any && !vc.held) continue;
            if (f1 <= 0.0f || vc.freq_target < f1) { f0 = vc.freq; f1 = vc.freq_target; }
        }
        if (f1 > 0.0f) {
            const uint32_t ns = 2u * frames; const double rate2 = 2.0 * (double)rate;
            const double d0 = (double)f0 / rate2, d1 = (double)f1 / rate2;
            double ph = v->strobe_phase;
            for (uint32_t s = 0; s < ns; s++) {
                const double dph = d0 + (d1 - d0) * ((double)s + 1.0) / (double)ns;
                const double before = ph; ph += dph;
                if (ph >= 1.0) {
                    ph -= 1.0;
                    if (v->strobe_n < STROBE_BLOCK_MAX) { v->strobe_s[v->strobe_n] = s; v->strobe_f[v->strobe_n] = (float)((1.0 - before) / dph); v->strobe_n++; }
                }
            }
            v->strobe_phase = ph; v->strobe_hz = f1;
        } else v->strobe_phase = 0.0;
    } else v->strobe_phase = 0.0;
    v->strobe_hz_out.store(v->strobe_hz, std::memory_order_relaxed);
    // Step 79 (TO_PROJECT_SPEC §13.9): THE BUS PLATE — set up from the patch at the output rate when it turns on or its
    // numbers move (the size, the tuning under the gate, the contraction), cleared when it turns on or changes size; the
    // sounding Suzu voices write their blocks beside the sum and inject them at their points after the loop
    const bool plate_on = sp.plate_on != 0 && v->source_now != VOXO_SOURCE_SAMPLER && frames <= PLATE_SRC_MAX;
    int plate_node[MAX_VOICES_CAP]; uint32_t plate_voice[MAX_VOICES_CAP]; uint32_t plate_count = 0;
    if (plate_on) {
        uint32_t pn = sp.plate_size < 16u ? 16u : sp.plate_size > (uint32_t)suzu::PLATE_MAX ? (uint32_t)suzu::PLATE_MAX : sp.plate_size;
#if defined(VOXO_MOBILE)
        if (pn > 32u) pn = 32u;
#endif
        const bool fresh_plate = !v->plate_live || v->plate_n != pn;
        if (fresh_plate) { v->plate->reset((int)pn, (int)pn); v->plate_live = true; v->plate_n = pn; }
        if (fresh_plate || v->plate_hz_set != sp.plate_hz || v->plate_kappa_set != sp.plate_kappa || v->plate_decay_set != sp.plate_decay_s || v->plate_bright_set != sp.plate_bright) {
            float eps, share; suzu_plate_tuning(sp, (float)rate, (int)pn, &eps, &share);
            v->plate->tune(eps, share); v->plate->damp(sp.plate_decay_s, sp.plate_bright, (float)rate);
            v->plate_hz_set = sp.plate_hz; v->plate_kappa_set = sp.plate_kappa; v->plate_decay_set = sp.plate_decay_s; v->plate_bright_set = sp.plate_bright;
        }
    } else v->plate_live = false;
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
            suzu_alive = render_suzu(v, vc, sp, out_lr, frames, freq_from, freq_step, pressure_live, rate, plate_on ? v->plate_src + (size_t)vi * PLATE_SRC_MAX : nullptr);
            if (plate_on) { plate_voice[plate_count] = vi; plate_node[plate_count++] = v->plate->node_at(vc.suzu.plate_x, vc.suzu.plate_y); }   // step 79: its block is written whether or not it ended
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
    // Step 79: THE PLATE under the voices — each voice's block into its node a sample at a time (a DRIVE: the voice's
    // own bounded sample), the plate stepped once an output sample, its two pickups into the stereo sum; it rings on
    // after the voices end, by its declared contraction
    if (plate_on) {
        suzu::Membrane<suzu::PLATE_MAX>& P = *v->plate;
        const float g = sp.plate_gain * PLATE_NORM, drive = PLATE_DRIVE * ((float)v->plate_n * (1.0f / 64.0f)) * ((float)v->plate_n * (1.0f / 64.0f));
        for (uint32_t f = 0; f < frames; f++) {
            for (uint32_t k = 0; k < plate_count; k++) P.inject(plate_node[k], drive * v->plate_src[(size_t)plate_voice[k] * PLATE_SRC_MAX + f]);
            P.step();
            out_lr[2u * f]      += g * P.pickup(PLATE_PICK_LX, PLATE_PICK_LY);
            out_lr[2u * f + 1u] += g * P.pickup(PLATE_PICK_RX, PLATE_PICK_RY);
        }
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
    // Step 75 (found by the strobe fuzz): THE BREAKER — a non-finite block never leaves the bus. The flute's ceiling above was
    // the cause found; whatever the next one is, the voices end, the reverb and the delay clear (a NaN in their feedback is
    // forever), the block goes out as silence and the stats count the trip. The check is 2·frames compares a block.
    {
        bool fin = true;
        for (uint32_t i = 0; i < 2u * frames; i++) if (!std::isfinite(out_lr[i])) { fin = false; break; }
        if (!fin) {
            for (uint32_t i = 0; i < v->max_voices; i++) { v->voices[i].active = false; for (uint32_t k = 0; k < MAX_LAYERS; k++) v->voices[i].layers[k].active = false; }
            v->reverb->clear(); v->delay->clear(); v->plate->clear();   // step 79: the plate's state goes with the tails
            std::memset(out_lr, 0, sizeof(float) * 2u * frames);
            active = 0; layers_active = 0;
            v->breaker_trips.fetch_add(1, std::memory_order_relaxed);
        }
    }
    v->clock_seconds += (double)frames * inv_rate;
    v->active_voices.store(active, std::memory_order_relaxed);
    v->active_layers.store(layers_active, std::memory_order_relaxed);
}

// Step 59 (SYNTH §2.7): the orbit trace's mask and poll.
// ---------------------------------------------------------------------------------------------------------------------
// Step 82 (TO_PROJECT_SPEC §13.12): THE TUNING — Scala's .scl and .kbm, parsed without allocation into the engine's
// note table. A scale: comment lines begin with '!'; then the description, the count, and `count` degrees — a value with
// a '.' is cents, with a '/' a ratio a/b, else an integer ratio — the last degree the period. A mapping: the size, the
// first and last notes to map, the middle note (degree 0), the reference note and its frequency, the formal octave's
// degree, then `size` lines each a degree or 'x' (unmapped: the key's own index). A note's frequency: the reference's
// times the scale's ratio at (the key's degree + its octave × the formal octave), over the reference's own ratio.
// ---------------------------------------------------------------------------------------------------------------------
namespace {
struct Scl { int count; double ratio[129]; char name[96]; };          // ratio[d] for d = 1..count (ratio[count] the period); [0] = 1
struct Kbm { int size, first, last, middle, ref, octave; double ref_hz; int map[128]; };
static const char* line_next(const char* s, const char** line, size_t* len) {   // the next non-comment line; nullptr at the end
    for (;;) {
        if (!s || !*s) return nullptr;
        const char* e = s; while (*e && *e != '\n') e++;
        const char* b = s; while (b < e && (*b == ' ' || *b == '\t' || *b == '\r')) b++;
        s = *e ? e + 1 : e;
        if (b < e && *b == '!') continue;
        *line = b; *len = (size_t)(e - b); return s;
    }
}
static bool scl_degree(const char* t, size_t n, double* out) {   // cents with a '.', a/b with a '/', else an integer ratio
    char buf[64]; size_t m = 0; while (m < n && m < sizeof buf - 1 && t[m] != ' ' && t[m] != '\t' && t[m] != '\r') { buf[m] = t[m]; m++; } buf[m] = 0;
    if (m == 0) return false;
    if (std::strchr(buf, '.')) { char* e; const double c = std::strtod(buf, &e); if (e == buf) return false; *out = std::exp2(c / 1200.0); return *out > 0.0; }
    if (const char* sl = std::strchr(buf, '/')) { char* e; const double a = std::strtod(buf, &e); if (e != sl) return false; const double b = std::strtod(sl + 1, &e); if (e == sl + 1 || b <= 0.0 || a <= 0.0) return false; *out = a / b; return true; }
    { char* e; const double a = std::strtod(buf, &e); if (e == buf || a <= 0.0) return false; *out = a; return true; }
}
static bool scl_parse(const char* text, Scl* o, char* err, size_t errn) {
    const char* line; size_t len; const char* s = text;
    if (!(s = line_next(s, &line, &len))) { std::snprintf(err, errn, "no description line"); return false; }
    const size_t nn = len < sizeof o->name - 1 ? len : sizeof o->name - 1; std::memcpy(o->name, line, nn); o->name[nn] = 0;
    if (!(s = line_next(s, &line, &len))) { std::snprintf(err, errn, "no count line"); return false; }
    const long count = std::strtol(line, nullptr, 10);
    if (count < 1 || count > 128) { std::snprintf(err, errn, "the count %ld is outside 1..128", count); return false; }
    o->count = (int)count; o->ratio[0] = 1.0;
    for (int d = 1; d <= o->count; d++) {
        if (!(s = line_next(s, &line, &len))) { std::snprintf(err, errn, "the scale ends after %d of %d degrees", d - 1, o->count); return false; }
        if (!scl_degree(line, len, &o->ratio[d])) { std::snprintf(err, errn, "degree %d is neither cents nor a ratio: '%.*s'", d, (int)(len < 40 ? len : 40), line); return false; }
    }
    return true;
}
static bool kbm_parse(const char* text, const Scl& sc, Kbm* o, char* err, size_t errn) {
    const char* line; size_t len; const char* s = text; long v[7];
    for (int i = 0; i < 7; i++) {
        if (!(s = line_next(s, &line, &len))) { std::snprintf(err, errn, "the mapping ends in its header (%d of 7 lines)", i); return false; }
        if (i == 5) { o->ref_hz = std::strtod(line, nullptr); if (!(o->ref_hz > 0.0)) { std::snprintf(err, errn, "the reference frequency is not positive"); return false; } }
        else v[i] = std::strtol(line, nullptr, 10);
    }
    o->size = (int)v[0]; o->first = (int)v[1]; o->last = (int)v[2]; o->middle = (int)v[3]; o->ref = (int)v[4]; o->octave = (int)v[6];
    if (o->size < 1 || o->size > 128) { std::snprintf(err, errn, "the mapping's size %d is outside 1..128", o->size); return false; }
    if (o->middle < 0 || o->middle > 127 || o->ref < 0 || o->ref > 127) { std::snprintf(err, errn, "the middle or the reference note is outside 0..127"); return false; }
    if (o->octave < 1 || o->octave > sc.count) { std::snprintf(err, errn, "the formal octave's degree %d is past the scale's %d", o->octave, sc.count); return false; }
    for (int i = 0; i < o->size; i++) {
        if (!(s = line_next(s, &line, &len))) { std::snprintf(err, errn, "the mapping ends after %d of %d keys", i, o->size); return false; }
        if (len > 0 && (line[0] == 'x' || line[0] == 'X')) { o->map[i] = -1; continue; }
        const long d = std::strtol(line, nullptr, 10);
        if (d < 0 || d > sc.count) { std::snprintf(err, errn, "key %d maps to degree %ld, past the scale's %d", i, d, sc.count); return false; }
        o->map[i] = (int)d;
    }
    return true;
}
static void kbm_default(const Scl& sc, Kbm* o) {
    o->size = sc.count; o->first = 0; o->last = 127; o->middle = 60; o->ref = 69; o->ref_hz = 440.0; o->octave = sc.count;
    for (int i = 0; i < o->size; i++) o->map[i] = i;
}
static double scale_ratio_at(const Scl& sc, long degree) {   // any integer degree: the period's power times the degree within
    long o = degree >= 0 ? degree / sc.count : -((-degree + sc.count - 1) / sc.count);
    const long r = degree - o * sc.count;
    return std::pow(sc.ratio[sc.count], (double)o) * sc.ratio[r];
}
static double note_degree_ratio(const Scl& sc, const Kbm& k, int note) {
    const long i = (long)note - k.middle;
    const long o = i >= 0 ? i / k.size : -((-i + k.size - 1) / k.size);
    const long j = i - o * k.size;
    const int d = k.map[j] < 0 ? (int)j : k.map[j];
    return scale_ratio_at(sc, (long)d + o * (long)k.octave);
}
}   // namespace
bool voxo_set_tuning(voxo_t* v, const char* scl_text, const char* kbm_text) {
    if (!v || !scl_text) return false;
    Scl* sc = new Scl; Kbm* k = new Kbm; char err[160] = { 0 }; bool ok = scl_parse(scl_text, sc, err, sizeof err);
    if (ok) { if (kbm_text && *kbm_text) ok = kbm_parse(kbm_text, *sc, k, err, sizeof err); else kbm_default(*sc, k); }
    if (!ok) {
        if (v->log_cb) { char msg[240]; std::snprintf(msg, sizeof msg, "voxo: the tuning is refused — %s (the tuning stands as it was)", err); v->log_cb(2, msg, v->log_user); }
        delete sc; delete k; return false;
    }
    const uint32_t live = v->tuning_live.load(std::memory_order_acquire), idle = live ^ 1u;
    const double ref = note_degree_ratio(*sc, *k, k->ref);
    for (int n = 0; n < 128; n++) {
        double hz = (n >= k->first && n <= k->last && ref > 0.0) ? k->ref_hz * note_degree_ratio(*sc, *k, n) / ref : (double)note_to_hz((float)n);
        if (!(hz > 0.1 && hz < 1.0e6)) hz = (double)note_to_hz((float)n);
        v->tuning_hz[idle][n] = (float)hz;
    }
    std::memcpy(v->tuning_name, sc->name, sizeof v->tuning_name);
    v->tuning_live.store(idle, std::memory_order_release);
    v->tuning_count.store((uint32_t)sc->count, std::memory_order_release);
    if (v->log_cb) { char msg[240]; std::snprintf(msg, sizeof msg, "voxo: the tuning is '%s' — %d degrees, %s mapping, MIDI %d at %.3f Hz", sc->name, sc->count, kbm_text && *kbm_text ? "the file's" : "the default", k->ref, k->ref_hz); v->log_cb(1, msg, v->log_user); }
    delete sc; delete k;
    return true;
}
static char* read_whole_file(const char* path) {   // the caller frees; nullptr if unreadable
    if (!path || !*path) return nullptr;
    FILE* f = std::fopen(path, "rb"); if (!f) return nullptr;
    std::fseek(f, 0, SEEK_END); const long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
    if (n < 0 || n > (1 << 20)) { std::fclose(f); return nullptr; }
    char* buf = new char[(size_t)n + 1]; const size_t got = std::fread(buf, 1, (size_t)n, f); std::fclose(f); buf[got] = 0; return buf;
}
bool voxo_load_tuning(voxo_t* v, const char* scl_path, const char* kbm_path) {
    if (!v) return false;
    char* scl = read_whole_file(scl_path);
    if (!scl) { if (v->log_cb) { char msg[240]; std::snprintf(msg, sizeof msg, "voxo: the tuning file could not be read: %s", scl_path ? scl_path : "(null)"); v->log_cb(2, msg, v->log_user); } return false; }
    char* kbm = read_whole_file(kbm_path);
    const bool ok = voxo_set_tuning(v, scl, kbm);
    delete[] scl; delete[] kbm;
    return ok;
}
void voxo_clear_tuning(voxo_t* v) { if (!v) return; v->tuning_count.store(0u, std::memory_order_release); v->tuning_name[0] = 0; }
uint32_t voxo_tuning_count(const voxo_t* v) { return v ? v->tuning_count.load(std::memory_order_acquire) : 0u; }
float voxo_tuning_hz(const voxo_t* v, uint32_t note) {
    if (!v) return 0.0f; note &= 127u;
    return v->tuning_count.load(std::memory_order_acquire) ? v->tuning_hz[v->tuning_live.load(std::memory_order_acquire)][note] : note_to_hz((float)note);
}
const char* voxo_tuning_name(const voxo_t* v) { return v && v->tuning_count.load(std::memory_order_acquire) ? v->tuning_name : ""; }
void voxo_suzu_table(const voxo_suzu_params_t* p, uint32_t* n, float ratio[VOXO_MODAL_MAX], float t60[VOXO_MODAL_MAX], float weight[VOXO_MODAL_MAX]) {
    if (!p) return;
    suzu::ModalTable t; suzu::modal_table(&t, (int)p->modal_preset, (int)p->modes, p->decay_s, p->decay_bright, p->stiffness, p->pluck, p->bow_position, p->modal_ratio, p->modal_t60, p->modal_weight);
    if (n) *n = (uint32_t)t.n;
    for (int k = 0; k < VOXO_MODAL_MAX; k++) { if (ratio) ratio[k] = k < t.n ? t.ratio[k] : 0.0f; if (t60) t60[k] = k < t.n ? t.t60[k] : 0.0f; if (weight) weight[k] = k < t.n ? t.kick[k] : 0.0f; }
}

void voxo_set_trace(voxo_t* v, uint32_t kinds_mask) {
    if (v) v->trace_mask.store(kinds_mask, std::memory_order_relaxed);
}
void voxo_set_trace_decimation(voxo_t* v, uint32_t sub_steps) {
    if (v) v->trace_decim.store(sub_steps < 1u ? 1u : (sub_steps > 64u ? 64u : sub_steps), std::memory_order_relaxed);
}
// Step 59c: the ring's last points at full density for one sounding voice (0 = the oldest by serial), oldest first.
uint32_t voxo_trace_recent(voxo_t* v, uint32_t voice, float* out_xyuw, uint32_t max_points) {
    if (!v || !out_xyuw || max_points == 0) return 0;
    const uint32_t mask = v->trace_mask.load(std::memory_order_relaxed);
    const uint32_t kind = v->suzu_params[v->suzu_live.load(std::memory_order_acquire)].voice_kind;
    if (((mask >> kind) & 1u) == 0) return 0;
    uint32_t order[MAX_VOICES_CAP]; uint32_t m = 0;
    for (uint32_t i = 0; i < v->max_voices; i++) if (v->voices[i].active) order[m++] = i;
    for (uint32_t i = 1; i < m; i++) { const uint32_t x = order[i]; uint32_t j = i; while (j > 0 && v->voices[order[j - 1]].serial > v->voices[x].serial) { order[j] = order[j - 1]; j--; } order[j] = x; }
    if (voice >= m) return 0;
    const SuzuVoice& sz = v->voices[order[voice]].suzu;
    const uint32_t w = sz.twrite.load(std::memory_order_acquire);
    uint32_t n = max_points > TRACE_RING - 1 ? TRACE_RING - 1 : max_points;
    if (n > w) n = w;
    for (uint32_t k = 0; k < n; k++) {
        const uint32_t j = (w - n + k) % TRACE_RING;
        out_xyuw[4 * k] = sz.tx[j]; out_xyuw[4 * k + 1] = sz.ty[j]; out_xyuw[4 * k + 2] = sz.tu[j]; out_xyuw[4 * k + 3] = sz.tw[j];
    }
    return n;
}
// Step 75 (KESSHO §2.5): the strobe's clock, its frequency, and the poll — the returns since the last poll per sounding
// traced voice (the newest `max_points` when more fell), raw in the cell's units; the host scales and places them.
void voxo_set_strobe(voxo_t* v, uint32_t clock) {
    if (v) v->strobe_mode.store(clock > 2u ? 2u : clock, std::memory_order_relaxed);
}
float voxo_strobe_hz(const voxo_t* v) { return v ? v->strobe_hz_out.load(std::memory_order_relaxed) : 0.0f; }
uint32_t voxo_strobe_poll(voxo_t* v, voxo_strobe_t* out, uint32_t max_voices, uint32_t max_points) {
    if (!v || !out || max_voices == 0) return 0;
    if (v->strobe_mode.load(std::memory_order_relaxed) == 0) return 0;
    const uint32_t mask = v->trace_mask.load(std::memory_order_relaxed);
    if (mask == 0) return 0;
    const uint32_t kind = v->suzu_params[v->suzu_live.load(std::memory_order_acquire)].voice_kind;
    if (((mask >> kind) & 1u) == 0) return 0;
    const uint32_t K = max_points < 1u ? 1u : (max_points > VOXO_STROBE_POINTS_MAX ? VOXO_STROBE_POINTS_MAX : max_points);
    uint32_t count = 0;
    for (uint32_t i = 0; i < v->max_voices && count < max_voices; i++) {
        const Voice& vc = v->voices[i];
        if (!vc.active) continue;
        const SuzuVoice& sz = vc.suzu;
        const uint32_t w = sz.swrite.load(std::memory_order_acquire);
        const uint32_t total = sz.sreturns;
        if (v->strobe_serial[i] != vc.serial) { v->strobe_serial[i] = vc.serial; const uint32_t back = total < STROBE_RING - 1 ? total : STROBE_RING - 1; v->strobe_read[i] = w > back ? w - back : 0; }   // a new voice: from its first return
        uint32_t rd = v->strobe_read[i];
        if (w - rd > STROBE_RING - 1) rd = w - (STROBE_RING - 1);
        uint32_t n = w - rd;
        if (n > K) { rd = w - K; n = K; }
        voxo_strobe_t& t = out[count++];
        t.channel = vc.channel; t.note = vc.note; t.voice_kind = (uint8_t)kind; t.held = vc.held ? 1 : 0; t.serial = vc.serial;
        t.count = n; t.total = total; t.hz = sz.strobe_hz;
        for (uint32_t k = 0; k < n; k++) { const uint32_t j = (rd + k) % STROBE_RING; t.x[k] = sz.sx[j]; t.y[k] = sz.sy[j]; t.u[k] = sz.su[j]; t.w[k] = sz.sw[j]; }
        v->strobe_read[i] = w;
    }
    return count;
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

// Step 59b (SYNTH §6): the inspection — a copy of each sounding Suzu voice's state (voxo.h has the arrays by kind).
static void inspect_bore(const suzu::Bore& b, voxo_inspect_t& t) {
    const int n = b.n + 1 > VOXO_INSPECT_MAX ? VOXO_INSPECT_MAX : b.n + 1;
    t.n = (uint32_t)n;
    for (int i = 0; i < n; i++) { t.a[i] = b.p[i]; t.s[i] = b.sp[i]; }
    for (int i = 0; i + 1 < n; i++) t.b[i] = b.u[i];
    t.k[0] = b.lam;
}
uint32_t voxo_suzu_inspect(voxo_t* v, voxo_inspect_t* out, uint32_t max_voices) {
    if (!v || !out || max_voices == 0) return 0;
    const voxo_suzu_params_t& sp = v->suzu_params[v->suzu_live.load(std::memory_order_acquire)];
    const uint32_t kind = sp.voice_kind;
    const float rate2 = 2.0f * (float)v->device_rate.load(std::memory_order_relaxed);
    // the oldest first: the active voices by serial
    uint32_t order[MAX_VOICES_CAP]; uint32_t m = 0;
    for (uint32_t i = 0; i < v->max_voices; i++) if (v->voices[i].active) order[m++] = i;
    for (uint32_t i = 1; i < m; i++) { const uint32_t x = order[i]; uint32_t j = i; while (j > 0 && v->voices[order[j - 1]].serial > v->voices[x].serial) { order[j] = order[j - 1]; j--; } order[j] = x; }
    uint32_t count = 0;
    for (uint32_t o = 0; o < m && count < max_voices; o++) {
        const Voice& vc = v->voices[order[o]];
        const SuzuVoice& sz = vc.suzu;
        voxo_inspect_t& t = out[count++];
        std::memset(&t, 0, sizeof t);
        t.channel = vc.channel; t.note = vc.note; t.voice_kind = (uint8_t)kind; t.held = vc.held ? 1 : 0; t.serial = vc.serial;
        t.freq = vc.freq; t.env = sz.out_env; t.rate2 = rate2;
        switch (kind) {
            case 0: t.n = 1; t.a[0] = sz.modes[0].x; t.b[0] = sz.modes[0].y; t.k[0] = sz.modes[0].eps; t.k[1] = sz.modes[0].amp; break;
            case 1: {
                const uint32_t n = sz.n > (uint32_t)suzu::MODES_MAX ? (uint32_t)suzu::MODES_MAX : sz.n;
                t.n = n;
                for (uint32_t k = 0; k < n; k++) { t.a[k] = sz.modes[k].x; t.b[k] = sz.modes[k].y; t.s[k] = sz.table.ratio[k]; t.c[k] = sz.bow_t[k]; }
                t.k[0] = sz.kappa; t.k[1] = sz.mix; t.k[2] = sz.bow_f; t.k[3] = sz.bow_vb; t.k[4] = (float)sz.bow_work; t.k[5] = sz.bow_stuck ? 1.0f : 0.0f; t.k[6] = (float)sz.bow_gain;   // step 80: the stick-slip bow's reading
            } break;
            case 2: {
                const int n = sz.string.m + 2 > VOXO_INSPECT_MAX ? VOXO_INSPECT_MAX : sz.string.m + 2;
                t.n = (uint32_t)n;
                for (int i = 0; i < n; i++) { t.a[i] = sz.string.u[i]; t.b[i] = sz.string.v[i]; }
                t.k[0] = sz.string.s2; t.k[1] = sz.bow_f; t.k[2] = sz.bow_vb; t.k[3] = (float)sz.bow_work; t.k[4] = (float)sz.bow_node; t.k[5] = sz.bow_stuck ? 1.0f : 0.0f; t.k[6] = sz.string.energy(); t.k[7] = (float)sz.bow_gain;   // step 80
            } break;
            case 11: {                                                              // step 84: the gyroscope's state, its field and the doublet it sounds
                t.n = 2; t.a[0] = (float)sz.gyro.x1; t.a[1] = (float)sz.gyro.x2; t.b[0] = (float)sz.gyro.y1; t.b[1] = (float)sz.gyro.y2;
                double ap = 0.0, am = 0.0; suzu::Gyro::angles(sz.gyro.eps, sz.gyro.theta, &ap, &am);
                const double rate2 = (double)sz.boris_rate2, w0 = 2.0 * M_PI * (double)vc.freq, wb = 2.0 * M_PI * (double)sz.boris_hz, root = sqrt(w0 * w0 + 0.25 * wb * wb);
                t.k[0] = (float)sz.gyro.eps; t.k[1] = (float)sz.gyro.theta; t.k[2] = (float)sz.gyro.energy(); t.k[3] = sz.boris_hz;
                t.k[4] = (float)(ap * rate2 / (2.0 * M_PI)); t.k[5] = (float)(am * rate2 / (2.0 * M_PI));
                t.k[6] = (float)((root + 0.5 * wb) / (2.0 * M_PI)); t.k[7] = (float)((root - 0.5 * wb) / (2.0 * M_PI)); t.k[8] = sp.boris_prewarp != 0u ? 1.0f : 0.0f;
            } break;
            case 10: {                                                              // step 83: the ring's state and its invariants
                const int n = sz.ring.n > VOXO_INSPECT_MAX ? VOXO_INSPECT_MAX : sz.ring.n;
                t.n = (uint32_t)n;
                for (int i = 0; i < n; i++) { t.a[i] = (float)sz.ring.q[i]; t.b[i] = (float)sz.ring.p[i]; }
                const double mm = (double)(sz.toda_m > 0 ? sz.toda_m : 2) * 0.5;   // H in the ×4 step's units whatever the inner steps (per-step units scale by the step's square)
                t.k[0] = sz.toda_k; t.k[1] = (float)sz.ring.b; t.k[2] = (float)(sz.ring.energy() * mm * mm); t.k[3] = sz.ring.momentum(); t.k[4] = sz.ring.strain_peak(); t.k[5] = (float)sz.ring.kcap; t.k[6] = (float)sz.toda_m; t.k[7] = sz.toda_b_want; t.k[8] = sz.toda_b_cap;
            } break;
            case 3: {
                const suzu::HybridString& h = sz.hybrid;
                const int len = h.n < 1 ? 1 : h.n, n = len > VOXO_INSPECT_MAX - 1 ? VOXO_INSPECT_MAX - 1 : len;
                t.n = (uint32_t)n;
                for (int i = 0; i < n; i++) {                                           // oldest to newest around the loop
                    const int back = len - (int)((float)i * (float)len / (float)n);
                    t.a[i] = h.d[(h.w - back + 2 * suzu::DELAY_MAX) % suzu::DELAY_MAX];
                }
                for (int b2 = 0; b2 < h.nb && b2 < 3; b2++) { t.k[b2] = h.bridge[b2].x; t.k[3 + b2] = h.bridge[b2].y; }
                t.k[6] = (float)h.n; t.k[7] = h.phase_j; t.k[8] = sz.bow_f; t.k[9] = sz.bow_vb; t.k[10] = (float)sz.bow_work; t.k[11] = sz.bow_stuck ? 1.0f : 0.0f; t.k[12] = h.energy(); t.k[13] = (float)sz.bow_gain;   // step 80: the bow's reading
            } break;
            case 4: t.n = 1; t.a[0] = sz.duffing.c.x; t.b[0] = sz.duffing.c.y; t.k[0] = sz.duffing.beta; t.k[1] = sz.duffing.phase; break;
            case 5: t.n = 1; t.a[0] = sz.rotor.c.x; t.b[0] = sz.rotor.c.y; t.k[0] = sz.rotor.p; t.k[1] = sz.k_smooth; break;
            case 6: {
                inspect_bore(sz.bore, t);
                const float tau = sz.ins_tau;
                for (int i = 0; i < 64; i++) t.c[i] = sz.jet.read(1.0f + (float)i / 63.0f * (tau > 1.0f ? tau - 1.0f : 0.0f));   // the jet from the flue to the labium
                t.k[1] = sz.ins_pm; t.k[2] = sz.ins_u0; t.k[3] = tau; t.k[4] = sz.ins_gain; t.k[5] = sp.jet_offset;
                t.k[6] = sz.jet.q_prev; t.k[7] = sz.jet.xi; t.k[8] = sz.breath_s; t.k[9] = -sz.ins_gain * sz.jet.read(tau); t.k[10] = sz.ins_area;
                t.k[11] = (float)suzu_holes_bits(sz.bore); t.k[12] = (float)sz.hole_j; t.k[13] = (float)sz.hole_reg;   // step 81: the holes open, the semitone and the register (−1: the v1 cut)
            } break;
            case 7: case 8:
                inspect_bore(sz.bore, t);
                t.k[1] = sz.breath_s; t.k[2] = sz.valve.opening(); t.k[3] = sz.valve.y; t.k[4] = sz.valve.v; t.k[5] = sz.valve.q;
                t.k[6] = sz.valve.h0; t.k[7] = sz.wind_dyn;
                t.k[8] = (float)sz.work; t.k[9] = sz.bore.energy() + sz.bore.holes_energy() + sz.valve.energy();   // 59c: the ledger — the mouth's work, the energy held (step 81: the holes' air too)
                t.k[10] = (float)sz.bore.shock_loss;                                        // step 81: the shock limiter's loss since the strike (a known positive term of the ledger)
                t.k[11] = (float)suzu_holes_bits(sz.bore); t.k[12] = (float)sz.hole_j; t.k[13] = (float)sz.hole_reg;   // step 81: the sax's holes
                break;
            case 9: {   // step 79: the membrane's interior, row-major, and its numbers
                const int nx = sz.drum.nx, ny = sz.drum.ny; const int n = nx * ny > VOXO_INSPECT_MAX ? VOXO_INSPECT_MAX : nx * ny;
                t.n = (uint32_t)n;
                for (int i = 0; i < n; i++) { const int node = (1 + i / nx) * suzu::Membrane<suzu::MEMBRANE_MAX>::S + 1 + i % nx; t.a[i] = sz.drum.u[node]; t.b[i] = sz.drum.p[node]; }
                t.k[0] = (float)nx; t.k[1] = (float)ny; t.k[2] = sz.drum.c2; t.k[3] = sz.drum.k2; t.k[4] = sz.drum_cfl; t.k[5] = sz.drum.E; t.k[6] = sz.drum.alpha;
                t.k[7] = (float)sz.drum.energy(); t.k[8] = sz.drum.g0; t.k[9] = sz.drum.g2; t.k[10] = sz.drum_cap; t.k[11] = sz.drum_emax;
            } break;
            default: t.n = 0; break;
        }
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
    out->breaker_trips = v->breaker_trips.load(std::memory_order_relaxed);   // step 75
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
