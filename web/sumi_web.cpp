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
    P_RIPPLE_ANGLE, P_PINCH_VARIANT, P_BEND_MODE, P_PRESS_MODE, P_WAKE_PROFILE, P_WAKE_SPREAD,
    P_TORSION_SWEEP,   // v0.10 (Phase 6 step 36)
    P_CHLADNI_CELL,   // v0.11 (Phase 6 step 37)
    P_BURST_AGE, P_BURST_LIFE, P_BURST_ORDER,   // v0.12 (Phase 6 step 38)
    P_SPARK_STACK, P_SPARK_PROFILE, P_SPARK_SHEAR, P_SPARK_TAU,   // v0.13 (Phase 6 step 39)
    P_CHIRIKOV_KMAX, P_CHIRIKOV_PERIODS, P_CHIRIKOV_EPS,          // v0.14 (Phase 6 step 40)
    P_MEDIUM,                                                     // 1.0.0 (Phase 6 step 41)
    P_ANOD_GLOW,                                                  // 1.1.0 (Phase 6 step 42)
    P_ANOD_PITCH,                                                 // 1.1.0 (Phase 6 step 42)
    P_CHLADNI_MODE,                                               // 1.1.0 (Phase 6 step 43)
    P_PAPER_TINT_R, P_PAPER_TINT_G, P_PAPER_TINT_B,               // 1.1.0 (Phase 6 step 43, QOL §2): the substrate
    P_FIBER_SCALE, P_ANOD_DARK, P_ANOD_GRAIN,
    P_ANOD_BLOOM, P_ANOD_BLOOM_LEVELS,                            // 1.1.0 (Phase 6 step 43): the glow
    P_ANOD_DROP,                                                  // 42: the Anod strike's charge (#71)
    P_COUNT
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
        case P_TORSION_SWEEP:  return (float)p.torsion_sweep;
        case P_CHLADNI_CELL:   return p.chladni_cell;
        case P_BURST_AGE:      return p.burst_age;
        case P_BURST_LIFE:     return p.burst_life;
        case P_BURST_ORDER:    return (float)p.burst_order;
        case P_SPARK_STACK:    return (float)p.spark_stack;
        case P_SPARK_PROFILE:  return (float)p.spark_profile;
        case P_SPARK_SHEAR:    return p.spark_shear;
        case P_SPARK_TAU:      return p.spark_tau;
        case P_CHIRIKOV_KMAX:  return p.chirikov_kmax;
        case P_CHIRIKOV_PERIODS: return (float)p.chirikov_periods;
        case P_CHIRIKOV_EPS:   return p.chirikov_eps;
        case P_MEDIUM:         return (float)p.medium;
        case P_ANOD_GLOW:      return p.anod_glow;
        case P_ANOD_PITCH:     return p.anod_pitch;
        case P_CHLADNI_MODE:   return (float)p.chladni_mode;
        case P_PAPER_TINT_R:   return p.paper_tint[0];
        case P_PAPER_TINT_G:   return p.paper_tint[1];
        case P_PAPER_TINT_B:   return p.paper_tint[2];
        case P_FIBER_SCALE:    return p.fiber_scale;
        case P_ANOD_DARK:      return p.anod_dark;
        case P_ANOD_GRAIN:     return p.anod_grain;
        case P_ANOD_BLOOM:     return p.anod_bloom;
        case P_ANOD_BLOOM_LEVELS: return (float)p.anod_bloom_levels;
        case P_ANOD_DROP:      return p.anod_drop;
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
        case P_PALETTE:        p.active_palette_id = u % 4; break;   // step 43: 3 = the custom slot
        case P_LAYOUT:         p.pitch_layout = u % 8; break;   // v0.8: eight layouts
        case P_SIM_SCALE:      p.sim_scale = v; break;
        case P_BPM:            p.bpm = v; break;
        case P_ROLL_SPEED:     p.roll_speed = v; break;
        case P_SLIDE_MODE:     p.slide_mode = u == 255u ? 255u : (u > 2u ? 2u : u); break;   // 0 aux, 1 pinch, 2 the spark's k; 255 the medium's default (1.1.0)
        case P_VORTEX_PROFILE: p.vortex_profile = u == 3 ? 3u : (u ? 1u : 0u); break;   // 0 exp, 1 rankine, 3 torsion (v0.10)
        case P_RIPPLE_BAKE:    p.ripple_bake = u ? 1u : 0u; break;
        case P_RIPPLE_ANGLE:   p.ripple_angle = v; break;
        case P_PINCH_VARIANT:  p.pinch_variant = u ? 1u : 0u; break;
        case P_BEND_MODE:      p.bend_mode = u == 255u ? 255u : (u > 4u ? 4u : u); break;   // 1.1.0: 2 torsion k, 3 spark k, 4 the Chladni stir (step 43), 255 medium default
        case P_PRESS_MODE:     p.press_mode = u == 255u ? 255u : (u > 2u ? 2u : u); break;   // 1.1.0: 2 torsion feed, 255 medium default
        case P_WAKE_PROFILE:   p.wake_profile = u ? 1u : 0u; break;
        case P_WAKE_SPREAD:    p.wake_spread = v; break;
        case P_TORSION_SWEEP:  p.torsion_sweep = u ? 1u : 0u; break;
        case P_CHLADNI_CELL:   p.chladni_cell = v < 0.5f ? 0.5f : (v > 1.5f ? 1.5f : v); break;
        case P_BURST_AGE:      p.burst_age = v < 1.5f ? 1.5f : (v > 12.0f ? 12.0f : v); break;
        case P_BURST_LIFE:     p.burst_life = v < 0.0f ? 0.0f : (v > 4.0f ? 4.0f : v); break;
        case P_BURST_ORDER:    p.burst_order = u < 2u ? 2u : (u > 8u ? 8u : u); break;
        case P_SPARK_STACK:    p.spark_stack = u < 1u ? 1u : (u > 4u ? 4u : u); break;
        case P_SPARK_PROFILE:  p.spark_profile = u ? 1u : 0u; break;
        case P_SPARK_SHEAR:    p.spark_shear = v < 0.0f ? 0.0f : (v > 2.0f ? 2.0f : v); break;
        case P_SPARK_TAU:      p.spark_tau = v < 0.05f ? 0.05f : (v > 2.0f ? 2.0f : v); break;
        case P_CHIRIKOV_KMAX:  p.chirikov_kmax = v < 0.0f ? 0.0f : (v > 2.0f ? 2.0f : v); break;
        case P_CHIRIKOV_PERIODS: p.chirikov_periods = u < 1u ? 1u : (u > 8u ? 8u : u); break;
        case P_CHIRIKOV_EPS:   p.chirikov_eps = v < 0.05f ? 0.05f : (v > 1.0f ? 1.0f : v); break;
        case P_MEDIUM:         p.medium = u > 1u ? 1u : u; break;   // 1.0.0: 0 sumi, 1 anod
        case P_ANOD_GLOW:      p.anod_glow = v < 0.2f ? 0.2f : (v > 5.0f ? 5.0f : v); break;
        case P_ANOD_PITCH:     p.anod_pitch = !(v > 0.0f) ? 0.0f : (v < 1.0f / 256.0f ? 1.0f / 256.0f : (v > 1.0f / 8.0f ? 1.0f / 8.0f : v)); break;   // 0 = no grid
        case P_CHLADNI_MODE:   p.chladni_mode = u > 1u ? 1u : u; break;   // 0 discs, 1 the blended field
        case P_PAPER_TINT_R:   p.paper_tint[0] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); break;
        case P_PAPER_TINT_G:   p.paper_tint[1] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); break;
        case P_PAPER_TINT_B:   p.paper_tint[2] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); break;
        case P_FIBER_SCALE:    p.fiber_scale = v < 0.5f ? 0.5f : (v > 2.0f ? 2.0f : v); break;
        case P_ANOD_DARK:      p.anod_dark = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); break;
        case P_ANOD_GRAIN:     p.anod_grain = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); break;
        case P_ANOD_BLOOM:     p.anod_bloom = v < 0.0f ? 0.0f : (v > 3.0f ? 3.0f : v); break;
        case P_ANOD_BLOOM_LEVELS: p.anod_bloom_levels = u < 1u ? 1u : (u > 5u ? 5u : u); break;
        case P_ANOD_DROP:      p.anod_drop = v < 0.1f ? 0.1f : (v > 1.0f ? 1.0f : v); break;
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
    if (!sumi_layout_probe(p.pitch_layout, &p, aspect, nullptr, x, y, &c)) return 0;
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
