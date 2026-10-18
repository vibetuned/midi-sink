// simd.h — Phase 13 step 78 (TO_PROJECT_SPEC §13.8, DECISIONS_12 #1): FOUR FLOAT LANES behind one header, one source.
//
//   NEON on arm64 (the Mac, the iPad, the Tab), SSE2 on x86-64 (the baseline every x86 lane builds — AVX2's eight lanes
//   would need a flag the release lanes do not set), WASM SIMD128 on the web (-msimd128), and a scalar fallback that
//   performs the same operations in the same order (VOXO_NO_SIMD: the web gate's reference build; any other target).
//
// THE CONTRACT the web gate rests on: every helper does per lane exactly what the scalar code does per element, in the
// same order — so a vectorised loop and its scalar twin are bit for bit when neither fuses, and `mul_add` / `mul_sub`
// are fused ONLY where the scalar build's compiler fuses too (VOXO_NO_FMA off: the shipping native builds; on: the
// reference build and the wasm, which has no fused multiply-add). Horizontal sums are the caller's, in scalar order.
#pragma once
#include <stdint.h>

// The helpers are forced inline: under a Debug build (-O0) a wrapper left as a call costs more than the scalar arithmetic
// it wraps — the suite's Debug run measured ten times the cost before this; the Release build inlines them anyway.
#if defined(_MSC_VER)
#define SUZU_SIMD_INLINE __forceinline
#else
#define SUZU_SIMD_INLINE inline __attribute__((always_inline))
#endif

#if !defined(VOXO_NO_SIMD) && defined(__ARM_NEON)
#include <arm_neon.h>
#define VOXO_SIMD_NEON 1
#elif !defined(VOXO_NO_SIMD) && defined(__wasm_simd128__)
#include <wasm_simd128.h>
#define VOXO_SIMD_WASM 1
#elif !defined(VOXO_NO_SIMD) && (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#include <emmintrin.h>
#define VOXO_SIMD_SSE2 1
#else
#define VOXO_SIMD_SCALAR 1
#endif

namespace suzu {

struct f32x4 {
#if VOXO_SIMD_NEON
    float32x4_t v;
#elif VOXO_SIMD_WASM
    v128_t v;
#elif VOXO_SIMD_SSE2
    __m128 v;
#else
    float v[4];
#endif
};

// ---- loads, stores, broadcasts ------------------------------------------------------------------------------------
static SUZU_SIMD_INLINE f32x4 f4_load(const float* p) {
#if VOXO_SIMD_NEON
    return { vld1q_f32(p) };
#elif VOXO_SIMD_WASM
    return { wasm_v128_load(p) };
#elif VOXO_SIMD_SSE2
    return { _mm_loadu_ps(p) };
#else
    return { { p[0], p[1], p[2], p[3] } };
#endif
}
static SUZU_SIMD_INLINE void f4_store(float* p, f32x4 a) {
#if VOXO_SIMD_NEON
    vst1q_f32(p, a.v);
#elif VOXO_SIMD_WASM
    wasm_v128_store(p, a.v);
#elif VOXO_SIMD_SSE2
    _mm_storeu_ps(p, a.v);
#else
    p[0] = a.v[0]; p[1] = a.v[1]; p[2] = a.v[2]; p[3] = a.v[3];
#endif
}
static SUZU_SIMD_INLINE f32x4 f4_set1(float x) {
#if VOXO_SIMD_NEON
    return { vdupq_n_f32(x) };
#elif VOXO_SIMD_WASM
    return { wasm_f32x4_splat(x) };
#elif VOXO_SIMD_SSE2
    return { _mm_set1_ps(x) };
#else
    return { { x, x, x, x } };
#endif
}
static SUZU_SIMD_INLINE f32x4 f4_set(float a, float b, float c, float d) {
#if VOXO_SIMD_NEON
    const float t[4] = { a, b, c, d }; return { vld1q_f32(t) };
#elif VOXO_SIMD_WASM
    return { wasm_f32x4_make(a, b, c, d) };
#elif VOXO_SIMD_SSE2
    return { _mm_setr_ps(a, b, c, d) };
#else
    return { { a, b, c, d } };
#endif
}
static SUZU_SIMD_INLINE float f4_lane(f32x4 a, int i) {
#if VOXO_SIMD_NEON || VOXO_SIMD_WASM || VOXO_SIMD_SSE2
    float t[4]; f4_store(t, a); return t[i];
#else
    return a.v[i];
#endif
}

// ---- arithmetic ------------------------------------------------------------------------------------------------------
static SUZU_SIMD_INLINE f32x4 f4_add(f32x4 a, f32x4 b) {
#if VOXO_SIMD_NEON
    return { vaddq_f32(a.v, b.v) };
#elif VOXO_SIMD_WASM
    return { wasm_f32x4_add(a.v, b.v) };
#elif VOXO_SIMD_SSE2
    return { _mm_add_ps(a.v, b.v) };
#else
    return { { a.v[0] + b.v[0], a.v[1] + b.v[1], a.v[2] + b.v[2], a.v[3] + b.v[3] } };
#endif
}
static SUZU_SIMD_INLINE f32x4 f4_sub(f32x4 a, f32x4 b) {
#if VOXO_SIMD_NEON
    return { vsubq_f32(a.v, b.v) };
#elif VOXO_SIMD_WASM
    return { wasm_f32x4_sub(a.v, b.v) };
#elif VOXO_SIMD_SSE2
    return { _mm_sub_ps(a.v, b.v) };
#else
    return { { a.v[0] - b.v[0], a.v[1] - b.v[1], a.v[2] - b.v[2], a.v[3] - b.v[3] } };
#endif
}
static SUZU_SIMD_INLINE f32x4 f4_mul(f32x4 a, f32x4 b) {
#if VOXO_SIMD_NEON
    return { vmulq_f32(a.v, b.v) };
#elif VOXO_SIMD_WASM
    return { wasm_f32x4_mul(a.v, b.v) };
#elif VOXO_SIMD_SSE2
    return { _mm_mul_ps(a.v, b.v) };
#else
    return { { a.v[0] * b.v[0], a.v[1] * b.v[1], a.v[2] * b.v[2], a.v[3] * b.v[3] } };
#endif
}
// a·b + c — fused where the scalar build fuses (NEON, SSE2 with FMA3 unavailable at the baseline → plain), plain under
// VOXO_NO_FMA. SSE2's baseline has no fused instruction: the x86 shipping path is plain, as its scalar compiler's
// contraction is unavailable without -mfma (so the x86 scalar and SIMD paths agree bit for bit too).
static SUZU_SIMD_INLINE f32x4 f4_mul_add(f32x4 a, f32x4 b, f32x4 c) {
#if VOXO_SIMD_NEON && !defined(VOXO_NO_FMA)
    return { vfmaq_f32(c.v, a.v, b.v) };
#else
    return f4_add(f4_mul(a, b), c);
#endif
}
// c − a·b
static SUZU_SIMD_INLINE f32x4 f4_mul_sub(f32x4 c, f32x4 a, f32x4 b) {
#if VOXO_SIMD_NEON && !defined(VOXO_NO_FMA)
    return { vfmsq_f32(c.v, a.v, b.v) };
#else
    return f4_sub(c, f4_mul(a, b));
#endif
}
static SUZU_SIMD_INLINE f32x4 f4_div(f32x4 a, f32x4 b) {
#if VOXO_SIMD_NEON
    return { vdivq_f32(a.v, b.v) };
#elif VOXO_SIMD_WASM
    return { wasm_f32x4_div(a.v, b.v) };
#elif VOXO_SIMD_SSE2
    return { _mm_div_ps(a.v, b.v) };
#else
    return { { a.v[0] / b.v[0], a.v[1] / b.v[1], a.v[2] / b.v[2], a.v[3] / b.v[3] } };
#endif
}
static SUZU_SIMD_INLINE f32x4 f4_abs(f32x4 a) {
#if VOXO_SIMD_NEON
    return { vabsq_f32(a.v) };
#elif VOXO_SIMD_WASM
    return { wasm_f32x4_abs(a.v) };
#elif VOXO_SIMD_SSE2
    return { _mm_andnot_ps(_mm_set1_ps(-0.0f), a.v) };
#else
    return { { a.v[0] < 0 ? -a.v[0] : a.v[0], a.v[1] < 0 ? -a.v[1] : a.v[1], a.v[2] < 0 ? -a.v[2] : a.v[2], a.v[3] < 0 ? -a.v[3] : a.v[3] } };
#endif
}
// per-lane select: mask lanes (all ones) take a, the others b
static SUZU_SIMD_INLINE f32x4 f4_gt(f32x4 a, f32x4 b) {   // a > b → all ones
#if VOXO_SIMD_NEON
    return { vreinterpretq_f32_u32(vcgtq_f32(a.v, b.v)) };
#elif VOXO_SIMD_WASM
    return { wasm_f32x4_gt(a.v, b.v) };
#elif VOXO_SIMD_SSE2
    return { _mm_cmpgt_ps(a.v, b.v) };
#else
    f32x4 r; for (int i = 0; i < 4; i++) { uint32_t m = a.v[i] > b.v[i] ? 0xFFFFFFFFu : 0u; float f; __builtin_memcpy(&f, &m, 4); r.v[i] = f; } return r;
#endif
}
static SUZU_SIMD_INLINE f32x4 f4_select(f32x4 mask, f32x4 a, f32x4 b) {
#if VOXO_SIMD_NEON
    return { vbslq_f32(vreinterpretq_u32_f32(mask.v), a.v, b.v) };
#elif VOXO_SIMD_WASM
    return { wasm_v128_bitselect(a.v, b.v, mask.v) };
#elif VOXO_SIMD_SSE2
    return { _mm_or_ps(_mm_and_ps(mask.v, a.v), _mm_andnot_ps(mask.v, b.v)) };
#else
    f32x4 r; for (int i = 0; i < 4; i++) { uint32_t m; __builtin_memcpy(&m, &mask.v[i], 4); r.v[i] = m ? a.v[i] : b.v[i]; } return r;
#endif
}

// ---- the lattice's cells are AoS (x, y, eps, amp): four cells de-interleaved into four lane vectors and back ---------
struct f32x4x4 { f32x4 x, y, eps, amp; };
static SUZU_SIMD_INLINE f32x4x4 f4_load_cells(const float* p) {   // p: 4 cells × 4 floats
#if VOXO_SIMD_NEON
    const float32x4x4_t t = vld4q_f32(p);
    return { { t.val[0] }, { t.val[1] }, { t.val[2] }, { t.val[3] } };
#else
    return { f4_set(p[0], p[4], p[8], p[12]), f4_set(p[1], p[5], p[9], p[13]), f4_set(p[2], p[6], p[10], p[14]), f4_set(p[3], p[7], p[11], p[15]) };
#endif
}
static SUZU_SIMD_INLINE void f4_store_cells(float* p, f32x4x4 c) {
#if VOXO_SIMD_NEON
    float32x4x4_t t; t.val[0] = c.x.v; t.val[1] = c.y.v; t.val[2] = c.eps.v; t.val[3] = c.amp.v;
    vst4q_f32(p, t);
#else
    float x[4], y[4], e[4], a[4]; f4_store(x, c.x); f4_store(y, c.y); f4_store(e, c.eps); f4_store(a, c.amp);
    for (int i = 0; i < 4; i++) { p[4 * i] = x[i]; p[4 * i + 1] = y[i]; p[4 * i + 2] = e[i]; p[4 * i + 3] = a[i]; }
#endif
}

inline const char* simd_name() {
#if VOXO_SIMD_NEON
    return "neon";
#elif VOXO_SIMD_WASM
    return "wasm-simd128";
#elif VOXO_SIMD_SSE2
    return "sse2";
#else
    return "scalar";
#endif
}

} // namespace suzu
