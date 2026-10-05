// Step 59 (SYNTH §2.7): THE ORBIT TRACE — the synth draws itself. The shell's
// bridge from Voxo to libsumi at frame rate, no core change: each traced
// voice's orbit (Voxo's poll, a short curvature-weighted polyline with its
// amplitude) is placed at its note's cell centre — the layout probe's table,
// built once per layout — and, on the GESTURE ROUTE, emitted as tine or
// wake segments through the existing gesture ABI (class by inheritance under
// the strictest-member rule: a tine is exact, a wake sub-stepped); on the
// SCOPE ROUTE the same polylines are kept for the settings window's scope,
// a miniature of the canvas (the canvas window is the core's swapchain on
// every backend, so the shell has no surface of its own over it — DECISIONS_7
// #25). The segments are budgeted like any feed: at most BUDGET a frame over
// all voices, the overflow merged within each voice (fewer, longer segments),
// never a whole voice dropped while another draws — the echo rule's spirit.
#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include "sumi_core.h"
#include "voxo.h"

struct OrbitTraceConfig {
    bool     scope = true;        // the scope route (the settings window's miniature)
    bool     ink = true;          // the gesture route (segments into the water at the voice's cell)
    uint32_t kinds = 1u << 5;     // the voice kinds traced (bit k): the kicked rotor by default — the chaos voice scribbling its own noise
    float    scale = 0.25f;       // canvas heights per unit orbit amplitude (a cell's amplitude is the patch's level: 0.25 at velocity 127)
    int      segments = 6;        // segments per voice per frame, 4..8 (SYNTH §2.7's per-voice budget)
    int      stroke = 0;          // 0 tine (exact), 1 wake (sub-stepped, the stylus doublet)
    int      canvas = 0;          // the scope on the canvas (libsumi 1.3.0's scope view): 0 off, 1 over the water, 2 instead of it
};

struct OrbitPolyline {
    float    cx, cy;                            // the voice's cell centre, normalized canvas coordinates
    float    radius;                            // amplitude × scale, canvas-height units
    uint32_t n;                                 // points
    float    x[VOXO_TRACE_POINTS_MAX], y[VOXO_TRACE_POINTS_MAX];   // unit orbit (Voxo's normalization)
    float    amplitude;
    uint8_t  note, kind, held;
    bool     placed;                            // false when the layout has no cell for the note (rolls): the scope draws it at the centre, the ink skips it
};

struct OrbitTraceStats {
    uint64_t frames = 0;
    uint32_t voices = 0;                 // this frame
    uint32_t polled_segments = 0;        // this frame, before the budget
    uint32_t emitted_segments = 0;       // this frame, into the gesture ABI
    uint32_t merged_segments = 0;        // this frame, the budget's merging
    uint32_t unplaced_voices = 0;        // this frame
    uint32_t peak_emitted = 0;           // since the reset
    uint64_t total_emitted = 0, total_merged = 0;
};

class OrbitTrace {
public:
    static constexpr uint32_t BUDGET = 24;
    static constexpr uint32_t CANVAS_SEGMENTS = 128;   // libsumi's scope view holds 128 segments: sixteen voices at eight   // segments a frame over all voices: the mapper's own budget is 64 (§3.4) and the queue holds 4096, so the trace never starves a feed

    void configure(const OrbitTraceConfig& c) { cfg_ = c; }
    // Phase 9 step 65: every segment the ink route emits is reported here too (the recorder's gesture event:
    // kind SUMI_REPLAY_G_TINE / _WAKE with the call's arguments), so a recording carries the trace.
    void set_gesture_hook(std::function<void(uint32_t kind, const float* args, uint32_t n)> hook) { hook_ = std::move(hook); }
    const OrbitTraceConfig& config() const { return cfg_; }

    // One frame: polls Voxo, places each voice, emits the ink segments (when `inst` is given and the ink route is on),
    // hands the canvas scope its polylines (when `inst` is given and the canvas mode is on) and keeps the polylines
    // for the settings scope. Call before sumi_update so the segments land in this frame's passes.
    void frame(voxo_t* voxo, sumi_instance_t* inst, const sumi_params_t& params, float aspect);

    const std::vector<OrbitPolyline>& polylines() const { return polys_; }
    const OrbitTraceStats& stats() const { return stats_; }
    void reset_stats() { stats_ = OrbitTraceStats{}; }

    // The note's cell centre in the current table (false: the layout places no such note).
    bool place(uint8_t note, float* cx, float* cy) const;
    // Builds the table for these params if it is not the current one (frame() does the same; the bench calls it first).
    void prepare(const sumi_params_t& params, float aspect);

private:
    // step 60: the table depends on the LAYOUT STATE too for the stateful layouts — the engine's copy (sumi_get_layout_state),
    // so a valve change moves the notes' homes as it moves what the cells sound; stateless layouts ignore it
    void rebuild_table(const sumi_params_t& params, float aspect, const sumi_layout_state_t& state);
    bool table_stale(const sumi_params_t& params, float aspect, const sumi_layout_state_t& state) const;

    OrbitTraceConfig cfg_;
    std::function<void(uint32_t, const float*, uint32_t)> hook_;
    std::vector<OrbitPolyline> polys_;
    OrbitTraceStats stats_;
    float    table_x_[128] = {}, table_y_[128] = {};
    bool     table_ok_[128] = {};
    uint32_t table_layout_ = ~0u;
    float    table_aspect_ = 0.0f;
    uint64_t table_hash_ = 0;
    sumi_layout_state_t table_state_ = {};
    voxo_trace_t buf_[64];
};
