#include "voxo.h"
#include <cstdio>
#include <vector>
static void logf_(int, const char* m, void*) { std::printf("    [log] %s\n", m); }
int main() {
    voxo_config_t c{}; c.sample_rate = 48000; c.block_frames = 128; c.max_voices = 4; c.log_cb = logf_; voxo_t* v = voxo_create(&c); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
    voxo_suzu_params_t sp; const bool got = voxo_suzu_patch(16, &sp);
    std::printf("patch 16 '%s': got %d, kind %u preset %u modes %u comp %u coupling %.4f; ratios:", voxo_suzu_patch_name(16), got, sp.voice_kind, sp.modal_preset, sp.modes, sp.modal_comp, sp.coupling);
    for (int k = 0; k < 8; k++) std::printf(" %.2f", sp.modal_ratio[k]); std::printf("; weights:"); for (int k = 0; k < 8; k++) std::printf(" %.2f", sp.modal_weight[k]); std::printf("\n");
    uint32_t n; float r[16], t[16], w[16]; voxo_suzu_table(&sp, &n, r, t, w); std::printf("voxo_suzu_table: n %u; ratio:", n); for (uint32_t k = 0; k < n; k++) std::printf(" %.2f", r[k]); std::printf("; t60:"); for (uint32_t k = 0; k < n; k++) std::printf(" %.2f", t[k]); std::printf("; kick:"); for (uint32_t k = 0; k < n; k++) std::printf(" %.2f", w[k]); std::printf("\n");
    const bool adm = voxo_set_suzu_params(v, &sp); std::printf("set_suzu_params: %s\n", adm ? "admitted" : "REFUSED");
    voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15); voxo_push_midi(v, 0x91, 57, 100);
    std::vector<float> out(256); float pk = 0;
    for (int b = 0; b < 400; b++) { voxo_render(v, out.data(), 128); for (int i = 0; i < 256; i += 2) if (out[i] > pk) pk = out[i]; if (b == 8) { voxo_inspect_t ins; if (voxo_suzu_inspect(v, &ins, 1)) { std::printf("inspect: kind %u n %u freq %.1f; s:", ins.voice_kind, ins.n, ins.freq); for (uint32_t k = 0; k < ins.n; k++) std::printf(" %.2f", ins.s[k]); std::printf("; |a|:"); for (uint32_t k = 0; k < ins.n; k++) std::printf(" %.3g", ins.a[k]); std::printf("\n"); } else std::printf("inspect: no voice\n"); } }
    std::printf("peak over 1 s: %.4f\n", pk);
    voxo_destroy(v); return 0;
}
