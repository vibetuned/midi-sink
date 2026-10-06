// suzu_web.c — the Suzu lab's wasm surface (Phase 8 step 59b, DECISIONS_7 #28).
//
// Voxo with no device, driven from JavaScript: the lab's AudioWorklet renders each
// block, node renders the web gate's script, and the native reference compiles this
// same file against the no-FMA Voxo (voxo_nofma) so the gate compares like with like.
// Flat functions and flat float buffers only — the JavaScript never depends on a C
// struct's layout: the parameters go by name through a table generated from voxo.h,
// the trace and the inspection come back as documented float records.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "voxo.h"

/* The exports the worklet imports. Under emscripten (and any GCC/Clang build
 * of the reference) the attribute keeps them alive and visible; MSVC has no
 * such attribute and the native reference is linked statically, so the
 * plain declaration is the export (step 67, the Windows lane — DECISIONS_9 #8). */
#if defined(__GNUC__) || defined(__clang__)
#define SW_EXPORT __attribute__((used, visibility("default")))
#else
#define SW_EXPORT
#endif

// ---- the parameters, by name (the table follows voxo_suzu_params_t field for field) ----
typedef struct { const char* name; uint32_t off; uint32_t is_int; } sw_field_t;
static const sw_field_t FIELDS[] = {
    { "level", offsetof(voxo_suzu_params_t, level), 0 },
    { "attack_s", offsetof(voxo_suzu_params_t, attack_s), 0 },
    { "release_s", offsetof(voxo_suzu_params_t, release_s), 0 },
    { "cutoff_hz", offsetof(voxo_suzu_params_t, cutoff_hz), 0 },
    { "resonance", offsetof(voxo_suzu_params_t, resonance), 0 },
    { "shear", offsetof(voxo_suzu_params_t, shear), 0 },
    { "shear_kind", offsetof(voxo_suzu_params_t, shear_kind), 1 },
    { "retune_mode", offsetof(voxo_suzu_params_t, retune_mode), 1 },
    { "update_mode", offsetof(voxo_suzu_params_t, update_mode), 1 },
    { "voice_kind", offsetof(voxo_suzu_params_t, voice_kind), 1 },
    { "modal_preset", offsetof(voxo_suzu_params_t, modal_preset), 1 },
    { "modes", offsetof(voxo_suzu_params_t, modes), 1 },
    { "coupling", offsetof(voxo_suzu_params_t, coupling), 0 },
    { "decay_s", offsetof(voxo_suzu_params_t, decay_s), 0 },
    { "decay_bright", offsetof(voxo_suzu_params_t, decay_bright), 0 },
    { "stiffness", offsetof(voxo_suzu_params_t, stiffness), 0 },
    { "pluck", offsetof(voxo_suzu_params_t, pluck), 0 },
    { "bow_onset_s", offsetof(voxo_suzu_params_t, bow_onset_s), 0 },
    { "bow_position", offsetof(voxo_suzu_params_t, bow_position), 0 },
    { "breath_cc", offsetof(voxo_suzu_params_t, breath_cc), 1 },
    { "lattice_gate", offsetof(voxo_suzu_params_t, lattice_gate), 1 },
    { "string_nodes", offsetof(voxo_suzu_params_t, string_nodes), 1 },
    { "string_decay_s", offsetof(voxo_suzu_params_t, string_decay_s), 0 },
    { "pickup", offsetof(voxo_suzu_params_t, pickup), 0 },
    { "cfl_gate", offsetof(voxo_suzu_params_t, cfl_gate), 1 },
    { "string_cfl", offsetof(voxo_suzu_params_t, string_cfl), 0 },
    { "bridge_hz", offsetof(voxo_suzu_params_t, bridge_hz), 0 },
    { "bridge_cells", offsetof(voxo_suzu_params_t, bridge_cells), 1 },
    { "bridge_coupling", offsetof(voxo_suzu_params_t, bridge_coupling), 0 },
    { "bridge_decay_s", offsetof(voxo_suzu_params_t, bridge_decay_s), 0 },
    { "bridge_gain", offsetof(voxo_suzu_params_t, bridge_gain), 0 },
    { "loop_loss", offsetof(voxo_suzu_params_t, loop_loss), 0 },
    { "passivity_gate", offsetof(voxo_suzu_params_t, passivity_gate), 1 },
    { "duffing_beta", offsetof(voxo_suzu_params_t, duffing_beta), 0 },
    { "drive", offsetof(voxo_suzu_params_t, drive), 0 },
    { "drive_ratio", offsetof(voxo_suzu_params_t, drive_ratio), 0 },
    { "rotor_k", offsetof(voxo_suzu_params_t, rotor_k), 0 },
    { "mod_target", offsetof(voxo_suzu_params_t, mod_target), 1 },
    { "mod_depth", offsetof(voxo_suzu_params_t, mod_depth), 0 },
    { "mod_rate", offsetof(voxo_suzu_params_t, mod_rate), 0 },
    { "bore_nodes", offsetof(voxo_suzu_params_t, bore_nodes), 1 },
    { "bore_loss", offsetof(voxo_suzu_params_t, bore_loss), 0 },
    { "bore_corner_hz", offsetof(voxo_suzu_params_t, bore_corner_hz), 0 },
    { "bore_cfl_gate", offsetof(voxo_suzu_params_t, bore_cfl_gate), 1 },
    { "bore_cfl", offsetof(voxo_suzu_params_t, bore_cfl), 0 },
    { "jet_gain", offsetof(voxo_suzu_params_t, jet_gain), 0 },
    { "jet_drive", offsetof(voxo_suzu_params_t, jet_drive), 0 },
    { "jet_tau", offsetof(voxo_suzu_params_t, jet_tau), 0 },
    { "jet_q", offsetof(voxo_suzu_params_t, jet_q), 0 },
    { "jet_noise", offsetof(voxo_suzu_params_t, jet_noise), 0 },
    { "jet_area", offsetof(voxo_suzu_params_t, jet_area), 0 },
    { "jet_offset", offsetof(voxo_suzu_params_t, jet_offset), 0 },
    { "breath_ref", offsetof(voxo_suzu_params_t, breath_ref), 0 },
    { "breath_range", offsetof(voxo_suzu_params_t, breath_range), 0 },
    { "bore_wall_s", offsetof(voxo_suzu_params_t, bore_wall_s), 0 },
    { "press_blows", offsetof(voxo_suzu_params_t, press_blows), 1 },
    { "reed_hz", offsetof(voxo_suzu_params_t, reed_hz), 0 },
    { "reed_q", offsetof(voxo_suzu_params_t, reed_q), 0 },
    { "reed_open", offsetof(voxo_suzu_params_t, reed_open), 0 },
    { "reed_close", offsetof(voxo_suzu_params_t, reed_close), 0 },
    { "reed_area", offsetof(voxo_suzu_params_t, reed_area), 0 },
    { "reed_noise", offsetof(voxo_suzu_params_t, reed_noise), 0 },
    { "cone_apex", offsetof(voxo_suzu_params_t, cone_apex), 0 },
    { "lip_ratio", offsetof(voxo_suzu_params_t, lip_ratio), 0 },
    { "lip_q", offsetof(voxo_suzu_params_t, lip_q), 0 },
    { "lip_open", offsetof(voxo_suzu_params_t, lip_open), 0 },
    { "lip_close", offsetof(voxo_suzu_params_t, lip_close), 0 },
    { "lip_area", offsetof(voxo_suzu_params_t, lip_area), 0 },
    { "lip_range", offsetof(voxo_suzu_params_t, lip_range), 0 },
    { "partial", offsetof(voxo_suzu_params_t, partial), 1 },
    { "bell_start", offsetof(voxo_suzu_params_t, bell_start), 0 },
    { "bell_gamma", offsetof(voxo_suzu_params_t, bell_gamma), 0 },
    { "brass", offsetof(voxo_suzu_params_t, brass), 0 },
    { "valve_naive", offsetof(voxo_suzu_params_t, valve_naive), 1 },
    { "valve_gate", offsetof(voxo_suzu_params_t, valve_gate), 1 },
};
#define FIELD_COUNT (sizeof FIELDS / sizeof FIELDS[0])
static voxo_suzu_params_t g_params;
static int g_params_ready = 0;
static void params_ready(void) { if (!g_params_ready) { voxo_suzu_default_params(&g_params); g_params_ready = 1; } }

SW_EXPORT uint32_t sw_version(void) { return voxo_version(); }
SW_EXPORT uint32_t sw_param_count(void) { return (uint32_t)FIELD_COUNT; }
SW_EXPORT const char* sw_param_name(uint32_t i) { return i < FIELD_COUNT ? FIELDS[i].name : ""; }
SW_EXPORT uint32_t sw_param_is_int(uint32_t i) { return i < FIELD_COUNT ? FIELDS[i].is_int : 0u; }
SW_EXPORT void sw_param_defaults(void) { voxo_suzu_default_params(&g_params); g_params_ready = 1; }
SW_EXPORT void sw_param_set(uint32_t i, double value) {
    params_ready();
    if (i >= FIELD_COUNT) return;
    unsigned char* base = (unsigned char*)&g_params + FIELDS[i].off;
    if (FIELDS[i].is_int) { const uint32_t u = value < 0.0 ? 0u : (uint32_t)(value + 0.5); memcpy(base, &u, sizeof u); }
    else { const float f = (float)value; memcpy(base, &f, sizeof f); }
}
SW_EXPORT double sw_param_get(uint32_t i) {
    params_ready();
    if (i >= FIELD_COUNT) return 0.0;
    const unsigned char* base = (const unsigned char*)&g_params + FIELDS[i].off;
    if (FIELDS[i].is_int) { uint32_t u; memcpy(&u, base, sizeof u); return (double)u; }
    float f; memcpy(&f, base, sizeof f); return (double)f;
}

// ---- the log: Voxo's lines (a rejected patch, the winds' calibration) kept for the host to take ----
static char g_log[8192];
static uint32_t g_log_len = 0;
static void log_cb(int level, const char* text, void* user) {
    (void)level; (void)user;
    const size_t n = strlen(text);
    if (g_log_len + n + 2 >= sizeof g_log) g_log_len = 0;                 // a full log restarts (the host takes it often)
    memcpy(g_log + g_log_len, text, n); g_log_len += (uint32_t)n; g_log[g_log_len++] = '\n'; g_log[g_log_len] = '\0';
}
SW_EXPORT const char* sw_log_ptr(void) { return g_log; }
SW_EXPORT uint32_t sw_log_len(void) { return g_log_len; }
SW_EXPORT void sw_log_clear(void) { g_log_len = 0; g_log[0] = '\0'; }

// ---- the instance ----
SW_EXPORT voxo_t* sw_create(uint32_t sample_rate, uint32_t max_voices) {
    voxo_config_t c; memset(&c, 0, sizeof c);
    c.sample_rate = sample_rate; c.block_frames = 128; c.max_voices = max_voices;
    c.log_cb = log_cb;
    voxo_t* v = voxo_create(&c);
    if (v) { voxo_set_input_mode(v, 1u /* MPE */); voxo_set_source(v, VOXO_SOURCE_SUZU); }
    params_ready();
    return v;
}
SW_EXPORT void sw_destroy(voxo_t* v) { voxo_destroy(v); }
SW_EXPORT uint32_t sw_apply(voxo_t* v) { params_ready(); return voxo_set_suzu_params(v, &g_params) ? 1u : 0u; }   // 0: rejected (the log says why)
SW_EXPORT void sw_midi(voxo_t* v, uint32_t status, uint32_t d1, uint32_t d2) { voxo_push_midi(v, (uint8_t)status, (uint8_t)d1, (uint8_t)d2); }
SW_EXPORT void sw_set_gain(voxo_t* v, float gain) { voxo_set_gain(v, gain); }

// ---- the render: interleaved stereo into a static buffer ----
#define SW_MAX_FRAMES 1024
static float g_out[2 * SW_MAX_FRAMES];
SW_EXPORT float* sw_out_ptr(void) { return g_out; }
SW_EXPORT void sw_render(voxo_t* v, uint32_t frames) { voxo_render(v, g_out, frames > SW_MAX_FRAMES ? SW_MAX_FRAMES : frames); }

// ---- the trace: per voice a record of SW_TRACE_STRIDE floats ----
//   [0] points  [1] amplitude  [2] note  [3] voice kind  [4] held  [5] serial  [6] channel  [7] 0
//   [8 … 8+16] x  [25 … 25+16] y   (VOXO_TRACE_POINTS_MAX = 17 each, unit-normalized)
#define SW_TRACE_VOICES 16
#define SW_TRACE_STRIDE (8 + 2 * VOXO_TRACE_POINTS_MAX)
static float g_trace[SW_TRACE_VOICES * SW_TRACE_STRIDE];
SW_EXPORT float* sw_trace_ptr(void) { return g_trace; }
SW_EXPORT uint32_t sw_trace_stride(void) { return SW_TRACE_STRIDE; }
SW_EXPORT void sw_trace_mask(voxo_t* v, uint32_t mask) { voxo_set_trace(v, mask); }
SW_EXPORT uint32_t sw_trace(voxo_t* v, uint32_t max_segments) {
    voxo_trace_t t[SW_TRACE_VOICES];
    const uint32_t n = voxo_trace_poll(v, t, SW_TRACE_VOICES, max_segments);
    for (uint32_t i = 0; i < n; i++) {
        float* r = g_trace + i * SW_TRACE_STRIDE;
        r[0] = (float)t[i].count; r[1] = t[i].amplitude; r[2] = (float)t[i].note; r[3] = (float)t[i].voice_kind;
        r[4] = (float)t[i].held; r[5] = (float)t[i].serial; r[6] = (float)t[i].channel; r[7] = 0.0f;
        for (uint32_t k = 0; k < VOXO_TRACE_POINTS_MAX; k++) { r[8 + k] = k < t[i].count ? t[i].x[k] : 0.0f; r[8 + VOXO_TRACE_POINTS_MAX + k] = k < t[i].count ? t[i].y[k] : 0.0f; }
    }
    return n;
}

// ---- the recent trace (step 59c): the ring's last points at full density, four floats a point (voxo.h) ----
#define SW_RECENT_MAX 1023
static float g_recent[4 * SW_RECENT_MAX];
SW_EXPORT float* sw_recent_ptr(void) { return g_recent; }
SW_EXPORT void sw_trace_decimation(voxo_t* v, uint32_t sub_steps) { voxo_set_trace_decimation(v, sub_steps); }
SW_EXPORT uint32_t sw_trace_recent(voxo_t* v, uint32_t voice, uint32_t max_points) {
    return voxo_trace_recent(v, voice, g_recent, max_points > SW_RECENT_MAX ? SW_RECENT_MAX : max_points);
}

// ---- the inspection: per voice a record of SW_INSPECT_STRIDE floats ----
//   [0] channel [1] note [2] voice kind [3] held [4] serial [5] freq [6] env [7] rate2 [8] n [9 … 15] 0
//   [16 …] a (257)  then b, c, s (257 each)  then k (16)
#define SW_INSPECT_VOICES 4
#define SW_INSPECT_STRIDE (16 + 4 * VOXO_INSPECT_MAX + 16)
static voxo_inspect_t g_ins[SW_INSPECT_VOICES];
static float g_inspect[SW_INSPECT_VOICES * SW_INSPECT_STRIDE];
SW_EXPORT float* sw_inspect_ptr(void) { return g_inspect; }
SW_EXPORT uint32_t sw_inspect_stride(void) { return SW_INSPECT_STRIDE; }
SW_EXPORT uint32_t sw_inspect(voxo_t* v) {
    const uint32_t n = voxo_suzu_inspect(v, g_ins, SW_INSPECT_VOICES);
    for (uint32_t i = 0; i < n; i++) {
        float* r = g_inspect + i * SW_INSPECT_STRIDE;
        const voxo_inspect_t* t = &g_ins[i];
        memset(r, 0, sizeof(float) * SW_INSPECT_STRIDE);
        r[0] = (float)t->channel; r[1] = (float)t->note; r[2] = (float)t->voice_kind; r[3] = (float)t->held; r[4] = (float)t->serial;
        r[5] = t->freq; r[6] = t->env; r[7] = t->rate2; r[8] = (float)t->n;
        memcpy(r + 16, t->a, sizeof t->a); memcpy(r + 16 + VOXO_INSPECT_MAX, t->b, sizeof t->b);
        memcpy(r + 16 + 2 * VOXO_INSPECT_MAX, t->c, sizeof t->c); memcpy(r + 16 + 3 * VOXO_INSPECT_MAX, t->s, sizeof t->s);
        memcpy(r + 16 + 4 * VOXO_INSPECT_MAX, t->k, sizeof t->k);
    }
    return n;
}
