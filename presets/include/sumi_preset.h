/* sumi_preset.h — Phase 6 step 43 (QOL §3): THE PRESET, and its one serializer.
 *
 * A preset is everything a session is made of that is not the field:
 *   params (sumi_params_t, every field by name), the input dialect, the custom
 *   palette slot (sumi_palette_t), the CC map (channel, cc, target), the
 *   control strip's latch-wheel assignments and the values of the routed
 *   controls, and the layout-state defaults (sumi_layout_state_t).
 * It is written and read as JSON by THIS library and nothing else — host-side,
 * pure C11, no allocation, no dependency but libc — so the desktop, iOS,
 * Android and the web share one code path and cannot drift. The core stays
 * stateless about files: a shell owns storage (its config folder, Files, the
 * browser's localStorage) and hands bytes in and out.
 *
 * THE SCHEMA RULE (presets/SCHEMA.md): a file carries the schema number and
 * the sumi_version that wrote it; a reader IGNORES keys it does not know and
 * KEEPS its defaults for keys that are missing — so a preset made by a newer
 * build loads in an older one with what both understand, and an old preset
 * loads in a new build with the new fields at their defaults. Reading starts
 * from the caller's defaults (sumi_preset_init) and overwrites what the file
 * says; the core then validates on apply (sumi_set_params / sumi_set_palette
 * clamp as they always have), so a hand-edited file cannot wound the engine.
 */
#ifndef SUMI_PRESET_H
#define SUMI_PRESET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sumi_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SUMI_PRESET_SCHEMA        1u
#define SUMI_PRESET_NAME_MAX      64u
#define SUMI_PRESET_MAX_CC        64u
#define SUMI_PRESET_MAX_CONTROLS  32u

typedef struct {
    uint8_t  channel;        /* 0xFF = any channel                                */
    uint8_t  cc;             /* 0..127                                            */
    uint32_t target;         /* sumi_ctl_t                                        */
} sumi_preset_cc_t;

typedef struct {
    uint32_t ctl;            /* sumi_ctl_t — a routed control the shell sends     */
    uint8_t  value;          /* its value, 0..127                                 */
} sumi_preset_control_t;

typedef struct {
    uint32_t schema;         /* the file's schema (SUMI_PRESET_SCHEMA when written) */
    uint32_t sumi_version;   /* the sumi_version() that wrote it: (maj<<16)|(min<<8)|patch */
    char     name[SUMI_PRESET_NAME_MAX];   /* UTF-8, NUL-terminated               */
    uint32_t input_mode;     /* sumi_input_mode_t (1 MPE, 2 classic, 3 wind)        */
    sumi_params_t  params;
    sumi_palette_t palette;  /* the custom slot                                   */
    uint32_t cc_count;
    sumi_preset_cc_t cc[SUMI_PRESET_MAX_CC];
    uint32_t control_count;
    sumi_preset_control_t controls[SUMI_PRESET_MAX_CONTROLS];
    uint8_t  strip_assign_a; /* the control strip's latch wheels' CCs (hostmpe_strip_assign); 0 = unset */
    uint8_t  strip_assign_b;
    sumi_layout_state_t layout_state;   /* the stateful layouts' defaults (Phase 8) */
} sumi_preset_t;

/* The schema this library writes. */
uint32_t sumi_preset_schema(void);

/* Zero the preset and take the given defaults (either may be NULL = zeros):
   the values a read keeps for keys the file lacks. A shell passes the core's
   own defaults (sumi_get_params on a fresh instance, sumi_palette_preset 0). */
void sumi_preset_init(sumi_preset_t* p, const sumi_params_t* params_defaults,
                      const sumi_palette_t* palette_defaults);

/* Write the preset as JSON. `sumi_version` is stamped into the file (pass
   sumi_version()). Returns the length the full text needs, NOT counting the
   terminating NUL — like snprintf: call with cap 0 to size, then with a buffer
   of length + 1. When cap > 0 the output is always NUL-terminated (truncated
   if it did not fit). */
size_t sumi_preset_write(const sumi_preset_t* p, uint32_t sumi_version, char* out, size_t cap);

/* Read JSON into `inout`, which the caller has initialised (sumi_preset_init)
   or filled with the session as it stands: keys the file has overwrite, keys
   it lacks are kept, keys this build does not know are skipped. Returns false
   — and leaves `inout` UNTOUCHED — only when the text is not a JSON object
   (malformed, truncated, or not a preset at all). `len` may be 0 for a NUL-
   terminated string. */
bool sumi_preset_read(const char* json, size_t len, sumi_preset_t* inout);

/* Push a preset into an instance: params, the input dialect, the custom
   palette, the CC map (cleared, then mapped). The controls' values, the strip
   assignments and the layout state are the HOST's to send — through its MIDI
   producer, its strip and its probe snapshot — because they live host-side.
   Links the core (its own translation unit). */
void sumi_preset_apply(sumi_instance_t* inst, const sumi_preset_t* p);

#ifdef __cplusplus
}
#endif
#endif /* SUMI_PRESET_H */
