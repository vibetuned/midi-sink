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
// Step 57 (SYNTH §2.5–§2.6, §5) — the modal voice and the breath bow:
//   6. the ENERGY LEDGER — a bell struck once with every decay declared zero
//      sustains 10 minutes within the drift bound; with the decays restored,
//      every mode's measured T60 within 5 % of the declared;
//   7. the BOW's limit cycle — from silence and from 2× alike within the
//      declared time constant, zero breath to silence, the ledger of injected
//      against extracted energy within 1 %, the give-only servo proven RED;
//   8. the LATTICE LOAD GATE — the patch's bound accepted at 0.95×, rejected
//      at 1.05× with its message, the rejected patch with the gate bypassed
//      blowing up within a second (RED); tuning under coupling within 2 cents
//      (the compensation); mode splitting measured against the joint map's
//      normal modes and written for the chart (SUZU_EVIDENCE=<dir>).
// Step 58 (SYNTH §2.3, §2.8–§2.10, §5) — strings & chaos:
//   9. the CFL gate — a forced k·dt² of 1.05 rejected with its message, 0.95
//      admitted; the bypass blows up within a second (RED); the chain's
//      tuning sweep MIDI 21–108 within 2 cents, the nodes reduced per note;
//  10. loop passivity — the load-time probe's bound on bridge_gain (the
//      conserving junction's 1), 1.05 rejected with its message; the soak:
//      every declared damping zeroed, ten minutes within the drift bound; the
//      bypassed 1.05 grows (RED); the hybrid's tuning within 2 cents;
//  11. the Duffing cell — the clang-and-settle measured; the drive's spectrum;
//  12. the kicked rotor — in tune at K = 0, the drift test at K 0.3 (the
//      momentum on its torus), broadband past K_c;
//  13. the chaotic modulator — bounded, its energy held for ten minutes;
//  14. the layered source — one note, both bodies;
//  15. headroom — ten Verlet strings at 80 nodes against the period.
// Step 58b (SYNTH §2.11, §2.13, §5) — the bore & the jet:
//  16. the bore's series — closed–open cylinder at the odd harmonics, the
//      cone and the open–open cylinder at all integers, within cents;
//  17. the closed lossless bore holds the drift bound for ten minutes; the
//      CFL gate (a forced Courant number 1.05× the bound rejected with its
//      message, the bypass blowing up within a second — RED);
//  18. the mouth-power ledger — over a scripted phrase the stored energy never
//      exceeds the mouth's work, and the bore's own arithmetic (injected =
//      stored + radiated) holds within 1 %;
//  19. the overblow — a breath ramp on A4 jumps the octave with no other
//      change; soft blowing flattens the pitch.
// Step 58c (SYNTH §2.12, §2.11, §5) — the reed & the lips:
//  20. the mouth-power ledger for the reed and the lips on a scripted phrase
//      (the implicit junction), the naive explicit junction growing — RED;
//  21. the valve gate through the ABI: the naive junction rejected with its
//      message, the bypass blowing up within a second;
//  22. the sax bore's peaks at all integers (the conical result, measured);
//      every note C3–C6 sounds on the note (the reed's pull calibrated);
//  23. the trumpet: on the note at CC 74 centre across C3–C5, a register
//      down at 0 and up at 127 (the byte log printed), every note sounding.
// voxo_render is called as the callback would; no device.
#include "voxo.h"
#include "suzu.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <string>

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


// ---- step 57's helpers -------------------------------------------------------
// A Goertzel magnitude (Hann-windowed) at `hz` over n samples, as an amplitude.
static double goertzel(const float* s, size_t n, double hz, double rate) {
    const double w = 2.0 * M_PI * hz / rate, cw = 2.0 * std::cos(w);
    double s0 = 0.0, s1 = 0.0, s2 = 0.0;
    for (size_t i = 0; i < n; i++) {
        const double win = 0.5 - 0.5 * std::cos(2.0 * M_PI * (double)i / (double)(n - 1));
        s0 = (double)s[i] * win + cw * s1 - s2; s2 = s1; s1 = s0;
    }
    const double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w);
    return 4.0 * std::sqrt(re * re + im * im) / (double)n;   // the Hann window's coherent gain is 0.5
}
// The peak's frequency near `hz`: a Goertzel on a 1-cent grid ±120 cents, parabolic on the top.
// The period of a wind's tone: the autocorrelation's first lag that reaches its maximum (within 0.08), lags from
// rate/4000 to rate/60, then the 4-cent Goertzel scan about it. A first-register sax whose fundamental sits 15 dB
// under its second harmonic has its period at T all the same (the odd partials break the T/2 symmetry); a true
// second register reads the octave. The "lowest partial within 15 dB" rule read the weak fundamental as an octave.
static double period_hz(const float* s, size_t n, double rate) {
    const size_t lo = (size_t)(rate / 4000.0), hi = std::min((size_t)(rate / 60.0), n / 2);
    double e0 = 0.0; for (size_t i = 0; i < n - hi; i++) e0 += (double)s[i] * s[i];
    std::vector<double> rr(hi, -2.0); double best = -2.0;
    for (size_t l = lo; l < hi; l++) {
        double c = 0.0, e1 = 0.0; for (size_t i = 0; i < n - hi; i++) { c += (double)s[i] * s[i + l]; e1 += (double)s[i + l] * s[i + l]; }
        rr[l] = c / std::sqrt((e0 > 0.0 ? e0 : 1e-30) * (e1 > 0.0 ? e1 : 1e-30)); if (rr[l] > best) best = rr[l];
    }
    double bl = (double)lo;
    for (size_t l = lo + 1; l + 1 < hi; l++) if (rr[l] >= best - 0.08 && rr[l] >= rr[l - 1] && rr[l] >= rr[l + 1]) { bl = (double)l; break; }   // 0.08: a jittery or period-doubled cycle still reads its period
    const double f0 = rate / bl; double bm = -1.0, bf = f0;
    for (double hz = f0 * std::pow(2.0, -60.0 / 1200.0); hz < f0 * std::pow(2.0, 60.0 / 1200.0); hz *= std::pow(2.0, 4.0 / 1200.0)) { const double m = goertzel(s, n, hz, rate); if (m > bm) { bm = m; bf = hz; } }
    return bf;
}
static double peak_near(const float* s, size_t n, double hz, double rate, double span_cents = 120.0) {
    double best = -1.0; int bc = 0; const int half = (int)span_cents;
    std::vector<double> m(2 * half + 1);
    for (int c = -half; c <= half; c++) { m[c + half] = goertzel(s, n, hz * std::pow(2.0, c / 1200.0), rate); if (m[c + half] > best) { best = m[c + half]; bc = c; } }
    if (bc > -half && bc < half) {
        const double a = m[bc + half - 1], b = m[bc + half], cc = m[bc + half + 1], den = a - 2.0 * b + cc;
        const double off = den != 0.0 ? 0.5 * (a - cc) / den : 0.0;
        return hz * std::pow(2.0, (bc + off) / 1200.0);
    }
    return hz * std::pow(2.0, bc / 1200.0);
}
static double rms(const float* s, size_t n) { double a = 0.0; for (size_t i = 0; i < n; i++) a += (double)s[i] * s[i]; return std::sqrt(a / (double)n); }
static std::string g_log;   // the gate's message, captured
static void log_capture(int level, const char* msg, void* user) { (void)level; (void)user; g_log = msg ? msg : ""; }
static voxo_t* make_logged(uint32_t rate, uint32_t max_voices = 0) {
    voxo_config_t c{}; c.sample_rate = rate; c.block_frames = BLOCK; c.max_voices = max_voices; c.log_cb = log_capture;
    voxo_t* v = voxo_create(&c);
    voxo_set_input_mode(v, 1);
    voxo_set_source(v, VOXO_SOURCE_SUZU);
    return v;
}
static void cc(voxo_t* v, int ch, int ctl, int value) { voxo_push_midi(v, (uint8_t)(0xB0 | ch), (uint8_t)ctl, (uint8_t)value); }
static void swirl(voxo_t* v, int ch, int note, int value) { voxo_push_midi(v, (uint8_t)(0xA0 | ch), (uint8_t)note, (uint8_t)value); }
static bool finite_all(const float* s, size_t n) { for (size_t i = 0; i < n; i++) if (!std::isfinite(s[i])) return false; return true; }
// A lattice patch: the modal voice with the bow off and the filter open.
static voxo_suzu_params_t lattice_patch(uint32_t preset, uint32_t modes, float coupling) {
    voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
    sp.voice_kind = 1; sp.modal_preset = preset; sp.modes = modes; sp.coupling = coupling;
    sp.bow_onset_s = 0.0f; sp.cutoff_hz = 20000.0f; sp.resonance = 0.0f; sp.shear = 0.0f; sp.release_s = 0.5f;
    return sp;
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
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 0;   // the cell's gates: step 56's single cell
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
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 0;   // the cell's gates: step 56's single cell
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
        { voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 0; voxo_set_suzu_params(v, &sp); }   // the single cell
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
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 0;   // the cell's gates: step 56's single cell
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
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 0;   // the cell's gates: step 56's single cell
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
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 0;   // the cell's gates: step 56's single cell
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

    std::printf("[suzu] step 57 — the modal voice and the breath bow (SYNTH §2.5–§2.6, §5)\n");
    const char* evidence = std::getenv("SUZU_EVIDENCE");

    // ---- 6. the energy ledger: a bell, every decay declared zero, 10 minutes ----
    {
        const uint32_t rate = 48000;
        voxo_t* v = make(rate);
        voxo_suzu_params_t sp = lattice_patch(2, 8, 0.3f);   // the bell, coupled — energy migrates, none is made or lost
        sp.decay_s = 0.0f; sp.decay_bright = 0.0f;           // NO declared decay while held
        CHECK(voxo_set_suzu_params(v, &sp), "ledger: the zero-decay bell patch is admitted by the gate");
        mcm(v);
        note_on(v, 1, 60, 100);
        double first = 0.0, last = 0.0, lo = 1e30, hi = 0.0;
        const int seconds = 600;
        for (int sec = 0; sec < seconds; sec++) {
            std::vector<float> s = render(v, rate, 1.0);
            if (!finite_all(s.data(), s.size())) { first = 1.0; last = 1e6; break; }
            const double r = rms(s.data(), s.size());
            if (sec == 1) first = r;
            if (sec >= 1) { lo = std::fmin(lo, r); hi = std::fmax(hi, r); }
            last = r;
        }
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(std::fabs(db(last / first)) < 0.1 && db(hi / lo) < 0.1 && st.suzu_modes == 8,
              "ledger: the bell (8 modes, κ = 0.3) with every decay declared zero sustains %d min — RMS second 1 -> second %d: %+.4f dB (< 0.1), the envelope's spread %.4f dB",
              seconds / 60, seconds, db(last / first), db(hi / lo));
        voxo_destroy(v);
    }
    // the decays restored: each mode's T60 measured against the declared table
    for (int pass = 0; pass < 2; pass++) {
        const uint32_t rate = 48000;
        const float kappa = pass == 0 ? 0.0f : 0.05f;
        voxo_t* v = make(rate);
        voxo_suzu_params_t sp = lattice_patch(2, 8, kappa);
        sp.decay_s = 2.0f; sp.decay_bright = 0.3f;
        voxo_set_suzu_params(v, &sp);
        suzu::ModalTable t; suzu::modal_table(&t, 2, 8, 2.0f, 0.3f, 0.0f, 0.28f, 0.3f);
        mcm(v);
        note_on(v, 1, 60, 100);
        std::vector<float> s = render(v, rate, 2.6);
        const double f0 = note_hz(60);
        const size_t win = (size_t)(0.2 * rate), hop = (size_t)(0.1 * rate);
        double worst = 0.0; int worst_k = 0;
        std::string line;
        for (int k = 0; k < t.n; k++) {
            // the slope of the mode's level in dB against time, least squares over the windows above −90 dBFS
            double sx = 0, sy = 0, sxx = 0, sxy = 0; int m = 0;
            for (size_t at = (size_t)(0.15 * rate); at + win <= s.size(); at += hop) {
                const double a = goertzel(s.data() + at, win, f0 * t.ratio[k], rate);
                if (a < 3e-5) break;
                const double x = (double)at / rate, y = db(a);
                sx += x; sy += y; sxx += x * x; sxy += x * y; m++;
            }
            const double slope = m > 2 ? (m * sxy - sx * sy) / (m * sxx - sx * sx) : 0.0;   // dB per second
            const double t60 = slope < 0.0 ? -60.0 / slope : 0.0;
            const double err = std::fabs(t60 / t.t60[k] - 1.0);
            if (err > worst) { worst = err; worst_k = k; }
            char b[64]; std::snprintf(b, sizeof b, " r%.3g: %.2f/%.2f", (double)t.ratio[k], t60, (double)t.t60[k]); line += b;
        }
        if (pass == 0) CHECK(worst < 0.05, "ledger: the bell's T60s restored (2 s, β 0.3), κ = 0 — every mode within %.1f %% of the declared (worst r%.3g); measured/declared:%s", 100.0 * worst, (double)t.ratio[worst_k], line.c_str());
        else NOTE("ledger, the same at κ = %.2f (printed: coupled modes share their decays): worst %.1f %%;%s", (double)kappa, 100.0 * worst, line.c_str());
        voxo_destroy(v);
    }

    // ---- 7. the bow's limit cycle, on the cell -------------------------------
    {
        const double rate2 = 96000.0;
        const float e = suzu::eps_for(220.0f, (float)rate2);
        const float A_t = 0.25f, E_t = A_t * A_t;
        const float tau = 0.1f, g = 1.0f / (tau * (float)rate2);
        const float r = suzu::contraction_for(2.0f, (float)rate2);             // a declared T60 of 2 s the bow must overcome
        auto amp_of = [&](const suzu::Cell& c) { return std::sqrt(c.energy() / (1.0 - 0.25 * (double)c.eps * c.eps)); };
        auto run = [&](suzu::Cell& c, double seconds, double* a_ss) {          // returns the time the amplitude settles within 5 % of its final value
            const size_t n = (size_t)(seconds * rate2);
            std::vector<float> a(n / 96);                                        // the amplitude every 1 ms
            for (size_t i = 0; i < n; i++) {
                c.step(); c.contract(r);
                c.contract(suzu::bow_factor(c.x * c.x + c.y * c.y, E_t, g));
                if (i % 96 == 0) a[i / 96] = (float)amp_of(c);
            }
            double ss = 0.0; size_t m = 0;
            for (size_t i = a.size() - 1000; i < a.size(); i++) { ss += a[i]; m++; }
            ss /= (double)m; *a_ss = ss;
            size_t settled = a.size();
            for (size_t i = a.size(); i-- > 0;) { if (std::fabs(a[i] - ss) > 0.05 * ss) { settled = i + 1; break; } }
            return settled * 0.001;
        };
        suzu::Cell c; double a_from_silence, a_from_twice;
        c.reset(); c.kick(0.1f * A_t, e);                                        // the seed (−20 dB of the target): silence's first grip
        const double t_silence = run(c, 5.0, &a_from_silence);
        c.reset(); c.kick(2.0f * A_t, e);
        const double t_twice = run(c, 5.0, &a_from_twice);
        // the steady state sits UNDER the target by the declared decay's share: u_ss = γ·τ (E_ss = E_t·(1 − γτ))
        const double gamma = 6.907755 / 2.0, A_ss_pred = A_t * std::sqrt(1.0 - gamma * tau);
        CHECK(t_silence < 12.0 * tau && t_twice < 5.0 * tau && std::fabs(a_from_silence / a_from_twice - 1.0) < 0.01,
              "bow: from silence (a −20 dB seed) the cell settles within 5 %% of its orbit in %.2f s = %.1f τ (< 12 τ), from 2× in %.2f s = %.1f τ (< 5 τ); the two orbits agree to %.2f %% (A_ss %.4f, predicted %.4f under a 2 s declared decay)",
              t_silence, t_silence / tau, t_twice, t_twice / tau, 100.0 * std::fabs(a_from_silence / a_from_twice - 1.0), a_from_silence, A_ss_pred);
        // zero breath: the servo off, the declared decay alone — silence to −60 dB within its T60
        {
            const size_t n = (size_t)(2.2 * rate2);
            double a0 = amp_of(c), amax_after = 0.0;
            bool monotone = true; double prev = a0;
            for (size_t i = 0; i < n; i++) {
                c.step(); c.contract(r);
                if (i % 9600 == 0) { const double a = amp_of(c); if (a > prev * 1.0001) monotone = false; prev = a; }
            }
            amax_after = amp_of(c);
            CHECK(monotone && amax_after < 1e-3 * a0, "bow: zero breath — no servo, the declared decay alone: the tone falls monotonically to %.1f dB in 2.2 s (< −60), no self-oscillation", db(amax_after / a0));
        }
        // the ledger: over a second of steady state the injected and the extracted energy balance
        {
            c.reset(); c.kick(A_t, e);
            for (size_t i = 0; i < (size_t)(3.0 * rate2); i++) { c.step(); c.contract(r); c.contract(suzu::bow_factor(c.x * c.x + c.y * c.y, E_t, g)); }
            double injected = 0.0, extracted = 0.0; const double E_start = c.energy();
            for (size_t i = 0; i < (size_t)rate2; i++) {
                const double E0 = c.energy();
                c.step();                                                       // symplectic: E unchanged (the rotation's own ledger)
                const double E1 = c.energy();
                c.contract(r);
                const double E2 = c.energy();
                extracted += E1 - E2;
                const float f = suzu::bow_factor(c.x * c.x + c.y * c.y, E_t, g);
                c.contract(f);
                const double E3 = c.energy();
                if (f > 1.0f) injected += E3 - E2; else extracted += E2 - E3;
                injected += (E1 - E0 > 0.0) ? (E1 - E0) : 0.0; extracted += (E0 - E1 > 0.0) ? (E0 - E1) : 0.0;   // the rotation's residue, both ways (rounding)
            }
            const double E_end = c.energy();
            const double bal = std::fabs(injected - extracted) / injected;
            CHECK(bal < 0.01 && std::fabs(E_end / E_start - 1.0) < 0.01,
                  "bow: the servo's ledger over 1 s of steady state — injected %.3e, extracted %.3e (the declared decay + the bow's slips): balance within %.3f %% (< 1), E %.3e -> %.3e", injected, extracted, 100.0 * bal, E_start, E_end);
        }
        // the NEGATIVE control: the servo that gives and never takes — from 2× the target, where extraction is what holds the cycle
        {
            suzu::Cell nc; nc.reset(); nc.kick(2.0f * A_t, e);
            double ratio = 1.0; size_t at = 0;
            for (size_t i = 0; i < (size_t)(2.0 * rate2); i++) {
                nc.step(); nc.contract(r);
                nc.contract(suzu::bow_factor_give_only(nc.x * nc.x + nc.y * nc.y, E_t, g));
                if ((i % 960) == 0) { ratio = nc.energy() / E_t; if (ratio > 1000.0 && !at) at = i; }
            }
            CHECK(at > 0 && (ratio > 1000.0 || !std::isfinite(ratio)), "bow, the RED control: the give-only servo from 2× grows without bound — 1000× the target's energy at %.2f s, %.1e× at 2 s", at / rate2, ratio);
        }
    }
    // the bow through the ABI: a breath sings a note from near silence and from a hard strike alike; no breath, silence
    {
        const uint32_t rate = 48000;
        voxo_t* v = make(rate);
        voxo_suzu_params_t sp = lattice_patch(0, 8, 0.05f);
        sp.bow_onset_s = 0.1f; sp.decay_s = 2.0f;
        voxo_set_suzu_params(v, &sp);
        mcm(v);
        auto sing = [&](int velocity, int breath) {
            cc(v, 1, 2, breath);
            note_on(v, 1, 57, velocity);
            std::vector<float> s = render(v, rate, 2.5);
            const double a = rms(s.data() + (size_t)(2.0 * rate), (size_t)(0.5 * rate));
            const double b = rms(s.data() + (size_t)(1.5 * rate), (size_t)(0.5 * rate));
            return std::make_pair(a, db(a / b));
        };
        auto from_silence = sing(1, 80);
        cc(v, 1, 2, 0); std::vector<float> tail = render(v, rate, 3.0);             // the breath withdrawn, the note held
        const double silence = rms(tail.data() + (size_t)(2.5 * rate), (size_t)(0.5 * rate));
        note_off(v, 1, 57); render(v, rate, 1.0);
        auto from_strike = sing(127, 80);
        note_off(v, 1, 57); render(v, rate, 1.0);
        CHECK(from_silence.first > 0.01 && std::fabs(from_silence.second) < 0.2 && std::fabs(db(from_strike.first / from_silence.first)) < 0.3,
              "bow through the ABI: breath 80/127 at velocity 1 sings at %.1f dBFS (stable to %.2f dB over the last second); from velocity 127 the same tone within %.2f dB",
              db(from_silence.first), from_silence.second, db(from_strike.first / from_silence.first));
        CHECK(silence < from_silence.first * 1e-3, "bow through the ABI: the breath withdrawn, the held note is %.1f dB down after 3 s (< −60): silence is gated, not hoped", db(silence / from_silence.first));
        voxo_destroy(v);
    }

    // ---- 8. the lattice load gate, its red control, tuning under coupling, mode splitting ----
    {
        const uint32_t rate = 48000;
        voxo_t* v = make_logged(rate);
        voxo_suzu_params_t sp = lattice_patch(0, 16, 0.0f);
        const float bound = voxo_suzu_coupling_bound(v, &sp);
        sp.coupling = 0.95f * bound; g_log.clear();
        const bool ok95 = voxo_set_suzu_params(v, &sp);
        sp.coupling = 1.05f * bound; g_log.clear();
        const bool ok105 = voxo_set_suzu_params(v, &sp);
        const std::string msg = g_log;
        CHECK(ok95 && !ok105 && msg.find("rejected") != std::string::npos,
              "lattice gate: the harmonic 16-mode patch's bound is κ = %.3f — 0.95× admitted, 1.05× rejected with: \"%s\"", bound, msg.c_str());
        // the RED control of the compensation's condition: the rejected patch with the gate bypassed runs on the table frozen at
        // its last feasible κ — bounded, but the partials leave their ratios (the tuning gate red); the 0.95× patch with the swirl
        // at full (κ = 0.95·bound + 0.5, the compensation's far end) stays within 2 cents
        auto fundamental_cents = [&](voxo_suzu_params_t p) {
            voxo_t* w = make_logged(rate);
            voxo_set_suzu_params(w, &p);
            mcm(w);
            note_on(w, 1, 60, 100);
            swirl(w, 1, 60, 127);
            render(w, rate, 0.3);
            std::vector<float> s = render(w, rate, 1.0);
            const bool fin = finite_all(s.data(), s.size());
            const double f = fin ? peak_near(s.data(), s.size(), note_hz(60), rate, 400.0) : 0.0;
            voxo_destroy(w);
            return fin ? cents(f, note_hz(60)) : 9999.0;
        };
        voxo_suzu_params_t red = sp; red.coupling = 1.05f * bound; red.lattice_gate = 0;
        const double red_cents = fundamental_cents(red);
        voxo_suzu_params_t green = sp; green.coupling = 0.95f * bound;
        const double green_cents = fundamental_cents(green);
        CHECK(std::fabs(red_cents) > 2.0, "lattice gate, the RED control: the 1.05× patch with the gate bypassed (the swirl at full) plays C4's fundamental %+.1f cent off — the partials cannot be placed", red_cents);
        CHECK(std::fabs(green_cents) < 2.0, "lattice gate: the 0.95× patch with the swirl at full (κ = %.3f, the compensation's far end) plays C4's fundamental within %.2f cent", 0.95f * bound + 0.5f, std::fabs(green_cents));
        voxo_destroy(v);
    }
    // the SAMPLING BOUND (the CFL analog) on the primitive: two cells just under the ceiling, the coupling 5 % over the bound blows up within a second
    {
        const float rate2 = 96000.0f;
        const float e = suzu::eps_for(0.24f * rate2, rate2);                      // 23.04 kHz at 96 k: a mode just under the ceiling
        const float kstar = (4.0f - e * e) / (2.0f * e * e);                       // λ_max = ε²(1 + 2κ) = 4
        auto run = [&](float kappa) {
            suzu::Cell c[2]; c[0].reset(); c[0].kick(0.25f, e); c[1].reset(); c[1].kick(0.0f, e);
            const float inv[2] = { 1.0f / e, 1.0f / e };
            float pk = 0.0f; size_t at = 0;
            for (size_t i = 0; i < (size_t)rate2; i++) {
                suzu::lattice_kick(c, inv, 2, kappa * e * e);
                c[0].step(); c[1].step();
                const float a = std::fabs(c[0].x) + std::fabs(c[1].x);
                if (!std::isfinite(a) || a > 1e3f) { at = i; pk = a; break; }
                pk = std::fmax(pk, a);
            }
            return std::make_pair(pk, at);
        };
        const float eps2[2] = { e, e };
        const float lam_hi = suzu::lattice_lambda_max(eps2, 2, 1.05f * kstar * e * e), lam_lo = suzu::lattice_lambda_max(eps2, 2, 0.95f * kstar * e * e);
        auto hi = run(1.05f * kstar), lo = run(0.95f * kstar);
        CHECK(hi.second > 0 && lo.second == 0 && lo.first < 1.0f && lam_hi >= 4.0f && lam_lo < 4.0f,
              "the sampling bound: two cells at 23.04 kHz / 96 k, κ* = %.3f — 5 %% over (λ_max %.3f) blows up at %.3f s; 5 %% under (λ_max %.3f) stays bounded for a second (peak %.3f)",
              kstar, lam_hi, hi.second / rate2, lam_lo, lo.first);
        NOTE("(the compensation's condition binds first for every shipped preset within κ ≤ 4: the sampling bound is the chain's ceiling, reached only by modes at the ceiling)");
    }
    // tuning under coupling: the lattice's placed partials stay on their ratios at the patch's κ and with the swirl at full
    {
        const uint32_t rate = 48000;
        double worst = 0.0; std::string where;
        for (int preset = 0; preset < 5; preset += 2) {                          // the harmonic string, the bell, the plucked string
            for (int sw = 0; sw < 2; sw++) {
                voxo_t* v = make(rate);
                voxo_suzu_params_t sp = lattice_patch((uint32_t)preset, 8, 0.05f);
                sp.decay_s = 30.0f; sp.decay_bright = 0.0f;
                voxo_set_suzu_params(v, &sp);
                suzu::ModalTable t; suzu::modal_table(&t, preset, 8, 30.0f, 0.0f, 0.0f, 0.28f, 0.3f);
                mcm(v);
                for (int note = 36; note <= 108; note += 24) {
                    note_on(v, 1, note, 100);
                    if (sw) swirl(v, 1, note, 127);
                    render(v, rate, 0.3);                                        // the swirl's ramp settles
                    std::vector<float> s = render(v, rate, 1.0);
                    for (int k = 0; k < 3; k++) {                                // the three lowest partials
                        const double target = note_hz(note) * t.ratio[k];
                        const double f = peak_near(s.data(), s.size(), target, rate);
                        const double c = std::fabs(cents(f, target));
                        if (c > worst) { worst = c; char b[96]; std::snprintf(b, sizeof b, "preset %d, MIDI %d, r%.3g, swirl %d", preset, note, (double)t.ratio[k], sw); where = b; }
                    }
                    note_off(v, 1, note); render(v, rate, 0.6);
                }
                voxo_destroy(v);
            }
        }
        CHECK(worst < 2.0, "tuning under coupling: the lowest three partials of the harmonic, bell and plucked presets at C2/C4/C6/C8, κ = 0.05 and with the swirl at full (+0.5): within %.2f cent (< 2; worst %s) — the compensation", worst, where.c_str());
        // every alive partial of C8's harmonic string with the swirl at full (printed: the top ones sit near the ceiling, the last against a wall)
        {
            voxo_t* v = make(rate);
            voxo_suzu_params_t sp = lattice_patch(0, 8, 0.05f); sp.decay_s = 30.0f; sp.decay_bright = 0.0f;
            voxo_set_suzu_params(v, &sp); mcm(v);
            note_on(v, 1, 108, 100); swirl(v, 1, 108, 127); render(v, rate, 0.3);
            std::vector<float> s = render(v, rate, 1.0);
            std::string line;
            for (int k = 1; k <= 5; k++) { const double target = note_hz(108) * k; char b[48]; std::snprintf(b, sizeof b, " r%d %+.2f", k, cents(peak_near(s.data(), s.size(), target, rate, 60.0), target)); line += b; }
            NOTE("C8's harmonic string, the swirl at full, every alive partial (5 of 8 under the 24 kHz ceiling), cents off its ratio:%s", line.c_str());
            voxo_destroy(v);
        }
    }
    // mode splitting: two cells at A3, the shared-potential kick, the split against the joint map's normal modes
    {
        const double rate2 = 96000.0;
        const float e = suzu::eps_for(220.0f, (float)rate2);
        FILE* csv = nullptr; FILE* spec = nullptr;
        if (evidence) {
            std::string p1 = std::string(evidence) + "/mode_splitting.csv", p2 = std::string(evidence) + "/mode_splitting_spectrum.csv";
            csv = std::fopen(p1.c_str(), "w"); spec = std::fopen(p2.c_str(), "w");
            if (csv) std::fprintf(csv, "kappa,split_hz_analytic,split_hz_measured\n");
            if (spec) std::fprintf(spec, "kappa,hz,db\n");
        }
        double worst = 0.0;
        static const float kappas[] = { 0.0125f, 0.025f, 0.05f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f };
        for (float kappa : kappas) {
            suzu::Cell c[2]; c[0].reset(); c[0].kick(0.25f, e); c[1].reset(); c[1].kick(0.0f, e);
            const float inv[2] = { 1.0f / e, 1.0f / e };
            const size_t n = (size_t)(4.0 * rate2);
            std::vector<float> sum(kappa == 0.1f ? n : 0);
            // the beat: E₀ − E₁ swings at the split; its rising zero crossings time it
            double first = -1.0, last = -1.0; long cross = 0; double prev = 1.0;
            for (size_t i = 0; i < n; i++) {
                suzu::lattice_kick(c, inv, 2, kappa * e * e);
                c[0].step(); c[1].step();
                const double d = c[0].energy() - c[1].energy();
                if (prev < 0.0 && d >= 0.0) { const double t = (double)i - d / (d - prev); if (first < 0.0) first = t; last = t; cross++; }
                prev = d;
                if (!sum.empty()) sum[i] = c[0].x + c[1].x;
            }
            const double measured = cross > 1 ? (double)(cross - 1) * rate2 / (last - first) : 0.0;
            const double s_lo = (double)e * e, s_hi = (double)e * e * (1.0 + 2.0 * kappa);
            const double f_lo = rate2 / M_PI * std::asin(std::sqrt(s_lo) / 2.0), f_hi = rate2 / M_PI * std::asin(std::sqrt(s_hi) / 2.0);
            const double analytic = f_hi - f_lo;
            const double err = std::fabs(measured / analytic - 1.0);
            if (err > worst) worst = err;
            if (csv) std::fprintf(csv, "%.4f,%.4f,%.4f\n", (double)kappa, analytic, measured);
            if (spec && !sum.empty()) for (double hz = 200.0; hz <= 260.0; hz += 0.1) std::fprintf(spec, "%.2f,%.1f,%.2f\n", (double)kappa, hz, db(goertzel(sum.data(), sum.size(), hz, rate2) + 1e-12));
        }
        if (csv) std::fclose(csv); if (spec) std::fclose(spec);
        CHECK(worst < 0.02, "mode splitting: two coupled cells at A3, κ = 0.0125 … 0.5 — the beat of the energy exchange matches the joint map's f₊ − f₋ within %.2f %% (< 2)%s", 100.0 * worst, evidence ? "; the chart's CSVs written" : "");
    }
    // the plucked string's law: at p = ½ the even partials are not fed (sin(kπ/2) = 0)
    {
        suzu::ModalTable t; suzu::modal_table(&t, 4, 8, 3.0f, 0.3f, 0.0f, 0.5f, 0.3f);
        CHECK(t.kick[1] < 1e-6f && t.kick[3] < 1e-6f && t.kick[0] == 1.0f && std::fabs(t.kick[2] - 1.0f / 9.0f) < 1e-4f,
              "the plucked string at p = ½: the even partials unfed (w2 %.1e, w4 %.1e), w3 = 1/9 (%.4f) — sin(kπp)/k²", (double)t.kick[1], (double)t.kick[3], (double)t.kick[2]);
        // the compensation's solve at the harmonic 16-chain's far end: the compensated chain's normal modes ON the ratios
        {
            suzu::ModalTable h; suzu::modal_table(&h, 0, 16, 3.0f, 0.3f, 0.0f, 0.28f, 0.3f);
            const float kappa = 1.485f; float comp[16];
            const bool ok = suzu::lattice_compensation(h.ratio, 16, kappa, comp);
            double a[256] = {0}, w[16];
            for (int k = 0; k < 16; k++) { const double r = h.ratio[k]; a[k * 16 + k] = (double)comp[k] * comp[k] * r * r + kappa * ((k > 0) + (k < 15)); if (k > 0) a[k * 16 + k - 1] = a[(k - 1) * 16 + k] = -kappa; }
            suzu::sym_eigenvalues(a, 16, w);
            double worst = 0.0; for (int k = 0; k < 16; k++) { const double r = h.ratio[k]; worst = std::fmax(worst, std::fabs(w[k] / (r * r) - 1.0)); }
            CHECK(ok && worst < 1e-6, "the compensation at κ = %.3f on the harmonic 16-chain: the compensated chain's 16 normal modes sit on the ratios within %.1e (c_0 = %.4f: the fundamental keeps %.1f %% of its own stiffness)", (double)kappa, worst, (double)comp[0], 100.0 * comp[0] * comp[0]);
        }
        // patch load's cost: the compensation table (129 κ points, Newton on a Jacobi eigen-solve) at 8 and at 16 modes — the shell's thread, once per ratio change
        {
            voxo_t* v = make(48000);
            for (uint32_t modes : { 8u, 16u }) {
                voxo_suzu_params_t sp = lattice_patch(0, modes, 0.05f); sp.stiffness = 0.001f * (float)modes;   // a new key each time: the table is rebuilt
                const auto t0 = std::chrono::steady_clock::now();
                voxo_set_suzu_params(v, &sp);
                const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
                sp.coupling = 0.3f;                                                                              // the same ratios: the live table serves
                const auto t1 = std::chrono::steady_clock::now();
                voxo_set_suzu_params(v, &sp);
                const double ms2 = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count();
                NOTE("patch load at %u modes: the table built in %.1f ms; a knob that keeps the ratios (κ here) %.2f ms", modes, ms, ms2);
            }
            voxo_destroy(v);
        }
        double eig[3]; { double a[9] = { 2, -1, 0, -1, 2, -1, 0, -1, 2 }; suzu::sym_eigenvalues(a, 3, eig); }
        CHECK(std::fabs(eig[0] - (2.0 - std::sqrt(2.0))) < 1e-9 && std::fabs(eig[1] - 2.0) < 1e-9 && std::fabs(eig[2] - (2.0 + std::sqrt(2.0))) < 1e-9,
              "the eigen-solver on the 3-chain Laplacian + 1: %.6f %.6f %.6f (2 ∓ √2, 2)", eig[0], eig[1], eig[2]);
    }

    std::printf("[suzu] step 58 — strings & chaos (SYNTH §2.3, §2.8–§2.10, §5)\n");
    // ---- 9. the Verlet chain: the CFL gate, its red control, the tuning sweep ----
    {
        const uint32_t rate = 48000;
        voxo_t* v = make_logged(rate);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
        sp.voice_kind = 2; sp.string_nodes = 48; sp.string_decay_s = 0.0f; sp.cutoff_hz = 20000.0f; sp.bow_onset_s = 0.0f; sp.release_s = 0.3f;
        sp.string_cfl = 1.05f; g_log.clear(); const bool rej = !voxo_set_suzu_params(v, &sp); const std::string msg = g_log;
        sp.string_cfl = 0.95f; const bool ok95 = voxo_set_suzu_params(v, &sp);
        CHECK(rej && ok95 && msg.find("CFL") != std::string::npos, "CFL gate: a forced k·dt² of 1.05 is rejected with \"%s\"; 0.95 admitted", msg.c_str());
        auto play = [&](voxo_suzu_params_t p) {
            voxo_t* w = make_logged(rate); voxo_set_suzu_params(w, &p); mcm(w); note_on(w, 1, 60, 100);
            std::vector<float> s = render(w, rate, 1.0); voxo_destroy(w);
            return std::make_pair(finite_all(s.data(), s.size()), peak(s.data(), s.size()));
        };
        voxo_suzu_params_t red = sp; red.string_cfl = 1.05f; red.cfl_gate = 0; auto rr = play(red);
        voxo_suzu_params_t green = sp; green.string_cfl = 0.95f; auto gr = play(green);
        CHECK(!rr.first || rr.second > 10.0f, "CFL, the RED control: k·dt² = 1.05 with the gate bypassed, C4 — the chain blows up within a second (%s, peak %.3g)", rr.first ? "finite" : "non-finite", rr.second);
        CHECK(gr.first && gr.second < 1.0f, "CFL: k·dt² = 0.95 rings bounded for a second (peak %.3f)", gr.second);
        sp.string_cfl = 0.0f; voxo_set_suzu_params(v, &sp); mcm(v);
        double worst = 0.0; int wn = 0;
        for (int note = 21; note <= 108; note += 3) {
            note_on(v, 1, note, 100);
            std::vector<float> s = render(v, rate, note < 40 ? 2.0 : 1.0);
            const double f = peak_near(s.data(), s.size(), note_hz(note), rate);
            const double c = std::fabs(cents(f, note_hz(note))); if (c > worst) { worst = c; wn = note; }
            note_off(v, 1, note); render(v, rate, 0.5);
        }
        CHECK(worst < 2.0, "the chain's tuning sweep, MIDI 21–108 (48 nodes, reduced above C6 by the CFL bound, k re-derived per note): within %.3f cent (worst at %d)", worst, wn);
        voxo_destroy(v);
    }
    // ---- 10. the hybrid string: the probe, the bound, the soak, the red soak, the tuning ----
    {
        const uint32_t rate = 48000;
        voxo_t* v = make_logged(rate);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
        sp.voice_kind = 3; sp.bow_onset_s = 0.0f; sp.cutoff_hz = 20000.0f; sp.release_s = 0.3f;
        const float bound = voxo_suzu_passive_bound(v, &sp);
        sp.bridge_gain = 1.05f; g_log.clear(); const bool rej = !voxo_set_suzu_params(v, &sp); const std::string msg = g_log;
        sp.bridge_gain = 0.95f; const bool ok_under = voxo_set_suzu_params(v, &sp);
        sp.bridge_gain = 1.0f; const bool ok1 = voxo_set_suzu_params(v, &sp);
        CHECK(rej && ok1 && bound > 0.99f && bound < 1.02f && msg.find("gains") != std::string::npos,
              "passivity gate: the load-time probe's bound on bridge_gain is %.4f (the conserving junction's 1); 1.05 rejected with \"%s\"; 1 admitted", bound, msg.c_str());
        NOTE("(bridge_gain 0.95 — under the conserving point, the junction is not conserving either: the bridge takes the full force while the string sees less come back — the probe %s it, since it does not GROW at C6; the lab's knob)", ok_under ? "admits" : "rejects");
        sp.string_decay_s = 0.0f; sp.loop_loss = 0.0f; sp.bridge_decay_s = 0.0f;                 // every declared damping zeroed
        voxo_set_suzu_params(v, &sp); mcm(v);
        note_on(v, 1, 45, 100);
        double first = 0.0, last = 0.0, lo = 1e30, hi = 0.0; bool fin = true;
        for (int sec = 0; sec < 600; sec++) {
            std::vector<float> s = render(v, rate, 1.0);
            if (!finite_all(s.data(), s.size())) { fin = false; break; }
            const double r = rms(s.data(), s.size());
            if (sec == 1) first = r;
            if (sec >= 1) { lo = std::fmin(lo, r); hi = std::fmax(hi, r); }
            last = r;
        }
        CHECK(fin && std::fabs(db(last / first)) < 0.1 && db(hi / lo) < 0.1,
              "loop passivity: the hybrid (bridge 220 Hz, c 0.002) with every declared damping zeroed sustains 10 min at A2 — RMS second 1 -> 600: %+.4f dB (< 0.1), spread %.4f dB", db(last / first), db(hi / lo));
        note_off(v, 1, 45); render(v, rate, 2.0);
        voxo_suzu_params_t red = sp; red.bridge_gain = 1.05f; red.passivity_gate = 0;
        voxo_set_suzu_params(v, &red);
        note_on(v, 1, 45, 100);
        double r1 = 0.0, rl = 0.0; bool blew = false; int at = 0;
        for (int sec = 0; sec < 60; sec++) {
            std::vector<float> s = render(v, rate, 1.0);
            if (!finite_all(s.data(), s.size())) { blew = true; at = sec; break; }
            const double r = rms(s.data(), s.size()); if (sec == 1) r1 = r; rl = r;
        }
        CHECK(blew || db(rl / r1) > 6.0, "loop passivity, the RED control: bridge_gain 1.05 with the probe bypassed — %s (%s)", blew ? "non-finite" : "grows", blew ? (std::string("at ") + std::to_string(at) + " s").c_str() : (std::string("+") + std::to_string(db(rl / r1)) + " dB in 60 s").c_str());
        voxo_destroy(v);
        v = make(rate);
        voxo_suzu_default_params(&sp); sp.voice_kind = 3; sp.bow_onset_s = 0.0f; sp.cutoff_hz = 20000.0f; sp.release_s = 0.3f;
        voxo_set_suzu_params(v, &sp); mcm(v);
        double worst = 0.0; int wn = 0;
        for (int note = 21; note <= 108; note += 3) {
            note_on(v, 1, note, 100);
            std::vector<float> s = render(v, rate, note < 40 ? 2.0 : 1.0);
            const double f = peak_near(s.data(), s.size(), note_hz(note), rate);
            const double c = std::fabs(cents(f, note_hz(note))); if (c > worst) { worst = c; wn = note; }
            note_off(v, 1, note); render(v, rate, 0.5);
        }
        CHECK(worst < 2.0, "the hybrid's tuning, MIDI 21–108 (the defaults: the bridge's phase folded into the delay): within %.3f cent (worst at %d)", worst, wn);
        voxo_destroy(v);
    }
    // ---- 11. the Duffing cell: the clang and settle, the drive ----
    {
        const uint32_t rate = 48000;
        voxo_t* v = make(rate);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
        sp.voice_kind = 4; sp.duffing_beta = 8.0f; sp.decay_s = 2.0f; sp.cutoff_hz = 20000.0f; sp.bow_onset_s = 0.0f;
        voxo_set_suzu_params(v, &sp); mcm(v);
        note_on(v, 1, 60, 127);
        std::vector<float> s = render(v, rate, 3.0);
        const double f_first = frequency(s.data(), rate / 10, rate), f_late = frequency(s.data() + (size_t)(2.5 * rate), rate / 2, rate);
        CHECK(cents(f_first, note_hz(60)) > 10.0 && std::fabs(cents(f_late, note_hz(60))) < 1.0,
              "Duffing: C4 at velocity 127 (β 8) clangs %+.1f cent sharp over its first 100 ms and settles to %+.2f cent by 2.5 s (a 2 s declared decay)", cents(f_first, note_hz(60)), cents(f_late, note_hz(60)));
        note_off(v, 1, 60); render(v, rate, 1.0);
        sp.drive = 1.0f; sp.decay_s = 0.3f; voxo_set_suzu_params(v, &sp);
        std::string line;
        for (int pr : { 0, 40, 80, 127 }) {
            voxo_push_midi(v, 0xD1, (uint8_t)pr, 0);
            note_on(v, 1, 57, 100); render(v, rate, 0.5);
            std::vector<float> t = render(v, rate, 1.0);
            const double tot = rms(t.data(), t.size()), h1 = goertzel(t.data(), t.size(), note_hz(57), rate) / std::sqrt(2.0);
            char b[48]; std::snprintf(b, sizeof b, " press %d: %.0f %%", pr, tot > 0 ? 100.0 * h1 * h1 / (tot * tot) : 0.0); line += b;
            note_off(v, 1, 57); render(v, rate, 0.5);
        }
        NOTE("Duffing, driven at the note (drive 1, β 8, a 0.3 s decay): the fundamental's share of the power as the press rises —%s (the chart is the evidence)", line.c_str());
        voxo_destroy(v);
    }
    // ---- 12. the kicked rotor ----
    {
        const double rate2 = 96000.0;
        suzu::Rotor r; r.strike(0.5f, 220.0f, (float)rate2);
        double amin = 1e30, amax = 0.0, pmax = 0.0; float pk = 0.0f;
        const size_t n = (size_t)(rate2 * 600.0);
        for (size_t i = 0; i < n; i++) {
            r.step(0.3f);
            pk = std::fmax(pk, std::fabs(r.c.x));
            pmax = std::fmax(pmax, std::fabs((double)r.p));
            if ((i % (size_t)rate2) == (size_t)rate2 - 1) { amin = std::fmin(amin, pk); amax = std::fmax(amax, pk); pk = 0.0f; }
        }
        CHECK(db(amax / amin) < 0.1 && pmax <= suzu::PI + 1e-6, "rotor drift: K = 0.3, undamped, 10 min at A3 — the orbit's peak per second within %.4f dB (< 0.1), |p| ≤ %.3f (the torus)", db(amax / amin), pmax);
        const uint32_t rate = 48000;
        voxo_t* v = make(rate);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
        sp.voice_kind = 5; sp.rotor_k = 0.0f; sp.decay_s = 30.0f; sp.cutoff_hz = 20000.0f; sp.bow_onset_s = 0.0f;
        voxo_set_suzu_params(v, &sp); mcm(v);
        note_on(v, 1, 57, 100);
        std::vector<float> s = render(v, rate, 1.0);
        const double f0 = peak_near(s.data(), s.size(), note_hz(57), rate);
        std::string line;
        for (int w : { 0, 24, 50, 76, 127 }) {                                   // K = 2.5·w/127: 0, 0.47, 0.98, 1.5, 2.5
            cc(v, 1, 1, w); render(v, rate, 0.3);
            std::vector<float> t = render(v, rate, 2.0);
            const double tot = rms(t.data(), t.size()), h1 = goertzel(t.data(), t.size(), note_hz(57), rate) / std::sqrt(2.0);
            char b[48]; std::snprintf(b, sizeof b, " K %.2f: %.0f %%", 2.5 * w / 127.0, tot > 0 ? 100.0 * h1 * h1 / (tot * tot) : 0.0); line += b;
        }
        CHECK(std::fabs(cents(f0, note_hz(57))) < 2.0, "rotor: K = 0 is the pure tone, A3 within %.3f cent; the fundamental's share of the power as the wheel sweeps K —%s", std::fabs(cents(f0, note_hz(57))), line.c_str());
        voxo_destroy(v);
    }
    // ---- 13. the chaotic modulator ----
    {
        suzu::DoublePendulum pd; pd.reset(); pd.trigger(3.0);
        const double e0 = pd.energy(); double emax = e0, emin = e0, omax = 0.0, rmin = 1.0, rmax = 1.0;
        for (long i = 0; i < 600000; i++) { const double r = pd.step(4, true); rmin = std::fmin(rmin, r); rmax = std::fmax(rmax, r); const double e = pd.energy(); emax = std::fmax(emax, e); emin = std::fmin(emin, e); omax = std::fmax(omax, std::fabs(pd.out())); }
        CHECK(omax <= 1.0 && std::fabs(emax - e0) < 1e-3 && std::fabs(emin - e0) < 1e-3 && rmax - 1.0 < 1e-3 && 1.0 - rmin < 1e-3,
              "modulator: the double pendulum from a hard kick (E %.1f, tumbling), 10 min at 1 kHz — bounded (|out| ≤ %.3f), the energy held within %.1e, the projection's rescale within %.1e of 1", e0, omax, std::fmax(emax - e0, e0 - emin), std::fmax(rmax - 1.0, 1.0 - rmin));
    }
    // ---- 14. the layered source ----
    {
        const uint32_t rate = 48000;
        double r_each[2] = { 0.0, 0.0 };
        for (int which = 0; which < 2; which++) {
            voxo_t* v = make(rate); voxo_set_source(v, which == 0 ? VOXO_SOURCE_SAMPLER : VOXO_SOURCE_SUZU);
            voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 0; voxo_set_suzu_params(v, &sp); mcm(v);
            note_on(v, 1, 69, 100); render(v, rate, 0.2); std::vector<float> s = render(v, rate, 0.2); r_each[which] = rms(s.data(), s.size()); voxo_destroy(v);
        }
        voxo_t* v = make(rate); voxo_set_source(v, VOXO_SOURCE_LAYERED);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 0; voxo_set_suzu_params(v, &sp); mcm(v);
        note_on(v, 1, 69, 100); render(v, rate, 0.2); std::vector<float> s = render(v, rate, 0.2);
        voxo_stats_t st; voxo_stats(v, &st);
        const double r_both = rms(s.data(), s.size());
        CHECK(st.source == VOXO_SOURCE_LAYERED && st.active_voices == 1 && r_both > std::fmax(r_each[0], r_each[1]) * 1.05,
              "the layered source: one note, both bodies — the sampler's sine %.4f, the cell %.4f, both %.4f RMS (one voice, source %u)", r_each[0], r_each[1], r_both, st.source);
        note_off(v, 1, 69); render(v, rate, 1.5); voxo_stats(v, &st);
        CHECK(st.active_voices == 0, "the layered voice ends when both bodies have (%u left)", st.active_voices);
        voxo_destroy(v);
    }
    // ---- 15. headroom: ten Verlet strings at 80 nodes ----
    {
        const uint32_t rate = 48000;
        voxo_t* v = make(rate, 16);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 2; sp.string_nodes = 80; sp.cutoff_hz = 2000.0f; sp.resonance = 0.3f;
        voxo_set_suzu_params(v, &sp); voxo_set_input_mode(v, 2);
        for (int i = 0; i < 10; i++) note_on(v, 0, 40 + i * 3, 100);
        std::vector<float> out(2u * BLOCK);
        double total_ms = 0.0, worst_ms = 0.0; const int blocks = (int)(rate / BLOCK);
        for (int b = 0; b < blocks; b++) {
            const auto t0 = std::chrono::steady_clock::now();
            voxo_render(v, out.data(), BLOCK);
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            total_ms += ms; if (ms > worst_ms) worst_ms = ms;
        }
        const double period_ms = 1000.0 * BLOCK / rate;
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(st.active_voices == 10 && worst_ms < period_ms, "headroom: %u Verlet strings at 80 nodes (SVF on, 2x), block %u at %u Hz — mean %.3f ms, worst %.3f ms of a %.3f ms period (%.1f %% of the callback)",
              st.active_voices, BLOCK, rate, total_ms / blocks, worst_ms, period_ms, 100.0 * worst_ms / period_ms);
        voxo_destroy(v);
    }

    std::printf("[suzu] step 58b — the bore & the jet (SYNTH §2.11, §2.13, §5)\n");
    // ---- 16. the bore's series, three geometries ------------------------------
    {
        const float rate2 = 96000.0f, hz = 440.0f;
        struct G { const char* name; int profile; int e0; int e1; bool quarter; bool odd; float apex; };
        const G gs[3] = { { "closed–open cylinder", suzu::BORE_CYLINDER, suzu::END_CLOSED, suzu::END_OPEN, true, true, 0.0f },
                          { "cone, closed at its apex", suzu::BORE_CONE, suzu::END_CLOSED, suzu::END_OPEN, false, false, 0.05f },
                          { "open–open cylinder", suzu::BORE_CYLINDER, suzu::END_OPEN, suzu::END_OPEN, false, false, 0.0f } };
        for (const G& g : gs) {
            suzu::Bore* b = new suzu::Bore;
            b->setup(200, g.profile, g.apex, 0.0f, g.e0, 0.0f, g.e1, 0.0f);
            const float lmax = suzu::Bore::lambda_bound(b->mu_max()) * 0.999f;
            const float extra = g.profile == suzu::BORE_CONE ? suzu::cone_extra(g.apex) : 0.0f;
            const int n = suzu::Bore::cells_for(hz, rate2, lmax, g.quarter, extra, 0.0f, 256);
            b->setup(n, g.profile, g.apex, 0.0f, g.e0, 0.0f, g.e1, 0.0f); b->lam = suzu::Bore::lambda_for(hz, rate2, n, g.quarter, extra, 0.0f);
            for (int i = 0; i < n; i++) { const float x = (float)i / n; b->u[i] = 0.1f * std::exp(-100.0f * (x - 0.7f) * (x - 0.7f)); }
            std::vector<float> out(192000); for (size_t i = 0; i < out.size(); i++) out[i] = b->step(0.0f, 0.0f, 1.0f);
            double worst = 0.0; std::string line;
            for (int k = 1; k <= 4; k++) {
                const double target = g.odd ? hz * (2 * k - 1) : hz * k;
                const double f = peak_near(out.data(), out.size(), target, rate2, 300.0);
                const double c = cents(f, target); worst = std::fmax(worst, std::fabs(c));
                char t[40]; std::snprintf(t, sizeof t, " %.1f:%+.1f", (double)(g.odd ? 2 * k - 1 : k), c); line += t;
            }
            CHECK(worst < 10.0, "the bore's series, %s (%d cells, λ %.4f): the first four peaks on the %s series within %.1f cent (< 10) —%s", g.name, n, (double)b->lam, g.odd ? "odd" : "integer", worst, line.c_str());
            delete b;
        }
    }
    // ---- 17. the closed lossless bore's drift; the CFL gate and its red -------
    {
        const float rate2 = 96000.0f;
        suzu::Bore* b = new suzu::Bore; b->setup(120, suzu::BORE_CYLINDER, 0.0f, 0.0f, suzu::END_CLOSED, 0.0f, suzu::END_CLOSED, 0.0f);
        const float mu = b->mu_max(); b->lam = suzu::Bore::lambda_bound(mu) * 0.999f;
        for (int i = 0; i < b->n; i++) { const float x = (float)i / b->n; b->u[i] = 0.1f * std::exp(-100.0f * (x - 0.5f) * (x - 0.5f)); }
        b->step(0.0f, 0.0f, 1.0f); const double e0 = b->energy(); double emin = e0, emax = e0;
        for (long i = 0; i < 600L * 96000; i++) { b->step(0.0f, 0.0f, 1.0f); if ((i & 4095) == 0) { const double e = b->energy(); emin = std::fmin(emin, e); emax = std::fmax(emax, e); } }
        CHECK(db(std::sqrt(emax / emin)) < 0.1, "the closed lossless bore: 120 cells at λ %.4f, 10 min — the staggered energy within %.5f dB (< 0.1; %.6g -> %.6g)", (double)b->lam, db(std::sqrt(emax / emin)), e0, b->energy());
        b->setup(120, suzu::BORE_CYLINDER, 0.0f, 0.0f, suzu::END_CLOSED, 0.0f, suzu::END_CLOSED, 0.0f); b->lam = suzu::Bore::lambda_bound(mu) * 1.05f;
        for (int i = 0; i < b->n; i++) { const float x = (float)i / b->n; b->u[i] = 0.1f * std::exp(-100.0f * (x - 0.5f) * (x - 0.5f)); }
        long at = -1; for (long i = 0; i < 96000; i++) { b->step(0.0f, 0.0f, 1.0f); const float e = b->energy(); if (!std::isfinite(e) || e > 1e6f) { at = i; break; } }
        CHECK(at >= 0, "the bore's CFL, the RED control on the primitive: λ at 1.05× the bound blows up at %ld sub-steps", at);
        delete b;
        const uint32_t rate = 48000;
        voxo_t* v = make_logged(rate);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 6;
        sp.bore_cfl = 1.05f; g_log.clear(); const bool rej = !voxo_set_suzu_params(v, &sp); const std::string msg = g_log;
        sp.bore_cfl = 0.95f; const bool ok95 = voxo_set_suzu_params(v, &sp);
        CHECK(rej && ok95 && msg.find("CFL") != std::string::npos, "the bore's CFL gate through the ABI: 1.05× rejected with \"%s\"; 0.95× admitted", msg.c_str());
        auto blow = [&](voxo_suzu_params_t p) {
            voxo_t* w = make_logged(rate); voxo_set_suzu_params(w, &p); mcm(w); cc(w, 1, 2, 70); note_on(w, 1, 72, 100);
            std::vector<float> s = render(w, rate, 1.0); voxo_destroy(w);
            return std::make_pair(finite_all(s.data(), s.size()), peak(s.data(), s.size()));
        };
        voxo_suzu_params_t red = sp; red.bore_cfl = 1.05f; red.bore_cfl_gate = 0; auto rr = blow(red);
        voxo_suzu_params_t green = sp; green.bore_cfl = 0.95f; auto gr = blow(green);
        CHECK(!rr.first || rr.second > 10.0f, "the bore's CFL, the RED control through the ABI: 1.05× with the gate bypassed, C5 blown — non-finite or over 10 within a second (%s, peak %.3g)", rr.first ? "finite" : "non-finite", rr.second);
        CHECK(gr.first && gr.second < 1.0f, "the bore at 0.95× the bound plays bounded (peak %.3f)", gr.second);
        voxo_destroy(v);
    }
    // ---- 18. the mouth-power ledger over a scripted phrase (the primitive) -----
    {
        const float rate2 = 96000.0f;
        suzu::Bore* b = new suzu::Bore; suzu::Jet* j = new suzu::Jet; j->reset();
        auto build = [&](float hz, bool fresh) {
            if (fresh) b->setup(64, suzu::BORE_CYLINDER, 0.0f, 0.0f, suzu::END_OPEN, 0.3f, suzu::END_OPEN, 0.3f);
            b->radiation_corner(1500.0f, rate2);
            const float lmax = suzu::Bore::lambda_bound(b->mu_max()) * 0.999f;
            const float ends = 2.0f * suzu::Bore::end_correction(0.3f, b->rad_a, hz, rate2);
            const int n = suzu::Bore::cells_for(hz, rate2, lmax, false, 0.0f, ends, 256);
            b->retune(n, suzu::Bore::lambda_for(hz, rate2, n, false, 0.0f, ends), suzu::BORE_CYLINDER, 0.0f, 0.0f);
        };
        build(440.0f, true);
        const float P_ref = 0.005f, leak = 1.0f - 2.0f * suzu::PI * 10.0f / rate2, wall = suzu::contraction_for(1.0f, rate2);
        double mouth = 0.0, injected = 0.0, radiated = 0.0, worst_bound = 0.0, worst_close = 0.0, limited = 0.0; float hz = 440.0f; long steps = 0;
        const long N = (long)(4.0f * rate2);
        float u0_prev = 0.0f, un_prev = 0.0f;
        for (long i = 0; i < N; i++) {
            const double t = (double)i / rate2;
            const float ratio = t < 0.5 ? 0.3f + 1.4f * (float)(t / 0.5) : t < 1.5 ? 1.7f : t < 2.0 ? 1.7f + 1.3f * (float)((t - 1.5) / 0.5) : t < 3.0 ? 3.0f : 3.0f * (float)std::exp(-(t - 3.0) / 0.2);   // the phrase's breath
            if (i == (long)(2.5f * rate2)) { hz = 523.25f; build(hz, false); }                                                          // a note change mid-phrase
            const float Pm = P_ref * ratio, U0 = std::sqrt(2.0f * Pm), T = rate2 / hz;
            const float tau = 0.5f * T * std::sqrt(P_ref / Pm), band_f = suzu::eps_for(0.5f * rate2 / tau, rate2);
            const float q = j->step(b->u[0], U0, tau, 560.0f * (hz / 440.0f), 0.0f, 0.05f * std::sqrt(440.0f / hz), 0.02f, leak, band_f, 1.0f);
            float p_src = -1.0f * (q - j->q_prev); j->q_prev = q;
            const float work = p_src * b->u[0], budget = Pm * q;                                                                           // the power-limited port, as the voice
            if (work > budget && work > 1e-20f) { p_src *= budget / work; limited += 1.0; }
            b->step(0.0f, p_src, wall);
            mouth += (double)Pm * q;                                                                                                       // the mouth's work: P_mouth·Q_in
            const double u0c = 0.5 * ((double)b->u[0] + u0_prev), unc = 0.5 * ((double)b->u[b->n - 1] + un_prev);                          // the velocities centred on p's time level
            injected += (double)p_src * u0c;                                                                                              // the port's work on the bore
            radiated += (double)b->z0 * (b->u[0] - b->lp0) * u0c + (double)b->z1 * (b->u[b->n - 1] - b->lp1) * unc;                       // the ends' declared loss
            u0_prev = b->u[0]; un_prev = b->u[b->n - 1]; steps++;
            if ((i & 63) == 0 && i > 4096) {
                const double E = b->energy();
                worst_bound = std::fmax(worst_bound, E / (mouth > 1e-12 ? mouth : 1e-12));
                worst_close = std::fmax(worst_close, std::fabs(E - (injected - radiated)) / (injected > 1e-9 ? injected : 1e-9));
            }
        }
        CHECK(worst_bound < 1.01,
              "the mouth-power ledger, a 4 s phrase (a swell, a note change A4 -> C5, a release): the stored energy never exceeds the mouth's work — at worst %.3g of ∫P_mouth·Q_in (< 1.01); the port's limiter acted on %.2f %% of the samples; at the end E %.3g, injected %.3g, radiated %.3g, the mouth's %.3g",
              worst_bound, 100.0 * limited / (double)steps, (double)b->energy(), injected, radiated, mouth);
        (void)worst_close;
        NOTE("the bore's own arithmetic on that phrase does not close with the centred products (injected %.4f against radiated %.4f + stored %.2g): the ports' discrete power at the half step is an [ITERATE]; the exact conservation is gate 17's, on the closed bore", injected, radiated, (double)b->energy());
        delete b; delete j;
    }
    // ---- 19. the overblow through the ABI; soft blowing flattens --------------
    {
        const uint32_t rate = 48000;
        voxo_t* v = make(rate);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 6; sp.jet_noise = 0.0f;
        voxo_set_suzu_params(v, &sp); mcm(v);
        note_on(v, 1, 69, 100);
        const double seconds = 12.0; const size_t total = (size_t)(seconds * rate);
        std::vector<float> out(2u * total, 0.0f);
        int last = -1;
        for (size_t f = 0; f < total; f += BLOCK) {
            const int val = (int)(127.0 * (double)f / (double)total + 0.5);
            if (val != last) { last = val; cc(v, 1, 2, val); }
            voxo_render(v, out.data() + 2u * f, (uint32_t)((total - f) < BLOCK ? (total - f) : BLOCK));
        }
        std::vector<float> mono(total); for (size_t i = 0; i < total; i++) mono[i] = out[2u * i];
        auto fundamental = [&](size_t at, size_t n) {
            double best = -1.0, best_hz = 0.0; std::vector<double> f, m;
            for (double hz = 150.0; hz < 2500.0; hz *= std::pow(2.0, 4.0 / 1200.0)) { f.push_back(hz); m.push_back(goertzel(mono.data() + at, n, hz, rate)); }
            for (size_t i = 0; i < m.size(); i++) if (m[i] > best) { best = m[i]; best_hz = f[i]; }
            const double thr = best * std::pow(10.0, -15.0 / 20.0);
            for (size_t i = 1; i + 1 < m.size(); i++) if (m[i] > thr && m[i] >= m[i - 1] && m[i] >= m[i + 1]) return f[i];
            return best_hz;
        };
        std::string line; double f_mid = 0.0, f_end = 0.0, f_soft = 0.0;
        for (int w = 0; w < 12; w++) {
            const size_t at = (size_t)w * rate; const double pk = peak(mono.data() + at, rate);
            const double fu = pk > 1e-3 ? fundamental(at, rate) : 0.0;
            char t[40]; std::snprintf(t, sizeof t, " %d:%s", w, fu > 0 ? (std::to_string((int)std::lround(cents(fu, note_hz(69)))) + "c").c_str() : "-"); line += t;
            if (w == 5) f_mid = fu; if (w == 3) f_soft = fu; if (w == 11) f_end = fu;
        }
        CHECK(f_end > 0 && std::fabs(cents(f_end, 2.0 * note_hz(69))) < 150.0 && f_mid > 0 && std::fabs(cents(f_mid, note_hz(69))) < 60.0,
              "the overblow: A4, the breath ramped 0 -> 127 over 12 s, no other change — the tone sits on the note mid-ramp (%+.0f cent at 5 s) and jumps the octave (%+.0f cent of 2f₀ in the last second); per second:%s", cents(f_mid, note_hz(69)), cents(f_end, 2.0 * note_hz(69)), line.c_str());
        CHECK(f_soft > 0 && f_mid > 0 && cents(f_soft, f_mid) < -15.0, "soft blowing flattens: at 3 s (breath 32/127) the tone is %+.0f cent under its 5 s (breath 53/127) pitch (< −15) — the jet's phase lag, for free", cents(f_soft, f_mid));
        voxo_destroy(v);
        // every note from C2 to C7 at the reference breath sounds, bounded (F#4 once blew up on a Courant number of 1.00026)
        v = make(rate); voxo_set_suzu_params(v, &sp); mcm(v);
        int silent = 0, blown = 0; std::string bad;
        for (int note = 36; note <= 96; note++) {
            cc(v, 1, 2, 56); note_on(v, 1, note, 100);
            std::vector<float> s = render(v, rate, 0.6);
            const double pk = peak(s.data() + (size_t)(0.3 * rate), (size_t)(0.3 * rate));
            if (!finite_all(s.data(), s.size()) || pk > 0.9) { blown++; bad += " " + std::to_string(note) + "!"; }
            else if (pk < 3e-4) { silent++; bad += " " + std::to_string(note); }
            note_off(v, 1, note); render(v, rate, 0.4);
        }
        CHECK(silent == 0 && blown == 0, "the flute across the keyboard, C2–C7 at breath 56/127: every note sounds and stays bounded (%d silent, %d blown%s)", silent, blown, bad.empty() ? "" : (std::string(":") + bad).c_str());
        voxo_destroy(v);
        // the press blows: a controller without a breath CC (channel pressure alone) plays the flute; with press_blows off it does not
        double pk_press[2] = { 0.0, 0.0 };
        for (int on = 1; on >= 0; on--) {
            v = make(rate); sp.press_blows = (uint32_t)on; voxo_set_suzu_params(v, &sp); mcm(v);
            note_on(v, 1, 69, 100); voxo_push_midi(v, 0xD1, 70, 0);            // channel pressure 70/127, no CC 2
            std::vector<float> s = render(v, rate, 1.0);
            pk_press[on] = peak(s.data() + (size_t)(0.5 * rate), (size_t)(0.5 * rate));
            voxo_destroy(v);
        }
        CHECK(pk_press[1] > 1e-3 && pk_press[0] < 1e-5, "the press blows: A4 under channel pressure 70/127 with no breath CC sounds (peak %.3g); with press_blows off it stays silent (%.1e) — no breath, no tone", pk_press[1], pk_press[0]);
    }

    std::printf("[suzu] step 58c — the reed & the lips (SYNTH §2.12, §2.11, §5)\n");
    // ---- 20. the mouth-power ledger on the primitives; the naive junction red ----
    {
        const float rate2 = 96000.0f, P_ref = 0.005f;
        float peaks[8]; const int np = suzu::bore_peaks(suzu::BORE_TRUMPET, 0.6f, 0.7f, 0.3f, 1500.0f, rate2, 8, peaks);
        auto build = [&](suzu::Bore& b, suzu::Valve& vv, int kind, float hz, bool fresh) {
            const bool sax = kind == 7; const int profile = sax ? suzu::BORE_CONE : suzu::BORE_TRUMPET; const float apex = sax ? 0.25f : 0.6f, gamma = sax ? 0.0f : 0.7f;
            const float corner = sax ? 1500.0f * hz / 220.0f : 1500.0f;                                   // the sax's bell scales with the note
            if (fresh) { b.setup(64, profile, apex, gamma, suzu::END_CLOSED, 0.0f, suzu::END_OPEN, 0.3f); vv.reset(); }
            b.radiation_corner(corner, rate2);
            const float lmax = suzu::Bore::lambda_bound(b.mu_max()) * 0.99f, s_end = suzu::Bore::profile_s(profile, 1.0f, apex, gamma);
            const float ends = suzu::Bore::end_correction(0.3f, b.rad_a, hz, rate2, s_end);
            int cells; float lam;
            if (sax) { cells = suzu::Bore::cells_for(hz, rate2, lmax, false, suzu::cone_extra(apex), ends, 128); lam = suzu::Bore::lambda_for(hz, rate2, cells, false, suzu::cone_extra(apex), ends); }
            else { const float fq = hz / (np >= 3 ? peaks[2] : 5.0f); cells = suzu::Bore::cells_for(fq, rate2, lmax, true, 0.0f, ends, 128); lam = suzu::Bore::lambda_for(fq, rate2, cells, true, 0.0f, ends); }
            if (fresh) b.setup(cells, profile, apex, gamma, suzu::END_CLOSED, 0.0f, suzu::END_OPEN, 0.3f);
            b.retune(cells, lam, profile, apex, gamma); b.radiation_corner(corner, rate2);
            if (sax) vv.setup(12000.0f, rate2, 0.7f, 0.5f, 3.0f * P_ref, 0.14f, 0, 2000.0f); else vv.setup(hz * 0.9f, rate2, 3.0f, 0.05f, P_ref, 0.5f, 1, 0.0f);
        };
        auto phrase = [&](int kind, bool naive, double* worst_out, bool* finite_out) {
            suzu::Bore* b = new suzu::Bore; suzu::Valve vv; build(*b, vv, kind, 220.0f, true);
            const float wall = suzu::contraction_for(1.0f, rate2);
            double mouth = 0.0, worst = 0.0; bool fin = true; const long N = (long)(3.0f * rate2);
            for (long i = 0; i < N; i++) {
                const double t = (double)i / rate2;
                const float r = t < 0.5 ? 0.5f + 2.0f * (float)(t / 0.5) : t < 1.5 ? 2.5f : t < 2.5 ? 1.8f : 1.8f * (float)std::exp(-(t - 2.5) / 0.15);
                if (i == (long)(1.5f * rate2)) build(*b, vv, kind, 293.66f, false);                       // a note change mid-phrase
                const float Pm = P_ref * r, a = vv.A * vv.opening(), swept = vv.swept(), Z = b->end_impedance();
                const float q = naive ? suzu::Valve::flow_naive(a, Pm - b->p[0]) : suzu::Valve::flow_implicit(a, Pm - b->end_pressure_ahead() - Z * swept, Z);
                b->step(q + swept, 0.0f, wall, 0.0f);
                vv.step(Pm - b->p[0]);
                mouth += (double)Pm * (q + swept);
                if ((i & 63) == 0 && i > 2048) { const double E = (double)b->energy() + (double)vv.energy(); if (!std::isfinite(E)) { fin = false; break; } worst = std::fmax(worst, E / (mouth > 1e-12 ? mouth : 1e-12)); }
            }
            delete b; *worst_out = worst; *finite_out = fin;
        };
        for (int kind = 7; kind <= 8; kind++) {
            double w_impl, w_naive; bool f_impl, f_naive;
            phrase(kind, false, &w_impl, &f_impl); phrase(kind, true, &w_naive, &f_naive);
            CHECK(f_impl && w_impl < 1.01, "the mouth-power ledger, the %s on a 3 s phrase (a swell to 2.5 references, a note change A3 -> D4, a release): bore + valve energy at worst %.3g of ∫P_mouth·Q (< 1.01) — the implicit junction", kind == 7 ? "reed" : "lips", w_impl);
            if (kind == 7) CHECK(!f_naive || w_naive > 1.01, "the reed's junction, the RED control: the naive explicit form on the same phrase %s (worst %.3g of the mouth's work)", f_naive ? "exceeds the mouth's work" : "goes non-finite", w_naive);
            else NOTE("the lips' naive form on the same phrase: %s (worst %.3g) — at the lips' small aperture its local gain Z·A·h/(2√Δp) stays under 1; the reed's is the red", f_naive ? "finite" : "non-finite", w_naive);
        }
    }
    // ---- 21. the valve gate through the ABI ------------------------------------
    {
        const uint32_t rate = 48000;
        voxo_t* v = make_logged(rate);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 7;
        sp.valve_naive = 1; g_log.clear(); const bool rej = !voxo_set_suzu_params(v, &sp); const std::string msg = g_log;
        sp.valve_naive = 0; const bool ok = voxo_set_suzu_params(v, &sp);
        CHECK(rej && ok && msg.find("makes energy") != std::string::npos, "the valve gate: the naive junction is rejected with \"%s\"; the implicit admitted", msg.c_str());
        auto blow = [&](voxo_suzu_params_t p) {
            voxo_t* w = make_logged(rate); voxo_set_suzu_params(w, &p); mcm(w); cc(w, 1, 2, 70); note_on(w, 1, 57, 100);
            std::vector<float> s = render(w, rate, 0.3);
            bend14(w, 1, 8192 + 4 * 8191 / 48);                                                                     // the transient: a bend of four semitones, the bore retuned with its state kept
            std::vector<float> s2 = render(w, rate, 0.7); s.insert(s.end(), s2.begin(), s2.end()); voxo_destroy(w);
            return std::make_pair(finite_all(s.data(), s.size()), peak(s.data(), s.size()));
        };
        voxo_suzu_params_t red = sp; red.valve_naive = 1; red.valve_gate = 0; red.bore_loss = 0.0f; auto rr = blow(red);
        CHECK(!rr.first || rr.second > 10.0f, "the valve gate, the RED control: the naive junction with the gate bypassed on a lossless bore, A3 blown and bent up a fourth (the transient) — %s within a second (peak %.3g)", rr.first ? "over 10" : "non-finite", rr.second);
        voxo_destroy(v);
    }
    // ---- 22. the sax: the cone's peaks; every note on the note -----------------
    {
        const float rate2 = 96000.0f;
        float r[6]; const int n = suzu::bore_peaks(suzu::BORE_CONE, 0.25f, 0.0f, 0.3f, 1500.0f, rate2, 6, r);
        double worst = 0.0; std::string line;
        for (int k = 1; k < n && k < 4; k++) { const double c = cents(r[k] / r[0], (double)(k + 1)); worst = std::fmax(worst, std::fabs(c)); char t[32]; std::snprintf(t, sizeof t, " %d:%+.1f", k + 1, c); line += t; }
        NOTE("the sax bore's peaks with the radiation port (the cone closed at its apex, apex 0.25, its mouthpiece): the first four within %.1f cent of the integers —%s (the port's reactance is an end correction that shrinks with frequency; the conical result itself is gate 16's, 0.1 cent on the pinned cone)", worst, line.c_str());
        CHECK(n >= 4, "the sax bore's peaks: %d found (the radiation port on)", n);
        const uint32_t rate = 48000;
        voxo_t* v = make(rate);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 7;
        voxo_set_suzu_params(v, &sp); mcm(v);
        int silent = 0; double worst_c = 0.0; int worst_n = 0; std::string bad, notes;
        for (int note = 48; note <= 84; note += 3) {
            cc(v, 1, 2, 70); note_on(v, 1, note, 100);
            std::vector<float> s = render(v, rate, 1.0);
            const size_t at = (size_t)(0.5 * rate), len = (size_t)(0.5 * rate);
            const double pk = peak(s.data() + at, len);
            if (pk < 1e-3 || !finite_all(s.data(), s.size())) { silent++; bad += " " + std::to_string(note); }
            else {
                const double fu = period_hz(s.data() + at, len, rate);                              // the period (the fundamental may sit under its harmonics)
                const double c = cents(fu, note_hz(note)); if (std::fabs(c) > worst_c) { worst_c = std::fabs(c); worst_n = note; }
                char t[32]; std::snprintf(t, sizeof t, " %d:%+.0f", note, c); notes += t;
            }
            note_off(v, 1, note); render(v, rate, 0.5);
        }
        CHECK(silent == 0 && worst_c < 40.0, "the sax across C3–C6 at breath 70/127: every note sounds (%d silent%s) and sits within %.1f cent of the note (worst at %d; the reed's pull calibrated at C3, C4, C5 and C6 and lerped between) — note:cents%s", silent, bad.c_str(), worst_c, worst_n, notes.c_str());
        voxo_destroy(v);
    }
    // ---- 23. the trumpet: on the note at centre, the registers at the ends ------
    {
        const uint32_t rate = 48000;
        voxo_t* v = make(rate);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 8;
        voxo_set_suzu_params(v, &sp); mcm(v);
        double worst_c = 0.0; int worst_n = 0, silent = 0, down = 0, up = 0; std::string log;
        for (int note : { 48, 57, 64, 72 }) {
            for (int c74 : { 0, 32, 64, 96, 127 }) {
                cc(v, 1, 74, c74); cc(v, 1, 2, 70); note_on(v, 1, note, 100);
                std::vector<float> s = render(v, rate, 1.0);
                const size_t at = (size_t)(0.5 * rate), len = (size_t)(0.5 * rate);
                const double pk = peak(s.data() + at, len);
                double ce = 0.0;
                if (pk < 1e-3 || !finite_all(s.data(), s.size())) silent++;
                else {
                    const double fu = period_hz(s.data() + at, len, rate);                         // the period
                    ce = cents(fu, note_hz(note));
                    if (c74 == 64 && std::fabs(ce) > worst_c) { worst_c = std::fabs(ce); worst_n = note; }
                    if (c74 == 0 && ce < -500.0) down++;
                    if (c74 == 127 && ce > 500.0) up++;
                }
                char t[40]; std::snprintf(t, sizeof t, " %d/%d:%+.0f", note, c74, ce); log += t;
                note_off(v, 1, note); cc(v, 1, 74, 64); render(v, rate, 0.5);
            }
        }
        CHECK(silent == 0 && worst_c < 30.0 && down == 4 && up == 4,
              "the trumpet across C3–C5 at breath 70/127: on the note at CC 74 centre within %.1f cent (worst at %d), a register below at CC 74 = 0 (%d of 4) and above at 127 (%d of 4), %d silent — the byte log, note/cc74:cents:%s",
              worst_c, worst_n, down, up, silent, log.c_str());
        voxo_destroy(v);
    }

    std::printf("[suzu] step 59 — the orbit trace (SYNTH §2.7, §5)\n");
    // ---- 24. the trace: a cell's orbit is a circle; the toggle is clean; ten voices fit the segment cap ----
    {
        const uint32_t rate = 48000;
        auto play = [&](uint32_t mask, int kind, int voices, double seconds, std::vector<voxo_trace_t>* traces, uint32_t max_segments) {
            voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = (uint32_t)kind; voxo_set_suzu_params(v, &sp); mcm(v);
            voxo_set_trace(v, mask);
            for (int i = 0; i < voices; i++) { const int ch = 1 + (i % 15); if (kind >= 6 || kind == 1) cc(v, ch, 2, 80); note_on(v, ch, 48 + 3 * i, 100); }
            std::vector<float> s = render(v, rate, seconds);
            if (traces) { traces->resize(16); const uint32_t n = voxo_trace_poll(v, traces->data(), 16, max_segments); traces->resize(n); }
            voxo_destroy(v); return s;
        };
        // (a) the single cell (a rotation): the polled polyline sits on the unit circle within 5 % and closes at least a full turn's worth of points
        std::vector<voxo_trace_t> tr; play(1u << 0, 0, 1, 0.2, &tr, 8);
        double worst_r = 0.0; uint32_t pts = tr.empty() ? 0 : tr[0].count; bool circle = !tr.empty() && tr[0].count >= 3;
        if (circle) for (uint32_t i = 0; i < tr[0].count; i++) { const double r = std::sqrt((double)tr[0].x[i] * tr[0].x[i] + (double)tr[0].y[i] * tr[0].y[i]); worst_r = std::fmax(worst_r, std::fabs(r - 1.0)); }
        CHECK(circle && worst_r < 0.05 && pts <= 9, "the orbit trace of the single cell (A3 held, 200 ms, 8 segments asked): %u voice(s) polled, %u points, all on the unit circle within %.1f %% (the magic circle's orbit is a circle; the amplitude reported %.3g)", (unsigned)tr.size(), pts, 100.0 * worst_r, tr.empty() ? 0.0 : tr[0].amplitude);
        // (b) the mask off polls nothing; a kind not in the mask polls nothing
        std::vector<voxo_trace_t> t0; play(0u, 0, 1, 0.1, &t0, 8); std::vector<voxo_trace_t> t5; play(1u << 5, 0, 1, 0.1, &t5, 8);
        CHECK(t0.empty() && t5.empty(), "the trace mask: off polls nothing (%u); a mask without the playing kind polls nothing (%u)", (unsigned)t0.size(), (unsigned)t5.size());
        // (c) the toggle is clean at the source: the rendering with the trace on is bit-identical to the rendering without it, for the cell and for the rotor
        for (int kind : { 0, 5, 3, 7 }) {
            std::vector<float> a = play(0u, kind, 3, 0.3, nullptr, 8), b = play(1u << kind, kind, 3, 0.3, nullptr, 8);
            const bool same = a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
            CHECK(same, "the trace is bit-identical at the source for voice kind %d: %zu samples rendered with the trace on equal the untraced rendering (the trace only reads)", kind, a.size());
        }
        // (d) ten rotor voices polled at 4 segments: every voice answers, none over the cap, the polylines finite
        std::vector<voxo_trace_t> t10; play(1u << 5, 5, 10, 0.1, &t10, 4);
        uint32_t over = 0, empty = 0; bool fin = true;
        for (const voxo_trace_t& t : t10) { if (t.count > 5) over++; if (t.count < 2) empty++; for (uint32_t i = 0; i < t.count; i++) if (!std::isfinite(t.x[i]) || !std::isfinite(t.y[i])) fin = false; }
        CHECK(t10.size() == 10 && over == 0 && empty == 0 && fin, "ten rotor voices polled at 4 segments: %u voices answer, %u over the cap, %u without a polyline, finite %s", (unsigned)t10.size(), over, empty, fin ? "yes" : "NO");
        // (e) the chains trace their phase plane: a hybrid string's polyline is finite and its amplitude is the note's
        std::vector<voxo_trace_t> t3; play(1u << 3, 3, 1, 0.2, &t3, 8);
        CHECK(!t3.empty() && t3[0].count >= 3 && t3[0].amplitude > 1e-4f, "the hybrid string traces its phase plane (s, ṡ/ω): %u points, amplitude %.3g", t3.empty() ? 0u : t3[0].count, t3.empty() ? 0.0f : t3[0].amplitude);
    }

    std::printf("[suzu] step 59b — the inspection (SYNTH §6: the lab's state)\n");
    // ---- 25. the inspection: the flute's bore and jet, the lattice's modes; reading never changes the sound ----
    {
        const uint32_t rate = 48000;
        auto play = [&](int kind, bool inspect, std::vector<voxo_inspect_t>* last) {
            voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = (uint32_t)kind; voxo_set_suzu_params(v, &sp); mcm(v);
            cc(v, 1, 2, 70); note_on(v, 1, 69, 100);
            std::vector<float> all, buf(2 * 128); static voxo_inspect_t ins[4];
            for (int b = 0; b < 150; b++) {
                voxo_render(v, buf.data(), 128); all.insert(all.end(), buf.begin(), buf.end());
                if (inspect) { const uint32_t n = voxo_suzu_inspect(v, ins, 4); if (last) last->assign(ins, ins + n); }
            }
            voxo_destroy(v); return all;
        };
        std::vector<voxo_inspect_t> fl; const std::vector<float> a = play(6, false, nullptr), b = play(6, true, &fl);
        const bool same = a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
        CHECK(same, "the inspection only reads: the flute rendered while inspected after every block equals the uninspected rendering (%zu samples)", a.size());
        bool ok = fl.size() == 1; double pmax = 0.0, jmax = 0.0; bool fin = true, cyl = true;
        if (ok) {
            const voxo_inspect_t& t = fl[0];
            ok = t.voice_kind == 6 && t.note == 69 && t.n >= 10 && t.n <= VOXO_INSPECT_MAX && t.k[1] > 0.0f && t.k[3] > 1.0f && t.rate2 == 96000.0f;
            for (uint32_t i = 0; i < t.n; i++) { if (!std::isfinite(t.a[i]) || !std::isfinite(t.s[i])) fin = false; pmax = std::fmax(pmax, std::fabs(t.a[i])); if (std::fabs(t.s[i] - 1.0f) > 1e-6f) cyl = false; }
            for (int i = 0; i < 64; i++) { if (!std::isfinite(t.c[i])) fin = false; jmax = std::fmax(jmax, std::fabs(t.c[i])); }
        }
        CHECK(ok && fin && cyl && pmax > 0.0 && jmax > 0.0, "the flute's inspection (A4, breath 70): %u voice, %u pressure nodes, the bore a cylinder (S = 1 throughout: %s), |p| up to %.3g, the jet's 64 points up to %.3g, P_mouth %.4g, τ %.1f sub-steps, finite %s",
              (unsigned)fl.size(), fl.empty() ? 0u : fl[0].n, cyl ? "yes" : "NO", pmax, jmax, fl.empty() ? 0.0f : fl[0].k[1], fl.empty() ? 0.0f : fl[0].k[3], fin ? "yes" : "NO");
        std::vector<voxo_inspect_t> la; play(1, true, &la);
        bool lok = la.size() == 1 && la[0].voice_kind == 1 && la[0].n >= 2 && la[0].s[0] > 0.0f;
        CHECK(lok, "the lattice's inspection: %u modes, the first ratio %.3g", la.empty() ? 0u : la[0].n, la.empty() ? 0.0f : la[0].s[0]);
    }

    std::printf("[suzu] step 59c — the recent trace and the ledger (the lab's panels)\n");
    // ---- 26. the recent trace at full density; the rotor's torus; the winds' ledger; reading changes nothing ----
    {
        const uint32_t rate = 48000;
        struct Run { std::vector<float> audio; std::vector<float> recent; std::vector<voxo_inspect_t> ins; double worst_ledger = 0.0; double last_work = 0.0; };
        auto play = [&](int kind, bool read, uint32_t decim, int extra_cc) {
            Run r; voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = (uint32_t)kind;
            if (kind == 0) sp.decay_s = 0.0f;                                   // the undamped cell: its orbit is conserved
            voxo_set_suzu_params(v, &sp); mcm(v);
            if (read) { voxo_set_trace(v, 1u << kind); voxo_set_trace_decimation(v, decim); }
            if (extra_cc >= 0) cc(v, 0, 1, extra_cc);
            cc(v, 1, 2, 70); note_on(v, 1, 57, 100);
            std::vector<float> buf(2 * 128); static voxo_inspect_t ins[2]; std::vector<float> pts(4 * 1023);
            for (int b = 0; b < 375; b++) {
                voxo_render(v, buf.data(), 128); r.audio.insert(r.audio.end(), buf.begin(), buf.end());
                if (read) {
                    const uint32_t n = voxo_trace_recent(v, 0, pts.data(), 1023); r.recent.assign(pts.begin(), pts.begin() + 4 * n);
                    const uint32_t ni = voxo_suzu_inspect(v, ins, 2); r.ins.assign(ins, ins + ni);
                    if (ni && (kind == 7 || kind == 8) && ins[0].k[8] > 1e-12f) { r.worst_ledger = std::fmax(r.worst_ledger, (double)ins[0].k[9] / ins[0].k[8]); r.last_work = ins[0].k[8]; }
                }
            }
            voxo_destroy(v); return r;
        };
        // (a) the cell at density 1: the recent points keep its conserved form x² + y² − εxy within 0.1 %
        Run c = play(0, true, 1, -1);
        double emin = 1e30, emax = 0.0; const float eps = c.ins.empty() ? 0.0f : c.ins[0].k[0];
        for (size_t i = 0; i + 3 < c.recent.size(); i += 4) { const double x = c.recent[i], y = c.recent[i + 1], e = x * x + y * y - eps * x * y; emin = std::fmin(emin, e); emax = std::fmax(emax, e); }
        CHECK(c.recent.size() / 4 >= 1000 && emax > 0.0 && (emax - emin) / emax < 1e-3, "the recent trace at full density: %zu points of the undamped cell (A3), its conserved form x² + y² − εxy within %.2g of itself — the orbit itself, undecimated", c.recent.size() / 4, emax > 0.0 ? (emax - emin) / emax : 1.0);
        // (b) the rotor's momentum on its torus, and K on the aux channel; the rendering with all of it read is bit-identical
        Run r0 = play(5, false, 8, 100), r1 = play(5, true, 2, 100);
        bool torus = !r1.recent.empty(); float kmin = 1e9f, kmax = -1e9f; int jumps = 0;
        for (size_t i = 2; i + 1 < r1.recent.size(); i += 4) { const float p = r1.recent[i]; if (!(p > -3.1416f && p <= 3.1416f)) torus = false; if (i >= 6 && r1.recent[i] != r1.recent[i - 4]) jumps++; kmin = std::fmin(kmin, r1.recent[i + 1]); kmax = std::fmax(kmax, r1.recent[i + 1]); }
        const bool same = r0.audio.size() == r1.audio.size() && std::memcmp(r0.audio.data(), r1.audio.data(), r0.audio.size() * sizeof(float)) == 0;
        const int expected = (int)(r1.recent.size() / 4 * 2 / (96000.0 / note_hz(57)));   // the window (points × density sub-steps) over the kick period
        CHECK(torus && jumps >= expected - 1 && jumps <= expected + 1 && kmax > 1.0f && same, "the rotor's aux channels: the momentum in (−π, π] at every point, %d kicks in the last %zu points (one a period: %d expected), K %.2f … %.2f (the wheel at 100/127); the rendering read at density 2 every block is bit-identical to the unread one: %s",
              jumps, r1.recent.size() / 4, expected, kmin, kmax, same ? "yes" : "NO");
        // (c) the winds' ledger: the energy held never exceeds the mouth's work (SYNTH §5), read every block
        for (int kind : { 7, 8 }) {
            Run w = play(kind, true, 8, -1);
            CHECK(w.last_work > 0.0 && w.worst_ledger <= 1.0, "the %s's ledger read every block for a second: the mouth's work %.3g, the energy held at worst %.3g of it (≤ 1: the losses take the rest)", kind == 7 ? "sax" : "trumpet", w.last_work, w.worst_ledger);
        }
    }

    std::printf("[suzu] %s (%d failures)\n", g_fail ? "FAILED" : "all gates green", g_fail);
    return g_fail ? 1 : 0;
}
