/* Strict C11 consumer of voxo.h (Phase 7 step 47, SOUND §1: the sibling core's
   header is pure C exactly like sumi_core.h and hostmpe.h — the Swift
   module-map pattern can only import C headers). Creates an instance, renders
   one block with no device, and checks the version. */
#include "voxo.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    const uint32_t ver = voxo_version();
    if (ver != ((0u << 16) | (4u << 8) | 0u)) {
        fprintf(stderr, "FAIL: voxo_version %u.%u.%u\n", ver >> 16, (ver >> 8) & 0xFF, ver & 0xFF);
        return 1;
    }
    if (voxo_default_block_frames() < 64u || voxo_default_block_frames() > 512u) {
        fprintf(stderr, "FAIL: default block %u\n", voxo_default_block_frames());
        return 1;
    }
    voxo_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.sample_rate = 48000;
    cfg.block_frames = 128;
    voxo_t* v = voxo_create(&cfg);
    if (!v) { fprintf(stderr, "FAIL: voxo_create\n"); return 1; }
    voxo_set_input_mode(v, 1);
    voxo_push_midi(v, 0x91, 69, 100);
    float block[2 * 128];
    voxo_render(v, block, 128);
    voxo_stats_t st;
    voxo_stats(v, &st);
    if (st.active_voices != 1 || st.sample_rate != 48000 || voxo_running(v)) {
        fprintf(stderr, "FAIL: stats voices %u rate %u running %d\n", st.active_voices, st.sample_rate, (int)voxo_running(v));
        voxo_destroy(v);
        return 1;
    }
    voxo_destroy(v);
    printf("voxo_c_compile: OK (voxo %u.%u.%u, default block %u)\n",
           ver >> 16, (ver >> 8) & 0xFF, ver & 0xFF, voxo_default_block_frames());
    return 0;
}
