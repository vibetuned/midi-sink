/* piano_sweep.c — evidence generator for SUMI_LAYOUT_PIANO_GRID: sweeps the
 * public probe over the canvas and prints the octave-4 band as ASCII, plus
 * the semitone axis at a few landmark notes. Pure C, links libsumi. */
#include "sumi_core.h"
#include <stdio.h>

static const char* NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G",
                                "G#", "A", "A#", "B"};

int main(void) {
    sumi_params_t p = {0};
    p.pitch_layout = SUMI_LAYOUT_PIANO_GRID;
    p.bpm = 120.0f; p.roll_speed = 0.0625f;
    const float aspect = 1.0f;
    sumi_cell_info_t c;

    puts("Probe sweep, full canvas (rows 0..13 of the lattice; each output row");
    puts("samples the vertical center of one lattice row; '.' = dead zone /");
    puts("off canvas). Notes shown as pitch-class letters, b = black key:");
    for (int row = 0; row < 14; row++) {
        const float y = 0.10f + ((row + 0.5f) / 14.0f) * 0.80f;
        printf("row %2d |", row);
        for (int i = 0; i < 70; i++) {
            const float x = 0.08f + ((i + 0.5f) / 70.0f) * 0.84f;
            if (sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &p, aspect, x, y, &c)) {
                const int pc = c.note % 12;
                putchar(NAMES[pc][1] == '#' ? NAMES[pc][0] + 32 : NAMES[pc][0]);
            } else {
                putchar('.');
            }
        }
        printf("|\n");
    }

    puts("\nCell + semitone axis at landmarks (aspect 1.0):");
    const int notes[] = {60, 61, 64, 65, 71, 24, 107};
    for (unsigned i = 0; i < sizeof(notes) / sizeof(notes[0]); i++) {
        const int n = notes[i];
        /* probe at the cell's own center via a position round-trip */
        float cx = 0, cy = 0;
        {
            /* reproduce center by probing a sweep hit; simplest: probe the
             * exact center from the layout via a fine search */
            sumi_cell_info_t best;
            int found = 0;
            for (int iy = 0; iy < 400 && !found; iy++)
                for (int ix = 0; ix < 400 && !found; ix++) {
                    const float x = ix / 399.0f, y = iy / 399.0f;
                    if (sumi_layout_probe(SUMI_LAYOUT_PIANO_GRID, &p, aspect,
                                          x, y, &best) && best.note == n) {
                        c = best; cx = best.cell_center_x; cy = best.cell_center_y;
                        found = 1;
                    }
                }
            if (!found) { printf("note %d NOT FOUND\n", n); continue; }
        }
        printf("  %-3s%d (n=%3d): center=(%.4f, %.4f) R=%.4f  "
               "axis=(%+.3f, %+.3f) step=%.4f\n",
               NAMES[n % 12], n / 12 - 1, n, (double)cx, (double)cy,
               (double)c.cell_radius, (double)c.semitone_dx,
               (double)c.semitone_dy, (double)c.semitone_step);
    }
    return 0;
}
