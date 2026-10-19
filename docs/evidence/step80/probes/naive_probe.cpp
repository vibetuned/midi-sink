#include "voxo.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <initializer_list>
int main() {
    const uint32_t rate = 48000;
    for (uint32_t kind : { 3u, 2u }) for (uint32_t naive : { 0u, 1u }) for (float force : { 1.0f, 2.0f, 4.0f }) {
        voxo_config_t c{}; c.sample_rate = rate; c.block_frames = 128; c.max_voices = 4; voxo_t* v = voxo_create(&c); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = kind; sp.bow_mode = 1; sp.bow_position = 0.12f; sp.bow_force = force; sp.bow_naive = naive; sp.cutoff_hz = 20000.0f; sp.string_decay_s = 0.0f; sp.loop_loss = 0.0f; sp.bridge_decay_s = 0.0f; sp.decay_s = 0.0f; sp.level = 0.25f;
        voxo_set_suzu_params(v, &sp); voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15); voxo_push_midi(v, 0xB1, 2, 127); voxo_push_midi(v, 0x91, 57, 100);
        std::vector<float> out(256); voxo_inspect_t ins; double e0 = -1, w0 = 0; float pk = 0; bool fin = true;
        std::printf("kind %u naive %u force %.1f:", kind, naive, force);
        for (int b = 0; b < 1125; b++) {
            voxo_render(v, out.data(), 128); for (float x : out) { if (!std::isfinite(x)) fin = false; pk = std::fmax(pk, std::fabs(x)); }
            if (voxo_suzu_inspect(v, &ins, 1)) {
                const double e = kind == 2 ? 0.5 * ins.k[6] : ins.k[12], w = kind == 2 ? ins.k[3] : ins.k[10], st = kind == 2 ? ins.k[5] : ins.k[11], gn = kind == 2 ? ins.k[7] : ins.k[13];
                if (e0 < 0) { e0 = e; w0 = w; }
                if (b % 375 == 374) std::printf("  t %.1f: E %.3g (over %.3g) work %.3g gain %.3g (gain-work %.3g) |", (b + 1) / 375.0, e, e - e0, w - w0, gn, gn - w);
            }
        }
        std::printf(" peak %.3f %s\n", pk, fin ? "" : "NON-FINITE"); voxo_destroy(v);
    }
}
