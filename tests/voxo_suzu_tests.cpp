// voxo_suzu_tests.cpp — Suzu's headless gates (Phase 8 step 56, SYNTH §5):
//   1. the DRIFT test — an undriven, undamped cell at A = 0.5 for 10 minutes:
//      peak amplitude drift < 0.1 dB, frequency drift < 0.5 cent; the naive
//      simultaneous update proven RED beside it (it spirals);
//   2. the GLIDE test — a scripted ±48-semitone sweep over 2 s through the ABI:
//      amplitude ripple < 0.5 dB with the orbit re-based on retune; the plain
//      recurrence's ripple printed beside it, not gated (the justification);
//   3. TUNING — the oscillator and the SVF's resonant peak within 2 cents of
//      the note across MIDI 21–108 at 44.1 k and 48 k;
//   4. the DENORMAL gate — 64 voices released together, the render time per
//      second of tail flat to the end under FTZ (and the same without, printed:
//      the cliff is the x86 boxes' to show, arm64 has none);
//   5. CALLBACK HEADROOM — sixteen Suzu voices for a second, the block's render
//      time against its period, recorded.
// voxo_render is called as the callback would; no device.
#include "voxo.h"
#include "suzu.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static int g_fail = 0;
#define CHECK(cond, ...) do { if (cond) { std::printf("ok   "); std::printf(__VA_ARGS__); std::printf("\n"); } \
                              else { g_fail++; std::printf("FAIL "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)
#define NOTE(...) do { std::printf("     "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

static const uint32_t BLOCK = 128;

static voxo_t* make(uint32_t rate, uint32_t max_voices = 0) {
    voxo_config_t c{}; c.sample_rate = rate; c.block_frames = BLOCK; c.max_voices = max_voices;
    voxo_t* v = voxo_create(&c);
    voxo_set_input_mode(v, 1);                 // MPE: the members carry their own bend (±48)
    voxo_set_source(v, VOXO_SOURCE_SUZU);
    return v;
}
static std::vector<float> render(voxo_t* v, uint32_t rate, double seconds) {
    const uint32_t frames = (uint32_t)(seconds * rate);
    std::vector<float> out(2u * frames), left(frames);
    for (uint32_t f = 0; f < frames; f += BLOCK) {
        const uint32_t n = (frames - f) < BLOCK ? (frames - f) : BLOCK;
        voxo_render(v, out.data() + 2u * f, n);
    }
    for (uint32_t f = 0; f < frames; f++) left[f] = out[2u * f];
    return left;
}
static float peak(const float* s, size_t n) { float p = 0; for (size_t i = 0; i < n; i++) p = std::fmax(p, std::fabs(s[i])); return p; }
// Frequency by rising zero crossings with linear interpolation of the crossing instants.
static double frequency(const float* s, size_t n, double rate) {
    double first = -1.0, last = -1.0; long cross = 0;
    for (size_t i = 1; i < n; i++) {
        if (s[i - 1] < 0.0f && s[i] >= 0.0f) {
            const double t = (double)(i - 1) + (double)(-s[i - 1]) / (double)(s[i] - s[i - 1]);
            if (first < 0.0) first = t; last = t; cross++;
        }
    }
    if (cross < 2) return 0.0;
    return (double)(cross - 1) * rate / (last - first);
}
// The same over a DECAYING signal: crossings are counted only while the cycle's
// peak stays above `floor` of the loudest one (a tail that flushes to exact zero
// would otherwise add a spurious crossing and skew the count).
static double frequency_while_loud(const float* s, size_t n, double rate, double floor_ratio) {
    float loudest = peak(s, n);
    double first = -1.0, last = -1.0; long cross = 0; float cyc = 0.0f;
    for (size_t i = 1; i < n; i++) {
        cyc = std::fmax(cyc, std::fabs(s[i]));
        if (s[i - 1] < 0.0f && s[i] >= 0.0f) {
            if (first >= 0.0 && cyc < loudest * floor_ratio) break;
            const double t = (double)(i - 1) + (double)(-s[i - 1]) / (double)(s[i] - s[i - 1]);
            if (first < 0.0) first = t; last = t; cross++;
            cyc = 0.0f;
        }
    }
    if (cross < 2) return 0.0;
    return (double)(cross - 1) * rate / (last - first);
}
static double cents(double f, double ref) { return 1200.0 * std::log2(f / ref); }
static double db(double x) { return 20.0 * std::log10(x); }
static double note_hz(int n) { return 440.0 * std::pow(2.0, (n - 69) / 12.0); }
static void note_on(voxo_t* v, int ch, int note, int vel) { voxo_push_midi(v, (uint8_t)(0x90 | ch), (uint8_t)note, (uint8_t)vel); }
static void note_off(voxo_t* v, int ch, int note) { voxo_push_midi(v, (uint8_t)(0x80 | ch), (uint8_t)note, 0); }
static void bend14(voxo_t* v, int ch, int value) { voxo_push_midi(v, (uint8_t)(0xE0 | ch), (uint8_t)(value & 0x7F), (uint8_t)(value >> 7)); }
static void mcm(voxo_t* v) { voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15); }

// The amplitude envelope's ripple over a signal: the peak of each cycle
// (between rising zero crossings), the ratio of the largest to the smallest
// after `skip` seconds, in dB.
static double ripple_db(const float* s, size_t n, double rate, double skip) {
    double hi = 0.0, lo = 1e30; size_t start = (size_t)(skip * rate);
    bool in = false; size_t kmax = 0; float cur = 0.0f;
    for (size_t i = start + 1; i + 1 < n; i++) {
        if (s[i - 1] < 0.0f && s[i] >= 0.0f) {
            if (in && cur > 0.0f && kmax > 0) {
                // the cycle's true peak: a parabola through the peak sample and its neighbours
                // (a sampled sine's largest sample sits under the crest by up to 1 − cos(π/N))
                const double a = s[kmax - 1], b = s[kmax], c = s[kmax + 1];
                const double den = a - 2.0 * b + c;
                const double pk = den != 0.0 ? b - (c - a) * (c - a) / (8.0 * den) : b;
                hi = std::fmax(hi, std::fabs(pk)); lo = std::fmin(lo, std::fabs(pk));
            }
            in = true; cur = 0.0f; kmax = 0;
        }
        if (s[i] > cur) { cur = s[i]; kmax = i; }
    }
    return lo > 0.0 ? db(hi / lo) : 999.0;
}

int main() {
    std::printf("[suzu] the cell's gates (SYNTH §5)\n");

    // ---- 1. the drift test: the bare cell, 10 minutes at 48 k ---------------
    {
        const double rate = 48000.0, minutes = 10.0;
        const size_t n = (size_t)(rate * 60.0 * minutes);
        suzu::Cell c; c.reset(); c.kick(0.5f, suzu::eps_for(440.0f, (float)rate));
        std::vector<float> head((size_t)rate), tail((size_t)rate);
        for (size_t i = 0; i < n; i++) {
            c.step();
            if (i < head.size()) head[i] = c.x;
            if (i >= n - tail.size()) tail[i - (n - tail.size())] = c.x;
        }
        const double a0 = peak(head.data(), head.size()), a1 = peak(tail.data(), tail.size());
        const double f0 = frequency(head.data(), head.size(), rate), f1 = frequency(tail.data(), tail.size(), rate);
        CHECK(std::fabs(db(a1 / a0)) < 0.1 && std::fabs(cents(f1, f0)) < 0.5,
              "drift: 10 min undriven, undamped at A = 0.5 — amplitude %.4f -> %.4f (%+.4f dB), frequency %.4f -> %.4f Hz (%+.3f cent)",
              a0, a1, db(a1 / a0), f0, f1, cents(f1, f0));
        CHECK(std::fabs(cents(f0, 440.0)) < 0.5, "drift: the exact tuning — 440 asked, %.4f measured (%+.3f cent)", f0, cents(f0, 440.0));
        // the negative control: the simultaneous update, det 1 + ε²
        suzu::Cell nc; nc.reset(); nc.kick(0.5f, suzu::eps_for(440.0f, (float)rate));
        double drift_db = 0.0; size_t spiralled_at = 0;
        for (size_t i = 0; i < n; i++) {
            nc.step_naive();
            if ((i % (size_t)rate) == 0 && i > 0) {
                const double a = std::sqrt((double)nc.x * nc.x + (double)nc.y * nc.y);
                drift_db = db(a / 0.5);
                if (drift_db > 0.1 && !spiralled_at) spiralled_at = i;
                if (!(a < 1e6)) break;
            }
        }
        CHECK(drift_db > 0.1, "drift, the RED control: the naive simultaneous update spirals — %+.1f dB after %zu s (over the 0.1 dB bound at %.1f s)",
              drift_db, n / (size_t)rate, (double)spiralled_at / rate);
    }

    // ---- 2. the glide test: ±48 semitones over 2 s, through the ABI ---------
    for (int mode = 0; mode < 3; mode++) {
        const uint32_t rate = 48000;
        voxo_t* v = make(rate);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
        sp.release_s = 10.0f; sp.retune_mode = (uint32_t)mode;
        voxo_set_suzu_params(v, &sp);
        mcm(v);
        bend14(v, 1, 0);                        // −48 semitones (the member range)
        note_on(v, 1, 60, 127);
        std::vector<float> all;
        std::vector<float> chunk = render(v, rate, 0.1);   // settle
        all.insert(all.end(), chunk.begin(), chunk.end());
        const int blocks = (int)(2.0 * rate / BLOCK);
        std::vector<float> out(2u * BLOCK);
        for (int b = 0; b < blocks; b++) {
            const int value = (int)std::lround(16383.0 * (double)b / (double)(blocks - 1));
            bend14(v, 1, value);
            voxo_render(v, out.data(), BLOCK);
            for (uint32_t f = 0; f < BLOCK; f++) all.push_back(out[2u * f]);
        }
        const double r = ripple_db(all.data(), all.size(), rate, 0.1);
        if (mode == 0) CHECK(r < 0.5, "glide: −48 -> +48 semitones over 2 s, the orbit re-based on retune: amplitude ripple %.3f dB (< 0.5)", r);
        else if (mode == 1) NOTE("glide, the plain recurrence under the per-sample ramp (not gated, printed): ripple %.3f dB", r);
        else NOTE("glide, the plain recurrence stepped once per block, no ramp — SYNTH §2.1's feared form (printed): ripple %.3f dB", r);
        voxo_destroy(v);
    }

    // the jump: −48 to +48 in ONE block, no ramp — where the plain form pays (printed)
    for (int mode = 0; mode < 3; mode += 2) {
        const uint32_t rate = 48000;
        voxo_t* v = make(rate);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
        sp.release_s = 10.0f; sp.retune_mode = (uint32_t)mode;
        voxo_set_suzu_params(v, &sp);
        mcm(v);
        double worst = 0.0;
        for (int phase = 0; phase < 8; phase++) {          // the jump lands at eight different phases of the orbit
            bend14(v, 1, 0);
            note_on(v, 1, 60, 127);
            std::vector<float> before = render(v, rate, 0.1 + 0.00025 * phase);
            const double a0 = peak(before.data() + rate / 20, before.size() - rate / 20);
            bend14(v, 1, 16383);
            std::vector<float> after = render(v, rate, 0.1);
            const double a1 = peak(after.data() + rate / 50, after.size() - rate / 50);
            worst = std::fmax(worst, std::fabs(db(a1 / a0)));
            note_off(v, 1, 60); voxo_push_midi(v, 0xB1, 120, 0); render(v, rate, 0.05);
        }
        NOTE("the jump, −48 -> +48 in one block, eight phases: worst amplitude change %.3f dB (%s)", worst,
             mode == 0 ? "the orbit re-based: the decimator's roll-off alone" : "the plain form stepped: the invariant's −Δε·x·y");
        voxo_destroy(v);
    }

    // ---- 3. tuning: the oscillator and the SVF across MIDI 21–108 -----------
    for (int ri = 0; ri < 2; ri++) {
        const uint32_t rate = ri == 0 ? 44100u : 48000u;
        const float rate2 = 2.0f * (float)rate;
        double worst_osc = 0.0; int worst_osc_note = 0;
        double worst_svf = 0.0; int worst_svf_note = 0;
        for (int note = 21; note <= 108; note++) {
            const double hz = note_hz(note);
            // the oscillator at the oversampled rate, decimated as the voice does
            suzu::Cell c; c.reset(); c.kick(0.5f, suzu::eps_for((float)hz, rate2));
            std::vector<float> s((size_t)rate);
            for (size_t i = 0; i < s.size(); i++) { c.step(); const float a = c.x; c.step(); s[i] = 0.5f * (a + c.x); }
            const double e = std::fabs(cents(frequency(s.data(), s.size(), rate), hz));
            if (e > worst_osc) { worst_osc = e; worst_osc_note = note; }
            // the SVF's resonant peak: rung by an impulse at high resonance, its ringing frequency
            suzu::Svf f; f.reset();
            const float q = 0.02f, fc = suzu::svf_f_for((float)hz, rate2, q);
            std::vector<float> ring((size_t)rate);
            for (size_t i = 0; i < ring.size(); i++) {
                float a = f.step(i == 0 ? 1.0f : 0.0f, fc, q);
                float b = f.step(0.0f, fc, q);
                ring[i] = 0.5f * (a + b);
                (void)b;
            }
            const double es = std::fabs(cents(frequency_while_loud(ring.data() + 64, ring.size() - 64, rate, 1e-3), hz));
            if (es > worst_svf) { worst_svf = es; worst_svf_note = note; }
        }
        CHECK(worst_osc < 2.0, "tuning at %u Hz: the oscillator within %.3f cent across MIDI 21–108 (worst at %d)", rate, worst_osc, worst_osc_note);
        CHECK(worst_svf < 2.0, "tuning at %u Hz: the SVF's resonant peak within %.3f cent across MIDI 21–108 (worst at %d)", rate, worst_svf, worst_svf_note);
    }
    // the same through the ABI: a note's pitch after the voice's own retune path
    {
        const uint32_t rate = 48000;
        voxo_t* v = make(rate);
        mcm(v);
        double worst = 0.0; int worst_note = 0;
        for (int note = 21; note <= 108; note += 3) {
            note_on(v, 1, note, 100);
            std::vector<float> s = render(v, rate, 0.5);
            const double e = std::fabs(cents(frequency(s.data() + rate / 10, s.size() - rate / 10, rate), note_hz(note)));
            if (e > worst) { worst = e; worst_note = note; }
            note_off(v, 1, note);
            render(v, rate, 0.6);
        }
        CHECK(worst < 2.0, "tuning through the ABI: a struck note within %.3f cent (worst at %d)", worst, worst_note);
        voxo_destroy(v);
    }

    // ---- 4. the denormal gate: 64 voices released together -----------------
    for (int pass = 0; pass < 2; pass++) {
        const uint32_t rate = 48000;
        voxo_t* v = make(rate, 64);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
        sp.release_s = 2.0f;                    // a 2 s T60: the tail crosses the subnormal range within the run
        voxo_set_suzu_params(v, &sp);
        voxo_set_input_mode(v, 2);              // classic: 64 notes on one channel
        for (int i = 0; i < 64; i++) note_on(v, 0, 30 + i, 100);
        render(v, rate, 0.2);
        for (int i = 0; i < 64; i++) note_off(v, 0, 30 + i);
        // pass 0: FTZ as the render sets it; pass 1: the negative control, FTZ off for the calling thread
        if (pass == 1) suzu::ftz_disable();
        std::vector<float> out(2u * BLOCK);
        double per_second[12] = {0};
        for (int s = 0; s < 12; s++) {
            const auto t0 = std::chrono::steady_clock::now();
            for (uint32_t f = 0; f < rate; f += BLOCK) voxo_render(v, out.data(), BLOCK);
            per_second[s] = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        }
        voxo_stats_t st; voxo_stats(v, &st);
        double first = per_second[0], worst = 0.0; int worst_s = 0;
        for (int s = 1; s < 12; s++) if (per_second[s] > worst) { worst = per_second[s]; worst_s = s; }
        if (pass == 0) {
            CHECK(st.ftz == 1, "denormals: the rendering thread set flush-to-zero at its first block (stats.ftz %u)", st.ftz);
            CHECK(worst < 2.0 * first, "denormals: 64 voices released, a 2 s T60, 12 s of tail — the render time per second stays flat (%.1f ms first, worst %.1f ms at second %d; the tail is under −80 dB after 2.7 s)",
                  first, worst, worst_s + 1);
        } else {
            NOTE("denormals, FTZ off (the control; the cliff is x86's): %.1f ms first, worst %.1f ms at second %d%s",
                 first, worst, worst_s + 1, worst > 2.0 * first ? " — RED, as a subnormal cliff" : " — no cliff on this CPU");
            suzu::ftz_enable();
        }
        voxo_destroy(v);
    }

    // ---- 4b. the shears' confinement: every note, both kinds, full gain -----
    for (int kind = 0; kind < 2; kind++) {
        const uint32_t rate = 48000;
        voxo_t* v = make(rate);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
        sp.shear = 1.0f; sp.shear_kind = (uint32_t)kind; sp.release_s = 10.0f;
        voxo_set_suzu_params(v, &sp);
        mcm(v);
        double worst_peak = 0.0; int worst_note = 0; bool finite = true; double h2_low = 0.0;
        for (int note = 21; note <= 108; note += 3) {
            note_on(v, 1, note, 127);
            std::vector<float> s = render(v, rate, 0.5);
            double p = 0.0;
            for (float x : s) { if (!std::isfinite(x)) finite = false; p = std::fmax(p, std::fabs(x)); }
            if (p > worst_peak) { worst_peak = p; worst_note = note; }
            if (note == 36) {   // C2: the harmonics the shear makes — the THIRD (odd shapes make odd harmonics), against the bare cell's −60 dB leakage
                const double hz = note_hz(note); double c0 = 2.0 * std::cos(2.0 * 3.14159265358979 * 3.0 * hz / rate), s1 = 0, s2 = 0;
                for (size_t i = rate / 10; i < s.size(); i++) { const double t = s[i] + c0 * s1 - s2; s2 = s1; s1 = t; }
                const double n = s.size() - rate / 10; h2_low = db(2.0 * std::sqrt(std::fmax(s1 * s1 + s2 * s2 - c0 * s1 * s2, 0.0)) / n);
            }
            note_off(v, 1, note); voxo_push_midi(v, 0xB1, 120, 0); render(v, rate, 0.05);
        }
        {   // the tuning under the shear: C2 and C4 at full gain, the first-order detune compensated
            double worst_cents = 0.0;
            NOTE("the %s shear's calibration: the sheared cell's frequency over the bare cell's at g = 0.25 / 0.5 / 1: %.4f / %.4f / %.4f",
                 kind == 0 ? "cubic" : "triangle", suzu::shear_period_ratio(kind == 0, 0.25f), suzu::shear_period_ratio(kind == 0, 0.5f), suzu::shear_period_ratio(kind == 0, 1.0f));
            for (int note : {36, 60, 96}) {
                note_on(v, 1, note, 100);
                std::vector<float> s = render(v, rate, 0.5);
                const double e = std::fabs(cents(frequency(s.data() + rate / 10, s.size() - rate / 10, rate), note_hz(note)));
                worst_cents = std::fmax(worst_cents, e);
                note_off(v, 1, note); voxo_push_midi(v, 0xB1, 120, 0); render(v, rate, 0.05);
            }
            CHECK(worst_cents < 10.0, "tuning under the %s shear at full gain (the cell's own calibration compensating ε): C2, C4 and C7 within %.2f cent (< 10, the reference ε's residual)",
                  kind == 0 ? "cubic" : "triangle", worst_cents);
        }
        CHECK(finite && worst_peak < 0.5,
              "confinement, the %s shear at full gain across MIDI 21–108 at velocity 127: every sample finite, the worst peak %.3f (< 0.5; the bare cell peaks 0.25) at note %d; the third harmonic at C2 reads %.1f dBFS",
              kind == 0 ? "cubic" : "triangle", worst_peak, worst_note, h2_low);
        voxo_destroy(v);
    }

    // ---- 5. callback headroom: sixteen voices for a second ------------------
    {
        const uint32_t rate = 48000;
        voxo_t* v = make(rate, 16);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
        sp.cutoff_hz = 2000.0f; sp.resonance = 0.5f; sp.shear = 0.2f;   // the filter and the shear in the path
        voxo_set_suzu_params(v, &sp);
        mcm(v);
        for (int i = 0; i < 15; i++) note_on(v, 1 + i, 48 + i * 2, 100);
        note_on(v, 0, 40, 100);
        std::vector<float> out(2u * BLOCK);
        const int blocks = (int)(rate / BLOCK);
        double worst_ms = 0.0, total_ms = 0.0;
        for (int b = 0; b < blocks; b++) {
            const auto t0 = std::chrono::steady_clock::now();
            voxo_render(v, out.data(), BLOCK);
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            total_ms += ms; if (ms > worst_ms) worst_ms = ms;
        }
        const double period_ms = 1000.0 * BLOCK / rate;
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(st.active_voices == 16 && st.source == VOXO_SOURCE_SUZU && worst_ms < period_ms,
              "headroom: %u Suzu voices (SVF + shear, 2x), block %u at %u Hz — mean %.3f ms, worst %.3f ms of a %.3f ms period (%.1f %% of the callback)",
              st.active_voices, BLOCK, rate, total_ms / blocks, worst_ms, period_ms, 100.0 * worst_ms / period_ms);
        voxo_destroy(v);
    }

    // ---- the source switch and the sampler untouched -------------------------
    {
        const uint32_t rate = 48000;
        voxo_t* v = make(rate);
        mcm(v);
        note_on(v, 1, 69, 100);
        render(v, rate, 0.05);
        voxo_stats_t st; voxo_stats(v, &st);
        const uint32_t before = st.active_voices;
        voxo_set_source(v, VOXO_SOURCE_SAMPLER);
        render(v, rate, 0.05);
        voxo_stats(v, &st);
        CHECK(before == 1 && st.active_voices == 0 && st.source == VOXO_SOURCE_SAMPLER,
              "the source switch ends every voice (as an instrument swap does): %u -> %u, source %u", before, st.active_voices, st.source);
        note_on(v, 1, 69, 100);
        std::vector<float> s = render(v, rate, 0.2);
        const double f = frequency(s.data() + rate / 20, s.size() - rate / 20, rate);
        CHECK(std::fabs(f - 440.0) < 0.5, "back on the sampler the sine plays as before: A4 measures %.2f Hz", f);
        voxo_destroy(v);
    }

    std::printf("[suzu] %s (%d failures)\n", g_fail ? "FAILED" : "all gates green", g_fail);
    return g_fail ? 1 : 0;
}
