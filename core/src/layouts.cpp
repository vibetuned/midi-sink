// layouts.cpp — pluggable pitch->position layouts (PROJECT_SPEC.md §3.4).
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
// pitch-class column (DECISIONS.md Part II).
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

// Piano rolls (§3.4): drops spawn on a fixed now-line; the field scrolls.
// Pitch spans the cross axis with a small inset (full MIDI range).
static const float ROLL_NOW_LINE = 0.12f;
static const float ROLL_INSET   = 0.06f;

static void layout_roll_h(uint8_t note, float* out_x, float* out_y) {
    // Pitch -> y, low notes at the BOTTOM; now-line at x = 0.12; drift +x.
    *out_x = ROLL_NOW_LINE;
    *out_y = 1.0f - (ROLL_INSET + (((float)note + 0.5f) / 128.0f) * (1.0f - 2.0f * ROLL_INSET));
}

static void layout_roll_v(uint8_t note, float* out_x, float* out_y) {
    // Pitch -> x, low notes at the LEFT; now-line at y = 0.12; drift down.
    *out_x = ROLL_INSET + (((float)note + 0.5f) / 128.0f) * (1.0f - 2.0f * ROLL_INSET);
    *out_y = ROLL_NOW_LINE;
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
        case SUMI_LAYOUT_ROLL_H:
            layout_roll_h(note, out_x, out_y);
            return 1;
        case SUMI_LAYOUT_ROLL_V:
            layout_roll_v(note, out_x, out_y);
            return 1;
        case SUMI_LAYOUT_FIFTHS:
        default:
            // Unknown ids (including the not-yet-implemented rolls) fall back
            // to the default layout rather than crashing or clustering at 0,0.
            layout_fifths(note, aspect, out_x, out_y);
            return 1;
    }
}

bool sumi_layout_semitone_delta(uint32_t layout, uint8_t note,
                                const sumi_params_t* params, float aspect,
                                float* out_dx, float* out_dy) {
    if (out_dx) *out_dx = 0.0f;
    if (out_dy) *out_dy = 0.0f;
    // Primary echo (echo 0): the lattice's semitone vector is uniform across
    // an echo set (§3.4), so one delta serves all echoes.
    float px[SUMI_MAX_ECHOES], py[SUMI_MAX_ECHOES];
    sumi_layout_position(layout, note, params, aspect, px, py);
    const float x0 = px[0], y0 = py[0];
    float ux = 0.0f, uy = 0.0f, ulen = 1e9f;
    if (note < 127) {
        sumi_layout_position(layout, (uint8_t)(note + 1), params, aspect, px, py);
        ux = px[0] - x0; uy = py[0] - y0;
        ulen = sqrtf(ux * ux + uy * uy);
    }
    float dxm = 0.0f, dym = 0.0f, dlen = 1e9f;
    if (note > 0) {
        sumi_layout_position(layout, (uint8_t)(note - 1), params, aspect, px, py);
        dxm = x0 - px[0]; dym = y0 - py[0];   // still points toward increasing pitch
        dlen = sqrtf(dxm * dxm + dym * dym);
    }
    float dx, dy, len;
    if (ulen <= dlen) { dx = ux; dy = uy; len = ulen; }
    else              { dx = dxm; dy = dym; len = dlen; }
    if (len < 1e-6f || len > 1e8f) return false;   // degenerate (clamped twin)
    if (out_dx) *out_dx = dx;
    if (out_dy) *out_dy = dy;
    return true;
}

// --- §PHASE4 §2: the public, instance-free layout probe (ABI v0.3) ---------

// Inverse of layout_chroma_grid: (x, y) inside the inset rect -> note.
static bool probe_chroma_grid(float x, float y, uint8_t* out_note) {
    const float fx = (x - GRID_INSET_X) / (1.0f - 2.0f * GRID_INSET_X);
    const float fy = (y - GRID_INSET_Y) / (1.0f - 2.0f * GRID_INSET_Y);
    if (fx < 0.0f || fx >= 1.0f || fy < 0.0f || fy >= 1.0f) return false;
    int pc  = (int)(fx * 12.0f); if (pc > 11) pc = 11;
    int row = (int)(fy * 7.0f);  if (row > 6) row = 6;
    *out_note = (uint8_t)((row + 2) * 12 + pc);   // C1 (24) .. B7 (107)
    return true;
}

// Inverse of layout_janko: the touched ROW decides parity; the nearest
// staggered column decides the note. The half-cell dead zones the stagger
// leaves at a row's ends are honestly unplayable (off the key bed).
static bool probe_janko(float x, float y, uint8_t* out_note, int* out_row) {
    const float fy = (y - JANKO_INSET_Y) / (1.0f - 2.0f * JANKO_INSET_Y);
    if (fy < 0.0f || fy >= 1.0f) return false;
    int row = (int)(fy * (float)JANKO_ROWS);
    if (row > JANKO_ROWS - 1) row = JANKO_ROWS - 1;
    const int parity = row % 2;
    const float ncols = (float)(JANKO_COL_MAX - JANKO_COL_MIN + 1);
    const float fx = (x - JANKO_INSET_X) / (1.0f - 2.0f * JANKO_INSET_X);
    if (fx < 0.0f || fx >= 1.0f) return false;
    const float cx = fx * (ncols + 0.5f);
    const float col_f = cx - 0.5f - (parity == 1 ? 0.5f : 0.0f);
    const int col_rel = (int)floorf(col_f + 0.5f);   // nearest cell center
    if (col_rel < 0 || col_rel > JANKO_COL_MAX - JANKO_COL_MIN) return false;
    *out_note = (uint8_t)(2 * (col_rel + JANKO_COL_MIN) + parity);
    *out_row = row;
    return true;
}

bool sumi_layout_probe(uint32_t layout, const sumi_params_t* params, float aspect,
                       float norm_x, float norm_y, sumi_cell_info_t* out) {
    if (!out) return false;
    if (aspect <= 0.0f) aspect = 1.0f;

    uint8_t note = 0;
    float cw_norm, ch_norm;      // cell extents, normalized x / y units
    float cx, cy;                // cell center, normalized coords

    switch (layout) {
        case SUMI_LAYOUT_CHROMA_GRID: {
            if (!probe_chroma_grid(norm_x, norm_y, &note)) return false;
            layout_chroma_grid(note, &cx, &cy);
            cw_norm = (1.0f - 2.0f * GRID_INSET_X) / 12.0f;
            ch_norm = (1.0f - 2.0f * GRID_INSET_Y) / 7.0f;
            break;
        }
        case SUMI_LAYOUT_JANKO: {
            int row = 0;
            if (!probe_janko(norm_x, norm_y, &note, &row)) return false;
            // Center of the TOUCHED row's cell (§2: any echo row plays the
            // note; the loopback re-echoes it to all three automatically).
            float ex[SUMI_MAX_ECHOES], ey[SUMI_MAX_ECHOES];
            (void)layout_janko(note, ex, ey);      // echo x is row-independent
            cx = ex[0];
            cy = JANKO_INSET_Y + (((float)row + 0.5f) / (float)JANKO_ROWS) *
                                 (1.0f - 2.0f * JANKO_INSET_Y);
            const float ncols = (float)(JANKO_COL_MAX - JANKO_COL_MIN + 1);
            cw_norm = (1.0f - 2.0f * JANKO_INSET_X) / (ncols + 0.5f);
            ch_norm = (1.0f - 2.0f * JANKO_INSET_Y) / (float)JANKO_ROWS;
            break;
        }
        default:
            return false;   // FIFTHS / rolls / unknown: Play mode is meaningless
    }

    // DECISIONS_2 #7 delta (normalized coords) -> aspect-corrected unit
    // vector + true step in canvas-height units (§2 units contract).
    float ndx = 0.0f, ndy = 0.0f;
    if (!sumi_layout_semitone_delta(layout, note, params, aspect, &ndx, &ndy)) {
        return false;
    }
    const float pdx = ndx * aspect, pdy = ndy;
    const float step = sqrtf(pdx * pdx + pdy * pdy);
    if (step < 1e-6f) return false;

    out->note          = note;
    out->cell_center_x = cx;
    out->cell_center_y = cy;
    const float pw = cw_norm * aspect;   // physically smaller cell dimension
    out->cell_radius   = 0.5f * (pw < ch_norm ? pw : ch_norm);
    out->semitone_dx   = pdx / step;
    out->semitone_dy   = pdy / step;
    out->semitone_step = step;
    return true;
}

bool sumi_layout_field_motion(uint32_t layout, const sumi_params_t* params,
                              double dt, float* out_dx, float* out_dy) {
    if (out_dx) *out_dx = 0.0f;
    if (out_dy) *out_dy = 0.0f;
    if (layout != SUMI_LAYOUT_ROLL_H && layout != SUMI_LAYOUT_ROLL_V) {
        return false;   // static layouts never move the field
    }
    // §3.4: speed s = (bpm / 60) * roll_speed, in canvas lengths per second
    // (roll_speed = canvas-lengths-per-beat; defaults 120 / 0.0625 -> 16 beats
    // of history on canvas, i.e. 4 bars of 4/4, 8 s residence at 120 BPM).
    float bpm = params ? params->bpm : 120.0f;
    float roll_speed = params ? params->roll_speed : 0.0625f;
    if (bpm <= 0.0f) bpm = 120.0f;
    if (roll_speed <= 0.0f) roll_speed = 0.0625f;
    if (dt < 0.0) dt = 0.0;
    const float step = (bpm / 60.0f) * roll_speed * (float)dt;
    if (layout == SUMI_LAYOUT_ROLL_H) { if (out_dx) *out_dx = step; }
    else                              { if (out_dy) *out_dy = step; }
    return step != 0.0f;
}

} // extern "C"
