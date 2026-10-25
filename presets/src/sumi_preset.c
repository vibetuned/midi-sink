/* sumi_preset.c — the one serializer (sumi_preset.h). Pure C11, no allocation:
 * a streaming JSON writer into the caller's buffer and a recursive-descent
 * reader that walks the text once, dispatching on the keys it knows and
 * skipping the rest. presets/SCHEMA.md is the document; the field table
 * below is the truth it is written from. */
#include "sumi_preset.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- the field table */
typedef enum { F32 = 0, U32 = 1 } kind_t;
typedef struct { const char* name; size_t offset; kind_t kind; uint32_t count; } field_t;
#define PF(nm, kind, cnt) { #nm, offsetof(sumi_params_t, nm), kind, cnt }
static const field_t PARAMS[] = {
    PF(fluid_viscosity, F32, 1), PF(expansion_rate, F32, 1), PF(paper_roughness, F32, 1), PF(smoothing_ms, F32, 1),
    PF(active_palette_id, U32, 1), PF(pitch_layout, U32, 1), PF(sim_scale, F32, 1), PF(bpm, F32, 1), PF(roll_speed, F32, 1),
    PF(slide_mode, U32, 1), PF(vortex_profile, U32, 1), PF(ripple_bake, U32, 1), PF(ripple_angle, F32, 1), PF(pinch_variant, U32, 1),
    PF(bend_mode, U32, 1), PF(press_mode, U32, 1), PF(wake_profile, U32, 1), PF(wake_spread, F32, 1), PF(torsion_sweep, U32, 1),
    PF(chladni_cell, F32, 1), PF(burst_age, F32, 1), PF(burst_life, F32, 1), PF(burst_order, U32, 1),
    PF(spark_stack, U32, 1), PF(spark_profile, U32, 1), PF(spark_shear, F32, 1), PF(spark_tau, F32, 1),
    PF(chirikov_kmax, F32, 1), PF(chirikov_periods, U32, 1), PF(chirikov_eps, F32, 1),
    PF(medium, U32, 1), PF(anod_glow, F32, 1), PF(burst_order_by_class, U32, 12), PF(anod_pitch, F32, 1), PF(chladni_mode, U32, 1),
    PF(paper_tint, F32, 3), PF(fiber_scale, F32, 1), PF(anod_dark, F32, 1), PF(anod_grain, F32, 1),
    PF(anod_bloom, F32, 1), PF(anod_bloom_levels, U32, 1), PF(anod_drop, F32, 1),   /* the glow (#69) and the strike's charge (#71) */
    PF(trumpet_arc, U32, 1), PF(string_tuning, U32, 1),   /* Phase 9 steps 60–61: the trumpet's arrangement, the strings' tuning preset */
    PF(polarizer_mode, U32, 1), PF(braid_mode, U32, 1), PF(kessho_retard, F32, 1), PF(kessho_backlight, F32, 1),   /* 1.6.0 (Phase 11 step 71): Kessho */
    PF(kessho_grain, F32, 1), PF(kessho_tint, F32, 1), PF(kessho_caustic, F32, 1),
    PF(drop_habit, U32, 1), PF(habit_roundness, F32, 1), PF(habit_blades, U32, 1), PF(tine_profile, U32, 1),   /* 1.7.0 (step 71b): the habit and the cleavage profile */
    PF(anod_glow_mode, U32, 1), PF(spark_afterglow, F32, 1),   /* 1.11.0 (step 76): the spark reading (spark_branch retired before ship, DECISIONS_11 #2) */
    PF(split_mode, U32, 1), PF(split_layout, U32, 1), PF(split_note, U32, 1),   /* 1.12.0 (step 77): the split surface */
};
#define N_PARAMS (sizeof(PARAMS) / sizeof(PARAMS[0]))

uint32_t sumi_preset_schema(void) { return SUMI_PRESET_SCHEMA; }

void sumi_preset_init(sumi_preset_t* p, const sumi_params_t* params_defaults, const sumi_palette_t* palette_defaults) {
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->schema = SUMI_PRESET_SCHEMA;
    p->input_mode = 1u;
    if (params_defaults) p->params = *params_defaults;
    if (palette_defaults) p->palette = *palette_defaults;
}

/* ---------------------------------------------------------------- the writer */
typedef struct { char* out; size_t cap, len; } w_t;
static void w_raw(w_t* w, const char* s) {
    const size_t n = strlen(s);
    if (w->out && w->cap > 0) {
        size_t room = w->len < w->cap - 1 ? w->cap - 1 - w->len : 0;
        const size_t k = n < room ? n : room;
        if (k) memcpy(w->out + w->len, s, k);
    }
    w->len += n;
}
static void w_f(w_t* w, float v) {
    char b[48];
    if (v != v) v = 0.0f;                                          /* a NaN is written as 0: JSON has no NaN */
    if (v > 3.4e38f) v = 3.4e38f; if (v < -3.4e38f) v = -3.4e38f;
    snprintf(b, sizeof b, "%.9g", (double)v);                     /* %.9g round-trips a float exactly */
    w_raw(w, b);
}
static void w_u(w_t* w, uint32_t v) { char b[16]; snprintf(b, sizeof b, "%u", v); w_raw(w, b); }
static void w_farr(w_t* w, const float* a, int n) { for (int i = 0; i < n; i++) { if (i) w_raw(w, ", "); w_f(w, a[i]); } }   /* step 82 */
static void w_str(w_t* w, const char* s) {
    w_raw(w, "\"");
    for (const unsigned char* c = (const unsigned char*)s; *c; c++) {
        char b[8];
        if (*c == '"') w_raw(w, "\\\"");
        else if (*c == '\\') w_raw(w, "\\\\");
        else if (*c < 0x20) { snprintf(b, sizeof b, "\\u%04x", (unsigned)*c); w_raw(w, b); }
        else { b[0] = (char)*c; b[1] = 0; w_raw(w, b); }
    }
    w_raw(w, "\"");
}
static void w_rgb(w_t* w, const float* rgb, int n) {
    w_raw(w, "[");
    for (int i = 0; i < n; i++) { if (i) w_raw(w, ", "); w_f(w, rgb[i]); }
    w_raw(w, "]");
}

size_t sumi_preset_write(const sumi_preset_t* p, uint32_t sumi_version, char* out, size_t cap) {
    w_t w = { out, cap, 0 };
    if (out && cap > 0) out[0] = 0;
    if (!p) return 0;
    w_raw(&w, "{\n  \"midi_sink_preset\": "); w_u(&w, SUMI_PRESET_SCHEMA);
    w_raw(&w, ",\n  \"sumi_version\": ["); w_u(&w, (sumi_version >> 16) & 0xFFu); w_raw(&w, ", "); w_u(&w, (sumi_version >> 8) & 0xFFu); w_raw(&w, ", "); w_u(&w, sumi_version & 0xFFu); w_raw(&w, "]");
    w_raw(&w, ",\n  \"name\": "); w_str(&w, p->name);
    w_raw(&w, ",\n  \"input_mode\": "); w_u(&w, p->input_mode);
    w_raw(&w, ",\n  \"params\": {");
    for (size_t i = 0; i < N_PARAMS; i++) {
        const field_t* f = &PARAMS[i];
        const char* base = (const char*)&p->params + f->offset;
        w_raw(&w, i ? ",\n    \"" : "\n    \""); w_raw(&w, f->name); w_raw(&w, "\": ");
        if (f->count > 1) w_raw(&w, "[");
        for (uint32_t k = 0; k < f->count; k++) {
            if (k) w_raw(&w, ", ");
            if (f->kind == F32) { float v; memcpy(&v, base + 4 * k, 4); w_f(&w, v); }
            else { uint32_t v; memcpy(&v, base + 4 * k, 4); w_u(&w, v); }
        }
        if (f->count > 1) w_raw(&w, "]");
    }
    w_raw(&w, "\n  },\n  \"palette\": {\n    \"stops\": [");
    {
        const uint32_t n = p->palette.stop_count < 2u ? 2u : (p->palette.stop_count > SUMI_PALETTE_MAX_STOPS ? SUMI_PALETTE_MAX_STOPS : p->palette.stop_count);
        for (uint32_t i = 0; i < n; i++) {
            const float s[4] = { p->palette.stops[i].rgb[0], p->palette.stops[i].rgb[1], p->palette.stops[i].rgb[2], p->palette.stops[i].position };
            if (i) w_raw(&w, ", ");
            w_rgb(&w, s, 4);
        }
    }
    w_raw(&w, "],\n    \"depth_gamma\": "); w_f(&w, p->palette.depth_gamma);
    w_raw(&w, ",\n    \"depth_floor\": "); w_f(&w, p->palette.depth_floor);
    w_raw(&w, ",\n    \"hue_drift\": "); w_f(&w, p->palette.hue_drift);
    w_raw(&w, ",\n    \"accent_rgb\": "); w_rgb(&w, p->palette.accent_rgb, 3);
    w_raw(&w, ",\n    \"clear_rgb\": "); w_rgb(&w, p->palette.clear_rgb, 3);
    w_raw(&w, "\n  },\n  \"cc_map\": [");
    {
        const uint32_t n = p->cc_count > SUMI_PRESET_MAX_CC ? SUMI_PRESET_MAX_CC : p->cc_count;
        for (uint32_t i = 0; i < n; i++) {
            w_raw(&w, i ? ", [" : "["); w_u(&w, p->cc[i].channel); w_raw(&w, ", "); w_u(&w, p->cc[i].cc); w_raw(&w, ", "); w_u(&w, p->cc[i].target); w_raw(&w, "]");
        }
    }
    w_raw(&w, "],\n  \"controls\": [");
    {
        const uint32_t n = p->control_count > SUMI_PRESET_MAX_CONTROLS ? SUMI_PRESET_MAX_CONTROLS : p->control_count;
        for (uint32_t i = 0; i < n; i++) {
            w_raw(&w, i ? ", [" : "["); w_u(&w, p->controls[i].ctl); w_raw(&w, ", "); w_u(&w, p->controls[i].value); w_raw(&w, "]");
        }
    }
    w_raw(&w, "],\n  \"strip\": {\"assign_a\": "); w_u(&w, p->strip_assign_a); w_raw(&w, ", \"assign_b\": "); w_u(&w, p->strip_assign_b); w_raw(&w, "}");
    w_raw(&w, ",\n  \"layout_state\": {\"buttons\": "); w_u(&w, p->layout_state.buttons); w_raw(&w, ", \"slider\": "); w_f(&w, p->layout_state.slider); w_raw(&w, "}");
    if (p->suzu_present) {   /* step 57: Suzu's patch, written only when the shell carries one */
        const struct { const char* k; const float* f; const uint32_t* u; } sf[] = {
            { "source", NULL, &p->suzu.source }, { "level", &p->suzu.level, NULL }, { "attack_s", &p->suzu.attack_s, NULL },
            { "release_s", &p->suzu.release_s, NULL }, { "cutoff_hz", &p->suzu.cutoff_hz, NULL }, { "resonance", &p->suzu.resonance, NULL },
            { "shear", &p->suzu.shear, NULL }, { "shear_kind", NULL, &p->suzu.shear_kind }, { "voice_kind", NULL, &p->suzu.voice_kind },
            { "modal_preset", NULL, &p->suzu.modal_preset }, { "modes", NULL, &p->suzu.modes }, { "coupling", &p->suzu.coupling, NULL },
            { "decay_s", &p->suzu.decay_s, NULL }, { "decay_bright", &p->suzu.decay_bright, NULL }, { "stiffness", &p->suzu.stiffness, NULL },
            { "pluck", &p->suzu.pluck, NULL }, { "bow_onset_s", &p->suzu.bow_onset_s, NULL }, { "bow_position", &p->suzu.bow_position, NULL },
            { "breath_cc", NULL, &p->suzu.breath_cc },
            { "string_nodes", NULL, &p->suzu.string_nodes }, { "string_decay_s", &p->suzu.string_decay_s, NULL }, { "pickup", &p->suzu.pickup, NULL },
            { "oversample", NULL, &p->suzu.oversample },   /* step 78 */
            { "bridge_hz", &p->suzu.bridge_hz, NULL }, { "bridge_cells", NULL, &p->suzu.bridge_cells }, { "bridge_coupling", &p->suzu.bridge_coupling, NULL },
            { "bridge_decay_s", &p->suzu.bridge_decay_s, NULL }, { "loop_loss", &p->suzu.loop_loss, NULL }, { "duffing_beta", &p->suzu.duffing_beta, NULL },
            { "drive", &p->suzu.drive, NULL }, { "drive_ratio", &p->suzu.drive_ratio, NULL }, { "rotor_k", &p->suzu.rotor_k, NULL },
            { "mod_target", NULL, &p->suzu.mod_target }, { "mod_depth", &p->suzu.mod_depth, NULL }, { "mod_rate", &p->suzu.mod_rate, NULL },
            { "bore_nodes", NULL, &p->suzu.bore_nodes }, { "bore_loss", &p->suzu.bore_loss, NULL }, { "bore_corner_hz", &p->suzu.bore_corner_hz, NULL },
            { "jet_gain", &p->suzu.jet_gain, NULL }, { "jet_drive", &p->suzu.jet_drive, NULL }, { "jet_tau", &p->suzu.jet_tau, NULL }, { "jet_q", &p->suzu.jet_q, NULL },
            { "jet_noise", &p->suzu.jet_noise, NULL }, { "jet_area", &p->suzu.jet_area, NULL }, { "jet_offset", &p->suzu.jet_offset, NULL }, { "breath_ref", &p->suzu.breath_ref, NULL }, { "breath_range", &p->suzu.breath_range, NULL }, { "bore_wall_s", &p->suzu.bore_wall_s, NULL }, { "press_blows", NULL, &p->suzu.press_blows },
            { "reed_hz", &p->suzu.reed_hz, NULL }, { "reed_q", &p->suzu.reed_q, NULL }, { "reed_open", &p->suzu.reed_open, NULL }, { "reed_close", &p->suzu.reed_close, NULL },
            { "reed_area", &p->suzu.reed_area, NULL }, { "reed_noise", &p->suzu.reed_noise, NULL }, { "cone_apex", &p->suzu.cone_apex, NULL },
            { "lip_ratio", &p->suzu.lip_ratio, NULL }, { "lip_q", &p->suzu.lip_q, NULL }, { "lip_open", &p->suzu.lip_open, NULL }, { "lip_close", &p->suzu.lip_close, NULL },
            { "lip_area", &p->suzu.lip_area, NULL }, { "lip_range", &p->suzu.lip_range, NULL }, { "partial", NULL, &p->suzu.partial },
            { "bell_start", &p->suzu.bell_start, NULL }, { "bell_gamma", &p->suzu.bell_gamma, NULL }, { "brass", &p->suzu.brass, NULL },
            { "trace_scope", NULL, &p->suzu.trace_scope }, { "trace_ink", NULL, &p->suzu.trace_ink }, { "trace_kinds", NULL, &p->suzu.trace_kinds },   /* step 59 */
            { "trace_segments", NULL, &p->suzu.trace_segments }, { "trace_stroke", NULL, &p->suzu.trace_stroke }, { "trace_scale", &p->suzu.trace_scale, NULL }, { "trace_canvas", NULL, &p->suzu.trace_canvas },
            { "trace_mode", NULL, &p->suzu.trace_mode },   /* step 75 */
            { "drum_size", NULL, &p->suzu.drum_size }, { "drum_kappa", &p->suzu.drum_kappa, NULL }, { "drum_alpha", &p->suzu.drum_alpha, NULL }, { "drum_cap", &p->suzu.drum_cap, NULL },   /* step 79 */
            { "plate_on", NULL, &p->suzu.plate_on }, { "plate_size", NULL, &p->suzu.plate_size }, { "plate_hz", &p->suzu.plate_hz, NULL }, { "plate_kappa", &p->suzu.plate_kappa, NULL },
            { "plate_decay_s", &p->suzu.plate_decay_s, NULL }, { "plate_bright", &p->suzu.plate_bright, NULL }, { "plate_gain", &p->suzu.plate_gain, NULL },
            { "bow_mode", NULL, &p->suzu.bow_mode }, { "bow_force", &p->suzu.bow_force, NULL }, { "bow_speed", &p->suzu.bow_speed, NULL },   /* step 80 */
            { "reed_inertance", &p->suzu.reed_inertance, NULL }, { "bore_leak", &p->suzu.bore_leak, NULL }, { "brass_shock", &p->suzu.brass_shock, NULL },   /* step 81 */
            { "lip_shear", &p->suzu.lip_shear, NULL }, { "lip_shear_ratio", &p->suzu.lip_shear_ratio, NULL }, { "tonehole_mode", NULL, &p->suzu.tonehole_mode },
            { "hole_size", &p->suzu.hole_size, NULL }, { "hole_loss", &p->suzu.hole_loss, NULL },
            { "toda_nodes", NULL, &p->suzu.toda_nodes }, { "toda_b", &p->suzu.toda_b, NULL }, { "toda_cap", &p->suzu.toda_cap, NULL }, { "toda_width", &p->suzu.toda_width, NULL },   /* step 83 */
            { "boris_b", &p->suzu.boris_b, NULL }, { "boris_b_rest", &p->suzu.boris_b_rest, NULL },   /* step 84 */
            { "dirac_masses", NULL, &p->suzu.dirac_masses }, { "dirac_bridge_hz", &p->suzu.dirac_bridge_hz, NULL }, { "dirac_bridge_mass", &p->suzu.dirac_bridge_mass, NULL }, { "dirac_stop", &p->suzu.dirac_stop, NULL }, { "dirac_stop_loss", &p->suzu.dirac_stop_loss, NULL }, { "dirac_bridge_decay", &p->suzu.dirac_bridge_decay, NULL },   /* step 85 */
            { "bill_drive", &p->suzu.bill_drive, NULL }, { "bill_aspect", &p->suzu.bill_aspect, NULL }, { "bill_length", &p->suzu.bill_length, NULL }, { "bill_length_rest", &p->suzu.bill_length_rest, NULL },
        };
        w_raw(&w, ",\n  \"suzu\": {");
        for (size_t i = 0; i < sizeof sf / sizeof sf[0]; i++) {
            w_raw(&w, i ? ", \"" : "\""); w_raw(&w, sf[i].k); w_raw(&w, "\": ");
            if (sf[i].f) w_f(&w, *sf[i].f); else w_u(&w, *sf[i].u);
        }
        /* step 82 (schema 2): the table's arrays and the compensation flag */
        w_raw(&w, ", \"modal_comp\": "); w_u(&w, p->suzu.modal_comp);
        w_raw(&w, ",\n    \"modal_ratio\": ["); w_farr(&w, p->suzu.modal_ratio, SUMI_PRESET_MODAL_MAX); w_raw(&w, "]");
        w_raw(&w, ",\n    \"modal_t60\": ["); w_farr(&w, p->suzu.modal_t60, SUMI_PRESET_MODAL_MAX); w_raw(&w, "]");
        w_raw(&w, ",\n    \"modal_weight\": ["); w_farr(&w, p->suzu.modal_weight, SUMI_PRESET_MODAL_MAX); w_raw(&w, "]");
        {   /* step 86: the mod matrix's slots, as three arrays of four */
            float t[4]; int i;
            for (i = 0; i < 4; i++) t[i] = (float)p->suzu.mod_src[i]; w_raw(&w, ",\n    \"mod_src\": ["); w_farr(&w, t, 4); w_raw(&w, "]");
            for (i = 0; i < 4; i++) t[i] = (float)p->suzu.mod_dst[i]; w_raw(&w, ", \"mod_dst\": ["); w_farr(&w, t, 4); w_raw(&w, "]");
            w_raw(&w, ", \"mod_amt\": ["); w_farr(&w, p->suzu.mod_amt, 4); w_raw(&w, "]");
        }
        w_raw(&w, "}");
    }
    if (p->mineral_present) {   /* 1.7.0 (step 71b): the mineral, every field by its name; `id` the library's, SUMI_MINERAL_COUNT = edited */
        const sumi_mineral_t* m = &p->mineral;
        const struct { const char* k; const float* f; const uint32_t* u; } mf[] = {
            { "id", NULL, &p->mineral_id }, { "optic_class", NULL, &m->optic_class }, { "retardation", &m->retardation, NULL },
            { "retardation_bias", &m->retardation_bias, NULL }, { "two_v", &m->two_v, NULL }, { "walkoff", &m->walkoff, NULL },
            { "dispersion", &m->dispersion, NULL }, { "film_n", &m->film_n, NULL }, { "film_r", &m->film_r, NULL },
            { "conical_ring", &m->conical_ring, NULL }, { "bloom_gain", &m->bloom_gain, NULL }, { "lut_id", NULL, &m->lut_id },
        };
        w_raw(&w, ",\n  \"mineral\": {");
        for (size_t i = 0; i < sizeof mf / sizeof mf[0]; i++) {
            w_raw(&w, i ? ", \"" : "\""); w_raw(&w, mf[i].k); w_raw(&w, "\": ");
            if (mf[i].f) w_f(&w, *mf[i].f); else w_u(&w, *mf[i].u);
        }
        w_raw(&w, ", \"body_tint\": ["); w_f(&w, m->body_tint[0]); w_raw(&w, ", "); w_f(&w, m->body_tint[1]); w_raw(&w, ", "); w_f(&w, m->body_tint[2]); w_raw(&w, "]}");
    }
    w_raw(&w, "\n}\n");
    if (out && cap > 0) out[w.len < cap - 1 ? w.len : cap - 1] = 0;   /* always NUL-terminated, truncated or not */
    return w.len;
}

/* ---------------------------------------------------------------- the reader */
typedef struct { const char* s; const char* end; bool ok; } r_t;
static void r_ws(r_t* r) { while (r->s < r->end && (*r->s == ' ' || *r->s == '\t' || *r->s == '\n' || *r->s == '\r')) r->s++; }
static bool r_peek(r_t* r, char c) { r_ws(r); return r->s < r->end && *r->s == c; }
static bool r_eat(r_t* r, char c) { if (r_peek(r, c)) { r->s++; return true; } r->ok = false; return false; }
static bool r_string(r_t* r, char* out, size_t cap) {   /* out may be NULL (skip); decodes the escapes it knows; keeps UTF-8 bytes */
    if (!r_eat(r, '"')) return false;
    size_t n = 0;
    while (r->s < r->end && *r->s != '"') {
        unsigned char c = (unsigned char)*r->s++;
        if (c == '\\') {
            if (r->s >= r->end) { r->ok = false; return false; }
            const char e = *r->s++;
            if (e == 'u') {
                if (r->end - r->s < 4) { r->ok = false; return false; }
                char hex[5] = { r->s[0], r->s[1], r->s[2], r->s[3], 0 }; r->s += 4;
                const long cp = strtol(hex, NULL, 16);
                c = (cp >= 0 && cp < 0x80) ? (unsigned char)cp : (unsigned char)'?';   /* ASCII (controls too); beyond it, a marker */
            } else if (e == 'n') c = '\n'; else if (e == 't') c = '\t'; else if (e == 'r') c = '\r';
            else if (e == 'b') c = '\b'; else if (e == 'f') c = '\f';
            else c = (unsigned char)e;                      /* \" \\ \/ */
        }
        if (out && n + 1 < cap) out[n++] = (char)c;
    }
    if (out && cap) out[n < cap ? n : cap - 1] = 0;
    if (!r_eat(r, '"')) return false;
    return true;
}
static bool r_number(r_t* r, double* v) {
    r_ws(r);
    char* endp = NULL;
    const double d = strtod(r->s, &endp);
    if (!endp || endp == r->s || endp > r->end) { r->ok = false; return false; }
    r->s = endp; *v = d;
    return true;
}
static bool r_skip(r_t* r);
static bool r_skip_object(r_t* r) {
    if (!r_eat(r, '{')) return false;
    if (r_peek(r, '}')) { r->s++; return true; }
    for (;;) {
        if (!r_string(r, NULL, 0) || !r_eat(r, ':') || !r_skip(r)) return false;
        if (r_peek(r, ',')) { r->s++; continue; }
        return r_eat(r, '}');
    }
}
static bool r_skip_array(r_t* r) {
    if (!r_eat(r, '[')) return false;
    if (r_peek(r, ']')) { r->s++; return true; }
    for (;;) {
        if (!r_skip(r)) return false;
        if (r_peek(r, ',')) { r->s++; continue; }
        return r_eat(r, ']');
    }
}
static bool r_skip(r_t* r) {   /* any value */
    r_ws(r);
    if (r->s >= r->end) { r->ok = false; return false; }
    const char c = *r->s;
    if (c == '{') return r_skip_object(r);
    if (c == '[') return r_skip_array(r);
    if (c == '"') return r_string(r, NULL, 0);
    if (c == 't' && r->end - r->s >= 4 && !strncmp(r->s, "true", 4)) { r->s += 4; return true; }
    if (c == 'f' && r->end - r->s >= 5 && !strncmp(r->s, "false", 5)) { r->s += 5; return true; }
    if (c == 'n' && r->end - r->s >= 4 && !strncmp(r->s, "null", 4)) { r->s += 4; return true; }
    double d; return r_number(r, &d);
}
/* an array of numbers into floats/u32s: up to max, extras skipped, missing kept */
static bool r_num_array(r_t* r, float* fout, uint32_t* uout, uint32_t max) {
    if (!r_eat(r, '[')) return false;
    if (r_peek(r, ']')) { r->s++; return true; }
    uint32_t i = 0;
    for (;;) {
        double d;
        if (!r_number(r, &d)) return false;
        if (i < max) { if (fout) fout[i] = (float)d; if (uout) uout[i] = d < 0 ? 0u : (uint32_t)(d + 0.5); }
        i++;
        if (r_peek(r, ',')) { r->s++; continue; }
        return r_eat(r, ']');
    }
}
static bool r_params(r_t* r, sumi_params_t* p) {
    if (!r_eat(r, '{')) return false;
    if (r_peek(r, '}')) { r->s++; return true; }
    for (;;) {
        char key[64];
        if (!r_string(r, key, sizeof key) || !r_eat(r, ':')) return false;
        const field_t* f = NULL;
        for (size_t i = 0; i < N_PARAMS; i++) if (!strcmp(PARAMS[i].name, key)) { f = &PARAMS[i]; break; }
        if (!f) { if (!r_skip(r)) return false; }
        else {
            char* base = (char*)p + f->offset;
            if (f->count > 1) {
                float tmpf[16]; uint32_t tmpu[16];
                for (uint32_t k = 0; k < f->count; k++) { memcpy(&tmpf[k], base + 4 * k, 4); memcpy(&tmpu[k], base + 4 * k, 4); }
                if (!r_num_array(r, f->kind == F32 ? tmpf : NULL, f->kind == U32 ? tmpu : NULL, f->count)) return false;
                for (uint32_t k = 0; k < f->count; k++) memcpy(base + 4 * k, f->kind == F32 ? (const void*)&tmpf[k] : (const void*)&tmpu[k], 4);
            } else {
                double d; if (!r_number(r, &d)) return false;
                if (f->kind == F32) { const float v = (float)d; memcpy(base, &v, 4); }
                else { const uint32_t v = d < 0 ? 0u : (uint32_t)(d + 0.5); memcpy(base, &v, 4); }
            }
        }
        if (r_peek(r, ',')) { r->s++; continue; }
        return r_eat(r, '}');
    }
}
static bool r_palette(r_t* r, sumi_palette_t* pal) {
    if (!r_eat(r, '{')) return false;
    if (r_peek(r, '}')) { r->s++; return true; }
    for (;;) {
        char key[32];
        if (!r_string(r, key, sizeof key) || !r_eat(r, ':')) return false;
        if (!strcmp(key, "stops")) {
            if (!r_eat(r, '[')) return false;
            uint32_t n = 0;
            if (!r_peek(r, ']')) for (;;) {
                float s[4] = {0, 0, 0, 0};
                if (!r_num_array(r, s, NULL, 4)) return false;
                if (n < SUMI_PALETTE_MAX_STOPS) { pal->stops[n].rgb[0] = s[0]; pal->stops[n].rgb[1] = s[1]; pal->stops[n].rgb[2] = s[2]; pal->stops[n].position = s[3]; }
                n++;
                if (r_peek(r, ',')) { r->s++; continue; }
                break;
            }
            if (!r_eat(r, ']')) return false;
            if (n) pal->stop_count = n > SUMI_PALETTE_MAX_STOPS ? SUMI_PALETTE_MAX_STOPS : n;
            for (uint32_t i = pal->stop_count; i < SUMI_PALETTE_MAX_STOPS && pal->stop_count; i++) pal->stops[i] = pal->stops[pal->stop_count - 1];
        }
        else if (!strcmp(key, "depth_gamma")) { double d; if (!r_number(r, &d)) return false; pal->depth_gamma = (float)d; }
        else if (!strcmp(key, "depth_floor")) { double d; if (!r_number(r, &d)) return false; pal->depth_floor = (float)d; }
        else if (!strcmp(key, "hue_drift"))   { double d; if (!r_number(r, &d)) return false; pal->hue_drift = (float)d; }
        else if (!strcmp(key, "accent_rgb"))  { if (!r_num_array(r, pal->accent_rgb, NULL, 3)) return false; }
        else if (!strcmp(key, "clear_rgb"))   { if (!r_num_array(r, pal->clear_rgb, NULL, 3)) return false; }
        else if (!r_skip(r)) return false;
        if (r_peek(r, ',')) { r->s++; continue; }
        return r_eat(r, '}');
    }
}
static bool r_triples(r_t* r, sumi_preset_t* p, bool cc) {   /* cc_map: [ch, cc, target]; controls: [ctl, value] */
    if (!r_eat(r, '[')) return false;
    uint32_t n = 0;
    if (!r_peek(r, ']')) for (;;) {
        uint32_t v[3] = {0, 0, 0};
        if (!r_num_array(r, NULL, v, 3)) return false;
        if (cc) { if (n < SUMI_PRESET_MAX_CC) { p->cc[n].channel = (uint8_t)(v[0] > 255u ? 255u : v[0]); p->cc[n].cc = (uint8_t)(v[1] > 127u ? 127u : v[1]); p->cc[n].target = v[2]; } }
        else    { if (n < SUMI_PRESET_MAX_CONTROLS) { p->controls[n].ctl = v[0]; p->controls[n].value = (uint8_t)(v[1] > 127u ? 127u : v[1]); } }
        n++;
        if (r_peek(r, ',')) { r->s++; continue; }
        break;
    }
    if (!r_eat(r, ']')) return false;
    if (cc) p->cc_count = n > SUMI_PRESET_MAX_CC ? SUMI_PRESET_MAX_CC : n;
    else    p->control_count = n > SUMI_PRESET_MAX_CONTROLS ? SUMI_PRESET_MAX_CONTROLS : n;
    return true;
}
static bool r_mineral(r_t* r, sumi_preset_t* p) {   /* 1.7.0 (step 71b): the mineral, every field by its name; unknown keys skipped */
    if (!r_eat(r, '{')) return false;
    p->mineral.size = (uint32_t)sizeof(sumi_mineral_t);
    if (r_peek(r, '}')) { r->s++; p->mineral_present = true; return true; }
    for (;;) {
        char key[32]; double d;
        if (!r_string(r, key, sizeof key) || !r_eat(r, ':')) return false;
        float* f = NULL; uint32_t* u = NULL;
        sumi_mineral_t* m = &p->mineral;
        if (!strcmp(key, "id")) u = &p->mineral_id;
        else if (!strcmp(key, "optic_class")) u = &m->optic_class;
        else if (!strcmp(key, "retardation")) f = &m->retardation;
        else if (!strcmp(key, "retardation_bias")) f = &m->retardation_bias;
        else if (!strcmp(key, "two_v")) f = &m->two_v;
        else if (!strcmp(key, "walkoff")) f = &m->walkoff;
        else if (!strcmp(key, "dispersion")) f = &m->dispersion;
        else if (!strcmp(key, "film_n")) f = &m->film_n;
        else if (!strcmp(key, "film_r")) f = &m->film_r;
        else if (!strcmp(key, "conical_ring")) f = &m->conical_ring;
        else if (!strcmp(key, "bloom_gain")) f = &m->bloom_gain;
        else if (!strcmp(key, "lut_id")) u = &m->lut_id;
        if (!strcmp(key, "body_tint")) {
            if (!r_eat(r, '[')) return false;
            for (int c = 0; c < 3; c++) {
                if (c && !r_eat(r, ',')) return false;
                if (!r_number(r, &d)) return false;
                m->body_tint[c] = (float)d;
            }
            if (!r_eat(r, ']')) return false;
        } else if (f || u) {
            if (!r_number(r, &d)) return false;
            if (f) *f = (float)d; else *u = d < 0 ? 0u : (uint32_t)(d + 0.5);
        } else if (!r_skip(r)) return false;
        if (r_peek(r, ',')) { r->s++; continue; }
        if (!r_eat(r, '}')) return false;
        break;
    }
    p->mineral_present = true;
    return true;
}
/* step 82: a JSON array of numbers into `out[max]`; extras dropped, the rest left as they were (the schema's rule) */
static bool r_farray(r_t* r, float* out, int max) {
    if (!r_eat(r, '[')) return false;
    if (r_peek(r, ']')) { r->s++; return true; }
    for (int i = 0;; i++) {
        double d; if (!r_number(r, &d)) return false;
        if (i < max) out[i] = (float)d;
        if (r_peek(r, ',')) { r->s++; continue; }
        return r_eat(r, ']');
    }
}
static bool r_suzu(r_t* r, sumi_preset_t* p) {   /* step 57: Suzu's patch, every field by its name; unknown keys skipped */
    if (!r_eat(r, '{')) return false;
    if (r_peek(r, '}')) { r->s++; p->suzu_present = true; return true; }
    for (;;) {
        char key[32]; double d;
        if (!r_string(r, key, sizeof key) || !r_eat(r, ':')) return false;
        float* f = NULL; uint32_t* u = NULL;
        if (!strcmp(key, "source")) u = &p->suzu.source;
        else if (!strcmp(key, "level")) f = &p->suzu.level;
        else if (!strcmp(key, "attack_s")) f = &p->suzu.attack_s;
        else if (!strcmp(key, "release_s")) f = &p->suzu.release_s;
        else if (!strcmp(key, "cutoff_hz")) f = &p->suzu.cutoff_hz;
        else if (!strcmp(key, "resonance")) f = &p->suzu.resonance;
        else if (!strcmp(key, "shear")) f = &p->suzu.shear;
        else if (!strcmp(key, "shear_kind")) u = &p->suzu.shear_kind;
        else if (!strcmp(key, "voice_kind")) u = &p->suzu.voice_kind;
        else if (!strcmp(key, "modal_preset")) u = &p->suzu.modal_preset;
        else if (!strcmp(key, "modes")) u = &p->suzu.modes;
        else if (!strcmp(key, "coupling")) f = &p->suzu.coupling;
        else if (!strcmp(key, "decay_s")) f = &p->suzu.decay_s;
        else if (!strcmp(key, "decay_bright")) f = &p->suzu.decay_bright;
        else if (!strcmp(key, "stiffness")) f = &p->suzu.stiffness;
        else if (!strcmp(key, "pluck")) f = &p->suzu.pluck;
        else if (!strcmp(key, "bow_onset_s")) f = &p->suzu.bow_onset_s;
        else if (!strcmp(key, "bow_position")) f = &p->suzu.bow_position;
        else if (!strcmp(key, "breath_cc")) u = &p->suzu.breath_cc;
        else if (!strcmp(key, "string_nodes")) u = &p->suzu.string_nodes;
        else if (!strcmp(key, "oversample")) u = &p->suzu.oversample;   /* step 78 */
        else if (!strcmp(key, "string_decay_s")) f = &p->suzu.string_decay_s;
        else if (!strcmp(key, "pickup")) f = &p->suzu.pickup;
        else if (!strcmp(key, "bridge_hz")) f = &p->suzu.bridge_hz;
        else if (!strcmp(key, "bridge_cells")) u = &p->suzu.bridge_cells;
        else if (!strcmp(key, "bridge_coupling")) f = &p->suzu.bridge_coupling;
        else if (!strcmp(key, "bridge_decay_s")) f = &p->suzu.bridge_decay_s;
        else if (!strcmp(key, "loop_loss")) f = &p->suzu.loop_loss;
        else if (!strcmp(key, "duffing_beta")) f = &p->suzu.duffing_beta;
        else if (!strcmp(key, "drive")) f = &p->suzu.drive;
        else if (!strcmp(key, "drive_ratio")) f = &p->suzu.drive_ratio;
        else if (!strcmp(key, "rotor_k")) f = &p->suzu.rotor_k;
        else if (!strcmp(key, "mod_target")) u = &p->suzu.mod_target;
        else if (!strcmp(key, "mod_depth")) f = &p->suzu.mod_depth;
        else if (!strcmp(key, "mod_rate")) f = &p->suzu.mod_rate;
        else if (!strcmp(key, "bore_nodes")) u = &p->suzu.bore_nodes;
        else if (!strcmp(key, "bore_loss")) f = &p->suzu.bore_loss;
        else if (!strcmp(key, "bore_corner_hz")) f = &p->suzu.bore_corner_hz;
        else if (!strcmp(key, "jet_gain")) f = &p->suzu.jet_gain;
        else if (!strcmp(key, "jet_drive")) f = &p->suzu.jet_drive;
        else if (!strcmp(key, "jet_tau")) f = &p->suzu.jet_tau;
        else if (!strcmp(key, "jet_q")) f = &p->suzu.jet_q;
        else if (!strcmp(key, "jet_noise")) f = &p->suzu.jet_noise;
        else if (!strcmp(key, "jet_area")) f = &p->suzu.jet_area;       /* step 67 (DECISIONS_9 #10) */
        else if (!strcmp(key, "jet_offset")) f = &p->suzu.jet_offset;
        else if (!strcmp(key, "breath_ref")) f = &p->suzu.breath_ref;
        else if (!strcmp(key, "breath_range")) f = &p->suzu.breath_range;
        else if (!strcmp(key, "bore_wall_s")) f = &p->suzu.bore_wall_s;
        else if (!strcmp(key, "press_blows")) u = &p->suzu.press_blows;
        else if (!strcmp(key, "reed_hz")) f = &p->suzu.reed_hz;
        else if (!strcmp(key, "reed_q")) f = &p->suzu.reed_q;
        else if (!strcmp(key, "reed_open")) f = &p->suzu.reed_open;
        else if (!strcmp(key, "reed_close")) f = &p->suzu.reed_close;
        else if (!strcmp(key, "reed_area")) f = &p->suzu.reed_area;
        else if (!strcmp(key, "reed_noise")) f = &p->suzu.reed_noise;
        else if (!strcmp(key, "cone_apex")) f = &p->suzu.cone_apex;
        else if (!strcmp(key, "lip_ratio")) f = &p->suzu.lip_ratio;
        else if (!strcmp(key, "lip_q")) f = &p->suzu.lip_q;
        else if (!strcmp(key, "lip_open")) f = &p->suzu.lip_open;
        else if (!strcmp(key, "lip_close")) f = &p->suzu.lip_close;
        else if (!strcmp(key, "lip_area")) f = &p->suzu.lip_area;
        else if (!strcmp(key, "lip_range")) f = &p->suzu.lip_range;
        else if (!strcmp(key, "partial")) u = &p->suzu.partial;
        else if (!strcmp(key, "bell_start")) f = &p->suzu.bell_start;
        else if (!strcmp(key, "bell_gamma")) f = &p->suzu.bell_gamma;
        else if (!strcmp(key, "brass")) f = &p->suzu.brass;
        else if (!strcmp(key, "trace_scope")) u = &p->suzu.trace_scope;        /* step 59 */
        else if (!strcmp(key, "trace_ink")) u = &p->suzu.trace_ink;
        else if (!strcmp(key, "trace_kinds")) u = &p->suzu.trace_kinds;
        else if (!strcmp(key, "drum_size")) u = &p->suzu.drum_size;          /* step 79 */
        else if (!strcmp(key, "drum_kappa")) f = &p->suzu.drum_kappa;
        else if (!strcmp(key, "drum_alpha")) f = &p->suzu.drum_alpha;
        else if (!strcmp(key, "drum_cap")) f = &p->suzu.drum_cap;
        else if (!strcmp(key, "plate_on")) u = &p->suzu.plate_on;
        else if (!strcmp(key, "plate_size")) u = &p->suzu.plate_size;
        else if (!strcmp(key, "plate_hz")) f = &p->suzu.plate_hz;
        else if (!strcmp(key, "plate_kappa")) f = &p->suzu.plate_kappa;
        else if (!strcmp(key, "plate_decay_s")) f = &p->suzu.plate_decay_s;
        else if (!strcmp(key, "plate_bright")) f = &p->suzu.plate_bright;
        else if (!strcmp(key, "plate_gain")) f = &p->suzu.plate_gain;
        else if (!strcmp(key, "bow_mode")) u = &p->suzu.bow_mode;            /* step 80 */
        else if (!strcmp(key, "bow_force")) f = &p->suzu.bow_force;
        else if (!strcmp(key, "bow_speed")) f = &p->suzu.bow_speed;
        else if (!strcmp(key, "reed_inertance")) f = &p->suzu.reed_inertance;   /* step 81 */
        else if (!strcmp(key, "bore_leak")) f = &p->suzu.bore_leak;
        else if (!strcmp(key, "brass_shock")) f = &p->suzu.brass_shock;
        else if (!strcmp(key, "lip_shear")) f = &p->suzu.lip_shear;
        else if (!strcmp(key, "lip_shear_ratio")) f = &p->suzu.lip_shear_ratio;
        else if (!strcmp(key, "tonehole_mode")) u = &p->suzu.tonehole_mode;
        else if (!strcmp(key, "hole_size")) f = &p->suzu.hole_size;
        else if (!strcmp(key, "hole_loss")) f = &p->suzu.hole_loss;
        else if (!strcmp(key, "trace_segments")) u = &p->suzu.trace_segments;
        else if (!strcmp(key, "trace_stroke")) u = &p->suzu.trace_stroke;
        else if (!strcmp(key, "trace_scale")) f = &p->suzu.trace_scale;
        else if (!strcmp(key, "trace_canvas")) u = &p->suzu.trace_canvas;
        else if (!strcmp(key, "trace_mode")) u = &p->suzu.trace_mode;        /* step 75 */
        else if (!strcmp(key, "modal_comp")) u = &p->suzu.modal_comp;        /* step 82 */
        else if (!strcmp(key, "toda_nodes")) u = &p->suzu.toda_nodes;        /* step 83 */
        else if (!strcmp(key, "toda_b")) f = &p->suzu.toda_b;
        else if (!strcmp(key, "toda_cap")) f = &p->suzu.toda_cap;
        else if (!strcmp(key, "toda_width")) f = &p->suzu.toda_width;
        else if (!strcmp(key, "boris_b")) f = &p->suzu.boris_b;             /* step 84 */
        else if (!strcmp(key, "boris_b_rest")) f = &p->suzu.boris_b_rest;
        else if (!strcmp(key, "dirac_masses")) u = &p->suzu.dirac_masses;     /* step 85 */
        else if (!strcmp(key, "dirac_bridge_hz")) f = &p->suzu.dirac_bridge_hz;
        else if (!strcmp(key, "dirac_bridge_mass")) f = &p->suzu.dirac_bridge_mass;
        else if (!strcmp(key, "dirac_stop")) f = &p->suzu.dirac_stop;
        else if (!strcmp(key, "dirac_stop_loss")) f = &p->suzu.dirac_stop_loss;
        else if (!strcmp(key, "dirac_bridge_decay")) f = &p->suzu.dirac_bridge_decay;
        else if (!strcmp(key, "bill_drive")) f = &p->suzu.bill_drive;
        else if (!strcmp(key, "bill_aspect")) f = &p->suzu.bill_aspect;
        else if (!strcmp(key, "bill_length")) f = &p->suzu.bill_length;
        else if (!strcmp(key, "bill_length_rest")) f = &p->suzu.bill_length_rest;
        float* arr = NULL;                                                     /* step 82: the table's arrays */
        if (!strcmp(key, "modal_ratio")) arr = p->suzu.modal_ratio; else if (!strcmp(key, "modal_t60")) arr = p->suzu.modal_t60; else if (!strcmp(key, "modal_weight")) arr = p->suzu.modal_weight;
        if (!strcmp(key, "mod_src") || !strcmp(key, "mod_dst") || !strcmp(key, "mod_amt")) {   /* step 86: the matrix's slots */
            float t[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; int i; if (!r_farray(r, t, 4)) return false;
            if (!strcmp(key, "mod_amt")) { for (i = 0; i < 4; i++) p->suzu.mod_amt[i] = t[i]; }
            else { uint32_t* a = !strcmp(key, "mod_src") ? p->suzu.mod_src : p->suzu.mod_dst; for (i = 0; i < 4; i++) a[i] = t[i] < 0.0f ? 0u : (uint32_t)(t[i] + 0.5f); }
        }
        else if (arr) { if (!r_farray(r, arr, SUMI_PRESET_MODAL_MAX)) return false; }
        else if (f || u) { if (!r_number(r, &d)) return false; if (f) *f = (float)d; else *u = d < 0 ? 0u : (uint32_t)(d + 0.5); }
        else if (!r_skip(r)) return false;
        if (r_peek(r, ',')) { r->s++; continue; }
        if (!r_eat(r, '}')) return false;
        p->suzu_present = true;
        return true;
    }
}

static bool r_small_object(r_t* r, sumi_preset_t* p, bool strip) {   /* strip: assign_a/assign_b; layout_state: buttons/slider */
    if (!r_eat(r, '{')) return false;
    if (r_peek(r, '}')) { r->s++; return true; }
    for (;;) {
        char key[32]; double d;
        if (!r_string(r, key, sizeof key) || !r_eat(r, ':')) return false;
        if (strip && !strcmp(key, "assign_a")) { if (!r_number(r, &d)) return false; p->strip_assign_a = (uint8_t)(d < 0 ? 0 : d > 127 ? 127 : d + 0.5); }
        else if (strip && !strcmp(key, "assign_b")) { if (!r_number(r, &d)) return false; p->strip_assign_b = (uint8_t)(d < 0 ? 0 : d > 127 ? 127 : d + 0.5); }
        else if (!strip && !strcmp(key, "buttons")) { if (!r_number(r, &d)) return false; p->layout_state.buttons = d < 0 ? 0u : (uint32_t)(d + 0.5); }
        else if (!strip && !strcmp(key, "slider"))  { if (!r_number(r, &d)) return false; p->layout_state.slider = (float)d; }
        else if (!r_skip(r)) return false;
        if (r_peek(r, ',')) { r->s++; continue; }
        return r_eat(r, '}');
    }
}

bool sumi_preset_read(const char* json, size_t len, sumi_preset_t* inout) {
    if (!json || !inout) return false;
    if (len == 0) len = strlen(json);
    sumi_preset_t p = *inout;                                     /* commit only on success */
    r_t r = { json, json + len, true };
    if (!r_eat(&r, '{')) return false;
    if (r_peek(&r, '}')) return false;                            /* an empty object is not a preset */
    for (;;) {
        char key[32]; double d;
        if (!r_string(&r, key, sizeof key) || !r_eat(&r, ':')) return false;
        bool ok = true;
        if (!strcmp(key, "midi_sink_preset")) { ok = r_number(&r, &d); if (ok) p.schema = d < 0 ? 0u : (uint32_t)(d + 0.5); }
        else if (!strcmp(key, "sumi_version")) { uint32_t v[3] = {0, 0, 0}; ok = r_num_array(&r, NULL, v, 3); if (ok) p.sumi_version = ((v[0] & 0xFFu) << 16) | ((v[1] & 0xFFu) << 8) | (v[2] & 0xFFu); }
        else if (!strcmp(key, "name")) ok = r_string(&r, p.name, sizeof p.name);
        else if (!strcmp(key, "input_mode")) { ok = r_number(&r, &d); if (ok) p.input_mode = d < 0 ? 0u : (uint32_t)(d + 0.5); }
        else if (!strcmp(key, "params")) ok = r_params(&r, &p.params);
        else if (!strcmp(key, "palette")) ok = r_palette(&r, &p.palette);
        else if (!strcmp(key, "cc_map")) ok = r_triples(&r, &p, true);
        else if (!strcmp(key, "controls")) ok = r_triples(&r, &p, false);
        else if (!strcmp(key, "strip")) ok = r_small_object(&r, &p, true);
        else if (!strcmp(key, "layout_state")) ok = r_small_object(&r, &p, false);
        else if (!strcmp(key, "suzu")) ok = r_suzu(&r, &p);
        else if (!strcmp(key, "mineral")) ok = r_mineral(&r, &p);   /* 1.7.0 (step 71b) */
        else ok = r_skip(&r);
        if (!ok || !r.ok) return false;
        if (r_peek(&r, ',')) { r.s++; continue; }
        if (!r_eat(&r, '}')) return false;
        break;
    }
    *inout = p;
    return true;
}
