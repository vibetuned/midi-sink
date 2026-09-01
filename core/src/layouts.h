// Internal pluggable pitch->position layouts (PROJECT_SPEC_2.md §3.4).
// A layout is a PURE function (note, params, aspect) -> (x, y) plus an
// optional per-frame field motion. GPU-free, unit-tested headlessly.
#pragma once

#include "sumi_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// A layout may place one note at up to SUMI_MAX_ECHOES canvas sites — an
// "echo set" that is all the same note (§3.4 echo-set rules).
#define SUMI_MAX_ECHOES 3

// Pure pitch -> position mapping for the given sumi_layout_t value: fills
// out_x/out_y (arrays of SUMI_MAX_ECHOES) and returns the echo count (1..3).
// `aspect` = field W/H (radial layouts keep circles circular on screen).
// Unknown / not-yet-implemented layout ids fall back to SUMI_LAYOUT_FIFTHS.
uint32_t sumi_layout_position(uint32_t layout, uint8_t note,
                              const sumi_params_t* params, float aspect,
                              float* out_x, float* out_y);

// Optional per-frame field motion (§3.4): roll layouts scroll the whole
// field as a closed-form translation. Returns true and fills the translation
// (canvas units, y-down) when the layout moves; false for static layouts.
// Dormant until the roll layouts land (step 10).
bool sumi_layout_field_motion(uint32_t layout, const sumi_params_t* params,
                              double dt, float* out_dx, float* out_dy);

#ifdef __cplusplus
}
#endif
