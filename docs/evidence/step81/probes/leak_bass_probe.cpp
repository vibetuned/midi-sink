// leak_bass_probe.cpp — step 81: the leak against the winds' bass. The flute at breath 56 (0.44) and the trumpet at 70 (0.55),
// bore_leak 0.3 / 0, notes C2..C6: the peak of the last 0.3 s of 0.6 s in dBFS.
#include "voxo.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <initializer_list>
int main() {
    const uint32_t rate = 48000;
    for (int kind : { 6, 8 }) for (float leak : { 0.3f, 0.0f }) {
        std::printf("kind %d leak %.1f:", kind, leak);
        for (int note : { 36, 43, 48, 55, 60, 67, 72, 84 }) {
            voxo_config_t c{}; c.sample_rate = rate; c.block_frames = 128; c.max_voices = 4; voxo_t* v = voxo_create(&c); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
            voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = (uint32_t)kind; sp.bore_leak = leak; voxo_set_suzu_params(v, &sp);
            voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15);
            voxo_push_midi(v, 0xB1, 2, kind == 6 ? 56 : 70); voxo_push_midi(v, 0x91, (uint8_t)note, 100);
            const uint32_t frames = rate * 6 / 10; std::vector<float> out(2u * frames);
            for (uint32_t f = 0; f < frames; f += 128) voxo_render(v, out.data() + 2u * f, 128);
            float pk = 0; for (uint32_t i = frames / 2; i < frames; i++) pk = std::fmax(pk, std::fabs(out[2u * i]));
            std::printf("  %d:%6.1f", note, 20.0 * std::log10(pk + 1e-9)); voxo_destroy(v);
        }
        std::printf("\n");
    }
    return 0;
}
