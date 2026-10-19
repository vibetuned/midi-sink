// holes_probe.cpp — step 81: the toneholes. For the flute (6) and the sax (7) in hole mode, each of the Boehm chart's
// twelve fingerings (D4..C♯5) keyed by its CCs and the note struck: the pitch in cents against the note, the inspection's
// holes open / j / register, the peak. Then the second register (the vent), and a note the keys disagree with (the v1 cut).
// CAL=0 skips the calibration's effect by reading hole_size only (the engine always calibrates); ENV HOLE_SIZE, HOLE_LOSS.
#include "voxo.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <chrono>
#include <initializer_list>
static const uint32_t BLOCK = 128;
static double goertzel(const float* s, size_t n, double hz, double rate) {
    const double w = 2.0 * M_PI * hz / rate, cw = 2.0 * std::cos(w); double s0 = 0, s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; i++) { const double win = 0.5 - 0.5 * std::cos(2.0 * M_PI * (double)i / (double)(n - 1)); s0 = (double)s[i] * win + cw * s1 - s2; s2 = s1; s1 = s0; }
    const double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w); return 4.0 * std::sqrt(re * re + im * im) / (double)n;
}
static double period_hz(const float* s, size_t n, double rate) {   // the autocorrelation's first strong lag
    const size_t lo = (size_t)(rate / 4000.0), hi = (size_t)(rate / 60.0);
    double e0 = 0.0; for (size_t i = 0; i < n - hi; i++) e0 += (double)s[i] * s[i];
    std::vector<double> rr(hi, -2.0); double best = -2.0;
    for (size_t l = lo; l < hi; l++) { double c = 0.0, e1 = 0.0; for (size_t i = 0; i < n - hi; i++) { c += (double)s[i] * s[i + l]; e1 += (double)s[i + l] * s[i + l]; } rr[l] = c / std::sqrt((e0 > 0 ? e0 : 1e-30) * (e1 > 0 ? e1 : 1e-30)); if (rr[l] > best) best = rr[l]; }
    double bl = (double)lo; for (size_t l = lo + 1; l + 1 < hi; l++) if (rr[l] >= best - 0.08 && rr[l] >= rr[l - 1] && rr[l] >= rr[l + 1]) { bl = (double)l; break; }
    return rate / bl;
}
static double peak_near(const float* s, size_t n, double hz, double rate, double span) {
    double best = -1, bf = hz;
    for (double c = -span; c <= span; c += 1.0) { const double f = hz * std::pow(2.0, c / 1200.0); const double m = goertzel(s, n, f, rate); if (m > best) { best = m; bf = f; } }
    return bf;
}
static void logf_(int, const char* m, void*) { std::printf("  [log] %s\n", m); }
static const uint32_t KEYS[12] = { 239u, 495u, 111u, 47u, 143u, 15u, 31u, 11u, 41u, 9u, 1u, 0u };   // D4 D♯4 E4 F4 F♯4 G4 G♯4 A4 B♭4 B4 C5 C♯5
static void send_keys(voxo_t* v, uint32_t bits) {
    for (int b = 0; b < 9; b++) { const uint8_t cc = b < 3 ? (uint8_t)(110 + b) : (uint8_t)(114 + b - 3); voxo_push_midi(v, 0xB0, cc, ((bits >> b) & 1u) ? 127 : 0); }
}
int main(int argc, char** argv) {
    const uint32_t rate = 48000; const int kind = argc > 1 ? std::atoi(argv[1]) : 6; const int breath = argc > 2 ? std::atoi(argv[2]) : 70;
    struct Case { int note; uint32_t keys; const char* what; };
    std::vector<Case> cases;
    for (int j = 0; j < 12; j++) cases.push_back({ 62 + j, KEYS[j], "r1" });
    for (int j = 0; j < 12; j += 2) cases.push_back({ 74 + j, KEYS[j], "r2" });
    for (int j = 0; j < 12; j += 4) cases.push_back({ 86 + j, KEYS[j], "r3" });
    cases.push_back({ 67, KEYS[0], "disagree" }); cases.push_back({ 60, KEYS[0], "below" });
    bool first = true;
    for (const Case& cs : cases) {
        voxo_config_t c{}; c.sample_rate = rate; c.block_frames = BLOCK; c.max_voices = 4; if (first) { c.log_cb = logf_; } voxo_t* v = voxo_create(&c); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = (uint32_t)kind; sp.tonehole_mode = getenv("MODE") ? (uint32_t)atoi(getenv("MODE")) : 1u; if (getenv("LEAK")) sp.bore_leak = (float)atof(getenv("LEAK"));
        if (getenv("HOLE_SIZE")) sp.hole_size = (float)atof(getenv("HOLE_SIZE")); if (getenv("HOLE_LOSS")) sp.hole_loss = (float)atof(getenv("HOLE_LOSS"));
        const auto t0 = std::chrono::steady_clock::now(); voxo_set_suzu_params(v, &sp); if (first) std::printf("  [set_suzu_params took %.0f ms]\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); first = false;
        voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15);
        send_keys(v, cs.keys);
        voxo_push_midi(v, 0xB1, 2, (uint8_t)breath); voxo_push_midi(v, 0x91, (uint8_t)cs.note, 100);
        const uint32_t frames = rate * 3 / 2; std::vector<float> out(2u * frames), s(frames); voxo_inspect_t ins{}; bool got = false;
        for (uint32_t f = 0; f < frames; f += BLOCK) { voxo_render(v, out.data() + 2u * f, BLOCK); if (f == BLOCK * 8) got = voxo_suzu_inspect(v, &ins, 1) > 0; }
        bool fin = true; float pk = 0; for (uint32_t i = 0; i < frames; i++) { s[i] = out[2u * i]; if (!std::isfinite(s[i])) fin = false; if (i > rate / 2) pk = std::fmax(pk, std::fabs(s[i])); }
        const double f_nom = 440.0 * std::pow(2.0, (cs.note - 69) / 12.0);
        const double f = fin && pk > 1e-3 ? (getenv("ACF") ? period_hz(s.data() + rate / 2, rate, rate) : peak_near(s.data() + rate / 2, rate, f_nom, rate, 300.0)) : 0.0;
        // the harmonics' strength: h2/h1 h3/h1 in dB (the cutoff's effect)
        double h1 = f > 0 ? goertzel(s.data() + rate / 2, rate, f, rate) : 1e-12, h3 = f > 0 ? goertzel(s.data() + rate / 2, rate, 3 * f, rate) : 1e-12, h5 = f > 0 ? goertzel(s.data() + rate / 2, rate, 5 * f, rate) : 1e-12;
        std::printf("%-8s note %3d keys %3u: %s%+7.0f c  pk %.3f  lam %.3f cells %3u  holes open %03x j %2.0f reg %.0f  h3 %+5.1f h5 %+5.1f dB\n", cs.what, cs.note, cs.keys, fin ? "" : "NON-FINITE ",
                    f > 0 ? 1200.0 * std::log2(f / f_nom) : -9999.0, pk, got ? ins.k[0] : 0.0f, got ? ins.n - 1 : 0u, got ? (unsigned)ins.k[11] : 0u, got ? ins.k[12] : -2.0f, got ? ins.k[13] : -2.0f,
                    20 * std::log10((h3 + 1e-12) / (h1 + 1e-12)), 20 * std::log10((h5 + 1e-12) / (h1 + 1e-12)));
        voxo_destroy(v);
    }
    return 0;
}
