// bus.h — Voxo's bus (Phase 7 step 52, SOUND §3; DECISIONS_6 #19): a
// Freeverb-class reverb and a feedback delay, post-sum, never per voice,
// reading the preset's Decent Sampler effect parameters. Real-time safe by
// construction: every buffer is allocated once at voxo_create for the
// largest rate, the parameters are plain numbers the callback owns.
#pragma once

#include <cstdint>
#include <vector>

namespace voxo_bus {

struct Params {
    bool  reverb_on = false, delay_on = false;
    float room_size = 0.7f, damping = 0.3f, reverb_wet = 0.3f;   // DS reverb: roomSize, damping, wetLevel (0..1)
    float delay_time = 0.5f, feedback = 0.3f, stereo_offset = 0.0f, delay_wet = 0.2f;   // DS delay: seconds, 0..1, seconds, 0..1
    // Step 88 (TO_PROJECT_SPEC §12.8; DECISIONS_14 #2): the bus chorus and the phaser, the format's attributes.
    bool  chorus_on = false, phaser_on = false;
    float chorus_mix = 0.5f, chorus_depth = 0.2f, chorus_rate = 0.2f;                        // DS chorus: mix, modDepth (0..1), modRate (Hz, 0..10)
    float phaser_mix = 0.5f, phaser_depth = 0.2f, phaser_rate = 0.2f, phaser_center = 400.0f, phaser_feedback = 0.7f;   // DS phaser: + centerFrequency (Hz), feedback (−1..1)
};

// Step 88: THE CHORUS — a stereo modulated delay, the format's mix / modDepth / modRate. Two delay lines at a centre
// of 7 ms swung by ±4 ms × depth; the LFO a magic-circle cell (the leapfrog rotation of suzu.h: exact, drift-free,
// its (x, y) the quadrature pair — the right channel 90° from the left for free); the fractional read the §12.2
// 4-point Hermite. The buffers are FIXED (16 ms at 96 kHz), so an instance lives inside every Voice — the format's
// group-level chorus is per note — and one on the bus, with no allocation anywhere. The arithmetic is
// contraction-free (bus.cpp's pragma): the same bits in every native build at a fixed seed (the LFO starts at phase 0).
struct Chorus {
    static constexpr uint32_t LEN = 1536;
    static constexpr float CENTRE_MS = 7.0f, SWING_MS = 4.0f;   // [ITERATE] by ear against the author's chorus library
    float buf[2][LEN];
    uint32_t idx = 0;
    float lx = 0.0f, ly = 1.0f;                                 // the LFO cell at phase 0: the fixed seed
    float mix = 0.0f, depth = 0.0f, rate_hz = 0.0f;
    void clear();                                               // the lines zero, the LFO at phase 0
    void set(float mix_, float depth_, float rate_hz_);
    void process(float* lr, uint32_t frames, uint32_t rate);    // in place: (1 − mix)·dry + mix·wet; mix 0 leaves the samples untouched
};

// THE PHASER — six first-order all-pass stages per channel swept by the same kind of LFO (±2 octaves about
// centerFrequency at depth 1, the coefficient refreshed every sixteen samples), the feedback from the chain's last
// output into its input, the format's mix / modDepth / modRate / centerFrequency / feedback. The bus's alone: the
// format admits no group-level phaser.
struct Phaser {
    static constexpr int STAGES = 6;
    float x1[2][STAGES], y1[2][STAGES];
    float last[2] = {0.0f, 0.0f};
    float lx = 0.0f, ly = 1.0f;
    float mix = 0.0f, depth = 0.0f, rate_hz = 0.0f, center_hz = 400.0f, feedback = 0.0f;
    void clear();
    void set(float mix_, float depth_, float rate_hz_, float center_hz_, float feedback_);
    void process(float* lr, uint32_t frames, uint32_t rate);
};

// Freeverb (Jezar's public-domain design): eight combs and four allpasses per
// channel, the right channel's lengths offset by 23 samples; the lengths
// scale with the rate from the 44.1 kHz originals.
struct Reverb {
    static constexpr int COMBS = 8, ALLPASSES = 4;
    struct Comb { std::vector<float> buf; uint32_t idx; float filterstore; };
    struct Allpass { std::vector<float> buf; uint32_t idx; };
    Comb comb[2][COMBS];
    Allpass allpass[2][ALLPASSES];
    float feedback = 0.84f, damp1 = 0.2f, damp2 = 0.8f, wet = 0.3f;
    void allocate(uint32_t rate);        // shell thread, once
    void clear();                        // callback: the tail dropped
    void set(const Params& p);           // callback, at block start
    void process(float* lr, uint32_t frames);   // in place, dry + wet
};

struct Delay {
    std::vector<float> buf[2];
    uint32_t idx = 0, len = 1;
    uint32_t time_l = 1, time_r = 1;     // in samples
    float feedback = 0.3f, wet = 0.2f;
    void allocate(uint32_t rate);        // 2 s at the rate
    void clear();
    void set(const Params& p, uint32_t rate);
    void process(float* lr, uint32_t frames);
};

} // namespace voxo_bus
