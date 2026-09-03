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

// ---- §5.3 outbound limiter (Step 17) ---------------------------------------

static hostmpe_msg_t mkbend(int ch, int pb) {
    hostmpe_msg_t m;
    m.status = (uint8_t)(0xE0 | ch);
    m.data1 = (uint8_t)(pb & 0x7F);
    m.data2 = (uint8_t)(pb >> 7);
    return m;
}
static hostmpe_msg_t mkpress(int ch, int v) {
    hostmpe_msg_t m;
    m.status = (uint8_t)(0xD0 | ch);
    m.data1 = (uint8_t)v;
    m.data2 = 0;
    return m;
}
static hostmpe_msg_t mknote(int ch, bool on) {
    hostmpe_msg_t m;
    m.status = (uint8_t)((on ? 0x90 : 0x80) | ch);
    m.data1 = 60;
    m.data2 = on ? 96 : 64;
    return m;
}

static void test_limiter_rate_policy() {
    hostmpe_limiter_t* l = hostmpe_limiter_create_rate(100.0f);
    hostmpe_msg_t out[64];
    // First value emits immediately.
    CHECK(hostmpe_limiter_push(l, 0.0, mkbend(1, 8200), false, out, 64) == 1);
    // Change-only: identical value never resent.
    CHECK(hostmpe_limiter_push(l, 0.001, mkbend(1, 8200), false, out, 64) == 0);
    CHECK(hostmpe_limiter_drain(l, 1.0, out, 64) == 0);
    // Latest-wins: a burst inside one period emits only the LAST value.
    CHECK(hostmpe_limiter_push(l, 1.000, mkbend(1, 8300), false, out, 64) == 1);
    CHECK(hostmpe_limiter_push(l, 1.002, mkbend(1, 8310), false, out, 64) == 0);
    CHECK(hostmpe_limiter_push(l, 1.004, mkbend(1, 8320), false, out, 64) == 0);
    uint32_t n = hostmpe_limiter_drain(l, 1.011, out, 64);
    CHECK(n == 1);
    CHECK((out[0].data1 | (out[0].data2 << 7)) == 8320);
    // Rate ceiling: 1 kHz of changes for one second -> <= 101 emissions.
    uint32_t emitted = 0;
    for (int i = 0; i < 1000; i++) {
        const double t = 2.0 + i * 0.001;
        emitted += hostmpe_limiter_push(l, t, mkbend(1, 8000 + i), false, out, 64);
    }
    emitted += hostmpe_limiter_drain(l, 3.02, out, 64);
    CHECK(emitted <= 101 && emitted >= 95);
    // Slots are independent: another channel is not throttled by ch 1.
    CHECK(hostmpe_limiter_push(l, 3.03, mkbend(2, 9000), false, out, 64) == 1);
    // Exempt passthrough mid-hold: a center bend goes out NOW and resets the
    // slot's change-only state.
    hostmpe_limiter_push(l, 4.0, mkbend(1, 5000), false, out, 64);
    hostmpe_limiter_push(l, 4.001, mkbend(1, 5001), false, out, 64);   // pending
    CHECK(hostmpe_limiter_push(l, 4.002, mkbend(1, 8192), true, out, 64) == 1);
    CHECK(hostmpe_limiter_drain(l, 5.0, out, 64) == 0);   // pending cleared
    hostmpe_limiter_destroy(l);
}

static void test_limiter_budget_policy() {
    hostmpe_limiter_t* l = hostmpe_limiter_create_budget(300.0f);
    hostmpe_msg_t out[64];
    // Storm: 10 voices x 3 dims, new values every 5 ms for 2 s (6,000 msg/s
    // offered). Assert the global ceiling AND per-slot fairness windows.
    uint32_t emitted = 0;
    double last_emit_per_slot[16][3];
    double worst_gap = 0.0;
    for (int c = 1; c <= 10; c++)
        for (int d = 0; d < 3; d++) last_emit_per_slot[c][d] = 0.0;
    uint32_t note_msgs = 0;
    for (int tick = 0; tick < 400; tick++) {
        const double t = tick * 0.005;
        for (int c = 1; c <= 10; c++) {
            hostmpe_msg_t msgs[3] = {
                mkbend(c, 6000 + tick),
                mkpress(c, tick % 128),
                { (uint8_t)(0xB0 | c), 74, (uint8_t)((tick + c) % 128) },
            };
            for (int d = 0; d < 3; d++) {
                uint32_t n = hostmpe_limiter_push(l, t, msgs[d], false, out, 64);
                for (uint32_t i = 0; i < n; i++) {
                    const int ch = out[i].status & 0x0F;
                    const int kind = out[i].status & 0xF0;
                    const int dim = kind == 0xE0 ? 0 : (kind == 0xD0 ? 1 : 2);
                    const double gap = t - last_emit_per_slot[ch][dim];
                    if (last_emit_per_slot[ch][dim] > 0.2 && gap > worst_gap)
                        worst_gap = gap;
                    last_emit_per_slot[ch][dim] = t;
                }
                emitted += n;
            }
        }
        // Note On/Off woven through the storm: NEVER dropped, no token cost.
        if (tick % 50 == 25) {
            note_msgs += hostmpe_limiter_push(l, t, mknote(11, true), true, out, 64);
            note_msgs += hostmpe_limiter_push(l, t, mknote(11, false), true, out, 64);
        }
    }
    CHECK(note_msgs == 16);                       // 8 pairs, all passed through
    CHECK(emitted <= 650);                        // ~300/s x 2 s + burst headroom
    CHECK(emitted >= 550);                        // and the budget is actually used
    // Fairness: every active slot updates within ~100 ms (30 slots at 300/s).
    CHECK(worst_gap < 0.115);
    hostmpe_limiter_destroy(l);
}

// Panic / zone silence (Step 17 close-out): a transport that stops receiving
// must never be left with hung notes.
static void test_panic_and_silence() {
    hostmpe_t* h = hostmpe_create();
    hostmpe_msg_t m[128];
    uint32_t n = 0;
    // Three live voices, then panic.
    for (int i = 0; i < 3; i++)
        hostmpe_touch_begin(h, 0.0, (uint8_t)(60 + i), 96, GRID_RMAX,
                            1.0f / GRID_STEP, 0.0f, m, 128, &n);
    CHECK(hostmpe_active_voices(h) == 3);
    n = hostmpe_panic(h, 1.0, m, 128);
    CHECK(hostmpe_active_voices(h) == 0);          // voices freed
    // Every voice released with pressure-0 BEFORE its Note Off.
    int offs = 0;
    for (uint32_t i = 0; i < n; i++) {
        if ((m[i].status & 0xF0) == 0x80) {
            offs++;
            CHECK(i > 0 && (m[i-1].status & 0xF0) == 0xD0 && m[i-1].data1 == 0);
        }
    }
    CHECK(offs == 3);
    // Zone silence present on master + all 15 members.
    int sustain = 0, allnotes = 0;
    for (uint32_t i = 0; i < n; i++) {
        if ((m[i].status & 0xF0) == 0xB0 && m[i].data1 == 64 && m[i].data2 == 0) sustain++;
        if ((m[i].status & 0xF0) == 0xB0 && m[i].data1 == 123 && m[i].data2 == 0) allnotes++;
    }
    CHECK(sustain == HOSTMPE_MEMBERS + 1);
    CHECK(allnotes == HOSTMPE_MEMBERS + 1);
    // Panic on an idle instance is just the zone silence (idempotent).
    n = hostmpe_panic(h, 2.0, m, 128);
    CHECK(n == 2 * (HOSTMPE_MEMBERS + 1));
    // Freed channels are immediately reusable.
    CHECK(hostmpe_touch_begin(h, 3.0, 60, 96, GRID_RMAX, 1.0f / GRID_STEP, 0.0f,
                              m, 128, &n) >= 1);
    hostmpe_destroy(h);

    // Stateless silence leaves the voice table alone (departing transport).
    h = hostmpe_create();
    hostmpe_touch_begin(h, 0.0, 60, 96, GRID_RMAX, 1.0f / GRID_STEP, 0.0f, m, 128, &n);
    const uint32_t zn = hostmpe_silence_zone(m, 128);
    CHECK(zn == 2 * (HOSTMPE_MEMBERS + 1));
    CHECK(hostmpe_active_voices(h) == 1);          // still playing elsewhere
    hostmpe_destroy(h);
}

// -------------------------------------------------------------------------
// §8 performance control strip (Step 18). Every widget emits on the MASTER
// channel only; the DONE gates: "spring always lands exactly at center",
// "latch never jumps on regrasp", CC64 never dropped or delayed by a storm.

static bool strip_all_master(const hostmpe_msg_t* m, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        if ((m[i].status & 0x0F) != 0) return false;
    }
    return true;
}

static void test_strip_spring() {
    hostmpe_strip_t* s = hostmpe_strip_create();
    hostmpe_msg_t m[8];

    // Grab and deflect: bend on the MASTER channel, change-only.
    CHECK(hostmpe_strip_pitch_move(s, 0.5f, m, 8) == 1);
    CHECK(m[0].status == 0xE0);
    CHECK((m[0].data1 | (m[0].data2 << 7)) == 8192 + 4096);   // round(0.5*8191)
    CHECK(hostmpe_strip_pitch_move(s, 0.5f, m, 8) == 0);      // identical: silent
    CHECK_NEAR(hostmpe_strip_pitch_value(s), 0.5f, 1e-6f);

    // Release: dense ticks ramp monotonically to center; the FINAL message is
    // exactly 8192; after that the wheel is silent.
    hostmpe_strip_pitch_release(s, 10.0);
    int last = 8192 + 4096;
    uint32_t emitted = 0;
    for (int i = 1; i <= 12; i++) {                            // 6 ms steps > 50 ms
        const uint32_t n = hostmpe_strip_tick(s, 10.0 + i * 0.006, m, 8);
        CHECK(n <= 1);
        if (n == 1) {
            const int pb = m[0].data1 | (m[0].data2 << 7);
            CHECK(pb < last);                                  // toward center
            CHECK(pb >= 8192);
            last = pb;
            emitted++;
        }
    }
    CHECK(last == 8192);                                       // EXACT center
    CHECK(emitted >= 3);                                       // it ramped, not snapped
    CHECK(hostmpe_strip_tick(s, 11.0, m, 8) == 0);             // done means done
    CHECK_NEAR(hostmpe_strip_pitch_value(s), 0.0f, 1e-7f);

    // Sparse ticks: release then ONE late tick still lands exactly at center.
    hostmpe_strip_pitch_move(s, -1.0f, m, 8);
    hostmpe_strip_pitch_release(s, 20.0);
    CHECK(hostmpe_strip_tick(s, 21.0, m, 8) == 1);
    CHECK((m[0].data1 | (m[0].data2 << 7)) == 8192);

    // Regrab mid-ramp cancels the ramp: the grab owns the wheel. (A regrab at
    // exactly the ramp's current value is change-only silent — also correct.)
    hostmpe_strip_pitch_move(s, 1.0f, m, 8);
    hostmpe_strip_pitch_release(s, 30.0);
    hostmpe_strip_tick(s, 30.010, m, 8);
    CHECK(hostmpe_strip_pitch_move(s, 0.5f, m, 8) == 1);
    CHECK(hostmpe_strip_tick(s, 30.020, m, 8) == 0);           // no ramp anymore

    hostmpe_strip_destroy(s);
}

static void test_strip_latch_and_assign() {
    hostmpe_strip_t* s = hostmpe_strip_create();
    hostmpe_msg_t m[8];

    // Relative accumulation, change-only on the ROUNDED value; sub-unit
    // deltas accumulate (arbitrarily fine control by dragging slowly).
    CHECK(hostmpe_strip_latch_move(s, HOSTMPE_STRIP_MOD, 0.3f, m, 8) == 0);
    CHECK(hostmpe_strip_latch_move(s, HOSTMPE_STRIP_MOD, 0.3f, m, 8) == 1);  // 0.6 -> 1
    CHECK(m[0].status == 0xB0 && m[0].data1 == 1 && m[0].data2 == 1);
    CHECK(hostmpe_strip_latch_move(s, HOSTMPE_STRIP_MOD, 40.0f, m, 8) == 1);
    CHECK(m[0].data2 == 41);
    // "Latch never jumps on regrasp" is structural: there is no absolute-set
    // call — a new grab that hasn't moved contributes delta 0.
    CHECK(hostmpe_strip_latch_move(s, HOSTMPE_STRIP_MOD, 0.0f, m, 8) == 0);
    CHECK_NEAR(hostmpe_strip_latch_value(s, HOSTMPE_STRIP_MOD), 40.6f, 1e-4f);
    // Clamps at the rails.
    hostmpe_strip_latch_move(s, HOSTMPE_STRIP_MOD, 500.0f, m, 8);
    CHECK(m[0].data2 == 127);
    CHECK(hostmpe_strip_latch_move(s, HOSTMPE_STRIP_MOD, 10.0f, m, 8) == 0);
    hostmpe_strip_latch_move(s, HOSTMPE_STRIP_MOD, -500.0f, m, 8);
    CHECK(m[0].data2 == 0);

    // Assignables: defaults, reassignment keeps the value and is silent.
    CHECK(hostmpe_strip_assigned_cc(s, HOSTMPE_STRIP_ASSIGN_A) == 23);
    CHECK(hostmpe_strip_assigned_cc(s, HOSTMPE_STRIP_ASSIGN_B) == 24);
    hostmpe_strip_latch_move(s, HOSTMPE_STRIP_ASSIGN_A, 60.0f, m, 8);
    CHECK(m[0].data1 == 23 && m[0].data2 == 60);
    CHECK(hostmpe_strip_assign(s, HOSTMPE_STRIP_ASSIGN_A, 74));
    CHECK_NEAR(hostmpe_strip_latch_value(s, HOSTMPE_STRIP_ASSIGN_A), 60.0f, 1e-6f);
    CHECK(hostmpe_strip_latch_move(s, HOSTMPE_STRIP_ASSIGN_A, 1.0f, m, 8) == 1);
    CHECK(m[0].data1 == 74 && m[0].data2 == 61);               // speaks on the new CC
    // The protocol CCs are refused (DECISIONS_3 #30): RPN/data-entry/mode.
    CHECK(!hostmpe_strip_assign(s, HOSTMPE_STRIP_ASSIGN_A, 6));
    CHECK(!hostmpe_strip_assign(s, HOSTMPE_STRIP_ASSIGN_A, 38));
    CHECK(!hostmpe_strip_assign(s, HOSTMPE_STRIP_ASSIGN_A, 64));
    CHECK(!hostmpe_strip_assign(s, HOSTMPE_STRIP_ASSIGN_A, 100));
    CHECK(!hostmpe_strip_assign(s, HOSTMPE_STRIP_ASSIGN_A, 123));
    CHECK(!hostmpe_strip_assign(s, HOSTMPE_STRIP_ASSIGN_A, 1));
    CHECK(hostmpe_strip_assigned_cc(s, HOSTMPE_STRIP_ASSIGN_A) == 74);  // unchanged
    // The MOD wheel is not reassignable.
    CHECK(!hostmpe_strip_assign(s, HOSTMPE_STRIP_MOD, 11));

    hostmpe_strip_destroy(s);
}

static void test_strip_sustain() {
    hostmpe_strip_t* s = hostmpe_strip_create();
    hostmpe_msg_t m[4];

    // Momentary (default): press 127, release 0, transitions only.
    CHECK(hostmpe_strip_sustain_press(s, m, 4) == 1);
    CHECK(m[0].status == 0xB0 && m[0].data1 == 64 && m[0].data2 == 127);
    CHECK(hostmpe_strip_sustain_press(s, m, 4) == 0);          // already on
    CHECK(hostmpe_strip_sustain_release(s, m, 4) == 1);
    CHECK(m[0].data2 == 0);
    CHECK(hostmpe_strip_sustain_release(s, m, 4) == 0);

    // Toggle: press flips, release is silent.
    CHECK(hostmpe_strip_sustain_mode(s, true, m, 4) == 0);     // off: nothing to fix
    CHECK(hostmpe_strip_sustain_press(s, m, 4) == 1);
    CHECK(m[0].data2 == 127);
    CHECK(hostmpe_strip_sustain_release(s, m, 4) == 0);
    CHECK(hostmpe_strip_sustain_on(s));
    CHECK(hostmpe_strip_sustain_press(s, m, 4) == 1);
    CHECK(m[0].data2 == 0);

    // A mode switch while ON emits the OFF — never a stranded pedal.
    hostmpe_strip_sustain_press(s, m, 4);                      // toggle on
    CHECK(hostmpe_strip_sustain_on(s));
    CHECK(hostmpe_strip_sustain_mode(s, false, m, 4) == 1);
    CHECK(m[0].data1 == 64 && m[0].data2 == 0);
    CHECK(!hostmpe_strip_sustain_on(s));

    hostmpe_strip_destroy(s);
}

static void test_strip_announce_and_channel_discipline() {
    hostmpe_strip_t* s = hostmpe_strip_create();
    hostmpe_msg_t m[16];

    // Work every widget, collecting everything emitted: all master-channel.
    uint32_t n = 0;
    n += hostmpe_strip_pitch_move(s, 0.7f, m + n, 16 - n);
    n += hostmpe_strip_latch_move(s, HOSTMPE_STRIP_MOD, 80.0f, m + n, 16 - n);
    n += hostmpe_strip_latch_move(s, HOSTMPE_STRIP_ASSIGN_B, 33.0f, m + n, 16 - n);
    n += hostmpe_strip_sustain_press(s, m + n, 16 - n);
    hostmpe_strip_pitch_release(s, 0.0);
    n += hostmpe_strip_tick(s, 1.0, m + n, 16 - n);
    CHECK(n >= 5);
    CHECK(strip_all_master(m, n));

    // Announce restates the full latched state in 5 master-channel messages —
    // the values a DAW must agree with after an MCM re-sync.
    const uint32_t an = hostmpe_strip_announce(s, m, 16);
    CHECK(an == 5);
    CHECK(strip_all_master(m, an));
    CHECK(m[0].status == 0xE0);                                // spring (at center)
    CHECK((m[0].data1 | (m[0].data2 << 7)) == 8192);
    CHECK(m[1].data1 == 1 && m[1].data2 == 80);                // mod
    CHECK(m[2].data1 == 23 && m[2].data2 == 0);                // A untouched
    CHECK(m[3].data1 == 24 && m[3].data2 == 33);               // B
    CHECK(m[4].data1 == 64 && m[4].data2 == 127);              // sustain held

    hostmpe_strip_destroy(s);
}

// The §8 never-dropped gate, headless form: a BLE-class budget limiter under
// a 10-voice bend storm must pass every CC64 transition through IMMEDIATELY
// (exempt: zero queueing) while the master-channel wheels are policed like
// any continuous dimension (DECISIONS_3 #30 slots).
static void test_limiter_strip_classes() {
    hostmpe_limiter_t* l = hostmpe_limiter_create_budget(300.0f);
    hostmpe_msg_t out[64];
    uint32_t cc64_in = 0, cc64_out = 0, cc1_out = 0;
    uint8_t sustain = 0;

    // Emissions surface from WHICHEVER push happens to drain when tokens are
    // available, so count CC1 by inspecting every batch, not by attribution.
    auto count_cc1 = [&](uint32_t n) {
        for (uint32_t k = 0; k < n && k < 64; k++) {
            if (out[k].status == 0xB0 && out[k].data1 == 1) cc1_out++;
        }
    };
    for (int i = 0; i < 10000; i++) {                          // 10 s at 1 kHz
        const double now = i * 0.001;
        // 10 member-channel bend streams (the storm).
        hostmpe_msg_t b;
        b.status = (uint8_t)(0xE0 | (1 + i % 10));
        b.data1 = (uint8_t)(i % 128);
        b.data2 = (uint8_t)((i / 128) % 128);
        count_cc1(hostmpe_limiter_push(l, now, b, false, out, 64));
        // A master-channel mod-wheel sweep (strip wheel: policed).
        hostmpe_msg_t w;
        w.status = 0xB0; w.data1 = 1; w.data2 = (uint8_t)(i % 128);
        count_cc1(hostmpe_limiter_push(l, now, w, false, out, 64));
        // A sustain transition every 500 ms (strip button: exempt).
        if (i % 500 == 250) {
            sustain = sustain ? 0 : 127;
            hostmpe_msg_t su;
            su.status = 0xB0; su.data1 = 64; su.data2 = sustain;
            cc64_in++;
            const uint32_t n = hostmpe_limiter_push(l, now, su, true, out, 64);
            CHECK(n >= 1);                                     // NOW, not queued
            bool found = false;
            for (uint32_t k = 0; k < n; k++) {
                if (out[k].status == 0xB0 && out[k].data1 == 64 &&
                    out[k].data2 == sustain) found = true;
            }
            CHECK(found);
            cc64_out++;
        }
    }
    CHECK(cc64_out == cc64_in);                                // zero dropped
    // The wheel was actually policed: far fewer emissions than pushes, but
    // the stream flowed (the budget shares ~300/s across all slots).
    CHECK(cc1_out > 0);
    CHECK(cc1_out < 4000);
    hostmpe_limiter_destroy(l);

    // Rate-policy transport: the wheel decimates to <= ~100 Hz, change-only
    // holds, and drain delivers the latest value (latest-wins).
    l = hostmpe_limiter_create_rate(100.0f);
    uint32_t sent = 0;
    for (int i = 0; i < 1000; i++) {                           // 1 s at 1 kHz
        hostmpe_msg_t w;
        w.status = 0xB0; w.data1 = 1; w.data2 = (uint8_t)(i % 128);
        sent += hostmpe_limiter_push(l, i * 0.001, w, false, out, 64);
    }
    CHECK(sent <= 101);
    const uint32_t dn = hostmpe_limiter_drain(l, 1.5, out, 64);
    CHECK(dn == 1);                                            // the held latest
    CHECK(out[0].data2 == 999 % 128);
    // Change-only: an identical master-CC value is never resent.
    hostmpe_msg_t same;
    same.status = 0xB0; same.data1 = 1; same.data2 = 999 % 128;
    CHECK(hostmpe_limiter_push(l, 2.0, same, false, out, 64) == 0);
    hostmpe_limiter_destroy(l);
}

// -------------------------------------------------------------------------
// v0.4 bipolar Y (PHASE4 §3.3, step-20 DONE): center = both zeros; up emits
// ONLY 0xD0; down emits ONLY 0xA0 (the voice's note on its member channel);
// the single radial knee is continuous through center; lift releases an
// engaged swirl half before Note Off; the limiter treats 0xA0 as a
// continuous dimension.
static void test_bipolar_y() {
    hostmpe_t* h = hostmpe_create();
    hostmpe_msg_t m[8];
    uint32_t n = 0;
    const int32_t v = hostmpe_touch_begin(h, 0.0, 60, 96, GRID_RMAX,
                                          1.0f / GRID_STEP, 0.0f, m, 8, &n);
    CHECK(v >= 1);

    // Center: both zeros — a tiny wobble inside the deadband emits nothing.
    CHECK(hostmpe_touch_update(h, v, 0.001f, 0.001f, m, 8) == 0);
    CHECK(hostmpe_touch_update(h, v, 0.0f, -0.002f, m, 8) == 0);

    // Up: ONLY 0xD0. Full-radius straight up = 127.
    n = hostmpe_touch_update(h, v, 0.0f, -GRID_RMAX, m, 8);
    bool saw_d0 = false, saw_a0 = false;
    for (uint32_t i = 0; i < n; i++) {
        if ((m[i].status & 0xF0) == 0xD0) { saw_d0 = true; CHECK(m[i].data1 == 127); }
        if ((m[i].status & 0xF0) == 0xA0) saw_a0 = true;
    }
    CHECK(saw_d0);
    CHECK(!saw_a0);

    // Crossing to DOWN: the departing half releases through zero (0xD0 0)
    // and ONLY 0xA0 carries value — on the voice's channel, with its note.
    n = hostmpe_touch_update(h, v, 0.0f, GRID_RMAX, m, 8);
    saw_d0 = saw_a0 = false;
    for (uint32_t i = 0; i < n; i++) {
        if ((m[i].status & 0xF0) == 0xD0) { saw_d0 = true; CHECK(m[i].data1 == 0); }
        if ((m[i].status & 0xF0) == 0xA0) {
            saw_a0 = true;
            CHECK((m[i].status & 0x0F) == v);
            CHECK(m[i].data1 == 60);          // keyed by the voice's note
            CHECK(m[i].data2 == 127);         // full-radius straight down
        }
    }
    CHECK(saw_d0);
    CHECK(saw_a0);

    // Knee continuity through center: just past the deadband floor on the
    // down half, the swirl value starts from ~0 — no jump.
    hostmpe_touch_update(h, v, 0.0f, 0.0f, m, 8);   // re-center (releases 0xA0)
    const float just_past = HOSTMPE_KNEE_FLOOR_CH + 0.0015f;
    n = hostmpe_touch_update(h, v, 0.0f, just_past, m, 8);
    for (uint32_t i = 0; i < n; i++) {
        if ((m[i].status & 0xF0) == 0xA0) CHECK(m[i].data2 <= 4);
    }

    // Lift with the swirl engaged: pressure 0, THEN 0xA0 0, THEN Note Off.
    hostmpe_touch_update(h, v, 0.0f, GRID_RMAX, m, 8);   // engage down half
    n = hostmpe_touch_end(h, v, 1.0, 64, m, 8);
    CHECK(n == 3);
    CHECK((m[0].status & 0xF0) == 0xD0 && m[0].data1 == 0);
    CHECK((m[1].status & 0xF0) == 0xA0 && m[1].data1 == 60 && m[1].data2 == 0);
    CHECK((m[2].status & 0xF0) == 0x80);
    hostmpe_destroy(h);

    // Limiter: 0xA0 is a continuous dimension — change-only + decimation.
    hostmpe_limiter_t* l = hostmpe_limiter_create_rate(100.0f);
    hostmpe_msg_t out[8];
    uint32_t sent = 0;
    for (int i = 0; i < 1000; i++) {                    // 1 s at 1 kHz
        hostmpe_msg_t p;
        p.status = 0xA1; p.data1 = 60; p.data2 = (uint8_t)(i % 128);
        sent += hostmpe_limiter_push(l, i * 0.001, p, false, out, 8);
    }
    CHECK(sent <= 101);
    hostmpe_msg_t same;
    same.status = 0xA1; same.data1 = 60; same.data2 = 999 % 128;
    hostmpe_limiter_drain(l, 1.5, out, 8);              // deliver the held latest
    CHECK(hostmpe_limiter_push(l, 2.0, same, false, out, 8) == 0);   // change-only
    hostmpe_limiter_destroy(l);
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
    test_limiter_rate_policy();
    test_limiter_budget_policy();
    test_panic_and_silence();
    test_strip_spring();
    test_strip_latch_and_assign();
    test_strip_sustain();
    test_strip_announce_and_channel_discipline();
    test_limiter_strip_classes();
    test_bipolar_y();
    if (g_failures) {
        std::fprintf(stderr, "%d/%d checks FAILED\n", g_failures, g_checks);
        return 1;
    }
    std::printf("hostmpe_tests: %d checks passed\n", g_checks);
    return 0;
}
