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
    // Halfway deflection. r_max = 0.1 -> the absolute floor governs the knee
    // (0.006/0.1 = 0.06 > 0.03, DECISIONS_3 #16).
    hostmpe_joystick_eff(0.05f, 0.0f, 0.1f, &x, &y);     // d = 0.5
    CHECK_NEAR(x, (0.5f - 0.06f) / 0.94f, 1e-5f);
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

// Grid probe values at 16:9 (the ±171 DONE test's geometry).
static const float GRID_RMAX = 0.0571f;
static const float GRID_STEP = 0.1244f;

static void test_bend_deflection_and_14bit() {
    // Deadband, knee, continuity at d = 1, identity beyond (#10).
    CHECK(hostmpe_bend_deflection(0.0f) == 0.0f);
    CHECK(hostmpe_bend_deflection(0.03f) == 0.0f);
    CHECK_NEAR(hostmpe_bend_deflection(0.5f), (0.5f - 0.03f) / 0.97f, 1e-6f);
    CHECK_NEAR(hostmpe_bend_deflection(1.0f), 1.0f, 1e-6f);
    CHECK_NEAR(hostmpe_bend_deflection(1.5f), 1.5f, 1e-6f);
    CHECK_NEAR(hostmpe_bend_deflection(2.18f), 2.18f, 1e-6f);
    // §3.3 formula endpoints and the semitone count.
    CHECK(hostmpe_bend14(0.0f) == 8192);
    CHECK(hostmpe_bend14(1.0f) == 8192 + 171);
    CHECK(hostmpe_bend14(-1.0f) == 8192 - 171);
    CHECK(hostmpe_bend14(48.0f) == 16383);    // clamp (8192+8192 = 16384)
    CHECK(hostmpe_bend14(-48.0f) == 0);
    CHECK(hostmpe_bend14(12.0f) == 8192 + 2048);
}

static void test_emit_order_and_column_drag() {
    hostmpe_t* h = hostmpe_create();
    hostmpe_msg_t m[8];
    uint32_t n = 0;
    // Touch-down: center bend FIRST, then Note On (§5.1 emit order).
    int32_t v = hostmpe_touch_begin(h, 0.0, 60, 96, GRID_RMAX, 1.0f / GRID_STEP, 0.0f, m, 8, &n);
    CHECK(v >= 1 && v <= HOSTMPE_MEMBERS);
    CHECK(n == 2);
    CHECK(m[0].status == (0xE0 | v) && m[0].data1 == 0x00 && m[0].data2 == 0x40);
    CHECK(m[1].status == (0x90 | v) && m[1].data1 == 60 && m[1].data2 == 96);
    // THE DONE TEST: one grid column of drag = exactly ±171 counts at ±48.
    n = hostmpe_touch_update(h, v, GRID_STEP, 0.0f, m, 8);
    bool saw = false;
    for (uint32_t i = 0; i < n; i++) {
        if ((m[i].status & 0xF0) == 0xE0) {
            const int pb = m[i].data1 | (m[i].data2 << 7);
            CHECK(pb == 8192 + 171);
            saw = true;
        }
    }
    CHECK(saw);
    n = hostmpe_touch_update(h, v, -GRID_STEP, 0.0f, m, 8);
    saw = false;
    for (uint32_t i = 0; i < n; i++) {
        if ((m[i].status & 0xF0) == 0xE0) {
            const int pb = m[i].data1 | (m[i].data2 << 7);
            CHECK(pb == 8192 - 171);
            saw = true;
        }
    }
    CHECK(saw);
    // Two columns = exactly two semitones (absolute tracking beyond r_max).
    n = hostmpe_touch_update(h, v, 2.0f * GRID_STEP, 0.0f, m, 8);
    for (uint32_t i = 0; i < n; i++)
        if ((m[i].status & 0xF0) == 0xE0)
            CHECK((m[i].data1 | (m[i].data2 << 7)) == 8192 + 341);   // 2*170.67
    // Inside the deadband: bend returns to dead center.
    n = hostmpe_touch_update(h, v, 0.0005f, 0.0f, m, 8);
    for (uint32_t i = 0; i < n; i++)
        if ((m[i].status & 0xF0) == 0xE0)
            CHECK((m[i].data1 | (m[i].data2 << 7)) == 8192);
    // Lift: pressure 0 FIRST, then Note Off with the lift velocity.
    n = hostmpe_touch_end(h, v, 1.0, 80, m, 8);
    CHECK(n == 2);
    CHECK(m[0].status == (0xD0 | v) && m[0].data1 == 0);
    CHECK(m[1].status == (0x80 | v) && m[1].data1 == 60 && m[1].data2 == 80);
    hostmpe_destroy(h);
}

// §3.3 rev (DECISIONS_3 #19): the finger's Y axis is PRESSURE, upward only —
// 0 at touch-down, 0 for any downward Δy, monotonic through the soft knee,
// 127 at full-radius up. Fingers emit no CC74, ever.
static void test_y_pressure_upward_only() {
    hostmpe_t* h = hostmpe_create();
    hostmpe_msg_t m[8];
    uint32_t n = 0;
    int32_t v = hostmpe_touch_begin(h, 0.0, 60, 96, GRID_RMAX, 1.0f / GRID_STEP, 0.0f, m, 8, &n);
    // Touch-down pressure is 0 (implicit): a downward drag emits NOTHING for
    // pressure (still 0) and never any CC74.
    n = hostmpe_touch_update(h, v, 0.0f, GRID_RMAX, m, 8);
    for (uint32_t i = 0; i < n; i++) {
        CHECK((m[i].status & 0xF0) != 0xD0);
        CHECK(!((m[i].status & 0xF0) == 0xB0 && m[i].data1 == 74));
    }
    // Full-radius straight up -> pressure 127.
    n = hostmpe_touch_update(h, v, 0.0f, -GRID_RMAX, m, 8);
    bool saw = false;
    for (uint32_t i = 0; i < n; i++) {
        if ((m[i].status & 0xF0) == 0xD0) { CHECK(m[i].data1 == 127); saw = true; }
        CHECK(!((m[i].status & 0xF0) == 0xB0 && m[i].data1 == 74));
    }
    CHECK(saw);
    // Monotonic through the knee: quarter/half/three-quarter radius up.
    uint8_t prev = 0;
    for (int s = 1; s <= 3; s++) {
        // reset to a fresh distance each time via a detour through center
        hostmpe_touch_update(h, v, 0.0f, 0.0f, m, 8);
        n = hostmpe_touch_update(h, v, 0.0f, -GRID_RMAX * 0.25f * (float)s, m, 8);
        for (uint32_t i = 0; i < n; i++)
            if ((m[i].status & 0xF0) == 0xD0) {
                CHECK(m[i].data1 >= prev);
                prev = m[i].data1;
            }
    }
    CHECK(prev > 0 && prev < 127);
    // Back below the knee -> pressure returns to 0.
    n = hostmpe_touch_update(h, v, 0.0f, -0.0005f, m, 8);
    saw = false;
    for (uint32_t i = 0; i < n; i++)
        if ((m[i].status & 0xF0) == 0xD0) { CHECK(m[i].data1 == 0); saw = true; }
    CHECK(saw);
    // Change-only: an identical update emits nothing.
    n = hostmpe_touch_update(h, v, 0.0f, -GRID_RMAX, m, 8);
    CHECK(n > 0);
    CHECK(hostmpe_touch_update(h, v, 0.0f, -GRID_RMAX, m, 8) == 0);
    hostmpe_destroy(h);
}

static void test_allocator_lru_round_robin() {
    hostmpe_t* h = hostmpe_create();
    hostmpe_msg_t m[4];
    uint32_t n;
    int32_t ch[HOSTMPE_MEMBERS];
    // 15 touches -> 15 DISTINCT channels, ascending on the first pass.
    for (int i = 0; i < HOSTMPE_MEMBERS; i++) {
        ch[i] = hostmpe_touch_begin(h, 0.0, (uint8_t)(40 + i), 96,
                                    GRID_RMAX, 1.0f / GRID_STEP, 0.0f, m, 4, &n);
        CHECK(ch[i] == i + 1);
    }
    CHECK(hostmpe_active_voices(h) == HOSTMPE_MEMBERS);
    // Saturation: silent drop, no messages, never steal.
    n = 99;
    CHECK(hostmpe_touch_begin(h, 0.0, 100, 96, GRID_RMAX, 1.0f / GRID_STEP, 0.0f, m, 4, &n) == -1);
    CHECK(n == 0);
    CHECK(hostmpe_active_voices(h) == HOSTMPE_MEMBERS);
    // Release 7 then 9: the next touch takes 7 (LRU), NEVER 9 (most recent)
    // while another free channel exists.
    hostmpe_touch_end(h, 7, 1.0, 64, m, 4);
    hostmpe_touch_end(h, 9, 2.0, 64, m, 4);
    CHECK(hostmpe_touch_begin(h, 3.0, 61, 96, GRID_RMAX, 1.0f / GRID_STEP, 0.0f, m, 4, &n) == 7);
    CHECK(hostmpe_touch_begin(h, 3.0, 62, 96, GRID_RMAX, 1.0f / GRID_STEP, 0.0f, m, 4, &n) == 9);
    hostmpe_destroy(h);
}

static void test_external_occupancy_masking() {
    hostmpe_t* h = hostmpe_create();
    hostmpe_msg_t m[4];
    uint32_t n;
    // A ROLI holds notes on channels 3 and 5: the allocator must skip both.
    hostmpe_observe_external(h, 0.0, 0x90 | 3, 60, 100);
    hostmpe_observe_external(h, 0.0, 0x90 | 5, 64, 100);
    for (int i = 0; i < HOSTMPE_MEMBERS - 2; i++) {
        int32_t v = hostmpe_touch_begin(h, 1.0, 70, 96, GRID_RMAX, 1.0f / GRID_STEP, 0.0f, m, 4, &n);
        CHECK(v != 3 && v != 5 && v >= 1);
    }
    // 13 members taken by touches + 2 externally held = saturated.
    CHECK(hostmpe_touch_begin(h, 1.0, 71, 96, GRID_RMAX, 1.0f / GRID_STEP, 0.0f, m, 4, &n) == -1);
    // External Note Off frees channel 3 for the allocator.
    hostmpe_observe_external(h, 2.0, 0x80 | 3, 60, 0);
    CHECK(hostmpe_touch_begin(h, 2.5, 72, 96, GRID_RMAX, 1.0f / GRID_STEP, 0.0f, m, 4, &n) == 3);
    hostmpe_destroy(h);

    // Stuck-note timeout: occupancy clears after 30 s of channel silence, and
    // ANY traffic on the channel (a held note's pressure stream) refreshes it.
    h = hostmpe_create();
    hostmpe_observe_external(h, 0.0, 0x90 | 2, 60, 100);
    hostmpe_observe_external(h, 20.0, 0xD0 | 2, 90, 0);   // pressure refresh
    // At t=31 the channel had traffic 11 s ago: still masked.
    for (int i = 0; i < HOSTMPE_MEMBERS - 1; i++) {
        CHECK(hostmpe_touch_begin(h, 31.0, 70, 96, GRID_RMAX, 1.0f / GRID_STEP, 0.0f, m, 4, &n) != 2);
    }
    hostmpe_msg_t mm[4]; uint32_t nn;
    CHECK(hostmpe_touch_begin(h, 31.0, 70, 96, GRID_RMAX, 1.0f / GRID_STEP, 0.0f, mm, 4, &nn) == -1);
    hostmpe_destroy(h);
    h = hostmpe_create();
    hostmpe_observe_external(h, 0.0, 0x90 | 2, 60, 100);
    // 31 s of complete silence on the channel: stuck, cleared, allocatable.
    CHECK(hostmpe_touch_begin(h, 31.0, 70, 96, GRID_RMAX, 1.0f / GRID_STEP, 0.0f, m, 4, &n) == 1);
    CHECK(hostmpe_touch_begin(h, 31.0, 71, 96, GRID_RMAX, 1.0f / GRID_STEP, 0.0f, m, 4, &n) == 2);
    hostmpe_destroy(h);
}

// Jankó-class cells (r_max smaller than the finger): the absolute deadband
// floor keeps micro-wobble silent while real drags still track exactly.
static void test_knee_floor_small_cells() {
    const float JANKO_RMAX = 0.018f;
    const float JANKO_STEP = 0.135f;
    // Indicator math: 0.004 canvas of jitter (d = 0.22 — way past the 3%
    // proportional knee) stays at zero thanks to the floor (k = 0.333).
    float x = 9, y = 9;
    hostmpe_joystick_eff(0.004f, 0.003f, JANKO_RMAX, &x, &y);
    CHECK(x == 0.0f && y == 0.0f);
    // MIDI: the same jitter emits NO bend away from center and no CC74 move.
    hostmpe_t* h = hostmpe_create();
    hostmpe_msg_t m[8];
    uint32_t n = 0;
    const int32_t v = hostmpe_touch_begin(h, 0.0, 61, 96, JANKO_RMAX, 1.0f / JANKO_STEP, 0.0f,
                                          m, 8, &n);
    n = hostmpe_touch_update(h, v, 0.004f, 0.003f, m, 8);
    for (uint32_t i = 0; i < n; i++) {
        if ((m[i].status & 0xF0) == 0xE0)
            CHECK((m[i].data1 | (m[i].data2 << 7)) == 8192);
        if ((m[i].status & 0xF0) == 0xB0 && m[i].data1 == 74)
            CHECK(m[i].data2 == 64);
    }
    // A real one-step drag is still EXACTLY one semitone (identity beyond the
    // circle is knee-independent).
    n = hostmpe_touch_update(h, v, JANKO_STEP, 0.0f, m, 8);
    bool saw = false;
    for (uint32_t i = 0; i < n; i++)
        if ((m[i].status & 0xF0) == 0xE0) {
            CHECK((m[i].data1 | (m[i].data2 << 7)) == 8192 + 171);
            saw = true;
        }
    CHECK(saw);
    hostmpe_destroy(h);
}

// DECISIONS_3 #17: bend follows the lattice's own 2D pitch metric — grid
// rows are octaves, Jankó columns are whole tones and its rows the ±1 step.
static void test_lattice_gradient_bend() {
    hostmpe_t* h = hostmpe_create();
    hostmpe_msg_t m[8];
    uint32_t n = 0;
    const float ROW_H = 0.1143f;   // grid row height at 16:9, canvas-height units
    int32_t v = hostmpe_touch_begin(h, 0.0, 60, 96, GRID_RMAX,
                                    1.0f / GRID_STEP, 12.0f / ROW_H, m, 8, &n);
    // One row straight down = one octave = +2048 counts at ±48.
    n = hostmpe_touch_update(h, v, 0.0f, ROW_H, m, 8);
    bool saw = false;
    for (uint32_t i = 0; i < n; i++)
        if ((m[i].status & 0xF0) == 0xE0) {
            CHECK((m[i].data1 | (m[i].data2 << 7)) == 8192 + 2048);
            saw = true;
        }
    CHECK(saw);
    // Diagonal: one column + one row = 13 semitones — the gradient composes.
    n = hostmpe_touch_update(h, v, GRID_STEP, ROW_H, m, 8);
    for (uint32_t i = 0; i < n; i++)
        if ((m[i].status & 0xF0) == 0xE0)
            CHECK((m[i].data1 | (m[i].data2 << 7)) == 8192 + 2219);   // 13*170.67
    hostmpe_destroy(h);

    // Jankó: pitch is a function of x ALONE (rows are echoes of the same
    // notes — the stagger puts each semitone half a column over), so the
    // true gradient is horizontal: gx = 2/col, gy = 0. Bend is horizontal
    // on BOTH playable layouts; a half-column drag is one semitone.
    h = hostmpe_create();
    const float JRMAX = 0.018f, JCOL = 0.0368f;
    v = hostmpe_touch_begin(h, 0.0, 61, 96, JRMAX, 2.0f / JCOL, 0.0f, m, 8, &n);
    n = hostmpe_touch_update(h, v, 0.5f * JCOL, 0.0f, m, 8);   // half column
    for (uint32_t i = 0; i < n; i++)
        if ((m[i].status & 0xF0) == 0xE0)
            CHECK((m[i].data1 | (m[i].data2 << 7)) == 8192 + 171);    // +1 st
    n = hostmpe_touch_update(h, v, JCOL, 0.0f, m, 8);          // full column
    for (uint32_t i = 0; i < n; i++)
        if ((m[i].status & 0xF0) == 0xE0)
            CHECK((m[i].data1 | (m[i].data2 << 7)) == 8192 + 341);    // +2 st
    // Vertical drag: pitch untouched (rows are the same note); CC74 moves.
    n = hostmpe_touch_update(h, v, 0.0f, 0.1333f, m, 8);
    for (uint32_t i = 0; i < n; i++) {
        if ((m[i].status & 0xF0) == 0xE0)
            CHECK((m[i].data1 | (m[i].data2 << 7)) == 8192);
        if ((m[i].status & 0xF0) == 0xB0 && m[i].data1 == 74)
            CHECK(m[i].data2 < 64);   // down = darker
    }
    hostmpe_destroy(h);
}

static void test_session_config() {
    hostmpe_t* h = hostmpe_create();
    hostmpe_msg_t m[128];
    const uint32_t n = hostmpe_session_config(h, m, 128);
    CHECK(n == 5 + 5 * HOSTMPE_MEMBERS);
    // MCM on the master first: RPN 6 = 15 members.
    CHECK(m[0].status == 0xB0 && m[0].data1 == 101 && m[0].data2 == 0);
    CHECK(m[1].status == 0xB0 && m[1].data1 == 100 && m[1].data2 == 6);
    CHECK(m[2].status == 0xB0 && m[2].data1 == 6 && m[2].data2 == HOSTMPE_MEMBERS);
    // RPN 0 = 48 present on every member channel.
    int rpn0_48 = 0;
    for (uint32_t i = 0; i < n; i++) {
        if ((m[i].status & 0xF0) == 0xB0 && (m[i].status & 0x0F) >= 1 &&
            m[i].data1 == 6 && m[i].data2 == HOSTMPE_BEND_RANGE) rpn0_48++;
    }
    CHECK(rpn0_48 == HOSTMPE_MEMBERS);
    hostmpe_destroy(h);
}

int main() {
    test_soft_knee();
    test_joystick_eff();
    test_bend_deflection_and_14bit();
    test_emit_order_and_column_drag();
    test_y_pressure_upward_only();
    test_allocator_lru_round_robin();
    test_external_occupancy_masking();
    test_knee_floor_small_cells();
    test_lattice_gradient_bend();
    test_session_config();
    if (g_failures) {
        std::fprintf(stderr, "%d/%d checks FAILED\n", g_failures, g_checks);
        return 1;
    }
    std::printf("hostmpe_tests: %d checks passed\n", g_checks);
    return 0;
}
