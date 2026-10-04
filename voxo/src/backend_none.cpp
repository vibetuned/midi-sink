// backend_none.cpp — Voxo with no audio device (Phase 8 step 59b, DECISIONS_7 #28).
//
// The host drives voxo_render itself: the web's AudioWorklet (the Suzu lab), node
// (the web gate), the native reference the gate compares against. voxo_start answers
// false and the instance renders offline, as every headless test already does with the
// miniaudio backend linked and no device started. No audio library is compiled in.
#include "voxo.h"
#include "voxo_internal.h"
#include <time.h>

bool voxo_backend_start(voxo_t*, uint32_t, uint32_t, uint32_t*, uint32_t*, char* name, size_t name_cap) {
    if (name && name_cap) name[0] = '\0';
    return false;                                   // no device: the host renders
}
void voxo_backend_stop(voxo_t*) {}
void voxo_backend_query(voxo_t*, voxo_stats_t*) {}

double voxo_now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);            // the web: the host's clock (WASI); natively, the monotonic clock
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
