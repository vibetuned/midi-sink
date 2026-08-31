// midi_normalizer.cpp — SPSC queue drain + stateful MIDI decoding
// (PROJECT_SPEC.md §3.1, §3.2, §2.5). No sokol, no STL containers; the only
// cross-thread state is the ring (std::atomic).
#include "midi_normalizer.h"
#include "log_levels.h"

#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Lock-free SPSC ring (§3.1): power-of-two capacity, drop-oldest on overflow.
// Each slot is a single atomic word (status | d1<<8 | d2<<16), so a slot read
// can never tear. head/tail are monotonically increasing 64-bit counters.
// The producer never loops: on overflow it makes ONE attempt to advance head
// (dropping the oldest message); if that races with the consumer, the
// consumer freed a slot anyway. Wait-free for the producer.
// ---------------------------------------------------------------------------
#define SUMI_MIDI_RING_CAPACITY 4096u   // power of two (§3.1)

struct sumi_midi_ring_t {
    std::atomic<uint32_t> slots[SUMI_MIDI_RING_CAPACITY];
    alignas(64) std::atomic<uint64_t> head;    // next slot to read
    alignas(64) std::atomic<uint64_t> tail;    // next slot to write
    alignas(64) std::atomic<uint32_t> dropped;
};

static void ring_push(sumi_midi_ring_t* r, uint8_t s, uint8_t d1, uint8_t d2) {
    const uint64_t t = r->tail.load(std::memory_order_relaxed);
    uint64_t h = r->head.load(std::memory_order_acquire);
    if (t - h >= SUMI_MIDI_RING_CAPACITY) {
        // Full: drop the oldest (§3.1). One CAS attempt — if it fails, the
        // consumer just consumed a slot and there is room now.
        if (r->head.compare_exchange_strong(h, h + 1,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire)) {
            r->dropped.fetch_add(1, std::memory_order_relaxed);
        }
    }
    const uint32_t packed = (uint32_t)s | ((uint32_t)d1 << 8) | ((uint32_t)d2 << 16);
    r->slots[t & (SUMI_MIDI_RING_CAPACITY - 1)].store(packed, std::memory_order_relaxed);
    r->tail.store(t + 1, std::memory_order_release);
}

// Returns true and fills the message if one was consumed.
static bool ring_pop(sumi_midi_ring_t* r, uint8_t* s, uint8_t* d1, uint8_t* d2) {
    uint64_t h = r->head.load(std::memory_order_relaxed);
    for (;;) {
        const uint64_t t = r->tail.load(std::memory_order_acquire);
        if (h >= t) return false;
        const uint32_t v = r->slots[h & (SUMI_MIDI_RING_CAPACITY - 1)].load(std::memory_order_relaxed);
        // CAS because the producer may steal this slot when dropping oldest.
        if (r->head.compare_exchange_strong(h, h + 1,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire)) {
            *s  = (uint8_t)(v & 0xFF);
            *d1 = (uint8_t)((v >> 8) & 0xFF);
            *d2 = (uint8_t)((v >> 16) & 0xFF);
            return true;
        }
        // h was updated by the failed CAS; retry with the new head.
    }
}

// ---------------------------------------------------------------------------
// Stateful decoder (§3.2)
// ---------------------------------------------------------------------------
struct sumi_channel_state_t {
    float   bend_range_semis;   // RPN 0 value when explicitly set
    bool    bend_range_explicit;// RPN 0 received on this channel
    uint8_t rpn_msb, rpn_lsb;   // active RPN (0x7F/0x7F = null)
    bool    nrpn_active;        // last parameter select was NRPN
    bool    sustain_down;       // CC64 state for rising-edge detection
};

struct sumi_normalizer_t {
    sumi_midi_ring_t  ring;
    sumi_log_fn       log_cb;
    void*             log_user;

    // Decoder state — consumer thread only.
    uint8_t               last_status;               // running-status tolerance
    sumi_channel_state_t  ch[16];

    // §2.5 auto-detection state — consumer thread only.
    sumi_input_mode_t override_mode;    // SUMI_INPUT_AUTO = heuristic decides
    sumi_input_mode_t detected_mode;
    uint16_t note_channel_mask;         // channels that have seen note-ons
    uint16_t expr_channel_mask;         // channels with bend or pressure
    uint32_t cc2_count;                 // breath controller density
    bool     mcm_received;

    // §2.1 MPE zone. v1: single (lower) zone; upper-zone MCM is log-and-ignored.
    sumi_mpe_zone_t zone;
};

static void n_log(sumi_normalizer_t* n, int level, const char* msg) {
    if (n->log_cb) n->log_cb(level, msg, n->log_user);
}

static const char* mode_name(sumi_input_mode_t m) {
    switch (m) {
        case SUMI_INPUT_MPE:     return "MPE";
        case SUMI_INPUT_CLASSIC: return "classic";
        case SUMI_INPUT_WIND:    return "wind";
        default:                 return "auto";
    }
}

// §2.5 heuristic. Member channels are ch 2..16 (index 1..15).
static sumi_input_mode_t detect_mode(const sumi_normalizer_t* n) {
    if (n->mcm_received) return SUMI_INPUT_MPE;
    const uint16_t member_notes = (uint16_t)(n->note_channel_mask & ~1u);
    const uint16_t member_expr  = (uint16_t)(n->expr_channel_mask & ~1u);
    int note_chans = 0, expr_note_chans = 0, total_note_chans = 0;
    for (int i = 0; i < 16; i++) {
        if (n->note_channel_mask & (1u << i)) total_note_chans++;
        if (member_notes & (1u << i)) {
            note_chans++;
            if (member_expr & (1u << i)) expr_note_chans++;
        }
    }
    if (note_chans >= 2 && expr_note_chans >= 2) return SUMI_INPUT_MPE;
    if (total_note_chans <= 1 && n->cc2_count >= 16) return SUMI_INPUT_WIND;
    return SUMI_INPUT_CLASSIC;
}

static void update_detection(sumi_normalizer_t* n) {
    const sumi_input_mode_t m = detect_mode(n);
    if (m != n->detected_mode) {
        n->detected_mode = m;
        if (n->override_mode == SUMI_INPUT_AUTO) {
            char buf[64];
            snprintf(buf, sizeof(buf), "normalizer: input mode -> %s", mode_name(m));
            n_log(n, SUMI_LOG_INFO, buf);
        }
    }
}

static uint32_t emit(sumi_midi_event_t* out, uint32_t count, uint32_t max,
                     sumi_midi_event_kind_t kind, uint8_t ch, uint8_t a, uint8_t b, float f) {
    if (count < max) {
        out[count].kind = kind;
        out[count].channel = ch;
        out[count].a = a;
        out[count].b = b;
        out[count].f = f;
        return count + 1;
    }
    return count;
}

// Decode one complete message; append events. Returns the new event count.
static uint32_t decode(sumi_normalizer_t* n, uint8_t status, uint8_t d1, uint8_t d2,
                       sumi_midi_event_t* out, uint32_t count, uint32_t max) {
    // Running-status tolerance (§3.2): a data byte in the status slot means
    // the source elided the status; reuse the last seen one, and the "status"
    // byte is really the first data byte.
    if ((status & 0x80) == 0) {
        if ((n->last_status & 0x80) == 0) return count;   // nothing to reuse
        d2 = d1;
        d1 = status;
        status = n->last_status;
    }
    if (status >= 0xF0) return count;   // SysEx / system messages: out of scope v1 (§3.1)
    n->last_status = status;

    const uint8_t kind = status & 0xF0;
    const uint8_t ch = status & 0x0F;
    sumi_channel_state_t* cs = &n->ch[ch];

    switch (kind) {
        case 0x90:   // note on (velocity 0 = note off)
            if ((d2 & 0x7F) == 0) {
                return emit(out, count, max, SUMI_MEV_NOTE_OFF, ch, d1 & 0x7F, 0, 0.0f);
            }
            n->note_channel_mask |= (uint16_t)(1u << ch);
            update_detection(n);
            return emit(out, count, max, SUMI_MEV_NOTE_ON, ch, d1 & 0x7F, d2 & 0x7F, 0.0f);

        case 0x80:   // note off
            return emit(out, count, max, SUMI_MEV_NOTE_OFF, ch, d1 & 0x7F, d2 & 0x7F, 0.0f);

        case 0xE0: { // pitch bend, 14-bit LSB-first (§3.2)
            n->expr_channel_mask |= (uint16_t)(1u << ch);
            const int32_t raw = (int32_t)((d1 & 0x7F) | ((uint32_t)(d2 & 0x7F) << 7));
            // Bend range (§2.1): RPN 0 when explicitly set; otherwise ±48 on
            // MPE member channels (ROLI/Osmose default), ±2 elsewhere.
            float range = 2.0f;
            if (cs->bend_range_explicit) {
                range = cs->bend_range_semis;
            } else {
                const sumi_mpe_zone_t z = n->zone;
                const bool member = z.member_count > 0 &&
                                    ch >= z.first_member &&
                                    ch < (uint8_t)(z.first_member + z.member_count);
                sumi_input_mode_t m = (n->override_mode != SUMI_INPUT_AUTO)
                                          ? n->override_mode : n->detected_mode;
                if (m == SUMI_INPUT_MPE && member) range = 48.0f;
            }
            const float semis = (float)(raw - 8192) / 8192.0f * range;
            return emit(out, count, max, SUMI_MEV_BEND, ch, 0, 0, semis);
        }

        case 0xD0:   // channel pressure
            n->expr_channel_mask |= (uint16_t)(1u << ch);
            return emit(out, count, max, SUMI_MEV_CHANNEL_PRESSURE, ch, 0, d1 & 0x7F, 0.0f);

        case 0xB0: { // control change — RPN/NRPN state machine (§3.2)
            const uint8_t cc = d1 & 0x7F;
            const uint8_t val = d2 & 0x7F;
            switch (cc) {
                case 101: cs->rpn_msb = val; cs->nrpn_active = false; break;
                case 100: cs->rpn_lsb = val; cs->nrpn_active = false; break;
                case 99: case 98: cs->nrpn_active = true; break;   // NRPN select: recognized, ignored
                case 6:
                    if (!cs->nrpn_active) {
                        if (cs->rpn_msb == 0 && cs->rpn_lsb == 0) {
                            cs->bend_range_semis = (float)val;   // RPN 0: bend range (semitones)
                            cs->bend_range_explicit = true;
                        } else if (cs->rpn_msb == 0 && cs->rpn_lsb == 6) {
                            // RPN 6: MPE Configuration Message (§2.1). Only the
                            // lower zone (MCM on ch 1) is supported in v1.
                            if (ch == 0) {
                                const uint8_t members = val > 15 ? 15 : val;
                                n->zone.master = 0;
                                n->zone.first_member = 1;
                                n->zone.member_count = members;
                                n->mcm_received = members > 0;
                                update_detection(n);
                                char buf[80];
                                snprintf(buf, sizeof(buf),
                                         "normalizer: MCM lower zone, %u member channels", members);
                                n_log(n, SUMI_LOG_INFO, buf);
                            } else if (ch == 15) {
                                n_log(n, SUMI_LOG_WARN,
                                      "normalizer: upper-zone MCM ignored (single zone in v1)");
                            }
                        }
                    }
                    break;
                case 38:   // data entry LSB: cents for RPN 0 — v1 ignores sub-semitone
                    break;
                default:
                    break;
            }
            if (cc == 2) {
                n->cc2_count++;
                update_detection(n);
            }
            // Every CC is forwarded; the voice mapper / CC map route them.
            return emit(out, count, max, SUMI_MEV_CC, ch, cc, val, 0.0f);
        }

        case 0xA0:   // poly aftertouch: out of scope for v1
        case 0xC0:   // program change: ignored
        default:
            return count;
    }
}

extern "C" {

sumi_normalizer_t* sumi_normalizer_create(sumi_log_fn log_cb, void* log_user) {
    sumi_normalizer_t* n = (sumi_normalizer_t*)calloc(1, sizeof(sumi_normalizer_t));
    if (!n) return nullptr;
    n->log_cb = log_cb;
    n->log_user = log_user;
    n->override_mode = SUMI_INPUT_AUTO;
    n->detected_mode = SUMI_INPUT_CLASSIC;
    for (int i = 0; i < 16; i++) {
        n->ch[i].bend_range_semis = 2.0f;
        n->ch[i].bend_range_explicit = false;
        n->ch[i].rpn_msb = 0x7F;
        n->ch[i].rpn_lsb = 0x7F;
    }
    // §2.1 default when no MCM is received: lower zone, master ch 1,
    // members ch 2..16.
    n->zone.master = 0;
    n->zone.first_member = 1;
    n->zone.member_count = 15;
    return n;
}

void sumi_normalizer_destroy(sumi_normalizer_t* n) {
    free(n);
}

void sumi_normalizer_push(sumi_normalizer_t* n, uint8_t status, uint8_t data1, uint8_t data2) {
    if (!n) return;
    ring_push(&n->ring, status, data1, data2);
}

uint32_t sumi_normalizer_drain(sumi_normalizer_t* n, sumi_midi_event_t* out, uint32_t max) {
    if (!n || !out || max == 0) return 0;
    uint32_t count = 0;
    uint8_t s, d1, d2;
    // Leave headroom: one message can emit at most one event today, but stop
    // early rather than dropping decoded state on a full output buffer.
    while (count < max && ring_pop(&n->ring, &s, &d1, &d2)) {
        count = decode(n, s, d1, d2, out, count, max);
    }
    return count;
}

uint32_t sumi_normalizer_dropped(const sumi_normalizer_t* n) {
    return n ? n->ring.dropped.load(std::memory_order_relaxed) : 0;
}

void sumi_normalizer_set_mode(sumi_normalizer_t* n, sumi_input_mode_t mode) {
    if (!n) return;
    n->override_mode = mode;
    char buf[64];
    snprintf(buf, sizeof(buf), "normalizer: input mode override -> %s", mode_name(mode));
    n_log(n, SUMI_LOG_INFO, buf);
}

sumi_input_mode_t sumi_normalizer_mode(const sumi_normalizer_t* n) {
    if (!n) return SUMI_INPUT_CLASSIC;
    if (n->override_mode != SUMI_INPUT_AUTO) return n->override_mode;
    return n->detected_mode;
}

sumi_mpe_zone_t sumi_normalizer_zone(const sumi_normalizer_t* n) {
    if (!n) {
        sumi_mpe_zone_t z = {0, 1, 15};
        return z;
    }
    return n->zone;
}

} // extern "C"
