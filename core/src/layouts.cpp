// layouts.cpp — pluggable pitch->position layouts (PROJECT_SPEC_2.md §3.4).
// Every layout is a pure function; nothing here touches voices, queues, or
// the GPU. The fifths layout is the v1 mapping moved verbatim (layout 0,
// zero behavior change).
#include "layouts.h"

#include <math.h>

// Circle-of-fifths radial layout (§3.4): low notes outer, high inner.
static const float COF_R_OUTER = 0.42f;
static const float COF_R_INNER = 0.10f;

// Chroma grid (§3.4): C1 (MIDI 24) top-left ... B7 (MIDI 107) bottom-right;
// 7 octave rows x 12 pitch-class columns, cells inset so edge drops stay on
// canvas. Notes outside C1..B7 clamp to the nearest edge ROW, keeping their
// pitch-class column (DECISIONS_2.md).
static const float GRID_INSET_X = 0.08f;
static const float GRID_INSET_Y = 0.10f;

// Jankó (§3.4): staggered whole-tone rows; column = note/2, row parity =
// note%2, half-column offset on alternate rows. The note stamps on ALL THREE
// rows of its parity (rows {0,2,4} or {1,3,5}, top to bottom) — three echoes
// keeping the full 6-row lattice live.
static const float JANKO_INSET_X = 0.06f;
static const float JANKO_INSET_Y = 0.10f;
static const int   JANKO_ROWS = 6;
static const int   JANKO_COL_MIN = 12;   // note 24 / 2
static const int   JANKO_COL_MAX = 53;   // note 107 / 2

static void layout_fifths(uint8_t note, float aspect, float* out_x, float* out_y) {
    const int pc = note % 12;
    const int octave = note / 12;               // 0..10 for MIDI 0..127
    const int fifths = (pc * 7) % 12;
    const float angle = ((float)fifths / 12.0f) * 6.28318530718f - 1.57079632679f;
    const float t = (float)octave / 10.0f;
    const float r = COF_R_OUTER + (COF_R_INNER - COF_R_OUTER) * t;
    // r is in canvas-height units; divide x by aspect so the ring of pitches
    // is a circle on screen, not an ellipse.
    *out_x = 0.5f + (r * cosf(angle)) / aspect;
    *out_y = 0.5f + r * sinf(angle);
}

static void layout_chroma_grid(uint8_t note, float* out_x, float* out_y) {
    const int pc = note % 12;                    // column keeps the pitch class
    int row = (int)(note / 12) - 1 - 1;          // octave 1 -> row 0 ... octave 7 -> row 6
    if (row < 0) row = 0;                        // below C1: nearest edge cell (top)
    if (row > 6) row = 6;                        // above B7: bottom row
    *out_x = GRID_INSET_X + (((float)pc + 0.5f) / 12.0f) * (1.0f - 2.0f * GRID_INSET_X);
    *out_y = GRID_INSET_Y + (((float)row + 0.5f) / 7.0f) * (1.0f - 2.0f * GRID_INSET_Y);
}

static uint32_t layout_janko(uint8_t note, float* out_x, float* out_y) {
    const int parity = note % 2;
    int col = note / 2;
    if (col < JANKO_COL_MIN) col = JANKO_COL_MIN;
    if (col > JANKO_COL_MAX) col = JANKO_COL_MAX;
    const float ncols = (float)(JANKO_COL_MAX - JANKO_COL_MIN + 1);
    // Classic half-column offset on alternate (odd) rows.
    const float cx = (float)(col - JANKO_COL_MIN) + 0.5f + (parity == 1 ? 0.5f : 0.0f);
    const float x = JANKO_INSET_X + (cx / (ncols + 0.5f)) * (1.0f - 2.0f * JANKO_INSET_X);
    // Three echoes: the parity's rows top to bottom ({0,2,4} or {1,3,5}).
    for (int e = 0; e < 3; e++) {
        const int row = parity + 2 * e;
        out_x[e] = x;
        out_y[e] = JANKO_INSET_Y +
                   (((float)row + 0.5f) / (float)JANKO_ROWS) * (1.0f - 2.0f * JANKO_INSET_Y);
    }
    return 3;
}

extern "C" {

uint32_t sumi_layout_position(uint32_t layout, uint8_t note,
                              const sumi_params_t* params, float aspect,
                              float* out_x, float* out_y) {
    (void)params;   // static layouts are param-free; rolls (step 10) use bpm/roll_speed
    if (aspect <= 0.0f) aspect = 1.0f;
    if (note > 127) note = 127;
    switch (layout) {
        case SUMI_LAYOUT_CHROMA_GRID:
            layout_chroma_grid(note, out_x, out_y);
            return 1;
        case SUMI_LAYOUT_JANKO:
            return layout_janko(note, out_x, out_y);
        case SUMI_LAYOUT_FIFTHS:
        default:
            // Unknown ids (including the not-yet-implemented rolls) fall back
            // to the default layout rather than crashing or clustering at 0,0.
            layout_fifths(note, aspect, out_x, out_y);
            return 1;
    }
}

bool sumi_layout_field_motion(uint32_t layout, const sumi_params_t* params,
                              double dt, float* out_dx, float* out_dy) {
    // Static layouts never move the field. The roll layouts (§3.4 field
    // motion, SUMI_LAYOUT_ROLL_*) implement this in step 10.
    (void)layout; (void)params; (void)dt;
    if (out_dx) *out_dx = 0.0f;
    if (out_dy) *out_dy = 0.0f;
    return false;
}

} // extern "C"
