// instrument.h — the COMPILED instrument (Phase 7 step 51, SOUND §2–§3;
// DECISIONS_6 #16–#18): what the callback plays. Built on the shell's thread
// from a parsed voxo_ds::Instrument (which it then owns, for the decoded
// samples) or from one raw sample (the step-49 API), published through the
// swap protocol, read by the voices without a lock or an allocation — flat
// arrays, plain numbers, pointers into the decoded frames. The per-group
// RUNTIME state (round-robin counters, the live parameters the CC bindings
// move) is allocated here too and becomes the callback's after the swap.
#pragma once

#include "bus.h"

#include <cstdint>
#include <vector>

namespace voxo_ds { struct Instrument; }

namespace voxo_inst {

constexpr uint32_t PAD_BEFORE = 2, PAD_AFTER = 4;   // frames of zeros around every sample: the 4-point read never bounds-checks

enum class SeqMode : uint8_t { Always, RoundRobin, Random };
enum class Target : uint8_t { None, AmpVolume, FilterCutoff, FilterResonance, EnvAttack, EnvDecay, EnvSustain, EnvRelease,
                              ChorusMix, ChorusDepth, ChorusRate,                 // step 88: a group's chorus, or the bus's when the curve's group is BUS_GROUP
                              ShaperDrive, ShaperBoost, ShaperLevel,              // step 89: a group's wave shaper, or the bus's when the curve's group is BUS_GROUP
                              AmpPan, GroupTuning, GlideTime,                     // step 90: PAN (−100..100), GROUP_TUNING / GLOBAL_TUNING (semitones), GLIDE_TIME (s)
                              ReverbWet, ReverbRoom, ReverbDamping, DelayWet, DelayTime, DelayFeedback,   // the bus (every target from ReverbWet on)
                              PhaserMix, PhaserDepth, PhaserRate, PhaserCenter, PhaserFeedback,            // step 88: the bus phaser
                              EqFrequency, EqQ, EqGain };                          // step 89: a bus EQ stage (the curve's group is the stage index)
constexpr int32_t BUS_GROUP = -2;   // step 88: a chorus curve aimed at the bus (an instrument-level binding); -1 = every group
inline bool target_is_chorus(Target t) { return t == Target::ChorusMix || t == Target::ChorusDepth || t == Target::ChorusRate; }
inline bool target_is_shaper(Target t) { return t == Target::ShaperDrive || t == Target::ShaperBoost || t == Target::ShaperLevel; }
inline bool target_is_eq(Target t) { return t == Target::EqFrequency || t == Target::EqQ || t == Target::EqGain; }
// Step 89: the voice filter's mode — the SVF's outputs (low, high, band, notch) and the one-pole lowpass.
enum : uint8_t { FILTER_LOW = 0, FILTER_LOW_1POLE = 1, FILTER_HIGH = 2, FILTER_BAND = 3, FILTER_NOTCH = 4 };
// Step 89: the envelope's modBehavior on its target — the format's set | add | multiply | modulate.
enum : uint8_t { MOD_SET = 0, MOD_ADD = 1, MOD_MULTIPLY = 2, MOD_MODULATE = 3 };
// Step 90: THE MODULATOR MATRIX (TO_PROJECT_SPEC §12.10; DECISIONS_14 #4) — the preset's <lfo>s and <envelope>s, each
// with its bindings compiled to targets. A modulator is a function of time (an LFO: of the instrument's clock, or of
// the voice's own since its note-on under scope="voice"; an envelope: of the voice's note-on and release — per voice
// whatever its scope), read once a block at the block's start and end; a target folds that value into a parameter
// the voice already ramps (the gain, the pitch, the cutoff, the Q, the pan, a group effect's live value) or, for the
// bus's parameters, into the block's live copy of the bus. The matrix is fixed-size: eight modulators of eight
// targets, nothing allocated in the callback.
constexpr uint32_t MOD_MAX = 8, MOD_TARGETS_MAX = 8;
enum : uint8_t { MOD_KIND_LFO = 0, MOD_KIND_ENV = 1 };
enum : uint8_t { LFO_SINE = 0, LFO_TRIANGLE = 1, LFO_SQUARE = 2, LFO_SAW = 3 };
enum : uint8_t { GLIDE_OFF = 0, GLIDE_LEGATO = 1, GLIDE_ALWAYS = 2 };

// A binding compiled to a curve: the source's 0..1 through the translation,
// sampled into 33 points (linear between) — a table lookup in the callback.
struct Curve {
    Target  target = Target::None;
    int32_t group = -1;        // -1 = every group (instrument level)
    float   points[33];        // out(in) for in = k / 32
    float   at(float in) const {
        in = in < 0.0f ? 0.0f : in > 1.0f ? 1.0f : in;
        const float x = in * 32.0f;
        const int i = (int)x;
        if (i >= 32) return points[32];
        return points[i] + (points[i + 1] - points[i]) * (x - (float)i);
    }
};

struct Zone {
    const float* data;         // frame 0 of the (padded) decoded sample
    uint32_t frames, channels, rate;
    float    root_hz;          // of the effective root (rootNote - tuning)
    uint8_t  lo_note, hi_note, lo_vel, hi_vel;
    float    fade_lo, fade_hi; // velocity crossfade widths at each edge (0 = hard)
    float    gain;             // the zone's volume × the group's × the tag's, linear
    float    pan;              // -1..1
    uint32_t start, end;       // frames; end <= frames
    bool     loop;
    uint32_t loop_start, loop_end, loop_xfade;
    uint16_t seq_position;
    uint16_t group;
    bool     release_trigger;
    // Step 87 (TO_PROJECT_SPEC §12.7; DECISIONS_14 #1): the pedal mechanics.
    uint8_t  on_cc, on_lo, on_hi;       // onLoCCN/onHiCCN: fires when CC on_cc enters [on_lo, on_hi]; on_cc 255 = a note zone
    uint8_t  cond_cc, cond_lo, cond_hi; // loCCN/hiCCN: plays on a note-on only with CC cond_cc's last value inside; 255 = none
    float    key_track;                 // pitchKeyTrack: the exponent on the note's ratio (1 the note, 0 the root)
    uint64_t tag_mask;                  // the zone's tags, one bit each (the instrument's dictionary, 64 at most)
    uint64_t silenced_by;               // the tags whose start silences this zone's layers
    float    silence_s;                 // how it stops: > 0 a fade of that many seconds (fast = 5 ms, silencingDecay as given),
                                        //   < 0 the group's release phase (normal)
    // Step 90: the legato attributes — trigger first / legato (by the other held notes), previousNotes (a bit per
    // note; all zero = any), legatoInterval (the semitones from the previous note; -1000 = any).
    bool     trigger_first, trigger_legato;
    uint8_t  prev_mask[16];
    int16_t  legato_interval;
};

struct Group {
    float   attack, decay, sustain, release;   // seconds, seconds, 0..1, seconds (the preset's defaults)
    float   sustain_decay;     // step 87: seconds a released note decays under the full pedal; 0 = held (today's pedal)
    float   amp_vel_track;
    SeqMode seq_mode;
    uint16_t seq_length;       // 0 = none
    bool    has_filter;        // a filter to play through (the group's first, else the instrument's first; a low-pass at 22 kHz when none)
    float   cutoff, resonance; // Hz, and Q (the format's `resonance`, 0.1..5; step 89 — 0..1 "amount" before)
    uint8_t filter_mode;       // step 89: FILTER_* — which output of the voice's SVF (the one-pole for lowpass_1pl)
    bool    shaper;            // step 89: the group's wave shaper (per voice, through the scratch)
    float   shaper_drive, shaper_boost, shaper_level;
    // Step 90: the glide (glideTime seconds, glideMode) and the polyphony (0 = no limit; 1 = monophonic).
    float    glide_time;
    uint8_t  glide_mode;       // GLIDE_*
    uint16_t polyphony;
    // The MPE sources of this group (the preset's <mpePressure> / <mpeTimbre>
    // bindings when it has them, else the defaults of DECISIONS_6 #17).
    Curve   pressure, timbre, swirl;
    bool    pressure_is_default, timbre_is_default;
    float   pressure_rise_ms, pressure_fall_ms, timbre_rise_ms, timbre_fall_ms;
    Curve   velocity_to_cutoff;   // a <velocity> binding on FX_FILTER_FREQUENCY, target None when absent
    bool    chorus;            // step 88: the group carries a chorus — per voice, the format's group-level effect
    float   chorus_mix, chorus_depth, chorus_rate;   // its preset values (mix 0..1, depth 0..1, rate Hz)
};

// Runtime state per group: the callback's after the swap.
struct GroupState {
    uint32_t rr_counter;       // the next round-robin position (1-based)
    uint32_t rng;
    float    gain;             // AMP_VOLUME live (1 = the preset's)
    float    cutoff, resonance;
    float    attack, decay, sustain, release;
    float    chorus_mix, chorus_depth, chorus_rate;   // step 88: the group chorus's live values (FX_MIX / FX_MOD_DEPTH / FX_MOD_RATE bindings)
    float    shaper_drive, shaper_boost, shaper_level;   // step 89: the group shaper's live values (FX_DRIVE / FX_DRIVE_BOOST / FX_OUTPUT_LEVEL)
    float    pan;              // step 90: PAN live, −1..1 added to the zones' (the binding's −100..100 over 100)
    float    tuning;           // step 90: GROUP_TUNING live, semitones
    float    glide_time;       // step 90: GLIDE_TIME live, seconds
    float    last_hz;          // step 90: the group's last note-on frequency (glideMode always glides from it)
};

// Step 90: one target of a modulator — the binding's curve of (value × modAmount), its behaviour and the group it
// reaches (-1 every group, BUS_GROUP the bus, an EQ stage index for the Eq* targets).
struct ModTarget {
    Target  target;
    int32_t group;
    uint8_t behavior;          // MOD_*
    float   amount;            // the element's modAmount × the binding's (the raw 0..1 scaled before the curve)
    float   neutral;           // the curve at the modulator's neutral input (the LFO's centre, the envelope's 0): modulate's zero
    Curve   curve;
};
struct Modulator {
    uint8_t  kind;             // MOD_KIND_*
    uint8_t  shape;            // LFO_*
    bool     voice_scope;      // the LFO restarts at every note-on (scope="voice"); an envelope is per voice regardless
    bool     musical;          // the frequency is a musical-time index (the host's tempo sets the rate)
    float    frequency;        // Hz, or whole notes per cycle under `musical`
    float    delay_time;       // the LFO's delayTime, seconds (the waveform held at its start until then)
    float    amount;           // modAmount, 0..1
    float    env_delay, attack, hold, decay, sustain, release;   // the envelope's stages, seconds (sustain 0..1)
    uint32_t target_count;
    ModTarget targets[MOD_TARGETS_MAX];
};

struct CcBinding { uint8_t cc; Curve curve; };

struct Instrument {
    std::vector<Zone>       zones;
    std::vector<Group>      groups;
    std::vector<GroupState> state;         // the callback's (mutable) after the swap
    std::vector<CcBinding>  cc_bindings;   // global CCs to group/instrument parameters
    voxo_bus::Params        bus;           // the preset's reverb and delay (step 52); the callback's live copy after the swap
    uint8_t                 note_mask[16] = {};   // the attack zones' notes, bit n of [n / 8] (step 53)
    std::vector<uint32_t>   cc_zones;      // step 87: the indices of the CC-triggered zones (onLoCCN) — the pedal noises
    uint8_t                 cc_trigger_mask[16] = {};   // step 87: the CC numbers those zones watch, bit n of [n / 8]
    bool                    choke = false; // step 87: any zone carries silencedByTags (the dispatch runs the silencing pass)
    bool                    group_chorus = false;   // step 88: any group carries a chorus (a fresh voice clears its own)
    Modulator               mods[MOD_MAX];         // step 90: the modulator matrix
    uint32_t                mod_count = 0;
    bool                    mods_voice = false;     // any modulator target on a voice's parameters (the voices read the matrix)
    bool                    mods_bus = false;       // any modulator target on the bus (the block's live copy is modulated)
    std::vector<float>      own_frames;    // the raw-sample form owns its copy here
    voxo_ds::Instrument*    model = nullptr;   // owned: keeps the decoded samples alive
    uint32_t zone_count() const { return (uint32_t)zones.size(); }
    ~Instrument();
};

// One raw sample as a one-zone instrument (the step-49 voxo_set_sample).
Instrument* compile_sample(const float* frames, uint32_t frame_count, uint32_t channels, uint32_t sample_rate, float root_note);
// The parsed model (ownership passes to the result; the model's samples are
// re-laid with padding on the way).
Instrument* compile(voxo_ds::Instrument* model);

} // namespace voxo_inst
