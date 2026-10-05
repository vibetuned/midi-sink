// layouts.cpp — pluggable pitch->position layouts (PROJECT_SPEC.md §3.4).
// Every layout is a pure function; nothing here touches voices, queues, or
// the GPU. The fifths layout is the v1 mapping moved verbatim (layout 0,
// zero behavior change).
#include "layouts.h"

#include <math.h>
#include <string.h>

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

// Piano grid (§3.4): the chroma grid's frame (C1..B7, same insets, same
// out-of-range clamp) with each octave drawn as a classical two-row keyboard:
// 5 accidentals on top at the classic boundary positions (C#/D#, F#/G#/A# —
// the E-F and B-C gaps stay empty), 7 naturals below. 7 octaves x 2 = 14
// rows, one echo. All x positions in "white-key units" (0..7 per octave row).
static const float PIANO_INSET_X = 0.08f;
static const float PIANO_INSET_Y = 0.10f;
static const int   PIANO_ROWS = 14;
// #61: a natural no longer fills its octave pair. It keeps the pair's BOTTOM
// 0.6 — the same fraction an accidental's key width already used, so the two
// cells come out the same size — and the strip freed above it is the
// GLISSANDO CORRIDOR: there an accidental is the only thing that can be hit,
// and between accidentals there is nothing, so a pen sliding through sustains
// (a dead zone makes no call, #39) and the black keys play as the pentatonic
// run a pianist expects. Below the corridor the naturals still tile their
// row, so their own glissando is untouched.
static const float PIANO_NATURAL_H = 0.6f;      // of the octave pair, bottom-aligned
// White-key index (0..6) per pitch class; -1 = accidental.
static const int   PIANO_WHITE_IDX[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
// Accidental center per pitch class, white-key units (0 for naturals).
static const float PIANO_BLACK_POS[12] = {0, 1.0f, 0, 2.0f, 0, 0, 4.0f, 0, 5.0f, 0, 6.0f, 0};

static void layout_piano_grid(uint8_t note, float* out_x, float* out_y) {
    const int pc = note % 12;
    int oct = (int)(note / 12) - 2;              // octave 1 -> 0 ... octave 7 -> 6
    if (oct < 0) oct = 0;                        // below C1: top octave pair
    if (oct > 6) oct = 6;                        // above B7: bottom octave pair
    const int wk = PIANO_WHITE_IDX[pc];
    const float xu = (wk >= 0) ? ((float)wk + 0.5f) : PIANO_BLACK_POS[pc];
    const int row = oct * 2 + (wk >= 0 ? 1 : 0); // accidentals above their naturals
    // #60: a cell sits at the centre of the region that PLAYS it, which for a
    // natural is the whole octave pair — its own row plus the white-key tops
    // above it (#41) — not the centre of the single drawn row. R_max has been
    // the pair's half-height since #29 ("a key's playable footprint is one key
    // wide by one octave tall"), so with the centre half a row too low the
    // drawn cell hung half a row below its own touch region: its bottom
    // quarter played the octave BELOW on screen, while the playable strip
    // above it was not drawn at all. Measured before the fix, at aspect 1.60:
    // C4 drawn y 0.4714..0.5857 against a touch region of 0.4430..0.5570.
    // Accidentals were always centred correctly — their region IS one row —
    // which is why this only ever showed on the piano grid.
    // #61: the natural is centred on the band it now owns — the pair's bottom
    // PIANO_NATURAL_H — so its cell still matches its touch region exactly
    // (#60's invariant), with its BOTTOM edge exactly where it was.
    const float row_pos = (wk >= 0)
        ? ((float)(row + 1) - PIANO_NATURAL_H * 2.0f * 0.5f)   // row+1 = pair bottom
        : ((float)row + 0.5f);
    *out_x = PIANO_INSET_X + (xu / 7.0f) * (1.0f - 2.0f * PIANO_INSET_X);
    *out_y = PIANO_INSET_Y + (row_pos / (float)PIANO_ROWS) *
                             (1.0f - 2.0f * PIANO_INSET_Y);
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

// v0.8 (DECISIONS_4 #64): the mirrored rolls — the same pitch axis, the
// now-line on the opposite edge, the field drifting the other way.
static void layout_roll_h_right(uint8_t note, float* out_x, float* out_y) {
    // Pitch -> y, low notes at the BOTTOM; now-line at x = 0.88; drift -x.
    *out_x = 1.0f - ROLL_NOW_LINE;
    *out_y = 1.0f - (ROLL_INSET + (((float)note + 0.5f) / 128.0f) * (1.0f - 2.0f * ROLL_INSET));
}

static void layout_roll_v_bottom(uint8_t note, float* out_x, float* out_y) {
    // Pitch -> x, low notes at the LEFT; now-line at y = 0.88; drift up.
    *out_x = ROLL_INSET + (((float)note + 0.5f) / 128.0f) * (1.0f - 2.0f * ROLL_INSET);
    *out_y = 1.0f - ROLL_NOW_LINE;
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


// --- Phase 9 step 60 (INSTRUMENT §2–§3, DECISIONS_8 #2): THE BRASS LAYOUTS, stateful -------------
// The trumpet: eight partial cells — the harmonic series a B♭ trumpet speaks,
// as SOUNDING MIDI notes (the pedal B♭1 through the 8th partial B♭4; the 7th
// idealised to A♭4) — and the three valves in `buttons` (bit 0 = valve 1 …).
// The cell under a touch is a PARTIAL; the note it sounds is the partial minus
// the valve combination's offset (1 = −2, 2 = −1, 3 = −3; combinations sum:
// the seven positions plus open). The real combinations' intonation quirks
// are deliberately not modelled — the idealised instrument (the spec's
// "realistic intonation" toggle is a later flavour). The trombone: seven
// partials (the 2nd through the 8th) and the slide in `slider`, 0..1 → 0..6
// semitones CONTINUOUS; the probe's note is the nearest semitone (the
// fraction rides the shell's pitch bend, so playing between positions is in
// tune with itself). Geometry: a COLUMN, the lowest partial at the bottom,
// the cells a tenth of the height tall (the trombone's a seventh of 0.8) and
// 2.5 cells wide to touch — or an ARC on the sheet (params.trumpet_arc, the
// name the ABI has; it arranges the trombone's seven too since the author's
// ask at step 63: the lowest partial at the left rising over the top to the
// right, the ring a little right of the middle). R_max is the
// column cell's half-height, the arc cell's radius. THE PITCH AXIS of both
// is +x with one semitone = R_max — INSTRUMENT §2's lip bend, ±1 semitone
// across the cell — and the glide renders along it under the mapper's cap
// (the valves' and the slide's bends nudge the drop, never move it a cell:
// the canvas stays ink, the strip shows the fingering).
static const uint8_t TRUMPET_PARTIALS[8]  = {46, 58, 65, 70, 74, 77, 80, 82};
static const uint8_t TROMBONE_PARTIALS[7] = {58, 65, 70, 74, 77, 80, 82};
static const int     VALVE_OFFSET[8]      = {0, 2, 1, 3, 3, 5, 4, 6};   // by buttons & 7: open, 1, 2, 1+2, 3, 1+3, 2+3, 1+2+3
static const float   BRASS_INSET_Y  = 0.10f;   // the column's vertical inset (the chroma grid's)
static const float   BRASS_COL_W    = 0.25f;   // the column's touch width, canvas heights (2.5 cells)
static const float   BRASS_ARC_R    = 0.30f;   // the arc's radius, canvas heights (the author's second fix at step 63: 0.42 was "the same
                                               //   size" — too big; 0.30 is as tight as the 0.055 cells allow, a fifth of a diameter between neighbours)
static const float   BRASS_ARC_CY   = 0.65f;   // the arc's centre height: the ring from y = 0.35 (the top) to 0.65 (the ends), its own middle the sheet's
static const float   BRASS_ARC_CX   = 0.08f;   // the arc's centre pushed right of the middle, canvas heights (the author's fix at step 63:
                                               //   the fingering panel takes the left side; mirrored, the overlay flips the sheet)
static const float   BRASS_ARC_CELL_R = 0.055f;   // the arc's cell radius (the author's fix: half the chord, 0.089, was "way too big")
static const float   SLIDE_SEMITONES = 6.0f;   // the slide's reach: seven positions over six semitones

struct brass_geom_t { int n; const uint8_t* partials; bool arc; float arc_r; };

static brass_geom_t brass_geom(uint32_t layout, const sumi_params_t* params, float aspect) {
    brass_geom_t g;
    if (layout == SUMI_LAYOUT_TROMBONE) { g.n = 7; g.partials = TROMBONE_PARTIALS; }
    else { g.n = 8; g.partials = TRUMPET_PARTIALS; }
    g.arc = params && params->trumpet_arc != 0u;   // one arrangement for the brass: the trombone's ring too (the author's ask at step 63)
    g.arc_r = BRASS_ARC_R;
    if (g.arc && (BRASS_ARC_CX + g.arc_r) / aspect > 0.46f) g.arc_r = 0.46f * aspect - BRASS_ARC_CX;   // a narrow sheet: the right end stays on it
    return g;
}

// Cell k's centre (normalized) and radius (canvas heights); k = 0 is the lowest partial.
static void brass_cell(const brass_geom_t& g, int k, float aspect, float* cx, float* cy, float* r) {
    if (g.arc) {
        const float th = 3.14159265358979f * (1.0f - (float)k / (float)(g.n - 1));   // π at the left … 0 at the right
        *cx = 0.5f + (BRASS_ARC_CX + g.arc_r * cosf(th)) / aspect;
        *cy = BRASS_ARC_CY - g.arc_r * sinf(th);
        *r  = BRASS_ARC_CELL_R;
    } else {
        const float h = (1.0f - 2.0f * BRASS_INSET_Y) / (float)g.n;
        *cx = 0.5f;
        *cy = (1.0f - BRASS_INSET_Y) - ((float)k + 0.5f) * h;
        const float w = BRASS_COL_W < h ? BRASS_COL_W : h;
        *r  = 0.5f * w;   // half the smaller dimension (the height: 0.05 for eight cells)
    }
}

// The cell under (x, y), or -1: the column's band and row; the arc's nearest centre within its radius.
static int brass_hit(const brass_geom_t& g, float aspect, float x, float y) {
    if (g.arc) {
        int best = -1; float bestd = 1e9f;
        for (int k = 0; k < g.n; k++) {
            float cx, cy, r; brass_cell(g, k, aspect, &cx, &cy, &r);
            const float dx = (x - cx) * aspect, dy = y - cy;
            const float d = sqrtf(dx * dx + dy * dy);
            if (d <= r && d < bestd) { best = k; bestd = d; }
        }
        return best;
    }
    if (fabsf(x - 0.5f) * aspect > 0.5f * BRASS_COL_W) return -1;
    if (y < BRASS_INSET_Y || y >= 1.0f - BRASS_INSET_Y) return -1;
    const float h = (1.0f - 2.0f * BRASS_INSET_Y) / (float)g.n;
    int k = (int)(((1.0f - BRASS_INSET_Y) - y) / h);
    if (k < 0) k = 0;
    if (k > g.n - 1) k = g.n - 1;
    return k;
}

static int trumpet_offset(const sumi_layout_state_t* st) { return VALVE_OFFSET[st ? (st->buttons & 7u) : 0u]; }
static float slide_semis(const sumi_layout_state_t* st) {
    float s = st ? st->slider : 0.0f;
    if (!(s > 0.0f)) s = 0.0f;
    if (s > 1.0f) s = 1.0f;
    return s * SLIDE_SEMITONES;
}
static int brass_nearest_partial(const brass_geom_t& g, float pitch) {
    int best = 0; float bestd = 1e9f;
    for (int k = 0; k < g.n; k++) {
        const float d = fabsf((float)g.partials[k] - pitch);
        if (d < bestd) { best = k; bestd = d; }   // ties: the lower partial
    }
    return best;
}

// Which cell a note on the wire lands in: the partial the CURRENT fingering sounds it from; failing
// that the standard fingering (the trumpet's smallest valve offset; the trombone's lowest partial
// within the slide's reach); failing that the nearest partial by pitch (the gaps a real horn has).
static int brass_cell_of_note(const brass_geom_t& g, uint32_t layout, uint8_t note, const sumi_layout_state_t* st) {
    if (layout == SUMI_LAYOUT_TROMBONE) {
        const float p = (float)note + slide_semis(st);
        const int k = brass_nearest_partial(g, p);
        if (fabsf((float)g.partials[k] - p) <= 1.0f + 1e-4f) return k;   // within a semitone: the slide's own partial (the smoothing's lag tolerated)
        for (int j = 0; j < g.n; j++)
            if (g.partials[j] >= note && (int)g.partials[j] - (int)note <= (int)SLIDE_SEMITONES) return j;
        return brass_nearest_partial(g, (float)note);
    }
    const int off = trumpet_offset(st);
    for (int k = 0; k < g.n; k++) if ((int)g.partials[k] == (int)note + off) return k;
    for (int o = 0; o <= 6; o++)
        for (int k = 0; k < g.n; k++) if ((int)g.partials[k] == (int)note + o) return k;
    return brass_nearest_partial(g, (float)note);
}

// The note cell k sounds under the state.
static uint8_t brass_note_of_cell(const brass_geom_t& g, uint32_t layout, int k, const sumi_layout_state_t* st) {
    int n = (int)g.partials[k];
    if (layout == SUMI_LAYOUT_TROMBONE) n -= (int)floorf(slide_semis(st) + 0.5f + 1e-3f);   // the nearest semitone; a midpoint rounds to the next position
    else n -= trumpet_offset(st);
    if (n < 0) n = 0;
    if (n > 127) n = 127;
    return (uint8_t)n;
}

static bool is_brass(uint32_t layout) { return layout == SUMI_LAYOUT_TRUMPET || layout == SUMI_LAYOUT_TROMBONE; }

// --- Phase 9 step 61 (INSTRUMENT §4, DECISIONS_8 #5–#7): THE STATELESS ADDITIONS --------------------
// WICKI–HAYDEN (SUMI_LAYOUT_WICKI): the concertina's button-field — a hex grid
// where a step right is a whole tone, up-right a fifth, up-left a fourth, and
// two rows straight up the octave. Six buttons a row (a whole-tone scale), the
// rows alternating the two whole-tone scales and offset half a button: the
// width at which every note has EXACTLY ONE button (a seventh would repeat
// the row two rows up and six buttons left), so the layout is one echo by
// construction. Fifteen rows from G0 — the row below C1, which holds C♯1, D♯1
// and F1 — to F8, C1..B7 among them; a note off the grid takes its whole-tone
// column on the nearest row of its parity (the pitch class kept, as the grids
// clamp). Pitch is a PLANE over the sheet (the stagger makes it exactly
// linear: two semitones a button, six a row), so the pitch axis is that
// plane's gradient — the shortest-neighbour rule would pick a semitone's
// neighbour three buttons away on the next row.
static const int   WICKI_COLS = 6, WICKI_ROWS = 15;
static const int   WICKI_BASE = 19;                        // row 0, button 0: G0
static const float WICKI_INSET_X = 0.08f, WICKI_INSET_Y = 0.10f;

static int   wicki_note(int i, int c) { return WICKI_BASE + 6 * i - (i & 1) + 2 * c; }   // row i from the bottom, button c
static float wicki_xoff(int i) { return (i & 1) ? 0.0f : 0.5f; }                         // the even rows sit half a button right
static void  wicki_cell(int i, int c, float* cx, float* cy) {
    *cx = WICKI_INSET_X + (((float)c + wicki_xoff(i) + 0.5f) / ((float)WICKI_COLS + 0.5f)) * (1.0f - 2.0f * WICKI_INSET_X);
    *cy = (1.0f - WICKI_INSET_Y) - (((float)i + 0.5f) / (float)WICKI_ROWS) * (1.0f - 2.0f * WICKI_INSET_Y);
}
static int floor_div(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
// The button of a note: an odd note sits on an even row (19 + 12m + 2c), an even one on an odd row
// (24 + 12m + 2c); a row off the grid clamps to the nearest row of its parity, the button kept.
static void wicki_button_of(int note, int* i, int* c) {
    if (note & 1) { const int k = note - 19; const int m = floor_div(k, 12); *i = 2 * m;     *c = (k - 12 * m) / 2; }
    else          { const int k = note - 24; const int m = floor_div(k, 12); *i = 2 * m + 1; *c = (k - 12 * m) / 2; }
    while (*i < 0) *i += 2;
    while (*i > WICKI_ROWS - 1) *i -= 2;
}
static bool probe_wicki(float x, float y, int* out_i, int* out_c) {
    const float fy = (y - WICKI_INSET_Y) / (1.0f - 2.0f * WICKI_INSET_Y);
    if (fy < 0.0f || fy >= 1.0f) return false;
    int i = (int)((1.0f - fy) * (float)WICKI_ROWS);
    if (i > WICKI_ROWS - 1) i = WICKI_ROWS - 1;
    if (i < 0) i = 0;
    const float fx = (x - WICKI_INSET_X) / (1.0f - 2.0f * WICKI_INSET_X);
    if (fx < 0.0f || fx >= 1.0f) return false;
    const float cxf = fx * ((float)WICKI_COLS + 0.5f) - 0.5f - wicki_xoff(i);
    const int c = (int)floorf(cxf + 0.5f);                 // the nearest button; the stagger's half-button ends are off the field
    if (c < 0 || c > WICKI_COLS - 1) return false;
    *out_i = i; *out_c = c;
    return true;
}

// STRINGS (SUMI_LAYOUT_STRINGS): string-rows × chromatic frets — the open string and two octaves,
// the lowest string at the BOTTOM (tab's way), the nut at the left. The tuning is a fixed preset
// (params.string_tuning). A note's sites are EVERY string that reaches it (SUMI_MAX_ECHOES holds
// them all — the author's fix at step 63: a note played high on a low string must light the cell
// under the hand), the first position first (the highest string that reaches it); a note under the
// lowest open string or over the top string's last fret takes that edge cell. The pitch axis runs along the
// string, one fret a semitone: dragging along a string is literally a string bend (INSTRUMENT §4).
static const int   STRINGS_FRETS = 24;                     // columns 0..24
static const float STRINGS_INSET_X = 0.08f, STRINGS_INSET_Y = 0.10f;
struct string_set_t { int n; uint8_t open[12]; };
static string_set_t string_set(const sumi_params_t* params) {
    const uint32_t t = params ? params->string_tuning : 0u;
    string_set_t s;
    if (t == SUMI_STRINGS_WHOLE_TONE_TAP) { s.n = 12; for (int k = 0; k < 12; k++) s.open[k] = (uint8_t)(40 + 2 * k); }   // E2 … D4
    else if (t == SUMI_STRINGS_ALL_FOURTHS) { s.n = 6; const uint8_t o[6] = {40, 45, 50, 55, 60, 65}; memcpy(s.open, o, 6); } // E2 A2 D3 G3 C4 F4
    else { s.n = 6; const uint8_t o[6] = {40, 45, 50, 55, 59, 64}; memcpy(s.open, o, 6); }                                  // E2 A2 D3 G3 B3 E4
    return s;
}
static void strings_cell(const string_set_t& s, int str, int fret, float* cx, float* cy) {
    *cx = STRINGS_INSET_X + (((float)fret + 0.5f) / (float)(STRINGS_FRETS + 1)) * (1.0f - 2.0f * STRINGS_INSET_X);
    *cy = (1.0f - STRINGS_INSET_Y) - (((float)str + 0.5f) / (float)s.n) * (1.0f - 2.0f * STRINGS_INSET_Y);
}
static uint32_t strings_sites(const string_set_t& s, int note, int* str_out, int* fret_out, uint32_t max) {
    uint32_t n = 0;
    for (int k = s.n - 1; k >= 0 && n < max; k--) {        // from the highest string down: the smallest fret first
        const int f = note - (int)s.open[k];
        if (f >= 0 && f <= STRINGS_FRETS) { str_out[n] = k; fret_out[n] = f; n++; }
    }
    if (n == 0) {
        if (note < (int)s.open[0]) { str_out[0] = 0; fret_out[0] = 0; }
        else { str_out[0] = s.n - 1; fret_out[0] = STRINGS_FRETS; }
        n = 1;
    }
    return n;
}
static bool probe_strings(const string_set_t& s, float x, float y, int* out_str, int* out_fret) {
    const float fx = (x - STRINGS_INSET_X) / (1.0f - 2.0f * STRINGS_INSET_X);
    const float fy = (y - STRINGS_INSET_Y) / (1.0f - 2.0f * STRINGS_INSET_Y);
    if (fx < 0.0f || fx >= 1.0f || fy < 0.0f || fy >= 1.0f) return false;
    int fret = (int)(fx * (float)(STRINGS_FRETS + 1)); if (fret > STRINGS_FRETS) fret = STRINGS_FRETS;
    int str = (int)((1.0f - fy) * (float)s.n); if (str > s.n - 1) str = s.n - 1; if (str < 0) str = 0;
    *out_str = str; *out_fret = fret;
    return true;
}

// THEREMIN (SUMI_LAYOUT_THEREMIN): no cells. X is pitch — C2 at the left edge of the field to C7 at
// the right, five octaves, continuous; Y the bipolar press axis about the middle. The probe answers
// anywhere on the field with the CONTINUOUS flag: the nearest semitone as the note, that semitone's
// x as the centre (so a surface reads the fraction off the step), the half height as R_max, the
// axis +x with one semitone the field's width over 61. The engine places a note at its semitone's x
// and renders a glide at the true step: the drop travels under the hand.
static const int   THEREMIN_LOW = 36, THEREMIN_RANGE = 60;   // C2 .. C7: 61 semitone slots
static const float THEREMIN_INSET_X = 0.08f, THEREMIN_INSET_Y = 0.10f;
static float theremin_x_of(int note) {
    return THEREMIN_INSET_X + (((float)(note - THEREMIN_LOW) + 0.5f) / (float)(THEREMIN_RANGE + 1)) * (1.0f - 2.0f * THEREMIN_INSET_X);
}
static int theremin_clamp(int note) { return note < THEREMIN_LOW ? THEREMIN_LOW : (note > THEREMIN_LOW + THEREMIN_RANGE ? THEREMIN_LOW + THEREMIN_RANGE : note); }
static bool probe_theremin(float x, float y, int* out_note) {
    const float fx = (x - THEREMIN_INSET_X) / (1.0f - 2.0f * THEREMIN_INSET_X);
    const float fy = (y - THEREMIN_INSET_Y) / (1.0f - 2.0f * THEREMIN_INSET_Y);
    if (fx < 0.0f || fx >= 1.0f || fy < 0.0f || fy >= 1.0f) return false;
    int slot = (int)(fx * (float)(THEREMIN_RANGE + 1)); if (slot > THEREMIN_RANGE) slot = THEREMIN_RANGE;
    *out_note = THEREMIN_LOW + slot;
    return true;
}

extern "C" {

uint32_t sumi_layout_position(uint32_t layout, uint8_t note,
                              const sumi_params_t* params, float aspect,
                              const sumi_layout_state_t* state,
                              float* out_x, float* out_y) {
    if (aspect <= 0.0f) aspect = 1.0f;
    if (note > 127) note = 127;
    switch (layout) {
        case SUMI_LAYOUT_TRUMPET:
        case SUMI_LAYOUT_TROMBONE: {
            // step 60: the partial cell the note sounds from under the state
            const brass_geom_t g = brass_geom(layout, params, aspect);
            float r;
            brass_cell(g, brass_cell_of_note(g, layout, note, state), aspect, out_x, out_y, &r);
            return 1;
        }
        case SUMI_LAYOUT_WICKI: {
            int i, c; wicki_button_of((int)note, &i, &c);
            wicki_cell(i, c, out_x, out_y);
            return 1;
        }
        case SUMI_LAYOUT_STRINGS: {
            const string_set_t s = string_set(params);
            int str[SUMI_MAX_ECHOES], fret[SUMI_MAX_ECHOES];
            const uint32_t n = strings_sites(s, (int)note, str, fret, SUMI_MAX_ECHOES);
            for (uint32_t e = 0; e < n; e++) strings_cell(s, str[e], fret[e], &out_x[e], &out_y[e]);
            return n;
        }
        case SUMI_LAYOUT_THEREMIN:
            *out_x = theremin_x_of(theremin_clamp((int)note));
            *out_y = 0.5f;
            return 1;
        case SUMI_LAYOUT_CHROMA_GRID:
            layout_chroma_grid(note, out_x, out_y);
            return 1;
        case SUMI_LAYOUT_JANKO:
            return layout_janko(note, out_x, out_y);
        case SUMI_LAYOUT_PIANO_GRID:
            layout_piano_grid(note, out_x, out_y);
            return 1;
        case SUMI_LAYOUT_ROLL_H:
            layout_roll_h(note, out_x, out_y);
            return 1;
        case SUMI_LAYOUT_ROLL_V:
            layout_roll_v(note, out_x, out_y);
            return 1;
        case SUMI_LAYOUT_ROLL_H_RIGHT:
            layout_roll_h_right(note, out_x, out_y);
            return 1;
        case SUMI_LAYOUT_ROLL_V_BOTTOM:
            layout_roll_v_bottom(note, out_x, out_y);
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
                                const sumi_layout_state_t* state,
                                float* out_dx, float* out_dy) {
    if (out_dx) *out_dx = 0.0f;
    if (out_dy) *out_dy = 0.0f;
    // step 60: the brass layouts' axis is the lip bend — +x, one semitone per cell radius (INSTRUMENT §2);
    // the partials are not a lattice, so the shortest-neighbour rule below would read the column.
    if (is_brass(layout)) {
        (void)note; (void)state;
        if (aspect <= 0.0f) aspect = 1.0f;
        const brass_geom_t g = brass_geom(layout, params, aspect);
        float cx, cy, r; brass_cell(g, 0, aspect, &cx, &cy, &r);
        if (out_dx) *out_dx = r / aspect;
        return true;
    }
    // step 61: the Wicki–Hayden plane's gradient (two semitones a button, six a row, the row above
    // being UP on the sheet), as the vector of one semitone — normalized coordinates, the true step
    if (layout == SUMI_LAYOUT_WICKI) {
        (void)note; (void)state;
        if (aspect <= 0.0f) aspect = 1.0f;
        const float cw = (1.0f - 2.0f * WICKI_INSET_X) / ((float)WICKI_COLS + 0.5f) * aspect;   // a button's width, canvas heights
        const float ch = (1.0f - 2.0f * WICKI_INSET_Y) / (float)WICKI_ROWS;
        const float gx = 2.0f / cw, gy = -6.0f / ch;                 // semitones per canvas height, +x and +y (down)
        const float g2 = gx * gx + gy * gy;
        if (out_dx) *out_dx = (gx / g2) / aspect;
        if (out_dy) *out_dy = gy / g2;
        return true;
    }
    // step 61: along the string, one fret; along the theremin's field, one semitone slot
    if (layout == SUMI_LAYOUT_STRINGS) {
        (void)note; (void)state; (void)params;
        if (out_dx) *out_dx = (1.0f - 2.0f * STRINGS_INSET_X) / (float)(STRINGS_FRETS + 1);
        return true;
    }
    if (layout == SUMI_LAYOUT_THEREMIN) {
        (void)note; (void)state; (void)params;
        if (out_dx) *out_dx = (1.0f - 2.0f * THEREMIN_INSET_X) / (float)(THEREMIN_RANGE + 1);
        return true;
    }
    // Jankó (DECISIONS_3 #18): pitch is a function of x ALONE — the parity
    // rows are ECHOES of the same notes, so the shortest-neighbor rule below
    // would pick the stagger vector (mostly vertical, toward note±1's echo
    // row), making glides read orthogonal to the chromatic grid's. The true
    // semitone step is half a column straight along +x; glissandi stay in
    // the touched row. (Amends the spec §3.4 "half-column over, one row up"
    // phrasing — the x component of that vector, without the echo-row hop.)
    if (layout == SUMI_LAYOUT_JANKO) {
        (void)note; (void)params; (void)aspect;
        const float ncols = (float)(JANKO_COL_MAX - JANKO_COL_MIN + 1);
        if (out_dx) *out_dx = (0.5f / (ncols + 0.5f)) * (1.0f - 2.0f * JANKO_INSET_X);
        return true;
    }
    // PIANO_GRID deliberately takes the generic rule below: unlike Jankó,
    // pitch is NOT a function of x alone (two rows per octave), so the honest
    // semitone axis is per-note — the half-key diagonal toward the adjacent
    // accidental/natural (DECISIONS_3 #29).
    // Primary echo (echo 0): the lattice's semitone vector is uniform across
    // an echo set (§3.4), so one delta serves all echoes.
    float px[SUMI_MAX_ECHOES], py[SUMI_MAX_ECHOES];
    sumi_layout_position(layout, note, params, aspect, state, px, py);
    const float x0 = px[0], y0 = py[0];
    float ux = 0.0f, uy = 0.0f, ulen = 1e9f;
    if (note < 127) {
        sumi_layout_position(layout, (uint8_t)(note + 1), params, aspect, state, px, py);
        ux = px[0] - x0; uy = py[0] - y0;
        ulen = sqrtf(ux * ux + uy * uy);
    }
    float dxm = 0.0f, dym = 0.0f, dlen = 1e9f;
    if (note > 0) {
        sumi_layout_position(layout, (uint8_t)(note - 1), params, aspect, state, px, py);
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

// --- PROJECT_SPEC.md §8.2: the public, instance-free layout probe (ABI v0.3) ---------

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

// Inverse of layout_piano_grid (#41 revision): accidentals are NARROW
// (0.6 white units, like real black keys), and the black-row area they do
// not cover belongs to the NATURAL below — white-key TOPS, which is what
// makes a natural-to-natural glissando possible without grazing accidentals
// (the whole point of the piano layout). No dead zones remain on this
// lattice: the E-F / B-C gaps and the row ends are white-key tops too.
static const float PIANO_BLACK_HALF_W = 0.3f;   // half of 0.6 white units

static bool probe_piano_grid(float x, float y, uint8_t* out_note) {
    const float fx = (x - PIANO_INSET_X) / (1.0f - 2.0f * PIANO_INSET_X);
    const float fy = (y - PIANO_INSET_Y) / (1.0f - 2.0f * PIANO_INSET_Y);
    if (fx < 0.0f || fx >= 1.0f || fy < 0.0f || fy >= 1.0f) return false;
    static const int WHITE_PC[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int BLACK_PC[5] = {1, 3, 6, 8, 10};
    // Octave PAIRS, not rows: t runs 0 at the pair's top to 1 at its bottom.
    const float pf = fy * (float)(PIANO_ROWS / 2);
    int oct = (int)pf;
    if (oct > PIANO_ROWS / 2 - 1) oct = PIANO_ROWS / 2 - 1;
    const float t = pf - (float)oct;
    const float xu = fx * 7.0f;                  // white-key units
    int pc = -1;
    if (t < 0.5f) {                              // the accidental's own row
        for (int i = 0; i < 5; i++) {
            const float c = PIANO_BLACK_POS[BLACK_PC[i]];
            if (xu >= c - PIANO_BLACK_HALF_W && xu < c + PIANO_BLACK_HALF_W) {
                pc = BLACK_PC[i];
                break;
            }
        }
    }
    if (pc < 0) {
        // #61: the natural owns the pair's BOTTOM PIANO_NATURAL_H only. Above
        // that, off an accidental, is the glissando corridor — deliberately
        // nothing, so a slide sustains instead of sounding the white key.
        if (t < 1.0f - PIANO_NATURAL_H) return false;
        int wk = (int)xu;
        if (wk > 6) wk = 6;
        pc = WHITE_PC[wk];
    }
    *out_note = (uint8_t)((oct + 2) * 12 + pc);  // C1 (24) .. B7 (107)
    return true;
}

bool sumi_layout_probe(uint32_t layout, const sumi_params_t* params, float aspect,
                       const sumi_layout_state_t* state, float norm_x, float norm_y,
                       sumi_cell_info_t* out) {
    if (!out) return false;
    if (aspect <= 0.0f) aspect = 1.0f;

    uint8_t note = 0;
    float cw_norm, ch_norm;      // cell extents, normalized x / y units
    float cx, cy;                // cell center, normalized coords

    switch (layout) {
        case SUMI_LAYOUT_TRUMPET:
        case SUMI_LAYOUT_TROMBONE: {
            // step 60 (INSTRUMENT §1): the state decides the NOTE, never the geometry — a NULL state is
            // open valves and the slide in. The cell is the partial's circle: R_max its radius.
            const brass_geom_t g = brass_geom(layout, params, aspect);
            const int k = brass_hit(g, aspect, norm_x, norm_y);
            if (k < 0) return false;
            float r;
            brass_cell(g, k, aspect, &cx, &cy, &r);
            note = brass_note_of_cell(g, layout, k, state);
            cw_norm = 2.0f * r / aspect;
            ch_norm = 2.0f * r;
            break;
        }
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
        case SUMI_LAYOUT_PIANO_GRID: {
            if (!probe_piano_grid(norm_x, norm_y, &note)) return false;
            layout_piano_grid(note, &cx, &cy);
            // R_max uses the key's playable footprint — one key wide, one
            // OCTAVE PAIR tall — not the single drawn row (DECISIONS_3 #29):
            // the black/white row split is a drawing convention, R_max is a
            // travel bound, and the inscribed single-row radius made the
            // Play-mode knobs half the chroma grid's. This makes the vertical
            // measure identical to the chroma grid's row height (0.8/7).
            // #41: accidentals are 0.6 keys wide — their footprint (and knob)
            // is proportionally smaller, like real black keys.
            // #57: scale BOTH axes by key_w, so the accidental's footprint is
            // a SIMILAR rectangle and the 0.6 proportion survives whichever
            // dimension the inscribed circle below ends up limited by. With
            // the height left at the full octave pair, the narrowing showed
            // only while the width was the smaller dimension — i.e. below
            // aspect (0.80/7)/(0.6·0.84/7) = 1.5873 — so accidentals read as
            // narrow on an iPad (1.44) and identical to naturals on any
            // 16:10 or wider tablet. One build, three looks.
            // #61: WIDTH still separates them — a black key is 0.6 of a white
            // one — but both now stand PIANO_NATURAL_H of an octave pair tall
            // (the accidental in its row, the natural in the band below the
            // corridor), so where height governs, which is every landscape
            // aspect, the two knobs come out the same size. That is the
            // "same size as the accidental" the surface is built around.
            const int pcq = note % 12;
            const bool blackq = pcq == 1 || pcq == 3 || pcq == 6 || pcq == 8 || pcq == 10;
            const float key_w = blackq ? 2.0f * PIANO_BLACK_HALF_W : 1.0f;
            cw_norm = key_w * (1.0f - 2.0f * PIANO_INSET_X) / 7.0f;
            ch_norm = PIANO_NATURAL_H * (1.0f - 2.0f * PIANO_INSET_Y) / 7.0f;
            break;
        }
        case SUMI_LAYOUT_WICKI: {
            int i, c;
            if (!probe_wicki(norm_x, norm_y, &i, &c)) return false;
            note = (uint8_t)wicki_note(i, c);
            wicki_cell(i, c, &cx, &cy);
            cw_norm = (1.0f - 2.0f * WICKI_INSET_X) / ((float)WICKI_COLS + 0.5f);
            ch_norm = (1.0f - 2.0f * WICKI_INSET_Y) / (float)WICKI_ROWS;
            break;
        }
        case SUMI_LAYOUT_STRINGS: {
            const string_set_t s = string_set(params);
            int str, fret;
            if (!probe_strings(s, norm_x, norm_y, &str, &fret)) return false;
            note = (uint8_t)((int)s.open[str] + fret);
            strings_cell(s, str, fret, &cx, &cy);
            cw_norm = (1.0f - 2.0f * STRINGS_INSET_X) / (float)(STRINGS_FRETS + 1);
            ch_norm = (1.0f - 2.0f * STRINGS_INSET_Y) / (float)s.n;
            break;
        }
        case SUMI_LAYOUT_THEREMIN: {
            int n;
            if (!probe_theremin(norm_x, norm_y, &n)) return false;
            note = (uint8_t)n;
            cx = theremin_x_of(n);
            cy = 0.5f;
            cw_norm = 1.0f;                                         // R_max is the half height: the press axis's travel
            ch_norm = 1.0f - 2.0f * THEREMIN_INSET_Y;
            break;
        }
        default:
            return false;   // FIFTHS / rolls / unknown: Play mode is meaningless
    }

    // DECISIONS_2 #7 delta (normalized coords) -> aspect-corrected unit
    // vector + true step in canvas-height units (§2 units contract).
    float ndx = 0.0f, ndy = 0.0f;
    if (!sumi_layout_semitone_delta(layout, note, params, aspect, state, &ndx, &ndy)) {
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
    out->flags         = layout == SUMI_LAYOUT_THEREMIN ? SUMI_CELL_CONTINUOUS : 0u;   // 1.5.0: the theremin's cell has no discrete note
    return true;
}

uint32_t sumi_layout_cells(uint32_t layout, const sumi_params_t* params, float aspect,
                           float* out, uint32_t max_cells) {
    if (!out || max_cells == 0u) return 0u;
    if (aspect <= 0.0f) aspect = 1.0f;
    if (is_brass(layout)) {
        // step 60: the partial cells themselves — eight or seven discs, the layout's own checkerboard
        // alternating up the series; the state changes what they sound, never where they are.
        const brass_geom_t g = brass_geom(layout, params, aspect);
        uint32_t n = 0;
        for (int k = 0; k < g.n && n < max_cells; k++, n++) {
            float cx, cy, r; brass_cell(g, k, aspect, &cx, &cy, &r);
            out[4u * n] = cx; out[4u * n + 1u] = cy; out[4u * n + 2u] = r;
            out[4u * n + 3u] = (k & 1) ? 2.0f : 0.0f;
        }
        return n;
    }
    if (layout == SUMI_LAYOUT_WICKI || layout == SUMI_LAYOUT_STRINGS || layout == SUMI_LAYOUT_THEREMIN) {
        // step 61: the cells enumerated as the grid has them — every button, every (string, fret),
        // the theremin's 61 semitone slots as imaginary cells — not through the notes' placements
        // (a string's high-fret cells are nobody's echo, yet they are keys the shells draw)
        uint32_t n = 0;
        if (layout == SUMI_LAYOUT_WICKI) {
            const float cw = (1.0f - 2.0f * WICKI_INSET_X) / ((float)WICKI_COLS + 0.5f) * aspect;
            const float ch = (1.0f - 2.0f * WICKI_INSET_Y) / (float)WICKI_ROWS;
            const float r = 0.5f * (cw < ch ? cw : ch);
            for (int i = 0; i < WICKI_ROWS; i++) for (int c = 0; c < WICKI_COLS && n < max_cells; c++, n++) {
                float cx, cy; wicki_cell(i, c, &cx, &cy);
                const int pc = wicki_note(i, c) % 12;
                const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
                out[4u * n] = cx; out[4u * n + 1u] = cy; out[4u * n + 2u] = r;
                out[4u * n + 3u] = (black ? 1.0f : 0.0f) + (((i + c) & 1) ? 2.0f : 0.0f);
            }
        } else if (layout == SUMI_LAYOUT_STRINGS) {
            const string_set_t s = string_set(params);
            const float cw = (1.0f - 2.0f * STRINGS_INSET_X) / (float)(STRINGS_FRETS + 1) * aspect;
            const float ch = (1.0f - 2.0f * STRINGS_INSET_Y) / (float)s.n;
            const float r = 0.5f * (cw < ch ? cw : ch);
            for (int k = 0; k < s.n; k++) for (int f = 0; f <= STRINGS_FRETS && n < max_cells; f++, n++) {
                float cx, cy; strings_cell(s, k, f, &cx, &cy);
                const int pc = ((int)s.open[k] + f) % 12;
                const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
                out[4u * n] = cx; out[4u * n + 1u] = cy; out[4u * n + 2u] = r;
                out[4u * n + 3u] = (black ? 1.0f : 0.0f) + (((k + f) & 1) ? 2.0f : 0.0f);
            }
        } else {
            const float r = 0.5f * (1.0f - 2.0f * THEREMIN_INSET_X) / (float)(THEREMIN_RANGE + 1) * aspect;   // half a semitone slot
            for (int k = 0; k <= THEREMIN_RANGE && n < max_cells; k++, n++) {
                const int pc = (THEREMIN_LOW + k) % 12;
                const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
                out[4u * n] = theremin_x_of(THEREMIN_LOW + k); out[4u * n + 1u] = 0.5f; out[4u * n + 2u] = r;
                out[4u * n + 3u] = (black ? 1.0f : 0.0f) + ((k & 1) ? 2.0f : 0.0f);
            }
        }
        return n;
    }
    const bool keyed = layout == SUMI_LAYOUT_CHROMA_GRID || layout == SUMI_LAYOUT_JANKO || layout == SUMI_LAYOUT_PIANO_GRID;
    float imag_r = 0.0f;                                   // the largest circle that touches no neighbour's
    switch (layout) {
        case SUMI_LAYOUT_FIFTHS: imag_r = 0.5f * (COF_R_OUTER - COF_R_INNER) / 10.0f; break;   // half an octave ring
        case SUMI_LAYOUT_ROLL_H: case SUMI_LAYOUT_ROLL_H_RIGHT: imag_r = 0.5f * (1.0f - 2.0f * ROLL_INSET) / 128.0f; break;            // half a semitone (y)
        case SUMI_LAYOUT_ROLL_V: case SUMI_LAYOUT_ROLL_V_BOTTOM: imag_r = 0.5f * (1.0f - 2.0f * ROLL_INSET) / 128.0f * aspect; break;   // half a semitone (x)
        default: if (!keyed) return 0u; break;
    }
    uint32_t n = 0;
    for (int note = 0; note < 128 && n < max_cells; note++) {
        float ex[SUMI_MAX_ECHOES], ey[SUMI_MAX_ECHOES];
        const uint32_t ne = sumi_layout_position(layout, (uint8_t)note, params, aspect, NULL, ex, ey);
        for (uint32_t e = 0; e < ne && n < max_cells; e++) {
            float cx = ex[e], cy = ey[e], r = imag_r;
            if (keyed) {
                sumi_cell_info_t info;
                if (!sumi_layout_probe(layout, params, aspect, NULL, cx, cy, &info)) continue;
                cx = info.cell_center_x; cy = info.cell_center_y; r = info.cell_radius;
            }
            bool seen = false;
            for (uint32_t k = 0; k < n && !seen; k++)
                seen = fabsf(out[4u * k] - cx) < 1e-5f && fabsf(out[4u * k + 1u] - cy) < 1e-5f;
            if (seen) continue;
            const int pc = note % 12, octave = note / 12;
            const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            // the layout's own checkerboard: neighbouring cells differ in parity
            int odd = 0;
            switch (layout) {
                case SUMI_LAYOUT_CHROMA_GRID: { int row = octave - 2; if (row < 0) row = 0; if (row > 6) row = 6; odd = (pc + row) & 1; break; }
                case SUMI_LAYOUT_JANKO: { int col = note / 2; if (col < JANKO_COL_MIN) col = JANKO_COL_MIN; if (col > JANKO_COL_MAX) col = JANKO_COL_MAX;
                                          odd = (col + (note % 2) + 2 * (int)e) & 1; break; }
                case SUMI_LAYOUT_PIANO_GRID: { static const int wk[12] = {0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6};
                                               odd = (wk[pc] + octave) & 1; break; }
                case SUMI_LAYOUT_FIFTHS: odd = (((pc * 7) % 12) + octave) & 1; break;
                default: odd = note & 1; break;                        // the rolls: semitone lanes
            }
            out[4u * n] = cx; out[4u * n + 1u] = cy; out[4u * n + 2u] = r;
            out[4u * n + 3u] = (black ? 1.0f : 0.0f) + (odd ? 2.0f : 0.0f);
            n++;
        }
    }
    return n;
}


bool sumi_layout_field_motion(uint32_t layout, const sumi_params_t* params,
                              double dt, float* out_dx, float* out_dy) {
    if (out_dx) *out_dx = 0.0f;
    if (out_dy) *out_dy = 0.0f;
    const bool roll = layout == SUMI_LAYOUT_ROLL_H || layout == SUMI_LAYOUT_ROLL_V ||
                      layout == SUMI_LAYOUT_ROLL_H_RIGHT || layout == SUMI_LAYOUT_ROLL_V_BOTTOM;
    if (!roll) {
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
    // Away from the now-line: +x from the left, -x from the right (#64),
    // down from the top, up from the bottom (#64).
    switch (layout) {
        case SUMI_LAYOUT_ROLL_H:        if (out_dx) *out_dx =  step; break;
        case SUMI_LAYOUT_ROLL_H_RIGHT:  if (out_dx) *out_dx = -step; break;
        case SUMI_LAYOUT_ROLL_V:        if (out_dy) *out_dy =  step; break;
        default:                        if (out_dy) *out_dy = -step; break;   // ROLL_V_BOTTOM
    }
    return step != 0.0f;
}

} // extern "C"
