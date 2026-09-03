// normalizer_tests.cpp — headless decoder/mapper unit tests (roadmap step 4).
// Links the normalizer, voice mapper, and displacement queue directly — no
// GPU, no sokol, CI-runnable on a bare macOS runner.
#include "midi_normalizer.h"
#include "voice_mapper.h"
#include "hostmpe.h"
#include "displacement.h"
#include "layouts.h"

#include <cmath>

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
        gx[0] = 0.08f + (xu / 7.0f) * 0.84f;
        gy[0] = 0.10f + ((row + 0.5f) / 14.0f) * 0.80f;
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
    for (uint32_t layout = 0; layout <= 5; layout++) {
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
    // Piano grid: C4 = first white cell of octave 4's WHITE row (row 7);
    // C#4 sits in the black row above (row 6) at the C-D boundary.
    sumi_layout_position(5, 60, &p, 1.0f, x, y);
    CHECK_NEAR(x[0], 0.08f + (0.5f / 7.0f) * 0.84f, 1e-4f);
    CHECK_NEAR(y[0], 0.10f + (7.5f / 14.0f) * 0.80f, 1e-4f);
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
            CHECK(sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, aspect,
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
            sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, aspect, px[0], py[0], &c);
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
                CHECK(sumi_layout_probe(SUMI_LAYOUT_JANKO, &params, aspect,
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
            sumi_layout_probe(SUMI_LAYOUT_JANKO, &params, aspect, ex[0], ey[0], &c);
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
            CHECK(sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &params, aspect,
                                    px[0], py[0], &c));
            CHECK(c.note == (uint8_t)note);
            CHECK_NEAR(c.cell_center_x, px[0], 1e-5f);
            CHECK_NEAR(c.cell_center_y, py[0], 1e-5f);
            CHECK(c.cell_radius > 0.0f);
            CHECK(c.semitone_step > 0.0f);
        }
        // Piano-grid semitone axis (DECISIONS_3 #29): the generic shortest-
        // neighbor rule — C4's nearest semitone is C#4, half a key over and
        // one row UP (pitch is not a function of x alone on this lattice).
        {
            float px[SUMI_MAX_ECHOES], py[SUMI_MAX_ECHOES];
            sumi_layout_position(SUMI_LAYOUT_PIANO_GRID, 60, &params, aspect, px, py);
            sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &params, aspect, px[0], py[0], &c);
            const float exdx = (0.5f / 7.0f) * 0.84f * aspect;   // half a key
            const float exdy = -(0.80f / 14.0f);                 // one row up
            const float exstep = std::sqrt(exdx * exdx + exdy * exdy);
            CHECK_NEAR(c.semitone_step, exstep, 1e-4f);
            CHECK_NEAR(c.semitone_dx, exdx / exstep, 1e-4f);
            CHECK_NEAR(c.semitone_dy, exdy / exstep, 1e-4f);
            // R_max golden (#29): half of min(key width, OCTAVE-PAIR height)
            // — the drawn row is half a key's playable footprint, so the
            // vertical measure matches the chroma grid's row height.
            const float pw = (0.84f / 7.0f) * aspect;
            const float oh = 0.80f / 7.0f;
            CHECK_NEAR(c.cell_radius, 0.5f * (pw < oh ? pw : oh), 1e-5f);
        }
        params.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
    }

    // Refusals: non-playable layouts, outside the playable area, Jankó
    // stagger dead zones.
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_FIFTHS, &params, 1.0f, 0.5f, 0.5f, &c));
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_ROLL_H, &params, 1.0f, 0.5f, 0.5f, &c));
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_ROLL_V, &params, 1.0f, 0.5f, 0.5f, &c));
    CHECK(!sumi_layout_probe(99u, &params, 1.0f, 0.5f, 0.5f, &c));
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, 1.0f, 0.02f, 0.5f, &c));
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, 1.0f, 0.5f, 0.95f, &c));
    // Odd (staggered) Jankó rows have a half-cell dead zone at their left
    // edge — off the key bed, honestly unplayable.
    const float row1_y = 0.10f + (1.5f / 6.0f) * 0.80f;
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_JANKO, &params, 1.0f,
                             0.06f + 0.001f, row1_y, &c));
    // Piano-grid black-row dead zones: the E-F gap (white-key unit 3) and the
    // row's left end (unit 0.2) are off the key bed; the white row below the
    // gap plays E as normal.
    const float black_row_y = 0.10f + (0.5f / 14.0f) * 0.80f;
    const float white_row_y = 0.10f + (1.5f / 14.0f) * 0.80f;
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &params, 1.0f,
                             0.08f + (3.0f / 7.0f) * 0.84f, black_row_y, &c));
    CHECK(!sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &params, 1.0f,
                             0.08f + (0.2f / 7.0f) * 0.84f, black_row_y, &c));
    CHECK(sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &params, 1.0f,
                            0.08f + (2.9f / 7.0f) * 0.84f, white_row_y, &c));
    CHECK(c.note == 28);   // E1

    // Purity: identical input, identical output, no instance anywhere.
    sumi_cell_info_t c2;
    CHECK(sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, 1.7f, 0.4f, 0.6f, &c));
    CHECK(sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, 1.7f, 0.4f, 0.6f, &c2));
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
    CHECK(sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, aspect,
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
    // A subtle vibrato maps proportionally: -0.6 semi -> amount 0.1.
    sweep(-0.6f, &tines, &ripples);
    CHECK(tines == 0);
    CHECK_NEAR(sumi_voice_mapper_ctl(vm, SUMI_CTL_RIPPLE_AMP), 0.1f, 0.03f);

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

    // CC64 rising edge -> PaperDip -> RESET deform; held/repeat -> nothing.
    sumi_midi_event_t sus_on  = {SUMI_MEV_CC, 0, 64, 127, 0.0f};
    sumi_midi_event_t sus_rep = {SUMI_MEV_CC, 0, 64, 100, 0.0f};
    sumi_midi_event_t sus_off = {SUMI_MEV_CC, 0, 64, 0, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &sus_on, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_PAPER_DIP);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_RESET);
    sumi_deform_queue_clear(q);
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &sus_rep, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 0);   // still held: no new dip
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &sus_off, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 0);   // release: no dip
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &sus_on, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1);   // second press: rising edge again

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

    // Lift with high release velocity -> one faint CLEAR ring at the center.
    sumi_midi_event_t off = {SUMI_MEV_NOTE_OFF, 4, 72, 127, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &off, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 16);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 1);
    const sumi_deform_t* ring = sumi_deform_queue_at(q, 0);
    CHECK(ring->type == SUMI_DEFORM_DROP);
    CHECK_NEAR(ring->as.drop.phase_base, 0.0f, 1e-6f);   // clear surfactant
    CHECK_NEAR(ring->as.drop.radius, 0.006f + 0.030f, 1e-4f);

    // The voice is gone: further frames emit nothing.
    sumi_deform_queue_clear(q);
    sumi_voice_mapper_lower(vm, vev, 0, 0.016, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 0);

    sumi_deform_queue_destroy(q);
    sumi_voice_mapper_destroy(vm);
}


// -------------------------------------------------------------------------
static void test_wind_mode_wandering_brush() {
    sumi_voice_mapper_t* vm = sumi_voice_mapper_create(nullptr, nullptr);
    sumi_deform_queue_t* q = sumi_deform_queue_create(256);
    sumi_params_t params = default_params();
    sumi_voice_event_t vev[16];
    uint32_t drop_counter = 0;

    // First note: the brush lands (VoiceBegin).
    sumi_midi_event_t on1 = {SUMI_MEV_NOTE_ON, 0, 60, 90, 0.0f};
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &on1, 1, SUMI_INPUT_WIND,
                                              default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_BEGIN && vev[0].voice_id == 0);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    const float phase = sumi_deform_queue_at(q, 0)->as.drop.phase_base;
    sumi_deform_queue_clear(q);

    // Legato note change: MIGRATE, not a new drop — even before the off.
    sumi_midi_event_t on2 = {SUMI_MEV_NOTE_ON, 0, 67, 90, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &on2, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_MIGRATE);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 1);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_TINE);   // wake
    CHECK_NEAR(sumi_deform_queue_at(q, 0)->as.tine.alpha, 0.035f, 1e-6f);
    sumi_deform_queue_clear(q);
    CHECK(drop_counter == 1);   // still ONE drop: the brush migrated

    // The off of the OLD note (legato overlap) is ignored.
    sumi_midi_event_t off_old = {SUMI_MEV_NOTE_OFF, 0, 60, 40, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &off_old, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 0);

    // Breath (CC2 via the default map) feeds the voice: expansions appear at
    // the MIGRATED position with the SAME ink band.
    sumi_midi_event_t breath = {SUMI_MEV_CC, 0, 2, 127, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &breath, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_PRESS && vev[0].voice_id == 0);
    float gxa[SUMI_MAX_ECHOES], gya[SUMI_MAX_ECHOES];
    sumi_layout_position(0, 67, &params, 1.0f, gxa, gya);
    const float gx = gxa[0], gy = gya[0];
    uint32_t feeds = 0;
    for (int f = 0; f < 30; f++) {
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, true, &drop_counter, q);
        for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
            const sumi_deform_t* d = sumi_deform_queue_at(q, i);
            if (d->type != SUMI_DEFORM_DROP) continue;
            CHECK_NEAR(d->as.drop.phase_base, phase, 1e-6f);
            CHECK_NEAR(d->as.drop.x, gx, 1e-3f);
            CHECK_NEAR(d->as.drop.y, gy, 1e-3f);
            feeds++;
        }
        sumi_deform_queue_clear(q);
    }
    CHECK(feeds >= 10);

    // Channel pressure aliases onto breath too (§2.3).
    sumi_midi_event_t at = {SUMI_MEV_CHANNEL_PRESSURE, 0, 0, 100, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &at, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_PRESS);

    // Off of the CURRENT note ends the brush.
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

    // Default map: CC20 (Airwave Raise) -> vortex strength.
    sumi_midi_event_t cc20 = {SUMI_MEV_CC, 0, 20, 127, 0.0f};
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &cc20, 1, SUMI_INPUT_CLASSIC,
                                              default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_GLOBAL_CTL);
    CHECK(vev[0].dimension == SUMI_CTL_VORTEX_STRENGTH);

    // Unmapped CC30 does nothing...
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

    // Channel-specific mapping overrides any-channel.
    sumi_voice_mapper_map_cc(vm, 3, 30, SUMI_CTL_VISCOSITY);
    sumi_midi_event_t cc30ch3 = {SUMI_MEV_CC, 3, 30, 64, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &cc30ch3, 1, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].dimension == SUMI_CTL_VISCOSITY);

    // Multiple dimensions coalesce independently in one update.
    sumi_midi_event_t multi[3] = {
        {SUMI_MEV_CC, 0, 20, 127, 0.0f},
        {SUMI_MEV_CC, 0, 21, 96, 0.0f},   // vortex X
        {SUMI_MEV_CC, 0, 23, 32, 0.0f},   // viscosity
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
        {SUMI_MEV_CC, 0, 20, 127, 0.0f},   // strength 1.0
        {SUMI_MEV_CC, 0, 21, 127, 0.0f},   // center x -> 1.0
        {SUMI_MEV_CC, 0, 22, 0, 0.0f},     // center y -> 0.0
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
            CHECK(d->as.vortex.x > 0.6f);   // center followed CC21
            CHECK(d->as.vortex.y < 0.4f);
        }
        sumi_deform_queue_clear(q);
    }
    CHECK(theta_low_visc > 0.05f);   // ~ 1.0 * 6 rad/s * 16 ms

    // High viscosity damps the same vortex strength (§2.2 R-Tilt).
    sumi_midi_event_t visc = {SUMI_MEV_CC, 0, 23, 127, 0.0f};
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
    sumi_midi_event_t other = {SUMI_MEV_CC, 5, 30, 64, 0.0f};
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
    sumi_midi_event_t sus_on = {SUMI_MEV_CC, 0, 64, 127, 0.0f};
    uint32_t nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &sus_on, 1, SUMI_INPUT_CLASSIC,
                                              default_zone(), &params, 1.0f, vev, 8);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_PAPER_DIP);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 1);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_RESET);
    CHECK(drop_counter == 0);
    sumi_deform_queue_clear(q);

    // Refused dip (both print buffers busy): no RESET, counter untouched.
    drop_counter = 42;
    sumi_midi_event_t sus_off = {SUMI_MEV_CC, 0, 64, 0, 0.0f};
    sumi_voice_mapper_normalize(vm, tnow(), 0, &sus_off, 1, SUMI_INPUT_CLASSIC,
                                default_zone(), &params, 1.0f, vev, 8);
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &sus_on, 1, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 8);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_PAPER_DIP);
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
    bool sus = false;

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
        if (i == 999 || i == 1999) {   // two paper dips (CC64 rising edges)
            sumi_midi_event_t cc = {SUMI_MEV_CC, 0, 64, (uint8_t)(sus ? 0 : 127), 0.0f};
            sus = !sus;
            nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &cc, 1, SUMI_INPUT_CLASSIC,
                                             default_zone(), &params, 1.0f, vev, 8);
            sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
            sumi_deform_queue_clear(q);
            if (sus) {   // release the pedal so the next dip is a rising edge
                sumi_midi_event_t rel = {SUMI_MEV_CC, 0, 64, 0, 0.0f};
                sumi_voice_mapper_normalize(vm, tnow(), 0, &rel, 1, SUMI_INPUT_CLASSIC,
                                            default_zone(), &params, 1.0f, vev, 8);
                sus = false;
            }
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

    // Lift: one surfactant ring per echo.
    sumi_midi_event_t off = {SUMI_MEV_NOTE_OFF, 2, 60, 127, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, tnow(), 0, &off, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 8);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, true, &drop_counter, q);
    uint32_t rings = 0;
    for (uint32_t i = 0; i < sumi_deform_queue_count(q); i++) {
        const sumi_deform_t* d = sumi_deform_queue_at(q, i);
        if (d->type == SUMI_DEFORM_DROP && d->as.drop.phase_base == 0.0f) rings++;
    }
    CHECK(rings == 3);

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
    test_bend_mode_single_consumer();
    test_mpe_zone_and_bend_range();
    test_mpe_voice_steal_and_coalescing();
    test_mpe_press_feed_and_glide();
    test_deform_budget_merging();
    test_mpe_lift_ring_and_slide_aux();
    test_wind_mode_wandering_brush();
    test_cc_routing_table();
    test_global_ctl_vortex_and_viscosity();
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
