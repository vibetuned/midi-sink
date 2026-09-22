/* abi_c_compile.c — proves sumi_core.h is pure C: compiled as C11 (no GNU
 * extensions), includes the header, references every exported symbol, and
 * links against the *shared* library so the full day-one ABI contract is
 * verified (roadmap step 1). Also checks sumi_version() == 0.1.0 and the
 * sumi_create(NULL) failure path — no GPU needed. */
#include "sumi_core.h"

#include <stdio.h>
#include <string.h>

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
        (fn_ptr)sumi_add_chladni,  /* v0.11 */
        (fn_ptr)sumi_add_burst,    /* v0.12 */
        (fn_ptr)sumi_add_spark_shear,  /* v0.13 */
        (fn_ptr)sumi_add_spark,        /* v0.13 */
        (fn_ptr)sumi_add_chirikov,     /* v0.14 */
        (fn_ptr)sumi_set_palette,      /* 1.0.0 */
    };
    const size_t sym_count = sizeof(syms) / sizeof(syms[0]);
    for (size_t i = 0; i < sym_count; i++) {
        if (!syms[i]) {
            fprintf(stderr, "FAIL: ABI symbol %zu resolved to NULL\n", i);
            return 1;
        }
    }

    /* Phase 5: the WebGPU host contract compiles as C11 and the enum value is
       stable (a host selects the backend by this number). */
    sumi_webgpu_surface_t wsurf = {0};
    wsurf.device = 0;
    wsurf.canvas_selector = "#sumi";
    wsurf.color_format = SUMI_WEBGPU_FORMAT_BGRA8;
    if (SUMI_BACKEND_WEBGPU != 4 || wsurf.color_format != 0) {
        fprintf(stderr, "FAIL: WebGPU ABI additions\n");
        return 1;
    }
    const uint32_t v = sumi_version();
    const uint32_t expected = (1u << 16) | (1u << 8) | 0u; /* 1.1.0 (Phase 6 step 42: anod_glow, anod_pitch, burst_order_by_class, mode values 2/3 + SUMI_MODE_MEDIUM_DEFAULT; step 43: chladni_mode - additive) */
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
    if (params.vortex_profile != 1u || SUMI_VORTEX_EXPONENTIAL != 0 || SUMI_VORTEX_TORSION != 3 ||
        SUMI_CTL_RIPPLE_AMP != 7 || SUMI_CTL_RIPPLE_FREQ != 8 ||
        /* v0.9 (#69): additive growth only — old values above unchanged. */
        SUMI_CTL_SWIRL_STRENGTH != 9 || SUMI_CTL_SWIRL_Y != 11 ||
        SUMI_CTL_PINCH_SADDLE != 12 || SUMI_CTL_PINCH_CROSS != 13 ||
        SUMI_CTL_TORSION_K != 14 || SUMI_CTL_TORSION_PHASE != 15 ||
        SUMI_CTL_CHLADNI_A != 16 || SUMI_CTL_CHLADNI_B != 17 ||
        SUMI_CTL_SPARK_K != 18 || SUMI_CTL_CHIRIKOV_K != 19 || SUMI_CTL_COUNT != 20) {
        fprintf(stderr, "FAIL: v0.4 params/enum values broken\n");
        return 1;
    }

    /* v0.3: the instance-free layout probe must be callable from plain C with
       no instance at all (that is its whole point — see PROJECT_SPEC.md §8.2). */
    {
        sumi_cell_info_t cell;
        params.pitch_layout = SUMI_LAYOUT_CHROMA_GRID;
        if (!sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, 16.0f / 9.0f, NULL,
                               0.5f, 0.5f, &cell)) {
            fprintf(stderr, "FAIL: probe rejected the grid center\n");
            return 1;
        }
        if (cell.note < 24u || cell.note > 107u || cell.cell_radius <= 0.0f ||
            cell.semitone_step <= 0.0f) {
            fprintf(stderr, "FAIL: probe cell info out of range\n");
            return 1;
        }
        if (sumi_layout_probe(SUMI_LAYOUT_FIFTHS, &params, 1.0f, NULL, 0.5f, 0.5f, &cell)) {
            fprintf(stderr, "FAIL: probe must refuse FIFTHS\n");
            return 1;
        }
        /* 1.0.0: an explicit zero state answers as NULL does; the cell's flags read 0;
           the reserved layouts are refused; the new enum values and PODs are pure C. */
        {
            sumi_layout_state_t zero = {0u, 0.0f, {0u, 0u}};
            sumi_cell_info_t cell2;
            if (!sumi_layout_probe(SUMI_LAYOUT_CHROMA_GRID, &params, 16.0f / 9.0f, &zero, 0.5f, 0.5f, &cell2) ||
                cell2.note != cell.note || cell2.flags != 0u) {
                fprintf(stderr, "FAIL: 1.0.0 probe state / flags\n");
                return 1;
            }
            if (sumi_layout_probe(SUMI_LAYOUT_TRUMPET, &params, 1.0f, NULL, 0.5f, 0.5f, &cell2) ||
                sumi_layout_probe(SUMI_LAYOUT_THEREMIN, &params, 1.0f, NULL, 0.5f, 0.5f, &cell2)) {
                fprintf(stderr, "FAIL: reserved layouts must be refused by the probe\n");
                return 1;
            }
            sumi_palette_t pal;
            memset(&pal, 0, sizeof pal);
            pal.stop_count = 2;
            params.medium = SUMI_MEDIUM_ANOD;
            params.active_palette_id = SUMI_PALETTE_CUSTOM;
            if (SUMI_MEDIUM_SUMI != 0 || SUMI_MEDIUM_ANOD != 1 || SUMI_PALETTE_CUSTOM != 3u || SUMI_CELL_CONTINUOUS != 1u ||
                SUMI_LAYOUT_TRUMPET != 8 || SUMI_LAYOUT_TROMBONE != 9 || SUMI_LAYOUT_WICKI != 10 ||
                SUMI_LAYOUT_FRETS != 11 || SUMI_LAYOUT_THEREMIN != 12 || SUMI_PALETTE_MAX_STOPS != 8 ||
                sizeof(sumi_layout_state_t) != 16u || sizeof(sumi_palette_stop_t) != 16u || pal.stop_count != 2u) {
                fprintf(stderr, "FAIL: 1.0.0 enum values / POD sizes\n");
                return 1;
            }
            params.medium = SUMI_MEDIUM_SUMI;
            params.active_palette_id = 0;
            /* 1.1.0: additive — the medium default sentinel and the new fields are pure C */
            params.bend_mode = SUMI_MODE_MEDIUM_DEFAULT;
            params.anod_glow = 1.0f;
            params.anod_pitch = 1.0f / 144.0f;
            params.burst_order_by_class[11] = 4u;
            if (SUMI_MODE_MEDIUM_DEFAULT != 255u || params.bend_mode != 255u || params.burst_order_by_class[11] != 4u) {
                fprintf(stderr, "FAIL: 1.1.0 additive fields\n");
                return 1;
            }
            /* step 43: the palette model's accent (from the reserved words: the POD's size is unchanged), the library */
            pal.accent_rgb[0] = 0.5f;
            {
                sumi_palette_t q; const char* nm = NULL;
                if (sizeof(sumi_palette_t) != 172u || sumi_palette_preset_count(0) < 3u || sumi_palette_preset_count(1) < 3u ||
                    !sumi_palette_preset(0, 0, &q, &nm) || nm == NULL || q.stop_count != 2u || sumi_palette_preset(0, 1000, &q, &nm) || nm != NULL) {
                    fprintf(stderr, "FAIL: 1.1.0 palette library\n");
                    return 1;
                }
            }
            /* step 43 (QOL §4): the export ABI is pure C and exported (a NULL instance is refused) */
            if (SUMI_EXPORT_MAX_DIM != 8192u || SUMI_EXPORT_ANOD_ALPHA != 1u || sumi_export_begin(NULL, NULL, 0, 0, 512, 512, 0) ||
                sumi_export_poll(NULL, NULL, 0, NULL, NULL) != 0 || sumi_read_field(NULL, NULL, 0, NULL, NULL)) {
                fprintf(stderr, "FAIL: 1.1.0 export ABI\n");
                return 1;
            }
            /* step 43 (QOL §2): the substrate fields are additive pure-C floats */
            params.paper_tint[0] = 0.9f; params.fiber_scale = 1.0f; params.anod_dark = 0.5f; params.anod_grain = 0.5f;
            params.anod_bloom = 0.0f; params.anod_bloom_levels = 4u;
            params.bend_mode = 0;
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
