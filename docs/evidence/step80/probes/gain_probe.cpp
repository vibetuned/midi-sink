#include "voxo.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <initializer_list>
int main() {
    const uint32_t rate = 48000;
    for (uint32_t kind : { 2u, 3u }) {
        voxo_config_t c{}; c.sample_rate = rate; c.block_frames = 128; c.max_voices = 4; voxo_t* v = voxo_create(&c); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = kind; sp.bow_mode = 1; sp.bow_position = 0.12f; sp.bow_force = 1.0f; sp.cutoff_hz = 20000.0f;
        sp.string_decay_s = 0.0f; sp.loop_loss = 0.0f; sp.bridge_decay_s = 0.0f; sp.bridge_cells = 0; sp.level = 0.25f; sp.release_s = 30.0f;
        voxo_set_suzu_params(v, &sp); voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15); voxo_push_midi(v, 0xB1, 2, 127); voxo_push_midi(v, 0x91, 57, 100);
        std::vector<float> out(256); voxo_inspect_t ins; double e_prev = 0, g_prev = 0, w_prev = 0; bool first = true;
        std::printf("kind %u (undamped): per 0.25 s — dE, dGain, dWork, dHeat = dWork − dGain\n", kind);
        for (int b = 0; b < 750; b++) {
            voxo_render(v, out.data(), 128);
            if (voxo_suzu_inspect(v, &ins, 1)) {
                const double e = kind == 2 ? 0.5 * ins.k[6] : ins.k[12], w = kind == 2 ? ins.k[3] : ins.k[10], g = kind == 2 ? ins.k[7] : ins.k[13];
                if (first) { e_prev = e; g_prev = g; w_prev = w; first = false; }
                if (b % 94 == 93) { std::printf("  t %.2f: E %.4g  dE %+.4g  dGain %+.4g  dWork %+.4g  dHeat %+.4g  f %.4g vb %.4g stuck %.0f\n", (b + 1) * 128.0 / rate, e, e - e_prev, g - g_prev, w - w_prev, (w - w_prev) - (g - g_prev), kind == 2 ? ins.k[1] : ins.k[8], kind == 2 ? ins.k[2] : ins.k[9], kind == 2 ? ins.k[5] : ins.k[11]); e_prev = e; g_prev = g; w_prev = w; }
            }
        }
        voxo_destroy(v);
    }
}
