/* sumi_replay_bounce.c — THE OFFLINE BOUNCE (Phase 15 step 91, TO_PROJECT_SPEC §12.12; DECISIONS_14 #5): a replay
 * re-sounded without a device into a WAV. The bench's --replay-wav of step 65 was the seed; this is its one body,
 * for every shell. The library knows no sound engine: the caller hands the push that feeds its (deviceless) Voxo
 * and the render that pulls stereo float frames from it, and this unit walks the replay's frames in the recorded
 * order — each frame's bytes pushed, then that frame's worth of samples rendered in `block`-frame calls, the
 * fractional frame carried — and writes a 32-bit float stereo WAV as it goes. The visuals' events (gestures, state,
 * resizes, dips) are not applied: the bounce is the sound alone. Deterministic by construction: the same replay,
 * the same engine configuration, the same rate and block give the same bytes, so a bounce is the record of a
 * performance's sound as the engine would have played it on a perfect clock. */
#include "sumi_replay.h"

#include <stdio.h>
#include <string.h>

typedef struct { sumi_replay_push_fn push; void* user; uint32_t events; } bounce_ctx_t;

static void bounce_midi(void* user, uint8_t status, uint8_t d1, uint8_t d2, uint8_t src) {
    bounce_ctx_t* c = (bounce_ctx_t*)user;
    c->events++;
    if (c->push) c->push(c->user, status, d1, d2, src);
}

static void put_u32(FILE* f, uint32_t v) { unsigned char b[4] = { (unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24) }; fwrite(b, 1, 4, f); }
static void put_u16(FILE* f, uint16_t v) { unsigned char b[2] = { (unsigned char)v, (unsigned char)(v >> 8) }; fwrite(b, 1, 2, f); }

/* The header for `frames` stereo float frames at `rate` (44 bytes; the sizes patched at the end). */
static void wav_header(FILE* f, uint32_t rate, uint32_t frames) {
    const uint32_t data = frames * 2u * 4u;
    fwrite("RIFF", 1, 4, f); put_u32(f, 36u + data); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); put_u32(f, 16u); put_u16(f, 3u /* IEEE float */); put_u16(f, 2u);
    put_u32(f, rate); put_u32(f, rate * 2u * 4u); put_u16(f, 8u); put_u16(f, 32u);
    fwrite("data", 1, 4, f); put_u32(f, data);
}

static void say(char* why, size_t cap, const char* text) { if (why && cap) { strncpy(why, text, cap - 1); why[cap - 1] = 0; } }

bool sumi_replay_bounce(const char* replay_path, const char* wav_path, uint32_t rate, uint32_t block,
                        sumi_replay_push_fn push, void* push_user, sumi_replay_render_fn render, void* render_user,
                        sumi_replay_bounce_info_t* out, char* why, size_t why_cap) {
    if (out) memset(out, 0, sizeof *out);
    if (why && why_cap) why[0] = 0;
    if (!replay_path || !wav_path || !render) { say(why, why_cap, "the bounce needs a replay, a WAV path and a renderer"); return false; }
    if (rate == 0) rate = 48000u;
    if (block == 0 || block > 4096u) block = 128u;
    sumi_replay_t* r = sumi_replay_load(replay_path);
    if (!r) { say(why, why_cap, "not a replay file (or truncated)"); return false; }
    FILE* f = fopen(wav_path, "wb");
    if (!f) { sumi_replay_close(r); say(why, why_cap, "cannot write the WAV"); return false; }
    wav_header(f, rate, 0u);
    bounce_ctx_t ctx; ctx.push = push; ctx.user = push_user; ctx.events = 0;
    sumi_replay_sink_t sink; memset(&sink, 0, sizeof sink);
    sink.midi = bounce_midi;
    float lr[2u * 4096u];
    double carry = 0.0, seconds = 0.0, dt = 0.0, t = 0.0;
    uint32_t frames = 0u, nframes = 0u;
    float peak = 0.0f;
    bool ok = true;
    while (sumi_replay_next(r, &sink, &ctx, &dt, &t)) {
        nframes++;
        seconds += dt;
        carry += dt * (double)rate;
        uint32_t want = (uint32_t)carry;
        carry -= (double)want;
        while (want) {
            const uint32_t k = want > block ? block : want;
            memset(lr, 0, sizeof(float) * 2u * k);
            render(render_user, lr, k);
            for (uint32_t i = 0; i < 2u * k; i++) { const float a = lr[i] < 0.0f ? -lr[i] : lr[i]; if (a > peak) peak = a; }
            if (fwrite(lr, sizeof(float), 2u * k, f) != 2u * k) { ok = false; break; }
            frames += k;
            want -= k;
        }
        if (!ok) break;
    }
    sumi_replay_close(r);
    if (ok) { fseek(f, 0, SEEK_SET); wav_header(f, rate, frames); }
    if (fclose(f) != 0) ok = false;
    if (!ok) { say(why, why_cap, "the WAV could not be written in full"); return false; }
    if (out) { out->frames = frames; out->seconds = seconds; out->peak = peak; out->replay_frames = nframes; out->events = ctx.events; }
    return true;
}
