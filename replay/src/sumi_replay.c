/* sumi_replay.c — Phase 9 step 65 (QOL §1; DECISIONS_8 #22): the recorder,
 * the file and the player. C11, libc only. The format is replay/FORMAT.md;
 * the thinking is in the header. */
#include "sumi_replay.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---- compiler atomics for the one SPSC stage (no <stdatomic.h>: MSVC's C
   mode lacks it; the GCC/Clang builtins and the Interlocked intrinsics cover
   every toolchain the shells build with) ---------------------------------- */
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
static uint32_t at_load(volatile uint32_t* p)              { return (uint32_t)_InterlockedCompareExchange((volatile long*)p, 0, 0); }
static void     at_store(volatile uint32_t* p, uint32_t v) { _InterlockedExchange((volatile long*)p, (long)v); }
#else
static uint32_t at_load(volatile uint32_t* p)              { return __atomic_load_n(p, __ATOMIC_ACQUIRE); }
static void     at_store(volatile uint32_t* p, uint32_t v) { __atomic_store_n(p, v, __ATOMIC_RELEASE); }
#endif

/* ---- the events ---------------------------------------------------------- */

enum { EV_MIDI = 'M', EV_GESTURE = 'G', EV_STATE = 'S', EV_RESIZE = 'R', EV_DIP = 'D' };

typedef struct {
    uint8_t  kind;
    uint8_t  n;                 /* gesture args */
    uint8_t  s, d1, d2, src;    /* midi */
    uint32_t gkind;             /* gesture */
    uint32_t frame;
    double   t;                 /* wall seconds (host clock; relative once written) */
    union {
        float    args[SUMI_REPLAY_G_ARGS];
        struct { uint32_t off, len; } text;        /* state: into the arena */
        struct { uint32_t w, h; float ratio; } size;
    } u;
} ev_t;

typedef struct { double dt, t; uint32_t first, count; } frame_t;

#define EV_CAP      4000000u
#define FR_CAP      2000000u
#define ARENA_CAP   (64u * 1024u * 1024u)
#define STAGE_CAP   4096u                 /* a power of two */

typedef struct { double t; uint8_t s, d1, d2, src; } staged_t;

static bool grow(void** p, uint32_t* cap, uint32_t need, size_t elem, uint32_t hard) {
    if (need <= *cap) return true;
    if (need > hard) return false;
    uint32_t c = *cap ? *cap : 1024u;
    while (c < need) { c = c > hard / 2u ? hard : c * 2u; }
    void* n = realloc(*p, (size_t)c * elem);
    if (!n) return false;
    *p = n; *cap = c;
    return true;
}

static void copy_str(char* dst, size_t cap, const char* src) {
    size_t n = src ? strlen(src) : 0;
    if (n >= cap) n = cap - 1;
    if (n) memcpy(dst, src, n);
    dst[n] = 0;
    /* no line breaks in a header value: the file is line-based */
    for (size_t i = 0; i < n; i++) if (dst[i] == '\n' || dst[i] == '\r') dst[i] = ' ';
}

/* ---- the recorder --------------------------------------------------------- */

struct sumi_replay_rec_t {
    sumi_replay_info_t info;
    char*    session; uint32_t session_len;
    ev_t*    ev; uint32_t ev_n, ev_cap;
    frame_t* fr; uint32_t fr_n, fr_cap;
    char*    arena; uint32_t arena_n, arena_cap;
    uint32_t last_state_off, last_state_len;   /* the dedupe */
    double   t0; bool have_t0;
    double   seconds;
    uint32_t dropped;
    bool     full;
    /* the stage: the producer writes head, the render thread writes tail */
    staged_t stage[STAGE_CAP];
    volatile uint32_t head, tail;
};

sumi_replay_rec_t* sumi_replay_rec_create(const sumi_replay_info_t* info, const char* session_json, size_t len) {
    sumi_replay_rec_t* r = (sumi_replay_rec_t*)calloc(1, sizeof *r);
    if (!r) return NULL;
    if (info) r->info = *info;
    r->info.platform[sizeof r->info.platform - 1] = 0;
    r->info.backend[sizeof r->info.backend - 1] = 0;
    r->info.device[sizeof r->info.device - 1] = 0;
    r->info.app[sizeof r->info.app - 1] = 0;
    r->info.recorded[sizeof r->info.recorded - 1] = 0;
    copy_str(r->info.platform, sizeof r->info.platform, r->info.platform);
    copy_str(r->info.backend, sizeof r->info.backend, r->info.backend);
    copy_str(r->info.device, sizeof r->info.device, r->info.device);
    copy_str(r->info.app, sizeof r->info.app, r->info.app);
    copy_str(r->info.recorded, sizeof r->info.recorded, r->info.recorded);
    if (session_json && len == 0) len = strlen(session_json);
    if (session_json && len) {
        r->session = (char*)malloc(len + 1);
        if (!r->session) { free(r); return NULL; }
        memcpy(r->session, session_json, len);
        r->session[len] = 0;
        r->session_len = (uint32_t)len;
    }
    return r;
}

void sumi_replay_rec_destroy(sumi_replay_rec_t* r) {
    if (!r) return;
    free(r->session); free(r->ev); free(r->fr); free(r->arena);
    free(r);
}

bool sumi_replay_rec_midi(sumi_replay_rec_t* r, double t, uint8_t status, uint8_t d1, uint8_t d2, uint8_t src) {
    if (!r) return false;
    const uint32_t h = r->head;                    /* the producer's own */
    const uint32_t tl = at_load(&r->tail);
    if (h - tl >= STAGE_CAP) { r->dropped++; return false; }   /* a stalled render thread: the stage is full */
    staged_t* s = &r->stage[h & (STAGE_CAP - 1u)];
    s->t = t; s->s = status; s->d1 = d1; s->d2 = d2; s->src = src;
    at_store(&r->head, h + 1u);
    return true;
}

static ev_t* ev_new(sumi_replay_rec_t* r) {
    if (r->full) return NULL;
    if (!grow((void**)&r->ev, &r->ev_cap, r->ev_n + 1u, sizeof(ev_t), EV_CAP)) { r->full = true; r->dropped++; return NULL; }
    ev_t* e = &r->ev[r->ev_n++];
    memset(e, 0, sizeof *e);
    e->frame = r->fr_n;   /* the frame whose close comes next */
    e->t = r->fr_n ? r->fr[r->fr_n - 1u].t : (r->have_t0 ? r->t0 : 0.0);
    return e;
}

uint32_t sumi_replay_rec_frame(sumi_replay_rec_t* r, double t, double dt, sumi_replay_push_fn push, void* user) {
    if (!r) return 0;
    if (!r->have_t0) { r->t0 = t; r->have_t0 = true; }
    /* the staged bytes: to the core, in order, under this frame */
    uint32_t pushed = 0;
    uint32_t tl = r->tail;
    const uint32_t h = at_load(&r->head);
    while (tl != h) {
        const staged_t* s = &r->stage[tl & (STAGE_CAP - 1u)];
        if (push) push(user, s->s, s->d1, s->d2, s->src);
        ev_t* e = ev_new(r);
        if (e) { e->kind = EV_MIDI; e->s = s->s; e->d1 = s->d1; e->d2 = s->d2; e->src = s->src; e->t = s->t; }
        tl++; pushed++;
    }
    at_store(&r->tail, tl);
    /* the boundary */
    if (!r->full) {
        if (!grow((void**)&r->fr, &r->fr_cap, r->fr_n + 1u, sizeof(frame_t), FR_CAP)) { r->full = true; r->dropped++; }
        else {
            const uint32_t first = r->fr_n ? r->fr[r->fr_n - 1u].first + r->fr[r->fr_n - 1u].count : 0u;
            frame_t* f = &r->fr[r->fr_n++];
            f->dt = dt; f->t = t; f->first = first; f->count = r->ev_n - first;
            r->seconds += dt;
        }
    }
    return pushed;
}

void sumi_replay_rec_gesture(sumi_replay_rec_t* r, uint32_t kind, const float* args, uint32_t n) {
    if (!r || kind >= SUMI_REPLAY_G_COUNT) return;
    ev_t* e = ev_new(r);
    if (!e) return;
    e->kind = EV_GESTURE; e->gkind = kind;
    if (n > SUMI_REPLAY_G_ARGS) n = SUMI_REPLAY_G_ARGS;
    e->n = (uint8_t)n;
    for (uint32_t i = 0; i < n; i++) e->u.args[i] = args ? args[i] : 0.0f;
}

void sumi_replay_rec_state(sumi_replay_rec_t* r, const char* session_json, size_t len) {
    if (!r || !session_json) return;
    if (len == 0) len = strlen(session_json);
    if (len == 0 || len > ARENA_CAP) return;
    if (r->last_state_len == len && memcmp(r->arena + r->last_state_off, session_json, len) == 0) return;   /* the same state again */
    if (r->full) return;
    if (!grow((void**)&r->arena, &r->arena_cap, r->arena_n + (uint32_t)len, 1, ARENA_CAP)) { r->full = true; r->dropped++; return; }
    ev_t* e = ev_new(r);
    if (!e) return;
    e->kind = EV_STATE;
    e->u.text.off = r->arena_n; e->u.text.len = (uint32_t)len;
    memcpy(r->arena + r->arena_n, session_json, len);
    r->arena_n += (uint32_t)len;
    r->last_state_off = e->u.text.off; r->last_state_len = e->u.text.len;
}

void sumi_replay_rec_resize(sumi_replay_rec_t* r, uint32_t w, uint32_t h, float pixel_ratio) {
    if (!r) return;
    ev_t* e = ev_new(r);
    if (!e) return;
    e->kind = EV_RESIZE; e->u.size.w = w; e->u.size.h = h; e->u.size.ratio = pixel_ratio;
}

void sumi_replay_rec_dip(sumi_replay_rec_t* r) {
    if (!r) return;
    ev_t* e = ev_new(r);
    if (e) e->kind = EV_DIP;
}

uint32_t sumi_replay_rec_flush(sumi_replay_rec_t* r, sumi_replay_push_fn push, void* user) {
    if (!r) return 0;
    uint32_t n = 0;
    uint32_t tl = r->tail;
    const uint32_t h = at_load(&r->head);
    while (tl != h) {
        const staged_t* s = &r->stage[tl & (STAGE_CAP - 1u)];
        if (push) push(user, s->s, s->d1, s->d2, s->src);
        tl++; n++;
    }
    at_store(&r->tail, tl);
    return n;
}

void sumi_replay_timestamp(char* out, size_t cap) {
    if (!out || cap == 0) return;
    out[0] = 0;
    const time_t now = time(NULL);
    struct tm tmv;
#if defined(_MSC_VER)
    if (gmtime_s(&tmv, &now) != 0) return;
#else
    if (!gmtime_r(&now, &tmv)) return;
#endif
    if (strftime(out, cap, "%Y-%m-%dT%H:%M:%SZ", &tmv) == 0) out[0] = 0;
}

uint32_t sumi_replay_rec_frames(const sumi_replay_rec_t* r)  { return r ? r->fr_n : 0u; }
double   sumi_replay_rec_seconds(const sumi_replay_rec_t* r) { return r ? r->seconds : 0.0; }
uint32_t sumi_replay_rec_events(const sumi_replay_rec_t* r)  { return r ? r->ev_n : 0u; }
uint32_t sumi_replay_rec_dropped(const sumi_replay_rec_t* r) { return r ? r->dropped : 0u; }
bool     sumi_replay_rec_full(const sumi_replay_rec_t* r)    { return r ? r->full : false; }

/* ---- the writer ----------------------------------------------------------- */

typedef struct { char* out; size_t cap; size_t len; } wr_t;

static void w_write(wr_t* w, const char* s, size_t n) {
    if (w->out && w->len < w->cap) {
        size_t room = w->cap - 1u - w->len;
        size_t k = n < room ? n : room;
        if (k) memcpy(w->out + w->len, s, k);
    }
    w->len += n;
}
static void w_puts(wr_t* w, const char* s) { w_write(w, s, strlen(s)); }
static void w_fmt(wr_t* w, const char* fmt, ...) {
    char buf[320];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n >= sizeof buf) n = (int)sizeof buf - 1;
    w_write(w, buf, (size_t)n);
}

static void write_events(wr_t* w, const ev_t* ev, const frame_t* fr, uint32_t fr_n, const char* arena, double t0) {
    for (uint32_t i = 0; i < fr_n; i++) {
        const frame_t* f = &fr[i];
        w_fmt(w, "F %.17g %.6f\n", f->dt, f->t - t0);
        for (uint32_t k = f->first; k < f->first + f->count; k++) {
            const ev_t* e = &ev[k];
            switch (e->kind) {
            case EV_MIDI:
                w_fmt(w, "M %.6f %u %u %u %u\n", e->t - t0, e->s, e->d1, e->d2, e->src);
                break;
            case EV_GESTURE:
                w_fmt(w, "G %u %u", e->gkind, e->n);
                for (uint32_t a = 0; a < e->n; a++) w_fmt(w, " %.9g", (double)e->u.args[a]);
                w_puts(w, "\n");
                break;
            case EV_STATE:
                w_puts(w, "S\n");
                w_write(w, arena + e->u.text.off, e->u.text.len);
                if (e->u.text.len && arena[e->u.text.off + e->u.text.len - 1u] != '\n') w_puts(w, "\n");
                w_puts(w, "#end\n");
                break;
            case EV_RESIZE:
                w_fmt(w, "R %u %u %.9g\n", e->u.size.w, e->u.size.h, (double)e->u.size.ratio);
                break;
            case EV_DIP:
                w_puts(w, "D\n");
                break;
            default: break;
            }
        }
    }
}

size_t sumi_replay_rec_write(const sumi_replay_rec_t* r, char* out, size_t cap) {
    wr_t w = { out, cap, 0 };
    if (out && cap) out[0] = 0;
    if (!r) return 0;
    w_fmt(&w, "#sumi-replay %u\n", SUMI_REPLAY_SCHEMA);
    w_fmt(&w, "platform %s\n", r->info.platform);
    w_fmt(&w, "backend %s\n", r->info.backend);
    w_fmt(&w, "device %s\n", r->info.device);
    w_fmt(&w, "app %s\n", r->info.app);
    w_fmt(&w, "sumi %u.%u.%u\n", r->info.sumi_version >> 16, (r->info.sumi_version >> 8) & 0xFFu, r->info.sumi_version & 0xFFu);
    w_fmt(&w, "recorded %s\n", r->info.recorded);
    w_fmt(&w, "size %u %u %.9g\n", r->info.width, r->info.height, (double)r->info.pixel_ratio);
    w_fmt(&w, "frames %u\n", r->fr_n);
    w_fmt(&w, "events %u\n", r->ev_n);
    w_fmt(&w, "seconds %.6f\n", r->seconds);
    w_puts(&w, "#session\n");
    if (r->session_len) { w_write(&w, r->session, r->session_len); if (r->session[r->session_len - 1u] != '\n') w_puts(&w, "\n"); }
    w_puts(&w, "#events\n");
    write_events(&w, r->ev, r->fr, r->fr_n, r->arena, r->have_t0 ? r->t0 : 0.0);
    w_puts(&w, "#eof\n");
    if (out && cap) out[w.len < cap ? w.len : cap - 1u] = 0;
    return w.len;
}

bool sumi_replay_rec_save(const sumi_replay_rec_t* r, const char* path) {
    if (!r || !path) return false;
    const size_t need = sumi_replay_rec_write(r, NULL, 0);
    char* buf = (char*)malloc(need + 1u);
    if (!buf) return false;
    sumi_replay_rec_write(r, buf, need + 1u);
    FILE* f = fopen(path, "wb");
    bool ok = f != NULL;
    if (ok) { ok = fwrite(buf, 1, need, f) == need; if (fclose(f) != 0) ok = false; }
    free(buf);
    return ok;
}

/* ---- the player ----------------------------------------------------------- */

struct sumi_replay_t {
    sumi_replay_info_t info;
    char*    session; uint32_t session_len;
    ev_t*    ev; uint32_t ev_n, ev_cap;
    frame_t* fr; uint32_t fr_n, fr_cap;
    char*    arena; uint32_t arena_n, arena_cap;
    double   duration;
    uint32_t pos;
    double   elapsed;
};

typedef struct { const char* p; const char* end; } rd_t;

/* the next line [s, e) without its terminator; false at the end */
static bool rd_line(rd_t* r, const char** s, const char** e) {
    if (r->p >= r->end) return false;
    *s = r->p;
    const char* nl = (const char*)memchr(r->p, '\n', (size_t)(r->end - r->p));
    if (!nl) { *e = r->end; r->p = r->end; }
    else { *e = nl; r->p = nl + 1; }
    if (*e > *s && (*e)[-1] == '\r') (*e)--;
    return true;
}
static bool line_is(const char* s, const char* e, const char* word) {
    const size_t n = strlen(word);
    return (size_t)(e - s) == n && memcmp(s, word, n) == 0;
}
static bool line_starts(const char* s, const char* e, const char* word) {
    const size_t n = strlen(word);
    return (size_t)(e - s) >= n && memcmp(s, word, n) == 0 && ((size_t)(e - s) == n || s[n] == ' ');
}
static void line_value(const char* s, const char* e, size_t skip, char* out, size_t cap) {
    const char* v = s + skip;
    while (v < e && *v == ' ') v++;
    size_t n = (size_t)(e - v);
    if (n >= cap) n = cap - 1;
    if (n) memcpy(out, v, n);
    out[n] = 0;
}

static ev_t* pl_ev_new(sumi_replay_t* r) {
    if (!grow((void**)&r->ev, &r->ev_cap, r->ev_n + 1u, sizeof(ev_t), EV_CAP)) return NULL;
    ev_t* e = &r->ev[r->ev_n++];
    memset(e, 0, sizeof *e);
    e->frame = r->fr_n ? r->fr_n - 1u : 0u;
    e->t = r->fr_n ? r->fr[r->fr_n - 1u].t : 0.0;
    return e;
}

/* a text block up to the line `terminator` (exclusive); copies into the arena */
static bool pl_block(sumi_replay_t* r, rd_t* rd, const char* terminator, uint32_t* off, uint32_t* len) {
    const char* start = rd->p;
    const char* s; const char* e;
    const char* stop = NULL;
    while (rd_line(rd, &s, &e)) {
        if (line_is(s, e, terminator)) { stop = s; break; }
    }
    if (!stop) return false;                     /* truncated */
    const uint32_t n = (uint32_t)(stop - start);
    if (!grow((void**)&r->arena, &r->arena_cap, r->arena_n + n + 1u, 1, ARENA_CAP)) return false;
    memcpy(r->arena + r->arena_n, start, n);
    *off = r->arena_n; *len = n;
    r->arena_n += n;
    r->arena[r->arena_n] = 0;
    return true;
}

sumi_replay_t* sumi_replay_open(const char* text, size_t len) {
    if (!text) return NULL;
    if (len == 0) len = strlen(text);
    rd_t rd = { text, text + len };
    const char* s; const char* e;
    if (!rd_line(&rd, &s, &e) || !line_starts(s, e, "#sumi-replay")) return NULL;
    {
        char v[32]; line_value(s, e, strlen("#sumi-replay"), v, sizeof v);
        const unsigned long schema = strtoul(v, NULL, 10);
        if (schema == 0 || schema > SUMI_REPLAY_SCHEMA) return NULL;
    }
    sumi_replay_t* r = (sumi_replay_t*)calloc(1, sizeof *r);
    if (!r) return NULL;
    r->info.pixel_ratio = 1.0f;
    bool have_events = false;
    uint32_t session_off = 0;
    /* the header */
    while (rd_line(&rd, &s, &e)) {
        char v[96];
        if (line_is(s, e, "#session")) {
            uint32_t off = 0, n = 0;
            if (!pl_block(r, &rd, "#events", &off, &n)) { sumi_replay_close(r); return NULL; }
            session_off = off;   /* the arena may still move: resolved once it is final */
            r->session_len = n;
            have_events = true;
            break;
        }
        if (line_is(s, e, "#events")) { have_events = true; break; }
        if (line_starts(s, e, "platform")) line_value(s, e, 8, r->info.platform, sizeof r->info.platform);
        else if (line_starts(s, e, "backend")) line_value(s, e, 7, r->info.backend, sizeof r->info.backend);
        else if (line_starts(s, e, "device")) line_value(s, e, 6, r->info.device, sizeof r->info.device);
        else if (line_starts(s, e, "app")) line_value(s, e, 3, r->info.app, sizeof r->info.app);
        else if (line_starts(s, e, "recorded")) line_value(s, e, 8, r->info.recorded, sizeof r->info.recorded);
        else if (line_starts(s, e, "sumi")) {
            line_value(s, e, 4, v, sizeof v);
            unsigned a = 0, b = 0, c = 0;
            if (sscanf(v, "%u.%u.%u", &a, &b, &c) == 3) r->info.sumi_version = (a << 16) | ((b & 0xFFu) << 8) | (c & 0xFFu);
        } else if (line_starts(s, e, "size")) {
            line_value(s, e, 4, v, sizeof v);
            unsigned w = 0, h = 0; float pr = 1.0f;
            if (sscanf(v, "%u %u %f", &w, &h, &pr) >= 2) { r->info.width = w; r->info.height = h; r->info.pixel_ratio = pr > 0.0f ? pr : 1.0f; }
        } else if (line_starts(s, e, "frames")) {
            line_value(s, e, 6, v, sizeof v);
            const unsigned long n = strtoul(v, NULL, 10);
            if (n && n <= FR_CAP) grow((void**)&r->fr, &r->fr_cap, (uint32_t)n, sizeof(frame_t), FR_CAP);
        } else if (line_starts(s, e, "events")) {
            line_value(s, e, 6, v, sizeof v);
            const unsigned long n = strtoul(v, NULL, 10);
            if (n && n <= EV_CAP) grow((void**)&r->ev, &r->ev_cap, (uint32_t)n, sizeof(ev_t), EV_CAP);
        }
        /* unknown header lines: skipped */
    }
    if (!have_events) { sumi_replay_close(r); return NULL; }
    /* the events */
    bool eof = false;
    char line[512];
    while (rd_line(&rd, &s, &e)) {
        if (line_is(s, e, "#eof")) { eof = true; break; }
        if (e == s) continue;
        const char k = *s;
        if (k == 'S' && e - s == 1) {
            ev_t* ev = pl_ev_new(r);
            if (!ev) { sumi_replay_close(r); return NULL; }
            ev->kind = EV_STATE;
            if (!pl_block(r, &rd, "#end", &ev->u.text.off, &ev->u.text.len)) { sumi_replay_close(r); return NULL; }
            continue;
        }
        size_t n = (size_t)(e - s);
        if (n >= sizeof line) n = sizeof line - 1u;
        memcpy(line, s, n); line[n] = 0;
        if (k == 'F') {
            char* p = line + 1;
            const double dt = strtod(p, &p);
            const double t = strtod(p, &p);
            if (!grow((void**)&r->fr, &r->fr_cap, r->fr_n + 1u, sizeof(frame_t), FR_CAP)) { sumi_replay_close(r); return NULL; }
            frame_t* f = &r->fr[r->fr_n++];
            f->dt = dt; f->t = t; f->first = r->ev_n; f->count = 0;
            r->duration += dt;
        } else if (k == 'M') {
            char* p = line + 1;
            const double t = strtod(p, &p);
            const unsigned long st = strtoul(p, &p, 10), d1 = strtoul(p, &p, 10), d2 = strtoul(p, &p, 10), src = strtoul(p, &p, 10);
            ev_t* ev = pl_ev_new(r);
            if (!ev) { sumi_replay_close(r); return NULL; }
            ev->kind = EV_MIDI; ev->t = t; ev->s = (uint8_t)st; ev->d1 = (uint8_t)d1; ev->d2 = (uint8_t)d2; ev->src = (uint8_t)src;
        } else if (k == 'G') {
            char* p = line + 1;
            const unsigned long kind = strtoul(p, &p, 10);
            unsigned long na = strtoul(p, &p, 10);
            if (kind >= SUMI_REPLAY_G_COUNT) continue;   /* a gesture this library does not know */
            if (na > SUMI_REPLAY_G_ARGS) na = SUMI_REPLAY_G_ARGS;
            ev_t* ev = pl_ev_new(r);
            if (!ev) { sumi_replay_close(r); return NULL; }
            ev->kind = EV_GESTURE; ev->gkind = (uint32_t)kind; ev->n = (uint8_t)na;
            for (unsigned long a = 0; a < na; a++) ev->u.args[a] = strtof(p, &p);
        } else if (k == 'R') {
            char* p = line + 1;
            const unsigned long w = strtoul(p, &p, 10), h = strtoul(p, &p, 10);
            const float pr = strtof(p, &p);
            ev_t* ev = pl_ev_new(r);
            if (!ev) { sumi_replay_close(r); return NULL; }
            ev->kind = EV_RESIZE; ev->u.size.w = (uint32_t)w; ev->u.size.h = (uint32_t)h; ev->u.size.ratio = pr > 0.0f ? pr : 1.0f;
        } else if (k == 'D' && n == 1) {
            ev_t* ev = pl_ev_new(r);
            if (!ev) { sumi_replay_close(r); return NULL; }
            ev->kind = EV_DIP;
        }
        /* an unknown line: skipped (the schema rule) */
    }
    if (!eof) { sumi_replay_close(r); return NULL; }   /* truncated */
    /* the frames' counts (events before the first F belong to frame 0 — only a recorder bug would write them) */
    for (uint32_t i = 0; i < r->fr_n; i++) {
        const uint32_t next_first = i + 1u < r->fr_n ? r->fr[i + 1u].first : r->ev_n;
        r->fr[i].count = next_first - r->fr[i].first;
        for (uint32_t k = r->fr[i].first; k < next_first; k++) r->ev[k].frame = i;
    }
    if (r->fr_n && r->fr[0].first > 0) { const uint32_t extra = r->fr[0].first; r->fr[0].first = 0; r->fr[0].count += extra; for (uint32_t k = 0; k < extra; k++) r->ev[k].frame = 0; }
    /* the session pointer, now that the arena is final */
    r->session = r->session_len ? r->arena + session_off : NULL;
    return r;
}

sumi_replay_t* sumi_replay_load(const char* path) {
    if (!path) return NULL;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    const long n = ftell(f);
    if (n < 0 || n > (long)(512u * 1024u * 1024u)) { fclose(f); return NULL; }
    rewind(f);
    char* buf = (char*)malloc((size_t)n + 1u);
    if (!buf) { fclose(f); return NULL; }
    const size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = 0;
    sumi_replay_t* r = sumi_replay_open(buf, got);
    free(buf);
    return r;
}

void sumi_replay_close(sumi_replay_t* r) {
    if (!r) return;
    free(r->ev); free(r->fr); free(r->arena);
    free(r);
}

const sumi_replay_info_t* sumi_replay_info(const sumi_replay_t* r) { return r ? &r->info : NULL; }
const char* sumi_replay_session(const sumi_replay_t* r, size_t* len) {
    if (len) *len = r ? r->session_len : 0u;
    return r ? r->session : NULL;
}
uint32_t sumi_replay_frame_count(const sumi_replay_t* r) { return r ? r->fr_n : 0u; }
uint32_t sumi_replay_event_count(const sumi_replay_t* r) { return r ? r->ev_n : 0u; }
double   sumi_replay_duration(const sumi_replay_t* r)    { return r ? r->duration : 0.0; }
uint32_t sumi_replay_position(const sumi_replay_t* r)    { return r ? r->pos : 0u; }
double   sumi_replay_elapsed(const sumi_replay_t* r)     { return r ? r->elapsed : 0.0; }
double   sumi_replay_peek_dt(const sumi_replay_t* r)     { return (r && r->pos < r->fr_n) ? r->fr[r->pos].dt : 0.0; }
void     sumi_replay_rewind(sumi_replay_t* r)            { if (r) { r->pos = 0; r->elapsed = 0.0; } }

bool sumi_replay_next(sumi_replay_t* r, const sumi_replay_sink_t* sink, void* user, double* dt_out, double* t_out) {
    if (!r || r->pos >= r->fr_n) return false;
    const frame_t* f = &r->fr[r->pos];
    for (uint32_t k = f->first; k < f->first + f->count; k++) {
        const ev_t* e = &r->ev[k];
        if (!sink) continue;
        switch (e->kind) {
        case EV_MIDI:    if (sink->midi)    sink->midi(user, e->s, e->d1, e->d2, e->src); break;
        case EV_GESTURE: if (sink->gesture) sink->gesture(user, e->gkind, e->u.args, e->n); break;
        case EV_STATE:   if (sink->state)   sink->state(user, r->arena + e->u.text.off, e->u.text.len); break;
        case EV_RESIZE:  if (sink->resize)  sink->resize(user, e->u.size.w, e->u.size.h, e->u.size.ratio); break;
        case EV_DIP:     if (sink->dip)     sink->dip(user); break;
        default: break;
        }
    }
    if (dt_out) *dt_out = f->dt;
    if (t_out) *t_out = f->t;
    r->elapsed += f->dt;
    r->pos++;
    return true;
}

bool sumi_replay_rebucket(sumi_replay_t* r, double fixed_dt) {
    if (!r || !(fixed_dt > 0.0) || r->fr_n == 0) return false;
    /* the span: from the first boundary to the last boundary + its dt */
    const double t0 = r->fr[0].t;
    double t_end = r->fr[r->fr_n - 1u].t + r->fr[r->fr_n - 1u].dt;
    for (uint32_t k = 0; k < r->ev_n; k++) if (r->ev[k].t + 1e-9 > t_end) t_end = r->ev[k].t + 1e-9;
    double nf = (t_end - t0) / fixed_dt;
    if (nf < 1.0) nf = 1.0;
    const uint32_t n = (uint32_t)(nf + 0.999999);
    if (n == 0 || n > FR_CAP) return false;
    /* bucket every event by its wall time, stable in the recorded order */
    uint32_t* bucket = (uint32_t*)malloc((size_t)r->ev_n * sizeof(uint32_t) + 1u);
    uint32_t* count = (uint32_t*)calloc((size_t)n + 1u, sizeof(uint32_t));
    ev_t* sorted = (ev_t*)malloc((size_t)r->ev_n * sizeof(ev_t) + 1u);
    frame_t* fr = (frame_t*)calloc((size_t)n, sizeof(frame_t));
    if (!bucket || !count || !sorted || !fr) { free(bucket); free(count); free(sorted); free(fr); return false; }
    for (uint32_t k = 0; k < r->ev_n; k++) {
        double b = (r->ev[k].t - t0) / fixed_dt;
        if (b < 0.0) b = 0.0;
        uint32_t bi = (uint32_t)b;
        if (bi >= n) bi = n - 1u;
        bucket[k] = bi; count[bi]++;
    }
    uint32_t acc = 0;
    for (uint32_t i = 0; i < n; i++) { fr[i].dt = fixed_dt; fr[i].t = t0 + (double)i * fixed_dt; fr[i].first = acc; fr[i].count = count[i]; acc += count[i]; count[i] = fr[i].first; }
    for (uint32_t k = 0; k < r->ev_n; k++) { const uint32_t dst = count[bucket[k]]++; sorted[dst] = r->ev[k]; sorted[dst].frame = bucket[k]; }
    free(r->ev); r->ev = sorted; r->ev_cap = r->ev_n;
    free(r->fr); r->fr = fr; r->fr_n = n; r->fr_cap = n;
    r->duration = (double)n * fixed_dt;
    free(bucket); free(count);
    sumi_replay_rewind(r);
    return true;
}
