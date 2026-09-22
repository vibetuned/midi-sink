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
        else ok = r_skip(&r);
        if (!ok || !r.ok) return false;
        if (r_peek(&r, ',')) { r.s++; continue; }
        if (!r_eat(&r, '}')) return false;
        break;
    }
    *inout = p;
    return true;
}
