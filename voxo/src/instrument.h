// instrument.h — the COMPILED instrument (Phase 7 step 51, SOUND §2–§3;
// DECISIONS_6 #16–#18): what the callback plays. Built on the shell's thread
// from a parsed voxo_ds::Instrument (which it then owns, for the decoded
// samples) or from one raw sample (the step-49 API), published through the
// swap protocol, read by the voices without a lock or an allocation — flat
// arrays, plain numbers, pointers into the decoded frames. The per-group
// RUNTIME state (round-robin counters, the live parameters the CC bindings
// move) is allocated here too and becomes the callback's after the swap.
#pragma once

#include <cstdint>
#include <vector>

namespace voxo_ds { struct Instrument; }

namespace voxo_inst {

constexpr uint32_t PAD_BEFORE = 2, PAD_AFTER = 4;   // frames of zeros around every sample: the 4-point read never bounds-checks

enum class SeqMode : uint8_t { Always, RoundRobin, Random };
enum class Target : uint8_t { None, AmpVolume, FilterCutoff, FilterResonance, EnvAttack, EnvDecay, EnvSustain, EnvRelease };

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
};

struct Group {
    float   attack, decay, sustain, release;   // seconds, seconds, 0..1, seconds (the preset's defaults)
    float   amp_vel_track;
    SeqMode seq_mode;
    uint16_t seq_length;       // 0 = none
    bool    has_filter;        // a low-pass to play through (the group's, else the instrument's)
    float   cutoff, resonance; // Hz, 0..1 (the preset's defaults)
    // The MPE sources of this group (the preset's <mpePressure> / <mpeTimbre>
    // bindings when it has them, else the defaults of DECISIONS_6 #17).
    Curve   pressure, timbre, swirl;
    bool    pressure_is_default, timbre_is_default;
    float   pressure_rise_ms, pressure_fall_ms, timbre_rise_ms, timbre_fall_ms;
    Curve   velocity_to_cutoff;   // a <velocity> binding on FX_FILTER_FREQUENCY, target None when absent
};

// Runtime state per group: the callback's after the swap.
struct GroupState {
    uint32_t rr_counter;       // the next round-robin position (1-based)
    uint32_t rng;
    float    gain;             // AMP_VOLUME live (1 = the preset's)
    float    cutoff, resonance;
    float    attack, decay, sustain, release;
};

struct CcBinding { uint8_t cc; Curve curve; };

struct Instrument {
    std::vector<Zone>       zones;
    std::vector<Group>      groups;
    std::vector<GroupState> state;         // the callback's (mutable) after the swap
    std::vector<CcBinding>  cc_bindings;   // global CCs to group/instrument parameters
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
