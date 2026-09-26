// voxo_internal.h — Voxo's private interface between its core (voxo.cpp) and
// its audio backend (backend_miniaudio.cpp). Not part of the ABI.
#pragma once
#include "voxo.h"

// The backend: opens the platform's default output at the wanted rate/period
// (0 = the device's own), reports the actual ones and the device's name, and
// calls voxo_render from its thread until stopped. The core owns the stats.
bool voxo_backend_start(voxo_t* v, uint32_t want_rate, uint32_t want_block,
                        uint32_t* out_rate, uint32_t* out_block, char* name, size_t name_cap);
void voxo_backend_stop(voxo_t* v);
// The core's hooks for the backend. set_rate recomputes the per-sample
// coefficients; device_opened records the device's rate and period and resets
// the run's counters; callback_timing is one callback of `frames` at the
// device's rate, `late_by_frames` how far past its expected time it arrived
// and `render_ms` how long voxo_render took; backend_slot is where the backend
// keeps its device object.
void   voxo_core_set_rate(voxo_t* v, uint32_t rate);
// Before each voxo_render from the device: the block's start on Voxo's clock
// (the latency probe stamps note-ons with it).
void   voxo_core_block_start(voxo_t* v, double seconds);
// The backend fills its own fields of the stats (burst, buffer, XRuns, the
// platform's output latency, the low-latency verdict); the core fills the rest.
void   voxo_backend_query(voxo_t* v, voxo_stats_t* out);
void   voxo_core_device_opened(voxo_t* v, uint32_t rate, uint32_t block);
void   voxo_core_callback_timing(voxo_t* v, uint32_t frames, double late_by_frames, double render_ms);
void** voxo_core_backend_slot(voxo_t* v);
