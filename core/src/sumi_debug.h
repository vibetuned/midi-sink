// Internal test hooks for the §4.6 cross-backend field regression. NOT part
// of the public ABI: no SUMI_API, never exported from the DLL — the desktop
// harness links the STATIC core and includes this header via core/src (the
// same arrangement the GPU-free unit tests use). Sokol-free (working rule 2).
#pragma once

#include "sumi_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// Pushes the canonical deform script into the instance's queue, exactly once,
// in this order (§4.6 / step-11 handoff — every backend must run these
// byte-identical inputs):
//   drop  (0.5, 0.5, r=0.20, phase 1, aux 0)
//   tine  ((0.2, 0.3) -> (0.8, 0.7), alpha=0.05, z=0.10)
//   vortex((0.6, 0.4), A=1.0, R=0.25)
//   drop  (0.3, 0.7, r=0.10, phase 2, aux 1)
//   3 x scroll(dx=0.01, dy=0)
// Values are written directly as float literals — no counter/layout/mapper
// involvement — so the uniforms are bit-identical on every platform.
void sumi_debug_run_field_script(sumi_instance_t* inst);

// Synchronous readback of the current RGBA16F field texture (raw half floats,
// tightly packed, w*h*8 bytes, row 0 = top per §4.6). out == NULL queries the
// size. Call after sumi_render() has flushed the frame. May block (bounded);
// test-only.
bool sumi_debug_read_field(sumi_instance_t* inst, uint8_t* out_rgba16f, size_t capacity,
                           uint32_t* out_w, uint32_t* out_h);

// Non-blocking form of the same readback (Phase 5 §5): a browser cannot block
// on a GPU map, so the web host starts the read and polls it across frames.
// begin: false if a readback is already in flight. poll: 0 idle (nothing
// started / failed), 1 in flight, 2 completed and copied (out may be NULL to
// query size only; then it never consumes).
bool sumi_debug_read_field_begin(sumi_instance_t* inst);
int  sumi_debug_read_field_poll(sumi_instance_t* inst, uint8_t* out_rgba16f, size_t capacity,
                                uint32_t* out_w, uint32_t* out_h);

#ifdef __cplusplus
}
#endif
