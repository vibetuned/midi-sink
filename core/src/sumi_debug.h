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

// v0.11 → step 43: the display cells the Chladni stir turns — the layout's
// cells (layouts.h sumi_layout_cells) with the radius scaled by
// params.chladni_cell, four floats each (centre x, centre y normalized,
// radius in canvas heights, kind: bit 0 accidental, bit 1 odd). Returns the
// count written. Test-only.
uint32_t sumi_debug_cells(sumi_instance_t* inst, float* out_xyrk, uint32_t max_cells);
// One SUMI_DEFORM_CELLS pass pushed directly — theta (rad, signed: the
// negative is the discs' exact inverse and the field's first-order one),
// the odd cells' weight, the mode (SUMI_CHLADNI_DISCS / _FIELD). The soak
// gate's sub-step and pair for the field operator. Test-only.
void sumi_debug_add_cells_pass(sumi_instance_t* inst, float theta, float odd_weight, uint32_t mode);
// Phase 6 step 43 (debugging): THE PLATE over the print — those cells drawn as
// the circles the shells draw, the odd cells tinted. Strength 0..1, 0 = off,
// which is every shipped path: the composite is bit-identical with the
// overlay off. Dev-only, no params and no ABI.
void sumi_debug_set_chladni_overlay(sumi_instance_t* inst, float strength);

// v0.12 (Phase 6 step 38): the number of burst episodes still running in
// the mapper — a test's "the release is over". Test-only.
uint32_t sumi_debug_burst_count(sumi_instance_t* inst);
// 1.7.0 (Phase 11 step 71b, KESSHO §4.4): force every tine's profile for the
// bench — −1 the records' own (the shipped path), 0..2 SUMI_TINE_*, 3 the first
// draft's UNWINDOWED fault (the two half-planes against each other, monotone):
// the zero-fabrication gate's red control. Test-only, no ABI.
void sumi_debug_set_tine_override(sumi_instance_t* inst, int profile);
// 1.7.0 (step 71b, gate 9): the deformation passes the renderer has run since
// the instance was created — a habit is a profile, not a pass. Test-only.
uint32_t sumi_debug_pass_count(sumi_instance_t* inst);
// 1.8.0 (Phase 11 step 72, KESSHO §2.2): the Arnol'd iteration with every
// field in hand — k as a fraction of K_max (may exceed 1: the red control),
// the kick's phase, and `inverse` 1 = the iteration's EXACT inverse (the
// soak's pairs). And the pass's CPU reference: for the normalized point
// (px, py) returns the Newton residual and writes M(P) (`forward`) or M⁻¹(P)
// into (sx, sy). Test-only, no ABI.
void   sumi_debug_add_arnold(sumi_instance_t* inst, float x, float y, float radius, float winding_omega, float k,
                             uint32_t resonance_q, float phase, int inverse);
double sumi_debug_arnold_map(float x, float y, float radius, float winding_omega, float k, uint32_t resonance_q,
                             float phase, float aspect, float px, float py, int forward, double* sx, double* sy);
// v0.13 (Phase 6 step 39): the spark shear episodes still running. Test-only.
uint32_t sumi_debug_spark_count(sumi_instance_t* inst);

#ifdef __cplusplus
}
#endif
