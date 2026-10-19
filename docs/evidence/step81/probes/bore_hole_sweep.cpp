// the admittance sweep: a sine drive p_src at node 0, 150..3000 Hz in 10 Hz steps, |u0| at f over the last 0.1 s of 0.3 s;
// the three largest; configurations to isolate the many-holes collapse.
#include "suzu.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>
static double goertzel(const float* s, size_t n, double hz, double rate) {
    const double w = 2.0 * M_PI * hz / rate, cw = 2.0 * std::cos(w); double s0 = 0, s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; i++) { const double win = 0.5 - 0.5 * std::cos(2.0 * M_PI * (double)i / (double)(n - 1)); s0 = (double)s[i] * win + cw * s1 - s2; s2 = s1; s1 = s0; }
    const double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w); return 4.0 * std::sqrt(re * re + im * im) / (double)n;
}
struct Cfg { const char* name; std::vector<int> at; uint32_t open; float h; };
int main() {
    const int T = 28800; std::vector<float> rec(T);
    std::vector<Cfg> cfgs = {
        { "plain", {}, 0u, 0.125f },
        { "ten set, none open", { 117, 109, 103, 97, 91, 86, 81, 75, 72, 67 }, 0u, 0.125f },
        { "one set at 117, open", { 117 }, 1u, 0.125f },
        { "ten set, only 117 open", { 117, 109, 103, 97, 91, 86, 81, 75, 72, 67 }, 1u, 0.125f },
        { "ten set, only 67 open", { 117, 109, 103, 97, 91, 86, 81, 75, 72, 67 }, 1u << 9, 0.125f },
        { "ten open, h 1e-3", { 117, 109, 103, 97, 91, 86, 81, 75, 72, 67 }, 1023u, 0.001f },
        { "ten open, h 1e-2", { 117, 109, 103, 97, 91, 86, 81, 75, 72, 67 }, 1023u, 0.01f },
        { "ten open, h 1/8", { 117, 109, 103, 97, 91, 86, 81, 75, 72, 67 }, 1023u, 0.125f },
        { "two open 117,109", { 117, 109 }, 3u, 0.125f },
        { "two open 72,67", { 72, 67 }, 3u, 0.125f },
        { "three open 75,72,67", { 75, 72, 67 }, 7u, 0.125f },
    };
    for (const Cfg& c : cfgs) {
        std::vector<std::pair<double, double>> res;
        for (double f = 150; f <= 3000; f += 10) {
            suzu::Bore* b = new suzu::Bore; b->setup(128, suzu::BORE_CYLINDER, 0, 0, suzu::END_OPEN, 0.3f, suzu::END_OPEN, 0.3f); b->retune(128, 0.79f, suzu::BORE_CYLINDER, 0, 0); b->radiation_corner(1500.0f, 96000.0f);
            int a[12]; float L[12], R[12]; int k = 0; for (int x : c.at) { a[k] = x; L[k] = 1.0f / (c.h * b->sp[x]); R[k] = 0.3f; k++; }
            b->holes_set(k, a, L, R, 3000.0f, 96000.0f); b->holes_open(c.open);
            for (int i = 0; i < T; i++) { const float drive = 0.001f * sinf(2.0f * suzu::PI * (float)f * (float)i / 96000.0f); rec[i] = b->step(0.0f, drive, 1.0f); }
            res.push_back({ goertzel(rec.data() + 2 * T / 3, T / 3, f, 96000.0), f }); delete b;
        }
        std::vector<std::pair<double, double>> pk; for (size_t i = 1; i + 1 < res.size(); i++) if (res[i].first > res[i - 1].first && res[i].first >= res[i + 1].first) pk.push_back(res[i]);
        std::sort(pk.begin(), pk.end(), [](auto& x, auto& y) { return x.first > y.first; });
        std::printf("%-26s peaks:", c.name); for (size_t i = 0; i < pk.size() && i < 5; i++) std::printf("  %4.0f Hz %.3g", pk[i].second, pk[i].first); std::printf("\n");
    }
    return 0;
}
