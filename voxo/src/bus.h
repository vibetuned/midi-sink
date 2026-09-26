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
