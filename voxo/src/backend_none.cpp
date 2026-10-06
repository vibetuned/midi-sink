// backend_none.cpp — Voxo with no audio device (Phase 8 step 59b, DECISIONS_7 #28).
//
// The host drives voxo_render itself: the web's AudioWorklet (the Suzu lab), node
// (the web gate), the native reference the gate compares against. voxo_start answers
// false and the instance renders offline, as every headless test already does with the
// miniaudio backend linked and no device started. No audio library is compiled in.
#include "voxo.h"
#include "voxo_internal.h"
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <time.h>
#endif

bool voxo_backend_start(voxo_t*, uint32_t, uint32_t, uint32_t*, uint32_t*, char* name, size_t name_cap) {
    if (name && name_cap) name[0] = '\0';
    return false;                                   // no device: the host renders
}
void voxo_backend_stop(voxo_t*) {}
void voxo_backend_query(voxo_t*, voxo_stats_t*) {}

#if defined(_WIN32)
// MSVC has no clock_gettime (the Windows lane of the v2 RC, step 67): the performance
// counter is the monotonic clock there, the same one miniaudio's backend reads.
double voxo_now_seconds(void) {
    static LARGE_INTEGER freq = {};
    LARGE_INTEGER now;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)freq.QuadPart;
}
#else
double voxo_now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);            // the web: the host's clock (WASI); natively, the monotonic clock
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
#endif
