// Internal pluggable pitch->position layouts (PROJECT_SPEC.md §3.4).
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

// DECISIONS_2 #7 shortest-neighbor semitone delta at `note`: of pos(note+1)
// and pos(note-1) (primary echo), the SHORTER step wins, always pointing
// toward increasing pitch. Returned in NORMALIZED canvas coordinates,
// UNCAPPED — the voice mapper's glide axis applies its own rendering cap on
// top; the public probe reports the true lattice step. One derivation, two
// consumers (Phase 4). Returns false when degenerate (no valid neighbor).
bool sumi_layout_semitone_delta(uint32_t layout, uint8_t note,
                                const sumi_params_t* params, float aspect,
                                float* out_dx, float* out_dy);
// Phase 6 step 43 (the author's call of 2026-09-22): THE CELLS ARE THE EDDIES.
// The Chladni operator stirs an eddy in every DISPLAY CELL — the circles the
// shells draw for the keys — and every layout has cells: the three key
// layouts theirs (the probe's centre and cell_radius, the geometry the
// shells sweep), the fifths and the rolls the largest circle that does not
// touch a neighbour's, at every note position. Four floats per cell — centre
// x, centre y (normalized), radius (canvas heights), and a kind: bit 0 an
// accidental, bit 1 an ODD cell of the layout's own checkerboard (neighbours
// differ, so with the odd cells reversed neighbours counter-rotate). Returns
// the count written (<= max_cells). No lattice: the cells are the source.
#define SUMI_LAYOUT_MAX_CELLS 320u
uint32_t sumi_layout_cells(uint32_t layout, const sumi_params_t* params, float aspect,
                           float* out_xyrk, uint32_t max_cells);

#ifdef __cplusplus
}
#endif
