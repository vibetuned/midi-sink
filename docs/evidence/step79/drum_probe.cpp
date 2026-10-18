// drum_probe.cpp — step 79's calibration probe: the membrane's level and pitch across the keyboard, the plate's level
// under the harp chord, the cost of sixteen drums and of the plate. Built against the tree's Voxo with -O2.
#include "voxo.h"
#include "suzu.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static const uint32_t BLOCK = 128;
static voxo_t* make(uint32_t rate) {
    voxo_config_t c{}; c.sample_rate = rate; c.block_frames = BLOCK; c.max_voices = 16;
    c.log_cb = [](int, const char* m, void*) { std::printf("  [log] %s\n", m); };
    voxo_t* v = voxo_create(&c); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU); return v;
}
static void mcm(voxo_t* v) { voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15); }
static double goertzel(const float* s, size_t n, double hz, double rate) {
    const double w = 2.0 * M_PI * hz / rate, cw = 2.0 * std::cos(w); double s0 = 0, s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; i++) { const double win = 0.5 - 0.5 * std::cos(2.0 * M_PI * (double)i / (double)(n - 1)); s0 = (double)s[i] * win + cw * s1 - s2; s2 = s1; s1 = s0; }
    const double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w); return 4.0 * std::sqrt(re * re + im * im) / (double)n;
}
static double peak_near(const float* s, size_t n, double hz, double rate, double span = 300.0) {
    double best = -1, bf = hz;
    for (double c = -span; c <= span; c += 2.0) { const double f = hz * std::pow(2.0, c / 1200.0); const double m = goertzel(s, n, f, rate); if (m > best) { best = m; bf = f; } }
    return bf;
}
static std::vector<float> render_left(voxo_t* v, uint32_t rate, double seconds) {
    const uint32_t frames = (uint32_t)(seconds * rate); std::vector<float> out(2u * frames), left(frames);
    for (uint32_t f = 0; f < frames; f += BLOCK) { const uint32_t n = frames - f < BLOCK ? frames - f : BLOCK; voxo_render(v, out.data() + 2u * f, n); }
    for (uint32_t f = 0; f < frames; f++) left[f] = out[2u * f];
    return left;
}
static void t60_probe(int patch);
int main(int argc, char** argv) {
    const uint32_t rate = 48000;
    const int patch = argc > 1 ? std::atoi(argv[1]) : 11;
    std::printf("[probe] patch %d '%s'\n", patch, voxo_suzu_patch_name((uint32_t)patch));
    // 1. the level and the pitch across the keyboard
    for (int note = 24; note <= 108; note += 6) {
        voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_patch((uint32_t)patch, &sp); voxo_set_suzu_params(v, &sp); mcm(v);
        voxo_push_midi(v, 0x91, (uint8_t)note, 127);
        std::vector<float> s = render_left(v, rate, 0.5);
        float pk = 0; for (size_t i = 0; i < 4800; i++) pk = std::fmax(pk, std::fabs(s[i]));
        float pk2 = 0; for (size_t i = 4800; i < s.size(); i++) pk2 = std::fmax(pk2, std::fabs(s[i]));
        const double hz = 440.0 * std::pow(2.0, (note - 69) / 12.0);
        const double f = peak_near(s.data() + 2400, 19200, hz, rate);
        voxo_inspect_t ins; const uint32_t ni = voxo_suzu_inspect(v, &ins, 1);
        std::printf("  note %3d %8.2f Hz: peak(0-100ms) %.4f, peak(100-500) %.4f, (1,1) at %+.1f cents; grid %gx%g cfl %.3f c2 %.4g k2 %.3g alpha %.3g H %.3g\n",
                    note, hz, pk, pk2, 1200.0 * std::log2(f / hz), ni ? ins.k[0] : 0.f, ni ? ins.k[1] : 0.f, ni ? ins.k[4] : 0.f, ni ? ins.k[2] : 0.f, ni ? ins.k[3] : 0.f, ni ? ins.k[6] : 0.f, ni ? ins.k[7] : 0.f);
        voxo_destroy(v);
    }
    // 2. the plate under the harp chord: dry vs with the plate
    {
        auto chord = [&](bool plate, uint32_t size) {
            voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_patch(13, &sp); sp.plate_on = plate ? 1u : 0u; sp.plate_size = size; voxo_set_suzu_params(v, &sp); mcm(v);
            voxo_push_midi(v, 0x91, 48, 100); voxo_push_midi(v, 0x92, 55, 100); voxo_push_midi(v, 0x93, 64, 100);
            return render_left(v, rate, 2.0);
        };
        std::vector<float> dry = chord(false, 64), wet64 = chord(true, 64), wet32 = chord(true, 32);
        double rd = 0, r64 = 0, r32 = 0, p64 = 0, p32 = 0;
        for (size_t i = 0; i < dry.size(); i++) { rd += (double)dry[i] * dry[i]; const double d64 = wet64[i] - dry[i], d32 = wet32[i] - dry[i]; r64 += d64 * d64; r32 += d32 * d32; p64 = std::fmax(p64, std::fabs(d64)); p32 = std::fmax(p32, std::fabs(d32)); }
        std::printf("  plate under the harp chord: dry RMS %.4f; the plate's signal RMS %.4f (64, %.1f dB), %.4f (32, %.1f dB); peaks %.3f / %.3f\n",
                    std::sqrt(rd / dry.size()), std::sqrt(r64 / dry.size()), 10 * std::log10(r64 / rd), std::sqrt(r32 / dry.size()), 10 * std::log10(r32 / rd), p64, p32);
    }
    // 3. the cost: sixteen drums held, a second of blocks; the plate alone
    {
        voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_patch((uint32_t)patch, &sp); sp.decay_s = 0.0f; sp.decay_bright = 0.0f; voxo_set_suzu_params(v, &sp); mcm(v);
        for (int i = 0; i < 16; i++) voxo_push_midi(v, (uint8_t)(0x91 + (i % 15)), (uint8_t)(36 + i * 3), 100);
        std::vector<float> buf(2 * BLOCK); double worst = 0, total = 0; const int blocks = (int)(rate / BLOCK);
        for (int b = 0; b < blocks; b++) { const auto t0 = std::chrono::steady_clock::now(); voxo_render(v, buf.data(), BLOCK); const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); total += ms; if (ms > worst) worst = ms; }
        voxo_stats_t st; voxo_stats(v, &st);
        std::printf("  sixteen membranes (%u active): mean %.3f ms, worst %.3f ms of %.3f (%.1f %%) [%s]\n", st.active_voices, total / blocks, worst, 1000.0 * BLOCK / rate, 100.0 * worst / (1000.0 * BLOCK / rate), suzu::simd_name());
        voxo_destroy(v);
        for (uint32_t size : { 64u, 32u }) {
            voxo_t* w = make(rate); voxo_suzu_params_t sq; voxo_suzu_patch(13, &sq); sq.plate_size = size; voxo_set_suzu_params(w, &sq); mcm(w);
            voxo_push_midi(w, 0x91, 48, 100);
            worst = 0; total = 0;
            for (int b = 0; b < blocks; b++) { const auto t0 = std::chrono::steady_clock::now(); voxo_render(w, buf.data(), BLOCK); const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); total += ms; if (ms > worst) worst = ms; }
            std::printf("  the plate at %u with one harp voice: mean %.3f ms, worst %.3f ms (%.1f %%)\n", size, total / blocks, worst, 100.0 * worst / (1000.0 * BLOCK / rate));
            voxo_destroy(w);
        }
    }
    t60_probe(patch);
    return 0;
}
// the T60 of the (1,1) mode under decay_s (bright 0): the RMS over 100 ms windows, the slope in dB/s
static void t60_probe(int patch) {
    const uint32_t rate = 48000;
    voxo_t* v = make(rate); voxo_suzu_params_t sp; voxo_suzu_patch((uint32_t)patch, &sp); sp.decay_s = 1.2f; sp.decay_bright = 0.0f; sp.release_s = 8.0f; voxo_set_suzu_params(v, &sp); mcm(v);
    voxo_push_midi(v, 0x91, 48, 100);
    std::vector<float> s = render_left(v, rate, 2.0);
    std::printf("  T60 probe (decay_s 1.2, bright 0, C3 held): ");
    double first = 0; int k = 0;
    for (size_t w = 4800; w + 4800 <= s.size(); w += 4800) { double q = 0; for (size_t i = w; i < w + 4800; i++) q += (double)s[i] * s[i]; const double db = 10 * std::log10(q / 4800 + 1e-30); if (k == 0) first = db; if (k % 3 == 0) std::printf("%.1f ", db); k++; }
    double q = 0; for (size_t i = s.size() - 4800; i < s.size(); i++) q += (double)s[i] * s[i];
    const double last = 10 * std::log10(q / 4800 + 1e-30);
    std::printf(" dB -> slope %.1f dB/s, T60 %.2f s\n", (last - first) / ((double)(k - 1) * 0.1), 60.0 / ((first - last) / ((double)(k - 1) * 0.1)));
    voxo_destroy(v);
}
struct T60Run { T60Run() {} } ;
