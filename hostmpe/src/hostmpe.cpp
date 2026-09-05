// hostmpe implementation (PROJECT_SPEC.md §8.3, §8.5). Header contract: pure C
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
    uint8_t  last_poly;      // 0xA0 swirl value (v0.4 bipolar Y, PROJECT_SPEC.md §8.3)
    // §7 stylus legato (Step 21, #39) — pen voices only.
    bool     is_pen;
    uint8_t  last_cc74;      // change-only slide (center 64)
    // external occupancy (§5.1 masking)
    int      ext_notes;      // active external note count on this channel
    double   ext_last_activity;
};

// #66 echo ring: what we delivered to transports, so the same bytes arriving
// back from a mirroring link can be recognised and dropped.
#define HOSTMPE_ECHO_SLOTS 512
struct hostmpe_echo_t {
    double  t;
    uint8_t status, d1, d2;
    bool    used;      // consumed by a match: one echo dropped per emission
};

struct hostmpe_t {
    hostmpe_voice_t ch[HOSTMPE_MEMBERS + 1];   // [0] = master, unused by voices
    uint64_t seq;                              // global release counter
    hostmpe_echo_t echo[HOSTMPE_ECHO_SLOTS];
    uint32_t echo_next;                        // ring cursor
    uint32_t echo_dropped;
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
    v->last_poly = 0;       // v0.4 bipolar Y: both halves start at center
    v->is_pen = false;      // §7: hostmpe_pen_begin marks pen voices
    v->last_cc74 = 64;      // §3.3 stylus center

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
    // Y is BIPOLAR (v0.4, PROJECT_SPEC.md §8.3): ONE radial soft-knee serves both
    // halves; whichever half is engaged carries |Δy_eff|·127. Up (screen -y)
    // -> channel pressure 0xD0 (the ink feed); down -> polyphonic key
    // pressure 0xA0 on the voice's member channel, keyed by ITS NOTE (the
    // Lamb-Oseen swirl). Touch-down = center = both zeros; crossing the
    // center releases the departing half through zero (change-only makes
    // that exactly one message). Fingers emit no CC74 — the stylus owns
    // timbre.
    float up = -ey;
    if (!(up > 0.0f)) up = 0.0f;
    float down = ey;
    if (!(down > 0.0f)) down = 0.0f;
    const uint8_t pv = (uint8_t)lroundf(up * 127.0f);
    if (pv != v->last_pressure) {
        v->last_pressure = pv;
        n = put(out, n, max, msg3((uint8_t)(0xD0 | voice), pv, 0));
    }
    const uint8_t sv = (uint8_t)lroundf(down * 127.0f);
    if (sv != v->last_poly) {
        v->last_poly = sv;
        n = put(out, n, max, msg3((uint8_t)(0xA0 | voice), v->note, sv));
    }
    return n <= max ? n : max;
}

uint32_t hostmpe_touch_end(hostmpe_t* h, int32_t voice, double now, uint8_t lift,
                           hostmpe_msg_t* out, uint32_t max) {
    if (!h || voice < 1 || voice > HOSTMPE_MEMBERS || !h->ch[voice].active) return 0;
    hostmpe_voice_t* v = &h->ch[voice];
    uint32_t n = 0;
    // §5.1 emit order: pressure 0 ALWAYS precedes Note Off; an engaged swirl
    // half goes home too (v0.4 — a synth latching poly AT must not stick).
    n = put(out, n, max, msg3((uint8_t)(0xD0 | voice), 0, 0));
    if (v->last_poly > 0) {
        n = put(out, n, max, msg3((uint8_t)(0xA0 | voice), v->note, 0));
    }
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

void hostmpe_echo_record(hostmpe_t* h, double now,
                         uint8_t status, uint8_t d1, uint8_t d2) {
    if (!h) return;
    hostmpe_echo_t* e = &h->echo[h->echo_next];
    h->echo_next = (h->echo_next + 1) % HOSTMPE_ECHO_SLOTS;
    e->t = now;
    e->status = status;
    e->d1 = d1;
    e->d2 = d2;
    e->used = false;
}

bool hostmpe_echo_is_ours(hostmpe_t* h, double now,
                          uint8_t status, uint8_t d1, uint8_t d2) {
    if (!h) return false;
    for (uint32_t i = 0; i < HOSTMPE_ECHO_SLOTS; i++) {
        hostmpe_echo_t* e = &h->echo[i];
        if (e->used || e->status != status || e->d1 != d1 || e->d2 != d2) continue;
        const double age = now - e->t;
        if (age < 0.0 || age > HOSTMPE_ECHO_WINDOW_S) continue;
        e->used = true;              // consume: one echo per emission
        h->echo_dropped++;
        return true;
    }
    return false;
}

uint32_t hostmpe_echo_dropped(const hostmpe_t* h) { return h ? h->echo_dropped : 0; }

uint32_t hostmpe_active_voices(const hostmpe_t* h) {
    if (!h) return 0;
    uint32_t n = 0;
    for (int c = 1; c <= HOSTMPE_MEMBERS; c++) n += h->ch[c].active ? 1u : 0u;
    return n;
}

} // extern "C"

// ---- §7 stylus legato engine (Step 21) --------------------------------------
// The shell owns the probe; hostmpe owns pitch state: bends, the ±47 same-
// channel re-anchor, and the piano-grid retune ramp — headless goldens here.

extern "C" {

int32_t hostmpe_pen_begin(hostmpe_t* h, double now, uint8_t note, uint8_t velocity,
                          hostmpe_msg_t* out, uint32_t max, uint32_t* out_count) {
    const int32_t v = hostmpe_touch_begin(h, now, note, velocity, 1.0f, 0.0f, 0.0f,
                                          out, max, out_count);
    if (v >= 1) h->ch[v].is_pen = true;
    return v;
}

uint32_t hostmpe_pen_glide(hostmpe_t* h, int32_t voice, uint8_t cell_note,
                           float offset_semis, float bend_scale, uint8_t velocity,
                           hostmpe_msg_t* out, uint32_t max) {
    if (!h || voice < 1 || voice > HOSTMPE_MEMBERS || cell_note > 127) return 0;
    hostmpe_voice_t* v = &h->ch[voice];
    if (!v->active || !v->is_pen) return 0;
    if (!(bend_scale >= 0.0f)) bend_scale = 0.0f;   // NaN/negative -> gated off
    uint32_t n = 0;
    const uint16_t pb = hostmpe_bend14(offset_semis * bend_scale);
    if (cell_note != v->note) {
        // Boundary HYSTERESIS (#39 refinement): a vibrato wiggle near a cell
        // edge must BEND, not machine-gun retriggers. Until the pen is
        // meaningfully INTO the new cell (±0.65 st past the current note),
        // keep bending the current note by the pen's true pitch offset —
        // in-cell bend exists everywhere, edges included.
        const float rel = offset_semis + (float)((int)cell_note - (int)v->note);
        if (rel > -HOSTMPE_PEN_HYST && rel < HOSTMPE_PEN_HYST) {
            const uint16_t pbr = hostmpe_bend14(rel * bend_scale);
            if (pbr != v->last_bend) {
                v->last_bend = pbr;
                n = put(out, n, max, bend_msg(voice, pbr));
            }
            return n <= max ? n : max;
        }
        // Same-channel legato retrigger (#39): bend first (in-tune attack),
        // Note On for the new cell, then the OLD note's Off — the legato
        // overlap idiom (mono/MPE synths glide; the DAW records terminated
        // notes; our normalizer's same-channel steal ignores the stale Off).
        const uint8_t old = v->note;
        v->note = cell_note;
        v->last_bend = pb;
        n = put(out, n, max, bend_msg(voice, pb));
        n = put(out, n, max, msg3((uint8_t)(0x90 | voice), cell_note,
                                  velocity ? velocity : 96));
        n = put(out, n, max, msg3((uint8_t)(0x80 | voice), old, 0));
        return n <= max ? n : max;
    }
    if (pb != v->last_bend) {
        v->last_bend = pb;
        n = put(out, n, max, bend_msg(voice, pb));
    }
    return n <= max ? n : max;
}

uint32_t hostmpe_pen_slide(hostmpe_t* h, int32_t voice, float eff,
                           hostmpe_msg_t* out, uint32_t max) {
    if (!h || voice < 1 || voice > HOSTMPE_MEMBERS) return 0;
    hostmpe_voice_t* v = &h->ch[voice];
    if (!v->active || !v->is_pen) return 0;
    if (eff > 1.0f) eff = 1.0f;
    if (eff < -1.0f || eff != eff) eff = eff != eff ? 0.0f : -1.0f;
    long cc = 64L + lroundf(eff * 63.0f);     // §3.3: center 64, up = brighter
    if (cc < 0) cc = 0;
    if (cc > 127) cc = 127;
    if ((uint8_t)cc == v->last_cc74) return 0;
    v->last_cc74 = (uint8_t)cc;
    if (max > 0) out[0] = msg3((uint8_t)(0xB0 | voice), 74, (uint8_t)cc);
    return 1;
}

uint32_t hostmpe_pen_pressure(hostmpe_t* h, int32_t voice, float force,
                              hostmpe_msg_t* out, uint32_t max) {
    if (!h || voice < 1 || voice > HOSTMPE_MEMBERS) return 0;
    hostmpe_voice_t* v = &h->ch[voice];
    if (!v->active || !v->is_pen) return 0;
    if (!(force > 0.0f)) force = 0.0f;
    if (force > 1.0f) force = 1.0f;
    const uint8_t pv = (uint8_t)lroundf(force * 127.0f);
    if (pv == v->last_pressure) return 0;
    v->last_pressure = pv;
    if (max > 0) out[0] = msg3((uint8_t)(0xD0 | voice), pv, 0);
    return 1;
}

} // extern "C"

// ---- §5.3 outbound limiter --------------------------------------------------
// One slot per (channel, continuous dimension). Change-only + either
// per-slot rate decimation (latest-wins) or a global token budget with a
// round-robin cursor over the slots (fairness).

enum { DIM_BEND = 0, DIM_PRESSURE = 1, DIM_CC74 = 2, DIM_POLY = 3, DIM_COUNT = 4 };
static const int LIM_VOICE_SLOTS = 16 * DIM_COUNT;
// + one slot per MASTER-channel CC (§8 strip wheels — DECISIONS_3 #30: before
// Step 18 generic CCs bypassed the policies entirely, so a latch wheel would
// have flooded the BLE budget unpoliced). Member-channel CCs other than 74
// still pass through: hostmpe generates none, and external-device bytes never
// enter the limiters.
static const int LIM_SLOTS = LIM_VOICE_SLOTS + 128;

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
    if (kind == 0xA0) return ch * DIM_COUNT + DIM_POLY;   // v0.4 swirl (§5.3)
    if (kind == 0xB0 && ch == 0) return LIM_VOICE_SLOTS + (m->data1 & 0x7F);
    if (kind == 0xB0 && m->data1 == 74) return ch * DIM_COUNT + DIM_CC74;
    return -1;
}
static uint32_t lim_value(const hostmpe_msg_t* m) {
    const int kind = m->status & 0xF0;
    if (kind == 0xE0) return (uint32_t)(m->data1 | (m->data2 << 7));
    if (kind == 0xD0) return m->data1;
    return m->data2;   // CC / poly-pressure value
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

// ---- §8 performance control strip (Step 18) ---------------------------------
// Master-channel widget value engines. Change-only everywhere; the spring's
// return ramp is time-driven and ends with a GUARANTEED exact-center message.

static const double STRIP_RAMP_S = 0.050;   // §8: ~50 ms return ramp

struct hostmpe_strip_t {
    // spring wheel (pitch, master bend)
    float    pitch_v;        // current normalized value, [-1, 1]
    uint16_t pitch_last;     // last emitted 14-bit value
    bool     ramping;
    double   ramp_t0;
    float    ramp_v0;
    // latch wheels: MOD, ASSIGN_A, ASSIGN_B
    float    latch_val[3];   // accumulated, [0, 127]
    uint8_t  latch_cc[3];
    uint8_t  latch_last[3];  // last emitted 7-bit value
    // sustain button
    bool     toggle_mode;
    bool     sustain_on;
};

static uint16_t strip_pb(float v) {
    long pb = 8192L + lroundf(v * 8191.0f);
    if (pb < 0) pb = 0;
    if (pb > 16383) pb = 16383;
    return (uint16_t)pb;
}

// The protocol CCs an assignable wheel may not take (DECISIONS_3 #30): the
// fixed widgets' own (1, 64), RPN/NRPN select + data entry (98..101, 6, 38),
// and channel mode (120..127) — a strip CC 6 on the master would corrupt the
// DAW's RPN handshake state.
static bool strip_cc_allowed(uint8_t cc) {
    if (cc > 127) return false;
    if (cc == 1 || cc == 6 || cc == 38 || cc == 64) return false;
    if (cc >= 98 && cc <= 101) return false;
    if (cc >= 120) return false;
    return true;
}

extern "C" {

hostmpe_strip_t* hostmpe_strip_create(void) {
    hostmpe_strip_t* s = (hostmpe_strip_t*)calloc(1, sizeof(hostmpe_strip_t));
    if (!s) return NULL;
    s->pitch_last = 8192;
    s->latch_cc[HOSTMPE_STRIP_MOD]      = 1;
    s->latch_cc[HOSTMPE_STRIP_ASSIGN_A] = 23;   // loopback default: viscosity
    s->latch_cc[HOSTMPE_STRIP_ASSIGN_B] = 24;   // loopback default: roughness
    return s;
}

void hostmpe_strip_destroy(hostmpe_strip_t* s) { free(s); }

uint32_t hostmpe_strip_pitch_move(hostmpe_strip_t* s, float v,
                                  hostmpe_msg_t* out, uint32_t max) {
    if (!s) return 0;
    if (!(v > -1.0f)) v = v == v ? -1.0f : 0.0f;   // clamp; NaN -> center
    if (v > 1.0f) v = 1.0f;
    s->ramping = false;                            // a grab owns the wheel
    s->pitch_v = v;
    const uint16_t pb = strip_pb(v);
    if (pb == s->pitch_last) return 0;
    s->pitch_last = pb;
    if (max > 0) out[0] = bend_msg(0, pb);
    return 1;
}

void hostmpe_strip_pitch_release(hostmpe_strip_t* s, double now) {
    if (!s) return;
    if (s->pitch_last == 8192 && s->pitch_v == 0.0f) return;   // already home
    s->ramping = true;
    s->ramp_t0 = now;
    s->ramp_v0 = s->pitch_v;
}

uint32_t hostmpe_strip_tick(hostmpe_strip_t* s, double now,
                            hostmpe_msg_t* out, uint32_t max) {
    if (!s || !s->ramping) return 0;
    const double f = (now - s->ramp_t0) / STRIP_RAMP_S;
    if (f >= 1.0) {
        // Ramp over: land EXACTLY at center, whatever the tick cadence was.
        s->ramping = false;
        s->pitch_v = 0.0f;
        if (s->pitch_last == 8192) return 0;
        s->pitch_last = 8192;
        if (max > 0) out[0] = bend_msg(0, 8192);
        return 1;
    }
    s->pitch_v = s->ramp_v0 * (float)(1.0 - (f < 0.0 ? 0.0 : f));
    const uint16_t pb = strip_pb(s->pitch_v);
    if (pb == s->pitch_last) return 0;
    s->pitch_last = pb;
    if (max > 0) out[0] = bend_msg(0, pb);
    return 1;
}

uint32_t hostmpe_strip_latch_move(hostmpe_strip_t* s, int wheel, float delta,
                                  hostmpe_msg_t* out, uint32_t max) {
    if (!s || wheel < 0 || wheel > 2) return 0;
    if (!(delta == delta)) return 0;               // NaN
    float v = s->latch_val[wheel] + delta;
    if (v < 0.0f) v = 0.0f;
    if (v > 127.0f) v = 127.0f;
    s->latch_val[wheel] = v;
    const uint8_t q = (uint8_t)lroundf(v);
    if (q == s->latch_last[wheel]) return 0;
    s->latch_last[wheel] = q;
    if (max > 0) out[0] = msg3(0xB0, s->latch_cc[wheel], q);
    return 1;
}

bool hostmpe_strip_assign(hostmpe_strip_t* s, int wheel, uint8_t cc) {
    if (!s || (wheel != HOSTMPE_STRIP_ASSIGN_A && wheel != HOSTMPE_STRIP_ASSIGN_B)) {
        return false;
    }
    if (!strip_cc_allowed(cc)) return false;
    s->latch_cc[wheel] = cc;                       // value kept; silent (§8)
    return true;
}

uint8_t hostmpe_strip_assigned_cc(const hostmpe_strip_t* s, int wheel) {
    if (!s || wheel < 0 || wheel > 2) return 0xFF;
    return s->latch_cc[wheel];
}

static uint32_t strip_sustain_emit(hostmpe_strip_t* s, bool on,
                                   hostmpe_msg_t* out, uint32_t max) {
    if (on == s->sustain_on) return 0;
    s->sustain_on = on;
    if (max > 0) out[0] = msg3(0xB0, 64, on ? 127 : 0);
    return 1;
}

uint32_t hostmpe_strip_sustain_press(hostmpe_strip_t* s, hostmpe_msg_t* out, uint32_t max) {
    if (!s) return 0;
    return strip_sustain_emit(s, s->toggle_mode ? !s->sustain_on : true, out, max);
}

uint32_t hostmpe_strip_sustain_release(hostmpe_strip_t* s, hostmpe_msg_t* out, uint32_t max) {
    if (!s) return 0;
    if (s->toggle_mode) return 0;                  // toggle: release is silent
    return strip_sustain_emit(s, false, out, max);
}

uint32_t hostmpe_strip_sustain_mode(hostmpe_strip_t* s, bool toggle,
                                    hostmpe_msg_t* out, uint32_t max) {
    if (!s) return 0;
    if (s->toggle_mode == toggle) return 0;
    s->toggle_mode = toggle;
    // A mode switch must never strand a pedal: if ON, emit the OFF now.
    return strip_sustain_emit(s, false, out, max);
}

uint32_t hostmpe_strip_announce(const hostmpe_strip_t* s, hostmpe_msg_t* out, uint32_t max) {
    if (!s) return 0;
    uint32_t n = 0;
    n = put(out, n, max, bend_msg(0, s->pitch_last));
    for (int w = 0; w < 3; w++) {
        n = put(out, n, max, msg3(0xB0, s->latch_cc[w], s->latch_last[w]));
    }
    n = put(out, n, max, msg3(0xB0, 64, s->sustain_on ? 127 : 0));
    return n <= max ? n : max;
}

float hostmpe_strip_pitch_value(const hostmpe_strip_t* s) {
    return s ? s->pitch_v : 0.0f;
}
float hostmpe_strip_latch_value(const hostmpe_strip_t* s, int wheel) {
    if (!s || wheel < 0 || wheel > 2) return 0.0f;
    return s->latch_val[wheel];
}
bool hostmpe_strip_sustain_on(const hostmpe_strip_t* s) {
    return s ? s->sustain_on : false;
}

} // extern "C"
