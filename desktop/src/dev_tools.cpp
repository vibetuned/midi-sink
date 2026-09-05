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
        {"--wake-test", &o.t_wake}, {"--flick-test", &o.t_flick},
        {"--rankine-test", &o.t_rankine}, {"--ripple-group-test", &o.t_ripple_group},
        {"--ripple-dip-test", &o.t_ripple_dip}, {"--pinch-demo", &o.t_pinch_demo},
        {"--ripple-permanence-test", &o.t_ripple_perm}, {"--swirl-test", &o.t_swirl},
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
        "    [--pinch-demo] [--ripple-permanence-test] [--swirl-test]\n", argv0);
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
        o.t_pinch_demo || o.t_ripple_perm || o.t_swirl || o.t_pinch_passes > 0) {
        sumi_resize(inst, 512, 512, 1.0f);
        t19_step(window, inst, 2);
        if (o.t_wake)             t19_wake_test(window, inst);
        if (o.t_flick)            t19_flick_test(window, inst);
        if (o.t_rankine)          t19_rankine_test(window, inst);
        if (o.t_pinch_passes > 0) t19_pinch_soak(window, inst, o.t_pinch_passes);
        if (o.t_ripple_group)     t19_ripple_group_test(window, inst);
        if (o.t_ripple_dip)       t19_ripple_dip_test(window, inst);
        if (o.t_pinch_demo)       t19_pinch_demo(window, inst);
        if (o.t_ripple_perm)      t19_ripple_permanence_test(window, inst);
        if (o.t_swirl)            t19_swirl_test(window, inst);
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
        case GLFW_KEY_V:
            p.vortex_profile = p.vortex_profile == SUMI_VORTEX_RANKINE
                                   ? SUMI_VORTEX_EXPONENTIAL : SUMI_VORTEX_RANKINE;
            std::printf("[params] vortex profile %s\n",
                        p.vortex_profile == SUMI_VORTEX_RANKINE ? "RANKINE" : "exponential");
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
