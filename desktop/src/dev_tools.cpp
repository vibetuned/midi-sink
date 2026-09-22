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
#include "sumi_debug.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "stb_image_write.h"   // implementation lives in print_export.cpp

#if defined(SUMI_HARNESS_GL)
// (defined by CMake on Linux — the host presents on GL, §5.1)
#endif

// Ripple key bindings ride the REAL ctl path: injected CCs, mapped at startup.
static const uint8_t RIPPLE_AMP_CC  = 102;
static const uint8_t RIPPLE_FREQ_CC = 103;

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
#define T19(cond, ...) do { \
        t19_checks++; \
        if (!(cond)) { t19_failures++; std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); } \
        else { std::printf("ok:   " __VA_ARGS__); std::printf("\n"); } \
    } while (0)

struct FieldF { uint32_t w = 0, h = 0; float* px = nullptr; };   // RGBA32F rows

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
    std::free(halves);
    out->w = w; out->h = h; out->px = px;
    return true;
}

// Raw half-float bytes, for the BITWISE ripple-group compare.
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

enum SoakOp {
    SOAK_TINE = 0, SOAK_PINCH_SADDLE, SOAK_PINCH_CROSS, SOAK_WAKE_DOUBLET, SOAK_WAKE_STOKESLET,
    SOAK_RIPPLE_BAKE, SOAK_SWIRL, SOAK_VORTEX_EXP, SOAK_VORTEX_RANKINE,
    SOAK_TORSION,   // Phase 6 step 36: the first new operator through the gate
    SOAK_CHLADNI,   // Phase 6 step 37
    SOAK_COUNT
};
struct SoakDesc { const char* name; bool exact; const char* det; };
static const SoakDesc SOAKS[SOAK_COUNT] = {
    {"tine",           true,  "shear along the line, p' = p + z*w(d)*u with d the perpendicular distance (unchanged by the shear) -> det J = 1 (Jaffer)"},
    {"pinch-saddle",   true,  "Hamiltonian saddle, the flow of H = xy: s = xy conserved along trajectories -> det J = 1 (DECISIONS_3 #32)"},
    {"pinch-cross",    true,  "two crossed tines composed, each a shear with det J = 1"},
    {"wake-doublet",   false, "potential doublet: div d = 0 as a field, applied in <= a/4 steps -> area-preserving to first order (DECISIONS_3 #32)"},
    {"wake-stokeslet", false, "2-D unsteady Stokeslet: div d = 0 as a field, applied in <= a/4 steps (DECISIONS_4 #53)"},
    {"ripple-bake",    true,  "shear x' = x + A*sin(k*y + phi) in the ripple frame: det J = 1 for ANY profile; CC route (fixed phi) - the bend route drifts phi on purpose (#36)"},
    {"swirl",          true,  "rotation by theta(r) = S*(1 - exp(-r^2/rc^2))/(2 pi r^2): r preserved -> det J = 1 (Lamb-Oseen)"},
    {"vortex-exp",     true,  "rotation by theta(r) = A*exp(-r/R): r preserved -> det J = 1"},
    {"vortex-rankine", true,  "rotation by theta(r), rigid core and 1/r^2 outside: r preserved -> det J = 1"},
    {"torsion",        true,  "wave torsion: rotation by theta(r) = A sin(k r - phi) e^(-r/R): r preserved -> det J = 1 at any A (MEDIUM 2.1)"},
    {"chladni",        true,  "Chladni lattice: kick-drift pair x1 = x + a cos(ky y), y1 = y + b cos(kx x1) - two shears, the second at the displaced x1 -> det J = 1 (MEDIUM 2.2)"},
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
static const double SOAK_DEV_EXACT = 8.0;
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
    if (op == SOAK_CHLADNI) {
        sumi_map_cc(inst, 0xFF, 106, SUMI_CTL_CHLADNI_A);
        sumi_map_cc(inst, 0xFF, 107, SUMI_CTL_CHLADNI_B);
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
    default: break;
    }
    sumi_set_params(inst, &p);
}
// One strong (+k, -k) pair through the gesture ABI (the ripple: its CC).
static void soak_pair(GLFWwindow* window, sumi_instance_t* inst, SoakOp op) {
    switch (op) {
    case SOAK_TINE:   // z = 0.05: ~25 texels at the line, the other pairs' order of displacement at the ink
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
    case SOAK_CHLADNI:    // one Taylor-Green step of Ψ 0.006 on a 3 x 2 lattice (~17 texels at the boundaries); the negative psi is the EXACT inverse
        sumi_add_chladni(inst,  0.006f, 0.0f, 1.0f / 3.0f, 0.0f, 0.5f, 0.0f);
        sumi_add_chladni(inst, -0.006f, 0.0f, 1.0f / 3.0f, 0.0f, 0.5f, 0.0f);
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
    case SOAK_TINE: {                                   // note bend, bend_mode 0: the glide tine
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
    case SOAK_SWIRL:                                    // channel pressure, press_mode 1
        sumi_push_midi(inst, 0xD1, v, 0);
        break;
    case SOAK_VORTEX_EXP: case SOAK_VORTEX_RANKINE: case SOAK_TORSION:   // the mod wheel (core default map)
        sumi_push_midi(inst, 0xB0, 1, v);
        break;
    case SOAK_CHLADNI:                                  // the stir control: a steady cellular flow whose rate wobbles
        sumi_push_midi(inst, 0xB0, 106, v);
        break;
    default: break;
    }
}
static void soak_stream_end(GLFWwindow* window, sumi_instance_t* inst, SoakOp op, const sumi_params_t& base) {
    switch (op) {
    case SOAK_TINE:        sumi_push_midi(inst, 0xE1, 0x00, 0x40); break;
    case SOAK_RIPPLE_BAKE: sumi_push_midi(inst, 0xB0, RIPPLE_AMP_CC, 0); break;
    case SOAK_SWIRL:       sumi_push_midi(inst, 0xD1, 0, 0); break;
    case SOAK_VORTEX_EXP: case SOAK_VORTEX_RANKINE: case SOAK_TORSION: sumi_push_midi(inst, 0xB0, 1, 0); break;
    case SOAK_CHLADNI: sumi_push_midi(inst, 0xB0, 106, 0); break;
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
    (void)op;   // the profile is already in params (soak_modes)
    sumi_add_wake(inst, SOAK_CX - d, 0.50f, SOAK_CX, 0.50f, a);
    t19_step(window, inst, 1);
    FieldF f;
    if (!t19_read_field(inst, &f)) return false;
    // The swept capsule (segment + a + 3 texels) is excluded, as in the flick
    // test: the potential doublet's body carries a slip surface — a genuine
    // tangential discontinuity of the flow, not a fold — where finite
    // differences stop measuring the map. Harmless for the viscous profile.
    const float margin = a + 3.0f / (float)f.w;
    const float x0 = SOAK_CX - d, x1 = SOAK_CX, yc = 0.50f;
    double mn = 1e9, sum = 0.0; long n = 0;
    for (uint32_t y = 2; y + 2 < f.h; y++) for (uint32_t x = 2; x + 2 < f.w; x++) {
        const float px = ((float)x + 0.5f) / (float)f.w, py = ((float)y + 0.5f) / (float)f.h;
        const float cx = px < x0 ? x0 : (px > x1 ? x1 : px);
        if ((px - cx) * (px - cx) + (py - yc) * (py - yc) < margin * margin) continue;
        const double det = t33_det_at(f, x, y);
        if (det < mn) mn = det;
        sum += det; n++;
    }
    std::free(f.px);
    *dmin = mn; *dmean = n ? sum / (double)n : 0.0;
    return true;
}

static void soak_one(GLFWwindow* window, sumi_instance_t* inst, const sumi_params_t& base, SoakOp op, long passes) {
    const SoakDesc& d = SOAKS[op];
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
        soak_pair(window, inst, op);
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
    if (d.exact) {
        T19(hi <= SOAK_MASS_PCT && lo >= -SOAK_MASS_PCT && dev <= SOAK_DEV_EXACT,
            "[soak] %s (b) inversion: 500 (+k,-k) pairs hold ink mass %+.2f%%/%+.2f%% (|<= %.0f%%|), pre-image dev %.2f texel (<= %.1f)",
            d.name, lo, hi, SOAK_MASS_PCT, dev, SOAK_DEV_EXACT);
    } else {
        std::printf("[soak] %s (b) pairs (informational for this class): mass %+.2f%%/%+.2f%%, pre-image dev %.2f texel after 500 one-sub-step pairs\n",
                    d.name, lo, hi, dev);
        if (!soak_substep_det(window, inst, op, &det_min, &det_mean)) { t19_failures++; std::printf("FAIL: [soak] %s field read\n", d.name); return; }
        T19(det_min > 0.5 && std::fabs(det_mean - 1.0) < 2e-3,
            "[soak] %s (b) first-order area preservation: one a/4 sub-step, pre-image det min %.3f (> 0.5) outside the swept capsule, mean %.5f (|1 - mean| < 2e-3)",
            d.name, det_min, det_mean);
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
    T19(moved > 100 && (s_max - mass_s0 <= 0.005 * mass_s0 || growth_rate <= SOAK_GROWTH_RATE_MAX),
        "[soak] %s (c) fabrication: mass grew at most %+.2f%% over %ld passes (%.1e/pass; <= 0.5%% or <= %.0e/pass), edge band %+.0f / interior %+.0f; the route moved %ld texels (alive)",
        d.name, growth, passes, growth_rate, SOAK_GROWTH_RATE_MAX, g_edge, g_int, moved);
    double control = 0.0;
    if (!soak_tine_control(window, inst, base, passes, &control)) { t19_failures++; std::printf("FAIL: [soak] %s control field read\n", d.name); return; }
    const double ratio = control > 1e-12 ? rate / control : 0.0;
    if (passes >= SOAK_GATE_MIN_PASSES) {
        T19(control > 0.0 && rate <= 2.0 * control + 1e-6,
            "[soak] %s (d) erosion: %.2e/pass vs glide-tine control %.2e/pass (x%.2f <= 2)", d.name, rate, control, ratio);
    } else {
        std::printf("[soak] %s (d) erosion (informational - %ld passes is below the %ld-pass steady window): %.2e/pass vs glide-tine control %.2e/pass\n",
                    d.name, passes, SOAK_GATE_MIN_PASSES, rate, control);
    }
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
static void soak_run(GLFWwindow* window, sumi_instance_t* inst, const char* which, long passes) {
    sumi_params_t base;
    sumi_get_params(inst, &base);            // the pristine core state (scripted modes never see settings)
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

    // --- Part B: THE LAYOUT IS THE PLATE — the flow's fixed points are the cell centres (eddies) and corners (saddles) ---
    // Chroma grid; the stir control up for 90 frames (~1.1 rad of cell rotation at
    // rate 1.5 rad/s); the cell geometry from the PUBLIC probe.
    sumi_cell_info_t c0, c1, cx1;
    sumi_params_t p = base; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID; p.chladni_faraday = 0;
    const bool pr = sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &p, 1.0f, 0.50f, 0.50f, &c0) &&
                    sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &p, 1.0f, 0.50f, 0.50f + 0.12f, &c1) &&
                    sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &p, 1.0f, 0.50f + 0.075f, 0.50f, &cx1);
    const float sy = pr ? (c1.cell_center_y - c0.cell_center_y) : 0.0f;
    const float sx = pr ? (cx1.cell_center_x - c0.cell_center_x) : 0.0f;
    const float ring = 0.15f * (sx < sy ? sx : sy);   // ~5 texels at 512: inside the linear regime of both point types
    // Run the flow on a fresh sheet, then measure: fixed points, types, and
    // the boundary midpoints (where the flow along the separatrices is fastest).
    auto stir_and_measure = [&](uint32_t faraday, double* fixed_c, double* fixed_k, double* c_tang, double* c_rad,
                                double* k_tang, double* k_rad, double* mid) {
        std::free(t19_dip_print(window, inst, &pw, &ph));
        p.chladni_faraday = faraday;
        sumi_set_params(inst, &p);
        t19_step(window, inst, 2);
        sumi_push_midi(inst, 0xB0, 106, 127);
        t19_step(window, inst, 150);                    // ~1.9 rad of cell rotation at 1.5 rad/s, less the smoother's ramp
        sumi_push_midi(inst, 0xB0, 106, 0);
        t19_step(window, inst, 30);
        FieldF g; if (!t19_read_field(inst, &g)) return false;
        double fc = 0.0, fk = 0.0, ct = 0.0, cr = 0.0, kt = 0.0, kr = 0.0, md = 0.0;
        for (int r = -3; r <= 3; r++) for (int k = -5; k <= 6; k++) {
            const float x = c0.cell_center_x + (float)k * sx, y = c0.cell_center_y + (float)r * sy;
            fc += t37_disp_at(g, x, y);
            // Neighbouring eddies COUNTER-rotate (the checkerboard of signs), so
            // each ring's rotation is taken in absolute value before averaging.
            double t, rr; t37_ring_type(g, x, y, ring, &t, &rr); ct += std::fabs(t); cr += rr;
        }
        for (int r = -3; r <= 2; r++) for (int k = -5; k <= 5; k++) {
            const float x = c0.cell_center_x + ((float)k + 0.5f) * sx, y = c0.cell_center_y + ((float)r + 0.5f) * sy;
            fk += t37_disp_at(g, x, y);
            double t, rr; t37_ring_type(g, x, y, ring, &t, &rr); kt += std::fabs(t); kr += rr;
            md += t37_disp_at(g, x, c0.cell_center_y + (float)r * sy);          // a vertical boundary's midpoint
        }
        std::free(g.px);
        *fixed_c = fc / 84.0; *fixed_k = fk / 66.0;
        *c_tang = ct / 84.0; *c_rad = cr / 84.0; *k_tang = kt / 66.0; *k_rad = kr / 66.0; *mid = md / 66.0;
        return true;
    };
    double fc, fk, ct, cr, kt, kr, md;
    const bool okb = pr && stir_and_measure(0, &fc, &fk, &ct, &cr, &kt, &kr, &md);
    T19(okb && md > 3.0 && fc < 0.15 * md && fk < 0.15 * md,
        "fixed points: after 150 stirred frames the 84 cell centres moved %.2f texel and the 66 corners %.2f, the boundary midpoints %.2f (both < 0.15 x)",
        fc, fk, md);
    T19(okb && std::fabs(ct) > 0.25 && std::fabs(kt) < 0.1 && kr > 0.15,
        "types: a ring round a cell centre ROTATES %.2f rad (an eddy); round a corner it rotates %.2f rad and STRETCHES |log r'/r| = %.2f (a saddle); centres stretch %.2f",
        ct, kt, kr, cr);
    // Faraday: the lattice half a cell over — the eddies sit on the corners and the saddles on the cells.
    double ffc, ffk, fct, fcr, fkt, fkr, fmd;
    const bool okf = pr && stir_and_measure(1, &ffc, &ffk, &fct, &fcr, &fkt, &fkr, &fmd);
    T19(okf && ffc < 0.15 * fmd && ffk < 0.15 * fmd && std::fabs(fct) < 0.1 && fcr > 0.15 && std::fabs(fkt) > 0.25,
        "Faraday swaps them: cell centres now saddles (rotation %.2f rad, stretch %.2f), corners now eddies (rotation %.2f rad); both still fixed (%.2f / %.2f texel)",
        fct, fcr, fkt, ffc, ffk);
    // The same fixed-point check on a 16:9 field: the lattice's x converts through the aspect.
    sumi_resize(inst, 768, 432, 1.0f);
    t19_step(window, inst, 2);
    double wfc, wfk, wct, wcr, wkt, wkr, wmd;
    const bool okw = pr && stir_and_measure(0, &wfc, &wfk, &wct, &wcr, &wkt, &wkr, &wmd);
    T19(okw && wmd > 3.0 && wfc < 0.15 * wmd && wfk < 0.15 * wmd && std::fabs(wct) > 0.25 && std::fabs(wkt) < 0.1,
        "on a 16:9 field too: centres %.2f and corners %.2f texel against boundary midpoints %.2f; centres rotate %.2f rad, corners %.2f",
        wfc, wfk, wmd, wct, wkt);
    sumi_resize(inst, 512, 512, 1.0f);
    t19_step(window, inst, 2);
    // The mapper's lattice and the probe's cells agree.
    float lsx = 0, lx0 = 0, lsy = 0, ly0 = 0;
    p.chladni_faraday = 0; sumi_set_params(inst, &p); t19_step(window, inst, 2);
    sumi_debug_chladni_lattice(inst, &lsx, &lx0, &lsy, &ly0);
    auto off_lattice = [](float u, float u0, float s) {
        return std::fabs(std::fmod((double)(u - u0) / s + 100.5, 1.0) - 0.5);
    };
    const double ox = off_lattice(c0.cell_center_x, lx0, lsx), oy = off_lattice(c0.cell_center_y, ly0, lsy);
    T19(pr && std::fabs(lsx - sx) < 1e-4 && std::fabs(lsy - sy) < 1e-4 && ox < 1e-3 && oy < 1e-3,
        "the lattice is the layout's: pitch %.4f / %.4f = the probe's %.4f / %.4f; the probe's cell centre is %.1e / %.1e pitch off a lattice centre",
        (double)lsx, (double)lsy, (double)sx, (double)sy, ox, oy);
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
            sum_u += std::fabs(back.px[i * 4] - half_to_float(hh[i * 4]));
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
    if (const char* v = need("--field-dump"))      { o.field_dump = v; return 1; }
    if (const char* v = need("--pinch-soak"))      { o.t_pinch_passes = std::atol(v); return 1; }
    if (const char* v = need("--soak"))            { o.soak = v; return 1; }
    if (const char* v = need("--soak-passes"))     { o.soak_passes = std::atol(v); return 1; }
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
        {"--soak-negative", &o.soak_negative}, {"--torsion-test", &o.t_torsion}, {"--chladni-test", &o.t_chladni},
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
        "    [--cycle-visuals] [--field-dump <file>] [--wake-test] [--flick-test]\n"
        "    [--rankine-test] [--pinch-soak <n>] [--ripple-group-test] [--ripple-dip-test]\n"
        "    [--pinch-demo] [--ripple-permanence-test] [--swirl-test] [--pressure-test] [--stokeslet-test]\n"
        "    [--soak <operator|all> [--soak-passes <n>]] [--soak-negative]   (the four-part conservation gate)\n"
        "    [--torsion-test]   (Phase 6 step 36: the wave torsion profile + the note-on sweep episode)\n"
        "    [--chladni-test]   (Phase 6 step 37: the Chladni lattice - inverse, live, dip, harmony, bake)\n", argv0);
}

const char* dev_key_legend() {
    return
        "1/2 viscosity   3/4 expansion   5/6 roughness   7 palette   8/L layout\n"
        "9 paper dip     B/Shift-B bpm   V vortex profile   K ripple live/bake\n"
        "C pinch variant P press_mode    M note-bend mode   O ripple angle +15\n"
        "R/T ripple amp (CC 102)   F/G ripple freq (CC 103)   X crossed-tine stamp\n"
        "J test voice (ch 2, n 60)  W/E 0xA0 swirl amount on it";
}

/* ------------------------------------------------------------------ */
/* Scripted run-and-exit modes                                         */
/* ------------------------------------------------------------------ */

int dev_run_scripted(const DevOptions& o, GLFWwindow* window, sumi_instance_t* inst) {
    // §4.6 cross-backend field regression: MIDI-free, scripted clock
    // (dt = 1/120), fixed 512x512 field, the canonical deform script from
    // sumi_debug.h; writes the raw dump and exits.
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
    if (o.t_wake || o.t_flick || o.t_rankine || o.t_ripple_group || o.t_ripple_dip ||
        o.t_pinch_demo || o.t_ripple_perm || o.t_swirl || o.t_pressure || o.t_stokeslet || o.t_pinch_passes > 0 ||
        o.soak || o.soak_negative || o.t_torsion || o.t_chladni) {
        sumi_resize(inst, 512, 512, 1.0f);
        t19_step(window, inst, 2);
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
        if (o.soak)               soak_run(window, inst, o.soak, o.soak_passes);
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
    if (o.map_cc >= 0) {
        st.cc_routes.push_back({0xFF, (uint8_t)o.map_cc, (uint32_t)o.map_target});
        std::printf("mapped CC%d -> ctl %d\n", o.map_cc, o.map_target);
    }
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
    if (o.exit_after > 0.0 && elapsed >= o.exit_after) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

void dev_loop_report(const DevLoop& d, sumi_instance_t* inst, double now, uint64_t frames) {
    const double total = now - d.start;
    if (total > 0.0 && frames > 1) {
        std::printf("frames: %llu in %.2fs (avg %.1f fps), frame time min/max %.2f/%.2f ms\n",
                    (unsigned long long)frames, total, (double)frames / total,
                    d.dt_min * 1000.0, d.dt_max * 1000.0);
    }
    if (d.dip_time > 0.0) std::printf("dip window worst frame: %.2f ms\n", d.dip_worst * 1000.0);
    std::printf("dropped MIDI messages: %u\n", sumi_dropped_midi_count(inst));
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
        case GLFW_KEY_7: p.active_palette_id = (p.active_palette_id + 1) % 3; break;
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
        case GLFW_KEY_P:
            // v0.4 press_mode (§3.4): one consumer owns 0xD0 — ink feed (v1)
            // or the Lamb-Oseen swirl (pressure-only hardware's door).
            p.press_mode = p.press_mode ? 0u : 1u;
            std::printf("[params] 0xD0 pressure -> %s\n",
                        p.press_mode ? "Lamb-Oseen SWIRL" : "ink feed (v1)");
            break;
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
        case GLFW_KEY_M:
            // v0.4 bend_mode (#35/#36): one consumer owns the PER-NOTE bend —
            // glide drag (v1) vs ripple vibrato (amount = bend distance,
            // baked so it feathers in permanently like glide; K can still
            // override live/bake manually). Mod wheel / vortex untouched.
            p.bend_mode = p.bend_mode ? 0u : 1u;
            p.ripple_bake = p.bend_mode;
            std::printf("[params] note bend -> %s\n",
                        p.bend_mode ? "RIPPLE vibrato (baked, permanent)" : "glide drag (v1)");
            break;
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
