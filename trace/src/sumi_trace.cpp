// sumi_trace.cpp — the C surface over OrbitTrace (orbit_trace.h): see sumi_trace.h.
#include "sumi_trace.h"
#include "orbit_trace.h"
#include <new>

struct sumi_trace_t { OrbitTrace t; };

extern "C" {

void sumi_trace_default_config(sumi_trace_config_t* out) {
    if (!out) return;
    const OrbitTraceConfig d;
    out->scope = d.scope ? 1u : 0u; out->ink = d.ink ? 1u : 0u; out->kinds = d.kinds; out->scale = d.scale;
    out->segments = (uint32_t)d.segments; out->stroke = (uint32_t)d.stroke; out->canvas = (uint32_t)d.canvas;
}
sumi_trace_t* sumi_trace_create(void) { return new (std::nothrow) sumi_trace_t(); }
void sumi_trace_destroy(sumi_trace_t* t) { delete t; }
void sumi_trace_configure(sumi_trace_t* t, const sumi_trace_config_t* c) {
    if (!t || !c) return;
    OrbitTraceConfig tc;
    tc.scope = c->scope != 0; tc.ink = c->ink != 0; tc.kinds = c->kinds; tc.scale = c->scale;
    tc.segments = (int)c->segments; tc.stroke = (int)c->stroke; tc.canvas = (int)c->canvas;
    t->t.configure(tc);
}
void sumi_trace_set_gesture_hook(sumi_trace_t* t, sumi_trace_gesture_fn fn, void* user) {
    if (!t) return;
    if (!fn) { t->t.set_gesture_hook(nullptr); return; }
    t->t.set_gesture_hook([fn, user](uint32_t kind, const float* args, uint32_t n) { fn(user, kind, args, n); });
}
void sumi_trace_frame(sumi_trace_t* t, voxo_t* voxo, sumi_instance_t* inst, const sumi_params_t* params, float aspect) {
    if (!t || !params) return;
    t->t.frame(voxo, inst, *params, aspect);
}
uint32_t sumi_trace_voices(const sumi_trace_t* t) { return t ? t->t.stats().voices : 0u; }
uint32_t sumi_trace_emitted(const sumi_trace_t* t) { return t ? t->t.stats().emitted_segments : 0u; }
uint64_t sumi_trace_total_emitted(const sumi_trace_t* t) { return t ? t->t.stats().total_emitted : 0ull; }

}
