// Step 59 (SYNTH §2.7): the orbit trace's bridge — see orbit_trace.h.
#include "orbit_trace.h"
#include <cmath>
#include <cstring>

namespace {
constexpr float TINE_ALPHA = 0.035f;   // main.cpp's mouse tine: the comb's sharpness
// Step 75: the Poincaré stamps — a micro-drop's radius at the default scale (a texel and a half at 512) and the micro-swirl's
// strength and core (the Lamb–Oseen profile: smooth, no crease ring); the scope's dust dash, half a dash each side. THE INK
// RATE: a voice's returns come at its clock (220 a second at A3) and every one a drop at a stationary point composes into a
// flood (measured: 876 of three texels in 4 s made one drop of 0.18 canvas), so the ink route stamps about INK_RATE a
// second per voice — a credit of INK_RATE / clock per return, a stamp when it reaches one — while the scopes keep every
// return [ITERATE]. The stride is fractional on purpose: every N-th return ALIASES the section (a 2-cycle inked every
// tenth return is one point), and the rate is not a round number so an equal-tempered pitch never divides it exactly.
constexpr float MICRO_DROP = 0.003f, MICRO_TWIST = 0.06f, MICRO_CORE = 3.0f, DUST_DASH = 0.002f, INK_RATE = 23.7f;
// THE ANOD STAMP (the author's choice, DECISIONS_10 #12): under Anod only CHARGED material glows, by its strain, and the
// water draws its grid only where the displacement exceeds a texel — a tine or a micro-swirl on the water around a charge
// shows nothing (the author's viewing: imperceptible in both modes). So under Anod every stamp, and every vertex of the
// continuous trace, is a CHARGE SPECK: an ink drop of two texels at the default scale (the strike's charge is the same
// call), glowing at the charge's base and brightening wherever the chord's own stir, sweeps and sparks strain it. The
// continuous trace specks at the ink rate too, one along the orbit per credit (a speck at every vertex floods: 5040 in
// 4 s charged discs far past the orbits); its credit runs per frame at an assumed CONT_FPS, the bridge having no clock.
constexpr float MICRO_CHARGE = 0.004f, CONT_FPS = 60.0f;
static inline float speck_radius(float scale) { float r = MICRO_CHARGE * (scale / 0.25f); return r < 0.002f ? 0.002f : (r > 0.02f ? 0.02f : r); }
constexpr int   SCAN_W = 96, SCAN_H = 54;

uint64_t hash_params(const sumi_params_t& p) {
    uint64_t h = 1469598103934665603ull;
    const unsigned char* b = (const unsigned char*)&p;
    for (size_t i = 0; i < sizeof p; i++) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}
}

void OrbitTrace::rebuild_table(const sumi_params_t& params, float aspect, const sumi_layout_state_t& state) {
    std::memset(table_ok_, 0, sizeof table_ok_);
    // the layout probe, scanned: the first cell centre each note answers with (a multi-echo layout's first echo; the
    // rolls answer nothing and their notes stay unplaced — a scrolling sheet has no home for a trace)
    for (int j = 0; j < SCAN_H; j++) {
        for (int i = 0; i < SCAN_W; i++) {
            sumi_cell_info_t info;
            const float x = ((float)i + 0.5f) / (float)SCAN_W, y = ((float)j + 0.5f) / (float)SCAN_H;
            if (!sumi_layout_probe(params.pitch_layout, &params, aspect, &state, x, y, &info)) continue;
            if (info.note > 127 || table_ok_[info.note]) continue;
            table_ok_[info.note] = true; table_x_[info.note] = info.cell_center_x; table_y_[info.note] = info.cell_center_y;
        }
    }
    table_layout_ = params.pitch_layout; table_aspect_ = aspect; table_hash_ = hash_params(params); table_state_ = state;
}

bool OrbitTrace::table_stale(const sumi_params_t& params, float aspect, const sumi_layout_state_t& state) const {
    if (params.pitch_layout != table_layout_ || std::fabs(aspect - table_aspect_) > 1e-4f || hash_params(params) != table_hash_) return true;
    const bool stateful = params.pitch_layout == SUMI_LAYOUT_TRUMPET || params.pitch_layout == SUMI_LAYOUT_TROMBONE;
    return stateful && (state.buttons != table_state_.buttons || std::fabs(state.slider - table_state_.slider) > 1e-6f);
}

void OrbitTrace::prepare(const sumi_params_t& params, float aspect) {
    if (aspect <= 0.0f) aspect = 1.0f;
    const sumi_layout_state_t zero = {};
    if (table_stale(params, aspect, zero)) rebuild_table(params, aspect, zero);
}

bool OrbitTrace::place(uint8_t note, float* cx, float* cy) const {
    if (note > 127 || !table_ok_[note]) return false;
    *cx = table_x_[note]; *cy = table_y_[note]; return true;
}

void OrbitTrace::frame(voxo_t* voxo, sumi_instance_t* inst, const sumi_params_t& params, float aspect) {
    stats_.frames++;
    stats_.voices = stats_.polled_segments = stats_.emitted_segments = stats_.merged_segments = stats_.unplaced_voices = 0;
    polys_.clear();
    if (inst && cfg_.canvas == 0) sumi_set_scope(inst, nullptr, nullptr, 0, SUMI_SCOPE_OFF);   // the canvas scope cleared when off (cheap; the mode is what it reads)
    if (voxo && cfg_.mode == 0) voxo_set_strobe(voxo, 0u);                                  // step 75: the strobe follows the mode
    if (!voxo || (!cfg_.scope && !cfg_.ink && cfg_.canvas == 0)) { for (OrbitDust& d : dust_) { d.live = false; d.n = d.head = d.fresh = 0; } return; }
    if (aspect <= 0.0f) aspect = 1.0f;
    sumi_layout_state_t state = {};
    if (inst) sumi_get_layout_state(inst, &state);   // step 60: the fingering as the engine decoded it
    if (table_stale(params, aspect, state)) rebuild_table(params, aspect, state);
    if (cfg_.mode != 0) { poincare_frame(voxo, inst, params, aspect); return; }               // step 75

    const int segs = cfg_.segments < 1 ? 1 : (cfg_.segments > VOXO_TRACE_POINTS_MAX - 1 ? VOXO_TRACE_POINTS_MAX - 1 : cfg_.segments);
    const uint32_t n = voxo_trace_poll(voxo, buf_, 64, (uint32_t)segs);
    uint32_t total = 0;
    for (uint32_t i = 0; i < n; i++) {
        const voxo_trace_t& t = buf_[i];
        if (t.count < 2) continue;
        OrbitPolyline p;
        p.n = t.count; p.amplitude = t.amplitude; p.note = t.note; p.kind = t.voice_kind; p.held = t.held; p.serial = t.serial;
        p.radius = t.amplitude * cfg_.scale;
        p.placed = place(t.note, &p.cx, &p.cy);
        if (!p.placed) { p.cx = 0.5f; p.cy = 0.5f; stats_.unplaced_voices++; }
        std::memcpy(p.x, t.x, sizeof(float) * t.count); std::memcpy(p.y, t.y, sizeof(float) * t.count);
        polys_.push_back(p);
        total += t.count - 1;
    }
    stats_.voices = (uint32_t)polys_.size();
    stats_.polled_segments = total;
    if (inst && cfg_.canvas != 0) {   // THE CANVAS SCOPE: the polylines at their cells, in canvas coordinates, to the live composite (1.3.0)
        float pts[2 * (VOXO_TRACE_POINTS_MAX * 64)]; uint32_t lens[64]; uint32_t strips = 0, at = 0, segs = 0;
        for (const OrbitPolyline& p : polys_) {
            if (strips >= 64 || segs + (p.n - 1) > CANVAS_SEGMENTS) break;
            for (uint32_t k = 0; k < p.n; k++) { pts[2 * (at + k)] = p.cx + p.x[k] * p.radius / aspect; pts[2 * (at + k) + 1] = p.cy + p.y[k] * p.radius; }
            lens[strips++] = p.n; at += p.n; segs += p.n - 1;
        }
        sumi_set_scope(inst, pts, lens, strips, cfg_.canvas == 2 ? SUMI_SCOPE_REPLACE : SUMI_SCOPE_OVER);
    }
    if (!cfg_.ink || !inst) return;

    // THE BUDGET: at most BUDGET segments this frame over all voices. Over it, every voice keeps the same share of its
    // polyline (at least one segment) — merged within the voice, never one voice culled while another draws.
    const float share = total > BUDGET ? (float)BUDGET / (float)total : 1.0f;
    const bool anod = params.medium == SUMI_MEDIUM_ANOD;
    const float speck = speck_radius(cfg_.scale);
    bool seen[DUST_SLOTS] = {};   // the Anod specks' credit lives in the dust slots, one per voice serial
    for (const OrbitPolyline& p : polys_) {
        if (!p.placed) continue;
        if (anod) {   // the charge speck along the orbit at the ink rate: the newest vertex, when the credit allows
            OrbitDust* d = dust_slot(p.serial, seen);
            if (!d) continue;
            seen[d - dust_] = true;
            d->ink_credit += INK_RATE / CONT_FPS;
            if (d->ink_credit < 1.0f) continue;
            d->ink_credit -= 1.0f;
            const float X = p.cx + p.x[p.n - 1] * p.radius / aspect, Y = p.cy + p.y[p.n - 1] * p.radius;
            if (X >= 0.0f && X <= 1.0f && Y >= 0.0f && Y <= 1.0f) {
                sumi_add_drop(inst, X, Y, speck, SUMI_DROP_INK);
                if (hook_) { const float a[4] = { X, Y, speck, (float)SUMI_DROP_INK }; hook_(7u /* SUMI_REPLAY_G_DROP */, a, 4); }
                stats_.emitted_segments++;
            }
            continue;
        }
        const uint32_t have = p.n - 1;
        uint32_t keep = share < 1.0f ? (uint32_t)std::floor((float)have * share) : have;
        if (keep < 1) keep = 1;
        stats_.merged_segments += have - keep;
        // the kept vertices: the first, the last, and evenly spaced ones between (a coarser polyline of the same orbit)
        float px = 0.0f, py = 0.0f;
        for (uint32_t k = 0; k <= keep; k++) {
            const uint32_t idx = keep == have ? k : (uint32_t)std::lround((double)k * (double)have / (double)keep);
            const float X = p.cx + p.x[idx] * p.radius / aspect, Y = p.cy + p.y[idx] * p.radius;
            if (k > 0) {
                const float dx = (X - px) * aspect, dy = Y - py;                 // canvas-height units
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len > 1e-5f) {
                    if (cfg_.stroke == 1) {
                        float tip = 0.25f * p.radius; if (tip < 0.005f) tip = 0.005f; if (tip > 0.08f) tip = 0.08f;
                        sumi_add_wake(inst, px, py, X, Y, tip);
                        if (hook_) { const float a[5] = { px, py, X, Y, tip }; hook_(6u /* SUMI_REPLAY_G_WAKE */, a, 5); }
                    } else {
                        sumi_add_tine(inst, px, py, X, Y, TINE_ALPHA, len);      // the mouse's convention: magnitude = the segment's length
                        if (hook_) { const float a[6] = { px, py, X, Y, TINE_ALPHA, len }; hook_(5u /* SUMI_REPLAY_G_TINE */, a, 6); }
                    }
                    stats_.emitted_segments++;
                }
            }
            px = X; py = Y;
        }
    }
    if (anod) for (uint32_t k = 0; k < DUST_SLOTS; k++) if (!seen[k]) { dust_[k].live = false; dust_[k].n = dust_[k].head = dust_[k].fresh = 0; }
    stats_.total_emitted += stats_.emitted_segments; stats_.total_merged += stats_.merged_segments;
    if (stats_.emitted_segments > stats_.peak_emitted) stats_.peak_emitted = stats_.emitted_segments;
}

OrbitDust* OrbitTrace::dust_slot(uint32_t serial, const bool* seen) {
    for (uint32_t k = 0; k < DUST_SLOTS; k++) if (dust_[k].live && dust_[k].serial == serial) return &dust_[k];
    int slot = -1;
    for (uint32_t k = 0; k < DUST_SLOTS; k++) if (!dust_[k].live) { slot = (int)k; break; }
    if (slot < 0) { uint32_t best = ~0u; for (uint32_t k = 0; k < DUST_SLOTS; k++) if (!seen[k] && dust_[k].serial < best) { best = dust_[k].serial; slot = (int)k; } }
    if (slot < 0) return nullptr;
    OrbitDust& d = dust_[slot]; d.live = true; d.serial = serial; d.n = d.head = d.fresh = 0; d.ink_credit = 1.0f;
    return &d;
}

// Step 75 (KESSHO §2.5): THE POINCARÉ MODE. Voxo strobes every traced voice at the reference phasor's wraps (or its own
// drive's); each return is a dust point on the scopes — the settings miniature reads the slots, the canvas scope takes
// the newest as tiny dashes, round-robin across voices up to its 128 — and, on the ink route, a stamp at the voice's
// cell: a micro-drop under Sumi, a micro-swirl under Anod and Kessho, through the gesture ABI and the hook (a
// recording carries them). Budgeted as the segments are: at most BUDGET stamps a frame over all voices, every voice
// keeping the same share of its returns (the newest; at least one), never a voice dropped while another stamps.
void OrbitTrace::poincare_frame(voxo_t* voxo, sumi_instance_t* inst, const sumi_params_t& params, float aspect) {
    voxo_set_strobe(voxo, cfg_.mode == 2 ? 2u : 1u);
    const uint32_t n = voxo_strobe_poll(voxo, sbuf_, 64, VOXO_STROBE_POINTS_MAX);
    bool seen[DUST_SLOTS] = {};
    uint32_t total = 0, placed_total = 0;
    for (uint32_t i = 0; i < n; i++) {
        const voxo_strobe_t& t = sbuf_[i];
        OrbitDust* dp = dust_slot(t.serial, seen);   // the voice's slot, a new one reset
        if (!dp) continue;
        OrbitDust& d = *dp; const uint32_t slot = (uint32_t)(dp - dust_); seen[slot] = true;
        if (d.n == 0 && d.head == 0) { d.note = t.note; d.kind = t.voice_kind; }
        d.held = t.held; d.hz = t.hz; d.total = t.total; d.fresh = t.count; d.scale = cfg_.scale;
        d.placed = place(t.note, &d.cx, &d.cy);
        if (!d.placed) { d.cx = 0.5f; d.cy = 0.5f; if (t.count) stats_.unplaced_voices++; }
        const float credit = t.hz > INK_RATE ? INK_RATE / t.hz : 1.0f;   // the ink's share of this voice's returns
        d.inked = 0;
        for (uint32_t k = 0; k < t.count; k++) {
            d.ink_credit += credit; const bool stamp = d.ink_credit >= 1.0f; if (stamp) d.ink_credit -= 1.0f;
            d.x[d.head] = t.x[k]; d.y[d.head] = t.y[k]; d.ink[d.head] = stamp; if (stamp) d.inked++;
            d.head = (d.head + 1) % OrbitDust::MAX; if (d.n < OrbitDust::MAX) d.n++;
        }
        total += t.count; if (d.placed) placed_total += d.inked;
    }
    for (uint32_t k = 0; k < DUST_SLOTS; k++) if (!seen[k]) { dust_[k].live = false; dust_[k].n = dust_[k].head = dust_[k].fresh = 0; }
    stats_.voices = n; stats_.polled_segments = total;
    if (inst && cfg_.canvas != 0) {   // THE CANVAS SCOPE: the newest dust as dashes, the voices round-robin, up to the view's 128
        float pts[4 * CANVAS_SEGMENTS]; uint32_t lens[CANVAS_SEGMENTS]; uint32_t strips = 0;
        for (uint32_t depth = 0; strips < CANVAS_SEGMENTS; depth++) {
            bool any = false;
            for (uint32_t k = 0; k < DUST_SLOTS && strips < CANVAS_SEGMENTS; k++) {
                const OrbitDust& d = dust_[k];
                if (!d.live || depth >= d.n) continue;
                any = true;
                const uint32_t idx = (d.head + OrbitDust::MAX - 1 - depth) % OrbitDust::MAX;
                const float X = d.cx + d.x[idx] * d.scale / aspect, Y = d.cy + d.y[idx] * d.scale;
                pts[4 * strips] = X - DUST_DASH / aspect; pts[4 * strips + 1] = Y; pts[4 * strips + 2] = X + DUST_DASH / aspect; pts[4 * strips + 3] = Y;
                lens[strips++] = 2;
            }
            if (!any) break;
        }
        sumi_set_scope(inst, pts, lens, strips, cfg_.canvas == 2 ? SUMI_SCOPE_REPLACE : SUMI_SCOPE_OVER);
    }
    if (!cfg_.ink || !inst) return;
    const float share = placed_total > BUDGET ? (float)BUDGET / (float)placed_total : 1.0f;
    const bool sumi = params.medium == SUMI_MEDIUM_SUMI, anod = params.medium == SUMI_MEDIUM_ANOD;
    float r = MICRO_DROP * (cfg_.scale / 0.25f); if (r < 0.003f) r = 0.003f; if (r > 0.02f) r = 0.02f;
    const float speck = speck_radius(cfg_.scale);
    for (uint32_t k = 0; k < DUST_SLOTS; k++) {
        const OrbitDust& d = dust_[k];
        if (!d.live || !d.placed || d.inked == 0) continue;
        uint32_t keep = share < 1.0f ? (uint32_t)std::floor((float)d.inked * share) : d.inked;
        if (keep < 1) keep = 1;
        stats_.merged_segments += d.inked - keep;
        uint32_t stamped = 0;
        for (uint32_t j = 0; j < d.fresh && stamped < keep; j++) {   // this frame's returns the rate kept, oldest first
            const uint32_t idx = (d.head + OrbitDust::MAX - d.fresh + j) % OrbitDust::MAX;
            if (!d.ink[idx]) continue;
            stamped++;
            const float X = d.cx + d.x[idx] * d.scale / aspect, Y = d.cy + d.y[idx] * d.scale;
            if (X < 0.0f || X > 1.0f || Y < 0.0f || Y > 1.0f) continue;
            if (sumi) {
                sumi_gesture_tap(inst, X, Y, r);
                if (hook_) { const float a[3] = { X, Y, r }; hook_(0u /* SUMI_REPLAY_G_TAP */, a, 3); }
            } else if (anod) {                                                 // the charge speck
                sumi_add_drop(inst, X, Y, speck, SUMI_DROP_INK);
                if (hook_) { const float a[4] = { X, Y, speck, (float)SUMI_DROP_INK }; hook_(7u /* SUMI_REPLAY_G_DROP */, a, 4); }
            } else {
                sumi_gesture_twist(inst, X, Y, MICRO_TWIST, MICRO_CORE * r, SUMI_VORTEX_EXPONENTIAL);
                if (hook_) { const float a[5] = { X, Y, MICRO_TWIST, MICRO_CORE * r, (float)SUMI_VORTEX_EXPONENTIAL }; hook_(2u /* SUMI_REPLAY_G_TWIST */, a, 5); }
            }
            stats_.emitted_segments++;
        }
    }
    stats_.total_emitted += stats_.emitted_segments; stats_.total_merged += stats_.merged_segments;
    if (stats_.emitted_segments > stats_.peak_emitted) stats_.peak_emitted = stats_.emitted_segments;
}
