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
    };
    const size_t sym_count = sizeof(syms) / sizeof(syms[0]);
    for (size_t i = 0; i < sym_count; i++) {
        if (!syms[i]) {
            fprintf(stderr, "FAIL: ABI symbol %zu resolved to NULL\n", i);
            return 1;
        }
    }

    const uint32_t v = sumi_version();
    const uint32_t expected = (0u << 16) | (1u << 8) | 0u; /* 0.1.0 */
    if (v != expected) {
        fprintf(stderr, "FAIL: sumi_version() = 0x%08x, expected 0x%08x\n", v, expected);
        return 1;
    }

    if (sumi_create(NULL) != NULL) {
        fprintf(stderr, "FAIL: sumi_create(NULL) must return NULL\n");
        return 1;
    }

    printf("OK: C11 compile+link, %zu ABI symbols resolved, version %u.%u.%u\n",
           sym_count, v >> 16, (v >> 8) & 0xFFu, v & 0xFFu);
    return 0;
}
