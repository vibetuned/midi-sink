/* abi_c_compile.c — proves sumi_core.h is pure C: compiled as C11 (no GNU
 * extensions), includes the header, references every exported symbol, and
 * links against the *shared* library so the full day-one ABI contract is
 * verified (roadmap step 1). Also checks sumi_version() == 0.1.0 and the
 * sumi_create(NULL) failure path — no GPU needed. */
#include "sumi_core.h"

#include <stdio.h>

typedef void (*fn_ptr)(void);

int main(void) {
    /* Taking every function's address forces the dynamic linker to resolve
     * the whole exported contract. */
    const fn_ptr syms[] = {
        (fn_ptr)sumi_version,
        (fn_ptr)sumi_dropped_midi_count,
        (fn_ptr)sumi_create,
        (fn_ptr)sumi_destroy,
        (fn_ptr)sumi_resize,
        (fn_ptr)sumi_update,
        (fn_ptr)sumi_render,
        (fn_ptr)sumi_push_midi,
        (fn_ptr)sumi_set_params,
        (fn_ptr)sumi_get_params,
        (fn_ptr)sumi_set_input_mode,
        (fn_ptr)sumi_map_cc,
        (fn_ptr)sumi_clear_cc_map,
        (fn_ptr)sumi_trigger_paper_dip,
        (fn_ptr)sumi_read_print,
        (fn_ptr)sumi_add_drop,
        (fn_ptr)sumi_add_tine,
        (fn_ptr)sumi_add_vortex,
        (fn_ptr)sumi_add_wake,     /* v0.4 */
        (fn_ptr)sumi_add_pinch,    /* v0.4 */
    };
    const size_t sym_count = sizeof(syms) / sizeof(syms[0]);
    for (size_t i = 0; i < sym_count; i++) {
        if (!syms[i]) {
            fprintf(stderr, "FAIL: ABI symbol %zu resolved to NULL\n", i);
            return 1;
        }
    }

    const uint32_t v = sumi_version();
    const uint32_t expected = (0u << 16) | (4u << 8) | 0u; /* 0.4.0 (operator batch) */
    if (v != expected) {
        fprintf(stderr, "FAIL: sumi_version() = 0x%08x, expected 0x%08x\n", v, expected);
        return 1;
    }

    /* params v0.2: the grown struct and the layout enum must be pure C. */
    sumi_params_t params;
    params.bpm = 120.0f;
    params.roll_speed = 0.0625f;
    params.pitch_layout = SUMI_LAYOUT_JANKO;
    if (params.pitch_layout != 2u || params.bpm != 120.0f || params.roll_speed != 0.0625f) {
        fprintf(stderr, "FAIL: sumi_params_t v0.2 fields broken\n");
        return 1;
    }
    params.pitch_layout = SUMI_LAYOUT_PIANO_GRID;
    if (params.pitch_layout != 5u) {
        fprintf(stderr, "FAIL: SUMI_LAYOUT_PIANO_GRID must be 5\n");
        return 1;
    }
    /* params v0.4: the grown struct and the new enums must be pure C. */
    params.slide_mode = 1;
    params.vortex_profile = SUMI_VORTEX_RANKINE;
    params.ripple_bake = 1;
    params.ripple_angle = 0.5f;
    params.pinch_variant = 1;
    params.bend_mode = 1;
    params.press_mode = 1;
    if (params.vortex_profile != 1u || SUMI_VORTEX_EXPONENTIAL != 0 ||
        SUMI_CTL_RIPPLE_AMP != 7 || SUMI_CTL_RIPPLE_FREQ != 8 ||
        SUMI_CTL_COUNT != 9) {
        fprintf(stderr, "FAIL: v0.4 params/enum values broken\n");
        return 1;
    }

    /* v0.3: the instance-free layout probe must be callable from plain C with
       no instance at all (that is its whole point — see PHASE4_SPEC.md §2). */
    {
        sumi_cell_info_t cell;
        params.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
        if (!sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, 16.0f / 9.0f,
                               0.5f, 0.5f, &cell)) {
            fprintf(stderr, "FAIL: probe rejected the grid center\n");
            return 1;
        }
        if (cell.note < 24u || cell.note > 107u || cell.cell_radius <= 0.0f ||
            cell.semitone_step <= 0.0f) {
            fprintf(stderr, "FAIL: probe cell info out of range\n");
            return 1;
        }
        if (sumi_layout_probe(SUMI_LAYOUT_FIFTHS, &params, 1.0f, 0.5f, 0.5f, &cell)) {
            fprintf(stderr, "FAIL: probe must refuse FIFTHS\n");
            return 1;
        }
    }

    if (sumi_create(NULL) != NULL) {
        fprintf(stderr, "FAIL: sumi_create(NULL) must return NULL\n");
        return 1;
    }

    printf("OK: C11 compile+link, %zu ABI symbols resolved, version %u.%u.%u\n",
           sym_count, v >> 16, (v >> 8) & 0xFFu, v & 0xFFu);
    return 0;
}
