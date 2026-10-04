/* suzu_web_reference.c — the web gate's native reference (Phase 8 step 59b, DECISIONS_7 #29).
 *
 * Renders tests/fixtures/suzu_web_script.txt through web/suzu/suzu_web.c — the lab's own
 * flat surface — linked natively against Voxo: `suzu_web_reference` against voxo_nofma
 * (no fused multiply-add, as wasm computes) for the bit-for-bit comparison,
 * `suzu_web_reference_desktop` against the shipping voxo for the report. Writes the
 * records tools/suzu_web_gate.mjs writes from the wasm, float32 little-endian:
 *   snap [2, kind, block, ntrace, trace…, ninspect, inspect…] as they fall, then the run's audio [1, kind, count, samples…]
 *   usage: suzu_web_reference <script> <out.bin>                                         */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "voxo.h"

/* suzu_web.c's surface (the lab's wasm exports) */
uint32_t sw_param_count(void); const char* sw_param_name(uint32_t i); void sw_param_set(uint32_t i, double value); void sw_param_defaults(void);
voxo_t* sw_create(uint32_t sample_rate, uint32_t max_voices); void sw_destroy(voxo_t* v); uint32_t sw_apply(voxo_t* v);
void sw_midi(voxo_t* v, uint32_t status, uint32_t d1, uint32_t d2); float* sw_out_ptr(void); void sw_render(voxo_t* v, uint32_t frames);
void sw_trace_mask(voxo_t* v, uint32_t mask); uint32_t sw_trace(voxo_t* v, uint32_t max_segments); float* sw_trace_ptr(void); uint32_t sw_trace_stride(void);
uint32_t sw_inspect(voxo_t* v); float* sw_inspect_ptr(void); uint32_t sw_inspect_stride(void);

static FILE* g_out;
static void put(float f) { fwrite(&f, sizeof f, 1, g_out); }
static long num(const char* s) { return strtol(s, NULL, 0); }

typedef struct { long block; uint32_t s, a, b; } ev_t;

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <script> <out.bin>\n", argv[0]); return 2; }
    FILE* in = fopen(argv[1], "r"); if (!in) { perror(argv[1]); return 2; }
    g_out = fopen(argv[2], "wb"); if (!g_out) { perror(argv[2]); return 2; }
    uint32_t rate = 48000, kind = 0; voxo_t* v = NULL;
    static ev_t evs[4096]; int nev = 0; static long snaps[256]; int nsnap = 0;
    char line[512];
    while (fgets(line, sizeof line, in)) {
        char* t[8] = {0}; int n = 0;
        for (char* p = strtok(line, " \t\r\n"); p && n < 8; p = strtok(NULL, " \t\r\n")) t[n++] = p;
        if (n == 0 || t[0][0] == '#') continue;
        if (!strcmp(t[0], "rate")) rate = (uint32_t)num(t[1]);
        else if (!strcmp(t[0], "create")) { if (v) sw_destroy(v); sw_param_defaults(); v = sw_create(rate, 16); }
        else if (!strcmp(t[0], "param")) {
            for (uint32_t i = 0; i < sw_param_count(); i++) if (!strcmp(sw_param_name(i), t[1])) { sw_param_set(i, atof(t[2])); if (!strcmp(t[1], "voice_kind")) kind = (uint32_t)atof(t[2]); }
        }
        else if (!strcmp(t[0], "apply")) { if (!sw_apply(v)) { fprintf(stderr, "the patch was rejected (kind %u)\n", kind); return 1; } }
        else if (!strcmp(t[0], "trace")) sw_trace_mask(v, (uint32_t)num(t[1]));
        else if (!strcmp(t[0], "midi")) { ev_t e = { num(t[1]), (uint32_t)num(t[2]), (uint32_t)num(t[3]), (uint32_t)num(t[4]) }; evs[nev++] = e; }
        else if (!strcmp(t[0], "snap")) snaps[nsnap++] = num(t[1]);
        else if (!strcmp(t[0], "render")) {
            const long blocks = num(t[1]);
            float* audio = (float*)malloc(sizeof(float) * (size_t)blocks * 256);   /* the run's audio follows its snaps (the gate's order) */
            if (!audio) return 1;
            int ei = 0;
            for (long b = 0; b < blocks; b++) {
                for (; ei < nev && evs[ei].block <= b; ei++) sw_midi(v, evs[ei].s, evs[ei].a, evs[ei].b);
                sw_render(v, 128);
                memcpy(audio + b * 256, sw_out_ptr(), sizeof(float) * 256);
                for (int s = 0; s < nsnap; s++) if (snaps[s] == b) {
                    const uint32_t nt = sw_trace(v, 8), ni = sw_inspect(v);
                    const uint32_t ft = nt * sw_trace_stride(), fi = ni * sw_inspect_stride();
                    put(2.0f); put((float)kind); put((float)b);
                    put((float)ft); fwrite(sw_trace_ptr(), sizeof(float), ft, g_out);
                    put((float)fi); fwrite(sw_inspect_ptr(), sizeof(float), fi, g_out);
                }
            }
            put(1.0f); put((float)kind); put((float)(blocks * 256));
            fwrite(audio, sizeof(float), (size_t)blocks * 256, g_out);
            free(audio);
            nev = 0; nsnap = 0;
        }
    }
    if (v) sw_destroy(v);
    fclose(g_out); fclose(in);
    return 0;
}
