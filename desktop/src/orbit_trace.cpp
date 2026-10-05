// Step 59 (SYNTH §2.7): the orbit trace's bridge — see orbit_trace.h.
#include "orbit_trace.h"
#include <cmath>
#include <cstring>

namespace {
constexpr float TINE_ALPHA = 0.035f;   // main.cpp's mouse tine: the comb's sharpness
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
    if (!voxo || (!cfg_.scope && !cfg_.ink && cfg_.canvas == 0)) return;
    if (aspect <= 0.0f) aspect = 1.0f;
    sumi_layout_state_t state = {};
    if (inst) sumi_get_layout_state(inst, &state);   // step 60: the fingering as the engine decoded it
    if (table_stale(params, aspect, state)) rebuild_table(params, aspect, state);

    const int segs = cfg_.segments < 1 ? 1 : (cfg_.segments > VOXO_TRACE_POINTS_MAX - 1 ? VOXO_TRACE_POINTS_MAX - 1 : cfg_.segments);
    const uint32_t n = voxo_trace_poll(voxo, buf_, 64, (uint32_t)segs);
    uint32_t total = 0;
    for (uint32_t i = 0; i < n; i++) {
        const voxo_trace_t& t = buf_[i];
        if (t.count < 2) continue;
        OrbitPolyline p;
        p.n = t.count; p.amplitude = t.amplitude; p.note = t.note; p.kind = t.voice_kind; p.held = t.held;
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
    for (const OrbitPolyline& p : polys_) {
        if (!p.placed) continue;
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
                    } else {
                        sumi_add_tine(inst, px, py, X, Y, TINE_ALPHA, len);      // the mouse's convention: magnitude = the segment's length
                    }
                    stats_.emitted_segments++;
                }
            }
            px = X; py = Y;
        }
    }
    stats_.total_emitted += stats_.emitted_segments; stats_.total_merged += stats_.merged_segments;
    if (stats_.emitted_segments > stats_.peak_emitted) stats_.peak_emitted = stats_.emitted_segments;
}
