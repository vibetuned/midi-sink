// hostmpe implementation (PHASE4_SPEC.md §3, §5.1). Header contract: pure C
// ABI; all calls externally serialized on the shell's single-producer queue.
#include "hostmpe.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const float KNEE = 0.03f;   // §3.2: radial deadband at 3% of R_max

// Working knee for a voice: 3% of R_max or the absolute floor, whichever is
// larger, expressed in d-space (÷ r_max). Capped so the knee never swallows
// the whole circle (continuity at d = 1 requires k < 1).
static float knee_for(float r_max) {
    float k = KNEE;
    if (r_max > 0.0f) {
        const float ka = HOSTMPE_KNEE_FLOOR_CH / r_max;
        if (ka > k) k = ka;
        if (k > 0.9f) k = 0.9f;
    }
    return k;
}
static float knee_g(float d, float k) {            // clamped: bounded axes
    if (!(d > k)) return 0.0f;                     // includes NaN -> 0
    if (d >= 1.0f) return 1.0f;
    return (d - k) / (1.0f - k);
}
static float bend_defl(float d, float k) {         // identity beyond (#10)
    if (!(d > k)) return 0.0f;
    if (d >= 1.0f) return d;
    return (d - k) / (1.0f - k);                   // both branches = 1 at d = 1
}

extern "C" {

float hostmpe_soft_knee(float d) { return knee_g(d, KNEE); }

void hostmpe_joystick_eff(float dx, float dy, float r_max,
                          float* out_x, float* out_y) {
    if (out_x) *out_x = 0.0f;
    if (out_y) *out_y = 0.0f;
    if (!(r_max > 0.0f)) return;
    const float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-9f) return;
    const float g = knee_g(len / r_max, knee_for(r_max));
    if (out_x) *out_x = dx / len * g;
    if (out_y) *out_y = dy / len * g;
}

float hostmpe_bend_deflection(float d) { return bend_defl(d, KNEE); }

uint16_t hostmpe_bend14(float semitones) {
    const float scaled = semitones / (float)HOSTMPE_BEND_RANGE * 8192.0f;
    // Round AFTER the 14-bit scale (DECISIONS_3 #1 — the parenthesization is
    // the whole point; rounding the ratio first quantizes to all-or-nothing).
    long pb = 8192L + lroundf(scaled);
    if (pb < 0) pb = 0;
    if (pb > 16383) pb = 16383;
    return (uint16_t)pb;
}

} // extern "C"

// ---- allocator + per-voice state -------------------------------------------

struct hostmpe_voice_t {
    bool     active;         // touch voice on this channel
    uint8_t  note;
    float    r_max;
    float    grad_x, grad_y; // lattice pitch gradient, semitones per unit (#17)
    uint64_t release_seq;    // LRU-by-release ordering (§5.1)
    // change-only emission (identical repeats are noise, not information)
    uint16_t last_bend;      // 14-bit
    uint8_t  last_pressure;  // implicit 0 at Note On (§3.3 rev)
    // external occupancy (§5.1 masking)
    int      ext_notes;      // active external note count on this channel
    double   ext_last_activity;
};

struct hostmpe_t {
    hostmpe_voice_t ch[HOSTMPE_MEMBERS + 1];   // [0] = master, unused by voices
    uint64_t seq;                              // global release counter
};

static hostmpe_msg_t msg3(uint8_t status, uint8_t d1, uint8_t d2) {
    hostmpe_msg_t m; m.status = status; m.data1 = d1; m.data2 = d2; return m;
}
static uint32_t put(hostmpe_msg_t* out, uint32_t count, uint32_t max, hostmpe_msg_t m) {
    if (count < max) out[count] = m;
    return count + 1;
}
static hostmpe_msg_t bend_msg(int ch, uint16_t v14) {
    return msg3((uint8_t)(0xE0 | ch), (uint8_t)(v14 & 0x7F), (uint8_t)(v14 >> 7));
}

// Sweep stale external holds: any channel silent past the timeout frees.
static void ext_sweep(hostmpe_t* h, double now) {
    for (int c = 1; c <= HOSTMPE_MEMBERS; c++) {
        if (h->ch[c].ext_notes > 0 &&
            now - h->ch[c].ext_last_activity > HOSTMPE_EXT_TIMEOUT_S) {
            h->ch[c].ext_notes = 0;
        }
    }
}

extern "C" {

hostmpe_t* hostmpe_create(void) {
    hostmpe_t* h = (hostmpe_t*)calloc(1, sizeof(hostmpe_t));
    if (!h) return NULL;
    // Initial LRU order = ascending channel index: the first pass round-robins
    // 2,3,4… (MIDI numbering) before any reuse.
    for (int c = 1; c <= HOSTMPE_MEMBERS; c++) h->ch[c].release_seq = (uint64_t)c;
    h->seq = HOSTMPE_MEMBERS + 1;
    return h;
}

void hostmpe_destroy(hostmpe_t* h) { free(h); }

uint32_t hostmpe_session_config(hostmpe_t* h, hostmpe_msg_t* out, uint32_t max) {
    (void)h;
    uint32_t n = 0;
    // MCM: RPN 6 = 15 member channels, on the master (lower zone), then null.
    n = put(out, n, max, msg3(0xB0, 101, 0));
    n = put(out, n, max, msg3(0xB0, 100, 6));
    n = put(out, n, max, msg3(0xB0, 6, HOSTMPE_MEMBERS));
    n = put(out, n, max, msg3(0xB0, 101, 127));
    n = put(out, n, max, msg3(0xB0, 100, 127));
    // RPN 0 = 48 semitones on every member channel (§5.3 — without this a DAW
    // assumes ±2 and every glide plays 24x too small), then null.
    for (int c = 1; c <= HOSTMPE_MEMBERS; c++) {
        const uint8_t st = (uint8_t)(0xB0 | c);
        n = put(out, n, max, msg3(st, 101, 0));
        n = put(out, n, max, msg3(st, 100, 0));
        n = put(out, n, max, msg3(st, 6, HOSTMPE_BEND_RANGE));
        n = put(out, n, max, msg3(st, 101, 127));
        n = put(out, n, max, msg3(st, 100, 127));
    }
    return n <= max ? n : max;
}

int32_t hostmpe_touch_begin(hostmpe_t* h, double now, uint8_t note, uint8_t velocity,
                            float r_max, float grad_x, float grad_y,
                            hostmpe_msg_t* out, uint32_t max, uint32_t* out_count) {
    if (out_count) *out_count = 0;
    if (!h || note > 127) return -1;
    ext_sweep(h, now);
    // §5.1: least-recently-released free channel, external holds masked.
    int best = -1;
    uint64_t best_seq = 0;
    for (int c = 1; c <= HOSTMPE_MEMBERS; c++) {
        if (h->ch[c].active || h->ch[c].ext_notes > 0) continue;
        if (best < 0 || h->ch[c].release_seq < best_seq) {
            best = c;
            best_seq = h->ch[c].release_seq;
        }
    }
    if (best < 0) return -1;   // saturation: silent drop, never steal

    hostmpe_voice_t* v = &h->ch[best];
    v->active = true;
    v->note = note;
    v->r_max = r_max > 0.0f ? r_max : 1.0f;
    v->grad_x = grad_x;
    v->grad_y = grad_y;
    v->last_bend = 8192;
    v->last_pressure = 0;   // §3.3 rev: pressure IS 0 at touch-down (implicit)

    uint32_t n = 0;
    n = put(out, n, max, bend_msg(best, 8192));              // center bend FIRST
    n = put(out, n, max, msg3((uint8_t)(0x90 | best), note,
                              velocity ? velocity : 1));     // then Note On
    if (out_count) *out_count = n <= max ? n : max;
    return best;
}

uint32_t hostmpe_touch_update(hostmpe_t* h, int32_t voice,
                              float dx, float dy,
                              hostmpe_msg_t* out, uint32_t max) {
    if (!h || voice < 1 || voice > HOSTMPE_MEMBERS || !h->ch[voice].active) return 0;
    hostmpe_voice_t* v = &h->ch[voice];
    uint32_t n = 0;

    const float len = sqrtf(dx * dx + dy * dy);
    // Bend, semitone-exact (§3.3, gradient form #17): the finger's 2D
    // displacement measured in the lattice's own pitch metric — deadband via
    // the UNCLAMPED deflection, absolute tracking beyond the circle (#10),
    // knee floored in absolute units so jitter stays silent on small cells
    // (#16).
    float bend_semitones = 0.0f;
    float ey = 0.0f;
    if (len > 1e-9f) {
        const float d = len / v->r_max;
        const float k = knee_for(v->r_max);
        bend_semitones = (bend_defl(d, k) / d) * (v->grad_x * dx + v->grad_y * dy);
        ey = knee_g(d, k) * (dy / len);           // clamped joystick, y component
    }
    const uint16_t pb = hostmpe_bend14(bend_semitones);
    if (pb != v->last_bend) {
        v->last_bend = pb;
        n = put(out, n, max, bend_msg(voice, pb));
    }
    // Y -> channel pressure, UPWARD only (§3.3 rev, DECISIONS_3 #19): screen
    // y grows down, so up is -ey; downward deflection clamps to 0. Fingers
    // emit no CC74 — the stylus matrix (Step 18) owns timbre.
    float p = -ey;
    if (!(p > 0.0f)) p = 0.0f;
    const uint8_t pv = (uint8_t)lroundf(p * 127.0f);
    if (pv != v->last_pressure) {
        v->last_pressure = pv;
        n = put(out, n, max, msg3((uint8_t)(0xD0 | voice), pv, 0));
    }
    return n <= max ? n : max;
}

uint32_t hostmpe_touch_end(hostmpe_t* h, int32_t voice, double now, uint8_t lift,
                           hostmpe_msg_t* out, uint32_t max) {
    if (!h || voice < 1 || voice > HOSTMPE_MEMBERS || !h->ch[voice].active) return 0;
    hostmpe_voice_t* v = &h->ch[voice];
    uint32_t n = 0;
    // §5.1 emit order: pressure 0 ALWAYS precedes Note Off.
    n = put(out, n, max, msg3((uint8_t)(0xD0 | voice), 0, 0));
    n = put(out, n, max, msg3((uint8_t)(0x80 | voice), v->note,
                              (uint8_t)(lift <= 127 ? lift : 64)));
    v->active = false;
    v->release_seq = ++h->seq;   // most-recently-released: last to be reused
    (void)now;
    return n <= max ? n : max;
}

uint32_t hostmpe_silence_zone(hostmpe_msg_t* out, uint32_t max) {
    uint32_t n = 0;
    for (int c = 0; c <= HOSTMPE_MEMBERS; c++) {
        const uint8_t st = (uint8_t)(0xB0 | c);
        n = put(out, n, max, msg3(st, 64, 0));    // sustain off
        n = put(out, n, max, msg3(st, 123, 0));   // all notes off
    }
    return n <= max ? n : max;
}

uint32_t hostmpe_panic(hostmpe_t* h, double now, hostmpe_msg_t* out, uint32_t max) {
    if (!h) return 0;
    uint32_t n = 0;
    // Release every live voice through the normal lift order so downstream
    // synths (and our own normalizer) see well-formed note ends.
    for (int c = 1; c <= HOSTMPE_MEMBERS; c++) {
        if (!h->ch[c].active) continue;
        hostmpe_msg_t m[2];
        const uint32_t k = hostmpe_touch_end(h, c, now, 64, m, 2);
        for (uint32_t i = 0; i < k; i++) n = put(out, n, max, m[i]);
    }
    // Then the belt-and-braces controllers, in case a sink missed an off.
    hostmpe_msg_t z[32];
    const uint32_t zn = hostmpe_silence_zone(z, 32);
    for (uint32_t i = 0; i < zn; i++) n = put(out, n, max, z[i]);
    return n <= max ? n : max;
}

void hostmpe_observe_external(hostmpe_t* h, double now,
                              uint8_t status, uint8_t data1, uint8_t data2) {
    if (!h) return;
    const uint8_t kind = status & 0xF0;
    const int c = status & 0x0F;
    if (c < 1 || c > HOSTMPE_MEMBERS) return;     // master / out-of-zone: ignore
    if (kind < 0x80 || kind > 0xE0) return;       // channel voice messages only
    hostmpe_voice_t* v = &h->ch[c];
    if (kind == 0x90 && data2 > 0) {
        v->ext_notes++;
        v->ext_last_activity = now;
    } else if (kind == 0x80 || (kind == 0x90 && data2 == 0)) {
        if (v->ext_notes > 0) v->ext_notes--;
        v->ext_last_activity = now;
    } else if (v->ext_notes > 0) {
        (void)data1;
        v->ext_last_activity = now;               // pressure/bend/CC refresh
    }
}

void hostmpe_external_clear(hostmpe_t* h) {
    if (!h) return;
    for (int c = 1; c <= HOSTMPE_MEMBERS; c++) h->ch[c].ext_notes = 0;
}

uint32_t hostmpe_active_voices(const hostmpe_t* h) {
    if (!h) return 0;
    uint32_t n = 0;
    for (int c = 1; c <= HOSTMPE_MEMBERS; c++) n += h->ch[c].active ? 1u : 0u;
    return n;
}

} // extern "C"

// ---- §5.3 outbound limiter --------------------------------------------------
// One slot per (channel, continuous dimension). Change-only + either
// per-slot rate decimation (latest-wins) or a global token budget with a
// round-robin cursor over the slots (fairness).

enum { DIM_BEND = 0, DIM_PRESSURE = 1, DIM_CC74 = 2, DIM_COUNT = 3 };
static const int LIM_SLOTS = 16 * DIM_COUNT;

struct lim_slot_t {
    uint32_t last_sent;      // change-only state; 0xFFFFFFFF = never sent
    bool     has_pending;
    hostmpe_msg_t pending;   // latest-wins
    double   last_emit;      // rate policy
};

struct hostmpe_limiter_t {
    bool       budget_mode;
    float      rate_hz;      // rate policy: per-slot ceiling
    float      budget_per_s; // budget policy: global msgs/s
    double     tokens;
    double     last_refill;
    int        cursor;       // budget round-robin over slots
    lim_slot_t slot[LIM_SLOTS];
};

// -1 = not a continuous dimension (passes through unfiltered).
static int lim_slot_index(const hostmpe_msg_t* m) {
    const int kind = m->status & 0xF0, ch = m->status & 0x0F;
    if (kind == 0xE0) return ch * DIM_COUNT + DIM_BEND;
    if (kind == 0xD0) return ch * DIM_COUNT + DIM_PRESSURE;
    if (kind == 0xB0 && m->data1 == 74) return ch * DIM_COUNT + DIM_CC74;
    return -1;
}
static uint32_t lim_value(const hostmpe_msg_t* m) {
    const int kind = m->status & 0xF0;
    if (kind == 0xE0) return (uint32_t)(m->data1 | (m->data2 << 7));
    if (kind == 0xD0) return m->data1;
    return m->data2;   // CC value
}

static hostmpe_limiter_t* lim_create(void) {
    hostmpe_limiter_t* l = (hostmpe_limiter_t*)calloc(1, sizeof(hostmpe_limiter_t));
    if (!l) return NULL;
    for (int i = 0; i < LIM_SLOTS; i++) {
        l->slot[i].last_sent = 0xFFFFFFFFu;
        l->slot[i].last_emit = -1e9;
    }
    return l;
}

static uint32_t lim_emit(hostmpe_limiter_t* l, lim_slot_t* s, double now,
                         const hostmpe_msg_t* m,
                         hostmpe_msg_t* out, uint32_t count, uint32_t max) {
    s->last_sent = lim_value(m);
    s->last_emit = now;
    if (count < max) out[count] = *m;
    return count + 1;
}

extern "C" {

hostmpe_limiter_t* hostmpe_limiter_create_rate(float rate_hz) {
    hostmpe_limiter_t* l = lim_create();
    if (l) l->rate_hz = rate_hz > 1.0f ? rate_hz : 1.0f;
    return l;
}

hostmpe_limiter_t* hostmpe_limiter_create_budget(float msgs_per_s) {
    hostmpe_limiter_t* l = lim_create();
    if (l) {
        l->budget_mode = true;
        l->budget_per_s = msgs_per_s > 1.0f ? msgs_per_s : 1.0f;
        l->tokens = 1.0;
        l->last_refill = -1.0;
    }
    return l;
}

void hostmpe_limiter_destroy(hostmpe_limiter_t* l) { free(l); }

uint32_t hostmpe_limiter_push(hostmpe_limiter_t* l, double now,
                              hostmpe_msg_t msg, bool exempt,
                              hostmpe_msg_t* out, uint32_t max) {
    if (!l) return 0;
    const int si = lim_slot_index(&msg);
    if (si < 0 || exempt) {
        // Never decimated. It still refreshes the slot's change-only state
        // (a center bend IS the bend value 8192) and clears stale pendings.
        if (si >= 0) {
            lim_slot_t* s = &l->slot[si];
            s->last_sent = lim_value(&msg);
            s->last_emit = now;
            s->has_pending = false;
        }
        if (max > 0) out[0] = msg;
        return 1;
    }
    lim_slot_t* s = &l->slot[si];
    const uint32_t val = lim_value(&msg);
    if (val == s->last_sent) {          // change-only: identical resend is noise
        s->has_pending = false;         // pending would only restate the sent value
        return 0;
    }
    s->pending = msg;                   // latest-wins
    s->has_pending = true;
    return hostmpe_limiter_drain(l, now, out, max);
}

uint32_t hostmpe_limiter_drain(hostmpe_limiter_t* l, double now,
                               hostmpe_msg_t* out, uint32_t max) {
    if (!l) return 0;
    uint32_t n = 0;
    if (!l->budget_mode) {
        const double period = 1.0 / (double)l->rate_hz;
        for (int i = 0; i < LIM_SLOTS; i++) {
            lim_slot_t* s = &l->slot[i];
            if (s->has_pending && now - s->last_emit >= period) {
                s->has_pending = false;
                n = lim_emit(l, s, now, &s->pending, out, n, max);
            }
        }
    } else {
        if (l->last_refill < 0.0) l->last_refill = now;
        l->tokens += (now - l->last_refill) * (double)l->budget_per_s;
        l->last_refill = now;
        const double cap = (double)l->budget_per_s * 0.1;   // small burst headroom
        if (l->tokens > cap) l->tokens = cap;
        // Round-robin over the slots: fairness across voices and dimensions.
        int visited = 0;
        while (l->tokens >= 1.0 && visited < LIM_SLOTS) {
            lim_slot_t* s = &l->slot[l->cursor];
            l->cursor = (l->cursor + 1) % LIM_SLOTS;
            visited++;
            if (s->has_pending) {
                s->has_pending = false;
                l->tokens -= 1.0;
                n = lim_emit(l, s, now, &s->pending, out, n, max);
                visited = 0;   // a hit restarts the scan allowance
            }
        }
    }
    return n <= max ? n : max;
}

} // extern "C"
