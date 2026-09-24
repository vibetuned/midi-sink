/* step 45a: the author's Mac session through the Linux build of the one serializer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sumi_preset.h"
int main(int argc, char** argv) {
    FILE* f = fopen(argv[1], "rb"); if (!f) return 2;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* in = calloc(1, n + 1); fread(in, 1, n, f); fclose(f);
    static sumi_preset_t p; sumi_preset_init(&p, NULL, NULL);
    if (!sumi_preset_read(in, 0, &p)) { puts("not a preset"); return 1; }
    uint32_t ver = (1u << 16) | (1u << 8) | 0u;   /* sumi_version() 1.1.0, as the file says */
    size_t need = sumi_preset_write(&p, ver, NULL, 0); char* out = malloc(need + 1); sumi_preset_write(&p, ver, out, need + 1);
    FILE* g = fopen(argv[2], "wb"); fwrite(out, 1, need, g); fclose(g);
    printf("read %ld bytes, wrote %zu bytes, name \"%s\", medium %u, anod_bloom %.9g\n", n, need, p.name, p.params.medium, p.params.anod_bloom);
    return 0;
}
