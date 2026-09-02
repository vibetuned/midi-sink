/* field_dump_compare.c — §4.6 cross-backend field regression comparator.
 *
 * Compares two `midi-sink --field-dump` files (little-endian header w,h as
 * uint32x2, then float32 RGBA rows, row 0 = top) and reports per-channel
 * max|delta| and overall mean|delta|.
 *
 *   field_dump_compare <a.bin> <b.bin> [max_tol] [mean_tol]
 *
 * Default tolerances (step-11 handoff; final values logged in DECISIONS.md Part II):
 * max|delta| <= 1e-2 per channel at deformation boundaries, mean|delta| <=
 * 1e-4 overall — different GPUs rasterize/filter slightly differently at
 * sharp edges. Exit code 0 = PASS, 1 = FAIL, 2 = usage/file error. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static float* load_dump(const char* path, uint32_t* w, uint32_t* h) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return NULL;
    }
    if (fread(w, sizeof(uint32_t), 1, f) != 1 || fread(h, sizeof(uint32_t), 1, f) != 1 ||
        *w == 0 || *h == 0 || *w > 16384 || *h > 16384) {
        fprintf(stderr, "bad header in %s\n", path);
        fclose(f);
        return NULL;
    }
    const size_t count = (size_t)*w * *h * 4;
    float* data = (float*)malloc(count * sizeof(float));
    if (!data || fread(data, sizeof(float), count, f) != count) {
        fprintf(stderr, "truncated data in %s\n", path);
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    return data;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <a.bin> <b.bin> [max_tol] [mean_tol]\n", argv[0]);
        return 2;
    }
    const double max_tol = argc > 3 ? atof(argv[3]) : 1e-2;
    const double mean_tol = argc > 4 ? atof(argv[4]) : 1e-4;

    uint32_t aw, ah, bw, bh;
    float* a = load_dump(argv[1], &aw, &ah);
    float* b = load_dump(argv[2], &bw, &bh);
    if (!a || !b) {
        free(a);
        free(b);
        return 2;
    }
    if (aw != bw || ah != bh) {
        fprintf(stderr, "size mismatch: %ux%u vs %ux%u\n", aw, ah, bw, bh);
        free(a);
        free(b);
        return 2;
    }

    static const char* names[4] = {"u", "v", "ink", "aux"};
    double max_d[4] = {0, 0, 0, 0};
    uint32_t max_x[4] = {0}, max_y[4] = {0};
    double sum = 0.0;
    const size_t texels = (size_t)aw * ah;
    for (size_t i = 0; i < texels; i++) {
        for (int c = 0; c < 4; c++) {
            const double d = fabs((double)a[i * 4 + c] - (double)b[i * 4 + c]);
            sum += d;
            if (d > max_d[c]) {
                max_d[c] = d;
                max_x[c] = (uint32_t)(i % aw);
                max_y[c] = (uint32_t)(i / aw);
            }
        }
    }
    const double mean = sum / (double)(texels * 4);

    double max_all = 0.0;
    for (int c = 0; c < 4; c++) {
        printf("channel %-3s  max|d| %.6e  at (%u, %u)\n", names[c], max_d[c], max_x[c], max_y[c]);
        if (max_d[c] > max_all) max_all = max_d[c];
    }
    printf("overall      mean|d| %.6e  (%ux%u, %zu texels)\n", mean, aw, ah, texels);

    const int pass = max_all <= max_tol && mean <= mean_tol;
    printf("%s  (max %.6e <= %.1e: %s, mean %.6e <= %.1e: %s)\n",
           pass ? "PASS" : "FAIL",
           max_all, max_tol, max_all <= max_tol ? "yes" : "NO",
           mean, mean_tol, mean <= mean_tol ? "yes" : "NO");
    free(a);
    free(b);
    return pass ? 0 : 1;
}
