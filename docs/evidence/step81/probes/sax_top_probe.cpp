// sax_top_probe.cpp — the sax's top notes (96..108) at breath 70 (0.55·127): the peak, finite, the breaker's trips, the bore's cells and λ;
// bore_leak from LEAK (dflt 0.4), brass_shock from SHOCK (dflt 1 — the engine ignores it on the sax unless built otherwise).
#include "voxo.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
int main() {
    const float leak = getenv("LEAK") ? (float)atof(getenv("LEAK")) : 0.4f;
    for (int note = 94; note <= 108; note++) {
        voxo_config_t c{}; c.sample_rate = 48000; c.block_frames = 128; c.max_voices = 4; voxo_t* v = voxo_create(&c); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 7; sp.bore_leak = leak; voxo_set_suzu_params(v, &sp);
        voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15);
        voxo_push_midi(v, 0xB1, 2, 70); voxo_push_midi(v, 0x91, (uint8_t)note, 100);
        const uint32_t frames = 48000; std::vector<float> out(2u * frames); voxo_inspect_t ins{}; bool got = false; float pk = 0; uint32_t first_big = 0;
        for (uint32_t f = 0; f < frames; f += 128) { voxo_render(v, out.data() + 2u * f, 128); if (f == 128 * 4) got = voxo_suzu_inspect(v, &ins, 1) > 0; for (uint32_t i = 0; i < 128; i++) { const float a = std::fabs(out[2u * (f + i)]); if (a > pk) pk = a; if (a > 0.5f && !first_big) first_big = f + i; } }
        voxo_stats_t st; voxo_stats(v, &st);
        std::printf("note %3d: peak %7.4f %s trips %u first>0.5 at %.3f s; cells %3u lam %.3f\n", note, pk, pk > 0.9f ? "FULL SCALE" : "", st.breaker_trips, first_big / 48000.0, got ? ins.n - 1 : 0u, got ? ins.k[0] : 0.0f);
        voxo_destroy(v);
    }
    return 0;
}
