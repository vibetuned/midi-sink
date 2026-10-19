// step82_probe.cpp — the unison pair's split at κ (the peaks near f0 and f0·√(1+2κ)), and the Scala tuning: a 19-EDO scale,
// a Pythagorean scale in ratios, a mapping, the refusals; the cell rendered under 19-EDO, its pitch by a fine scan.
#include "voxo.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <initializer_list>
static double goertzel(const float* s, size_t n, double hz, double rate) {
    const double w = 2.0 * M_PI * hz / rate, cw = 2.0 * std::cos(w); double s0 = 0, s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; i++) { const double win = 0.5 - 0.5 * std::cos(2.0 * M_PI * (double)i / (double)(n - 1)); s0 = (double)s[i] * win + cw * s1 - s2; s2 = s1; s1 = s0; }
    const double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w); return 4.0 * std::sqrt(re * re + im * im) / (double)n;
}
static double peak_near(const float* s, size_t n, double hz, double rate, double span_c, double step_c) {
    double best = -1, bf = hz; for (double c = -span_c; c <= span_c; c += step_c) { const double f = hz * std::pow(2.0, c / 1200.0), m = goertzel(s, n, f, rate); if (m > best) { best = m; bf = f; } } return bf;
}
static void logf_(int, const char* m, void*) { std::printf("    [log] %s\n", m); }
int main() {
    const uint32_t rate = 48000;
    // ---- the unison pair
    for (int note : { 57, 69 }) for (float kappa : { 0.006f, 0.01f }) {
        voxo_config_t c{}; c.sample_rate = rate; c.block_frames = 128; c.max_voices = 4; voxo_t* v = voxo_create(&c); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
        voxo_suzu_params_t sp; voxo_suzu_patch(16, &sp); sp.coupling = kappa; voxo_set_suzu_params(v, &sp);
        voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15); voxo_push_midi(v, 0x91, (uint8_t)note, 100);
        const uint32_t frames = rate * 3; std::vector<float> out(2u * frames), s(frames);
        for (uint32_t f = 0; f < frames; f += 128) voxo_render(v, out.data() + 2u * f, 128);
        for (uint32_t i = 0; i < frames; i++) s[i] = out[2u * i];
        const double f0 = 440.0 * std::pow(2.0, (note - 69) / 12.0), fs = f0 * std::sqrt(1.0 + 2.0 * kappa);
        // the two peaks: scan −20..+60 cents in 0.5-cent steps over 2.5 s, local maxima
        std::vector<std::pair<double,double>> m; for (double cc = -20; cc <= 80; cc += 0.5) m.push_back({ goertzel(s.data() + rate / 2, frames - rate / 2, f0 * std::pow(2.0, cc / 1200.0), rate), cc });
        std::printf("unison note %d kappa %.3f: theory +%.1f c;", note, kappa, 1200.0 * std::log2(fs / f0));
        for (size_t i = 1; i + 1 < m.size(); i++) if (m[i].first > m[i-1].first && m[i].first >= m[i+1].first && m[i].first > 1e-4) std::printf("  peak %+.1f c (%.3g)", m[i].second, m[i].first);
        std::printf("\n"); voxo_destroy(v);
    }
    // ---- the Scala tuning
    voxo_config_t c{}; c.sample_rate = rate; c.block_frames = 128; c.max_voices = 4; c.log_cb = logf_; voxo_t* v = voxo_create(&c); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
    std::string edo19 = "! 19edo.scl\n!\n19 tone equal temperament\n 19\n!\n";
    for (int d = 1; d <= 19; d++) { char b[64]; std::snprintf(b, sizeof b, " %.6f\n", 1200.0 * d / 19.0); edo19 += b; }
    bool ok = voxo_set_tuning(v, edo19.c_str(), nullptr); std::printf("19-EDO set: %s, count %u, name '%s'\n", ok ? "ok" : "REFUSED", voxo_tuning_count(v), voxo_tuning_name(v));
    double worst = 0; for (int n = 0; n < 128; n++) { const double want = 440.0 * std::pow(2.0, (n - 69) / 19.0), got = voxo_tuning_hz(v, n); worst = std::fmax(worst, std::fabs(1200.0 * std::log2(got / want))); }
    std::printf("  the table against 440·2^((n−69)/19): worst %.4f cents over 128 notes; MIDI 60 %.4f Hz, 69 %.4f, 79 %.4f\n", worst, voxo_tuning_hz(v, 60), voxo_tuning_hz(v, 69), voxo_tuning_hz(v, 79));
    const char* pyth = "! pyth12.scl\nPythagorean 12\n12\n!\n256/243\n9/8\n32/27\n81/64\n4/3\n729/512\n3/2\n128/81\n27/16\n16/9\n243/128\n2/1\n";
    ok = voxo_set_tuning(v, pyth, nullptr); const double fifth = voxo_tuning_hz(v, 76) / voxo_tuning_hz(v, 69), third = voxo_tuning_hz(v, 73) / voxo_tuning_hz(v, 69);
    std::printf("Pythagorean set: %s; the fifth A4→E5 %.6f (3/2 = 1.5), the third A4→C#5 %.6f (81/64 = %.6f), MIDI 69 %.3f Hz, 57 %.3f\n", ok ? "ok" : "REFUSED", fifth, third, 81.0 / 64.0, voxo_tuning_hz(v, 69), voxo_tuning_hz(v, 57));
    const char* kbm = "! a mapping: 12 keys, middle 60, reference 60 at 256 Hz, the formal octave 12\n12\n0\n127\n60\n60\n256.0\n12\n0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n";
    ok = voxo_set_tuning(v, pyth, kbm); std::printf("with the mapping: %s; MIDI 60 %.3f Hz (256), 72 %.3f (512), 67 %.3f (384 = 256·3/2)\n", ok ? "ok" : "REFUSED", voxo_tuning_hz(v, 60), voxo_tuning_hz(v, 72), voxo_tuning_hz(v, 67));
    ok = voxo_set_tuning(v, "! bad\nno count\nabc\n", nullptr); std::printf("a bad file: %s (expected refused); the tuning still %s, count %u\n", ok ? "ACCEPTED" : "refused", voxo_tuning_name(v), voxo_tuning_count(v));
    ok = voxo_set_tuning(v, "! bad degree\nbad\n2\n100.0\nfoo\n", nullptr); std::printf("a bad degree: %s (expected refused)\n", ok ? "ACCEPTED" : "refused");
    voxo_clear_tuning(v); std::printf("cleared: count %u, MIDI 69 %.3f Hz\n", voxo_tuning_count(v), voxo_tuning_hz(v, 69));
    // the cell under 19-EDO: the rendered pitch within 0.1 cent
    voxo_set_tuning(v, edo19.c_str(), nullptr);
    voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 0; sp.decay_s = 0.0f; voxo_set_suzu_params(v, &sp);
    voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15);
    for (int note : { 60, 64, 69, 81 }) {
        voxo_push_midi(v, 0x91, (uint8_t)note, 100);
        const uint32_t frames = rate * 4; std::vector<float> out(2u * frames), s(frames);
        for (uint32_t f = 0; f < frames; f += 128) voxo_render(v, out.data() + 2u * f, 128);
        for (uint32_t i = 0; i < frames; i++) s[i] = out[2u * i];
        const double want = 440.0 * std::pow(2.0, (note - 69) / 19.0), got = peak_near(s.data() + rate, frames - rate, want, rate, 3.0, 0.02);
        std::printf("the cell at MIDI %d under 19-EDO: %.4f Hz wanted, %.4f read — %+.3f cents\n", note, want, got, 1200.0 * std::log2(got / want));
        voxo_push_midi(v, 0x81, (uint8_t)note, 0); for (int i = 0; i < 400; i++) voxo_render(v, out.data(), 128);
    }
    voxo_destroy(v);
    return 0;
}
