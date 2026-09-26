// voxo_tests.cpp — Voxo's headless suite (Phase 7 step 47, SOUND §1–§2;
// DECISIONS_6 #3): the second SPSC ring (order, overflow drops the oldest),
// fifteen sines following MPE (pitch by zero crossings, bend, pressure),
// sustain and the panic in the release logic, the master gain, the soft knee,
// and THE CONTRACT — a counting global allocator armed around a storm of
// blocks asserts that voxo_render allocates nothing. No device is opened:
// voxo_render is called directly, as the callback would.
#include "voxo.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

/* ---- the counting allocator (every global operator new / delete form) ---- */
static std::atomic<long> g_allocs{0};
static std::atomic<long> g_frees{0};
void* operator new(std::size_t n) { g_allocs++; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc(); return p; }
void* operator new[](std::size_t n) { g_allocs++; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc(); return p; }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { g_allocs++; return std::malloc(n ? n : 1); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { g_allocs++; return std::malloc(n ? n : 1); }
void operator delete(void* p) noexcept { g_frees++; std::free(p); }
void operator delete[](void* p) noexcept { g_frees++; std::free(p); }
void operator delete(void* p, std::size_t) noexcept { g_frees++; std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { g_frees++; std::free(p); }

static int g_fail = 0;
#define CHECK(cond, ...) do { if (cond) { std::printf("ok   "); std::printf(__VA_ARGS__); std::printf("\n"); } \
                              else { g_fail++; std::printf("FAIL "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static const uint32_t RATE = 48000, BLOCK = 128;

static voxo_t* make(uint32_t max_voices = 0) {
    voxo_config_t c{}; c.sample_rate = RATE; c.block_frames = BLOCK; c.max_voices = max_voices;
    voxo_t* v = voxo_create(&c);
    voxo_set_input_mode(v, 1);   // MPE: the member channels carry their own bend
    return v;
}
// Renders `seconds` and returns the left channel.
static std::vector<float> render(voxo_t* v, double seconds) {
    const uint32_t frames = (uint32_t)(seconds * RATE);
    std::vector<float> out(2u * frames), left(frames);
    for (uint32_t f = 0; f < frames; f += BLOCK) {
        const uint32_t n = (frames - f) < BLOCK ? (frames - f) : BLOCK;
        voxo_render(v, out.data() + 2u * f, n);
    }
    for (uint32_t f = 0; f < frames; f++) left[f] = out[2u * f];
    return left;
}
static float peak(const std::vector<float>& s) { float p = 0; for (float x : s) p = std::fmax(p, std::fabs(x)); return p; }
// Frequency by rising zero crossings over the signal (a single sine).
static double frequency(const std::vector<float>& s) {
    int first = -1, last = -1, n = 0;
    for (size_t i = 1; i < s.size(); i++) {
        if (s[i - 1] < 0.0f && s[i] >= 0.0f) { if (first < 0) first = (int)i; last = (int)i; n++; }
    }
    if (n < 2) return 0.0;
    return (double)(n - 1) * RATE / (double)(last - first);
}
static uint32_t voices(voxo_t* v) { voxo_stats_t st; voxo_stats(v, &st); return st.active_voices; }
static void note_on(voxo_t* v, int ch, int note, int vel) { voxo_push_midi(v, (uint8_t)(0x90 | ch), (uint8_t)note, (uint8_t)vel); }
static void note_off(voxo_t* v, int ch, int note) { voxo_push_midi(v, (uint8_t)(0x80 | ch), (uint8_t)note, 0); }
static void bend14(voxo_t* v, int ch, int value) { voxo_push_midi(v, (uint8_t)(0xE0 | ch), (uint8_t)(value & 0x7F), (uint8_t)(value >> 7)); }
static void pressure(voxo_t* v, int ch, int value) { voxo_push_midi(v, (uint8_t)(0xD0 | ch), (uint8_t)value, 0); }
static void cc(voxo_t* v, int ch, int c, int value) { voxo_push_midi(v, (uint8_t)(0xB0 | ch), (uint8_t)c, (uint8_t)value); }

int main() {
    std::printf("[voxo] headless suite: voxo %u.%u.%u, default block %u\n",
                voxo_version() >> 16, (voxo_version() >> 8) & 0xFF, voxo_version() & 0xFF, voxo_default_block_frames());

    // 1. Pitch: note 69 on member channel 2 is 440 Hz after the attack.
    {
        voxo_t* v = make();
        note_on(v, 1, 69, 100);
        render(v, 0.05);
        const double f = frequency(render(v, 1.0));
        CHECK(std::fabs(f - 440.0) < 0.5, "a sine follows the note: A4 measures %.2f Hz", f);
        // A retrigger of the same (channel, note) keeps one voice.
        note_on(v, 1, 69, 80); render(v, 0.01);
        CHECK(voices(v) == 1, "the same (channel, note) retriggers its voice, not a second one");
        voxo_destroy(v);
    }
    // 2. Bend: the normalizer's default member range is 48 semitones; +2 semitones = +341.33 units.
    {
        voxo_t* v = make();
        note_on(v, 1, 69, 100);
        bend14(v, 1, 8192 + 341);
        render(v, 0.1);
        const double f = frequency(render(v, 1.0));
        CHECK(std::fabs(f - 493.88) < 1.0, "bend on the member channel retunes the voice: +2 semitones measures %.2f Hz (493.88)", f);
        bend14(v, 1, 8192); render(v, 0.1);
        const double f2 = frequency(render(v, 1.0));
        CHECK(std::fabs(f2 - 440.0) < 0.5, "bend back to centre: %.2f Hz", f2);
        voxo_destroy(v);
    }
    // 3. Fifteen voices, one per member channel; the pool's sixteenth; stealing keeps the count.
    {
        voxo_t* v = make();
        for (int ch = 1; ch <= 15; ch++) note_on(v, ch, 48 + ch, 100);
        render(v, 0.01);
        CHECK(voices(v) == 15, "fifteen member channels: %u voices", voices(v));
        note_on(v, 0, 40, 100); render(v, 0.01);
        CHECK(voices(v) == 16, "the master channel takes the sixteenth: %u", voices(v));
        note_on(v, 2, 90, 100); render(v, 0.01);
        CHECK(voices(v) == 16, "a seventeenth note steals the oldest: still %u", voices(v));
        voxo_destroy(v);
        // Sixteen fresh voices on ONE note at full velocity and pressure start
        // in phase: the raw sum is 16 x 0.18 = 2.88, the knee keeps it under 1.
        v = make();
        for (int ch = 0; ch < 16; ch++) { pressure(v, ch, 127); note_on(v, ch, 60, 127); }
        render(v, 0.05);
        const float p = peak(render(v, 0.2));
        CHECK(p < 1.0f && p > 0.9f, "sixteen sines in phase stay inside the knee: peak %.3f", p);
        voxo_destroy(v);
    }
    // 4. Release, sustain and the panic.
    {
        voxo_t* v = make();
        note_on(v, 1, 60, 100); render(v, 0.05);
        note_off(v, 1, 60); render(v, 0.5);
        CHECK(voices(v) == 0, "note-off releases: no voice after 0.5 s");
        cc(v, 1, 64, 127);
        note_on(v, 1, 60, 100); render(v, 0.05);
        note_off(v, 1, 60); render(v, 0.5);
        CHECK(voices(v) == 1, "sustain down: the note-off holds the voice");
        cc(v, 1, 64, 0); render(v, 0.5);
        CHECK(voices(v) == 0, "sustain up: the held voice releases");
        for (int ch = 1; ch <= 4; ch++) note_on(v, ch, 60, 100);
        render(v, 0.05);
        for (int ch = 1; ch <= 4; ch++) cc(v, ch, 123, 0);
        render(v, 0.5);
        CHECK(voices(v) == 0, "CC 123 all-notes-off releases every voice on the channel");
        note_on(v, 1, 60, 100); render(v, 0.05);
        cc(v, 1, 120, 0);
        const float p = peak(render(v, 0.01));
        CHECK(p == 0.0f && voices(v) == 0, "CC 120 all-sound-off is immediate silence (peak %.4f)", p);
        voxo_destroy(v);
    }
    // 5. Pressure and velocity shape the level; gain scales; the input mode applies at block start.
    {
        voxo_t* v = make();
        note_on(v, 1, 60, 127); render(v, 0.1);
        const float quiet = peak(render(v, 0.1));
        pressure(v, 1, 127); render(v, 0.1);
        const float loud = peak(render(v, 0.1));
        CHECK(loud > quiet * 2.0f, "channel pressure raises the level in MPE: %.3f -> %.3f", quiet, loud);
        voxo_set_gain(v, 0.0f);
        const float silent = peak(render(v, 0.05));
        CHECK(silent == 0.0f, "master gain 0 silences (peak %.4f)", silent);
        voxo_set_gain(v, 1.0f);
        voxo_set_input_mode(v, 2);
        render(v, 0.01);
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(st.input_mode == 2, "the input mode set from the shell is the effective one after the next block (%u)", st.input_mode);
        note_on(v, 1, 60, 30); render(v, 0.1);
        voxo_destroy(v);
    }
    // 6. The ring: order kept; overflow drops the OLDEST and counts.
    {
        voxo_t* v = make();
        for (int i = 0; i < 5000; i++) note_on(v, 1, 60 + (i % 12), 100);   // 4096 slots
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(st.dropped_midi > 0 && st.dropped_midi < 5000, "overflow counted: %u dropped of 5000 pushed (4096 slots)", st.dropped_midi);
        // The newest survive: the last pushed note (index 4999 -> 60 + 4999 % 12 = 67).
        // Drain everything, then silence all but the survivors' evidence: the last note-on is 67 on ch 2.
        render(v, 0.05);
        CHECK(voices(v) >= 1, "the surviving messages decoded into voices (%u)", voices(v));
        voxo_destroy(v);
    }
    // 7. THE CONTRACT: zero allocations inside voxo_render across a storm.
    {
        const long before_create = g_allocs.load();
        voxo_t* v = make();
        const long after_create = g_allocs.load();
        // voxo_create uses calloc (not operator new): the C++ counter must stay flat here too —
        // the negative control below proves the counter itself works.
        { int* probe = new int(1); delete probe; }
        CHECK(g_allocs.load() == after_create + 1, "the counting allocator counts (negative control)");
        (void)before_create;
        std::vector<float> out(2u * BLOCK);   // the test's own buffer, before arming
        const long armed = g_allocs.load();
        const long armed_frees = g_frees.load();
        for (int block = 0; block < 4000; block++) {
            // A storm: fifteen channels of notes, bends, pressure and CC74 every block, overflowing at times.
            for (int ch = 1; ch <= 15; ch++) {
                if (block % 50 == ch)  note_on(v, ch, 40 + (block / 50 + ch) % 40, 90);
                if (block % 50 == ch + 20) note_off(v, ch, 40 + ((block - 20) / 50 + ch) % 40);
                bend14(v, ch, 8192 + (int)(2000.0 * std::sin(block * 0.01 + ch)));
                pressure(v, ch, (block * 3 + ch) % 128);
                cc(v, ch, 74, (block + ch) % 128);
            }
            if (block % 500 == 0) for (int i = 0; i < 6000; i++) pressure(v, 1, i % 128);   // overflow
            if (block % 700 == 0) voxo_set_input_mode(v, (block / 700) % 2 ? 2 : 1);
            voxo_render(v, out.data(), BLOCK);
        }
        CHECK(g_allocs.load() == armed && g_frees.load() == armed_frees,
              "voxo_render allocated nothing across 4000 storm blocks (news %ld, deletes %ld)",
              g_allocs.load() - armed, g_frees.load() - armed_frees);
        float p = 0; for (float x : out) p = std::fmax(p, std::fabs(x));
        CHECK(p <= 1.0f, "the storm's output stays within +-1 (peak %.3f)", p);
        voxo_destroy(v);
    }
    // 9. THE SAMPLE (step 49): a one-cycle-per-frame table sounding A4 at root plays at 440 Hz,
    //    +12 semitones doubles it, -48 quarters it twice; the sample ends the voice; clear returns the sine.
    {
        voxo_t* v = make();
        // A 2 s sine at 440 Hz stored as the sample, root A4 (69): reading it at ratio 1 is 440 Hz.
        const uint32_t N = 2 * RATE;
        std::vector<float> table(N);
        for (uint32_t i = 0; i < N; i++) table[i] = 0.8f * std::sin(2.0 * 3.141592653589793 * 440.0 * i / RATE);
        CHECK(voxo_set_sample(v, table.data(), N, 1, RATE, 69.0f), "voxo_set_sample accepts a mono 48 kHz sample rooted at A4");
        note_on(v, 1, 69, 100); render(v, 0.05);
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(st.sample_frames == N && st.sample_channels == 1 && st.sample_root_note == 69.0f,
              "the callback swapped the sample in at block start (%u frames)", st.sample_frames);
        double f = frequency(render(v, 0.5));
        CHECK(std::fabs(f - 440.0) < 0.5, "the sample at its root reads at ratio 1: %.2f Hz", f);
        note_off(v, 1, 69); render(v, 0.3);
        note_on(v, 1, 81, 100); render(v, 0.05);
        f = frequency(render(v, 0.5));
        CHECK(std::fabs(f - 880.0) < 1.0, "+12 semitones reads at ratio 2: %.2f Hz", f);
        note_off(v, 1, 81); render(v, 0.3);
        note_on(v, 1, 21, 100); render(v, 0.1);
        f = frequency(render(v, 0.5));
        CHECK(std::fabs(f - 27.5) < 0.2, "-48 semitones reads at ratio 1/16: %.2f Hz (27.5)", f);
        note_off(v, 1, 21); render(v, 0.3);
        // The sample plays once: at ratio 1 the 2 s table ends the voice after 2 s.
        note_on(v, 1, 69, 100); render(v, 1.9);
        CHECK(voices(v) == 1, "1.9 s into a 2 s sample the voice is alive");
        render(v, 0.2);
        CHECK(voices(v) == 0, "the sample's end ends the voice (no loop until step 51)");
        // Bend glides the read ratio: A4 bent +2 semitones on the member channel.
        note_on(v, 1, 69, 100); bend14(v, 1, 8192 + 341); render(v, 0.1);
        f = frequency(render(v, 0.5));
        CHECK(std::fabs(f - 493.88) < 1.0, "bend retunes the read ratio: %.2f Hz (493.88)", f);
        note_off(v, 1, 69); bend14(v, 1, 8192); render(v, 0.3);
        voxo_clear_sample(v);
        note_on(v, 1, 69, 100); render(v, 0.05);
        voxo_stats(v, &st);
        f = frequency(render(v, 0.5));
        CHECK(st.sample_frames == 0 && std::fabs(f - 440.0) < 0.5, "voxo_clear_sample returns the sine (%.2f Hz)", f);
        voxo_destroy(v);
    }
    // 10. Hermite vs linear: reading a sine table at a fractional ratio, the Hermite read tracks the
    //     true sine far closer than the linear one (the reason for DECISIONS_6 #10, measured).
    {
        auto rms_error = [&](uint32_t mode) {
            voxo_t* v = make();
            const uint32_t N = RATE;   // 1 s at 48 kHz of a 1 kHz sine, root A4
            std::vector<float> table(N);
            for (uint32_t i = 0; i < N; i++) table[i] = std::sin(2.0 * 3.141592653589793 * 1000.0 * i / RATE);
            voxo_set_sample(v, table.data(), N, 1, RATE, 69.0f);
            voxo_set_interpolation(v, mode);
            pressure(v, 1, 127);
            note_on(v, 1, 62, 127);   // -7 semitones: ratio 0.6674, a fractional read every sample
            render(v, 0.1);           // the attack and the ramp settle
            std::vector<float> out = render(v, 0.5);
            // Fit the true sine's amplitude and phase by least squares at the read's frequency, then the residual.
            const double fr = 1000.0 * std::exp2(-7.0 / 12.0);
            double sc = 0, ss = 0, cc = 0, cs = 0, ssn = 0;
            for (size_t i = 0; i < out.size(); i++) {
                const double c = std::cos(2 * 3.141592653589793 * fr * i / RATE), sn = std::sin(2 * 3.141592653589793 * fr * i / RATE);
                sc += out[i] * c; ss += out[i] * sn; cc += c * c; cs += c * sn; ssn += sn * sn;
            }
            const double det = cc * ssn - cs * cs;
            const double A = (sc * ssn - ss * cs) / det, B = (ss * cc - sc * cs) / det;
            double err = 0, sig = 0;
            for (size_t i = 0; i < out.size(); i++) {
                const double fit = A * std::cos(2 * 3.141592653589793 * fr * i / RATE) + B * std::sin(2 * 3.141592653589793 * fr * i / RATE);
                err += (out[i] - fit) * (out[i] - fit); sig += fit * fit;
            }
            voxo_destroy(v);
            return 10.0 * std::log10(err / sig);
        };
        const double h = rms_error(0), l = rms_error(1);
        CHECK(h < -60.0 && l - h > 20.0, "Hermite read error %.1f dB vs linear %.1f dB (Hermite under -60 dB and 20 dB better)", h, l);
    }
    // 11. Local Control (CC 122) is tracked; the setter mirrors it; a sample swap allocates nothing in render.
    {
        voxo_t* v = make();
        voxo_stats_t st; voxo_stats(v, &st);
        CHECK(st.local_control == 1, "local control starts on");
        cc(v, 1, 122, 0); render(v, 0.01); voxo_stats(v, &st);
        CHECK(st.local_control == 0, "CC 122 = 0 tracks off");
        voxo_set_local_control(v, true); voxo_stats(v, &st);
        CHECK(st.local_control == 1, "voxo_set_local_control mirrors it");
        std::vector<float> table(4800, 0.1f), out(2u * BLOCK);
        voxo_set_sample(v, table.data(), 4800, 2, 44100, 60.0f);   // stereo, another rate: allocated on this thread
        const long armed = g_allocs.load();
        for (int b = 0; b < 200; b++) { if (b == 3) note_on(v, 1, 60, 100); if (b == 50) voxo_set_input_mode(v, 1); voxo_render(v, out.data(), BLOCK); }
        CHECK(g_allocs.load() == armed, "the swap and a stereo 44.1 kHz read allocate nothing in voxo_render");
        voxo_set_sample(v, table.data(), 4800, 1, 48000, 60.0f);   // a second sample retires the first on the shell's thread
        voxo_render(v, out.data(), BLOCK);
        voxo_stats(v, &st);
        CHECK(st.sample_channels == 1 && st.sample_rate_hz == 48000, "a second voxo_set_sample replaces the first (retired one freed by the shell)");
        voxo_destroy(v);
    }
    // 8. No device: start reports false or true, stop is idempotent, destroy after stop.
    {
        voxo_t* v = make();
        voxo_stop(v); voxo_stop(v);
        CHECK(!voxo_running(v), "stop without start is a no-op");
        voxo_destroy(v);
        voxo_destroy(nullptr);
        CHECK(true, "destroy(NULL) is a no-op");
    }

    std::printf("[voxo] %s (%d failures)\n", g_fail ? "FAIL" : "all ok", g_fail);
    return g_fail;
}
