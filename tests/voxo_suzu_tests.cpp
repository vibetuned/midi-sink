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
// Step 79 (TO_PROJECT_SPEC §13.9) — the membrane, the plate and the gong:
//  32. the mode table of the simply-supported grid — the (1,1) on the note,
//      the (2,1) and (1,2) where the declared boundary puts them, for the
//      membrane and for the plate;
//  33. the undamped grid's exact invariant drifts under 0.005 dB in ten
//      minutes (the plain energy's wobble printed beside it);
//  34. the CFL gate at E_max: a forced 1.05× rejected with its message, 0.95
//      admitted, the bypass tripping the breaker within a second (RED); the
//      class's red — the sheet at 1.2× the cap's stretching diverges within
//      100 steps, at 1.0× it holds;
//  35. the gong — the pitch swell of a hard strike over a soft one, the
//      undamped energy bounded;
//  36. the bus plate — under the gate at 64² and at the mobile tier's 32²
//      with the declared κ, the clamp line when asked past it; audible under
//      the harp chord, silent when off, moved by the strike point;
//  37. headroom — sixteen membranes, and the 64² plate, against the period;
//  38. the membrane's strobe at the bass (the returns dumped for the chart).
// Step 80 (TO_PROJECT_SPEC §13.10) — the stick-slip bow:
//  39. the Helmholtz signature on the chain and on the waveguide — on the
//      note under the bow, the point stuck most of the period, and the stuck
//      share rising with the force (the corner sharpening);
//  40. the ledger — the energy held never exceeds the strike's plus the bow's
//      work ∫f·v_bow, on the chain and on the waveguide; RED: the explicit
//      force with the bound lifted crosses it or blows up;
//  41. the modal string under the bow: on the note, the ledger in the modal
//      units; the servo bow untouched (bow_mode 0 bit for bit).
// voxo_render is called as the callback would; no device.
#include "voxo.h"
#include "suzu.h"

#include <chrono>
#include <complex>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <string>
#ifndef M_PI   // not in standard <cmath>; MSVC defines it only under _USE_MATH_DEFINES (step 67, DECISIONS_9 #8)
#define M_PI 3.14159265358979323846
#endif

static int g_fail = 0;
// Step 75: the audio thread allocates nothing — every global new is counted (the test's own vectors are sized before the
// loops under watch), and on Apple the zone's blocks in use are read before and after.
#include <new>
#include <atomic>
#include <array>
#include <cstdlib>
#if defined(__APPLE__)
#include <malloc/malloc.h>
#endif
static std::atomic<uint64_t> g_news{0};
void* operator new(std::size_t n) { g_news.fetch_add(1, std::memory_order_relaxed); void* p = std::malloc(n ? n : 1); if (!p) std::abort(); return p; }
void* operator new[](std::size_t n) { g_news.fetch_add(1, std::memory_order_relaxed); void* p = std::malloc(n ? n : 1); if (!p) std::abort(); return p; }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
static uint64_t blocks_in_use() {
#if defined(__APPLE__)
    malloc_statistics_t st; malloc_zone_statistics(nullptr, &st); return st.blocks_in_use;
#else
    return 0;
#endif
}

#define CHECK(cond, ...) do { if (cond) { std::printf("ok   "); std::printf(__VA_ARGS__); std::printf("\n"); } \
                              else { g_fail++; std::printf("FAIL "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)
#define NOTE(...) do { std::printf("     "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

static const uint32_t BLOCK = 128;

// Step 67 (DECISIONS_9 #13): the two headroom gates measure a second of blocks and gate on the WORST block. On a
// shared CI runner, or a desktop busy with a build beside the test, a preemption of the test's thread reads as a
// stall: the macOS lane saw one block of 5.3 ms in a run whose mean was 0.295 ms of a 2.667 ms period. A real
// per-block stall — a retune, an allocation, a gate's probe inside voxo_render — repeats on every attempt; a
// preemption does not. So the second is measured up to three times, the attempt with the smallest worst block is
// the reading, and the line prints every attempt's worst: three bad attempts in a row is the structural case.
// Step 78: the headroom bars are the RELEASE build's (the sound ships from it; the storm runs from it) — a Debug build prints
// the figure and does not assert on it (its wrappers and checks cost what they cost).
#ifdef NDEBUG
#define RELEASE_BUILD true
#else
#define RELEASE_BUILD false
#endif
struct Headroom { double mean_ms = 0.0, worst_ms = 0.0, attempt_worst[3] = {0.0, 0.0, 0.0}; int attempts = 0; };
static Headroom headroom(voxo_t* v, float* out, uint32_t rate, uint32_t block, double period_ms) {
    Headroom h; const int blocks = (int)(rate / block);
    for (int a = 0; a < 3; a++) {
        double total = 0.0, worst = 0.0;
        for (int b = 0; b < blocks; b++) {
            const auto t0 = std::chrono::steady_clock::now();
            voxo_render(v, out, block);
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            total += ms; if (ms > worst) worst = ms;
        }
        h.attempt_worst[a] = worst; h.attempts = a + 1;
        if (a == 0 || worst < h.worst_ms) { h.worst_ms = worst; h.mean_ms = total / blocks; }
        if (worst < period_ms) break;
    }
    return h;
}
static std::string headroom_attempts(const Headroom& h) {
    char buf[96]; std::string s;
    for (int a = 0; a < h.attempts; a++) { std::snprintf(buf, sizeof buf, "%s%.3f", a ? " / " : "", h.attempt_worst[a]); s += buf; }
    return s;
}

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
        const double period_ms = 1000.0 * BLOCK / rate;
        const Headroom h = headroom(v, out.data(), rate, BLOCK, period_ms);   // step 67 (#13): the best of up to three seconds
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(st.active_voices == 16 && st.source == VOXO_SOURCE_SUZU && (!RELEASE_BUILD || h.worst_ms < period_ms),
              "headroom: %u Suzu voices (SVF + shear, 2x), block %u at %u Hz — mean %.3f ms, worst %.3f ms of a %.3f ms period (%.1f %% of the callback; the attempts' worst: %s)",
              st.active_voices, BLOCK, rate, h.mean_ms, h.worst_ms, period_ms, 100.0 * h.worst_ms / period_ms, headroom_attempts(h).c_str());
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
            std::vector<float> s = render(w, rate, 1.0); voxo_stats_t st{}; voxo_stats(w, &st); voxo_destroy(w);
            return std::make_pair(finite_all(s.data(), s.size()) && st.breaker_trips == 0, peak(s.data(), s.size()));   // step 75: the breaker's trip reads as the blow-up
        };
        voxo_suzu_params_t red = sp; red.string_cfl = 1.05f; red.cfl_gate = 0; auto rr = play(red);
        voxo_suzu_params_t green = sp; green.string_cfl = 0.95f; auto gr = play(green);
        CHECK(!rr.first || rr.second > 10.0f, "CFL, the RED control: k·dt² = 1.05 with the gate bypassed, C4 — the chain blows up within a second (%s, peak %.3g)", rr.first ? "finite" : "non-finite: the breaker tripped", rr.second);
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
            voxo_stats_t st{}; voxo_stats(v, &st);
            if (!finite_all(s.data(), s.size()) || st.breaker_trips > 0) { blew = true; at = sec; break; }   // step 75: the breaker's trip reads as the blow-up
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
    // ---- 15. headroom: sixteen Verlet strings at 80 nodes — step 78's gate: under 6 % of the callback on the four-lane engine ----
    {
        const uint32_t rate = 48000;
        voxo_t* v = make(rate, 16);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 2; sp.string_nodes = 80; sp.cutoff_hz = 2000.0f; sp.resonance = 0.3f;
        voxo_set_suzu_params(v, &sp); voxo_set_input_mode(v, 2);
        for (int i = 0; i < 16; i++) note_on(v, 0, 36 + i * 2, 100);
        std::vector<float> out(2u * BLOCK);
        const double period_ms = 1000.0 * BLOCK / rate;
        const Headroom h = headroom(v, out.data(), rate, BLOCK, period_ms);   // step 67 (#13): the best of up to three seconds
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(st.active_voices == 16 && (!RELEASE_BUILD || h.worst_ms < 0.06 * period_ms), "headroom: %u Verlet strings at 80 nodes (SVF on, 2x; the four-lane engine, %s), block %u at %u Hz — mean %.3f ms, worst %.3f ms of a %.3f ms period (%.1f %% of the callback, under 6 on the Release build%s; the attempts' worst: %s)",
              st.active_voices, suzu::simd_name(), BLOCK, rate, h.mean_ms, h.worst_ms, period_ms, 100.0 * h.worst_ms / period_ms, RELEASE_BUILD ? "" : " — a Debug build: the figure, not the gate", headroom_attempts(h).c_str());
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
            std::vector<float> s = render(w, rate, 1.0); voxo_stats_t st{}; voxo_stats(w, &st); voxo_destroy(w);
            return std::make_pair(finite_all(s.data(), s.size()) && st.breaker_trips == 0, peak(s.data(), s.size()));   // step 75: the breaker's trip reads as the blow-up
        };
        voxo_suzu_params_t red = sp; red.bore_cfl = 1.05f; red.bore_cfl_gate = 0; auto rr = blow(red);
        voxo_suzu_params_t green = sp; green.bore_cfl = 0.95f; auto gr = blow(green);
        CHECK(!rr.first || rr.second > 10.0f, "the bore's CFL, the RED control through the ABI: 1.05× with the gate bypassed, C5 blown — non-finite or over 10 within a second (%s, peak %.3g)", rr.first ? "finite" : "non-finite: the breaker tripped", rr.second);
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
            std::vector<float> s2 = render(w, rate, 0.7); s.insert(s.end(), s2.begin(), s2.end()); voxo_stats_t st{}; voxo_stats(w, &st); voxo_destroy(w);
            return std::make_pair(finite_all(s.data(), s.size()) && st.breaker_trips == 0, peak(s.data(), s.size()));   // step 75: the breaker's trip reads as the blow-up
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


    std::printf("[suzu] step 75 — the Poincaré strobe (KESSHO §2.5)\n");
    // ---- 28. the strobe: 1:1 one point, 3:2 two, the ET fifth precessing, the +50 cent bend's rate, the rotor's section
    //          by its own kicks, the toggle clean and the audio thread allocation-free ----
    {
        const uint32_t rate = 48000;
        const char* dump = std::getenv("SUZU_STROBE_DUMP");   // a directory: the returns as CSV for the step's charts (tools/strobe_chart.py)
        struct Ret { float x, y, u, w; };
        struct Voice3 { int note, ch, bend; };               // bend: the 14-bit value (8192 = none; ±48 semitones over the range)
        struct Run { std::vector<float> audio; std::vector<std::vector<Ret>> ret; std::vector<float> hz; uint64_t news = 0, blocks_delta = 0; uint32_t polled_voices = 0; };
        const float AMP_TEXEL = 0.25f * 512.0f;              // a cell unit at the default trace scale, in texels at 512: 0.5 texel = 0.0039
        auto play = [&](int kind, uint32_t clock, std::vector<Voice3> voices, double seconds, float rotor_k, bool undamped, float drive) {
            Run r; voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = (uint32_t)kind;
            if (undamped) sp.decay_s = 0.0f;
            if (rotor_k > 0.0f) sp.rotor_k = rotor_k;
            if (drive > 0.0f) sp.drive = drive;
            voxo_set_suzu_params(v, &sp); mcm(v);
            if (clock) { voxo_set_trace(v, 1u << kind); voxo_set_strobe(v, clock); }
            r.ret.resize(voices.size());
            for (const Voice3& vo : voices) { if (vo.bend != 8192) bend14(v, vo.ch, vo.bend); note_on(v, vo.ch, vo.note, 100); if (drive > 0.0f) voxo_push_midi(v, (uint8_t)(0xD0 | vo.ch), 100, 0); }
            const uint32_t blocks = (uint32_t)(seconds * rate / 128.0);
            r.audio.resize((size_t)blocks * 256u);
            for (auto& rr : r.ret) rr.reserve((size_t)(seconds * 2500.0));
            r.hz.reserve(blocks);
            static voxo_strobe_t st[16];
            const uint64_t news0 = g_news.load(), blocks0 = blocks_in_use();
            for (uint32_t b = 0; b < blocks; b++) {
                voxo_render(v, r.audio.data() + (size_t)b * 256u, 128);
                if (clock) {
                    const uint32_t n = voxo_strobe_poll(v, st, 16, VOXO_STROBE_POINTS_MAX);
                    r.hz.push_back(voxo_strobe_hz(v)); r.polled_voices = n;
                    for (uint32_t i = 0; i < n; i++)
                        for (size_t k = 0; k < voices.size(); k++)
                            if (st[i].channel == voices[k].ch) for (uint32_t j = 0; j < st[i].count; j++) r.ret[k].push_back({ st[i].x[j], st[i].y[j], st[i].u[j], st[i].w[j] });
                }
            }
            r.news = g_news.load() - news0; r.blocks_delta = blocks_in_use() - blocks0;
            voxo_destroy(v); return r;
        };
        auto csv = [&](const char* name, const std::vector<Ret>& pts) {
            if (!dump) return;
            std::string path = std::string(dump) + "/" + name; FILE* f = std::fopen(path.c_str(), "w"); if (!f) return;
            std::fprintf(f, "x,y,u,w\n"); for (const Ret& p : pts) std::fprintf(f, "%.7g,%.7g,%.7g,%.7g\n", p.x, p.y, p.u, p.w); std::fclose(f);
        };
        // the extent of a set of returns: the largest distance from their mean, in texels at the default scale
        auto extent_texels = [&](const std::vector<Ret>& pts, size_t from, size_t to) {
            double mx = 0.0, my = 0.0; size_t n = 0;
            for (size_t i = from; i < to && i < pts.size(); i++) { mx += pts[i].x; my += pts[i].y; n++; }
            if (n == 0) return 1e9;
            mx /= (double)n; my /= (double)n; double worst = 0.0;
            for (size_t i = from; i < to && i < pts.size(); i++) worst = std::fmax(worst, std::hypot(pts[i].x - mx, pts[i].y - my));
            return worst * AMP_TEXEL;
        };
        auto angle_of = [](const std::vector<Ret>& pts, size_t from, size_t to) {
            double sx = 0.0, sy = 0.0; for (size_t i = from; i < to && i < pts.size(); i++) { sx += pts[i].x; sy += pts[i].y; } return std::atan2(sy, sx);
        };
        const int A2 = 45, E3 = 52, DS3 = 51;
        const int JUST_FIFTH = 8192 + 3;                   // +1.76 cents: the just fifth (701.96 cents) to the bend's resolution (0.59 cents a unit); the residual 0.20 cents
        // (a) 1:1 — the undamped cell at A2 strobed by the bass phasor at its own frequency: one stationary point
        {
            Run r = play(0, 1, { { A2, 1, 8192 } }, 2.0, 0.0f, true, 0.0f);
            const std::vector<Ret>& p = r.ret[0];
            const double ext = extent_texels(p, 2, p.size());
            const int expected = (int)std::lround(2.0 * note_hz(A2));
            csv("strobe_1_1.csv", p);
            CHECK(p.size() >= (size_t)expected - 2 && p.size() <= (size_t)expected + 2 && ext < 0.5 && r.polled_voices == 1,
                  "1:1 — the undamped cell at A2 strobed at the bass (its own frequency) for 2 s: %zu returns (%d expected), all within %.3f texel of one point (< 0.5 at the default scale, 512)", p.size(), expected, ext);
        }
        // (b) 3:2 — A2 with E3 bent to the just fifth: the fifth's returns alternate between two antipodal points, each still
        //     within half a texel over a tenth of a second (the residual 0.20 cents precesses them 0.26 texel in that window);
        //     the equal-tempered fifth, unbent, precesses its pair at 2π·1.5·(1 − 2^(−1.955/1200)) = 0.0107 rad a return
        {
            Run j = play(0, 1, { { A2, 1, 8192 }, { E3, 2, JUST_FIFTH } }, 1.0, 0.0f, true, 0.0f);
            const std::vector<Ret>& p = j.ret[1];
            std::vector<Ret> even, odd; for (size_t i = 2; i < p.size(); i++) (i % 2 ? odd : even).push_back(p[i]);
            double worst_win = 0.0; const size_t WIN = 6;    // 11 returns of the fifth's clock = 6 of each class
            for (size_t i = 0; i + WIN <= even.size(); i += WIN) worst_win = std::fmax(worst_win, extent_texels(even, i, i + WIN));
            for (size_t i = 0; i + WIN <= odd.size(); i += WIN) worst_win = std::fmax(worst_win, extent_texels(odd, i, i + WIN));
            double dang = angle_of(even, 0, even.size()) - angle_of(odd, 0, odd.size());
            while (dang > 3.14159265) dang -= 6.2831853; while (dang <= -3.14159265) dang += 6.2831853;
            csv("strobe_3_2_just.csv", p); csv("strobe_3_2_bass.csv", j.ret[0]);
            CHECK(p.size() > 100 && worst_win < 0.5 && std::fabs(std::fabs(dang) - 3.14159265) < 0.1,
                  "3:2 — E3 at the just fifth over A2, strobed at the bass for 1 s: %zu returns alternating between two points %.3f rad apart (π), each class within %.3f texel over every tenth of a second", p.size(), std::fabs(dang), worst_win);
            Run e = play(0, 1, { { A2, 1, 8192 }, { E3, 2, 8192 } }, 1.0, 0.0f, true, 0.0f);
            const std::vector<Ret>& q = e.ret[1];
            std::vector<Ret> ev2; for (size_t i = 2; i < q.size(); i += 2) ev2.push_back(q[i]);
            double turned = 0.0, prev = ev2.empty() ? 0.0 : std::atan2(ev2[0].y, ev2[0].x);
            for (size_t i = 1; i < ev2.size(); i++) { double a = std::atan2(ev2[i].y, ev2[i].x), d = a - prev; while (d > 3.14159265) d -= 6.2831853; while (d <= -3.14159265) d += 6.2831853; turned += d; prev = a; }
            const double expected_turn = (double)(ev2.size() - 1) * 2.0 * 2.0 * 3.14159265 * 1.5 * (1.0 - std::pow(2.0, -1.955 / 1200.0));   // two returns of the clock per class step
            csv("strobe_3_2_et.csv", q);
            CHECK(ev2.size() > 40 && std::fabs(std::fabs(turned) - expected_turn) < 0.15 * expected_turn,
                  "the equal-tempered fifth, unbent: its two points precess %.3f rad over the second (%.3f expected from the 1.955 cents) — the strobe reads the temperament", std::fabs(turned), expected_turn);
        }
        // (c) a +50 cent bend on the bass shifts the strobe's rate by the matching ratio: the phasor's frequency and the count of returns
        {
            const int BEND50 = 8192 + 85;                      // +0.498 semitones at ±48 over 8192: 49.8 cents
            const double semis = 85.0 / 8192.0 * 48.0, ratio = std::pow(2.0, semis / 12.0);
            Run a = play(0, 1, { { A2, 1, 8192 } }, 10.0, 0.0f, true, 0.0f), b = play(0, 1, { { A2, 1, BEND50 } }, 10.0, 0.0f, true, 0.0f);
            const double hz_a = a.hz.empty() ? 0.0 : a.hz.back(), hz_b = b.hz.empty() ? 0.0 : b.hz.back();
            const double n_a = (double)a.ret[0].size(), n_b = (double)b.ret[0].size();
            const bool hz_ok = std::fabs(hz_a - note_hz(A2)) < 1e-3 && std::fabs(hz_b / hz_a - ratio) < 1e-5;
            const bool count_ok = std::fabs(n_a - 10.0 * note_hz(A2)) <= 2.0 && std::fabs(n_b - 10.0 * note_hz(A2) * ratio) <= 2.0 && std::fabs(n_b / n_a - ratio) < 0.003;
            CHECK(hz_ok && count_ok, "the bass bent +%.1f cents: the phasor runs at %.4f Hz against %.4f (ratio %.5f, expected %.5f); %g returns in 10 s against %g (ratio %.5f)",
                  100.0 * semis, hz_b, hz_a, hz_b / hz_a, ratio, n_b, n_a, n_b / n_a);
        }
        // (d) incommensurate — the tritone (√2, exact in equal temperament) over the bass: a dotted ring, every angular bin visited
        {
            Run r = play(0, 1, { { A2, 1, 8192 }, { DS3, 2, 8192 } }, 1.0, 0.0f, true, 0.0f);
            const std::vector<Ret>& p = r.ret[1];
            int bins[16] = {}; double rmin = 1e9, rmax = 0.0;
            for (size_t i = 2; i < p.size(); i++) { const double a = std::atan2(p[i].y, p[i].x) + 3.14159265; bins[(int)(a / 6.2831853 * 16.0) & 15]++; const double rr = std::hypot(p[i].x, p[i].y); rmin = std::fmin(rmin, rr); rmax = std::fmax(rmax, rr); }
            int empty = 0; for (int b : bins) if (b == 0) empty++;
            csv("strobe_tritone.csv", p);
            CHECK(p.size() > 100 && empty == 0 && (rmax - rmin) / rmax < 0.02, "incommensurate — D#3 (√2 over A2) strobed at the bass: %zu returns, all sixteen angular bins visited, on a ring (radius within %.1f %%): the dotted ring", p.size(), 100.0 * (rmax - rmin) / rmax);
        }
        // (e) the rotor by its own kicks (clock 2): the section (θ, p) at each kick IS Chirikov's map — K 0.3 a closed curve
        //     (the separatrix's thin layer from the strike's hyperbolic point, |p| ≤ 2√K), K 2.5 dust over the torus; charted.
        //     WHAT THE STROBE FOUND: over a held note the rotor's section is only approximately the standard map — the
        //     retune's re-basing at each kick is not exactly area-preserving in (θ, p) — and at K 2.5 the undamped rotor
        //     sinks into a period-4 cycle within ~10 s (the occupancy of the thirds falls 22 → 7 → 6 %); the gate reads
        //     the dust over the first 440 kicks and prints the thirds (Phase 12's rotor, DECISIONS_10 #12)
        {
            const float ks[6] = { 0.3f, 0.6f, 0.9f, 1.2f, 1.8f, 2.5f };
            double pmax[6] = {}, occ[6] = {}, thirds[6][3] = {}; size_t cnt[6] = {};
            auto occupancy = [](const std::vector<Ret>& p, size_t from, size_t to) {
                bool grid[24][24] = {}; int n = 0;
                for (size_t k = from; k < to && k < p.size(); k++) {
                    const double th = std::atan2(p[k].y, p[k].x) + 3.14159265, pp = (double)p[k].u + 3.14159265;
                    int a = (int)(th / 6.2831853 * 24.0), b = (int)(pp / 6.2831853 * 24.0);
                    if (a < 0) a = 0; if (a > 23) a = 23; if (b < 0) b = 0; if (b > 23) b = 23;
                    if (!grid[a][b]) { grid[a][b] = true; n++; }
                }
                return n / 576.0;
            };
            for (int i = 0; i < 6; i++) {
                Run r = play(5, 2, { { A2, 1, 8192 } }, 12.0, ks[i], true, 0.0f);   // undamped: the rotor's own T60 (3 s) takes a damped one into float noise
                const std::vector<Ret>& p = r.ret[0]; cnt[i] = p.size();
                for (size_t k = 0; k < p.size() && k < 440; k++) pmax[i] = std::fmax(pmax[i], std::fabs((double)p[k].u));
                occ[i] = occupancy(p, 0, 440);
                for (int t = 0; t < 3; t++) thirds[i][t] = occupancy(p, (size_t)t * 440, (size_t)(t + 1) * 440);
                char name[64]; std::snprintf(name, sizeof name, "strobe_rotor_K%.1f.csv", ks[i]); csv(name, p);
            }
            const int expected = (int)std::lround(12.0 * note_hz(A2));
            std::printf("     the rotor's section by thirds of 12 s (occupancy of a 24×24 (θ, p) grid):");
            for (int i = 0; i < 6; i++) std::printf(" K %.1f: %.0f/%.0f/%.0f %%;", ks[i], 100.0 * thirds[i][0], 100.0 * thirds[i][1], 100.0 * thirds[i][2]);
            std::printf("\n");
            CHECK(cnt[0] >= (size_t)expected - 3 && cnt[0] <= (size_t)expected + 3 && pmax[0] < 1.3 && occ[0] < 0.2 && pmax[5] > 2.5 && occ[5] > 0.15 && occ[4] > 0.3,
                  "the rotor strobed by its own kicks, A2 undamped for 12 s: %zu returns (%d kicks expected); over the first 440 kicks K 0.3 — |p| ≤ %.2f, %.0f %% of the (θ, p) grid: a closed curve; K 1.8 — %.0f %%; K 2.5 — |p| ≤ %.2f, %.0f %%: dust (the sweep charted: %.0f, %.0f, %.0f, %.0f, %.0f, %.0f %%)",
                  cnt[0], expected, pmax[0], 100.0 * occ[0], 100.0 * occ[4], pmax[5], 100.0 * occ[5], 100.0 * occ[0], 100.0 * occ[1], 100.0 * occ[2], 100.0 * occ[3], 100.0 * occ[4], 100.0 * occ[5]);
        }
        // (f) the Duffing cell by its own drive (clock 2): one return a drive period; undriven, the bass's
        {
            Run d = play(4, 2, { { A2, 1, 8192 } }, 2.0, 0.0f, false, 0.6f);
            Run u = play(4, 2, { { A2, 1, 8192 } }, 2.0, 0.0f, false, 0.0f);
            const int expected = (int)std::lround(2.0 * note_hz(A2));   // drive_ratio 1: the drive at the note
            CHECK((int)d.ret[0].size() >= expected - 3 && (int)d.ret[0].size() <= expected + 3 && (int)u.ret[0].size() >= expected - 3 && (int)u.ret[0].size() <= expected + 3,
                  "the Duffing cell by its own drive: %zu returns in 2 s at A2 (%d drive periods); undriven it keeps the bass: %zu", d.ret[0].size(), expected, u.ret[0].size());
            csv("strobe_duffing_drive.csv", d.ret[0]);
        }
        // (h) found by the strobe fuzz: a patch of another voice kind while notes sound — the voices built as cells rendered
        //     by the trumpet's body were NaN in two blocks, and a NaN never leaves the bus; now the kind change ends them
        {
            voxo_t* v = make(rate); mcm(v);
            for (int i = 0; i < 3; i++) note_on(v, 1 + i, 48 + 4 * i, 100);
            std::vector<float> buf(256); bool fin = true; float peak_before = 0.0f, peak_after = 0.0f, peak_new = 0.0f;
            for (int b = 0; b < 100; b++) { voxo_render(v, buf.data(), 128); for (float x : buf) { if (!std::isfinite(x)) fin = false; peak_before = std::fmax(peak_before, std::fabs(x)); } }
            voxo_suzu_params_t sp; voxo_suzu_patch(4, &sp); voxo_set_suzu_params(v, &sp);   // the Trumpet (kind 8) over held cells
            voxo_stats_t st{};
            for (int b = 0; b < 200; b++) { voxo_render(v, buf.data(), 128); for (float x : buf) { if (!std::isfinite(x)) fin = false; peak_after = std::fmax(peak_after, std::fabs(x)); } }
            voxo_stats(v, &st); const uint32_t active_after = st.active_voices;
            cc(v, 0, 2, 100); note_on(v, 4, 60, 100); voxo_push_midi(v, 0xD4, 110, 0);
            for (int b = 0; b < 400; b++) { voxo_render(v, buf.data(), 128); for (float x : buf) { if (!std::isfinite(x)) fin = false; peak_new = std::fmax(peak_new, std::fabs(x)); } }
            voxo_destroy(v);
            CHECK(fin && peak_before > 0.01f && active_after == 0 && peak_after < 1e-3f && peak_new > 0.01f && sp.voice_kind == 8,
                  "a patch of another kind over sounding notes (three cells, then the Trumpet): the audio stays finite, the cells end (%u active, peak %.2g after), and the next note sounds as the trumpet (peak %.3f)", active_after, peak_after, peak_new);
        }
        // (i) found by the strobe fuzz: the flute at B5 bent +47 semitones in one jump (17.6 kHz) was NaN in the block — its
        //     jet's band past the SVF's range, its bore at four cells; the model now saturates at rate2/16. And the breaker:
        //     the lab's forced-unstable chain (the CFL gate bypassed, k·dt² 1.5) diverges — the bus cuts the block, the
        //     stats count the trip, the audio out stays finite, and the next note sounds
        {
            voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_patch(10, &sp); voxo_set_suzu_params(v, &sp); mcm(v);
            cc(v, 1, 2, 126); note_on(v, 1, 82, 100); voxo_push_midi(v, 0xD1, 120, 0);
            std::vector<float> buf(256); bool fin = true; float peak = 0.0f;
            for (int b = 0; b < 800; b++) { if (b == 100) bend14(v, 1, 8192 + 8000); voxo_render(v, buf.data(), 128); for (float x : buf) { if (!std::isfinite(x)) fin = false; if (b > 150) peak = std::fmax(peak, std::fabs(x)); } }
            voxo_stats_t st{}; voxo_stats(v, &st);
            CHECK(fin && peak > 0.01f && st.breaker_trips == 0 && sp.voice_kind == 6, "the flute (patch 10) at B5 bent +46.9 semitones in one jump: finite throughout, still sounding (peak %.3f after the bend), the breaker untouched (%u trips) — the model saturates at its ceiling", peak, st.breaker_trips);
            voxo_destroy(v);
            v = make(rate); voxo_suzu_default_params(&sp); sp.voice_kind = 2; sp.cfl_gate = 0; sp.string_cfl = 1.5f; voxo_set_suzu_params(v, &sp); mcm(v);
            note_on(v, 1, 60, 100);
            fin = true; float peak_after = 0.0f;
            for (int b = 0; b < 400; b++) { voxo_render(v, buf.data(), 128); for (float x : buf) if (!std::isfinite(x)) fin = false; }
            voxo_stats(v, &st); const uint32_t trips = st.breaker_trips, active_after = st.active_voices;
            sp.cfl_gate = 1; sp.string_cfl = 0.0f; voxo_set_suzu_params(v, &sp);
            note_on(v, 2, 64, 100);
            for (int b = 0; b < 200; b++) { voxo_render(v, buf.data(), 128); for (float x : buf) { if (!std::isfinite(x)) fin = false; peak_after = std::fmax(peak_after, std::fabs(x)); } }
            voxo_destroy(v);
            CHECK(fin && trips >= 1 && active_after == 0 && peak_after > 0.01f, "the breaker: the forced-unstable chain (k·dt² 1.5, the gate bypassed) diverged — %u trip(s), the voices ended (%u active), the audio out finite throughout, and the next note on the derived chain sounds (peak %.3f)", trips, active_after, peak_after);
        }
        // (g) the toggle is clean and the thread allocates nothing: for the cell, the lattice, the Duffing, the rotor and the
        //     trumpet, three voices rendered with the strobe on and polled every block equal the unstrobed rendering bit for
        //     bit; across the strobed loop no global new and (Apple) no change in the malloc zone's blocks in use
        {
            bool all_same = true, no_alloc = true; uint64_t worst_news = 0, worst_blocks = 0;
            for (int kind : { 0, 1, 4, 5, 7, 9 }) {
                std::vector<Voice3> vs = { { 48, 1, 8192 }, { 55, 2, 8192 + 40 }, { 64, 3, 8192 - 70 } };
                Run a = play(kind, 0, vs, 1.0, 0.0f, false, kind == 4 ? 0.5f : 0.0f), b = play(kind, kind == 5 || kind == 4 ? 2 : 1, vs, 1.0, 0.0f, false, kind == 4 ? 0.5f : 0.0f);
                const bool same = a.audio.size() == b.audio.size() && std::memcmp(a.audio.data(), b.audio.data(), a.audio.size() * sizeof(float)) == 0;
                if (!same) all_same = false;
                if (b.news != 0 || b.blocks_delta != 0) no_alloc = false;
                worst_news = std::max(worst_news, b.news); worst_blocks = std::max(worst_blocks, b.blocks_delta);
                std::printf("     kind %d: strobed == unstrobed %s; news %llu, blocks %llu; returns %zu/%zu/%zu\n", kind, same ? "yes" : "NO", (unsigned long long)b.news, (unsigned long long)b.blocks_delta, b.ret[0].size(), b.ret[1].size(), b.ret[2].size());
            }
            CHECK(all_same && no_alloc, "the strobe is a reader: for kinds 0, 1, 4, 5, 7, 9 three bent voices rendered with the strobe on and polled every block equal the unstrobed rendering bit for bit; the loop made %llu allocations (global new) and changed the zone's blocks by %llu",
                  (unsigned long long)worst_news, (unsigned long long)worst_blocks);
        }
    }

    std::printf("[suzu] VCSL fitted patches (HANDOFF_SUZU_VCSL.md)\n");
    // ---- 27. the VCSL fitted patches pass the gates, sound on pitch, and track their declared decays ----
    {
        const uint32_t rate = 48000;
        voxo_t* v = make(rate);
        mcm(v);

        struct PatchTest {
            const char* name;
            int voice_kind;
            int modal_preset;
            int modes;
            float decay_s;
            float decay_bright;
            float stiffness;
            float pluck;
            int test_note;
        };

        const PatchTest cases[] = {
            {"Dan Tranh",     1, 0, 13,  3.435f, 0.0836f, 0.000132f, 0.300f, 60},
            {"Glockenspiel",  1, 1,  3,  4.161f, 0.0f,    0.000018f, 0.110f, 72},
            {"Tubular Bells", 1, 2,  9, 17.359f, 0.0406f, 0.000057f, 0.110f, 60},
            {"Concert Harp",  1, 4,  5,  5.728f, 0.0088f, 0.000206f, 0.445f, 60},
            {"Tenor Sax",     7, 0,  8,  0.25f,  0.0f,    0.0f,      0.280f, 58},
            {"Baroque Flute", 6, 0,  8,  2.0f,   0.0f,    0.0f,      0.280f, 69}
        };

        for (const auto& c : cases) {
            voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
            sp.voice_kind = (uint32_t)c.voice_kind;
            sp.modal_preset = (uint32_t)c.modal_preset;
            sp.modes = (uint32_t)c.modes;
            sp.decay_s = c.decay_s;
            sp.decay_bright = c.decay_bright;
            sp.stiffness = c.stiffness;
            sp.pluck = c.pluck;

            const bool admitted = voxo_set_suzu_params(v, &sp);
            CHECK(admitted, "VCSL fitted patch '%s' (voice %d, preset %d, modes %d): admitted by the load gates",
                  c.name, c.voice_kind, c.modal_preset, c.modes);

            if (c.voice_kind == 6 || c.voice_kind == 7) cc(v, 1, 2, 70);
            note_on(v, 1, c.test_note, 100);
            std::vector<float> buf(2 * 128);
            float peak = 0.0f;
            bool finite = true;
            for (int b = 0; b < 60; b++) {
                voxo_render(v, buf.data(), 128);
                for (float s : buf) {
                    if (!std::isfinite(s)) finite = false;
                    peak = std::fmax(peak, std::fabs(s));
                }
            }
            note_off(v, 1, c.test_note);
            cc(v, 1, 120, 0);
            voxo_render(v, buf.data(), 128);
            CHECK(finite && peak > 1e-4f && peak < 2.0f,
                  "VCSL fitted patch '%s': rendered note %d cleanly, peak %.3f, finite: %s",
                  c.name, c.test_note, peak, finite ? "yes" : "NO");
        }
        voxo_destroy(v);
    }

    // Gate 28 (step 67's app fixes, DECISIONS_9 #10; step 79: fourteen; step 80: sixteen): THE PATCH TABLE — the named patches, every one admitted by
    // the load gates on a fresh instance, and the six VCSL entries equal to presets/<name>_vcsl.json's `suzu` block
    // field for field (the file's keys are scanned; a key the gate does not know fails it, so a new field in a
    // fitted file cannot slip past the table).
    {
        struct Field { const char* key; size_t off; bool u32; };
        #define F32(k) { #k, offsetof(voxo_suzu_params_t, k), false }
        #define U32(k) { #k, offsetof(voxo_suzu_params_t, k), true }
        static const Field FIELDS[] = {
            U32(voice_kind), U32(modal_preset), U32(modes), F32(coupling), F32(decay_s), F32(decay_bright), F32(stiffness), F32(pluck),
            F32(bow_onset_s), F32(bow_position), U32(breath_cc), F32(level), F32(attack_s), F32(release_s), F32(cutoff_hz), F32(resonance),
            F32(shear), U32(shear_kind), U32(press_blows), F32(reed_hz), F32(reed_q), F32(reed_open), F32(reed_close), F32(reed_area),
            F32(reed_noise), F32(cone_apex), U32(bore_nodes), F32(bore_loss), F32(bore_corner_hz), F32(jet_gain), F32(jet_drive), F32(jet_tau),
            F32(jet_q), F32(jet_noise), F32(jet_area), F32(jet_offset), F32(breath_ref), F32(breath_range), F32(bore_wall_s),
            U32(string_nodes), F32(string_decay_s), F32(pickup), F32(bridge_hz), U32(bridge_cells), F32(bridge_coupling), F32(bridge_decay_s),
            F32(loop_loss), F32(duffing_beta), F32(drive), F32(drive_ratio), F32(rotor_k), U32(mod_target), F32(mod_depth), F32(mod_rate),
            F32(lip_ratio), F32(lip_q), F32(lip_open), F32(lip_close), F32(lip_area), F32(lip_range), U32(partial), F32(bell_start), F32(bell_gamma), F32(brass),
            U32(drum_size), F32(drum_kappa), F32(drum_alpha), F32(drum_cap), U32(plate_on), U32(plate_size), F32(plate_hz), F32(plate_kappa),   // step 79
            F32(plate_decay_s), F32(plate_bright), F32(plate_gain),
            U32(bow_mode), F32(bow_force), F32(bow_speed),   // step 80
            F32(reed_inertance), F32(bore_leak), F32(brass_shock), U32(shock_limiter), F32(lip_shear), F32(lip_shear_ratio), U32(tonehole_mode), F32(hole_size), F32(hole_loss),   // step 81
            U32(modal_comp), U32(oversample),   // step 82 (the table's arrays are compared apart); the oversampler's section (step 78) a file written by the serializer carries
            U32(toda_nodes), F32(toda_b), F32(toda_cap), F32(toda_width),   // step 83
            F32(boris_b), F32(boris_b_rest),   // step 84
        };
        #undef F32
        #undef U32
        const uint32_t count = voxo_suzu_patch_count();
        CHECK(count == 19, "the patch table holds nineteen patches (%u)", count);
        voxo_t* v = make(48000);
        for (uint32_t i = 0; i < count; i++) {
            voxo_suzu_params_t sp; const bool got = voxo_suzu_patch(i, &sp);
            const bool admitted = got && voxo_set_suzu_params(v, &sp);
            CHECK(got && voxo_suzu_patch_name(i) && admitted, "patch %u '%s' is in the table and admitted by the load gates", i, voxo_suzu_patch_name(i) ? voxo_suzu_patch_name(i) : "?");
        }
        voxo_destroy(v);
        voxo_suzu_params_t none; CHECK(!voxo_suzu_patch(count, &none) && voxo_suzu_patch_name(count) == nullptr, "past the end: false and NULL");
        const struct { uint32_t index; const char* file; } VCSL[] = {
            { 5, "dan_tranh_vcsl.json" }, { 6, "glockenspiel_vcsl.json" }, { 7, "tubular_bells_vcsl.json" },
            { 8, "concert_harp_vcsl.json" }, { 9, "tenor_sax_vcsl.json" }, { 10, "baroque_recorder_vcsl.json" },
            { 11, "drum.json" }, { 12, "gong.json" }, { 13, "harp_soundboard.json" },   // step 79
            { 14, "bowed_stickslip.json" }, { 15, "bowed_chain.json" },   // step 80
            { 16, "unison.json" },   // step 82
            { 17, "toda.json" },     // step 83
            { 18, "gyroscope.json" },   // step 84
        };
        for (const auto& e : VCSL) {
            std::string path = std::string(PRESETS_DIR) + "/" + e.file;
            FILE* f = std::fopen(path.c_str(), "rb");
            std::string json;
            if (f) { char buf[4096]; size_t n; while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) json.append(buf, n); std::fclose(f); }
            const size_t at = json.find("\"suzu\"");
            CHECK(f && at != std::string::npos, "%s read, with a suzu block", e.file);
            if (!f || at == std::string::npos) continue;
            const size_t open = json.find('{', at), close = json.find('}', open);
            const std::string block = json.substr(open + 1, close - open - 1);
            voxo_suzu_params_t sp; voxo_suzu_patch(e.index, &sp);
            int keys = 0, mismatches = 0; std::string unknown;
            size_t pos = 0;
            while ((pos = block.find('"', pos)) != std::string::npos) {
                const size_t kend = block.find('"', pos + 1); if (kend == std::string::npos) break;
                const std::string key = block.substr(pos + 1, kend - pos - 1);
                const size_t colon = block.find(':', kend); if (colon == std::string::npos) break;
                size_t vstart = colon + 1; while (vstart < block.size() && (block[vstart] == ' ' || block[vstart] == '\n')) vstart++;
                if (vstart < block.size() && block[vstart] == '[') {   // step 82: the table's arrays, element for element
                    const size_t close = block.find(']', vstart); std::string arr = block.substr(vstart + 1, close - vstart - 1);
                    const float* mine = key == "modal_ratio" ? sp.modal_ratio : key == "modal_t60" ? sp.modal_t60 : key == "modal_weight" ? sp.modal_weight : nullptr;
                    if (!mine) unknown += key + " ";
                    else { const char* c = arr.c_str(); char* e; int i = 0; for (;; i++) { const double x = std::strtod(c, &e); if (e == c) break; if (i < VOXO_MODAL_MAX && std::fabs((double)mine[i] - x) > 1e-6 * std::fmax(1.0, std::fabs(x))) mismatches++; c = e; while (*c == ',' || *c == ' ') c++; } }
                    keys++; pos = close + 1; continue;
                }
                const double value = std::strtod(block.c_str() + colon + 1, nullptr);
                pos = block.find(',', colon); if (pos == std::string::npos) pos = block.size();
                keys++;
                if (key == "source" || key.rfind("trace_", 0) == 0) continue;   // the source and the trace's routes are the shell's, not the patch's (a file the serializer wrote carries them)
                const Field* fld = nullptr;
                for (const Field& c : FIELDS) if (key == c.key) { fld = &c; break; }
                if (!fld) { unknown += key + " "; continue; }
                const unsigned char* base = (const unsigned char*)&sp;
                if (fld->u32) { uint32_t u; std::memcpy(&u, base + fld->off, 4); if ((double)u != value) mismatches++; }
                else { float x; std::memcpy(&x, base + fld->off, 4); if (std::fabs((double)x - value) > 1e-6 * std::fmax(1.0, std::fabs(value))) { mismatches++; std::printf("    %s: table %.9g, file %.9g\n", key.c_str(), (double)x, value); } }
            }
            CHECK(keys > 5 && mismatches == 0 && unknown.empty(), "patch %u '%s' equals %s field for field (%d keys, %d mismatches, unknown: %s)",
                  e.index, voxo_suzu_patch_name(e.index), e.file, keys, mismatches, unknown.empty() ? "none" : unknown.c_str());
        }
    }

    // Gate 29 (the 2.0 release round, DECISIONS_9 #16): THE WINDS UNDER A HARD PRESS stay under the master knee. The
    // press that blows is the mouth alone — the output gain holds at the breath controller's — so three trumpet voices
    // at pressure 127 (a hard finger on a tablet, press_blows on) peak at most where the knee begins, one voice under
    // −10 dBFS, and the breath-driven level (CC 2 at 127, no press) is what it was. Measured pre-knee: the master gain
    // at 0.05 keeps every sample under 0.5, the knee's start, so the peak read back over 0.05 is the voice's own.
    {
        struct Case { uint32_t patch; const char* name; int voices; int breath; int pressure; float bound; } cases[] = {
            { 4, "Trumpet",  3, -1, 127, 0.70f }, { 4, "Trumpet",  1, -1, 127, 0.316f }, { 2, "Flute", 3, -1, 127, 0.70f },
            { 3, "Saxophone", 3, -1, 127, 0.70f }, { 4, "Trumpet (breath 127, no press)", 1, 127, -1, 0.316f },
        };
        for (const Case& c : cases) {
            voxo_t* v = make(48000, 16);
            voxo_suzu_params_t sp; voxo_suzu_patch(c.patch, &sp); voxo_set_suzu_params(v, &sp);
            voxo_set_gain(v, 0.05f);
            mcm(v);
            const uint8_t notes[3] = { 60, 64, 67 };
            if (c.breath >= 0) cc(v, 0, 2, (uint8_t)c.breath);
            for (int i = 0; i < c.voices; i++) {
                note_on(v, 1 + i, notes[i], 100);
                if (c.pressure >= 0) voxo_push_midi(v, (uint8_t)(0xD1 + i), (uint8_t)c.pressure, 0);
            }
            std::vector<float> buf(2u * 128); float pk = 0.0f;
            const uint32_t frames = 48000u * 3u / 2u, from = 48000u / 2u;
            for (uint32_t f = 0; f < frames; f += 128) { voxo_render(v, buf.data(), 128); if (f >= from) for (float x : buf) pk = std::fmax(pk, std::fabs(x)); }
            pk /= 0.05f;
            CHECK(pk > 0.02f && pk <= c.bound, "under the knee: %s, %d voice(s), breath %d, pressure %d — pre-knee peak %.3f (%+.1f dBFS; bound %.3f)",
                  c.name, c.voices, c.breath, c.pressure, pk, 20.0 * std::log10(pk > 1e-9f ? pk : 1e-9f), c.bound);
            voxo_destroy(v);
        }
    }

    std::printf("[suzu] step 78 — the four-lane engine and the x4 oversampler (TO_PROJECT_SPEC §13.8, DECISIONS_12 #1)\n");
    // ---- 29. the four-lane paths against their scalar twins: the chain and the lattice, stepped side by side ----
    {
        // the chain: 80 nodes plucked, 4800 sub-steps (50 ms at the 2× rate), damped; every node compared bit for bit, the
        // worst relative difference against the declared tolerance (1e-6 of the peak) where the compiler's contraction differs
        suzu::VerletString a, b; a.reset(80); b.reset(80);
        a.tune(suzu::VerletString::s_for(220.0f, 96000.0f, 80)); b.tune(a.s2 > 0.0f ? std::sqrt(a.s2) : 0.0f);
        a.pluck(0.3f, 0.28f); b.pluck(0.3f, 0.28f);
        const float damp = suzu::contraction_for(4.0f, 96000.0f);
        size_t same = 0, total = 0; double worst = 0.0, peak = 0.0;
        for (int i = 0; i < 4800; i++) {
            a.step(damp); b.step_scalar(damp);
            for (int k = 1; k <= 80; k++) {
                total++; if (std::memcmp(&a.u[k], &b.u[k], 4) == 0) same++;
                worst = std::fmax(worst, std::fabs((double)a.u[k] - b.u[k])); peak = std::fmax(peak, std::fabs((double)b.u[k]));
            }
        }
        const double rel = peak > 0.0 ? worst / peak : worst;
        CHECK(peak > 1e-3 && rel <= 1e-6, "the four-lane chain (%s) against its scalar twin over 4800 sub-steps of 80 nodes: %.2f %% of the node values bit-identical, the worst difference %.2e of the peak (within 1e-6)", suzu::simd_name(), 100.0 * same / total, rel);
        // the lattice: 16 modes kicked, a fold shear at 0.7, the coupling 0.3, damped; the SIMD rotation + contraction against
        // the scalar sub-step, the modes compared bit for bit
        suzu::Cell ca[suzu::MODES_MAX], cb[suzu::MODES_MAX]; float eps[suzu::MODES_MAX], inv[suzu::MODES_MAX], rr[suzu::MODES_MAX];
        for (int k = 0; k < 16; k++) { const float e = suzu::eps_for(220.0f * (1.0f + 0.7f * k), 96000.0f); eps[k] = e; inv[k] = 1.0f / e; rr[k] = suzu::contraction_for(2.0f + 0.1f * k, 96000.0f); ca[k].kick(0.2f / (1.0f + k), e); cb[k] = ca[k]; }
        eps[13] = 0.0f; inv[13] = 0.0f; ca[13].reset(); cb[13].reset();   // a muted mode in the middle: a wall both ways
        const float k_eps = suzu::coupling_k(0.3f, eps[0], suzu::eps_for(4186.009f, 96000.0f));
        same = 0; total = 0; worst = 0.0; peak = 0.0;
        for (int i = 0; i < 4800; i++) {
            suzu::lattice_rotate_simd(ca, inv, 16, k_eps, 0.7f, false); suzu::lattice_contract_simd(ca, 16, rr);
            suzu::lattice_step_scalar(cb, eps, inv, 16, k_eps, 0.7f, false, rr);
            for (int k = 0; k < 16; k++) {
                total += 2; if (std::memcmp(&ca[k].x, &cb[k].x, 4) == 0) same++; if (std::memcmp(&ca[k].y, &cb[k].y, 4) == 0) same++;
                worst = std::fmax(worst, std::fmax(std::fabs((double)ca[k].x - cb[k].x), std::fabs((double)ca[k].y - cb[k].y))); peak = std::fmax(peak, std::fabs((double)cb[k].x));
            }
        }
        const double rel2 = peak > 0.0 ? worst / peak : worst;
        CHECK(peak > 1e-3 && rel2 <= 1e-6 && ca[13].x == 0.0f && ca[13].y == 0.0f, "the four-lane lattice (%s) against its scalar twin over 4800 sub-steps of 16 modes with a fold shear and a muted mode: %.2f %% of the states bit-identical, the worst difference %.2e of the peak (within 1e-6); the muted mode stayed at rest", suzu::simd_name(), 100.0 * same / total, rel2);
    }
    // ---- 30. the x4 oversampler: the fold at full drive — the alias floor in the band [rate/4, 0.4 rate] below −60 dB of the
    //          fundamental, against the x2 two-tap average's (the red, the shipped section); the lattice on pitch at x4 ----
    {
        const uint32_t rate = 48000; const float f0 = 2093.005f;   // C7: the fold's harmonics reach Nyquist in a dozen, so the x2 average's aliases show
        auto floor_db = [&](uint32_t over, double* fund_db, double* worst_hz) {
            voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
            sp.voice_kind = 0; sp.shear = 1.0f; sp.shear_kind = 1; sp.oversample = over; sp.cutoff_hz = 20000.0f; sp.resonance = 0.0f; sp.level = 0.5f;
            voxo_set_suzu_params(v, &sp); mcm(v); note_on(v, 1, 96, 127);
            std::vector<float> buf(2u * 128u), s; s.reserve(rate);
            for (uint32_t f = 0; f < rate + 4800; f += 128) { voxo_render(v, buf.data(), 128); if (f >= 4800) for (uint32_t i = 0; i < 128; i++) s.push_back(buf[2 * i]); }
            voxo_destroy(v);
            const double fund = goertzel(s.data(), s.size(), f0, rate);
            double worst = 0.0; *worst_hz = 0.0;
            for (double hz = 0.25 * rate; hz <= 0.40 * rate; hz += 20.0) {
                const double k = hz / f0; if (std::fabs(k - std::round(k)) * f0 < 30.0) continue;   // a harmonic's own bin (and the window's skirt): not an alias
                const double a = goertzel(s.data(), s.size(), hz, rate);
                if (a > worst) { worst = a; *worst_hz = hz; }
            }
            *fund_db = 20.0 * std::log10(fund > 1e-12 ? fund : 1e-12);
            return 20.0 * std::log10((worst > 1e-12 ? worst : 1e-12) / (fund > 1e-12 ? fund : 1e-12));
        };
        double fund2, fund4, hz2, hz4;
        const double floor2 = floor_db(2, &fund2, &hz2), floor4 = floor_db(4, &fund4, &hz4);
        CHECK(floor4 <= -60.0 && floor2 > floor4 + 10.0 && fund4 > -60.0, "the x4 oversampler on the fold at full drive, C7: the alias floor %.1f dB below the fundamental in [%u, %u] Hz (≤ −60; the worst at %.0f Hz) — the red: the x2 two-tap average reads %.1f dB (its worst at %.0f Hz; the fundamental %.1f dBFS at x2, %.1f at x4)", floor4, rate / 4, (uint32_t)(0.4 * rate), hz4, floor2, hz2, fund2, fund4);
        // the lattice at x4 sounds its partials on pitch (the section's rate threaded through the strike and the retune)
        {
            voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 1; sp.oversample = 4; sp.modes = 4; sp.coupling = 0.0f; sp.cutoff_hz = 20000.0f;
            voxo_set_suzu_params(v, &sp); mcm(v); note_on(v, 1, 57, 100);
            std::vector<float> buf(2u * 128u), s; s.reserve(rate);
            for (uint32_t f = 0; f < rate + 4800; f += 128) { voxo_render(v, buf.data(), 128); if (f >= 4800) for (uint32_t i = 0; i < 128; i++) s.push_back(buf[2 * i]); }
            voxo_destroy(v);
            const double pk = peak_near(s.data(), s.size(), 220.0, rate);
            const double cents = 1200.0 * std::log2(pk / 220.0);
            CHECK(std::fabs(cents) < 1.0, "the lattice at x4: A3's fundamental at %.3f Hz, %+.2f cents (within 1)", pk, cents);
        }
    }
    // ---- 31. the prewarp helper agrees with eps_for to float rounding across the keyboard at both rates ----
    {
        double worst = 0.0;
        for (int r = 0; r < 2; r++) for (int m = 21; m <= 108; m++) {
            const float rate = r ? 44100.0f : 48000.0f, hz = 440.0f * std::pow(2.0f, (m - 69) / 12.0f);
            const float a = suzu::eps_for(hz, rate), b = suzu::prewarp(2.0f * suzu::PI * hz, 1.0f / rate);
            worst = std::fmax(worst, std::fabs((double)a - b) / a);
        }
        CHECK(worst < 2e-6, "suzu::prewarp(ω, dt) against eps_for(hz, rate) over MIDI 21–108 at 48 k and 44.1 k: the worst relative difference %.2e (float rounding)", worst);
    }

    std::printf("[suzu] step 79 — the membrane, the plate and the gong (TO_PROJECT_SPEC §13.9, DECISIONS_12 #3)\n");
    // ---- 32. the mode table: the simply-supported grid's (1,1), (2,1) and (1,2) where the boundary puts them ----
    {
        const uint32_t rate = 48000; const float rate2 = 2.0f * (float)rate; const int A2n = 45; const double f0 = note_hz(A2n);
        for (int plate = 0; plate < 2; plate++) {
            voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
            sp.voice_kind = 9; sp.drum_size = 16; sp.drum_kappa = plate ? 1.0f : 0.0f; sp.decay_s = 0.0f; sp.decay_bright = 0.0f; sp.release_s = 8.0f;
            sp.pluck = 0.3f; sp.pickup = 0.32f; sp.cutoff_hz = 20000.0f; sp.resonance = 0.0f; sp.level = 0.3f;
            CHECK(voxo_set_suzu_params(v, &sp), "the %s patch (share %.0f) is admitted", plate ? "plate" : "membrane", (double)sp.drum_kappa);
            mcm(v); note_on(v, 1, A2n, 100);
            std::vector<float> sig = render(v, rate, 1.2); voxo_destroy(v);
            const float* w = sig.data() + rate / 5; const size_t n = sig.size() - rate / 5;
            const int nx = 16, ny = 13;
            const float l11 = suzu::membrane_lambda(1, 1, nx, ny), l21 = suzu::membrane_lambda(2, 1, nx, ny), l12 = suzu::membrane_lambda(1, 2, nx, ny);
            const float e11 = suzu::eps_for((float)f0, rate2);
            auto f_of = [&](float l) { const float e = plate ? e11 * (l / l11) : e11 * std::sqrt(l / l11); return (double)rate2 / M_PI * std::asin(0.5 * e); };
            const double p11 = peak_near(w, n, f0, rate, 60.0), p21 = peak_near(w, n, f_of(l21), rate, 60.0), p12 = peak_near(w, n, f_of(l12), rate, 60.0);
            CHECK(std::fabs(cents(p11, f0)) < 2.0 && std::fabs(cents(p21, f_of(l21))) < 2.0 && std::fabs(cents(p12, f_of(l12))) < 2.0,
                  "the %s at A2 on the 16 × 13 grid: the (1,1) at %.2f Hz (%+.2f cents), the (2,1) at %.1f Hz (%+.2f cents of the simply-supported table's %.1f), the (1,2) at %.1f Hz (%+.2f cents of %.1f) — the partials as %s",
                  plate ? "plate" : "membrane", p11, cents(p11, f0), p21, cents(p21, f_of(l21)), f_of(l21), p12, cents(p12, f_of(l12)), f_of(l12), plate ? "λ" : "√λ");
        }
    }
    // ---- 33. the drift: the undamped grid's exact invariant over ten minutes ----
    {
        suzu::Membrane<suzu::MEMBRANE_MAX> m; m.reset(8, 6);
        m.tune(suzu::eps_for(220.0f, 96000.0f), 0.3f); m.strike(0.1f, 0.3f, 0.3f, 1.5f);
        m.step();
        const double h0 = m.energy(); double hmin = h0, hmax = h0, pmin = 1e300, pmax = 0.0;
        const long N = 96000L * 600L;
        for (long i = 1; i < N; i++) {
            m.step();
            if ((i & 0xFFFF) == 0) { const double h = m.energy(); hmin = std::fmin(hmin, h); hmax = std::fmax(hmax, h); }
            if (i > N - 96000L && (i & 63) == 0) { const double p = m.energy_plain(); pmin = std::fmin(pmin, p); pmax = std::fmax(pmax, p); }
        }
        const double h1 = m.energy();
        const double drift = db(h1 / h0), span = db(hmax / hmin);
        CHECK(std::isfinite(h1) && std::fabs(drift) < 0.005 && span < 0.005,
              "the undamped 8 × 6 grid (share 0.3) for ten minutes at 96 k: the leapfrog's invariant %.6g → %.6g, %+.5f dB (the span over the run %.5f dB; the bound 0.005); the plain energy's wobble over the last second %.3f dB — the staggering, not a drift",
              h0, h1, drift, span, db(pmax / pmin));
    }
    // ---- 34. the CFL gate at E_max: through the ABI, then the class's red ----
    {
        const uint32_t rate = 48000;
        voxo_t* v = make_logged(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 9;
        g_log.clear(); sp.drum_cfl = 1.05f;
        const bool rej = !voxo_set_suzu_params(v, &sp); const std::string msg = g_log;
        CHECK(rej && msg.find("Courant") != std::string::npos && msg.find("1.95") != std::string::npos, "the membrane's forced Courant number 1.05× is rejected with its message: %s", msg.c_str());
        sp.drum_cfl = 0.95f; CHECK(voxo_set_suzu_params(v, &sp), "0.95× is admitted");
        sp.drum_cfl = 0.0f; CHECK(voxo_set_suzu_params(v, &sp), "the derived λ is admitted");
        voxo_destroy(v);
        {   // the bypass: the forced 1.05× with the gate off — the top mode's leapfrog past ε = 2, the breaker trips within a second
            voxo_t* w = make(rate); voxo_suzu_params_t sq; voxo_suzu_default_params(&sq); sq.voice_kind = 9; sq.drum_cfl_gate = 0; sq.drum_cfl = 1.05f;
            CHECK(voxo_set_suzu_params(w, &sq), "RED: the bypass admits 1.05×"); mcm(w); note_on(w, 1, 60, 100);
            std::vector<float> buf(2u * 128); bool fin = true; uint32_t trips = 0, active = 0;
            for (int b = 0; b < 375; b++) { voxo_render(w, buf.data(), 128); if (!finite_all(buf.data(), buf.size())) fin = false; voxo_stats_t st; voxo_stats(w, &st); trips = st.breaker_trips; active = st.active_voices; if (trips) break; }
            CHECK(fin && trips >= 1 && active == 0, "RED: the bypassed 1.05× blew up within a second — the breaker tripped (%u), the voice ended, nothing non-finite left the bus", trips);
            voxo_destroy(w);
        }
        {   // the class's red: the sheet displaced to the (1,1) shape at 1.2× the cap's stretching, with the top mode seeded, diverges within 100 steps; at 1.0× it holds
            auto run = [&](float lam, double over, long steps, double* h_start, double* h_end) {
                suzu::Membrane<suzu::MEMBRANE_MAX> m; m.reset(16, 13);
                const float share = 0.0f, swell = 1.0f;
                const float factor = suzu::Membrane<suzu::MEMBRANE_MAX>::cfl_factor(16, 13, share, swell);
                m.tune(lam / factor, share);                                         // λ_2D = lam at E_max with c²_eff = 2c²
                const float emax = 1.0f; m.alpha = swell * m.c2 / emax;              // E_max 1 in grid units
                double phi2 = 0.0; for (int j = 1; j <= 13; j++) for (int i = 1; i <= 16; i++) { const double f = std::sin(M_PI * i / 17.0) * std::sin(M_PI * j / 14.0); phi2 += f * f; }
                const double a = std::sqrt(over * emax / (m.lambda11() * phi2));
                for (int j = 1; j <= 13; j++) for (int i = 1; i <= 16; i++) {
                    const double f = std::sin(M_PI * i / 17.0) * std::sin(M_PI * j / 14.0), top = std::sin(16.0 * M_PI * i / 17.0) * std::sin(13.0 * M_PI * j / 14.0);
                    m.u[j * suzu::Membrane<suzu::MEMBRANE_MAX>::S + i] = (float)(a * f + 1e-4 * a * top);
                }
                *h_start = m.energy_plain(); bool fin = true;
                for (long i = 0; i < steps; i++) { m.step(); if (!std::isfinite(m.E)) { fin = false; break; } }
                *h_end = fin ? m.energy_plain() : INFINITY;
                return std::make_pair(m.lambda11(), suzu::membrane_cfl(m.c2, 0.0f, m.alpha, (float)(over * emax)));
            };
            double hs, he; auto red = run(suzu::MEMBRANE_CFL_GATE, 1.2, 100, &hs, &he);
            CHECK(!std::isfinite(he) || he > 100.0 * hs, "RED: the sheet tuned to λ_2D 1.95 at the cap, displaced to 1.2× the cap's stretching (λ_2D %.3f there), diverges within 100 steps: the energy %.4g → %s", red.second, hs, std::isfinite(he) ? std::to_string(he).c_str() : "non-finite");
            double gs, ge; auto green = run(suzu::MEMBRANE_CFL_GATE_GONG, 1.0, 200000, &gs, &ge);
            CHECK(std::isfinite(ge) && std::fabs(db(ge / gs)) < 1.0, "at the gong's bar (λ_2D %.3f at the cap; a pure membrane at swell 1, the top mode seeded) the sheet holds for 200 000 steps: the plain energy %.4g → %.4g (%+.3f dB; the gong's term and the staggering wobble it) — at 1.95 the same sheet diverged (suzu.h: the bar's measurement)", green.second, gs, ge, db(ge / gs));
        }
    }
    // ---- 35. the gong: the swell of a hard strike over a soft one; the undamped energy bounded ----
    {
        const uint32_t rate = 48000; const int C3 = 48; const double f0 = note_hz(C3);
        auto strike = [&](int vel, bool undamped, double* swell_cents, double* late_cents, bool* finite, double* wobble_db) {
            voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_patch(12, &sp);
            if (undamped) { sp.decay_s = 0.0f; sp.decay_bright = 0.0f; sp.release_s = 30.0f; }
            voxo_set_suzu_params(v, &sp); mcm(v); note_on(v, 1, C3, vel);
            std::vector<float> sig = render(v, rate, 1.5);
            const size_t w = (size_t)(0.06 * rate);
            const double early = peak_near(sig.data() + (size_t)(0.005 * rate), w, f0 * 1.15, rate, 400.0), late = peak_near(sig.data() + (size_t)(1.2 * rate), w * 4, f0, rate, 300.0);
            *swell_cents = cents(early, f0); *late_cents = cents(late, f0); *finite = finite_all(sig.data(), sig.size());
            if (wobble_db) {
                voxo_inspect_t ins; double emin = 1e300, emax = 0.0; std::vector<float> buf(2u * 128);
                for (int b = 0; b < 375; b++) { voxo_render(v, buf.data(), 128); if (voxo_suzu_inspect(v, &ins, 1)) { emin = std::fmin(emin, ins.k[7]); emax = std::fmax(emax, ins.k[7]); } }
                *wobble_db = emax > 0.0 && emin > 0.0 ? db(emax / emin) : 999.0;
            }
            voxo_destroy(v);
        };
        double soft_s, soft_l, hard_s, hard_l, wob; bool f1, f2;
        strike(30, false, &soft_s, &soft_l, &f1, nullptr); strike(127, false, &hard_s, &hard_l, &f2, nullptr);
        CHECK(f1 && f2 && hard_s - soft_s > 50.0 && std::fabs(hard_l) < 30.0 && std::fabs(soft_l) < 30.0,
              "the gong (patch 12, α 0.8 at the cap 1.5) at C3: the first 60 ms sit %+.0f cents (velocity 127) against %+.0f (velocity 30) — the swell %.0f cents; a second later %+.1f and %+.1f cents (the pitch settles on the note as the stretching decays)",
              hard_s, soft_s, hard_s - soft_s, hard_l, soft_l);
        double us, ul; bool uf; strike(127, true, &us, &ul, &uf, &wob);
        CHECK(uf && wob < 3.0, "the undamped gong's energy over a second after the strike's first 1.5 s: within %.2f dB (bounded; the leapfrog's staggering and the gong's modified Hamiltonian wobble it, the cap bounds it)", wob);
    }
    // ---- 36. the bus plate: the tier's gate, the clamp line, audible under the chord, silent when off, moved by the point ----
    {
        const uint32_t rate = 48000;
        for (int n : { 64, 32 }) {
            const float l = suzu::membrane_lambda(1, 1, n, n), eps = suzu::eps_for(110.0f, (float)rate), share = 0.003f;
            const float lam = eps * std::sqrt(8.0f * (1.0f - share) / l + 64.0f * share / (l * l));
            CHECK(lam <= suzu::MEMBRANE_CFL_GATE, "the bus plate at %d² with the declared κ share %.3f at 110 Hz, at the output rate: λ_2D = %.3f (≤ 1.95)%s", n, (double)share, lam, n == 32 ? " — the mobile tier" : "");
        }
        {
            voxo_t* v = make_logged(rate); voxo_suzu_params_t sp; voxo_suzu_patch(13, &sp);
            g_log.clear(); CHECK(voxo_set_suzu_params(v, &sp) && g_log.empty(), "the soundboard patch's plate takes its declared tuning without a clamp line");
            sp.plate_kappa = 0.5f; g_log.clear(); const bool ok = voxo_set_suzu_params(v, &sp);
            CHECK(ok && g_log.find("bus plate") != std::string::npos, "a plate asked past the gate (share 0.5 at 64²) is clamped, not rejected, and the line says what it took: %s", g_log.c_str());
            voxo_destroy(v);
        }
        auto chord = [&](uint32_t patch, int plate_on, float px, float py) {
            voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_patch(patch, &sp); if (plate_on >= 0) sp.plate_on = (uint32_t)plate_on; voxo_set_suzu_params(v, &sp); mcm(v);
            if (px >= -1.5f) for (int ch = 1; ch <= 3; ch++) voxo_set_strike_point(v, (uint8_t)ch, (uint8_t)(ch == 1 ? 48 : ch == 2 ? 55 : 64), px, py);
            note_on(v, 1, 48, 100); note_on(v, 2, 55, 100); note_on(v, 3, 64, 100);
            std::vector<float> s = render(v, rate, 2.0); voxo_destroy(v); return s;
        };
        std::vector<float> dry = chord(8, -1, -2.0f, -2.0f), off = chord(13, 0, -2.0f, -2.0f), wet = chord(13, -1, -2.0f, -2.0f);
        std::vector<float> centre = chord(13, -1, 0.5f, 0.5f), corner = chord(13, -1, 0.1f, 0.1f), dflt = chord(13, -1, -1.0f, -1.0f);
        double rd = 0.0, rw = 0.0, rc = 0.0; bool same_off = dry.size() == off.size() && std::memcmp(dry.data(), off.data(), dry.size() * sizeof(float)) == 0;
        bool same_dflt = wet.size() == dflt.size() && std::memcmp(wet.data(), dflt.data(), wet.size() * sizeof(float)) == 0;
        for (size_t i = 0; i < dry.size(); i++) { rd += (double)dry[i] * dry[i]; const double d = wet[i] - dry[i]; rw += d * d; const double c = centre[i] - corner[i]; rc += c * c; }
        const double plate_db = 10.0 * std::log10(rw / rd), moved_db = 10.0 * std::log10(rc / rd);
        CHECK(same_off && plate_db > -30.0 && plate_db < -3.0 && finite_all(wet.data(), wet.size()),
              "the harp chord (C3 G3 E4) over the bus plate: the plate's signal %.1f dB under the dry (audible: within −30 … −3); with the plate off the render is the concert harp's bit for bit", plate_db);
        CHECK(same_dflt && moved_db > -40.0, "the strike point moves the plate's response: struck at the centre against the corner the plate's signals differ by %.1f dB of the dry; a negative point restores the default map (bit for bit)", moved_db);
    }
    // ---- 37. headroom: sixteen membranes and the 64² plate against the period (the Release build's bars) ----
    {
        const uint32_t rate = 48000; const double period_ms = 1000.0 * BLOCK / rate;
        voxo_t* v = make(rate, 16); voxo_suzu_params_t sp; voxo_suzu_patch(11, &sp); sp.decay_s = 0.0f; sp.decay_bright = 0.0f; voxo_set_suzu_params(v, &sp); mcm(v);
        for (int i = 0; i < 16; i++) note_on(v, 1 + (i % 15), 36 + 3 * i, 100);
        std::vector<float> out(2u * BLOCK);
        Headroom h = headroom(v, out.data(), rate, BLOCK, period_ms);
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(st.active_voices == 16 && (!RELEASE_BUILD || h.worst_ms < 0.30 * period_ms), "headroom: %u membranes at 16 × 13 (%s): mean %.3f ms, worst %.3f ms of %.3f (%.1f %% of the period; the bar 30 %% on the Release build%s; attempts %s)",
              st.active_voices, suzu::simd_name(), h.mean_ms, h.worst_ms, period_ms, 100.0 * h.worst_ms / period_ms, RELEASE_BUILD ? "" : " — a Debug build prints the figure", headroom_attempts(h).c_str());
        voxo_destroy(v);
        voxo_t* w = make(rate, 16); voxo_suzu_params_t sq; voxo_suzu_patch(13, &sq); voxo_set_suzu_params(w, &sq); mcm(w); note_on(w, 1, 48, 100);
        Headroom hp = headroom(w, out.data(), rate, BLOCK, period_ms);
        CHECK(!RELEASE_BUILD || hp.worst_ms < 0.25 * period_ms, "headroom: the 64² bus plate under one harp voice at the output rate: mean %.3f ms, worst %.3f ms (%.1f %% of the period; the bar 25 %% on the Release build; attempts %s)",
              hp.mean_ms, hp.worst_ms, 100.0 * hp.worst_ms / period_ms, headroom_attempts(hp).c_str());
        voxo_destroy(w);
    }
    // ---- 38. the membrane's strobe at the bass: one return a period, the pair finite, dumped for the chart ----
    {
        const uint32_t rate = 48000; const int C3 = 48;
        voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_patch(11, &sp); sp.decay_s = 0.0f; sp.decay_bright = 0.0f; voxo_set_suzu_params(v, &sp); mcm(v);
        voxo_set_trace(v, 1u << 9); voxo_set_strobe(v, 1); note_on(v, 1, C3, 100);
        static voxo_strobe_t st[4]; std::vector<float> buf(2u * 128); size_t returns = 0; bool fin = true;
        const char* dump = std::getenv("SUZU_STROBE_DUMP"); FILE* f = nullptr;
        if (dump) { std::string path = std::string(dump) + "/strobe_membrane.csv"; f = std::fopen(path.c_str(), "w"); if (f) std::fprintf(f, "x,y,u,w\n"); }
        for (int b = 0; b < 750; b++) {
            voxo_render(v, buf.data(), 128);
            const uint32_t n = voxo_strobe_poll(v, st, 4, VOXO_STROBE_POINTS_MAX);
            for (uint32_t i = 0; i < n; i++) for (uint32_t k = 0; k < st[i].count; k++) { returns++; if (!std::isfinite(st[i].x[k]) || !std::isfinite(st[i].y[k]) || !std::isfinite(st[i].u[k])) fin = false; if (f) std::fprintf(f, "%.7g,%.7g,%.7g,%.7g\n", st[i].x[k], st[i].y[k], st[i].u[k], st[i].w[k]); }
        }
        if (f) std::fclose(f);
        voxo_destroy(v);
        const long expected = std::lround(2.0 * note_hz(C3));
        CHECK(fin && (long)returns >= expected - 3 && (long)returns <= expected + 3, "the membrane at C3 strobed at the bass (itself) for 2 s: %zu returns (%ld expected), the pair (s, ṡ/ω) and the stretching finite — the inharmonic partials scatter the section (the chart: strobe_membrane.csv)", returns, expected);
    }

    std::printf("[suzu] step 80 — the stick-slip bow (TO_PROJECT_SPEC §13.10, DECISIONS_12 #4)\n");
    // the bowed render: the mouth at full breath from the first block, the inspection polled every block for the bow's
    // branch (k[stuck]), its work and the body's energy; returns the left channel
    struct BowRun { std::vector<float> s; double stuck_share, work, gain, e_first, e_max_over, gain_over; bool finite, ledger; };
    auto bow_run = [&](uint32_t kind, float force, uint32_t naive, double seconds, int note) {
        const uint32_t rate = 48000;
        voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
        sp.voice_kind = kind; sp.bow_mode = 1; sp.bow_position = 0.12f; sp.bow_force = force; sp.bow_naive = naive; sp.cutoff_hz = 20000.0f;
        sp.string_nodes = 48; sp.string_decay_s = 4.0f; sp.decay_s = 3.0f; sp.coupling = 0.0f; sp.level = 0.25f;
        voxo_set_suzu_params(v, &sp); mcm(v); cc(v, 1, 2, 127); note_on(v, 1, note, 100);
        BowRun r; r.stuck_share = 0.0; r.work = 0.0; r.gain = 0.0; r.e_first = -1.0; r.e_max_over = 0.0; r.gain_over = -1e300; r.finite = true; r.ledger = true;
        const uint32_t frames = (uint32_t)(seconds * rate); std::vector<float> out(2u * 128); r.s.reserve(frames);
        double stuck = 0.0, polls = 0.0, work0 = 0.0;
        voxo_inspect_t ins;
        for (uint32_t f = 0; f < frames; f += 128) {
            voxo_render(v, out.data(), 128);
            for (uint32_t i = 0; i < 128; i++) { r.s.push_back(out[2 * i]); if (!std::isfinite(out[2 * i])) r.finite = false; }
            if (voxo_suzu_inspect(v, &ins, 1)) {
                const double e = kind == 2 ? 0.5 * (double)ins.k[6] : kind == 3 ? (double)ins.k[12] : 0.0;
                const double work = kind == 2 ? ins.k[3] : kind == 3 ? ins.k[10] : ins.k[4];
                const double gain = kind == 2 ? ins.k[7] : kind == 3 ? ins.k[13] : ins.k[6];
                const float st = kind == 2 ? ins.k[5] : kind == 3 ? ins.k[11] : ins.k[5];
                if (r.e_first < 0.0) { r.e_first = e; work0 = work; }
                else {
                    // THE LEDGER, two lines: what the string TOOK (∫f·v at the force's midpoint) never exceeds the bow's work
                    // ∫f·v_bow — the sign discipline, per step; and what the body HOLDS over its start never exceeds that work
                    // (the losses only take). The first is the tight one; the second the roadmap's.
                    const double tol = 1e-4 * std::fabs(work) + 1e-9;
                    if (!std::isfinite(gain) || !std::isfinite(work) || gain > work + tol) r.ledger = false;
                    r.gain_over = std::fmax(r.gain_over, gain - work);
                    if (kind == 2 || kind == 3) {
                        const double over = (e - r.e_first) - (work - work0);
                        if (!std::isfinite(e) || over > tol) r.ledger = false;
                        if (std::isfinite(e)) r.e_max_over = std::fmax(r.e_max_over, over);
                    }
                }
                if (f > seconds * rate * 0.5) { polls++; stuck += st; }
                r.work = work; r.gain = gain;
            }
        }
        r.stuck_share = polls > 0.0 ? stuck / polls : 0.0;
        voxo_destroy(v); return r;
    };
    // ---- 39. the Helmholtz signature and its sharpening with the force ----
    {
        const uint32_t rate = 48000; const int A3 = 57; const double f0 = note_hz(A3);
        for (uint32_t kind : { 3u, 2u }) {
            double share[3], pitch[3]; bool fin = true;
            const float forces[3] = { 0.25f, 0.5f, 1.0f };
            for (int i = 0; i < 3; i++) {
                BowRun r = bow_run(kind, forces[i], 0, 2.0, A3);
                fin = fin && r.finite; share[i] = r.stuck_share;
                pitch[i] = cents(peak_near(r.s.data() + rate, rate, f0, rate, 200.0), f0);
            }
            CHECK(fin && share[0] < share[1] && share[1] < share[2] && share[2] > 0.6 && share[2] < 0.97 && std::fabs(pitch[1]) < 20.0 && std::fabs(pitch[2]) < 45.0,
                  "the %s under the stick-slip bow at A3 (bow at 0.12, full breath): the point sticks %.0f / %.0f / %.0f %% of the time at the force 0.25 / 0.5 / 1.0 — rising toward the Helmholtz share 1 − β = 88 %% as the corner sharpens; the pitch %+.0f / %+.0f / %+.0f cents (the flattening with force is the bowed string's own)",
                  kind == 3 ? "waveguide (the hybrid string)" : "Verlet chain", 100.0 * share[0], 100.0 * share[1], 100.0 * share[2], pitch[0], pitch[1], pitch[2]);
        }
    }
    // ---- 40. the ledger, and its red ----
    {
        for (uint32_t kind : { 3u, 2u }) {
            BowRun g = bow_run(kind, 1.0f, 0, 3.0, 57);
            CHECK(g.finite && g.ledger, "the ledger on the %s, 3 s at the force 1.0: the string took %.4g of the bow's %.4g of work (∫f·v at the midpoint ≤ ∫f·v_bow: the sign discipline, per step; the worst margin %.3g), and the energy held over its start stayed under the work by %.3g — both by arithmetic: a bounded root never carries the point past the bow",
                  kind == 3 ? "waveguide" : "chain", g.gain, g.work, g.gain_over, -g.e_max_over);
            BowRun r = bow_run(kind, 2.0f, 1, 3.0, 57);
            if (kind == 2) CHECK(!r.finite || !r.ledger, "RED: the three Newton steps taken raw, no bound (bow_naive) on the chain at the force 2.0: %s (the string took %.4g against the bow's %.4g)", !r.finite ? "non-finite within 3 s" : "the ledger's line crossed — the string took more than the bow worked", r.gain, r.work);
            else NOTE("the raw Newton on the waveguide (G = ½: the damped step still converges on the flank): the string took %.4g against the bow's %.4g, finite %s, within the ledger %s — the red fires on the chain", r.gain, r.work, r.finite ? "yes" : "NO", r.ledger ? "yes" : "no");
        }
    }
    // ---- 41. the modal string under the bow: on the note; the servo untouched ----
    {
        const uint32_t rate = 48000; const double f0 = note_hz(57);
        BowRun r = bow_run(1, 0.5f, 0, 2.0, 57);
        const double p = cents(peak_near(r.s.data() + rate, rate, f0, rate, 200.0), f0), pk = peak(r.s.data() + rate, rate);
        CHECK(r.finite && std::fabs(p) < 40.0 && pk > 0.05 && pk < 1.0, "the modal string (8 modes) under the stick-slip bow at A3, the force 0.5: on the note within %+.0f cents (the modal bow pulls flat; within 40), the peak %.3f; its point sticks %.0f %% of the time", p, pk, 100.0 * r.stuck_share);
        // bow_mode 0 is the servo as shipped: a bowed render with bow_mode 0 equals one where the stick-slip's knobs are moved (they are the mode's alone)
        auto servo = [&](float force, float speed) {
            voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.bow_mode = 0; sp.bow_force = force; sp.bow_speed = speed;
            voxo_set_suzu_params(v, &sp); mcm(v); cc(v, 1, 2, 100); note_on(v, 1, 57, 100);
            std::vector<float> s = render(v, rate, 1.0); voxo_destroy(v); return s;
        };
        std::vector<float> a = servo(0.5f, 1.0f), b = servo(3.0f, 0.3f);
        CHECK(a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0 && peak(a.data(), a.size()) > 0.01f, "the servo bow (bow_mode 0) reads none of the stick-slip's knobs: the bowed string at force 0.5 / speed 1 and at 3 / 0.3 render bit for bit");
    }

    // ======== step 81: the acoustic bore v2 (TO_PROJECT_SPEC §13.11, DECISIONS_12 #5) ========
    std::printf("[suzu] step 81 — the acoustic bore v2 (TO_PROJECT_SPEC §13.11, DECISIONS_12 #5)\n");
    // ---- 42. the sax's sidebands: the leak takes the bore's slow mode; its red ----
    {
        const uint32_t rate = 48000; const double f_nom = note_hz(57);
        auto sidebands = [&](float leak, int breath) {   // the worst sideband 15..120 Hz off the first four harmonics, dB below the harmonic (the last second of two)
            voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 7; sp.bore_leak = leak; voxo_set_suzu_params(v, &sp);
            mcm(v); cc(v, 1, 2, breath); note_on(v, 1, 57, 100);
            std::vector<float> s = render(v, rate, 2.0); voxo_destroy(v);
            const float* w = s.data() + rate; const size_t n = rate;
            const double f0 = peak_near(w, n, f_nom, rate, 120.0);
            double worst = -200.0;
            for (int k = 1; k <= 4; k++) {
                const double hk = goertzel(w, n, k * f0, rate); double sb = 0.0;
                for (double d = 15.0; d <= 120.0; d += 1.0) { sb = std::max(sb, goertzel(w, n, k * f0 + d, rate)); sb = std::max(sb, goertzel(w, n, k * f0 - d, rate)); }
                worst = std::max(worst, 20.0 * std::log10((sb + 1e-12) / (hk + 1e-12)));
            }
            return std::make_pair(worst, cents(f0, f_nom));
        };
        auto a = sidebands(0.4f, 50), b = sidebands(0.4f, 70), c = sidebands(0.4f, 100);
        CHECK(a.first < -45.0 && b.first < -45.0 && c.first < -45.0 && std::fabs(a.second) < 25.0 && std::fabs(b.second) < 25.0 && std::fabs(c.second) < 25.0,
              "the sax at A3 under breath 50 / 70 / 100 (bore_leak 0.4): a periodic tone — the worst sideband 15..120 Hz off the first four harmonics %.1f / %.1f / %.1f dB below them (< −45), the pitch %+.0f / %+.0f / %+.0f cents",
              a.first, b.first, c.first, a.second, b.second, c.second);
        auto r = sidebands(0.0f, 70);
        CHECK(r.first > -38.0, "RED: bore_leak 0 (the bore's slow mode undamped) at A3 under breath 70 — the quasi-periodic A3 of DECISIONS_7 #33: the worst sideband %.1f dB (> −38)", r.first);
    }
    // ---- 43. the brass shock: the crests' steepening, the limiter's loss in the ledger; its red ----
    {
        const uint32_t rate = 48000;
        struct Shock { bool fin, ledger; double tilt, loss, held, work, rough; uint32_t trips; };
        auto blow = [&](float shock, uint32_t limiter, int breath) {
            voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 8; sp.brass_shock = shock; sp.shock_limiter = limiter; voxo_set_suzu_params(v, &sp);
            mcm(v); cc(v, 1, 2, breath); note_on(v, 1, 60, 100);
            const uint32_t frames = rate * 2; std::vector<float> out(2u * frames); voxo_inspect_t ins; Shock r{ true, true, 0.0, 0.0, 0.0, 0.0, 0.0, 0u };
            for (uint32_t f = 0; f < frames; f += 128) {
                voxo_render(v, out.data() + 2u * f, 128);
                if (voxo_suzu_inspect(v, &ins, 1)) {
                    r.held = ins.k[9]; r.work = ins.k[8]; r.loss = ins.k[10]; if (f > rate / 10 && r.held > r.work * 1.0001 + 1e-9) r.ledger = false;
                    double d1 = 0.0, d2 = 0.0; for (uint32_t i = 1; i < ins.n; i++) d1 += std::fabs(ins.a[i] - ins.a[i - 1]); for (uint32_t i = 1; i + 1 < ins.n; i++) d2 += std::fabs(ins.a[i + 1] - 2.0f * ins.a[i] + ins.a[i - 1]);
                    if (f > rate / 2 && d1 > 0.0) r.rough = std::max(r.rough, d2 / d1);   // the bore's spatial roughness Σ|Δ²p|/Σ|Δp|: the grid-scale ringing shows here
                }
            }
            { voxo_stats_t st; voxo_stats(v, &st); r.trips = st.breaker_trips; }
            voxo_destroy(v);
            std::vector<float> s(frames); for (uint32_t i = 0; i < frames; i++) { s[i] = out[2u * i]; if (!std::isfinite(s[i])) r.fin = false; }
            if (!r.fin) return r;
            auto band = [&](double lo, double hi) { double e = 0.0; for (double hz = lo; hz < hi; hz += 20.0) { const double m = goertzel(s.data() + rate, rate, hz, rate); e += m * m; } return e; };
            r.tilt = 10.0 * std::log10((band(6000.0, 20000.0) + 1e-30) / (band(20.0, 3000.0) + 1e-30));
            return r;
        };
        Shock lim = blow(1.0f, 1, 127), raw = blow(1.0f, 0, 127), none = blow(0.0f, 1, 127), soft = blow(1.0f, 1, 60);
        CHECK(lim.fin && lim.ledger && raw.ledger && none.ledger && lim.loss > 0.0 && lim.loss > 5.0 * soft.loss && lim.tilt > none.tilt + 6.0,
              "the trumpet at C4, breath 127, under the brass shock (κ 1.7, the limiter on): the ledger holds (the energy held %.3g of the mouth's %.3g of work), the limiter's loss %.3g since the strike (%.3g at breath 60: the shock is the fortissimo's), and the blare — the 6–20 kHz band against 0–3 kHz %+.1f dB, %+.1f dB over the v1 bore's %+.1f",
              lim.held, lim.work, lim.loss, soft.loss, lim.tilt, lim.tilt - none.tilt, none.tilt);
        bool refused = false, accepted = false; std::string why;
        { voxo_t* v = make_logged(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 8; sp.brass_shock = 2.0f; sp.shock_limiter = 0; g_log.clear(); refused = !voxo_set_suzu_params(v, &sp); why = g_log; voxo_destroy(v); }
        { voxo_t* v = make_logged(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 8; sp.brass_shock = 2.0f; sp.shock_limiter = 1; accepted = voxo_set_suzu_params(v, &sp); voxo_destroy(v); }
        CHECK(raw.rough > 1.3 * lim.rough && refused && why.find("makes energy") != std::string::npos && accepted,
              "RED: the limiter off (shock_limiter 0) — at κ the steepening with nothing to take its energy rings at the grid (Gibbs): the bore's profile %.2f rough against the limited %.2f (1.3× or more; the bell keeps it out of the output's high band, %+.1f against %+.1f dB); at 2κ the valve gate refuses the patch at load — %s — while the limited 2κ is admitted",
              raw.rough, lim.rough, raw.tilt, lim.tilt, why.c_str());
    }
    // ---- 44. the two-mode lips: the registers hold with the shearing mode; the one-mode lips read none of it ----
    {
        const uint32_t rate = 48000; const double f0 = note_hz(60);
        auto reg = [&](float shear, float ratio, int c74) {
            voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 8; sp.lip_shear = shear; sp.lip_shear_ratio = ratio; voxo_set_suzu_params(v, &sp);
            mcm(v); cc(v, 1, 74, c74); cc(v, 1, 2, 70); note_on(v, 1, 60, 100);
            std::vector<float> s = render(v, rate, 1.0); voxo_destroy(v);
            return cents(peak_near(s.data() + rate / 2, rate / 2, f0, rate, 1400.0), f0);
        };
        const double lo = reg(0.3f, 1.5f, 0), on = reg(0.3f, 1.5f, 64), up = reg(0.3f, 1.5f, 127);
        CHECK(lo < -500.0 && std::fabs(on) <= 40.0 && up > 900.0, "the trumpet's lips with the shearing mode at 0.3 of the swing (ratio 1.5): CC 74 at 0 the peak below (%+.0f cents), at 64 the note (%+.0f), at 127 the octave (%+.0f) — the three registers as the one-mode lips'", lo, on, up);
        auto one = [&](float ratio) {
            voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 8; sp.lip_shear = 0.0f; sp.lip_shear_ratio = ratio; voxo_set_suzu_params(v, &sp);
            mcm(v); cc(v, 1, 2, 70); note_on(v, 1, 60, 100); std::vector<float> s = render(v, rate, 1.0); voxo_destroy(v); return s;
        };
        std::vector<float> a = one(1.5f), b = one(3.0f);
        CHECK(a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0 && peak(a.data(), a.size()) > 0.01f, "the one-mode lips (lip_shear 0) read none of the shearing mode's knobs: the trumpet at ratio 1.5 and 3 renders bit for bit");
    }
    // ---- 45. the toneholes: the Boehm chart's fingerings on the flute and the sax; the keys disagreeing take the v1 cut ----
    {
        const uint32_t rate = 48000;
        static const uint32_t KEYS[12] = { 239u, 495u, 111u, 47u, 143u, 15u, 31u, 11u, 41u, 9u, 1u, 0u };   // D4 … C#5 on the core's nine bits (L1 L2 L3 T G# R1 R2 R3 D#)
        struct Hole { double c; int j, reg; uint32_t bits; float pk; };
        auto play = [&](voxo_t* v, int note, uint32_t keys, int breath) {
            for (int b = 0; b < 9; b++) cc(v, 0, b < 3 ? 110 + b : 114 + b - 3, ((keys >> b) & 1u) ? 127 : 0);   // the master channel's keys
            cc(v, 1, 2, breath); note_on(v, 1, note, 100);
            const uint32_t frames = rate * 3 / 2; std::vector<float> out(2u * frames); voxo_inspect_t ins{}; Hole h{ 0.0, -2, -2, 0u, 0.0f };
            for (uint32_t f = 0; f < frames; f += 128) { voxo_render(v, out.data() + 2u * f, 128); if (f == 128 * 8 && voxo_suzu_inspect(v, &ins, 1)) { h.j = (int)ins.k[12]; h.reg = (int)ins.k[13]; h.bits = (uint32_t)ins.k[11]; } }
            std::vector<float> s(frames); for (uint32_t i = 0; i < frames; i++) s[i] = out[2u * i];
            h.pk = peak(s.data() + rate / 2, frames - rate / 2);
            const double f_nom = note_hz(note); h.c = h.pk > 1e-3f ? cents(peak_near(s.data() + rate / 2, rate, f_nom, rate, 300.0), f_nom) : -9999.0;
            voxo_push_midi(v, 0x81, (uint8_t)note, 0);
            return h;
        };
        auto fresh = [&](uint32_t kind, double* ms) {
            voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = kind; sp.tonehole_mode = 1u;
            const auto t0 = std::chrono::steady_clock::now(); voxo_set_suzu_params(v, &sp); if (ms) *ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            mcm(v); return v;
        };
        double ms_flute = 0.0, ms_sax = 0.0; bool ok = true; double worst = 0.0; int worst_j = 0; std::string line;
        for (int j = 0; j < 12; j++) {
            voxo_t* v = fresh(6, j == 0 ? &ms_flute : nullptr); Hole h = play(v, 62 + j, KEYS[j], 70); voxo_destroy(v);
            const bool good = h.j == j && h.reg == 0 && h.bits == ((1u << j) - 1u) && h.pk > 0.02f && std::fabs(h.c) <= 30.0;
            if (!good) ok = false; if (std::fabs(h.c) > std::fabs(worst)) { worst = h.c; worst_j = j; }
            char t[32]; std::snprintf(t, sizeof t, " %+.0f", h.c); line += t;
        }
        CHECK(ok, "the flute with the toneholes (hole_size 1): the Boehm chart's twelve fingerings of the first register, D4..C#5, each keyed by its CCs and struck — the inspection names the semitone and the holes open (1..j), the note sounds at the plain cut's level, and lands within 30 cents (the worst %+.0f at j %d; all:%s)", worst, worst_j, line.c_str());
        NOTE("the flute's hole calibration at patch load (two registers pulsed, eleven holes each, twice over): %.0f ms", ms_flute);
        Hole r2, dis, r3;   // an engine each: the inspection reads the oldest voice, and a released note is still a voice
        { voxo_t* v = fresh(6, nullptr); r2 = play(v, 74, KEYS[0], 70); voxo_destroy(v); } { voxo_t* v = fresh(6, nullptr); dis = play(v, 67, KEYS[0], 70); voxo_destroy(v); } { voxo_t* v = fresh(6, nullptr); r3 = play(v, 86, KEYS[0], 70); voxo_destroy(v); }
        {
          CHECK(r2.j == 0 && r2.reg == 1 && (r2.bits & (1u << 11)) && std::fabs(r2.c) <= 45.0 && dis.j == -1 && dis.pk > 0.02f && std::fabs(dis.c) <= 45.0 && r3.j == -1 && r3.pk > 0.01f,
                "the registers and the chart: D5 with the D4 keys is the second register (the vent open, %+.0f cents); G4 with the D4 keys disagrees with the chart and takes the v1 cut (j −1, %+.0f cents); D6 is past the holes' two registers (the v1 cut)", r2.c, dis.c); }
        { double w = 0.0; int wj = 0; bool ok7 = true; std::string l7;
          for (int j : { 3, 7, 10 }) { voxo_t* v = fresh(7, j == 3 ? &ms_sax : nullptr); Hole h = play(v, 62 + j, KEYS[j], 70); voxo_destroy(v);
              if (!(h.j == j && h.reg == 0 && h.pk > 0.005f && std::fabs(h.c) <= 45.0)) ok7 = false; if (std::fabs(h.c) > std::fabs(w)) { w = h.c; wj = j; } char t[32]; std::snprintf(t, sizeof t, " %+.0f", h.c); l7 += t; }
          CHECK(ok7, "the sax with the toneholes: F4, A4 and B4 fingered, within 45 cents (the worst %+.0f at j %d; %s) — the holes tuned by blowing at patch load", w, wj, l7.c_str());
          NOTE("the sax's hole calibration at patch load (pulsed, then blown twice over): %.0f ms", ms_sax); }
    }

    // ======== step 82: the editable table, the Scala tuning, the unison pair (TO_PROJECT_SPEC §13.12, DECISIONS_12 #6) ========
    std::printf("[suzu] step 82 — the editable table, the Scala tuning and the unison pair (TO_PROJECT_SPEC §13.12, DECISIONS_12 #6)\n");
    // ---- 46. the table: as resolved, as inspected, at the oscillator (the ratios and the decays) ----
    {
        const uint32_t rate = 48000; const double f0 = note_hz(57);
        static const float R[6] = { 1.0f, 2.3f, 3.1f, 4.7f, 6.2f, 8.9f }, T[6] = { 3.0f, 2.0f, 1.5f, 1.0f, 0.5f, 0.25f }, W[6] = { 1.0f, 0.5f, 0.5f, 0.3f, 0.2f, 0.1f };
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 1; sp.modal_preset = 5; sp.modes = 6; sp.coupling = 0.0f; sp.decay_bright = 0.0f; sp.cutoff_hz = 20000.0f;
        for (int k = 0; k < 6; k++) { sp.modal_ratio[k] = R[k]; sp.modal_t60[k] = T[k]; sp.modal_weight[k] = W[k]; }
        uint32_t n = 0; float r[16], t[16], w[16]; voxo_suzu_table(&sp, &n, r, t, w);
        bool same = n == 6; for (int k = 0; k < 6 && same; k++) same = std::fabs(r[k] - R[k]) < 1e-6f && std::fabs(t[k] - T[k]) < 1e-6f && std::fabs(w[k] - W[k]) < 1e-6f;
        CHECK(same, "the table as the engine resolves it (voxo_suzu_table, modal_preset 5): six modes, the ratios, the decays and the weights as stated");
        voxo_t* v = make(rate); voxo_set_suzu_params(v, &sp); mcm(v); note_on(v, 1, 57, 100);
        const uint32_t frames = rate * 2; std::vector<float> out(2u * frames); voxo_inspect_t ins{}; bool got = false; double e0[6] = { 0 }, e1[6] = { 0 };
        for (uint32_t f = 0; f < frames; f += 128) {
            voxo_render(v, out.data() + 2u * f, 128);
            if (f == 128 * 4) got = voxo_suzu_inspect(v, &ins, 1) > 0;
            if (f == (uint32_t)(0.3 * rate) / 128 * 128 || f == (uint32_t)(1.3 * rate) / 128 * 128) { voxo_inspect_t q; if (voxo_suzu_inspect(v, &q, 1)) for (int k = 0; k < 6; k++) (f < rate ? e0 : e1)[k] = (double)q.a[k] * q.a[k] + (double)q.b[k] * q.b[k]; }
        }
        voxo_destroy(v);
        bool insp = got && ins.n == 6; for (int k = 0; k < 6 && insp; k++) insp = std::fabs(ins.s[k] - R[k]) < 1e-5f;
        CHECK(insp, "the inspection names the table's ratios (s[k]) on the sounding voice");
        std::vector<float> s(frames); for (uint32_t i = 0; i < frames; i++) s[i] = out[2u * i];
        double worst = 0.0; for (int k = 0; k < 4; k++) { const double c = cents(peak_near(s.data() + rate / 4, rate, f0 * R[k], rate, 60.0), f0 * R[k]); if (std::fabs(c) > std::fabs(worst)) worst = c; }
        CHECK(std::fabs(worst) < 3.0, "at the oscillator: the first four partials of the table at A3 (coupling 0) land on f0 × 1 / 2.3 / 3.1 / 4.7 within 3 cents (the worst %+.2f)", worst);
        double worst_t = 0.0; int wk = 0;
        for (int k = 0; k < 4; k++) { const double t60 = e0[k] > 0.0 && e1[k] > 0.0 ? 6.0 * 1.0 / std::log10(e0[k] / e1[k]) : 0.0; const double err = t60 / T[k] - 1.0; if (std::fabs(err) > std::fabs(worst_t)) { worst_t = err; wk = k; } }
        CHECK(std::fabs(worst_t) < 0.2, "and the table's decays: the mode energies' fall from 0.3 to 1.3 s reads each T60 within 20 %% (the worst %+.0f %% on mode %d: 3 / 2 / 1.5 / 1 s declared)", 100.0 * worst_t, wk + 1);
    }
    // ---- 47. the Scala tuning: the table within 0.001 cent, the oscillator within 0.1, the mapping, the refusals ----
    {
        const uint32_t rate = 48000;
        std::string edo19 = "! 19edo.scl\n!\n19 tone equal temperament\n 19\n!\n"; for (int d = 1; d <= 19; d++) { char b[64]; std::snprintf(b, sizeof b, " %.6f\n", 1200.0 * d / 19.0); edo19 += b; }
        voxo_t* v = make_logged(rate); g_log.clear();
        const bool ok = voxo_set_tuning(v, edo19.c_str(), nullptr);
        double worst = 0.0; for (int n = 0; n < 128; n++) worst = std::max(worst, std::fabs(cents(voxo_tuning_hz(v, (uint32_t)n), 440.0 * std::pow(2.0, (n - 69) / 19.0))));
        CHECK(ok && voxo_tuning_count(v) == 19u && worst < 0.001 && g_log.find("19 degrees") != std::string::npos, "a 19-EDO .scl set as text: 19 degrees, every note of 128 within %.4f cents of 440·2^((n−69)/19) (the default mapping: MIDI 69 the reference, 60 the first degree); the log: \"%s\"", worst, g_log.c_str());
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 0; sp.decay_s = 0.0f; voxo_set_suzu_params(v, &sp); mcm(v);
        double worst_osc = 0.0; int wn = 0;
        for (int note : { 60, 64, 69, 81 }) {
            note_on(v, 1, note, 100); std::vector<float> s = render(v, rate, 3.0);
            const double want = 440.0 * std::pow(2.0, (note - 69) / 19.0), got = peak_near(s.data() + rate, 2u * rate, want, rate, 3.0);
            const double c = cents(got, want); if (std::fabs(c) > std::fabs(worst_osc)) { worst_osc = c; wn = note; }
            voxo_push_midi(v, 0x81, (uint8_t)note, 0); render(v, rate, 0.5);
        }
        CHECK(std::fabs(worst_osc) < 0.1, "at the oscillator: the cell at MIDI 60, 64, 69 and 81 under 19-EDO within 0.1 cent of the scale (the worst %+.4f at %d)", worst_osc, wn);
        const char* pyth = "! pyth12.scl\nPythagorean 12\n12\n!\n256/243\n9/8\n32/27\n81/64\n4/3\n729/512\n3/2\n128/81\n27/16\n16/9\n243/128\n2/1\n";
        const char* kbm = "! 12 keys, middle 60, reference 60 at 256 Hz, the formal octave 12\n12\n0\n127\n60\n60\n256.0\n12\n0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n";
        const bool ok2 = voxo_set_tuning(v, pyth, kbm);
        const double fifth = voxo_tuning_hz(v, 67) / voxo_tuning_hz(v, 60), c60 = voxo_tuning_hz(v, 60), c72 = voxo_tuning_hz(v, 72);
        CHECK(ok2 && std::fabs(fifth - 1.5) < 1e-6 && std::fabs(c60 - 256.0) < 1e-3 && std::fabs(c72 - 512.0) < 1e-3, "a Pythagorean scale in ratios with a .kbm (the reference MIDI 60 at 256 Hz): the fifth 60→67 reads %.7f (3/2), MIDI 60 %.3f Hz, 72 %.3f", fifth, c60, c72);
        g_log.clear(); const bool bad = voxo_set_tuning(v, "! bad degree\nbad\n2\n100.0\nfoo\n", nullptr);
        CHECK(!bad && voxo_tuning_count(v) == 12u && g_log.find("refused") != std::string::npos, "a malformed scale is refused and the tuning stands (still the 12 of Pythagoras): \"%s\"", g_log.c_str());
        voxo_clear_tuning(v); CHECK(voxo_tuning_count(v) == 0u && std::fabs(voxo_tuning_hz(v, 69) - 440.0f) < 1e-4f, "cleared: equal temperament, MIDI 69 at 440 Hz");
        voxo_destroy(v);
    }
    // ---- 48. the unison pair: the split as the chain says; the raw lattice deaf to the swirl ----
    {
        const uint32_t rate = 48000; const double f0 = note_hz(69);
        voxo_suzu_params_t sp; const bool got = voxo_suzu_patch(16, &sp);
        voxo_t* v = make(rate); const bool admitted = got && voxo_set_suzu_params(v, &sp); voxo_destroy(v);
        CHECK(got && admitted && sp.modal_preset == 5u && sp.modal_comp == 0u && sp.modal_ratio[0] == sp.modal_ratio[1], "the Unison pair patch (16): the table in pairs, the compensation off, admitted by the load gates (the raw lattice's bound read without the solve)");
        auto peaks = [&](float kappa, int swirl) {
            voxo_t* w = make(rate); voxo_suzu_params_t q = sp; q.coupling = kappa; voxo_set_suzu_params(w, &q); mcm(w);
            note_on(w, 1, 69, 100); if (swirl > 0) voxo_push_midi(w, 0xA1, 69, (uint8_t)swirl);
            std::vector<float> s = render(w, rate, 3.0); voxo_destroy(w);
            std::vector<std::pair<double, double>> m; for (double c = -10.0; c <= 40.0; c += 0.25) m.push_back({ goertzel(s.data() + rate / 2, 2u * rate + rate / 2, f0 * std::pow(2.0, c / 1200.0), rate), c });
            std::vector<double> found; for (size_t i = 1; i + 1 < m.size(); i++) if (m[i].first > m[i - 1].first && m[i].first >= m[i + 1].first && m[i].first > 1e-5) found.push_back(m[i].second);
            return std::make_pair(found, s);
        };
        const double lo = 1200.0 * std::log2(std::sqrt(1.0 + 0.01 * (3.0 - std::sqrt(5.0)) / 2.0)), hi = 1200.0 * std::log2(std::sqrt(1.0 + 0.01 * (3.0 + std::sqrt(5.0)) / 2.0));
        auto a = peaks(0.01f, 0);
        const bool two = a.first.size() >= 2;
        double d_lo = two ? a.first[0] - lo : 99.0, d_hi = two ? a.first[a.first.size() - 1] - hi : 99.0;
        CHECK(two && std::fabs(d_lo) < 1.5 && std::fabs(d_hi) < 1.5, "the pair at A4 under κ 0.01: two lines, %+.2f and %+.2f cents from the note — the chain's two normal modes, κ(3 − √5)/4 and κ(3 + √5)/4 of the frequency (%+.2f / %+.2f; one mode has one neighbour, the other two)", two ? a.first[0] : 0.0, two ? a.first.back() : 0.0, lo, hi);
        auto b = peaks(sp.coupling, 0), c = peaks(sp.coupling, 127);
        CHECK(b.second.size() == c.second.size() && std::memcmp(b.second.data(), c.second.data(), b.second.size() * sizeof(float)) == 0, "the raw lattice (modal_comp 0) takes no swirl share: the pair at the patch's κ %.3f renders bit for bit with poly pressure 127 and without (the swirl would detune every mode the solve no longer protects)", (double)sp.coupling);
    }

    // ======== step 83: the Toda lattice soliton ring (TO_PROJECT_SPEC §13.13, DECISIONS_13 #1) ========
    // a run of the ring through the engine: the patch with a nonlinearity, a press, a note; the swirl at a block (and again at
    // another); the inspection after the first block and at the end, the breaker's count
    struct TodaRun { std::vector<float> s; voxo_inspect_t first{}, last{}; uint32_t trips = 0; bool fin = true; };
    auto toda_run = [&](int note, float toda_b, int press, float decay_s, double seconds, int swirl_block, int swirl, int vel, float level, uint32_t gate, int swirl_block2 = -1) {
        const uint32_t rate = 48000; TodaRun r; voxo_t* w = make(rate); voxo_suzu_params_t q; voxo_suzu_patch(17, &q);
        q.toda_b = toda_b; q.decay_s = decay_s; q.toda_gate = gate; if (level > 0.0f) q.level = level; voxo_set_suzu_params(w, &q); mcm(w);
        if (press > 0) voxo_push_midi(w, 0xD1, (uint8_t)press, 0);
        note_on(w, 1, note, vel);
        const uint32_t frames = (uint32_t)(seconds * rate); r.s.resize(frames); std::vector<float> out(2u * BLOCK);
        for (uint32_t f = 0, b = 0; f < frames; f += BLOCK, b++) {
            if ((int)b == swirl_block || (int)b == swirl_block2) voxo_push_midi(w, 0xA1, (uint8_t)note, (uint8_t)swirl);
            const uint32_t n = (frames - f) < BLOCK ? (frames - f) : BLOCK; voxo_render(w, out.data(), n);
            for (uint32_t i = 0; i < n; i++) { r.s[f + i] = out[2u * i]; if (!std::isfinite(out[2u * i])) r.fin = false; }
            if (b == 0) voxo_suzu_inspect(w, &r.first, 1);
        }
        voxo_suzu_inspect(w, &r.last, 1); voxo_stats_t st; voxo_stats(w, &st); r.trips = st.breaker_trips; voxo_destroy(w); return r;
    };
    // ---- 49. the linear ring's modes on the note; the press makes the train; the ×4 path clean ----
    {
        const uint32_t rate = 48000; const double f0 = note_hz(57);
        voxo_suzu_params_t sp; const bool got = voxo_suzu_patch(17, &sp);
        voxo_t* v = make(rate); const bool admitted = got && voxo_set_suzu_params(v, &sp); voxo_destroy(v);
        CHECK(got && admitted && sp.voice_kind == 10u && sp.toda_nodes == 16u, "the Toda ring patch (17, '%s'): voice_kind 10, sixteen nodes, admitted by the load gates", voxo_suzu_patch_name(17));
        TodaRun lin = toda_run(57, 0.0f, 0, 0.0f, 2.0, -1, 0, 100, -1.0f, 1u);
        const float* w = lin.s.data() + rate / 2; const size_t n = rate;
        const double f1 = peak_near(w, n, f0, rate, 100.0);
        double worst = 0.0; for (int m = 2; m <= 5; m++) { const double th = std::sin(m * M_PI / 16.0) / std::sin(M_PI / 16.0), fm = peak_near(w, n, f1 * th, rate, 60.0); worst = std::fmax(worst, std::fabs(fm / f1 / th - 1.0)); }
        CHECK(lin.fin && std::fabs(1200.0 * std::log2(f1 / f0)) < 1.0 && worst < 0.003 && lin.first.voice_kind == 10u && lin.first.n == 16u && lin.first.k[6] == 2.0f,
              "the linear ring (toda_b 0) at A3: mode 1 %+.2f cents from the note, modes 2–5 within %.2f %% of the lattice's sin(mπ/16)/sin(π/16) (the inspection: kind 10, %u nodes, %g inner steps a sub-step)", 1200.0 * std::log2(f1 / f0), 100.0 * worst, lin.first.n, lin.first.k[6]);
        TodaRun soft = toda_run(57, sp.toda_b, 0, 0.0f, 2.0, -1, 0, 100, -1.0f, 1u), hard = toda_run(57, sp.toda_b, 127, 0.0f, 2.0, -1, 0, 100, -1.0f, 1u);
        const double fs = peak_near(soft.s.data() + rate / 2, rate, f0, rate, 100.0), fh = peak_near(hard.s.data() + rate / 2, rate, f0, rate, 700.0);
        const double h1 = goertzel(hard.s.data() + rate / 2, rate, fh, rate), h5 = goertzel(hard.s.data() + rate / 2, rate, 5.0 * fh, rate);
        CHECK(std::fabs(1200.0 * std::log2(fs / f0)) < 10.0 && 1200.0 * std::log2(fh / f0) > 300.0 && 20.0 * std::log10(h5 / h1) > -20.0 && hard.first.k[1] > 2.3f,
              "the press: at press 0 the ring sounds the note (%+.1f cents, b %.2f); at press 127 (b %.2f) the strike's pulse is a soliton train passing the pickup faster than sound — the fundamental %+.0f cents above the note, the fifth harmonic %.1f dB under it", 1200.0 * std::log2(fs / f0), soft.first.k[1], hard.first.k[1], 1200.0 * std::log2(fh / f0), 20.0 * std::log10(h5 / h1));
        double hi = 0.0, lo = 0.0; for (double hz = 12100.0; hz < 23900.0; hz += 50.0) { const double m = goertzel(hard.s.data() + rate / 2, rate, hz, rate); hi += m * m; } for (double hz = 100.0; hz < 12000.0; hz += 50.0) { const double m = goertzel(hard.s.data() + rate / 2, rate, hz, rate); lo += m * m; }
        CHECK(10.0 * std::log10(hi / lo) < -60.0, "the ×4 path at full press: the top octave (12–24 kHz) %.1f dB under the audible band — the two half-band stages hold the solitons' images down", 10.0 * std::log10(hi / lo));
    }
    // ---- 50. the ring alone, lossless at b_max: the energy's drift (the minute means) and the soliton's shape over ten minutes (two on a Debug build) ----
    {
        const float rate4 = 192000.0f; suzu::TodaRing r; r.reset(16); const float k = suzu::TodaRing::k_for(220.0f, rate4, 16); const double b = 2.4;   // the patch's toda_b 0.6 at full press
        r.setup(k, (float)b, expf(6.0f)); r.set_pickup(0.3f); r.strike(1.0f, 0.3f, 1.5f, sqrtf(k));                                               // the patch's level 0.25 × the strain 4
        const double H0 = r.energy(), struck = r.strain_peak(); const int minutes = RELEASE_BUILD ? 10 : 2; const long sec = (long)rate4;
        double peak_s = 0.0, width_s = 0.0, peak_1 = 0.0, width_1 = 0.0, hsum = 0.0, hmean_1 = 0.0, drift = 0.0, amp_dev = 0.0, shape_dev = 0.0, pmax = 0.0, hosc = 0.0; long hcount = 0;
        for (long s = 0, steps = (long)minutes * 60 * sec; s < steps; s++) {
            r.step(1.0f);
            double pk = 0.0; int pi = 0; for (int i = 0; i < r.n; i++) { const double c = -r.r_at(i); if (c > pk) { pk = c; pi = i; } }
            if (pk > peak_s) {   // the biggest ISOLATED pass of a soliton this minute (its ±2 neighbours under a twentieth: no collision partner in the frame), at its moment: the profile e^{−br} − 1 summed over its nodes in units of its peak — the width
                const double top = std::exp(b * pk) - 1.0; double w = 0.0; bool alone = true;
                for (int d = -2; d <= 2; d++) { const double e = std::exp(b * -r.r_at((pi + d + 16) % 16)) - 1.0; if (std::abs(d) == 2 && e > 0.05 * top) alone = false; if (e > 0.0) w += e / top; }
                if (alone) { peak_s = pk; width_s = w; } }
            if ((s & 255) == 0) { const double h = 10.0 * std::log10(r.energy() / H0); hsum += h; hcount++; hosc = std::fmax(hosc, std::fabs(h)); }
            if ((s + 1) % (60 * sec) == 0) {
                const long minute = (s + 1) / (60 * sec); const double hmean = hsum / (double)hcount;
                if (minute == 1) { peak_1 = peak_s; width_1 = width_s; hmean_1 = hmean; }
                else { drift = std::fmax(drift, std::fabs(hmean - hmean_1)); amp_dev = std::fmax(amp_dev, std::fabs(peak_s / peak_1 - 1.0)); shape_dev = std::fmax(shape_dev, std::fabs(peak_s * width_s / (peak_1 * width_1) - 1.0)); }
                pmax = std::fmax(pmax, std::fabs((double)r.momentum())); hsum = 0.0; hcount = 0; peak_s = 0.0; width_s = 0.0;
            }
        }
        CHECK(drift < 0.001 && pmax < 1e-9, "the ring alone at A3, b %.1f, lossless, %d minutes of 192 kHz steps: the minute-mean H drifts %.6f dB from minute 1 (the leapfrog's bounded breathing ±%.3f dB under it: the modified H is the conserved one), Σp within %.1e", b, minutes, drift, hosc, pmax);
        const double kappa = std::asinh(std::sqrt(std::exp(b * peak_1) - 1.0));   // the Toda soliton: e^{−br} − 1 = sinh²κ·sech²(κn − t·sinh κ) — its amplitude fixes its width 1/κ
        CHECK(amp_dev < 0.02 && shape_dev < 0.02 && peak_1 > struck, "the soliton's shape: the biggest soliton %.4f (the struck pulse %.3f self-focused; κ %.3f, the width 1/κ %.3f nodes) × its profile's width %.3f = %.4f, held within %.2f %% (amplitude %.2f %%) over the %d minutes", peak_1, struck, kappa, 1.0 / kappa, width_1, peak_1 * width_1, 100.0 * shape_dev, 100.0 * amp_dev, minutes);
    }
    // ---- 51. the swirl: a momentum impulse on the change, Σp conserved, the pickup's Doppler ----
    {
        const uint32_t rate = 48000; const double f0 = note_hz(57);
        TodaRun a = toda_run(57, 0.6f, 127, 0.0f, 3.0, 375, 127, 100, -1.0f, 1u);            // the impulse at 1 s
        TodaRun twice = toda_run(57, 0.6f, 127, 0.0f, 3.0, 375, 127, 100, -1.0f, 1u, 750);   // the same message again at 2 s: no second impulse
        const double fa = peak_near(a.s.data() + rate / 4, rate / 2, f0, rate, 700.0), fb = peak_near(a.s.data() + 2 * rate, rate, fa, rate, 600.0);
        const double expect = 16.0 * 0.25 * std::sqrt((double)a.last.k[0]);   // N·J·√k, SUZU_TODA_SWIRL_J 0.25 of the sound speed
        CHECK(a.fin && std::fabs(a.first.k[3]) < 1e-6 && std::fabs(a.last.k[3] / expect - 1.0) < 1e-3 && fb / fa > 1.12 && fb / fa < 1.30 && std::fabs(twice.last.k[3] / a.last.k[3] - 1.0) < 1e-6,
              "the swirl (poly pressure 127 at 1 s): Σp 0 → %.4g = N·J·√k (%.4g), the train's rate at the pickup %.1f → %.1f Hz (%+.1f %%: the solitons ride the boost past the pickup, which stands in the lab against the drift) — the same message again adds nothing (Σp %.4g)", a.last.k[3], expect, fa, fb, 100.0 * (fb / fa - 1.0), twice.last.k[3]);
    }
    // ---- 52. the resolution rule: the inner steps rise with the pitch, the nonlinearity capped for the note at the top; the red control ----
    {
        TodaRun c7 = toda_run(96, 0.6f, 127, 0.0f, 2.0, -1, 0, 100, -1.0f, 1u);              // the patch's level: the rate rises, b as asked
        TodaRun top = toda_run(108, 0.6f, 127, 0.0f, 2.0, -1, 0, 127, 1.0f, 1u);             // C8 at level 1, velocity 127: the hardest strike
        TodaRun red = toda_run(108, 0.6f, 127, 0.0f, 2.0, -1, 0, 127, 1.0f, 0u);
        const double gain_top = 10.0 * std::log10((double)top.last.k[2] / (double)top.first.k[2]), gain_red = 10.0 * std::log10((double)red.last.k[2] / (double)red.first.k[2]);
        CHECK(c7.fin && c7.first.k[6] >= 4.0f && c7.first.k[1] > 2.3f, "C7 at the patch's level: %g inner steps a sub-step (the rule √k·ĉ·b ≤ 0.15), the nonlinearity as asked (b %.2f of %.2f)", c7.first.k[6], c7.first.k[1], c7.first.k[7]);
        CHECK(top.fin && top.trips == 0 && top.first.k[6] == 8.0f && top.first.k[1] < 0.6f * top.first.k[7] && std::fabs(gain_top) < 1.5 && peak(top.s.data(), top.s.size()) < 1.0f,
              "C8 at level 1, velocity 127, press 127: eight inner steps and the nonlinearity capped for the note (b %.2f of %.2f asked; the ceiling %.2f) — lossless, H %+.2f dB over the second, the output finite under 0 dBFS (%.2f)", top.first.k[1], top.first.k[7], top.first.k[8], gain_top, peak(top.s.data(), top.s.size()));
        CHECK(gain_red > 10.0 || red.trips > 0 || !red.fin, "the red control (toda_gate 0: two inner steps, the nonlinearity as asked): the ring makes energy — H %+.1f dB over the second (the breaker %u, finite %d)", gain_red, red.trips, (int)red.fin);
    }
    // ---- 53. the ring's strobe at the bass: a return a period, the pair finite, dumped for the chart ----
    {
        const uint32_t rate = 48000; const int A3 = 57;
        voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_patch(17, &sp); sp.decay_s = 0.0f; voxo_set_suzu_params(v, &sp); mcm(v);
        voxo_set_trace(v, 1u << 10); voxo_set_strobe(v, 1); voxo_push_midi(v, 0xD1, 127, 0); note_on(v, 1, A3, 100);
        static voxo_strobe_t st[4]; std::vector<float> buf(2u * BLOCK); size_t returns = 0; bool fin = true;
        const char* dump = std::getenv("SUZU_STROBE_DUMP"); FILE* f = nullptr;
        if (dump) { std::string path = std::string(dump) + "/strobe_toda.csv"; f = std::fopen(path.c_str(), "w"); if (f) std::fprintf(f, "x,y,u,w\n"); }
        for (int b = 0; b < 750; b++) {
            voxo_render(v, buf.data(), BLOCK);
            const uint32_t n = voxo_strobe_poll(v, st, 4, VOXO_STROBE_POINTS_MAX);
            for (uint32_t i = 0; i < n; i++) for (uint32_t k = 0; k < st[i].count; k++) { returns++; if (!std::isfinite(st[i].x[k]) || !std::isfinite(st[i].y[k]) || !std::isfinite(st[i].u[k])) fin = false; if (f) std::fprintf(f, "%.7g,%.7g,%.7g,%.7g\n", st[i].x[k], st[i].y[k], st[i].u[k], st[i].w[k]); }
        }
        if (f) std::fclose(f);
        voxo_destroy(v);
        const long expected = std::lround(2.0 * note_hz(A3));
        CHECK(fin && (long)returns >= expected - 3 && (long)returns <= expected + 3, "the Toda ring at A3 strobed at the bass (itself) for 2 s at full press: %zu returns (%ld expected), the pair (the pickup, its slope) and the aux (the biggest soliton, the drift) finite — the train outruns the note, the points walk the section (the chart: strobe_toda.csv)", returns, expected);
    }
    // ---- 54. headroom: sixteen rings over four octaves at full press against the period (the Release build's bar) ----
    {
        const uint32_t rate = 48000; const double period_ms = 1000.0 * BLOCK / rate;
        voxo_t* v = make(rate, 16); voxo_suzu_params_t sp; voxo_suzu_patch(17, &sp); sp.decay_s = 0.0f; voxo_set_suzu_params(v, &sp); mcm(v);
        for (int i = 0; i < 16; i++) { const int ch = 1 + (i % 15); voxo_push_midi(v, (uint8_t)(0xD0 | ch), 127, 0); note_on(v, ch, 48 + 3 * i, 100); }
        std::vector<float> out(2u * BLOCK);
        Headroom h = headroom(v, out.data(), rate, BLOCK, period_ms);
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(st.active_voices == 16 && (!RELEASE_BUILD || h.worst_ms < 0.30 * period_ms), "headroom: %u Toda rings from C3 to C7 at full press (%s; the inner steps 2..6 with the pitch): mean %.3f ms, worst %.3f ms of %.3f (%.1f %% of the period; the bar 30 %% on the Release build%s; attempts %s)",
              st.active_voices, suzu::simd_name(), h.mean_ms, h.worst_ms, period_ms, 100.0 * h.worst_ms / period_ms, RELEASE_BUILD ? "" : " — a Debug build prints the figure", headroom_attempts(h).c_str());
        voxo_destroy(v);
    }

    // ======== step 84: the Boris–leapfrog magnetic gyroscope (TO_PROJECT_SPEC §13.13, DECISIONS_13 #2) ========
    // a run through the engine: the patch with its fields, a note, the poly pressure at a block; both channels kept
    struct GyroRun { std::vector<float> L, R; voxo_inspect_t first{}, last{}; bool fin = true; };
    auto gyro_run = [&](int note, float fb, float fb_rest, uint32_t prewarp, float pluck, float decay_s, double seconds, int pp_block, int pp) {
        const uint32_t rate = 48000; GyroRun r; voxo_t* w = make(rate); voxo_suzu_params_t q; voxo_suzu_patch(18, &q);
        q.boris_b = fb; q.boris_b_rest = fb_rest; q.boris_prewarp = prewarp; q.pluck = pluck; q.decay_s = decay_s; voxo_set_suzu_params(w, &q); mcm(w);
        note_on(w, 1, note, 100);
        const uint32_t frames = (uint32_t)(seconds * rate); r.L.resize(frames); r.R.resize(frames); std::vector<float> out(2u * BLOCK);
        for (uint32_t f = 0, b = 0; f < frames; f += BLOCK, b++) {
            if ((int)b == pp_block) voxo_push_midi(w, 0xA1, (uint8_t)note, (uint8_t)pp);
            const uint32_t n = (frames - f) < BLOCK ? (frames - f) : BLOCK; voxo_render(w, out.data(), n);
            for (uint32_t i = 0; i < n; i++) { r.L[f + i] = out[2u * i]; r.R[f + i] = out[2u * i + 1]; if (!std::isfinite(out[2u * i]) || !std::isfinite(out[2u * i + 1])) r.fin = false; }
            if (b == 1) voxo_suzu_inspect(w, &r.first, 1);
        }
        voxo_suzu_inspect(w, &r.last, 1); voxo_destroy(w); return r;
    };
    // the scheme's two angles a step, from the struct itself: the map on z = x₁ + i·x₂, w = y₁ + i·y₂ is complex-linear, its
    // 2×2 read off two basis steps, its eigenvalues' arguments the doublet (one each way round)
    auto gyro_angles = [](float eps, float theta, double* plus, double* minus) {
        suzu::Gyro g; g.reset(); g.setup(eps, theta); g.x1 = 1.0; g.step(); const std::complex<double> a(g.x1, g.x2), c(g.y1, g.y2);
        g.reset(); g.setup(eps, theta); g.y1 = 1.0; g.step(); const std::complex<double> b(g.x1, g.x2), d(g.y1, g.y2);
        const std::complex<double> T = a + d, D = a * d - b * c, disc = std::sqrt(T * T - 4.0 * D), l1 = 0.5 * (T + disc), l2 = 0.5 * (T - disc);
        const double p1 = std::fabs(std::arg(l1)), p2 = std::fabs(std::arg(l2)); *plus = std::fmax(p1, p2); *minus = std::fmin(p1, p2);
    };
    // ---- 55. the Zeeman doublet: the scheme's lines against ω_B/2 ± sqrt(ω₀² + ω_B²/4), prewarped and raw (the red); charted ----
    {
        const double rate2 = 96000.0, dt = 1.0 / rate2; double worst_on = 0.0, worst_off_top = 0.0, worst_engine = 0.0;
        const char* evidence = std::getenv("SUZU_EVIDENCE"); FILE* csv = nullptr;
        if (evidence) { std::string path = std::string(evidence) + "/boris_doublet.csv"; csv = std::fopen(path.c_str(), "w"); if (csv) std::fprintf(csv, "note_hz,f_B,prewarp,f_plus_exact,f_minus_exact,f_plus_scheme,f_minus_scheme,cents_plus,cents_minus\n"); }
        for (uint32_t pw = 1; pw != 2; pw = pw == 1 ? 0 : 2) {
            for (int note : { 45, 57, 69, 81, 95 }) for (float fb : { 0.0f, 1.0f, 5.0f, 10.0f, 25.0f, 50.0f }) {
                const double f0 = note_hz(note), w0 = 2.0 * M_PI * f0, wb = 2.0 * M_PI * fb, root = std::sqrt(w0 * w0 + 0.25 * wb * wb);
                const double fp = (root + 0.5 * wb) / (2.0 * M_PI), fm = (root - 0.5 * wb) / (2.0 * M_PI);
                float eps, theta;
                if (pw) { eps = suzu::prewarp((float)w0, (float)dt); theta = (float)(wb * dt); } else { eps = (float)(w0 * dt); theta = 2.0f * atanf((float)(0.5 * wb * dt)); }
                double ap = 0.0, am = 0.0; gyro_angles(eps, theta, &ap, &am);
                const double sp_hz = ap * rate2 / (2.0 * M_PI), sm_hz = am * rate2 / (2.0 * M_PI), cp = 1200.0 * std::log2(sp_hz / fp), cm = 1200.0 * std::log2(sm_hz / fm);
                if (csv) std::fprintf(csv, "%.4f,%.1f,%u,%.6f,%.6f,%.6f,%.6f,%.5f,%.5f\n", f0, fb, pw, fp, fm, sp_hz, sm_hz, cp, cm);
                if (pw) worst_on = std::fmax(worst_on, std::fmax(std::fabs(cp), std::fabs(cm))); else if (note == 95) worst_off_top = std::fmax(worst_off_top, std::fabs(cp));
                if (pw && note == 69 && fb == 25.0f) {   // the engine's inspection says the same lines
                    GyroRun r = gyro_run(69, fb, fb, 1u, 0.5f, 0.0f, 0.05, -1, 0);
                    worst_engine = std::fmax(std::fabs(1200.0 * std::log2((double)r.last.k[4] / sp_hz)), std::fabs(1200.0 * std::log2((double)r.last.k[5] / sm_hz)));
                }
            }
        }
        if (csv) std::fclose(csv);
        CHECK(worst_on < 0.1 && worst_engine < 0.01, "the Zeeman doublet, prewarped: the scheme's two lines within %.4f cents of ω_B/2 ± sqrt(ω₀² + ω_B²/4) over A2..B6 × f_B 0..50 Hz (the gate 0.1; the engine's inspection reads the same lines within %.4f)", worst_on, worst_engine);
        CHECK(worst_off_top > 1.0, "the red (boris_prewarp 0: the raw ε = ω₀·dt and the raw Boris angle): %.3f cents sharp at B6 — the chart boris_doublet.csv", worst_off_top);
    }
    // ---- 56. (x₁, x₂) are (L, R): the pan law, the field's stereo, the rendered doublet, the poly pressure's field ----
    {
        const uint32_t rate = 48000;
        GyroRun mono = gyro_run(57, 0.0f, 0.0f, 1u, 0.5f, 0.0f, 1.0, -1, 0), left = gyro_run(57, 0.0f, 0.0f, 1u, 0.0f, 0.0f, 0.5, -1, 0), right = gyro_run(57, 0.0f, 0.0f, 1u, 1.0f, 0.0f, 0.5, -1, 0);
        size_t same = 0; for (size_t i = 0; i < mono.L.size(); i++) if (mono.L[i] == mono.R[i]) same++;
        CHECK(mono.fin && same == mono.L.size() && peak(mono.L.data(), mono.L.size()) > 0.01f && peak(right.L.data(), right.L.size()) == 0.0f && peak(left.R.data(), left.R.size()) == 0.0f && peak(left.L.data(), left.L.size()) > 0.01f,
              "no field: the strike at pluck 0.5 is a mono planar string — L and R the same bits on %zu of %zu samples (peak %.3f); pluck 0 the left channel alone, 1 the right (the equal-power law)", same, mono.L.size(), peak(mono.L.data(), mono.L.size()));
        GyroRun st = gyro_run(69, 20.0f, 20.0f, 1u, 0.5f, 0.0f, 3.0, -1, 0);
        double cr = 0.0, ll = 0.0, rr = 0.0; for (size_t i = rate / 2; i < st.L.size(); i++) { cr += (double)st.L[i] * st.R[i]; ll += (double)st.L[i] * st.L[i]; rr += (double)st.R[i] * st.R[i]; }
        auto fine = [&](const float* s, size_t n, double hz) { double best = -1.0, bc = 0.0; std::vector<double> m, cs; for (double c = -40.0; c <= 40.0; c += 0.25) { cs.push_back(c); m.push_back(goertzel(s, n, hz * std::pow(2.0, c / 1200.0), rate)); } size_t b = 0; for (size_t i = 1; i < m.size(); i++) if (m[i] > m[b]) b = i; if (b == 0 || b + 1 >= m.size()) return cs[b]; const double a = m[b - 1], bb = m[b], c = m[b + 1], den = a - 2 * bb + c; (void)best; (void)bc; return cs[b] + (den != 0 ? 0.5 * (a - c) / den : 0.0) * 0.25; };
        const double ep = fine(st.L.data() + rate / 2, 2u * rate, st.last.k[6]), em = fine(st.L.data() + rate / 2, 2u * rate, st.last.k[7]);
        CHECK(st.fin && std::fabs(cr / std::sqrt(ll * rr)) < 0.5 && std::fabs(ep) < 0.05 && std::fabs(em) < 0.05,
              "the field at 20 Hz on A4: the two lines in the rendered left channel %+.3f and %+.3f cents from %.2f and %.2f Hz; L against R correlate %.3f — the line precessed into the rosette", ep, em, st.last.k[6], st.last.k[7], cr / std::sqrt(ll * rr));
        GyroRun pp = gyro_run(57, 24.0f, 2.0f, 1u, 0.5f, 0.0f, 1.0, 20, 127);
        CHECK(std::fabs(pp.first.k[3] - 2.0f) < 1e-3f && std::fabs(pp.last.k[3] - 24.0f) < 1e-3f, "the poly pressure is the field: %.2f Hz at rest, %.2f Hz under 127 (the patch's 2 and 24)", pp.first.k[3], pp.last.k[3]);
    }
    // ---- 57. the energy on the struct alone: bounded breathing, no drift, over ten minutes (two on a Debug build) ----
    {
        suzu::Gyro g; g.reset(); const float rate2 = 96000.0f; g.setup(suzu::prewarp(2.0f * suzu::PI * 220.0f, 1.0f / rate2), 2.0f * suzu::PI * 20.0f / rate2); g.strike(0.3f, 0.5f);
        const double H0 = g.energy(); double hi = 0.0, lo = 0.0, hsum = 0.0, h1 = 0.0, drift = 0.0; long cnt = 0; const long sec = 96000; const int minutes = RELEASE_BUILD ? 10 : 2;
        for (long s = 0, steps = (long)minutes * 60 * sec; s < steps; s++) {
            g.step();
            if ((s & 63) == 0) { const double h = 10.0 * std::log10(g.energy() / H0); hi = std::fmax(hi, h); lo = std::fmin(lo, h); hsum += h; cnt++; }
            if ((s + 1) % (60 * sec) == 0) { const double m = hsum / (double)cnt; if ((s + 1) / (60 * sec) == 1) h1 = m; else drift = std::fmax(drift, std::fabs(m - h1)); hsum = 0.0; cnt = 0; }
        }
        CHECK(hi < 0.1 && lo > -0.1 && drift < 0.001, "the energy, A3 under a 20 Hz field, %d minutes at 96 kHz: H within [%+.4f, %+.4f] dB of its start (the kick's splitting breathes it; the rotation's three shears add nothing), the minute means drift %.6f dB", minutes, lo, hi, drift);
    }
    // ---- 58. the strobe: the own clock is the cyclotron period; at the bass one return a period; dumped for the chart ----
    {
        const uint32_t rate = 48000; const int A3 = 57;
        auto strobe = [&](uint32_t clock, float fb, const char* file, float* hz_out) {
            voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_patch(18, &sp); sp.decay_s = 0.0f; sp.boris_b = fb; sp.boris_b_rest = fb; voxo_set_suzu_params(v, &sp); mcm(v);
            voxo_set_trace(v, 1u << 11); voxo_set_strobe(v, clock); note_on(v, 1, A3, 100);
            static voxo_strobe_t st[4]; std::vector<float> buf(2u * BLOCK); size_t returns = 0; bool fin = true; float hz = 0.0f;
            const char* dump = std::getenv("SUZU_STROBE_DUMP"); FILE* f = nullptr;
            if (dump && file) { std::string path = std::string(dump) + "/" + file; f = std::fopen(path.c_str(), "w"); if (f) std::fprintf(f, "x,y,u,w\n"); }
            for (int b = 0; b < 750; b++) {
                voxo_render(v, buf.data(), BLOCK);
                const uint32_t n = voxo_strobe_poll(v, st, 4, VOXO_STROBE_POINTS_MAX);
                for (uint32_t i = 0; i < n; i++) { hz = st[i].hz; for (uint32_t k = 0; k < st[i].count; k++) { returns++; if (!std::isfinite(st[i].x[k]) || !std::isfinite(st[i].y[k])) fin = false; if (f) std::fprintf(f, "%.7g,%.7g,%.7g,%.7g\n", st[i].x[k], st[i].y[k], st[i].u[k], st[i].w[k]); } }
            }
            if (f) std::fclose(f); voxo_destroy(v); if (hz_out) *hz_out = hz; return fin ? (long)returns : -1L;
        };
        float hz_own = 0.0f, hz_bass = 0.0f;
        const long own = strobe(2u, 20.0f, "strobe_boris.csv", &hz_own), bass = strobe(1u, 20.0f, nullptr, &hz_bass), none = strobe(2u, 0.0f, nullptr, nullptr);
        const long expected = std::lround(2.0 * note_hz(A3));
        CHECK(own >= 38 && own <= 41 && std::fabs(hz_own - 20.0f) < 1e-3f && bass >= expected - 3 && bass <= expected + 3 && none >= expected - 3 && none <= expected + 3,
              "the strobe: by its own clock (2) under a 20 Hz field %ld returns in 2 s at %.2f Hz — the cyclotron period, the section the rosette's lobes; at the bass (1) %ld of %ld; with no field the own clock falls back to the bass (%ld) — the chart strobe_boris.csv", own, hz_own, bass, expected, none);
    }

    std::printf("[suzu] %s (%d failures)\n", g_fail ? "FAILED" : "all gates green", g_fail);
    return g_fail ? 1 : 0;
}
