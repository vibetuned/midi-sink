// the spatial profile of the trumpet's bore at fff: the front and what trails it; the roughness Σ|Δ²p| / Σ|Δp| over the bore
#include "voxo.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
int main(int argc, char** argv) {
    const uint32_t rate = 48000; const float cq = argc > 1 ? (float)std::atof(argv[1]) : -1.0f; const int limiter = argc > 2 ? std::atoi(argv[2]) : 1;
    voxo_config_t c{}; c.sample_rate = rate; c.block_frames = 128; c.max_voices = 4; voxo_t* v = voxo_create(&c); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
    voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 8; sp.shock_limiter = (uint32_t)limiter; if (getenv("BRASS")) sp.brass = (float)atof(getenv("BRASS")); if (getenv("NOSHOCK")) sp.brass_shock = 0.0f; voxo_set_suzu_params(v, &sp);
    voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15); voxo_push_midi(v, 0xB1, 2, 127); voxo_push_midi(v, 0x91, 60, 100);
    std::vector<float> out(256); voxo_inspect_t ins; double rough = 0, worst = 0; int cnt = 0; std::string prof;
    for (int b = 0; b < 750; b++) {
        voxo_render(v, out.data(), 128);
        if (b > 375 && voxo_suzu_inspect(v, &ins, 1)) {
            double d1 = 0, d2 = 0; for (uint32_t i = 1; i + 1 < ins.n; i++) { d1 += std::fabs(ins.a[i + 1] - ins.a[i]); d2 += std::fabs(ins.a[i + 1] - 2 * ins.a[i] + ins.a[i - 1]); }
            const double r = d1 > 0 ? d2 / d1 : 0; rough += r; cnt++; if (r > worst) { worst = r; prof.clear(); for (uint32_t i = 0; i < ins.n; i++) { char t[16]; std::snprintf(t, sizeof t, "%+.3f ", ins.a[i] / 0.005f); prof += t; } }
        }
    }
    std::printf("limiter %d: roughness Σ|Δ²p|/Σ|Δp| mean %.3f worst %.3f; the roughest profile (P_REF units, node 0 the lips):\n  %s\n", limiter, rough / cnt, worst, prof.c_str());
    voxo_destroy(v); return 0;
}
