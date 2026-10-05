/* replay_tests.c — Phase 9 step 65 (DECISIONS_8 #22): the replay library's
 * headless suite, in strict C11 like the presets'. The round trip; the stage
 * (a byte staged before a frame lands in that frame, pushed in order); the
 * events' frames and order; the state dedupe; malformed and truncated input
 * refused; unknown lines skipped (the schema rule); the wall-time
 * re-bucketing of the negative test; save and load. No core: nothing here
 * pulls sumi_replay_apply. */
#include "sumi_replay.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, ...) do { if (cond) { printf("ok: "); printf(__VA_ARGS__); printf("\n"); } else { failures++; printf("FAIL: "); printf(__VA_ARGS__); printf("  (%s:%d)\n", __FILE__, __LINE__); } } while (0)

/* a push that records what it got */
typedef struct { uint8_t s[64], d1[64], d2[64], src[64]; uint32_t n; } got_t;
static void push_cb(void* u, uint8_t s, uint8_t d1, uint8_t d2, uint8_t src) {
    got_t* g = (got_t*)u;
    if (g->n < 64) { g->s[g->n] = s; g->d1[g->n] = d1; g->d2[g->n] = d2; g->src[g->n] = src; g->n++; }
}

/* a sink that logs one line per event */
typedef struct { char log[8192]; size_t len; uint32_t frame; } sink_log_t;
static void lg(sink_log_t* l, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(l->log + l->len, sizeof l->log - l->len, fmt, ap);
    va_end(ap);
    if (n > 0) l->len += (size_t)n;
}
static void sk_midi(void* u, uint8_t s, uint8_t d1, uint8_t d2, uint8_t src) { lg((sink_log_t*)u, "M%u:%u,%u,%u,%u ", ((sink_log_t*)u)->frame, s, d1, d2, src); }
static void sk_gesture(void* u, uint32_t k, const float* a, uint32_t n) { sink_log_t* l = (sink_log_t*)u; lg(l, "G%u:%u/%u", l->frame, k, n); for (uint32_t i = 0; i < n; i++) lg(l, ",%.9g", (double)a[i]); lg(l, " "); }
static void sk_state(void* u, const char* j, size_t n) { lg((sink_log_t*)u, "S%u:%u ", ((sink_log_t*)u)->frame, (unsigned)n); (void)j; }
static void sk_resize(void* u, uint32_t w, uint32_t h, float pr) { lg((sink_log_t*)u, "R%u:%ux%u@%.9g ", ((sink_log_t*)u)->frame, w, h, (double)pr); }
static void sk_dip(void* u) { lg((sink_log_t*)u, "D%u ", ((sink_log_t*)u)->frame); }
static const sumi_replay_sink_t SINK = { sk_midi, sk_gesture, sk_state, sk_resize, sk_dip };

static void drain_all(sumi_replay_t* r, sink_log_t* l, double* dts, double* ts, uint32_t cap) {
    double dt, t; uint32_t i = 0;
    l->len = 0; l->log[0] = 0;
    while (sumi_replay_next(r, &SINK, l, &dt, &t)) {
        if (i < cap) { dts[i] = dt; ts[i] = t; }
        i++;
        l->frame = sumi_replay_position(r);
    }
}

static const char* SESSION = "{\n  \"midi_sink_preset\": 1,\n  \"params\": { \"viscosity\": 0.5 }\n}\n";

static sumi_replay_rec_t* make_recording(got_t* got) {
    sumi_replay_info_t info; memset(&info, 0, sizeof info);
    strcpy(info.platform, "test"); strcpy(info.backend, "none"); strcpy(info.device, "replay_tests"); strcpy(info.app, "0.0.0-test");
    info.sumi_version = (1u << 16) | (5u << 8) | 0u; strcpy(info.recorded, "2026-10-05T00:00:00Z");
    info.width = 512; info.height = 320; info.pixel_ratio = 1.0f;
    sumi_replay_rec_t* r = sumi_replay_rec_create(&info, SESSION, 0);
    const double dt = 1.0 / 120.0;
    /* frame 0: a dip, the config bytes staged before the boundary */
    sumi_replay_rec_dip(r);
    sumi_replay_rec_midi(r, 100.001, 0xB0, 101, 0, 2);
    sumi_replay_rec_midi(r, 100.002, 0x91, 60, 100, 1);
    sumi_replay_rec_frame(r, 100.000, dt, push_cb, got);
    /* frame 1: a gesture before the boundary, one byte after frame 0's boundary */
    float tap[3] = { 0.5f, 0.25f, 0.06f };
    sumi_replay_rec_gesture(r, SUMI_REPLAY_G_TAP, tap, 3);
    sumi_replay_rec_midi(r, 100.009, 0xE1, 0, 64, 1);
    sumi_replay_rec_frame(r, 100.0083333333333333, dt, push_cb, got);
    /* frame 2: a state (twice: the dedupe), a resize, nothing staged */
    sumi_replay_rec_state(r, SESSION, 0);
    sumi_replay_rec_state(r, SESSION, 0);
    sumi_replay_rec_resize(r, 640, 400, 2.0f);
    sumi_replay_rec_frame(r, 100.0166666666666667, dt, push_cb, got);
    /* frame 3: a different state, a press with six args, a byte */
    sumi_replay_rec_state(r, "{ \"params\": { \"viscosity\": 0.7 } }", 0);
    float press[6] = { 0.5f, 0.25f, 0.06f, 0.5f, 0.0f, 1.0f / 120.0f };
    sumi_replay_rec_gesture(r, SUMI_REPLAY_G_PRESS, press, 6);
    sumi_replay_rec_midi(r, 100.030, 0x81, 60, 0, 1);
    sumi_replay_rec_frame(r, 100.025, dt, push_cb, got);
    return r;
}

int main(void) {
    printf("replay_tests (step 65)\n");
    /* ---- the recorder and the stage ---- */
    got_t got; memset(&got, 0, sizeof got);
    sumi_replay_rec_t* rec = make_recording(&got);
    CHECK(rec != NULL, "recorder created");
    CHECK(got.n == 4 && got.s[0] == 0xB0 && got.d1[0] == 101 && got.s[1] == 0x91 && got.s[2] == 0xE1 && got.s[3] == 0x81,
          "the staged bytes reached the core in order at the frame boundaries (%u pushed)", got.n);
    CHECK(sumi_replay_rec_frames(rec) == 4, "four frames closed");
    CHECK(fabs(sumi_replay_rec_seconds(rec) - 4.0 / 120.0) < 1e-12, "the recording's seconds are the sum of the dts");
    CHECK(sumi_replay_rec_events(rec) == 2 + 1 + 2 + 1 + 1 + 3, "the events: a dip, 2 bytes | a tap, a byte | ONE state (the repeat skipped), a resize | a state, a press, a byte = %u", sumi_replay_rec_events(rec));
    CHECK(sumi_replay_rec_dropped(rec) == 0 && !sumi_replay_rec_full(rec), "nothing dropped, not full");

    /* ---- the text ---- */
    const size_t need = sumi_replay_rec_write(rec, NULL, 0);
    char* text = (char*)malloc(need + 1);
    const size_t wrote = sumi_replay_rec_write(rec, text, need + 1);
    CHECK(wrote == need && strlen(text) == need, "the size query and the write agree (%u bytes)", (unsigned)need);
    CHECK(strncmp(text, "#sumi-replay 1\n", 15) == 0, "the magic line");
    CHECK(strstr(text, "\nplatform test\n") && strstr(text, "\ndevice replay_tests\n") && strstr(text, "\nsumi 1.5.0\n") && strstr(text, "\nsize 512 320 1\n"), "the header lines");
    CHECK(strstr(text, "#session\n{\n  \"midi_sink_preset\": 1,") != NULL, "the session block");
    CHECK(strstr(text, "\nF 0.0083333333333333332 0.000000\nD\nM 0.001000 176 101 0 2\nM 0.002000 145 60 100 1\n") != NULL, "frame 0: the boundary first, then its dip and its two bytes, wall times relative to the start");
    CHECK(strstr(text, "\nG 0 3 0.5 0.25 0.0599999987\nM 0.009000 225 0 64 1\n") != NULL, "frame 1: the tap then the byte, floats at %%.9g");
    CHECK(strstr(text, "\nS\n{\n  \"midi_sink_preset\": 1,\n  \"params\": { \"viscosity\": 0.5 }\n}\n#end\nR 640 400 2\n") != NULL, "frame 2: the state block, terminated, then the resize");
    CHECK(strstr(text, "\nG 3 6 0.5 0.25 0.0599999987 0.5 0 0.00833333377\n") != NULL, "frame 3: the press with six args");
    CHECK(strstr(text, "\n#eof\n") != NULL && text[need - 1] == '\n', "the end marker");
    {
        char small[64];
        const size_t n = sumi_replay_rec_write(rec, small, sizeof small);
        CHECK(n == need && strlen(small) == 63, "a short buffer is truncated and NUL-terminated, the full length reported");
    }

    /* ---- the player ---- */
    sumi_replay_t* pl = sumi_replay_open(text, 0);
    CHECK(pl != NULL, "the text opens");
    if (pl) {
        const sumi_replay_info_t* i = sumi_replay_info(pl);
        CHECK(strcmp(i->platform, "test") == 0 && strcmp(i->device, "replay_tests") == 0 && strcmp(i->app, "0.0.0-test") == 0 && i->sumi_version == ((1u << 16) | (5u << 8)) && i->width == 512 && i->height == 320 && i->pixel_ratio == 1.0f && strcmp(i->recorded, "2026-10-05T00:00:00Z") == 0,
              "the header round-trips");
        size_t sl = 0; const char* s = sumi_replay_session(pl, &sl);
        CHECK(s && sl == strlen(SESSION) && memcmp(s, SESSION, sl) == 0, "the session round-trips byte for byte");
        CHECK(sumi_replay_frame_count(pl) == 4 && sumi_replay_event_count(pl) == 10, "four frames, ten events");
        CHECK(fabs(sumi_replay_duration(pl) - 4.0 / 120.0) < 1e-15, "the duration is exact (dt at %%.17g)");
        CHECK(sumi_replay_peek_dt(pl) == 1.0 / 120.0, "the next dt is the recorded one, bit for bit");
        sink_log_t l; memset(&l, 0, sizeof l);
        double dts[8], ts[8];
        drain_all(pl, &l, dts, ts, 8);
        CHECK(strcmp(l.log, "D0 M0:176,101,0,2 M0:145,60,100,1 G1:0/3,0.5,0.25,0.0599999987 M1:225,0,64,1 S2:62 R2:640x400@2 S3:35 G3:3/6,0.5,0.25,0.0599999987,0.5,0,0.00833333377 M3:129,60,0,1 ") == 0,
              "the events come back in their frames, in order (a state block carries its closing newline): %s", l.log);
        CHECK(dts[0] == 1.0 / 120.0 && dts[3] == 1.0 / 120.0 && ts[0] == 0.0 && fabs(ts[1] - 0.008333) < 1e-9 && fabs(ts[3] - 0.025) < 1e-9, "the frames' dt and wall time");
        CHECK(sumi_replay_position(pl) == 4 && !sumi_replay_next(pl, &SINK, &l, NULL, NULL) && sumi_replay_peek_dt(pl) == 0.0, "the end: no more frames, peek 0");
        sumi_replay_rewind(pl);
        CHECK(sumi_replay_position(pl) == 0 && sumi_replay_elapsed(pl) == 0.0, "rewound");
        /* the wall-time re-bucketing (the negative test): 4 frames at 1/120 -> 2 at 1/60 */
        CHECK(sumi_replay_rebucket(pl, 1.0 / 60.0), "re-bucketed at 60 Hz");
        CHECK(sumi_replay_frame_count(pl) == 2 || sumi_replay_frame_count(pl) == 3, "the frames halved (%u)", sumi_replay_frame_count(pl));
        memset(&l, 0, sizeof l);
        drain_all(pl, &l, dts, ts, 8);
        /* bytes at 0.001, 0.002, 0.009 -> bucket 0 ([0, 16.7 ms)); the byte at 0.030 -> bucket 1; the gesture of frame 1 carries frame 0's boundary time -> bucket 0;
           frame 2's state/resize carry 16.7 ms -> bucket 1; frame 3's carry 25 ms -> bucket 1 */
        CHECK(strcmp(l.log, "D0 M0:176,101,0,2 M0:145,60,100,1 G0:0/3,0.5,0.25,0.0599999987 M0:225,0,64,1 S1:62 R1:640x400@2 S1:35 G1:3/6,0.5,0.25,0.0599999987,0.5,0,0.00833333377 M1:129,60,0,1 ") == 0,
              "every event re-bucketed by its wall time, the order kept: %s", l.log);
        CHECK(dts[0] == 1.0 / 60.0 && dts[1] == 1.0 / 60.0, "the re-bucketed frames run at the fixed dt");
        sumi_replay_close(pl);
    }

    /* ---- the schema rule, malformed input ---- */
    CHECK(sumi_replay_open("", 0) == NULL, "empty text refused");
    CHECK(sumi_replay_open("#sumi-preset 1\n", 0) == NULL, "a wrong magic refused");
    CHECK(sumi_replay_open("#sumi-replay 99\n#events\n#eof\n", 0) == NULL, "a newer schema refused");
    CHECK(sumi_replay_open("#sumi-replay 1\nplatform x\n#session\n{}\n", 0) == NULL, "a session block without its end refused");
    CHECK(sumi_replay_open("#sumi-replay 1\n#events\nF 0.01 0\nS\n{}\n", 0) == NULL, "a state block without its end refused");
    CHECK(sumi_replay_open("#sumi-replay 1\n#events\nF 0.01 0\nM 0 144 60 100 1\n", 0) == NULL, "a file without its end marker (truncated) refused");
    {
        sumi_replay_t* p = sumi_replay_open("#sumi-replay 1\ncolour blue\nplatform ios\n#events\nF 0.01 0\nX 1 2 3\nG 77 2 1 2\nM 0.001 144 60 100 0\nQ\n#eof\n", 0);
        CHECK(p != NULL, "unknown header lines, an unknown event line and an unknown gesture kind are skipped");
        if (p) {
            sink_log_t l; memset(&l, 0, sizeof l); double dts[2], ts[2];
            drain_all(p, &l, dts, ts, 2);
            CHECK(strcmp(l.log, "M0:144,60,100,0 ") == 0 && strcmp(sumi_replay_info(p)->platform, "ios") == 0 && dts[0] == 0.01, "the known survive: %s", l.log);
            CHECK(sumi_replay_session(p, NULL) == NULL, "no session block: NULL");
            sumi_replay_close(p);
        }
    }
    {
        /* the recorder refuses what it does not know too */
        got_t g; memset(&g, 0, sizeof g);
        sumi_replay_rec_t* r = sumi_replay_rec_create(NULL, NULL, 0);
        float a[2] = { 1, 2 };
        sumi_replay_rec_gesture(r, 999, a, 2);
        sumi_replay_rec_frame(r, 0.0, 0.01, push_cb, &g);
        CHECK(sumi_replay_rec_events(r) == 0 && sumi_replay_rec_frames(r) == 1, "an unknown gesture kind is not recorded; a frame closes anyway");
        const size_t n = sumi_replay_rec_write(r, NULL, 0);
        char* t = (char*)malloc(n + 1); sumi_replay_rec_write(r, t, n + 1);
        sumi_replay_t* p = sumi_replay_open(t, n);
        CHECK(p && sumi_replay_frame_count(p) == 1 && sumi_replay_session(p, NULL) == NULL && sumi_replay_info(p)->platform[0] == 0, "an empty recorder writes a file that opens: one frame, no session");
        sumi_replay_close(p); free(t); sumi_replay_rec_destroy(r);
    }
    {
        /* the stage: a producer that outruns the render thread by more than the ring drops, counted */
        sumi_replay_rec_t* r = sumi_replay_rec_create(NULL, NULL, 0);
        uint32_t ok = 0;
        for (uint32_t i = 0; i < 5000; i++) ok += sumi_replay_rec_midi(r, (double)i * 1e-4, 0x90, 60, 1, 1) ? 1u : 0u;
        got_t g; memset(&g, 0, sizeof g);
        const uint32_t pushed = sumi_replay_rec_frame(r, 1.0, 0.01, push_cb, &g);
        CHECK(ok == 4096 && pushed == 4096 && sumi_replay_rec_dropped(r) == 904, "the stage holds 4096 bytes; the rest are refused and counted (%u ok, %u pushed, %u dropped)", ok, pushed, sumi_replay_rec_dropped(r));
        ok = 0;
        for (uint32_t i = 0; i < 10; i++) ok += sumi_replay_rec_midi(r, 2.0, 0x90, 61, 1, 1) ? 1u : 0u;
        CHECK(ok == 10 && sumi_replay_rec_frame(r, 2.0, 0.01, NULL, NULL) == 10, "drained, the stage takes bytes again (a NULL push only records)");
        sumi_replay_rec_destroy(r);
    }

    /* ---- save and load ---- */
    {
        const char* path = "replay_tests_roundtrip.sumireplay";
        CHECK(sumi_replay_rec_save(rec, path), "saved to %s", path);
        sumi_replay_t* p = sumi_replay_load(path);
        CHECK(p != NULL && sumi_replay_frame_count(p) == 4 && sumi_replay_event_count(p) == 10, "loaded back: four frames, ten events");
        if (p) {
            sink_log_t l; memset(&l, 0, sizeof l); double dts[8], ts[8];
            drain_all(p, &l, dts, ts, 8);
            CHECK(strstr(l.log, "G3:3/6,0.5,0.25,0.0599999987,0.5,0,0.00833333377 M3:129,60,0,1 ") != NULL, "the file's last frame is the recording's");
            sumi_replay_close(p);
        }
        remove(path);
        CHECK(sumi_replay_load("replay_tests_does_not_exist.sumireplay") == NULL, "a missing file: NULL");
    }
    free(text);
    sumi_replay_rec_destroy(rec);

    printf("%s: %d failure(s)\n", failures ? "FAIL" : "OK", failures);
    return failures ? 1 : 0;
}
