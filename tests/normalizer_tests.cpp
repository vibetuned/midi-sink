// normalizer_tests.cpp — headless decoder/mapper unit tests (roadmap step 4).
// Links the normalizer, voice mapper, and displacement queue directly — no
// GPU, no sokol, CI-runnable on a bare macOS runner.
#include "midi_normalizer.h"
#include "palettes.h"
#include "voice_mapper.h"
#include "hostmpe.h"
#include "displacement.h"
#include "layouts.h"

#include <cmath>
#include <array>
#include <initializer_list>

static double g_now = 0.0;
static double tnow() { g_now += 0.05; return g_now; }
#include <cstdio>
#include <cstring>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        g_checks++;                                                        \
        if (!(cond)) {                                                     \
            g_failures++;                                                  \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                  \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                              \
    do {                                                                   \
        g_checks++;                                                        \
        const float _a = (a), _b = (b);                                    \
        if (std::fabs(_a - _b) > (eps)) {                                  \
            g_failures++;                                                  \
            std::fprintf(stderr, "FAIL %s:%d: %s=%f != %s=%f\n",           \
                         __FILE__, __LINE__, #a, (double)_a, #b, (double)_b); \
        }                                                                  \
    } while (0)

static sumi_params_t default_params() {
    sumi_params_t p = {};
    p.pitch_layout = 0;
    p.expansion_rate = 1.0f;
    p.smoothing_ms = 30.0f;
    p.bpm = 120.0f;          // engine defaults (engine.cpp), mirrored
    p.roll_speed = 0.0625f;  // §3.4: 16 beats of history span the canvas
    return p;
}

static sumi_mpe_zone_t default_zone() {
    sumi_mpe_zone_t z = {0, 1, 15};
    return z;
}

// -------------------------------------------------------------------------
static void test_ring_basic_and_overflow() {
    sumi_normalizer_t* n = sumi_normalizer_create(nullptr, nullptr);
    sumi_midi_event_t ev[8];

    // Basic: one note-on through the ring.
    sumi_normalizer_push(n, 0x90, 60, 100);
    CHECK(sumi_normalizer_drain(n, tnow(), ev, 8) == 1);
    CHECK(ev[0].kind == SUMI_MEV_NOTE_ON);
    CHECK(ev[0].channel == 0);
    CHECK(ev[0].a == 60);
    CHECK(ev[0].b == 100);
    CHECK(sumi_normalizer_dropped(n) == 0);

    // Overflow: push 5000 into a 4096 ring -> 904 dropped (oldest), and the
    // survivors are the NEWEST 4096.
    for (int i = 0; i < 5000; i++) {
        sumi_normalizer_push(n, 0xB0, 7, (uint8_t)(i & 0x7F));
    }
    CHECK(sumi_normalizer_dropped(n) == 5000 - 4096);
    uint32_t total = 0;
    static sumi_midi_event_t big[4096];
    uint32_t got;
    uint8_t first_val = 0xFF;
    while ((got = sumi_normalizer_drain(n, tnow(), big, 4096)) > 0) {
        if (first_val == 0xFF) first_val = big[0].b;
        total += got;
    }
    CHECK(total == 4096);
    CHECK(first_val == ((5000 - 4096) & 0x7F));   // oldest were dropped
    sumi_normalizer_destroy(n);
}

// -------------------------------------------------------------------------
static void test_note_on_off_and_vel0() {
    sumi_normalizer_t* n = sumi_normalizer_create(nullptr, nullptr);
    sumi_midi_event_t ev[8];
    sumi_normalizer_push(n, 0x91, 64, 90);    // ch 2 note on
    sumi_normalizer_push(n, 0x91, 64, 0);     // vel 0 == note off
    sumi_normalizer_push(n, 0x81, 64, 40);    // explicit off w/ release vel
    CHECK(sumi_normalizer_drain(n, tnow(), ev, 8) == 3);
    CHECK(ev[0].kind == SUMI_MEV_NOTE_ON && ev[0].channel == 1);
    CHECK(ev[1].kind == SUMI_MEV_NOTE_OFF && ev[1].b == 0);
    CHECK(ev[2].kind == SUMI_MEV_NOTE_OFF && ev[2].b == 40);
    sumi_normalizer_destroy(n);
}

// -------------------------------------------------------------------------
static void test_bend_assembly_and_rpn_range() {
    sumi_normalizer_t* n = sumi_normalizer_create(nullptr, nullptr);
    sumi_midi_event_t ev[8];

    // Center (8192) -> 0 semitones at the ±2 default.
    sumi_normalizer_push(n, 0xE0, 0x00, 0x40);
    CHECK(sumi_normalizer_drain(n, tnow(), ev, 8) == 1);
    CHECK(ev[0].kind == SUMI_MEV_BEND);
    CHECK_NEAR(ev[0].f, 0.0f, 1e-6f);

    // Max up (16383) -> +2 (well, 8191/8192 * 2).
    sumi_normalizer_push(n, 0xE0, 0x7F, 0x7F);
    CHECK(sumi_normalizer_drain(n, tnow(), ev, 8) == 1);
    CHECK_NEAR(ev[0].f, 2.0f * 8191.0f / 8192.0f, 1e-4f);

    // RPN 0 -> bend range 48, then max-up bend reads ±48-scaled.
    sumi_normalizer_push(n, 0xB0, 101, 0);    // RPN MSB
    sumi_normalizer_push(n, 0xB0, 100, 0);    // RPN LSB -> RPN 0
    sumi_normalizer_push(n, 0xB0, 6, 48);     // data entry: 48 semitones
    sumi_normalizer_push(n, 0xE0, 0x00, 0x00);   // min (-8192) -> -48
    uint32_t cnt = sumi_normalizer_drain(n, tnow(), ev, 8);
    CHECK(cnt == 4);   // 3 CC events + bend
    CHECK(ev[3].kind == SUMI_MEV_BEND);
    CHECK_NEAR(ev[3].f, -48.0f, 1e-4f);

    // NRPN data entry must NOT change bend range.
    sumi_normalizer_push(n, 0xB0, 99, 1);     // NRPN select
    sumi_normalizer_push(n, 0xB0, 6, 2);      // data entry -> NRPN, ignored
    sumi_normalizer_push(n, 0xE0, 0x00, 0x00);
    cnt = sumi_normalizer_drain(n, tnow(), ev, 8);
    CHECK(ev[cnt - 1].kind == SUMI_MEV_BEND);
    CHECK_NEAR(ev[cnt - 1].f, -48.0f, 1e-4f);   // still 48, not 2
    sumi_normalizer_destroy(n);
}

// -------------------------------------------------------------------------
static void test_running_status_tolerance() {
    sumi_normalizer_t* n = sumi_normalizer_create(nullptr, nullptr);
    sumi_midi_event_t ev[8];
    sumi_normalizer_push(n, 0x90, 60, 100);   // status establishes running state
    sumi_normalizer_push(n, 62, 90, 0);       // data byte in status slot -> note on 62
    sumi_normalizer_push(n, 64, 0, 0);        // running note-on vel 0 -> note off 64
    CHECK(sumi_normalizer_drain(n, tnow(), ev, 8) == 3);
    CHECK(ev[1].kind == SUMI_MEV_NOTE_ON && ev[1].a == 62 && ev[1].b == 90);
    CHECK(ev[2].kind == SUMI_MEV_NOTE_OFF && ev[2].a == 64);
    sumi_normalizer_destroy(n);
}

// -------------------------------------------------------------------------
static void test_sysex_and_system_ignored() {
    sumi_normalizer_t* n = sumi_normalizer_create(nullptr, nullptr);
    sumi_midi_event_t ev[8];
    sumi_normalizer_push(n, 0xF0, 0x7E, 0x7F);   // sysex start: ignored (§3.1)
    sumi_normalizer_push(n, 0xF8, 0, 0);         // clock: ignored
    sumi_normalizer_push(n, 0x90, 60, 10);
    CHECK(sumi_normalizer_drain(n, tnow(), ev, 8) == 1);
    CHECK(ev[0].kind == SUMI_MEV_NOTE_ON);
    sumi_normalizer_destroy(n);
}

// -------------------------------------------------------------------------
static void test_mode_detection() {
    // Classic: notes on one channel, no expression.
    sumi_normalizer_t* n = sumi_normalizer_create(nullptr, nullptr);
    sumi_midi_event_t ev[64];
    sumi_normalizer_push(n, 0x90, 60, 100);
    sumi_normalizer_drain(n, tnow(), ev, 64);
    CHECK(sumi_normalizer_mode(n) == SUMI_INPUT_CLASSIC);

    // MPE-ish: note-ons on member channels 2..4 with per-channel pressure.
    for (uint8_t ch = 1; ch <= 3; ch++) {
        sumi_normalizer_push(n, (uint8_t)(0x90 | ch), 60, 100);
        sumi_normalizer_push(n, (uint8_t)(0xD0 | ch), 64, 0);
    }
    sumi_normalizer_drain(n, tnow(), ev, 64);
    CHECK(sumi_normalizer_mode(n) == SUMI_INPUT_MPE);
    sumi_normalizer_destroy(n);

    // Wind: single note channel + dense CC2.
    n = sumi_normalizer_create(nullptr, nullptr);
    sumi_normalizer_push(n, 0x90, 60, 100);
    for (int i = 0; i < 20; i++) sumi_normalizer_push(n, 0xB0, 2, (uint8_t)(40 + i));
    sumi_normalizer_drain(n, tnow(), ev, 64);
    CHECK(sumi_normalizer_mode(n) == SUMI_INPUT_WIND);

    // Override wins over heuristic.
    sumi_normalizer_set_mode(n, SUMI_INPUT_CLASSIC);
    CHECK(sumi_normalizer_mode(n) == SUMI_INPUT_CLASSIC);
    sumi_normalizer_destroy(n);

    // MCM (RPN 6) forces MPE.
    n = sumi_normalizer_create(nullptr, nullptr);
    sumi_normalizer_push(n, 0xB0, 101, 0);
    sumi_normalizer_push(n, 0xB0, 100, 6);
    sumi_normalizer_push(n, 0xB0, 6, 15);
    sumi_normalizer_drain(n, tnow(), ev, 64);
    CHECK(sumi_normalizer_mode(n) == SUMI_INPUT_MPE);
    sumi_normalizer_destroy(n);
}

// -------------------------------------------------------------------------
// Independent golden reference for the three layouts, coded straight from
// spec §3.4 (deliberately separate from layouts.cpp). Returns echo count.
static uint32_t golden_position(uint32_t layout, uint8_t note, float aspect,
                                float* gx, float* gy) {
    if (layout == 1) {   // chroma grid: C1 top-left .. B7 bottom-right
        int pc = note % 12;
        int row = (int)(note / 12) - 2;
        if (row < 0) row = 0;
        if (row > 6) row = 6;
        gx[0] = 0.08f + ((pc + 0.5f) / 12.0f) * 0.84f;
        gy[0] = 0.10f + ((row + 0.5f) / 7.0f) * 0.80f;
        return 1;
    } else if (layout == 2) {   // Janko: all three rows of the note's parity
        int parity = note % 2;
        int col = note / 2;
        if (col < 12) col = 12;
        if (col > 53) col = 53;
        float cx = (float)(col - 12) + 0.5f + (parity == 1 ? 0.5f : 0.0f);
        float x = 0.06f + (cx / 42.5f) * 0.88f;
        for (int e = 0; e < 3; e++) {
            int row = parity + 2 * e;   // {0,2,4} or {1,3,5}, top to bottom
            gx[e] = x;
            gy[e] = 0.10f + ((row + 0.5f) / 6.0f) * 0.80f;
        }
        return 3;
    }
    if (layout == 5) {   // piano grid: two-row classical octaves, C1..B7
        static const int   white_idx[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
        static const float black_pos[12] = {0, 1.f, 0, 2.f, 0, 0, 4.f, 0, 5.f, 0, 6.f, 0};
        int pc = note % 12;
        int oct = (int)(note / 12) - 2;
        if (oct < 0) oct = 0;
        if (oct > 6) oct = 6;
        float xu = (white_idx[pc] >= 0) ? (white_idx[pc] + 0.5f) : black_pos[pc];
        int row = oct * 2 + (white_idx[pc] >= 0 ? 1 : 0);   // accidentals on top
        // #60/#61: a natural is centred on the band it owns — the octave
        // pair's BOTTOM 0.6, below the glissando corridor; an accidental on
        // its own row.
        float row_pos = (white_idx[pc] >= 0) ? ((row + 1) - 0.6f) : (row + 0.5f);
        gx[0] = 0.08f + (xu / 7.0f) * 0.84f;
        gy[0] = 0.10f + (row_pos / 14.0f) * 0.80f;
        return 1;
    }
    if (layout == 3) {   // roll H: now-line x = 0.12, pitch -> y (low bottom)
        gx[0] = 0.12f;
        gy[0] = 1.0f - (0.06f + ((note + 0.5f) / 128.0f) * 0.88f);
        return 1;
    }
    if (layout == 4) {   // roll V: pitch -> x (low left), now-line y = 0.12
        gx[0] = 0.06f + ((note + 0.5f) / 128.0f) * 0.88f;
        gy[0] = 0.12f;
        return 1;
    }
    if (layout == 6) {   // roll H from the RIGHT (#64): now-line x = 0.88
        gx[0] = 0.88f;
        gy[0] = 1.0f - (0.06f + ((note + 0.5f) / 128.0f) * 0.88f);
        return 1;
    }
    if (layout == 7) {   // roll V from the BOTTOM (#64): now-line y = 0.88
        gx[0] = 0.06f + ((note + 0.5f) / 128.0f) * 0.88f;
        gy[0] = 0.88f;
        return 1;
    }
    // fifths (v1 mapping, unchanged)
    int pc = note % 12;
    int octave = note / 12;
    int fifths = (pc * 7) % 12;
    float angle = ((float)fifths / 12.0f) * 6.28318530718f - 1.57079632679f;
    float r = 0.42f + (0.10f - 0.42f) * ((float)octave / 10.0f);
    gx[0] = 0.5f + (r * std::cos(angle)) / aspect;
    gy[0] = 0.5f + r * std::sin(angle);
    return 1;
}

static void test_layout_golden_positions() {
    sumi_params_t params = default_params();
    const float aspects[2] = {1.0f, 16.0f / 9.0f};
    for (uint32_t layout = 0; layout <= 7; layout++) {
        for (int a = 0; a < 2; a++) {
            for (int note = 0; note <= 127; note++) {
                float x[SUMI_MAX_ECHOES], y[SUMI_MAX_ECHOES];
                float gx[SUMI_MAX_ECHOES], gy[SUMI_MAX_ECHOES];
                const uint32_t n = sumi_layout_position(layout, (uint8_t)note, &params,
                                                        aspects[a], x, y);
                const uint32_t gn = golden_position(layout, (uint8_t)note, aspects[a], gx, gy);
                CHECK(n == gn);
                for (uint32_t e = 0; e < gn; e++) {
                    if (std::fabs(x[e] - gx[e]) > 1e-4f || std::fabs(y[e] - gy[e]) > 1e-4f) {
                        g_failures++;
                        std::fprintf(stderr, "FAIL golden: layout %u note %d echo %u "
                                     "aspect %.2f: (%f,%f) != (%f,%f)\n", layout, note, e,
                                     (double)aspects[a], (double)x[e], (double)y[e],
                                     (double)gx[e], (double)gy[e]);
                    }
                    g_checks++;
                    CHECK(x[e] >= 0.0f && x[e] <= 1.0f && y[e] >= 0.0f && y[e] <= 1.0f);
                }
                // Purity: same input -> same output.
                float x2[SUMI_MAX_ECHOES], y2[SUMI_MAX_ECHOES];
                CHECK(sumi_layout_position(layout, (uint8_t)note, &params, aspects[a], x2, y2) == n);
                for (uint32_t e = 0; e < n; e++) CHECK(x[e] == x2[e] && y[e] == y2[e]);
            }
        }
    }

    // #64: the four rolls drift AWAY from their now-line at the same speed.
    {
        sumi_params_t rp = default_params();
        float dx = 0, dy = 0;
        CHECK(sumi_layout_field_motion(3, &rp, 0.5, &dx, &dy) && dx > 0.0f && dy == 0.0f);
        const float sp = dx;
        CHECK(sumi_layout_field_motion(6, &rp, 0.5, &dx, &dy) && dx == -sp && dy == 0.0f);
        CHECK(sumi_layout_field_motion(4, &rp, 0.5, &dx, &dy) && dx == 0.0f && dy == sp);
        CHECK(sumi_layout_field_motion(7, &rp, 0.5, &dx, &dy) && dx == 0.0f && dy == -sp);
        CHECK(!sumi_layout_field_motion(5, &rp, 0.5, &dx, &dy) && dx == 0.0f && dy == 0.0f);
    }

    // Spot checks (spec landmarks).
    sumi_params_t p = default_params();
    float x[SUMI_MAX_ECHOES], y[SUMI_MAX_ECHOES];
    // Fifths: C4 (60) at 12 o'clock, centered x (aspect 1), single echo.
    CHECK(sumi_layout_position(0, 60, &p, 1.0f, x, y) == 1);
    CHECK_NEAR(x[0], 0.5f, 1e-4f);
    CHECK(y[0] < 0.5f);
    // Chroma grid: C1 = top-left cell; B7 = bottom-right cell; single echo.
    CHECK(sumi_layout_position(1, 24, &p, 1.0f, x, y) == 1);
    CHECK_NEAR(x[0], 0.08f + (0.5f / 12.0f) * 0.84f, 1e-4f);
    CHECK_NEAR(y[0], 0.10f + (0.5f / 7.0f) * 0.80f, 1e-4f);
    sumi_layout_position(1, 107, &p, 1.0f, x, y);
    CHECK_NEAR(x[0], 0.08f + (11.5f / 12.0f) * 0.84f, 1e-4f);
    CHECK_NEAR(y[0], 0.10f + (6.5f / 7.0f) * 0.80f, 1e-4f);
    // Chroma grid: out-of-range notes clamp to the edge ROW, keep the column.
    float xl[SUMI_MAX_ECHOES], yl[SUMI_MAX_ECHOES];
    sumi_layout_position(1, 12 + 5, &p, 1.0f, xl, yl);   // F0 -> row 0, col F
    sumi_layout_position(1, 24 + 5, &p, 1.0f, x, y);     // F1
    CHECK_NEAR(xl[0], x[0], 1e-5f);
    CHECK_NEAR(yl[0], y[0], 1e-5f);
    // Janko echo set: 3 echoes, shared x, parity rows top to bottom, and the
    // half-column stagger between adjacent semitones.
    float xe[SUMI_MAX_ECHOES], ye[SUMI_MAX_ECHOES];
    float xo[SUMI_MAX_ECHOES], yo[SUMI_MAX_ECHOES];
    CHECK(sumi_layout_position(2, 60, &p, 1.0f, xe, ye) == 3);   // even -> rows 0,2,4
    CHECK(sumi_layout_position(2, 61, &p, 1.0f, xo, yo) == 3);   // odd  -> rows 1,3,5
    CHECK(xe[0] == xe[1] && xe[1] == xe[2]);        // echoes share the column
    CHECK(ye[0] < ye[1] && ye[1] < ye[2]);          // top to bottom
    for (int e = 0; e < 3; e++) {
        CHECK(yo[e] > ye[e]);                       // odd rows sit one below
        CHECK(xo[e] > xe[e] && xo[e] - xe[e] < 0.02f);   // half-column stagger
    }
    // Whole tone = one full column along the SAME rows.
    float xe2[SUMI_MAX_ECHOES], ye2[SUMI_MAX_ECHOES];
    sumi_layout_position(2, 62, &p, 1.0f, xe2, ye2);
    CHECK_NEAR(ye2[0], ye[0], 1e-5f);
    CHECK(xe2[0] > xe[0]);
    // Rolls: every note spawns ON the now-line; pitch spans the cross axis.
    float xr[SUMI_MAX_ECHOES], yr[SUMI_MAX_ECHOES];
    CHECK(sumi_layout_position(3, 0, &p, 1.0f, xr, yr) == 1);
    CHECK_NEAR(xr[0], 0.12f, 1e-6f);
    CHECK(yr[0] > 0.9f);                       // lowest note at the bottom
    sumi_layout_position(3, 127, &p, 1.0f, xr, yr);
    CHECK_NEAR(xr[0], 0.12f, 1e-6f);
    CHECK(yr[0] < 0.1f);
    sumi_layout_position(4, 0, &p, 1.0f, xr, yr);
    CHECK_NEAR(yr[0], 0.12f, 1e-6f);
    CHECK(xr[0] < 0.1f);                       // lowest note at the left
    // Piano grid: C4 is a natural of octave 4, so it is centred on that
    // octave's PAIR — the boundary between the black row 6 and the white row
    // 7 — because the pair is the region that plays it (#60). C#4 sits in the
    // black row above at the C-D boundary, centred on its own row.
    sumi_layout_position(5, 60, &p, 1.0f, x, y);
    CHECK_NEAR(x[0], 0.08f + (0.5f / 7.0f) * 0.84f, 1e-4f);
    CHECK_NEAR(y[0], 0.10f + (7.4f / 14.0f) * 0.80f, 1e-4f);
    sumi_layout_position(5, 61, &p, 1.0f, x, y);
    CHECK_NEAR(x[0], 0.08f + (1.0f / 7.0f) * 0.84f, 1e-4f);
    CHECK_NEAR(y[0], 0.10f + (6.5f / 14.0f) * 0.80f, 1e-4f);
    // Out-of-range notes clamp to the edge octave PAIR, keeping pitch class.
    sumi_layout_position(5, 12 + 5, &p, 1.0f, xl, yl);   // F0 -> octave-1 cell
    sumi_layout_position(5, 24 + 5, &p, 1.0f, x, y);     // F1
    CHECK_NEAR(xl[0], x[0], 1e-5f);
    CHECK_NEAR(yl[0], y[0], 1e-5f);
}

// -------------------------------------------------------------------------
// Phase 4 §2: the instance-free layout probe (ABI v0.3). Units contract:
// centers normalized, radius/step in canvas-height units, direction an
// aspect-corrected unit vector (see sumi_core.h).
// The test's own copies of the piano-grid proportions, so the expectations
// are stated independently of the core's internal constants: an accidental is
// 0.6 of a white key WIDE, and both cells are 0.6 of an octave pair TALL
// (#61 — the natural owns the band below the glissando corridor).
static const float PIANO_BLACK_KEY_W_GOLDEN = 0.6f;
static const float PIANO_NATURAL_H_GOLDEN = 0.6f;

static void test_layout_probe_golden() {
    sumi_params_t params = default_params();
    sumi_cell_info_t c;
    const float aspects[2] = {1.0f, 16.0f / 9.0f};

    for (int a = 0; a < 2; a++) {
        const float aspect = aspects[a];

        // CHROMA_GRID: probing every cell center round-trips the note and
        // returns that exact center; the semitone axis is the row direction
        // (+x) for EVERY note — B notes take the note-1 neighbor (shortest-
        // neighbor rule), which is the same column step.
        const float grid_step = ((1.0f - 2.0f * 0.08f) / 12.0f) * aspect;
        for (int note = 24; note <= 107; note++) {
            float px[SUMI_MAX_ECHOES], py[SUMI_MAX_ECHOES];
            sumi_layout_position(SUMI_LAYOUT_CHROMA_GRID, (uint8_t)note, &params,
                                 aspect, px, py);
            CHECK(sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, aspect, nullptr,
                                    px[0], py[0], &c));
            CHECK(c.note == (uint8_t)note);
            CHECK_NEAR(c.cell_center_x, px[0], 1e-5f);
            CHECK_NEAR(c.cell_center_y, py[0], 1e-5f);
            CHECK(c.cell_radius > 0.0f);
            CHECK_NEAR(c.semitone_step, grid_step, 1e-4f);
            CHECK_NEAR(c.semitone_dx, 1.0f, 1e-4f);
            CHECK_NEAR(c.semitone_dy, 0.0f, 1e-4f);
        }
        // Radius = half the physically smaller cell dimension.
        {
            const float cw = ((1.0f - 2.0f * 0.08f) / 12.0f) * aspect;
            const float ch = (1.0f - 2.0f * 0.10f) / 7.0f;
            float px[SUMI_MAX_ECHOES], py[SUMI_MAX_ECHOES];
            sumi_layout_position(SUMI_LAYOUT_CHROMA_GRID, 60, &params, aspect, px, py);
            sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, aspect, nullptr, px[0], py[0], &c);
            CHECK_NEAR(c.cell_radius, 0.5f * (cw < ch ? cw : ch), 1e-5f);
        }

        // JANKO: ALL THREE echo rows of every note probe back to that note,
        // each returning the touched row's own center.
        params.pitch_layout = SUMI_LAYOUT_JANKO;
        for (int note = 24; note <= 107; note++) {
            float ex[SUMI_MAX_ECHOES], ey[SUMI_MAX_ECHOES];
            CHECK(sumi_layout_position(SUMI_LAYOUT_JANKO, (uint8_t)note, &params,
                                       aspect, ex, ey) == 3);
            for (int e = 0; e < 3; e++) {
                CHECK(sumi_layout_probe(SUMI_LAYOUT_JANKO, &params, aspect, nullptr,
                                        ex[e], ey[e], &c));
                CHECK(c.note == (uint8_t)note);
                CHECK_NEAR(c.cell_center_x, ex[e], 1e-5f);
                CHECK_NEAR(c.cell_center_y, ey[e], 1e-5f);
            }
        }
        // Jankó semitone step golden (DECISIONS_3 #18): pitch lives on x
        // alone — the step is half a column straight along +x (the parity
        // rows are echoes of the same notes; the stagger interleaves each
        // semitone half a column over). Glides stay in the touched row.
        {
            const float ncols = 42.0f;   // cols 12..53
            const float expect = (0.5f / (ncols + 0.5f)) * (1.0f - 2.0f * 0.06f) * aspect;
            float ex[SUMI_MAX_ECHOES], ey[SUMI_MAX_ECHOES];
            sumi_layout_position(SUMI_LAYOUT_JANKO, 60, &params, aspect, ex, ey);
            sumi_layout_probe(SUMI_LAYOUT_JANKO, &params, aspect, nullptr, ex[0], ey[0], &c);
            CHECK_NEAR(c.semitone_step, expect, 1e-4f);
            CHECK_NEAR(c.semitone_dx, 1.0f, 1e-4f);   // horizontal, like the grid
            CHECK_NEAR(c.semitone_dy, 0.0f, 1e-4f);
        }
        // PIANO_GRID: probing every cell center (both key rows) round-trips
        // the note and returns that exact center.
        params.pitch_layout = SUMI_LAYOUT_PIANO_GRID;
        for (int note = 24; note <= 107; note++) {
            float px[SUMI_MAX_ECHOES], py[SUMI_MAX_ECHOES];
            CHECK(sumi_layout_position(SUMI_LAYOUT_PIANO_GRID, (uint8_t)note,
                                       &params, aspect, px, py) == 1);
            CHECK(sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &params, aspect, nullptr,
                                    px[0], py[0], &c));
            CHECK(c.note == (uint8_t)note);
            CHECK_NEAR(c.cell_center_x, px[0], 1e-5f);
            CHECK_NEAR(c.cell_center_y, py[0], 1e-5f);
            CHECK(c.cell_radius > 0.0f);
            CHECK(c.semitone_step > 0.0f);
        }
        // Piano-grid semitone axis (DECISIONS_3 #29): the generic shortest-
        // neighbor rule — C4's nearest semitone is C#4, half a key over and
        // 0.9 of a row up (#60/#61: the natural is centred on the band it
        // owns, the pair's bottom 0.6, and the accidental on its own row —
        // pitch is not a function of x alone on this lattice).
        {
            float px[SUMI_MAX_ECHOES], py[SUMI_MAX_ECHOES];
            sumi_layout_position(SUMI_LAYOUT_PIANO_GRID, 60, &params, aspect, px, py);
            sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &params, aspect, nullptr, px[0], py[0], &c);
            const float exdx = (0.5f / 7.0f) * 0.84f * aspect;   // half a key
            const float exdy = -(0.80f / 14.0f) * 0.9f;          // 0.9 of a row up
            const float exstep = std::sqrt(exdx * exdx + exdy * exdy);
            CHECK_NEAR(c.semitone_step, exstep, 1e-4f);
            CHECK_NEAR(c.semitone_dx, exdx / exstep, 1e-4f);
            CHECK_NEAR(c.semitone_dy, exdy / exstep, 1e-4f);
            // #60: the cell a shell DRAWS (centre +/- cell_radius) must be the
            // region the probe actually gives that note. Before the fix the
            // natural's circle hung half a row below its own touch region —
            // its bottom quarter played the octave below on screen — which is
            // what "the touch squares are not aligned with the cells" meant.
            // Scan the vertical line through C4's centre and compare.
            {
                float top = -1.0f, bot = -1.0f;
                sumi_cell_info_t s;
                for (int i = 0; i <= 4000; i++) {
                    const float yy = (float)i / 4000.0f;
                    if (sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &params, aspect, nullptr,
                                          px[0], yy, &s) && s.note == 60) {
                        if (top < 0.0f) top = yy;
                        bot = yy;
                    }
                }
                CHECK(top > 0.0f && bot > top);
                // Same centre, to within the scan's own resolution.
                CHECK_NEAR((top + bot) * 0.5f, py[0], 1e-3f);
                // And the drawn circle spans that region, not something else.
                CHECK_NEAR(bot - top, 2.0f * c.cell_radius, 3e-3f);
            }
            // R_max golden (#29 as amended by #61): half of min(key width,
            // 0.6 of the OCTAVE-PAIR height) — a natural is a full key wide
            // but only owns the band below the glissando corridor.
            const float pw = (0.84f / 7.0f) * aspect;
            const float oh = 0.6f * (0.80f / 7.0f);
            CHECK_NEAR(c.cell_radius, 0.5f * (pw < oh ? pw : oh), 1e-5f);
        }
        params.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    }

    // Refusals: non-playable layouts, outside the playable area, Jankó
    // stagger dead zones.
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_FIFTHS, &params, 1.0f, nullptr, 0.5f, 0.5f, &c));
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_ROLL_H, &params, 1.0f, nullptr, 0.5f, 0.5f, &c));
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_ROLL_V, &params, 1.0f, nullptr, 0.5f, 0.5f, &c));
    CHECK(!sumi_layout_probe(99u, &params, 1.0f, nullptr, 0.5f, 0.5f, &c));
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, 1.0f, nullptr, 0.02f, 0.5f, &c));
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, 1.0f, nullptr, 0.5f, 0.95f, &c));
    // Odd (staggered) Jankó rows have a half-cell dead zone at their left
    // edge — off the key bed, honestly unplayable.
    const float row1_y = 0.10f + (1.5f / 6.0f) * 0.80f;
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_JANKO, &params, 1.0f, nullptr,
                             0.06f + 0.001f, row1_y, &c));
    // Piano-grid #61: the accidental row is a GLISSANDO CORRIDOR. Accidentals
    // are 0.6 keys wide and are the only thing in it; between and beyond them
    // there is nothing, so a pen sliding through sustains (#39) and the black
    // keys play as the pentatonic run. The naturals keep the pair's bottom
    // 0.6 and still tile it, so their own glissando is untouched. (This
    // supersedes #41's white-key tops, which made that same slide play the
    // full chromatic scale — measured on device before the change.)
    const float black_row_y = 0.10f + (0.5f / 14.0f) * 0.80f;   // corridor
    const float white_row_y = 0.10f + (1.5f / 14.0f) * 0.80f;   // natural band
    CHECK(sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &params, 1.0f, nullptr,
                            0.08f + (2.25f / 7.0f) * 0.84f, black_row_y, &c));
    CHECK(c.note == 27);   // D#1: within 0.3 of unit 2 — the accidental
    // Everything else in the corridor is deliberately empty.
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &params, 1.0f, nullptr,
                             0.08f + (2.35f / 7.0f) * 0.84f, black_row_y, &c));
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &params, 1.0f, nullptr,
                             0.08f + (3.0f / 7.0f) * 0.84f, black_row_y, &c));
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &params, 1.0f, nullptr,
                             0.08f + (0.2f / 7.0f) * 0.84f, black_row_y, &c));
    // The natural band below it tiles completely — no gaps, no dead spots.
    for (int k = 0; k < 70; k++) {
        const float xu = 0.05f + (float)k * 0.099f;   // 0.05 .. 6.88 white units
        CHECK(sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &params, 1.0f, nullptr,
                                0.08f + (xu / 7.0f) * 0.84f, white_row_y, &c));
    }
    CHECK(sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &params, 1.0f, nullptr,
                            0.08f + (2.9f / 7.0f) * 0.84f, white_row_y, &c));
    CHECK(c.note == 28);   // E1
    // Cell SIZE across aspects (#57 as amended by #61). Both cells are 0.6 of
    // an octave pair tall; width is what still separates them — an accidental
    // is 0.6 of a key, a natural a full one. So wherever HEIGHT governs (any
    // landscape aspect) the two knobs are the same size, which is what the
    // surface is built around, and the accidental is smaller only on screens
    // tall enough for width to govern. Pinned as the FORMULA at aspects on
    // both sides of that crossover — the original assertion ran at aspect 1.0
    // alone, and that is precisely why #56's defect went unseen for a phase.
    {
        sumi_cell_info_t cb, cn;
        static const float ASPECTS[7] = {1.0f, 1.2f, 1.4390f, 1.5873f, 1.7778f, 2.16f, 0.624f};
        const float key = 0.84f / 7.0f;            // white key width, normalised x
        const float pair = 0.80f / 7.0f;           // octave-pair height
        const float h = PIANO_NATURAL_H_GOLDEN * pair;
        for (int ai = 0; ai < 7; ai++) {
            const float a = ASPECTS[ai];
            CHECK(sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &params, a, nullptr,
                                    0.08f + (1.0f / 7.0f) * 0.84f, black_row_y, &cb));
            CHECK(cb.note == 25);                  // C#1, in the corridor
            CHECK(sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &params, a, nullptr,
                                    0.08f + (0.5f / 7.0f) * 0.84f, white_row_y, &cn));
            CHECK(cn.note == 24);                  // C1, in the natural band
            const float wn = key * a, wb = PIANO_BLACK_KEY_W_GOLDEN * key * a;
            CHECK_NEAR(cn.cell_radius, 0.5f * (wn < h ? wn : h), 1e-5f);
            CHECK_NEAR(cb.cell_radius, 0.5f * (wb < h ? wb : h), 1e-5f);
            CHECK(cb.cell_radius <= cn.cell_radius);
        }
    }

    // Purity: identical input, identical output, no instance anywhere.
    sumi_cell_info_t c2;
    CHECK(sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, 1.7f, nullptr, 0.4f, 0.6f, &c));
    CHECK(sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, 1.7f, nullptr, 0.4f, 0.6f, &c2));
    CHECK(c.note == c2.note && c.cell_center_x == c2.cell_center_x &&
          c.semitone_step == c2.semitone_step && c.cell_radius == c2.cell_radius);
}

// -------------------------------------------------------------------------
// Phase 4 working rule: every byte stream hostmpe generates must be valid MPE
// as OUR OWN normalizer defines it — the loopback is a permanent conformance
// test of both sides. Session config -> deterministic MPE mode + ±48 range;
// a touch -> VoiceBegin at the probed cell; one column of drag -> a glide the
// normalizer decodes as EXACTLY one semitone (the full MCM/RPN0 round trip).
static void test_hostmpe_loopback_conformance() {
    sumi_normalizer_t* nz = sumi_normalizer_create(nullptr, nullptr);
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    hostmpe_t* h = hostmpe_create();
    sumi_params_t params = default_params();
    params.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    const float aspect = 16.0f / 9.0f;

    // Play-mode activation pushes the session config into the loopback.
    hostmpe_msg_t cfg[128];
    const uint32_t nc = hostmpe_session_config(h, cfg, 128);
    for (uint32_t i = 0; i < nc; i++)
        sumi_normalizer_push(nz, cfg[i].status, cfg[i].data1, cfg[i].data2);
    sumi_midi_event_t mev[256];
    sumi_normalizer_drain(nz, tnow(), mev, 256);
    CHECK(sumi_normalizer_mode(nz) == SUMI_INPUT_MPE);        // deterministic,
    CHECK(sumi_normalizer_zone(nz).member_count == 15);       // never heuristic

    // Touch-down at note 66's cell (probed like the shell does it).
    float px[SUMI_MAX_ECHOES], py[SUMI_MAX_ECHOES];
    sumi_layout_position(SUMI_LAYOUT_CHROMA_GRID, 66, &params, aspect, px, py);
    sumi_cell_info_t cell;
    CHECK(sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, aspect, nullptr,
                            px[0], py[0], &cell));
    hostmpe_msg_t m[8];
    uint32_t n = 0;
    const int32_t v = hostmpe_touch_begin(h, tnow(), 66, 96,
                                          cell.cell_radius, 1.0f / cell.semitone_step, 0.0f,
                                          m, 8, &n);
    CHECK(v >= 1);
    for (uint32_t i = 0; i < n; i++)
        sumi_normalizer_push(nz, m[i].status, m[i].data1, m[i].data2);
    uint32_t nm = sumi_normalizer_drain(nz, tnow(), mev, 256);
    sumi_voice_event_t vev[16];
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, mev, nm, SUMI_INPUT_MPE,
                                              sumi_normalizer_zone(nz), &params,
                                              aspect, vev, 16);
    bool began = false;
    for (uint32_t i = 0; i < nv; i++) {
        if (vev[i].kind == SUMI_VEV_VOICE_BEGIN) {
            began = true;
            CHECK_NEAR(vev[i].x, cell.cell_center_x, 1e-4f);   // the lattice IS
            CHECK_NEAR(vev[i].y, cell.cell_center_y, 1e-4f);   // where drops land
            CHECK_NEAR(vev[i].value, 96.0f / 127.0f, 1e-3f);
        }
    }
    CHECK(began);

    // One grid column of drag, angled slightly UPWARD (§3.3 rev: up = press) ->
    // the normalizer must decode ±48-scaled bend back to EXACTLY one semitone
    // (hostmpe encoded at 48; RPN 0 = 48 landed) AND see channel pressure.
    n = hostmpe_touch_update(h, v, cell.semitone_step, -0.6f * cell.cell_radius, m, 8);
    for (uint32_t i = 0; i < n; i++)
        sumi_normalizer_push(nz, m[i].status, m[i].data1, m[i].data2);
    nm = sumi_normalizer_drain(nz, tnow(), mev, 256);
    bool bend_seen = false, press_seen = false;
    for (uint32_t i = 0; i < nm; i++) {
        if (mev[i].kind == SUMI_MEV_BEND) {
            CHECK_NEAR(mev[i].f, 1.0f, 0.01f);   // 171/8192*48 = 1.002
            bend_seen = true;
        }
        if (mev[i].kind == SUMI_MEV_CHANNEL_PRESSURE) press_seen = true;
    }
    CHECK(bend_seen);
    CHECK(press_seen);

    // Lift: pressure 0 then Note Off -> a VoiceEnd with the lift velocity.
    n = hostmpe_touch_end(h, v, tnow(), 80, m, 8);
    for (uint32_t i = 0; i < n; i++)
        sumi_normalizer_push(nz, m[i].status, m[i].data1, m[i].data2);
    nm = sumi_normalizer_drain(nz, tnow(), mev, 256);
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, mev, nm, SUMI_INPUT_MPE,
                                     sumi_normalizer_zone(nz), &params,
                                     aspect, vev, 16);
    bool ended = false;
    for (uint32_t i = 0; i < nv; i++)
        if (vev[i].kind == SUMI_VEV_VOICE_END) {
            ended = true;
            CHECK_NEAR(vev[i].value, 80.0f / 127.0f, 1e-3f);
        }
    CHECK(ended);

    hostmpe_destroy(h);
    sumi_voice_mapper_destroy(vm);
    sumi_normalizer_destroy(nz);
}

// -------------------------------------------------------------------------
static void test_roll_field_motion_clock() {
    // §3.4 DONE check with the scripted clock: at 120 BPM and the default
    // roll_speed 0.0625 (canvas-lengths per BEAT), the accumulated translation
    // must be 1/16 canvas per beat — 4 beats traverse a quarter canvas, a full
    // traversal every 16 beats (4 bars of 4/4).
    sumi_params_t params = default_params();
    params.pitch_layout = SUMI_LAYOUT_ROLL_H;
    params.bpm = 120.0f;
    CHECK_NEAR(params.roll_speed, 0.0625f, 1e-9f);   // spec default

    // One beat at 120 BPM = 0.5 s. Scripted clock: 60 frames × (1/120 s).
    double acc = 0.0;
    for (int f = 0; f < 60; f++) {
        float dx = 0.0f, dy = 0.0f;
        CHECK(sumi_layout_field_motion(SUMI_LAYOUT_ROLL_H, &params, 1.0 / 120.0, &dx, &dy));
        CHECK(dy == 0.0f);   // horizontal roll drifts +x only
        acc += dx;
    }
    CHECK_NEAR((float)acc, 0.0625f, 1e-4f);    // 1/16 canvas per beat
    // 3 more beats -> quarter canvas after 4 beats.
    for (int f = 0; f < 180; f++) {
        float dx = 0.0f, dy = 0.0f;
        sumi_layout_field_motion(SUMI_LAYOUT_ROLL_H, &params, 1.0 / 120.0, &dx, &dy);
        acc += dx;
    }
    CHECK_NEAR((float)acc, 0.25f, 1e-3f);
    // 12 more beats -> full canvas traversal after 16.
    for (int f = 0; f < 720; f++) {
        float dx = 0.0f, dy = 0.0f;
        sumi_layout_field_motion(SUMI_LAYOUT_ROLL_H, &params, 1.0 / 120.0, &dx, &dy);
        acc += dx;
    }
    CHECK_NEAR((float)acc, 1.0f, 1e-3f);

    // Vertical roll drifts +y (down); static layouts report no motion.
    float dx = 0.0f, dy = 0.0f;
    CHECK(sumi_layout_field_motion(SUMI_LAYOUT_ROLL_V, &params, 0.016, &dx, &dy));
    CHECK(dx == 0.0f && dy > 0.0f);
    CHECK(!sumi_layout_field_motion(SUMI_LAYOUT_FIFTHS, &params, 0.016, &dx, &dy));
    CHECK(!sumi_layout_field_motion(SUMI_LAYOUT_JANKO, &params, 0.016, &dx, &dy));

    // Live bpm/roll_speed changes scale the very next query (no latching).
    params.bpm = 240.0f;
    params.roll_speed = 0.25f;
    sumi_layout_field_motion(SUMI_LAYOUT_ROLL_H, &params, 0.5, &dx, &dy);
    CHECK_NEAR(dx, 0.5f, 1e-5f);   // 4 beats/s * 0.25 * 0.5 s
}

// -------------------------------------------------------------------------
static void test_layout_glide_axis_and_live_switch() {
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[8];

    // CHROMA_GRID: the pitch axis is the row direction (horizontal), even for
    // B notes where note+1 wraps to the next row (shorter-neighbor rule).
    params.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    sumi_midi_event_t on_c = {SUMI_MEV_NOTE_ON, 2, 60, 100, 0.0f};   // C4
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &on_c, 1, SUMI_INPUT_MPE,
                                              default_zone(), &params, 1.0f, vev, 8);
    CHECK(nv == 1);
    CHECK(std::fabs(vev[0].ay) < 1e-5f);   // along the row
    CHECK(vev[0].ax > 0.0f);
    sumi_midi_event_t off_c = {SUMI_MEV_NOTE_OFF, 2, 60, 10, 0.0f};
    sumi_voice_mapper_normalize(vm, tnow(), 0, &off_c, 1, SUMI_INPUT_MPE,
                                default_zone(), &params, 1.0f, vev, 8);
    sumi_midi_event_t on_b = {SUMI_MEV_NOTE_ON, 2, 59, 100, 0.0f};   // B3 (row wrap up)
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &on_b, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 8);
    CHECK(nv == 1);
    CHECK(std::fabs(vev[0].ay) < 1e-5f);   // still along the row, no diagonal
    CHECK(vev[0].ax > 0.0f);

    // Live switch mid-note: the ACTIVE voice keeps its position and axis —
    // only new placements use the new layout (no teleporting).
    sumi_deform_queue_t* q = sumi_deform_queue_create(64);
    uint32_t drop_counter = 0;
    sumi_midi_event_t off_b = {SUMI_MEV_NOTE_OFF, 2, 59, 10, 0.0f};
    sumi_voice_mapper_normalize(vm, tnow(), 0, &off_b, 1, SUMI_INPUT_MPE,
                                default_zone(), &params, 1.0f, vev, 8);
    params.pitch_layout = SUMI_LAYOUT_FIFTHS;
    sumi_midi_event_t on1 = {SUMI_MEV_NOTE_ON, 3, 60, 100, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &on1, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 8);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    float fifths_x = sumi_deform_queue_at(q, 0)->as.drop.x;
    float fifths_y = sumi_deform_queue_at(q, 0)->as.drop.y;
    sumi_deform_queue_clear(q);

    params.pitch_layout = SUMI_LAYOUT_JANKO;   // switch WHILE the note is held
    sumi_midi_event_t bend = {SUMI_MEV_BEND, 3, 0, 0, 6.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &bend, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 8);
    bool saw_tine = false;
    for (int fdx = 0; fdx < 50; fdx++) {
        sumi_voice_mapper_lower(vm, vev, fdx == 0 ? nv : 0, 0.016, &params, true, &drop_counter, q);
        for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
            const sumi_deform_t* d = sumi_deform_queue_at(q, i);
            if (d->type == SUMI_DEFORM_TINE && !saw_tine) {
                saw_tine = true;
                // Glide starts from the FIFTHS position captured at note-on.
                CHECK_NEAR(d->as.tine.x0, fifths_x, 1e-3f);
                CHECK_NEAR(d->as.tine.y0, fifths_y, 1e-3f);
            }
        }
        sumi_deform_queue_clear(q);
    }
    CHECK(saw_tine);
    // A NEW note under the new layout places per Janko.
    sumi_midi_event_t on2 = {SUMI_MEV_NOTE_ON, 4, 64, 100, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &on2, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 8);
    float jx[SUMI_MAX_ECHOES], jy[SUMI_MAX_ECHOES];
    CHECK(sumi_layout_position(SUMI_LAYOUT_JANKO, 64, &params, 1.0f, jx, jy) == 3);
    CHECK(vev[0].echo_count == 3);
    for (int e = 0; e < 3; e++) {
        CHECK_NEAR(vev[0].ex[e], jx[e], 1e-5f);
        CHECK_NEAR(vev[0].ey[e], jy[e], 1e-5f);
    }

    sumi_deform_queue_destroy(q);
    sumi_voice_mapper_destroy(vm);
}

// -------------------------------------------------------------------------
// v0.4 bend_mode (roadmap step-19 DONE, DECISIONS_3 #35, corrected): ONE
// consumer owns a note's pitch bend — a bend sweep drags the drop (glide
// tines) OR raises the ripple, never both. The ripple AMOUNT is the bend's
// distance from center, exactly like glide displacement: vibrato breathes
// the shimmer, and the water stills itself when the note re-centers,
// releases, or the mode flips back — "you can come back from a ripple".
static void test_bend_mode_single_consumer() {
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_deform_queue_t* q = sumi_deform_queue_create(64);
    sumi_params_t params = default_params();
    params.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    sumi_voice_event_t vev[16];
    uint32_t drop_counter = 0;

    auto note_on = [&]() {
        sumi_midi_event_t on = {SUMI_MEV_NOTE_ON, 2, 60, 100, 0.0f};
        uint32_t n = sumi_voice_mapper_normalize(vm, tnow(), 0, &on, 1, SUMI_INPUT_MPE,
                                                 default_zone(), &params, 1.0f, vev, 16);
        sumi_voice_mapper_lower(vm, vev, n, 0.016, &params, true, &drop_counter, q);
        sumi_deform_queue_clear(q);
    };
    float phase_min = 1e9f, phase_max = -1e9f;   // #36 drift tracking
    auto sweep = [&](float semis, uint32_t* glide_tines, uint32_t* ripples) {
        *glide_tines = 0;
        *ripples = 0;
        sumi_midi_event_t bend = {SUMI_MEV_BEND, 2, 0, 0, semis};   // member ch
        uint32_t n = sumi_voice_mapper_normalize(vm, tnow(), 0, &bend, 1,
                                                 SUMI_INPUT_MPE, default_zone(),
                                                 &params, 1.0f, vev, 16);
        for (int f = 0; f < 60; f++) {
            sumi_voice_mapper_lower(vm, vev, f == 0 ? n : 0, 0.016, &params, true,
                                    &drop_counter, q);
            for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
                const sumi_deform_t* d = sumi_deform_queue_at(q, i);
                if (d->type == SUMI_DEFORM_TINE) (*glide_tines)++;
                if (d->type == SUMI_DEFORM_RIPPLE) {
                    (*ripples)++;
                    if (d->as.ripple.phase < phase_min) phase_min = d->as.ripple.phase;
                    if (d->as.ripple.phase > phase_max) phase_max = d->as.ripple.phase;
                }
            }
            sumi_deform_queue_clear(q);
        }
    };

    note_on();
    uint32_t tines = 0, ripples = 0;

    // Mode 0 (v1): a note bend drags the drop — glide tines, no ripple.
    params.bend_mode = 0;
    params.ripple_bake = 1;   // bake armed: a ripple WOULD be a visible pass
    sweep(3.0f, &tines, &ripples);
    CHECK(tines >= 1);
    CHECK(ripples == 0);

    // Mode 1: the same note bend raises the ripple — amount = |distance from
    // center| / 6, the drop HOLDS. No CC feed needed: the bend IS the amount.
    params.bend_mode = 1;
    sweep(6.0f, &tines, &ripples);
    CHECK(tines == 0);
    CHECK(ripples >= 1);
    CHECK(sumi_voice_mapper_ctl(vm, SUMI_CTL_RIPPLE_AMP) > 0.8f);   // saturated
    // A subtle vibrato maps proportionally: -0.6 semi -> amount 0.4 (#66: |1.5| saturates).
    sweep(-0.6f, &tines, &ripples);
    CHECK(tines == 0);
    CHECK_NEAR(sumi_voice_mapper_ctl(vm, SUMI_CTL_RIPPLE_AMP), 0.6f / 1.5f, 0.03f);

    // Coming back from a ripple, way 1: the bend re-centers -> the amount
    // goes home to zero. The DYNAMIC stills; the MARK stays (#36): under
    // bend-driven bake the phase drifted across the excursion, so the up and
    // down passes laid slightly shifted combs — the feathered residue is the
    // permanence, like glide's tines.
    sweep(0.0f, &tines, &ripples);
    CHECK(tines == 0);
    CHECK(sumi_voice_mapper_ctl(vm, SUMI_CTL_RIPPLE_AMP) < 0.02f);
    CHECK(phase_max - phase_min > 0.1f);   // drift engaged: passes don't retrace

    // Way 2: the last note releases mid-bend -> the water stills.
    sweep(4.0f, &tines, &ripples);
    CHECK(sumi_voice_mapper_ctl(vm, SUMI_CTL_RIPPLE_AMP) > 0.5f);
    sumi_midi_event_t off = {SUMI_MEV_NOTE_OFF, 2, 60, 10, 0.0f};
    uint32_t n = sumi_voice_mapper_normalize(vm, tnow(), 0, &off, 1, SUMI_INPUT_MPE,
                                             default_zone(), &params, 1.0f, vev, 16);
    for (int f = 0; f < 60; f++) {
        sumi_voice_mapper_lower(vm, vev, f == 0 ? n : 0, 0.016, &params, true,
                                &drop_counter, q);
        sumi_deform_queue_clear(q);
    }
    CHECK(sumi_voice_mapper_ctl(vm, SUMI_CTL_RIPPLE_AMP) < 0.02f);

    // Way 3: flipping the mode back mid-bend zeroes the residual amount and
    // ordinary glide resumes — never both consumers.
    note_on();
    params.bend_mode = 1;
    sweep(5.0f, &tines, &ripples);
    CHECK(sumi_voice_mapper_ctl(vm, SUMI_CTL_RIPPLE_AMP) > 0.5f);
    params.bend_mode = 0;
    sweep(2.0f, &tines, &ripples);
    CHECK(tines >= 1);
    CHECK(sumi_voice_mapper_ctl(vm, SUMI_CTL_RIPPLE_AMP) < 0.02f);

    sumi_deform_queue_destroy(q);
    sumi_voice_mapper_destroy(vm);
}

// -------------------------------------------------------------------------
// v0.4 Lamb-Oseen swirl routing (step-20 DONE, §2.1/§3.4): 0xA0 decodes to
// the swirl dimension (keyed by the voice's note); press_mode = 1 routes 0xD0
// into swirls with ZERO grow passes (one consumer); adjacent notes
// counter-rotate (band-parity sign); r_c is the voice's boundary R.
// Phase 6 step 37 (MEDIUM §2.2): the Chladni lattice is exact BECAUSE the
// second shear is evaluated at the displaced x₁ (kick-drift). The
// "simultaneous" form — both shears from the undisplaced point — is not, and
// its sign flip is not its inverse. This is the negative the roadmap asks for:
// the formulas are the shader's (deform.glsl chladni_fs), in double.
static void test_chladni_kick_drift_order() {
    const double a = 0.04, b = 0.04, kx = 3.0 * 6.283185307179586, ky = 2.0 * 6.283185307179586;
    auto T = [&](double x, double y, double* u, double* v) { *u = x + a * std::cos(ky * y); *v = y + b * std::cos(kx * *u); };
    auto Tinv = [&](double u, double v, double* x, double* y) { *y = v - b * std::cos(kx * u); *x = u - a * std::cos(ky * *y); };
    auto S = [&](double x, double y, double* u, double* v) { *u = x + a * std::cos(ky * y); *v = y + b * std::cos(kx * x); };
    auto Sflip = [&](double u, double v, double* x, double* y) { *x = u - a * std::cos(ky * v); *y = v - b * std::cos(kx * u); };
    auto det = [&](auto&& F, double x, double y) {
        const double h = 1e-6;
        double ux1, vx1, ux0, vx0, uy1, vy1, uy0, vy0;
        F(x + h, y, &ux1, &vx1); F(x - h, y, &ux0, &vx0);
        F(x, y + h, &uy1, &vy1); F(x, y - h, &uy0, &vy0);
        return ((ux1 - ux0) / (2 * h)) * ((vy1 - vy0) / (2 * h)) - ((uy1 - uy0) / (2 * h)) * ((vx1 - vx0) / (2 * h));
    };
    double worst_inv = 0.0, worst_det_T = 0.0, worst_det_S = 0.0, worst_flip = 0.0;
    for (int i = 0; i <= 40; i++) {
        for (int j = 0; j <= 40; j++) {
            const double x = i / 40.0, y = j / 40.0;
            double u, v, xb, yb;
            T(x, y, &u, &v); Tinv(u, v, &xb, &yb);
            worst_inv = std::fmax(worst_inv, std::hypot(xb - x, yb - y));
            worst_det_T = std::fmax(worst_det_T, std::fabs(det(T, x, y) - 1.0));
            worst_det_S = std::fmax(worst_det_S, std::fabs(det(S, x, y) - 1.0));
            S(x, y, &u, &v); Sflip(u, v, &xb, &yb);
            worst_flip = std::fmax(worst_flip, std::hypot(xb - x, yb - y));
        }
    }
    CHECK(worst_inv < 1e-9);       // the kick-drift's inverse (y first, then x) is exact
    CHECK(worst_det_T < 1e-6);     // det J = 1 everywhere, at this (strong) amplitude
    CHECK(worst_det_S > 0.05);     // NEGATIVE: the simultaneous form is not area-preserving (|1 − det| = a·b·kx·ky·|sin·sin|, up to 0.38 here)
    CHECK(worst_flip > 1e-3);      // NEGATIVE: a sign flip does not invert it either
    std::printf("  chladni: kick-drift inverse %.1e, |det−1| %.1e; simultaneous |det−1| up to %.3f, flip residue up to %.4f\n",
                worst_inv, worst_det_T, worst_det_S, worst_flip);
}

// Phase 6 step 43 (QOL §1): the preset library and the morph ring, headless.
static void test_palette_presets_and_ring() {
    // the ring: a built-in active id travels the three built-ins with the shader's own arithmetic
    uint32_t a, b; float m;
    for (uint32_t id = 0; id < 4u; id++) { sumi_palette_ring(id, 0.0f, &a, &b, &m); CHECK(a == id); CHECK(m == 0.0f); CHECK(b == (id == 3u ? 0u : (id + 1u) % 3u)); }
    sumi_palette_ring(0u, 0.25f, &a, &b, &m); CHECK(a == 0u && b == 1u); CHECK(std::fabs(m - 0.5f) < 1e-7f);
    sumi_palette_ring(0u, 0.5f, &a, &b, &m);  CHECK(a == 1u && b == 2u); CHECK(m == 0.0f);          // t = 1: the second segment's start
    sumi_palette_ring(2u, 0.75f, &a, &b, &m); CHECK(a == 0u && b == 1u); CHECK(std::fabs(m - 0.5f) < 1e-7f);
    sumi_palette_ring(3u, 0.5f, &a, &b, &m);  CHECK(a == 0u && b == 1u); CHECK(std::fabs(m - 0.5f) < 1e-7f);   // custom -> 0 -> 1 -> 2
    sumi_palette_ring(3u, 1.0f, &a, &b, &m);  CHECK(a == 1u && b == 2u && m == 1.0f);   // t = 3: the last segment's end, palette 2 in full
    sumi_palette_ring(1u, -1.0f, &a, &b, &m); CHECK(a == 1u && m == 0.0f);                             // clamped
    // the library: six presets a medium, ascending stops, everything in range, the built-ins the legacy literals
    for (uint32_t medium = 0; medium < 2u; medium++) {
        CHECK(sumi_palette_preset_count(medium) == 6u);
        for (uint32_t i = 0; i < 6u; i++) {
            sumi_palette_t p; const char* nm = nullptr;
            CHECK(sumi_palette_preset(medium, i, &p, &nm)); CHECK(nm != nullptr);
            CHECK(p.stop_count >= 2u && p.stop_count <= 8u);
            for (uint32_t k = 1; k < p.stop_count; k++) CHECK(p.stops[k].position >= p.stops[k - 1].position);
            CHECK(p.stops[0].position == 0.0f && p.stops[p.stop_count - 1].position == 1.0f);
            CHECK(p.hue_drift >= 0.0f && p.hue_drift <= 1.0f && p.depth_gamma >= 0.25f && p.depth_gamma <= 4.0f);
            for (uint32_t k = 0; k < 8u; k++) for (int c = 0; c < 3; c++) CHECK(p.stops[k].rgb[c] >= 0.0f && p.stops[k].rgb[c] <= 1.0f);
        }
    }
    CHECK(sumi_palette_preset_count(2u) == 0u);
    sumi_palette_t s1; sumi_palette_preset(0u, 1u, &s1, nullptr);
    CHECK(s1.stops[0].rgb[0] == 0.015f && s1.stops[0].rgb[1] == 0.035f && s1.stops[0].rgb[2] == 0.170f && s1.hue_drift == 0.45f);   // indigo
    CHECK(s1.accent_rgb[1] == 0.110f && s1.clear_rgb[2] == 0.830f);
    std::printf("  palettes: 6 + 6 presets, the ring 0->1->2 and custom->0->1->2 in the shader's arithmetic\n");
}

// Phase 6 step 38 (MEDIUM §2.3): the viscous multipole burst's mathematics
// and its episode, headless — displacement.h in double (the shader carries
// the same formulas in float; tools/multipole_verify.py is the derivation).
static void test_burst_math_and_episode() {
    const double a = 0.04, l_end = 4.0 * a;
    // Φ_2 = χ; the plateau 1/(m−1); Φ_m(S1) − Φ_m(S0) by the closed form
    CHECK_NEAR(sumi_burst_phi(2, 0.5), (1.0 - std::exp(-0.5)) / 0.5, 1e-12);
    CHECK_NEAR(sumi_burst_phi(3, 1e-9), 0.5, 1e-8);
    CHECK_NEAR(sumi_burst_dphi(2, 1.0, 1.0 / 16.0), (1.0 - std::exp(-1.0 / 16.0)) * 16.0 - (1.0 - std::exp(-1.0)), 1e-12);
    for (uint32_t m = 2; m <= 8; m++) {   // the series and the closed form meet at S = 1
        CHECK_NEAR(sumi_burst_phi(m, 0.999999), sumi_burst_phi(m, 1.000001), 1e-6);
        CHECK_NEAR(sumi_burst_gs(m, 0.999999), sumi_burst_gs(m, 1.000001), 1e-6);
    }
    // The normalisation: the radial displacement at r = a on the axis IS D; −D negates
    for (uint32_t m = 2; m <= 4; m++) {
        const double D = 0.01, A = sumi_burst_amp(m, D, a, l_end);
        const double dr = (double)m * A / a * sumi_burst_dphi(m, 1.0, a * a / (l_end * l_end));
        CHECK_NEAR(dr, D, 1e-12);
        CHECK_NEAR(sumi_burst_amp(m, -D, a, l_end), -A, 1e-15);
    }
    // The peak of the quadrupole at age 4 sits at 1.36 a and is 1.066 D (multipole_verify §7)
    {
        const double A = sumi_burst_amp(2, 0.01, a, l_end);
        const double pk = sumi_burst_peak(2, A, a, a, l_end);
        CHECK(pk > 0.01060 && pk < 0.01072);
    }
    // The greedy march tiles a strong burst (D = 2a) into budgeted pieces
    {
        const uint32_t m = 2;
        const double D = 2.0 * a, A = sumi_burst_amp(m, D, a, l_end);
        double l = a, worst = 0.0; int n = 0;
        while (l < l_end * (1.0 - 1e-9) && n < 200) {
            const double ln = sumi_burst_step(m, A, a, l, l_end);
            CHECK(ln > l);
            const double ratio = sumi_burst_peak(m, A, a, l, ln) / (sumi_burst_budget(m) * l);
            if (ratio > worst) worst = ratio;
            l = ln; n++;
        }
        CHECK(worst <= 1.0 + 1e-9);
        CHECK(n >= 6 && n < 60);
        CHECK_NEAR(l, l_end, 1e-9);
        std::printf("  burst: D = 2a over age 4 marches in %d pieces, worst peak/budget %.4f\n", n, worst);
    }
    // The episode through the mapper: life 0 -> the whole burst in one lower(),
    // its passes tiling a -> l_end; life 0.5 s -> increments over 60 frames of
    // 1/120 with l² linear in t, the episode over at the end of the release.
    {
        sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
        sumi_deform_queue_t* q = sumi_deform_queue_create(256);
        sumi_params_t p; std::memset(&p, 0, sizeof p);
        p.burst_age = 4.0f; p.burst_life = 0.0f; p.smoothing_ms = 30.0f;
        uint32_t counter = 0;
        sumi_voice_event_t none[1];
        CHECK(!sumi_voice_mapper_add_burst(vm, 0.5f, 0.5f, 0.0f, 0.02f, 0.0f, 2, &p));   // refused: no core
        CHECK(sumi_voice_mapper_add_burst(vm, 0.5f, 0.5f, (float)a, 0.02f, 0.3f, 2, &p));
        CHECK(sumi_voice_mapper_burst_count(vm) == 1);
        sumi_voice_mapper_lower(vm, none, 0, 1.0 / 120.0, &p, true, &counter, q);
        uint32_t nb = 0; float l_prev = (float)a; bool tiled = true;
        for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
            const sumi_deform_t* d = sumi_deform_queue_at(q, i);
            if (d->type != SUMI_DEFORM_BURST) continue;
            if (std::fabs(d->as.burst.l0 - l_prev) > 1e-6f) tiled = false;
            l_prev = d->as.burst.l1; nb++;
            CHECK(d->as.burst.m == 2 && std::fabs(d->as.burst.theta0 - 0.3f) < 1e-6f && std::fabs(d->as.burst.a - (float)a) < 1e-7f);
        }
        CHECK(nb >= 2 && tiled && std::fabs(l_prev - (float)l_end) < 1e-5f);
        CHECK(sumi_voice_mapper_burst_count(vm) == 0);
        std::printf("  burst episode, life 0: %u passes tile a -> 4a in one frame\n", nb);
        sumi_deform_queue_clear(q);
        p.burst_life = 0.5f;
        CHECK(sumi_voice_mapper_add_burst(vm, 0.5f, 0.5f, (float)a, 0.005f, 0.0f, 3, &p));
        int frames_with_pass = 0; float l1_first = 0.0f;
        for (int f = 1; f <= 70; f++) {
            sumi_deform_queue_clear(q);
            sumi_voice_mapper_lower(vm, none, 0, 1.0 / 120.0, &p, true, &counter, q);
            bool any = false;
            for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
                const sumi_deform_t* d = sumi_deform_queue_at(q, i);
                if (d->type != SUMI_DEFORM_BURST) continue;
                any = true;
                if (f == 1) l1_first = d->as.burst.l1;
                CHECK(d->as.burst.m == 3);
            }
            frames_with_pass += any ? 1 : 0;
            if (f == 30) CHECK(sumi_voice_mapper_burst_count(vm) == 1);
        }
        CHECK(sumi_voice_mapper_burst_count(vm) == 0);
        // the first frame's increment ends at l(1/120) = a·sqrt(1 + 15/60)
        CHECK_NEAR(l1_first, (float)(a * std::sqrt(1.25)), 1e-5);
        CHECK(frames_with_pass >= 10 && frames_with_pass <= 61);
        std::printf("  burst episode, life 0.5 s: passes on %d of 70 frames, first age %.5f (a*sqrt(1.25) = %.5f)\n",
                    frames_with_pass, (double)l1_first, a * std::sqrt(1.25));
        sumi_deform_queue_destroy(q);
        sumi_voice_mapper_destroy(vm);
    }
}

// Phase 6 step 39 (MEDIUM §2.4): the spark shear — "shears invert for any
// profile" as a test, in double, for the triangle stack AND a noise profile;
// the same-order sign flip is NOT the inverse; then the mapper's decaying
// episode and the slide_mode 2 preparation.
static double t39_tri(double t) { const double u = t / 6.283185307179586 - std::floor(t / 6.283185307179586); return 1.0 - 4.0 * std::fabs(u - 0.5); }
static double t39_noise(double t) {   // piecewise-linear value noise on a unit lattice, an LCG per cell
    const double c = t / 6.283185307179586 + 4096.0, fl = std::floor(c), fr = c - fl;
    auto h = [](uint32_t i) { uint32_t n = i * 2654435761u + 17u; n ^= n >> 16; n *= 0x7feb352du; n ^= n >> 15; n *= 0x846ca68bu; n ^= n >> 16; return (double)(n & 0x00ffffffu) / 16777216.0 * 2.0 - 1.0; };
    const uint32_t i0 = (uint32_t)fl;
    return h(i0) + (h(i0 + 1) - h(i0)) * fr;
}
static double t39_profile(double c, double k, double ph, int stack, bool noise) {
    double acc = 0.0, wsum = 0.0, w = 1.0, kk = k;
    for (int n = 0; n < stack; n++) { const double t = kk * c + ph * (n + 1) + n * 1.9; acc += w * (noise ? t39_noise(t) : t39_tri(t)); wsum += w; w *= 0.5; kk *= 2.0; }
    return acc / wsum;
}
static void test_spark_shear_math_and_episode() {
    const double A = 0.03, B = 0.03, k = 6.283185307179586 * 12.0, ph = 0.7, band = 0.2;
    auto w = [&](double c) { return std::exp(-0.5 * c * c / (band * band)); };
    for (int prof = 0; prof < 2; prof++) {
        const bool noise = prof == 1;
        auto S0 = [&](double x, double y, double s, double* u, double* v) { *u = x + s * A * w(y) * t39_profile(y, k, ph, 3, noise); *v = y; };
        auto S1 = [&](double x, double y, double s, double* u, double* v) { *u = x; *v = y + s * B * w(x) * t39_profile(x, k, ph + 2.3, 3, noise); };
        auto T = [&](double x, double y, double* u, double* v) { double a, b; S0(x, y, 1.0, &a, &b); S1(a, b, 1.0, u, v); };
        auto Tinv = [&](double u, double v, double* x, double* y) { double a, b; S1(u, v, -1.0, &a, &b); S0(a, b, -1.0, x, y); };      // reversed order, negated
        auto Tflip = [&](double u, double v, double* x, double* y) { double a, b; S0(u, v, -1.0, &a, &b); S1(a, b, -1.0, x, y); };     // same order, negated: NOT the inverse
        auto det = [&](double x, double y) {
            const double h = 1e-7;
            double ux1, vx1, ux0, vx0, uy1, vy1, uy0, vy0;
            T(x + h, y, &ux1, &vx1); T(x - h, y, &ux0, &vx0); T(x, y + h, &uy1, &vy1); T(x, y - h, &uy0, &vy0);
            return ((ux1 - ux0) / (2 * h)) * ((vy1 - vy0) / (2 * h)) - ((uy1 - uy0) / (2 * h)) * ((vx1 - vx0) / (2 * h));
        };
        double worst_inv = 0.0, worst_det = 0.0, worst_flip = 0.0, moved = 0.0;
        for (int i = 0; i <= 60; i++) for (int j = 0; j <= 60; j++) {
            const double x = -0.5 + i / 60.0, y = -0.5 + j / 60.0;      // the frame is centred on the strike
            double u, v, xb, yb;
            T(x, y, &u, &v); Tinv(u, v, &xb, &yb);
            worst_inv = std::fmax(worst_inv, std::hypot(xb - x, yb - y));
            moved = std::fmax(moved, std::hypot(u - x, v - y));
            worst_det = std::fmax(worst_det, std::fabs(det(x, y) - 1.0));
            Tflip(u, v, &xb, &yb);
            worst_flip = std::fmax(worst_flip, std::hypot(xb - x, yb - y));
        }
        CHECK(worst_inv < 1e-12);      // the kick-drift inverts exactly — for this profile
        CHECK(worst_det < 1e-4);       // det J = 1 (finite differences meet the kinks to O(h))
        CHECK(moved > 0.02);           // and it does move the sheet
        CHECK(worst_flip > 1e-3);      // NEGATIVE: the same-order sign flip is not the inverse
        std::printf("  spark shear (%s): moves up to %.4f, inverse residue %.1e, |det−1| %.1e; same-order flip residue up to %.4f\n",
                    noise ? "noise" : "triangle stack", moved, worst_inv, worst_det, worst_flip);
    }
    // The episode through the mapper: kicks A = B = shear·r spent as e^{−t/τ},
    // emitted as stage-0/stage-1 pairs, summing to A_tot(1 − e^{−4}) at 4τ.
    {
        sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
        sumi_deform_queue_t* q = sumi_deform_queue_create(256);
        sumi_params_t p; std::memset(&p, 0, sizeof p);
        p.spark_shear = 0.6f; p.spark_tau = 0.1f; p.spark_stack = 3; p.spark_profile = 0; p.smoothing_ms = 30.0f;
        uint32_t counter = 0;
        sumi_voice_event_t none[1];
        const float r = 0.05f;
        CHECK(!sumi_voice_mapper_add_spark(vm, 0.5f, 0.5f, 0.0f, 0.3f, &p));    // refused: no radius
        CHECK(sumi_voice_mapper_add_spark(vm, 0.5f, 0.5f, r, 0.3f, &p));
        CHECK(sumi_voice_mapper_spark_count(vm) == 1);
        double sum_a = 0.0, first = -1.0, last = 0.0; int pairs = 0, frames_with = 0; bool paired = true, frame_ok = true;
        double t_end = 0.0;
        for (int f = 1; f <= 70; f++) {
            sumi_deform_queue_clear(q);
            sumi_voice_mapper_lower(vm, none, 0, 1.0 / 120.0, &p, true, &counter, q);
            const sumi_deform_t* prev = nullptr; bool any = false;
            for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
                const sumi_deform_t* d = sumi_deform_queue_at(q, i);
                if (d->type != SUMI_DEFORM_SPARK) continue;
                any = true;
                if (d->as.spark.stage == 0) { sum_a += d->as.spark.amp; if (first < 0) first = d->as.spark.amp; last = d->as.spark.amp; prev = d; }
                else { if (!prev || std::fabs(prev->as.spark.amp - d->as.spark.amp) > 1e-7f) paired = false; pairs++; prev = nullptr; }
                if (std::fabs(d->as.spark.band - 2.0f * r) > 1e-6f || std::fabs(d->as.spark.theta0 - 0.3f) > 1e-6f || d->as.spark.stack != 3 ||
                    std::fabs(d->as.spark.k - (SUMI_SPARK_K_MIN + 0.5f * (SUMI_SPARK_K_MAX - SUMI_SPARK_K_MIN))) > 1e-3f) frame_ok = false;
            }
            if (any) { frames_with++; t_end = f / 120.0; }
        }
        const double a_tot = 0.6 * r, expect = a_tot * (1.0 - std::exp(-t_end / 0.1));
        CHECK(paired && frame_ok && pairs >= 4);
        CHECK(std::fabs(sum_a - expect) < 1e-6);        // the exact integral of the decay, flushed at the end
        CHECK(first > last && first > 0.0);            // it decays
        CHECK(sumi_voice_mapper_spark_count(vm) == 0);
        std::printf("  spark episode: %d kick-drift steps on %d frames, kicks %.5f -> %.5f, sum %.6f (A_tot(1 − e^(−t/τ)) = %.6f), over at %.3f s\n",
                    pairs, frames_with, first, last, sum_a, expect, t_end);
        sumi_deform_queue_destroy(q);
        sumi_voice_mapper_destroy(vm);
    }
    // slide_mode 2 (prepared for the binding tables): a member channel's CC 74
    // sets the SPARK_K target; in modes 0 and 1 it does not.
    for (uint32_t mode = 0; mode < 3; mode++) {
        sumi_normalizer_t* nz = sumi_normalizer_create(nullptr, nullptr);
        sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
        sumi_deform_queue_t* q = sumi_deform_queue_create(64);
        sumi_params_t p; std::memset(&p, 0, sizeof p);
        p.slide_mode = mode; p.smoothing_ms = 1.0f; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
        sumi_midi_event_t mev[32]; sumi_voice_event_t vev[32];
        uint32_t counter = 0;
        sumi_normalizer_push(nz, 0xB0, 101, 0); sumi_normalizer_push(nz, 0xB0, 100, 6); sumi_normalizer_push(nz, 0xB0, 6, 15);   // MCM: MPE, 15 members
        sumi_normalizer_push(nz, 0x91, 60, 100);
        sumi_normalizer_push(nz, 0xB1, 74, 64);                 // the rest position (primes)
        sumi_normalizer_push(nz, 0xB1, 74, 120);                // the slide
        const uint32_t nm = sumi_normalizer_drain(nz, tnow(), mev, 32);
        const uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, mev, nm, sumi_normalizer_mode(nz), sumi_normalizer_zone(nz), &p, 1.0f, vev, 32);
        for (int f = 0; f < 60; f++) sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 1.0 / 120.0, &p, true, &counter, q), sumi_deform_queue_clear(q);
        const float kc = sumi_voice_mapper_ctl(vm, SUMI_CTL_SPARK_K);
        if (mode == 2) CHECK(std::fabs(kc - 120.0f / 127.0f) < 0.02f);
        else           CHECK(std::fabs(kc - 0.5f) < 1e-6f);
        sumi_deform_queue_destroy(q); sumi_voice_mapper_destroy(vm); sumi_normalizer_destroy(nz);
    }
}

// Phase 6 step 40 (MEDIUM §2.5): the scaled Chirikov standard map — the
// kick-drift in double (exact inverse, det J = 1, the same-order flip is not
// the inverse), the step helper's ordering, and the DELTA-driven route: a
// throw of δ is one step at δ²·K_max, capped per step by the core's erosion
// ceiling with the remainder following, a negative δ the exact inverse.
static void test_chirikov_map_and_route() {
    const double A = 0.08, k = 4.0 * 3.141592653589793, ph = 0.3, eps = 0.5;   // K = A k ε = 0.503
    auto kick  = [&](double x, double y, double s, double* u, double* v) { *u = x; *v = y + s * A * std::sin(k * x + ph); };
    auto drift = [&](double x, double y, double s, double* u, double* v) { *u = x + s * eps * y; *v = y; };
    auto T    = [&](double x, double y, double* u, double* v) { double a, b; kick(x, y, 1.0, &a, &b); drift(a, b, 1.0, u, v); };
    auto Tinv = [&](double u, double v, double* x, double* y) { double a, b; drift(u, v, -1.0, &a, &b); kick(a, b, -1.0, x, y); };
    auto Tflip = [&](double u, double v, double* x, double* y) { double a, b; kick(u, v, -1.0, &a, &b); drift(a, b, -1.0, x, y); };
    double worst_inv = 0.0, worst_det = 0.0, worst_flip = 0.0;
    for (int i = 0; i <= 40; i++) for (int j = 0; j <= 40; j++) {
        const double x = -0.5 + i / 40.0, y = -0.5 + j / 40.0, h = 1e-6;
        double u, v, xb, yb;
        T(x, y, &u, &v); Tinv(u, v, &xb, &yb);
        worst_inv = std::fmax(worst_inv, std::hypot(xb - x, yb - y));
        double ux1, vx1, ux0, vx0, uy1, vy1, uy0, vy0;
        T(x + h, y, &ux1, &vx1); T(x - h, y, &ux0, &vx0); T(x, y + h, &uy1, &vy1); T(x, y - h, &uy0, &vy0);
        const double det = ((ux1 - ux0) / (2 * h)) * ((vy1 - vy0) / (2 * h)) - ((uy1 - uy0) / (2 * h)) * ((vx1 - vx0) / (2 * h));
        worst_det = std::fmax(worst_det, std::fabs(det - 1.0));
        Tflip(u, v, &xb, &yb);
        worst_flip = std::fmax(worst_flip, std::hypot(xb - x, yb - y));
    }
    CHECK(worst_inv < 1e-12);
    CHECK(worst_det < 1e-6);
    CHECK(worst_flip > 1e-3);
    std::printf("  chirikov K = %.3f: inverse residue %.1e, |det−1| %.1e, same-order flip residue up to %.4f\n", A * k * eps, worst_inv, worst_det, worst_flip);
    // the step helper: forward = kick (stage 0) then drift (stage 1); inverse = drift (−ε) then kick (−A)
    {
        sumi_deform_queue_t* q = sumi_deform_queue_create(8);
        CHECK(sumi_chirikov_emit_step(q, 0.5f, 0.5f, 0.08f, (float)k, 0.3f, 0.5f, false) == 2);
        CHECK(sumi_chirikov_emit_step(q, 0.5f, 0.5f, 0.08f, (float)k, 0.3f, 0.5f, true) == 2);
        const sumi_deform_t* d0 = sumi_deform_queue_at(q, 0); const sumi_deform_t* d1 = sumi_deform_queue_at(q, 1);
        const sumi_deform_t* d2 = sumi_deform_queue_at(q, 2); const sumi_deform_t* d3 = sumi_deform_queue_at(q, 3);
        CHECK(d0->type == SUMI_DEFORM_CHIRIKOV && d0->as.chirikov.stage == 0 && d0->as.chirikov.amp > 0.0f);
        CHECK(d1->as.chirikov.stage == 1 && d1->as.chirikov.eps > 0.0f);
        CHECK(d2->as.chirikov.stage == 1 && d2->as.chirikov.eps < 0.0f);
        CHECK(d3->as.chirikov.stage == 0 && d3->as.chirikov.amp < 0.0f);
        sumi_deform_queue_destroy(q);
    }
    // the route: a full throw in one frame at K_max 2 is capped at the ceiling, the rest follows; the wheel down retraces
    for (int variant = 0; variant < 2; variant++) {
        sumi_normalizer_t* nz = sumi_normalizer_create(nullptr, nullptr);
        sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
        sumi_deform_queue_t* q = sumi_deform_queue_create(64);
        sumi_params_t p; std::memset(&p, 0, sizeof p);
        p.smoothing_ms = 0.01f; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
        p.chirikov_kmax = variant == 0 ? 2.0f : 0.5f; p.chirikov_periods = 2; p.chirikov_eps = 0.5f;
        sumi_voice_mapper_map_cc(vm, 0xFF, 109, SUMI_CTL_CHIRIKOV_K);
        sumi_midi_event_t mev[8]; sumi_voice_event_t vev[8];
        uint32_t counter = 0;
        auto frame = [&](int cc_value, double* K_steps, int* n_steps, int* first_stage) {
            if (cc_value >= 0) sumi_normalizer_push(nz, 0xB0, 109, (uint8_t)cc_value);
            const uint32_t nm = sumi_normalizer_drain(nz, tnow(), mev, 8);
            const uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, mev, nm, sumi_normalizer_mode(nz), sumi_normalizer_zone(nz), &p, 1.0f, vev, 8);
            sumi_deform_queue_clear(q);
            sumi_voice_mapper_lower(vm, vev, nv, 1.0 / 120.0, &p, true, &counter, q);
            *n_steps = 0; *first_stage = -1;
            double ak = 0.0, ep = 0.0; bool have_ak = false, have_ep = false;
            for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
                const sumi_deform_t* d = sumi_deform_queue_at(q, i);
                if (d->type != SUMI_DEFORM_CHIRIKOV) continue;
                if (*first_stage < 0) *first_stage = (int)d->as.chirikov.stage;
                if (d->as.chirikov.stage == 0) { ak = (double)d->as.chirikov.amp * d->as.chirikov.k; have_ak = true; }
                else                           { ep = (double)d->as.chirikov.eps; have_ep = true; }
                if (have_ak && have_ep) { K_steps[(*n_steps)++] = ak * ep; have_ak = have_ep = false; }   // |K| = A·k·ε, either order
            }
        };
        double Ks[8]; int n = 0, stage0 = -1;
        frame(127, Ks, &n, &stage0);                         // the throw: 0 -> 1 in one frame (smoothing ~0)
        if (variant == 0) {
            CHECK(n == 1 && stage0 == 0);
            CHECK_NEAR(Ks[0], (double)SUMI_CHIRIKOV_K_CEIL, 1e-3);       // capped: δ² K_max = ceiling
            frame(-1, Ks, &n, &stage0);                      // the remainder of the throw (the smoother's 1 ms floor leaves 2e-4 of it for later)
            CHECK(n == 1);
            const double rem = 1.0 - std::sqrt((double)SUMI_CHIRIKOV_K_CEIL / 2.0);
            CHECK_NEAR(Ks[0], rem * rem * 2.0, 2e-3);
            frame(-1, Ks, &n, &stage0);
            CHECK(n == 0);                                   // the throw is spent
            std::printf("  chirikov route, K_max 2: a one-frame throw = a step at the ceiling %.3f, then the remainder %.4f, then nothing\n", (double)SUMI_CHIRIKOV_K_CEIL, rem * rem * 2.0);
        } else {
            CHECK(n == 1 && stage0 == 0);
            CHECK_NEAR(Ks[0], 0.5, 1e-3);                    // under the ceiling: one step at K_max (the smoother's 1 ms floor: 0.9998² of it)
            frame(-1, Ks, &n, &stage0);
            CHECK(n == 0);
            frame(0, Ks, &n, &stage0);                       // the wheel down: the exact inverse step, drift first
            CHECK(n == 1 && stage0 == 1);
            CHECK_NEAR(Ks[0], 0.5, 1e-3);                    // (−A)·k·(−ε): the same K, the inverse ordering
            std::printf("  chirikov route, K_max 0.5: one step at 0.5; the wheel down one inverse step, drift first\n");
        }
        sumi_deform_queue_destroy(q); sumi_voice_mapper_destroy(vm); sumi_normalizer_destroy(nz);
    }
}

// Phase 6 step 42 (MEDIUM §4): the medium's DEFAULT BINDING TABLE. With the
// modes at SUMI_MODE_MEDIUM_DEFAULT, Sumi behaves as 0.x (a strike is a drop,
// pressure grows it, the bend glides, the mod wheel stirs the vortex) and
// Anod re-routes: the strike is the spark composition (drop + burst + shear
// episodes, the burst's order from the pitch class), the bend plays the
// torsion's wavenumber, the slide the spark's, pressure spends torsion
// deltas instead of ink, poly pressure sets the Chladni stir, the mod-wheel
// dimension throws the Chirikov map and the vortex stays quiet. An explicit
// mode overrides the table in either medium; the CC map overrides the stir.
struct t42_counts { int drop, burst, spark, vortex_torsion, vortex_other, chirikov, tine, swirl, chladni; };
static t42_counts t42_count(const sumi_deform_queue_t* q) {
    t42_counts c = {};
    for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
        const sumi_deform_t* d = sumi_deform_queue_at(q, i);
        switch (d->type) {
            case SUMI_DEFORM_DROP: c.drop++; break;
            case SUMI_DEFORM_BURST: c.burst++; break;
            case SUMI_DEFORM_SPARK: c.spark++; break;
            case SUMI_DEFORM_VORTEX: if (d->as.vortex.profile == SUMI_VORTEX_TORSION) c.vortex_torsion++; else c.vortex_other++; break;
            case SUMI_DEFORM_CHIRIKOV: c.chirikov++; break;
            case SUMI_DEFORM_TINE: c.tine++; break;
            case SUMI_DEFORM_SWIRL: c.swirl++; break;
            case SUMI_DEFORM_CHLADNI: case SUMI_DEFORM_CELLS: c.chladni++; break;   // step 43: the stir is the cells pass (an eddy in every display cell); the gesture keeps the two-wave lattice
            default: break;
        }
    }
    return c;
}
static void test_medium_binding_tables() {
    for (int medium = 0; medium < 3; medium++) {          // 0 sumi, 1 anod, 2 anod with explicit overrides
        const bool anod = medium >= 1, overridden = medium == 2;
        sumi_normalizer_t* nz = sumi_normalizer_create(nullptr, nullptr);
        sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
        sumi_deform_queue_t* q = sumi_deform_queue_create(256);
        sumi_params_t p; std::memset(&p, 0, sizeof p);
        p.smoothing_ms = 0.01f; p.pitch_layout = SUMI_LAYOUT_CHROMA_GRID; p.expansion_rate = 1.0f;
        p.medium = anod ? SUMI_MEDIUM_ANOD : SUMI_MEDIUM_SUMI;
        p.bend_mode = overridden ? 0u : SUMI_MODE_MEDIUM_DEFAULT;
        p.slide_mode = overridden ? 0u : SUMI_MODE_MEDIUM_DEFAULT;
        p.press_mode = overridden ? 0u : SUMI_MODE_MEDIUM_DEFAULT;
        p.burst_age = 4.0f; p.burst_life = 0.0f; p.spark_shear = 0.6f; p.spark_tau = 0.05f; p.spark_stack = 3;
        p.chirikov_kmax = 1.0f; p.chirikov_periods = 2; p.chirikov_eps = 0.5f; p.chladni_cell = 1.0f;
        p.anod_drop = 0.57f; p.burst_order = 2;   // the core's defaults: the Anod strike's charge and its burst's order (#88)
        for (int i = 0; i < 12; i++) p.burst_order_by_class[i] = (i % 2) ? 3u : 2u;
        sumi_midi_event_t mev[16]; sumi_voice_event_t vev[16];
        uint32_t counter = 0;
        auto frame = [&](std::initializer_list<std::array<uint8_t, 3>> bytes) {
            for (auto& b : bytes) sumi_normalizer_push(nz, b[0], b[1], b[2]);
            const uint32_t nm = sumi_normalizer_drain(nz, tnow(), mev, 16);
            const uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, mev, nm, sumi_normalizer_mode(nz), sumi_normalizer_zone(nz), &p, 1.0f, vev, 16);
            sumi_deform_queue_clear(q);
            sumi_voice_mapper_lower(vm, vev, nv, 1.0 / 120.0, &p, true, &counter, q);
            return t42_count(q);
        };
        frame({{0xB0, 101, 0}, {0xB0, 100, 6}, {0xB0, 6, 15}});   // MCM: MPE, 15 members
        // the strike: note 61 (C#) on channel 2 — in Anod the classic SPARK on the charge (#71, #88): the charge
        // at anod_drop of the Sumi drop, the burst's first pieces and the shear's first step in the same frame
        t42_counts s1 = frame({{0x91, 61, 100}});
        CHECK(s1.drop == 1);
        float drop_r = 0.0f;
        for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
            const sumi_deform_t* d = sumi_deform_queue_at(q, i);
            if (d->type == SUMI_DEFORM_DROP) drop_r = d->as.drop.radius;
        }
        const float sumi_r = 0.020f + 0.075f * std::sqrt(100.0f / 127.0f);
        if (anod) {
            CHECK(s1.burst >= 1 && s1.spark >= 2);
            CHECK(std::fabs(drop_r - sumi_r * p.anod_drop) < 1e-5f);   // the charge: 0.57 of the Sumi drop by default
        } else {
            CHECK(std::fabs(drop_r - sumi_r) < 1e-5f);
            CHECK(s1.burst == 0 && s1.spark == 0);
        }
        for (int f = 0; f < 30; f++) frame({});                 // the episodes run out
        // the bend: +1 semitone on the note's channel — Anod's table (step 43): the CHLADNI STIR, its distance the
        // rate and its sign the sense; the wavenumbers untouched (they are poly pressure's now). #72: the desktop
        // maps CC 106 to the stir and CC 104 to the torsion's k by default — a mapped, silent CC must not own the dim
        if (anod && !overridden) { sumi_voice_mapper_map_cc(vm, 0xFF, 106, SUMI_CTL_CHLADNI_A); sumi_voice_mapper_map_cc(vm, 0xFF, 104, SUMI_CTL_TORSION_K); }
        const float tk0 = sumi_voice_mapper_ctl(vm, SUMI_CTL_TORSION_K);
        t42_counts b1 = frame({{0xE1, 0x00, 0x50}});           // 8192 + 2048 -> +1 semitone at the default ±48 range? (the range is the normalizer's)
        int bend_cells = b1.chladni;
        for (int f = 0; f < 12; f++) { t42_counts c = frame({}); bend_cells += c.chladni; }
        const float tk1 = sumi_voice_mapper_ctl(vm, SUMI_CTL_TORSION_K);
        const float ca_bend = sumi_voice_mapper_ctl(vm, SUMI_CTL_CHLADNI_A);
        if (anod && !overridden) { CHECK(ca_bend > 0.3f); CHECK(bend_cells >= 1); CHECK(std::fabs(tk1 - tk0) < 1e-6f); CHECK(b1.tine == 0); }   // the stir turns, no glide tine
        else                     { CHECK(std::fabs(tk1 - tk0) < 1e-6f); CHECK(ca_bend < 1e-6f); }   // glide (a tine, or the drop's drag pending) — nothing else moved
        frame({{0xE1, 0x00, 0x40}});                             // bend home
        for (int f = 0; f < 40; f++) frame({});                  // the stir's smoother comes home
        // the slide: CC 74 on the note's channel
        frame({{0xB1, 74, 64}}); frame({{0xB1, 74, 120}});
        for (int f = 0; f < 5; f++) frame({});
        const float sk = sumi_voice_mapper_ctl(vm, SUMI_CTL_SPARK_K);
        if (anod && !overridden) CHECK(std::fabs(sk - 120.0f / 127.0f) < 0.02f); else CHECK(std::fabs(sk - 0.5f) < 1e-6f);
        // channel pressure: the ink feed grows the drop (Sumi / overridden) or spends torsion (Anod)
        t42_counts pr = {};
        for (int f = 0; f < 40; f++) { t42_counts c = frame({{0xD1, 110, 0}}); pr.drop += c.drop; pr.vortex_torsion += c.vortex_torsion; }
        if (anod && !overridden) { CHECK(pr.drop == 0); CHECK(pr.vortex_torsion >= 5); }
        else                     { CHECK(pr.drop >= 5); CHECK(pr.vortex_torsion == 0); }
        frame({{0xD1, 0, 0}});
        for (int f = 0; f < 20; f++) frame({});
        // poly pressure: the swirl (Sumi) or the WAVENUMBERS (Anod, step 43: torsion k from its rest at mid up; spark k too
        // unless the slide owns it — the Anod default — so only the overridden table hands it to pressure); no Chladni passes
        t42_counts pp = {};
        for (int f = 0; f < 40; f++) { t42_counts c = frame({{0xA1, 61, 100}}); pp.swirl += c.swirl; pp.chladni += c.chladni; }
        const float ca = sumi_voice_mapper_ctl(vm, SUMI_CTL_CHLADNI_A);
        const float tk_p = sumi_voice_mapper_ctl(vm, SUMI_CTL_TORSION_K), sk_p = sumi_voice_mapper_ctl(vm, SUMI_CTL_SPARK_K);
        if (anod) {
            CHECK(pp.swirl == 0); CHECK(pp.chladni == 0); CHECK(ca < 1e-6f);
            CHECK(tk_p > 0.8f);                                                    // ½ + ½·(100/127) = 0.89, smoothed over 40 frames
            if (overridden) CHECK(sk_p > 0.8f); else CHECK(std::fabs(sk_p - 120.0f / 127.0f) < 0.02f);   // the slide's, unless overridden to 0
        }
        else      { CHECK(pp.swirl >= 3); CHECK(ca < 1e-6f); CHECK(std::fabs(tk_p - 0.5f) < 1e-6f); }
        frame({{0xA1, 61, 0}});
        for (int f = 0; f < 40; f++) frame({});
        // #72: the release gives the wavenumbers back where they were (the rest, 0.5 — a knob's setting would survive)
        CHECK(std::fabs(sumi_voice_mapper_ctl(vm, SUMI_CTL_TORSION_K) - 0.5f) < 0.01f);
        // the mod wheel (CC 1 -> VORTEX_STRENGTH by the core's default map): the vortex (Sumi) or the Chirikov throw (Anod)
        t42_counts mw = {};
        for (int f = 0; f < 10; f++) { t42_counts c = frame({{0xB0, 1, (uint8_t)(f == 0 ? 127 : 127)}}); mw.vortex_other += c.vortex_other; mw.chirikov += c.chirikov; }
        if (anod) { CHECK(mw.vortex_other == 0); CHECK(mw.chirikov >= 2); }
        else      { CHECK(mw.vortex_other >= 5); CHECK(mw.chirikov == 0); }
        // #72: a note lifted while bent (the ROLI's slide-and-lift) must not stir on; a flip away from mode 4 stills it too
        frame({{0x81, 61, 0}});
        for (int f = 0; f < 10; f++) frame({});
        frame({{0x92, 64, 100}});
        frame({{0xE2, 0x00, 0x50}});
        for (int f = 0; f < 5; f++) frame({});
        const float ca_held = sumi_voice_mapper_ctl(vm, SUMI_CTL_CHLADNI_A);
        frame({{0x82, 64, 0}});
        for (int f = 0; f < 40; f++) frame({});
        const float ca_lifted = sumi_voice_mapper_ctl(vm, SUMI_CTL_CHLADNI_A);
        if (anod && !overridden) { CHECK(ca_held > 0.5f); CHECK(ca_lifted < 1e-3f); } else { CHECK(ca_held < 1e-6f); }
        frame({{0x92, 64, 100}}); frame({{0xE2, 0x00, 0x50}}); for (int f = 0; f < 5; f++) frame({});
        { const uint32_t bm = p.bend_mode; p.bend_mode = 0u; for (int f = 0; f < 40; f++) frame({}); p.bend_mode = bm; }
        if (anod && !overridden) CHECK(sumi_voice_mapper_ctl(vm, SUMI_CTL_CHLADNI_A) < 1e-3f);
        frame({{0xE2, 0x00, 0x40}}); frame({{0x82, 64, 0}});
        std::printf("  binding table %s: strike drop %d burst %d spark %d | bend stir %.2f cells %d tine %d | slide K %.3f | press drops %d torsion %d | poly swirl %d chladni %d torsion K %.2f spark K %.2f | wheel vortex %d chirikov %d\n",
                    medium == 0 ? "sumi" : medium == 1 ? "anod" : "anod+overrides", s1.drop, s1.burst, s1.spark, ca_bend, bend_cells, b1.tine, sk, pr.drop, pr.vortex_torsion, pp.swirl, pp.chladni, tk_p, sk_p, mw.vortex_other, mw.chirikov);
        sumi_deform_queue_destroy(q); sumi_voice_mapper_destroy(vm); sumi_normalizer_destroy(nz);
    }
}

static void test_swirl_routing() {
    // Normalizer: 0xA0 -> POLY_PRESSURE events.
    sumi_normalizer_t* nz = sumi_normalizer_create(nullptr, nullptr);
    sumi_midi_event_t mev[16];
    sumi_normalizer_push(nz, 0xA2, 60, 90);
    uint32_t nm = sumi_normalizer_drain(nz, tnow(), mev, 16);
    CHECK(nm == 1);
    CHECK(mev[0].kind == SUMI_MEV_POLY_PRESSURE);
    CHECK(mev[0].channel == 2 && mev[0].a == 60 && mev[0].b == 90);
    sumi_normalizer_destroy(nz);

    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_deform_queue_t* q = sumi_deform_queue_create(64);
    sumi_params_t params = default_params();
    params.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    sumi_voice_event_t vev[16];
    uint32_t drop_counter = 0;

    // Two adjacent notes -> consecutive drop counter -> opposite band parity.
    sumi_midi_event_t on1 = {SUMI_MEV_NOTE_ON, 2, 60, 100, 0.0f};
    sumi_midi_event_t on2 = {SUMI_MEV_NOTE_ON, 3, 61, 100, 0.0f};
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &on1, 1, SUMI_INPUT_MPE,
                                              default_zone(), &params, 1.0f, vev, 16);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &on2, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 16);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    sumi_deform_queue_clear(q);

    // 0xA0 on BOTH voices (note must match; a stray note is ignored).
    sumi_midi_event_t sw1 = {SUMI_MEV_POLY_PRESSURE, 2, 60, 127, 0.0f};
    sumi_midi_event_t sw2 = {SUMI_MEV_POLY_PRESSURE, 3, 61, 127, 0.0f};
    sumi_midi_event_t stray = {SUMI_MEV_POLY_PRESSURE, 2, 72, 127, 0.0f};
    sumi_midi_event_t batch[3] = {sw1, sw2, stray};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, batch, 3, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 16);
    uint32_t swirl_evs = 0;
    for (uint32_t i = 0; i < nv; i++) {
        if (vev[i].kind == SUMI_VEV_VOICE_SWIRL) swirl_evs++;
    }
    CHECK(swirl_evs == 2);   // the stray note produced nothing

    // Tick until swirl passes emit: signs opposite (parity), core_r = the
    // strike radius (velocity 100 -> 0.02 + 0.075*sqrt(100/127)).
    float s2 = 0.0f, s3 = 0.0f;
    float rc_seen = 0.0f;
    for (int f = 0; f < 60; f++) {
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, true,
                                &drop_counter, q);
        for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
            const sumi_deform_t* d = sumi_deform_queue_at(q, i);
            if (d->type != SUMI_DEFORM_SWIRL) continue;
            float px[SUMI_MAX_ECHOES], py[SUMI_MAX_ECHOES];
            sumi_layout_position(SUMI_LAYOUT_CHROMA_GRID, 60, &params, 1.0f, px, py);
            if (std::fabs(d->as.swirl.x - px[0]) < 1e-4f) s2 = d->as.swirl.strength;
            else s3 = d->as.swirl.strength;
            rc_seen = d->as.swirl.core_r;
        }
        sumi_deform_queue_clear(q);
    }
    CHECK(s2 != 0.0f && s3 != 0.0f);
    CHECK(s2 * s3 < 0.0f);   // adjacent notes counter-rotate
    CHECK_NEAR(rc_seen, 0.020f + 0.075f * std::sqrt(100.0f / 127.0f), 1e-3f);

    // press_mode = 1: a scripted 0xD0 stream becomes swirls with ZERO grow
    // passes (one consumer owns 0xD0); press_mode = 0 keeps the v1 feed.
    sumi_voice_mapper_destroy(vm);
    vm = sumi_voice_mapper_create(nullptr, nullptr);
    drop_counter = 0;
    for (int mode = 0; mode <= 1; mode++) {
        params.press_mode = (uint32_t)mode;
        sumi_midi_event_t on = {SUMI_MEV_NOTE_ON, 4, 64, 100, 0.0f};
        nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &on, 1, SUMI_INPUT_MPE,
                                         default_zone(), &params, 1.0f, vev, 16);
        sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
        sumi_deform_queue_clear(q);
        sumi_midi_event_t press = {SUMI_MEV_CHANNEL_PRESSURE, 4, 0, 120, 0.0f};
        nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &press, 1, SUMI_INPUT_MPE,
                                         default_zone(), &params, 1.0f, vev, 16);
        uint32_t grows = 0, swirls = 0;
        for (int f = 0; f < 90; f++) {
            sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, true,
                                    &drop_counter, q);
            for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
                const sumi_deform_t* d = sumi_deform_queue_at(q, i);
                if (d->type == SUMI_DEFORM_DROP && d->as.drop.phase_base >= 1.0f) grows++;
                if (d->type == SUMI_DEFORM_SWIRL) swirls++;
            }
            sumi_deform_queue_clear(q);
        }
        if (mode == 0) {
            CHECK(grows >= 1);
            CHECK(swirls == 0);
        } else {
            CHECK(grows == 0);   // the one-consumer assert
            CHECK(swirls >= 1);
        }
        sumi_midi_event_t off = {SUMI_MEV_NOTE_OFF, 4, 64, 0, 0.0f};
        nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &off, 1, SUMI_INPUT_MPE,
                                         default_zone(), &params, 1.0f, vev, 16);
        sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
        sumi_deform_queue_clear(q);
    }

    sumi_deform_queue_destroy(q);
    sumi_voice_mapper_destroy(vm);
}

// -------------------------------------------------------------------------
// Step 21 (#39): the pen's IN-CELL offset bend routes through the same
// bend_mode machinery as any per-note bend — mode 0 drags the drop (glide
// tine), mode 1 breathes the ripple — and a retrigger crossing keeps working
// in both modes.
static void test_pen_in_cell_bend_modes() {
    sumi_normalizer_t* nz = sumi_normalizer_create(nullptr, nullptr);
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_deform_queue_t* q = sumi_deform_queue_create(64);
    sumi_params_t params = default_params();
    params.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    sumi_midi_event_t mev[32];
    sumi_voice_event_t vev[32];
    uint32_t drop_counter = 0;

    // Deterministic MPE mode + ±48 range: the session config, like the shell.
    sumi_normalizer_push(nz, 0xB0, 101, 0);
    sumi_normalizer_push(nz, 0xB0, 100, 6);
    sumi_normalizer_push(nz, 0xB0, 6, 15);
    sumi_normalizer_push(nz, 0xB1, 101, 0);
    sumi_normalizer_push(nz, 0xB1, 100, 0);
    sumi_normalizer_push(nz, 0xB1, 6, 48);

    // The pen's own byte stream, via hostmpe-equivalent messages: center
    // bend, Note On, then an IN-CELL offset bend of +0.3 semitones.
    sumi_normalizer_push(nz, 0xE1, 0x00, 0x40);
    sumi_normalizer_push(nz, 0x91, 60, 100);
    const uint16_t pb03 = (uint16_t)(8192 + 51);   // +0.3 st at ±48 (170.67/st)
    sumi_normalizer_push(nz, 0xE1, (uint8_t)(pb03 & 0x7F), (uint8_t)(pb03 >> 7));

    auto pump = [&](int frames, uint32_t* tines, uint32_t* ripples) {
        *tines = 0;
        *ripples = 0;
        uint32_t nm = sumi_normalizer_drain(nz, tnow(), mev, 32);
        uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, mev, nm,
                                                  SUMI_INPUT_MPE, default_zone(),
                                                  &params, 1.0f, vev, 32);
        for (int f = 0; f < frames; f++) {
            sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, true,
                                    &drop_counter, q);
            for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
                const sumi_deform_type_t t = sumi_deform_queue_at(q, i)->type;
                if (t == SUMI_DEFORM_TINE) (*tines)++;
                if (t == SUMI_DEFORM_RIPPLE) (*ripples)++;
            }
            sumi_deform_queue_clear(q);
        }
    };

    // Mode 0 (glide): the in-cell bend drags the drop — glide tines emit.
    params.bend_mode = 0;
    params.ripple_bake = 1;
    uint32_t tines = 0, ripples = 0;
    pump(40, &tines, &ripples);
    CHECK(tines >= 1);
    CHECK(ripples == 0);

    // Mode 1 (ripple): the same in-cell wobble breathes the ripple instead —
    // amp ctl moves, no glide tines. (+0.5 st -> amp 0.5/6 ≈ 0.083.)
    params.bend_mode = 1;
    const uint16_t pb05 = (uint16_t)(8192 + 85);   // +0.5 st
    sumi_normalizer_push(nz, 0xE1, (uint8_t)(pb05 & 0x7F), (uint8_t)(pb05 >> 7));
    pump(40, &tines, &ripples);
    CHECK(tines == 0);
    CHECK(ripples >= 1);
    CHECK_NEAR(sumi_voice_mapper_ctl(vm, SUMI_CTL_RIPPLE_AMP), 0.5f / 1.5f, 0.02f);   // #66

    // A retrigger crossing (bend -> On(61) -> Off(60), the #39 idiom) works
    // in ripple mode too: the new voice's in-cell bend keeps breathing.
    const uint16_t pbm05 = (uint16_t)(8192 - 85);  // -0.5 st rel. the NEW cell
    sumi_normalizer_push(nz, 0xE1, (uint8_t)(pbm05 & 0x7F), (uint8_t)(pbm05 >> 7));
    sumi_normalizer_push(nz, 0x91, 61, 100);
    sumi_normalizer_push(nz, 0x81, 60, 0);
    pump(40, &tines, &ripples);
    CHECK(tines == 0);
    CHECK(sumi_voice_mapper_ctl(vm, SUMI_CTL_RIPPLE_AMP) > 0.05f);

    sumi_deform_queue_destroy(q);
    sumi_voice_mapper_destroy(vm);
    sumi_normalizer_destroy(nz);
}

// -------------------------------------------------------------------------
// #67 / #62: the sustain pedal never touches the canvas — in MPE it is the
// synth's pedal (the strip pad, the Pencil squeeze, the S-Pen button), and
// since #62 the classic-keyboard dip mapping is gone too: CC 64 is an
// ordinary, unmapped controller in every mode. The dip is an ABI action.
static void test_sustain_never_dips() {
    sumi_deform_queue_t* q = sumi_deform_queue_create(16);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[8];
    uint32_t drop_counter = 7;
    const sumi_input_mode_t modes[3] = {SUMI_INPUT_MPE, SUMI_INPUT_CLASSIC, SUMI_INPUT_WIND};
    for (int mi = 0; mi < 3; mi++) {
        sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
        sumi_midi_event_t seq[3] = {
            {SUMI_MEV_CC, 0, 64, 127, 0.0f}, {SUMI_MEV_CC, 0, 64, 0, 0.0f}, {SUMI_MEV_CC, 0, 64, 127, 0.0f},
        };
        uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, seq, 3, modes[mi],
                                                  default_zone(), &params, 1.0f, vev, 8);
        CHECK(nv == 0);   // no dip, no ctl: unmapped
        sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
        CHECK(sumi_deform_queue_count(q) == 0);
        CHECK(drop_counter == 7);
        sumi_voice_mapper_destroy(vm);
    }
    sumi_deform_queue_destroy(q);
}

// #60: MPE is the default input mode, so a plain keyboard on the master
// channel must keep its chords — non-member notes are per-(channel, note)
// voices, while member channels keep the newest-steals MPE identity.
static void test_mpe_master_channel_keyboard() {
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[16];
    sumi_midi_event_t chord[3] = {
        {SUMI_MEV_NOTE_ON, 0, 60, 100, 0.0f},
        {SUMI_MEV_NOTE_ON, 0, 64, 100, 0.0f},
        {SUMI_MEV_NOTE_ON, 0, 67, 100, 0.0f},
    };
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, chord, 3, SUMI_INPUT_MPE,
                                              default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 3);   // three voices, no steals
    for (uint32_t i = 0; i < nv; i++) {
        CHECK(vev[i].kind == SUMI_VEV_VOICE_BEGIN);
        CHECK(vev[i].voice_id == (0x1000u | chord[i].a));   // classic id on ch 0
    }
    // Releasing the middle note ends exactly that voice.
    sumi_midi_event_t off = {SUMI_MEV_NOTE_OFF, 0, 64, 40, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &off, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_END && vev[0].voice_id == (0x1000u | 64u));
    // Master-channel bend is still the global shear; sustain stays musical (no dip).
    sumi_midi_event_t bend = {SUMI_MEV_BEND, 0, 0, 0, 1.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &bend, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_GLOBAL_BEND);
    sumi_midi_event_t sus = {SUMI_MEV_CC, 0, 64, 127, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &sus, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 0);
    // A member channel keeps MPE identity: the second note steals the first.
    sumi_midi_event_t m1 = {SUMI_MEV_NOTE_ON, 2, 60, 100, 0.0f};
    sumi_midi_event_t m2 = {SUMI_MEV_NOTE_ON, 2, 62, 100, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &m1, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].voice_id == 2);
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &m2, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 2 && vev[0].kind == SUMI_VEV_VOICE_END && vev[1].kind == SUMI_VEV_VOICE_BEGIN);
    sumi_voice_mapper_destroy(vm);
}

// #60: wind mode reads the expression layer on its single brush — an IMU
// wind controller's CC 74, poly pressure and (on a member channel) bend —
// while breath keeps driving the width and a single-channel bend stays the
// global shear.
static void test_wind_expression_layer() {
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[16];
    // Single-channel wind controller (ch 0 = master): note, breath, CC 74, 0xA0.
    sumi_midi_event_t on = {SUMI_MEV_NOTE_ON, 0, 60, 90, 0.0f};
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &on, 1, SUMI_INPUT_WIND,
                                              default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_BEGIN && vev[0].voice_id == 0);
    sumi_midi_event_t layer[3] = {
        {SUMI_MEV_CC, 0, 2, 100, 0.0f},            // breath -> press (width)
        {SUMI_MEV_CC, 0, 74, 64, 0.0f},            // slide layer
        {SUMI_MEV_POLY_PRESSURE, 0, 60, 80, 0.0f}, // swirl layer
    };
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, layer, 3, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    bool press = false, slide = false, swirl = false;
    for (uint32_t i = 0; i < nv; i++) {
        CHECK(vev[i].voice_id == 0);
        if (vev[i].kind == SUMI_VEV_VOICE_PRESS) press = true;
        if (vev[i].kind == SUMI_VEV_VOICE_SLIDE) { slide = true; CHECK_NEAR(vev[i].value, 64.0f / 127.0f, 1e-4f); }
        if (vev[i].kind == SUMI_VEV_VOICE_SWIRL) { swirl = true; CHECK_NEAR(vev[i].value, 80.0f / 127.0f, 1e-4f); }
    }
    CHECK(press && slide && swirl);
    // A bend on the single (master) channel stays the global shear...
    sumi_midi_event_t bend0 = {SUMI_MEV_BEND, 0, 0, 0, 0.5f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &bend0, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_GLOBAL_BEND);
    sumi_voice_mapper_destroy(vm);

    // ...while an MPE wind controller (note on member ch 2) glides the brush.
    vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_midi_event_t on2 = {SUMI_MEV_NOTE_ON, 2, 60, 90, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &on2, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].voice_id == 0);
    sumi_midi_event_t bend2 = {SUMI_MEV_BEND, 2, 0, 0, 3.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &bend2, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_GLIDE && vev[0].voice_id == 0);
    CHECK_NEAR(vev[0].value, 3.0f, 1e-5f);
    // Legato to another note: wake, silent end, new strike (#63).
    sumi_midi_event_t on3 = {SUMI_MEV_NOTE_ON, 2, 67, 90, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &on3, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 3 && vev[0].kind == SUMI_VEV_VOICE_MIGRATE && vev[1].kind == SUMI_VEV_VOICE_END &&
          vev[2].kind == SUMI_VEV_VOICE_BEGIN);
    sumi_voice_mapper_destroy(vm);
}

static void test_classic_mapping_to_deforms() {
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_deform_queue_t* q = sumi_deform_queue_create(64);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[16];
    uint32_t drop_counter = 0;

    // Note on -> VoiceBegin -> ink drop with sqrt(strike) radius.
    sumi_midi_event_t note = {SUMI_MEV_NOTE_ON, 0, 60, 127, 0.0f};
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &note, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1);
    CHECK(vev[0].kind == SUMI_VEV_VOICE_BEGIN);
    CHECK_NEAR(vev[0].value, 1.0f, 1e-4f);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 1);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_DROP);
    CHECK(sumi_deform_queue_at(q, 0)->as.drop.phase_base >= 1.0f);   // ink, not water
    CHECK_NEAR(sumi_deform_queue_at(q, 0)->as.drop.radius, 0.020f + 0.075f, 1e-4f);
    CHECK(drop_counter == 1);
    sumi_deform_queue_clear(q);

    // Velocity 32 (1/4 of 127ish) -> radius scales with sqrt.
    note.b = 32;
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &note, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    CHECK_NEAR(sumi_deform_queue_at(q, 0)->as.drop.radius,
               0.020f + 0.075f * std::sqrt(32.0f / 127.0f), 1e-4f);
    sumi_deform_queue_clear(q);

    // Global bend -> one shear tine on the delta.
    sumi_midi_event_t bend = {SUMI_MEV_BEND, 0, 0, 0, 1.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &bend, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_GLOBAL_BEND);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 1);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_TINE);
    CHECK_NEAR(sumi_deform_queue_at(q, 0)->as.tine.magnitude, 0.015f, 1e-5f);
    sumi_deform_queue_clear(q);

    // Same bend again -> no delta -> no tine.
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &bend, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 0);

    // Mod wheel -> coalesced to ONE vortex per update even for many CC1s.
    sumi_midi_event_t mods[3] = {
        {SUMI_MEV_CC, 0, 1, 10, 0.0f},
        {SUMI_MEV_CC, 0, 1, 60, 0.0f},
        {SUMI_MEV_CC, 0, 1, 127, 0.0f},
    };
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, mods, 3, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1);
    CHECK(vev[0].kind == SUMI_VEV_GLOBAL_CTL && vev[0].dimension == SUMI_CTL_VORTEX_STRENGTH);
    CHECK_NEAR(vev[0].value, 1.0f, 1e-4f);   // last one wins
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 1);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_VORTEX);
    sumi_deform_queue_clear(q);

    // CC 64 (#62): no paper dip in classic mode any more — nothing at all.
    sumi_midi_event_t sus_on  = {SUMI_MEV_CC, 0, 64, 127, 0.0f};
    sumi_midi_event_t sus_off = {SUMI_MEV_CC, 0, 64, 0, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &sus_on, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 0);
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &sus_off, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 0);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
        CHECK(sumi_deform_queue_at(q, i)->type != SUMI_DEFORM_RESET);   // (the mod-wheel vortex still streams)
    }

    sumi_deform_queue_destroy(q);
    sumi_voice_mapper_destroy(vm);
}


// -------------------------------------------------------------------------
static void test_mpe_zone_and_bend_range() {
    sumi_normalizer_t* n = sumi_normalizer_create(nullptr, nullptr);
    sumi_midi_event_t ev[16];

    // MCM on ch 1: lower zone with 5 members; forces MPE mode.
    sumi_normalizer_push(n, 0xB0, 101, 0);
    sumi_normalizer_push(n, 0xB0, 100, 6);
    sumi_normalizer_push(n, 0xB0, 6, 5);
    sumi_normalizer_drain(n, tnow(), ev, 16);
    sumi_mpe_zone_t z = sumi_normalizer_zone(n);
    CHECK(z.master == 0 && z.first_member == 1 && z.member_count == 5);
    CHECK(sumi_normalizer_mode(n) == SUMI_INPUT_MPE);

    // Upper-zone MCM (ch 16): log-and-ignore, zone unchanged (v1 single zone).
    sumi_normalizer_push(n, 0xBF, 101, 0);
    sumi_normalizer_push(n, 0xBF, 100, 6);
    sumi_normalizer_push(n, 0xBF, 6, 7);
    sumi_normalizer_drain(n, tnow(), ev, 16);
    z = sumi_normalizer_zone(n);
    CHECK(z.first_member == 1 && z.member_count == 5);

    // Member-channel bend defaults to ±48 in MPE mode (§2.1)...
    sumi_normalizer_push(n, 0xE2, 0x7F, 0x7F);   // ch 3 (member), max up
    CHECK(sumi_normalizer_drain(n, tnow(), ev, 16) == 1);
    CHECK_NEAR(ev[0].f, 48.0f * 8191.0f / 8192.0f, 1e-3f);
    // ...master stays ±2...
    sumi_normalizer_push(n, 0xE0, 0x7F, 0x7F);
    CHECK(sumi_normalizer_drain(n, tnow(), ev, 16) == 1);
    CHECK_NEAR(ev[0].f, 2.0f * 8191.0f / 8192.0f, 1e-4f);
    // ...and explicit RPN 0 on a member channel wins over the ±48 default.
    sumi_normalizer_push(n, 0xB2, 101, 0);
    sumi_normalizer_push(n, 0xB2, 100, 0);
    sumi_normalizer_push(n, 0xB2, 6, 12);
    sumi_normalizer_push(n, 0xE2, 0x7F, 0x7F);
    uint32_t cnt = sumi_normalizer_drain(n, tnow(), ev, 16);
    CHECK_NEAR(ev[cnt - 1].f, 12.0f * 8191.0f / 8192.0f, 1e-3f);
    sumi_normalizer_destroy(n);
}

// -------------------------------------------------------------------------
static void test_mpe_voice_steal_and_coalescing() {
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[32];

    // Steal: two note-ons on the same member channel -> End(steal) between Begins.
    sumi_midi_event_t steal_seq[2] = {
        {SUMI_MEV_NOTE_ON, 2, 60, 100, 0.0f},
        {SUMI_MEV_NOTE_ON, 2, 64, 90, 0.0f},
    };
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, steal_seq, 2, SUMI_INPUT_MPE,
                                              default_zone(), &params, 1.0f, vev, 32);
    CHECK(nv == 3);
    CHECK(vev[0].kind == SUMI_VEV_VOICE_BEGIN && vev[0].voice_id == 2);
    CHECK(vev[1].kind == SUMI_VEV_VOICE_END && vev[1].voice_id == 2);
    CHECK_NEAR(vev[1].value, 0.0f, 1e-6f);   // stolen, not lifted
    CHECK(vev[2].kind == SUMI_VEV_VOICE_BEGIN && vev[2].voice_id == 2);

    // Note-off for the stolen note (60) is ignored; off for the owner (64) ends.
    sumi_midi_event_t offs[2] = {
        {SUMI_MEV_NOTE_OFF, 2, 60, 30, 0.0f},
        {SUMI_MEV_NOTE_OFF, 2, 64, 80, 0.0f},
    };
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, offs, 2, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 32);
    CHECK(nv == 1);
    CHECK(vev[0].kind == SUMI_VEV_VOICE_END);
    CHECK_NEAR(vev[0].value, 80.0f / 127.0f, 1e-4f);

    // Coalescing (§3.4): a dense burst on one voice -> ONE event per dimension,
    // last value wins.
    sumi_midi_event_t burst[8] = {
        {SUMI_MEV_NOTE_ON, 3, 60, 100, 0.0f},
        {SUMI_MEV_CHANNEL_PRESSURE, 3, 0, 10, 0.0f},
        {SUMI_MEV_CHANNEL_PRESSURE, 3, 0, 60, 0.0f},
        {SUMI_MEV_CHANNEL_PRESSURE, 3, 0, 120, 0.0f},
        {SUMI_MEV_BEND, 3, 0, 0, 1.0f},
        {SUMI_MEV_BEND, 3, 0, 0, 3.0f},
        {SUMI_MEV_CC, 3, 74, 20, 0.0f},
        {SUMI_MEV_CC, 3, 74, 90, 0.0f},
    };
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, burst, 8, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 32);
    CHECK(nv == 4);   // Begin + one glide + one press + one slide
    int glides = 0, presses = 0, slides = 0;
    for (uint32_t i = 0; i < nv; i++) {
        if (vev[i].kind == SUMI_VEV_VOICE_GLIDE) { glides++; CHECK_NEAR(vev[i].value, 3.0f, 1e-6f); }
        if (vev[i].kind == SUMI_VEV_VOICE_PRESS) { presses++; CHECK_NEAR(vev[i].value, 120.0f / 127.0f, 1e-4f); }
        if (vev[i].kind == SUMI_VEV_VOICE_SLIDE) { slides++; CHECK_NEAR(vev[i].value, 90.0f / 127.0f, 1e-4f); }
    }
    CHECK(glides == 1 && presses == 1 && slides == 1);
    sumi_voice_mapper_destroy(vm);
}

// -------------------------------------------------------------------------
static void test_mpe_press_feed_and_glide() {
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_deform_queue_t* q = sumi_deform_queue_create(256);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[16];
    uint32_t drop_counter = 0;

    // Begin a voice on ch 2 and press hard.
    sumi_midi_event_t seq[2] = {
        {SUMI_MEV_NOTE_ON, 2, 60, 100, 0.0f},
        {SUMI_MEV_CHANNEL_PRESSURE, 2, 0, 127, 0.0f},
    };
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, seq, 2, SUMI_INPUT_MPE,
                                              default_zone(), &params, 1.0f, vev, 16);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) >= 1);
    const float strike_phase = sumi_deform_queue_at(q, 0)->as.drop.phase_base;
    const float strike_x = sumi_deform_queue_at(q, 0)->as.drop.x;
    sumi_deform_queue_clear(q);

    // §4.4: sustained pressure re-emits small expansions each frame at the
    // voice center, SAME ink band as the strike (the drop grows, no new ring).
    uint32_t expansions = 0;
    for (int f = 0; f < 30; f++) {
        sumi_voice_mapper_lower(vm, vev, 0, 0.016, &params, true, &drop_counter, q);
        for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
            const sumi_deform_t* d = sumi_deform_queue_at(q, i);
            CHECK(d->type == SUMI_DEFORM_DROP);
            CHECK_NEAR(d->as.drop.phase_base, strike_phase, 1e-6f);
            CHECK_NEAR(d->as.drop.x, strike_x, 1e-4f);
            CHECK(d->as.drop.radius > 0.0f && d->as.drop.radius <= 0.05f);
            expansions++;
        }
        sumi_deform_queue_clear(q);
    }
    CHECK(expansions >= 10);   // continuous feed, not a one-off
    CHECK(drop_counter == 1);  // expansions do NOT advance the drop counter

    // Glide: bend the voice; expect a narrow per-voice tine, then the center
    // moves (a later glide back emits a tine from the NEW position).
    sumi_midi_event_t bend = {SUMI_MEV_BEND, 2, 0, 0, 4.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &bend, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 16);
    float moved_x = 0.0f;
    bool saw_tine = false;
    for (int f = 0; f < 60; f++) {
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, true, &drop_counter, q);
        for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
            const sumi_deform_t* d = sumi_deform_queue_at(q, i);
            if (d->type == SUMI_DEFORM_TINE) {
                saw_tine = true;
                CHECK_NEAR(d->as.tine.alpha, 0.030f, 1e-6f);   // narrow, never a shear
                moved_x = d->as.tine.x1;
            }
        }
        sumi_deform_queue_clear(q);
    }
    CHECK(saw_tine);
    CHECK(std::fabs(moved_x - strike_x) > 0.01f);   // the drop's center was dragged

    sumi_deform_queue_destroy(q);
    sumi_voice_mapper_destroy(vm);
}

// -------------------------------------------------------------------------
static void test_deform_budget_merging() {
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_deform_queue_t* q = sumi_deform_queue_create(256);
    sumi_params_t params = default_params();
    params.expansion_rate = 2.0f;
    sumi_voice_event_t vev[64];
    uint32_t drop_counter = 0;

    // 6 voices, all pressing hard.
    sumi_midi_event_t seq[12];
    for (int i = 0; i < 6; i++) {
        seq[2 * i]     = {SUMI_MEV_NOTE_ON, (uint8_t)(1 + i), (uint8_t)(50 + i), 100, 0.0f};
        seq[2 * i + 1] = {SUMI_MEV_CHANNEL_PRESSURE, (uint8_t)(1 + i), 0, 127, 0.0f};
    }
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, seq, 12, SUMI_INPUT_MPE,
                                              default_zone(), &params, 1.0f, vev, 64);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);   // strikes land
    sumi_deform_queue_clear(q);

    // Cap the budget below the number of pressing voices: per frame only
    // `budget` expansions may emit; the rest MERGE into later frames.
    sumi_voice_mapper_set_budget(vm, 3);
    // Big dt so every voice's feed accumulator exceeds the emission threshold.
    sumi_voice_mapper_lower(vm, vev, 0, 0.05, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 3);          // budget respected
    CHECK(sumi_voice_mapper_merged_count(vm) >= 3);  // the other voices merged
    sumi_deform_queue_clear(q);

    // Next frame: the merged (pending) feeds emit — nothing was lost.
    sumi_voice_mapper_lower(vm, vev, 0, 0.05, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 3);
    sumi_deform_queue_clear(q);

    // Raising the budget lets all 6 emit in one frame again.
    sumi_voice_mapper_set_budget(vm, 64);
    sumi_voice_mapper_lower(vm, vev, 0, 0.05, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 6);

    sumi_deform_queue_destroy(q);
    sumi_voice_mapper_destroy(vm);
}

// -------------------------------------------------------------------------
static void test_mpe_lift_ring_and_slide_aux() {
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_deform_queue_t* q = sumi_deform_queue_create(64);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[16];
    uint32_t drop_counter = 0;

    // Voice with slide at max: subsequent feed expansions carry aux offset.
    sumi_midi_event_t seq[3] = {
        {SUMI_MEV_NOTE_ON, 4, 72, 100, 0.0f},
        {SUMI_MEV_CC, 4, 74, 127, 0.0f},
        {SUMI_MEV_CHANNEL_PRESSURE, 4, 0, 127, 0.0f},
    };
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, seq, 3, SUMI_INPUT_MPE,
                                              default_zone(), &params, 1.0f, vev, 16);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    const float aux_base = sumi_deform_queue_at(q, 0)->as.drop.aux;
    sumi_deform_queue_clear(q);
    for (int f = 0; f < 40; f++) {   // let slide smoothing converge, collect a feed
        sumi_voice_mapper_lower(vm, vev, 0, 0.016, &params, true, &drop_counter, q);
    }
    CHECK(sumi_deform_queue_count(q) > 0);
    const sumi_deform_t* feed = sumi_deform_queue_at(q, sumi_deform_queue_count(q) - 1);
    CHECK(feed->as.drop.aux > aux_base + 0.5f);   // slide -> aux modulation
    sumi_deform_queue_clear(q);

    // Lift (#41): NO ring — the drop simply sets, nothing is stamped, even
    // at maximum release velocity.
    sumi_midi_event_t off = {SUMI_MEV_NOTE_OFF, 4, 72, 127, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &off, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 16);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 0);

    // The voice is gone: further frames emit nothing.
    sumi_deform_queue_clear(q);
    sumi_voice_mapper_lower(vm, vev, 0, 0.016, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 0);

    sumi_deform_queue_destroy(q);
    sumi_voice_mapper_destroy(vm);
}


// -------------------------------------------------------------------------
static void test_wind_mode_wake_legato() {
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_deform_queue_t* q = sumi_deform_queue_create(512);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[16];
    uint32_t drop_counter = 0;

    // First note: a full MPE-sized strike drop (#63: no thin brush touch-down).
    sumi_midi_event_t on1 = {SUMI_MEV_NOTE_ON, 0, 60, 90, 0.0f};
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &on1, 1, SUMI_INPUT_WIND,
                                              default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_BEGIN && vev[0].voice_id == 0);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 1);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_DROP);
    const float r0 = sumi_deform_queue_at(q, 0)->as.drop.radius;
    CHECK_NEAR(r0, 0.020f + 0.075f * std::sqrt(90.0f / 127.0f), 1e-4f);
    const float phase0 = sumi_deform_queue_at(q, 0)->as.drop.phase_base;
    sumi_deform_queue_clear(q);

    // Legato note change: the old drop is WAKED to the new site (rigid tip of
    // its radius, <= a/4 sub-steps), the old voice ends silently, and the new
    // note strikes a new drop — exactly the MPE same-channel hand-over plus
    // the wake.
    sumi_midi_event_t on2 = {SUMI_MEV_NOTE_ON, 0, 67, 90, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &on2, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 3);
    CHECK(vev[0].kind == SUMI_VEV_VOICE_MIGRATE && vev[0].voice_id == 0);
    CHECK(vev[1].kind == SUMI_VEV_VOICE_END && vev[1].voice_id == 0 && vev[1].value == 0.0f);
    CHECK(vev[2].kind == SUMI_VEV_VOICE_BEGIN && vev[2].voice_id == 0);
    float p60x[SUMI_MAX_ECHOES], p60y[SUMI_MAX_ECHOES], p67x[SUMI_MAX_ECHOES], p67y[SUMI_MAX_ECHOES];
    sumi_layout_position(0, 60, &params, 1.0f, p60x, p60y);
    sumi_layout_position(0, 67, &params, 1.0f, p67x, p67y);
    CHECK_NEAR(vev[0].ax, p67x[0] - p60x[0], 1e-6f);   // aspect 1: displacement as is
    CHECK_NEAR(vev[0].ay, p67y[0] - p60y[0], 1e-6f);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    const uint32_t n = sumi_deform_queue_count(q);
    CHECK(n >= 3);
    uint32_t wakes = 0;
    float sum_dx = 0.0f, sum_dy = 0.0f;
    for (uint32_t i = 0; i + 1 < n; i++) {
        const sumi_deform_t* d = sumi_deform_queue_at(q, i);
        CHECK(d->type == SUMI_DEFORM_WAKE);            // default profile: the doublet
        CHECK_NEAR(d->as.wake.tip_radius, r0, 1e-6f);  // the old drop is the tip
        const float step = std::sqrt(d->as.wake.dx_ac * d->as.wake.dx_ac + d->as.wake.dy_ac * d->as.wake.dy_ac);
        CHECK(step <= r0 * 0.25f + 1e-6f);             // <= a/4 sub-steps
        sum_dx += d->as.wake.dx_ac; sum_dy += d->as.wake.dy_ac;
        wakes++;
    }
    CHECK(wakes >= 2);
    CHECK_NEAR(sum_dx, p67x[0] - p60x[0], 1e-4f);      // the sub-steps add up to the move
    CHECK_NEAR(sum_dy, p67y[0] - p60y[0], 1e-4f);
    const sumi_deform_t* last = sumi_deform_queue_at(q, n - 1);
    CHECK(last->type == SUMI_DEFORM_DROP);              // ...then the new strike
    CHECK_NEAR(last->as.drop.x, p67x[0], 1e-6f);
    CHECK_NEAR(last->as.drop.y, p67y[0], 1e-6f);
    CHECK(last->as.drop.phase_base != phase0);          // a new band: a new drop
    CHECK(drop_counter == 2);
    sumi_deform_queue_clear(q);

    // The off of the OLD note (legato overlap) is ignored.
    sumi_midi_event_t off_old = {SUMI_MEV_NOTE_OFF, 0, 60, 40, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &off_old, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 0);

    // Breath (CC 2 via the default map) feeds the voice, UNBOUNDED (#63): the
    // nominal radius keeps growing past the old brush width (0.056) at full
    // breath, feed drops land at the new note with the new band.
    sumi_midi_event_t breath = {SUMI_MEV_CC, 0, 2, 127, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &breath, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_PRESS && vev[0].voice_id == 0);
    const float phase1 = last->as.drop.phase_base;
    uint32_t feeds = 0;
    for (int f = 0; f < 240; f++) {   // 4 s of full breath at 60 Hz
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 1.0 / 60.0, &params, true, &drop_counter, q);
        for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
            const sumi_deform_t* d = sumi_deform_queue_at(q, i);
            if (d->type != SUMI_DEFORM_DROP) continue;
            CHECK_NEAR(d->as.drop.phase_base, phase1, 1e-6f);
            CHECK_NEAR(d->as.drop.x, p67x[0], 1e-3f);
            CHECK_NEAR(d->as.drop.y, p67y[0], 1e-3f);
            feeds++;
        }
        sumi_deform_queue_clear(q);
    }
    CHECK(feeds >= 10);
    CHECK(sumi_voice_mapper_voice_radius(vm, 0) > 0.056f + 0.02f);   // past the retired width clamp

    // Channel pressure: press_mode 0 -> press, as MPE (#63).
    sumi_midi_event_t at = {SUMI_MEV_CHANNEL_PRESSURE, 0, 0, 100, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &at, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_PRESS);
    params.press_mode = 1;
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &at, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_SWIRL);
    params.press_mode = 0;

    // Off of the CURRENT note ends the voice.
    sumi_midi_event_t off_cur = {SUMI_MEV_NOTE_OFF, 0, 67, 50, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &off_cur, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_END);

    sumi_deform_queue_destroy(q);
    sumi_voice_mapper_destroy(vm);
}

// -------------------------------------------------------------------------
static void test_cc_routing_table() {
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[16];

    // Default map (DECISIONS_4 #50, measured Airwave): CC26 (Raise L) -> vortex strength.
    sumi_midi_event_t cc20 = {SUMI_MEV_CC, 0, 26, 127, 0.0f};
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &cc20, 1, SUMI_INPUT_CLASSIC,
                                              default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_GLOBAL_CTL);
    CHECK(vev[0].dimension == SUMI_CTL_VORTEX_STRENGTH);

    // Unmapped CC30 (Airwave Flex L, free by default since #69) does nothing...
    sumi_midi_event_t cc30 = {SUMI_MEV_CC, 0, 30, 100, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &cc30, 1, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 0);
    // ...until mapped at runtime (§5.3).
    sumi_voice_mapper_map_cc(vm, 0xFF, 30, SUMI_CTL_VORTEX_STRENGTH);
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &cc30, 1, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].dimension == SUMI_CTL_VORTEX_STRENGTH);
    CHECK_NEAR(vev[0].value, 100.0f / 127.0f, 1e-4f);

    // #69 default routes: the right hand's swirl trio and the two grasps.
    sumi_midi_event_t rhand[4] = {
        {SUMI_MEV_CC, 0, 27, 127, 0.0f},  // Raise R -> swirl strength
        {SUMI_MEV_CC, 0, 25, 96, 0.0f},   // Glide R -> swirl X
        {SUMI_MEV_CC, 0, 20, 64, 0.0f},   // Grasp L -> saddle pinch
        {SUMI_MEV_CC, 0, 21, 64, 0.0f},   // Grasp R -> crossed pinch
    };
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, rhand, 4, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 4);
    CHECK(vev[0].dimension == SUMI_CTL_SWIRL_STRENGTH);
    CHECK(vev[1].dimension == SUMI_CTL_SWIRL_X);
    CHECK(vev[2].dimension == SUMI_CTL_PINCH_SADDLE);
    CHECK(vev[3].dimension == SUMI_CTL_PINCH_CROSS);

    // Channel-specific mapping overrides any-channel.
    sumi_voice_mapper_map_cc(vm, 3, 30, SUMI_CTL_VISCOSITY);
    sumi_midi_event_t cc30ch3 = {SUMI_MEV_CC, 3, 30, 64, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &cc30ch3, 1, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].dimension == SUMI_CTL_VISCOSITY);

    // Multiple dimensions coalesce independently in one update.
    sumi_midi_event_t multi[3] = {
        {SUMI_MEV_CC, 0, 26, 127, 0.0f},  // vortex strength (Raise L)
        {SUMI_MEV_CC, 0, 24, 96, 0.0f},   // vortex X (Glide L)
        {SUMI_MEV_CC, 0, 29, 32, 0.0f},   // ripple amount (Tilt R, #69)
    };
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, multi, 3, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 3);

    // clear_cc_map removes everything, defaults included.
    sumi_voice_mapper_clear_cc_map(vm);
    sumi_midi_event_t cc1 = {SUMI_MEV_CC, 0, 1, 127, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &cc1, 1, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 0);

    sumi_voice_mapper_destroy(vm);
}

// -------------------------------------------------------------------------
static void test_global_ctl_vortex_and_viscosity() {
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_deform_queue_t* q = sumi_deform_queue_create(64);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[16];
    uint32_t drop_counter = 0;

    // Vortex strength + center via GlobalCtl: per-frame dt-scaled passes at
    // the routed center.
    sumi_midi_event_t ccs[3] = {
        {SUMI_MEV_CC, 0, 26, 127, 0.0f},   // strength 1.0 (Raise L)
        {SUMI_MEV_CC, 0, 24, 127, 0.0f},   // center x -> 1.0 (Glide L)
        {SUMI_MEV_CC, 0, 22, 0, 0.0f},     // center y: CC 0 -> BOTTOM (#69 reversed)
    };
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, ccs, 3, SUMI_INPUT_CLASSIC,
                                              default_zone(), &params, 1.0f, vev, 16);
    float theta_low_visc = 0.0f;
    for (int f = 0; f < 40; f++) {   // let smoothing converge
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, true, &drop_counter, q);
        for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
            const sumi_deform_t* d = sumi_deform_queue_at(q, i);
            CHECK(d->type == SUMI_DEFORM_VORTEX);
            theta_low_visc = d->as.vortex.strength;
            CHECK(d->as.vortex.x > 0.6f);   // center followed CC24
            CHECK(d->as.vortex.y > 0.6f);   // #69: reversed — hand low = centre low
        }
        sumi_deform_queue_clear(q);
    }
    CHECK(theta_low_visc > 0.05f);   // ~ 1.0 * 6 rad/s * 16 ms

    // High viscosity damps the same vortex strength. Viscosity has no Airwave
    // route since #69 — route a CC to it, the editor's path.
    sumi_voice_mapper_map_cc(vm, 0xFF, 40, SUMI_CTL_VISCOSITY);
    sumi_midi_event_t visc = {SUMI_MEV_CC, 0, 40, 127, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &visc, 1, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 16);
    float theta_high_visc = 0.0f;
    for (int f = 0; f < 40; f++) {
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, true, &drop_counter, q);
        for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
            theta_high_visc = sumi_deform_queue_at(q, i)->as.vortex.strength;
        }
        sumi_deform_queue_clear(q);
    }
    CHECK(theta_high_visc < theta_low_visc * 0.35f);   // damped hard

    sumi_deform_queue_destroy(q);
    sumi_voice_mapper_destroy(vm);
}


// -------------------------------------------------------------------------
// v0.9 #69: the right hand's Lamb-Oseen stir and the two Grasp pinches as
// global controls — the swirl mirrors the vortex agitation (own centre,
// reversed Y), the pinches are delta-driven (a squeeze-and-release emits +k
// then -k; holding still emits nothing).
static void test_global_ctl_swirl_and_pinches() {
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_deform_queue_t* q = sumi_deform_queue_create(64);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[16];
    uint32_t drop_counter = 0;

    // Raise R full, Glide R right, Slide R HIGH (CC 127 -> top of the canvas).
    sumi_midi_event_t ccs[3] = {
        {SUMI_MEV_CC, 0, 27, 127, 0.0f},
        {SUMI_MEV_CC, 0, 25, 127, 0.0f},
        {SUMI_MEV_CC, 0, 23, 127, 0.0f},
    };
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, ccs, 3, SUMI_INPUT_CLASSIC,
                                              default_zone(), &params, 1.0f, vev, 16);
    float S = 0.0f, rc = 0.0f, sx = 0.0f, sy = 1.0f;
    int swirl_passes = 0;
    for (int f = 0; f < 40; f++) {
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, true, &drop_counter, q);
        for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
            const sumi_deform_t* d = sumi_deform_queue_at(q, i);
            CHECK(d->type == SUMI_DEFORM_SWIRL);
            swirl_passes++;
            S = d->as.swirl.strength;
            rc = d->as.swirl.core_r;
            sx = d->as.swirl.x;
            sy = d->as.swirl.y;
        }
        sumi_deform_queue_clear(q);
    }
    CHECK(swirl_passes > 10);
    CHECK(rc > 0.05f && rc < 0.5f);
    // S = theta_core * 2pi * rc^2 with theta ~ 3 rad/s * 16 ms at full ctl.
    CHECK_NEAR(S / (6.2831853f * rc * rc), 3.0f * 0.016f, 0.02f);
    CHECK(sx > 0.6f);                     // centre followed Glide R
    CHECK(sy < 0.4f);                     // #69: CC high = centre TOP (y small)

    // Grasp L (saddle): a squeeze emits +k once at the LEFT hand's centre;
    // holding emits nothing more; release emits -k.
    sumi_midi_event_t squeeze = {SUMI_MEV_CC, 0, 20, 127, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &squeeze, 1, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 16);
    float k_sum = 0.0f;
    int pinch_passes = 0;
    for (int f = 0; f < 60; f++) {
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, true, &drop_counter, q);
        for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
            const sumi_deform_t* d = sumi_deform_queue_at(q, i);
            if (d->type != SUMI_DEFORM_PINCH) continue;   // swirl still stirring
            pinch_passes++;
            k_sum += d->as.pinch.k;
            CHECK_NEAR(d->as.pinch.x, 0.5f, 0.05f);   // vortex centre at rest
            CHECK_NEAR(d->as.pinch.y, 0.5f, 0.05f);
        }
        sumi_deform_queue_clear(q);
    }
    CHECK(pinch_passes > 0);
    CHECK_NEAR(k_sum, 1.2f, 0.05f);       // PINCH_K_SCALE at full squeeze
    // Release: the deltas retrace to ~0 net.
    sumi_midi_event_t release = {SUMI_MEV_CC, 0, 20, 0, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &release, 1, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 16);
    for (int f = 0; f < 60; f++) {
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, true, &drop_counter, q);
        for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
            const sumi_deform_t* d = sumi_deform_queue_at(q, i);
            if (d->type == SUMI_DEFORM_PINCH) k_sum += d->as.pinch.k;
        }
        sumi_deform_queue_clear(q);
    }
    CHECK_NEAR(k_sum, 0.0f, 0.05f);

    // Grasp R (crossed tines): one squeeze emits TINE passes (the crossed
    // pinch lowers to two tines) at the swirl centre's side of the canvas.
    sumi_midi_event_t squeezeR = {SUMI_MEV_CC, 0, 21, 127, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &squeezeR, 1, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 16);
    int tine_passes = 0;
    for (int f = 0; f < 60; f++) {
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, true, &drop_counter, q);
        for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
            if (sumi_deform_queue_at(q, i)->type == SUMI_DEFORM_TINE) tine_passes++;
        }
        sumi_deform_queue_clear(q);
    }
    CHECK(tine_passes >= 2 && tine_passes % 2 == 0);   // pairs, per crossed pass

    sumi_deform_queue_destroy(q);
    sumi_voice_mapper_destroy(vm);
}

// -------------------------------------------------------------------------
static void test_mode_handover_piano_then_wind() {
    // The user scenario: play MPE piano, stop, then play a wind instrument —
    // the mode must hand over once the piano leaves the activity window.
    sumi_normalizer_t* n = sumi_normalizer_create(nullptr, nullptr);
    sumi_midi_event_t ev[64];

    // MPE piano: notes + pressure across member channels.
    for (uint8_t ch = 1; ch <= 3; ch++) {
        sumi_normalizer_push(n, (uint8_t)(0x90 | ch), 60, 100);
        sumi_normalizer_push(n, (uint8_t)(0xD0 | ch), 64, 0);
    }
    sumi_normalizer_drain(n, tnow(), ev, 64);
    CHECK(sumi_normalizer_mode(n) == SUMI_INPUT_MPE);

    // Silence: the mode HOLDS (no flapping between phrases).
    g_now += 3.0;
    sumi_normalizer_push(n, 0xB0, 7, 1);   // single stray CC (volume creep)
    sumi_normalizer_drain(n, tnow(), ev, 64);
    CHECK(sumi_normalizer_mode(n) == SUMI_INPUT_MPE);

    // 8 s later the piano is out of the window; a mono wind stream starts
    // (single channel + dense breath, CC11 flavor).
    g_now += 8.0;
    sumi_normalizer_push(n, 0x90, 60, 80);
    for (int i = 0; i < 16; i++) sumi_normalizer_push(n, 0xB0, 11, (uint8_t)(40 + i));
    sumi_normalizer_drain(n, tnow(), ev, 64);
    CHECK(sumi_normalizer_mode(n) == SUMI_INPUT_WIND);

    // Piano resumes: back to MPE within one phrase.
    for (uint8_t ch = 4; ch <= 6; ch++) {
        sumi_normalizer_push(n, (uint8_t)(0x90 | ch), 62, 100);
        sumi_normalizer_push(n, (uint8_t)(0xD0 | ch), 70, 0);
    }
    sumi_normalizer_drain(n, tnow(), ev, 64);
    CHECK(sumi_normalizer_mode(n) == SUMI_INPUT_MPE);
    sumi_normalizer_destroy(n);

    // Mapper side: a mode switch ends the voices tracked under the old mode
    // so nothing keeps feeding (stuck-voice guard).
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[32];
    sumi_midi_event_t mpe_seq[2] = {
        {SUMI_MEV_NOTE_ON, 2, 60, 100, 0.0f},
        {SUMI_MEV_CHANNEL_PRESSURE, 2, 0, 127, 0.0f},
    };
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, mpe_seq, 2, SUMI_INPUT_MPE,
                                              default_zone(), &params, 1.0f, vev, 32);
    CHECK(nv == 2);
    // Mode flips to wind with no explicit note-off: a VoiceEnd is synthesized.
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, nullptr, 0, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 32);
    CHECK(nv == 1);
    CHECK(vev[0].kind == SUMI_VEV_VOICE_END && vev[0].voice_id == 2);
    CHECK_NEAR(vev[0].value, 0.0f, 1e-6f);
    sumi_voice_mapper_destroy(vm);
}


// -------------------------------------------------------------------------
static void test_overflow_stuck_voice_timeout() {
    // §3.1: an overflow that swallowed a Note Off must not leave a voice
    // feeding forever — the mapper arms per-voice inactivity timeouts on the
    // first overflow and synthesizes VoiceEnd after ~10 s of voice silence
    // while other traffic flows.
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[32];

    // A voice begins on member ch 3 (its Note Off will be "swallowed").
    sumi_midi_event_t on = {SUMI_MEV_NOTE_ON, 3, 60, 100, 0.0f};
    double now = 100.0;
    uint32_t nv = sumi_voice_mapper_normalize(vm, now, 0, &on, 1, SUMI_INPUT_MPE,
                                              default_zone(), &params, 1.0f, vev, 32);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_BEGIN);

    // WITHOUT an overflow: 20 s of other traffic, the voice must NOT expire.
    sumi_midi_event_t other = {SUMI_MEV_CC, 5, 30, 64, 0.0f};   // unmapped by default (Flex L, #69)
    now += 20.0;
    nv = sumi_voice_mapper_normalize(vm, now, 0, &other, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 32);
    CHECK(nv == 0);   // timeouts are armed only after an overflow

    // Overflow reported (dropped counter incremented): timeouts arm. The
    // voice stays silent while other traffic flows; after >10 s it expires.
    now += 1.0;
    nv = sumi_voice_mapper_normalize(vm, now, 5, &other, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 32);
    CHECK(nv == 0);   // armed, but only ~1 s of voice silence since arming... (activity clock)
    now += 11.0;
    nv = sumi_voice_mapper_normalize(vm, now, 5, &other, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 32);
    CHECK(nv == 1);
    CHECK(vev[0].kind == SUMI_VEV_VOICE_END && vev[0].voice_id == 3);
    CHECK_NEAR(vev[0].value, 0.0f, 1e-6f);   // synthetic lift 0

    // Expired voices do not re-expire.
    now += 11.0;
    nv = sumi_voice_mapper_normalize(vm, now, 5, &other, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 32);
    CHECK(nv == 0);
    sumi_voice_mapper_destroy(vm);
}

// -------------------------------------------------------------------------
static void test_dip_rebase_and_refusal() {
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_deform_queue_t* q = sumi_deform_queue_create(64);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[8];
    uint32_t drop_counter = 777;

    // Accepted dip (dip_allowed = true): RESET pushed, counter rebased (§4.2).
    // The event comes from the ABI path (sumi_trigger_paper_dip) — #62 removed
    // the CC 64 route — so it is built directly here.
    sumi_voice_event_t dip = {};
    dip.kind = SUMI_VEV_PAPER_DIP;
    vev[0] = dip;
    uint32_t nv = 1;
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 1);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_RESET);
    CHECK(drop_counter == 0);
    sumi_deform_queue_clear(q);

    // Refused dip (both print buffers busy): no RESET, counter untouched.
    drop_counter = 42;
    vev[0] = dip;
    nv = 1;
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, false, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 0);
    CHECK(drop_counter == 42);

    sumi_deform_queue_destroy(q);
    sumi_voice_mapper_destroy(vm);
}

// -------------------------------------------------------------------------
static void test_aux_rebase_3000_drop_session() {
    // §4.2: aux must stay < 2048 across a 3000-drop session with two dips.
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_deform_queue_t* q = sumi_deform_queue_create(8);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[8];
    uint32_t drop_counter = 0;
    float max_aux = 0.0f;

    for (int i = 0; i < 3000; i++) {
        sumi_midi_event_t note = {SUMI_MEV_NOTE_ON, 0, (uint8_t)(30 + i % 60), 100, 0.0f};
        uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &note, 1, SUMI_INPUT_CLASSIC,
                                                  default_zone(), &params, 1.0f, vev, 8);
        sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
        for (uint32_t d = 0; d < sumi_deform_queue_count(q); d++) {
            const sumi_deform_t* def = sumi_deform_queue_at(q, d);
            if (def->type == SUMI_DEFORM_DROP && def->as.drop.aux > max_aux) {
                max_aux = def->as.drop.aux;
            }
        }
        sumi_deform_queue_clear(q);
        if (i == 999 || i == 1999) {   // two paper dips (the ABI action, #62: no CC 64 route)
            sumi_voice_event_t dip = {};
            dip.kind = SUMI_VEV_PAPER_DIP;
            vev[0] = dip;
            sumi_voice_mapper_lower(vm, vev, 1, 0.016, &params, true, &drop_counter, q);
            sumi_deform_queue_clear(q);
        }
    }
    CHECK(max_aux < 2048.0f);
    CHECK(drop_counter < 2048u);
    sumi_deform_queue_destroy(q);
    sumi_voice_mapper_destroy(vm);
}

// -------------------------------------------------------------------------
static void test_feed_episodes_nested_rings() {
    // §4.4: repeated pressure pulses on ONE voice must stamp nested rings —
    // each onset after a release starts a NEW ink band; the first episode
    // continues the strike's band.
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_deform_queue_t* q = sumi_deform_queue_create(256);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[8];
    uint32_t drop_counter = 0;

    sumi_midi_event_t on = {SUMI_MEV_NOTE_ON, 2, 60, 100, 0.0f};
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &on, 1, SUMI_INPUT_MPE,
                                              default_zone(), &params, 1.0f, vev, 8);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    const float strike_phase = sumi_deform_queue_at(q, 0)->as.drop.phase_base;
    const float strike_aux = sumi_deform_queue_at(q, 0)->as.drop.aux;
    sumi_deform_queue_clear(q);
    CHECK(drop_counter == 1);

    float last_phase = strike_phase;
    float last_aux = strike_aux;
    int distinct_bands = 1;   // the strike band
    for (int pulse = 0; pulse < 5; pulse++) {
        // Press to full...
        sumi_midi_event_t press = {SUMI_MEV_CHANNEL_PRESSURE, 2, 0, 127, 0.0f};
        nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &press, 1, SUMI_INPUT_MPE,
                                         default_zone(), &params, 1.0f, vev, 8);
        float pulse_phase = -1.0f, pulse_aux = -1.0f;
        for (int f = 0; f < 30; f++) {
            sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, true, &drop_counter, q);
            for (uint32_t d = 0; d < sumi_deform_queue_count(q); d++) {
                pulse_phase = sumi_deform_queue_at(q, d)->as.drop.phase_base;
                pulse_aux = sumi_deform_queue_at(q, d)->as.drop.aux;
            }
            sumi_deform_queue_clear(q);
        }
        CHECK(pulse_phase > 0.0f);   // the pulse fed
        if (pulse == 0) {
            // First episode: SAME band as the strike (the drop grows).
            CHECK_NEAR(pulse_phase, strike_phase, 1e-6f);
            CHECK_NEAR(pulse_aux, strike_aux, 1e-4f);
        } else {
            // Later episodes: NEW band, parity alternating, new aux.
            CHECK(pulse_aux > last_aux + 0.5f);
            CHECK(std::fabs(pulse_phase - last_phase) > 0.5f);   // parity flipped
            distinct_bands++;
        }
        last_phase = pulse_phase;
        last_aux = pulse_aux;
        // ...and release fully (episode ends).
        sumi_midi_event_t rel = {SUMI_MEV_CHANNEL_PRESSURE, 2, 0, 0, 0.0f};
        nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &rel, 1, SUMI_INPUT_MPE,
                                         default_zone(), &params, 1.0f, vev, 8);
        for (int f = 0; f < 40; f++) {   // let smoothing decay below release
            sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, true, &drop_counter, q);
            sumi_deform_queue_clear(q);
        }
    }
    CHECK(distinct_bands == 5);       // 5 pulses -> 5 nested bands
    CHECK(drop_counter == 5);         // strike + 4 new episode bands
    sumi_deform_queue_destroy(q);
    sumi_voice_mapper_destroy(vm);
}


// -------------------------------------------------------------------------
static void test_janko_echo_sets() {
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_deform_queue_t* q = sumi_deform_queue_create(256);
    sumi_params_t params = default_params();
    params.pitch_layout = SUMI_LAYOUT_JANKO;
    sumi_voice_event_t vev[8];
    uint32_t drop_counter = 0;

    // VoiceBegin: THREE strike drops, identical band and aux, counter +1.
    sumi_midi_event_t on = {SUMI_MEV_NOTE_ON, 2, 60, 100, 0.0f};
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &on, 1, SUMI_INPUT_MPE,
                                              default_zone(), &params, 1.0f, vev, 8);
    CHECK(nv == 1 && vev[0].echo_count == 3);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 3);
    CHECK(drop_counter == 1);   // ticks ONCE per VoiceBegin (§3.4/§4.2)
    const float phase = sumi_deform_queue_at(q, 0)->as.drop.phase_base;
    const float aux = sumi_deform_queue_at(q, 0)->as.drop.aux;
    float strike_r = sumi_deform_queue_at(q, 0)->as.drop.radius;
    float ys[3];
    for (int e = 0; e < 3; e++) {
        const sumi_deform_t* d = sumi_deform_queue_at(q, (uint32_t)e);
        CHECK(d->type == SUMI_DEFORM_DROP);
        CHECK_NEAR(d->as.drop.phase_base, phase, 1e-6f);   // shared band
        CHECK_NEAR(d->as.drop.aux, aux, 1e-6f);            // shared hue
        CHECK_NEAR(d->as.drop.radius, strike_r, 1e-6f);
        CHECK_NEAR(d->as.drop.x, sumi_deform_queue_at(q, 0)->as.drop.x, 1e-6f);
        ys[e] = d->as.drop.y;
    }
    CHECK(ys[0] < ys[1] && ys[1] < ys[2]);   // three aligned lattice rows
    sumi_deform_queue_clear(q);

    // Press: all three echoes grow in LOCKSTEP (3 expansions per emission,
    // same radius, same band/aux, at the three centers).
    sumi_midi_event_t press = {SUMI_MEV_CHANNEL_PRESSURE, 2, 0, 127, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &press, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 8);
    uint32_t feed_cycles = 0;
    for (int f = 0; f < 30; f++) {
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, true, &drop_counter, q);
        const uint32_t n = sumi_deform_queue_count(q);
        CHECK(n % 3 == 0);   // echo sets are atomic: multiples of 3 only
        if (n >= 3) {
            feed_cycles++;
            const float r0 = sumi_deform_queue_at(q, 0)->as.drop.radius;
            for (uint32_t i = 0; i < 3; i++) {
                CHECK_NEAR(sumi_deform_queue_at(q, i)->as.drop.radius, r0, 1e-6f);
                CHECK_NEAR(sumi_deform_queue_at(q, i)->as.drop.phase_base, phase, 1e-6f);
            }
        }
        sumi_deform_queue_clear(q);
    }
    CHECK(feed_cycles >= 8);
    CHECK(drop_counter == 1);   // feeds never tick the counter

    // Glide: three tines per emission, all along the SAME lattice vector.
    sumi_midi_event_t bend = {SUMI_MEV_BEND, 2, 0, 0, 4.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &bend, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 8);
    bool saw_triplet_tines = false;
    for (int f = 0; f < 60; f++) {
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, true, &drop_counter, q);
        uint32_t tines = 0;
        float dx0 = 0, dy0 = 0;
        for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
            const sumi_deform_t* d = sumi_deform_queue_at(q, i);
            if (d->type != SUMI_DEFORM_TINE) continue;
            if (tines == 0) { dx0 = d->as.tine.x1 - d->as.tine.x0; dy0 = d->as.tine.y1 - d->as.tine.y0; }
            else {
                CHECK_NEAR(d->as.tine.x1 - d->as.tine.x0, dx0, 1e-5f);   // same vector
                CHECK_NEAR(d->as.tine.y1 - d->as.tine.y0, dy0, 1e-5f);
            }
            tines++;
        }
        CHECK(tines % 3 == 0);
        if (tines == 3) saw_triplet_tines = true;
        sumi_deform_queue_clear(q);
    }
    CHECK(saw_triplet_tines);

    // Budget atomicity: with room for only 2 passes, a 3-echo feed emits
    // NOTHING (merged within the echo across frames — never a partial set).
    sumi_voice_mapper_set_budget(vm, 2);
    sumi_midi_event_t press2 = {SUMI_MEV_CHANNEL_PRESSURE, 2, 0, 127, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &press2, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 8);
    const uint32_t merged_before = sumi_voice_mapper_merged_count(vm);
    for (int f = 0; f < 10; f++) {
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.05, &params, true, &drop_counter, q);
        CHECK(sumi_deform_queue_count(q) == 0);   // all-or-none: none fits
        sumi_deform_queue_clear(q);
    }
    CHECK(sumi_voice_mapper_merged_count(vm) > merged_before);
    // Raising the budget releases the merged growth as full triplets.
    sumi_voice_mapper_set_budget(vm, 64);
    sumi_voice_mapper_lower(vm, vev, 0, 0.016, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 3);
    sumi_deform_queue_clear(q);

    // Lift (#41): no rings, on any echo — the set simply ends.
    sumi_midi_event_t off = {SUMI_MEV_NOTE_OFF, 2, 60, 127, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &off, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 8);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    uint32_t rings = 0;
    for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
        const sumi_deform_t* d = sumi_deform_queue_at(q, i);
        if (d->type == SUMI_DEFORM_DROP && d->as.drop.phase_base == 0.0f) rings++;
    }
    CHECK(rings == 0);

    sumi_deform_queue_destroy(q);
    sumi_voice_mapper_destroy(vm);
}

// -------------------------------------------------------------------------
int main() {
    test_ring_basic_and_overflow();
    test_note_on_off_and_vel0();
    test_bend_assembly_and_rpn_range();
    test_running_status_tolerance();
    test_sysex_and_system_ignored();
    test_mode_detection();
    test_layout_golden_positions();
    test_layout_probe_golden();
    test_hostmpe_loopback_conformance();
    test_layout_glide_axis_and_live_switch();
    test_janko_echo_sets();
    test_roll_field_motion_clock();
    test_classic_mapping_to_deforms();
    test_mpe_master_channel_keyboard();
    test_wind_expression_layer();
    test_bend_mode_single_consumer();
    test_swirl_routing();
    test_pen_in_cell_bend_modes();
    test_sustain_never_dips();
    test_mpe_zone_and_bend_range();
    test_mpe_voice_steal_and_coalescing();
    test_mpe_press_feed_and_glide();
    test_deform_budget_merging();
    test_mpe_lift_ring_and_slide_aux();
    test_wind_mode_wake_legato();
    test_cc_routing_table();
    test_global_ctl_vortex_and_viscosity();
    test_global_ctl_swirl_and_pinches();
    test_chladni_kick_drift_order();
    test_palette_presets_and_ring();
    test_burst_math_and_episode();
    test_spark_shear_math_and_episode();
    test_chirikov_map_and_route();
    test_medium_binding_tables();
    test_mode_handover_piano_then_wind();
    test_overflow_stuck_voice_timeout();
    test_dip_rebase_and_refusal();
    test_aux_rebase_3000_drop_session();
    test_feed_episodes_nested_rings();

    if (g_failures == 0) {
        std::printf("OK: %d checks passed (normalizer/mapper, headless)\n", g_checks);
        return 0;
    }
    std::fprintf(stderr, "%d/%d checks FAILED\n", g_failures, g_checks);
    return 1;
}
