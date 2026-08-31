// normalizer_tests.cpp — headless decoder/mapper unit tests (roadmap step 4).
// Links the normalizer, voice mapper, and displacement queue directly — no
// GPU, no sokol, CI-runnable on a bare macOS runner.
#include "midi_normalizer.h"
#include "voice_mapper.h"
#include "displacement.h"

#include <cmath>
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
    return p;
}

// -------------------------------------------------------------------------
static void test_ring_basic_and_overflow() {
    sumi_normalizer_t* n = sumi_normalizer_create(nullptr, nullptr);
    sumi_midi_event_t ev[8];

    // Basic: one note-on through the ring.
    sumi_normalizer_push(n, 0x90, 60, 100);
    CHECK(sumi_normalizer_drain(n, ev, 8) == 1);
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
    while ((got = sumi_normalizer_drain(n, big, 4096)) > 0) {
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
    CHECK(sumi_normalizer_drain(n, ev, 8) == 3);
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
    CHECK(sumi_normalizer_drain(n, ev, 8) == 1);
    CHECK(ev[0].kind == SUMI_MEV_BEND);
    CHECK_NEAR(ev[0].f, 0.0f, 1e-6f);

    // Max up (16383) -> +2 (well, 8191/8192 * 2).
    sumi_normalizer_push(n, 0xE0, 0x7F, 0x7F);
    CHECK(sumi_normalizer_drain(n, ev, 8) == 1);
    CHECK_NEAR(ev[0].f, 2.0f * 8191.0f / 8192.0f, 1e-4f);

    // RPN 0 -> bend range 48, then max-up bend reads ±48-scaled.
    sumi_normalizer_push(n, 0xB0, 101, 0);    // RPN MSB
    sumi_normalizer_push(n, 0xB0, 100, 0);    // RPN LSB -> RPN 0
    sumi_normalizer_push(n, 0xB0, 6, 48);     // data entry: 48 semitones
    sumi_normalizer_push(n, 0xE0, 0x00, 0x00);   // min (-8192) -> -48
    uint32_t cnt = sumi_normalizer_drain(n, ev, 8);
    CHECK(cnt == 4);   // 3 CC events + bend
    CHECK(ev[3].kind == SUMI_MEV_BEND);
    CHECK_NEAR(ev[3].f, -48.0f, 1e-4f);

    // NRPN data entry must NOT change bend range.
    sumi_normalizer_push(n, 0xB0, 99, 1);     // NRPN select
    sumi_normalizer_push(n, 0xB0, 6, 2);      // data entry -> NRPN, ignored
    sumi_normalizer_push(n, 0xE0, 0x00, 0x00);
    cnt = sumi_normalizer_drain(n, ev, 8);
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
    CHECK(sumi_normalizer_drain(n, ev, 8) == 3);
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
    CHECK(sumi_normalizer_drain(n, ev, 8) == 1);
    CHECK(ev[0].kind == SUMI_MEV_NOTE_ON);
    sumi_normalizer_destroy(n);
}

// -------------------------------------------------------------------------
static void test_mode_detection() {
    // Classic: notes on one channel, no expression.
    sumi_normalizer_t* n = sumi_normalizer_create(nullptr, nullptr);
    sumi_midi_event_t ev[64];
    sumi_normalizer_push(n, 0x90, 60, 100);
    sumi_normalizer_drain(n, ev, 64);
    CHECK(sumi_normalizer_mode(n) == SUMI_INPUT_CLASSIC);

    // MPE-ish: note-ons on member channels 2..4 with per-channel pressure.
    for (uint8_t ch = 1; ch <= 3; ch++) {
        sumi_normalizer_push(n, (uint8_t)(0x90 | ch), 60, 100);
        sumi_normalizer_push(n, (uint8_t)(0xD0 | ch), 64, 0);
    }
    sumi_normalizer_drain(n, ev, 64);
    CHECK(sumi_normalizer_mode(n) == SUMI_INPUT_MPE);
    sumi_normalizer_destroy(n);

    // Wind: single note channel + dense CC2.
    n = sumi_normalizer_create(nullptr, nullptr);
    sumi_normalizer_push(n, 0x90, 60, 100);
    for (int i = 0; i < 20; i++) sumi_normalizer_push(n, 0xB0, 2, (uint8_t)(40 + i));
    sumi_normalizer_drain(n, ev, 64);
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
    sumi_normalizer_drain(n, ev, 64);
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
    uint32_t nv = sumi_voice_mapper_normalize(vm, &note, 1, SUMI_INPUT_CLASSIC,
                                              &params, 1.0f, vev, 16);
    CHECK(nv == 1);
    CHECK(vev[0].kind == SUMI_VEV_VOICE_BEGIN);
    CHECK_NEAR(vev[0].value, 1.0f, 1e-4f);
    sumi_voice_mapper_lower(vm, vev, nv, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 1);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_DROP);
    CHECK(sumi_deform_queue_at(q, 0)->as.drop.phase_base >= 1.0f);   // ink, not water
    CHECK_NEAR(sumi_deform_queue_at(q, 0)->as.drop.radius, 0.020f + 0.075f, 1e-4f);
    CHECK(drop_counter == 1);
    sumi_deform_queue_clear(q);

    // Velocity 32 (1/4 of 127ish) -> radius scales with sqrt.
    note.b = 32;
    nv = sumi_voice_mapper_normalize(vm, &note, 1, SUMI_INPUT_CLASSIC, &params, 1.0f, vev, 16);
    sumi_voice_mapper_lower(vm, vev, nv, &drop_counter, q);
    CHECK_NEAR(sumi_deform_queue_at(q, 0)->as.drop.radius,
               0.020f + 0.075f * std::sqrt(32.0f / 127.0f), 1e-4f);
    sumi_deform_queue_clear(q);

    // Global bend -> one shear tine on the delta.
    sumi_midi_event_t bend = {SUMI_MEV_BEND, 0, 0, 0, 1.0f};
    nv = sumi_voice_mapper_normalize(vm, &bend, 1, SUMI_INPUT_CLASSIC, &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_GLOBAL_BEND);
    sumi_voice_mapper_lower(vm, vev, nv, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 1);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_TINE);
    CHECK_NEAR(sumi_deform_queue_at(q, 0)->as.tine.magnitude, 0.015f, 1e-5f);
    sumi_deform_queue_clear(q);

    // Same bend again -> no delta -> no tine.
    nv = sumi_voice_mapper_normalize(vm, &bend, 1, SUMI_INPUT_CLASSIC, &params, 1.0f, vev, 16);
    sumi_voice_mapper_lower(vm, vev, nv, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 0);

    // Mod wheel -> coalesced to ONE vortex per update even for many CC1s.
    sumi_midi_event_t mods[3] = {
        {SUMI_MEV_CC, 0, 1, 10, 0.0f},
        {SUMI_MEV_CC, 0, 1, 60, 0.0f},
        {SUMI_MEV_CC, 0, 1, 127, 0.0f},
    };
    nv = sumi_voice_mapper_normalize(vm, mods, 3, SUMI_INPUT_CLASSIC, &params, 1.0f, vev, 16);
    CHECK(nv == 1);
    CHECK(vev[0].kind == SUMI_VEV_GLOBAL_CTL && vev[0].dimension == SUMI_CTL_VORTEX_STRENGTH);
    CHECK_NEAR(vev[0].value, 1.0f, 1e-4f);   // last one wins
    sumi_voice_mapper_lower(vm, vev, nv, &drop_counter, q);
    CHECK(sumi_deform_queue_count(q) == 1);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_VORTEX);
    sumi_deform_queue_clear(q);

    // CC64 rising edge -> PaperDip -> RESET deform; held/repeat -> nothing.
    sumi_midi_event_t sus_on  = {SUMI_MEV_CC, 0, 64, 127, 0.0f};
    sumi_midi_event_t sus_rep = {SUMI_MEV_CC, 0, 64, 100, 0.0f};
    sumi_midi_event_t sus_off = {SUMI_MEV_CC, 0, 64, 0, 0.0f};
    nv = sumi_voice_mapper_normalize(vm, &sus_on, 1, SUMI_INPUT_CLASSIC, &params, 1.0f, vev, 16);
    CHECK(nv == 1 && vev[0].kind == SUMI_VEV_PAPER_DIP);
    sumi_voice_mapper_lower(vm, vev, nv, &drop_counter, q);
    CHECK(sumi_deform_queue_at(q, 0)->type == SUMI_DEFORM_RESET);
    sumi_deform_queue_clear(q);
    nv = sumi_voice_mapper_normalize(vm, &sus_rep, 1, SUMI_INPUT_CLASSIC, &params, 1.0f, vev, 16);
    CHECK(nv == 0);   // still held: no new dip
    nv = sumi_voice_mapper_normalize(vm, &sus_off, 1, SUMI_INPUT_CLASSIC, &params, 1.0f, vev, 16);
    CHECK(nv == 0);   // release: no dip
    nv = sumi_voice_mapper_normalize(vm, &sus_on, 1, SUMI_INPUT_CLASSIC, &params, 1.0f, vev, 16);
    CHECK(nv == 1);   // second press: rising edge again

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
    test_pitch_positions();
    test_classic_mapping_to_deforms();

    if (g_failures == 0) {
        std::printf("OK: %d checks passed (normalizer/mapper, headless)\n", g_checks);
        return 0;
    }
    std::fprintf(stderr, "%d/%d checks FAILED\n", g_failures, g_checks);
    return 1;
}
