// hostmpe headless unit tests (PHASE4_SPEC.md §3.2; ROADMAP_3 Step 15 DONE:
// "g is 0 at d = 0.03, continuous, reaches 1 at d = 1"). Grows with the
// allocator / bend mapping / rate limiter in Step 16.
#include "hostmpe.h"

#include <cmath>
#include <cstdio>

static int g_checks = 0, g_failures = 0;
#define CHECK(cond) do { \
        g_checks++; \
        if (!(cond)) { g_failures++; \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)
#define CHECK_NEAR(a, b, eps) do { \
        g_checks++; \
        const float _a = (a), _b = (b); \
        if (std::fabs(_a - _b) > (eps)) { g_failures++; \
            std::fprintf(stderr, "FAIL %s:%d: %s=%f != %s=%f\n", __FILE__, __LINE__, \
                         #a, (double)_a, #b, (double)_b); } \
    } while (0)

static void test_soft_knee() {
    // Zero inside and AT the deadband edge.
    CHECK(hostmpe_soft_knee(0.0f) == 0.0f);
    CHECK(hostmpe_soft_knee(0.015f) == 0.0f);
    CHECK(hostmpe_soft_knee(0.03f) == 0.0f);
    // Continuous from exactly 0: just past the knee, g ≈ ε/0.97 — no jump to
    // 3% (the zipper the hard threshold would cause).
    CHECK_NEAR(hostmpe_soft_knee(0.03f + 1e-4f), 1e-4f / 0.97f, 1e-6f);
    CHECK(hostmpe_soft_knee(0.0301f) < 0.001f);
    // Reaches exactly 1 at d = 1, clamps beyond.
    CHECK_NEAR(hostmpe_soft_knee(1.0f), 1.0f, 1e-7f);
    CHECK(hostmpe_soft_knee(1.5f) == 1.0f);
    // Midpoint value and monotonicity across the range.
    CHECK_NEAR(hostmpe_soft_knee(0.515f), (0.515f - 0.03f) / 0.97f, 1e-6f);
    float prev = -1.0f;
    for (int i = 0; i <= 200; i++) {
        const float g = hostmpe_soft_knee((float)i / 200.0f);
        CHECK(g >= prev);
        prev = g;
    }
    // Negative / NaN inputs stay silent, never negative.
    CHECK(hostmpe_soft_knee(-0.5f) == 0.0f);
    CHECK(hostmpe_soft_knee(NAN) == 0.0f);
}

static void test_joystick_eff() {
    float x = 9.0f, y = 9.0f;
    // Inside the deadband: (0, 0).
    hostmpe_joystick_eff(0.001f, 0.001f, 0.1f, &x, &y);
    CHECK(x == 0.0f && y == 0.0f);
    // Direction preserved, magnitude = g.
    hostmpe_joystick_eff(0.06f, 0.08f, 0.1f, &x, &y);   // ‖Δ‖ = 0.1 -> d = 1
    CHECK_NEAR(x, 0.6f, 1e-5f);                          // Δ̂ · 1
    CHECK_NEAR(y, 0.8f, 1e-5f);
    // Halfway deflection.
    hostmpe_joystick_eff(0.05f, 0.0f, 0.1f, &x, &y);     // d = 0.5
    CHECK_NEAR(x, (0.5f - 0.03f) / 0.97f, 1e-5f);
    CHECK_NEAR(y, 0.0f, 1e-7f);
    // Negative axes keep their sign.
    hostmpe_joystick_eff(-0.1f, 0.0f, 0.1f, &x, &y);
    CHECK_NEAR(x, -1.0f, 1e-5f);
    // Beyond R_max clamps to unit magnitude.
    hostmpe_joystick_eff(0.0f, 0.5f, 0.1f, &x, &y);
    CHECK_NEAR(y, 1.0f, 1e-5f);
    // Degenerate inputs: zero delta, non-positive radius.
    hostmpe_joystick_eff(0.0f, 0.0f, 0.1f, &x, &y);
    CHECK(x == 0.0f && y == 0.0f);
    hostmpe_joystick_eff(0.05f, 0.05f, 0.0f, &x, &y);
    CHECK(x == 0.0f && y == 0.0f);
    hostmpe_joystick_eff(0.05f, 0.05f, -1.0f, &x, &y);
    CHECK(x == 0.0f && y == 0.0f);
}

int main() {
    test_soft_knee();
    test_joystick_eff();
    if (g_failures) {
        std::fprintf(stderr, "%d/%d checks FAILED\n", g_failures, g_checks);
        return 1;
    }
    std::printf("hostmpe_tests: %d checks passed\n", g_checks);
    return 0;
}
