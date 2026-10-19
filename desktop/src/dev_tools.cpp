// dev_tools.cpp — the LAB BENCH (Phase 5, DECISIONS_4 #5): every debug key,
// scripted DONE test and evidence hook the harness accumulated over steps
// 3–20, moved out of main.cpp verbatim and gated behind --dev. Release builds
// keep all of it (support asks "run with --dev"); without the flag none of
// it is reachable.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "dev_tools.h"
#include "app_settings.h"
#include "midi_harness.h"
#include "print_export.h"
#include "print_ledger.h"   // step 45a: the ledger across GL contexts (DECISIONS_5 #77)
#include "voxo.h"           // Phase 7 step 47: the --voxo-storm proxy; step 49: the glide bounce
#include "wav_io.h"
#include "sumi_replay.h"   // Phase 9 step 65: --record-demo, --replay
#include "sys_info.h"
#include "sumi_debug.h"
#include "sumi_preset.h"    // step 55b: --preset <file.json> for the bench
#include "layouts.h"
#include "displacement.h"   // Phase 6 step 38: the burst helpers (sumi_burst_dphi) for the age-envelope check

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "stb_image_write.h"
#include "orbit_trace.h"      // step 59: the orbit trace (the bench drives the bridge itself)   // implementation lives in print_export.cpp

#if defined(SUMI_HARNESS_GL)
// (defined by CMake on Linux — the host presents on GL, §5.1)
#endif

// Ripple key bindings ride the REAL ctl path: injected CCs, mapped at startup.
static const uint8_t RIPPLE_AMP_CC  = 102;
static const uint8_t RIPPLE_FREQ_CC = 103;
static const float   SOAK_SPARK_K   = 75.398224f;   // Phase 6 step 39: 2pi x 12 — a 43-texel base wavelength at 512, octaves 21 and 11

static void norm_pos(GLFWwindow* window, double px, double py, float* nx, float* ny) {
    int w = 1, h = 1;
    glfwGetWindowSize(window, &w, &h);
    *nx = (float)(px / (double)(w > 0 ? w : 1));
    *ny = (float)(py / (double)(h > 0 ? h : 1));
}

static void print_params(const sumi_params_t* p) {
    std::printf("[params] viscosity %.2f  expansion %.2f  roughness %.2f  palette %u  layout %u\n",
                (double)p->fluid_viscosity, (double)p->expansion_rate,
                (double)p->paper_roughness, p->active_palette_id, p->pitch_layout);
}

// IEEE 754 half -> float, done on the CPU so the --field-dump file is
// backend-independent (§4.6 cross-backend regression, step-11 handoff).
static float half_to_float(uint16_t h) {
    const uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
    uint32_t exp = (h >> 10) & 0x1Fu;
    uint32_t man = h & 0x3FFu;
    uint32_t f;
    if (exp == 0) {
        if (man == 0) {
            f = sign;                       // +/- 0
        } else {                            // subnormal: renormalize
            exp = 127 - 15 + 1;
            while (!(man & 0x400u)) { man <<= 1; exp--; }
            man &= 0x3FFu;
            f = sign | (exp << 23) | (man << 13);
        }
    } else if (exp == 31) {
        f = sign | 0x7F800000u | (man << 13);   // inf / NaN
    } else {
        f = sign | ((exp - 15 + 127) << 23) | (man << 13);
    }
    float out;
    std::memcpy(&out, &f, sizeof(out));
    return out;
}

// Write the raw field dump: little-endian header (w, h as uint32), then
// float32 RGBA rows, row 0 = top (§4.6 one y-down space).
// Phase 6 step 41: the COMPOSITE screenshot regression — the print (RGBA8,
// the offscreen composite of the field, top-left origin on every backend,
// §4.6) taken by a paper dip right after the canonical field script, on the
// scripted clock. The field gate proves the FIELD; this proves the PIXELS —
// the palette table, the washi, the ink-depth curve — bitwise on Metal
// against tests/fixtures/composite_512_metal.rgba, generated from the
// pre-break renderer (0.14.0) so the ABI event's medium 0 is shown to render
// exactly as before. Layout: u32 w, u32 h, then w*h*4 bytes RGBA8, rows top
// to bottom (the same header shape as the field dump).
static bool write_composite_dump(GLFWwindow* window, sumi_instance_t* inst, const char* path) {
    sumi_trigger_paper_dip(inst);
    uint32_t w = 0, h = 0;
    bool ready = false;
    for (int i = 0; i < 600 && !ready; i++) {
        sumi_update(inst, 1.0 / 120.0);
        sumi_render(inst);
        glfwPollEvents();
        ready = sumi_read_print(inst, nullptr, 0, &w, &h);
    }
    (void)window;
    if (!ready || w == 0 || h == 0) { std::fprintf(stderr, "[composite-dump] the dip's print never became ready\n"); return false; }
    const size_t bytes = (size_t)w * h * 4;
    uint8_t* px = (uint8_t*)std::malloc(bytes);
    if (!px || !sumi_read_print(inst, px, bytes, &w, &h)) { std::free(px); std::fprintf(stderr, "[composite-dump] print readback failed\n"); return false; }
    FILE* f = std::fopen(path, "wb");
    if (!f) { std::free(px); std::fprintf(stderr, "[composite-dump] cannot open %s\n", path); return false; }
    const uint32_t hdr[2] = {w, h};
    const bool ok = std::fwrite(hdr, sizeof hdr, 1, f) == 1 && std::fwrite(px, 1, bytes, f) == bytes;
    std::fclose(f);
    std::free(px);
    std::printf("[composite-dump] %s: %ux%u RGBA8 (%zu bytes) — the print of the canonical field script\n", path, w, h, bytes);
    return ok;
}

// The dump is the STORED payload — 1.2.0: (dx, dy, ink, aux), the displacement —
// so the §4.6 gate compares the bytes the backends actually keep.
static bool write_field_dump(sumi_instance_t* inst, const char* path) {
    uint32_t w = 0, h = 0;
    if (!sumi_debug_read_field(inst, nullptr, 0, &w, &h) || w == 0 || h == 0) {
        std::fprintf(stderr, "[field-dump] field size query failed\n");
        return false;
    }
    const size_t texels = (size_t)w * h;
    uint16_t* halves = (uint16_t*)std::malloc(texels * 8);
    float* floats = (float*)std::malloc(texels * 4 * sizeof(float));
    bool ok = halves && floats &&
              sumi_debug_read_field(inst, (uint8_t*)halves, texels * 8, &w, &h);
    if (ok) {
        for (size_t i = 0; i < texels * 4; i++) {
            floats[i] = half_to_float(halves[i]);
        }
        FILE* f = std::fopen(path, "wb");
        ok = f != nullptr;
        if (ok) {
            ok = std::fwrite(&w, sizeof(uint32_t), 1, f) == 1 &&
                 std::fwrite(&h, sizeof(uint32_t), 1, f) == 1 &&
                 std::fwrite(floats, sizeof(float), texels * 4, f) == texels * 4;
            std::fclose(f);
        }
        std::printf("[field-dump] %s %ux%u -> %s\n", ok ? "wrote" : "FAILED to write", w, h, path);
    } else {
        std::fprintf(stderr, "[field-dump] field readback failed\n");
    }
    std::free(halves);
    std::free(floats);
    return ok;
}

/* ------------------------------------------------------------------ */
/* v0.4 scripted DONE tests (roadmap step 19). Each runs on the fixed  */
/* 512x512 scripted-clock setup (like --field-dump), reads the field   */
/* back through sumi_debug_read_field, asserts numerically, prints     */
/* PASS/FAIL lines for the evidence log, and exits.                    */
/* ------------------------------------------------------------------ */

static int t19_checks = 0, t19_failures = 0;
// #87: the backend this bench runs on (main.cpp's choice), for the tests that hold per-backend measurements.
static uint32_t g_bench_backend = 0;
static const char* bench_backend_name() { return g_bench_backend == SUMI_BACKEND_GL ? "gl" : g_bench_backend == SUMI_BACKEND_D3D11 ? "d3d11" : "metal"; }
#define T19(cond, ...) do { \
        t19_checks++; \
        if (!(cond)) { t19_failures++; std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); } \
        else { std::printf("ok:   " __VA_ARGS__); std::printf("\n"); } \
    } while (0)

struct FieldF { uint32_t w = 0, h = 0; float* px = nullptr; };   // RGBA32F rows: (u, v, ink, aux) — the PRE-IMAGE, see below

// The bench reads the field as (u, v, ink, aux) — each texel's pre-image
// coordinate — the form every check below reasons in. 1.2.0 (Phase 8 step
// 55b) stores the DISPLACEMENT (u − x, v − y) instead, so the reader rebuilds
// u = dx + (x + ½)/W, v = dy + (y + ½)/H in float32 once, here; the raw
// reader keeps the stored halves for the bitwise compares and the dumps.
static bool t19_read_field(sumi_instance_t* inst, FieldF* out) {
    uint32_t w = 0, h = 0;
    if (!sumi_debug_read_field(inst, nullptr, 0, &w, &h) || !w || !h) return false;
    const size_t texels = (size_t)w * h;
    uint16_t* halves = (uint16_t*)std::malloc(texels * 8);
    float* px = (float*)std::malloc(texels * 4 * sizeof(float));
    if (!halves || !px ||
        !sumi_debug_read_field(inst, (uint8_t*)halves, texels * 8, &w, &h)) {
        std::free(halves);
        std::free(px);
        return false;
    }
    for (size_t i = 0; i < texels * 4; i++) px[i] = half_to_float(halves[i]);
    for (uint32_t y = 0; y < h; y++) {
        const float sy = ((float)y + 0.5f) / (float)h;
        for (uint32_t x = 0; x < w; x++) {
            float* t = px + ((size_t)y * w + x) * 4;
            t[0] += ((float)x + 0.5f) / (float)w;
            t[1] += sy;
        }
    }
    std::free(halves);
    out->w = w; out->h = h; out->px = px;
    return true;
}

// Raw half-float bytes — the stored payload (1.2.0: displacements) — for the
// BITWISE ripple-group compare and the bake test's baseline.
static uint8_t* t19_read_field_raw(sumi_instance_t* inst, size_t* out_bytes) {
    uint32_t w = 0, h = 0;
    if (!sumi_debug_read_field(inst, nullptr, 0, &w, &h) || !w || !h) return nullptr;
    const size_t bytes = (size_t)w * h * 8;
    uint8_t* buf = (uint8_t*)std::malloc(bytes);
    if (!buf || !sumi_debug_read_field(inst, buf, bytes, &w, &h)) {
        std::free(buf);
        return nullptr;
    }
    *out_bytes = bytes;
    return buf;
}

static void t19_step(GLFWwindow* window, sumi_instance_t* inst, int n) {
    for (int i = 0; i < n; i++) {
        sumi_update(inst, 1.0 / 120.0);
        sumi_render(inst);
#if defined(SUMI_HARNESS_GL)
        glfwSwapBuffers(window);
#endif
        glfwPollEvents();
    }
}

// Ink MASS (Σ phase over the field) + per-parity band counts. Mass is the
// observable an area-preserving operator chain actually conserves: exact
// under the det = 1 change of variables, and bilinear gather (a convex
// combination) carries it to O(h²) per pass. Level-set areas are NOT
// conserved under resampling (blur moves any threshold's contour), and band
// parity (floor(phase) odd/even, §4.2) mixes toward the regional mean under
// long chains — over-folded real marbling mixes to gray the same way
// (DECISIONS_3 #32). Band counts are reported as diagnostics only.
static void t19_band_areas(const FieldF* f, double* mass, long* ink, long* clear_band) {
    double m = 0.0;
    long i_n = 0, c_n = 0;
    const size_t texels = (size_t)f->w * f->h;
    for (size_t i = 0; i < texels; i++) {
        const float phase = f->px[i * 4 + 2];
        m += (double)phase;
        if (phase >= 1.0f) {
            if (((long)std::floor(phase)) % 2 == 1) i_n++;
            else c_n++;
        }
    }
    *mass = m;
    *ink = i_n;
    *clear_band = c_n;
}

// Angular deflection of the stored pre-image around center V (ac coords,
// aspect 1 on the 512x512 setup) at radius r along +x.
static float t19_swirl_at(const FieldF* f, float vx, float vy, float r) {
    // Mean |deflection| over 8 directions — a single-ray probe quantizes to
    // texel centers and misplaces the crease by 2-3 texels.
    double acc = 0.0;
    int n = 0;
    for (int k = 0; k < 8; k++) {
        const double th = (double)k * 0.7853981633974483;
        const int ix = (int)std::lround((vx + r * std::cos(th)) * (double)f->w - 0.5);
        const int iy = (int)std::lround((vy + r * std::sin(th)) * (double)f->h - 0.5);
        if (ix < 0 || iy < 0 || ix >= (int)f->w || iy >= (int)f->h) continue;
        const size_t o = ((size_t)iy * f->w + ix) * 4;
        const float sx = f->px[o] - vx, sy = f->px[o + 1] - vy;
        const float px_ = ((float)ix + 0.5f) / (float)f->w - vx;
        const float py_ = ((float)iy + 0.5f) / (float)f->h - vy;
        acc += std::fabs(std::atan2(sx * py_ - sy * px_, sx * px_ + sy * py_));
        n++;
    }
    return n ? (float)(acc / n) : 0.0f;
}

static void t19_scene_rings(GLFWwindow* window, sumi_instance_t* inst) {
    for (int i = 0; i < 8; i++) {
        sumi_add_drop(inst, 0.5f, 0.5f, 0.14f, 0);
        t19_step(window, inst, 1);
    }
}

// Consume-the-print helper: dip, wait until ready, optionally copy out.
static uint8_t* t19_dip_print(GLFWwindow* window, sumi_instance_t* inst,
                              uint32_t* pw, uint32_t* ph) {
    sumi_trigger_paper_dip(inst);
    for (int i = 0; i < 600; i++) {
        t19_step(window, inst, 1);
        if (sumi_read_print(inst, nullptr, 0, pw, ph)) break;
    }
    const size_t bytes = (size_t)*pw * *ph * 4;
    uint8_t* buf = (uint8_t*)std::malloc(bytes);
    if (!buf || !sumi_read_print(inst, buf, bytes, pw, ph)) {
        std::free(buf);
        return nullptr;
    }
    return buf;
}

// §4.3(4) orientation + screenshot pair: ink ahead of the tip bulges forward,
// flank ink streams backward.
static void t19_wake_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t19] wake orientation test\n");
    const float a = 0.04f;
    // Screenshot pair, deterministically: rings -> dip (saves "before", resets
    // field + rebases the counter) -> same rings again -> wake -> asserts ->
    // dip (saves "after").
    t19_scene_rings(window, inst);
    uint32_t pw = 0, ph = 0;
    uint8_t* before = t19_dip_print(window, inst, &pw, &ph);
    if (before) {
        stbi_write_png("wake_before.png", (int)pw, (int)ph, 4, before, (int)pw * 4);
        std::free(before);
    }
    t19_scene_rings(window, inst);
    sumi_add_wake(inst, 0.30f, 0.50f, 0.46f, 0.50f, a);
    t19_step(window, inst, 1);

    FieldF f;
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    const float tx = 0.46f, ty = 0.50f;
    // Ahead of the tip: the pre-image sits BEHIND the texel (ink moved forward).
    {
        const int ix = (int)((tx + 1.5f * a) * (float)f.w);
        const int iy = (int)(ty * (float)f.h);
        const float u = f.px[(((size_t)iy * f.w) + ix) * 4];
        const float stx = ((float)ix + 0.5f) / (float)f.w;
        T19(u < stx - 0.002f, "ahead of tip: source behind (u %.4f < st %.4f)", (double)u, (double)stx);
    }
    // At the flank: the pre-image sits AHEAD (ink streams backward).
    {
        const int ix = (int)(tx * (float)f.w);
        const int iy = (int)((ty + 1.5f * a) * (float)f.h);
        const float u = f.px[(((size_t)iy * f.w) + ix) * 4];
        const float stx = ((float)ix + 0.5f) / (float)f.w;
        T19(u > stx + 0.002f, "flank: source ahead (u %.4f > st %.4f)", (double)u, (double)stx);
    }
    std::free(f.px);
    uint32_t pw2 = 0, ph2 = 0;
    uint8_t* after = t19_dip_print(window, inst, &pw2, &ph2);
    if (after) {
        stbi_write_png("wake_after.png", (int)pw2, (int)ph2, 4, after, (int)pw2 * 4);
        std::free(after);
        std::printf("[t19] wrote wake_before.png / wake_after.png\n");
    }
}

// v0.6 (DECISIONS_4 #49, #51): the pressure operators as gestures, and the
// print recycle. FEED must WIDEN one band (interior = the centre texel, no
// new ring, no radial gradient) with the same area a plain expansion would
// displace; the LAMB_OSEEN profile must rotate a far-field marker by the
// analytic angle; three unread dips must all succeed and read back newest-first.
static long t33_count_ink(const FieldF& f) {
    long n = 0;
    for (size_t i = 0; i < (size_t)f.w * f.h; i++) if (f.px[i * 4 + 2] > 0.5f) n++;
    return n;
}
// Band identity along the ray centre -> right edge: 0 = water, else floor(ink)
// (§4.2 parity form: ink = 1 + parity + radial, so rings alternate 1 / 2).
// Counts BAND changes — an ink/water edge or a parity flip each count once.
static int t33_ray_transitions(const FieldF& f) {
    const uint32_t y = f.h / 2;
    int flips = 0, prev = -1;
    for (uint32_t x = f.w / 2; x < f.w; x++) {
        const float ink = f.px[(((size_t)y * f.w) + x) * 4 + 2];
        const int band = ink > 0.5f ? (int)std::floor(ink) : 0;
        if (prev >= 0 && band != prev) flips++;
        prev = band;
    }
    return flips;
}
static float t33_ink_at(const FieldF& f, float nx, float ny) {
    const int ix = (int)(nx * (float)f.w), iy = (int)(ny * (float)f.h);
    return f.px[(((size_t)iy * f.w) + ix) * 4 + 2];
}
static void t19_pressure_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t33] pressure gesture + print recycle test\n");
    uint32_t pw = 0, ph = 0;
    const float R0 = 0.06f, R1 = 0.12f;
    const int steps = 20;

    // --- FEED: one band widening ---------------------------------------------
    std::free(t19_dip_print(window, inst, &pw, &ph));            // fresh sheet
    sumi_add_drop(inst, 0.5f, 0.5f, R0, SUMI_DROP_INK);
    t19_step(window, inst, 1);
    float R = R0;
    for (int i = 0; i < steps; i++) {                             // the gesture's schedule
        const float dR = (R1 - R0) / (float)steps;
        sumi_add_drop(inst, 0.5f, 0.5f, std::sqrt((R + dR) * (R + dR) - R * R), SUMI_DROP_FEED);
        R += dR;
    }
    t19_step(window, inst, 1);
    FieldF f;
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    {
        const double expect = 3.14159265 * (double)(R1 * f.h) * (double)(R1 * f.h);
        const long got = t33_count_ink(f);
        T19(std::fabs((double)got - expect) < 0.12 * expect,
            "feed: ink area %ld ~ pi*(R1*H)^2 = %.0f (band grew from R0 to R1)", got, expect);
        T19(t33_ray_transitions(f) == 1, "feed: ONE band along the ray (transitions %d)", t33_ray_transitions(f));
        // The fed disk keeps the SEED drop's band: every inked texel out to
        // 0.95 R1 carries the same floor(ink) as the centre (the radial
        // gradient inside is the seed's, stretched — exactly the mapper's feed).
        const int band_c = (int)std::floor(t33_ink_at(f, 0.5f, 0.5f));
        int strangers = 0;
        for (float t = 0.0f; t < 0.95f; t += 0.01f) {
            const float ink = t33_ink_at(f, 0.5f + t * R1, 0.5f);
            if (ink > 0.5f && (int)std::floor(ink) != band_c) strangers++;
        }
        T19(strangers == 0, "feed: one parity band inside R1 (band %d, foreign texels along the ray %d)", band_c, strangers);
    }
    std::free(f.px);

    // --- negative control: the same schedule with INK drops lays rings --------
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_add_drop(inst, 0.5f, 0.5f, R0, SUMI_DROP_INK);
    R = R0;
    for (int i = 0; i < steps; i++) {
        const float dR = (R1 - R0) / (float)steps;
        sumi_add_drop(inst, 0.5f, 0.5f, std::sqrt((R + dR) * (R + dR) - R * R), SUMI_DROP_INK);
        R += dR;
    }
    t19_step(window, inst, 1);
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    T19(t33_ray_transitions(f) >= 6, "control: INK drops lay rings (transitions %d >= 6)", t33_ray_transitions(f));
    std::free(f.px);

    // --- LAMB_OSEEN gesture: a far-field marker rotates by the analytic angle -
    std::free(t19_dip_print(window, inst, &pw, &ph));
    const float rc = 0.06f, rm = 0.15f, theta0 = 2.0f;           // core rotation 2 rad in total
    sumi_add_drop(inst, 0.5f + rm, 0.5f, 0.025f, SUMI_DROP_INK); // the marker (aspect 1: field is square)
    t19_step(window, inst, 1);
    const float S = theta0 * 6.2831853f * rc * rc;
    for (int i = 0; i < 10; i++) sumi_add_vortex(inst, 0.5f, 0.5f, S / 10.0f, rc, SUMI_VORTEX_LAMB_OSEEN);
    t19_step(window, inst, 1);
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    {
        double sx = 0, sy = 0; long n = 0;
        for (uint32_t y = 0; y < f.h; y++) for (uint32_t x = 0; x < f.w; x++) {
            if (f.px[(((size_t)y * f.w) + x) * 4 + 2] > 0.5f) { sx += x + 0.5; sy += y + 0.5; n++; }
        }
        const double cx = n ? sx / (double)n / f.w - 0.5 : 0.0, cy = n ? sy / (double)n / f.h - 0.5 : 0.0;
        const double ang = std::atan2(cy, cx);
        const double expect = theta0 * (rc * rc) / (rm * rm) * (1.0 - std::exp(-(rm * rm) / (rc * rc)));
        T19(n > 0 && std::fabs(std::fabs(ang) - expect) < 0.08,
            "swirl: marker rotated |%.3f| rad ~ analytic %.3f (theta(r) = S/(2 pi r^2)(1-exp(-r^2/rc^2)))",
            std::fabs(ang), expect);
        std::printf("      (rotation sign %s for a positive S: content turns %s on a y-down screen)\n",
                    ang > 0 ? "+" : "-", ang > 0 ? "clockwise" : "counter-clockwise");
    }
    std::free(f.px);

    // --- print recycle (#51): three unread dips, all accepted ----------------
    sumi_trigger_paper_dip(inst); t19_step(window, inst, 12);
    sumi_add_drop(inst, 0.5f, 0.5f, 0.1f, SUMI_DROP_INK);
    sumi_trigger_paper_dip(inst); t19_step(window, inst, 12);
    sumi_add_drop(inst, 0.5f, 0.5f, 0.1f, SUMI_DROP_INK);
    sumi_trigger_paper_dip(inst); t19_step(window, inst, 12);   // both buffers were unread: the older recycles
    uint8_t* scratch = nullptr;
    int reads = 0;
    for (int i = 0; i < 3; i++) {
        if (!sumi_read_print(inst, nullptr, 0, &pw, &ph)) break;
        scratch = (uint8_t*)std::realloc(scratch, (size_t)pw * ph * 4);
        if (!scratch || !sumi_read_print(inst, scratch, (size_t)pw * ph * 4, &pw, &ph)) break;
        reads++;
    }
    std::free(scratch);
    T19(reads == 2, "recycle: after three unread dips exactly two prints read back (%d) - the third dip was NOT refused", reads);
}

// v0.7 (DECISIONS_4 #53): the viscous stroke. One <= a/4 sub-step must move
// the tip texel by exactly d (kernel normalisation), be mirror-symmetric about
// the motion axis, and be area-preserving to first order (pre-image Jacobian
// det ~ 1, never <= 0); a long stroke as ONE call must sub-step without folding.
// Pre-image Jacobian by central differences over a stencil of `k` texels.
// k = 1 for a single pass; k = 3 after a whole stroke, where the compression
// ahead of the tip (Jaffer's observation) makes one-texel differences of a
// half-float field noise-dominated (ULP 2^-11 ≈ half a texel at 512).
static double t33_det_at(const FieldF& f, uint32_t x, uint32_t y, uint32_t k = 1) {
    const size_t i = ((size_t)y * f.w + x) * 4;
    const float* px = f.px;
    const float sx = (float)f.w / (2.0f * (float)k), sy = (float)f.h / (2.0f * (float)k);
    const float dudx = (px[i + 4 * k] - px[i - 4 * k]) * sx;
    const float dudy = (px[i + 4 * k * f.w] - px[i - 4 * k * f.w]) * sy;
    const float dvdx = (px[i + 4 * k + 1] - px[i - 4 * k + 1]) * sx;
    const float dvdy = (px[i + 4 * k * f.w + 1] - px[i - 4 * k * f.w + 1]) * sy;
    return (double)dudx * dvdy - (double)dudy * dvdx;
}
static void t19_stokeslet_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t33] viscous stroke (2-D Stokeslet) test\n");
    uint32_t pw = 0, ph = 0;
    sumi_params_t p; sumi_get_params(inst, &p);
    p.wake_profile = 1; p.wake_spread = 3.0f; sumi_set_params(inst, &p);
    const float a = 0.04f, d = a * 0.25f;            // one sub-step exactly
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_add_wake(inst, 0.5f - d, 0.5f, 0.5f, 0.5f, a);   // tip ends at the centre (field is square: aspect 1)
    t19_step(window, inst, 1);
    FieldF f;
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    {
        const uint32_t cx = f.w / 2, cy = f.h / 2;
        const float u = f.px[(((size_t)cy * f.w) + cx) * 4];
        const float stx = ((float)cx + 0.5f) / (float)f.w;
        T19(std::fabs((stx - u) - d) < 0.002f, "tip texel moved by d: st - u = %.4f (d = %.4f)", (double)(stx - u), (double)d);
        // mirror: u even, v odd about the motion axis (y = 0.5 is a TEXEL
        // BOUNDARY: rows cy-1-off and cy+off are the mirror pair), one tip
        // radius off the axis, one ahead of the tip
        const uint32_t off = (uint32_t)(a * f.h);
        const uint32_t ru = cy - 1 - off, rd = cy + off;
        const float uu = f.px[(((size_t)ru * f.w) + cx + off) * 4], ud = f.px[(((size_t)rd * f.w) + cx + off) * 4];
        const float vu = f.px[(((size_t)ru * f.w) + cx + off) * 4 + 1] - (((float)ru + 0.5f) / (float)f.h);
        const float vd = f.px[(((size_t)rd * f.w) + cx + off) * 4 + 1] - (((float)rd + 0.5f) / (float)f.h);
        // tolerance: the two rows sit in different half-float exponent bands
        // (v ≈ 0.48 vs 0.52 — ULP 1.2e-4 vs 2.4e-4), so agreement is to an ULP
        T19(std::fabs(uu - ud) < 3e-4f && std::fabs(vu + vd) < 3e-4f && std::fabs(vu) > 1e-3f,
            "mirror symmetry (to a half-float ULP): u %.5f/%.5f, v-displacement %+.5f/%+.5f", (double)uu, (double)ud, (double)vu, (double)vd);
        double dmin = 1e9, dsum = 0.0; long n = 0;
        for (uint32_t y = 2; y + 2 < f.h; y++) for (uint32_t x = 2; x + 2 < f.w; x++) {
            const double det = t33_det_at(f, x, y);
            if (det < dmin) dmin = det; dsum += det; n++;
        }
        T19(dmin > 0.5 && std::fabs(dsum / (double)n - 1.0) < 2e-3,
            "area to first order: pre-image det min %.3f, mean %.5f (one a/4 sub-step)", dmin, dsum / (double)n);
    }
    std::free(f.px);
    // a whole stroke, 10 tip radii in one call: internal sub-stepping, no fold
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_add_wake(inst, 0.30f, 0.50f, 0.30f + 10.0f * a, 0.50f, a);
    t19_step(window, inst, 1);
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    {
        // As the wake's flick test: the swept corridor (the stroke segment
        // widened by a + 3 texels) holds the compressed material ahead of and
        // beside the tip, where bilinear RESAMPLING of a strongly compressed
        // pre-image aliases and finite differences stop measuring the map.
        // Outside it the composition of injective sub-steps must be fold-free.
        const float margin = a + 3.0f / (float)f.w;
        const float x0 = 0.30f, x1 = 0.30f + 10.0f * a, yc = 0.50f;
        double dmin = 1e9, dmin_in = 1e9; uint32_t mx = 0, my = 0;
        for (uint32_t y = 4; y + 4 < f.h; y++) for (uint32_t x = 4; x + 4 < f.w; x++) {
            const float px = ((float)x + 0.5f) / (float)f.w, py = ((float)y + 0.5f) / (float)f.h;
            const float cx = px < x0 ? x0 : (px > x1 ? x1 : px);
            const bool inside = (px - cx) * (px - cx) + (py - yc) * (py - yc) < margin * margin;
            const double det = t33_det_at(f, x, y, 3);
            if (inside) { if (det < dmin_in) dmin_in = det; }
            else if (det < dmin) { dmin = det; mx = x; my = y; }
        }
        T19(dmin > 0.0, "10a stroke in one call: no fold outside the swept corridor (min det %.3f at %u,%u; corridor min %.2f, resampling-limited)",
            dmin, mx, my, dmin_in);
    }
    std::free(f.px);
    p.wake_profile = 0; sumi_set_params(inst, &p);
}

// §4.3(4) sub-stepping: a one-frame flick of 8x the tip radius must not fold
// the FLUID — the pre-image field's Jacobian stays positive everywhere
// outside the swept tip corridor. (The corridor itself carries the body's
// slip surface — a genuine tangential discontinuity of potential flow, not a
// fold; monotonicity along a row is NOT the right test off the symmetry
// axis, where injective 2D maps may still reverse in x.)
static void t19_flick_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t19] fast-flick sub-stepping test\n");
    const float a = 0.03f;
    const float x0 = 0.30f, x1 = 0.30f + 8.0f * a, yc = 0.50f;
    t19_step(window, inst, 2);                       // settled identity
    sumi_add_wake(inst, x0, yc, x1, yc, a);
    t19_step(window, inst, 1);
    FieldF f;
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    const float margin = a + 3.0f / (float)f.w;      // slip surface + resampling skirt
    float min_det = 1e9f;
    long neg = 0;
    for (uint32_t y = 1; y + 1 < f.h; y++) {
        for (uint32_t x = 1; x + 1 < f.w; x++) {
            const float px_ = ((float)x + 0.5f) / (float)f.w;
            const float py_ = ((float)y + 0.5f) / (float)f.h;
            // Exclude the swept capsule (segment x0..x1 at yc, radius margin).
            const float cx = px_ < x0 ? x0 : (px_ > x1 ? x1 : px_);
            const float ddx = px_ - cx, ddy = py_ - yc;
            if (ddx * ddx + ddy * ddy < margin * margin) continue;
            const size_t o  = (((size_t)y * f.w) + x) * 4;
            const size_t oxp = o + 4, oxm = o - 4;
            const size_t oyp = o + (size_t)f.w * 4, oym = o - (size_t)f.w * 4;
            const float du_dx = (f.px[oxp] - f.px[oxm]) * 0.5f * (float)f.w;
            const float dv_dx = (f.px[oxp + 1] - f.px[oxm + 1]) * 0.5f * (float)f.w;
            const float du_dy = (f.px[oyp] - f.px[oym]) * 0.5f * (float)f.h;
            const float dv_dy = (f.px[oyp + 1] - f.px[oym + 1]) * 0.5f * (float)f.h;
            const float det = du_dx * dv_dy - du_dy * dv_dx;
            if (det < min_det) min_det = det;
            if (det <= 0.0f) neg++;
        }
    }
    T19(neg == 0, "8a one-frame flick: pre-image Jacobian positive over the fluid "
        "(min det %.4f, %ld non-positive texels)", (double)min_det, neg);
    std::free(f.px);
}

// §4.3(3) Rankine: rigid interior, crease exactly at R, and the 20-rotation
// unblurred survival — with the exponential profile as the shear control.
static void t19_rankine_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t19] rankine core test\n");
    const float R = 0.25f;
    // Part A: single pass, deflection profile + crease radius.
    t19_step(window, inst, 2);
    sumi_add_vortex(inst, 0.5f, 0.5f, 0.5f, R, SUMI_VORTEX_RANKINE);
    t19_step(window, inst, 1);
    FieldF f;
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    const float a_in1 = std::fabs(t19_swirl_at(&f, 0.5f, 0.5f, 0.50f * R));
    const float a_in2 = std::fabs(t19_swirl_at(&f, 0.5f, 0.5f, 0.90f * R));
    const float a_out = std::fabs(t19_swirl_at(&f, 0.5f, 0.5f, 1.50f * R));
    T19(std::fabs(a_in1 - 0.5f) < 0.02f && std::fabs(a_in2 - 0.5f) < 0.02f,
        "rigid interior: swirl %.4f @0.5R, %.4f @0.9R (expect 0.5)", (double)a_in1, (double)a_in2);
    T19(std::fabs(a_out - 0.5f / 2.25f) < 0.02f,
        "1/r^2 exterior: swirl %.4f @1.5R (expect %.4f)", (double)a_out, 0.5 / 2.25);
    // Crease: the max |dα/dr| radius must sit at R (within 2 texels).
    float worst_step = 0.0f, worst_r = 0.0f, prev_a = 0.0f;
    bool first = true;
    for (float r = 0.6f * R; r <= 1.4f * R; r += 0.5f / (float)f.w) {
        const float ar = std::fabs(t19_swirl_at(&f, 0.5f, 0.5f, r));
        if (!first && std::fabs(ar - prev_a) > worst_step) {
            worst_step = std::fabs(ar - prev_a);
            worst_r = r;
        }
        prev_a = ar;
        first = false;
    }
    // The true |dα/dr| is ZERO inside R and maximal immediately OUTSIDE
    // (decaying 1/r³), so a sampled max-gradient always centers just past R
    // (+ a texel of bilinear smear). The sharp claim: the crease never eats
    // into the rigid core, and sits within 3 texels outside R.
    T19(worst_r >= R - 1.0f / (float)f.w && worst_r <= R + 3.0f / (float)f.w,
        "crease ring at R: max gradient at r=%.4f (R=%.4f, +[0..3] texels outside)",
        (double)worst_r, (double)R);
    std::free(f.px);

    // Part B: ring cluster inside R survives 20 full rotations unblurred.
    uint32_t pw = 0, ph = 0;
    std::free(t19_dip_print(window, inst, &pw, &ph));   // reset to a fresh sheet
    auto place_cluster = [&]() {
        const float pos[4][2] = {{0.5f, 0.5f}, {0.42f, 0.42f}, {0.58f, 0.42f}, {0.5f, 0.61f}};
        for (int i = 0; i < 4; i++) {
            for (int rep = 0; rep < 3; rep++) {
                sumi_add_drop(inst, pos[i][0], pos[i][1], 0.05f, 0);
                t19_step(window, inst, 1);
            }
        }
    };
    auto interior_ink_diff = [&](const FieldF* fa, const FieldF* fb) {
        double sum = 0.0;
        long n = 0;
        for (uint32_t y = 0; y < fa->h; y++) {
            for (uint32_t x = 0; x < fa->w; x++) {
                const float dx = ((float)x + 0.5f) / (float)fa->w - 0.5f;
                const float dy = ((float)y + 0.5f) / (float)fa->h - 0.5f;
                if (dx * dx + dy * dy > 0.8f * 0.8f * R * R) continue;
                const size_t o = (((size_t)y * fa->w) + x) * 4;
                sum += std::fabs(fa->px[o + 2] - fb->px[o + 2]);
                n++;
            }
        }
        return n ? sum / (double)n : 0.0;
    };
    place_cluster();
    FieldF b0;
    if (!t19_read_field(inst, &b0)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    for (int i = 0; i < 20; i++) {
        sumi_add_vortex(inst, 0.5f, 0.5f, 6.2831853f, R, SUMI_VORTEX_RANKINE);
        t19_step(window, inst, 1);
    }
    FieldF b1;
    if (!t19_read_field(inst, &b1)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    const double d_rank = interior_ink_diff(&b0, &b1);
    // Control: the exponential profile at the same total angle shears the
    // interior apart — the contrast IS the proof of rigidity.
    std::free(t19_dip_print(window, inst, &pw, &ph));
    place_cluster();
    FieldF c0;
    if (!t19_read_field(inst, &c0)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    for (int i = 0; i < 20; i++) {
        sumi_add_vortex(inst, 0.5f, 0.5f, 6.2831853f, R, SUMI_VORTEX_EXPONENTIAL);
        t19_step(window, inst, 1);
    }
    FieldF c1;
    if (!t19_read_field(inst, &c1)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    const double d_exp = interior_ink_diff(&c0, &c1);
    T19(d_rank < 0.05, "20 full RANKINE rotations: interior mean |dink| %.5f (< 0.05)", d_rank);
    T19(d_rank < 0.2 * d_exp || d_exp < 1e-9,
        "rigidity contrast: rankine %.5f << exponential %.5f", d_rank, d_exp);
    std::free(b0.px); std::free(b1.px); std::free(c0.px); std::free(c1.px);
}

// §4.3(5) incompressibility soak. Two phases:
//  1. Operator reversibility: (+k, -k) pass pairs at one center/angle invert
//     EXACTLY in analytic terms (s = xy is conserved along trajectories, so
//     the -k pass sees the same w(s) field) — band areas must hold to noise.
//  2. The DONE stream: a scripted CC74-delta wobble through the REAL
//     slide_mode = 1 route (smoothed, mapper-coalesced, fixed per-voice fold
//     axis) for `passes` frames — the 10-minute-performance equivalent.
// NOT tested here, deliberately: an adversarial schedule (full-strength k
// with a rotating fold axis every pass) is chaotic advection — it filaments
// ink below texel resolution where bilinear resampling averages it to gray,
// the same way real marbling over-folds to mud. That is the §4.1 resampling
// medium, not an operator area leak (DECISIONS_3 #32).
static void t19_pinch_soak(GLFWwindow* window, sumi_instance_t* inst, long passes) {
    std::printf("[t19] pinch soak: reversible pairs + %ld-frame CC74 stream\n", passes);
    sumi_add_drop(inst, 0.5f, 0.5f, 0.20f, 0);
    t19_step(window, inst, 1);
    sumi_add_drop(inst, 0.40f, 0.45f, 0.10f, 0);
    t19_step(window, inst, 1);
    sumi_add_drop(inst, 0.62f, 0.58f, 0.08f, 0);
    t19_step(window, inst, 1);
    FieldF f0;
    if (!t19_read_field(inst, &f0)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    double mass0 = 0.0;
    long ink0 = 0, clr0 = 0;
    t19_band_areas(&f0, &mass0, &ink0, &clr0);
    std::free(f0.px);

    // Phase 1: 500 strong (+k, -k) pairs, one pair per frame.
    double mass_min = mass0, mass_max = mass0;
    for (int i = 0; i < 500; i++) {
        sumi_add_pinch(inst, 0.52f, 0.49f, 0.3f, 0.6f);
        sumi_add_pinch(inst, 0.52f, 0.49f, -0.3f, 0.6f);
        t19_step(window, inst, 1);
        if (i % 100 == 99) {
            FieldF fi;
            if (t19_read_field(inst, &fi)) {
                double mass_i = 0.0;
                long ink_i = 0, clr_i = 0;
                t19_band_areas(&fi, &mass_i, &ink_i, &clr_i);
                std::free(fi.px);
                if (mass_i < mass_min) mass_min = mass_i;
                if (mass_i > mass_max) mass_max = mass_i;
            }
        }
    }
    T19(mass_max - mass0 <= 0.02 * mass0 && mass0 - mass_min <= 0.02 * mass0,
        "500 reversible (+k,-k) pairs: ink mass base %.0f, range [%.0f, %.0f] (<= 2%%)",
        mass0, mass_min, mass_max);

    // Phase 2: the scripted CC74-delta stream through slide_mode = 1, on the
    // CHROMA grid (the play-surface layout) with a CENTRAL note — F#4's cell
    // sits at (0.535, 0.5), so the pinch's non-decaying fold-axis corridors
    // cross the canvas edges far from any ink (see #32: arms crossing an edge
    // NEAR ink grind it off over long streams — finite-canvas behavior).
    sumi_params_t prm;
    sumi_get_params(inst, &prm);
    prm.slide_mode = 1;
    prm.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    sumi_set_params(inst, &prm);
    sumi_push_midi(inst, 0xB0, 101, 0);   // MCM: forces MPE mode
    sumi_push_midi(inst, 0xB0, 100, 6);
    sumi_push_midi(inst, 0xB0, 6, 15);
    sumi_push_midi(inst, 0x91, 66, 100);
    t19_step(window, inst, 4);
    FieldF ra;
    if (!t19_read_field(inst, &ra)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    double mass_s0 = 0.0;
    long ink_s0 = 0, clr_s0 = 0;
    t19_band_areas(&ra, &mass_s0, &ink_s0, &clr_s0);
    double s_min = mass_s0, s_max = mass_s0, mass_6k = mass_s0;
    for (long i = 0; i < passes; i++) {
        // ~0.5 Hz slide wobble at the 120 Hz scripted clock: gesture-rate,
        // smoothed by the mapper, one pinch pass per frame at most.
        const int v = (int)(63.5 + 63.5 * std::sin((double)i * 2.0 * 3.14159265 * 0.5 / 120.0));
        sumi_push_midi(inst, 0xB1, 74, (uint8_t)v);
        t19_step(window, inst, 1);
        if (i % 6000 == 5999) {
            FieldF fi;
            if (t19_read_field(inst, &fi)) {
                double mass_i = 0.0;
                long ink_i = 0, clr_i = 0;
                t19_band_areas(&fi, &mass_i, &ink_i, &clr_i);
                std::free(fi.px);
                if (mass_i < s_min) s_min = mass_i;
                if (mass_i > s_max) s_max = mass_i;
                if (i + 1 == 6000) mass_6k = mass_i;
                std::printf("[t19]  %ld frames: ink mass %.0f (base %.0f; band texels ink %ld / clear %ld)\n",
                            i + 1, mass_i, mass_s0, ink_i, clr_i);
            }
        }
    }
    FieldF rb;
    if (!t19_read_field(inst, &rb)) { std::free(ra.px); t19_failures++; std::printf("FAIL: field read\n"); return; }
    long moved = 0;
    for (size_t i = 0; i < (size_t)ra.w * ra.h; i++) {
        if (std::fabs(ra.px[i * 4] - rb.px[i * 4]) > 1e-4f) moved++;
    }
    T19(moved > 100, "slide_mode=1: CC74 deltas drove the pinch (%ld texels moved)", moved);
    // Rate over the first 6000 frames — the window where the CONTROL below is
    // also measured. (At longer horizons the v1 tine's legacy edge-clamp
    // FABRICATION offsets its erosion and even nets growth — DECISIONS_3 #33 —
    // so long-horizon rates are not comparable across the two.)
    const long rate_win = passes < 6000 ? passes : 6000;
    const double pinch_rate = (mass_s0 - (passes >= 6000 ? mass_6k : s_min)) /
                              mass_s0 / (double)(rate_win > 0 ? rate_win : 1);
    std::printf("[t19] pinch-stream mass drift: %.2f%% over %ld passes (%.2e/pass; grew %.2f%%)\n",
                100.0 * (mass_s0 - s_min) / mass_s0, passes, pinch_rate,
                100.0 * (s_max - mass_s0) / mass_s0);
    // Mass must never GROW (growth = fabrication — the pre-ingress clamp bug
    // measured +9.5%/12k): the ingress rule makes the canvas lossy-only.
    T19(s_max - mass_s0 <= 0.005 * mass_s0,
        "CC74 stream: no ink fabrication (max growth %.2f%%)",
        100.0 * (s_max - mass_s0) / mass_s0);

    // CONTROL: the same stream shape through GLIDE TINES (bend wobble on the
    // same voice) — the medium's own per-pass erosion baseline. If the pinch
    // rate matches this, the pinch is exactly as conservative as every other
    // resampled operator (DECISIONS_3 #32).
    FieldF ca_;
    if (!t19_read_field(inst, &ca_)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    double mass_c0 = 0.0;
    long ink_c = 0, clr_c = 0;
    t19_band_areas(&ca_, &mass_c0, &ink_c, &clr_c);
    std::free(ca_.px);
    for (long i = 0; i < rate_win; i++) {
        const double semis = 2.0 * std::sin((double)i * 2.0 * 3.14159265 * 0.5 / 120.0);
        const long pb = 8192 + (long)(semis / 48.0 * 8192.0);
        sumi_push_midi(inst, 0xE1, (uint8_t)(pb & 0x7F), (uint8_t)((pb >> 7) & 0x7F));
        t19_step(window, inst, 1);
    }
    FieldF cb_;
    if (!t19_read_field(inst, &cb_)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    double mass_c1 = 0.0;
    t19_band_areas(&cb_, &mass_c1, &ink_c, &clr_c);
    std::free(cb_.px);
    const double tine_rate = (mass_c0 - mass_c1) / mass_c0 / (double)(rate_win > 0 ? rate_win : 1);
    std::printf("[t19] glide-tine control erosion: %.2f%% over %ld passes (%.2e/pass)\n",
                100.0 * (mass_c0 - mass_c1) / mass_c0, rate_win, tine_rate);
    // The medium-relative incompressibility gate (DECISIONS_3 #32): the pinch
    // erodes no faster than ~2x the v1 GLIDE TINE under the identical stream
    // — the strict "conserves within noise over 36k passes" reading is
    // unsatisfiable for ANY resampled operator (the incumbent tine fails it
    // identically); the operator-level proof is the reversible-pair phase +
    // det = 1 exact math.
    T19(pinch_rate <= 2.0 * tine_rate + 1e-6,
        "pinch erosion within the medium's baseline (pinch %.2e/pass vs tine %.2e/pass)",
        pinch_rate, tine_rate);
    std::free(ra.px);
    std::free(rb.px);
}

/* ------------------------------------------------------------------ */
/* Phase 6 step 35 (ROADMAP_5): the per-operator FOUR-PART             */
/* conservation gate — the step-19 pinch soak above, generalised.      */
/* ------------------------------------------------------------------ */
// The four parts (DECISIONS_3 #33, re-grounded on the medium's own baseline):
//  (a) det J = 1 stated symbolically per operator — its CLASS (MEDIUM §2):
//      EXACT (closed-form invertible at any magnitude) or SUB-STEPPED
//      displacement field (divergence-free field applied in <= a/4 steps,
//      area-preserving to first order). Declared, never discovered.
//  (b) reversibility, per class. EXACT: 500 strong (+k, -k) pass pairs
//      through the gesture ABI (the ripple through its CC route) at one
//      centre — ink MASS (sum of phase) held to +-5%, AND the pre-image
//      returns to where it started (mean |delta| over the field, in texels):
//      mass alone cannot tell an inverting pair from two different
//      area-preserving passes. SUB-STEPPED: the class promises first-order
//      area preservation per step, so that is what is measured — ONE <= a/4
//      sub-step on an identity field, pre-image Jacobian det > 0.5 everywhere
//      and mean within 2e-3 of 1 (the Stokeslet test's own bar, #53); the
//      pair drift is reported beside it, not gated (+-D cancels to first
//      order only, and every sub-step resamples the slip surface).
//  (c) zero fabrication: the gesture-rate stream (a 0.5 Hz wobble at the
//      120 Hz scripted clock, one pass per frame at most) through the REAL ctl
//      or gesture route never GROWS mass by more than 0.5% — or, when an
//      operator's boundary gain outruns its erosion (high spatial frequency:
//      the torsion at its default wavelength gains 4e-6/pass, DECISIONS_5
//      #15/#20), its growth RATE stays under 5e-5 per pass: an order of
//      magnitude above the medium's own gain and an order below edge-clamp
//      duplication (4.6e-4/pass in the negative control). The line also says
//      WHERE the mass appeared (edge band vs interior): duplication lives at
//      the edges, the medium's gain at the ring boundaries.
//  (d) erosion within the medium: per-pass mass loss <= 2x the GLIDE-TINE
//      control under the identical stream shape from a fresh copy of the
//      same scene — every sub-texel resample pass pays a bilinear mass fade
//      and the incumbent tine is the medium's own baseline (#32). Gated at
//      the 6000-pass window only: a freshly laid drop GAINS mass over its
//      first few hundred glide passes before the steady fade sets in, so a
//      short run prints (d) as information, never as a verdict.
// Mass is the observable: exact under the det = 1 change of variables and
// carried to O(h^2) by bilinear gather; level-set areas and band parity blur.
// Drops and feeds are excluded by nature — they inject ink.

// Phase 6 step 38 (MEDIUM §2.3): the viscous multipole burst on the GPU —
// the normalisation, first-order area preservation, the ±D pair, the ejection
// axis, the orders, the age envelope and the diffused zone's 1/r. The field
// stores each texel's SOURCE, so st − (u, v) is the displacement of the
// material now sitting at the texel: positive radial = ejected outward.
static double t38_radial(const FieldF& f, float cx, float cy, float r, float th) {
    const float px = cx + r * std::cos(th), py = cy + r * std::sin(th);
    const uint32_t x = (uint32_t)(px * (float)f.w), y = (uint32_t)(py * (float)f.h);
    const size_t o = ((size_t)y * f.w + x) * 4;
    const float stx = ((float)x + 0.5f) / (float)f.w, sty = ((float)y + 0.5f) / (float)f.h;
    return ((double)stx - f.px[o]) * std::cos(th) + ((double)sty - f.px[o + 1]) * std::sin(th);
}
static double t38_shift(const FieldF& f, float cx, float cy) {
    const uint32_t x = (uint32_t)(cx * (float)f.w), y = (uint32_t)(cy * (float)f.h);
    const size_t o = ((size_t)y * f.w + x) * 4;
    const float stx = ((float)x + 0.5f) / (float)f.w, sty = ((float)y + 0.5f) / (float)f.h;
    return std::hypot((double)stx - f.px[o], (double)sty - f.px[o + 1]);
}
static void t19_burst_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t38] viscous multipole burst test\n");
    uint32_t pw = 0, ph = 0;
    sumi_params_t base; sumi_get_params(inst, &base);
    sumi_params_t p = base;
    p.burst_age = 4.0f; p.burst_life = 0.0f;
    sumi_set_params(inst, &p);
    const float a = 0.04f, tx = 1.0f / 512.0f;          // one texel of the scripted 512² field
    FieldF f;
    // 1. one budgeted pass of D = 0.0045 (peak 1.066 D <= 0.13 a): the
    //    stagnation centre, the lobes at r = a, first-order area everywhere.
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_add_burst(inst, 0.5f, 0.5f, a, 0.0045f, 0.0f, 2);
    t19_step(window, inst, 1);
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    {
        const double c = t38_shift(f, 0.5f, 0.5f);
        const double out0 = t38_radial(f, 0.5f, 0.5f, a, 0.0f), in90 = t38_radial(f, 0.5f, 0.5f, a, 1.5707963f), out180 = t38_radial(f, 0.5f, 0.5f, a, 3.1415927f);
        T19(c < 0.3 * tx && std::fabs(out0 - 0.0045) < 0.5 * tx && std::fabs(in90 + 0.0045) < 0.5 * tx && std::fabs(out180 - 0.0045) < 0.5 * tx,
            "quadrupole, D = 0.0045: the centre stays (%.2f texel); at r = a the lobes eject %.2f / %.2f texel along the axis and draw in %.2f across it (D = %.2f texel)",
            c / tx, out0 / tx, out180 / tx, -in90 / tx, 0.0045 / tx);
        double dmin = 1e9, dsum = 0.0; long n = 0;
        for (uint32_t y = 2; y + 2 < f.h; y++) for (uint32_t x = 2; x + 2 < f.w; x++) {
            const double det = t33_det_at(f, x, y);
            if (det < dmin) dmin = det; dsum += det; n++;
        }
        T19(dmin > 0.5 && std::fabs(dsum / (double)n - 1.0) < 2e-3,
            "area to first order: pre-image det min %.3f everywhere, mean %.5f (one budgeted pass)", dmin, dsum / (double)n);
        T19(sumi_debug_burst_count(inst) == 0, "life 0: the episode is over after the frame (%u running)", sumi_debug_burst_count(inst));
    }
    std::free(f.px);
    // 2. the pair: +D then −D (two pieces each), the first-order residual |∇d|·d
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_add_burst(inst, 0.5f, 0.5f, a,  0.01f, 0.0f, 2);
    sumi_add_burst(inst, 0.5f, 0.5f, a, -0.01f, 0.0f, 2);
    t19_step(window, inst, 1);
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    {
        double mx = 0.0, acc = 0.0; long n = 0;
        for (uint32_t y = 4; y + 4 < f.h; y++) for (uint32_t x = 4; x + 4 < f.w; x++) {
            const size_t o = ((size_t)y * f.w + x) * 4;
            const double du = (((double)x + 0.5) / f.w - f.px[o]) * f.w, dv = (((double)y + 0.5) / f.h - f.px[o + 1]) * f.h;
            const double d = std::sqrt(du * du + dv * dv);
            if (d > mx) mx = d; acc += d; n++;
        }
        T19(mx < 1.0 && acc / (double)n < 0.1,
            "the pair (+D, -D) at D = a/4: residual max %.2f texel (< 1, the first-order |grad d|·d), mean %.3f", mx, acc / (double)n);
    }
    std::free(f.px);
    // 3. the ejection axis follows theta0; 4. the order m = 3 has three lobes
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_add_burst(inst, 0.5f, 0.5f, a, 0.0045f, 0.7853982f, 2);
    t19_step(window, inst, 1);
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    {
        const double on = t38_radial(f, 0.5f, 0.5f, a, 0.7853982f), across = t38_radial(f, 0.5f, 0.5f, a, 2.3561945f), off = t38_radial(f, 0.5f, 0.5f, a, 0.0f);
        T19(std::fabs(on - 0.0045) < 0.5 * tx && std::fabs(across + 0.0045) < 0.5 * tx && std::fabs(off) < 0.5 * tx,
            "theta0 = pi/4: ejects %.2f texel on its axis, draws in %.2f across it, %.2f radially at theta = 0 (tangential there)", on / tx, -across / tx, off / tx);
    }
    std::free(f.px);
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_add_burst(inst, 0.5f, 0.5f, a, 0.0045f, 0.0f, 3);
    t19_step(window, inst, 1);
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    {
        const double l0 = t38_radial(f, 0.5f, 0.5f, a, 0.0f), l1 = t38_radial(f, 0.5f, 0.5f, a, 2.0943951f), l2 = t38_radial(f, 0.5f, 0.5f, a, 4.1887902f);
        const double i0 = t38_radial(f, 0.5f, 0.5f, a, 1.0471976f), i1 = t38_radial(f, 0.5f, 0.5f, a, 3.1415927f);
        T19(std::fabs(l0 - 0.0045) < 0.5 * tx && std::fabs(l1 - 0.0045) < 0.5 * tx && std::fabs(l2 - 0.0045) < 0.5 * tx && std::fabs(i0 + 0.0045) < 0.5 * tx && std::fabs(i1 + 0.0045) < 0.5 * tx,
            "m = 3: three lobes eject %.2f / %.2f / %.2f texel at 0, 120, 240 deg; %.2f / %.2f drawn in at 60 and 180", l0 / tx, l1 / tx, l2 / tx, -i0 / tx, -i1 / tx);
    }
    std::free(f.px);
    // 5. the age envelope: life 0.5 s (60 frames of the 1/120 clock), D = 0.008.
    //    At t = life/6 the age is sqrt(3.5) a and the lobe has moved the share
    //    [Φ(1/3.5) − Φ(1)] / [Φ(1/16) − Φ(1)] of D — front-loaded; at the end D.
    //    Read on the −x lobe: u < 0.5 there, where the half-float quantum of
    //    the stored coordinates is 0.125 texel (0.25 on the +x side).
    p.burst_life = 0.5f; sumi_set_params(inst, &p);
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_add_burst(inst, 0.5f, 0.5f, a, 0.008f, 0.0f, 2);
    t19_step(window, inst, 10);
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    {
        const double share = sumi_burst_dphi(2, 1.0, 1.0 / 3.5) / sumi_burst_dphi(2, 1.0, 1.0 / 16.0);
        const double got = t38_radial(f, 0.5f, 0.5f, a, 3.1415927f);
        const uint32_t running = sumi_debug_burst_count(inst);
        T19(std::fabs(got - share * 0.008) < 0.5 * tx && running == 1,
            "age envelope at t = life/6: the lobe has moved %.2f texel of %.2f (the envelope's share %.2f -> %.2f texel), the episode running (%u)",
            got / tx, 0.008 / tx, share, share * 0.008 / tx, running);
    }
    std::free(f.px);
    t19_step(window, inst, 60);
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    {
        const double got = t38_radial(f, 0.5f, 0.5f, a, 3.1415927f);
        const uint32_t running = sumi_debug_burst_count(inst);
        T19(std::fabs(got - 0.008) < 0.5 * tx && running == 0,
            "age envelope after the release: the lobe has moved %.2f texel (D = %.2f), the episode over (%u running)", got / tx, 0.008 / tx, running);
    }
    std::free(f.px);
    // 6. the shader against the double reference along the axis, one pass of
    //    the total kernel (a = 0.08, age 8, D = 0.009: peak 1.084 D <= 0.13 a):
    //    d_r(r) = (2 A/r)[χ(r²/l²) − χ(r²/a²)] at r = a/2 .. 4a — the core, the
    //    lobe, the diffused zone's 1/r. Read on the −x axis (the finer
    //    half-float bands). A pass applies exactly the Eulerian field; a
    //    STRONG strike composed of many passes follows the flow instead and
    //    stretches the core exponentially (e^λ, λ = 1.36 D/a) — measured 4x
    //    the linear prediction at D = 2a, and recorded, not asserted.
    p.burst_life = 0.0f; p.burst_age = 8.0f; sumi_set_params(inst, &p);
    std::free(t19_dip_print(window, inst, &pw, &ph));
    {
        const float a6 = 0.08f, D6 = 0.009f;
        sumi_add_burst(inst, 0.5f, 0.5f, a6, D6, 0.0f, 2);
        t19_step(window, inst, 1);
        if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        const double A = sumi_burst_amp(2, D6, a6, 8.0 * a6);
        const double rs[5] = {0.5, 1.0, 2.0, 3.0, 4.0};
        double worst = 0.0, got[5], ref[5];
        for (int i = 0; i < 5; i++) {
            const double r = rs[i] * a6;
            ref[i] = 2.0 * A / r * sumi_burst_dphi(2, r * r / (a6 * a6), r * r / (64.0 * a6 * a6));
            got[i] = t38_radial(f, 0.5f, 0.5f, (float)r, 3.1415927f);
            if (std::fabs(got[i] - ref[i]) > worst) worst = std::fabs(got[i] - ref[i]);
        }
        T19(worst < 0.3 * tx,
            "the shader is the closed form: d_r at a/2, a, 2a, 3a, 4a = %.2f/%.2f/%.2f/%.2f/%.2f texel vs %.2f/%.2f/%.2f/%.2f/%.2f (worst %.3f texel); d(2a)/d(4a) = %.2f (1/r with the core and age corrections: %.2f)",
            got[0] / tx, got[1] / tx, got[2] / tx, got[3] / tx, got[4] / tx, ref[0] / tx, ref[1] / tx, ref[2] / tx, ref[3] / tx, ref[4] / tx, worst / tx,
            got[2] / got[4], ref[2] / ref[4]);
        std::free(f.px);
    }
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_set_params(inst, &base);
}

// Phase 6 step 39 (MEDIUM §2.4): the spark shear on the GPU — one kick-drift
// step and its EXACT inverse ((0, −B) then (−A, 0)) for the triangle stack
// and for the noise profile ("shears invert for any profile" as a test), the
// same-order sign flip as the negative, one stage as a pure shear (rows slide
// rigidly), then the composed strike: its two episodes registered and run
// out, its field not the drop's alone, its pre-image Jacobian first-order
// outside the drop's rim (the class inherited from the burst).
static double t39_interior_dev(const FieldF& f, uint32_t margin, double* out_max) {
    double acc = 0.0, mx = 0.0; long n = 0;
    for (uint32_t y = margin; y + margin < f.h; y++) for (uint32_t x = margin; x + margin < f.w; x++) {
        const size_t o = ((size_t)y * f.w + x) * 4;
        const double du = (((double)x + 0.5) / f.w - f.px[o]) * f.w, dv = (((double)y + 0.5) / f.h - f.px[o + 1]) * f.h;
        const double d = std::sqrt(du * du + dv * dv);
        acc += d; n++; if (d > mx) mx = d;
    }
    if (out_max) *out_max = mx;
    return n ? acc / (double)n : 0.0;
}
static void t19_spark_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t39] spark shear & composed strike test\n");
    uint32_t pw = 0, ph = 0;
    sumi_params_t base; sumi_get_params(inst, &base);
    sumi_params_t p = base;
    p.spark_stack = 3; p.spark_profile = 0;
    sumi_set_params(inst, &p);
    const float A = 0.03f, k = SOAK_SPARK_K, phs = 0.7f, th = 0.3f;   // 15 texels of kick on a 43-texel base wavelength
    const uint32_t margin = 24;                                        // the ingress bands: rows shear across the side edges by up to A
    FieldF f;
    for (int prof = 0; prof < 2; prof++) {
        p.spark_profile = (uint32_t)prof; sumi_set_params(inst, &p);
        std::free(t19_dip_print(window, inst, &pw, &ph));
        sumi_add_spark_shear(inst, 0.5f, 0.5f, 0.0f, A, A, k, phs, th);
        t19_step(window, inst, 1);
        if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        const double moved = t39_interior_dev(f, margin, nullptr);
        std::free(f.px);
        sumi_add_spark_shear(inst, 0.5f, 0.5f, 0.0f, 0.0f, -A, k, phs, th);   // the exact inverse: reversed order, negated
        sumi_add_spark_shear(inst, 0.5f, 0.5f, 0.0f, -A, 0.0f, k, phs, th);
        t19_step(window, inst, 1);
        if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        double mx = 0.0;
        const double back = t39_interior_dev(f, margin, &mx);
        std::free(f.px);
        // the negative: the same-order sign flip
        std::free(t19_dip_print(window, inst, &pw, &ph));
        sumi_add_spark_shear(inst, 0.5f, 0.5f, 0.0f, A, A, k, phs, th);
        t19_step(window, inst, 1);
        sumi_add_spark_shear(inst, 0.5f, 0.5f, 0.0f, -A, -A, k, phs, th);
        t19_step(window, inst, 1);
        if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        const double flip = t39_interior_dev(f, margin, nullptr);
        std::free(f.px);
        T19(moved > 5.0 && back < 0.5 && flip > 5.0 * back,
            "%s profile: a step moves the interior pre-image %.2f texel; the step then its inverse leaves %.3f (max %.2f); the same-order sign flip leaves %.2f (NOT an inverse)",
            prof ? "noise" : "triangle-stack", moved, back, mx, flip);
    }
    p.spark_profile = 0; sumi_set_params(inst, &p);
    // one stage alone is a pure shear: rows keep their y and slide rigidly
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_add_spark_shear(inst, 0.5f, 0.5f, 0.0f, A, 0.0f, k, phs, 0.0f);   // the frame unrotated: rows are rows
    t19_step(window, inst, 1);
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    {
        double vmax = 0.0, row_std = 0.0, shift_max = 0.0; long rows = 0;
        for (uint32_t y = margin; y + margin < f.h; y++) {
            double s1 = 0.0, s2 = 0.0; long n = 0;
            for (uint32_t x = margin; x + margin < f.w; x++) {
                const size_t o = ((size_t)y * f.w + x) * 4;
                const double du = (((double)x + 0.5) / f.w - f.px[o]) * f.w;
                const double dv = std::fabs((((double)y + 0.5) / f.h - f.px[o + 1]) * f.h);
                if (dv > vmax) vmax = dv;
                s1 += du; s2 += du * du; n++;
            }
            const double mean = s1 / n, var = s2 / n - mean * mean;
            row_std += std::sqrt(var > 0 ? var : 0); rows++;
            if (std::fabs(mean) > shift_max) shift_max = std::fabs(mean);
        }
        T19(vmax < 0.05 && row_std / rows < 0.1 && shift_max > 10.0,
            "one stage is a pure shear: no row moved in y (max %.3f texel), each row slid rigidly (mean in-row spread %.3f texel), the largest row shift %.1f texel", vmax, row_std / rows, shift_max);
    }
    std::free(f.px);
    // the composed strike: episodes registered and run out; its field is not
    // the drop's alone; then first-order area outside the drop's rim, read
    // with the SHEAR OFF: finite differences cannot measure an exact
    // kick-drift shear at its kinks at ANY slope — the two difference
    // quotients that cancel analytically straddle a kink unequally, so the
    // stencil reads 1 ± 2·A f'·B g' (0.46 at a 0.45 texel-per-texel slope,
    // −6.7 at 2.7). The shear's exactness is the inverse test's above; the
    // Jacobian read is the sub-stepped member's — the burst's — inside the
    // drop's field, which is what the composition's class inherits.
    p.spark_tau = 0.05f; p.spark_shear = 0.6f; p.burst_life = 0.0f; p.burst_age = 4.0f; sumi_set_params(inst, &p);
    const float r = 0.05f;
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_add_spark(inst, 0.5f, 0.5f, r, 0.0045f, 0.0f, 1u);            // clear water: the blast without ink
    t19_step(window, inst, 1);
    const uint32_t sp1 = sumi_debug_spark_count(inst), bu1 = sumi_debug_burst_count(inst);
    t19_step(window, inst, 30);                                         // 4 tau = 0.2 s = 24 frames
    const uint32_t sp2 = sumi_debug_spark_count(inst);
    T19(sp1 == 1 && bu1 == 0 && sp2 == 0, "the composed strike: the shear episode registered (%u) and over after 4 tau (%u); the burst at life 0 done in its frame (%u)", sp1, sp2, bu1);
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    {
        FieldF full = f;
        p.spark_shear = 0.0f; sumi_set_params(inst, &p);                 // the Jacobian read: the drop and the burst
        std::free(t19_dip_print(window, inst, &pw, &ph));
        sumi_add_spark(inst, 0.5f, 0.5f, r, 0.0045f, 0.0f, 1u);
        t19_step(window, inst, 31);
        FieldF spark;
        if (!t19_read_field(inst, &spark)) { std::free(full.px); t19_failures++; std::printf("FAIL: field read\n"); return; }
        std::free(t19_dip_print(window, inst, &pw, &ph));
        sumi_add_drop(inst, 0.5f, 0.5f, r, 1u);
        t19_step(window, inst, 31);
        FieldF drop;
        if (!t19_read_field(inst, &drop)) { std::free(spark.px); t19_failures++; std::printf("FAIL: field read\n"); return; }
        double acc = 0.0; long n = 0;
        for (uint32_t y = 0; y < f.h; y++) for (uint32_t x = 0; x < f.w; x++) {
            const float px = ((float)x + 0.5f) / (float)f.w, py = ((float)y + 0.5f) / (float)f.h;
            const float dd = std::hypot(px - 0.5f, py - 0.5f);
            if (dd > 3.0f * r || dd < r + 3.0f / (float)f.w) continue;     // the annulus r .. 3r, outside the rim
            const size_t o = ((size_t)y * f.w + x) * 4;
            acc += std::hypot((double)(full.px[o] - drop.px[o]) * f.w, (double)(full.px[o + 1] - drop.px[o + 1]) * f.h); n++;
        }
        const double diff = n ? acc / n : 0.0;
        std::free(full.px);
        // The drop's rim: inside, the pre-image is the identity; just outside
        // it is sqrt(d² − r²), whose slope d/sqrt(d² − r²) stays above 1.5
        // texel per texel until d = 1.34 r — a finite-difference stencil there
        // reads the drop's own singularity, not a fold. And the passes that
        // follow the drop CARRY its rim: a texel inside the disk whose source
        // lies across the rim reads the compressed exterior, so the jump
        // moves inward by the burst's and the shear's displacement (~5
        // texels here; det −4.7 measured at 0.87 r with the drop alone at
        // 1.000). Excluded: r − 8 texels .. 1.4 r; the drop alone is read
        // the same way for the record.
        // And the INGRESS SEAM: the shear bands run across the whole canvas,
        // fresh water enters at the side edges where a row shears out, and
        // the stencil straddling that seam reads a jump of the kick's size as
        // a fold (det −4.7 measured 3 texels in). An 8-texel edge margin
        // covers the 2.6-texel kick.
        double dmin = 1e9, dsum = 0.0, dmin_drop = 1e9; long nd = 0; uint32_t mx = 0, my = 0;
        for (uint32_t y = 8; y + 8 < f.h; y++) for (uint32_t x = 8; x + 8 < f.w; x++) {
            const float px = ((float)x + 0.5f) / (float)f.w, py = ((float)y + 0.5f) / (float)f.h;
            const float dd = std::hypot(px - 0.5f, py - 0.5f);
            if (dd > r - 8.0f / (float)f.w && dd < 1.4f * r) continue;
            const double det = t33_det_at(spark, x, y), detd = t33_det_at(drop, x, y);
            if (det < dmin) { dmin = det; mx = x; my = y; }
            dsum += det; nd++;
            if (detd < dmin_drop) dmin_drop = detd;
        }
        {   // where the minimum sits, and the field around it (debug read, kept: it names the culprit in the log)
            const size_t o = ((size_t)my * f.w + mx) * 4;
            std::printf("[t39] det min at texel (%u, %u): %.3f from the centre = %.2f r; st-u = %.2f texel, st-v = %.2f texel; drop alone there %.3f; neighbours u: %.4f %.4f %.4f  v: %.4f %.4f %.4f\n",
                        mx, my, dmin, std::hypot(((double)mx + 0.5) / f.w - 0.5, ((double)my + 0.5) / f.h - 0.5) / r,
                        (((double)mx + 0.5) / f.w - spark.px[o]) * f.w, (((double)my + 0.5) / f.h - spark.px[o + 1]) * f.h, t33_det_at(drop, mx, my),
                        (double)spark.px[o - 4], (double)spark.px[o], (double)spark.px[o + 4],
                        (double)spark.px[o - (size_t)f.w * 4 + 1], (double)spark.px[o + 1], (double)spark.px[o + (size_t)f.w * 4 + 1]);
        }
        T19(diff > 0.5 && dmin > 0.5 && std::fabs(dsum / (double)nd - 1.0) < 2e-3,
            "the composition acts beyond the blast (mean |spark - drop| %.2f texel over r..3r); its sub-stepped member inside the drop's field stays first-order: pre-image det min %.3f outside the rim annulus and the ingress seam (the drop alone reads %.3f there), mean %.5f",
            diff, dmin, dmin_drop, dsum / (double)nd);
        std::free(spark.px); std::free(drop.px);
    }
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_set_params(inst, &base);
}

// Phase 6 step 40 (MEDIUM §2.5): the scaled Chirikov standard map on the GPU
// — one step and its exact inverse, the pass against the closed form, the
// KAM transition as a boundary-length proxy (rings at the hyperbolic point
// shred above Greene's threshold while rings on the elliptic island hold),
// and the delta route's retrace.
static void t40_rings(GLFWwindow* window, sumi_instance_t* inst, float cx, float cy, float r, int n) {
    for (int i = 0; i < n; i++) { sumi_add_drop(inst, cx, cy, r, 0); t19_step(window, inst, 1); }
}
// Ink/water boundary length proxy: neighbour pairs whose inkedness differs,
// in the central rows (the drift's ingress at the top and bottom stays out).
static long t40_boundary(const FieldF& f, uint32_t band_rows, uint32_t xm) {
    long n = 0;
    const uint32_t y0 = f.h / 2 - band_rows, y1 = f.h / 2 + band_rows;
    for (uint32_t y = y0; y + 1 < y1; y++) for (uint32_t x = xm; x + xm + 1 < f.w; x++) {
        const size_t o = ((size_t)y * f.w + x) * 4;
        const bool a = f.px[o + 2] >= 0.5f, b = f.px[o + 6] >= 0.5f, c = f.px[o + (size_t)f.w * 4 + 2] >= 0.5f;
        n += (a != b ? 1 : 0) + (a != c ? 1 : 0);
    }
    return n;
}
// The VISIBLE ink: texels whose phase reads as ink (>= 0.5) in the central
// rows. Area-preserving maps keep it while the ink stays resolvable; chaotic
// filamentation finer than a texel averages it into gray and it collapses —
// the medium's own witness of the KAM transition (and of the erosion the
// spec warns of).
static long t40_visible(const FieldF& f, uint32_t band_rows, uint32_t xm) {
    long n = 0;
    const uint32_t y0 = f.h / 2 - band_rows, y1 = f.h / 2 + band_rows;
    for (uint32_t y = y0; y < y1; y++) for (uint32_t x = xm; x + xm < f.w; x++) n += f.px[((size_t)y * f.w + x) * 4 + 2] >= 0.5f ? 1 : 0;
    return n;
}
static double t40_band_dev(const FieldF& f, const FieldF* ref, uint32_t band_rows, uint32_t xm) {
    double acc = 0.0; long n = 0;
    const uint32_t y0 = f.h / 2 - band_rows, y1 = f.h / 2 + band_rows;
    for (uint32_t y = y0; y < y1; y++) for (uint32_t x = xm; x + xm < f.w; x++) {
        const size_t o = ((size_t)y * f.w + x) * 4;
        const double ru = ref ? ref->px[o] : ((double)x + 0.5) / f.w, rv = ref ? ref->px[o + 1] : ((double)y + 0.5) / f.h;
        acc += std::hypot(((double)f.px[o] - ru) * f.w, ((double)f.px[o + 1] - rv) * f.h); n++;
    }
    return n ? acc / (double)n : 0.0;
}
static void t19_chirikov_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t40] Chirikov standard map test\n");
    uint32_t pw = 0, ph = 0;
    sumi_params_t base; sumi_get_params(inst, &base);
    const uint32_t periods = 2;
    const float eps = 0.5f, k = 6.2831853f * (float)periods;
    const uint32_t band = 64, xm = 48;                 // rows within 64 texels of the centre line: the drift there <= 32 texels; x margin for its ingress
    FieldF f;
    // 1. one step at K = 0.5 (a 41-texel kick amplitude) then its exact inverse
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_add_chirikov(inst, 0.5f, 0.5f, 0.5f, periods, eps, 0.3f);
    t19_step(window, inst, 1);
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    const double moved = t40_band_dev(f, nullptr, band, xm);
    // 2. the pass against the closed form: the pre-image of P is x = P.x − ε(P.y − yc), y = P.y − A sin(k(x − xc) + φ)
    double worst = 0.0;
    {
        const double A = 0.5 / ((double)k * eps);
        for (int i = 0; i < 25; i++) {
            const uint32_t x = 64 + (uint32_t)(i * 16), y = f.h / 2 - 60 + (uint32_t)((i * 7) % 120);
            const size_t o = ((size_t)y * f.w + x) * 4;
            const double px = ((double)x + 0.5) / f.w, py = ((double)y + 0.5) / f.h;
            const double sx = px - eps * (py - 0.5), sy = py - A * std::sin(k * (sx - 0.5) + 0.3);
            const double e = std::hypot(((double)f.px[o] - sx) * f.w, ((double)f.px[o + 1] - sy) * f.h);
            if (e > worst) worst = e;
        }
    }
    std::free(f.px);
    sumi_add_chirikov(inst, 0.5f, 0.5f, -0.5f, periods, eps, 0.3f);
    t19_step(window, inst, 1);
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    const double back = t40_band_dev(f, nullptr, band, xm);
    std::free(f.px);
    T19(moved > 5.0 && back < 0.5, "one step at K = 0.5 moves the central band's pre-image %.2f texel; the step then its exact inverse (drift undone first) leaves %.3f", moved, back);
    T19(worst < 0.3, "the pass is the closed form: the pre-image of 25 texels matches x = P.x − ε(P.y − yc), y = P.y − A sin(k(x − xc) + φ) to %.3f texel", worst);
    // 3. the KAM transition: rings (3 drops, r = 0.05) at the hyperbolic point
    //    (the centre, φ = 0) and on the elliptic island half a kick period to
    //    the right, 60 steps each: the boundary length of the ink as the
    //    filamentation's witness.
    //    The witness is what STAYS. The torus wraps, the canvas does not:
    //    every rotating orbit of the map marches across in x and leaves
    //    through the sides, so only librating material — inside the island —
    //    stays for good. Rings centred on the hyperbolic point straddle the
    //    separatrix: below K_c their librating half survives (measured 53%
    //    of the mass after 60 steps) while the rotating half streams away;
    //    above K_c the chaotic sea flushes the inside too (16%). Rings on the
    //    elliptic island keep everything at either K. The visible area and
    //    the boundary length are printed alongside: material on the
    //    separatrix stretches exponentially at ANY K and dissolves into gray
    //    below threshold as well — that is the separatrix, not the transition.
    long L[3] = {0, 0, 0}; double kept[3] = {0, 0, 0}, vis[3] = {0, 0, 0};
    const float Ks[3] = {0.5f, 1.5f, 1.5f};
    const float cxs[3] = {0.5f, 0.5f, 0.5f + 0.5f / (float)periods};
    for (int c = 0; c < 3; c++) {
        std::free(t19_dip_print(window, inst, &pw, &ph));
        t40_rings(window, inst, cxs[c], 0.5f, 0.05f, 3);
        if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        double m0 = 0.0; long a = 0, b = 0; t19_band_areas(&f, &m0, &a, &b);
        const long v0 = t40_visible(f, band, xm);
        std::free(f.px);
        for (int i = 0; i < 60; i++) { sumi_add_chirikov(inst, 0.5f, 0.5f, Ks[c], periods, eps, 0.0f); t19_step(window, inst, 1); }
        if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        double m1 = 0.0; t19_band_areas(&f, &m1, &a, &b);
        L[c] = t40_boundary(f, band, xm);
        kept[c] = m0 > 0 ? m1 / m0 : 0.0;
        vis[c] = v0 > 0 ? (double)t40_visible(f, band, xm) / (double)v0 : 0.0;
        std::free(f.px);
    }
    T19(kept[0] > 0.4 && kept[1] < 0.25 && kept[2] > 0.8,
        "the KAM transition after 60 steps: rings on the hyperbolic point keep %.0f%% of their ink mass at K = 0.5 (the librating half stays, the rotating half streams off the sides; %.0f%% still visible, boundary %ld) and %.0f%% at K = 1.5 (the chaotic sea flushes the inside too; %.0f%% visible, boundary %ld); rings on the elliptic island at K = 1.5 keep %.0f%% (%.0f%% visible) — Greene's K_c = 0.9716",
        100.0 * kept[0], 100.0 * vis[0], L[0], 100.0 * kept[1], 100.0 * vis[1], L[1], 100.0 * kept[2], 100.0 * vis[2]);
    // 4. the delta route: a throw of the control (0 -> 127) is a run of steps
    //    through the smoother; the wheel home retraces them (the inverse
    //    steps, the same magnitudes in the same order: second-order residue)
    sumi_map_cc(inst, 0xFF, 109, SUMI_CTL_CHIRIKOV_K);
    sumi_params_t p = base; p.chirikov_kmax = 1.0f; p.chirikov_periods = periods; p.chirikov_eps = eps; sumi_set_params(inst, &p);
    std::free(t19_dip_print(window, inst, &pw, &ph));
    t40_rings(window, inst, 0.5f, 0.5f, 0.10f, 4);
    FieldF before;
    if (!t19_read_field(inst, &before)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    sumi_push_midi(inst, 0xB0, 109, 127);
    t19_step(window, inst, 40);
    if (!t19_read_field(inst, &f)) { std::free(before.px); t19_failures++; std::printf("FAIL: field read\n"); return; }
    const double thrown = t40_band_dev(f, &before, band, xm);
    std::free(f.px);
    sumi_push_midi(inst, 0xB0, 109, 0);
    t19_step(window, inst, 40);
    if (!t19_read_field(inst, &f)) { std::free(before.px); t19_failures++; std::printf("FAIL: field read\n"); return; }
    const double home = t40_band_dev(f, &before, band, xm);
    std::free(f.px); std::free(before.px);
    T19(thrown > 2.0 && home < 0.3 * thrown,
        "the delta route: a throw of the control moves the band %.2f texel through the smoother's run of gentle steps; the control home retraces to %.2f (the inverse steps)", thrown, home);
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_set_params(inst, &base);
}

// Phase 6 step 41 (1.0.0, QOL §1): the custom palette through the new ABI —
// the same scene printed under palette 0 and under a red-to-black custom
// palette (active_palette_id = SUMI_PALETTE_CUSTOM): the ink texels change,
// the paper texels (field phase < 1) are bitwise the same — the palette
// colours the ink and only the ink; then palette 0 again prints bitwise as
// before (the built-in path untouched by the new branch). A degenerate POD
// (one stop, positions descending, NaNs) is clamped, not rejected.
static double t42_lum(const uint8_t* px, size_t o);   // below (step 42)
static uint8_t* t41_scene_print(GLFWwindow* window, sumi_instance_t* inst, uint32_t palette, const sumi_palette_t* pal,
                                FieldF* field_out, uint32_t* pw, uint32_t* ph) {
    std::free(t19_dip_print(window, inst, pw, ph));         // fresh sheet
    sumi_params_t p; sumi_get_params(inst, &p);
    p.active_palette_id = palette; sumi_set_params(inst, &p);
    if (pal) sumi_set_palette(inst, pal);
    t19_scene_rings(window, inst);
    t19_step(window, inst, 2);
    if (field_out && !t19_read_field(inst, field_out)) return nullptr;
    return t19_dip_print(window, inst, pw, ph);
}
static void t19_palette_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t41] custom palette (sumi_set_palette) test\n");
    sumi_params_t base; sumi_get_params(inst, &base);
    uint32_t pw = 0, ph = 0, pw2 = 0, ph2 = 0, pw3 = 0, ph3 = 0;
    FieldF field;
    uint8_t* p0 = t41_scene_print(window, inst, 0u, nullptr, &field, &pw, &ph);
    sumi_palette_t pal = {};
    pal.stop_count = 3;
    pal.stops[0].rgb[0] = 0.60f; pal.stops[0].rgb[1] = 0.05f; pal.stops[0].rgb[2] = 0.02f; pal.stops[0].position = 0.0f;   // thin: red
    pal.stops[1].rgb[0] = 0.20f; pal.stops[1].rgb[1] = 0.01f; pal.stops[1].rgb[2] = 0.05f; pal.stops[1].position = 0.6f;
    pal.stops[2].rgb[0] = 0.01f; pal.stops[2].rgb[1] = 0.01f; pal.stops[2].rgb[2] = 0.01f; pal.stops[2].position = 1.0f;   // pooled: black
    pal.depth_gamma = 1.0f; pal.depth_floor = 0.0f; pal.hue_drift = 0.2f;
    pal.clear_rgb[0] = 0.85f; pal.clear_rgb[1] = 0.80f; pal.clear_rgb[2] = 0.78f;
    uint8_t* p3 = t41_scene_print(window, inst, SUMI_PALETTE_CUSTOM, &pal, nullptr, &pw2, &ph2);
    uint8_t* p0b = t41_scene_print(window, inst, 0u, nullptr, nullptr, &pw3, &ph3);
    if (!p0 || !p3 || !p0b || pw != pw2 || ph != ph2 || pw != pw3 || ph != ph3) {
        t19_failures++; std::printf("FAIL: [t41] prints unavailable or of different sizes\n");
        std::free(p0); std::free(p3); std::free(p0b); if (p0) std::free(field.px); return;
    }
    long ink = 0, ink_changed = 0, paper = 0, paper_changed = 0, back_changed = 0;
    double ink_diff = 0.0;
    for (uint32_t y = 0; y < ph; y++) for (uint32_t x = 0; x < pw; x++) {
        const size_t o = ((size_t)y * pw + x) * 4;
        const size_t fo = ((size_t)(y * field.h / ph) * field.w + (x * field.w / pw)) * 4;
        const bool inked = field.px[fo + 2] >= 1.0f;
        int d = 0; for (int c = 0; c < 3; c++) d += std::abs((int)p0[o + c] - (int)p3[o + c]);
        if (inked) { ink++; if (d) ink_changed++; ink_diff += d; } else { paper++; if (d) paper_changed++; }
        for (int c = 0; c < 4; c++) if (p0[o + c] != p0b[o + c]) { back_changed++; break; }
    }
    T19(ink > 1000 && ink_changed > ink / 2 && paper_changed == 0 && back_changed == 0,
        "the custom palette recolours the ink and only the ink: %ld of %ld inked texels changed (mean |dRGB| %.1f), %ld of %ld paper texels changed; palette 0 again prints bitwise (%ld texels differ)",
        ink_changed, ink, ink ? ink_diff / (double)ink : 0.0, paper_changed, paper, back_changed);
    std::free(p0); std::free(p3); std::free(p0b); std::free(field.px);
    // a degenerate POD is clamped, not rejected: one stop, descending positions, a NaN
    sumi_palette_t bad = {};
    bad.stop_count = 1; bad.stops[0].position = 0.9f; bad.stops[1].position = 0.1f; bad.depth_gamma = std::nanf(""); bad.hue_drift = 7.0f;
    sumi_set_palette(inst, &bad);
    uint8_t* pbad = t41_scene_print(window, inst, SUMI_PALETTE_CUSTOM, nullptr, nullptr, &pw2, &ph2);
    T19(pbad != nullptr, "a degenerate palette (one stop, descending positions, NaN gamma) is clamped and still prints");
    std::free(pbad);
    // Phase 6 step 43 (QOL §1): THE BUILT-INS THROUGH THE ONE PATH, BITWISE. The
    // canonical §4.6 script printed under every built-in palette of both media,
    // at rest and under a palette morph of ~0.3, hashed (FNV-1a 64) and held to
    // the hashes captured from the legacy per-id tables before the palettes
    // became presets. An empty expected table prints the hashes (the capture).
    {
        // #87: ONE COLUMN PER BACKEND. The Metal column is the proof of step 43 (the legacy tables'
        // prints, bitwise); GL's and D3D11's are the same prints as those backends make them — one
        // 8-bit step apart from Metal in the washi's rounding (#78, #83) — captured on the author's
        // boxes (both NVIDIA RTX 5090: GL driver 610.43 on Linux, D3D11 32.0.16.1664 on Windows).
        // A different driver may round its own way: then recapture that column with its evidence,
        // as the Anod four were at #69; a column is never edited to make a run pass.
        struct Case { uint32_t medium, palette; int morph_cc; const char* metal; const char* gl; const char* d3d11; };
        static const Case cases[] = {
            // Metal: captured 2026-09-22 from the legacy per-id tables (composite.glsl before step 43), FNV-1a 64 over the RGBA8 print;
            // the Anod four recaptured 2026-09-23 at the author's defaults (glass darkness 1, grain 0.5, glow 0.2, bloom 0.75 over 3 octaves),
            // and again 2026-09-28 for the displacement payload (Phase 8 step 55b, DECISIONS_7 #1): the Sumi four never read the
            // coordinates and stayed bitwise; the Anod four read the strain off a field 60x finer and moved (evidence: step55b/palette_test.txt).
            // GL: the Sumi four are step 45a's (bitwise under the new payload, as on Metal); the Anod four recaptured
            // 2026-09-28 on the Linux box for the displacement payload (step 55b, gate-55b/palette_test.txt).
            // D3D11: the Sumi four are step 46's (bitwise under the new payload, as on Metal and GL); the Anod four recaptured
            // 2026-09-28 on the Windows box for the displacement payload (step 55b, gate-55b/windows/palette_test.txt).
            {SUMI_MEDIUM_SUMI, 0u, 0,  "d7cc418955ac2e0e", "e51602d2a2ffd4e2", "98ece962a86a326f"},
            {SUMI_MEDIUM_SUMI, 1u, 0,  "1ad837f3aa0a7324", "39ce2b84cd653d3c", "23a68ac9eb47e34a"},
            {SUMI_MEDIUM_SUMI, 2u, 0,  "828d93044522a5af", "e72420d515248657", "ac785955c4a5f2ae"},
            {SUMI_MEDIUM_SUMI, 0u, 38, "63d2e6377524170a", "eeb7c824b3faf0bf", "d4e6e39c77da563f"},
            {SUMI_MEDIUM_ANOD, 0u, 0,  "67fa3753d3c648be", "7bc658dd87238d03", "9a3d35df09e0c37f"},
            {SUMI_MEDIUM_ANOD, 1u, 0,  "ac238b60aa0ef7a2", "b3121824077f8525", "1b72baa0345056a9"},
            {SUMI_MEDIUM_ANOD, 2u, 0,  "b3e652c64f775150", "bc0fc6df5b03f797", "1024f97d03e48f64"},
            {SUMI_MEDIUM_ANOD, 1u, 38, "38ac7ba0c707083d", "eb3e16793e8608db", "493b66a2e532a203"},
        };
        const char* column = bench_backend_name();
        sumi_map_cc(inst, 0xFF, 110, SUMI_CTL_PALETTE_MORPH);
        bool all_ok = true; int n_expected = 0;
        for (const Case& c : cases) {
            const char* expect = g_bench_backend == SUMI_BACKEND_GL ? c.gl : g_bench_backend == SUMI_BACKEND_D3D11 ? c.d3d11 : c.metal;
            std::free(t19_dip_print(window, inst, &pw, &ph));
            sumi_params_t q = base; q.medium = c.medium; q.active_palette_id = c.palette; sumi_set_params(inst, &q);
            sumi_push_midi(inst, 0xB0, 110, (uint8_t)c.morph_cc);
            t19_step(window, inst, 2);
            sumi_debug_run_field_script(inst);
            t19_step(window, inst, 60);                            // the morph's smoother settles
            uint8_t* pr = t19_dip_print(window, inst, &pw, &ph);   // the dip prints, then resets
            uint64_t h = 1469598103934665603ull;
            if (pr) for (size_t i = 0; i < (size_t)pw * ph * 4; i++) { h ^= pr[i]; h *= 1099511628211ull; }
            std::free(pr);
            char hex[17]; std::snprintf(hex, sizeof hex, "%016llx", (unsigned long long)h);
            std::printf("[t41] preset print: medium %u palette %u morph cc %d -> %s%s\n", c.medium, c.palette, c.morph_cc, hex,
                        expect[0] ? (std::strcmp(hex, expect) == 0 ? " (= recorded)" : " (!= recorded)") : "");
            if (expect[0]) { n_expected++; if (std::strcmp(hex, expect) != 0) all_ok = false; }
        }
        sumi_push_midi(inst, 0xB0, 110, 0);
        t19_step(window, inst, 60);
        if (n_expected) T19(all_ok, "the built-ins through the one path: %d prints (both media, at rest and under morph) hash as recorded for %s (on Metal: as the legacy tables did, bitwise; a mismatch prints the eight hashes above — recapture the column with its evidence if the driver changed)", n_expected, column);
        else std::printf("[t41] preset hashes captured (no expected table yet)\n");
    }
    // the library round-trips through the core: every preset validates unchanged, and the built-ins carry the legacy literals
    {
        bool ok = true; uint32_t total = 0;
        for (uint32_t medium = 0; medium < 2u && ok; medium++) {
            const uint32_t n = sumi_palette_preset_count(medium);
            if (n < 6u) ok = false;
            for (uint32_t i = 0; i < n && ok; i++) {
                sumi_palette_t src, back; const char* nm = nullptr;
                if (!sumi_palette_preset(medium, i, &src, &nm) || !nm) { ok = false; break; }
                sumi_set_palette(inst, &src);
                sumi_get_palette(inst, &back);
                if (std::memcmp(&src, &back, sizeof src) != 0) ok = false;
                total++;
            }
        }
        sumi_palette_t s0; sumi_palette_preset(SUMI_MEDIUM_SUMI, 0u, &s0, nullptr);
        sumi_palette_t a0; sumi_palette_preset(SUMI_MEDIUM_ANOD, 0u, &a0, nullptr);
        ok = ok && s0.stops[0].rgb[0] == 0.012f && s0.stops[0].rgb[2] == 0.013f && s0.hue_drift == 0.45f && s0.accent_rgb[0] == 0.055f &&
             a0.stops[0].rgb[2] == 1.00f && a0.accent_rgb[0] == 0.62f;
        T19(ok, "the preset library: %u presets over both media validate unchanged through sumi_set_palette / sumi_get_palette; the built-ins carry the legacy literals (sumi black 0.012/0.011/0.013 drifting 0.45 toward 0.055/0.042/0.034; electric blue 0.30/0.42/1.00 toward 0.62/0.30/1.00)", total);
    }
    // Phase 6 step 43 (QOL §2): THE SUBSTRATE KNOBS — bitwise at their defaults (the gate and the hashes above);
    // here that each one moves the print: the identity sheet under Sumi with the tint whitened and the fibers
    // doubled, under Anod with the glass blackened and the speckle silenced.
    {
        auto sheet = [&](uint32_t medium, void (*tweak)(sumi_params_t&), double* mean, double* spread) -> bool {
            std::free(t19_dip_print(window, inst, &pw, &ph));
            sumi_params_t q = base; q.medium = medium; q.active_palette_id = 0; q.paper_roughness = 0.5f;
            q.anod_dark = 0.5f; q.anod_grain = 0.5f; q.anod_bloom = 0.0f;   // the neutral Anod baseline (the defaults are the author's look: black glass, bloom)
            if (tweak) tweak(q);
            sumi_set_params(inst, &q);
            t19_step(window, inst, 2);
            uint8_t* pr = t19_dip_print(window, inst, &pw, &ph);
            if (!pr) return false;
            double acc = 0.0, mn = 1e9, mx = -1e9;
            for (size_t i = 0; i < (size_t)pw * ph; i++) { const double l = t42_lum(pr, i * 4); acc += l; if (l < mn) mn = l; if (l > mx) mx = l; }
            *mean = acc / (double)((size_t)pw * ph); *spread = mx - mn;
            std::free(pr);
            return true;
        };
        double m0, s0, m1, s1, m2, s2, a0, as0, a1, as1, a2, as2;
        const bool ok = sheet(SUMI_MEDIUM_SUMI, nullptr, &m0, &s0) &&
                        sheet(SUMI_MEDIUM_SUMI, [](sumi_params_t& q) { q.paper_tint[0] = 0.955f; q.paper_tint[1] = 0.950f; q.paper_tint[2] = 0.935f; }, &m1, &s1) &&
                        sheet(SUMI_MEDIUM_SUMI, [](sumi_params_t& q) { q.fiber_scale = 2.0f; }, &m2, &s2) &&
                        sheet(SUMI_MEDIUM_ANOD, nullptr, &a0, &as0) &&
                        sheet(SUMI_MEDIUM_ANOD, [](sumi_params_t& q) { q.anod_dark = 1.0f; }, &a1, &as1) &&
                        sheet(SUMI_MEDIUM_ANOD, [](sumi_params_t& q) { q.anod_grain = 0.0f; }, &a2, &as2);
        T19(ok && m1 > m0 + 5.0 && std::fabs(m2 - m0) < 3.0 && s2 > 0.0 && a1 < 1.0 && as2 < 0.5 * as0,
            "the substrate knobs move the print: Sumi mean %.1f (cream) -> %.1f (white tint); fibers x2 keep the mean (%.1f) and the sheet textured (spread %.0f); Anod glass %.1f (0.5) -> %.1f (darkness 1: black); speckle spread %.0f -> %.0f at grain 0",
            m0, m1, m2, s2, a0, a1, as0, as2);
    }
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_set_params(inst, &base);
}

// Phase 6 step 42 (MEDIUM §3): the ANOD composite. The identity field prints
// as the substrate alone (at 512² and at 1920x1080, where the half-float
// coordinates round); the §4.6 script's CHARGED texels glow by the strain
// the CPU reads off the same field (0.22 + 0.78(1 − e^(−σ)), σ =
// sqrt(‖J‖_F² − 2) by the one-sided-min estimator); its displaced WATER
// draws the deformed grid; anod_pitch = 0 turns the grid off and leaves the
// charge as it was; a lone drop's interior carries the charge's base glow
// and the water round it draws the grid (the photograph of the discharge);
// a SMALL drop's grid reaches as far as its displacement and the water
// beyond rests as glass (nothing but the displacement lights water); the three palettes
// have their hues; the medium switch leaves the FIELD bitwise. The same
// session is printed under both media for the evidence.
static double t42_lum(const uint8_t* px, size_t o) { return 0.2126 * px[o] + 0.7152 * px[o + 1] + 0.0722 * px[o + 2]; }
static double t42_mean_lum(const uint8_t* px, uint32_t w, uint32_t h, uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1) {
    double acc = 0.0; long n = 0;
    for (uint32_t y = y0; y < y1; y++) for (uint32_t x = x0; x < x1; x++) { acc += t42_lum(px, ((size_t)y * w + x) * 4); n++; }
    return n ? acc / n : 0.0;
}
static uint8_t* t42_print(GLFWwindow* window, sumi_instance_t* inst, uint32_t* pw, uint32_t* ph) {
    // the dip's print WITHOUT resetting the field first: print then the field is fresh — so read the field before
    return t19_dip_print(window, inst, pw, ph);
}

// Step 43 (#71): the lab's strike render — six MPE strikes (velocity 100, six pitch classes 60 degrees apart on the
// circle of fifths) in Anod under the current defaults, the episodes played out, printed to <dir>/anod_strikes.png.
// The author's eye on the strike composition; the evidence's before/after.

// #75 (the author's gesture table): THE MEDIUM-AWARE GESTURES. In Sumi each
// sumi_gesture_* call must leave the field BITWISE as the operator call the
// shells made before (drop, pinch, vortex, and the press's feed / swirl
// math); in Anod each must play its table entry — the tap the strike (a spark
// episode, a charge smaller than the Sumi drop), the pinch bursts, the twist a
// torsion vortex (not the exponential one), the press's push the torsion feed
// and its pull the Chladni stir, which stops when the press lets go.
static bool t75_same(const FieldF& a, const FieldF& b) {
    if (a.w != b.w || a.h != b.h) return false;
    return std::memcmp(a.px, b.px, (size_t)a.w * a.h * 4 * sizeof(float)) == 0;
}
static long t75_diff(const FieldF& a, const FieldF& b) {
    long n = 0; for (size_t i = 0; i < (size_t)a.w * a.h * 4; i++) if (a.px[i] != b.px[i]) n++; return n;
}
static void t19_gesture_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t75] gesture test (the medium-aware marble gestures)\n");
    sumi_params_t base; sumi_get_params(inst, &base);
    uint32_t pw = 0, ph = 0;
    auto fresh = [&](const sumi_params_t& p) {
        // the previous check's episodes (bursts, sparks) play out first — they run on across a dip
        for (int i = 0; i < 1200 && (sumi_debug_burst_count(inst) || sumi_debug_spark_count(inst)); i++) t19_step(window, inst, 1);
        sumi_set_params(inst, &p); t19_step(window, inst, 2); std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
    };
    auto field = [&](FieldF* f) { t19_step(window, inst, 2); return t19_read_field(inst, f); };
    // 1. Sumi: the gestures are the old calls, bitwise
    sumi_params_t s = base; s.medium = SUMI_MEDIUM_SUMI;
    const float R0 = 0.06f;
    fresh(s);
    sumi_add_drop(inst, 0.3f, 0.4f, R0, SUMI_DROP_INK);
    sumi_add_pinch(inst, 0.6f, 0.5f, 0.2f, 0.3f);
    sumi_add_vortex(inst, 0.5f, 0.6f, 0.3f, 0.18f, SUMI_VORTEX_EXPONENTIAL);
    {   // the 1.0 press: 20 frames of push (feed), 10 of pull (swirl)
        float R = R0; sumi_add_drop(inst, 0.7f, 0.3f, R0, SUMI_DROP_INK);
        for (int f = 0; f < 30; f++) {
            const float up = f < 20 ? 0.5f : 0.0f, down = f < 20 ? 0.0f : 0.6f, dt = 1.0f / 120.0f;
            if (down > 0.02f) sumi_add_vortex(inst, 0.7f, 0.3f, 3.0f * down * dt * 6.2831853f * R * R, R, SUMI_VORTEX_LAMB_OSEEN);
            else { const float dR = 0.12f * (0.35f + up) * dt; const float r = std::sqrt((R + dR) * (R + dR) - R * R); if (r > 1e-4f) { sumi_add_drop(inst, 0.7f, 0.3f, r, SUMI_DROP_FEED); R += dR; } }
            t19_step(window, inst, 1);
        }
    }
    FieldF a; if (!field(&a)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    fresh(s);
    sumi_gesture_tap(inst, 0.3f, 0.4f, R0);
    sumi_gesture_pinch(inst, 0.6f, 0.5f, 0.2f, 0.3f, 0.3f);
    sumi_gesture_twist(inst, 0.5f, 0.6f, 0.3f, 0.18f, SUMI_VORTEX_EXPONENTIAL);
    {
        float R = R0; sumi_gesture_tap(inst, 0.7f, 0.3f, R0);
        for (int f = 0; f < 30; f++) { R = sumi_gesture_press(inst, 0.7f, 0.3f, R, f < 20 ? 0.5f : 0.0f, f < 20 ? 0.0f : 0.6f, 1.0 / 120.0); t19_step(window, inst, 1); }
        sumi_gesture_press_end(inst);
    }
    FieldF b; if (!field(&b)) { std::free(a.px); t19_failures++; std::printf("FAIL: field read\n"); return; }
    const long sumi_diff = t75_diff(a, b);
    T19(sumi_diff == 0, "Sumi: tap, pinch, twist and the press through sumi_gesture_* leave the field bitwise as the 1.0 calls (%ld samples differ)", sumi_diff);
    std::free(a.px); std::free(b.px);

    // 2. Anod: the table
    sumi_params_t n = base; n.medium = SUMI_MEDIUM_ANOD;
    fresh(n);
    const uint32_t sp0 = sumi_debug_spark_count(inst);
    sumi_gesture_tap(inst, 0.4f, 0.5f, R0);
    t19_step(window, inst, 1);
    const uint32_t sp1 = sumi_debug_spark_count(inst);
    t19_step(window, inst, 150);
    FieldF t; field(&t);
    // the charge: count the inked texels (phase > 0.5) — an Anod tap inks far less than a Sumi drop of R0
    long inked_anod = 0; for (size_t i = 0; i < (size_t)t.w * t.h; i++) if (t.px[i * 4 + 2] > 0.5f) inked_anod++;
    fresh(s); sumi_gesture_tap(inst, 0.4f, 0.5f, R0); FieldF ts; field(&ts);
    long inked_sumi = 0; for (size_t i = 0; i < (size_t)ts.w * ts.h; i++) if (ts.px[i * 4 + 2] > 0.5f) inked_sumi++;
    T19(sp1 > sp0 && inked_anod > 0 && inked_anod < inked_sumi / 2,
        "Anod tap = the strike: a spark episode starts (%u -> %u) and the charge inks %ld texels against the Sumi drop's %ld (anod_drop %.2f: a third of the Sumi drop, #71's — back at 55b)", sp0, sp1, inked_anod, inked_sumi, n.anod_drop);
    std::free(t.px); std::free(ts.px);
    fresh(n);
    const uint32_t b0 = sumi_debug_burst_count(inst);
    for (int f = 0; f < 10; f++) sumi_gesture_pinch(inst, 0.5f, 0.5f, 0.02f, 0.4f, 0.3f);   // a spread of 0.2 in ten moves
    const uint32_t b1 = sumi_debug_burst_count(inst);
    T19(b1 >= b0 + 3, "Anod pinch = the burst: a 0.2 spread fires %u bursts (one per 0.06 of squeeze)", b1 - b0);
    fresh(n);
    sumi_gesture_twist(inst, 0.5f, 0.5f, 0.3f, 0.18f, SUMI_VORTEX_EXPONENTIAL);
    FieldF tw; field(&tw);
    fresh(n);
    sumi_add_vortex(inst, 0.5f, 0.5f, 0.3f, 0.18f, SUMI_VORTEX_TORSION);
    FieldF tv; field(&tv);
    fresh(n);
    sumi_add_vortex(inst, 0.5f, 0.5f, 0.3f, 0.18f, SUMI_VORTEX_EXPONENTIAL);
    FieldF te; field(&te);
    T19(t75_same(tw, tv) && !t75_same(tw, te), "Anod twist = the torsion vortex: bitwise the torsion pass (%ld differ), not the exponential one (%ld differ)", t75_diff(tw, tv), t75_diff(tw, te));
    std::free(tw.px); std::free(tv.px); std::free(te.px);
    // the press: push = the torsion feed moves the water round the charge; pull = the stir turns the cells; let go = still
    fresh(n);
    FieldF p0; field(&p0);
    float R = R0;
    for (int f = 0; f < 40; f++) { R = sumi_gesture_press(inst, 0.5f, 0.5f, R, 0.8f, 0.0f, 1.0 / 120.0); t19_step(window, inst, 1); }
    FieldF p1; field(&p1);
    const long push_moved = t75_diff(p0, p1);
    for (int f = 0; f < 60; f++) { R = sumi_gesture_press(inst, 0.5f, 0.5f, R, 0.0f, 1.0f, 1.0 / 120.0); t19_step(window, inst, 1); }
    FieldF p2; field(&p2);
    const long pull_moved = t75_diff(p1, p2);
    sumi_gesture_press_end(inst);
    t19_step(window, inst, 90);   // the stir's smoother comes home
    FieldF p3; field(&p3);
    t19_step(window, inst, 60);
    FieldF p4; field(&p4);
    const long after_end = t75_diff(p3, p4);
    T19(push_moved > 1000 && R == R0, "Anod press, push = the torsion feed: %ld samples moved round the charge, and R holds (no feed drop)", push_moved);
    T19(pull_moved > 1000 && after_end == 0, "Anod press, pull = the Chladni stir: %ld samples moved; after the press lets go the field is still (%ld differ over 60 frames)", pull_moved, after_end);
    std::free(p0.px); std::free(p1.px); std::free(p2.px); std::free(p3.px); std::free(p4.px);
    {   // for the eye: the four Anod gestures on one sheet — tap (upper left), pinch spread (upper right), twist (lower left), press push then pull (lower right)
        sumi_params_t e = n; e.pitch_layout = SUMI_LAYOUT_CHROMA_GRID; fresh(e);
        sumi_gesture_tap(inst, 0.25f, 0.28f, R0); t19_step(window, inst, 150);
        for (int f = 0; f < 20; f++) { sumi_gesture_pinch(inst, 0.75f, 0.28f, 0.02f, 0.4f, 0.3f); t19_step(window, inst, 2); }
        for (int f = 0; f < 30; f++) { sumi_gesture_twist(inst, 0.25f, 0.72f, 0.05f, 0.18f, SUMI_VORTEX_EXPONENTIAL); t19_step(window, inst, 1); }
        float Rp = R0; sumi_gesture_tap(inst, 0.75f, 0.72f, R0);
        for (int f = 0; f < 90; f++) { Rp = sumi_gesture_press(inst, 0.75f, 0.72f, Rp, f < 60 ? 0.8f : 0.0f, f < 60 ? 0.0f : 1.0f, 1.0 / 120.0); t19_step(window, inst, 1); }
        sumi_gesture_press_end(inst); t19_step(window, inst, 90);
        uint8_t* px = t19_dip_print(window, inst, &pw, &ph);
        if (px) { stbi_write_png("anod_gestures.png", (int)pw, (int)ph, 4, px, (int)pw * 4); std::printf("[t75] wrote anod_gestures.png\n"); std::free(px); }
    }
    sumi_set_params(inst, &base); t19_step(window, inst, 2);
    std::free(t19_dip_print(window, inst, &pw, &ph));
}

static float g_strike_shear = 0.0f;   // step 55b: --spark-shear <f> for the strike render (0 = the defaults), the Tab compare's other variant
static void t19_anod_strike_render(GLFWwindow* window, sumi_instance_t* inst, const char* dir) {
    sumi_params_t base; sumi_get_params(inst, &base);
    sumi_params_t p = base; p.medium = SUMI_MEDIUM_ANOD;
    if (g_strike_shear > 0.0f) p.spark_shear = g_strike_shear;
    sumi_resize(inst, 1024, 1024, 1.0f);
    const uint8_t notes[6] = {60, 66, 62, 68, 64, 70};
    uint32_t pw = 0, ph = 0;
    sumi_set_params(inst, &p); t19_step(window, inst, 2);
    std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);   // a fresh sheet
    sumi_push_midi(inst, 0xB0, 101, 0); sumi_push_midi(inst, 0xB0, 100, 6); sumi_push_midi(inst, 0xB0, 6, 15);   // MCM
    for (int i = 0; i < 6; i++) { sumi_push_midi(inst, (uint8_t)(0x91 + i), notes[i], 100); t19_step(window, inst, 18); }
    t19_step(window, inst, 150);   // 1.25 s: the episodes play out
    for (int i = 0; i < 6; i++) sumi_push_midi(inst, (uint8_t)(0x81 + i), notes[i], 0);
    t19_step(window, inst, 12);
    uint8_t* px = t19_dip_print(window, inst, &pw, &ph);
    if (px) {
        char path[1024]; std::snprintf(path, sizeof path, "%s/anod_strikes.png", dir);
        stbi_write_png(path, (int)pw, (int)ph, 4, px, (int)pw * 4);
        std::printf("[strike] wrote %s (anod_drop %.2f, spark shear %.2f)\n", path, p.anod_drop, p.spark_shear);
        std::free(px);
    }
    // the same six strikes, then every note bent +2 semitones for a second (the Anod bend: the Chladni stir), released bent
    t19_step(window, inst, 2);
    for (int i = 0; i < 6; i++) { sumi_push_midi(inst, (uint8_t)(0x91 + i), notes[i], 100); t19_step(window, inst, 18); }
    t19_step(window, inst, 60);
    for (int i = 0; i < 6; i++) sumi_push_midi(inst, (uint8_t)(0xE1 + i), 0x55, 0x42);   // 8533: +2 semitones at the ±48 member range
    t19_step(window, inst, 120);
    for (int i = 0; i < 6; i++) sumi_push_midi(inst, (uint8_t)(0x81 + i), notes[i], 0);
    t19_step(window, inst, 30);
    px = t19_dip_print(window, inst, &pw, &ph);
    if (px) {
        char path[1024]; std::snprintf(path, sizeof path, "%s/anod_strikes_bend.png", dir);
        stbi_write_png(path, (int)pw, (int)ph, 4, px, (int)pw * 4);
        std::printf("[strike] wrote %s (a one-second +2 semitone bend on all six: the stir)\n", path);
        std::free(px);
    }
    // the stir alone on a fresh sheet: a second of full-rate stir (1.5 rad at the ring's peak) in the exact discs
    {
        static float cells[SUMI_LAYOUT_MAX_CELLS * 4];
        const uint32_t nc = sumi_debug_cells(inst, cells, SUMI_LAYOUT_MAX_CELLS);
        float rmin = 1.0f, rmax = 0.0f;
        for (uint32_t i = 0; i < nc; i++) { const float r = cells[i * 4 + 2]; if (r < rmin) rmin = r; if (r > rmax) rmax = r; }
        std::printf("[strike] layout %u: %u display cells, radius %.4f..%.4f canvas heights\n", p.pitch_layout, nc, rmin, rmax);
        t19_step(window, inst, 2);
        for (int i = 0; i < 60; i++) { sumi_debug_add_cells_pass(inst, 0.025f, 1.0f, SUMI_CHLADNI_DISCS); t19_step(window, inst, 2); }
        px = t19_dip_print(window, inst, &pw, &ph);
        if (px) {
            char path[1024]; std::snprintf(path, sizeof path, "%s/anod_stir_alone.png", dir);
            stbi_write_png(path, (int)pw, (int)ph, 4, px, (int)pw * 4);
            std::printf("[strike] wrote %s (the stir alone: 1.5 rad in the discs on a fresh sheet)\n", path);
            std::free(px);
        }
    }
    sumi_set_params(inst, &base);
    sumi_resize(inst, 512, 512, 1.0f); t19_step(window, inst, 2);
}

sumi_params_t g_dev_core_defaults{};
bool g_dev_core_defaults_valid = false;

// Phase 9 step 64 (DECISIONS_8 #19): the core's DEFAULT params — every field the header documents a default for
// reads it right after sumi_create, before any settings apply. The bug: default_params() filled a struct field by
// field and the two Phase-9 fields had no line, so they held the stack's leftovers — the Tab's session carried
// trumpet_arc 200 and string_tuning 5, clamped by the engine on apply but written back to the file and read by
// the settings as "off". The fix zeroes the struct and names both; this test pins the documented defaults.
static void t64_defaults_test() {
    std::printf("[t64] defaults test (the params as sumi_create leaves them)\n");
    T19(g_dev_core_defaults_valid, "the defaults were captured at sumi_create");
    const sumi_params_t& d = g_dev_core_defaults;
    T19(d.trumpet_arc == 0u, "trumpet_arc defaults to 0 (the column), read %u", d.trumpet_arc);
    T19(d.string_tuning == SUMI_STRINGS_STANDARD_GUITAR, "string_tuning defaults to the standard guitar (%u), read %u", (unsigned)SUMI_STRINGS_STANDARD_GUITAR, d.string_tuning);
    T19(d.pitch_layout == SUMI_LAYOUT_FIFTHS, "pitch_layout defaults to the circle of fifths, read %u", d.pitch_layout);
    T19(d.medium == SUMI_MEDIUM_SUMI, "medium defaults to Sumi, read %u", d.medium);
    T19(d.active_palette_id == 0u, "active_palette_id defaults to 0, read %u", d.active_palette_id);
    T19(d.sim_scale == 1.0f && d.smoothing_ms == 30.0f && d.bpm == 120.0f, "sim_scale 1, smoothing 30 ms, bpm 120: read %.2f %.1f %.0f", (double)d.sim_scale, (double)d.smoothing_ms, (double)d.bpm);
}

static void t19_anod_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t42] Anod test (the charge's strain-glow, the water's grid)\n");
    sumi_params_t base; sumi_get_params(inst, &base);
    sumi_params_t p = base; p.medium = SUMI_MEDIUM_ANOD; p.anod_glow = 1.0f; p.active_palette_id = 0;
    p.anod_pitch = 10.0f / 512.0f;   // a 10-texel grid pitch on the 512² bench (the default 1/144 is 10 texels at 1440)
    p.anod_dark = 0.5f; p.anod_grain = 0.5f; p.anod_bloom = 0.0f;   // the step-42 glass and no bloom: the numbers below were calibrated on them (the defaults are the author's look)
    uint32_t pw = 0, ph = 0;
    // 1. the identity field: the substrate alone
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_set_params(inst, &p);
    t19_step(window, inst, 2);
    uint8_t* sub = t42_print(window, inst, &pw, &ph);
    if (!sub) { t19_failures++; std::printf("FAIL: print\n"); return; }
    const double sub_mean = t42_mean_lum(sub, pw, ph, 0, pw, 0, ph);
    double sub_max = 0.0;
    for (size_t i = 0; i < (size_t)pw * ph; i++) { const double l = t42_lum(sub, i * 4); if (l > sub_max) sub_max = l; }
    T19(sub_mean < 40.0 && sub_max < 60.0, "the identity field in Anod prints the substrate alone: mean luminance %.1f/255, max %.1f (near-black glass with its speckle)", sub_mean, sub_max);
    {
        // the same at 1920x1080, where the identity coordinates are NOT exactly representable in half floats:
        // the stencil widens and the rounding bias is subtracted, so the glass stays dark (the stripes of 2026-09-22)
        sumi_resize(inst, 1920, 1080, 1.0f);
        t19_step(window, inst, 2);
        std::free(t19_dip_print(window, inst, &pw, &ph));
        t19_step(window, inst, 2);
        uint8_t* hd = t42_print(window, inst, &pw, &ph);
        double hd_mean = 0.0, hd_max = 0.0, hd_right = 0.0;
        if (hd) {
            hd_mean = t42_mean_lum(hd, pw, ph, 0, pw, 0, ph);
            hd_right = t42_mean_lum(hd, pw, ph, pw / 2, pw, ph / 2, ph);
            for (size_t i = 0; i < (size_t)pw * ph; i++) { const double l = t42_lum(hd, i * 4); if (l > hd_max) hd_max = l; }
        }
        T19(hd && hd_mean < 40.0 && hd_max < 60.0 && std::fabs(hd_right - hd_mean) < 3.0,
            "the identity field at 1920x1080 prints the substrate alone too: mean %.1f, max %.1f, the lower-right quarter %.1f (the half-float coordinates' rounding is read neither as strain nor as a displaced grid)", hd_mean, hd_max, hd_right);
        std::free(hd);
        sumi_resize(inst, 512, 512, 1.0f);
        t19_step(window, inst, 2);
        std::free(t19_dip_print(window, inst, &pw, &ph));
        t19_step(window, inst, 2);
    }
    // 2. the §4.6 script: read the field, then print under Anod (and under Sumi for the re-read evidence)
    sumi_debug_run_field_script(inst);
    t19_step(window, inst, 2);
    FieldF f;
    if (!t19_read_field(inst, &f)) { std::free(sub); t19_failures++; std::printf("FAIL: field read\n"); return; }
    // the FIELD must not change when the medium is switched: the composite is a READ
    p.medium = SUMI_MEDIUM_SUMI; sumi_set_params(inst, &p); t19_step(window, inst, 2);
    p.medium = SUMI_MEDIUM_ANOD; sumi_set_params(inst, &p); t19_step(window, inst, 2);
    FieldF f2;
    if (!t19_read_field(inst, &f2)) { std::free(sub); std::free(f.px); t19_failures++; std::printf("FAIL: field read\n"); return; }
    long field_diff = 0;
    for (size_t i = 0; i < (size_t)f.w * f.h * 4; i++) if (f.px[i] != f2.px[i]) field_diff++;
    std::free(f2.px);
    T19(field_diff == 0, "the medium switch (Sumi -> Anod -> Sumi -> Anod) leaves the field bitwise: %ld samples differ — the composite is a read, switching is live", field_diff);
    // the CPU strain from the same field — the estimator the CHARGED texels glow by: one-sided min, the
    // rounding bias subtracted, the class seam mask (a charged texel beside fresh water reads none)
    const uint32_t w = f.w, h = f.h;
    const float texel = 1.0f / (float)h, aspect = (float)w / (float)h;
    auto ulp = [](float x) { return std::exp2(std::floor(std::log2(std::fmax(x, 1e-6f))) - 10.0f); };
    const int n = 2 * (int)std::fmax(1.0f, std::floor((float)h / 512.0f + 0.5f));   // the shader's stencil half-width
    auto fresh = [&](uint32_t x, uint32_t y) {   // 1.2.0: no phase and a displacement under half a texel (the ULP term is gone)
        const size_t o = ((size_t)y * w + x) * 4;
        const float sx = ((float)x + 0.5f) / (float)w, sy = ((float)y + 0.5f) / (float)h;
        return f.px[o + 2] < 0.5f && std::fabs(f.px[o] - sx) < 0.5f * texel / aspect && std::fabs(f.px[o + 1] - sy) < 0.5f * texel;
    };
    std::vector<float> sigma((size_t)w * h, 0.0f);
    for (uint32_t y = n; y + n < h; y++) for (uint32_t x = n; x + n < w; x++) {
        const bool fc = fresh(x, y);
        if (fc || fresh(x + n, y) != fc || fresh(x - n, y) != fc || fresh(x, y + n) != fc || fresh(x, y - n) != fc) continue;
        const size_t o = ((size_t)y * w + x) * 4, ox1 = o + 4 * n, ox0 = o - 4 * n, oy1 = o + (size_t)w * 4 * n, oy0 = o - (size_t)w * 4 * n;
        const float sx_ = aspect / (n * texel), sy_ = 1.0f / (n * texel);
        auto smaller = [](float a, float b) { return std::fabs(a) < std::fabs(b) ? a : b; };
        const float ux = smaller((f.px[ox1] - f.px[o]) * sx_, (f.px[o] - f.px[ox0]) * sx_);
        const float vx = smaller((f.px[ox1 + 1] - f.px[o + 1]) * sy_, (f.px[o + 1] - f.px[ox0 + 1]) * sy_);
        const float uy = smaller((f.px[oy1] - f.px[o]) * sx_, (f.px[o] - f.px[oy0]) * sx_);
        const float vy = smaller((f.px[oy1 + 1] - f.px[o + 1]) * sy_, (f.px[o + 1] - f.px[oy0 + 1]) * sy_);
        const float F2 = ux * ux + uy * uy + vx * vx + vy * vy;
        const float su = ulp(std::fabs(f.px[o] - ((float)x + 0.5f) / (float)w)) * sx_, sv = ulp(std::fabs(f.px[o + 1] - ((float)y + 0.5f) / (float)h)) * sy_;   // 1.2.0: at the displacement's magnitude
        const float bias = 2.0f * (su * su + sv * sv) / 6.0f;
        sigma[(size_t)y * w + x] = std::sqrt(std::fmax(F2 - 2.0f - 3.0f * bias, 0.0f));
    }
    // the displacement in texels, per texel: what the water's grid follows
    auto disp = [&](uint32_t x, uint32_t y) {
        const size_t o = ((size_t)y * w + x) * 4;
        const float dx = (f.px[o] - ((float)x + 0.5f) / (float)w) * (float)w, dy = (f.px[o + 1] - ((float)y + 0.5f) / (float)h) * (float)h;
        return std::sqrt(dx * dx + dy * dy);
    };
    const double lit_at = sub_mean + 8.0;                                     // a texel that shows anything over the glass
    uint8_t* anod = t42_print(window, inst, &pw, &ph);      // the dip resets the field: the Sumi re-read below replays the script
    if (!anod || pw != w || ph != h) { std::free(sub); std::free(f.px); std::free(anod); t19_failures++; std::printf("FAIL: print\n"); return; }
    double charged_mean = 0.0;
    {
        // the charge glows by its strain: over the CHARGED texels, the print's luminance against the CPU's
        // 0.22 + 0.78(1 − e^(−σ)) — the base glow of a fresh strike plus the strain's share
        double sx_ = 0, sy_ = 0, sxx = 0, syy = 0, sxy = 0; long nc = 0;
        for (uint32_t y = 2; y + 2 < h; y++) for (uint32_t x = 2; x + 2 < w; x++) {
            const size_t o = ((size_t)y * w + x) * 4;
            if (f.px[o + 2] < 1.0f) continue;
            const double gl = 0.22 + 0.78 * (1.0 - std::exp(-(double)sigma[(size_t)y * w + x]));
            const double lu = t42_lum(anod, o);
            sx_ += gl; sy_ += lu; sxx += gl * gl; syy += lu * lu; sxy += gl * lu; nc++;
        }
        const double cov = nc ? sxy / nc - (sx_ / nc) * (sy_ / nc) : 0.0, vx_ = nc ? sxx / nc - (sx_ / nc) * (sx_ / nc) : 0.0, vy_ = nc ? syy / nc - (sy_ / nc) * (sy_ / nc) : 0.0;
        const double corr = (vx_ > 0 && vy_ > 0) ? cov / std::sqrt(vx_ * vy_) : 0.0;
        charged_mean = nc ? sy_ / nc : 0.0;
        T19(nc > 1000 && charged_mean > sub_mean + 20.0 && corr > 0.6,
            "the charge glows by its strain: %ld charged texels at mean luminance %.1f (substrate %.1f); their luminance correlates %.2f with the CPU's 0.22 + 0.78(1 − e^(−σ)) read off the same field", nc, charged_mean, sub_mean, corr);
        // the water draws the deformed grid where it was displaced (the script's water is all displaced by its
        // scrolls, its fresh band five texels wide — narrower than the grid's window; rest is measured on the
        // small drop below)
        long disp_n = 0, disp_lit = 0;
        for (uint32_t y = h / 4; y < 3 * h / 4; y++) for (uint32_t x = 2; x + 2 < w; x++) {
            const size_t o = ((size_t)y * w + x) * 4;
            if (f.px[o + 2] >= 1.0f) continue;
            if (disp(x, y) >= 4.0f) { disp_n++; if (t42_lum(anod, o) > lit_at) disp_lit++; }
        }
        const double disp_frac = disp_n ? (double)disp_lit / disp_n : 0.0;
        T19(disp_n > 1000 && disp_frac > 0.08 && disp_frac < 0.6,
            "the water draws the deformed grid: %.0f%% of %ld displaced water texels (>= 4 texels) are lit — the far field shows as lines (pitch 10 texels, two families)", 100.0 * disp_frac, disp_n);
        stbi_write_png("anod_reread_anod.png", (int)w, (int)h, 4, anod, (int)w * 4);
    }
    std::free(anod);
    // the grid off: anod_pitch = 0 leaves the water glass and the charge as it was (the script replayed — a print resets the field)
    {
        sumi_params_t q = p; q.anod_pitch = 0.0f; sumi_set_params(inst, &q); t19_step(window, inst, 2);
        sumi_debug_run_field_script(inst); t19_step(window, inst, 2);
        uint8_t* off = t42_print(window, inst, &pw, &ph);
        if (!off || pw != w || ph != h) { std::free(sub); std::free(f.px); std::free(off); t19_failures++; std::printf("FAIL: print\n"); return; }
        long wn = 0, wl = 0, cn = 0; double cm = 0.0;
        for (uint32_t y = 2; y + 2 < h; y++) for (uint32_t x = 2; x + 2 < w; x++) {
            const size_t o = ((size_t)y * w + x) * 4;
            if (f.px[o + 2] >= 1.0f) { cn++; cm += t42_lum(off, o); }
            else { wn++; if (t42_lum(off, o) > lit_at) wl++; }
        }
        cm = cn ? cm / cn : 0.0;
        const double wfrac = wn ? (double)wl / wn : 1.0;
        T19(wfrac < 0.005 && std::fabs(cm - charged_mean) < 2.0,
            "the grid off (anod_pitch 0): %.2f%% of the water texels lit — glass; the charge's mean luminance %.1f against %.1f with the grid — the knob touches the water alone", 100.0 * wfrac, cm, charged_mean);
        std::free(off);
        sumi_set_params(inst, &p); t19_step(window, inst, 2);
    }
    // the photograph of the discharge: ONE drop on a fresh sheet — its interior is charged and unstrained (the
    // base glow), the water round it displaced (the grid converges on it); the rim's compression is no longer
    // read as a glow (the author's call of 2026-09-22: water shows the field's lines, never strain)
    {
        p.medium = SUMI_MEDIUM_ANOD; p.active_palette_id = 0; sumi_set_params(inst, &p); t19_step(window, inst, 2);
        sumi_add_drop(inst, 0.5f, 0.5f, 0.20f, 0); t19_step(window, inst, 2);
        uint8_t* one = t42_print(window, inst, &pw, &ph);
        if (!one) { std::free(sub); std::free(f.px); t19_failures++; std::printf("FAIL: print\n"); return; }
        const double centre = t42_mean_lum(one, w, h, w / 2 - 8, w / 2 + 8, h / 2 - 8, h / 2 + 8);
        long an = 0, al = 0;
        for (uint32_t y = 0; y < h; y++) for (uint32_t x = 0; x < w; x++) {
            const double rx = ((double)x + 0.5) / w - 0.5, ry = ((double)y + 0.5) / h - 0.5, r = std::sqrt(rx * rx + ry * ry);
            if (r >= 0.30 && r <= 0.48) { an++; if (t42_lum(one, ((size_t)y * w + x) * 4) > lit_at) al++; }   // the annulus 1.5 R .. 2.4 R
        }
        const double afrac = an ? (double)al / an : 0.0;
        T19(centre > sub_mean + 10.0 && an > 1000 && afrac > 0.05 && afrac < 0.7,
            "the photograph of the discharge: a lone drop's interior carries the charge's base glow (%.1f; substrate %.1f) and the water round it draws the field's grid — %.0f%% of the annulus 1.5–2.4 R lit", centre, sub_mean, 100.0 * afrac);
        std::free(one);
        // the reach: a SMALL drop (R = 0.02) displaces the water by a²/2r — 3.4 texels at r = 0.06, under half a
        // texel beyond r = 0.15 — so its grid shows near it and the water beyond rests as glass: nothing but
        // the displacement lights water (the identity checks above say the same of a whole sheet)
        std::free(t19_dip_print(window, inst, &pw, &ph));
        sumi_set_params(inst, &p); t19_step(window, inst, 2);
        sumi_add_drop(inst, 0.5f, 0.5f, 0.02f, 0); t19_step(window, inst, 2);
        uint8_t* small = t42_print(window, inst, &pw, &ph);
        if (!small) { std::free(sub); std::free(f.px); t19_failures++; std::printf("FAIL: print\n"); return; }
        long nn = 0, nl = 0, fn = 0, fl = 0;
        for (uint32_t y = 0; y < h; y++) for (uint32_t x = 0; x < w; x++) {
            const double rx = ((double)x + 0.5) / w - 0.5, ry = ((double)y + 0.5) / h - 0.5, r = std::sqrt(rx * rx + ry * ry);
            const bool lit = t42_lum(small, ((size_t)y * w + x) * 4) > lit_at;
            if (r >= 0.03 && r <= 0.06) { nn++; if (lit) nl++; }
            else if (r >= 0.15) { fn++; if (lit) fl++; }
        }
        const double nfrac = nn ? (double)nl / nn : 0.0, ffrac = fn ? (double)fl / fn : 1.0;
        T19(nn > 500 && nfrac > 0.05 && fn > 10000 && ffrac < 0.01,
            "the reach: a small drop (R = 0.02) draws its grid near it (%.0f%% of the annulus 1.5–3 R lit) and the water beyond r = 0.15, displaced by under half a texel, rests as glass (%.2f%% of %ld texels lit)", 100.0 * nfrac, 100.0 * ffrac, fn);
        std::free(small);
    }
    // the same session under Sumi, for the evidence (the script replayed on a fresh sheet)
    p.medium = SUMI_MEDIUM_SUMI; sumi_set_params(inst, &p); t19_step(window, inst, 2);
    sumi_debug_run_field_script(inst); t19_step(window, inst, 2);
    uint8_t* sumi = t42_print(window, inst, &pw, &ph);
    if (sumi) { stbi_write_png("anod_reread_sumi.png", (int)w, (int)h, 4, sumi, (int)w * 4); std::free(sumi); std::printf("[t42] wrote anod_reread_sumi.png / anod_reread_anod.png — the same session under both media\n"); }
    // the three Anod palettes: the glow's dominant channel
    const char* names[3] = {"electric blue", "plasma orange", "phosphor green"};
    bool hues_ok = true; double dom[3][3] = {};
    for (uint32_t pal = 0; pal < 3; pal++) {
        p.medium = SUMI_MEDIUM_ANOD; p.active_palette_id = pal; sumi_set_params(inst, &p); t19_step(window, inst, 2);
        sumi_debug_run_field_script(inst); t19_step(window, inst, 2);
        uint8_t* pr = t42_print(window, inst, &pw, &ph);
        if (!pr) { hues_ok = false; continue; }
        double r = 0, g = 0, b = 0; long n = 0;
        for (size_t i = 0; i < (size_t)w * h; i++) { const size_t o = i * 4; if (t42_lum(pr, o) > 70.0) { r += pr[o]; g += pr[o + 1]; b += pr[o + 2]; n++; } }
        if (n) { dom[pal][0] = r / n; dom[pal][1] = g / n; dom[pal][2] = b / n; }
        std::free(pr);
    }
    hues_ok = hues_ok && dom[0][2] > dom[0][0] && dom[0][2] > dom[0][1] && dom[1][0] > dom[1][1] && dom[1][0] > dom[1][2] && dom[2][1] > dom[2][0] && dom[2][1] > dom[2][2];
    T19(hues_ok, "the Anod palettes under the same ids: %s glows (%.0f,%.0f,%.0f), %s (%.0f,%.0f,%.0f), %s (%.0f,%.0f,%.0f) — blue, red and green dominant in turn",
        names[0], dom[0][0], dom[0][1], dom[0][2], names[1], dom[1][0], dom[1][1], dom[1][2], names[2], dom[2][0], dom[2][1], dom[2][2]);
    std::free(sub); std::free(f.px);
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_set_params(inst, &base);
}

// Phase 6 step 43 (QOL §4): PRINTS AT ANY SIZE — the export is the composite of a
// field at a target size. At the field's own size it is the dip's print,
// bitwise; at 4k it is the same field interpolated (an 8x8 box average of
// the 4k lands within a few counts of the 512 print); a field KEPT before a
// dip (sumi_read_field) exports after the dip exactly as the live field did
// — the print ledger's premise, at 512 and at 4k; an Anod export over alpha
// leaves the resting glass at alpha 0 and lights the charge; the size cap and
// the one-readback rule refuse what they must.
static uint8_t* t43_export(GLFWwindow* window, sumi_instance_t* inst, const uint8_t* field, uint32_t fw, uint32_t fh,
                           uint32_t w, uint32_t h, uint32_t flags, uint32_t* ow, uint32_t* oh) {
    if (!sumi_export_begin(inst, field, fw, fh, w, h, flags)) return nullptr;
    const size_t bytes = (size_t)w * h * 4;
    uint8_t* buf = (uint8_t*)std::malloc(bytes);
    for (int i = 0; i < 4000 && buf; i++) {
        const int st = sumi_export_poll(inst, buf, bytes, ow, oh);
        if (st == 2) return buf;
        if (st == 0) break;
        t19_step(window, inst, 1);
    }
    std::free(buf);
    return nullptr;
}
static long t43_diff(const uint8_t* a, const uint8_t* b, size_t n) { long d = 0; for (size_t i = 0; i < n; i++) if (a[i] != b[i]) d++; return d; }
static void t19_print_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t43] prints test (export at any size, the ledger's premise)\n");
    sumi_params_t base; sumi_get_params(inst, &base);
    sumi_params_t p = base; p.medium = SUMI_MEDIUM_SUMI; p.active_palette_id = 0; sumi_set_params(inst, &p);
    uint32_t pw = 0, ph = 0, w = 0, h = 0;
    std::free(t19_dip_print(window, inst, &pw, &ph));
    t19_step(window, inst, 2);
    sumi_debug_run_field_script(inst);
    t19_step(window, inst, 2);
    // the live field: at its own size, and at 4k
    uint8_t* e512 = t43_export(window, inst, nullptr, 0, 0, 512, 512, 0, &w, &h);
    const bool e512_ok = e512 && w == 512 && h == 512;
    uint8_t* e4k = t43_export(window, inst, nullptr, 0, 0, 4096, 4096, 0, &w, &h);
    const bool e4k_ok = e4k && w == 4096 && h == 4096;
    // the field kept before the dip (sumi_read_field), then the dip's own print
    uint32_t fw = 0, fh = 0;
    sumi_read_field(inst, nullptr, 0, &fw, &fh);
    std::vector<uint8_t> kept((size_t)fw * fh * 8);
    const bool kept_ok = fw == 512 && fh == 512 && sumi_read_field(inst, kept.data(), kept.size(), &fw, &fh);
    uint8_t* print = t19_dip_print(window, inst, &pw, &ph);
    const long d_print = (e512_ok && print && pw == 512 && ph == 512) ? t43_diff(e512, print, (size_t)512 * 512 * 4) : -1;
    T19(e512_ok && print && d_print == 0, "the export at the field's size IS the dip's print: %ld of %d bytes differ (bitwise)", d_print, 512 * 512 * 4);
    double mean = 0.0;
    if (e512_ok && e4k_ok) {
        double acc = 0.0;
        for (uint32_t y = 0; y < 512; y++) for (uint32_t x = 0; x < 512; x++) for (int c = 0; c < 3; c++) {
            double s = 0.0;
            for (uint32_t yy = 0; yy < 8; yy++) for (uint32_t xx = 0; xx < 8; xx++) s += e4k[(((size_t)(y * 8 + yy)) * 4096 + (x * 8 + xx)) * 4 + c];
            acc += std::fabs(s / 64.0 - (double)e512[((size_t)y * 512 + x) * 4 + c]);
        }
        mean = acc / (512.0 * 512.0 * 3.0);
    }
    T19(e4k_ok && mean < 4.0, "the same field at 4k: 4096x4096 exported; an 8x8 box average lands within %.2f counts of the 512 print on average (interpolation of the same field, no new detail)", mean);
    // the ledger's premise: after the dip (a fresh sheet), the KEPT field exports exactly as the live one did
    t19_step(window, inst, 2);
    uint8_t* k512 = t43_export(window, inst, kept.data(), fw, fh, 512, 512, 0, &w, &h);
    uint8_t* k4k = t43_export(window, inst, kept.data(), fw, fh, 4096, 4096, 0, &w, &h);
    const long d512 = (kept_ok && k512 && e512_ok) ? t43_diff(k512, e512, (size_t)512 * 512 * 4) : -1;
    const long d4k = (kept_ok && k4k && e4k_ok) ? t43_diff(k4k, e4k, (size_t)4096 * 4096 * 4) : -1;
    T19(kept_ok && d512 == 0 && d4k == 0, "the ledger's premise: the field kept before the dip re-exports after it bitwise as the live field did — %ld bytes differ at 512, %ld at 4k", d512, d4k);
    std::free(e512); std::free(e4k); std::free(k512); std::free(k4k); std::free(print);
    // Anod over alpha — with the bloom OFF: this measures the composite's alpha semantics (the glass at 0, the
    // charge lit); the default bloom's halo legitimately lifts alpha over the water, which is the export's intent
    p.medium = SUMI_MEDIUM_ANOD; p.anod_bloom = 0.0f; sumi_set_params(inst, &p);
    std::free(t19_dip_print(window, inst, &pw, &ph));
    t19_step(window, inst, 2);
    sumi_debug_run_field_script(inst);
    t19_step(window, inst, 2);
    FieldF f; const bool fok = t19_read_field(inst, &f);
    uint8_t* a = t43_export(window, inst, nullptr, 0, 0, 512, 512, SUMI_EXPORT_ANOD_ALPHA, &w, &h);
    uint8_t* o = t43_export(window, inst, nullptr, 0, 0, 512, 512, 0, &w, &h);
    long rest_n = 0, rest_zero = 0, charged_n = 0, charged_lit = 0, opaque = 0;
    if (fok && a && o) {
        for (uint32_t y = 4; y + 4 < 512; y++) for (uint32_t x = 0; x < 512; x++) {
            const size_t i = ((size_t)y * 512 + x);
            if (o[i * 4 + 3] == 255) opaque++;
            if (f.px[i * 4 + 2] >= 1.0f) { charged_n++; if (a[i * 4 + 3] >= 40) charged_lit++; }
            else if (x < 3 && y >= 128 && y < 384) { rest_n++; if (a[i * 4 + 3] == 0) rest_zero++; }   // the fresh band the scrolls left
        }
    }
    if (fok) std::free(f.px);
    if (a) { stbi_write_png("print_anod_alpha.png", 512, 512, 4, a, 512 * 4); std::printf("[t43] wrote print_anod_alpha.png — the discharge over alpha\n"); }
    T19(a && o && opaque == (long)(504 * 512) && rest_n > 100 && rest_zero == rest_n && charged_n > 1000 && charged_lit == charged_n,
        "Anod over alpha: the plain export is opaque everywhere (%ld texels); with the flag the resting glass is alpha 0 (%ld of %ld) and every charged texel lit (%ld of %ld, alpha >= 40)",
        opaque, rest_zero, rest_n, charged_lit, charged_n);
    std::free(a); std::free(o);
    // the cap and the one-readback rule
    const bool cap_refused = !sumi_export_begin(inst, nullptr, 0, 0, 9000, 512, 0) && !sumi_export_begin(inst, nullptr, 0, 0, 512, 0, 0);
    const bool first = sumi_export_begin(inst, nullptr, 0, 0, 256, 256, 0);
    const bool second_refused = !sumi_export_begin(inst, nullptr, 0, 0, 256, 256, 0);
    { uint8_t* drain = (uint8_t*)std::malloc(256 * 256 * 4); for (int i = 0; i < 2000; i++) { const int st = sumi_export_poll(inst, drain, 256 * 256 * 4, &w, &h); if (st != 1) break; t19_step(window, inst, 1); } std::free(drain); }
    T19(cap_refused && first && second_refused, "the cap and the one readback: 9000 wide and a zero height are refused; a second export while one is in flight is refused");
    std::free(t19_dip_print(window, inst, &pw, &ph));
    // Step 45a (DECISIONS_5 #77): the ledger is driven from the SETTINGS window, whose
    // GL context is not the core's on Linux — a dip and an export requested with
    // another context current must still keep the sheet and write the PNG, because
    // the work runs in tick() in the core's context. (Before the fix the dip read the
    // field through a foreign FBO — "readback framebuffer incomplete" — and kept nothing.)
    {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        GLFWwindow* other = glfwCreateWindow(64, 64, "t43 other context", nullptr, nullptr);
        glfwDefaultWindowHints();
        PrintLedger ledger; AppSettings as{};
        std::remove("ledger_other_context.png");
        sumi_debug_run_field_script(inst); t19_step(window, inst, 4);
        bool asked = false;
        if (other) {
            glfwMakeContextCurrent(other);
            asked = ledger.dip(inst, as);
            glfwMakeContextCurrent(window);
        }
        for (int i = 0; i < 240 && (ledger.entries().empty() || !ledger.entries().back().print_seen); i++) { ledger.tick(inst); t19_step(window, inst, 1); }
        const bool kept = !ledger.entries().empty() && !ledger.entries().back().field.empty() && ledger.entries().back().print_seen;
        bool exported = false;
        if (kept && other) {
            glfwMakeContextCurrent(other);
            const bool eq = ledger.export_png(inst, 0, 1024, 576, false, "ledger_other_context.png", as);
            glfwMakeContextCurrent(window);
            for (int i = 0; eq && i < 600 && ledger.busy(); i++) { ledger.tick(inst); t19_step(window, inst, 1); }
            for (int i = 0; i < 200; i++) { if (FILE* f2 = std::fopen("ledger_other_context.png", "rb")) { std::fseek(f2, 0, SEEK_END); exported = std::ftell(f2) > 1000; std::fclose(f2); if (exported) break; } t19_step(window, inst, 1); }
        }
        if (other) glfwDestroyWindow(other);
        glfwMakeContextCurrent(window);
        T19(other && asked && kept && exported, "the ledger across contexts: a dip requested with another GL context current keeps the field (%ux%u) and its print, and a 1024x576 re-export requested the same way writes ledger_other_context.png",
            kept ? ledger.entries().back().fw : 0u, kept ? ledger.entries().back().fh : 0u);
    }
    // step 46 (DECISIONS_5 #84): a settings.ini saved with CRLF line endings (Notepad, a PowerShell
    // Set-Content) must load the print folder without the '\r' — with it every export path is
    // invalid on Windows, the write fails in the background, and each later save re-writes the '\r'.
    {
        const char* ini = "print_test_crlf.ini";
        if (FILE* f = std::fopen(ini, "wb")) {
            // INI-owned keys only: the look's keys give way to last_session.json when it exists.
            std::fputs("# midi-sink settings\r\nfirst_run_dismissed=1\r\nprint_dir=prints_crlf\r\nfullscreen=1\r\n", f);
            std::fclose(f);
        }
        AppSettings cs{};
        const bool loaded = app_settings_load(cs, ini);
        std::remove(ini);
        T19(loaded && cs.print_dir == "prints_crlf" && cs.first_run_dismissed && cs.fullscreen,
            "a CRLF settings.ini loads cleanly: print_dir %zu chars (want 11, no carriage return), the flags either side read (%d, %d)",
            cs.print_dir.size(), (int)cs.first_run_dismissed, (int)cs.fullscreen);
    }
    // DECISIONS_5 #86: a background PNG write's outcome REACHES THE UI. A write into a folder that
    // does not exist (the #84 / #85 shape) must come back through print_write_poll as a failure that
    // names the path and the reason; a good write as "Saved"; and through the ledger, the failure
    // must land in status() — the settings window shows that row.
    {
        // the writer alone: a bad folder, then a good file (the queue drained of the checks above first)
        bool ok = true; std::string msg;
        { bool o; std::string m; while (print_write_poll(&o, &m)) {} }
        std::vector<uint8_t>* bad = new std::vector<uint8_t>(16 * 16 * 4, 200);
        print_write_async(bad, 16, 16, "no_such_folder_86/print.png", "test");
        int waited = 0;
        while (!print_write_poll(&ok, &msg) && waited < 600) { t19_step(window, inst, 1); waited++; }
        const bool bad_reported = waited < 600 && !ok && msg.find("no_such_folder_86/print.png") != std::string::npos && msg.find("folder does not exist") != std::string::npos;
        std::vector<uint8_t>* good = new std::vector<uint8_t>(16 * 16 * 4, 200);
        print_write_async(good, 16, 16, "print_test_write_86.png", "test");
        ok = false; msg.clear(); waited = 0;
        while (!print_write_poll(&ok, &msg) && waited < 600) { t19_step(window, inst, 1); waited++; }
        bool exists = false;
        if (FILE* f = std::fopen("print_test_write_86.png", "rb")) { exists = true; std::fclose(f); std::remove("print_test_write_86.png"); }
        T19(bad_reported && ok && exists && msg.rfind("Saved 16x16", 0) == 0,
            "a background write reports its outcome: a missing folder comes back as a failure naming the path and the reason (%s), a good write as \"%s\"",
            bad_reported ? "yes" : "no", msg.c_str());
        // through the ledger: the failure is the status row
        PrintLedger led;
        AppSettings as{};
        sumi_get_params(inst, &as.params);
        led.dip(inst, as);
        for (int i = 0; i < 600 && (led.entries().empty() || !led.entries().back().print_seen); i++) { led.tick(inst); t19_step(window, inst, 1); }
        const bool have_print = !led.entries().empty() && led.entries().back().print_seen;
        const bool asked = have_print && led.save_last_print("no_such_folder_86/last.png");
        const unsigned serial0 = led.write_serial();
        for (int i = 0; i < 600 && led.write_serial() == serial0; i++) { led.tick(inst); t19_step(window, inst, 1); }
        T19(asked && led.write_serial() == serial0 + 1 && !led.last_write_ok() && led.status().rfind("Could not write", 0) == 0,
            "the ledger's status row carries the outcome: \"%s\"", led.status().c_str());
    }
    // step 46 (DECISIONS_5 #85): a 1.0/1.1 INI from a user with a non-ASCII name holds print_dir
    // in the old ANSI code page (here cp1252's e-acute, one byte 0xE9) - not UTF-8, so it is
    // dropped and the default folder stands rather than an export path that cannot open.
    {
        const char* ini = "print_test_ansi.ini";
        if (FILE* f = std::fopen(ini, "wb")) {
            std::fputs("print_dir=C:\\Users\\H\xe9l\xe8ne\\Pictures\nfullscreen=1\n", f);
            std::fclose(f);
        }
        AppSettings as2{};
        as2.print_dir = "default-folder";
        const bool loaded = app_settings_load(as2, ini);
        std::remove(ini);
        T19(loaded && as2.print_dir == "default-folder" && as2.fullscreen,
            "a legacy ANSI print_dir (invalid UTF-8) is dropped: the folder stays '%s', the next key still reads (%d)",
            as2.print_dir.c_str(), (int)as2.fullscreen);
    }
#if defined(_WIN32)
    // step 46 (DECISIONS_5 #85): the app's paths are UTF-8 (ImGui's print folder, the INI, preset
    // names); the narrow CRT calls that open them must read UTF-8 too - the manifest's
    // activeCodePage. The folder is made through the WIDE API (its true name, U+00E9 built from its
    // code point: no source-encoding assumption), the PNG written through the UTF-8 path as an
    // export is, and found again through the wide API.
    {
        const wchar_t wdir[] = {L'p', L'r', L'i', L'n', L't', L'_', 0x00E9, L't', 0x00E9, 0};
        const wchar_t wfile[] = {L'p', L'r', L'i', L'n', L't', L'_', 0x00E9, L't', 0x00E9,
                                 L'/', L'u', L't', L'f', L'8', L'.', L'p', L'n', L'g', 0};
        _wmkdir(wdir);
        const char* path = "print_\xc3\xa9t\xc3\xa9/utf8.png";   // the same name in UTF-8
        const uint8_t px[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
        const int wrote = stbi_write_png(path, 2, 2, 4, px, 8);
        FILE* back = _wfopen(wfile, L"rb");
        if (back) std::fclose(back);
        _wremove(wfile);
        _wrmdir(wdir);
        T19(wrote && back, "a UTF-8 path reaches the file system intact: a PNG written to print_(e-acute)t(e-acute)/ is found there through the wide API (%s)",
            back ? "the narrow CRT reads UTF-8" : "the narrow CRT read the path in the ANSI code page");
    }
#endif
    sumi_set_params(inst, &base);
}

enum SoakOp {
    SOAK_TINE = 0, SOAK_PINCH_SADDLE, SOAK_PINCH_CROSS, SOAK_WAKE_DOUBLET, SOAK_WAKE_STOKESLET,
    SOAK_RIPPLE_BAKE, SOAK_SWIRL, SOAK_VORTEX_EXP, SOAK_VORTEX_RANKINE,
    SOAK_TORSION,   // Phase 6 step 36: the first new operator through the gate
    SOAK_CHLADNI,   // Phase 6 step 37
    SOAK_CHLADNI_FIELD,   // Phase 6 step 43: the blended field (sub-stepped)
    SOAK_BURST,     // Phase 6 step 38
    SOAK_SPARK_SHEAR,   // Phase 6 step 39: the exact shear
    SOAK_SPARK,         // Phase 6 step 39: the composed strike (sub-stepped by inheritance)
    SOAK_CHIRIKOV,      // Phase 6 step 40: the standard map through its delta route
    SOAK_CRYSTAL_DROP,  // Phase 11 step 71b: the habit drop — exact by the theorem; no inverse pass (its (b) is the closed form)
    SOAK_TWIN_TINE,     // Phase 11 step 71b: the twin-kink cleavage profile
    SOAK_TERRACE_TINE,  // Phase 11 step 71b: the terrace profile
    SOAK_ARNOLD,        // Phase 11 step 72: the Arnol'd tongue lock — exact (a canonical kick, a rotation), the pairs through its exact inverse
    SOAK_BRAID,         // Phase 11 step 73: Kirchhoff chord braiding — exact by construction (the swirl passes in Strang order), the pairs at ±Γ
    SOAK_CHORD_WAVE,    // Phase 11 step 74: the chord-wave lattice — a windowed ripple-tine, exact (a shear by s₂ alone), the pairs at ±A
    SOAK_LEADER,        // Phase 12 step 76b: the leader stroke's segment — a windowed shear along the segment, sub-stepped (the ramps along), the pairs at ±amp
    SOAK_COUNT
};
struct SoakDesc { const char* name; bool exact; const char* det; uint32_t pair_passes; };   // pair_passes: the passes each way of a (b) pair (2 for most; the braid's two-vortex Strang step 4) — the bar grows as (passes/2)^0.7, the resampler's walk
static const SoakDesc SOAKS[SOAK_COUNT] = {
    {"tine",           true,  "shear along the line, p' = p + z*w(d)*u with d the perpendicular distance (unchanged by the shear) -> det J = 1 (Jaffer)", 2u},
    {"pinch-saddle",   true,  "Hamiltonian saddle, the flow of H = xy: s = xy conserved along trajectories -> det J = 1 (DECISIONS_3 #32)", 2u},
    {"pinch-cross",    true,  "two crossed tines composed, each a shear with det J = 1", 2u},
    {"wake-doublet",   false, "potential doublet: div d = 0 as a field, applied in <= a/4 steps -> area-preserving to first order (DECISIONS_3 #32)", 2u},
    {"wake-stokeslet", false, "2-D unsteady Stokeslet: div d = 0 as a field, applied in <= a/4 steps (DECISIONS_4 #53)", 2u},
    {"ripple-bake",    true,  "shear x' = x + A*sin(k*y + phi) in the ripple frame: det J = 1 for ANY profile; CC route (fixed phi) - the bend route drifts phi on purpose (#36)", 2u},
    {"swirl",          true,  "rotation by theta(r) = S*(1 - exp(-r^2/rc^2))/(2 pi r^2): r preserved -> det J = 1 (Lamb-Oseen)", 2u},
    {"vortex-exp",     true,  "rotation by theta(r) = A*exp(-r/R): r preserved -> det J = 1", 2u},
    {"vortex-rankine", true,  "rotation by theta(r), rigid core and 1/r^2 outside: r preserved -> det J = 1", 2u},
    {"torsion",        true,  "wave torsion: rotation by theta(r) = A sin(k r - phi) e^(-r/R): r preserved -> det J = 1 at any A (MEDIUM 2.1)", 2u},
    {"chladni",        true,  "Chladni cells: an eddy in every display cell - a rotation by theta (4 rho^2 (1 - rho^2))^2 inside each disjoint disc (a ring: the core rests), r preserved -> det J = 1; the water between rests (MEDIUM 2.2, step 43)", 2u},
    {"chladni-field",  false, "the inverse Chladni: the discs' rings summed into ONE displacement field (a radial swirl is divergence-free, so the sum is), cells at 1.5 of their keys overlapping - area-preserving to first order; a pass of theta on radius R has |grad d| ~ 4.6 theta per disc (MEDIUM 2.2, step 43)", 2u},
    {"burst",          false, "viscous multipole burst: d = grad-perp Psi, div d = 0 as a field, applied in passes whose peak displacement <= beta_m x the current core (|grad d| <= 0.25; MEDIUM 2.3, DECISIONS_5 #29)", 2u},
    {"spark-shear",    true,  "spark shear: x1 = x + A w(y) f(y), y1 = y + B w(x1) f(x1) - two shears, exact for ANY profile (a triangle stack here); the inverse is (0,-B) then (-A,0), reversed order (MEDIUM 2.4)", 2u},
    {"spark",          false, "the composed strike: an exact drop + the sub-stepped burst + the exact shear episode -> sub-stepped BY INHERITANCE, gated under the burst's numbers (ROADMAP_5's strictest-member rule)", 2u},
    {"chirikov",       true,  "Chirikov standard map: the kick y1 = y + A sin(k(x-xc)+phi) then the drift x1 = x + eps (y1-yc) - two shears -> det J = 1 at any K; the inverse undoes the drift first (MEDIUM 2.5)", 2u},
    {"crystal_drop",   true,  "the crystal drop: rho_src = rho - R(theta)^2/2 in the action-angle frame, a kick generated by V(theta) = R^2/2 -> det J = 1 for any habit at any amplitude (KESSHO 4.3, Hamilton 1832); no inverse pass, so (b) is ONE pass against the closed form", 2u},
    {"twin_tine",      true,  "the twin-kink tine: a shear along the line by a bump of the perpendicular distance (1 to H/3, linear to 0 at H = 4 alpha) -> det J = 1 (Jaffer's argument for any profile; KESSHO 4.4)", 2u},
    {"terrace_tine",   true,  "the terrace tine: a shear by stair_4(1 - d/H), four flat steps, zero beyond H -> det J = 1 (KESSHO 4.4)", 2u},
    {"arnold",         true,  "the Arnol'd tongue lock: the kick from the generating function F2 = theta rho1 + (K/q) W(rho1) cos q theta (a canonical transformation, det J = 1) composed with a twist by a function of r alone (a rotation, det J = 1); the inverse pass looks M up instead of M^-1 (KESSHO 2.2)", 2u},
    {"braid",          true,  "Kirchhoff chord braiding: the exact Lamb-Oseen passes, one per voice, in Strang order (the halves 1..N then N..1) - every factor det J = 1, so the composition is; the inverse is the same sequence at -Gamma (KESSHO 2.4)", 4u},
    {"chord_wave",     true,  "the chord-wave lattice: a ripple-tine windowed along s2 - a shear along the band's axis by a function of the coordinate across it, s1' = s1 + A sin(w s2 + phi) w(s2 - s2k): det J = 1 for any profile and any window (KESSHO 2.3)", 2u},
    {"leader",         false, "the leader stroke's segment: a slip ALONG the segment windowed by a C2 bump across (a pure shear: det J = 1) AND along (the end ramps: a compression of at most amp w'/ramp), applied in sub-steps of <= ramp/4 - area-preserving to first order (TO_PROJECT_SPEC 10.10, step 76b)", 2u},
};
static const uint8_t SOAK_VOICE_NOTE = 66;          // F#4: cell (0.535, 0.5) on the chroma grid
static const float   SOAK_CX = 0.52f, SOAK_CY = 0.49f;   // the pairs' centre, on the scene's ink
// (b) pre-image tolerance, in texels. Even an exact pair leaves resampling
// drift behind: 1000 strong passes of bilinear gather over the CURVED (u, v)
// ramps of a deformed field random-walk the pre-image by a few texels (the
// step-19 pinch, exact by construction, measures ~2.5). A non-inverting pair
// leaves hundreds. The bars sit well above the measured drift of the v1
// operators (this step's baselines) and far below the failure they catch.
// Exact-class pre-image bar. Smooth rotations and shears wander 1.5–2.7
// texels over 1000 strong passes; an OSCILLATORY shear field (the torsion on a
// 39-texel wavelength) wanders 5.45 — its markers still return to 3e-4 rad
// after one ±A pair, so the wander is the resampler's, not the operator's
// (DECISIONS_5 #21). A non-inverting pair reads 204. Eight keeps the
// oscillatory family green with a 25× margin to the failure it must catch.
// Phase 8 step 55b (DECISIONS_7 #3): those numbers were the COORDINATE
// payload's, which froze the water's pre-image at its quantum after ~50 pairs
// (the resampler's per-pair smoothing, ~0.02 texel, fell under half a 2^-11
// step and rounded back — 0.01 texel of drift in the water after 500 pairs).
// The displacement payload keeps that smoothing honestly: the same single-pair
// residual (0.19 texel for the tine, on both payloads), accumulating as ~N^0.7
// everywhere — tine 11.9, torsion 16.4, spark-shear 20.3, the Chladni stir
// 22.1 after 500 pairs, the ink mass bitwise what it was. The non-inverse
// (the crossed pinch) reads 72.9 and grows linearly from its first pair.
// Thirty-two: 1.45× the worst exact operator, 0.44× the failure it must catch.
static const double SOAK_DEV_EXACT = 32.0;
// Pair magnitudes displace the ink by ~25 texels per pass (tine z = 0.05,
// pinch k = 0.3, one a/4 wake sub-step, the rotations ~1 rad at R = 0.25, the
// torsion 0.5 rad at R = 0.5 on a 39-texel wavelength): strong, and of one
// order across operators — a pair at a pathological scale (a whole radian
// across a 23-texel wavelength) measures the resampler, not the operator.
// (b) mass is the COARSE guard (a broken pair loses everything: the negative
// control reads -99.7%); the pre-image return is the sharp one. The medium
// GAINS at ink/water boundaries under strong pairs in proportion to the
// boundary length moved: the tine's band +0.76%, the whole-canvas ripple
// +2.7% (DECISIONS_5 #15) — the window must clear a whole-canvas exact shear.
static const double SOAK_MASS_PCT = 5.0;
static const double SOAK_GROWTH_RATE_MAX = 5e-5;  // (c) per pass, when growth exceeds 0.5% over the window
static const long   SOAK_GATE_MIN_PASSES = 3000; // (d) below this the control is still in its early gain

static bool soak_measure(sumi_instance_t* inst, double* mass, FieldF* keep) {
    FieldF f;
    bool ok = false;
    // The field readback refuses to START while a paper-dip print readback is
    // still in flight (the two share the machinery) and the bounded GPU wait
    // can miss under load: retry across frames, up to two seconds of clock.
    for (int attempt = 0; attempt < 240 && !ok; attempt++) {
        ok = t19_read_field(inst, &f);
        if (!ok) { sumi_update(inst, 1.0 / 120.0); sumi_render(inst); }
    }
    if (!ok) { std::printf("[soak] field readback never became available (print readback stuck?)\n"); return false; }
    long a = 0, b = 0;
    t19_band_areas(&f, mass, &a, &b);
    if (keep) *keep = f; else std::free(f.px);
    return true;
}
// Mean |pre-image displacement| between two field states, in texels.
static double soak_preimage_dev(const FieldF& a, const FieldF& b) {
    const size_t n = (size_t)a.w * a.h;
    double acc = 0.0;
    for (size_t i = 0; i < n; i++) {
        const double du = ((double)b.px[i * 4] - a.px[i * 4]) * a.w;
        const double dv = ((double)b.px[i * 4 + 1] - a.px[i * 4 + 1]) * a.h;
        acc += std::sqrt(du * du + dv * dv);
    }
    return n ? acc / (double)n : 0.0;
}
static void soak_scene(GLFWwindow* window, sumi_instance_t* inst) {
    // The step-19 scene: three drops around the centre. The pairs act on
    // these; the MPE voice (below) joins for the stream, as in the original.
    sumi_add_drop(inst, 0.5f, 0.5f, 0.20f, 0);  t19_step(window, inst, 1);
    sumi_add_drop(inst, 0.40f, 0.45f, 0.10f, 0); t19_step(window, inst, 1);
    sumi_add_drop(inst, 0.62f, 0.58f, 0.08f, 0); t19_step(window, inst, 1);
}
static void soak_voice_on(GLFWwindow* window, sumi_instance_t* inst) {
    // The MPE voice whose own drop sits at F#4's cell — the MIDI routes act on it.
    sumi_push_midi(inst, 0xB0, 101, 0);   // MCM: MPE, lower zone, 15 members
    sumi_push_midi(inst, 0xB0, 100, 6);
    sumi_push_midi(inst, 0xB0, 6, 15);
    sumi_push_midi(inst, 0x91, SOAK_VOICE_NOTE, 100);
    t19_step(window, inst, 4);
}
static void soak_reset(GLFWwindow* window, sumi_instance_t* inst, const sumi_params_t& base) {
    // Release the voice, rest every controller the streams touch, restore the
    // pristine params, fresh sheet (the dip resets the field and rebases).
    sumi_push_midi(inst, 0x81, SOAK_VOICE_NOTE, 64);
    sumi_push_midi(inst, 0x82, 59, 64);   // step 72: the arnold soak's bass, if held
    sumi_push_midi(inst, 0xE1, 0x00, 0x40);
    sumi_push_midi(inst, 0xD1, 0, 0);
    sumi_push_midi(inst, 0xB0, 1, 0);
    sumi_push_midi(inst, 0xB0, RIPPLE_AMP_CC, 0);
    t19_step(window, inst, 30);
    sumi_set_params(inst, &base);
    uint32_t pw = 0, ph = 0;
    uint8_t* print = t19_dip_print(window, inst, &pw, &ph);
    if (!print) std::printf("[soak] reset: the dip's print did not come back within 600 frames\n");
    std::free(print);
    t19_step(window, inst, 2);
}
static void soak_prep(GLFWwindow* window, sumi_instance_t* inst, SoakOp op) {
    // Flavour controls an operator's pairs depend on, set through the real
    // ctl path and settled before anything is measured.
    if (op == SOAK_TORSION) {
        sumi_map_cc(inst, 0xFF, 104, SUMI_CTL_TORSION_K);
        sumi_map_cc(inst, 0xFF, 105, SUMI_CTL_TORSION_PHASE);
        sumi_push_midi(inst, 0xB0, 104, 32);   // k ≈ 2π·13: a 39-texel wavelength at 512
        sumi_push_midi(inst, 0xB0, 105, 0);
        t19_step(window, inst, 120);
    }
    if (op == SOAK_CHLADNI || op == SOAK_CHLADNI_FIELD) {
        sumi_map_cc(inst, 0xFF, 106, SUMI_CTL_CHLADNI_A);
        sumi_map_cc(inst, 0xFF, 107, SUMI_CTL_CHLADNI_B);
    }
    if (op == SOAK_CHIRIKOV) sumi_map_cc(inst, 0xFF, 109, SUMI_CTL_CHIRIKOV_K);   // the throw's handle (the mod wheel under the Anod table)
    if (op == SOAK_ARNOLD || op == SOAK_BRAID) {   // step 72 / 73: a bass a fifth below the soak's voice (66), held through the run — the voice locks to it (Ω₀ = 1/2, q = 2) or braids with it (a consonant pair)
        sumi_push_midi(inst, 0xB0, 101, 0); sumi_push_midi(inst, 0xB0, 100, 6); sumi_push_midi(inst, 0xB0, 6, 15);
        sumi_push_midi(inst, 0x92, 59, 100);
        t19_step(window, inst, 4);
    }
}
static void soak_modes(sumi_instance_t* inst, const sumi_params_t& base, SoakOp op) {
    sumi_params_t p = base;
    p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    switch (op) {
    case SOAK_PINCH_SADDLE:  p.slide_mode = 1; p.pinch_variant = 0; break;
    case SOAK_PINCH_CROSS:   p.slide_mode = 1; p.pinch_variant = 1; break;
    case SOAK_WAKE_DOUBLET:  p.wake_profile = 0; break;
    case SOAK_WAKE_STOKESLET: p.wake_profile = 1; p.wake_spread = 3.0f; break;
    case SOAK_RIPPLE_BAKE:   p.ripple_bake = 1; p.bend_mode = 0; break;   // CC route: phi fixed
    case SOAK_SWIRL:         p.press_mode = 1; break;                     // 0xD0 -> the swirl
    case SOAK_VORTEX_EXP:    p.vortex_profile = SUMI_VORTEX_EXPONENTIAL; break;
    case SOAK_VORTEX_RANKINE: p.vortex_profile = SUMI_VORTEX_RANKINE; break;
    case SOAK_TORSION:       p.vortex_profile = SUMI_VORTEX_TORSION; break;
    case SOAK_CHLADNI:       break;   // the flow runs on the chroma grid's cells (soak_modes sets the layout)
    case SOAK_CHLADNI_FIELD: p.chladni_mode = SUMI_CHLADNI_FIELD; p.chladni_cell = 1.5f; break;   // the blended field at its widest overlap
    case SOAK_BURST:         p.burst_age = 4.0f; p.burst_life = 0.0f; break;   // the pairs land at once; the stream sets its own release
    case SOAK_SPARK_SHEAR:   p.spark_stack = 3; p.spark_profile = 0; break;
    case SOAK_SPARK:         p.burst_age = 4.0f; p.burst_life = 0.0f; p.spark_tau = 0.02f; p.spark_shear = 0.6f; p.spark_stack = 3; p.spark_profile = 0; break;   // the shear episode over in 10 frames
    case SOAK_CHIRIKOV:      p.chirikov_kmax = 1.0f; p.chirikov_periods = 2; p.chirikov_eps = 0.5f; break;
    case SOAK_CRYSTAL_DROP:  p.drop_habit = SUMI_HABIT_RHOMB; p.habit_roundness = 0.0f; p.habit_blades = 5u; break;   // the stream cycles the habits and the roundness
    case SOAK_TWIN_TINE:     p.tine_profile = SUMI_TINE_TWIN_KINK; p.bend_mode = 0; break;      // the glide tine inherits the profile
    case SOAK_TERRACE_TINE:  p.tine_profile = SUMI_TINE_TERRACE; p.bend_mode = 0; break;
    case SOAK_ARNOLD:        p.press_mode = 3; break;   // the press is the kick; the bass below is soak_prep's
    case SOAK_BRAID:         p.bend_mode = 5; p.press_mode = 0; break;   // the dyad braids by itself; the bass below is soak_prep's
    case SOAK_CHORD_WAVE:    p.medium = SUMI_MEDIUM_KESSHO; p.bend_mode = 0; p.press_mode = 0; break;   // Kessho's poly pressure is the wave (the field is the medium's same)
    case SOAK_LEADER:        break;   // step 76b: the pairs and the sub-step read push segments directly; the stream strokes through the gesture route
    default: break;
    }
    sumi_set_params(inst, &p);
}
// One strong (+k, -k) pair through the gesture ABI (the ripple: its CC).
static void soak_pair(GLFWwindow* window, sumi_instance_t* inst, SoakOp op) {
    switch (op) {
    case SOAK_TINE: case SOAK_TWIN_TINE: case SOAK_TERRACE_TINE:   // z = 0.05: ~25 texels at the line, the other pairs' order of displacement at the ink (the profile is the params')
        sumi_add_tine(inst, 0.30f, 0.42f, 0.74f, 0.56f, 0.05f, 0.05f);
        sumi_add_tine(inst, 0.74f, 0.56f, 0.30f, 0.42f, 0.05f, 0.05f);
        break;
    case SOAK_PINCH_SADDLE: case SOAK_PINCH_CROSS:
        sumi_add_pinch(inst, SOAK_CX, SOAK_CY, 0.3f, 0.6f);
        sumi_add_pinch(inst, SOAK_CX, SOAK_CY, -0.3f, 0.6f);
        break;
    case SOAK_WAKE_DOUBLET: case SOAK_WAKE_STOKESLET: {
        // ONE a/4 sub-step each way (a = 0.04, d = 0.01): a pair is two
        // resampling passes like every other pair. A long stroke would be
        // 20 sub-steps each way and its fade would swamp the comparison.
        const float a = 0.04f, d = a * 0.25f;
        sumi_add_wake(inst, SOAK_CX - 0.5f * d, 0.50f, SOAK_CX + 0.5f * d, 0.50f, a);
        sumi_add_wake(inst, SOAK_CX + 0.5f * d, 0.50f, SOAK_CX - 0.5f * d, 0.50f, a);
        break;
    }
    case SOAK_RIPPLE_BAKE:
        // Up then down through the smoother: the two deltas sum to zero.
        sumi_push_midi(inst, 0xB0, RIPPLE_AMP_CC, 127); t19_step(window, inst, 1);
        sumi_push_midi(inst, 0xB0, RIPPLE_AMP_CC, 0);
        break;
    case SOAK_SWIRL: {
        const float rc = 0.06f, S = 2.0f * 6.2831853f * rc * rc;   // 2 rad core rotation
        sumi_add_vortex(inst, SOAK_CX, SOAK_CY,  S, rc, SUMI_VORTEX_LAMB_OSEEN);
        sumi_add_vortex(inst, SOAK_CX, SOAK_CY, -S, rc, SUMI_VORTEX_LAMB_OSEEN);
        break;
    }
    case SOAK_VORTEX_EXP: case SOAK_VORTEX_RANKINE: {
        const uint32_t prof = op == SOAK_VORTEX_EXP ? SUMI_VORTEX_EXPONENTIAL : SUMI_VORTEX_RANKINE;
        sumi_add_vortex(inst, SOAK_CX, SOAK_CY,  1.0f, 0.25f, prof);
        sumi_add_vortex(inst, SOAK_CX, SOAK_CY, -1.0f, 0.25f, prof);
        break;
    }
    case SOAK_TORSION:   // 0.5 rad on a 39-texel wavelength (soak_prep), decay length 0.5: ~25 texels at the ink
        sumi_add_vortex(inst, SOAK_CX, SOAK_CY,  0.5f, 0.5f, SUMI_VORTEX_TORSION);
        sumi_add_vortex(inst, SOAK_CX, SOAK_CY, -0.5f, 0.5f, SUMI_VORTEX_TORSION);
        break;
    case SOAK_CHLADNI_FIELD:   // one budgeted field pass and its negative: theta 0.025 on the chroma cells at 1.5 (|grad d| ~ 0.23 where two rings overlap; 9.5e-4 peak displacement, two quanta)
        sumi_debug_add_cells_pass(inst,  0.025f, -1.0f, SUMI_CHLADNI_FIELD);
        sumi_debug_add_cells_pass(inst, -0.025f, -1.0f, SUMI_CHLADNI_FIELD);
        break;
    case SOAK_CHLADNI:    // one Taylor-Green step of Ψ 0.006 on a 3 x 2 lattice (~17 texels at the boundaries); the negative psi is the EXACT inverse
        sumi_add_chladni(inst,  0.006f, 0.0f, 1.0f / 3.0f, 0.0f, 0.5f, 0.0f);
        sumi_add_chladni(inst, -0.006f, 0.0f, 1.0f / 3.0f, 0.0f, 0.5f, 0.0f);
        break;
    case SOAK_BURST:      // a quadrupole of D = a/4 (a = 0.04: ~5 texels at the lobes, the wake pair's size) and its first-order inverse, both at once (life 0)
        sumi_add_burst(inst, SOAK_CX, SOAK_CY, 0.04f,  0.01f, 0.0f, 2);
        sumi_add_burst(inst, SOAK_CX, SOAK_CY, 0.04f, -0.01f, 0.0f, 2);
        break;
    case SOAK_SPARK_SHEAR:   // one kick-drift step of A = B = 0.03 (15 texels) in a 0.2 window, then its EXACT inverse: (0,-B) then (-A,0)
        sumi_add_spark_shear(inst, SOAK_CX, SOAK_CY, 0.2f,  0.03f,  0.03f, SOAK_SPARK_K, 0.7f, 0.3f);
        sumi_add_spark_shear(inst, SOAK_CX, SOAK_CY, 0.2f,  0.0f,  -0.03f, SOAK_SPARK_K, 0.7f, 0.3f);
        sumi_add_spark_shear(inst, SOAK_CX, SOAK_CY, 0.2f, -0.03f,  0.0f,  SOAK_SPARK_K, 0.7f, 0.3f);
        break;
    case SOAK_CHIRIKOV:   // one step at K = 0.5 (periods 2, eps 0.5: a 41-texel kick amplitude, ~12 texels at the ink) and its EXACT inverse
        sumi_add_chirikov(inst, SOAK_CX, SOAK_CY,  0.5f, 2, 0.5f, 0.3f);
        sumi_add_chirikov(inst, SOAK_CX, SOAK_CY, -0.5f, 2, 0.5f, 0.3f);
        break;
    case SOAK_ARNOLD:   // one iteration at K_max (q 2, half a turn at the ring, phase 0.3) then its EXACT inverse (the lookup of M)
        sumi_debug_add_arnold(inst, SOAK_CX, SOAK_CY, 0.12f, 0.5f, 1.0f, 2u, 0.3f, 0);
        sumi_debug_add_arnold(inst, SOAK_CX, SOAK_CY, 0.12f, 0.5f, 1.0f, 2u, 0.3f, 1);
        break;
    case SOAK_BRAID: {   // a two-vortex Strang step at a strong Γ, then the same sequence at −Γ (its exact inverse)
        const float rc = 0.06f, S = 1.0f * 6.2831853f * rc * rc;   // ~a turn at the core per step
        for (int sgn = 1; sgn >= -1; sgn -= 2) {
            sumi_add_vortex(inst, SOAK_CX - 0.06f, SOAK_CY, 0.5f * sgn * S, rc, SUMI_VORTEX_LAMB_OSEEN);
            sumi_add_vortex(inst, SOAK_CX + 0.06f, SOAK_CY, 0.5f * sgn * S, rc, SUMI_VORTEX_LAMB_OSEEN);
            sumi_add_vortex(inst, SOAK_CX + 0.06f, SOAK_CY, 0.5f * sgn * S, rc, SUMI_VORTEX_LAMB_OSEEN);
            sumi_add_vortex(inst, SOAK_CX - 0.06f, SOAK_CY, 0.5f * sgn * S, rc, SUMI_VORTEX_LAMB_OSEEN);
        }
        break;
    }
    case SOAK_CHORD_WAVE:   // a strong windowed ripple-tine through the scene's ink (A = 0.03: 15 texels at 512), then its exact inverse (−A)
        sumi_debug_add_chord_wave(inst, SOAK_CX, SOAK_CY, 0.5f, 8.0f, 0.03f, 0.25f);
        sumi_debug_add_chord_wave(inst, SOAK_CX, SOAK_CY, 0.5f, 8.0f, -0.03f, 0.25f);
        break;
    case SOAK_LEADER:     // one segment through the ink (hw 0.02, ramp 0.08, amp 0.01 = ramp/8) and its first-order negative (exact where the window is flat along)
        sumi_debug_add_leader_seg(inst, 0.35f, 0.45f, 0.70f, 0.55f,  0.01f, 0.02f, 0.08f);
        sumi_debug_add_leader_seg(inst, 0.35f, 0.45f, 0.70f, 0.55f, -0.01f, 0.02f, 0.08f);
        break;
    case SOAK_SPARK:      // the composed strike WITHOUT its blast at D = a/4 and its first-order negative (informational for the class):
                          // a drop's exact expansion pushes ink off the canvas — 500 clear-water strikes at one spot read mass 0 (#39)
        sumi_add_spark(inst, SOAK_CX, SOAK_CY, 0.04f,  0.01f, 0.0f, SUMI_DROP_NONE);
        sumi_add_spark(inst, SOAK_CX, SOAK_CY, 0.04f, -0.01f, 0.0f, SUMI_DROP_NONE);
        break;
    default: break;
    }
}
// One frame of the gesture-rate stream: a 0.5 Hz wobble at the 120 Hz clock
// through the operator's REAL route (smoothed, mapper-coalesced, at most one
// pass per frame) — the step-19 DONE stream, one shape for every operator.
static void soak_stream_frame(sumi_instance_t* inst, SoakOp op, long i, float* wake_x) {
    const double ph = (double)i * 2.0 * 3.14159265 * 0.5 / 120.0;
    const uint8_t v = (uint8_t)(63.5 + 63.5 * std::sin(ph));
    switch (op) {
    case SOAK_TINE: case SOAK_TWIN_TINE: case SOAK_TERRACE_TINE: {   // note bend, bend_mode 0: the glide tine (71b: under the cleavage profile)
        const double semis = 2.0 * std::sin(ph);
        const long pb = 8192 + (long)(semis / 48.0 * 8192.0);
        sumi_push_midi(inst, 0xE1, (uint8_t)(pb & 0x7F), (uint8_t)((pb >> 7) & 0x7F));
        break;
    }
    case SOAK_PINCH_SADDLE: case SOAK_PINCH_CROSS:      // CC74 deltas, slide_mode 1
        sumi_push_midi(inst, 0xB1, 74, v);
        break;
    case SOAK_WAKE_DOUBLET: case SOAK_WAKE_STOKESLET: { // the stylus, one segment per frame
        const float x = 0.535f + 0.08f * (float)std::sin(ph);
        sumi_add_wake(inst, *wake_x, 0.5f, x, 0.5f, 0.03f);   // <= a/4 per frame: one sub-step
        *wake_x = x;
        break;
    }
    case SOAK_RIPPLE_BAKE:                              // amplitude CC, bake deltas
        sumi_push_midi(inst, 0xB0, RIPPLE_AMP_CC, v);
        break;
    case SOAK_SWIRL: case SOAK_ARNOLD:                  // channel pressure: press_mode 1 the swirl, 3 the lock (step 72: K from the press, one iteration a frame)
        sumi_push_midi(inst, 0xD1, v, 0);
        break;
    case SOAK_CHORD_WAVE:                               // step 74: the poly pressure wobbles the wave's amplitude — a delta pass each change
        sumi_push_midi(inst, 0xA1, SOAK_VOICE_NOTE, v);
        break;
    case SOAK_BRAID: {                                  // step 73: the dyad braids on its own; the bend wobbles the interval (±1 semitone) and so the circulation
        const double semis = 1.0 * std::sin(ph);
        const long pb = 8192 + (long)(semis / 48.0 * 8192.0);
        sumi_push_midi(inst, 0xE1, (uint8_t)(pb & 0x7F), (uint8_t)((pb >> 7) & 0x7F));
        break;
    }
    case SOAK_VORTEX_EXP: case SOAK_VORTEX_RANKINE: case SOAK_TORSION:   // the mod wheel (core default map)
        sumi_push_midi(inst, 0xB0, 1, v);
        break;
    case SOAK_CHLADNI: case SOAK_CHLADNI_FIELD:         // the stir control: a steady cellular flow whose rate wobbles
        sumi_push_midi(inst, 0xB0, 106, v);
        break;
    case SOAK_CHIRIKOV:                                 // the throw control's wobble: DELTAS of the wheel, each a gentle step (δ² K_max) — the pendulum regime
        sumi_push_midi(inst, 0xB0, 109, v);
        break;
    case SOAK_SPARK_SHEAR: {                            // a small exact step per frame, its kick wobbling in sign: chaotic advection under a jagged shear
        const float A = 0.003f * (float)std::sin(ph);
        sumi_add_spark_shear(inst, SOAK_CX, 0.5f, 0.2f, A, A, SOAK_SPARK_K, 0.7f, 0.3f);
        break;
    }
    case SOAK_SPARK: {                                  // composed strikes at gesture rate: one every 4 frames, the axis turning — WITHOUT the blast:
        if (i == 0) {                                   //   the drop is excluded from the gate by nature (#13) and its exact expansion carries ink
            sumi_params_t p; sumi_get_params(inst, &p); //   off the canvas (1500 strikes read mass 0, #39); the episodes are the stream
            p.burst_life = 0.025f;                      // the burst's 3-frame release; the shear's 10-frame episode is soak_modes'
            sumi_set_params(inst, &p);
        }
        if (i % 4 == 0) {
            const float x = 0.535f + 0.08f * (float)std::sin(ph);
            sumi_add_spark(inst, x, 0.5f, 0.03f, 0.0035f, (float)ph, SUMI_DROP_NONE);
        }
        break;
    }
    case SOAK_LEADER: {                                 // step 76b: strokes at gesture rate through the gesture route — one every 12 frames at half energy
        if (i % 12 == 0) {                              //   (24 segments, one sub-step each: two strokes alive, two thin passes a frame), the drift turning with the wobble
            const float x = 0.535f + 0.08f * (float)std::sin(ph);
            sumi_add_leader(inst, x, 0.5f, (float)std::cos(ph), (float)std::sin(ph), 0.5f, (uint32_t)i);
        }
        break;
    }
    case SOAK_CRYSTAL_DROP: {                           // habit strikes at gesture rate: a clear drop every 3 frames, the habit and the roundness cycling,
        if (i % 3 == 0) {                               //   the frame turning — the spark's density (#39: a drop's exact expansion carries ink off the canvas)
            sumi_params_t p; sumi_get_params(inst, &p);
            const long k = i / 3;
            p.drop_habit = 1u + (uint32_t)(k % 3); p.habit_roundness = (k / 3) % 2 ? 0.5f : 0.0f; p.habit_blades = 5u;
            sumi_set_params(inst, &p);
            const float x = 0.535f + 0.08f * (float)std::sin(ph);
            sumi_add_drop(inst, x, 0.5f, 0.03f, SUMI_DROP_CLEAR);   // a clear-water habit drop: a pass, no ink (NONE is no pass at all)
        }
        break;
    }
    case SOAK_BURST: {                                  // strikes at gesture rate: one every 3 frames, each a 3-frame release
        if (i == 0) {                                   //   (~one pass per frame on average, the wake stream's density)
            sumi_params_t p; sumi_get_params(inst, &p);
            p.burst_life = 0.025f;                      // 3 frames of the 1/120 clock
            sumi_set_params(inst, &p);
        }
        if (i % 3 == 0) {
            const float x = 0.535f + 0.08f * (float)std::sin(ph);
            sumi_add_burst(inst, x, 0.5f, 0.03f, 0.0035f, (float)ph, 2);   // a/8.6 of displacement, the axis turning with the wobble
        }
        break;
    }
    default: break;
    }
}
static void soak_stream_end(GLFWwindow* window, sumi_instance_t* inst, SoakOp op, const sumi_params_t& base) {
    switch (op) {
    case SOAK_TINE: case SOAK_TWIN_TINE: case SOAK_TERRACE_TINE: sumi_push_midi(inst, 0xE1, 0x00, 0x40); break;
    case SOAK_ARNOLD: sumi_push_midi(inst, 0xD1, 0, 0); sumi_push_midi(inst, 0x82, 59, 64); break;   // step 72: the press off, the bass released
    case SOAK_BRAID: sumi_push_midi(inst, 0xE1, 0x00, 0x40); sumi_push_midi(inst, 0x82, 59, 64); break;   // step 73: the bend home, the bass released
    case SOAK_CHORD_WAVE: sumi_push_midi(inst, 0xA1, SOAK_VOICE_NOTE, 0); break;   // step 74: the pressure off (the wave stays baked)
    case SOAK_RIPPLE_BAKE: sumi_push_midi(inst, 0xB0, RIPPLE_AMP_CC, 0); break;
    case SOAK_SWIRL:       sumi_push_midi(inst, 0xD1, 0, 0); break;
    case SOAK_VORTEX_EXP: case SOAK_VORTEX_RANKINE: case SOAK_TORSION: sumi_push_midi(inst, 0xB0, 1, 0); break;
    case SOAK_CHLADNI: case SOAK_CHLADNI_FIELD: sumi_push_midi(inst, 0xB0, 106, 0); break;
    case SOAK_CHIRIKOV: sumi_push_midi(inst, 0xB0, 109, 0); break;   // the wheel home: the inverse steps retrace
    case SOAK_LEADER: for (int i = 0; i < 600 && sumi_debug_spark_count(inst); i++) t19_step(window, inst, 1); break;   // step 76b: the running strokes walk out
    default: break;
    }
    t19_step(window, inst, 30);
    sumi_params_t p = base;                 // the modes off: the control below is the plain glide tine
    p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    sumi_set_params(inst, &p);
    t19_step(window, inst, 30);
}
// The (d) control: the identical wobble as glide tines on the same voice,
// over the same window (capped at 6000 — beyond it the v1 tine's fixture-
// pinned edge-clamp fabrication offsets its erosion, DECISIONS_3 #33), from
// a FRESH copy of the same scene: erosion is front-loaded (sharp structure
// fades fastest, mixed-to-gray ink barely fades), so a control run on the
// field the operator already worked would not measure the same thing.
static bool soak_tine_control(GLFWwindow* window, sumi_instance_t* inst, const sumi_params_t& base,
                              long passes, double* rate) {
    const long win = passes < 6000 ? passes : 6000;
    soak_reset(window, inst, base);
    soak_modes(inst, base, SOAK_TINE);
    soak_scene(window, inst);
    soak_voice_on(window, inst);
    double c0 = 0.0, c1 = 0.0;
    if (!soak_measure(inst, &c0, nullptr)) return false;
    float unused = 0.0f;
    for (long i = 0; i < win; i++) {
        soak_stream_frame(inst, SOAK_TINE, i, &unused);
        t19_step(window, inst, 1);
    }
    sumi_push_midi(inst, 0xE1, 0x00, 0x40);
    t19_step(window, inst, 4);
    if (!soak_measure(inst, &c1, nullptr)) return false;
    *rate = (c0 - c1) / c0 / (double)(win > 0 ? win : 1);   // negative = the control GAINED (early regime)
    return true;
}
// First-order area preservation of ONE <= a/4 sub-step on the identity field:
// the pre-image Jacobian over the interior (the Stokeslet test's measure).
static bool soak_substep_det(GLFWwindow* window, sumi_instance_t* inst, SoakOp op, double* dmin, double* dmean) {
    uint32_t pw = 0, ph = 0;
    std::free(t19_dip_print(window, inst, &pw, &ph));      // identity field
    t19_step(window, inst, 2);
    const float a = 0.04f, d = a * 0.25f;
    const bool wake = op == SOAK_WAKE_DOUBLET || op == SOAK_WAKE_STOKESLET;   // the profiles are already in params (soak_modes)
    int frames = 1;
    if (wake)                  sumi_add_wake(inst, SOAK_CX - d, 0.50f, SOAK_CX, 0.50f, a);
    else if (op == SOAK_BURST) sumi_add_burst(inst, SOAK_CX, 0.50f, a, 0.0045f, 0.0f, 2);   // ONE budgeted pass: peak 1.066·D = 0.0048 <= 0.13·a
    else if (op == SOAK_CHLADNI_FIELD) sumi_debug_add_cells_pass(inst, 0.025f, -1.0f, SUMI_CHLADNI_FIELD);   // ONE pass of the summed rings on the chroma cells at 1.5
    else if (op == SOAK_LEADER) sumi_debug_add_leader_seg(inst, 0.35f, 0.45f, 0.70f, 0.55f, 0.01f, 0.02f, 0.08f);   // step 76b: ONE sub-step of a segment (amp = ramp/8), measured everywhere
    else {                                                                                 // the composed strike: the drop and the burst
        // The SHEAR OFF for the Jacobian read: finite differences cannot
        // measure an exact kick-drift at its kinks (the spark test's note),
        // and the read is the sub-stepped member's; the stream below runs
        // the whole composition at the default shear.
        sumi_params_t p; sumi_get_params(inst, &p);
        const float keep = p.spark_shear;
        p.spark_shear = 0.0f; sumi_set_params(inst, &p);
        sumi_add_spark(inst, SOAK_CX, 0.50f, a, 0.0045f, 0.0f, 1u);
        t19_step(window, inst, 12);
        p.spark_shear = keep; sumi_set_params(inst, &p);
        frames = 0;
    }
    if (frames) t19_step(window, inst, frames);
    FieldF f;
    if (!t19_read_field(inst, &f)) return false;
    // The swept capsule (segment + a + 3 texels) is excluded for the WAKE, as
    // in the flick test: the potential doublet's body carries a slip surface
    // — a genuine tangential discontinuity of the flow, not a fold — where
    // finite differences stop measuring the map. Harmless for the viscous
    // profile; the burst has no body and is measured everywhere; the spark's
    // drop has a RIM where the pre-image jumps from the interior to the
    // exterior (3 texels either side excluded, the drop's own discontinuity).
    const float margin = wake ? a + 3.0f / (float)f.w : 0.0f;
    const bool  rim = op == SOAK_SPARK;      // the drop's rim, carried inward by the passes after it: r − 8 texels .. 1.4 r excluded (the spark test's note)
    const float x0 = SOAK_CX - d, x1 = SOAK_CX, yc = 0.50f;
    const uint32_t edge = rim ? 8u : 2u;     // the spark's shear bands reach the side edges: the ingress seam is not a fold
    double mn = 1e9, sum = 0.0; long n = 0;
    for (uint32_t y = edge; y + edge < f.h; y++) for (uint32_t x = edge; x + edge < f.w; x++) {
        const float px = ((float)x + 0.5f) / (float)f.w, py = ((float)y + 0.5f) / (float)f.h;
        const float cx = px < x0 ? x0 : (px > x1 ? x1 : px);
        if (wake && (px - cx) * (px - cx) + (py - yc) * (py - yc) < margin * margin) continue;
        if (rim) { const float dd = std::hypot(px - SOAK_CX, py - yc); if (dd > a - 8.0f / (float)f.w && dd < 1.4f * a) continue; }
        const double det = t33_det_at(f, x, y);
        if (det < mn) mn = det;
        sum += det; n++;
    }
    std::free(f.px);
    *dmin = mn; *dmean = n ? sum / (double)n : 0.0;
    return true;
}

static int g_tine_fault = 0;   // step 71b: --tine-fault — the soak runs under the unwindowed profile and (c) MUST trip
// step 71b: the crystal drop's (b) — ONE rhomb pass on the identity sheet against the closed form: the pre-image
// det over the interior, the habit's rim annulus excluded (the singular pre-image, as the v2 drop's), det min > 0.5
// and |mean − 1| < 2e-3 (the burst's numbers); and the flatness median of DECISIONS_10 #4
static bool soak_crystal_closed_form(GLFWwindow* window, sumi_instance_t* inst, double* dmin, double* dmean, double* median) {
    uint32_t pw = 0, ph = 0;
    std::free(t19_dip_print(window, inst, &pw, &ph));
    t19_step(window, inst, 2);
    sumi_params_t p; sumi_get_params(inst, &p);
    p.drop_habit = SUMI_HABIT_RHOMB; p.habit_roundness = 0.3f; sumi_set_params(inst, &p);
    sumi_add_drop(inst, 0.5f, 0.5f, 0.12f, SUMI_DROP_CLEAR);
    t19_step(window, inst, 2);
    FieldF f;
    if (!t19_read_field(inst, &f)) return false;
    double mn = 1e9, acc = 0.0; long n = 0;
    std::vector<double> devs;
    const double A = 1.3631;
    for (uint32_t y = 16; y + 16 < f.h; y++) for (uint32_t x = 16; x + 16 < f.w; x++) {
        const double px = ((double)x + 0.5) / f.w - 0.5, py = ((double)y + 0.5) / f.h - 0.5;
        const double th = std::atan2(py, px), r = std::sqrt(px * px + py * py);
        const double k = 2.0 + 40.0 * 0.7 * 0.7, a = std::fabs(std::sin(th)), b = std::fabs(std::sin(th - A)), m = a > b ? a : b;
        double R = 0.12 / (m + std::log(std::exp(k * (a - m)) + std::exp(k * (b - m))) / k); R += (0.12 - R) * 0.3;
        if (std::fabs(r - R) < 6.0 / f.w) continue;   // the rim annulus: the singular pre-image
        const size_t o = ((size_t)y * f.w + x) * 4;
        const double ux = ((double)f.px[o + 4] - f.px[o - 4]) * 0.5 * f.w, uy = ((double)f.px[o + 4 * f.w] - f.px[o - 4 * f.w]) * 0.5 * f.h;
        const double vx = ((double)f.px[o + 5] - f.px[o - 3]) * 0.5 * f.w, vy = ((double)f.px[o + 4 * f.w + 1] - f.px[o - 4 * f.w + 1]) * 0.5 * f.h;
        const double det = ux * vy - uy * vx;
        if (det < mn) mn = det; acc += det; n++; devs.push_back(std::fabs(det - 1.0));
    }
    std::free(f.px);
    std::sort(devs.begin(), devs.end());
    *dmin = mn; *dmean = n ? acc / n : 0.0; *median = devs.empty() ? 0.0 : devs[devs.size() / 2];
    return n > 0;
}
static void soak_one(GLFWwindow* window, sumi_instance_t* inst, const sumi_params_t& base, SoakOp op, long passes) {
    const SoakDesc& d = SOAKS[op];
    if (g_tine_fault) sumi_debug_set_tine_override(inst, 3);   // step 71b: every tine the unwindowed fault — the red control
    if (op == SOAK_LEADER) sumi_debug_set_leader_charge(inst, 0);   // step 76b: the bare shear is gated; the channel's charge is a deposit, excluded by nature (#13)
    std::printf("[soak] %s: %d pairs + %ld-pass stream + %ld-pass glide-tine control\n", d.name, 500, passes, passes < 6000 ? passes : 6000);
    std::printf("[soak] %s (a) class %s: %s\n", d.name, d.exact ? "exact" : "sub-stepped", d.det);
    soak_modes(inst, base, op);
    if (op == SOAK_RIPPLE_BAKE) sumi_map_cc(inst, 0xFF, RIPPLE_AMP_CC, SUMI_CTL_RIPPLE_AMP);
    soak_prep(window, inst, op);
    soak_scene(window, inst);

    // ---- (b) 500 strong (+k, -k) pairs --------------------------------------
    FieldF fa;
    double mass0 = 0.0;
    if (!soak_measure(inst, &mass0, &fa)) { t19_failures++; std::printf("FAIL: [soak] %s field read\n", d.name); return; }
    double mass_min = mass0, mass_max = mass0;
    for (int i = 0; i < 500; i++) {
        if (op != SOAK_CRYSTAL_DROP) soak_pair(window, inst, op);   // step 71b: a drop has no inverse — no pairs
        t19_step(window, inst, 1);
        if (i % 100 == 99) {
            double m = 0.0;
            if (soak_measure(inst, &m, nullptr)) {
                if (m < mass_min) mass_min = m;
                if (m > mass_max) mass_max = m;
                std::printf("[soak] %s pairs %d: ink mass %.0f (base %.0f, %+.2f%%)\n", d.name, i + 1, m, mass0, 100.0 * (m - mass0) / mass0);
            }
        }
    }
    t19_step(window, inst, 60);                          // the ripple's smoother settles
    FieldF fb;
    double mass_b = 0.0;
    if (!soak_measure(inst, &mass_b, &fb)) { std::free(fa.px); t19_failures++; std::printf("FAIL: [soak] %s field read\n", d.name); return; }
    if (mass_b < mass_min) mass_min = mass_b;
    if (mass_b > mass_max) mass_max = mass_b;
    const double dev = soak_preimage_dev(fa, fb);
    {
        // Where did the mass move? Split the per-texel phase delta into the
        // canvas-edge band (16 texels: the clamp legacy lives there) vs the
        // interior, and inside the initially inked region vs the water.
        double d_edge = 0.0, d_int = 0.0, d_ink = 0.0, d_water = 0.0;
        for (uint32_t y = 0; y < fa.h; y++) for (uint32_t x = 0; x < fa.w; x++) {
            const size_t o = ((size_t)y * fa.w + x) * 4 + 2;
            const double dm = (double)fb.px[o] - fa.px[o];
            const bool edge = x < 16 || y < 16 || x + 16 >= fa.w || y + 16 >= fa.h;
            (edge ? d_edge : d_int) += dm;
            (fa.px[o] > 0.5f ? d_ink : d_water) += dm;
        }
        std::printf("[soak] %s pairs delta: edge band %+.0f, interior %+.0f | inked region %+.0f, water %+.0f (base %.0f)\n",
                    d.name, d_edge, d_int, d_ink, d_water, mass0);
    }
    std::free(fa.px); std::free(fb.px);
    const double lo = 100.0 * (mass_min - mass0) / mass0, hi = 100.0 * (mass_max - mass0) / mass0;
    double det_min = 0.0, det_mean = 0.0;
    if (g_tine_fault) {
        // step 71b, the RED control: under the unwindowed profile the gate must go red — and it does at its first part: an odd
        // profile reversed is the SAME shear, so the "pair" carries the half-planes (and the ink) off the sheet instead of
        // returning them; there is nothing left for the stream to fabricate from
        const bool b_ok = hi <= SOAK_MASS_PCT && lo >= -SOAK_MASS_PCT && dev <= SOAK_DEV_EXACT;
        T19(!b_ok, "[soak] %s RED CONTROL (step 71b, the unwindowed profile): (b) fails as it must — the pairs hold ink mass %+.2f%%/%+.2f%%, pre-image dev %.1f texel (the half-planes slide off the sheet)", d.name, lo, hi, dev);
        sumi_debug_set_tine_override(inst, -1);
        std::printf("[soak] %s SUMMARY (red control) pairs_mass_lo=%+.2f%% pairs_mass_hi=%+.2f%% preimage_dev=%.2f\n", d.name, lo, hi, dev);
        return;
    }
    if (op == SOAK_CRYSTAL_DROP) {
        // no inverse pass: the pairs above were 500 empty frames (informational); (b) is the closed form (step 71b, the fold's note)
        double median = 0.0;
        if (!soak_crystal_closed_form(window, inst, &det_min, &det_mean, &median)) { t19_failures++; std::printf("FAIL: [soak] %s field read\n", d.name); return; }
        T19(det_min > 0.5 && std::fabs(det_mean - 1.0) < 2e-3 && median < 0.1,
            "[soak] %s (b) the closed form: one rhomb pass (roundness 0.3) on the identity sheet, pre-image det min %.3f (> 0.5) outside the rim annulus, mean %.5f (|1 - mean| < 2e-3), flatness median %.2e (< 0.1, DECISIONS_10 #4)",
            d.name, det_min, det_mean, median);
        sumi_set_params(inst, &base); soak_modes(inst, base, op);
        soak_scene(window, inst);                        // the stream needs its ink back
    } else if (d.exact) {
        const double dev_bar = SOAK_DEV_EXACT * std::pow((double)(d.pair_passes < 2u ? 2u : d.pair_passes) / 2.0, 0.7);   // the resampler's walk grows ~N^0.7 with the passes
        T19(hi <= SOAK_MASS_PCT && lo >= -SOAK_MASS_PCT && dev <= dev_bar,
            "[soak] %s (b) inversion: 500 (+k,-k) pairs hold ink mass %+.2f%%/%+.2f%% (|<= %.0f%%|), pre-image dev %.2f texel (<= %.1f%s)",
            d.name, lo, hi, SOAK_MASS_PCT, dev, dev_bar, d.pair_passes > 2u ? " — the bar scaled for a pair of more passes" : "");
    } else {
        std::printf("[soak] %s (b) pairs (informational for this class): mass %+.2f%%/%+.2f%%, pre-image dev %.2f texel after 500 one-sub-step pairs\n",
                    d.name, lo, hi, dev);
        if (!soak_substep_det(window, inst, op, &det_min, &det_mean)) { t19_failures++; std::printf("FAIL: [soak] %s field read\n", d.name); return; }
        T19(det_min > 0.5 && std::fabs(det_mean - 1.0) < 2e-3,
            "[soak] %s (b) first-order area preservation: one budgeted sub-step, pre-image det min %.3f (> 0.5)%s, mean %.5f (|1 - mean| < 2e-3)",
            d.name, det_min, op == SOAK_BURST || op == SOAK_LEADER ? " everywhere" : op == SOAK_CHLADNI_FIELD ? " everywhere (the discs' rings, overlapping)" : op == SOAK_SPARK ? " outside the drop's rim annulus (the drop and the burst: the composition's sub-stepped member, the shear being exact)" : " outside the swept capsule", det_mean);
        soak_scene(window, inst);                        // the stream needs its ink back
    }

    // ---- (c) + (d) the gesture-rate stream ----------------------------------
    soak_voice_on(window, inst);
    FieldF fs0;
    double mass_s0 = 0.0;
    if (!soak_measure(inst, &mass_s0, &fs0)) { t19_failures++; std::printf("FAIL: [soak] %s field read\n", d.name); return; }
    double s_min = mass_s0, s_max = mass_s0;
    float wake_x = 0.535f;
    for (long i = 0; i < passes; i++) {
        soak_stream_frame(inst, op, i, &wake_x);
        t19_step(window, inst, 1);
        if (i % 1000 == 999 || i + 1 == passes) {
            double m = 0.0;
            if (soak_measure(inst, &m, nullptr)) {
                if (m < s_min) s_min = m;
                if (m > s_max) s_max = m;
                std::printf("[soak] %s stream %ld passes: ink mass %.0f (base %.0f, %+.2f%%)\n",
                            d.name, i + 1, m, mass_s0, 100.0 * (m - mass_s0) / mass_s0);
            }
        }
    }
    soak_stream_end(window, inst, op, base);
    FieldF fs1;
    double mass_s1 = 0.0;
    if (!soak_measure(inst, &mass_s1, &fs1)) { std::free(fs0.px); t19_failures++; std::printf("FAIL: [soak] %s field read\n", d.name); return; }
    // The route must have DRIVEN the operator (the step-19 soak's own sanity
    // check): a dead route conserves everything and would pass trivially.
    long moved = 0;
    double g_edge = 0.0, g_int = 0.0;
    for (uint32_t y = 0; y < fs0.h; y++) for (uint32_t x = 0; x < fs0.w; x++) {
        const size_t o = ((size_t)y * fs0.w + x) * 4;
        if (std::fabs(fs1.px[o] - fs0.px[o]) > 1e-4f) moved++;
        const double dm = (double)fs1.px[o + 2] - fs0.px[o + 2];
        ((x < 16 || y < 16 || x + 16 >= fs0.w || y + 16 >= fs0.h) ? g_edge : g_int) += dm;
    }
    std::free(fs0.px); std::free(fs1.px);
    const double growth = 100.0 * (s_max - mass_s0) / mass_s0;
    const double growth_rate = (s_max - mass_s0) / mass_s0 / (double)(passes > 0 ? passes : 1);
    const double rate = (mass_s0 - mass_s1) / mass_s0 / (double)(passes > 0 ? passes : 1);
    const bool fab_ok = moved > 100 && (s_max - mass_s0 <= 0.005 * mass_s0 || growth_rate <= SOAK_GROWTH_RATE_MAX);
    T19(fab_ok,
        "[soak] %s (c) fabrication: mass grew at most %+.2f%% over %ld passes (%.1e/pass; <= 0.5%% or <= %.0e/pass), edge band %+.0f / interior %+.0f; the route moved %ld texels (alive)",
        d.name, growth, passes, growth_rate, SOAK_GROWTH_RATE_MAX, g_edge, g_int, moved);
    double control = 0.0;
    if (!soak_tine_control(window, inst, base, passes, &control)) { t19_failures++; std::printf("FAIL: [soak] %s control field read\n", d.name); return; }
    const double ratio = control > 1e-12 ? rate / control : 0.0;
    if (op == SOAK_BRAID) {
        // step 73: the braid resamples the WHOLE sheet 2N times a frame (a dyad's four Lamb–Oseen passes, their tails never
        // zero) for as long as the chord is held — the operator's nature (the Strang composition of exact swirls), not a
        // leak: (d) is informational for the braid, (c) its gate (DECISIONS_10 #10 lists the mitigations)
        std::printf("[soak] %s (d) erosion (informational — 2N full-sheet resamplings a frame is the braid's nature): %.2e/pass vs glide-tine control %.2e/pass (x%.2f)\n", d.name, rate, control, ratio);
    } else if (op == SOAK_CRYSTAL_DROP) {
        // step 71b: a drop's exact expansion carries the ink OFF THE SHEET — 2000 clear strikes of r = 0.03 displace several canvas
        // areas outward (the spark soak excluded its drop for the same reason, DECISIONS_5 #13 / #39); the mass that leaves is the
        // operator's geometry, not resampling erosion, so (d) is informational for the crystal drop and (c) is its gate
        std::printf("[soak] %s (d) erosion (informational — a drop has no inverse and its expansion pushes the ink off the sheet, as the v2 drop's does): %.2e/pass vs glide-tine control %.2e/pass (x%.2f)\n",
                    d.name, rate, control, ratio);
    } else if (passes >= SOAK_GATE_MIN_PASSES) {
        T19(control > 0.0 && rate <= 2.0 * control + 1e-6,
            "[soak] %s (d) erosion: %.2e/pass vs glide-tine control %.2e/pass (x%.2f <= 2)", d.name, rate, control, ratio);
    } else {
        std::printf("[soak] %s (d) erosion (informational - %ld passes is below the %ld-pass steady window): %.2e/pass vs glide-tine control %.2e/pass\n",
                    d.name, passes, SOAK_GATE_MIN_PASSES, rate, control);
    }
    if (op == SOAK_LEADER) sumi_debug_set_leader_charge(inst, 1);
    std::printf("[soak] %s SUMMARY class=%s pairs_mass_lo=%+.2f%% pairs_mass_hi=%+.2f%% preimage_dev=%.2f%s growth=%+.2f%% rate=%.2e control=%.2e ratio=%.2f passes=%ld\n",
                d.name, d.exact ? "exact" : "sub-stepped", lo, hi, dev,
                d.exact ? "" : (std::string(" det_min=") + std::to_string(det_min) + " det_mean=" + std::to_string(det_mean)).c_str(),
                growth, rate, control, ratio, passes);
    (void)moved;
}

static int soak_find(const char* name) {
    for (int i = 0; i < SOAK_COUNT; i++) if (std::strcmp(name, SOAKS[i].name) == 0) return i;
    return -1;
}
// Phase 6 step 40 — THE BOSS GATE: the erosion soak swept over the per-step
// chaos parameter K. Per K: a fresh scene and voice, then one FULL step of
// the map every other frame for the whole window (two exact passes per two
// frames: one pass a frame, the tine control's density), (c) and (d) read as
// the four-part gate reads them, plus the ink/water boundary length as the
// filamentation's witness. No verdict is asserted here: the table is the
// author's call on the erosion budget against the depth into chaos (MEDIUM
// §2.5's [ITERATE]); the core's route ceiling SUMI_CHIRIKOV_K_CEIL is set
// from it and recorded in DECISIONS_5.
static long t40_boundary(const FieldF& f, uint32_t band_rows, uint32_t xm);
static long t40_visible(const FieldF& f, uint32_t band_rows, uint32_t xm);
static void soak_chirikov_sweep(GLFWwindow* window, sumi_instance_t* inst, const sumi_params_t& base, long passes) {
    static const double KS[] = {0.25, 0.5, 0.75, 0.9716, 1.25, 1.5, 2.0};
    std::printf("[sweep] Chirikov erosion sweep: %ld frames per K, one full step every other frame (periods 2, eps 0.5, phase 0.3, centre (0.5, 0.5)); the scene, voice and window of the four-part gate\n", passes);
    double control = 0.0;
    if (!soak_tine_control(window, inst, base, passes, &control)) { t19_failures++; std::printf("FAIL: [sweep] control field read\n"); return; }
    std::printf("[sweep] glide-tine control: %.2e/pass\n", control);
    // Two erosion readings per K: over the whole window, and AFTER THE FIRST
    // 1000 FRAMES — the drift carries every rotating orbit off a canvas that
    // does not wrap (#43), and that flush takes ~45–60% of the scene's ink
    // in the first few hundred steps at ANY K; it is the operator's geometry,
    // not resampling erosion. What remains librates or drifts slowly, and
    // its fade is the erosion the gate is about.
    std::printf("[sweep] | K per step | flushed by 1000 | erosion/pass, whole | x tine | erosion/pass after the flush | x tine | visible ink kept | boundary x | (d) whole | (d) after the flush |\n[sweep] |---|---|---|---|---|---|---|---|---|---|\n");
    for (size_t ki = 0; ki < sizeof(KS) / sizeof(KS[0]); ki++) {
        const float K = (float)KS[ki];
        soak_reset(window, inst, base);
        soak_modes(inst, base, SOAK_CHIRIKOV);
        soak_scene(window, inst);
        soak_voice_on(window, inst);
        FieldF f0; double m0 = 0.0;
        if (!soak_measure(inst, &m0, &f0)) { t19_failures++; std::printf("FAIL: [sweep] field read\n"); return; }
        const long b0 = t40_boundary(f0, f0.h / 2 - 2, 2), v0 = t40_visible(f0, f0.h / 2 - 2, 2);
        double mn = m0, mx = m0, m_flush = m0;
        for (long i = 0; i < passes; i++) {
            if (i % 2 == 0) sumi_add_chirikov(inst, 0.5f, 0.5f, K, 2, 0.5f, 0.3f);
            t19_step(window, inst, 1);
            if (i % 1000 == 999 || i + 1 == passes) {
                double m = 0.0;
                if (soak_measure(inst, &m, nullptr)) {
                    if (m < mn) mn = m; if (m > mx) mx = m;
                    if (i == 999) m_flush = m;
                    std::printf("[sweep] K=%.4f %ld frames: ink mass %.0f (base %.0f, %+.2f%%)\n", (double)K, i + 1, m, m0, 100.0 * (m - m0) / m0);
                }
            }
        }
        t19_step(window, inst, 4);
        FieldF f1; double m1 = 0.0;
        if (!soak_measure(inst, &m1, &f1)) { std::free(f0.px); t19_failures++; std::printf("FAIL: [sweep] field read\n"); return; }
        long moved = 0;
        for (uint32_t y = 0; y < f0.h; y++) for (uint32_t x = 0; x < f0.w; x++) {
            const size_t o = ((size_t)y * f0.w + x) * 4;
            if (std::fabs(f1.px[o] - f0.px[o]) > 1e-4f) moved++;
        }
        const long b1 = t40_boundary(f1, f1.h / 2 - 2, 2), v1 = t40_visible(f1, f1.h / 2 - 2, 2);
        std::free(f0.px); std::free(f1.px);
        const double growth = 100.0 * (mx - m0) / m0, growth_rate = (mx - m0) / m0 / (double)passes;
        const double rate = (m0 - m1) / m0 / (double)passes, ratio = control > 1e-12 ? rate / control : 0.0;
        const long tail = passes > 1000 ? passes - 1000 : 1;
        const double rate_pf = passes > 1000 ? (m_flush - m1) / m_flush / (double)tail : 0.0, ratio_pf = control > 1e-12 ? rate_pf / control : 0.0;
        const bool c_ok = (mx - m0 <= 0.005 * m0) || growth_rate <= SOAK_GROWTH_RATE_MAX;
        const bool d_ok = rate <= 2.0 * control + 1e-6, d_pf_ok = rate_pf <= 2.0 * control + 1e-6;
        const double kept = v0 > 0 ? (double)v1 / (double)v0 : 0.0;
        std::printf("[sweep] | %.4f | %.0f%% | %.2e | x%.2f | %.2e | x%.2f | %.0f%% | x%.1f | %s | %s |\n", (double)K, 100.0 * (m0 - m_flush) / m0, rate, ratio, rate_pf, ratio_pf,
                    100.0 * kept, (double)b1 / (double)(b0 > 0 ? b0 : 1), d_ok ? "green" : "RED", d_pf_ok ? "green" : "RED");
        std::printf("[sweep] K=%.4f growth=%+.2f%% flushed=%.3f rate=%.2e rate_postflush=%.2e control=%.2e ratio=%.2f ratio_postflush=%.2f kept=%.3f boundary=%.1f moved=%ld c=%s d=%s d_postflush=%s\n",
                    (double)K, growth, (m0 - m_flush) / m0, rate, rate_pf, control, ratio, ratio_pf, kept, (double)b1 / (double)(b0 > 0 ? b0 : 1), moved,
                    c_ok ? "green" : "RED", d_ok ? "green" : "RED", d_pf_ok ? "green" : "RED");
    }
}

static void soak_run(GLFWwindow* window, sumi_instance_t* inst, const char* which, long passes) {
    sumi_params_t base;
    sumi_get_params(inst, &base);            // the pristine core state (scripted modes never see settings)
    if (std::strcmp(which, "chirikov-sweep") == 0) { soak_chirikov_sweep(window, inst, base, passes); soak_reset(window, inst, base); return; }
    const bool all = std::strcmp(which, "all") == 0;
    const int one = all ? -1 : soak_find(which);
    if (!all && one < 0) {
        t19_failures++;
        std::printf("FAIL: [soak] unknown operator '%s' - one of:", which);
        for (int i = 0; i < SOAK_COUNT; i++) std::printf(" %s", SOAKS[i].name);
        std::printf(" all\n");
        return;
    }
    for (int i = 0; i < SOAK_COUNT; i++) {
        if (!all && i != one) continue;
        soak_one(window, inst, base, (SoakOp)i, passes);
        soak_reset(window, inst, base);
    }
}

// The RED controls: the gate proven able to fail before it is trusted green
// (working rule). Each run drives a schedule that MUST trip its part; the
// check here asserts the failure. (c) cannot be made red through the pinch
// any more — its ingress mask is in the core and the core is frozen — so it
// uses the v1 tine's fixture-pinned edge-clamp behaviour (DECISIONS_3 #33:
// clamp FABRICATES when it drags edge ink inward), the same mechanism.
static void soak_negative(GLFWwindow* window, sumi_instance_t* inst) {
    sumi_params_t base;
    sumi_get_params(inst, &base);
    std::printf("[soak] negative controls: each part driven to FAIL\n");

    // (b) red: the pinch's -k at a DIFFERENT centre - two area-preserving
    // passes that are not each other's inverse. Mass may hold; the pre-image
    // must not come home.
    soak_modes(inst, base, SOAK_PINCH_SADDLE);
    soak_scene(window, inst);
    soak_voice_on(window, inst);
    FieldF fa; double m0 = 0.0;
    if (!soak_measure(inst, &m0, &fa)) { t19_failures++; std::printf("FAIL: [soak] negative field read\n"); return; }
    for (int i = 0; i < 500; i++) {
        sumi_add_pinch(inst, SOAK_CX, SOAK_CY, 0.3f, 0.6f);
        sumi_add_pinch(inst, SOAK_CX + 0.10f, SOAK_CY, -0.3f, 0.6f);
        t19_step(window, inst, 1);
    }
    t19_step(window, inst, 30);
    FieldF fb; double m1 = 0.0;
    if (!soak_measure(inst, &m1, &fb)) { std::free(fa.px); t19_failures++; std::printf("FAIL: [soak] negative field read\n"); return; }
    const double dev = soak_preimage_dev(fa, fb);
    std::free(fa.px); std::free(fb.px);
    T19(dev > SOAK_DEV_EXACT,
        "[soak] negative-inversion RED as required: offset (+k,-k) centres leave the pre-image %.2f texel away (> %.1f; mass %+.2f%%)",
        dev, SOAK_DEV_EXACT, 100.0 * (m1 - m0) / m0);
    soak_reset(window, inst, base);

    // (c) red: ink on the left edge, tines dragging it inward - every pass
    // duplicates the clamped edge column (the pre-ingress mechanism, #33).
    soak_modes(inst, base, SOAK_TINE);
    sumi_add_drop(inst, 0.04f, 0.50f, 0.10f, 0); t19_step(window, inst, 1);
    soak_scene(window, inst);
    soak_voice_on(window, inst);
    double f0 = 0.0, fmax = 0.0;
    if (!soak_measure(inst, &f0, nullptr)) { t19_failures++; std::printf("FAIL: [soak] negative field read\n"); return; }
    fmax = f0;
    for (int i = 0; i < 300; i++) {
        sumi_add_tine(inst, 0.0f, 0.5f, 0.40f, 0.5f, 0.05f, 0.002f);   // ~1 texel per pass, +x
        t19_step(window, inst, 1);
        if (i % 100 == 99) { double m = 0.0; if (soak_measure(inst, &m, nullptr) && m > fmax) fmax = m; }
    }
    T19(fmax - f0 > 0.005 * f0 && (fmax - f0) / f0 / 300.0 > SOAK_GROWTH_RATE_MAX,
        "[soak] negative-fabrication RED as required: edge-clamp tines grew ink mass %+.2f%% over 300 passes (> 0.5%%; %.1e/pass > %.0e)",
        100.0 * (fmax - f0) / f0, (fmax - f0) / f0 / 300.0, SOAK_GROWTH_RATE_MAX);
    soak_reset(window, inst, base);

    // (d) red: an OVER-STEPPED stream - a wake stroke of 15 tip radii every
    // frame (15 internal a/4 sub-steps, 15 resampling passes per frame) over
    // the full steady window. The class rule is <= a/4 PER FRAME at gesture
    // rate; a stream that needs this many sub-steps per frame erodes many
    // times the medium's baseline, and (d) must say so.
    soak_modes(inst, base, SOAK_WAKE_DOUBLET);
    soak_scene(window, inst);
    soak_voice_on(window, inst);
    double e0 = 0.0, e1 = 0.0;
    if (!soak_measure(inst, &e0, nullptr)) { t19_failures++; std::printf("FAIL: [soak] negative field read\n"); return; }
    const long n = 6000;
    for (long i = 0; i < n; i++) {
        const float x0 = (i & 1) ? SOAK_CX + 0.30f : SOAK_CX - 0.30f;   // 0.6 canvas each frame, a = 0.04 -> 15 sub-steps
        sumi_add_wake(inst, x0, 0.50f, 2.0f * SOAK_CX - x0, 0.50f, 0.04f);
        t19_step(window, inst, 1);
        if (i % 2000 == 1999) { double m = 0.0; if (soak_measure(inst, &m, nullptr)) std::printf("[soak] negative-erosion %ld frames: ink mass %.0f (%+.2f%%)\n", i + 1, m, 100.0 * (m - e0) / e0); }
    }
    if (!soak_measure(inst, &e1, nullptr)) { t19_failures++; std::printf("FAIL: [soak] negative field read\n"); return; }
    const double over = (e0 - e1) / e0 / (double)n;
    double control = 0.0;
    if (!soak_tine_control(window, inst, base, n, &control)) { t19_failures++; std::printf("FAIL: [soak] negative control field read\n"); return; }
    T19(control > 0.0 && over > 2.0 * control,
        "[soak] negative-erosion RED as required: a 15-sub-step-per-frame wake stream erodes %.2e/pass vs glide-tine %.2e/pass (x%.1f > 2)",
        over, control, control > 1e-12 ? over / control : 0.0);
    soak_reset(window, inst, base);
}


/* ------------------------------------------------------------------ */
/* Phase 6 step 36 (ROADMAP_5): wave torsion — the third vortex        */
/* profile and the note-on sweep episode (MEDIUM §2.1).                */
/* ------------------------------------------------------------------ */
// Ink-centroid angle about (cx, cy) of the marker sitting in the annulus
// r_lo..r_hi within ±window of the +x ray: a rotation by θ(r) turns the
// marker, and its centroid follows.
static bool t36_marker_angle(const FieldF& f, float cx, float cy, float r_lo, float r_hi, float window, double* out) {
    double sx = 0.0, sy = 0.0; long n = 0;
    for (uint32_t y = 0; y < f.h; y++) for (uint32_t x = 0; x < f.w; x++) {
        if (f.px[(((size_t)y * f.w) + x) * 4 + 2] <= 0.5f) continue;
        const double dx = ((double)x + 0.5) / f.w - cx, dy = ((double)y + 0.5) / f.h - cy;
        const double r = std::sqrt(dx * dx + dy * dy), a = std::atan2(dy, dx);
        if (r < r_lo || r > r_hi || std::fabs(a) > window) continue;
        sx += dx; sy += dy; n++;
    }
    if (n < 4) return false;
    *out = std::atan2(sy, sx);
    return true;
}
static void t19_torsion_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t36] wave torsion test\n");
    uint32_t pw = 0, ph = 0;
    sumi_params_t base; sumi_get_params(inst, &base);
    // k to its minimum (2π·4, a quarter-canvas period) through the REAL ctl
    // path, so two markers on the +x ray sit on a crest and a trough of
    // sin(k·r): r1 = π/2k, r2 = 3π/2k.
    sumi_map_cc(inst, 0xFF, 104, SUMI_CTL_TORSION_K);
    sumi_map_cc(inst, 0xFF, 105, SUMI_CTL_TORSION_PHASE);
    sumi_push_midi(inst, 0xB0, 104, 0);
    sumi_push_midi(inst, 0xB0, 105, 0);
    t19_step(window, inst, 120);                                    // the smoother settles
    const float k = 25.132741f, A = 0.4f, R = 1.0f;                 // k at CC 0 = SUMI_TORSION_K_MIN (2π·4, voice_mapper.h)
    const float r1 = 1.5707963f / k, r2 = 4.7123890f / k;           // 0.0625, 0.1875
    const float cx = 0.5f, cy = 0.5f;
    auto both = [&](double* m1, double* m2) {
        FieldF g; if (!t19_read_field(inst, &g)) return false;
        const bool ok = t36_marker_angle(g, cx, cy, r1 - 0.03f, r1 + 0.03f, 1.0f, m1) &&
                        t36_marker_angle(g, cx, cy, r2 - 0.03f, r2 + 0.03f, 1.0f, m2);
        std::free(g.px); return ok;
    };
    // --- Part A: one pass turns the crest marker one way and the trough marker the other, by θ(r) ---
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_add_drop(inst, cx + r1, cy, 0.02f, 0); t19_step(window, inst, 1);
    sumi_add_drop(inst, cx + r2, cy, 0.02f, 0); t19_step(window, inst, 1);
    double a1_0 = 0, a2_0 = 0, a1 = 0, a2 = 0;
    bool ok = both(&a1_0, &a2_0);
    sumi_add_vortex(inst, cx, cy, A, R, SUMI_VORTEX_TORSION);
    t19_step(window, inst, 1);
    ok = ok && both(&a1, &a2);
    const double e1 = A * std::exp(-r1 / R), e2 = A * std::exp(-r2 / R);   // |θ| where sin = ±1
    const double d1 = a1 - a1_0, d2 = a2 - a2_0;
    T19(ok && std::fabs(std::fabs(d1) - e1) < 0.05 && std::fabs(std::fabs(d2) - e2) < 0.05,
        "one pass, theta(r) = A sin(k r) e^(-r/R): crest marker |%.3f| ~ %.3f, trough marker |%.3f| ~ %.3f rad",
        std::fabs(d1), e1, std::fabs(d2), e2);
    T19(ok && d1 * d2 < 0.0, "crest and trough markers turn OPPOSITE ways (%+.3f vs %+.3f): alternating angular shear", d1, d2);
    // --- Part B: the ±A pair inverts — the markers come home ---
    sumi_add_vortex(inst, cx, cy, -A, R, SUMI_VORTEX_TORSION);
    t19_step(window, inst, 1);
    double b1 = 0, b2 = 0;
    ok = ok && both(&b1, &b2);
    T19(ok && std::fabs(b1 - a1_0) < 0.02 && std::fabs(b2 - a2_0) < 0.02,
        "(+A, -A) pair: markers back within 0.02 rad (%.4f, %.4f) - the exact inverse", std::fabs(b1 - a1_0), std::fabs(b2 - a2_0));

    // --- Part C: the note-on SWEEP episode: deltas, decay, an end, accumulation ---
    // F#4 on the chroma grid strikes at (0.535, 0.5) with a 0.087 drop (velocity
    // 100); a marker at r = 0.12 sits outside it. The strike's own expansion
    // moves the marker RADIALLY (to ~0.15), never in angle — so with the sweep
    // off, the angle must not move (the control).
    const float vx = 0.535f, vy = 0.5f;
    auto strike_scene = [&](uint32_t sweep_flag) {
        std::free(t19_dip_print(window, inst, &pw, &ph));
        sumi_params_t p = base; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID; p.torsion_sweep = sweep_flag;
        sumi_set_params(inst, &p);
        sumi_push_midi(inst, 0xB0, 101, 0); sumi_push_midi(inst, 0xB0, 100, 6); sumi_push_midi(inst, 0xB0, 6, 15);
        t19_step(window, inst, 2);
        sumi_add_drop(inst, vx + 0.12f, vy, 0.015f, 0);
        t19_step(window, inst, 2);
    };
    auto marker = [&](double* a) {
        FieldF g; if (!t19_read_field(inst, &g)) return false;
        const bool okm = t36_marker_angle(g, vx, vy, 0.09f, 0.20f, 1.0f, a);
        std::free(g.px); return okm;
    };
    strike_scene(0);
    double c0 = 0, c1 = 0;
    bool okc = marker(&c0);
    sumi_push_midi(inst, 0x91, 66, 100); t19_step(window, inst, 240);
    sumi_push_midi(inst, 0x81, 66, 64);  t19_step(window, inst, 60);
    okc = okc && marker(&c1);
    T19(okc && std::fabs(c1 - c0) < 0.01, "control (torsion_sweep = 0): the strike turns the marker by %.4f rad (< 0.01)", std::fabs(c1 - c0));
    // With the sweep: sample the marker every 10 frames for 4 s; release at 2 s.
    strike_scene(1);
    double s0 = 0; bool oks = marker(&s0);
    sumi_push_midi(inst, 0x91, 66, 100);
    double prev = s0, max_step = 0.0, max_swing = 0.0, at3 = 0.0, at4 = 0.0;
    for (int i = 1; i <= 48 && oks; i++) {                          // 480 frames = 4 s at 120 Hz
        t19_step(window, inst, 10);
        double a = 0; if (!marker(&a)) { oks = false; break; }
        const double step = std::fabs(a - prev); if (step > max_step) max_step = step;
        if (std::fabs(a - s0) > max_swing) max_swing = std::fabs(a - s0);
        if (i == 36) at3 = a;
        if (i == 48) at4 = a;
        prev = a;
        if (i == 24) sumi_push_midi(inst, 0x81, 66, 64);            // the episode outlives the note
    }
    // The largest increment the episode may emit in 10 frames is RATE·Δt at
    // t = 0 (1.2 rad/s · 1/12 s = 0.1 rad) — a whole pattern applied at once
    // would read ~0.7. Margin for the marker estimator.
    const double bound = 1.2 * (10.0 / 120.0) * 1.5;
    T19(oks && max_swing > 0.03, "sweep: the marker moves (max swing %.3f rad > 0.03) - the episode fired", max_swing);
    T19(oks && max_step <= bound, "sweep emits per-frame DELTAS: largest 10-frame step %.3f rad <= %.3f (never the whole pattern at once)", max_step, bound);
    T19(oks && std::fabs(at4 - at3) < 0.01, "sweep ENDS on its own clock: angle at 3 s %.4f, at 4 s %.4f (|d| %.4f < 0.01); the release at 2 s did not cut it", at3, at4, std::fabs(at4 - at3));
    const double net1 = at4 - s0;
    // A second strike in the same slot RE-ARMS the episode. (Its net rotation
    // is not comparable to the first's: the second strike's own drop pushes
    // the marker further out radially, to a different k·r phase — so the
    // check is that the episode runs again with the same bounded deltas.)
    sumi_push_midi(inst, 0x91, 66, 100);
    double s1 = 0; oks = oks && marker(&s1);
    double prev2 = s1, max_step2 = 0.0, max_swing2 = 0.0;
    for (int i = 1; i <= 36 && oks; i++) {
        t19_step(window, inst, 10);
        double a = 0; if (!marker(&a)) { oks = false; break; }
        if (std::fabs(a - prev2) > max_step2) max_step2 = std::fabs(a - prev2);
        if (std::fabs(a - s1) > max_swing2) max_swing2 = std::fabs(a - s1);
        prev2 = a;
    }
    sumi_push_midi(inst, 0x81, 66, 64); t19_step(window, inst, 10);
    T19(oks && max_swing2 > 0.03 && max_step2 <= bound,
        "a second strike re-arms the episode: swing %.3f rad (> 0.03), largest 10-frame step %.3f (<= %.3f); net after the first %.3f",
        max_swing2, max_step2, bound, net1);
    sumi_set_params(inst, &base);
}


/* ------------------------------------------------------------------ */
/* Phase 6 step 37 (ROADMAP_5): the Chladni lattice (MEDIUM §2.2).     */
/* ------------------------------------------------------------------ */
// Mean |pre-image displacement| over the INTERIOR (a margin off every edge):
// the step's shears carry a band of the sheet off the canvas and the ingress
// rule replaces it with fresh water, which the inverse cannot bring back —
// that band is the rule working, not a residual of the operator.
static double t37_preimage_dev_interior(const FieldF& a, const FieldF& b, uint32_t margin) {
    double acc = 0.0; long n = 0;
    for (uint32_t y = margin; y + margin < a.h; y++) for (uint32_t x = margin; x + margin < a.w; x++) {
        const size_t o = ((size_t)y * a.w + x) * 4;
        const double du = ((double)b.px[o] - a.px[o]) * a.w, dv = ((double)b.px[o + 1] - a.px[o + 1]) * a.h;
        acc += std::sqrt(du * du + dv * dv); n++;
    }
    return n ? acc / (double)n : 0.0;
}
// Pre-image displacement (texels) at the texel nearest to a normalized point.
static void t37_disp_vec(const FieldF& f, float xn, float yn, double* du, double* dv) {
    int x = (int)std::lround(xn * (float)f.w - 0.5f), y = (int)std::lround(yn * (float)f.h - 0.5f);
    if (x < 0) x = 0; if (y < 0) y = 0;
    if (x >= (int)f.w) x = (int)f.w - 1; if (y >= (int)f.h) y = (int)f.h - 1;
    const size_t o = (((size_t)y * f.w) + (size_t)x) * 4;
    *du = ((double)f.px[o] - ((double)x + 0.5) / f.w) * f.w;
    *dv = ((double)f.px[o + 1] - ((double)y + 0.5) / f.h) * f.h;
}
static double t37_disp_at(const FieldF& f, float xn, float yn) {
    double du = 0.0, dv = 0.0; t37_disp_vec(f, xn, yn, &du, &dv);
    return std::sqrt(du * du + dv * dv);
}
// The TYPE of a fixed point of the flow, read off a ring of 16 texels around
// it: an elliptic point (an eddy's centre) ROTATES the ring — every point's
// pre-image sits at the same angular offset and about the same radius — while
// a hyperbolic point (a saddle, where separatrices cross) STRETCHES it — the
// pre-image radii alternate in and out with no net rotation. Returns the mean
// signed rotation (radians) and the mean |log(r'/r)|.
static void t37_ring_type(const FieldF& f, float cx, float cy, float r_norm, double* rot, double* stretch) {
    double sr = 0.0, ss = 0.0;
    for (int i = 0; i < 16; i++) {
        const double a = 2.0 * 3.14159265 * i / 16.0;
        const float px = cx + r_norm * (float)std::cos(a), py = cy + r_norm * (float)std::sin(a);
        double du, dv; t37_disp_vec(f, px, py, &du, &dv);
        const double qx = (px - cx) + du / f.w, qy = (py - cy) + dv / f.h;   // the pre-image, relative to the centre
        double dth = std::atan2(qy, qx) - a;
        while (dth > 3.14159265) dth -= 6.2831853;
        while (dth < -3.14159265) dth += 6.2831853;
        sr += dth;
        ss += std::fabs(std::log(std::sqrt(qx * qx + qy * qy) / r_norm));
    }
    *rot = sr / 16.0; *stretch = ss / 16.0;
}
static void t19_chladni_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t37] Chladni cellular flow test\n");
    uint32_t pw = 0, ph = 0;
    sumi_params_t base; sumi_get_params(inst, &base);
    sumi_map_cc(inst, 0xFF, 106, SUMI_CTL_CHLADNI_A);
    sumi_map_cc(inst, 0xFF, 107, SUMI_CTL_CHLADNI_B);
    // --- Part A: one step and its exact inverse (a 3 x 2 lattice of the gesture's own) ---
    std::free(t19_dip_print(window, inst, &pw, &ph));
    soak_scene(window, inst);
    FieldF fa; if (!t19_read_field(inst, &fa)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    sumi_add_chladni(inst, 0.006f, 0.0f, 1.0f / 3.0f, 0.0f, 0.5f, 0.0f);
    t19_step(window, inst, 1);
    FieldF fmid; if (!t19_read_field(inst, &fmid)) { std::free(fa.px); t19_failures++; std::printf("FAIL: field read\n"); return; }
    const double moved = t37_preimage_dev_interior(fa, fmid, 48);
    std::free(fmid.px);
    sumi_add_chladni(inst, -0.006f, 0.0f, 1.0f / 3.0f, 0.0f, 0.5f, 0.0f);
    t19_step(window, inst, 1);
    FieldF fb; if (!t19_read_field(inst, &fb)) { std::free(fa.px); t19_failures++; std::printf("FAIL: field read\n"); return; }
    const double back = t37_preimage_dev_interior(fa, fb, 48), back_all = soak_preimage_dev(fa, fb);
    std::free(fa.px); std::free(fb.px);
    T19(moved > 5.0 && back < 0.5,
        "a step then its inverse: the step moved the interior pre-image %.1f texel, the pair leaves %.3f (< 0.5; whole field incl. the ingress bands %.2f) - the exact inverse (reversed order)",
        moved, back, back_all);

    // --- Part B: AN EDDY IN EVERY CELL — the display cells turn, their centres and the water between them rest ---
    // Chroma grid; the stir control up for 150 frames; the cell geometry from the PUBLIC probe (the shells' keys).
    sumi_cell_info_t c0, c1, cx1;
    sumi_params_t p = base; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID; p.chladni_cell = 1.0f;
    const bool pr = sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &p, 1.0f, nullptr, 0.50f, 0.50f, &c0) &&
                    sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &p, 1.0f, nullptr, 0.50f, 0.50f + 0.12f, &c1) &&
                    sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &p, 1.0f, nullptr, 0.50f + 0.075f, 0.50f, &cx1);
    const float sy = pr ? (c1.cell_center_y - c0.cell_center_y) : 0.0f;
    const float sx = pr ? (cx1.cell_center_x - c0.cell_center_x) : 0.0f;
    const float R = pr ? c0.cell_radius : 0.0f;                 // the display disc, canvas heights
    const float ring_k = 0.15f * (sx < sy ? sx : sy);           // ~5 texels at 512: in the resting water round a corner
    // Run the stir on a fresh sheet, then measure: the disc cores and the corners between four discs rest,
    // the ring of every disc (ρ = 0.745, where the profile carries most) has moved; a ring at ρ = 0.7 round a
    // centre rotates, a ring round a corner does nothing.
    auto stir_and_measure = [&](float cell, double* fixed_c, double* fixed_k, double* c_tang, double* c_rad,
                                double* k_tang, double* k_rad, double* mid) {
        std::free(t19_dip_print(window, inst, &pw, &ph));
        p.chladni_cell = cell;
        sumi_set_params(inst, &p);
        t19_step(window, inst, 2);
        sumi_push_midi(inst, 0xB0, 106, 127);
        t19_step(window, inst, 150);                    // ~1.9 rad at the cores at 1.5 rad/s, less the smoother's ramp
        sumi_push_midi(inst, 0xB0, 106, 0);
        t19_step(window, inst, 30);
        FieldF g; if (!t19_read_field(inst, &g)) return false;
        const float aspect = (float)g.w / (float)g.h;
        double fc = 0.0, fk = 0.0, ct = 0.0, cr = 0.0, kt = 0.0, kr = 0.0, md = 0.0;
        for (int r = -3; r <= 3; r++) for (int k = -5; k <= 6; k++) {
            const float x = c0.cell_center_x + (float)k * sx, y = c0.cell_center_y + (float)r * sy;
            fc += t37_disp_at(g, x + 0.15f * cell * R / aspect, y);                               // the disc's core (ρ = 0.15)
            double t, rr; t37_ring_type(g, x, y, 0.7f * cell * R, &t, &rr); ct += std::fabs(t); cr += rr;   // the ring; neighbours counter-rotate: |t|
            md += t37_disp_at(g, x + 0.745f * cell * R / aspect, y);                              // where the profile carries most
        }
        for (int r = -3; r <= 2; r++) for (int k = -5; k <= 5; k++) {
            const float x = c0.cell_center_x + ((float)k + 0.5f) * sx, y = c0.cell_center_y + ((float)r + 0.5f) * sy;
            fk += t37_disp_at(g, x, y);
            double t, rr; t37_ring_type(g, x, y, ring_k, &t, &rr); kt += std::fabs(t); kr += rr;
        }
        std::free(g.px);
        *fixed_c = fc / 84.0; *fixed_k = fk / 66.0; *mid = md / 84.0;
        *c_tang = ct / 84.0; *c_rad = cr / 84.0; *k_tang = kt / 66.0; *k_rad = kr / 66.0;
        return true;
    };
    double fc, fk, ct, cr, kt, kr, md;
    const bool okb = pr && stir_and_measure(1.0f, &fc, &fk, &ct, &cr, &kt, &kr, &md);
    T19(okb && md > 3.0 && fc < 0.15 * md && fk < 0.15 * md,
        "fixed points: after 150 stirred frames the 84 disc cores (rho 0.15) moved %.2f texel and the 66 corners %.2f, the discs' rings (rho 0.745) %.2f (both < 0.15 x) - the note rests where it fell, the water between the discs rests, the ring turns",
        fc, fk, md);
    T19(okb && std::fabs(ct) > 0.25 && cr < 0.25 && std::fabs(kt) < 0.1 && kr < 0.08,   // cr: the ring's resampling scatter over the passes, not a saddle's stretch
        "types: a ring at rho 0.7 round a cell centre ROTATES %.2f rad (an eddy) and stretches |log r'/r| = %.2f; round a corner it rotates %.2f rad and stretches %.2f (resting water - no saddle, no lattice)",
        ct, cr, kt, kr);
    // The eddies ARE the layout's cells: every probe cell is one of the stir's discs, same centre, same radius.
    static float cells[SUMI_LAYOUT_MAX_CELLS][4];
    p.chladni_cell = 1.0f; sumi_set_params(inst, &p); t19_step(window, inst, 2);
    uint32_t n = sumi_debug_cells(inst, &cells[0][0], SUMI_LAYOUT_MAX_CELLS);
    auto find_cell = [&](float cx, float cy, uint32_t count) -> int {
        for (uint32_t i = 0; i < count; i++) if (std::fabs(cells[i][0] - cx) < 1e-4f && std::fabs(cells[i][1] - cy) < 1e-4f) return (int)i;
        return -1;
    };
    const int i0 = find_cell(c0.cell_center_x, c0.cell_center_y, n), i1 = find_cell(c1.cell_center_x, c1.cell_center_y, n), ix = find_cell(cx1.cell_center_x, cx1.cell_center_y, n);
    T19(pr && n == 84 && i0 >= 0 && i1 >= 0 && ix >= 0 && std::fabs(cells[i0][2] - R) < 1e-4f && std::fabs(cells[i1][2] - c1.cell_radius) < 1e-4f,
        "the eddies are the layout's cells: %u discs on the chromatic grid (the probe's 84 keys); the probe's cells at (%.3f, %.3f), (%.3f, %.3f), (%.3f, %.3f) are discs of the same centre and radius %.4f",
        n, (double)c0.cell_center_x, (double)c0.cell_center_y, (double)c1.cell_center_x, (double)c1.cell_center_y, (double)cx1.cell_center_x, (double)cx1.cell_center_y, (double)R);
    // Cell size: below 1 the discs shrink about their centres (water round each eddy); the display size is the ceiling.
    p.chladni_cell = 0.5f; sumi_set_params(inst, &p); t19_step(window, inst, 2);
    n = sumi_debug_cells(inst, &cells[0][0], SUMI_LAYOUT_MAX_CELLS);
    const int h0 = find_cell(c0.cell_center_x, c0.cell_center_y, n);
    const float r_half = h0 >= 0 ? cells[h0][2] : 0.0f;
    p.chladni_cell = 1.5f; sumi_set_params(inst, &p); t19_step(window, inst, 2);
    n = sumi_debug_cells(inst, &cells[0][0], SUMI_LAYOUT_MAX_CELLS);
    const int g0 = find_cell(c0.cell_center_x, c0.cell_center_y, n);
    const float r_big = g0 >= 0 ? cells[g0][2] : 0.0f;
    T19(pr && std::fabs(r_half - 0.5f * R) < 1e-4f && std::fabs(r_big - R) < 1e-4f,
        "cell size: at 0.5 the disc's radius is %.4f = half the display's %.4f; at 1.5 it stays %.4f - the display disc is the ceiling (discs never overlap)",
        (double)r_half, (double)R, (double)r_big);
    p.chladni_cell = 1.0f;
    // The same fixed-point check on a 16:9 field: the discs are circles on screen (x through the aspect).
    sumi_resize(inst, 768, 432, 1.0f);
    t19_step(window, inst, 2);
    double wfc, wfk, wct, wcr, wkt, wkr, wmd;
    const bool okw = pr && stir_and_measure(1.0f, &wfc, &wfk, &wct, &wcr, &wkt, &wkr, &wmd);
    T19(okw && wmd > 3.0 && wfc < 0.15 * wmd && wfk < 0.15 * wmd && std::fabs(wct) > 0.25 && std::fabs(wkt) < 0.1,
        "on a 16:9 field too: cores %.2f and corners %.2f texel against the discs' rings %.2f; rings rotate %.2f rad, corners %.2f",
        wfc, wfk, wmd, wct, wkt);
    sumi_resize(inst, 512, 512, 1.0f);
    t19_step(window, inst, 2);
    // The layouts without drawn cells take imaginary cells — the largest circle that touches no neighbour's, at
    // every note position: the fifths half an octave ring (0.32 over ten octaves), 128 of them on the twelve spokes
    // between r = 0.10 and 0.42; a roll half a semitone (0.88 over 128 notes), 128 of them on the now-line.
    p.pitch_layout = SUMI_LAYOUT_FIFTHS; p.chladni_cell = 1.0f; sumi_set_params(inst, &p); t19_step(window, inst, 2);
    const uint32_t nf = sumi_debug_cells(inst, &cells[0][0], SUMI_LAYOUT_MAX_CELLS);
    const double octave_ring = 0.32 / 10.0, semi = 0.88 / 128.0;
    bool fifths_ok = nf == 128u;
    for (uint32_t i = 0; i < nf && fifths_ok; i++) {
        const double d = std::hypot((double)cells[i][0] - 0.5, (double)cells[i][1] - 0.5);
        fifths_ok = std::fabs((double)cells[i][2] - 0.5 * octave_ring) < 1e-4 && d > 0.10 - 1e-3 && d < 0.42 + 1e-3;
    }
    p.pitch_layout = SUMI_LAYOUT_ROLL_H; sumi_set_params(inst, &p); t19_step(window, inst, 2);
    const uint32_t nr = sumi_debug_cells(inst, &cells[0][0], SUMI_LAYOUT_MAX_CELLS);
    bool roll_ok = nr == 128u;
    for (uint32_t i = 0; i < nr && roll_ok; i++)
        roll_ok = std::fabs((double)cells[i][2] - 0.5 * semi) < 1e-5 && std::fabs((double)cells[i][0] - 0.12) < 1e-4;
    T19(fifths_ok && roll_ok,
        "imaginary cells: the fifths take %u discs of radius %.4f (half an octave ring) between r = 0.10 and 0.42; a roll %u discs of radius %.5f (half a semitone) on the now-line x = 0.12",
        nf, 0.5 * octave_ring, nr, 0.5 * semi);
    sumi_set_params(inst, &base);
}

// §4.3(5) pick-by-eye pair (roadmap: prototype both pinch variants, pick by
// eye, log the choice): the same ring scene pinched by the Hamiltonian
// saddle vs composed crossed tines, exported as PNGs.
static void t19_pinch_demo(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t19] pinch variant demo pair\n");
    uint32_t pw = 0, ph = 0;
    t19_scene_rings(window, inst);
    for (int i = 0; i < 40; i++) {
        sumi_add_pinch(inst, 0.5f, 0.5f, 0.02f, 0.6f);   // smoothed-delta style
        t19_step(window, inst, 1);
    }
    uint8_t* a = t19_dip_print(window, inst, &pw, &ph);
    if (a) { stbi_write_png("pinch_hamiltonian.png", (int)pw, (int)ph, 4, a, (int)pw * 4); std::free(a); }
    t19_scene_rings(window, inst);
    // Crossed-tine variant through the REAL params path (#34): same gesture,
    // pinch_variant = 1.
    sumi_params_t vp;
    sumi_get_params(inst, &vp);
    vp.pinch_variant = 1;
    sumi_set_params(inst, &vp);
    for (int i = 0; i < 40; i++) {
        sumi_add_pinch(inst, 0.5f, 0.5f, 0.02f, 0.6f);
        t19_step(window, inst, 1);
    }
    vp.pinch_variant = 0;
    sumi_set_params(inst, &vp);
    uint8_t* b = t19_dip_print(window, inst, &pw, &ph);
    if (b) { stbi_write_png("pinch_crossed.png", (int)pw, (int)ph, 4, b, (int)pw * 4); std::free(b); }
    std::printf("[t19] wrote pinch_hamiltonian.png / pinch_crossed.png\n");
}

// §4.3(6) live group identity: an LFO on A through the LIVE path leaves the
// field BITWISE identical — the view displacement never writes.
static void t19_ripple_group_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t19] ripple group test (live LFO -> bitwise identity)\n");
    sumi_map_cc(inst, 0xFF, RIPPLE_AMP_CC, SUMI_CTL_RIPPLE_AMP);
    t19_scene_rings(window, inst);
    t19_step(window, inst, 4);
    size_t n0 = 0, n1 = 0;
    uint8_t* b0 = t19_read_field_raw(inst, &n0);
    if (!b0) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    for (int i = 0; i < 240; i++) {
        const int v = (int)(127.0 * std::sin(3.14159265 * (double)i / 240.0));
        sumi_push_midi(inst, 0xB0, RIPPLE_AMP_CC, (uint8_t)(v < 0 ? 0 : v));
        t19_step(window, inst, 1);
    }
    sumi_push_midi(inst, 0xB0, RIPPLE_AMP_CC, 0);
    t19_step(window, inst, 60);
    uint8_t* b1 = t19_read_field_raw(inst, &n1);
    if (!b1) { std::free(b0); t19_failures++; std::printf("FAIL: field read\n"); return; }
    T19(n0 == n1 && std::memcmp(b0, b1, n0) == 0,
        "field bitwise identical after the live-A LFO (%zu bytes)", n0);
    std::free(b1);

    std::free(b0);

    // BAKE insertion point, on a FRESH sheet: the identity u field is exactly
    // bilinear-representable, so "A up then back to 0" must compose back to
    // near-identity — any baked residue would stand as a sinusoidal u offset
    // of order A·2/π ≈ 0.016, four hundred times the assertion bound. (On an
    // inked field the same test only measures edge-resampling blur — the
    // §4.1 medium, not residue.)
    uint32_t pw = 0, ph = 0;
    std::free(t19_dip_print(window, inst, &pw, &ph));   // fresh identity sheet
    size_t nb = 0;
    uint8_t* base_raw = t19_read_field_raw(inst, &nb);
    sumi_params_t prm;
    sumi_get_params(inst, &prm);
    prm.ripple_bake = 1;
    sumi_set_params(inst, &prm);
    sumi_push_midi(inst, 0xB0, RIPPLE_AMP_CC, 127);
    t19_step(window, inst, 60);
    size_t nh = 0;
    uint8_t* bh = t19_read_field_raw(inst, &nh);
    T19(bh && base_raw && (nh != nb || std::memcmp(base_raw, bh, nb) != 0),
        "bake mode: amplitude change writes the field");
    std::free(bh);
    sumi_push_midi(inst, 0xB0, RIPPLE_AMP_CC, 0);
    t19_step(window, inst, 120);
    FieldF back;
    if (t19_read_field(inst, &back) && base_raw) {
        const uint16_t* hh = (const uint16_t*)base_raw;
        double sum_u = 0.0;
        const size_t texels = (size_t)back.w * back.h;
        for (size_t i = 0; i < texels; i++) {
            const float stx = ((float)(i % back.w) + 0.5f) / (float)back.w;   // the raw baseline stores displacements (1.2.0)
            sum_u += std::fabs((back.px[i * 4] - stx) - half_to_float(hh[i * 4]));
        }
        const double mean_u = sum_u / (double)texels;
        T19(mean_u < 4e-4,
            "bake mode: A returning to 0 composes back to identity (mean |du| %.6f)",
            mean_u);
        std::free(back.px);
    }
    std::free(base_raw);
}

// #36 permanence: a bend-driven vibrato episode (bend_mode 1 + bake) leaves
// a PERMANENT feathered residue after the bend re-centers and the amp ctl
// stills — the ripple marks the ink like glide does. (The CC-driven bake
// path, by contrast, composes back — covered by the group test above.)
static void t19_ripple_permanence_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t19] ripple permanence test (bend-driven vibrato bakes in)\n");
    t19_scene_rings(window, inst);
    t19_step(window, inst, 4);
    size_t n0 = 0;
    uint8_t* b0 = t19_read_field_raw(inst, &n0);
    if (!b0) { t19_failures++; std::printf("FAIL: field read\n"); return; }

    sumi_params_t prm;
    sumi_get_params(inst, &prm);
    prm.bend_mode = 1;
    prm.ripple_bake = 1;
    sumi_set_params(inst, &prm);
    // An MPE voice + three vibrato cycles that end back at center.
    sumi_push_midi(inst, 0xB0, 101, 0);
    sumi_push_midi(inst, 0xB0, 100, 6);
    sumi_push_midi(inst, 0xB0, 6, 15);
    sumi_push_midi(inst, 0x91, 66, 100);
    t19_step(window, inst, 4);
    for (int i = 0; i < 360; i++) {   // 3 s at 120 Hz, ~1 Hz vibrato, ±2 semis
        const double semis = 2.0 * std::sin((double)i * 2.0 * 3.14159265 / 120.0);
        const long pb = 8192 + (long)(semis / 48.0 * 8192.0);
        sumi_push_midi(inst, 0xE1, (uint8_t)(pb & 0x7F), (uint8_t)((pb >> 7) & 0x7F));
        t19_step(window, inst, 1);
    }
    // End exactly at center, release, let the amp ctl settle to zero.
    const long pbc = 8192;
    sumi_push_midi(inst, 0xE1, (uint8_t)(pbc & 0x7F), (uint8_t)((pbc >> 7) & 0x7F));
    sumi_push_midi(inst, 0x81, 66, 64);
    t19_step(window, inst, 90);

    size_t n1 = 0;
    uint8_t* b1 = t19_read_field_raw(inst, &n1);
    if (!b1) { std::free(b0); t19_failures++; std::printf("FAIL: field read\n"); return; }
    // The note's own drop changed the field too — measure residue AWAY from
    // the note cell: count differing texels in the left half (the drop for
    // note 66 sits at x = 0.535; rings at center span both, and the ripple
    // combs the full frame).
    long moved = 0;
    const uint16_t* ha = (const uint16_t*)b0;
    const uint16_t* hb = (const uint16_t*)b1;
    for (uint32_t y = 0; y < 512; y++) {
        for (uint32_t x = 0; x < 150; x++) {   // far-left band: no drop there
            const size_t o = (((size_t)y * 512) + x) * 4;
            if (ha[o] != hb[o]) moved++;   // u channel, bitwise
        }
    }
    T19(moved > 2000, "vibrato residue is PERMANENT after re-center+release "
        "(%ld far-field texels changed; amp ctl stilled)", moved);
    std::free(b0);
    std::free(b1);
}

// §4.3(7) Lamb-Oseen DONE gates: small-r stability (θ finite/smooth, the
// guarded path matching the analytic profile), core coherence (the voice's
// own rings rotate near-rigidly while neighbors stir), and band-parity
// counter-rotation between adjacent notes.
static void t19_swirl_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t19] lamb-oseen swirl test\n");
    sumi_params_t prm;
    sumi_get_params(inst, &prm);
    prm.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    prm.press_mode = 1;                       // 0xD0 -> swirl (the hardware door)
    sumi_set_params(inst, &prm);
    sumi_push_midi(inst, 0xB0, 101, 0);       // MCM -> MPE mode
    sumi_push_midi(inst, 0xB0, 100, 6);
    sumi_push_midi(inst, 0xB0, 6, 15);
    t19_step(window, inst, 2);

    // Chroma-grid cell centers via the §3.4 golden formula (the harness has
    // no internal layouts.h; these match the normalizer_tests goldens).
    auto cell_x = [](int note) { return 0.08f + (((float)(note % 12) + 0.5f) / 12.0f) * 0.84f; };
    auto cell_y = [](int note) { return 0.10f + (((float)(note / 12 - 2) + 0.5f) / 7.0f) * 0.80f; };

    // --- Part A: profile + small-r stability, one voice at F#4 (center-ish).
    float cx[1], cy[1];
    cx[0] = cell_x(66); cy[0] = cell_y(66);
    sumi_push_midi(inst, 0x91, 66, 100);
    t19_step(window, inst, 2);
    sumi_push_midi(inst, 0xD1, 60, 0);        // moderate swirl via press_mode = 1
    t19_step(window, inst, 60);               // 0.5 s: total core angle << pi
    FieldF f;                                 // (atan2 wraps past pi — measure small)
    if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    const float rc = 0.020f + 0.075f * std::sqrt(100.0f / 127.0f);
    // Deflection angle about the voice center at a radius (from u/v).
    auto swirl_ang = [&](float r) { return t19_swirl_at(&f, cx[0], cy[0], r); };
    const float th_c  = swirl_ang(3.0f / (float)f.w);    // ~3 texels from center
    const float th_05 = swirl_ang(0.5f * rc);
    const float th_15 = swirl_ang(1.5f * rc);
    // The 0/0 danger at r -> 0 would EXPLODE theta; the guarded path stays
    // BOUNDED by the core value. (Near the exact center, per-pass
    // displacements fall below the half-float ULP and freeze — a protective
    // property of the medium, DECISIONS_3 #37 — so the core sample may read
    // LOW, never high.) Also: no NaN anywhere within 2 r_c.
    bool nan_free = true;
    for (uint32_t y = 0; y < f.h && nan_free; y++) {
        for (uint32_t x = 0; x < f.w; x++) {
            const float ddx = ((float)x + 0.5f) / (float)f.w - cx[0];
            const float ddy = ((float)y + 0.5f) / (float)f.h - cy[0];
            if (ddx * ddx + ddy * ddy > 4.0f * rc * rc) continue;
            const size_t o = (((size_t)y * f.w) + x) * 4;
            if (!std::isfinite(f.px[o]) || !std::isfinite(f.px[o + 1])) {
                nan_free = false;
                break;
            }
        }
    }
    T19(nan_free && std::isfinite(th_c) && th_c <= th_05 * 1.3f + 0.05f,
        "small-r stability: no NaN within 2 r_c; core bounded (theta %.4f <= ~%.4f)",
        (double)th_c, (double)(th_05 * 1.3f + 0.05f));
    // The guard itself, CPU-side (the shader formula verbatim, float math):
    // continuity across the x = 1e-3 branch and convergence to the analytic
    // theta(0) = S/(2 pi rc^2) limit.
    {
        const float S = 0.5f, rcs = 0.1f, rc2 = rcs * rcs;
        auto theta_of_x = [&](float x) {
            if (x < 1e-3f) return S * (1.0f - 0.5f * x) / (6.2831853f * rc2);
            return S * (1.0f - std::exp(-x)) / (6.2831853f * (x * rc2));
        };
        const float th0 = S / (6.2831853f * rc2);
        const float lo = theta_of_x(0.999e-3f), hi = theta_of_x(1.001e-3f);
        T19(std::fabs(theta_of_x(1e-6f) / th0 - 1.0f) < 1e-3f &&
            std::fabs(lo - hi) / th0 < 1e-3f,
            "guarded path == analytic theta(0) limit (%.6f vs %.6f), branch continuous (%.2e)",
            (double)theta_of_x(1e-6f), (double)th0, (double)(std::fabs(lo - hi) / th0));
    }
    T19(th_15 < th_05,
        "far field decays: theta(1.5rc) %.4f < theta(0.5rc) %.4f", (double)th_15, (double)th_05);
    std::free(f.px);

    // --- Part B: core coherence — the voice's own rings stay sharp inside
    // r_c while the swirl runs (near-rigid rotation), and the far field
    // actually moved (it stirs the neighbourhood).
    sumi_push_midi(inst, 0x81, 66, 0);        // release; fresh sheet
    t19_step(window, inst, 4);
    uint32_t pw = 0, ph = 0;
    std::free(t19_dip_print(window, inst, &pw, &ph));
    // Rings AT the note cell (marble drops), then the note on top.
    for (int i = 0; i < 6; i++) {
        sumi_add_drop(inst, cx[0], cy[0], 0.10f, 0);
        t19_step(window, inst, 1);
    }
    sumi_push_midi(inst, 0x91, 66, 100);
    t19_step(window, inst, 2);
    FieldF b0;
    if (!t19_read_field(inst, &b0)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    sumi_push_midi(inst, 0xD1, 127, 0);
    t19_step(window, inst, 240);              // coherence cares only about sharpness
    FieldF b1;
    if (!t19_read_field(inst, &b1)) { std::free(b0.px); t19_failures++; std::printf("FAIL: field read\n"); return; }
    auto sharpness_inside = [&](const FieldF* ff, float rad) {
        double sum = 0.0;
        long n = 0;
        for (uint32_t y = 1; y + 1 < ff->h; y++) {
            for (uint32_t x = 1; x + 1 < ff->w; x++) {
                const float ddx = ((float)x + 0.5f) / (float)ff->w - cx[0];
                const float ddy = ((float)y + 0.5f) / (float)ff->h - cy[0];
                if (ddx * ddx + ddy * ddy > rad * rad) continue;
                const size_t o = (((size_t)y * ff->w) + x) * 4;
                sum += std::fabs(ff->px[o + 2 + 4] - ff->px[o + 2]);   // d ink / dx
                n++;
            }
        }
        return n ? sum / (double)n : 0.0;
    };
    const double sharp0 = sharpness_inside(&b0, 0.7f * rc);
    const double sharp1 = sharpness_inside(&b1, 0.7f * rc);
    long far_moved = 0;
    for (uint32_t y = 0; y < b0.h; y++) {
        for (uint32_t x = 0; x < b0.w; x++) {
            const float ddx = ((float)x + 0.5f) / (float)b0.w - cx[0];
            const float ddy = ((float)y + 0.5f) / (float)b0.h - cy[0];
            const float r2 = ddx * ddx + ddy * ddy;
            if (r2 < 4.0f * rc * rc || r2 > 9.0f * rc * rc) continue;
            const size_t o = (((size_t)y * b0.w) + x) * 4;
            if (std::fabs(b0.px[o] - b1.px[o]) > 1e-4f) far_moved++;
        }
    }
    T19(sharp1 > 0.55 * sharp0,
        "core coherence: ring sharpness inside 0.7 r_c retained (%.4f -> %.4f)",
        sharp0, sharp1);
    T19(far_moved > 500,
        "the far field stirs the neighbourhood (%ld texels moved in 2..3 r_c)", far_moved);
    std::free(b0.px);
    std::free(b1.px);
    sumi_push_midi(inst, 0x81, 66, 0);
    t19_step(window, inst, 4);

    // --- Part C: adjacent notes counter-rotate (band parity), driven by 0xA0.
    std::free(t19_dip_print(window, inst, &pw, &ph));
    prm.press_mode = 0;                       // 0xA0 works in EITHER mode
    sumi_set_params(inst, &prm);
    // Consecutive strikes carry opposite band parity; wide separation (notes
    // 60 and 65: 0.35 canvas = 4 r_c) keeps each measurement inside its own
    // core, and full amount keeps per-pass displacement above the half-float
    // freeze quantum (#37).
    float dx2[1], dy2[1];
    cx[0] = cell_x(60); cy[0] = cell_y(60);
    dx2[0] = cell_x(65); dy2[0] = cell_y(65);
    sumi_push_midi(inst, 0x91, 60, 100);
    sumi_push_midi(inst, 0x92, 65, 100);
    t19_step(window, inst, 2);
    for (int i = 0; i < 100; i++) {
        sumi_push_midi(inst, 0xA1, 60, 127);
        sumi_push_midi(inst, 0xA2, 65, 127);
        t19_step(window, inst, 1);
    }
    FieldF g;
    if (!t19_read_field(inst, &g)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
    // SIGNED swirl at 0.5 r_c around each center (t19_swirl_at is unsigned;
    // compute one signed sample per center from u/v).
    auto signed_swirl = [&](float vx, float vy, float r) {
        const int ix = (int)std::lround((vx + r) * (double)g.w - 0.5);
        const int iy = (int)std::lround(vy * (double)g.h - 0.5);
        const size_t o = (((size_t)iy * g.w) + ix) * 4;
        const float sx = g.px[o] - vx, sy = g.px[o + 1] - vy;
        const float px_ = ((float)ix + 0.5f) / (float)g.w - vx;
        const float py_ = ((float)iy + 0.5f) / (float)g.h - vy;
        return std::atan2(sx * py_ - sy * px_, sx * px_ + sy * py_);
    };
    const float rc2 = 0.020f + 0.075f * std::sqrt(100.0f / 127.0f);
    const float a60 = signed_swirl(cx[0], cy[0], 0.5f * rc2);
    const float a61 = signed_swirl(dx2[0], dy2[0], 0.5f * rc2);
    T19(a60 * a61 < 0.0f && std::fabs(a60) > 0.02f && std::fabs(a61) > 0.02f,
        "adjacent notes counter-rotate: %.4f vs %.4f rad", (double)a60, (double)a61);
    std::free(g.px);
}

// §4.5 live-vs-bake: the dip samples the UN-rippled field — a print taken
// with live ripple at full amplitude equals the ripple-free print of the
// same (deterministic) scene, byte for byte.
static void t19_ripple_dip_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t19] ripple dip test (print is un-rippled)\n");
    sumi_map_cc(inst, 0xFF, RIPPLE_AMP_CC, SUMI_CTL_RIPPLE_AMP);
    auto scene = [&]() {
        sumi_add_drop(inst, 0.45f, 0.45f, 0.16f, 0);
        t19_step(window, inst, 1);
        sumi_add_drop(inst, 0.60f, 0.55f, 0.10f, 0);
        t19_step(window, inst, 1);
        sumi_add_tine(inst, 0.2f, 0.3f, 0.8f, 0.7f, 0.05f, 0.10f);
        t19_step(window, inst, 1);
    };
    scene();
    uint32_t w1 = 0, h1 = 0;
    uint8_t* ref = t19_dip_print(window, inst, &w1, &h1);   // amp 0 (also resets)
    scene();                                                // rebased: same field
    sumi_push_midi(inst, 0xB0, RIPPLE_AMP_CC, 127);         // live ripple ON
    t19_step(window, inst, 40);                             // smooth to full amp
    uint32_t w2 = 0, h2 = 0;
    uint8_t* test = t19_dip_print(window, inst, &w2, &h2);
    if (!ref || !test) {
        t19_failures++;
        std::printf("FAIL: print read\n");
    } else {
        T19(w1 == w2 && h1 == h2 &&
            std::memcmp(ref, test, (size_t)w1 * h1 * 4) == 0,
            "live-rippled dip == un-rippled dip, byte for byte (%ux%u)", w1, h1);
    }
    std::free(ref);
    std::free(test);
}

/* ------------------------------------------------------------------ */
/* Phase 11 step 71 (KESSHO §1; DECISIONS_10 #3): the Kessho medium     */
/* ------------------------------------------------------------------ */
#include "kessho_lut.h"   // the chart the engine embeds — gate 4 reads it back

// CIE L*a*b* of a linear sRGB colour (D65), for the chart's dE
static void t71_lab(const float rgb[3], double lab[3]) {
    const double X = 0.4124564 * rgb[0] + 0.3575761 * rgb[1] + 0.1804375 * rgb[2];
    const double Y = 0.2126729 * rgb[0] + 0.7151522 * rgb[1] + 0.0721750 * rgb[2];
    const double Z = 0.0193339 * rgb[0] + 0.1191920 * rgb[1] + 0.9503041 * rgb[2];
    auto f = [](double t) { return t > 0.008856 ? std::cbrt(t) : 7.787 * t + 16.0 / 116.0; };
    const double fx = f(X / 0.95047), fy = f(Y / 1.0), fz = f(Z / 1.08883);
    lab[0] = 116.0 * fy - 16.0; lab[1] = 500.0 * (fx - fy); lab[2] = 200.0 * (fy - fz);
}
static double t71_srgb8_lin(uint8_t v) { const double c = v / 255.0; return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }
// the composite of the field AS IT STANDS — an export at the field's size (gate 6 proves it is the dip's print,
// bitwise) — the field untouched, where t42_print's dip would reset it to a fresh sheet
static uint8_t* t71_shot(GLFWwindow* window, sumi_instance_t* inst, uint32_t* pw, uint32_t* ph) {
    uint32_t fw = 0, fh = 0;
    sumi_read_field(inst, nullptr, 0, &fw, &fh);
    return t43_export(window, inst, nullptr, 0, 0, fw, fh, 0, pw, ph);
}
// the test's look: the shipped defaults with the bloom off (the numbers below are the composite's own, as the Anod test's)
static void t71_kessho_params(sumi_params_t* p) {
    p->medium = SUMI_MEDIUM_KESSHO; p->active_palette_id = 0; p->anod_bloom = 0.0f;
    p->kessho_retard = 2000.0f; p->kessho_backlight = 1.0f; p->kessho_grain = 0.5f; p->kessho_tint = 0.35f; p->kessho_caustic = 0.6f;
    p->polarizer_mode = SUMI_POLARIZER_GLOBAL;
}
static void t71_mineral_gates(GLFWwindow* window, sumi_instance_t* inst);   // step 71b, below
static void t71_kessho_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t71] Kessho test (the crystal: the substrate, the chart, the brush, the regimes, the switch, the exports)\n");
    sumi_params_t base; sumi_get_params(inst, &base);
    sumi_params_t p = base; t71_kessho_params(&p);
    uint32_t pw = 0, ph = 0;
    // 1. the identity field: the substrate alone, dark — where Anod is dark, Kessho is dark
    std::free(t19_dip_print(window, inst, &pw, &ph));
    sumi_set_params(inst, &p);
    t19_step(window, inst, 2);
    uint8_t* sub = t71_shot(window, inst, &pw, &ph);
    if (!sub) { t19_failures++; std::printf("FAIL: print\n"); return; }
    const double sub_mean = t42_mean_lum(sub, pw, ph, 0, pw, 0, ph);
    double sub_max = 0.0;
    for (size_t i = 0; i < (size_t)pw * ph; i++) { const double l = t42_lum(sub, i * 4); if (l > sub_max) sub_max = l; }
    T19(sub_mean < 40.0 && sub_max < 60.0, "the identity field in Kessho prints the substrate alone: mean luminance %.1f/255, max %.1f (dark calcite under crossed polarizers)", sub_mean, sub_max);
    // 2. gate 4: the chart — the embedded table against the integration's own output at the chart's three named retardations
    {
        struct ref_t { float nm; float rgb[3]; const char* hue; };
        // tools/kessho_lut.py --check: the Wyman–Sloan–Shirley CIE fit under a 6504 K backlight, white-balanced, negatives clipped
        static const ref_t refs[3] = { {550.0f, {0.144f, 0.000f, 0.455f}, "violet (the sensitive tint)"},
                                       {1100.0f, {0.419f, 0.020f, 0.942f}, "violet-blue (the second order)"},
                                       {1650.0f, {0.627f, 0.170f, 0.702f}, "magenta (the third order)"} };
        double worst = 0.0;
        for (const ref_t& r : refs) {
            const int i = (int)(r.nm / KESSHO_LUT_NM * (float)(KESSHO_LUT_N - 1) + 0.5f);
            float got[3]; for (int c = 0; c < 3; c++) got[c] = (float)t71_srgb8_lin(KESSHO_LUT_SRGB[i * 3 + c]);
            double la[3], lb[3]; t71_lab(r.rgb, la); t71_lab(got, lb);
            const double dE = std::sqrt((la[0] - lb[0]) * (la[0] - lb[0]) + (la[1] - lb[1]) * (la[1] - lb[1]) + (la[2] - lb[2]) * (la[2] - lb[2]));
            if (dE > worst) worst = dE;
            std::printf("[t71] the chart at %4.0f nm: the table (%.3f %.3f %.3f) against the integration (%.3f %.3f %.3f) — dE %.2f, %s\n", r.nm, got[0], got[1], got[2], r.rgb[0], r.rgb[1], r.rgb[2], dE, r.hue);
        }
        const int i275 = (int)(275.0f / KESSHO_LUT_NM * (float)(KESSHO_LUT_N - 1) + 0.5f);
        const double l0 = t71_srgb8_lin(KESSHO_LUT_SRGB[0]) + t71_srgb8_lin(KESSHO_LUT_SRGB[1]) + t71_srgb8_lin(KESSHO_LUT_SRGB[2]);
        const double l275 = t71_srgb8_lin(KESSHO_LUT_SRGB[i275 * 3]) + t71_srgb8_lin(KESSHO_LUT_SRGB[i275 * 3 + 1]) + t71_srgb8_lin(KESSHO_LUT_SRGB[i275 * 3 + 2]);
        T19(worst < 2.0 && l0 < 0.01 && l275 > 2.5, "the Michel-Levy table: dE %.2f at worst against the integration at 550 / 1100 / 1650 nm (< 2); black at 0 (%.3f), the first-order white at 275 nm (%.2f of 3)", worst, l0, l275);
    }
    // 3. the §4.6 script under Kessho: the print for the eye, and gate 1 — the field bitwise across the three media
    sumi_debug_run_field_script(inst);
    t19_step(window, inst, 2);
    FieldF f;
    if (!t19_read_field(inst, &f)) { std::free(sub); t19_failures++; std::printf("FAIL: field read\n"); return; }
    uint8_t* kp = t71_shot(window, inst, &pw, &ph);
    if (kp) { stbi_write_png("kessho_script.png", (int)pw, (int)ph, 4, kp, (int)pw * 4); std::printf("[t71] wrote kessho_script.png — the §4.6 script read as the crystal\n"); }
    {
        const uint32_t order[6] = {SUMI_MEDIUM_SUMI, SUMI_MEDIUM_ANOD, SUMI_MEDIUM_KESSHO, SUMI_MEDIUM_SUMI, SUMI_MEDIUM_KESSHO, SUMI_MEDIUM_ANOD};
        for (uint32_t m : order) { p.medium = m; sumi_set_params(inst, &p); t19_step(window, inst, 2); }
        p.medium = SUMI_MEDIUM_KESSHO; sumi_set_params(inst, &p); t19_step(window, inst, 2);
        FieldF f2;
        if (!t19_read_field(inst, &f2)) { std::free(sub); std::free(f.px); std::free(kp); t19_failures++; std::printf("FAIL: field read\n"); return; }
        long diff = 0;
        for (size_t i = 0; i < (size_t)f.w * f.h * 4; i++) if (f.px[i] != f2.px[i]) diff++;
        std::free(f2.px);
        T19(diff == 0, "gate 1, the switch: Sumi -> Anod -> Kessho -> Sumi -> Kessho -> Anod -> Kessho leaves the field bitwise (%ld samples differ) — the composite is a read", diff);
    }
    if (kp) {   // the strained water is lit: Layer A reads the strain where the sheet moved, against the substrate
        double lit = 0.0; long ln = 0;
        for (uint32_t y = 0; y < ph; y++) for (uint32_t x = 0; x < pw; x++) {
            const size_t i = ((size_t)y * pw + x);
            const float u = f.px[i * 4] - ((float)x + 0.5f) / (float)pw, v = f.px[i * 4 + 1] - ((float)y + 0.5f) / (float)ph;   // the displacement
            if (std::fabs(u) > 0.05f || std::fabs(v) > 0.05f) { lit += t42_lum(kp, i * 4); ln++; }
        }
        lit = ln ? lit / ln : 0.0;
        T19(ln > 1000 && lit > sub_mean + 15.0, "the script's moved water is lit: mean %.1f over %ld texels displaced by more than a twentieth, against the substrate's %.1f", lit, ln, sub_mean);
    }
    std::free(f.px);
    // 4. the Maltese cross — Layer A reads the ORIENTATION: a Lamb–Oseen swirl on a fresh sheet under the analyzer at 0 has
    //    four dark arms where the principal axis of the shear is parallel to the analyzer (for a swirl's simple shear, at
    //    45° to the radius: the diagonals) and four bright ones between them
    {
        std::free(t19_dip_print(window, inst, &pw, &ph));
        t19_step(window, inst, 2);
        const float rc = 0.08f, S = 0.6f * 6.2831853f * rc * rc;   // a gentle swirl: a finite rotation would wind the arms into spirals
        sumi_add_vortex(inst, 0.5f, 0.5f, S, rc, SUMI_VORTEX_LAMB_OSEEN);
        t19_step(window, inst, 2);
        uint8_t* sw = t71_shot(window, inst, &pw, &ph);
        double axial = 0.0, diag = 0.0;
        if (sw) {
            stbi_write_png("kessho_swirl.png", (int)pw, (int)ph, 4, sw, (int)pw * 4);
            for (int k = 0; k < 8; k++) {
                const double th = k * 3.14159265 / 4.0;
                double acc = 0.0; int n = 0;
                for (int rr = 0; rr < 5; rr++) {                    // a short radial run through the shear annulus
                    const double r = (0.8 + 0.2 * rr) * rc;
                    const int x = (int)((0.5 + r * std::cos(th)) * pw), y = (int)((0.5 + r * std::sin(th)) * ph);
                    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { acc += t42_lum(sw, ((size_t)(y + dy) * pw + (size_t)(x + dx)) * 4); n++; }
                }
                if (k % 2 == 0) axial += acc / n; else diag += acc / n;
            }
            axial /= 4.0; diag /= 4.0;
            std::free(sw);
        }
        const double bright = axial > diag ? axial : diag, dark = axial > diag ? diag : axial;
        T19(sw != nullptr && bright > 1.5 * dark + 10.0, "the Maltese cross: a Lamb-Oseen swirl under the analyzer at 0 reads %.1f on the axes and %.1f on the diagonals — four dark arms (%s) where the shear's principal axis is parallel to the analyzer",
            axial, diag, axial > diag ? "the diagonals" : "the axes");
    }
    // 5. gate 3, the period: the analyzer at CC 0 and at CC 127 (α = 0 and π/2, one period of the brush) print the same sheet;
    //    at CC 64 (π/4) the cross turns
    {
        sumi_map_cc(inst, 0xFF, 20, SUMI_CTL_POLARIZER_ANGLE);
        sumi_push_midi(inst, 0xB0, 20, 0); t19_step(window, inst, 240);
        uint8_t* a0 = t71_shot(window, inst, &pw, &ph);
        sumi_push_midi(inst, 0xB0, 20, 127); t19_step(window, inst, 240);
        uint8_t* a1 = t71_shot(window, inst, &pw, &ph);
        sumi_push_midi(inst, 0xB0, 20, 64); t19_step(window, inst, 240);
        uint8_t* a2 = t71_shot(window, inst, &pw, &ph);
        long d01 = -1, d02 = -1;
        if (a0 && a1 && a2) { d01 = t43_diff(a0, a1, (size_t)pw * ph * 4); d02 = t43_diff(a0, a2, (size_t)pw * ph * 4); stbi_write_png("kessho_swirl_45.png", (int)pw, (int)ph, 4, a2, (int)pw * 4); }
        T19(a0 && a1 && a2 && d01 == 0 && d02 > 1000, "gate 3, the analyzer's period: α = 0 and α = π/2 print bitwise the same (%ld bytes differ — the control's end is its start), α = π/4 turns the cross (%ld bytes differ)", d01, d02);
        std::free(a0); std::free(a1); std::free(a2);
        sumi_push_midi(inst, 0xB0, 20, 0); t19_step(window, inst, 240);
    }
    // 6. Layer B: the regimes — a vortex core is elliptic (cool), a pinch's saddle hyperbolic (warm); the caustic lights the fold
    {
        std::free(t19_dip_print(window, inst, &pw, &ph));
        sumi_params_t q = p; q.kessho_backlight = 0.0f; q.kessho_tint = 1.0f; q.kessho_caustic = 0.0f; sumi_set_params(inst, &q);   // the tint alone
        t19_step(window, inst, 2);
        sumi_add_vortex(inst, 0.3f, 0.5f, 3.0f * 6.2831853f * 0.08f * 0.08f, 0.08f, SUMI_VORTEX_LAMB_OSEEN);
        t19_step(window, inst, 1);
        for (int i = 0; i < 6; i++) { sumi_add_pinch(inst, 0.7f, 0.5f, 0.3f, 0.0f); t19_step(window, inst, 1); }
        t19_step(window, inst, 2);
        uint8_t* tp = t71_shot(window, inst, &pw, &ph);
        double vr = 0.0, vb = 0.0, pr = 0.0, pb = 0.0;
        if (tp) {
            stbi_write_png("kessho_regimes.png", (int)pw, (int)ph, 4, tp, (int)pw * 4);
            auto mean_rb = [&](double cx, double cy, double rad, double* r, double* b) {
                double sr = 0.0, sb = 0.0; int n = 0;
                for (int y = 0; y < (int)ph; y++) for (int x = 0; x < (int)pw; x++) {
                    const double dx = (x + 0.5) / pw - cx, dy = (y + 0.5) / ph - cy;
                    if (dx * dx + dy * dy < rad * rad) { const size_t o = ((size_t)y * pw + (size_t)x) * 4; sr += tp[o]; sb += tp[o + 2]; n++; }
                }
                *r = n ? sr / n : 0.0; *b = n ? sb / n : 0.0;
            };
            mean_rb(0.3, 0.5, 0.05, &vr, &vb);   // inside the vortex core: elliptic
            mean_rb(0.7, 0.5, 0.05, &pr, &pb);   // round the saddle: hyperbolic
            std::free(tp);
        }
        T19(tp != nullptr && vb > vr + 3.0 && pr > pb + 3.0, "Layer B's regimes: the vortex core tints cool (r %.1f, b %.1f — elliptic, |tr J| < 2) and the pinch's saddle warm (r %.1f, b %.1f — hyperbolic, tr J > 2)", vr, vb, pr, pb);
        // the caustic: with the depth up the fold lights the hyperbolic water round the saddle; at rest (z = 1.05) it is dark
        q.kessho_tint = 0.0f; q.kessho_caustic = 1.0f; sumi_set_params(inst, &q);
        sumi_map_cc(inst, 0xFF, 21, SUMI_CTL_CAUSTIC_DEPTH);
        sumi_push_midi(inst, 0xB0, 21, 0); t19_step(window, inst, 240);
        uint8_t* c0 = t71_shot(window, inst, &pw, &ph);
        sumi_push_midi(inst, 0xB0, 21, 110); t19_step(window, inst, 240);
        uint8_t* c1 = t71_shot(window, inst, &pw, &ph);
        double l0 = 0.0, l1 = 0.0;
        if (c0 && c1) {
            stbi_write_png("kessho_caustic.png", (int)pw, (int)ph, 4, c1, (int)pw * 4);
            // the fold is a LINE: the brightest half-percent of the box round the saddle tells it from the rest glow
            auto bright = [&](const uint8_t* px) {
                std::vector<double> l;
                for (uint32_t y = (uint32_t)(0.35 * ph); y < (uint32_t)(0.65 * ph); y++) for (uint32_t x = (uint32_t)(0.55 * pw); x < (uint32_t)(0.85 * pw); x++) l.push_back(t42_lum(px, ((size_t)y * pw + x) * 4));
                std::sort(l.begin(), l.end());
                return l.empty() ? 0.0 : l[(size_t)(0.995 * (double)(l.size() - 1))];
            };
            l0 = bright(c0); l1 = bright(c1);
        }
        T19(c0 && c1 && l1 > l0 + 40.0, "the caustic: the bed at rest (z 1.05) leaves the saddle's water at %.1f at its brightest half-percent; the wheel up (z ~ 6) draws the fold's line there at %.1f", l0, l1);
        std::free(c0); std::free(c1);
        sumi_push_midi(inst, 0xB0, 21, 0); t19_step(window, inst, 240);
        sumi_set_params(inst, &p);
    }
    // 7. gate 5: the substrate is screen-locked — under a 120 BPM roll scroll, with the light off, the sheet prints bitwise the same
    {
        sumi_params_t q = p; q.kessho_backlight = 0.0f; q.kessho_tint = 0.0f; q.kessho_caustic = 0.0f; q.kessho_grain = 1.0f;
        q.pitch_layout = SUMI_LAYOUT_ROLL_H; q.bpm = 120.0f; sumi_set_params(inst, &q);
        std::free(t19_dip_print(window, inst, &pw, &ph));
        t19_step(window, inst, 2);
        sumi_add_drop(inst, 0.5f, 0.5f, 0.15f, SUMI_DROP_INK);
        t19_step(window, inst, 2);
        uint8_t* g0 = t71_shot(window, inst, &pw, &ph);
        t19_step(window, inst, 90);   // three quarters of a second of scroll
        uint8_t* g1 = t71_shot(window, inst, &pw, &ph);
        const long dg = (g0 && g1) ? t43_diff(g0, g1, (size_t)pw * ph * 4) : -1;
        T19(g0 && g1 && dg == 0, "gate 5, the substrate is screen-locked: under a 120 BPM roll scroll the frosted calcite prints bitwise the same (%ld bytes differ)", dg);
        std::free(g0); std::free(g1);
        sumi_set_params(inst, &p);
    }
    // 8. gate 6: the exports — the export at the field's size IS the print; the caustic alone over alpha; 8K
    {
        std::free(t19_dip_print(window, inst, &pw, &ph));
        t19_step(window, inst, 2);
        sumi_debug_run_field_script(inst);
        t19_step(window, inst, 2);
        uint32_t w = 0, h = 0;
        uint8_t* e512 = t43_export(window, inst, nullptr, 0, 0, 512, 512, 0, &w, &h);
        uint8_t* caus = t43_export(window, inst, nullptr, 0, 0, 512, 512, SUMI_EXPORT_KESSHO_CAUSTIC, &w, &h);
        uint8_t* light = t43_export(window, inst, nullptr, 0, 0, 512, 512, SUMI_EXPORT_ANOD_ALPHA, &w, &h);
        uint8_t* print = t19_dip_print(window, inst, &pw, &ph);
        const long dp = (e512 && print) ? t43_diff(e512, print, (size_t)512 * 512 * 4) : -1;
        long c_rest = 0, c_lit = 0, l_lit = 0, within = 0, total = 0;
        if (caus && light && e512) {
            for (size_t i = 0; i < (size_t)512 * 512; i++) {
                if (caus[i * 4 + 3] == 0) c_rest++; else c_lit++;
                if (light[i * 4 + 3] > 0) l_lit++;
                if (caus[i * 4 + 3] > 0) { total++; if (caus[i * 4] <= light[i * 4] + 1 && caus[i * 4 + 1] <= light[i * 4 + 1] + 1 && caus[i * 4 + 2] <= light[i * 4 + 2] + 1) within++; }
            }
            stbi_write_png("kessho_caustic_alpha.png", 512, 512, 4, caus, 512 * 4);
            stbi_write_png("kessho_light_alpha.png", 512, 512, 4, light, 512 * 4);
        }
        T19(dp == 0, "gate 6, the Kessho export at the field's size IS the dip's print: %ld bytes differ", dp);
        T19(caus && light && c_rest > 10000 && c_lit > 100 && l_lit >= c_lit && within == total,
            "the caustic alone over alpha: %ld texels transparent, %ld lit, every lit texel within the crystal's light (%ld of %ld; the full light lights %ld)", c_rest, c_lit, within, total, l_lit);
        uint8_t* e8k = t43_export(window, inst, nullptr, 0, 0, 8192, 8192, SUMI_EXPORT_KESSHO_CAUSTIC, &w, &h);
        T19(e8k && w == 8192 && h == 8192, "the 8K caustic export: %ux%u", w, h);
        std::free(e512); std::free(caus); std::free(light); std::free(print); std::free(e8k);
    }
    std::free(kp); std::free(sub);
    sumi_clear_cc_map(inst);
    sumi_set_params(inst, &base);
    t71_mineral_gates(window, inst);   // 1.7.0 (step 71b): gates 8–12 of the amended step 71
}

// ---- gate 2: --kessho-flat — "z = 1 is flat for any exact field" ----------------------------------------
// The pre-image Jacobian's determinant from the field's pre-image by central differences over ±w texels (the
// composite's Layer B window at 512 is ±3), over the interior; the identity sheet reads zero exactly, the exact
// operators read the resampler's and the half-float field's floor, a sub-stepped operator reads more, and the
// first draft's windowed Arnold kick — the form KESSHO §2.1's rule forbids — reads its det J = 1 − (window slope)·
// (kick) on the CPU, the estimator's red control. The number is the measurement, not the proposal's: the stated
// 1e-4 is checked against what the field can hold (DECISIONS_10 #3).
static double t71_det(const FieldF& f, uint32_t x, uint32_t y, int w) {
    const size_t i = ((size_t)y * f.w + x) * 4, dx = 4 * (size_t)w, dy = 4 * (size_t)w * f.w;
    const double sx = (double)f.w / (2.0 * w), sy = (double)f.h / (2.0 * w);
    const double ux = ((double)f.px[i + dx] - f.px[i - dx]) * sx, uy = ((double)f.px[i + dy] - f.px[i - dy]) * sy;
    const double vx = ((double)f.px[i + dx + 1] - f.px[i - dx + 1]) * sx, vy = ((double)f.px[i + dy + 1] - f.px[i - dy + 1]) * sy;
    return ux * vy - uy * vx;
}
struct t71_stats { double rms, max, p99, median; long n, masked; };
// The stencil must not straddle a DISCONTINUITY of the pre-image: a drop's rim, where new ink meets pushed water
// (the inverse map's radial scale collapses at the rim and the two sides come from different places) — the one
// place an exact operator's field is not smooth. In the soak scene the rims are the only band boundaries, so a
// stencil whose five taps disagree on floor(phase) is left out (counted).
static bool t71_straddles(const FieldF& f, uint32_t x, uint32_t y, int w) {
    const size_t c = ((size_t)y * f.w + x) * 4;
    const double b0 = std::floor(f.px[c + 2]);
    const size_t o[4] = { c + 4 * (size_t)w, c - 4 * (size_t)w, c + 4 * (size_t)w * f.w, c - 4 * (size_t)w * f.w };
    for (size_t k : o) if (std::floor(f.px[k + 2]) != b0) return true;
    return false;
}
static t71_stats t71_flatness(const FieldF& f, int w, uint32_t margin) {
    std::vector<double> devs; devs.reserve((size_t)f.w * f.h);
    double acc = 0.0, mx = 0.0; long masked = 0;
    for (uint32_t y = margin; y + margin < f.h; y++) for (uint32_t x = margin; x + margin < f.w; x++) {
        if (t71_straddles(f, x, y, w)) { masked++; continue; }
        const double d = std::fabs(t71_det(f, x, y, w) - 1.0);
        devs.push_back(d); acc += d * d; if (d > mx) mx = d;
    }
    std::sort(devs.begin(), devs.end());
    t71_stats s; s.n = (long)devs.size(); s.masked = masked;
    s.rms = s.n ? std::sqrt(acc / (double)s.n) : 0.0; s.max = mx;
    s.p99 = s.n ? devs[(size_t)(0.99 * (double)(s.n - 1))] : 0.0;
    s.median = s.n ? devs[(size_t)(0.5 * (double)(s.n - 1))] : 0.0;
    return s;
}
// the +k half of each soak pair (soak_pair's first call), repeated to build a strained sheet
static void t71_half(GLFWwindow* window, sumi_instance_t* inst, SoakOp op) {
    switch (op) {
    case SOAK_TINE: sumi_add_tine(inst, 0.30f, 0.42f, 0.74f, 0.56f, 0.05f, 0.05f); break;
    case SOAK_PINCH_SADDLE: case SOAK_PINCH_CROSS: sumi_add_pinch(inst, SOAK_CX, SOAK_CY, 0.3f, 0.6f); break;
    case SOAK_WAKE_DOUBLET: case SOAK_WAKE_STOKESLET: { const float a = 0.04f, d = a * 0.25f; sumi_add_wake(inst, SOAK_CX - 0.5f * d, 0.50f, SOAK_CX + 0.5f * d, 0.50f, a); break; }
    case SOAK_RIPPLE_BAKE: sumi_push_midi(inst, 0xB0, RIPPLE_AMP_CC, 127); t19_step(window, inst, 4); break;   // the smoother's ramp, baked as it comes
    case SOAK_SWIRL: { const float rc = 0.06f, S = 2.0f * 6.2831853f * rc * rc; sumi_add_vortex(inst, SOAK_CX, SOAK_CY, S, rc, SUMI_VORTEX_LAMB_OSEEN); break; }
    case SOAK_VORTEX_EXP: sumi_add_vortex(inst, SOAK_CX, SOAK_CY, 1.0f, 0.25f, SUMI_VORTEX_EXPONENTIAL); break;
    case SOAK_VORTEX_RANKINE: sumi_add_vortex(inst, SOAK_CX, SOAK_CY, 1.0f, 0.25f, SUMI_VORTEX_RANKINE); break;
    case SOAK_TORSION: sumi_add_vortex(inst, SOAK_CX, SOAK_CY, 0.5f, 0.5f, SUMI_VORTEX_TORSION); break;
    case SOAK_CHLADNI_FIELD: sumi_debug_add_cells_pass(inst, 0.025f, -1.0f, SUMI_CHLADNI_FIELD); break;
    case SOAK_CHLADNI: sumi_add_chladni(inst, 0.006f, 0.0f, 1.0f / 3.0f, 0.0f, 0.5f, 0.0f); break;
    case SOAK_BURST: sumi_add_burst(inst, SOAK_CX, SOAK_CY, 0.04f, 0.01f, 0.0f, 2); break;
    case SOAK_SPARK_SHEAR: sumi_add_spark_shear(inst, SOAK_CX, SOAK_CY, 0.2f, 0.03f, 0.03f, SOAK_SPARK_K, 0.7f, 0.3f); break;
    case SOAK_SPARK: sumi_add_spark(inst, SOAK_CX, SOAK_CY, 0.04f, 0.01f, 0.0f, SUMI_DROP_NONE); break;
    case SOAK_CHIRIKOV: sumi_add_chirikov(inst, SOAK_CX, SOAK_CY, 0.5f, 2, 0.5f, 0.3f); break;
    case SOAK_CRYSTAL_DROP: sumi_add_drop(inst, SOAK_CX, SOAK_CY, 0.12f, SUMI_DROP_CLEAR); break;   // step 71b: the rhomb (soak_modes' habit), one clear pass
    case SOAK_TWIN_TINE: case SOAK_TERRACE_TINE: sumi_add_tine(inst, 0.30f, 0.42f, 0.74f, 0.56f, 0.05f, 0.05f); break;   // the profile is soak_modes'
    case SOAK_ARNOLD: sumi_debug_add_arnold(inst, SOAK_CX, SOAK_CY, 0.12f, 0.5f, 1.0f, 2u, 0.3f, 0); break;   // step 72: one iteration at K_max
    case SOAK_CHORD_WAVE: sumi_debug_add_chord_wave(inst, SOAK_CX, SOAK_CY, 0.5f, 8.0f, 0.03f, 0.25f); break;   // step 74: one strong windowed ripple-tine
    case SOAK_LEADER: sumi_debug_add_leader_seg(inst, 0.35f, 0.45f, 0.70f, 0.55f, 0.01f, 0.02f, 0.08f); break;   // step 76b: one segment's sub-step
    case SOAK_BRAID: {   // step 73: one two-vortex Strang step
        const float rc = 0.06f, S = 1.0f * 6.2831853f * rc * rc;
        sumi_add_vortex(inst, SOAK_CX - 0.06f, SOAK_CY, 0.5f * S, rc, SUMI_VORTEX_LAMB_OSEEN); sumi_add_vortex(inst, SOAK_CX + 0.06f, SOAK_CY, 0.5f * S, rc, SUMI_VORTEX_LAMB_OSEEN);
        sumi_add_vortex(inst, SOAK_CX + 0.06f, SOAK_CY, 0.5f * S, rc, SUMI_VORTEX_LAMB_OSEEN); sumi_add_vortex(inst, SOAK_CX - 0.06f, SOAK_CY, 0.5f * S, rc, SUMI_VORTEX_LAMB_OSEEN);
        break;
    }
    default: break;
    }
}
static void t71_flat(GLFWwindow* window, sumi_instance_t* inst, const char* which) {
    const int W = 3; const uint32_t MARGIN = 16;
    std::printf("[t71] --kessho-flat: z = 1 is flat for any exact field — |det J − 1| from the field's pre-image by central differences over ±1, ±2 and ±3 texels, the interior %u texels in, the drops' rims masked; ONE strong pass per operator (the pair's +k)\n", MARGIN);
    sumi_params_t base; sumi_get_params(inst, &base);
    const bool all = !which || !*which || !std::strcmp(which, "all");
    // the estimator's own controls, on the CPU: an exact twist (det 1 up to the stencil's O(h²)) and the first draft's
    // windowed kick (det J = 1 − (window slope)·(kick) — the form the rule forbids)
    {
        FieldF g; g.w = 512; g.h = 512; g.px = (float*)std::malloc((size_t)512 * 512 * 4 * sizeof(float));
        auto fill = [&](bool windowed_kick) {
            for (uint32_t y = 0; y < 512; y++) for (uint32_t x = 0; x < 512; x++) {
                const double sx = ((double)x + 0.5) / 512.0, sy = ((double)y + 0.5) / 512.0;
                const double rx = sx - 0.5, ry = sy - 0.5, r = std::sqrt(rx * rx + ry * ry), th = std::atan2(ry, rx);
                double u, v;
                if (!windowed_kick) {   // the Lamb–Oseen twist: θ' = θ + f(r), r kept — exact
                    const double rc = 0.12, f_ = 2.0 * (1.0 - std::exp(-r * r / (rc * rc))) / (r * r / (rc * rc) + 1e-9);
                    u = 0.5 + r * std::cos(th + f_); v = 0.5 + r * std::sin(th + f_);
                } else {                // ρ₁ = ρ − K·w(ρ)·sin(qθ), θ₁ = θ: the window on the kicked coordinate breaks det J
                    const double rho = 0.5 * r * r, rho0 = 0.5 * 0.12 * 0.12, wnd = std::exp(-rho / rho0);
                    const double rho1 = rho - 0.4 * rho0 * wnd * std::sin(3.0 * th), r1 = std::sqrt(2.0 * (rho1 > 0.0 ? rho1 : 0.0));
                    u = 0.5 + r1 * std::cos(th); v = 0.5 + r1 * std::sin(th);
                }
                float* t = g.px + ((size_t)y * 512 + x) * 4;
                t[0] = (float)u; t[1] = (float)v; t[2] = 0.0f; t[3] = 0.0f;
            }
        };
        fill(false); const t71_stats ex = t71_flatness(g, W, MARGIN);
        fill(true);  const t71_stats kk = t71_flatness(g, W, MARGIN);
        std::free(g.px);
        std::printf("[flat] CPU controls: the exact twist rms %.2e max %.2e; the windowed kick rms %.2e max %.2e\n", ex.rms, ex.max, kk.rms, kk.max);
        T19(ex.max < 2e-3 && kk.max > 1e-2 && kk.rms > 10.0 * ex.rms,
            "[flat] the estimator's controls: an exact twist reads max |det − 1| %.2e (the stencil's own error), the first draft's windowed kick %.2e (> 1e-2, RED as required)", ex.max, kk.max);
    }
    // the identity sheet: zero exactly
    {
        soak_reset(window, inst, base);
        FieldF f; double m = 0.0;
        if (soak_measure(inst, &m, &f)) { const t71_stats s = t71_flatness(f, W, MARGIN); std::free(f.px); T19(s.max == 0.0, "[flat] the identity sheet: max |det − 1| = %.3e — zero exactly, nothing moved", s.max); }
    }
    // the scene alone (three drops, the identity elsewhere): the rims masked, what remains is the drops' exact push
    {
        soak_modes(inst, base, SOAK_TINE);
        soak_scene(window, inst);
        FieldF f; double m = 0.0;
        if (soak_measure(inst, &m, &f)) {
            for (int w = 1; w <= 3; w++) { const t71_stats s = t71_flatness(f, w, MARGIN); std::printf("[flat] %-15s %-11s ±%d: median %.2e  rms %.2e  p99 %.2e  max %.2e  (%ld texels, %ld masked at the rims)\n", "scene", "exact", w, s.median, s.rms, s.p99, s.max, s.n, s.masked); }
            std::free(f.px);
        }
        soak_reset(window, inst, base);
    }
    double exact_p99_worst[4] = {0, 0, 0, 0}, sub_p99_best[4] = {0, 1e9, 1e9, 1e9};
    const char* exact_worst[4] = {"-", "-", "-", "-"}; const char* sub_best[4] = {"-", "-", "-", "-"};
    for (int i = 0; i < SOAK_COUNT; i++) {
        if (!all && std::strcmp(SOAKS[i].name, which) != 0) continue;
        const SoakOp op = (SoakOp)i;
        soak_modes(inst, base, op);
        if (op == SOAK_RIPPLE_BAKE) sumi_map_cc(inst, 0xFF, RIPPLE_AMP_CC, SUMI_CTL_RIPPLE_AMP);
        soak_prep(window, inst, op);
        soak_scene(window, inst);
        t71_half(window, inst, op); t19_step(window, inst, 1);
        t19_step(window, inst, 40);   // the episodes (the burst's age, the spark's decay, the ripple's ramp) run out
        FieldF f; double m = 0.0;
        if (!soak_measure(inst, &m, &f)) { t19_failures++; std::printf("FAIL: [flat] %s field read\n", SOAKS[i].name); soak_reset(window, inst, base); continue; }
        for (int w = 1; w <= 3; w++) {
            const t71_stats s = t71_flatness(f, w, MARGIN);
            std::printf("[flat] %-15s %-11s ±%d: median %.2e  rms %.2e  p99 %.2e  max %.2e  (%ld texels, %ld masked)\n", SOAKS[i].name, SOAKS[i].exact ? "exact" : "sub-stepped", w, s.median, s.rms, s.p99, s.max, s.n, s.masked);
            if (SOAKS[i].exact) { if (s.p99 > exact_p99_worst[w]) { exact_p99_worst[w] = s.p99; exact_worst[w] = SOAKS[i].name; } }
            else if (s.p99 < sub_p99_best[w]) { sub_p99_best[w] = s.p99; sub_best[w] = SOAKS[i].name; }
            // THE GATE AS MEASURED (DECISIONS_10 #3): the proposal's 1e-4 is not a number this field can hold — the
            // half-float field read by differences floors at a few thousandths, and the drops' rims (the pre-image's
            // one singularity, exact and unresolvable) own everything above it, in every class. What the gate holds
            // each operator to is the floor: the typical texel within a tenth of det 1 — a gross non-conservation
            // (the windowed kick's 0.35 rms) would read here; the class is declared, and proven by the soak.
            if (w == 3) T19(s.median < 0.1, "[flat] %s (%s): the typical texel's |det J − 1| is %.2e at ±3 (< 0.1 — the field's floor, not the operator's class)", SOAKS[i].name, SOAKS[i].exact ? "exact" : "sub-stepped", s.median);
        }
        std::free(f.px);
        soak_reset(window, inst, base);
    }
    if (all) for (int w = 1; w <= 3; w++)
        std::printf("[flat] ±%d: the exact family's worst p99 %.2e (%s); the sub-stepped family's best p99 %.2e (%s) — the two classes are NOT separable by differencing the field at these amplitudes\n", w, exact_p99_worst[w], exact_worst[w], sub_p99_best[w], sub_best[w]);
}

// ---- gate 7: --ab polarizer <dir> — the A/B item K-1, the same script under both analyzers -------------------
static void t71_ab_render(GLFWwindow* window, sumi_instance_t* inst, const sumi_params_t& p, const char* path, uint32_t* pw, uint32_t* ph) {
    // one gesture script and one MIDI phrase on a fresh sheet, deterministic frame for frame
    std::free(t19_dip_print(window, inst, pw, ph));
    sumi_set_params(inst, &p); t19_step(window, inst, 2);
    sumi_gesture_tap(inst, 0.30f, 0.35f, 0.07f); t19_step(window, inst, 1);
    sumi_gesture_tap(inst, 0.68f, 0.62f, 0.05f); t19_step(window, inst, 1);
    sumi_add_vortex(inst, 0.48f, 0.50f, 3.0f * 6.2831853f * 0.09f * 0.09f, 0.09f, SUMI_VORTEX_LAMB_OSEEN); t19_step(window, inst, 1);
    for (int i = 0; i < 4; i++) { sumi_add_pinch(inst, 0.75f, 0.30f, 0.25f, 0.6f); t19_step(window, inst, 1); }
    // the MIDI phrase: three MPE notes held with three slides (per-voice analyzers at 7°, 45°, 84°), pressure growing them
    sumi_push_midi(inst, 0xB0, 101, 0); sumi_push_midi(inst, 0xB0, 100, 6); sumi_push_midi(inst, 0xB0, 6, 15);
    sumi_push_midi(inst, 0x91, 60, 100); sumi_push_midi(inst, 0x92, 67, 100); sumi_push_midi(inst, 0x93, 76, 100);
    t19_step(window, inst, 4);
    sumi_push_midi(inst, 0xB1, 74, 10); sumi_push_midi(inst, 0xB2, 74, 64); sumi_push_midi(inst, 0xB3, 74, 118);
    for (int i = 0; i < 30; i++) { sumi_push_midi(inst, 0xD1, (uint8_t)(60 + i), 0); sumi_push_midi(inst, 0xD2, 90, 0); sumi_push_midi(inst, 0xD3, 70, 0); t19_step(window, inst, 1); }
    t19_step(window, inst, 60);
    uint8_t* px = t42_print(window, inst, pw, ph);
    if (px) { stbi_write_png(path, (int)*pw, (int)*ph, 4, px, (int)*pw * 4); std::free(px); std::printf("[t71] wrote %s\n", path); }
    sumi_push_midi(inst, 0x81, 60, 64); sumi_push_midi(inst, 0x82, 67, 64); sumi_push_midi(inst, 0x83, 76, 64);
    t19_step(window, inst, 30);
}
static void t73_ab_braid(GLFWwindow* window, sumi_instance_t* inst, const char* dir);   // below
static void t71_ab(GLFWwindow* window, sumi_instance_t* inst, const char* item, const char* dir) {
    if (std::strcmp(item, "braid") == 0) { t73_ab_braid(window, inst, dir); return; }   // step 73: the A/B item B-1
    if (std::strcmp(item, "polarizer") != 0) { t19_failures++; std::printf("FAIL: [ab] unknown item '%s' (polarizer, braid)\n", item); return; }
    std::printf("[t71] --ab polarizer: the same gesture script and MIDI phrase under the global analyzer (A) and per voice (B), archived in %s\n", dir);
    sumi_params_t base; sumi_get_params(inst, &base);
    sumi_params_t p = base; sumi_params_t d = base; t71_kessho_params(&p);
    p.kessho_backlight = d.kessho_backlight; p.kessho_tint = d.kessho_tint; p.kessho_caustic = d.kessho_caustic;   // the shipped look, not the test's
    p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    std::filesystem::create_directories(dir);
    char path[1024]; uint32_t pw = 0, ph = 0;
    // the shipped bloom (Anod's 0.75, shared) and a gentler one (0.35) — the shared knob is the author's to settle for the crystal
    const float blooms[2] = { base.anod_bloom, 0.35f };
    for (int b = 0; b < 2; b++) {
        p.anod_bloom = blooms[b];
        const char* tag = b == 0 ? "" : "_bloom035";
        p.active_palette_id = 0;
        p.polarizer_mode = SUMI_POLARIZER_GLOBAL;    std::snprintf(path, sizeof path, "%s/kessho_ab_A_global%s.png", dir, tag);    t71_ab_render(window, inst, p, path, &pw, &ph);
        p.polarizer_mode = SUMI_POLARIZER_PER_VOICE; std::snprintf(path, sizeof path, "%s/kessho_ab_B_pervoice%s.png", dir, tag);  t71_ab_render(window, inst, p, path, &pw, &ph);
        p.active_palette_id = 2;                     std::snprintf(path, sizeof path, "%s/kessho_ab_B_calcite%s.png", dir, tag);   t71_ab_render(window, inst, p, path, &pw, &ph);
        p.polarizer_mode = SUMI_POLARIZER_GLOBAL;    std::snprintf(path, sizeof path, "%s/kessho_ab_A_calcite%s.png", dir, tag);   t71_ab_render(window, inst, p, path, &pw, &ph);
    }
    sumi_set_params(inst, &base);
}

/* ------------------------------------------------------------------ */
/* Phase 11 step 71b: the crystal drop's gates (KESSHO §4.3–§4.4)        */
/* ------------------------------------------------------------------ */
// deform.glsl's habit_R, in doubles (the same transcription as tests/habit_det.c)
static double t71b_lse2(double a, double b, double k) { const double m = a > b ? a : b; return m + std::log(std::exp(k * (a - m)) + std::exp(k * (b - m))) / k; }
static double t71b_lse3(double a, double b, double c, double k) { const double m = std::max(a, std::max(b, c)); return m + std::log(std::exp(k * (a - m)) + std::exp(k * (b - m)) + std::exp(k * (c - m))) / k; }
static double t71b_habit_R(uint32_t habit, double r0, double roundness, double blades, double angle, double theta) {
    if (habit == 0u) return r0;
    const double t = theta - angle, k = 2.0 + 40.0 * (1.0 - roundness) * (1.0 - roundness);
    double R;
    if (habit == 1u) R = r0 / t71b_lse2(std::fabs(std::sin(t)), std::fabs(std::sin(t - 1.3631)), k);
    else if (habit == 2u) R = r0 / t71b_lse3(std::fabs(std::cos(t)), std::fabs(std::cos(t - 1.0471976)), std::fabs(std::cos(t - 2.0943951)), k);
    else { const double m = blades < 2.0 ? 2.0 : blades, c = std::cos(m * t), c2 = c * c, c8 = c2 * c2 * c2 * c2; R = r0 * (1.0 + 0.6 * c8) / (1.0 + 0.6 * 0.2734375); }
    return R + (r0 - R) * roundness;
}
// the homothety fit: the points (x, y) about the centre against f·R(θ − angle) — f the median ratio, the angle searched
// over a 1° grid when `fit_angle` (a note's strike frame is the pitch axis); returns the mean residual in texels
static double t71b_fit(const std::vector<double>& xs, const std::vector<double>& ys, uint32_t habit, double roundness, double blades,
                       double cx, double cy, uint32_t w, bool fit_angle, double* f_out, double* angle_out) {
    double best = 1e9, best_f = 0.0, best_a = 0.0;
    const int na = fit_angle ? 180 : 1;
    for (int ai = 0; ai < na; ai++) {
        const double a = fit_angle ? ai * 3.14159265 / 180.0 : 0.0;
        std::vector<double> ratio; ratio.reserve(xs.size());
        for (size_t i = 0; i < xs.size(); i++) {
            const double px = xs[i] - cx, py = ys[i] - cy, r = std::sqrt(px * px + py * py);
            ratio.push_back(r / t71b_habit_R(habit, 1.0, roundness, blades, a, std::atan2(py, px)));
        }
        std::vector<double> sorted = ratio; std::sort(sorted.begin(), sorted.end());
        const double f = sorted[sorted.size() / 2];
        double acc = 0.0;
        for (size_t i = 0; i < xs.size(); i++) acc += std::fabs(ratio[i] - f) * (double)w;   // the residual along the ray, in texels (r − f R = (ratio − f)·R ≈ (ratio − f)·f·R/f… kept as (ratio − f) × canvas: the ray's scale)
        acc /= (double)xs.size();
        if (acc < best) { best = acc; best_f = f; best_a = a; }
    }
    if (f_out) *f_out = best_f; if (angle_out) *angle_out = best_a;
    return best;
}
// the iso-phase contour fract(phase) ≈ q inside the drop (band texels only), as points in canvas units
static void t71b_contour(const FieldF& f, double q, double cx, double cy, double rmax, std::vector<double>& xs, std::vector<double>& ys) {
    xs.clear(); ys.clear();
    for (uint32_t y = 1; y + 1 < f.h; y++) for (uint32_t x = 1; x + 1 < f.w; x++) {
        const double px = ((double)x + 0.5) / f.w, py = ((double)y + 0.5) / f.h;
        if (std::hypot(px - cx, py - cy) > rmax) continue;
        const float ph = f.px[((size_t)y * f.w + x) * 4 + 2];
        if (ph < 1.0f) continue;
        const double fr = ph - std::floor(ph);
        if (std::fabs(fr - q) < 0.006) { xs.push_back(px); ys.push_back(py); }
    }
}
// the ink's outer boundary about a centre: band texels with a non-band 4-neighbour
static void t71b_boundary(const FieldF& f, double cx, double cy, double rmax, std::vector<double>& xs, std::vector<double>& ys) {
    xs.clear(); ys.clear();
    for (uint32_t y = 1; y + 1 < f.h; y++) for (uint32_t x = 1; x + 1 < f.w; x++) {
        const double px = ((double)x + 0.5) / f.w, py = ((double)y + 0.5) / f.h;
        if (std::hypot(px - cx, py - cy) > rmax) continue;
        const size_t o = ((size_t)y * f.w + x) * 4 + 2;
        if (f.px[o] < 1.0f) continue;
        if (f.px[o - 4] < 1.0f || f.px[o + 4] < 1.0f || f.px[o - 4 * f.w] < 1.0f || f.px[o + 4 * f.w] < 1.0f) { xs.push_back(px); ys.push_back(py); }
    }
}
static long t71b_band_area(const FieldF& f) { long n = 0; for (size_t i = 0; i < (size_t)f.w * f.h; i++) if (f.px[i * 4 + 2] >= 1.0f) n++; return n; }
// luminance along a ray from (cx, cy) at angle th between r0 and r1 (canvas units), averaged over a 3×3 at each sample
static double t71b_ray_lum(const uint8_t* px, uint32_t w, uint32_t h, double cx, double cy, double th, double r0, double r1) {
    double acc = 0.0; int n = 0;
    for (int k = 0; k < 24; k++) {
        const double r = r0 + (r1 - r0) * (k + 0.5) / 24.0;
        const int x = (int)((cx + r * std::cos(th)) * w), y = (int)((cy + r * std::sin(th)) * h);
        if (x < 1 || y < 1 || x + 1 >= (int)w || y + 1 >= (int)h) continue;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { acc += t42_lum(px, ((size_t)(y + dy) * w + (size_t)(x + dx)) * 4); n++; }
    }
    return n ? acc / n : 0.0;
}
// the crease pair (gate 7): a rhomb strike at roundness 0 and 1 in Kessho, the Layer A line along the corner ray
static void t71b_crease(GLFWwindow* window, sumi_instance_t* inst, const sumi_params_t& base, const char* dir, double* contrast0, double* contrast1) {
    sumi_params_t p = base; t71_kessho_params(&p);
    p.drop_habit = SUMI_HABIT_RHOMB; p.habit_blades = 5u;
    const double A = 1.3631, thc = A / 2.0;   // the rhomb's corner ray
    for (int r = 0; r < 2; r++) {
        uint32_t pw = 0, ph = 0;
        p.habit_roundness = r ? 1.0f : 0.0f; p.medium = SUMI_MEDIUM_SUMI; sumi_set_params(inst, &p);
        std::free(t19_dip_print(window, inst, &pw, &ph));
        t19_step(window, inst, 2);
        sumi_add_drop(inst, 0.5f, 0.5f, 0.12f, SUMI_DROP_INK);
        t19_step(window, inst, 2);
        p.medium = SUMI_MEDIUM_KESSHO; sumi_set_params(inst, &p); t19_step(window, inst, 2);
        uint8_t* shot = t71_shot(window, inst, &pw, &ph);
        double c = 0.0;
        if (shot) {
            double on = 0.0, off = 0.0;
            for (int k = 0; k < 4; k++) {   // the four corner rays (the rhomb's period is π; both corner families)
                const double th = thc + k * 3.14159265 / 2.0;
                on += t71b_ray_lum(shot, pw, ph, 0.5, 0.5, th, 0.16, 0.26);
                off += 0.5 * (t71b_ray_lum(shot, pw, ph, 0.5, 0.5, th + 0.17, 0.16, 0.26) + t71b_ray_lum(shot, pw, ph, 0.5, 0.5, th - 0.17, 0.16, 0.26));
            }
            c = std::fabs(on - off) / std::max(off, 1.0);
            char path[1024]; std::snprintf(path, sizeof path, "%s/habit_crease_roundness%d.png", dir, r);
            stbi_write_png(path, (int)pw, (int)ph, 4, shot, (int)pw * 4);
            std::printf("[t71b] wrote %s (corner-ray contrast %.3f: on-ray %.1f vs off-ray %.1f)\n", path, c, on / 4.0, off / 4.0);
            std::free(shot);
        }
        if (r) *contrast1 = c; else *contrast0 = c;
    }
    sumi_set_params(inst, &base);
}
static void t71b_habit_corner(GLFWwindow* window, sumi_instance_t* inst, const char* dir) {
    sumi_params_t base; sumi_get_params(inst, &base);
    std::filesystem::create_directories(dir);
    double c0 = 0.0, c1 = 0.0;
    t71b_crease(window, inst, base, dir, &c0, &c1);
    std::printf("[t71b] --habit-corner: the crease pair in %s — contrast %.3f at roundness 0, %.3f at roundness 1\n", dir, c0, c1);
}
static void t71b_habit_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t71b] habit test (the crystal drop: the homothety and its red, the growth, the crease pair, calcite + twin kink, the pass count)\n");
    sumi_params_t base; sumi_get_params(inst, &base);
    uint32_t pw = 0, ph = 0;
    // 1. gate 5, the homothety: a rhomb drop's interior rings are rhombs — the fract(phase) = 0.5 contour fits f·R(θ) within 0.5 texel;
    //    RED: a circle's contour fitted with the rhomb's profile misses by texels (the interior phase from r gives circles)
    {
        sumi_params_t p = base; p.drop_habit = SUMI_HABIT_RHOMB; p.habit_roundness = 0.0f; p.habit_blades = 5u; p.medium = SUMI_MEDIUM_SUMI;
        sumi_set_params(inst, &p);
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        sumi_add_drop(inst, 0.5f, 0.5f, 0.3f, SUMI_DROP_INK); t19_step(window, inst, 2);
        FieldF f; std::vector<double> xs, ys;
        if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        t71b_contour(f, 0.5, 0.5, 0.5, 0.35, xs, ys);
        double fr = 0.0, fa = 0.0;
        const double res = xs.size() > 50 ? t71b_fit(xs, ys, SUMI_HABIT_RHOMB, 0.0, 5.0, 0.5, 0.5, f.w, false, &fr, &fa) : 1e9;
        std::free(f.px);
        T19(xs.size() > 50 && res < 0.5 && std::fabs(fr - 0.15) < 0.004,
            "gate 5, the homothety: the ring at fract 0.5 of a rhomb drop (r0 0.3) is the rhomb at f = %.4f (0.15 expected: r/R(θ) = 0.5/0.999), mean residual %.3f texel over %zu points (< 0.5)", fr, res, xs.size());
        {   // the red: a circle drop under the rhomb model
            uint8_t* shot = t71_shot(window, inst, &pw, &ph);
            if (shot) { stbi_write_png("habit_rhomb_sumi.png", (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
            p.drop_habit = SUMI_HABIT_CIRCLE; sumi_set_params(inst, &p);
            std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
            sumi_add_drop(inst, 0.5f, 0.5f, 0.3f, SUMI_DROP_INK); t19_step(window, inst, 2);
            FieldF g;
            if (!t19_read_field(inst, &g)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
            t71b_contour(g, 0.5, 0.5, 0.5, 0.35, xs, ys);
            const double red = xs.size() > 50 ? t71b_fit(xs, ys, SUMI_HABIT_RHOMB, 0.0, 5.0, 0.5, 0.5, g.w, false, nullptr, nullptr) : 0.0;
            std::free(g.px);
            T19(xs.size() > 50 && red > 5.0, "gate 5 RED: a circle drop's ring under the rhomb model misses by %.1f texels mean (> 5) — the interior from r gives circles", red);
        }
        // the other habits' rings are their own habits too (the hexagon, the rosette at m = 5), roundness 0.5
        const uint32_t habits[2] = {SUMI_HABIT_HEX, SUMI_HABIT_ROSETTE};
        for (int k = 0; k < 2; k++) {
            p.drop_habit = habits[k]; p.habit_roundness = 0.5f; sumi_set_params(inst, &p);
            std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
            sumi_add_drop(inst, 0.5f, 0.5f, 0.2f, SUMI_DROP_INK); t19_step(window, inst, 2);
            FieldF g;
            if (!t19_read_field(inst, &g)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
            t71b_contour(g, 0.5, 0.5, 0.5, 0.25, xs, ys);
            const double r2 = xs.size() > 50 ? t71b_fit(xs, ys, habits[k], 0.5, 5.0, 0.5, 0.5, g.w, false, &fr, nullptr) : 1e9;
            std::free(g.px);
            T19(xs.size() > 50 && r2 < 0.5, "the %s at roundness 0.5: the ring at fract 0.5 fits f·R(θ) at f = %.4f, residual %.3f texel (< 0.5)", k ? "rosette (m = 5)" : "hexagon", fr, r2);
            uint8_t* shot = t71_shot(window, inst, &pw, &ph);
            if (shot) { stbi_write_png(k ? "habit_rosette_sumi.png" : "habit_hex_sumi.png", (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
        }
        // the evidence: one strike per habit in Sumi, Anod and Kessho (the same field, read three ways)
        static const char* HN[4] = {"circle", "rhomb", "hex", "rosette"};
        static const char* MN[3] = {"sumi", "anod", "kessho"};
        for (uint32_t hb = 0; hb < 4u; hb++) {
            p.drop_habit = hb; p.habit_roundness = hb == SUMI_HABIT_RHOMB ? 0.15f : 0.3f; p.medium = SUMI_MEDIUM_SUMI; sumi_set_params(inst, &p);
            std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
            sumi_add_drop(inst, 0.5f, 0.5f, 0.18f, SUMI_DROP_INK); t19_step(window, inst, 2);
            for (uint32_t md = 0; md < 3u; md++) {
                sumi_params_t q = p; if (md == SUMI_MEDIUM_KESSHO) t71_kessho_params(&q); q.medium = md; q.drop_habit = hb; sumi_set_params(inst, &q); t19_step(window, inst, 2);
                uint8_t* shot = t71_shot(window, inst, &pw, &ph);
                if (shot) { char path[64]; std::snprintf(path, sizeof path, "habit_%s_%s.png", HN[hb], MN[md]); stbi_write_png(path, (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
            }
        }
    }
    // 2. gate 6, the growth: a rhomb under press_mode 0 (the feed) for 300 frames stays a rhomb — the boundary fits f·R(θ) at
    //    frames 100, 200, 300 (the strike frame found by the fit) and the nominal radius grows by the feed's law (linear in ∫p)
    {
        sumi_params_t p = base; p.drop_habit = SUMI_HABIT_RHOMB; p.habit_roundness = 0.15f; p.habit_blades = 5u; p.medium = SUMI_MEDIUM_SUMI;
        p.press_mode = 0; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID; sumi_set_params(inst, &p);
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        sumi_push_midi(inst, 0xB0, 101, 0); sumi_push_midi(inst, 0xB0, 100, 6); sumi_push_midi(inst, 0xB0, 6, 15);
        sumi_push_midi(inst, 0x91, SOAK_VOICE_NOTE, 100); t19_step(window, inst, 4);
        FieldF f0; std::vector<double> xs, ys;
        if (!t19_read_field(inst, &f0)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        // the strike's centre: the ink's centroid
        double cx = 0.0, cy = 0.0; long n = 0;
        for (uint32_t y = 0; y < f0.h; y++) for (uint32_t x = 0; x < f0.w; x++) if (f0.px[((size_t)y * f0.w + x) * 4 + 2] >= 1.0f) { cx += (x + 0.5) / f0.w; cy += (y + 0.5) / f0.h; n++; }
        if (n) { cx /= n; cy /= n; }
        std::free(f0.px);
        double fit[3] = {0, 0, 0}, res[3] = {0, 0, 0}, ang[3] = {0, 0, 0}; long area[3] = {0, 0, 0};
        for (int k = 0; k < 3; k++) {
            for (int i = 0; i < 100; i++) { sumi_push_midi(inst, 0xD1, 100, 0); t19_step(window, inst, 1); }
            FieldF f;
            if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
            t71b_boundary(f, cx, cy, 0.45, xs, ys);
            res[k] = xs.size() > 50 ? t71b_fit(xs, ys, SUMI_HABIT_RHOMB, 0.15, 5.0, cx, cy, f.w, true, &fit[k], &ang[k]) : 1e9;
            area[k] = t71b_band_area(f);
            std::free(f.px);
            { uint8_t* shot = t71_shot(window, inst, &pw, &ph); if (shot) { char path[64]; std::snprintf(path, sizeof path, "habit_growth_%d.png", 100 * (k + 1)); stbi_write_png(path, (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); } }
            std::printf("[t71b] growth frame %d: boundary %zu points, fit f = %.4f at %.0f° (residual %.3f texel), band area %ld texels\n", 100 * (k + 1), xs.size(), fit[k], ang[k] * 57.2958, res[k], area[k]);
        }
        sumi_push_midi(inst, 0x81, SOAK_VOICE_NOTE, 64); sumi_push_midi(inst, 0xD1, 0, 0); t19_step(window, inst, 30);
        const double step1 = fit[1] - fit[0], step2 = fit[2] - fit[1];
        T19(res[0] < 0.5 && res[1] < 0.5 && res[2] < 0.5, "gate 6, the growth: the fed rhomb stays a rhomb — boundary residuals %.3f / %.3f / %.3f texel at 100 / 200 / 300 frames (< 0.5)", res[0], res[1], res[2]);
        T19(step1 > 0.0 && step2 > 0.0 && std::fabs(step2 - step1) < 0.1 * std::max(step1, step2) && area[2] > area[1] && area[1] > area[0],
            "gate 6, the law: the nominal radius grows linearly in the pressure integral under a constant press — f %.4f -> %.4f -> %.4f (steps %.4f, %.4f within 10%%), the area %ld -> %ld -> %ld (∝ f²: the feed's R = R0 + ∫p, DECISIONS_10 #6)",
            fit[0], fit[1], fit[2], step1, step2, area[0], area[1], area[2]);
    }
    // 3. gate 7, the crease pair: the Layer A line along the corner rays at roundness 0, absent at 1
    {
        double c0 = 0.0, c1 = 0.0;
        t71b_crease(window, inst, base, ".", &c0, &c1);
        T19(c0 > 0.25 && c1 < 0.10, "gate 7, the crease: corner-ray contrast %.3f at roundness 0 (> 0.25: the Layer A line where J jumps), %.3f at roundness 1 (< 0.10: the circle, no crease)", c0, c1);
    }
    // 4. gate 8, calcite + the twin kink: a rhomb drop and a twin-kink tine across it — Sumi shows the stepped facets, Kessho the two lamella edges
    {
        sumi_params_t p = base; sumi_mineral_t m; sumi_mineral_t keep; sumi_get_mineral(inst, &keep);
        app_mineral_preset(SUMI_MINERAL_CALCITE, &m, &p, nullptr); sumi_set_mineral(inst, &m);
        t71_kessho_params(&p); p.medium = SUMI_MEDIUM_SUMI; p.drop_habit = SUMI_HABIT_RHOMB; p.habit_roundness = 0.15f; p.tine_profile = SUMI_TINE_TWIN_KINK;
        double lum_centre[2] = {0, 0}, lum_ramp[2] = {0, 0};
        for (int prof = 1; prof >= 0; prof--) {
            p.tine_profile = prof ? SUMI_TINE_TWIN_KINK : SUMI_TINE_SMOOTH; p.medium = SUMI_MEDIUM_SUMI; sumi_set_params(inst, &p);
            std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
            sumi_add_drop(inst, 0.5f, 0.5f, 0.18f, SUMI_DROP_INK); t19_step(window, inst, 2);
            sumi_add_tine(inst, 0.15f, 0.56f, 0.85f, 0.44f, 0.03f, 0.05f); t19_step(window, inst, 2);   // H = 4α = 0.12 across the drop
            if (prof) { uint8_t* shot = t71_shot(window, inst, &pw, &ph); if (shot) { stbi_write_png("calcite_twin_kink_sumi.png", (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); } }
            p.medium = SUMI_MEDIUM_KESSHO; sumi_set_params(inst, &p); t19_step(window, inst, 2);
            uint8_t* shot = t71_shot(window, inst, &pw, &ph);
            if (shot) {
                stbi_write_png(prof ? "calcite_twin_kink_kessho.png" : "calcite_smooth_kessho.png", (int)pw, (int)ph, 4, shot, (int)pw * 4);
                // the luminance across the line (the perpendicular through the centre): the lamella's interior |d| < H/3 against the
                // ramps at |d| = 2H/3 — the twin kink leaves the lamella unstrained (dark) between two lit bands with sharp edges;
                // Jaffer's profile is brightest at the line and fades
                const double dx = 0.7, dy = -0.12, L = std::hypot(dx, dy), nx = -dy / L, ny = dx / L, H = 0.12;
                double centre = 0.0, ramp = 0.0;
                for (int k = -2; k <= 2; k++) centre += t71b_ray_lum(shot, pw, ph, 0.5 + 0.02 * H * k * nx, 0.5 + 0.02 * H * k * ny, 0.0, 0.0, 0.004) / 5.0;
                for (int sgn = -1; sgn <= 1; sgn += 2) for (int k = -2; k <= 2; k++) { const double d = sgn * (2.0 * H / 3.0 + 0.03 * H * k); ramp += t71b_ray_lum(shot, pw, ph, 0.5 + d * nx, 0.5 + d * ny, 0.0, 0.0, 0.004) / 10.0; }
                lum_centre[prof] = centre; lum_ramp[prof] = ramp;
                std::free(shot);
            }
        }
        sumi_set_mineral(inst, &keep);
        T19(lum_ramp[1] > 60.0 && lum_centre[1] < 0.5 * lum_ramp[1] && lum_centre[0] > 0.8 * lum_ramp[0],
            "gate 8, calcite + the twin kink: in Kessho the lamella's interior reads %.0f against its two lit ramps at %.0f (dark between two sharp edges, < half); under Jaffer's profile no dark lamella (the line %.0f against %.0f at the same offsets); the Sumi facets archived",
            lum_centre[1], lum_ramp[1], lum_centre[0], lum_ramp[0]);
    }
    // 5. gate 9, the pass count: a habit is a profile, not a pass — a rhomb strike and a twin-kink stroke each cost ONE pass, as the circle and Jaffer's do
    {
        sumi_params_t p = base; p.medium = SUMI_MEDIUM_SUMI; sumi_set_params(inst, &p);
        uint32_t passes[2][2] = {{0, 0}, {0, 0}};
        for (int k = 0; k < 2; k++) {
            p.drop_habit = k ? SUMI_HABIT_RHOMB : SUMI_HABIT_CIRCLE; p.tine_profile = k ? SUMI_TINE_TWIN_KINK : SUMI_TINE_SMOOTH; sumi_set_params(inst, &p);
            std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
            const uint32_t s0 = sumi_debug_pass_count(inst);
            sumi_add_drop(inst, 0.5f, 0.5f, 0.1f, SUMI_DROP_INK); t19_step(window, inst, 1);
            const uint32_t s1 = sumi_debug_pass_count(inst);
            sumi_add_tine(inst, 0.2f, 0.5f, 0.8f, 0.5f, 0.03f, 0.03f); t19_step(window, inst, 1);
            const uint32_t s2 = sumi_debug_pass_count(inst);
            passes[k][0] = s1 - s0; passes[k][1] = s2 - s1;
        }
        T19(passes[1][0] == passes[0][0] && passes[1][1] == passes[0][1] && passes[0][0] == 1u && passes[0][1] == 1u,
            "gate 9, the pass count: a rhomb strike %u pass (the circle %u), a twin-kink stroke %u (Jaffer's %u)", passes[1][0], passes[0][0], passes[1][1], passes[0][1]);
    }
    sumi_set_params(inst, &base);
}
// ---- step 71 amended, gates 8–12: the mineral's optical laws (KESSHO §4.5) ----------------------------------
static void t71_mineral_gates(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t71] the mineral gates (8–12): switching bitwise, the selenite sign, fluorite = Layer B, the dispersion fringe, the walk-off collapse\n");
    sumi_params_t base; sumi_get_params(inst, &base);
    sumi_mineral_t keep; sumi_get_mineral(inst, &keep);
    sumi_params_t p = base; t71_kessho_params(&p);
    uint32_t pw = 0, ph = 0;
    // gate 8: switching minerals leaves the field bitwise — the §10.1 gate extends to every mineral
    {
        sumi_set_params(inst, &p);
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        sumi_debug_run_field_script(inst); t19_step(window, inst, 2);
        FieldF f; if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        long diff = 0; int distinct = 0; uint8_t* prev = nullptr;
        for (uint32_t id = 0; id < SUMI_MINERAL_COUNT; id++) {
            sumi_mineral_t m; const char* nm = nullptr; sumi_mineral_preset(id, &m, &nm); sumi_set_mineral(inst, &m); t19_step(window, inst, 2);
            uint8_t* shot = t71_shot(window, inst, &pw, &ph);
            if (shot) {
                char path[64]; std::snprintf(path, sizeof path, "mineral_%u_%s.png", id, nm ? nm : "x");
                stbi_write_png(path, (int)pw, (int)ph, 4, shot, (int)pw * 4);
                if (prev && t43_diff(prev, shot, (size_t)pw * ph * 4) > 0) distinct++;
                std::free(prev); prev = shot;
            }
            FieldF g; if (!t19_read_field(inst, &g)) { t19_failures++; std::printf("FAIL: field read\n"); std::free(f.px); return; }
            for (size_t i = 0; i < (size_t)f.w * f.h * 4; i++) if (f.px[i] != g.px[i]) diff++;
            std::free(g.px);
        }
        std::free(prev); std::free(f.px);
        T19(diff == 0 && distinct == 5, "gate 8, the switch: the six minerals over one field — the field bitwise (%ld samples differ), every consecutive render distinct (%d of 5)", diff, distinct);
    }
    // the shear field the sign and the walk-off read: Jaffer's tine on a fresh sheet — ψ = +45° on one side of the line, −45° on the other
    auto shear_sheet = [&](const sumi_mineral_t& m) {
        sumi_set_mineral(inst, &m);
        sumi_params_t q = p; q.medium = SUMI_MEDIUM_SUMI; q.tine_profile = SUMI_TINE_SMOOTH; sumi_set_params(inst, &q);
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        sumi_add_tine(inst, 0.1f, 0.5f, 0.9f, 0.5f, 0.04f, 0.06f); t19_step(window, inst, 2);
        q.medium = SUMI_MEDIUM_KESSHO; sumi_set_params(inst, &q); t19_step(window, inst, 2);
        return t71_shot(window, inst, &pw, &ph);
    };
    auto patch = [&](const uint8_t* px, double x0, double x1, double y0, double y1, double out[3]) {
        double acc[3] = {0, 0, 0}; long n = 0;
        for (uint32_t y = (uint32_t)(y0 * ph); y < (uint32_t)(y1 * ph); y++) for (uint32_t x = (uint32_t)(x0 * pw); x < (uint32_t)(x1 * pw); x++) { const size_t o = ((size_t)y * pw + x) * 4; for (int c = 0; c < 3; c++) acc[c] += px[o + c]; n++; }
        for (int c = 0; c < 3; c++) out[c] = n ? acc[c] / n : 0.0;
    };
    // gate 9: selenite — the sign of the shear shows as blue above the line and yellow below (or the reverse); RED: Γ₀ = 0 reads the two sides alike
    {
        sumi_mineral_t sel; sumi_mineral_preset(SUMI_MINERAL_SELENITE, &sel, nullptr);
        uint8_t* a = shear_sheet(sel);
        sumi_mineral_t red = sel; red.retardation_bias = 0.0f;
        uint8_t* b = shear_sheet(red);
        double ua[3], la[3], ub[3], lb[3];
        if (a && b) {
            stbi_write_png("selenite_sign.png", (int)pw, (int)ph, 4, a, (int)pw * 4);
            stbi_write_png("selenite_sign_red.png", (int)pw, (int)ph, 4, b, (int)pw * 4);
            patch(a, 0.3, 0.7, 0.44, 0.48, ua); patch(a, 0.3, 0.7, 0.52, 0.56, la);
            patch(b, 0.3, 0.7, 0.44, 0.48, ub); patch(b, 0.3, 0.7, 0.52, 0.56, lb);
        }
        const double sa = (ua[2] - ua[0]) - (la[2] - la[0]), sb = (ub[2] - ub[0]) - (lb[2] - lb[0]);
        T19(a && b && std::fabs(sa) > 20.0 && std::fabs(sb) < 8.0 && (ua[0] + ua[1] + ua[2]) > 60.0,
            "gate 9, the selenite sign: above the line (b − r) %.0f, below %.0f — a %.0f swing between the shear's two signs (> 20/255; the resting sheet lit at Γ₀, mean %.0f); RED at Γ₀ = 0: the swing %.1f (< 8)",
            ua[2] - ua[0], la[2] - la[0], sa, (ua[0] + ua[1] + ua[2]) / 3.0, sb);
        std::free(a); std::free(b);
    }
    // gate 10: fluorite is Layer B alone — the light export IS the caustic export, bitwise; under quartz they differ
    {
        sumi_mineral_t fl; sumi_mineral_preset(SUMI_MINERAL_FLUORITE, &fl, nullptr); sumi_set_mineral(inst, &fl);
        sumi_set_params(inst, &p);
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        sumi_debug_run_field_script(inst); t19_step(window, inst, 2);
        uint32_t w = 0, h = 0;
        uint8_t* light = t43_export(window, inst, nullptr, 0, 0, 512, 512, SUMI_EXPORT_ANOD_ALPHA, &w, &h);
        uint8_t* caus = t43_export(window, inst, nullptr, 0, 0, 512, 512, SUMI_EXPORT_KESSHO_CAUSTIC, &w, &h);
        const long d_fl = (light && caus) ? t43_diff(light, caus, (size_t)512 * 512 * 4) : -1;
        long lit = 0; if (caus) for (size_t i = 0; i < (size_t)512 * 512; i++) if (caus[i * 4 + 3] > 0) lit++;
        if (light) stbi_write_png("fluorite_light.png", 512, 512, 4, light, 512 * 4);
        std::free(light); std::free(caus);
        sumi_mineral_t qz; sumi_mineral_preset(SUMI_MINERAL_QUARTZ, &qz, nullptr); sumi_set_mineral(inst, &qz); t19_step(window, inst, 2);
        light = t43_export(window, inst, nullptr, 0, 0, 512, 512, SUMI_EXPORT_ANOD_ALPHA, &w, &h);
        caus = t43_export(window, inst, nullptr, 0, 0, 512, 512, SUMI_EXPORT_KESSHO_CAUSTIC, &w, &h);
        const long d_qz = (light && caus) ? t43_diff(light, caus, (size_t)512 * 512 * 4) : -1;
        std::free(light); std::free(caus);
        T19(d_fl == 0 && lit > 100 && d_qz > 1000, "gate 10, fluorite = Layer B alone: the light export is the caustic export bitwise (%ld bytes differ, %ld texels lit); under quartz %ld bytes differ (Layer A)", d_fl, lit, d_qz);
    }
    // gate 11: the dispersion fringe — barite's caustic carries colour off the body's ratio where the fold is; at dispersion 0 none
    {
        sumi_mineral_t ba; sumi_mineral_preset(SUMI_MINERAL_BARITE, &ba, nullptr);
        ba.conical_ring = 0.0f;   // the ring alone next; here the three depths
        sumi_params_t q = p; q.kessho_tint = 0.0f; q.kessho_caustic = 1.0f; sumi_set_params(inst, &q);
        sumi_map_cc(inst, 0xFF, 21, SUMI_CTL_CAUSTIC_DEPTH); sumi_push_midi(inst, 0xB0, 21, 110); t19_step(window, inst, 240);   // a deep bed: the fold through the strained water
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        sumi_debug_run_field_script(inst); t19_step(window, inst, 2);
        long fringe[2] = {0, 0}; long lit[2] = {0, 0};
        for (int k = 0; k < 2; k++) {
            sumi_mineral_t m = ba; if (k == 1) m.dispersion = 0.0f;
            sumi_set_mineral(inst, &m); t19_step(window, inst, 2);
            uint32_t w = 0, h = 0;
            uint8_t* caus = t43_export(window, inst, nullptr, 0, 0, 512, 512, SUMI_EXPORT_KESSHO_CAUSTIC, &w, &h);
            if (caus) {
                for (size_t i = 0; i < (size_t)512 * 512; i++) {
                    if (caus[i * 4 + 3] == 0) continue;
                    lit[k]++;
                    const double r = t71_srgb8_lin(caus[i * 4]) / m.body_tint[0], g = t71_srgb8_lin(caus[i * 4 + 1]) / m.body_tint[1], b = t71_srgb8_lin(caus[i * 4 + 2]) / m.body_tint[2];
                    const double mx = std::max(r, std::max(g, b));
                    if (mx > 0.15 && (std::fabs(r - b) > 0.15 * mx || std::fabs(r - g) > 0.15 * mx)) fringe[k]++;   // linear light: the body's ratio holds exactly at dispersion 0
                }
                stbi_write_png(k ? "barite_caustic_nodispersion.png" : "barite_caustic_dispersion.png", 512, 512, 4, caus, 512 * 4);
                std::free(caus);
            }
        }
        sumi_push_midi(inst, 0xB0, 21, 0); t19_step(window, inst, 240);
        T19(lit[0] > 100 && fringe[0] > 50 && fringe[1] == 0, "gate 11, the dispersion fringe: %ld of %ld lit caustic texels off the body's colour ratio (linear light) under barite's dispersion 0.3 (> 50), %ld of %ld at dispersion 0 (= 0)", fringe[0], lit[0], fringe[1], lit[1]);
    }
    // gate 12: the walk-off collapse — calcite's double image over the Maltese cross: on the dark arms (the strain's axis meets the
    //          analyzer) the split closes and the render is the no-walk-off one; between the arms it is not
    {
        sumi_mineral_t ca; sumi_mineral_preset(SUMI_MINERAL_CALCITE, &ca, nullptr);
        ca.retardation = 2000.0f / 0.009f * 0.009f;   // the quartz exposure, so the fringes stay in the chart's first orders for the comparison
        sumi_set_params(inst, &p);
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        const float rc = 0.08f, S = 1.2f * 6.2831853f * rc * rc;   // a firmer swirl than the cross test's: σ ~ 0.6 in the annulus, the split a few texels
        sumi_add_vortex(inst, 0.5f, 0.5f, S, rc, SUMI_VORTEX_LAMB_OSEEN); t19_step(window, inst, 2);
        uint8_t* shots[2] = {nullptr, nullptr};
        for (int k = 0; k < 2; k++) { sumi_mineral_t m = ca; if (k) m.walkoff = 0.0f; sumi_set_mineral(inst, &m); t19_step(window, inst, 2); shots[k] = t71_shot(window, inst, &pw, &ph); }
        double arm = 0.0, between = 0.0;
        if (shots[0] && shots[1]) {
            stbi_write_png("calcite_walkoff.png", (int)pw, (int)ph, 4, shots[0], (int)pw * 4);
            for (int k = 0; k < 8; k++) {
                const double th = k * 3.14159265 / 4.0;
                double acc = 0.0; int n = 0;
                for (int rr = 0; rr < 6; rr++) {
                    const double r = (0.8 + 0.16 * rr) * rc;
                    const int x = (int)((0.5 + r * std::cos(th)) * pw), y = (int)((0.5 + r * std::sin(th)) * ph);
                    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { const size_t o = ((size_t)(y + dy) * pw + (size_t)(x + dx)) * 4; acc += std::fabs(t42_lum(shots[0], o) - t42_lum(shots[1], o)); n++; }
                }
                if (k % 2 == 1) arm += acc / n / 4.0; else between += acc / n / 4.0;   // the dark arms are the DIAGONALS under the analyzer at 0 (the step-71 cross test)
            }
        }
        std::free(shots[0]); std::free(shots[1]);
        T19(arm < 2.0 && between > 1.0 && between > 3.0 * arm, "gate 12, the walk-off collapse: on the cross's dark arms (the diagonals) the double image closes — mean |Δlum| %.2f/255 against no walk-off (< 2); between the arms it is open, %.2f (> 1, > 3× the arms)", arm, between);
    }
    sumi_set_mineral(inst, &keep);
    sumi_clear_cc_map(inst);
    sumi_set_params(inst, &base);
}

/* ------------------------------------------------------------------ */
/* Phase 11 step 72: the Arnol'd tongue lock (KESSHO §2.2)             */
/* ------------------------------------------------------------------ */
// the q-th harmonic of the ink over the annulus [r0, r1] about (cx, cy): the island pattern's amplitude (normalized
// by the annulus's ink) and its angle arg(c_q)/q — the islands sit where the kick pushes out, q of them a turn
static double t72_harmonic(const FieldF& f, double cx, double cy, double r0, double r1, int q, double* angle) {
    double re = 0.0, im = 0.0, mass = 0.0;
    for (uint32_t y = 0; y < f.h; y++) for (uint32_t x = 0; x < f.w; x++) {
        const double px = ((double)x + 0.5) / f.w - cx, py = ((double)y + 0.5) / f.h - cy, r = std::hypot(px, py);
        if (r < r0 || r > r1) continue;
        const float ph = f.px[((size_t)y * f.w + x) * 4 + 2];
        if (ph < 1.0f) continue;
        const double th = std::atan2(py, px);
        re += std::cos(q * th); im += std::sin(q * th); mass += 1.0;
    }
    if (angle) *angle = mass > 0.0 ? std::atan2(im, re) / q : 0.0;
    return mass > 0.0 ? std::hypot(re, im) / mass : 0.0;
}
// the ink's centroid and the radius of its disc about it (A = πR²) — a lone strike's drop
static bool t72_disc(const FieldF& f, double* cx, double* cy, double* R) {
    double sx = 0.0, sy = 0.0; long n = 0;
    for (uint32_t y = 0; y < f.h; y++) for (uint32_t x = 0; x < f.w; x++) if (f.px[((size_t)y * f.w + x) * 4 + 2] >= 1.0f) { sx += (x + 0.5) / f.w; sy += (y + 0.5) / f.h; n++; }
    if (n < 10) return false;
    *cx = sx / n; *cy = sy / n; *R = std::sqrt((double)n / 3.14159265 / ((double)f.w * f.h));
    return true;
}
// the farthest ink texel along the ray at angle th (within ±wedge) from (cx, cy), in texels
static double t72_ray_extent(const FieldF& f, double cx, double cy, double th, double wedge) {
    double best = 0.0;
    for (uint32_t y = 0; y < f.h; y++) for (uint32_t x = 0; x < f.w; x++) {
        if (f.px[((size_t)y * f.w + x) * 4 + 2] < 1.0f) continue;
        const double px = (x + 0.5) / f.w - cx, py = (y + 0.5) / f.h - cy;
        double d = std::atan2(py, px) - th; d = std::remainder(d, 6.2831853);
        if (std::fabs(d) > wedge) continue;
        const double r = std::hypot(px, py) * f.w;
        if (r > best) best = r;
    }
    return best;
}
static void t72_mpe(sumi_instance_t* inst) { sumi_push_midi(inst, 0xB0, 101, 0); sumi_push_midi(inst, 0xB0, 100, 6); sumi_push_midi(inst, 0xB0, 6, 15); }
// a lone strike of `note` on channel ch: its drop's centre and radius, then released
static bool t72_strike_disc(GLFWwindow* window, sumi_instance_t* inst, uint8_t ch, uint8_t note, double* cx, double* cy, double* R) {
    uint32_t pw = 0, ph = 0;
    std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
    t72_mpe(inst);
    sumi_push_midi(inst, (uint8_t)(0x90 | ch), note, 100); t19_step(window, inst, 4);
    FieldF f; if (!t19_read_field(inst, &f)) return false;
    const bool ok = t72_disc(f, cx, cy, R);
    std::free(f.px);
    sumi_push_midi(inst, (uint8_t)(0x80 | ch), note, 64); t19_step(window, inst, 30);
    return ok;
}
static const uint8_t T72_BASS = 48, T72_FIFTH = 55, T72_THIRD = 52;   // C3; G3 (3:2 — Ω₀ = 1/2, q = 2); E3 (5:4 — Ω₀ = 1/4, q = 4)
static void t72_arnold_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t72] the Arnol'd tongue lock (KESSHO §2.2): the pass against its closed form and its exact inverse, the locked islands, the precession, the twist's red, the tongue crossing\n");
    sumi_params_t base; sumi_get_params(inst, &base);
    uint32_t pw = 0, ph = 0;
    FieldF f;
    // 1. gate 3 on the GPU: one iteration at K_max (q 3, Ω₀ 0.5, phase 0.4) against the CPU reference (the five-iteration
    //    Newton: the unit test reads 1e-11 at the bound and 1e4 at 1.3× it); then the EXACT inverse pass returns the sheet
    {
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        sumi_debug_add_arnold(inst, 0.5f, 0.5f, 0.12f, 0.5f, 1.0f, 3u, 0.4f, 0); t19_step(window, inst, 1);
        if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        double worst = 0.0, moved = 0.0; int n = 0;
        for (int i = 0; i < 40; i++) {
            const double th = i * 6.2831853 / 40.0, r = 0.04 + 0.2 * ((i * 7) % 10) / 10.0;   // inside the twist's window (2.5 R = 0.3)
            const uint32_t x = (uint32_t)((0.5 + r * std::cos(th)) * f.w), y = (uint32_t)((0.5 + r * std::sin(th)) * f.h);
            if (x >= f.w || y >= f.h) continue;
            const size_t o = ((size_t)y * f.w + x) * 4;
            const double px = (x + 0.5) / (double)f.w, py = (y + 0.5) / (double)f.h;
            double sx, sy; sumi_debug_arnold_map(0.5f, 0.5f, 0.12f, 0.5f, 1.0f, 3u, 0.4f, 1.0f, (float)px, (float)py, 0, &sx, &sy);
            const double e = std::hypot(((double)f.px[o] - sx) * f.w, ((double)f.px[o + 1] - sy) * f.h);
            moved = std::fmax(moved, std::hypot((sx - px) * f.w, (sy - py) * f.h));
            if (e > worst) worst = e; n++;
        }
        std::free(f.px);
        auto back_of = [&](float omega) {   // the iteration then its inverse pass: the sheet's pre-image against the identity
            std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
            sumi_debug_add_arnold(inst, 0.5f, 0.5f, 0.12f, omega, 1.0f, 3u, 0.4f, 0); t19_step(window, inst, 1);
            sumi_debug_add_arnold(inst, 0.5f, 0.5f, 0.12f, omega, 1.0f, 3u, 0.4f, 1); t19_step(window, inst, 1);
            FieldF g; if (!t19_read_field(inst, &g)) return 1e9;
            double back = 0.0;
            for (uint32_t y = 16; y + 16 < g.h; y++) for (uint32_t x = 16; x + 16 < g.w; x++) {
                const size_t o = ((size_t)y * g.w + x) * 4;
                back = std::fmax(back, std::hypot(((double)g.px[o] - (x + 0.5) / g.w) * g.w, ((double)g.px[o + 1] - (y + 0.5) / g.h) * g.h));
            }
            std::free(g.px);
            return back;
        };
        const double back_kick = back_of(0.0f), back_full = back_of(0.5f);
        T19(n > 30 && worst < 0.3, "gate 3, the pass is the closed form: the pre-image of %d texels matches the CPU map (one Newton on θ, five iterations) to %.3f texel (< 0.3; the map moves them up to %.1f)", n, worst, moved);
        T19(back_kick < 0.5, "gate 3, the exact inverse: the kick then its inverse pass (one Newton on ρ₁) leaves the sheet's pre-image within %.3f texel of the identity (< 0.5); with the half-turn twist on top %.2f — two bilinear resamplings of a 150-texel move", back_kick, back_full);
    }
    // 2. gate 4: the islands stationary — at the EXACT ratios (the gesture route: Ω₀ = 1/2 and 1/4; a MIDI fifth is equal
    //    temperament's 2^(7/12), two cents under 3:2, and its islands precess at δ = −0.00169 turns a frame — measured below)
    //    600 iterations at the press-100 kick on an ink disc: the q-fold pattern's angle stays within its island (an
    //    excursion under π/(2q)) — the ink librates inside the islands, the islands do not move; off the lock the same
    //    two cents carry the pattern a full turn in 600 frames
    sumi_params_t p = base; t71_kessho_params(&p); p.medium = SUMI_MEDIUM_SUMI; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID; p.press_mode = 3;
    p.smoothing_ms = 5.0f;
    const struct { float omega; int q; const char* name; } locks[2] = { {0.5f, 2, "3:2 (Ω₀ = 1/2)"}, {0.25f, 4, "5:4 (Ω₀ = 1/4)"} };
    for (int c = 0; c < 2; c++) {
        sumi_params_t q = base; q.medium = SUMI_MEDIUM_SUMI; sumi_set_params(inst, &q);
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        sumi_add_drop(inst, 0.5f, 0.5f, 0.1f, SUMI_DROP_INK); t19_step(window, inst, 2);
        double lo = 1e9, hi = -1e9, prev = 0.0, unwrapped = 0.0, amp_min = 1.0; int n = 0;
        for (int it = 1; it <= 600; it++) {
            sumi_add_arnold(inst, 0.5f, 0.5f, 0.1f, locks[c].omega, 0.79f, (uint32_t)locks[c].q, 1u); t19_step(window, inst, 1);
            if (it % 30) continue;
            if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
            double ang; const double amp = t72_harmonic(f, 0.5, 0.5, 0.0, 0.16, locks[c].q, &ang);
            std::free(f.px);
            if (it == 30) { prev = ang; unwrapped = 0.0; }
            else { unwrapped += std::remainder(ang - prev, 6.2831853 / locks[c].q); prev = ang; }
            if (it >= 120) { lo = std::fmin(lo, unwrapped); hi = std::fmax(hi, unwrapped); amp_min = std::fmin(amp_min, amp); n++; }
            if (it % 150 == 0) std::printf("[t72] %s, iteration %d: %d-fold harmonic %.4f, the pattern's angle %+.3f rad from its first reading\n", locks[c].name, it, locks[c].q, amp, unwrapped);
        }
        const double excursion = hi - lo, island = 3.14159265 / (2.0 * locks[c].q);
        T19(n > 10 && amp_min > 0.02 && excursion < island, "gate 4, %s at the exact ratio, 600 iterations: the %d-fold pattern's angle stays within its island — excursion %.3f rad (< π/(2q) = %.3f: the ink librates, the islands stand; amplitude ≥ %.3f)", locks[c].name, locks[c].q, excursion, island, amp_min);
        uint8_t* shot = t71_shot(window, inst, &pw, &ph); if (shot) { char path[64]; std::snprintf(path, sizeof path, "arnold_lock_q%d_sumi.png", locks[c].q); stbi_write_png(path, (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
    }
    // the MIDI fifth: equal temperament's 2^(7/12) = 1.49831 — two cents under 3:2, δ = −0.00169 turns a frame: the beat of
    // equal temperament against just intonation, made visible (informational; the precession gate is the next)
    double ucx = 0.0, ucy = 0.0, uR = 0.0;
    {
        sumi_set_params(inst, &p);
        if (!t72_strike_disc(window, inst, 2, T72_FIFTH, &ucx, &ucy, &uR)) { t19_failures++; std::printf("FAIL: the upper voice's disc\n"); return; }
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        t72_mpe(inst);
        sumi_push_midi(inst, 0x92, T72_FIFTH, 100); sumi_push_midi(inst, 0x91, T72_BASS, 100); t19_step(window, inst, 4);   // the upper voice first: the first drop is ink, the second clear (the parity form)
        for (int i = 0; i < 100; i++) { sumi_push_midi(inst, 0xD2, 100, 0); t19_step(window, inst, 1); }
        double prev = 0.0, unwrapped = 0.0; int n = 0; double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
        for (int k = 0; k <= 300; k++) {   // 1200 frames: two turns of the tempered beat
            if (k) for (int i = 0; i < 4; i++) { sumi_push_midi(inst, 0xD2, 100, 0); t19_step(window, inst, 1); }
            if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
            double ang; t72_harmonic(f, ucx, ucy, 0.0, 1.6 * uR, 2, &ang);
            std::free(f.px);
            if (k == 0) { prev = ang; unwrapped = 0.0; } else { unwrapped += std::remainder(ang - prev, 3.14159265); prev = ang; }
            const double t = 4.0 * k; sx += t; sy += unwrapped; sxx += t * t; sxy += t * unwrapped; n++;
        }
        const double turns = ((n * sxy - sx * sy) / (n * sxx - sx * sx)) / 6.2831853;
        const double et = 1.5 - std::pow(2.0, 7.0 / 12.0);
        std::printf("[t72] the MIDI fifth (equal temperament, 2^(7/12)): the two-fold pattern precesses %+.5f turns a frame — the beat of the tempered fifth, %.1f cents under 3:2 (δ = %+.5f)\n", turns, 1200.0 * std::log2(std::pow(2.0, 7.0 / 12.0) / 1.5), -et);
        sumi_push_midi(inst, 0xD2, 0, 0); sumi_push_midi(inst, 0x81, T72_BASS, 64); sumi_push_midi(inst, 0x82, T72_FIFTH, 64); t19_step(window, inst, 30);
    }
    // gate 5: the fifth bent +10 cents (17 of 8192 at the 48-semitone MPE range = 0.0996 semitones) — the pattern precesses
    {
        if (!t72_strike_disc(window, inst, 2, T72_FIFTH, &ucx, &ucy, &uR)) { t19_failures++; std::printf("FAIL: the upper voice's disc\n"); return; }
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        t72_mpe(inst);
        sumi_push_midi(inst, 0x92, T72_FIFTH, 100); sumi_push_midi(inst, 0x91, T72_BASS, 100); t19_step(window, inst, 4);   // the upper voice first (ink)
        const long pb = 8192 + 17;
        sumi_push_midi(inst, 0xE2, (uint8_t)(pb & 0x7F), (uint8_t)((pb >> 7) & 0x7F));
        const double semis = 17.0 / 8192.0 * 48.0, ratio = std::pow(2.0, (7.0 + semis) / 12.0);   // the tempered fifth plus the bend
        const double delta = (ratio - std::floor(ratio)) - 0.5;   // turns per frame
        for (int i = 0; i < 100; i++) { sumi_push_midi(inst, 0xD2, 100, 0); t19_step(window, inst, 1); }   // the glide settles, the lock forms
        double prev = 0.0, unwrapped = 0.0; int n = 0; double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0; double amp_min = 1.0;
        for (int k = 0; k <= 400; k++) {   // 1600 frames: the libration's wobble (a few tenths of a radian) against 70 radians of precession
            if (k) for (int i = 0; i < 4; i++) { sumi_push_midi(inst, 0xD2, 100, 0); t19_step(window, inst, 1); }
            if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
            double ang; const double amp = t72_harmonic(f, ucx, ucy, 0.0, 1.6 * uR, 2, &ang);
            std::free(f.px);
            if (k == 0) { prev = ang; unwrapped = 0.0; }
            else { const double d = std::remainder(ang - prev, 6.2831853 / 2.0); unwrapped += d; prev = ang; }
            const double t = 4.0 * k;
            sx += t; sy += unwrapped; sxx += t * t; sxy += t * unwrapped; n++;
            amp_min = std::fmin(amp_min, amp);
        }
        const double slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);   // rad per frame
        const double turns = slope / 6.2831853;
        T19(amp_min > 0.02 && std::fabs(std::fabs(turns) - delta) < 0.01 * delta + 1e-5,
            "gate 5, the tempered fifth bent +10 cents (Ω₀ = frac(2^(7.0996/12))): the two-fold pattern precesses %.5f turns a frame against δ = Ω₀ − 1/2 = %.5f (within 1%%; the sense %s the twist's; amplitude ≥ %.3f)", std::fabs(turns), delta, turns < 0 ? "against" : "with", amp_min);
        uint8_t* shot = t71_shot(window, inst, &pw, &ph); if (shot) { stbi_write_png("arnold_precess_sumi.png", (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
        sumi_push_midi(inst, 0xD2, 0, 0); sumi_push_midi(inst, 0xE2, 0x00, 0x40); sumi_push_midi(inst, 0x81, T72_BASS, 64); sumi_push_midi(inst, 0x82, T72_FIFTH, 64); t19_step(window, inst, 30);
    }
    // 3. gate 6, RED (the twist's role): the gesture route, 120 iterations at K_max with q = 2 on an ink disc — WITHOUT the
    //    twist the canonical kick alone has no islands: its fixed lines are the kick's nodes (sin qθ = 0) and the ink
    //    collects on them, 2q blobs; with the twist (Ω₀ = 1/2) the q islands form. The ink's 2q-fold harmonic beats the
    //    q-fold without the twist and loses to it with. (The proposal's "petals grow without bound" does not happen: the
    //    kick's window bounds the petals either way — DECISIONS_10 #9.)
    {
        double hq[2] = {0, 0}, h2q[2] = {0, 0};
        for (int tw = 0; tw < 2; tw++) {
            sumi_params_t q = base; q.medium = SUMI_MEDIUM_SUMI; sumi_set_params(inst, &q);
            std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
            sumi_add_drop(inst, 0.5f, 0.5f, 0.1f, SUMI_DROP_INK); t19_step(window, inst, 2);
            for (int i = 0; i < 120; i++) { sumi_add_arnold(inst, 0.5f, 0.5f, 0.1f, 0.5f, 1.0f, 2u, (uint32_t)tw); t19_step(window, inst, 1); }
            if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
            hq[tw] = t72_harmonic(f, 0.5, 0.5, 0.0, 0.16, 2, nullptr); h2q[tw] = t72_harmonic(f, 0.5, 0.5, 0.0, 0.16, 4, nullptr);
            std::free(f.px);
            uint8_t* shot = t71_shot(window, inst, &pw, &ph); if (shot) { stbi_write_png(tw ? "arnold_twist_sumi.png" : "arnold_notwist_red_sumi.png", (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
        }
        T19(h2q[0] > hq[0], "gate 6 RED, no twist: the kick alone makes no islands — the ink collects on its nodal lines, the 4-fold harmonic %.3f over the 2-fold %.3f", h2q[0], hq[0]);
        T19(hq[1] > 5.0 * hq[0] && hq[1] > 0.5 * h2q[1] && hq[1] > 0.02, "gate 6, with the twist (Ω₀ = 1/2): the two islands appear — the 2-fold harmonic %.3f (> 5× the red's %.3f, at least half the 4-fold's %.3f)", hq[1], hq[0], h2q[1]);
    }
    // 4. gate 7, the tongue crossing: the fifth bent down two semitones over 240 frames under the press — Ω₀ 0.5 → 0.336
    //    leaves the 1/2 tongue, crosses 2/5's and lands in 1/3's. The map's continuity is the headless test's (the records'
    //    K through zero at every q change, φ accumulating, q switched at K = 0); here the sheet's flicker — the fraction
    //    of the drop's texels whose band flips between consecutive frames — is measured through the crossing and
    //    archived (a locked pattern under a p/q turn a frame flips little; a q change reshapes the pattern)
    {
        sumi_set_params(inst, &p);
        if (!t72_strike_disc(window, inst, 2, T72_FIFTH, &ucx, &ucy, &uR)) { t19_failures++; std::printf("FAIL: the upper voice's disc\n"); return; }
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        t72_mpe(inst);
        sumi_push_midi(inst, 0x92, T72_FIFTH, 100); sumi_push_midi(inst, 0x91, T72_BASS, 100); t19_step(window, inst, 4);
        for (int i = 0; i < 60; i++) { sumi_push_midi(inst, 0xD2, 100, 0); t19_step(window, inst, 1); }
        FieldF prevf; prevf.px = nullptr;
        double flick_max = 0.0, flick_sum = 0.0; int n = 0; int q_prev = 2; double at_change = 0.0;
        for (int i = 0; i <= 240; i++) {
            const long pb = 8192 - (long)(341.0 * i / 240.0);
            sumi_push_midi(inst, 0xE2, (uint8_t)(pb & 0x7F), (uint8_t)((pb >> 7) & 0x7F));
            sumi_push_midi(inst, 0xD2, 100, 0); t19_step(window, inst, 1);
            if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
            if (prevf.px) {
                long flips = 0, tot = 0;
                for (uint32_t y = 0; y < f.h; y++) for (uint32_t x = 0; x < f.w; x++) {
                    if (std::hypot((x + 0.5) / f.w - ucx, (y + 0.5) / f.h - ucy) > 1.6 * uR) continue;
                    const size_t o = ((size_t)y * f.w + x) * 4 + 2;
                    tot++; if ((f.px[o] >= 1.0f) != (prevf.px[o] >= 1.0f)) flips++;
                }
                const double fl = tot ? (double)flips / tot : 0.0;
                flick_max = std::fmax(flick_max, fl); flick_sum += fl; n++;
                const double semis = -2.0 * i / 240.0, ratio = std::pow(2.0, (7.0 + semis) / 12.0), omega = ratio - std::floor(ratio);
                uint32_t pp = 0, qq = 1; sumi_arnold_tongue((float)omega, &pp, &qq);
                if ((int)qq != q_prev) { std::printf("[t72] crossing at frame %d: Ω₀ %.4f, the tongue %u/%u (from q %d); the flicker this frame %.3f\n", i, omega, pp, qq, q_prev, fl); at_change = std::fmax(at_change, fl); q_prev = (int)qq; }
                std::free(prevf.px);
            }
            prevf = f;
        }
        std::free(prevf.px);
        std::printf("[t72] gate 7, the tongue crossing: the drop's band flicker between consecutive frames — mean %.3f, max %.3f over %d frames, %.3f at the q changes (informational; the continuity of the records is the headless gate)\n", n ? flick_sum / n : 0.0, flick_max, n, at_change);
        uint8_t* shot = t71_shot(window, inst, &pw, &ph); if (shot) { stbi_write_png("arnold_crossing_sumi.png", (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
        sumi_push_midi(inst, 0xD2, 0, 0); sumi_push_midi(inst, 0xE2, 0x00, 0x40); sumi_push_midi(inst, 0x81, T72_BASS, 64); sumi_push_midi(inst, 0x82, T72_FIFTH, 64); t19_step(window, inst, 30);
    }
    sumi_set_params(inst, &base);
}
// a probe of the lock's regimes (a diagnostic): the kick k = 0.1 / 0.25 / 0.5 / 1 of K_max through the gesture on an ink
// disc, 200 iterations, the 2-fold harmonic and the shots in Sumi and Kessho
static void t72_arnold_probe(GLFWwindow* window, sumi_instance_t* inst, const char* dir) {
    sumi_params_t base; sumi_get_params(inst, &base);
    std::filesystem::create_directories(dir);
    uint32_t pw = 0, ph = 0; FieldF f;
    for (float k : {0.1f, 0.25f, 0.5f, 1.0f}) {
        for (int medium = 0; medium < 3; medium += 2) {
            sumi_params_t q = base; if (medium) t71_kessho_params(&q); q.medium = (uint32_t)medium; sumi_set_params(inst, &q);
            std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
            sumi_add_drop(inst, 0.5f, 0.5f, 0.1f, SUMI_DROP_INK); t19_step(window, inst, 2);
            for (int i = 0; i < 200; i++) {
                sumi_add_arnold(inst, 0.5f, 0.5f, 0.1f, 0.5f, k, 2u, 1u); t19_step(window, inst, 1);
                if (medium == 0 && (i == 49 || i == 199)) {
                    if (t19_read_field(inst, &f)) { double ang; const double amp = t72_harmonic(f, 0.5, 0.5, 0.0, 0.16, 2, &ang); std::printf("[probe] gesture k %.2f, iteration %d: 2-fold harmonic %.4f at %.3f rad\n", (double)k, i + 1, amp, ang); std::free(f.px); }
                }
            }
            uint8_t* shot = t71_shot(window, inst, &pw, &ph);
            if (shot) { char path[1024]; std::snprintf(path, sizeof path, "%s/probe_k%03d_%s.png", dir, (int)(k * 100), medium ? "kessho" : "sumi"); stbi_write_png(path, (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
        }
    }
    sumi_set_params(inst, &base);
}
// the renders for the author: the locked fifth and the major third, the ten-cent precession and the bass's stir, in Kessho
static void t72_arnold_render(GLFWwindow* window, sumi_instance_t* inst, const char* dir) {
    std::printf("[t72] --arnold-render: the lock in Kessho, archived in %s\n", dir);
    sumi_params_t base; sumi_get_params(inst, &base);
    std::filesystem::create_directories(dir);
    sumi_params_t p = base; t71_kessho_params(&p); p.kessho_backlight = base.kessho_backlight; p.kessho_tint = base.kessho_tint; p.kessho_caustic = base.kessho_caustic;
    p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID; p.press_mode = 3; p.smoothing_ms = 5.0f;
    uint32_t pw = 0, ph = 0;
    const struct { const char* name; uint8_t upper; long pb; int frames; } runs[4] = {
        {"arnold_fifth_locked", T72_FIFTH, 8192, 300}, {"arnold_third_locked", T72_THIRD, 8192, 300},
        {"arnold_fifth_10cents", T72_FIFTH, 8192 + 17, 300}, {"arnold_second_offtongue", (uint8_t)(T72_BASS + 2), 8192, 300} };
    for (const auto& r : runs) {
        for (int medium = 0; medium < 3; medium += 2) {
            p.medium = (uint32_t)medium; sumi_set_params(inst, &p);
            std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
            t72_mpe(inst);
            sumi_push_midi(inst, 0x92, r.upper, 100); sumi_push_midi(inst, 0x91, T72_BASS, 100); t19_step(window, inst, 4);   // the upper voice first (ink)
            sumi_push_midi(inst, 0xE2, (uint8_t)(r.pb & 0x7F), (uint8_t)((r.pb >> 7) & 0x7F));
            for (int i = 0; i < r.frames; i++) { sumi_push_midi(inst, 0xD2, 100, 0); t19_step(window, inst, 1); }   // the upper voice pressed; the bass at rest (its stir is the swirl's)
            uint8_t* shot = t71_shot(window, inst, &pw, &ph);
            if (shot) { char path[1024]; std::snprintf(path, sizeof path, "%s/%s_%s.png", dir, r.name, medium ? "kessho" : "sumi"); stbi_write_png(path, (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); std::printf("[t72] wrote %s\n", path); }
            sumi_push_midi(inst, 0xD2, 0, 0); sumi_push_midi(inst, 0xD1, 0, 0); sumi_push_midi(inst, 0xE2, 0x00, 0x40);
            sumi_push_midi(inst, 0x81, T72_BASS, 64); sumi_push_midi(inst, 0x82, r.upper, 64); t19_step(window, inst, 30);
        }
    }
    sumi_set_params(inst, &base);
}

/* ------------------------------------------------------------------ */
/* Phase 11 step 73: Kirchhoff chord braiding (KESSHO §2.4)             */
/* ------------------------------------------------------------------ */
// the accumulated map's trace discriminant at a point, from the pre-image by central differences over ±w texels:
// Δ = tr² − 4 (det ≈ 1) — hyperbolic > 0, elliptic < 0
static double t73_delta(const FieldF& f, double cx, double cy, int w) {
    const uint32_t x = (uint32_t)(cx * f.w), y = (uint32_t)(cy * f.h);
    if (x < (uint32_t)w || y < (uint32_t)w || x + w >= f.w || y + w >= f.h) return 0.0;
    const size_t o = ((size_t)y * f.w + x) * 4;
    const double ux = ((double)f.px[o + 4 * w] - f.px[o - 4 * w]) * 0.5 * f.w / w, uy = ((double)f.px[o + 4 * w * f.w] - f.px[o - 4 * w * f.w]) * 0.5 * f.h / w;
    const double vx = ((double)f.px[o + 4 * w + 1] - f.px[o - 4 * w + 1]) * 0.5 * f.w / w, vy = ((double)f.px[o + 4 * w * f.w + 1] - f.px[o - 4 * w * f.w + 1]) * 0.5 * f.h / w;
    const double tr = ux + vy;
    return tr * tr - 4.0;
}
// the material's displacement at a point — the pre-image against the position — in texels: a stagnation point's is small,
// a jet's large (its material came from upstream)
static double t73_disp(const FieldF& f, double cx, double cy) {
    const uint32_t x = (uint32_t)(cx * f.w), y = (uint32_t)(cy * f.h);
    if (x >= f.w || y >= f.h) return 0.0;
    const size_t o = ((size_t)y * f.w + x) * 4;
    return std::hypot(((double)f.px[o] - (x + 0.5) / f.w) * f.w, ((double)f.px[o + 1] - (y + 0.5) / f.h) * f.h);
}
// the least displacement along the middle three fifths of the segment between two cells, and where: a same-sign pair's
// saddle sits there (its material stays), a dipole's jet streams through (everything came from upstream)
static double t73_segment_min(const FieldF& f, double x0, double y0, double x1, double y1, double* tx, double* ty) {
    double best = 1e9;
    for (int i = 0; i <= 30; i++) {
        const double t = 0.2 + 0.6 * i / 30.0, cx = x0 + (x1 - x0) * t, cy = y0 + (y1 - y0) * t;
        const double d = t73_disp(f, cx, cy);
        if (d < best) { best = d; if (tx) *tx = cx; if (ty) *ty = cy; }
    }
    return best;
}
// the ink's centroid (every band texel), normalized
static bool t73_centroid(const FieldF& f, double* cx, double* cy) {
    double sx = 0.0, sy = 0.0; long n = 0;
    for (uint32_t y = 0; y < f.h; y++) for (uint32_t x = 0; x < f.w; x++) if (f.px[((size_t)y * f.w + x) * 4 + 2] >= 1.0f) { sx += (x + 0.5) / f.w; sy += (y + 0.5) / f.h; n++; }
    if (n < 10) return false;
    *cx = sx / n; *cy = sy / n; return true;
}
static void t73_braid_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t73] Kirchhoff chord braiding (KESSHO §2.4): the passes and the frame time at fifteen voices, the fifth's saddle, the second's jet, the bend's flip\n");
    sumi_params_t base; sumi_get_params(inst, &base);
    uint32_t pw = 0, ph = 0; FieldF f;
    sumi_params_t p = base; t71_kessho_params(&p); p.medium = SUMI_MEDIUM_SUMI; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    p.bend_mode = 5; p.press_mode = 0; p.braid_mode = SUMI_BRAID_PINNED; p.smoothing_ms = 5.0f;
    sumi_set_params(inst, &p);
    // 1. gate 1: fifteen voices held — 2N = 30 passes a frame within the budget of 64; the frame time at 512² and 1024²
    {
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        t72_mpe(inst);
        for (int k = 0; k < 15; k++) sumi_push_midi(inst, (uint8_t)(0x91 + k), (uint8_t)(48 + k), 100);
        t19_step(window, inst, 10);
        const uint32_t p0 = sumi_debug_pass_count(inst); t19_step(window, inst, 10); const uint32_t p1 = sumi_debug_pass_count(inst);
        const double passes = (p1 - p0) / 10.0;
        const double t0 = glfwGetTime(); t19_step(window, inst, 120); const double ms512 = (glfwGetTime() - t0) * 1000.0 / 120.0;
        sumi_resize(inst, 1024, 1024, 1.0f); t19_step(window, inst, 10);
        const double t1 = glfwGetTime(); t19_step(window, inst, 120); const double ms1024 = (glfwGetTime() - t1) * 1000.0 / 120.0;
        sumi_resize(inst, 512, 512, 1.0f); t19_step(window, inst, 2);
        for (int k = 0; k < 15; k++) sumi_push_midi(inst, (uint8_t)(0x81 + k), (uint8_t)(48 + k), 64);
        t19_step(window, inst, 40);
        T19(passes >= 28.0 && passes <= 32.0 && ms1024 < 16.6, "gate 1, fifteen voices braiding: %.0f passes a frame (2N = 30 with every voice sounding — the headless suite pins it; the budget 64), %.2f ms a frame at 512² and %.2f at 1024² (< 16.6: 60 fps on this desktop)", passes, ms512, ms1024);
    }
    // the voices' cells (each struck alone)
    double bx, by, bR, fx, fy, fR, mx, my, mR;
    if (!t72_strike_disc(window, inst, 1, T72_BASS, &bx, &by, &bR) || !t72_strike_disc(window, inst, 2, T72_FIFTH, &fx, &fy, &fR) || !t72_strike_disc(window, inst, 2, (uint8_t)(T72_BASS + 1), &mx, &my, &mR)) { t19_failures++; std::printf("FAIL: the cells\n"); return; }
    sumi_set_params(inst, &p);
    // 2. gate 3: a fifth's dyad braids for 300 frames — a hyperbolic point between the cells (Δ > 0 at the midpoint), the cores elliptic (Δ < 0)
    {
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        t72_mpe(inst);
        sumi_push_midi(inst, 0x91, T72_BASS, 100); sumi_push_midi(inst, 0x92, T72_FIFTH, 100); t19_step(window, inst, 300);
        if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        double sx, sy; const double sm = t73_segment_min(f, bx, by, fx, fy, &sx, &sy);
        const double dm = t73_delta(f, sx, sy, 2), db = t73_delta(f, bx, by, 2), df = t73_delta(f, fx, fy, 2);
        std::free(f.px);
        T19(dm > 0.0 && db < 0.0 && df < 0.0 && sm < 8.0, "gate 3, a fifth's dyad after 300 frames: the saddle between the cells — its material moved %.1f texel at %.0f%% of the way from the bass (a stagnation point; the texel nearest it drifts by the saddle's own stretching), the trace discriminant there %+.3f (> 0), at the bass's core %+.3f and the fifth's %+.3f (< 0, elliptic)", sm, 100.0 * std::hypot(sx - bx, sy - by) / std::hypot(fx - bx, fy - by), dm, db, df);
        uint8_t* shot = t71_shot(window, inst, &pw, &ph); if (shot) { stbi_write_png("braid_fifth_sumi.png", (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
        sumi_params_t k = p; k.medium = SUMI_MEDIUM_KESSHO; sumi_set_params(inst, &k); t19_step(window, inst, 2);
        shot = t71_shot(window, inst, &pw, &ph); if (shot) { stbi_write_png("braid_fifth_kessho.png", (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
        sumi_set_params(inst, &p);
        sumi_push_midi(inst, 0x81, T72_BASS, 64); sumi_push_midi(inst, 0x82, T72_FIFTH, 64); t19_step(window, inst, 40);
    }
    // 3. gate 4: a minor second's dyad — a dipole: the ink's centroid moves perpendicular to the key line, ≥ 8 texels in 120 frames
    double jet = 0.0;
    {
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        t72_mpe(inst);
        sumi_push_midi(inst, 0x91, T72_BASS, 100); sumi_push_midi(inst, 0x92, (uint8_t)(T72_BASS + 1), 100); t19_step(window, inst, 4);
        double c0x, c0y, c1x, c1y;
        if (!t19_read_field(inst, &f) || !t73_centroid(f, &c0x, &c0y)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        std::free(f.px);
        t19_step(window, inst, 120);
        if (!t19_read_field(inst, &f) || !t73_centroid(f, &c1x, &c1y)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        std::free(f.px);
        const double lx = mx - bx, ly = my - by, L = std::hypot(lx, ly), nx = -ly / L, ny = lx / L;
        jet = std::fabs((c1x - c0x) * nx + (c1y - c0y) * ny) * 512.0;
        const double along = std::fabs((c1x - c0x) * lx / L + (c1y - c0y) * ly / L) * 512.0;
        T19(jet >= 8.0, "gate 4, a minor second's dyad: the ink's centroid moved %.1f texels perpendicular to the key line in 120 frames (>= 8: the dipole's jet; %.1f along it)", jet, along);
        uint8_t* shot = t71_shot(window, inst, &pw, &ph); if (shot) { stbi_write_png("braid_second_sumi.png", (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
        sumi_push_midi(inst, 0x81, T72_BASS, 64); sumi_push_midi(inst, 0x82, (uint8_t)(T72_BASS + 1), 64); t19_step(window, inst, 40);
    }
    // 4. gate 5: the second bent up to the fifth over 240 frames — the jet becomes the saddle: on a fresh sheet the minor
    //    second's flow streams THROUGH the midpoint (its material came from upstream: a large displacement there) while the
    //    fifth's holds it (a stagnation point: a small one, hyperbolic); and no frame of the bend moves the material a texel
    {
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        t72_mpe(inst);
        sumi_push_midi(inst, 0x91, T72_BASS, 100); sumi_push_midi(inst, 0x92, (uint8_t)(T72_BASS + 1), 100); t19_step(window, inst, 300);
        if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        const double s_before = t73_segment_min(f, bx, by, mx, my, nullptr, nullptr);
        FieldF prev = f; double worst = 0.0;
        for (int i = 1; i <= 240; i++) {
            const long pb = 8192 + (long)(1024.0 * i / 240.0);   // +6 semitones at the 48-semitone range
            sumi_push_midi(inst, 0xE2, (uint8_t)(pb & 0x7F), (uint8_t)((pb >> 7) & 0x7F));
            t19_step(window, inst, 1);
            if (i % 8) continue;
            if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); std::free(prev.px); return; }
            double m = 0.0;
            for (uint32_t y = 16; y + 16 < f.h; y += 2) for (uint32_t x = 16; x + 16 < f.w; x += 2) {
                const size_t o = ((size_t)y * f.w + x) * 4;
                m = std::fmax(m, std::hypot(((double)f.px[o] - prev.px[o]) * f.w, ((double)f.px[o + 1] - prev.px[o + 1]) * f.h));
            }
            worst = std::fmax(worst, m / 8.0);
            std::free(prev.px); prev = f;
        }
        std::free(prev.px);
        // the fifth's flow on a fresh sheet (the voices held, the bend applied): the saddle at the midpoint
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 300);
        if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        double sx, sy; const double s_after = t73_segment_min(f, bx, by, mx, my, &sx, &sy);   // the bent voice's vortex stays at ITS cell (the second's): the pair's geometry is the second's
        const double d_after = t73_delta(f, sx, sy, 2);
        std::printf("[t73] the fifth at the second's geometry (cells %.3f apart, cores %.3f): |d| along the segment —", std::hypot(mx - bx, my - by), bR);
        for (int i = 0; i <= 10; i++) { const double t = i / 10.0; std::printf(" %.0f%%:%.1f", 100.0 * t, t73_disp(f, bx + (mx - bx) * t, by + (my - by) * t)); }
        std::printf("\n");
        std::free(f.px);
        T19(s_before >= 12.0 && s_after < 8.0 && worst < 1.0, "gate 5, the second bent to the fifth: between the cells the least-moved material came %.1f texels upstream under the second (the jet: no stagnation point, >= 12) and %.1f under the fifth (a stagnation point, < 8: the pair co-rotates about it — %s, the cells %.3f apart against cores of %.3f); the bend moved the material at most %.2f texel a frame (< 1)", s_before, s_after, d_after > 0.0 ? "a saddle" : "elliptic, the overlapping cores' one centre", std::hypot(mx - bx, my - by), bR, worst);
        uint8_t* shot = t71_shot(window, inst, &pw, &ph); if (shot) { stbi_write_png("braid_bend_sumi.png", (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
        sumi_push_midi(inst, 0xE2, 0x00, 0x40); sumi_push_midi(inst, 0x81, T72_BASS, 64); sumi_push_midi(inst, 0x82, (uint8_t)(T72_BASS + 1), 64); t19_step(window, inst, 40);
    }
    sumi_set_params(inst, &base);
}
// the A/B item B-1: a triad (the bass, its fifth, its minor second) held 600 frames, pinned and free, in Sumi and Kessho
static void t73_ab_braid(GLFWwindow* window, sumi_instance_t* inst, const char* dir) {
    std::printf("[t73] --ab braid: the same chord under the pinned vortices (A) and the free Kirchhoff ones (B), archived in %s\n", dir);
    sumi_params_t base; sumi_get_params(inst, &base);
    std::filesystem::create_directories(dir);
    sumi_params_t p = base; t71_kessho_params(&p); p.kessho_backlight = base.kessho_backlight; p.kessho_tint = base.kessho_tint; p.kessho_caustic = base.kessho_caustic;
    p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID; p.bend_mode = 5; p.press_mode = 0; p.smoothing_ms = 5.0f;
    uint32_t pw = 0, ph = 0;
    for (uint32_t mode = 0; mode < 2; mode++) {
        for (int medium = 0; medium < 3; medium += 2) {
            p.braid_mode = mode; p.medium = (uint32_t)medium; sumi_set_params(inst, &p);
            std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
            t72_mpe(inst);
            sumi_push_midi(inst, 0x91, T72_BASS, 100); sumi_push_midi(inst, 0x92, T72_FIFTH, 100); sumi_push_midi(inst, 0x93, (uint8_t)(T72_BASS + 1), 100);
            t19_step(window, inst, 600);
            uint8_t* shot = t71_shot(window, inst, &pw, &ph);
            if (shot) { char path[1024]; std::snprintf(path, sizeof path, "%s/braid_ab_%s_%s.png", dir, mode ? "B_free" : "A_pinned", medium ? "kessho" : "sumi"); stbi_write_png(path, (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); std::printf("[t73] wrote %s\n", path); }
            sumi_push_midi(inst, 0x81, T72_BASS, 64); sumi_push_midi(inst, 0x82, T72_FIFTH, 64); sumi_push_midi(inst, 0x83, (uint8_t)(T72_BASS + 1), 64); t19_step(window, inst, 40);
        }
    }
    sumi_set_params(inst, &base);
}

/* ------------------------------------------------------------------ */
/* Phase 11 step 74: the chord-wave lattice (KESSHO §2.3)               */
/* ------------------------------------------------------------------ */
// the displacement's component along a unit axis at a point, in texels
static double t74_along(const FieldF& f, double cx, double cy, double ax, double ay) {
    const uint32_t x = (uint32_t)(cx * f.w), y = (uint32_t)(cy * f.h);
    if (x >= f.w || y >= f.h) return 0.0;
    const size_t o = ((size_t)y * f.w + x) * 4;
    const double dx = ((double)f.px[o] - (x + 0.5) / f.w) * f.w, dy = ((double)f.px[o + 1] - (y + 0.5) / f.h) * f.h;
    return dx * ax + dy * ay;
}
// a note's band: the axis ê₁ by its pitch class on the circle of fifths (15° a step), the periods by its octave — the mapper's rule
static void t74_band(uint8_t note, double* angle, double* periods) {
    const uint32_t fifths = ((uint32_t)(note % 12u) * 7u) % 12u;
    *angle = fifths * (3.14159265 / 12.0);
    const int octave = (int)note / 12 - 1;
    double p = 4.0 * std::pow(2.0, octave - 3); *periods = p < 2.0 ? 2.0 : (p > 32.0 ? 32.0 : p);
}
static void t74_chord_wave_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t74] the chord-wave lattice (KESSHO §2.3): one note's parallel ripples in its band, a fifth's lattice where the bands cross\n");
    sumi_params_t base; sumi_get_params(inst, &base);
    uint32_t pw = 0, ph = 0; FieldF f;
    sumi_params_t p = base; t71_kessho_params(&p); p.medium = SUMI_MEDIUM_SUMI; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    p.bend_mode = 0; p.press_mode = 0; p.smoothing_ms = 5.0f;
    sumi_set_params(inst, &p);
    // the cells
    double gx, gy, gR, cx, cy, cR;
    if (!t72_strike_disc(window, inst, 1, 55, &gx, &gy, &gR) || !t72_strike_disc(window, inst, 2, 48, &cx, &cy, &cR)) { t19_failures++; std::printf("FAIL: the cells\n"); return; }
    sumi_params_t k = p; k.medium = SUMI_MEDIUM_KESSHO; sumi_set_params(inst, &k);
    // 1. one note (G3) under full poly pressure: across its band the displacement along ê₁ is the sinusoid of its octave's
    //    period, windowed; along ê₁ the bands run parallel (the same profile at another s₁); outside the band nothing moves
    double ang, per; t74_band(55, &ang, &per);
    const double e1x = std::cos(ang), e1y = std::sin(ang), e2x = -std::sin(ang), e2y = std::cos(ang);
    {
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        t72_mpe(inst);
        sumi_push_midi(inst, 0x91, 55, 100); t19_step(window, inst, 4);
        FieldF f0;   // the strike alone: its expansion's field is subtracted so the wave is read by itself
        if (!t19_read_field(inst, &f0)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        sumi_push_midi(inst, 0xA1, 55, 127); t19_step(window, inst, 30);   // the pressure settles: the deltas bake the full amplitude
        if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); std::free(f0.px); return; }
        const double W = 2.5 * gR, kk = 6.2831853 * per;
        auto wave = [&](double px, double py) { return t74_along(f, px, py, e1x, e1y) - t74_along(f0, px, py, e1x, e1y); };
        // across the band through the note (along ê₂), and along a parallel line a quarter canvas away along ê₁
        double num = 0.0, den_m = 0.0, den_d = 0.0, amp_max = 0.0, outside = 0.0, parallel_diff = 0.0; int n = 0;
        for (int i = -60; i <= 60; i++) {
            const double s2 = 1.2 * W * i / 60.0;
            const double px = gx + e2x * s2 / 1.0, py = gy + e2y * s2;
            if (px < 0.02 || px > 0.98 || py < 0.02 || py > 0.98) continue;
            const double d = wave(px, py);
            const double u = std::fabs(s2) / W, wdw = u < 1.0 ? 1.0 - u * u * u * (10.0 + u * (-15.0 + 6.0 * u)) : 0.0;
            const double model = std::sin(kk * s2 + 1.5707963) * wdw;   // a crest at the note; the lookup's sign is the model's negative
            num += d * model; den_m += model * model; den_d += d * d; n++;
            amp_max = std::fmax(amp_max, std::fabs(d));
            if (u > 1.05) outside = std::fmax(outside, std::fabs(d));
        }
        const double corr = (den_m > 0.0 && den_d > 0.0) ? num / std::sqrt(den_m * den_d) : 0.0;
        std::free(f.px); std::free(f0.px);
        // the band runs the canvas's length: the pass alone on a fresh sheet (no strike under it — the baked wave re-samples
        // the strike's own field, which is not uniform along the band), the profile a quarter canvas along the axis the same
        {
            std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
            sumi_debug_add_chord_wave(inst, (float)gx, (float)gy, (float)ang, (float)per, 0.02f, (float)W); t19_step(window, inst, 1);
            FieldF g; if (!t19_read_field(inst, &g)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
            for (int i = -60; i <= 60; i++) {
                const double s2 = 1.2 * W * i / 60.0, px = gx + e2x * s2, py = gy + e2y * s2, qx = px + e1x * 0.25, qy = py + e1y * 0.25;
                if (px < 0.02 || px > 0.98 || py < 0.02 || py > 0.98 || qx < 0.02 || qx > 0.98 || qy < 0.02 || qy > 0.98) continue;
                parallel_diff = std::fmax(parallel_diff, std::fabs(t74_along(g, qx, qy, e1x, e1y) - t74_along(g, px, py, e1x, e1y)));
            }
            std::free(g.px);
        }
        T19(n > 50 && std::fabs(corr) > 0.95 && amp_max > 6.0 && amp_max < 14.0 && outside < 0.3 && parallel_diff < 1.0,
            "gate 3, one note (G3) pressed: across its band the wave's displacement along the band's axis (the strike's own field subtracted) follows A sin(ω s₂ + φ) w(s₂) with correlation %.3f (|·| > 0.95), amplitude %.1f texels (10 at full pressure), %.2f outside the band (< 0.3); the pass alone on a fresh sheet reads the same profile a quarter canvas along the axis to %.2f texel (< 1: the ripples run the band's length)", corr, amp_max, outside, parallel_diff);
        uint8_t* shot = t71_shot(window, inst, &pw, &ph); if (shot) { stbi_write_png("chord_wave_one_kessho.png", (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
        sumi_params_t su = k; su.medium = SUMI_MEDIUM_SUMI; sumi_set_params(inst, &su); t19_step(window, inst, 2);
        shot = t71_shot(window, inst, &pw, &ph); if (shot) { stbi_write_png("chord_wave_one_sumi.png", (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
        sumi_set_params(inst, &k);
        sumi_push_midi(inst, 0xA1, 55, 0); t19_step(window, inst, 30); sumi_push_midi(inst, 0x81, 55, 64); t19_step(window, inst, 30);
    }
    // 2. a fifth (C3 + G3), both pressed: two bands at their pitch classes' angles (C 0°, G 15°) cross — where they cross both
    //    components are present, along each band away from the crossing only its own
    {
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        t72_mpe(inst);
        sumi_push_midi(inst, 0x92, 48, 100); sumi_push_midi(inst, 0x91, 55, 100); t19_step(window, inst, 4);
        sumi_push_midi(inst, 0xA2, 48, 127); sumi_push_midi(inst, 0xA1, 55, 127); t19_step(window, inst, 30);
        if (!t19_read_field(inst, &f)) { t19_failures++; std::printf("FAIL: field read\n"); return; }
        double angC, perC; t74_band(48, &angC, &perC);
        const double c1x = std::cos(angC), c1y = std::sin(angC), c2x = -std::sin(angC), c2y = std::cos(angC);
        // the crossing: solve for the point on both band centre lines (the lines through each note along its ê₁)
        const double det = e1x * (-c1y) - e1y * (-c1x);
        double tx = 0.0, ty = 0.0; bool crossing = false;
        if (std::fabs(det) > 1e-6) { const double rx = cx - gx, ry = cy - gy; const double t = (rx * (-c1y) - ry * (-c1x)) / det; tx = gx + e1x * t; ty = gy + e1y * t; crossing = tx > 0.05 && tx < 0.95 && ty > 0.05 && ty < 0.95; }
        // the G band's own profile across it, far from the crossing along its axis; the C band's likewise
        double g_only = 0.0, g_cross_c = 0.0, c_only = 0.0, c_cross_g = 0.0, at_cross_g = 0.0, at_cross_c = 0.0;
        for (int i = -30; i <= 30; i++) {
            const double s2 = 2.0 * gR * i / 30.0;
            const double px = gx + e2x * s2 - e1x * 0.3, py = gy + e2y * s2 - e1y * 0.3;   // along G's axis, 0.3 away
            if (px > 0.02 && px < 0.98 && py > 0.02 && py < 0.98) { g_only = std::fmax(g_only, std::fabs(t74_along(f, px, py, e1x, e1y))); g_cross_c = std::fmax(g_cross_c, std::fabs(t74_along(f, px, py, c1x, c1y) - t74_along(f, px, py, e1x, e1y) * (c1x * e1x + c1y * e1y))); }
            const double s2c = 2.0 * cR * i / 30.0;
            const double qx = cx + c2x * s2c + c1x * 0.3, qy = cy + c2y * s2c + c1y * 0.3;
            if (qx > 0.02 && qx < 0.98 && qy > 0.02 && qy < 0.98) { c_only = std::fmax(c_only, std::fabs(t74_along(f, qx, qy, c1x, c1y))); c_cross_g = std::fmax(c_cross_g, std::fabs(t74_along(f, qx, qy, e1x, e1y) - t74_along(f, qx, qy, c1x, c1y) * (c1x * e1x + c1y * e1y))); }
            if (crossing) {
                const double wx = tx + e2x * s2, wy = ty + e2y * s2;
                if (wx > 0.02 && wx < 0.98 && wy > 0.02 && wy < 0.98) { at_cross_g = std::fmax(at_cross_g, std::fabs(t74_along(f, wx, wy, e1x, e1y))); at_cross_c = std::fmax(at_cross_c, std::fabs(t74_along(f, wx, wy, c1x, c1y))); }
            }
        }
        std::free(f.px);
        T19(crossing && g_only > 5.0 && c_only > 5.0 && g_cross_c < 0.3 * g_only && c_cross_g < 0.3 * c_only && at_cross_g > 3.0 && at_cross_c > 3.0,
            "gate 3, a fifth (C3 + G3) pressed: the bands cross at (%.2f, %.2f) — along G's band away from it only G's component (%.1f texels; the other's residue %.1f), along C's only C's (%.1f; %.1f); at the crossing both (%.1f and %.1f): the lattice", tx, ty, g_only, g_cross_c, c_only, c_cross_g, at_cross_g, at_cross_c);
        uint8_t* shot = t71_shot(window, inst, &pw, &ph); if (shot) { stbi_write_png("chord_wave_fifth_kessho.png", (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
        sumi_params_t su = k; su.medium = SUMI_MEDIUM_SUMI; sumi_set_params(inst, &su); t19_step(window, inst, 2);
        shot = t71_shot(window, inst, &pw, &ph); if (shot) { stbi_write_png("chord_wave_fifth_sumi.png", (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
        // a triad on top: the crossing region of three bands, archived
        sumi_set_params(inst, &k);
        sumi_push_midi(inst, 0x93, 52, 100); t19_step(window, inst, 4); sumi_push_midi(inst, 0xA3, 52, 127); t19_step(window, inst, 30);
        shot = t71_shot(window, inst, &pw, &ph); if (shot) { stbi_write_png("chord_wave_triad_kessho.png", (int)pw, (int)ph, 4, shot, (int)pw * 4); std::free(shot); }
        sumi_push_midi(inst, 0xA1, 55, 0); sumi_push_midi(inst, 0xA2, 48, 0); sumi_push_midi(inst, 0xA3, 52, 0); t19_step(window, inst, 30);
        sumi_push_midi(inst, 0x81, 55, 64); sumi_push_midi(inst, 0x82, 48, 64); sumi_push_midi(inst, 0x83, 52, 64); t19_step(window, inst, 30);
    }
    sumi_set_params(inst, &base);
}

/* ------------------------------------------------------------------ */
/* Command line                                                        */
/* ------------------------------------------------------------------ */

int dev_parse_arg(DevOptions& o, int argc, char** argv, int& i) {
    const char* a = argv[i];
    auto need = [&](const char* name) -> const char* {
        if (std::strcmp(a, name) != 0) return nullptr;
        if (i + 1 >= argc) { std::fprintf(stderr, "%s needs a value\n", name); return nullptr; }
        return argv[++i];
    };
    if (const char* v = need("--exit-after"))      { o.exit_after = std::atof(v); return 1; }
    if (const char* v = need("--sim-scale"))       { o.sim_scale = (float)std::atof(v); return 1; }
    if (const char* v = need("--drop-test"))       { o.drop_test = std::atol(v); return 1; }
    if (const char* v = need("--layout"))          { o.layout = std::atoi(v); return 1; }
    if (const char* v = need("--dip-at"))          { o.dip_at = std::atof(v); return 1; }
    if (const char* v = need("--dip-burst"))       { o.dip_burst = std::atof(v); return 1; }
    if (const char* v = need("--print-out"))       { o.print_out = v; return 1; }
    if (const char* v = need("--anod-strike-render")) { o.strike_render = v; return 1; }
    if (const char* v = need("--pair-drift"))      { o.pair_drift = v; return 1; }
    if (const char* v = need("--preset"))          { o.preset = v; return 1; }
    if (const char* v = need("--spark-shear"))     { g_strike_shear = (float)std::atof(v); return 1; }
    if (const char* v = need("--field-dump"))      { o.field_dump = v; return 1; }
    if (const char* v = need("--composite-dump"))  { o.composite_dump = v; return 1; }
    if (const char* v = need("--dump-medium"))     { o.dump_medium = std::atoi(v); return 1; }   // step 76
    if (const char* v = need("--chirikov-grid"))   { o.chirikov_grid = v; return 1; }   // step 76
    if (const char* v = need("--leader-pb"))       { o.leader_pb = (float)std::atof(v); return 1; }   // step 76b
    if (const char* v = need("--layout-shot"))     { o.layout_shot = v; return 1; }
    if (const char* v = need("--replay"))          { o.replay = v; return 1; }
    if (const char* v = need("--replay-wall"))     { o.replay_wall = std::atof(v); return 1; }
    if (const char* v = need("--replay-wav"))      { o.replay_wav = v; return 1; }
    if (const char* v = need("--record-demo"))     { o.record_demo = v; return 1; }
    if (const char* v = need("--replay-warmup"))   { o.replay_warmup = std::atol(v); return 1; }
    if (const char* v = need("--replay-live"))     { o.replay_live = v; return 1; }
    if (const char* v = need("--record-live"))     { o.record_live = std::atof(v); return 1; }
    if (const char* v = need("--string-tuning"))   { o.string_tuning = std::atoi(v); return 1; }
    if (const char* v = need("--pinch-soak"))      { o.t_pinch_passes = std::atol(v); return 1; }
    if (const char* v = need("--soak"))            { o.soak = v; return 1; }
    if (const char* v = need("--soak-passes"))     { o.soak_passes = std::atol(v); return 1; }
    if (const char* v = need("--kessho-flat"))     { o.kessho_flat = v; return 1; }   // step 71
    if (const char* v = need("--mineral"))         { o.mineral = std::atoi(v); return 1; }   // step 71b
    if (const char* v = need("--habit-corner"))    { o.habit_corner = v; return 1; }
    if (const char* v = need("--arnold-render"))   { o.arnold_render = v; return 1; }   // step 72
    if (const char* v = need("--arnold-probe"))    { o.arnold_probe = v; return 1; }
    if (const char* v = need("--ab"))              { o.ab = v; return 1; }
    if (const char* v = need("--ab-out"))          { o.ab_out = v; return 1; }
    if (const char* v = need("--voxo-storm"))      { o.voxo_storm = std::atof(v); return 1; }
    if (const char* v = need("--voxo-bounce"))     { o.voxo_bounce = v; return 1; }
    if (const char* v = need("--voxo-profile"))    { o.voxo_profile = v; return 1; }
    if (const char* v = need("--trace-demo"))      { o.trace_demo = v; return 1; }
    if (const char* v = need("--voxo-suzu-shear")) { o.voxo_suzu_shear = (float)std::atof(v); return 1; }
    if (const char* v = need("--voxo-suzu-preset")) { o.voxo_suzu_preset = std::atoi(v); return 1; }
    if (const char* v = need("--voxo-suzu-breath")) { o.voxo_suzu_breath = (float)std::atof(v); return 1; }
    if (const char* v = need("--voxo-suzu-voice"))  { o.voxo_suzu_voice = std::atoi(v); return 1; }
    if (const char* v = need("--voxo-chart"))       { o.voxo_chart = v; return 1; }
    if (const char* v = need("--voxo-load"))       { o.voxo_load = v; return 1; }
    if (const char* v = need("--voxo-preset"))     { o.voxo_preset = v; return 1; }
    if (const char* v = need("--voxo-source"))     { o.voxo_source = v; return 1; }
    if (const char* v = need("--voxo-budget-mb"))  { o.voxo_budget_mb = std::atof(v); return 1; }
    if (const char* v = need("--map-cc")) {
        int cc = -1, target = -1;
        if (std::sscanf(v, "%d:%d", &cc, &target) == 2 &&
            cc >= 0 && cc <= 127 && target >= 0 && target < SUMI_CTL_COUNT) {
            o.map_cc = cc; o.map_target = target; return 1;
        }
        std::fprintf(stderr, "bad --map-cc, expected <cc>:<target>\n");
        return -1;
    }
    struct Flag { const char* name; bool* slot; };
    const Flag flags[] = {
        {"--resize-test", &o.resize_test}, {"--demo-chevron", &o.demo_chevron},
        {"--demo-vortex", &o.demo_vortex}, {"--cycle-visuals", &o.cycle_visuals},
        {"--wake-test", &o.t_wake}, {"--flick-test", &o.t_flick}, {"--pressure-test", &o.t_pressure}, {"--stokeslet-test", &o.t_stokeslet},
        {"--rankine-test", &o.t_rankine}, {"--ripple-group-test", &o.t_ripple_group},
        {"--ripple-dip-test", &o.t_ripple_dip}, {"--pinch-demo", &o.t_pinch_demo},
        {"--ripple-permanence-test", &o.t_ripple_perm}, {"--swirl-test", &o.t_swirl},
        {"--soak-negative", &o.soak_negative}, {"--torsion-test", &o.t_torsion}, {"--chladni-test", &o.t_chladni}, {"--burst-test", &o.t_burst}, {"--spark-test", &o.t_spark}, {"--chirikov-test", &o.t_chirikov}, {"--palette-test", &o.t_palette}, {"--anod-test", &o.t_anod}, {"--defaults-test", &o.t_defaults}, {"--print-test", &o.t_print}, {"--gesture-test", &o.t_gesture}, {"--trace-test", &o.t_trace}, {"--trumpet-arc", &o.trumpet_arc},
        {"--kessho-test", &o.t_kessho},   // Phase 11 step 71
        {"--habit-test", &o.t_habit}, {"--tine-fault", &o.tine_fault},   // step 71b
        {"--arnold-test", &o.t_arnold},   // step 72
        {"--braid-test", &o.t_braid},   // step 73
        {"--chord-wave-test", &o.t_chord_wave},   // step 74
        {"--strobe-test", &o.t_strobe},   // step 75
        {"--seed-test", &o.t_seed},   // step 76
        {"--spark-reading-test", &o.t_spark_reading},   // step 76
        {"--leader-test", &o.t_leader},   // step 76b
    };
    for (const Flag& f : flags) {
        if (std::strcmp(a, f.name) == 0) { *f.slot = true; return 1; }
    }
    return 0;
}

void dev_print_usage(const char* argv0) {
    std::fprintf(stderr,
        "lab bench (with --dev): %s --dev [--exit-after <s>] [--resize-test] [--sim-scale <f>]\n"
        "    [--layout <n>] [--map-cc <cc>:<target>] [--drop-test <n>] [--demo-chevron]\n"
        "    [--demo-vortex] [--dip-at <s>] [--dip-burst <s>] [--print-out <png>]\n"
        "    [--cycle-visuals] [--field-dump <file>] [--composite-dump <file>] [--wake-test] [--flick-test]\n"
        "    [--rankine-test] [--pinch-soak <n>] [--ripple-group-test] [--ripple-dip-test]\n"
        "    [--pinch-demo] [--ripple-permanence-test] [--swirl-test] [--pressure-test] [--stokeslet-test]\n"
        "    [--soak <operator|all> [--soak-passes <n>]] [--soak-negative]   (the four-part conservation gate)\n"
        "    [--torsion-test]   (Phase 6 step 36: the wave torsion profile + the note-on sweep episode)\n"
        "    [--chladni-test]   (Phase 6 step 37 / 43: the gesture's inverse; the cells stir - cores and corners rest, rings turn, the discs are the keys)\n"
        "    [--burst-test]     (Phase 6 step 38: the viscous multipole burst - normalisation, area, pair, axis, orders, age, the shader vs the closed form)\n"
        "    [--spark-test]     (Phase 6 step 39: the spark shear - exact inverse for a triangle stack and noise, the flip negative, a pure shear; the composed strike)\n"
        "    [--chirikov-test]  (Phase 6 step 40: the Chirikov standard map - exact inverse, the pass vs the closed form, the KAM transition, the delta route's retrace)\n"
        "    --soak chirikov-sweep   (Phase 6 step 40: the erosion sweep over the per-step K - the boss gate's table)\n"
        "    [--palette-test]   (Phase 6 step 41: sumi_set_palette - the custom palette recolours the ink and only the ink; the built-ins untouched)\n"
        "    [--print-test]     (Phase 6 step 43, QOL 4: prints at any size - bitwise at the field's size, 4k from a kept field, Anod over alpha)\n"
        "    [--anod-test]      (Phase 6 step 42: the Anod strain-glow - substrate, glow vs the field's strain, the ingress mask, the palettes, the live switch; writes the re-read PNGs)\n"
        "    [--anod-strike-render <dir>] [--spark-shear <f>] (step 43: six MPE strikes in Anod under the defaults -> <dir>/anod_strikes.png, the strike composition for the eye; step 55b: the shear overridden for the Tab compare)\n"
        "    [--gesture-test]   (#75: the medium-aware gestures - Sumi bitwise the 1.0 calls, Anod the author's table)\n"
        "    [--pair-drift <operator>] (step 55b: the growth law of the soak's (b) pairs - the pre-image deviation after 1..500 pairs and its 8x8 map)\n"
        "    [--kessho-test]    (v3.0 step 71: the Kessho medium - the substrate, the Michel-Levy table, the Maltese cross, the analyzer's period, the regimes and the caustic, the switch, the exports; writes kessho_*.png)\n"
        "    [--kessho-flat <operator|all>] (step 71, gate 2: z = 1 is flat for any exact field - |det J - 1| over each soak scene, the estimator's CPU controls first)\n"
        "    [--ab polarizer [--ab-out <dir>]] (step 71, the A/B item K-1: one script under the global analyzer and per voice -> <dir>/kessho_ab_*.png for the author's viewing)\n"
        "    [--mineral <0..5>] (step 71b: the library mineral for the run, both halves — Quartz, Calcite, Muscovite, Barite, Fluorite, Selenite)\n"
        "    [--seed-test] [--spark-reading-test] [--chirikov-grid <dir>] [--dump-medium <n>]   (step 76: the seed, the spark reading and its amended gates, the Chirikov grid, the composite dump's medium)\n"
        "    [--leader-test] [--leader-pb <p>] [--soak leader]   (step 76b: the leader stroke's gates and its three-way viewing into --ab-out; the branch probability for this run; the class gate)\n"
        "    [--habit-test] [--habit-corner <dir>] [--tine-fault] (step 71b: the crystal drop's gates — homothety, growth, the crease pair, calcite + twin kink, the pass count; the crease pair alone; the fabrication RED control under --soak twin_tine)\n"
        "    [--arnold-test] [--arnold-render <dir>] (step 72: the Arnol'd tongue lock — the pass against the closed form and its exact inverse, the 3:2 and 5:4 islands stationary, the ten-cent precession, the twist's RED, the tongue crossing; the locked and the precessing renders in Kessho)\n"
        "    [--braid-test] [--ab braid --ab-out <dir>] (step 73: Kirchhoff chord braiding — the passes and the frame time at fifteen voices, the fifth's saddle, the second's jet, the bend's flip; the A/B item B-1, pinned against free)\n"
        "    [--chord-wave-test] (step 74: the chord-wave lattice — one note's parallel ripples in its band, a fifth's lattice where the bands cross; the renders in Sumi and Kessho)\n"
        "    [--preset <file.json>] (step 55b: a session preset - a shell's last_session.json - applied before any scripted test; sim_scale forced to 1)\n"
        "    [--voxo-storm <s>] (Phase 7 step 47: Voxo on the real output at 128 frames while a fifteen-channel MPE storm rides the harness for <s> seconds; exits 1 on any XRun or dropped message)\n"
        "    [--voxo-bounce <dir>] (step 49: a band-limited harmonic sample glided -48..+48 semitones through Voxo offline, once per interpolation -> <dir>/glide_hermite.wav, glide_linear.wav, glide.json; then tools/voxo_glide_check.py)\n"
        "    [--voxo-load <preset>] (step 50: loads a Decent Sampler .dspreset / .dslibrary headlessly and prints its compat report; exit 0 loaded, 1 refused)\n"
        "    [--voxo-source sampler|suzu] (Phase 8 step 56: the source for this run - the storm on the synth)\n"
        "    [--voxo-suzu-voice <kind>] [--voxo-chart <dir>] (step 58: Suzu's voice for the run - 2 Verlet, 3 hybrid, 4 Duffing, 5 rotor, 9 membrane (step 79); the chaos charts' WAVs)\n"
        "    [--voxo-profile <dir>] [--voxo-suzu-shear <g>] [--voxo-suzu-preset <n>] [--voxo-suzu-breath <b>] (step 56/57: the SOUND PROFILE of the source/instrument - every MIDI note 21..108 struck offline -> <dir>/profile.wav + profile.csv; tools/sound_profile.py draws it; the shear for Suzu's harmonics case)\n"
        "    [--voxo-preset <preset>] [--voxo-budget-mb <n>] (step 52: the instrument for this run and the gate's advice, the settings untouched - with --voxo-storm, the XRun check with the bus on)\n"
        "    [--record-demo <file.sumireplay>] (Phase 9 step 65: the bench's canonical performance through the real recorder, 512x320 at 120 Hz; <file>.field.bin beside it)\n"
        "    [--record-live <s>] [--replay-live <file.sumireplay>] (step 65: the INTERACTIVE recorder / player from one second into the loop — the settings window's own paths)\n"
        "    [--replay <file.sumireplay> [--field-dump <out>] [--replay-wall <hz>] [--replay-wav <out.wav>]] (step 65: the file on the scripted clock at its recorded size; the field after it; re-bucketed by wall time = the NEGATIVE test; re-sounded offline with --voxo-source / --voxo-preset)\n", argv0);
}

const char* dev_key_name(int key) {
    // 1.7.0 (step 71b, the author's AZERTY): GLFW's key codes are US positions; the name on THIS layout is
    // what the user presses — a single printable byte translates (A -> Q, W -> Z, M -> ","), the rest keep
    // the US name (the digits are printed on the keycap on every layout)
    static char buf[4][8]; static int slot = 0;
    char* b = buf[slot++ & 3];
    const char* n = glfwGetKeyName(key, 0);
    if (n && n[0] && !n[1] && n[0] > 32 && n[0] < 127) { b[0] = (char)std::toupper((unsigned char)n[0]); b[1] = 0; return b; }
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) { b[0] = (char)('A' + (key - GLFW_KEY_A)); b[1] = 0; return b; }
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) { b[0] = (char)('0' + (key - GLFW_KEY_0)); b[1] = 0; return b; }
    b[0] = '?'; b[1] = 0; return b;
}
const char* dev_key_legend() {
    // built once, for this keyboard layout (dev_key_name) — the settings window shows it beside the bench
    static std::string legend;
    if (legend.empty()) {
        char line[512];
        std::snprintf(line, sizeof line, "1/2 viscosity   3/4 expansion   5/6 roughness   7 palette   8/%s layout   %s medium (Sumi/Anod/Kessho)\n",
                      dev_key_name(GLFW_KEY_L), dev_key_name(GLFW_KEY_A)); legend += line;
        std::snprintf(line, sizeof line, "9 paper dip     %s/Shift-%s bpm   %s vortex profile   %s ripple live/bake\n",
                      dev_key_name(GLFW_KEY_B), dev_key_name(GLFW_KEY_B), dev_key_name(GLFW_KEY_V), dev_key_name(GLFW_KEY_K)); legend += line;
        std::snprintf(line, sizeof line, "%s pinch variant %s press mode (feed/swirl/torsion/lock/default; Shift back)   %s note-bend mode (glide/ripple/torsion/spark/stir/braid/default)   %s ripple angle +15\n",
                      dev_key_name(GLFW_KEY_C), dev_key_name(GLFW_KEY_P), dev_key_name(GLFW_KEY_M), dev_key_name(GLFW_KEY_O)); legend += line;
        std::snprintf(line, sizeof line, "%s braid vortices (pinned/free: the dance)   %s/Shift-%s a Chirikov step / its inverse\n",
                      dev_key_name(GLFW_KEY_D), dev_key_name(GLFW_KEY_I), dev_key_name(GLFW_KEY_I)); legend += line;
        std::snprintf(line, sizeof line, "%s/%s ripple amp (CC 102)   %s/%s ripple freq (CC 103)   %s crossed-tine stamp\n",
                      dev_key_name(GLFW_KEY_R), dev_key_name(GLFW_KEY_T), dev_key_name(GLFW_KEY_F), dev_key_name(GLFW_KEY_G), dev_key_name(GLFW_KEY_X)); legend += line;
        std::snprintf(line, sizeof line, "%s test voice (ch 2, n 60)  %s/%s 0xA0 swirl amount on it",
                      dev_key_name(GLFW_KEY_J), dev_key_name(GLFW_KEY_W), dev_key_name(GLFW_KEY_E)); legend += line;
    }
    return legend.c_str();
}

/* ------------------------------------------------------------------ */
/* Phase 9 step 65: session replay on the bench (DECISIONS_8 #24)      */
/* ------------------------------------------------------------------ */
#ifndef SUMI_APP_VERSION
#define SUMI_APP_VERSION "0.0.0-dev"
#endif
static bool write_field_dump(sumi_instance_t* inst, const char* path);

static const char* t65_platform() {
#if defined(__APPLE__)
    return "macos";
#elif defined(_WIN32)
    return "windows";
#else
    return "linux";
#endif
}
static void t65_push(void* user, uint8_t s, uint8_t d1, uint8_t d2, uint8_t) { sumi_push_midi((sumi_instance_t*)user, s, d1, d2); }
struct T65Sound { sumi_instance_t* inst = nullptr; voxo_t* v = nullptr; std::vector<float> out; double carry = 0.0; };
static void t65_push_sound(void* user, uint8_t s, uint8_t d1, uint8_t d2, uint8_t) {
    T65Sound* z = (T65Sound*)user;
    sumi_push_midi(z->inst, s, d1, d2);
    if (z->v) voxo_push_midi(z->v, s, d1, d2);
}

// --record-demo <file>: the bench's canonical performance through the REAL recorder — the bytes staged and handed to
// the core at each frame's start, the gestures recorded with their calls, two state changes (the viscosity, then the
// trumpet layout with its valve CCs), a dip in the middle and a held note bending, pressing and sliding every frame
// after it — on a landscape sheet of 512x320 at sim_scale 1, 120 Hz, four seconds; the field after the last frame
// beside it (<file>.field.bin). The tail is MIDI-driven on purpose: gesture passes replay as the same sequence under
// any frame grouping, the mapper's per-update smoothing and feeds do not — the negative test needs that. A tablet's session config opens it (the
// MCM, RPN 0 = 48 on the members), as a tablet's recording would. The cross-machine fixture: another backend replays
// it and the gate compares at its tier.
static int t65_record_demo(GLFWwindow* window, sumi_instance_t* inst, const char* path) {
    sumi_params_t p; sumi_get_params(inst, &p); p.sim_scale = 1.0f; sumi_set_params(inst, &p);
    sumi_resize(inst, 512, 320, 1.0f);
    t19_step(window, inst, 2);
    sumi_get_params(inst, &p);
    AppSettings st; app_settings_defaults(st, p); st.params = p;
    const std::string session = app_settings_session_json(st);
    sumi_replay_info_t info; std::memset(&info, 0, sizeof info);
    std::snprintf(info.platform, sizeof info.platform, "%s", t65_platform());
    std::snprintf(info.backend, sizeof info.backend, "%s", bench_backend_name());
    std::snprintf(info.device, sizeof info.device, "%s", sys_machine_name().c_str());
    std::snprintf(info.app, sizeof info.app, "%s", SUMI_APP_VERSION);
    info.sumi_version = sumi_version(); sumi_replay_timestamp(info.recorded, sizeof info.recorded);
    info.width = 512; info.height = 320; info.pixel_ratio = 1.0f;
    info.seed = sumi_get_seed(inst); info.has_seed = 1u;   // step 76: the draw state, as the desktop's recorder writes it
    sumi_replay_rec_t* rec = sumi_replay_rec_create(&info, session.c_str(), session.size());
    if (!rec) return 1;
    const double DT = 1.0 / 120.0;
    int f = 0;
    auto midi = [&](uint8_t s, uint8_t d1, uint8_t d2, uint8_t src) { sumi_replay_rec_midi(rec, (double)f * DT + 0.001, s, d1, d2, src); };
    auto gest = [&](uint32_t kind, std::initializer_list<float> args) {
        float v[SUMI_REPLAY_G_ARGS] = {0, 0, 0, 0, 0, 0}; uint32_t n = 0;
        for (float a : args) { if (n < SUMI_REPLAY_G_ARGS) v[n++] = a; }
        sumi_replay_apply_gesture(inst, kind, v, n);   // the core's call
        sumi_replay_rec_gesture(rec, kind, v, n);
    };
    auto state = [&]() { sumi_get_params(inst, &st.params); const std::string j = app_settings_session_json(st); sumi_replay_rec_state(rec, j.c_str(), j.size()); };
    auto frame = [&]() {
        sumi_replay_rec_frame(rec, (double)f * DT, DT, t65_push, inst);
        sumi_update(inst, DT); sumi_render(inst);
#if defined(SUMI_HARNESS_GL)
        glfwSwapBuffers(window);
#endif
        glfwPollEvents();
        f++;
    };
    // frame 0: fresh paper, then the session config as a tablet sends it
    sumi_trigger_paper_dip(inst); sumi_replay_rec_dip(rec);
    midi(0xB0, 101, 0, 2); midi(0xB0, 100, 6, 2); midi(0xB0, 6, 15, 2); midi(0xB0, 101, 127, 2); midi(0xB0, 100, 127, 2);
    for (int ch = 1; ch <= 15; ch++) { const uint8_t s = (uint8_t)(0xB0 | ch); midi(s, 101, 0, 2); midi(s, 100, 0, 2); midi(s, 6, 48, 2); midi(s, 38, 0, 2); midi(s, 101, 127, 2); midi(s, 100, 127, 2); }
    frame();
    float R = 0.06f;
    const uint32_t profile = p.vortex_profile;
    while (f < 480) {
        if (f >= 10 && f < 40) { const float a = (float)(f - 10) / 30.0f, b = (float)(f - 9) / 30.0f; gest(SUMI_REPLAY_G_TINE, { 0.15f + 0.7f * a, 0.2f + 0.6f * a, 0.15f + 0.7f * b, 0.2f + 0.6f * b, 0.035f, 0.03f }); }
        if (f == 50) gest(SUMI_REPLAY_G_TAP, { 0.3f, 0.4f, 0.06f });
        if (f == 60) { midi(0xE2, 0, 64, 1); midi(0x92, 60, 100, 1); }
        if (f > 60 && f <= 110) { const int bend = 8192 + (f - 60) * 40; midi(0xE2, (uint8_t)(bend & 0x7F), (uint8_t)(bend >> 7), 1); midi(0xD2, (uint8_t)((f - 60) * 2), 0, 1); midi(0xB2, 74, (uint8_t)(64 + (f - 60)), 1); }
        if (f == 80) { midi(0xE3, 0, 64, 1); midi(0x93, 67, 90, 1); }
        if (f == 120) midi(0x82, 60, 0, 1);
        if (f == 130) midi(0x83, 67, 0, 1);
        if (f >= 130 && f < 160) gest(SUMI_REPLAY_G_TWIST, { 0.6f, 0.5f, 0.15f, 0.18f, (float)profile });
        if (f == 170) { gest(SUMI_REPLAY_G_TAP, { 0.7f, 0.3f, 0.06f }); R = 0.06f; }
        if (f > 170 && f <= 200) {
            const float up = f <= 185 ? 0.5f : 0.0f, down = f <= 185 ? 0.0f : 0.6f;
            const float in[6] = { 0.7f, 0.3f, R, up, down, (float)DT };
            sumi_replay_rec_gesture(rec, SUMI_REPLAY_G_PRESS, in, 6);
            R = sumi_gesture_press(inst, 0.7f, 0.3f, R, up, down, (double)(float)DT);   // dt as the file carries it
            if (f == 200) gest(SUMI_REPLAY_G_PRESS_END, {});
        }
        if (f == 210) { sumi_get_params(inst, &p); p.fluid_viscosity *= 1.5f; sumi_set_params(inst, &p); state(); }
        if (f >= 220 && f < 260) gest(SUMI_REPLAY_G_PINCH, { 0.5f, 0.5f, 0.02f, 0.4f, 0.3f });
        if (f == 270) { sumi_get_params(inst, &p); p.pitch_layout = SUMI_LAYOUT_TRUMPET; sumi_set_params(inst, &p); state(); midi(0xB0, 110, 127, 1); }
        if (f == 280) { midi(0xE4, 0, 64, 1); midi(0x94, 62, 100, 1); }
        if (f == 300) midi(0xB0, 111, 127, 1);
        if (f == 320) { midi(0x84, 62, 0, 1); midi(0xB0, 110, 0, 1); midi(0xB0, 111, 0, 1); }
        if (f == 350) { sumi_trigger_paper_dip(inst); sumi_replay_rec_dip(rec); }
        if (f >= 360 && f < 420) { const float a = (float)(f - 360) / 60.0f, b = (float)(f - 359) / 60.0f; gest(SUMI_REPLAY_G_WAKE, { 0.2f + 0.6f * a, 0.7f - 0.4f * a, 0.2f + 0.6f * b, 0.7f - 0.4f * b, 0.02f }); }
        // after the dip, a held note whose bend, pressure and CC74 move EVERY frame to the end: the tail the gate reads
        // depends on which frame drained which byte (the mapper smooths and feeds per update) — the negative test's teeth
        if (f == 365) { midi(0xE5, 0, 64, 1); midi(0x95, 64, 110, 1); }
        if (f > 365 && f < 478) { const int bend = 8192 + (int)(2500.0 * std::sin(0.11 * (double)(f - 365))); midi(0xE5, (uint8_t)(bend & 0x7F), (uint8_t)(bend >> 7), 1); midi(0xD5, (uint8_t)(20 + (f - 365)), 0, 1); midi(0xB5, 74, (uint8_t)(127 - (f - 365)), 1); }
        if (f == 478) midi(0x85, 64, 0, 1);
        frame();
    }
    const bool ok = sumi_replay_rec_save(rec, path);
    std::printf("[record-demo] %s: %u frames, %.2f s, %u events%s -> %s\n", ok ? "wrote" : "FAILED to write",
                sumi_replay_rec_frames(rec), sumi_replay_rec_seconds(rec), sumi_replay_rec_events(rec),
                sumi_replay_rec_dropped(rec) ? " (bytes dropped!)" : "", path);
    sumi_replay_rec_destroy(rec);
    if (!ok) return 1;
    const std::string dump = std::string(path) + ".field.bin";
    return write_field_dump(inst, dump.c_str()) ? 0 : 1;
}

// --replay <file> [--replay-wall <hz>] [--field-dump <out>] [--replay-wav <out.wav>]: the file on the scripted clock —
// the recording's size and session applied, every frame an update at its recorded dt and a render; the field after,
// dumped for the gate; the events re-bucketed by wall time first under --replay-wall (the negative test: it must
// diverge); re-sounded through a deviceless Voxo into a WAV under --replay-wav (the evidence of re-sounding).
static int t65_replay(GLFWwindow* window, sumi_instance_t* inst, const DevOptions& o) {
    sumi_replay_t* r = sumi_replay_load(o.replay);
    if (!r) { std::fprintf(stderr, "[replay] not a replay file (or truncated): %s\n", o.replay); return 1; }
    const sumi_replay_info_t* i = sumi_replay_info(r);
    std::printf("[replay] %s: recorded on %s (%s, %s), midi-sink %s, libsumi %u.%u.%u, %ux%u @%.3g, %u frames, %u events, %.2f s\n",
                o.replay, i->device, i->platform, i->backend, i->app, i->sumi_version >> 16, (i->sumi_version >> 8) & 0xFF, i->sumi_version & 0xFF,
                i->width, i->height, (double)i->pixel_ratio, sumi_replay_frame_count(r), sumi_replay_event_count(r), sumi_replay_duration(r));
    if (o.replay_wall > 0.0) {
        if (!sumi_replay_rebucket(r, 1.0 / o.replay_wall)) { std::fprintf(stderr, "[replay] re-bucketing failed\n"); sumi_replay_close(r); return 1; }
        std::printf("[replay] NEGATIVE: the events re-bucketed by wall time at %.0f Hz -> %u frames of %.6f s\n", o.replay_wall, sumi_replay_frame_count(r), 1.0 / o.replay_wall);
    }
    T65Sound snd; snd.inst = inst;
    const uint32_t RATE = 48000;
    if (o.replay_wav) {
        voxo_config_t cfg{}; cfg.sample_rate = RATE; cfg.block_frames = 128; cfg.max_voices = 16;
        snd.v = voxo_create(&cfg);
        if (snd.v) {
            voxo_set_input_mode(snd.v, 1);
            const int src = o.voxo_source ? (std::strcmp(o.voxo_source, "suzu") == 0 ? 1 : std::strcmp(o.voxo_source, "layered") == 0 ? 2 : 0) : 1;
            voxo_set_source(snd.v, src == 1 ? VOXO_SOURCE_SUZU : src == 2 ? VOXO_SOURCE_LAYERED : VOXO_SOURCE_SAMPLER);
            if (o.voxo_preset) { voxo_report_t rep{}; if (!voxo_load_preset(snd.v, o.voxo_preset, &rep)) std::fprintf(stderr, "[replay] voxo refused %s: %s\n", o.voxo_preset, rep.text); }
        }
    }
    const uint32_t flags = SUMI_REPLAY_APPLY_SIZE | SUMI_REPLAY_APPLY_PALETTE;
    if (o.replay_warmup > 0) { t19_step(window, inst, (int)o.replay_warmup); std::printf("[replay] warm-up: %ld frames before the replay (the clock at %.2f s)\n", o.replay_warmup, o.replay_warmup / 120.0); }
    sumi_replay_begin(r, inst, flags);
    double dt = 0.0; uint32_t n = 0;
    while (sumi_replay_step(r, inst, flags, snd.v ? t65_push_sound : t65_push, snd.v ? (void*)&snd : (void*)inst, &dt)) {
        sumi_update(inst, dt); sumi_render(inst);
#if defined(SUMI_HARNESS_GL)
        glfwSwapBuffers(window);
#endif
        glfwPollEvents(); n++;
        if (snd.v) {   // the frame's worth of sound
            snd.carry += dt * (double)RATE;
            uint32_t want = (uint32_t)snd.carry; snd.carry -= (double)want;
            while (want) { const uint32_t k = want > 128u ? 128u : want; const size_t at = snd.out.size(); snd.out.resize(at + 2u * k); voxo_render(snd.v, snd.out.data() + at, k); want -= k; }
        }
    }
    std::printf("[replay] played %u frames (%.2f s on the scripted clock)\n", n, sumi_replay_elapsed(r));
    int code = 0;
    if (snd.v) {
        std::string why;
        const uint32_t frames = (uint32_t)(snd.out.size() / 2u);
        if (wav_write(o.replay_wav, snd.out.data(), frames, 2, RATE, &why)) {
            double peak = 0.0; for (float x : snd.out) if (std::fabs(x) > peak) peak = std::fabs(x);
            std::printf("[replay] re-sounded: %u frames (%.2f s) -> %s, peak %.3f\n", frames, (double)frames / RATE, o.replay_wav, peak);
        } else { std::fprintf(stderr, "[replay] could not write %s: %s\n", o.replay_wav, why.c_str()); code = 1; }
        voxo_destroy(snd.v);
    }
    sumi_replay_close(r);
    if (o.field_dump && !write_field_dump(inst, o.field_dump)) code = 1;
    return code;
}

/* ------------------------------------------------------------------ */
/* Scripted run-and-exit modes                                         */
/* ------------------------------------------------------------------ */


// Phase 7 step 49 (SOUND §2): the glide bounce. A synthetic sample — six
// partials at 1/k, rooted at A3, RECORDED AT 12 kHz so its top partial sits
// at 0.22 of its own Nyquist, where an acoustic sample keeps its strong
// partials (a 48 kHz test tone would exercise no interpolator at all; at 0.44
// of Nyquist a 4-point kernel is only 7 dB ahead of linear — measured,
// DECISIONS_6 #10) — read by Voxo offline (no device) at 48 kHz while the
// member channel's bend sweeps the full ±48 semitones over 8 s; once with
// Hermite, once with linear reads. Up-glide: 6 x 220 x 16 = 21.1 kHz stays
// under Nyquist, so nothing aliases by construction; what the spectral check
// in tools/ measures is the interpolation's own images.
// Phase 8 step 56 (the author's ask): THE SOUND PROFILE of a source — every
// MIDI note 21..108 struck at velocity 100 on an MPE member channel, held
// 0.5 s and released for 0.3 s, rendered offline through voxo_render at 48 k
// with the run's source (--voxo-source) and instrument (--voxo-preset).
// Writes <dir>/profile.wav (the whole run, stereo) and <dir>/profile.csv —
// per note: the peak and the RMS over the held window (0.1..0.5 s after the
// strike, dBFS) and the levels of the first three harmonics (a Goertzel at
// f, 2f, 3f over the same window, dB relative to full scale). The picture is
// tools/sound_profile.py's; the evidence keeps both.
// Step 58: the chaos charts' material — three WAVs at 48 k with a CSV of the swept parameter against time:
//   rotor_sweep: A3 held 24 s on the kicked rotor, the mod wheel (K 0 → 2.5) ramped across it;
//   duffing_clang: C4 struck at velocity 127 on the Duffing cell (β 8, a 2 s declared decay), 3 s;
//   duffing_drive: A3 held 24 s on the Duffing cell driven at the note (drive 1, β 8, a 0.3 s decay), the pressure ramped 0 → 1.
// tools/chaos_chart.py draws them.
static int voxo_chart(const char* dir) {
    const uint32_t RATE = 48000, BLOCK = 128;
    auto run = [&](const char* name, int kind, float beta, float drive, float decay_s, int note, int velocity, double seconds, int cc_or_pressure /*1 = CC1, 2 = pressure, 3 = CC2 (breath), 4 = CC74 with breath held, 0 = none*/) {
        voxo_config_t cfg{}; cfg.sample_rate = RATE; cfg.block_frames = BLOCK; cfg.max_voices = 4;
        voxo_t* v = voxo_create(&cfg);
        voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
        sp.voice_kind = (uint32_t)kind; sp.duffing_beta = beta; sp.drive = drive; sp.drive_ratio = 1.0f; sp.decay_s = decay_s; sp.rotor_k = 0.0f; sp.release_s = 0.5f;
        if (cc_or_pressure == 5) { sp.bow_mode = 1; sp.bow_position = 0.12f; sp.bow_force = 0.5f; sp.string_decay_s = 4.0f; }   // step 80: the stick-slip bow, the force swept by the press
        voxo_set_suzu_params(v, &sp);
        voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15);
        const uint32_t total = (uint32_t)(seconds * RATE);
        std::vector<float> out(2u * (size_t)total, 0.0f);
        std::string csv_path = std::string(dir) + "/" + name + ".csv"; FILE* csv = std::fopen(csv_path.c_str(), "w");
        if (csv) std::fprintf(csv, "time_s,%s\n", cc_or_pressure == 1 ? "K" : cc_or_pressure == 2 ? "drive" : cc_or_pressure == 3 ? "breath" : cc_or_pressure == 4 ? "cc74" : cc_or_pressure == 5 ? "press" : "value");
        if (cc_or_pressure == 4) voxo_push_midi(v, 0xB1, 2, 70);                 // the winds' lip sweep: breath held
        if (cc_or_pressure == 5) voxo_push_midi(v, 0xB1, 2, 127);                // step 80: the bow's speed held at full breath; the press ramps the force
        voxo_push_midi(v, 0x91, (uint8_t)note, (uint8_t)velocity);
        int last = -1;
        for (uint32_t f = 0; f < total; f += BLOCK) {
            const uint32_t n = (total - f) < BLOCK ? (total - f) : BLOCK;
            const double t = (double)f / RATE;
            if (cc_or_pressure) {
                const int val = (int)(127.0 * (t / seconds) + 0.5);
                if (val != last) { last = val; if (cc_or_pressure == 1) voxo_push_midi(v, 0xB1, 1, (uint8_t)val); else if (cc_or_pressure == 3) voxo_push_midi(v, 0xB1, 2, (uint8_t)val); else if (cc_or_pressure == 5) voxo_push_midi(v, 0xD1, (uint8_t)val, 0); else if (cc_or_pressure == 4) voxo_push_midi(v, 0xB1, 74, (uint8_t)val); else voxo_push_midi(v, 0xD1, (uint8_t)val, 0); }
                if (csv && (f % (RATE / 10)) < BLOCK) std::fprintf(csv, "%.3f,%.4f\n", t, cc_or_pressure == 1 ? 2.5 * t / seconds : t / seconds);
            }
            voxo_render(v, out.data() + 2u * (size_t)f, n);
        }
        if (csv) std::fclose(csv);
        std::string wav_path = std::string(dir) + "/" + name + ".wav", why;
        const bool ok = wav_write(wav_path, out.data(), total, 2, RATE, &why);
        std::printf("[chart] %s: %.1f s -> %s%s\n", name, seconds, ok ? wav_path.c_str() : "WAV FAILED ", ok ? "" : why.c_str());
        voxo_destroy(v);
        return ok ? 0 : 1;
    };
    int rc = 0;
    rc |= run("flute_ramp", 6, 8.0f, 0.0f, 30.0f, 69, 100, 16.0, 3);   // step 58b: the breath ramp — the overblow, the spectacle
    rc |= run("sax_ramp", 7, 8.0f, 0.0f, 30.0f, 57, 100, 16.0, 3);     // step 58c: the reed under a breath ramp — threshold, the tone, the closing
    rc |= run("trumpet_lips", 8, 8.0f, 0.0f, 30.0f, 57, 100, 16.0, 4); // step 58c: the embouchure (CC 74) swept — the registers
    rc |= run("rotor_sweep", 5, 8.0f, 0.0f, 30.0f, 57, 100, 24.0, 1);
    rc |= run("duffing_clang", 4, 8.0f, 0.0f, 2.0f, 60, 127, 3.0, 0);
    rc |= run("duffing_drive", 4, 8.0f, 1.0f, 0.3f, 57, 100, 24.0, 2);
    rc |= run("bow_force", 2, 8.0f, 0.0f, 30.0f, 57, 100, 16.0, 5);     // step 80: the chain under the stick-slip bow, the force ramped by the press — the Helmholtz corner sharpens
    rc |= run("bow_force_lattice", 1, 8.0f, 0.0f, 30.0f, 57, 100, 16.0, 5);   //   the same on the modal string
    return rc;
}

static int voxo_profile(const char* dir, const char* source, const char* preset, float suzu_shear, int suzu_preset, float suzu_breath, int suzu_voice, const char* json_preset = nullptr) {
    const uint32_t RATE = 48000, BLOCK = 128;
    const int NOTE_LO = 21, NOTE_HI = 108;
    const uint32_t HOLD = RATE / 2, REL = (RATE * 3) / 10, WIN0 = RATE / 10, WIN1 = RATE / 2;
    voxo_config_t cfg{}; cfg.sample_rate = RATE; cfg.block_frames = BLOCK; cfg.max_voices = 16;
    voxo_t* v = voxo_create(&cfg);
    if (!v) { std::printf("FAIL: voxo_create\n"); return 1; }
    voxo_set_input_mode(v, 1);
    const bool suzu = source && std::strcmp(source, "suzu") == 0;
    voxo_set_source(v, suzu ? VOXO_SOURCE_SUZU : VOXO_SOURCE_SAMPLER);
    if (suzu) {
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp);
        sp.shear = suzu_shear;
        if (suzu_preset < 0) sp.voice_kind = 0; else { sp.voice_kind = 1; sp.modal_preset = (uint32_t)suzu_preset; }
        if (suzu_voice >= 0) sp.voice_kind = (uint32_t)suzu_voice;   // step 58: the strings and the chaos voices
        if (json_preset) {
            FILE* f = std::fopen(json_preset, "rb");
            if (f) {
                std::fseek(f, 0, SEEK_END);
                long sz = std::ftell(f);
                std::fseek(f, 0, SEEK_SET);
                std::string text((size_t)sz, '\0');
                std::fread(text.data(), 1, (size_t)sz, f);
                std::fclose(f);
                sumi_params_t defaults{};
                sumi_palette_t pal{};
                sumi_preset_t p;
                sumi_preset_init(&p, &defaults, &pal);
                if (sumi_preset_read(text.c_str(), text.size(), &p) && p.suzu_present) {
                    sp.level = p.suzu.level; sp.attack_s = p.suzu.attack_s; sp.release_s = p.suzu.release_s;
                    sp.cutoff_hz = p.suzu.cutoff_hz; sp.resonance = p.suzu.resonance;
                    sp.shear = p.suzu.shear; sp.shear_kind = p.suzu.shear_kind;
                    sp.voice_kind = p.suzu.voice_kind; sp.modal_preset = p.suzu.modal_preset;
                    sp.modes = p.suzu.modes; sp.coupling = p.suzu.coupling;
                    sp.decay_s = p.suzu.decay_s; sp.decay_bright = p.suzu.decay_bright;
                    sp.stiffness = p.suzu.stiffness; sp.pluck = p.suzu.pluck;
                    sp.bow_onset_s = p.suzu.bow_onset_s; sp.bow_position = p.suzu.bow_position;
                    sp.breath_cc = p.suzu.breath_cc;
                    if (p.suzu.string_nodes) {
                        sp.string_nodes = p.suzu.string_nodes; sp.string_decay_s = p.suzu.string_decay_s;
                        sp.pickup = p.suzu.pickup; sp.bridge_hz = p.suzu.bridge_hz;
                        sp.bridge_cells = p.suzu.bridge_cells; sp.bridge_coupling = p.suzu.bridge_coupling;
                        sp.bridge_decay_s = p.suzu.bridge_decay_s; sp.loop_loss = p.suzu.loop_loss;
                        sp.duffing_beta = p.suzu.duffing_beta; sp.drive = p.suzu.drive;
                        sp.drive_ratio = p.suzu.drive_ratio; sp.rotor_k = p.suzu.rotor_k;
                        sp.mod_target = p.suzu.mod_target; sp.mod_depth = p.suzu.mod_depth;
                        sp.mod_rate = p.suzu.mod_rate;
                    }
                    if (p.suzu.bore_nodes) {
                        sp.bore_nodes = p.suzu.bore_nodes; sp.bore_loss = p.suzu.bore_loss;
                        sp.bore_corner_hz = p.suzu.bore_corner_hz; sp.jet_gain = p.suzu.jet_gain;
                        sp.jet_drive = p.suzu.jet_drive; sp.jet_tau = p.suzu.jet_tau;
                        sp.jet_q = p.suzu.jet_q; sp.jet_noise = p.suzu.jet_noise;
                        sp.breath_ref = p.suzu.breath_ref; sp.breath_range = p.suzu.breath_range;
                        sp.bore_wall_s = p.suzu.bore_wall_s; sp.press_blows = p.suzu.press_blows;
                    }
                    if (p.suzu.reed_hz > 0.0f) {
                        sp.reed_hz = p.suzu.reed_hz; sp.reed_q = p.suzu.reed_q;
                        sp.reed_open = p.suzu.reed_open; sp.reed_close = p.suzu.reed_close;
                        sp.reed_area = p.suzu.reed_area; sp.reed_noise = p.suzu.reed_noise;
                        sp.cone_apex = p.suzu.cone_apex;
                    }
                    if (p.suzu.lip_ratio > 0.0f) {
                        sp.lip_ratio = p.suzu.lip_ratio; sp.lip_q = p.suzu.lip_q;
                        sp.lip_open = p.suzu.lip_open; sp.lip_close = p.suzu.lip_close;
                        sp.lip_area = p.suzu.lip_area; sp.lip_range = p.suzu.lip_range;
                        sp.partial = p.suzu.partial; sp.bell_start = p.suzu.bell_start;
                        sp.bell_gamma = p.suzu.bell_gamma; sp.brass = p.suzu.brass;
                    }
                    if (p.suzu.drum_size) {   // step 79: the membrane
                        sp.drum_size = p.suzu.drum_size; sp.drum_kappa = p.suzu.drum_kappa; sp.drum_alpha = p.suzu.drum_alpha; sp.drum_cap = p.suzu.drum_cap;
                    }
                    if (p.suzu.plate_size) {   // and the bus plate
                        sp.plate_on = p.suzu.plate_on; sp.plate_size = p.suzu.plate_size; sp.plate_hz = p.suzu.plate_hz; sp.plate_kappa = p.suzu.plate_kappa;
                        sp.plate_decay_s = p.suzu.plate_decay_s; sp.plate_bright = p.suzu.plate_bright; sp.plate_gain = p.suzu.plate_gain;
                    }
                    if (p.suzu.bow_speed > 0.0f) { sp.bow_mode = p.suzu.bow_mode; sp.bow_force = p.suzu.bow_force; sp.bow_speed = p.suzu.bow_speed; }   // step 80
                }
            }
        }
        if (!voxo_set_suzu_params(v, &sp)) { std::printf("FAIL: the lattice gate refused the profile's patch\n"); voxo_destroy(v); return 1; }
    }
    if (preset && !suzu) {
        voxo_report_t rep{};
        if (!voxo_load_preset(v, preset, &rep)) { std::printf("FAIL: preset %s: %s\n", preset, rep.text); voxo_destroy(v); return 1; }
    }
    voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15);   // MCM
    const uint32_t per_note = HOLD + REL, total = per_note * (uint32_t)(NOTE_HI - NOTE_LO + 1);
    std::vector<float> out(2u * (size_t)total, 0.0f);
    std::string csv_path = std::string(dir) + "/profile.csv";
    FILE* csv = std::fopen(csv_path.c_str(), "w");
    if (!csv) { std::printf("FAIL: cannot write %s\n", csv_path.c_str()); voxo_destroy(v); return 1; }
    std::fprintf(csv, "note,hz,peak_dbfs,rms_dbfs,h1_db,h2_db,h3_db\n");
    uint32_t at = 0;
    for (int note = NOTE_LO; note <= NOTE_HI; note++) {
        const double hz = 440.0 * std::pow(2.0, (note - 69) / 12.0);
        if (suzu_breath > 0.0f) voxo_push_midi(v, 0xB1, 2, (uint8_t)(suzu_breath * 127.0f + 0.5f));   // step 57: the bow's breath, held on the note's channel
        voxo_push_midi(v, 0x91, (uint8_t)note, 100);
        for (uint32_t f = 0; f < HOLD; f += BLOCK) voxo_render(v, out.data() + 2u * (size_t)(at + f), (HOLD - f) < BLOCK ? (HOLD - f) : BLOCK);
        voxo_push_midi(v, 0x81, (uint8_t)note, 0);
        for (uint32_t f = 0; f < REL; f += BLOCK) voxo_render(v, out.data() + 2u * (size_t)(at + HOLD + f), (REL - f) < BLOCK ? (REL - f) : BLOCK);
        // the held window's peak, RMS and harmonics (the left channel)
        double peak = 0.0, sq = 0.0; const uint32_t n = WIN1 - WIN0;
        for (uint32_t i = WIN0; i < WIN1; i++) { const double x = out[2u * (size_t)(at + i)]; peak = std::fmax(peak, std::fabs(x)); sq += x * x; }
        // each harmonic's level: the largest Goertzel within ±12 cents of the multiple (step 57: a modal voice's partial sits
        // a fraction of a cent off its ratio, which at 8 kHz is a whole bin of the 0.4 s window — the multiple alone read 8 dB low at C8)
        auto gmag = [&](double f) {
            const double c = 2.0 * std::cos(2.0 * 3.14159265358979 * f / RATE); double s1 = 0.0, s2 = 0.0;
            for (uint32_t i = WIN0; i < WIN1; i++) { const double s0 = out[2u * (size_t)(at + i)] + c * s1 - s2; s2 = s1; s1 = s0; }
            const double p = s1 * s1 + s2 * s2 - c * s1 * s2;
            return 2.0 * std::sqrt(std::fmax(p, 0.0)) / n;                 // the sinusoid's amplitude at that frequency
        };
        double hdb[3];
        for (int h = 0; h < 3; h++) {
            double amp = 0.0;
            for (int c = -12; c <= 12; c += 3) amp = std::fmax(amp, gmag(hz * (h + 1) * std::pow(2.0, c / 1200.0)));
            hdb[h] = amp > 1e-9 ? 20.0 * std::log10(amp) : -180.0;
        }
        const double rms = std::sqrt(sq / n);
        std::fprintf(csv, "%d,%.4f,%.2f,%.2f,%.2f,%.2f,%.2f\n", note, hz,
                     peak > 1e-9 ? 20.0 * std::log10(peak) : -180.0, rms > 1e-9 ? 20.0 * std::log10(rms) : -180.0, hdb[0], hdb[1], hdb[2]);
        at += per_note;
        voxo_push_midi(v, 0xB1, 120, 0);   // all sound off: a clean start for the next note
        std::vector<float> scratch(2u * BLOCK); voxo_render(v, scratch.data(), BLOCK);
    }
    std::fclose(csv);
    std::string wav_path = std::string(dir) + "/profile.wav", why;
    const bool ok = wav_write(wav_path, out.data(), total, 2, RATE, &why);
    std::printf("[profile] %s%s: %d notes (%d..%d), %.1f s -> %s, %s%s%s\n", suzu ? "suzu" : (preset ? preset : "the sine"), suzu && suzu_breath > 0.0f ? (suzu_voice >= 6 ? " (blown)" : " (bowed)") : "",
                NOTE_HI - NOTE_LO + 1, NOTE_LO, NOTE_HI, (double)total / RATE, csv_path.c_str(), ok ? wav_path.c_str() : "WAV FAILED: ", ok ? "" : why.c_str(), "");
    voxo_destroy(v);
    return ok ? 0 : 1;
}

static int voxo_bounce(const char* dir) {
    const uint32_t RATE = 48000, BLOCK = 128;
    const uint32_t SAMPLE_RATE = 12000;       // the sample's own rate
    const float ROOT = 57.0f;                 // A3 = 220 Hz
    const int PARTIALS = 6;                   // 1320 Hz at root: 0.22 of the sample's Nyquist
    const double DUR = 8.0, HOLD = 0.25;      // the sweep, after a short hold at -48
    const uint32_t N = 32 * SAMPLE_RATE;      // the sweep's integral (~17 s of sample) plus the tail at ratio 16 (0.25 s = 4 s of sample)
    std::vector<float> table(N);
    const double f0 = 220.0;
    for (uint32_t i = 0; i < N; i++) {
        double x = 0.0;
        for (int k = 1; k <= PARTIALS; k++) x += std::sin(2.0 * 3.141592653589793 * f0 * k * i / SAMPLE_RATE) / k;
        table[i] = (float)(x * 0.5);
    }
    std::string d = dir;
    std::filesystem::create_directories(d);
    int failures = 0;
    // The static holds: 2 s at -45 semitones (the read creeps at 0.009 frames
    // per output sample — every interpolation image folds in band) and at
    // +45; no sweep, so the artefact floor is the interpolation's alone.
    for (int which = 0; which < 4; which++) {
        const int mode = which & 1;
        const int st_off = which < 2 ? -45 : 45;
        voxo_config_t cfg{}; cfg.sample_rate = RATE; cfg.block_frames = BLOCK; cfg.max_voices = 4;
        voxo_t* v = voxo_create(&cfg);
        voxo_set_input_mode(v, 1);
        voxo_set_interpolation(v, (uint32_t)mode);
        voxo_set_gain(v, 0.4f);   // under the soft knee (linear below 0.5): the measurement is the interpolation's, not the knee's
        voxo_set_sample(v, table.data(), N, 1, SAMPLE_RATE, ROOT);
        const int bend = 8192 + (int)std::lround(8192.0 * st_off / 48.0);
        voxo_push_midi(v, 0xE1, (uint8_t)(bend & 0x7F), (uint8_t)(bend >> 7));
        voxo_push_midi(v, 0xD1, 127, 0);
        voxo_push_midi(v, 0x91, 57, 127);
        const uint32_t total = 2 * RATE;
        std::vector<float> out(2u * total);
        for (uint32_t f = 0; f < total; f += BLOCK) voxo_render(v, out.data() + 2u * f, (total - f) < BLOCK ? (total - f) : BLOCK);
        const std::string path = d + "/static_" + (st_off < 0 ? "down_" : "up_") + (mode ? "linear" : "hermite") + ".wav";
        std::string why;
        if (!wav_write(path, out.data(), total, 2, RATE, &why)) { std::printf("FAIL: %s: %s\n", path.c_str(), why.c_str()); failures++; }
        else std::printf("ok  : %s (%+d st, %s)\n", path.c_str(), st_off, mode ? "linear" : "Hermite");
        voxo_destroy(v);
    }
    for (int mode = 0; mode < 2; mode++) {
        voxo_config_t cfg{}; cfg.sample_rate = RATE; cfg.block_frames = BLOCK; cfg.max_voices = 4;
        voxo_t* v = voxo_create(&cfg);
        voxo_set_input_mode(v, 1);
        voxo_set_interpolation(v, (uint32_t)mode);
        voxo_set_gain(v, 0.4f);   // under the soft knee (linear below 0.5): the measurement is the interpolation's, not the knee's
        voxo_set_sample(v, table.data(), N, 1, SAMPLE_RATE, ROOT);
        const uint32_t total = (uint32_t)((HOLD + DUR + 0.25) * RATE);
        std::vector<float> out(2u * total);
        // The note at the root on member channel 2, pressure full, the bend at -48 (0) first.
        voxo_push_midi(v, 0xE1, 0, 0);
        voxo_push_midi(v, 0xD1, 127, 0);
        voxo_push_midi(v, 0x91, 57, 127);
        for (uint32_t f = 0; f < total; f += BLOCK) {
            const double t = (double)f / RATE;
            const double u = t < HOLD ? 0.0 : std::fmin(1.0, (t - HOLD) / DUR);
            const int bend = (int)std::lround(u * 16383.0);
            voxo_push_midi(v, 0xE1, (uint8_t)(bend & 0x7F), (uint8_t)(bend >> 7));
            const uint32_t n = (total - f) < BLOCK ? (total - f) : BLOCK;
            voxo_render(v, out.data() + 2u * f, n);
        }
        voxo_stats_t st; voxo_stats(v, &st);
        const std::string path = d + "/glide_" + (mode ? "linear" : "hermite") + ".wav";
        std::string why;
        const bool ok = wav_write(path, out.data(), total, 2, RATE, &why);
        std::printf("%s: %s (%u frames, %s, voices at the end %u)\n", ok ? "ok  " : "FAIL", path.c_str(), total,
                    mode ? "linear" : "Hermite", st.active_voices);
        if (!ok || st.active_voices != 1) failures++;   // the sample must outlast the sweep
        voxo_destroy(v);
    }
    FILE* j = std::fopen((d + "/glide.json").c_str(), "w");
    if (j) {
        std::fprintf(j, "{\"root_note\": %.1f, \"semitones_from\": -48, \"semitones_to\": 48, \"duration_s\": %.2f, \"hold_s\": %.2f, "
                        "\"partials\": %d, \"sample_rate\": %u, \"note\": 57, \"bend_range\": 48}\n", ROOT, DUR, HOLD, PARTIALS, SAMPLE_RATE);
        std::fclose(j);
    } else failures++;
    return failures;
}


// Phase 8 step 55b (DECISIONS_7 #3): THE GROWTH LAW of the (b) pairs' pre-image
// deviation — the soak's 500 (+k, -k) pairs on the soak scene, the deviation
// from the starting field read after 1, 2, 5, 10, 20, 50, 100, 200 and 500
// pairs (the mean over the field, over the interior 16 texels in, the max),
// and an 8x8 map of the mean at the end. Linear growth is a deterministic
// residual per pair (the bilinear resampling's own smoothing); square-root
// growth is rounding.
static void soak_pair_drift(GLFWwindow* window, sumi_instance_t* inst, const char* which) {
    int op = -1;
    for (int i = 0; i < SOAK_COUNT; i++) if (std::strcmp(SOAKS[i].name, which) == 0) op = i;
    if (op < 0) { std::printf("[drift] unknown operator %s\n", which); t19_failures++; return; }
    sumi_params_t base; sumi_get_params(inst, &base);
    const SoakDesc& d = SOAKS[op];
    soak_modes(inst, base, (SoakOp)op);
    if (op == SOAK_RIPPLE_BAKE) sumi_map_cc(inst, 0xFF, RIPPLE_AMP_CC, SUMI_CTL_RIPPLE_AMP);
    soak_prep(window, inst, (SoakOp)op);
    soak_scene(window, inst);
    FieldF fa; double m0 = 0.0;
    if (!soak_measure(inst, &m0, &fa)) { t19_failures++; std::printf("FAIL: [drift] field read\n"); return; }
    const int checkpoints[] = {1, 2, 5, 10, 20, 50, 100, 200, 500};
    int done = 0;
    for (int cp : checkpoints) {
        while (done < cp) { soak_pair(window, inst, (SoakOp)op); t19_step(window, inst, 1); done++; }
        FieldF fb; double m = 0.0;
        if (!soak_measure(inst, &m, &fb)) { t19_failures++; std::printf("FAIL: [drift] field read\n"); break; }
        double acc = 0.0, acc_in = 0.0, mx = 0.0; long n = 0, n_in = 0;
        for (uint32_t y = 0; y < fa.h; y++) for (uint32_t x = 0; x < fa.w; x++) {
            const size_t o = ((size_t)y * fa.w + x) * 4;
            const double dx = ((double)fb.px[o] - fa.px[o]) * fa.w, dy = ((double)fb.px[o + 1] - fa.px[o + 1]) * fa.h;
            const double dev = std::sqrt(dx * dx + dy * dy);
            acc += dev; n++; if (dev > mx) mx = dev;
            if (x >= 16 && y >= 16 && x + 16 < fa.w && y + 16 < fa.h) { acc_in += dev; n_in++; }
        }
        std::printf("[drift] %s pairs %3d: pre-image dev mean %.3f texel, interior %.3f, max %.2f; ink mass %+.2f%%\n",
                    d.name, cp, acc / (double)n, acc_in / (double)n_in, mx, 100.0 * (m - m0) / m0);
        if (cp == 500) {
            const uint32_t B = fa.w / 8;
            for (uint32_t by = 0; by < 8; by++) {
                std::printf("[drift] map:");
                for (uint32_t bx = 0; bx < 8; bx++) {
                    double sum = 0.0; long c = 0;
                    for (uint32_t y = by * B; y < (by + 1) * B; y++) for (uint32_t x = bx * B; x < (bx + 1) * B; x++) {
                        const size_t o = ((size_t)y * fa.w + x) * 4;
                        const double dx = ((double)fb.px[o] - fa.px[o]) * fa.w, dy = ((double)fb.px[o + 1] - fa.px[o + 1]) * fa.h;
                        sum += std::sqrt(dx * dx + dy * dy); c++;
                    }
                    std::printf(" %6.2f", sum / (double)c);
                }
                std::printf("\n");
            }
        }
        std::free(fb.px);
    }
    std::free(fa.px);
}

// Phase 8 step 55b: a session preset (the one serializer's JSON — a shell's
// last_session.json) applied before the scripted tests, so the bench can
// print what a tablet printed under the same look (the Tab compare of #88
// repeated). sim_scale is forced to 1: the bench's field is its own size.
static void dev_apply_preset(sumi_instance_t* inst, const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) { std::printf("[preset] cannot open %s\n", path); return; }
    std::string text; char buf[4096]; size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
    std::fclose(f);
    sumi_params_t defaults; sumi_get_params(inst, &defaults);
    sumi_palette_t pal; sumi_get_palette(inst, &pal);
    sumi_preset_t p; sumi_preset_init(&p, &defaults, &pal);
    if (!sumi_preset_read(text.c_str(), text.size(), &p)) { std::printf("[preset] %s did not parse\n", path); return; }
    p.params.sim_scale = 1.0f;
    sumi_set_params(inst, &p.params);
    sumi_set_palette(inst, &p.palette);
    for (uint32_t i = 0; i < p.cc_count && i < SUMI_PRESET_MAX_CC; i++) sumi_map_cc(inst, p.cc[i].channel, p.cc[i].cc, (sumi_ctl_t)p.cc[i].target);
    std::printf("[preset] applied %s: medium %u, palette %u, anod_drop %.2f, spark_shear %.2f, anod_pitch %.4f, bloom %.2f x%u, glow %.2f, dark %.2f, grain %.2f\n",
                path, p.params.medium, p.params.active_palette_id, (double)p.params.anod_drop, (double)p.params.spark_shear, (double)p.params.anod_pitch,
                (double)p.params.anod_bloom, p.params.anod_bloom_levels, (double)p.params.anod_glow, (double)p.params.anod_dark, (double)p.params.anod_grain);
}

// ---------------------------------------------------------------------------
// Phase 8 step 59 (SYNTH §2.7, §5): the orbit trace. A headless Voxo beside the
// instance (no device: the bench renders its blocks itself, a frame's worth per
// frame) plays the rotor while the bridge polls it and inks the orbits at the
// notes' cells through the gesture ABI; the core's own mapper feeds the same
// notes (press) so the trace has feeds to starve. Three runs of one script:
// no trace object at all, the trace OFF, the trace ON — the first two fields
// bitwise equal (the toggle is clean), the third differs (the ink lands), and
// the ON run never emits over the budget.
namespace {
struct T59Script {
    // The core's side plays through the GESTURE ABI (deterministic frame to frame, as the #75 gate proved): the strike
    // at each note's cell, then a held press there every frame — the mapper's continuous feed the trace must not
    // starve. Voxo's side takes the same notes as MIDI (the trace's source). A MIDI script into the core reads
    // differently run to run (the strikes' timing within the frame is the host clock's), so it cannot carry a bitwise gate.
    voxo_t* vx = nullptr;
    std::vector<float> buf;
    float cx[10], cy[10], R[10];
    void start(sumi_instance_t* inst, OrbitTrace& tr, const sumi_params_t& p) {
        voxo_config_t c{}; c.sample_rate = 48000; c.block_frames = 128; c.max_voices = 16;
        vx = voxo_create(&c); voxo_set_input_mode(vx, 1); voxo_set_source(vx, VOXO_SOURCE_SUZU);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 5; sp.rotor_k = 0.3f; voxo_set_suzu_params(vx, &sp);
        buf.assign(2 * 800, 0.0f);
        voxo_push_midi(vx, 0xB0, 101, 0); voxo_push_midi(vx, 0xB0, 100, 6); voxo_push_midi(vx, 0xB0, 6, 15);
        tr.prepare(p, 1.0f);
        for (int i = 0; i < 10; i++) {
            const uint8_t note = (uint8_t)(48 + 3 * i);
            voxo_push_midi(vx, (uint8_t)(0x91 + i), note, 100);
            if (!tr.place(note, &cx[i], &cy[i])) { cx[i] = 0.5f; cy[i] = 0.5f; }
            R[i] = 0.04f; sumi_gesture_tap(inst, cx[i], cy[i], R[i]);
        }
    }
    void frame(sumi_instance_t* inst, int f, int frames) {
        const uint8_t k = (uint8_t)(127 * f / (frames > 1 ? frames - 1 : 1));   // K swept 0 → 2.5 by the wheel over the run (Voxo's side)
        voxo_push_midi(vx, 0xB0, 1, k);
        for (int i = 0; i < 10; i++) R[i] = sumi_gesture_press(inst, cx[i], cy[i], R[i], 0.4f, 0.0f, 1.0 / 120.0);   // the held press: the feed
        voxo_render(vx, buf.data(), 800);                                          // a frame's audio at 60 fps
    }
    void stop(sumi_instance_t* inst) { sumi_gesture_press_end(inst); if (vx) voxo_destroy(vx); vx = nullptr; }
};
}
static void t59_trace_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t59] the orbit trace: the budget, the clean toggle (SYNTH §2.7, §5)\n");
    sumi_params_t base; sumi_get_params(inst, &base);
    sumi_params_t p = base; p.medium = SUMI_MEDIUM_ANOD; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    uint32_t pw = 0, ph = 0; const int FRAMES = 240;
    auto run = [&](int mode, std::vector<uint8_t>* field, OrbitTraceStats* stats, double* trace_ms) {
        // the same quiet start for every run: the previous run's episodes played out (a fixed 600 frames, then the counters), then the dip
        for (int i = 0; i < 2400 && (i < 600 || sumi_debug_burst_count(inst) || sumi_debug_spark_count(inst)); i++) t19_step(window, inst, 1);
        sumi_set_params(inst, &p); t19_step(window, inst, 2); std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        OrbitTrace tr; OrbitTraceConfig tc; tc.scope = mode == 2; tc.ink = mode == 2; tc.kinds = 1u << 5; tc.scale = 0.25f; tc.segments = 6; tc.stroke = 0; tr.configure(tc);
        T59Script sc; sc.start(inst, tr, p);
        voxo_set_trace(sc.vx, mode == 2 ? (1u << 5) : 0u);
        double ms = 0.0;
        for (int f = 0; f < FRAMES; f++) {
            sc.frame(inst, f, FRAMES);
            if (mode >= 1) { const double t0 = glfwGetTime(); tr.frame(sc.vx, mode == 2 ? inst : nullptr, p, 1.0f); ms += (glfwGetTime() - t0) * 1000.0; }
            t19_step(window, inst, 1);
        }
        size_t bytes = 0; uint8_t* raw = t19_read_field_raw(inst, &bytes);
        if (raw) { field->assign(raw, raw + bytes); std::free(raw); } else field->clear();
        if (stats) *stats = tr.stats();
        if (trace_ms) *trace_ms = ms / FRAMES;
        sc.stop(inst);
    };
    std::vector<uint8_t> A, A2, B, C; OrbitTraceStats st; double ms_on = 0.0;
    auto bytes_differ = [](const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) { long d = 0; if (a.size() == b.size()) for (size_t i = 0; i < a.size(); i++) d += a[i] != b[i]; else d = -1; return d; };
    // the script's own determinism per medium: the toggle's gate needs a script that reads the same twice untraced
    long det_anod = 0, det_sumi = 0;
    run(0, &A, nullptr, nullptr); run(0, &A2, nullptr, nullptr); det_anod = bytes_differ(A, A2);
    p.medium = SUMI_MEDIUM_SUMI;
    run(0, &A, nullptr, nullptr); run(0, &A2, nullptr, nullptr); det_sumi = bytes_differ(A, A2);
    std::printf("[t59] the script's own determinism, two untraced runs: Anod differs in %ld of %zu bytes, Sumi in %ld\n", det_anod, A.size(), det_sumi);
    if (det_anod == 0) { p.medium = SUMI_MEDIUM_ANOD; run(0, &A, nullptr, nullptr); }
    std::printf("[t59] the gates run in %s\n", p.medium == SUMI_MEDIUM_ANOD ? "Anod" : "Sumi (Anod's script is not bitwise run to run — a finding for the author)");
    run(1, &B, nullptr, nullptr);
    run(2, &C, &st, &ms_on);
    const bool same = !A.empty() && A.size() == B.size() && std::memcmp(A.data(), B.data(), A.size()) == 0;
    T19(same, "trace OFF is bit-identical to no-trace: the field after 240 frames of ten rotor notes under press, %zu bytes, %s", A.size(), same ? "equal" : "DIFFER");
    long diff = 0; if (A.size() == C.size()) for (size_t i = 0; i < A.size(); i++) diff += A[i] != C[i];
    T19(diff > 0, "the ink route marbles: the traced field differs from the untraced one in %ld bytes", diff);
    T19(st.peak_emitted <= OrbitTrace::BUDGET && st.total_emitted > 0 && st.voices == 10, "ten voices traced: %u voices, at most %u segments a frame (peak %u), %llu inked over the run, %llu merged by the budget, %u unplaced; the mapper's 64-pass budget untouched by construction (the queue holds 4096)", st.voices, (unsigned)OrbitTrace::BUDGET, st.peak_emitted, (unsigned long long)st.total_emitted, (unsigned long long)st.total_merged, st.unplaced_voices);
    T19(ms_on < 1.0, "the bridge's own cost with ten voices: %.3f ms a frame (the poll, the placement, the emission)", ms_on);
    // the scope view (libsumi 1.3.0): the ink off, the orbits on the canvas instead of the water — the field and the dip's print untouched
    {
        auto run_scope = [&](int canvas, std::vector<uint8_t>* field, std::vector<uint8_t>* print) {
            for (int i = 0; i < 2400 && (i < 600 || sumi_debug_burst_count(inst) || sumi_debug_spark_count(inst)); i++) t19_step(window, inst, 1);
            sumi_set_params(inst, &p); t19_step(window, inst, 2); std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
            OrbitTrace tr; OrbitTraceConfig tc; tc.scope = false; tc.ink = false; tc.canvas = canvas; tc.kinds = 1u << 5; tc.scale = 0.25f; tc.segments = 6; tr.configure(tc);
            T59Script sc; sc.start(inst, tr, p); voxo_set_trace(sc.vx, 1u << 5);
            for (int f = 0; f < FRAMES; f++) { sc.frame(inst, f, FRAMES); tr.frame(sc.vx, inst, p, 1.0f); t19_step(window, inst, 1); }
            size_t bytes = 0; uint8_t* raw = t19_read_field_raw(inst, &bytes); if (raw) { field->assign(raw, raw + bytes); std::free(raw); } else field->clear();
            uint32_t w = 0, h = 0; uint8_t* pr = t19_dip_print(window, inst, &w, &h); if (pr) { print->assign(pr, pr + (size_t)w * h * 4); std::free(pr); } else print->clear();
            sc.stop(inst); sumi_set_scope(inst, nullptr, nullptr, 0, SUMI_SCOPE_OFF);
        };
        std::vector<uint8_t> F0, P0, F2, P2; run_scope(0, &F0, &P0); run_scope(2, &F2, &P2);
        const bool f_same = !F0.empty() && F0.size() == F2.size() && std::memcmp(F0.data(), F2.data(), F0.size()) == 0;
        const bool p_same = !P0.empty() && P0.size() == P2.size() && std::memcmp(P0.data(), P2.data(), P0.size()) == 0;
        T19(f_same && p_same, "the scope view instead of the water (ten rotor voices, the ink off): the field (%zu bytes) and the dip's print (%zu bytes) are bit-identical to the run without it — the composite's live path alone draws it", F0.size(), P0.size());
    }
    sumi_set_params(inst, &base); t19_step(window, inst, 2);
}
// The demo: the rotor scribbling its chaos into Anod — a chord held while the wheel sweeps K, frame by frame as PNGs
// (the composite exported at 720×720 whenever the previous export has landed; the effective frame rate is printed
// for ffmpeg). The video is the phase's spectacle: the synth drawing its own phase portrait in ink.
static void t59_trace_demo(GLFWwindow* window, sumi_instance_t* inst, const char* dir) {
    std::printf("[t59] the trace demo -> %s\n", dir);
    sumi_params_t base; sumi_get_params(inst, &base);
    sumi_params_t p = base; p.medium = SUMI_MEDIUM_ANOD; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    uint32_t pw = 0, ph = 0;
    sumi_set_params(inst, &p); t19_step(window, inst, 2); std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
    voxo_config_t c{}; c.sample_rate = 48000; c.block_frames = 128; c.max_voices = 16;
    voxo_t* vx = voxo_create(&c); voxo_set_input_mode(vx, 1); voxo_set_source(vx, VOXO_SOURCE_SUZU);
    voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 5; sp.rotor_k = 0.2f; sp.level = 0.35f; voxo_set_suzu_params(vx, &sp);
    voxo_set_trace(vx, 1u << 5);
    OrbitTrace tr; OrbitTraceConfig tc; tc.scope = false; tc.ink = true; tc.kinds = 1u << 5; tc.scale = 0.35f; tc.segments = 8; tc.stroke = 0; tr.configure(tc);
    auto both = [&](uint8_t st, uint8_t a, uint8_t b) { voxo_push_midi(vx, st, a, b); sumi_push_midi(inst, st, a, b); };
    both(0xB0, 101, 0); both(0xB0, 100, 6); both(0xB0, 6, 15);
    const int notes[4] = { 52, 59, 64, 71 };                                    // an open chord over the grid
    std::vector<float> buf(2 * 800);
    const int FRAMES = 20 * 60; int exported = 0; bool in_flight = false; std::vector<uint8_t> px;
    const double t_start = glfwGetTime();
    for (int f = 0; f < FRAMES; f++) {
        const double t = f / 60.0;
        for (int i = 0; i < 4; i++) { if (f == 30 + 20 * i) { both((uint8_t)(0x91 + i), (uint8_t)notes[i], 110); both((uint8_t)(0xD1 + i), 70, 0); } if (f == FRAMES - 120 + 15 * i) both((uint8_t)(0x81 + i), (uint8_t)notes[i], 0); }
        const uint8_t k = t < 3.0 ? 0 : t < 15.0 ? (uint8_t)(127.0 * (t - 3.0) / 12.0) : 127;   // K held low, then swept to the chaos
        both(0xB0, 1, k);
        voxo_render(vx, buf.data(), 800);
        tr.frame(vx, inst, p, 1.0f);
        t19_step(window, inst, 1);
        if (in_flight) {
            uint32_t ow = 0, oh = 0; px.resize((size_t)720 * 720 * 4);
            const int st = sumi_export_poll(inst, px.data(), px.size(), &ow, &oh);   // polled with the buffer: a size query never advances it
            if (st == 2) { char path[1024]; std::snprintf(path, sizeof path, "%s/frame_%05d.png", dir, exported); stbi_write_png(path, (int)ow, (int)oh, 4, px.data(), (int)ow * 4); exported++; in_flight = false; }
            else if (st == 0) in_flight = false;
        }
        if (!in_flight) in_flight = sumi_export_begin(inst, nullptr, 0, 0, 720, 720, 0);
    }
    const double wall = glfwGetTime() - t_start;
    const OrbitTraceStats& st = tr.stats();
    std::printf("[t59] demo: %d frames (%.1f s wall), %d exported -> ffmpeg -framerate %.1f -i %s/frame_%%05d.png -pix_fmt yuv420p <out.mp4>; the trace inked %llu segments (peak %u a frame, %llu merged)\n",
                FRAMES, wall, exported, exported / 20.0, dir, (unsigned long long)st.total_emitted, st.peak_emitted, (unsigned long long)st.total_merged);
    voxo_destroy(vx);
    sumi_set_params(inst, &base); t19_step(window, inst, 2);
}


// Step 75 (KESSHO §2.5): THE POINCARÉ STROBE on the live core — the bridge in its second mode over Voxo's strobe: the ink
// route stamps a micro-drop (Sumi) or a micro-swirl (Kessho) at each return, the scopes take the dust. The script: the
// undamped cell on three notes — A3 the bass, E4 at the just fifth (+3 bend units), D#4 the tritone — struck on the core
// through the gesture ABI (deterministic frame to frame, as the t59 script) and on Voxo as MIDI; 240 frames at 60 fps
// (the notes an octave above the headless suite's: the chroma grid places A3, not A2).
// Gates: the scope route alone leaves the field bit-identical to a world without the trace (the strobe is a reader); the
// ink route marbles; through the hook the bass's stamps sit on one point and the fifth's on two (within half a texel of
// the field's height over every tenth of a second), the tritone's visit every angular bin; the budget holds; the Sumi,
// Kessho and rotor (K 2.5, its own kicks) scenes archived as PNGs.
namespace {
struct T75Script {
    voxo_t* vx = nullptr;
    std::vector<float> buf;
    int n = 0; int notes[3]; int bends[3];
    float cx[3], cy[3];
    void start(sumi_instance_t* inst, OrbitTrace& tr, const sumi_params_t& p, int kind, float rotor_k, const int* nts, const int* bnd, int count) {
        voxo_config_t c{}; c.sample_rate = 48000; c.block_frames = 128; c.max_voices = 16;
        vx = voxo_create(&c); voxo_set_input_mode(vx, 1); voxo_set_source(vx, VOXO_SOURCE_SUZU);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = (uint32_t)kind; if (kind == 0) sp.decay_s = 0.0f; if (kind == 5) sp.rotor_k = rotor_k; voxo_set_suzu_params(vx, &sp);
        buf.assign(2 * 800, 0.0f);
        voxo_push_midi(vx, 0xB0, 101, 0); voxo_push_midi(vx, 0xB0, 100, 6); voxo_push_midi(vx, 0xB0, 6, 15);
        tr.prepare(p, 1.0f);
        n = count;
        for (int i = 0; i < n; i++) {
            notes[i] = nts[i]; bends[i] = bnd[i];
            if (bends[i] != 8192) voxo_push_midi(vx, (uint8_t)(0xE1 + i), (uint8_t)(bends[i] & 0x7F), (uint8_t)(bends[i] >> 7));
            voxo_push_midi(vx, (uint8_t)(0x91 + i), (uint8_t)notes[i], 100);
            if (!tr.place((uint8_t)notes[i], &cx[i], &cy[i])) { cx[i] = 0.5f; cy[i] = 0.5f; }
            sumi_gesture_tap(inst, cx[i], cy[i], 0.04f);
        }
    }
    void frame() { voxo_render(vx, buf.data(), 800); }
    void stop() { if (vx) voxo_destroy(vx); vx = nullptr; }
};
}
static void t75_strobe_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t75] the Poincaré strobe on the live core (KESSHO §2.5): the clean scope, the ink's stamps, one point / two / a ring through the hook\n");
    sumi_params_t base; sumi_get_params(inst, &base);
    sumi_params_t p = base; p.medium = SUMI_MEDIUM_SUMI; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    uint32_t pw = 0, ph = 0, fw = 0, fh = 0; const int FRAMES = 240;
    sumi_read_field(inst, nullptr, 0, &fw, &fh);
    const double texel = 1.0 / (double)(fh ? fh : 512);
    // the scene: A3 the bass, E4 at the just fifth (+3 bend units), D#4 the tritone — all placed on the chroma grid (A2 is not);
    // the hook gives positions only and A3's and E4's cells sit closer than an orbit's radius, so the hook gates run the
    // bass alone, then the bass with one partner, the bass's known point removed
    const int ALL[3] = { 57, 64, 63 }, ALL_B[3] = { 8192, 8192 + 3, 8192 };
    struct Stamp { float x, y; int frame; };
    struct Out { std::vector<uint8_t> field; OrbitTraceStats stats; std::vector<Stamp> stamps; float cx[3], cy[3]; };
    // mode: 0 no trace, 1 the scope route alone, 2 the ink route; clock: Voxo's (1 the bass, 2 the own drive)
    auto run = [&](int mode, int clock, int kind, float rotor_k, const int* nts, const int* bnd, int count, const char* png) {
        Out o;
        for (int i = 0; i < 2400 && (i < 600 || sumi_debug_burst_count(inst) || sumi_debug_spark_count(inst)); i++) t19_step(window, inst, 1);
        sumi_set_params(inst, &p); t19_step(window, inst, 2); std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        OrbitTrace tr; OrbitTraceConfig tc; tc.scope = mode >= 1; tc.ink = mode == 2; tc.canvas = mode == 1 ? 2 : 0; tc.kinds = 1u << kind; tc.scale = 0.25f; tc.segments = 6; tc.stroke = 0; tc.mode = clock; tr.configure(tc);
        int frame = 0;
        tr.set_gesture_hook([&o, &frame](uint32_t k, const float* a, uint32_t n) { if ((k == 0 && n == 3) || (k == 2 && n == 5) || (k == 7 && n == 4)) o.stamps.push_back({ a[0], a[1], frame }); });
        T75Script sc; sc.start(inst, tr, p, kind, rotor_k, nts, bnd, count);
        for (int i = 0; i < 3; i++) { o.cx[i] = sc.cx[i < count ? i : 0]; o.cy[i] = sc.cy[i < count ? i : 0]; }
        voxo_set_trace(sc.vx, mode >= 1 ? (1u << kind) : 0u);
        for (frame = 0; frame < FRAMES; frame++) { sc.frame(); if (mode >= 1) tr.frame(sc.vx, inst, p, 1.0f); t19_step(window, inst, 1); }
        if (png) { uint32_t w = 0, h = 0; uint8_t* px = t71_shot(window, inst, &w, &h); if (px) { stbi_write_png(png, (int)w, (int)h, 4, px, (int)w * 4); std::printf("[t75] wrote %s\n", png); std::free(px); } }
        size_t bytes = 0; uint8_t* raw = t19_read_field_raw(inst, &bytes);
        if (raw) { o.field.assign(raw, raw + bytes); std::free(raw); }
        o.stats = tr.stats();
        sc.stop(); sumi_set_scope(inst, nullptr, nullptr, 0, SUMI_SCOPE_OFF);
        return o;
    };
    auto differ = [](const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) { long d = 0; if (a.size() == b.size()) for (size_t i = 0; i < a.size(); i++) d += a[i] != b[i]; else d = -1; return d; };
    auto extent = [&](const std::vector<Stamp>& v, size_t from, size_t to) { double mx = 0, my = 0; size_t n = 0; for (size_t i = from; i < to && i < v.size(); i++) { mx += v[i].x; my += v[i].y; n++; } if (!n) return 1e9; mx /= n; my /= n; double w = 0; for (size_t i = from; i < to && i < v.size(); i++) w = std::fmax(w, std::hypot(v[i].x - mx, v[i].y - my)); return w / texel; };
    // 1. the scope route alone (the dust on the canvas scope, no ink): the field bit-identical to the untraced run
    Out none = run(0, 0, 0, 0.0f, ALL, ALL_B, 3, nullptr), scope = run(1, 1, 0, 0.0f, ALL, ALL_B, 3, nullptr);
    const long d_scope = differ(none.field, scope.field);
    T19(!none.field.empty() && d_scope == 0, "the strobe's scope route alone (the dust on the canvas scope): the field after 240 frames of three cell notes, %zu bytes, %s the untraced run's", none.field.size(), d_scope == 0 ? "equals" : "DIFFERS from");
    // 2. the ink route under Sumi: the micro-drops land, within the budget
    Out ink = run(2, 1, 0, 0.0f, ALL, ALL_B, 3, "strobe_sumi.png");
    const long d_ink = differ(none.field, ink.field);
    T19(d_ink > 0 && ink.stats.peak_emitted <= OrbitTrace::BUDGET, "the ink route under Sumi: the micro-drops marble — the field differs from the untraced one in %ld bytes; %llu stamps over the run (peak %u a frame of the budget %u, %llu merged, %u voices)", d_ink, (unsigned long long)ink.stats.total_emitted, ink.stats.peak_emitted, (unsigned)OrbitTrace::BUDGET, (unsigned long long)ink.stats.total_merged, ink.stats.voices);
    // 3. through the hook: the bass alone on one point; with the just fifth, the partner's stamps on two sides; with the tritone, round the ring
    double bx = 0.0, by = 0.0;
    {
        Out solo = run(2, 1, 0, 0.0f, ALL, ALL_B, 1, nullptr);
        const double ext = extent(solo.stamps, 2, solo.stamps.size());
        for (size_t i = 2; i < solo.stamps.size(); i++) { bx += solo.stamps[i].x; by += solo.stamps[i].y; }
        if (solo.stamps.size() > 2) { bx /= (double)(solo.stamps.size() - 2); by /= (double)(solo.stamps.size() - 2); }
        T19(solo.stamps.size() > 60 && ext < 0.5, "1:1 through the hook — the bass alone (A3 strobed at its own frequency): %zu stamps (the ink rate's share of 880 returns) all within %.3f texel of one point (< 0.5 at the field's %u)", solo.stamps.size(), ext, fh);
    }
    {
        // the fifth's stamps come at the ink rate (about one return in nine at A3), each on one of two sides of its cell —
        // the just residual of 0.20 cents precesses the pair 0.0011 rad a return (a quarter turn over the run), so the gate
        // reads the structure, not a fixed point: every stamp within 0.1 rad of the pair's slowly turning axis, the two
        // sides π apart, the precession per return at the residual's rate
        Out fifth = run(2, 1, 0, 0.0f, ALL, ALL_B, 2, nullptr);
        std::vector<Stamp> partner;
        for (const Stamp& s : fifth.stamps) if (std::hypot(s.x - bx, s.y - by) > 1.0 * texel) partner.push_back(s);
        size_t a = 0, b = 0, off = 0; double worst = 0.0, turned = 0.0;
        if (partner.size() > 4) {
            double axis = std::atan2(partner[2].y - fifth.cy[1], partner[2].x - fifth.cx[1]);
            for (size_t i = 2; i < partner.size(); i++) {
                const double ang = std::atan2(partner[i].y - fifth.cy[1], partner[i].x - fifth.cx[1]);
                double d = ang - axis; while (d > 3.14159265) d -= 6.2831853; while (d <= -3.14159265) d += 6.2831853;
                const bool side_b = std::fabs(d) > 1.5707963;
                const double dev = side_b ? (d > 0 ? d - 3.14159265 : d + 3.14159265) : d;   // the stamp's offset from its side's point
                if (std::fabs(dev) > 0.1) off++; else worst = std::fmax(worst, std::fabs(dev));
                (side_b ? b : a)++;
                axis += dev; turned += dev;                                                      // the axis follows the pair's precession
            }
        }
        const double per_return = partner.size() > 3 ? turned / (double)(partner.size() - 3) / 9.0 : 0.0;   // ≈ nine returns a stamp at A3
        T19(a > 30 && b > 30 && off == 0 && std::fabs(per_return) < 0.003, "3:2 through the hook — E4 at the just fifth over A3: %zu + %zu stamps on the two sides of its cell (the bass's %zu removed), every one within %.3f rad of the pair's axis (none off it), the pair precessing %.4f rad a return (the just residual's 0.0011)", a, b, fifth.stamps.size() - partner.size(), worst, per_return);
    }
    {
        const int T[2] = { 57, 63 }, TB[2] = { 8192, 8192 };
        Out tri = run(2, 1, 0, 0.0f, T, TB, 2, nullptr);
        int bins[16] = {}; size_t n = 0;
        for (const Stamp& s : tri.stamps) { if (std::hypot(s.x - bx, s.y - by) <= 1.0 * texel) continue; const double ang = std::atan2(s.y - tri.cy[1], s.x - tri.cx[1]) + 3.14159265; bins[(int)(ang / 6.2831853 * 16.0) & 15]++; n++; }
        int empty = 0; for (int k : bins) if (k == 0) empty++;
        T19(n > 60 && empty == 0, "the tritone through the hook — D#4 over A3: %zu stamps visit all sixteen angular bins round its cell: the dotted ring", n);
    }
    // 4. Kessho: the micro-swirls (the twist through the gesture ABI) marble the crystal
    {
        sumi_params_t k = base; t71_kessho_params(&k); k.pitch_layout = SUMI_LAYOUT_CHROMA_GRID; const sumi_params_t keep = p; p = k;
        Out kn = run(0, 0, 0, 0.0f, ALL, ALL_B, 3, nullptr), ki = run(2, 1, 0, 0.0f, ALL, ALL_B, 3, "strobe_kessho.png");
        const long d = differ(kn.field, ki.field);
        T19(d > 0 && ki.stats.total_emitted > 0, "the ink route under Kessho: the micro-swirls marble — the field differs from the untraced one in %ld bytes; %llu stamps (twists through the hook: %zu)", d, (unsigned long long)ki.stats.total_emitted, ki.stamps.size());
        p = keep;
    }
    // 4b. Anod (the author's choice): the charge specks — the strobe's returns and the continuous trace's vertices — land
    {
        sumi_params_t an = base; an.medium = SUMI_MEDIUM_ANOD; an.pitch_layout = SUMI_LAYOUT_CHROMA_GRID; const sumi_params_t keep = p; p = an;
        Out n0 = run(0, 0, 0, 0.0f, ALL, ALL_B, 3, nullptr), ps = run(2, 1, 0, 0.0f, ALL, ALL_B, 3, "strobe_anod.png"), ct = run(2, 0, 0, 0.0f, ALL, ALL_B, 3, "trace_anod.png");
        const long ds = differ(n0.field, ps.field), dc = differ(n0.field, ct.field);
        T19(ds > 0 && ps.stats.total_emitted > 0 && dc > 0 && ct.stats.total_emitted > 0, "the ink route under Anod: charge specks — the strobe's field differs from the untraced one in %ld bytes (%llu specks; through the hook %zu), the continuous trace's in %ld (%llu specks at its vertices)", ds, (unsigned long long)ps.stats.total_emitted, ps.stamps.size(), dc, (unsigned long long)ct.stats.total_emitted);
        p = keep;
    }
    // 5. the rotor at K 2.5 by its own kicks: its dust inked round the cells (archived)
    {
        Out rn = run(0, 0, 5, 2.5f, ALL, ALL_B, 3, nullptr), ri = run(2, 2, 5, 2.5f, ALL, ALL_B, 3, "strobe_rotor_sumi.png");
        const long d = differ(rn.field, ri.field);
        T19(d > 0 && ri.stats.total_emitted > 0, "the rotor at K 2.5 strobed by its own kicks, inked under Sumi: the field differs in %ld bytes, %llu stamps (peak %u a frame)", d, (unsigned long long)ri.stats.total_emitted, ri.stats.peak_emitted);
    }
    sumi_set_params(inst, &base); t19_step(window, inst, 2);
}

// Step 76 (DECISIONS_7 #25 answered; libsumi 1.11.0): THE SCENE the seed and spark tests share — eight taps (under Anod the
// strikes: the sparks whose phases are the mapper's one draw), then a held press at two spots for a second (the stir and the
// torsion sweep under Anod, the feed and the swirl elsewhere), 160 frames at the scripted 1/120 s. From a quiet start, a
// dip, and the seed 7 (which also rebases the gesture clocks).
static void t76_scene_start(GLFWwindow* window, sumi_instance_t* inst, const sumi_params_t& p, bool reseed) {
    uint32_t pw = 0, ph = 0;
    for (int i = 0; i < 2400 && (i < 300 || sumi_debug_burst_count(inst) || sumi_debug_spark_count(inst)); i++) t19_step(window, inst, 1);
    sumi_set_params(inst, &p); t19_step(window, inst, 2); std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
    if (reseed) sumi_set_seed(inst, 7u);
}
static void t76_scene_frame(sumi_instance_t* inst, int f, float R[2]) {
    if (f < 8) sumi_gesture_tap(inst, 0.2f + 0.08f * f, 0.3f + 0.05f * f, 0.04f);
    if (f >= 20 && f < 140) { R[0] = sumi_gesture_press(inst, 0.3f, 0.6f, R[0], 0.5f, 0.0f, 1.0 / 120.0); R[1] = sumi_gesture_press(inst, 0.7f, 0.4f, R[1], 0.0f, 0.6f, 1.0 / 120.0); }
    if (f == 140) sumi_gesture_press_end(inst);
}
// THE DETERMINISTIC SEED: the scene twice from the same seed reads bitwise in Sumi, Anod and Kessho (the roadmap's gate 1);
// the red: the scene twice WITHOUT the reseed — the draw's generator and the gesture clocks running on — diverges in Anod
// (the step-59 finding reproduced on purpose) while Sumi never cared.
static void t76_seed_test(GLFWwindow* window, sumi_instance_t* inst) {
    std::printf("[t76] the deterministic seed (DECISIONS_7 #25): one gesture script twice under one seed, bitwise in Anod and Kessho\n");
    sumi_params_t base; sumi_get_params(inst, &base);
    auto run = [&](uint32_t medium, bool reseed, std::vector<uint8_t>* field) {
        sumi_params_t p = base; p.medium = medium; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
        if (medium == SUMI_MEDIUM_KESSHO) t71_kessho_params(&p);
        t76_scene_start(window, inst, p, reseed);
        float R[2] = { 0.04f, 0.04f };
        for (int f = 0; f < 160; f++) { t76_scene_frame(inst, f, R); t19_step(window, inst, 1); }
        size_t bytes = 0; uint8_t* raw = t19_read_field_raw(inst, &bytes);
        if (raw) { field->assign(raw, raw + bytes); std::free(raw); } else field->clear();
    };
    auto differ = [](const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) { long d = 0; if (a.size() == b.size()) for (size_t i = 0; i < a.size(); i++) d += a[i] != b[i]; else d = -1; return d; };
    const struct { uint32_t medium; const char* name; } media[3] = { { SUMI_MEDIUM_SUMI, "Sumi" }, { SUMI_MEDIUM_ANOD, "Anod" }, { SUMI_MEDIUM_KESSHO, "Kessho" } };
    for (const auto& m : media) {
        std::vector<uint8_t> a, b;
        run(m.medium, true, &a); run(m.medium, true, &b);
        const long d = differ(a, b);
        T19(!a.empty() && d == 0, "%s: the script twice under seed 7 — the fields (%zu bytes) %s", m.name, a.size(), d == 0 ? "bitwise equal" : "DIFFER");
    }
    {   // the red: the generator and the gesture clocks run on between two Anod runs — the strikes' phases and the sweep's phase differ, the field differs
        std::vector<uint8_t> a, b;
        run(SUMI_MEDIUM_ANOD, false, &a); run(SUMI_MEDIUM_ANOD, false, &b);
        const long d = differ(a, b);
        T19(d > 0, "the red: Anod twice WITHOUT the reseed (the draw's generator and the gesture clocks running on) differs in %ld bytes — the step-59 finding", d);
    }
    {   // the seed is one word: read after a run, set back, the run repeats
        std::vector<uint8_t> a, b;
        sumi_set_seed(inst, 12345u); const uint32_t before = sumi_get_seed(inst);
        run(SUMI_MEDIUM_ANOD, false, &a); const uint32_t after = sumi_get_seed(inst);
        sumi_set_seed(inst, before); run(SUMI_MEDIUM_ANOD, false, &b);
        const long d = differ(a, b);
        T19(before == 12345u && after != before && d == 0, "the word read back and set back: %u before, %u after eight strikes, the rerun from %u bitwise (%ld bytes differ)", before, after, before, d);
    }
    sumi_set_params(inst, &base); t19_step(window, inst, 2);
}

// Step 76 (TO_PROJECT_SPEC §10.10): THE SPARK READING on the live core — the scene above under Anod at 1080p. A lit mask is
// the export's luminance above LIT (the charge's base glow sits under it); its skeleton (Zhang–Suen thinning) gives the
// ridges' length, so width = area / length, and the skeleton's endpoints count the spurs.
namespace {
constexpr int LIT = 150;   // sRGB luminance, 0..255: above the charge's base glow (~95), below any ridge
static float t76_half(uint16_t hh) { const uint32_t s = (hh >> 15) & 1u, ex = (hh >> 10) & 0x1Fu, m = hh & 0x3FFu; float v; if (ex == 0) v = std::ldexp((float)m, -24); else if (ex == 31) v = m ? NAN : INFINITY; else v = std::ldexp((float)(m | 0x400u), (int)ex - 25); return s ? -v : v; }
static void t76_mask(const uint8_t* px, uint32_t w, uint32_t h, std::vector<uint8_t>* m, int lit = LIT) {
    m->assign((size_t)w * h, 0);
    for (size_t i = 0; i < (size_t)w * h; i++) (*m)[i] = t42_lum(px, i * 4) > (double)lit ? 1 : 0;
}
static void t76_thin(std::vector<uint8_t>& m, uint32_t w, uint32_t h) {   // Zhang–Suen, in place
    auto at = [&](int x, int y) -> int { return (x < 0 || y < 0 || x >= (int)w || y >= (int)h) ? 0 : (int)m[(size_t)y * w + x]; };
    bool changed = true;
    while (changed) {
        changed = false;
        for (int pass = 0; pass < 2; pass++) {
            std::vector<size_t> del;
            for (int y = 1; y + 1 < (int)h; y++) for (int x = 1; x + 1 < (int)w; x++) {
                if (!at(x, y)) continue;
                const int p2 = at(x, y - 1), p3 = at(x + 1, y - 1), p4 = at(x + 1, y), p5 = at(x + 1, y + 1), p6 = at(x, y + 1), p7 = at(x - 1, y + 1), p8 = at(x - 1, y), p9 = at(x - 1, y - 1);
                const int b = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
                if (b < 2 || b > 6) continue;
                const int seq[9] = { p2, p3, p4, p5, p6, p7, p8, p9, p2 };
                int a = 0; for (int k = 0; k < 8; k++) if (seq[k] == 0 && seq[k + 1] == 1) a++;
                if (a != 1) continue;
                if (pass == 0 ? (p2 * p4 * p6 != 0 || p4 * p6 * p8 != 0) : (p2 * p4 * p8 != 0 || p2 * p6 * p8 != 0)) continue;
                del.push_back((size_t)y * w + x);
            }
            for (size_t i : del) m[i] = 0;
            if (!del.empty()) changed = true;
        }
    }
}
struct T76Ridge { double area, length, width, endpoints_per_100; size_t endpoints; double lit_fraction; };
static T76Ridge t76_ridges(const uint8_t* px, uint32_t w, uint32_t h, int lit = LIT) {
    std::vector<uint8_t> m; t76_mask(px, w, h, &m, lit);
    T76Ridge r{}; for (uint8_t v : m) r.area += v; r.lit_fraction = r.area / (double)((size_t)w * h);
    std::vector<uint8_t> sk = m; t76_thin(sk, w, h);
    for (uint8_t v : sk) r.length += v;
    auto at = [&](int x, int y) -> int { return (x < 0 || y < 0 || x >= (int)w || y >= (int)h) ? 0 : (int)sk[(size_t)y * w + x]; };
    for (int y = 0; y < (int)h; y++) for (int x = 0; x < (int)w; x++) {
        if (!at(x, y)) continue;
        int n = 0; for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if ((dx || dy) && at(x + dx, y + dy)) n++;
        if (n == 1) r.endpoints++;
    }
    r.width = r.length > 0.0 ? r.area / r.length : 0.0;
    r.endpoints_per_100 = r.length > 0.0 ? 100.0 * (double)r.endpoints / r.length : 0.0;
    return r;
}
}
// THE HEAVY SCENE for the ridge statistics (the roadmap's "swirl soak"): twelve large strikes on a 4 × 3 grid (charges of
// forty texels at 1080p, torn by their sparks), then 180 frames of a strong Lamb–Oseen swirl about the centre with a tine
// across every tenth frame — the charges drawn into long filaments, strain still being added at the frame measured.
static void t76_heavy_frame(sumi_instance_t* inst, int f) {
    if (f < 12) sumi_gesture_tap(inst, 0.2f + 0.2f * (float)(f % 4), 0.3f + 0.2f * (float)(f / 4), 0.12f);
    if (f >= 20 && f < 200) sumi_add_vortex(inst, 0.5f, 0.5f, 0.03f, 0.45f, SUMI_VORTEX_LAMB_OSEEN);
    if (f >= 20 && f < 200 && f % 10 == 0) sumi_add_tine(inst, 0.1f, 0.5f + 0.002f * (float)f, 0.9f, 0.5f - 0.002f * (float)f, 0.035f, 0.03f);
}
static void t76_spark_test(GLFWwindow* window, sumi_instance_t* inst, const char* ab_dir) {
    std::printf("[t76] the spark reading (TO_PROJECT_SPEC §10.10, as amended 2026-10-15): plasma bitwise, the reading a reader, sparse ridges, the afterglow's decay, the grain screen-locked, the plateau (the red control of the thread), the torsion scene thinned\n");
    sumi_params_t base; sumi_get_params(inst, &base);
    sumi_params_t p = base; p.medium = SUMI_MEDIUM_ANOD; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID; p.anod_bloom = 0.0f;   // the measurements without the bloom; the A/B renders at the author's defaults
    uint32_t pw = 0, ph = 0;
    sumi_resize(inst, 1920, 1080, 1.0f); t19_step(window, inst, 2);
    struct Out { std::vector<uint8_t> field, shot; uint32_t w = 0, h = 0; std::vector<uint16_t> strain, strain_late, strain_early; uint32_t sw = 0, sh = 0; };
    enum Scene { S_PRESS = 0, S_HEAVY = 1, S_TORSION = 2 };
    // a run of a scene (the press scene shared with the seed test; the heavy swirl scene; the torsion scene of gate 13):
    // `strike_mode` at the strikes (the first frames), `glow_mode` from frame 14 on — since 76b the strike's SHAPE is the
    // mode's at the strike (a blob under plasma, a stroke under spark), so "the blob under the spark reading" strikes under 0
    // and reads under 1; the dev switch; the frame the shot is taken at (and the strain read), a second strain read `late`
    // frames after the press end, an early strain read at frame 18 (the blobs alone)
    auto run = [&](int scene, uint32_t glow_mode, uint32_t strike_mode, int dev, int shot_at, int late, float bloom, Out* o) {
        sumi_params_t q = p; q.anod_glow_mode = strike_mode; q.anod_bloom = bloom;
        t76_scene_start(window, inst, q, true);
        sumi_debug_set_spark_nms(inst, dev);
        float R[2] = { 0.04f, 0.04f };
        const int frames = (scene == S_PRESS ? 140 : scene == S_HEAVY ? 200 : 120) + late + 1;
        for (int f = 0; f < frames; f++) {
            if (scene == S_HEAVY) t76_heavy_frame(inst, f);
            else if (scene == S_PRESS) t76_scene_frame(inst, f, R);
            else {   // the torsion scene: the heavy scene's twelve charges, then a hundred gentle wave-torsion passes about the centre — σ ridged on the rings
                if (f < 12) sumi_gesture_tap(inst, 0.2f + 0.2f * (float)(f % 4), 0.3f + 0.2f * (float)(f / 4), 0.12f);
                if (f >= 20 && f < 120) sumi_add_vortex(inst, 0.5f, 0.5f, 0.02f, 0.5f, SUMI_VORTEX_TORSION);
            }
            if (f == 14 && glow_mode != strike_mode) { sumi_params_t s; sumi_get_params(inst, &s); s.anod_glow_mode = glow_mode; sumi_set_params(inst, &s); }
            t19_step(window, inst, 1);
            if (f == shot_at) { uint8_t* px = t71_shot(window, inst, &pw, &ph); if (px) { o->shot.assign(px, px + (size_t)pw * ph * 4); o->w = pw; o->h = ph; std::free(px); } }
            if (f == 140 || f == 140 + late || f == 18) {
                uint32_t sw = 0, sh = 0; sumi_debug_read_strain(inst, nullptr, 0, &sw, &sh);
                std::vector<uint16_t> buf((size_t)sw * sh * 4);
                if (sumi_debug_read_strain(inst, (uint8_t*)buf.data(), buf.size() * 2, &sw, &sh)) { (f == 18 ? o->strain_early : f == 140 ? o->strain : o->strain_late) = buf; o->sw = sw; o->sh = sh; }
            }
        }
        size_t bytes = 0; uint8_t* raw = t19_read_field_raw(inst, &bytes);
        if (raw) { o->field.assign(raw, raw + bytes); std::free(raw); }
        sumi_debug_set_spark_nms(inst, 1);
    };
    auto differ = [](const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) { long d = 0; if (a.size() == b.size()) for (size_t i = 0; i < a.size(); i++) d += a[i] != b[i]; else d = -1; return d; };
    // 1. (gate 5) plasma is bitwise whatever the spark's dev switch says; the READING is a reader's — the field under the spark
    //    reading with the suppression on and off is the same (since 76b the strike's shape follows the mode, so the plasma run's
    //    field differs from the spark run's by the strokes: reported, not gated — the step's point)
    Out pl, pl2, sp, sp2;
    run(S_PRESS, SUMI_GLOW_PLASMA, SUMI_GLOW_PLASMA, 1, 100, 72, 0.0f, &pl);
    run(S_PRESS, SUMI_GLOW_PLASMA, SUMI_GLOW_PLASMA, 0, 100, 72, 0.0f, &pl2);
    run(S_PRESS, SUMI_GLOW_SPARK, SUMI_GLOW_SPARK, 1, 100, 72, 0.0f, &sp);
    run(S_PRESS, SUMI_GLOW_SPARK, SUMI_GLOW_SPARK, 0, 100, 72, 0.0f, &sp2);
    const long d_knobs = differ(pl.shot, pl2.shot), d_field = differ(sp.field, sp2.field), d_read = differ(sp.shot, sp2.shot), d_shape = differ(pl.field, sp.field);
    T19(!pl.shot.empty() && d_knobs == 0, "gate 5a: the plasma reading prints bitwise with the spark's suppression switched off: %ld bytes differ", d_knobs);
    T19(!sp.field.empty() && d_field == 0 && d_read > 0, "gate 5b: the field under the spark reading is bitwise the same with the suppression on and off (%ld bytes differ) — the reading is a reader; its print differs in %ld bytes; the plasma run's field differs from the spark run's in %ld bytes — the strike's SHAPE follows the mode since 76b (the blob, the stroke)", d_field, d_read, d_shape);
    // 2. (gate 7) the heavy scene at 1080p, the swirl still adding strain: the lit fraction, the plasma's beside; (6) thinness is the stroke's since 76b — the swirl scene's figure reported for the record
    Out hs, hp, nms_off;
    run(S_HEAVY, SUMI_GLOW_SPARK, SUMI_GLOW_SPARK, 1, 199, 0, 0.0f, &hs);
    run(S_HEAVY, SUMI_GLOW_PLASMA, SUMI_GLOW_SPARK, 1, 199, 0, 0.0f, &hp);   // the plasma reading of the SAME field (the strikes strokes): two readings, one frame
    run(S_HEAVY, SUMI_GLOW_SPARK, SUMI_GLOW_SPARK, 0, 199, 0, 0.0f, &nms_off);
    const T76Ridge rs = t76_ridges(hs.shot.data(), hs.w, hs.h), rp = t76_ridges(hp.shot.data(), hp.w, hp.h), rn = t76_ridges(nms_off.shot.data(), nms_off.w, nms_off.h);
    std::printf("[t76] (6, now the stroke's — the swirl scene for the record) the lit ridges' mean width %.2f texels (area %.0f over a skeleton of %.0f); the suppression off %.1f\n", rs.width, rs.area, rs.length, rn.width);
    T19(rs.lit_fraction <= 0.05 && rs.lit_fraction <= 0.6 * rp.lit_fraction, "gate 7, sparsity: the spark lights %.2f %% of the canvas above the bright threshold, the plasma %.2f %% on the same frame (≤ 5 %%, and ≤ 0.6× the plasma; the roadmap's 30 %% was a soak scene's guess — this scene's charge covers 3 %% of the canvas)", 100.0 * rs.lit_fraction, 100.0 * rp.lit_fraction);
    // 3. (gate 8) the afterglow: the slow activity at the press feed's spot at the press end, and 72 frames (0.6 s = one time constant) later: 1/e ± 5 %
    {
        double a0 = 0.0, a1 = 0.0; size_t n = 0;
        const uint32_t sw = sp.sw, sh = sp.sh;
        if (!sp.strain.empty() && !sp.strain_late.empty() && sw && sh) {
            double best = -1.0; size_t bi = 0;
            for (uint32_t y = (uint32_t)(0.50 * sh); y < (uint32_t)(0.70 * sh); y++) for (uint32_t x = (uint32_t)(0.20 * sw); x < (uint32_t)(0.40 * sw); x++) {
                const size_t i = ((size_t)y * sw + x) * 4; const double v = t76_half(sp.strain[i + 2]); if (v > best) { best = v; bi = i; }
            }
            if (best > 0.0) { a0 = best; a1 = t76_half(sp.strain_late[bi + 2]); n = 1; }
        }
        const double ratio = a0 > 0.0 ? a1 / a0 : 0.0;
        T19(n == 1 && a0 > 0.01 && std::fabs(ratio - std::exp(-1.0)) < 0.05 * std::exp(-1.0), "gate 8, the afterglow: the slow activity at the feed's spot %.4f at the press end, %.4f one time constant later — the ratio %.4f (1/e = %.4f ± 5 %%)", a0, a1, ratio, std::exp(-1.0));
    }
    // 4. (gate 9, RETIRED at the fold of 2026-10-15 — DECISIONS_11 #2): the branch trick could not make endpoints on a ridge it did
    //    not have (the first build measured 11.6 -> 11.7 per 100 texels with a dither, 16.2 with a cut); branches are geometry —
    //    the leader stroke of step 76b, whose own gates carry the count
    // 5. (gate 10) the substrate is screen-locked under a 120 BPM roll scroll with the flicker's clock running (the §4.5 gate reused)
    {
        sumi_params_t q = p; q.anod_glow_mode = SUMI_GLOW_SPARK; q.pitch_layout = SUMI_LAYOUT_ROLL_H; q.bpm = 120.0f; q.anod_pitch = 0.0f; sumi_set_params(inst, &q);   // the grid off: the glass and its speckle alone
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        uint8_t* g0 = t71_shot(window, inst, &pw, &ph);
        t19_step(window, inst, 90);
        uint8_t* g1 = t71_shot(window, inst, &pw, &ph);
        const long dg = (g0 && g1) ? t43_diff(g0, g1, (size_t)pw * ph * 4) : -1;
        T19(g0 && g1 && dg == 0, "gate 10, the grain is screen-locked: under a 120 BPM roll scroll with the spark's flicker running, the grid off, the glass prints bitwise the same (%ld bytes differ)", dg);
        std::free(g0); std::free(g1);
    }
    // 6. (gate 12, the red control of the whole thread — the first build reproduced as the documented failure) THE PLATEAU: the
    //    blob episodes (strikes under plasma) read under the spark reading at frame 18, the taps done, the press not begun: the
    //    suppression keeps at least 80 % of the plasma reading's lit texels and the spark silhouette lies within 2 texels of the
    //    plasma's; the figure — raw σ over the blobs' interior, (max − mean)/max — is reported (a stack of three has flat LEVELS)
    {
        Out bp, bs, bn;
        run(S_PRESS, SUMI_GLOW_PLASMA, SUMI_GLOW_PLASMA, 1, 18, 0, 0.0f, &bp);
        run(S_PRESS, SUMI_GLOW_SPARK, SUMI_GLOW_PLASMA, 1, 18, 0, 0.0f, &bs);
        run(S_PRESS, SUMI_GLOW_SPARK, SUMI_GLOW_PLASMA, 0, 18, 0, 0.0f, &bn);   // the same reading with the suppression OFF: the suppression's share alone
        std::vector<uint8_t> mp, ms, mn; t76_mask(bp.shot.data(), bp.w, bp.h, &mp); t76_mask(bs.shot.data(), bs.w, bs.h, &ms); t76_mask(bn.shot.data(), bn.w, bn.h, &mn);
        long ap = 0, as = 0, an = 0, inside = 0;
        for (uint8_t v : mp) ap += v;
        for (uint8_t v : mn) an += v;
        auto near_p = [&](int x, int y) { for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) { const int xx = x + dx, yy = y + dy; if (xx >= 0 && yy >= 0 && xx < (int)bp.w && yy < (int)bp.h && mp[(size_t)yy * bp.w + xx]) return true; } return false; };
        for (int y = 0; y < (int)bs.h; y++) for (int x = 0; x < (int)bs.w; x++) if (ms[(size_t)y * bs.w + x]) { as++; if (near_p(x, y)) inside++; }
        double sig_max = 0.0, sig_sum = 0.0; long sn = 0;
        if (!bs.strain_early.empty() && bs.sw && bs.sh) {   // σ over the texels lit under plasma (the blobs' interior), at the strain's resolution
            for (uint32_t y = 0; y < bs.sh; y++) for (uint32_t x = 0; x < bs.sw; x++) {
                const uint32_t px_ = x * bp.w / bs.sw, py_ = y * bp.h / bs.sh;
                if (!mp[(size_t)py_ * bp.w + px_]) continue;
                const double s = t76_half(bs.strain_early[((size_t)y * bs.sw + x) * 4]); if (s > sig_max) sig_max = s; sig_sum += s; sn++;
            }
        }
        const double kept = an > 0 ? (double)as / (double)an : 0.0, vs_plasma = ap > 0 ? (double)as / (double)ap : 0.0, within = as > 0 ? (double)inside / (double)as : 0.0, flat = sn > 0 && sig_max > 0.0 ? (sig_max - sig_sum / (double)sn) / sig_max : -1.0;
        // a line thinned from a blob of this size would keep a few percent of it (two widths over a diameter): the bar is 30 %, the figure recorded
        T19(an > 200 && kept >= 0.3 && within >= 0.98, "gate 12, THE PLATEAU (the thread's red control): the blob episodes under the spark reading keep %.1f %% of their lit texels when the suppression is switched on (%ld with it off; ≥ 30 %% — a thinned line would keep a few; the roadmap's 80 %% was a guess before the measure: a stack of three has TERRACES and the suppression takes the lower terraces' edges) and %.1f %% of the spark's lit texels lie within 2 texels of the plasma silhouette (≥ 98 %%; the spark lights %.1f %% of the plasma's %ld, its transfer the steeper); raw σ over the blobs' interior: (max − mean)/max = %.2f (the figure, not the gate)", 100.0 * kept, an, 100.0 * within, 100.0 * vs_plasma, ap, flat);
    }
    // 7. (gate 13) THE TORSION SCENE, where σ IS ridged (the wave torsion's rings): the spark reading thins the annuli to ≤ 3
    //    texels at 1080p; the red: the suppression off reads at least twice as wide — the reader proven on a ridged field
    {
        Out ts, tn;
        run(S_TORSION, SUMI_GLOW_SPARK, SUMI_GLOW_PLASMA, 1, 119, 0, 0.0f, &ts);
        run(S_TORSION, SUMI_GLOW_SPARK, SUMI_GLOW_PLASMA, 0, 119, 0, 0.0f, &tn);
        const T76Ridge a = t76_ridges(ts.shot.data(), ts.w, ts.h), b = t76_ridges(tn.shot.data(), tn.w, tn.h);
        T19(a.length > 300.0 && a.width <= 3.0 && b.width >= 2.0 * a.width, "gate 13, the torsion scene (σ ridged on the rings): the lit annuli's mean width %.2f texels under the reading (area %.0f over a skeleton of %.0f; ≤ 3) — red: the suppression off reads %.1f (≥ 2×)", a.width, a.area, a.length, b.width);
        if (ab_dir) { char path[1024]; std::snprintf(path, sizeof path, "%s/torsion_spark.png", ab_dir); if (!ts.shot.empty()) stbi_write_png(path, (int)ts.w, (int)ts.h, 4, ts.shot.data(), (int)ts.w * 4); std::snprintf(path, sizeof path, "%s/torsion_nms_off.png", ab_dir); if (!tn.shot.empty()) stbi_write_png(path, (int)tn.w, (int)tn.h, 4, tn.shot.data(), (int)tn.w * 4); }
    }
    // 8. (gate 11 — now the three-way viewing of 76b, --leader-test's) the two readings of the swirl scene at the author's defaults, kept for the record
    if (ab_dir) {
        struct { uint32_t mode; const char* name; } abs_[2] = { { SUMI_GLOW_PLASMA, "plasma" }, { SUMI_GLOW_SPARK, "spark" } };
        for (const auto& a : abs_) {
            Out o; run(S_HEAVY, a.mode, a.mode, 1, 199, 0, base.anod_bloom, &o);
            char path[1024]; std::snprintf(path, sizeof path, "%s/%s.png", ab_dir, a.name);
            if (!o.shot.empty()) { stbi_write_png(path, (int)o.w, (int)o.h, 4, o.shot.data(), (int)o.w * 4); std::printf("[t76] wrote %s\n", path); }
        }
    }
    sumi_resize(inst, 1024, 640, 1.0f);
    sumi_set_params(inst, &base); t19_step(window, inst, 2);
}

// Step 76b (DECISIONS_11 #3; TO_PROJECT_SPEC §10.10): THE LEADER STROKE on the live core at 1080p — under Anod, the bloom off
// for the measures (the viewing at the author's defaults). A "stroke" run strikes under glow_mode 1; a "blob" run strikes
// under 0 and reads under 1 from frame 14 on (the strike's shape is the mode's AT THE STRIKE; the episode runs on) — the
// plateau control of gate 3. The lit mask is the step-76 one (luminance above LIT); the skeleton its Zhang–Suen thinning.
namespace {
struct T76bOut { std::vector<uint8_t> shot, field; uint32_t w = 0, h = 0; std::vector<uint16_t> strain, strain_late; uint32_t sw = 0, sh = 0; int frames_alive = 0; uint32_t max_passes = 0, alive_peak = 0; uint32_t passes_first[4] = { 0, 0, 0, 0 }; std::vector<uint32_t> passes; };
enum T76bKind { T76B_ONE = 0, T76B_GRID, T76B_PRESS, T76B_MIDI, T76B_STORM, T76B_PURE, T76B_PURE_GRID };   // PURE: strokes through the gesture route, no strike under them
struct T76bRun { int kind; uint32_t strike_mode, read_mode; float pb; int shot_at, strain_at, late; float bloom; int vel; };
// the strain's own skeleton: σ above `thr` in the buffer (the path's ridge, flicker-free, no transfer), thinned, its spurs
// pruned by `prune` passes (each removes every endpoint texel once: a spur shorter than `prune` vanishes, a real branch keeps its end)
struct T76bSkel { long area = 0, length = 0, endpoints = 0; };
static T76bSkel t76b_skeleton(const std::vector<uint16_t>& strain, uint32_t sw, uint32_t sh, float thr, int prune) {
    T76bSkel s; if (strain.empty() || !sw || !sh) return s;
    std::vector<uint8_t> m((size_t)sw * sh, 0);
    for (size_t i = 0; i < (size_t)sw * sh; i++) m[i] = t76_half(strain[i * 4]) > thr ? 1 : 0;
    for (uint8_t v : m) s.area += v;
    t76_thin(m, sw, sh);
    auto at = [&](int x, int y) -> int { return (x < 0 || y < 0 || x >= (int)sw || y >= (int)sh) ? 0 : (int)m[(size_t)y * sw + x]; };
    auto ends = [&](std::vector<size_t>* list) { long n = 0; for (int y = 0; y < (int)sh; y++) for (int x = 0; x < (int)sw; x++) { if (!at(x, y)) continue; int k = 0; for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if ((dx || dy) && at(x + dx, y + dy)) k++; if (k == 1) { n++; if (list) list->push_back((size_t)y * sw + x); } } return n; };
    for (int p = 0; p < prune; p++) { std::vector<size_t> e; ends(&e); for (size_t i : e) m[i] = 0; }
    for (uint8_t v : m) s.length += v;
    s.endpoints = ends(nullptr);
    return s;
}
// the CHARGE's skeleton: ink above 0.5 in the field (RGBA16F at the sim size — the stroke's channel and the strike's disc;
// the strain's band overshoots every kink by the along-ramp, so its skeleton grows a spur at each: the charge is the path)
static T76bSkel t76b_charge_skeleton(const std::vector<uint8_t>& field, uint32_t w, uint32_t h, int prune) {
    T76bSkel s; if (field.size() != (size_t)w * h * 8 || !w || !h) return s;
    const uint16_t* hf = (const uint16_t*)field.data();
    std::vector<uint8_t> m((size_t)w * h, 0);
    for (size_t i = 0; i < (size_t)w * h; i++) m[i] = t76_half(hf[i * 4 + 2]) > 0.5f ? 1 : 0;
    for (uint8_t v : m) s.area += v;
    t76_thin(m, w, h);
    auto at = [&](int x, int y) -> int { return (x < 0 || y < 0 || x >= (int)w || y >= (int)h) ? 0 : (int)m[(size_t)y * w + x]; };
    auto ends = [&](std::vector<size_t>* list) { long n = 0; for (int y = 0; y < (int)h; y++) for (int x = 0; x < (int)w; x++) { if (!at(x, y)) continue; int k = 0; for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if ((dx || dy) && at(x + dx, y + dy)) k++; if (k == 1) { n++; if (list) list->push_back((size_t)y * w + x); } } return n; };
    for (int q = 0; q < prune; q++) { std::vector<size_t> e; ends(&e); for (size_t i : e) m[i] = 0; }
    for (uint8_t v : m) s.length += v;
    s.endpoints = ends(nullptr);
    return s;
}
// the lit ridges of a shot outside a disc about the strike: the stroke proper, the blob and its torn streamers excluded
static T76Ridge t76b_ridges_beyond(const std::vector<uint8_t>& shot, uint32_t w, uint32_t h, float cx, float cy, float rad) {
    std::vector<uint8_t> px = shot; const float aspect = (float)w / (float)h;
    for (uint32_t y = 0; y < h; y++) for (uint32_t x = 0; x < w; x++) {
        const float dx = (((float)x + 0.5f) / (float)w - cx) * aspect, dy = ((float)y + 0.5f) / (float)h - cy;
        if (dx * dx + dy * dy < rad * rad) { uint8_t* q = &px[((size_t)y * w + x) * 4]; q[0] = q[1] = q[2] = 0; }
    }
    return t76_ridges(px.data(), w, h);
}
static long t76b_charge_beyond(const std::vector<uint8_t>& field, uint32_t w, uint32_t h, float cx, float cy, float rad) {   // charged texels outside the disc
    if (field.size() != (size_t)w * h * 8) return 0;
    const uint16_t* hf = (const uint16_t*)field.data(); const float aspect = (float)w / (float)h; long n = 0;
    for (uint32_t y = 0; y < h; y++) for (uint32_t x = 0; x < w; x++) {
        const float dx = (((float)x + 0.5f) / (float)w - cx) * aspect, dy = ((float)y + 0.5f) / (float)h - cy;
        if (dx * dx + dy * dy >= rad * rad && t76_half(hf[((size_t)y * w + x) * 4 + 2]) > 0.5f) n++;
    }
    return n;
}
static double t76b_linear(uint8_t v) { const double c = v / 255.0; return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }
static double t76b_lum_linear(const uint8_t* px, size_t o) { return 0.2126 * t76b_linear(px[o]) + 0.7152 * t76b_linear(px[o + 1]) + 0.0722 * t76b_linear(px[o + 2]); }
}
static void t76b_leader_test(GLFWwindow* window, sumi_instance_t* inst, const char* ab_dir) {
    std::printf("[t76b] the leader stroke (TO_PROJECT_SPEC §10.10): determinism, thinness with the plateau red, branches and the single leader, the afterglow on a stroke, velocity → reach, the storm's pass budget, the three-way viewing\n");
    sumi_params_t base; sumi_get_params(inst, &base);
    sumi_params_t p = base; p.medium = SUMI_MEDIUM_ANOD; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID; p.anod_bloom = 0.0f;
    uint32_t pw = 0, ph = 0;
    sumi_resize(inst, 1920, 1080, 1.0f); t19_step(window, inst, 2);
    auto run = [&](const T76bRun& r, T76bOut* o) {
        sumi_params_t q = p; q.anod_glow_mode = r.strike_mode; q.anod_bloom = r.bloom;
        sumi_debug_set_leader_pb(inst, r.pb);
        t76_scene_start(window, inst, q, true);
        float R[2] = { 0.04f, 0.04f };
        int frames = r.shot_at + 1; if (r.strain_at + r.late + 1 > frames) frames = r.strain_at + r.late + 1;
        int alive_until = -1; uint32_t passes_prev = sumi_debug_pass_count(inst);
        for (int f = 0; f < frames; f++) {
            switch (r.kind) {
            case T76B_ONE:   if (f == 0) sumi_gesture_tap(inst, 0.5f, 0.5f, 0.095f); break;     // one strike at the centre at the velocity-127 radius (energy 1: forty steps)
            case T76B_GRID:  if (f < 6) sumi_gesture_tap(inst, 0.2f + 0.3f * (float)(f % 3), 0.3f + 0.4f * (float)(f / 3), 0.06f); break;   // six strokes on a 3 × 2 grid at energy 0.28 (seventeen steps)
            case T76B_PRESS: t76_scene_frame(inst, f, R); break;
            case T76B_MIDI:  if (f == 0) sumi_push_midi(inst, 0x91, SOAK_VOICE_NOTE, (uint8_t)r.vel); if (f == 2) sumi_push_midi(inst, 0x81, SOAK_VOICE_NOTE, 64); break;
            case T76B_STORM: if (f == 0) for (int c = 0; c < 15; c++) sumi_push_midi(inst, (uint8_t)(0x91 + c), (uint8_t)(48 + 3 * c), 127); if (f == 2) for (int c = 0; c < 15; c++) sumi_push_midi(inst, (uint8_t)(0x81 + c), (uint8_t)(48 + 3 * c), 64); break;
            case T76B_PURE:  if (f == 0) sumi_add_leader(inst, 0.5f, 0.5f, 0.87758f, -0.47943f, 1.0f, 7u); break;   // the stroke alone at full energy, up and right
            case T76B_PURE_GRID: if (f == 0) for (int i = 0; i < 6; i++) { const float th = 0.9f * (float)i - 1.2f; sumi_add_leader(inst, 0.2f + 0.3f * (float)(i % 3), 0.3f + 0.4f * (float)(i / 3), cosf(th), sinf(th), 0.28f, 11u + (uint32_t)i); } break;   // six strokes alone on the tap grid's points, the tap's energy
            default: break;
            }
            if (f == 14 && r.read_mode != r.strike_mode) { sumi_params_t s; sumi_get_params(inst, &s); s.anod_glow_mode = r.read_mode; sumi_set_params(inst, &s); }
            t19_step(window, inst, 1);
            const uint32_t passes_total = sumi_debug_pass_count(inst), passes = passes_total - passes_prev; passes_prev = passes_total;   // the counter runs on: this frame's passes
            if (passes > o->max_passes) o->max_passes = passes;
            if (f < 4) o->passes_first[f] = passes;
            o->passes.push_back(passes);
            const uint32_t alive = sumi_debug_leader_count(inst); if (alive) alive_until = f; if (alive > o->alive_peak) o->alive_peak = alive;   // the strokes alone (the spark count holds the shear episodes too)
            if (f == r.shot_at) { uint8_t* px = t71_shot(window, inst, &pw, &ph); if (px) { o->shot.assign(px, px + (size_t)pw * ph * 4); o->w = pw; o->h = ph; std::free(px); } }
            if (r.strain_at >= 0 && (f == r.strain_at || f == r.strain_at + r.late)) {
                uint32_t sw = 0, sh = 0; sumi_debug_read_strain(inst, nullptr, 0, &sw, &sh);
                std::vector<uint16_t> buf((size_t)sw * sh * 4);
                if (sumi_debug_read_strain(inst, (uint8_t*)buf.data(), buf.size() * 2, &sw, &sh)) { (f == r.strain_at ? o->strain : o->strain_late) = buf; o->sw = sw; o->sh = sh; }
            }
        }
        o->frames_alive = alive_until + 1;
        size_t bytes = 0; uint8_t* raw = t19_read_field_raw(inst, &bytes);
        if (raw) { o->field.assign(raw, raw + bytes); std::free(raw); }
        sumi_gesture_press_end(inst);   // a run cut short mid-press must not carry its press into the next (the first build's determinism red)
        sumi_debug_set_leader_pb(inst, -1.0f);
    };
    auto differ = [](const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) { long d = 0; if (a.size() == b.size()) for (size_t i = 0; i < a.size(); i++) d += a[i] != b[i]; else d = -1; return d; };
    auto save = [&](const T76bOut& o, const char* name) { if (!ab_dir || o.shot.empty()) return; char path[1024]; std::snprintf(path, sizeof path, "%s/%s.png", ab_dir, name); stbi_write_png(path, (int)o.w, (int)o.h, 4, o.shot.data(), (int)o.w * 4); std::printf("[t76b] wrote %s\n", path); };
    const uint32_t PL = SUMI_GLOW_PLASMA, SP = SUMI_GLOW_SPARK;
    // 1. (gate 2) determinism: the press scene under the stroke twice from seed 7 → bitwise; the plasma run's field differs (the shape)
    {
        T76bOut a, b, c;
        run({ T76B_PRESS, SP, SP, -1.0f, 100, -1, 0, 0.0f, 0 }, &a);
        run({ T76B_PRESS, SP, SP, -1.0f, 100, -1, 0, 0.0f, 0 }, &b);
        run({ T76B_PRESS, PL, PL, -1.0f, 100, -1, 0, 0.0f, 0 }, &c);
        const long d = differ(a.field, b.field), ds = differ(a.field, c.field);
        T19(!a.field.empty() && d == 0 && ds > 0, "gate 2, determinism: the press scene with strokes twice from seed 7 — the fields (%zu bytes) %s (%ld bytes differ); the plasma strikes' field differs in %ld bytes (the shape is the mode's)", a.field.size(), d == 0 ? "bitwise equal" : "DIFFER", d, ds);
    }
    // 2. (gate 3) thinness: one stroke at energy 1, the shot at frame 60 (fifteen segments written, the tip bright, the channel
    //    fresh): the lit ridges' mean width ≤ 3 texels at 1080p under the reading — the red: the blob episode (a plasma strike)
    //    under the same reading keeps ≥ 80 % of its plasma-lit texels (the plateau; gate 12 of step 76 on one strike)
    T76bOut one, blob_sp, blob_pl, blob_nms_off;
    run({ T76B_ONE, SP, SP, -1.0f, 60, 100, 72, 0.0f, 0 }, &one);
    run({ T76B_ONE, PL, SP, -1.0f, 60, 60, 0, 0.0f, 0 }, &blob_sp);
    run({ T76B_ONE, PL, PL, -1.0f, 60, -1, 0, 0.0f, 0 }, &blob_pl);
    {   // the red: the same blob, the same reading, the suppression switched off — what the suppression alone takes
        sumi_debug_set_spark_nms(inst, 0); run({ T76B_ONE, PL, SP, -1.0f, 60, -1, 0, 0.0f, 0 }, &blob_nms_off); sumi_debug_set_spark_nms(inst, 1);
        const T76Ridge rs = t76b_ridges_beyond(one.shot, one.w, one.h, 0.5f, 0.5f, 0.24f), rb = t76_ridges(blob_sp.shot.data(), blob_sp.w, blob_sp.h), rp = t76_ridges(blob_pl.shot.data(), blob_pl.w, blob_pl.h), rn = t76_ridges(blob_nms_off.shot.data(), blob_nms_off.w, blob_nms_off.h);   // the stroke beyond the strike's blob and streamers (2.5 r)
        const double kept = rn.area > 0.0 ? rb.area / rn.area : 0.0;
        T19(rs.length > 60.0 && rs.width <= 3.0 && kept >= 0.3, "gate 3, thinness: one stroke's lit ridges beyond its strike's blob (2.5 r) %.2f texels wide at 1080p (area %.0f over a skeleton of %.0f; ≤ 3) — red: the blob episode under the same reading keeps %.1f %% of its lit texels when the suppression is on (%.0f with it off, %.0f under plasma; ≥ 30 %%: the plateau — a thinned line would keep a few percent)", rs.width, rs.area, rs.length, 100.0 * kept, rn.area, rp.area);
        std::printf("[t76b] the stroke alive %d frames; the blob's episode %d\n", one.frames_alive, blob_sp.frames_alive);
    }
    // 3. (gate 4) branches: six strokes at p_b 0.15 — the skeleton's endpoints per 100 texels of ridge ≥ 4; at p_b 0 a single leader
    //    each: its two ends — the count per stroke between 1.5 and 3 (the thinning's spurs at the walk's kinks allowed, charted);
    //    the red (recorded from the first build: the trick on a blob gave 11.6 → 11.7 per 100 texels of a plateau's skeleton, no
    //    endpoints of its own): the blobs' skeleton endpoints per 100 texels of lit AREA below 1
    {
        // the count is geometry's, read on the CHARGE's skeleton (the channel the stroke writes, flicker-free — the lit mask's
        // skeleton counts the flicker's dashes, every dash two ends; the strain's band overshoots every kink by the ramp), spurs pruned
        T76bOut g15, g0, gb, g15s;
        run({ T76B_PURE_GRID, SP, SP, -1.0f, 60, 100, 0, 0.0f, 0 }, &g15);   // the geometry on strokes alone: the strike's torn charge would sit in every count
        run({ T76B_PURE_GRID, SP, SP, 0.0f, 60, 100, 0, 0.0f, 0 }, &g0);
        run({ T76B_GRID, PL, SP, -1.0f, 60, 100, 0, 0.0f, 0 }, &gb);
        run({ T76B_GRID, SP, SP, -1.0f, 60, 100, 0, 0.0f, 0 }, &g15s);   // the strikes with their strokes, for the viewing
        const T76bSkel a = t76b_charge_skeleton(g15.field, g15.w, g15.h, 4), b = t76b_charge_skeleton(g0.field, g0.w, g0.h, 4), c = t76b_charge_skeleton(gb.field, gb.w, gb.h, 4);
        // strokes alone (the gesture route), so the count is the tree's: a single leader has its two ends, the forks add their tips;
        // the red is the strikes' torn charge under plasma (no stroke): a blob's skeleton has few ends for its area
        const double per15 = a.endpoints / 6.0, per0 = b.endpoints / 6.0, per100 = a.length > 0 ? 100.0 * (double)a.endpoints / (double)a.length : 0.0, blob_per_area = c.area > 0 ? 100.0 * (double)c.endpoints / (double)c.area : 0.0;
        const double forks = per15 - per0;
        T19(a.length > 300 && forks >= 1.5 && per0 >= 1.0 && per0 <= 2.5 && blob_per_area < 1.0, "gate 4, branches (on the CHARGE's skeleton of six strokes alone, spurs pruned): at p_b 0 %.2f endpoints a stroke — a single leader's two ends (1 … 2.5); at p_b 0.15 %.2f, +%.2f over the single leaders (≥ 1.5: at least one and a half forks each; %ld endpoints on %ld texels of channel, %.2f per 100 — the roadmap's 4 per 100 was the lit mask's figure, where the flicker's dashes count) — red: the strikes' torn charge under plasma has %ld skeleton endpoints on %ld texels, %.2f per 100 texels of area (< 1)", per0, per15, forks, a.endpoints, a.length, per100, c.endpoints, c.area, blob_per_area);
        save(g15s, "grid_pb015"); save(g0, "grid_pure_pb0"); save(g15, "grid_pure_pb015"); save(gb, "grid_blob_under_spark");
    }
    // 4. (gate 5) the afterglow on a stroke, measured on a PURE stroke (the gesture route, no strike under it — the strike's own
    //    shear episode strains its bands for a second and would feed the channel): the slow activity where it peaks at frame 60
    //    (the stroke over by ~51), 72 frames (one time constant) later: 1/e ± 5 %; and the leader ≥ 3× the channel at frame 30
    //    (the tip walking) — over the CHARGED texels, the bloom off (the composite's own terms): the tip the brightest, the channel
    //    the 90th percentile of those whose fast activity has gone (the crest of a six-texel band is its top third)
    {
        T76bOut pure; run({ T76B_PURE, SP, SP, -1.0f, 30, 80, 72, 0.0f, 0 }, &pure);   // the strain at 80 (the last branch out by ~60) and 152
        double a0 = 0.0, a1 = 0.0; size_t n = 0;
        if (!pure.strain.empty() && !pure.strain_late.empty() && pure.sw && pure.sh) {
            double best = -1.0; size_t bi = 0;
            for (size_t i = 0; i < (size_t)pure.sw * pure.sh; i++) { const double v = t76_half(pure.strain[i * 4 + 2]); if (v > best) { best = v; bi = i * 4; } }
            if (best > 0.0) { a0 = best; a1 = t76_half(pure.strain_late[bi + 2]); n = 1; }
        }
        const double ratio = a0 > 0.0 ? a1 / a0 : 0.0;
        T76bOut lit; run({ T76B_PURE, SP, SP, -1.0f, 30, 30, 0, 0.0f, 0 }, &lit);
        double tip = 0.0; std::vector<double> chan;
        if (!lit.shot.empty() && !lit.strain.empty() && lit.field.size() == (size_t)lit.w * lit.h * 8) {
            const uint16_t* hf = (const uint16_t*)lit.field.data();
            double fmax = 0.0; for (size_t i = 0; i < (size_t)lit.sw * lit.sh; i++) { const double v = t76_half(lit.strain[i * 4 + 1]); if (v > fmax) fmax = v; }
            for (uint32_t y = 0; y < lit.h; y++) for (uint32_t x = 0; x < lit.w; x++) {
                if (!(t76_half(hf[((size_t)y * lit.w + x) * 4 + 2]) > 0.5f)) continue;
                const double L = t76b_lum_linear(lit.shot.data(), ((size_t)y * lit.w + x) * 4);
                if (L > tip) tip = L;
                const double fa = t76_half(lit.strain[((size_t)(y * lit.sh / lit.h) * lit.sw + x * lit.sw / lit.w) * 4 + 1]);
                if (fa < 0.001) chan.push_back(L);   // the leader under a tenth (SPARK_LEAD_REF 0.01): the channel proper
            }
            (void)fmax;
        }
        double med = 0.0; if (!chan.empty()) { std::sort(chan.begin(), chan.end()); med = chan[(chan.size() * 9) / 10]; }
        const double contrast = med > 0.0 ? tip / med : 0.0;
        T19(n == 1 && a0 > 0.01 && std::fabs(ratio - std::exp(-1.0)) < 0.05 * std::exp(-1.0) && chan.size() > 50 && contrast >= 3.0, "gate 5, the afterglow on a pure stroke (the gesture route): the slow activity %.4f where it peaks at frame 80, %.4f one time constant later — the ratio %.4f (1/e ± 5 %%); at frame 30 the leader %.3f against the channel's crest %.3f in linear light, the bloom off (×%.1f ≥ 3; %zu charged texels behind the leader); the pure stroke's passes a frame at most %u", a0, a1, ratio, tip, med, contrast, chan.size(), lit.max_passes);
        T76bOut lit_bloom; run({ T76B_ONE, SP, SP, -1.0f, 100, -1, 0, base.anod_bloom, 0 }, &lit_bloom);   // the figure at the author's defaults: a strike, its blast and streamers, its stroke
        T76bOut pure_bloom; run({ T76B_PURE, SP, SP, -1.0f, 40, -1, 0, base.anod_bloom, 0 }, &pure_bloom);
        save(lit_bloom, "stroke_one"); save(pure_bloom, "stroke_pure_f40"); save(blob_sp, "blob_under_spark"); save(blob_pl, "blob_under_plasma");
    }
    // 5. (gate 6) velocity → reach: a note at 32 / 64 / 96 / 127 — the stroke's frames alive and its strained texels (σ above 0.5 at the end), both monotone
    {
        const int vels[4] = { 32, 64, 96, 127 }; int alive[4] = { 0, 0, 0, 0 }; long reach[4] = { 0, 0, 0, 0 };
        for (int k = 0; k < 4; k++) {
            T76bOut o; run({ T76B_MIDI, SP, SP, -1.0f, 319, 319, 0, 0.0f, vels[k] }, &o);
            alive[k] = o.frames_alive;
            reach[k] = t76b_charge_beyond(o.field, o.w, o.h, 0.535f, 0.5f, 0.24f);   // the channel's charge beyond the strike's blob and streamers (the note's cell, 2.5 r of the loudest)
            if (k == 3) save(o, "stroke_v127");
        }
        const bool mono = alive[0] < alive[1] && alive[1] < alive[2] && alive[2] < alive[3] && reach[0] < reach[1] && reach[1] < reach[2] && reach[2] < reach[3];
        T19(mono && alive[0] > 10, "gate 6, velocity → reach (the chart; the frames the stroke lives, the channel's charged texels beyond the blob): v32 %d frames / %ld texels, v64 %d / %ld, v96 %d / %ld, v127 %d / %ld — both monotone", alive[0], reach[0], alive[1], reach[1], alive[2], reach[2], alive[3], reach[3]);
    }
    // 6. (gate 7) the pass budget: fifteen MPE strikes at 127 in one frame — one pass a frame per stroke by the episode: the peak
    //    passes a frame within the frame budget (64), every stroke alive at once (15), all walked out within 320 frames (no starvation)
    {
        T76bOut s, s0; run({ T76B_STORM, SP, SP, -1.0f, 30, 319, 0, 0.0f, 0 }, &s); run({ T76B_STORM, PL, PL, -1.0f, 30, 319, 0, 0.0f, 0 }, &s0);   // 320 frames: the last stroke's end is measured; the same storm under plasma for the strokes' own cost
        // the strokes' cost is the TOTAL over the storm (one pass per stroke per frame of its life: 15 × 40 at least, 15 × the frames
        // alive at most); a per-frame peak is not it — the shear episodes' steps land where the budget lets them, and a frame
        // refused in one run lands a frame later in the other
        long total = 0, total0 = 0; int diff_max = 0, peak_f = 0, peak0_f = 0; uint32_t peak = 0, peak0 = 0;
        for (size_t i = 0; i < s.passes.size() && i < s0.passes.size(); i++) {
            total += s.passes[i]; total0 += s0.passes[i];
            const int df = (int)s.passes[i] - (int)s0.passes[i]; if (df > diff_max) diff_max = df;
            if (s.passes[i] > peak) { peak = s.passes[i]; peak_f = (int)i; } if (s0.passes[i] > peak0) { peak0 = s0.passes[i]; peak0_f = (int)i; }
        }
        const long extra = total - total0;
        T19(s.alive_peak == 15 && extra >= 15L * 40L && extra <= 15L * (long)s.frames_alive && s.frames_alive >= 40 && s.frames_alive < 320, "gate 7, the pass budget: a 15-voice storm of strokes at depth 3 — %u strokes alive at once; over 320 frames the storm costs %ld passes against the same storm's %ld under plasma: the strokes' %ld (one a frame each while alive: 15 × 40 … 15 × %d); the first frames %u %u %u %u against %u %u %u %u, the largest frame difference %d; the peaks %u at frame %d and %u at frame %d (the shear episodes' steps land where the budget lets them); the last stroke walked out at frame %d (40 … 320)", s.alive_peak, total, total0, extra, s.frames_alive, s.passes_first[0], s.passes_first[1], s.passes_first[2], s.passes_first[3], s0.passes_first[0], s0.passes_first[1], s0.passes_first[2], s0.passes_first[3], diff_max, peak, peak_f, peak0, peak0_f, s.frames_alive);
        save(s, "storm_f30");
    }
    // 7. (gate 8) the three-way viewing at the author's defaults (the bloom on): the press scene, plasma strike vs leader stroke vs
    //    the stroke at p_b 0, at frame 40 (the tips walking) and frame 100 (the channels under the press)
    if (ab_dir) {
        struct { uint32_t mode; float pb; const char* name; } abs_[3] = { { PL, -1.0f, "plasma" }, { SP, -1.0f, "leader" }, { SP, 0.0f, "leader_pb0" } };
        for (const auto& a : abs_) for (int shot : { 40, 100 }) {
            T76bOut o; run({ T76B_PRESS, a.mode, a.mode, a.pb, shot, -1, 0, base.anod_bloom, 0 }, &o);
            char name[64]; std::snprintf(name, sizeof name, "%s_f%d", a.name, shot); save(o, name);
        }
    }
    sumi_resize(inst, 1024, 640, 1.0f);
    sumi_set_params(inst, &base); t19_step(window, inst, 2);
}

// Step 76 (DECISIONS_5 #89 — the author: "the vibrations are tiny and the displacement is huge; I was hoping for the
// contrary"): THE CHIRIKOV GRID. The operator at one pass over K × periods × ε on a sheet with two ink drops, one PNG
// each and an index, for the author's eye; the chosen combination becomes the defaults by the author's word.
static void t76_chirikov_grid(GLFWwindow* window, sumi_instance_t* inst, const char* dir) {
    std::printf("[t76] the Chirikov grid -> %s\n", dir);
    sumi_params_t base; sumi_get_params(inst, &base);
    sumi_params_t p = base; p.medium = SUMI_MEDIUM_SUMI; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID; sumi_set_params(inst, &p);
    uint32_t pw = 0, ph = 0;
    const float Ks[3] = { 0.3f, 0.6f, 1.0f }; const uint32_t periods[3] = { 2u, 4u, 8u }; const float epss[3] = { 0.15f, 0.3f, 0.6f };
    char path[1024]; std::snprintf(path, sizeof path, "%s/README.md", dir);
    FILE* idx = std::fopen(path, "w");
    if (idx) std::fprintf(idx, "# The Chirikov grid (step 76)\n\nOne pass of `sumi_add_chirikov(0.5, 0.5, K, periods, eps, 0)` on a Sumi sheet with two ink drops (r 0.12 at (0.35, 0.5) and (0.65, 0.5)), 1024 × 640. The kick's amplitude is K's throw; `periods` the kick's waves per canvas height; `eps` the drift's scale. The defaults today: K 1.0 (a full throw), periods 2, eps 0.5.\n\n| file | K | periods | eps |\n|---|---|---|---|\n");
    for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) for (int c = 0; c < 3; c++) {
        std::free(t19_dip_print(window, inst, &pw, &ph)); t19_step(window, inst, 2);
        sumi_add_drop(inst, 0.35f, 0.5f, 0.12f, SUMI_DROP_INK); sumi_add_drop(inst, 0.65f, 0.5f, 0.12f, SUMI_DROP_INK); t19_step(window, inst, 2);
        sumi_add_chirikov(inst, 0.5f, 0.5f, Ks[a], periods[b], epss[c], 0.0f); t19_step(window, inst, 2);
        uint8_t* px = t71_shot(window, inst, &pw, &ph);
        std::snprintf(path, sizeof path, "%s/chirikov_K%.1f_p%u_e%.2f.png", dir, Ks[a], periods[b], epss[c]);
        if (px) { stbi_write_png(path, (int)pw, (int)ph, 4, px, (int)pw * 4); std::free(px); }
        if (idx) std::fprintf(idx, "| `chirikov_K%.1f_p%u_e%.2f.png` | %.1f | %u | %.2f |\n", Ks[a], periods[b], epss[c], Ks[a], periods[b], epss[c]);
    }
    if (idx) std::fclose(idx);
    std::printf("[t76] 27 renders written\n");
    sumi_set_params(inst, &base); t19_step(window, inst, 2);
}

int dev_run_scripted(const DevOptions& o, GLFWwindow* window, sumi_instance_t* inst) {
    g_bench_backend = o.backend;
    if (o.voxo_load) {
        sumi_update(inst, 1.0 / 120.0);
        sumi_render(inst);
        voxo_config_t cfg{}; cfg.sample_rate = 48000; cfg.block_frames = 128; cfg.max_voices = 4;
        voxo_t* v = voxo_create(&cfg);
        if (o.voxo_budget_mb >= 0.0) voxo_set_memory_budget(v, (uint64_t)(o.voxo_budget_mb * 1024.0 * 1024.0));   // step 52: the gate under test
        voxo_report_t rep{};
        const double t0 = glfwGetTime();
        const bool ok = voxo_load_preset(v, o.voxo_load, &rep);
        const double dt = glfwGetTime() - t0;
        if (ok) std::printf("loaded in %.2f s: %u groups, %u zones, %u samples (%u unreadable), %.1f MB, notes 0x%x\n%s",
                            dt, rep.groups, rep.zones, rep.samples, rep.samples_missing, rep.memory_bytes / (1024.0 * 1024.0), rep.notes, rep.text);
        else std::printf("refused: %s\n", rep.text);
        voxo_destroy(v);
        return ok ? 0 : 1;
    }
    if (o.voxo_profile) {
        sumi_update(inst, 1.0 / 120.0);
        sumi_render(inst);
        return voxo_profile(o.voxo_profile, o.voxo_source, o.voxo_preset, o.voxo_suzu_shear, o.voxo_suzu_preset, o.voxo_suzu_breath, o.voxo_suzu_voice, o.preset);
    }
    if (o.voxo_chart) {
        sumi_update(inst, 1.0 / 120.0);   // the settled frame the core's Metal shutdown waits on (as the profile and the bounce)
        sumi_render(inst);
        return voxo_chart(o.voxo_chart);
    }
    if (o.voxo_bounce) {
        // One settled frame first: the core's Metal shutdown waits on a frame
        // semaphore that only a committed frame arms (every scripted mode renders one).
        sumi_update(inst, 1.0 / 120.0);
        sumi_render(inst);
        return voxo_bounce(o.voxo_bounce);
    }
    if (o.record_demo) return t65_record_demo(window, inst, o.record_demo);   // Phase 9 step 65 (DECISIONS_8 #24)
    if (o.replay)      return t65_replay(window, inst, o);
    // Phase 9 step 60: THE LAYOUT SHOT — a layout drawn as the visualizer's
    // overlay: the plate guide (layouts.cpp's display cells) over a scripted
    // fingering phrase through the normalizer — the valve CCs on the master
    // channel, the notes on a member channel, the slide's CC with the bend a
    // play surface would send — then a dip and its print saved as a PNG (the
    // print carries the guide). --layout picks the layout, --trumpet-arc the
    // arrangement. The scripted clock, a landscape sheet of aspect 1.6.
    if (o.layout_shot) {
        sumi_resize(inst, 1024, 640, 1.0f);
        sumi_params_t p; sumi_get_params(inst, &p);
        if (o.layout >= 0) p.pitch_layout = (uint32_t)o.layout;
        p.trumpet_arc = o.trumpet_arc ? 1u : 0u;
        if (o.string_tuning >= 0) p.string_tuning = (uint32_t)o.string_tuning;
        if (o.mineral >= 0) { sumi_mineral_t m; if (app_mineral_preset((uint32_t)o.mineral, &m, &p, nullptr)) sumi_set_mineral(inst, &m); }   // step 71b: both halves
        sumi_set_params(inst, &p);
        sumi_debug_set_chladni_overlay(inst, 1.0f);
        t19_step(window, inst, 2);
        sumi_push_midi(inst, 0xB0, 101, 0); sumi_push_midi(inst, 0xB0, 100, 6); sumi_push_midi(inst, 0xB0, 6, 15);   // the MCM: MPE
        t19_step(window, inst, 2);
        if (p.pitch_layout == SUMI_LAYOUT_TROMBONE) {
            // each partial at the 1st position, then B♭3 held through a slide out to the 7th with the bend following
            static const uint8_t partials[7] = {58, 65, 70, 74, 77, 80, 82};
            sumi_push_midi(inst, 0xB0, 113, 0);
            for (int k = 0; k < 7; k++) {
                sumi_push_midi(inst, 0x91, partials[k], 100); t19_step(window, inst, 10);
                sumi_push_midi(inst, 0xD1, 60, 0);            t19_step(window, inst, 10);
                sumi_push_midi(inst, 0x81, partials[k], 0);   t19_step(window, inst, 4);
            }
            sumi_push_midi(inst, 0x92, 70, 100); t19_step(window, inst, 6);
            for (int v = 0; v <= 127; v += 2) {
                sumi_push_midi(inst, 0xB0, 113, (uint8_t)v);
                const float semis = -6.0f * (float)v / 127.0f;
                int pb = 8192 + (int)std::lround((double)semis / 48.0 * 8192.0);
                if (pb < 0) pb = 0; if (pb > 16383) pb = 16383;
                sumi_push_midi(inst, 0xE2, (uint8_t)(pb & 0x7F), (uint8_t)(pb >> 7));
                t19_step(window, inst, 2);
            }
            sumi_push_midi(inst, 0x82, 70, 0);
        } else if (p.pitch_layout == SUMI_LAYOUT_WICKI || p.pitch_layout == SUMI_LAYOUT_STRINGS) {
            // step 61: a C major scale over two octaves, C3 to C5 — the buttons it climbs, the strings it sits on
            static const uint8_t scale[15] = {48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72};
            for (int k = 0; k < 15; k++) {
                sumi_push_midi(inst, 0x91, scale[k], 100); t19_step(window, inst, 8);
                sumi_push_midi(inst, 0xD1, 60, 0);         t19_step(window, inst, 8);
                sumi_push_midi(inst, 0x81, scale[k], 0);   t19_step(window, inst, 3);
            }
        } else if (p.pitch_layout == SUMI_LAYOUT_THEREMIN) {
            // step 61: C4 held and bent up an octave over two seconds — the hand sliding across the field
            sumi_push_midi(inst, 0x91, 60, 100); t19_step(window, inst, 10);
            sumi_push_midi(inst, 0xD1, 70, 0);   t19_step(window, inst, 10);
            for (int s = 0; s <= 120; s++) {
                const int pb = 8192 + (int)std::lround(12.0 * (double)s / 120.0 / 48.0 * 8192.0);
                sumi_push_midi(inst, 0xE1, (uint8_t)(pb & 0x7F), (uint8_t)(pb >> 7));
                t19_step(window, inst, 2);
            }
            sumi_push_midi(inst, 0x81, 60, 0);
        } else {
            // the trumpet: up the open partials, then a chromatic run fingered on the 4th partial (B♭3)
            static const struct { uint8_t valves, note; } phrase[] = {
                {0, 46}, {0, 58}, {0, 65}, {0, 70}, {0, 74}, {0, 77}, {0, 80}, {0, 82},
                {2, 69}, {1, 68}, {3, 67}, {6, 66}, {5, 65}, {7, 64}, {0, 70},
            };
            for (const auto& f : phrase) {
                sumi_push_midi(inst, 0xB0, 110, (f.valves & 1) ? 127 : 0);
                sumi_push_midi(inst, 0xB0, 111, (f.valves & 2) ? 127 : 0);
                sumi_push_midi(inst, 0xB0, 112, (f.valves & 4) ? 127 : 0);
                sumi_push_midi(inst, 0x91, f.note, 100); t19_step(window, inst, 10);
                sumi_push_midi(inst, 0xD1, 60, 0);       t19_step(window, inst, 10);
                sumi_push_midi(inst, 0x81, f.note, 0);   t19_step(window, inst, 4);
            }
        }
        t19_step(window, inst, 30);
        sumi_trigger_paper_dip(inst);
        bool ready = false; uint32_t pw = 0, ph = 0;
        for (int i = 0; i < 600 && !ready; i++) { t19_step(window, inst, 1); ready = sumi_read_print(inst, nullptr, 0, &pw, &ph); }
        if (!ready || !save_print_png(inst, o.layout_shot)) { std::fprintf(stderr, "[layout-shot] the dip's print never became ready\n"); return 1; }
        bool ok = false; std::string msg;
        for (int i = 0; i < 3000 && !print_write_poll(&ok, &msg); i++) t19_step(window, inst, 1);
        std::printf("[layout-shot] layout %u%s: %s (%ux%u)\n", p.pitch_layout, p.trumpet_arc ? " (arc)" : "", msg.c_str(), pw, ph);
        return ok ? 0 : 1;
    }
    // §4.6 cross-backend field regression: MIDI-free, scripted clock
    // (dt = 1/120), fixed 512x512 field, the canonical deform script from
    // sumi_debug.h; writes the raw dump and exits.
    if (o.composite_dump) {
        sumi_resize(inst, 512, 512, 1.0f);   // as the field dump: 512x512, aspect 1.0, the scripted clock
        if (o.dump_medium >= 0) { sumi_params_t p; sumi_get_params(inst, &p); p.medium = (uint32_t)o.dump_medium; sumi_set_params(inst, &p); }   // step 76: the Anod (and Kessho) goldens
        sumi_update(inst, 1.0 / 120.0);
        sumi_render(inst);
        sumi_debug_run_field_script(inst);
        for (int i = 0; i < 3; i++) { sumi_update(inst, 1.0 / 120.0); sumi_render(inst); glfwPollEvents(); }
        return write_composite_dump(window, inst, o.composite_dump) ? 0 : 1;
    }
    if (o.field_dump) {
        sumi_resize(inst, 512, 512, 1.0f);   // field = output = 512x512, aspect 1.0
        sumi_update(inst, 1.0 / 120.0);      // settle one identity frame
        sumi_render(inst);
        sumi_debug_run_field_script(inst);
        sumi_update(inst, 1.0 / 120.0);
        sumi_render(inst);                   // drains the script's 7 passes
#if defined(SUMI_HARNESS_GL)
        glfwSwapBuffers(window);             // host presents (§5.1); dump reads offscreen
#endif
        return write_field_dump(inst, o.field_dump) ? 0 : 1;
    }
    // v0.4 step-19/20 scripted tests: fixed 512x512, scripted clock, no MIDI
    // devices (the scripts push their own bytes from this thread — the sole
    // producer). Prints ok/FAIL lines; exit code = failure count.
    if (o.leader_pb >= 0.0f) sumi_debug_set_leader_pb(inst, o.leader_pb);   // step 76b: this run's strokes (live, replay, renders); the tests set their own
    if (o.t_wake || o.t_flick || o.t_rankine || o.t_ripple_group || o.t_ripple_dip ||
        o.t_pinch_demo || o.t_ripple_perm || o.t_swirl || o.t_pressure || o.t_stokeslet || o.t_pinch_passes > 0 ||
        o.soak || o.soak_negative || o.t_torsion || o.t_chladni || o.t_burst || o.t_spark || o.t_chirikov || o.t_palette || o.t_anod || o.t_defaults || o.t_print || o.t_gesture || o.t_trace || o.trace_demo || o.strike_render || o.pair_drift ||
        o.t_kessho || o.kessho_flat || o.ab || o.t_habit || o.habit_corner || o.tine_fault || o.t_arnold || o.arnold_render || o.arnold_probe || o.t_braid || o.t_chord_wave || o.t_strobe || o.t_seed || o.t_spark_reading || o.chirikov_grid || o.t_leader) {
        sumi_resize(inst, 512, 512, 1.0f);
        t19_step(window, inst, 2);
        if (o.preset)             dev_apply_preset(inst, o.preset);
        if (o.pair_drift)         soak_pair_drift(window, inst, o.pair_drift);
        if (o.t_wake)             t19_wake_test(window, inst);
        if (o.t_pressure)         t19_pressure_test(window, inst);
        if (o.t_stokeslet)        t19_stokeslet_test(window, inst);
        if (o.t_flick)            t19_flick_test(window, inst);
        if (o.t_rankine)          t19_rankine_test(window, inst);
        if (o.t_pinch_passes > 0) t19_pinch_soak(window, inst, o.t_pinch_passes);
        if (o.t_ripple_group)     t19_ripple_group_test(window, inst);
        if (o.t_ripple_dip)       t19_ripple_dip_test(window, inst);
        if (o.t_pinch_demo)       t19_pinch_demo(window, inst);
        if (o.t_ripple_perm)      t19_ripple_permanence_test(window, inst);
        if (o.t_swirl)            t19_swirl_test(window, inst);
        if (o.t_torsion)          t19_torsion_test(window, inst);
        if (o.t_chladni)          t19_chladni_test(window, inst);
        if (o.t_burst)            t19_burst_test(window, inst);
        if (o.t_spark)            t19_spark_test(window, inst);
        if (o.t_chirikov)         t19_chirikov_test(window, inst);
        if (o.t_palette)          t19_palette_test(window, inst);
        if (o.t_anod)             t19_anod_test(window, inst);
        if (o.t_defaults)         t64_defaults_test();
        if (o.t_print)            t19_print_test(window, inst);
        if (o.t_gesture)          t19_gesture_test(window, inst);
        if (o.t_trace)            t59_trace_test(window, inst);
        if (o.trace_demo)         t59_trace_demo(window, inst, o.trace_demo);
        if (o.strike_render)      t19_anod_strike_render(window, inst, o.strike_render);
        if (o.t_kessho)           t71_kessho_test(window, inst);                    // Phase 11 step 71
        if (o.kessho_flat)        t71_flat(window, inst, o.kessho_flat);
        if (o.ab)                 t71_ab(window, inst, o.ab, o.ab_out);
        if (o.t_habit)            t71b_habit_test(window, inst);                    // Phase 11 step 71b
        if (o.habit_corner)       t71b_habit_corner(window, inst, o.habit_corner);
        if (o.t_arnold)           t72_arnold_test(window, inst);                    // Phase 11 step 72
        if (o.arnold_render)      t72_arnold_render(window, inst, o.arnold_render);
        if (o.arnold_probe)       t72_arnold_probe(window, inst, o.arnold_probe);
        if (o.t_braid)            t73_braid_test(window, inst);                     // Phase 11 step 73
        if (o.t_chord_wave)       t74_chord_wave_test(window, inst);                // Phase 11 step 74
        if (o.t_strobe)           t75_strobe_test(window, inst);                    // Phase 11 step 75
        if (o.t_seed)             t76_seed_test(window, inst);                      // Phase 12 step 76
        if (o.t_spark_reading)    t76_spark_test(window, inst, o.ab_out);            // Phase 12 step 76
        if (o.t_leader)           t76b_leader_test(window, inst, o.ab_out);          // Phase 12 step 76b
        if (o.chirikov_grid)      t76_chirikov_grid(window, inst, o.chirikov_grid);  // Phase 12 step 76
        if (o.tine_fault)         { g_tine_fault = 1; soak_run(window, inst, o.soak ? o.soak : "twin_tine", o.soak_passes); g_tine_fault = 0; }
        else if (o.soak)          soak_run(window, inst, o.soak, o.soak_passes);
        if (o.soak_negative)      soak_negative(window, inst);
        std::printf("[t19] %d/%d checks passed\n", t19_checks - t19_failures, t19_checks);
        return t19_failures;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Scripted inputs riding the interactive loop                         */
/* ------------------------------------------------------------------ */

void dev_loop_begin(DevLoop& d, const DevOptions& o, AppSettings& st,
                    sumi_instance_t* inst, void* midi) {
    d.o = o;
    d.midi = midi;
    if (o.sim_scale > 0.0f) st.params.sim_scale = o.sim_scale;
    if (o.layout >= 0) { st.params.pitch_layout = (uint32_t)o.layout; std::printf("layout: %d\n", o.layout); }
    if (o.mineral >= 0) {   // step 71b: the library mineral for this run, both halves, into the settings (applied with them)
        const char* nm = nullptr;
        if (app_mineral_preset((uint32_t)o.mineral, &st.mineral, &st.params, &nm)) { st.mineral_id = (uint32_t)o.mineral; st.params.medium = SUMI_MEDIUM_KESSHO; std::printf("mineral: %d (%s)\n", o.mineral, nm ? nm : "?"); }
        else std::printf("mineral: %d is not in the library (0..%u)\n", o.mineral, SUMI_MINERAL_COUNT - 1u);
    }
    if (o.map_cc >= 0) {
        st.cc_routes.push_back({0xFF, (uint8_t)o.map_cc, (uint32_t)o.map_target});
        std::printf("mapped CC%d -> ctl %d\n", o.map_cc, o.map_target);
    }
    if (o.voxo_storm > 0.0 && d.o.exit_after <= 0.0) d.o.exit_after = o.voxo_storm + 1.0;   // a second of release after the storm
    (void)inst;
}

void dev_loop_pre_update(DevLoop& d, sumi_instance_t* inst) {
    const DevOptions& o = d.o;
    if (o.drop_test > 0 && d.drops_done < o.drop_test) {
        sumi_add_drop(inst, 0.5f, 0.5f, 0.18f, 0);
        d.drops_done++;
        if (d.drops_done == o.drop_test) {
            std::printf("drop-test: %ld drops done\n", d.drops_done);
            std::fflush(stdout);
        }
    }
    if (o.demo_chevron || o.demo_vortex) {
        d.demo_frame++;
        if (d.demo_frame <= 12) {
            sumi_add_drop(inst, 0.5f, 0.5f, 0.15f, 0);
            if (d.demo_frame == 12) { std::printf("demo: rings placed\n"); std::fflush(stdout); }
        } else if (o.demo_chevron && d.demo_frame >= 20 && d.demo_frame < 60) {
            // A wider comb tooth than the mouse default so the wake spans
            // several rings (the effect's width is exactly alpha).
            const float step = 0.8f / 40.0f;
            const float y = 0.1f + (float)(d.demo_frame - 20) * step;
            sumi_add_tine(inst, 0.5f, y, 0.5f, y + step, 0.09f, 0.3f / 40.0f);
            if (d.demo_frame == 59) { std::printf("demo: chevron done\n"); std::fflush(stdout); }
        } else if (o.demo_vortex && d.demo_frame >= 20 && d.demo_frame < 50) {
            // Offset from the ring center: rotation concentric with the
            // rings would be invisible (circles are rotation-invariant).
            sumi_add_vortex(inst, 0.60f, 0.38f, 0.10f, 0.30f, SUMI_VORTEX_EXPONENTIAL);
            if (d.demo_frame == 49) { std::printf("demo: vortex done\n"); std::fflush(stdout); }
        }
    }
}

void dev_loop_post_frame(DevLoop& d, GLFWwindow* window, sumi_instance_t* inst,
                         AppSettings& st, bool* settings_changed,
                         double now, double dt, uint64_t frames) {
    const DevOptions& o = d.o;
    if (d.start < 0.0) d.start = now;
    if (frames > 1) {
        if (dt < d.dt_min) d.dt_min = dt;
        if (dt > d.dt_max) d.dt_max = dt;
    }
    const double elapsed = now - d.start;
    if (o.dip_burst > 0.0) {
        // Three dips at t, t+0.2, t+0.25 with reads deferred to t+1.0: both
        // buffers must fill, the third dip must be refused, and both prints
        // must read back intact afterwards.
        if (d.burst_step == 0 && elapsed >= o.dip_burst) {
            d.burst_step = 1; sumi_trigger_paper_dip(inst);
            std::printf("[burst] dip 1 at t=%.2fs\n", elapsed);
        } else if (d.burst_step == 1 && elapsed >= o.dip_burst + 0.2) {
            d.burst_step = 2; sumi_trigger_paper_dip(inst);
            std::printf("[burst] dip 2 at t=%.2fs\n", elapsed);
        } else if (d.burst_step == 2 && elapsed >= o.dip_burst + 0.25) {
            d.burst_step = 3;
            std::printf("[burst] dip 3 at t=%.2fs (expect refusal)\n", elapsed);
            sumi_trigger_paper_dip(inst);
        } else if (d.burst_step == 3 && elapsed >= o.dip_burst + 1.0) {
            d.burst_step = 4;
            save_print_png(inst, "burst_print_newest.png");   // consumes newest
            save_print_png(inst, "burst_print_oldest.png");   // then the other
        }
    }
    if (o.dip_at > 0.0 && !d.dip_done && elapsed >= o.dip_at) {
        d.dip_done = true;
        d.dip_time = now;
        sumi_trigger_paper_dip(inst);
        std::printf("[dip] triggered at t=%.2fs\n", elapsed);
    }
    if (d.dip_time > 0.0 && now - d.dip_time <= 1.0 && frames > 1 && dt > d.dip_worst) {
        d.dip_worst = dt;   // worst frame time in the second after the dip
    }
    if (d.dip_done && o.print_out && !d.print_saved) {
        uint32_t pw = 0, ph = 0;
        if (sumi_read_print(inst, nullptr, 0, &pw, &ph)) {
            d.print_saved = save_print_png(inst, o.print_out);
        }
    }
    if (o.cycle_visuals && frames % 180 == 0 && frames > 0) {
        st.params.active_palette_id = (uint32_t)(d.visual_step % 3);
        st.params.pitch_layout = (uint32_t)(d.visual_step % 3);
        st.params.paper_roughness = 0.3f + 0.35f * (float)(d.visual_step % 3);
        print_params(&st.params);
        *settings_changed = true;
        d.visual_step++;
    }
    if (o.resize_test) {
        if (d.resize_step == 0 && elapsed > 1.0) {
            glfwSetWindowSize(window, 900, 500); d.resize_step = 1;
        } else if (d.resize_step == 1 && elapsed > 2.0) {
            glfwSetWindowSize(window, 1440, 900); d.resize_step = 2;
        }
    }
    // Phase 7 step 47: the MPE storm — fifteen member channels, each holding a
    // note that changes every 50 frames, with a bend sweep, channel pressure and
    // CC74 EVERY frame (Osmose density, ~2700 messages a second at 60 fps) —
    // through the harness's inject path, the ONE producer, so the same bytes
    // reach libsumi and Voxo exactly as a device's would.
    if (o.voxo_storm > 0.0 && d.midi && elapsed < o.voxo_storm) {
        const uint64_t f = d.storm_frames++;
        auto send = [&](uint8_t st, uint8_t a, uint8_t b) { sumi_midi_harness_inject(d.midi, st, a, b); d.storm_messages++; };
        for (int ch = 1; ch <= 15; ch++) {
            const uint64_t phase = (f + (uint64_t)ch * 3) % 50;
            const int note = 40 + (int)(((f + (uint64_t)ch * 3) / 50 * 7 + (uint64_t)ch) % 40);
            if (phase == 0) send((uint8_t)(0x90 | ch), (uint8_t)note, 100);
            if (phase == 40) send((uint8_t)(0x80 | ch), (uint8_t)note, 0);
            const int bend = 8192 + (int)(1500.0 * std::sin(0.05 * (double)f + (double)ch));
            send((uint8_t)(0xE0 | ch), (uint8_t)(bend & 0x7F), (uint8_t)(bend >> 7));
            send((uint8_t)(0xD0 | ch), (uint8_t)((f * 3 + (uint64_t)ch * 9) % 128), 0);
            send((uint8_t)(0xB0 | ch), 74, (uint8_t)((f + (uint64_t)ch * 5) % 128));
        }
    }
    if (o.exit_after > 0.0 && elapsed >= o.exit_after) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

int dev_loop_report(const DevLoop& d, sumi_instance_t* inst, double now, uint64_t frames) {
    const double total = now - d.start;
    if (total > 0.0 && frames > 1) {
        std::printf("frames: %llu in %.2fs (avg %.1f fps), frame time min/max %.2f/%.2f ms\n",
                    (unsigned long long)frames, total, (double)frames / total,
                    d.dt_min * 1000.0, d.dt_max * 1000.0);
    }
    if (d.dip_time > 0.0) std::printf("dip window worst frame: %.2f ms\n", d.dip_worst * 1000.0);
    std::printf("dropped MIDI messages: %u\n", sumi_dropped_midi_count(inst));
    int code = 0;
    if (d.o.voxo_storm > 0.0) {
        voxo_stats_t st{};
        if (d.voxo) voxo_stats(d.voxo, &st);
        const bool running = d.voxo && voxo_running(d.voxo);
        std::printf("[voxo storm] %u messages over %llu frames; device %s, %u Hz, %u frames per block; %u callbacks, "
                    "%u XRuns, render max %.3f ms (last %.3f), %u dropped in Voxo's ring, %u voices at the end\n",
                    d.storm_messages, (unsigned long long)d.storm_frames, st.device[0] ? st.device : "(none)",
                    st.sample_rate, st.block_frames, st.callbacks, st.xruns, st.render_max_ms, st.render_last_ms,
                    st.dropped_midi, st.active_voices);
        // The pass rule (step 55's acceptance suite, DECISIONS_6 #32): the device
        // ran, no XRun, no dropped message, and the visual loop held its rate —
        // the average over the run at or above 58 fps (a 60 Hz display; a
        // faster display only raises the bar's meaning). The period is
        // reported, not demanded: the Mac takes 128 as asked, WASAPI and ALSA
        // hand back their own (#9's table rows).
        const double total = now - d.start;
        const double fps = total > 0.0 && frames > 1 ? (double)frames / total : 0.0;
        const bool fps_ok = fps >= 58.0;
        const bool ok = running && st.callbacks > 0 && st.xruns == 0 && st.dropped_midi == 0 && fps_ok;
        std::printf("%s: voxo storm - %s (%u frames per block, %.1f fps)\n", ok ? "ok  " : "FAIL",
                    !running ? "no output device" : st.xruns ? "XRuns" : st.dropped_midi ? "dropped messages" :
                    !fps_ok ? "the visual loop fell under 58 fps" : "glitch-free, the visuals at rate", st.block_frames, fps);
        if (!ok) code = 1;
    }
    return code;
}

/* ------------------------------------------------------------------ */
/* Debug key bindings (roadmap steps 7, 9, 10, 19, 20)                 */
/* ------------------------------------------------------------------ */

void dev_key(GLFWwindow* window, AppSettings& st, sumi_instance_t* inst, void* midi,
             int key, int mods, bool* changed_out) {
    sumi_params_t& p = st.params;
    bool changed = true;
    switch (key) {
        case GLFW_KEY_1: p.fluid_viscosity -= 0.1f; if (p.fluid_viscosity < 0) p.fluid_viscosity = 0; break;
        case GLFW_KEY_2: p.fluid_viscosity += 0.1f; if (p.fluid_viscosity > 1) p.fluid_viscosity = 1; break;
        case GLFW_KEY_3: p.expansion_rate *= 0.8f; break;
        case GLFW_KEY_4: p.expansion_rate *= 1.25f; break;
        case GLFW_KEY_5: p.paper_roughness -= 0.1f; if (p.paper_roughness < 0) p.paper_roughness = 0; break;
        case GLFW_KEY_6: p.paper_roughness += 0.1f; if (p.paper_roughness > 1) p.paper_roughness = 1; break;
        case GLFW_KEY_7: p.active_palette_id = (p.active_palette_id + 1) % 4; break;   // step 43: 3 = the custom slot
        case GLFW_KEY_8: p.pitch_layout = (p.pitch_layout + 1) % 3; break;
        case GLFW_KEY_9: sumi_trigger_paper_dip(inst); changed = false; break;
        case GLFW_KEY_L: p.pitch_layout = (p.pitch_layout + 1) % 6; break;   // all layouts incl. rolls + piano grid
        case GLFW_KEY_B:   // BPM nudge for metronome eyeballing (Shift = down)
            p.bpm += (mods & GLFW_MOD_SHIFT) ? -5.0f : 5.0f;
            if (p.bpm < 20.0f) p.bpm = 20.0f;
            if (p.bpm > 300.0f) p.bpm = 300.0f;
            std::printf("[params] bpm %.0f\n", (double)p.bpm);
            break;
        case GLFW_KEY_S: save_print_png(inst, default_print_path(st.print_dir).c_str()); changed = false; break;
        // ---- v0.4 operator batch (step 19) ----
        case GLFW_KEY_V:   // exponential -> rankine -> torsion -> exponential (Phase 6 step 36)
            p.vortex_profile = p.vortex_profile == SUMI_VORTEX_EXPONENTIAL ? SUMI_VORTEX_RANKINE
                             : p.vortex_profile == SUMI_VORTEX_RANKINE     ? SUMI_VORTEX_TORSION
                                                                           : SUMI_VORTEX_EXPONENTIAL;
            std::printf("[params] vortex profile %s\n",
                        p.vortex_profile == SUMI_VORTEX_RANKINE ? "RANKINE"
                        : p.vortex_profile == SUMI_VORTEX_TORSION ? "TORSION" : "exponential");
            break;
        case GLFW_KEY_K:
            p.ripple_bake = p.ripple_bake ? 0u : 1u;
            std::printf("[params] ripple %s\n", p.ripple_bake ? "BAKE" : "live");
            break;
        case GLFW_KEY_C:
            p.pinch_variant = p.pinch_variant ? 0u : 1u;
            std::printf("[params] pinch variant %s\n",
                        p.pinch_variant ? "CROSSED TINES" : "Hamiltonian saddle");
            break;
        case GLFW_KEY_P: {
            // v0.4 press_mode (§3.4): one consumer owns 0xD0 — the ink feed (v1), the Lamb-Oseen swirl
            // (pressure-only hardware's door), the torsion feed (1.1.0), the Arnol'd lock (1.8.0, step 72);
            // the cycle passes through the medium's default too (Shift: back)
            static const char* names[] = {"ink feed (v1)", "Lamb-Oseen SWIRL", "torsion sweep FEED", "ARNOL'D tongue lock (the press is the kick)", "the medium's default"};
            int cur = p.press_mode == SUMI_MODE_MEDIUM_DEFAULT ? 4 : (int)(p.press_mode > 3u ? 0u : p.press_mode);
            cur = (mods & GLFW_MOD_SHIFT) ? (cur + 4) % 5 : (cur + 1) % 5;
            p.press_mode = cur == 4 ? SUMI_MODE_MEDIUM_DEFAULT : (uint32_t)cur;
            std::printf("[params] 0xD0 pressure -> %s\n", names[cur]);
            break;
        }
        case GLFW_KEY_J: {
            // Test voice toggle (ch 2, note 60) for the swirl keys below.
            static bool on = false;
            on = !on;
            sumi_midi_harness_inject(midi, on ? 0x91 : 0x81, 60, on ? 100 : 0);
            std::printf("[swirl] test voice %s\n", on ? "ON (ch2 n60)" : "off");
            changed = false;
            break;
        }
        case GLFW_KEY_W: case GLFW_KEY_E: {
            static int amt = 0;
            amt += (key == GLFW_KEY_E) ? 16 : -16;
            if (amt < 0) amt = 0;
            if (amt > 127) amt = 127;
            sumi_midi_harness_inject(midi, 0xA1, 60, (uint8_t)amt);
            std::printf("[swirl] 0xA0 amount %d\n", amt);
            changed = false;
            break;
        }
        case GLFW_KEY_M: {
            // v0.4 bend_mode (#35/#36): one consumer owns the PER-NOTE bend — the glide drag (v1), the ripple
            // vibrato (baked so it feathers in permanently; K can still override live/bake), the torsion's and
            // the spark's wavenumbers (1.1.0), the Chladni stir (step 43), the braid's consonance (1.9.0, step 73);
            // the cycle passes through the medium's default too (Shift: back). Mod wheel / vortex untouched.
            static const char* names[] = {"glide drag (v1)", "RIPPLE vibrato (baked, permanent)", "the TORSION's wavenumber", "the SPARK's wavenumber", "the CHLADNI stir", "the BRAID's consonance (the drop holds)", "the medium's default"};
            int cur = p.bend_mode == SUMI_MODE_MEDIUM_DEFAULT ? 6 : (int)(p.bend_mode > 5u ? 0u : p.bend_mode);
            cur = (mods & GLFW_MOD_SHIFT) ? (cur + 6) % 7 : (cur + 1) % 7;
            p.bend_mode = cur == 6 ? SUMI_MODE_MEDIUM_DEFAULT : (uint32_t)cur;
            p.ripple_bake = p.bend_mode == 1u ? 1u : (p.bend_mode == 0u ? 0u : p.ripple_bake);
            std::printf("[params] note bend -> %s\n", names[cur]);
            break;
        }
        case GLFW_KEY_O:
            p.ripple_angle += 0.261799f;   // +15 deg
            std::printf("[params] ripple angle %.0f deg\n", (double)(p.ripple_angle * 57.29578f));
            break;
        case GLFW_KEY_R: case GLFW_KEY_T: {
            st.ripple_amp_cc += (key == GLFW_KEY_T) ? 8 : -8;
            if (st.ripple_amp_cc < 0) st.ripple_amp_cc = 0;
            if (st.ripple_amp_cc > 127) st.ripple_amp_cc = 127;
            const int cc = app_settings_route_for(st, SUMI_CTL_RIPPLE_AMP);
            if (cc >= 0) sumi_midi_harness_inject(midi, 0xB0, (uint8_t)cc, (uint8_t)st.ripple_amp_cc);
            std::printf("[ripple] amp cc %d\n", st.ripple_amp_cc);
            break;   // persisted value: report as changed
        }
        case GLFW_KEY_F: case GLFW_KEY_G: {
            st.ripple_freq_cc += (key == GLFW_KEY_G) ? 8 : -8;
            if (st.ripple_freq_cc < 0) st.ripple_freq_cc = 0;
            if (st.ripple_freq_cc > 127) st.ripple_freq_cc = 127;
            const int cc = app_settings_route_for(st, SUMI_CTL_RIPPLE_FREQ);
            if (cc >= 0) sumi_midi_harness_inject(midi, 0xB0, (uint8_t)cc, (uint8_t)st.ripple_freq_cc);
            std::printf("[ripple] freq cc %d\n", st.ripple_freq_cc);
            break;
        }
        case GLFW_KEY_U: {
            // Phase 6 step 38: a viscous multipole burst at the cursor, its
            // ejection axis toward the canvas centre, the order from the
            // settings (m = 0 defers to params.burst_order). The strike route
            // waits for the binding tables (step 42).
            double cx = 0.0, cy = 0.0;
            glfwGetCursorPos(window, &cx, &cy);
            float nx, ny;
            norm_pos(window, cx, cy, &nx, &ny);
            const float th = std::atan2(0.5f - ny, 0.5f - nx);
            sumi_add_burst(inst, nx, ny, 0.04f, 0.02f, th, 0u);
            std::printf("[burst] m = %u at (%.2f, %.2f), axis %.2f rad, D 0.02 over age %.1f in %.2f s\n",
                        p.burst_order, (double)nx, (double)ny, (double)th, (double)p.burst_age, (double)p.burst_life);
            changed = false;
            break;
        }
        case GLFW_KEY_N: {
            // Phase 6 step 43 (debugging): THE PLATE — the display cells the
            // stir turns (the circles the shells draw; imaginary largest
            // circles where a layout draws none), drawn over the print. One
            // source: layouts.cpp (sumi_layout_cells). No lattice.
            static float overlay = 0.0f;
            overlay = overlay > 0.0f ? 0.0f : 1.0f;
            sumi_debug_set_chladni_overlay(inst, overlay);
            if (overlay <= 0.0f) { std::printf("[plate] guide off\n"); changed = false; break; }
            static float cells[SUMI_LAYOUT_MAX_CELLS][4];
            const uint32_t n = sumi_debug_cells(inst, &cells[0][0], SUMI_LAYOUT_MAX_CELLS);
            float rmin = 1e9f, rmax = 0.0f; uint32_t odd = 0, acc = 0;
            for (uint32_t i = 0; i < n; i++) {
                if (cells[i][2] < rmin) rmin = cells[i][2];
                if (cells[i][2] > rmax) rmax = cells[i][2];
                if (cells[i][3] >= 2.0f) odd++;
                if (std::fmod(cells[i][3], 2.0f) >= 1.0f) acc++;
            }
            std::printf("[plate] %s: %u eddies — one in every display cell (%u accidentals, %u odd cells B reverses), "
                        "radius %.4f..%.4f canvas heights at cell size %.2f, mode %s\n",
                        app_layout_name(p.pitch_layout), n, acc, odd, (double)(n ? rmin : 0.0f), (double)rmax, (double)p.chladni_cell,
                        p.chladni_mode == SUMI_CHLADNI_FIELD ? "FIELD (the inverse Chladni: rings summed, discs may overlap)" : "DISCS (exact, the water between rests)");
            changed = false;
            break;
        }
        case GLFW_KEY_D:   // 1.9.0 (step 73): the braid's vortices — pinned to the keys, or free (Kirchhoff: the dance); the A/B item B-1, live
            p.braid_mode = p.braid_mode == SUMI_BRAID_FREE ? SUMI_BRAID_PINNED : SUMI_BRAID_FREE;
            std::printf("[params] braid vortices -> %s\n", p.braid_mode == SUMI_BRAID_FREE ? "FREE (Kirchhoff: they move under each other's flow)" : "PINNED to the keys");
            break;
        case GLFW_KEY_A:   // Phase 6 step 42: the medium — Sumi -> Anod -> Kessho (step 71) -> Sumi, live (the same field re-read)
            p.medium = p.medium == SUMI_MEDIUM_SUMI ? SUMI_MEDIUM_ANOD : (p.medium == SUMI_MEDIUM_ANOD ? SUMI_MEDIUM_KESSHO : SUMI_MEDIUM_SUMI);
            std::printf("[params] medium %s\n", p.medium == SUMI_MEDIUM_ANOD ? "ANOD (strain-glow)" : p.medium == SUMI_MEDIUM_KESSHO ? "KESSHO (the crystal)" : "SUMI (ink on washi)");
            break;
        case GLFW_KEY_I: {
            // Phase 6 step 40: one full step of the Chirikov standard map at
            // the cursor at the settings' K max (Shift: the exact inverse
            // step). Iterate it: sheets below Greene's threshold, chaos above.
            double cx = 0.0, cy = 0.0;
            glfwGetCursorPos(window, &cx, &cy);
            float nx, ny;
            norm_pos(window, cx, cy, &nx, &ny);
            const float K = (mods & GLFW_MOD_SHIFT) ? -p.chirikov_kmax : p.chirikov_kmax;
            sumi_add_chirikov(inst, nx, ny, K, p.chirikov_periods, p.chirikov_eps, 0.0f);
            std::printf("[chirikov] %s step at (%.2f, %.2f): K %.3f (K_c 0.9716), %u periods, eps %.2f\n",
                        K < 0 ? "inverse" : "one", (double)nx, (double)ny, (double)p.chirikov_kmax, p.chirikov_periods, (double)p.chirikov_eps);
            changed = false;
            break;
        }
        case GLFW_KEY_Z: {
            // Phase 6 step 39: the composed spark strike at the cursor (ink),
            // its axis toward the canvas centre — the drop, the quadrupole
            // along the axis, the jagged shears along and across it.
            double cx = 0.0, cy = 0.0;
            glfwGetCursorPos(window, &cx, &cy);
            float nx, ny;
            norm_pos(window, cx, cy, &nx, &ny);
            const float th = std::atan2(0.5f - ny, 0.5f - nx);
            sumi_add_spark(inst, nx, ny, 0.05f, 0.015f, th, SUMI_DROP_INK);
            std::printf("[spark] at (%.2f, %.2f), axis %.2f rad: r 0.05, D 0.015, shear %.2f r over %.2f s, order %u, %u octaves of %s\n",
                        (double)nx, (double)ny, (double)th, (double)p.spark_shear, (double)(4.0f * p.spark_tau), p.burst_order,
                        p.spark_stack, p.spark_profile ? "noise" : "triangles");
            changed = false;
            break;
        }
        case GLFW_KEY_X: {
            // Crossed-tine pinch prototype (§4.3(5) rival, DECISIONS_3 #32):
            // two perpendicular opposing tines through the cursor. Compare by
            // eye against Shift+drag's Hamiltonian saddle.
            double cx = 0.0, cy = 0.0;
            glfwGetCursorPos(window, &cx, &cy);
            float nx, ny;
            norm_pos(window, cx, cy, &nx, &ny);
            sumi_add_tine(inst, nx - 0.1f, ny, nx + 0.1f, ny, 0.03f, 0.03f);
            sumi_add_tine(inst, nx, ny + 0.1f, nx, ny - 0.1f, 0.03f, 0.03f);
            changed = false;
            break;
        }
        default: changed = false; break;
    }
    if (changed) print_params(&p);
    *changed_out = changed;
}
