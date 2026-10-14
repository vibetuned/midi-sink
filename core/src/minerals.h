// minerals.h — Phase 11 step 71b (KESSHO §4.6; DECISIONS_10 #6): the mineral
// preset library and the descriptor's validation. The library is the OPTICAL
// half (sumi_mineral_t, the composite's); the mechanical half a mineral pairs
// with it — the habit, its roundness, the blades, the tine profile — rides
// in params and is the shells' to set (the six presets/kessho_<mineral>.json
// carry both halves; the halves stay independent parameters).
#pragma once
#include "sumi_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// The descriptor clamped to its ranges and its size set to this version's;
// a smaller `size` on the way in (an older host) keeps the known prefix and
// defaults the rest from Quartz.
void sumi_mineral_validate(sumi_mineral_t* m);

#ifdef __cplusplus
}
#endif
