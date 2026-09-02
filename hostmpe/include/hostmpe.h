/* hostmpe — shared HOST-side MPE generation library (PHASE4_SPEC.md §3–§5).
   Lives OUTSIDE core/: the core never generates MIDI. One implementation
   consumed by both tablet shells — Swift via module.modulemap, Kotlin via
   the JNI layer — so the joystick math, allocator, and rate limiter cannot
   drift between platforms.

   PURE C HEADER, exactly like sumi_core.h (working rule): no STL, no C++
   types cross this boundary — the C++ lives behind it. Enforced by
   tests/hostmpe_c_compile.c.

   Step 15 scope: the §3.2 joystick indicator math only (the deadband knee).
   The voice allocator, bend mapping, and rate limiter land in Step 16. */
#ifndef HOSTMPE_H
#define HOSTMPE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* §3.2 radial deadband with a soft knee (no zipper):
     d = ‖Δ‖ / R_max, clamped to 1
     g = 0                    if d <= 0.03
     g = (d − 0.03) / 0.97    otherwise      (smooth from exactly 0)
   Returns g in [0, 1]. A hard threshold would make the first vibrato wiggle
   jump from 0 to 3% — audible zipper on the outbound pipe. */
float hostmpe_soft_knee(float d);

/* Δ_eff = Δ̂ · g(‖Δ‖ / r_max): the joystick's effective deflection, a
   direction-preserving vector of magnitude g ∈ [0, 1]. Δ and r_max must be
   in ONE metric (the shells use canvas-height units, matching the probe's
   cell_radius). Zero-length Δ and r_max <= 0 yield (0, 0). */
void hostmpe_joystick_eff(float dx, float dy, float r_max,
                          float* out_x, float* out_y);

#ifdef __cplusplus
}
#endif
#endif /* HOSTMPE_H */
