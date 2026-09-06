// sumi_web.cpp — the wasm export glue (Phase 5 §5, DECISIONS_4 #15/#16).
//
// The C-ABI itself is the export surface: every SUMI_API function is listed
// in web/CMakeLists.txt's EXPORTED_FUNCTIONS and called from JS through
// cwrap. This file adds only what JS cannot do sanely by itself:
//   * sumi_web_create — builds sumi_config_t + sumi_webgpu_surface_t (a C
//     struct JS would otherwise have to lay out byte by byte);
//   * flat param accessors by field id (same reason: sumi_params_t layout);
//   * the field-dump hooks (internal, static-link-only — the wasm IS a static
//     link) for the §4.6 web tier.
// No logic lives here: the browser page is the host, this is its ABI shim.
#include <emscripten/emscripten.h>

#include "sumi_core.h"
#include "sumi_debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sumi_webgpu_surface_t g_surface;
static char g_selector[128];

// emdawnwebgpu's bridge for a device the PAGE created: the host sets
// Module.preinitializedWebGPUDevice before the module initializes and this
// returns its wasm-side handle (declared here — the port ships no C header
// for it; it is the documented, if deprecated-in-name, import path).
extern "C" void* emscripten_webgpu_get_device(void);

static void web_log(int level, const char* msg, void* /*user*/) {
    static const char* names[] = {"PANIC", "ERROR", "WARN", "INFO"};
    const char* name = (level >= 0 && level <= 3) ? names[level] : "?";
    // stderr -> console.error in Emscripten; the page filters INFO to console.log
    fprintf(level <= 1 ? stderr : stdout, "[sumi %s] %s\n", name, msg);
}

extern "C" {

EMSCRIPTEN_KEEPALIVE
sumi_instance_t* sumi_web_create(const void* device, const char* canvas_selector,
                                 uint32_t color_format, uint32_t width, uint32_t height,
                                 float pixel_ratio) {
    snprintf(g_selector, sizeof(g_selector), "%s", canvas_selector ? canvas_selector : "#sumi");
    if (!device) device = emscripten_webgpu_get_device();   // Module.preinitializedWebGPUDevice
    g_surface.device = device;
    g_surface.canvas_selector = g_selector;
    g_surface.color_format = color_format;
    sumi_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.native_surface_handle = &g_surface;
    cfg.backend = SUMI_BACKEND_WEBGPU;
    cfg.width = width;
    cfg.height = height;
    cfg.pixel_ratio = pixel_ratio;
    cfg.log_cb = web_log;
    return sumi_create(&cfg);
}

// sumi_params_t by field id (order = the struct's, spec §5.3).
enum {
    P_VISCOSITY = 0, P_EXPANSION, P_ROUGHNESS, P_SMOOTHING_MS, P_PALETTE, P_LAYOUT,
    P_SIM_SCALE, P_BPM, P_ROLL_SPEED, P_SLIDE_MODE, P_VORTEX_PROFILE, P_RIPPLE_BAKE,
    P_RIPPLE_ANGLE, P_PINCH_VARIANT, P_BEND_MODE, P_PRESS_MODE, P_WAKE_PROFILE, P_WAKE_SPREAD, P_COUNT
};

EMSCRIPTEN_KEEPALIVE
float sumi_web_get_param(sumi_instance_t* inst, int id) {
    if (!inst) return 0.0f;
    sumi_params_t p;
    sumi_get_params(inst, &p);
    switch (id) {
        case P_VISCOSITY:      return p.fluid_viscosity;
        case P_EXPANSION:      return p.expansion_rate;
        case P_ROUGHNESS:      return p.paper_roughness;
        case P_SMOOTHING_MS:   return p.smoothing_ms;
        case P_PALETTE:        return (float)p.active_palette_id;
        case P_LAYOUT:         return (float)p.pitch_layout;
        case P_SIM_SCALE:      return p.sim_scale;
        case P_BPM:            return p.bpm;
        case P_ROLL_SPEED:     return p.roll_speed;
        case P_SLIDE_MODE:     return (float)p.slide_mode;
        case P_VORTEX_PROFILE: return (float)p.vortex_profile;
        case P_RIPPLE_BAKE:    return (float)p.ripple_bake;
        case P_RIPPLE_ANGLE:   return p.ripple_angle;
        case P_PINCH_VARIANT:  return (float)p.pinch_variant;
        case P_BEND_MODE:      return (float)p.bend_mode;
        case P_PRESS_MODE:     return (float)p.press_mode;
        case P_WAKE_PROFILE:   return (float)p.wake_profile;
        case P_WAKE_SPREAD:    return p.wake_spread;
        default:               return 0.0f;
    }
}

EMSCRIPTEN_KEEPALIVE
void sumi_web_set_param(sumi_instance_t* inst, int id, float v) {
    if (!inst) return;
    sumi_params_t p;
    sumi_get_params(inst, &p);
    const uint32_t u = v < 0.0f ? 0u : (uint32_t)(v + 0.5f);
    switch (id) {
        case P_VISCOSITY:      p.fluid_viscosity = v; break;
        case P_EXPANSION:      p.expansion_rate = v; break;
        case P_ROUGHNESS:      p.paper_roughness = v; break;
        case P_SMOOTHING_MS:   p.smoothing_ms = v; break;
        case P_PALETTE:        p.active_palette_id = u % 3; break;
        case P_LAYOUT:         p.pitch_layout = u % 6; break;
        case P_SIM_SCALE:      p.sim_scale = v; break;
        case P_BPM:            p.bpm = v; break;
        case P_ROLL_SPEED:     p.roll_speed = v; break;
        case P_SLIDE_MODE:     p.slide_mode = u ? 1u : 0u; break;
        case P_VORTEX_PROFILE: p.vortex_profile = u ? 1u : 0u; break;
        case P_RIPPLE_BAKE:    p.ripple_bake = u ? 1u : 0u; break;
        case P_RIPPLE_ANGLE:   p.ripple_angle = v; break;
        case P_PINCH_VARIANT:  p.pinch_variant = u ? 1u : 0u; break;
        case P_BEND_MODE:      p.bend_mode = u ? 1u : 0u; break;
        case P_PRESS_MODE:     p.press_mode = u ? 1u : 0u; break;
        case P_WAKE_PROFILE:   p.wake_profile = u ? 1u : 0u; break;
        case P_WAKE_SPREAD:    p.wake_spread = v; break;
        default: return;
    }
    sumi_set_params(inst, &p);
}

// §4.6 web tier: the canonical script + the non-blocking field readback.
EMSCRIPTEN_KEEPALIVE
void sumi_web_field_script(sumi_instance_t* inst) { sumi_debug_run_field_script(inst); }

EMSCRIPTEN_KEEPALIVE
int sumi_web_field_begin(sumi_instance_t* inst) { return sumi_debug_read_field_begin(inst) ? 1 : 0; }

// Returns the readback state (0 idle, 1 in flight, 2 done); with out == NULL
// only fills w/h.
EMSCRIPTEN_KEEPALIVE
int sumi_web_field_poll(sumi_instance_t* inst, uint8_t* out, uint32_t capacity,
                        uint32_t* out_w, uint32_t* out_h) {
    return sumi_debug_read_field_poll(inst, out, (size_t)capacity, out_w, out_h);
}

// The layout probe for the page (Step 26 scenes place voices where they want a
// picture): the cell under (x, y) on the instance's current layout. out[4] =
// note, centre x, centre y, cell radius. Returns 1 on a playable cell, 0 for
// non-playable layouts, dead zones and points off the lattice. Shim only —
// sumi_layout_probe is existing ABI; the struct is flattened for JS.
EMSCRIPTEN_KEEPALIVE
int sumi_web_probe(sumi_instance_t* inst, float aspect, float x, float y, float* out) {
    if (!inst || !out) return 0;
    sumi_params_t p;
    sumi_get_params(inst, &p);
    sumi_cell_info_t c;
    if (!sumi_layout_probe(p.pitch_layout, &p, aspect, x, y, &c)) return 0;
    out[0] = (float)c.note; out[1] = c.cell_center_x; out[2] = c.cell_center_y; out[3] = c.cell_radius;
    return 1;
}

EMSCRIPTEN_KEEPALIVE
const char* sumi_web_version_string(void) {
    static char buf[64];
    const uint32_t v = sumi_version();
    snprintf(buf, sizeof(buf), "%u.%u.%u", v >> 16, (v >> 8) & 0xFF, v & 0xFF);
    return buf;
}

} // extern "C"
