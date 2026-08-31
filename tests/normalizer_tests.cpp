// normalizer_tests.cpp — headless decoder/mapper unit tests (roadmap step 4).
// Links the normalizer, voice mapper, and displacement queue directly — no
// GPU, no sokol, CI-runnable on a bare macOS runner.
#include "midi_normalizer.h"
#include "voice_mapper.h"
#include "displacement.h"

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
static void test_pitch_positions() {
    float x, y;
    // Circle of fifths, aspect 1: C4 (60) at 12 o'clock (angle -pi/2).
    sumi_pitch_to_position(60, 0, 1.0f, &x, &y);
    CHECK_NEAR(x, 0.5f, 1e-4f);
    CHECK(y < 0.5f);   // C points up (v grows down)

    // Same pitch class, lower octave -> farther from center (low = outer).
    float x2, y2;
    sumi_pitch_to_position(36, 0, 1.0f, &x2, &y2);
    const float r1 = std::fabs(y - 0.5f);
    const float r2 = std::fabs(y2 - 0.5f);
    CHECK(r2 > r1);

    // G (pc 7) is one fifth step around the circle from C.
    sumi_pitch_to_position(67, 0, 1.0f, &x, &y);
    CHECK(x > 0.5f);   // 1/12 turn clockwise from 12 o'clock

    // Aspect correction: at aspect 2, x-offsets halve, y-offsets don't.
    float xa, ya;
    sumi_pitch_to_position(67, 0, 2.0f, &xa, &ya);
    CHECK_NEAR((xa - 0.5f) * 2.0f, (x - 0.5f), 1e-4f);
    CHECK_NEAR(ya, y, 1e-6f);

    // Grid layout: pc -> column, octave -> row (low at the bottom).
    sumi_pitch_to_position(0, 1, 1.0f, &x, &y);     // C-1: first column, bottom
    CHECK_NEAR(x, 0.5f / 12.0f, 1e-4f);
    CHECK(y > 0.9f);
    sumi_pitch_to_position(127, 1, 1.0f, &x, &y);   // G9: col 8, top
    CHECK_NEAR(x, 7.5f / 12.0f, 1e-4f);
    CHECK(y < 0.1f);
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
    uint32_t nv = sumi_voice_mapper_normalize(vm, &note, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1);
    CHECK(vev[0].kind == SUMI_VEV_VOICE_BEGIN);
    CHECK_NEAR(vev[0].value, 1.0f, 1e-4f);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 1);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_DROP);
    CHECK(sumi_deform_queue_at(q, 0)->as.drop.phase_base >= 1.0f);   // ink, not water
    CHECK_NEAR(sumi_deform_queue_at(q, 0)->as.drop.radius, 0.020f + 0.075f, 1e-4f);
    CHECK(drop_counter == 1);
    sumi_deform_queue_clear(q);

    // Velocity 32 (1/4 of 127ish) -> radius scales with sqrt.
    note.b = 32;
    nv = sumi_voice_mapper_normalize(vm, &note, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, &drop_counter, q);
    CHECK_NEAR(sumi_deform_queue_at(q, 0)->as.drop.radius,
               0.020f + 0.075f * std::sqrt(32.0f / 127.0f), 1e-4f);
    sumi_deform_queue_clear(q);

    // Global bend -> one shear tine on the delta.
    sumi_midi_event_t bend = {SUMI_MEV_BEND, 0, 0, 0, 1.0f};
    nv = sumi_voice_mapper_normalize(vm, &bend, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_GLOBAL_BEND);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 1);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_TINE);
    CHECK_NEAR(sumi_deform_queue_at(q, 0)->as.tine.magnitude, 0.015f, 1e-5f);
    sumi_deform_queue_clear(q);

    // Same bend again -> no delta -> no tine.
    nv = sumi_voice_mapper_normalize(vm, &bend, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 0);

    // Mod wheel -> coalesced to ONE vortex per update even for many CC1s.
    sumi_midi_event_t mods[3] = {
        {SUMI_MEV_CC, 0, 1, 10, 0.0f},
        {SUMI_MEV_CC, 0, 1, 60, 0.0f},
        {SUMI_MEV_CC, 0, 1, 127, 0.0f},
    };
    nv = sumi_voice_mapper_normalize(vm, mods, 3, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1);
    CHECK(vev[0].kind == SUMI_VEV_GLOBAL_CTL && vev[0].dimension == SUMI_CTL_VORTEX_STRENGTH);
    CHECK_NEAR(vev[0].value, 1.0f, 1e-4f);   // last one wins
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 1);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_VORTEX);
    sumi_deform_queue_clear(q);

    // CC64 rising edge -> PaperDip -> RESET deform; held/repeat -> nothing.
    sumi_midi_event_t sus_on  = {SUMI_MEV_CC, 0, 64, 127, 0.0f};
    sumi_midi_event_t sus_rep = {SUMI_MEV_CC, 0, 64, 100, 0.0f};
    sumi_midi_event_t sus_off = {SUMI_MEV_CC, 0, 64, 0, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, &sus_on, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_PAPER_DIP);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, &drop_counter, q);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_RESET);
    sumi_deform_queue_clear(q);
    nv = sumi_voice_mapper_normalize(vm, &sus_rep, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 0);   // still held: no new dip
    nv = sumi_voice_mapper_normalize(vm, &sus_off, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 0);   // release: no dip
    nv = sumi_voice_mapper_normalize(vm, &sus_on, 1, SUMI_INPUT_CLASSIC, default_zone(), &params, 1.0f, vev, 16);
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
    uint32_t nv = sumi_voice_mapper_normalize(vm, steal_seq, 2, SUMI_INPUT_MPE,
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
    nv = sumi_voice_mapper_normalize(vm, offs, 2, SUMI_INPUT_MPE,
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
    nv = sumi_voice_mapper_normalize(vm, burst, 8, SUMI_INPUT_MPE,
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
    uint32_t nv = sumi_voice_mapper_normalize(vm, seq, 2, SUMI_INPUT_MPE,
                                              default_zone(), &params, 1.0f, vev, 16);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) >= 1);
    const float strike_phase = sumi_deform_queue_at(q, 0)->as.drop.phase_base;
    const float strike_x = sumi_deform_queue_at(q, 0)->as.drop.x;
    sumi_deform_queue_clear(q);

    // §4.4: sustained pressure re-emits small expansions each frame at the
    // voice center, SAME ink band as the strike (the drop grows, no new ring).
    uint32_t expansions = 0;
    for (int f = 0; f < 30; f++) {
        sumi_voice_mapper_lower(vm, vev, 0, 0.016, &params, &drop_counter, q);
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
    nv = sumi_voice_mapper_normalize(vm, &bend, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 16);
    float moved_x = 0.0f;
    bool saw_tine = false;
    for (int f = 0; f < 60; f++) {
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, &drop_counter, q);
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
    uint32_t nv = sumi_voice_mapper_normalize(vm, seq, 12, SUMI_INPUT_MPE,
                                              default_zone(), &params, 1.0f, vev, 64);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, &drop_counter, q);   // strikes land
    sumi_deform_queue_clear(q);

    // Cap the budget below the number of pressing voices: per frame only
    // `budget` expansions may emit; the rest MERGE into later frames.
    sumi_voice_mapper_set_budget(vm, 3);
    // Big dt so every voice's feed accumulator exceeds the emission threshold.
    sumi_voice_mapper_lower(vm, vev, 0, 0.05, &params, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 3);          // budget respected
    CHECK(sumi_voice_mapper_merged_count(vm) >= 3);  // the other voices merged
    sumi_deform_queue_clear(q);

    // Next frame: the merged (pending) feeds emit — nothing was lost.
    sumi_voice_mapper_lower(vm, vev, 0, 0.05, &params, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 3);
    sumi_deform_queue_clear(q);

    // Raising the budget lets all 6 emit in one frame again.
    sumi_voice_mapper_set_budget(vm, 64);
    sumi_voice_mapper_lower(vm, vev, 0, 0.05, &params, &drop_counter, q);
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
    uint32_t nv = sumi_voice_mapper_normalize(vm, seq, 3, SUMI_INPUT_MPE,
                                              default_zone(), &params, 1.0f, vev, 16);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, &drop_counter, q);
    const float aux_base = sumi_deform_queue_at(q, 0)->as.drop.aux;
    sumi_deform_queue_clear(q);
    for (int f = 0; f < 40; f++) {   // let slide smoothing converge, collect a feed
        sumi_voice_mapper_lower(vm, vev, 0, 0.016, &params, &drop_counter, q);
    }
    CHECK(sumi_deform_queue_count(q) > 0);
    const sumi_deform_t* feed = sumi_deform_queue_at(q, sumi_deform_queue_count(q) - 1);
    CHECK(feed->as.drop.aux > aux_base + 0.5f);   // slide -> aux modulation
    sumi_deform_queue_clear(q);

    // Lift with high release velocity -> one faint CLEAR ring at the center.
    sumi_midi_event_t off = {SUMI_MEV_NOTE_OFF, 4, 72, 127, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, &off, 1, SUMI_INPUT_MPE,
                                     default_zone(), &params, 1.0f, vev, 16);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 1);
    const sumi_deform_t* ring = sumi_deform_queue_at(q, 0);
    CHECK(ring->type == SUMI_DEFORM_DROP);
    CHECK_NEAR(ring->as.drop.phase_base, 0.0f, 1e-6f);   // clear surfactant
    CHECK_NEAR(ring->as.drop.radius, 0.006f + 0.030f, 1e-4f);

    // The voice is gone: further frames emit nothing.
    sumi_deform_queue_clear(q);
    sumi_voice_mapper_lower(vm, vev, 0, 0.016, &params, &drop_counter, q);
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
    uint32_t nv = sumi_voice_mapper_normalize(vm, &on1, 1, SUMI_INPUT_WIND,
                                              default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_BEGIN && vev[0].voice_id == 0);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, &drop_counter, q);
    const float phase = sumi_deform_queue_at(q, 0)->as.drop.phase_base;
    sumi_deform_queue_clear(q);

    // Legato note change: MIGRATE, not a new drop — even before the off.
    sumi_midi_event_t on2 = {SUMI_MEV_NOTE_ON, 0, 67, 90, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, &on2, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_MIGRATE);
    sumi_voice_mapper_lower(vm, vev, nv, 0.016, &params, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 1);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_TINE);   // wake
    CHECK_NEAR(sumi_deform_queue_at(q, 0)->as.tine.alpha, 0.035f, 1e-6f);
    sumi_deform_queue_clear(q);
    CHECK(drop_counter == 1);   // still ONE drop: the brush migrated

    // The off of the OLD note (legato overlap) is ignored.
    sumi_midi_event_t off_old = {SUMI_MEV_NOTE_OFF, 0, 60, 40, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, &off_old, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 0);

    // Breath (CC2 via the default map) feeds the voice: expansions appear at
    // the MIGRATED position with the SAME ink band.
    sumi_midi_event_t breath = {SUMI_MEV_CC, 0, 2, 127, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, &breath, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_PRESS && vev[0].voice_id == 0);
    float gx, gy;
    sumi_pitch_to_position(67, 0, 1.0f, &gx, &gy);
    uint32_t feeds = 0;
    for (int f = 0; f < 30; f++) {
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, &drop_counter, q);
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
    nv = sumi_voice_mapper_normalize(vm, &at, 1, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_VOICE_PRESS);

    // Off of the CURRENT note ends the brush.
    sumi_midi_event_t off_cur = {SUMI_MEV_NOTE_OFF, 0, 67, 50, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, &off_cur, 1, SUMI_INPUT_WIND,
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
    uint32_t nv = sumi_voice_mapper_normalize(vm, &cc20, 1, SUMI_INPUT_CLASSIC,
                                              default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_GLOBAL_CTL);
    CHECK(vev[0].dimension == SUMI_CTL_VORTEX_STRENGTH);

    // Unmapped CC30 does nothing...
    sumi_midi_event_t cc30 = {SUMI_MEV_CC, 0, 30, 100, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, &cc30, 1, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 0);
    // ...until mapped at runtime (§5.3).
    sumi_voice_mapper_map_cc(vm, 0xFF, 30, SUMI_CTL_VORTEX_STRENGTH);
    nv = sumi_voice_mapper_normalize(vm, &cc30, 1, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].dimension == SUMI_CTL_VORTEX_STRENGTH);
    CHECK_NEAR(vev[0].value, 100.0f / 127.0f, 1e-4f);

    // Channel-specific mapping overrides any-channel.
    sumi_voice_mapper_map_cc(vm, 3, 30, SUMI_CTL_VISCOSITY);
    sumi_midi_event_t cc30ch3 = {SUMI_MEV_CC, 3, 30, 64, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, &cc30ch3, 1, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].dimension == SUMI_CTL_VISCOSITY);

    // Multiple dimensions coalesce independently in one update.
    sumi_midi_event_t multi[3] = {
        {SUMI_MEV_CC, 0, 20, 127, 0.0f},
        {SUMI_MEV_CC, 0, 21, 96, 0.0f},   // vortex X
        {SUMI_MEV_CC, 0, 23, 32, 0.0f},   // viscosity
    };
    nv = sumi_voice_mapper_normalize(vm, multi, 3, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 16);
    CHECK(nv == 3);

    // clear_cc_map removes everything, defaults included.
    sumi_voice_mapper_clear_cc_map(vm);
    sumi_midi_event_t cc1 = {SUMI_MEV_CC, 0, 1, 127, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, &cc1, 1, SUMI_INPUT_CLASSIC,
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
    uint32_t nv = sumi_voice_mapper_normalize(vm, ccs, 3, SUMI_INPUT_CLASSIC,
                                              default_zone(), &params, 1.0f, vev, 16);
    float theta_low_visc = 0.0f;
    for (int f = 0; f < 40; f++) {   // let smoothing converge
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, &drop_counter, q);
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
    nv = sumi_voice_mapper_normalize(vm, &visc, 1, SUMI_INPUT_CLASSIC,
                                     default_zone(), &params, 1.0f, vev, 16);
    float theta_high_visc = 0.0f;
    for (int f = 0; f < 40; f++) {
        sumi_voice_mapper_lower(vm, vev, f == 0 ? nv : 0, 0.016, &params, &drop_counter, q);
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
    uint32_t nv = sumi_voice_mapper_normalize(vm, mpe_seq, 2, SUMI_INPUT_MPE,
                                              default_zone(), &params, 1.0f, vev, 32);
    CHECK(nv == 2);
    // Mode flips to wind with no explicit note-off: a VoiceEnd is synthesized.
    nv = sumi_voice_mapper_normalize(vm, nullptr, 0, SUMI_INPUT_WIND,
                                     default_zone(), &params, 1.0f, vev, 32);
    CHECK(nv == 1);
    CHECK(vev[0].kind == SUMI_VEV_VOICE_END && vev[0].voice_id == 2);
    CHECK_NEAR(vev[0].value, 0.0f, 1e-6f);
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
    test_pitch_positions();
    test_classic_mapping_to_deforms();
    test_mpe_zone_and_bend_range();
    test_mpe_voice_steal_and_coalescing();
    test_mpe_press_feed_and_glide();
    test_deform_budget_merging();
    test_mpe_lift_ring_and_slide_aux();
    test_wind_mode_wandering_brush();
    test_cc_routing_table();
    test_global_ctl_vortex_and_viscosity();
    test_mode_handover_piano_then_wind();

    if (g_failures == 0) {
        std::printf("OK: %d checks passed (normalizer/mapper, headless)\n", g_checks);
        return 0;
    }
    std::fprintf(stderr, "%d/%d checks FAILED\n", g_failures, g_checks);
    return 1;
}
