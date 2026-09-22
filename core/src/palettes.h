// palettes.h — Phase 6 step 43 (QOL §1): the preset library and the morph
// ring. The built-in palettes of both media live here as data in the
// sumi_palette_t model (they were literal tables in composite.glsl until
// step 43), so a preset, the custom slot and the composite share ONE path.
#pragma once
#include "sumi_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// The morph ring: which two palette slots the composite blends and by how
// much, for an active id and the smoothed PALETTE_MORPH control (0..1).
// A built-in active id travels the medium's three built-ins, as the shader
// did from 0.x: t = clamp(morph)·2, the pair (active + ⌊t⌋, +1) mod 3, the
// blend t − ⌊t⌋ — the SAME float operations, so a rest position is bitwise.
// The custom slot (3) travels custom → 0 → 1 → 2 (t = clamp(morph)·3).
void sumi_palette_ring(uint32_t active_id, float morph, uint32_t* id_a, uint32_t* id_b, float* m);

#ifdef __cplusplus
}
#endif
