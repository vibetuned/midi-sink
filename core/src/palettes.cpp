// palettes.cpp — Phase 6 step 43 (QOL §1): the preset library. The three
// built-ins of each medium are the 0.x / step-42 tables VERBATIM (the same
// float literals the shader carried), as two-stop palettes of one colour
// with the medium's hue drift of 0.45 toward the accent — so the one
// composite path renders them bitwise (the `--palette-test` hashes). From
// index 3: colour-blind-considerate presets — the Okabe–Ito cobalt/amber
// pair (safe for deuteranopia and protanopia) and the viridis / cividis
// perceptual ramps as depth (Sumi) or glow (Anod) gradients — a PROPOSAL the
// author signs by eye (DECISIONS_5 #6x).
#include "palettes.h"

#include <math.h>
#include <string.h>

typedef struct {
    const char* name;
    uint32_t    n;
    float       stops[4][4];   // linear rgb + position, ascending (n <= 4 here)
    float       gamma, floor_, drift;
    float       accent[3];
    float       clear_[3];
} preset_t;

static const preset_t SUMI_PRESETS[] = {
    // the 0.x built-ins: one colour along the depth axis, the hue drift toward the accent
    {"Sumi black",  2, {{0.012f, 0.011f, 0.013f, 0.0f}, {0.012f, 0.011f, 0.013f, 1.0f}}, 1.0f, 0.0f, 0.45f, {0.055f, 0.042f, 0.034f}, {0.830f, 0.815f, 0.760f}},
    {"Indigo",      2, {{0.015f, 0.035f, 0.170f, 0.0f}, {0.015f, 0.035f, 0.170f, 1.0f}}, 1.0f, 0.0f, 0.45f, {0.020f, 0.110f, 0.150f}, {0.780f, 0.800f, 0.830f}},
    {"Ochre",       2, {{0.430f, 0.185f, 0.022f, 0.0f}, {0.430f, 0.185f, 0.022f, 1.0f}}, 1.0f, 0.0f, 0.45f, {0.300f, 0.060f, 0.015f}, {0.840f, 0.780f, 0.660f}},
    // colour-blind-considerate (proposal): the Okabe–Ito blue/orange pair as ink and drift
    {"Cobalt & amber", 2, {{0.000f, 0.165f, 0.445f, 0.0f}, {0.000f, 0.165f, 0.445f, 1.0f}}, 1.0f, 0.0f, 0.45f, {0.800f, 0.350f, 0.000f}, {0.800f, 0.820f, 0.860f}},
    // perceptual ramps along the depth axis: thin ink bright, pooled ink dark
    {"Viridis ink", 3, {{0.980f, 0.810f, 0.020f, 0.0f}, {0.016f, 0.280f, 0.260f, 0.5f}, {0.054f, 0.000f, 0.089f, 1.0f}}, 1.0f, 0.0f, 0.20f, {0.016f, 0.280f, 0.260f}, {0.850f, 0.850f, 0.800f}},
    {"Cividis ink", 3, {{0.990f, 0.820f, 0.060f, 0.0f}, {0.210f, 0.210f, 0.200f, 0.5f}, {0.000f, 0.015f, 0.075f, 1.0f}}, 1.0f, 0.0f, 0.20f, {0.210f, 0.210f, 0.200f}, {0.850f, 0.850f, 0.820f}},
};
static const preset_t ANOD_PRESETS[] = {
    // the step-42 built-ins: the core as the one colour, the halo as the accent
    {"Electric blue",  2, {{0.30f, 0.42f, 1.00f, 0.0f}, {0.30f, 0.42f, 1.00f, 1.0f}}, 1.0f, 0.0f, 0.45f, {0.62f, 0.30f, 1.00f}, {0.010f, 0.010f, 0.014f}},
    {"Plasma orange",  2, {{1.00f, 0.40f, 0.08f, 0.0f}, {1.00f, 0.40f, 0.08f, 1.0f}}, 1.0f, 0.0f, 0.45f, {1.00f, 0.82f, 0.30f}, {0.010f, 0.010f, 0.014f}},
    {"Phosphor green", 2, {{0.22f, 1.00f, 0.34f, 0.0f}, {0.22f, 1.00f, 0.34f, 1.0f}}, 1.0f, 0.0f, 0.45f, {0.72f, 1.00f, 0.50f}, {0.010f, 0.010f, 0.014f}},
    // colour-blind-considerate (proposal)
    {"Cobalt & amber", 2, {{0.10f, 0.45f, 1.00f, 0.0f}, {0.10f, 0.45f, 1.00f, 1.0f}}, 1.0f, 0.0f, 0.45f, {1.00f, 0.62f, 0.00f}, {0.010f, 0.010f, 0.014f}},
    // perceptual ramps along the glow: a dim charge violet, a burning one yellow
    {"Viridis glow", 3, {{0.054f, 0.000f, 0.089f, 0.0f}, {0.016f, 0.280f, 0.260f, 0.5f}, {0.980f, 0.810f, 0.020f, 1.0f}}, 1.0f, 0.0f, 0.30f, {0.800f, 0.900f, 0.500f}, {0.010f, 0.010f, 0.014f}},
    {"Cividis glow", 3, {{0.000f, 0.015f, 0.075f, 0.0f}, {0.210f, 0.210f, 0.200f, 0.5f}, {0.990f, 0.820f, 0.060f, 1.0f}}, 1.0f, 0.0f, 0.30f, {0.900f, 0.850f, 0.500f}, {0.010f, 0.010f, 0.014f}},
};
// 1.6.0 (Phase 11 step 71, KESSHO §1.2; DECISIONS_10 #3): THE KESSHO PALETTES — twelve, in the one model.
// Index 0 is the PHYSICAL Michel-Lévy chart: the composite's LUT for it is the spectral table
// (core/src/kessho_lut.h, tools/kessho_lut.py) and its stops are not read — only its accent (the
// caustic's light); the clear tone is carried for the model's sake (the calcite under the crossed
// polarizers is the composite's own fixed tone, as Anod's glass is). The eleven
// stylised ones are gradients read PERIODICALLY in the retardation's order (Γσ / 550 nm: the
// first order runs the gradient once, the next orders repeat it, fading toward their own grey as
// the chart's colours fade), the first 200 nm darkened so a resting crystal is dark under every
// palette — which is how a user's custom palette reads too. The depth curve is the gradient's
// position curve within an order; the hue drift is unused (Kessho reads no aux).
static const preset_t KESSHO_PRESETS[] = {
    {"Michel-Lévy",     2, {{0.000f, 0.000f, 0.000f, 0.0f}, {1.000f, 1.000f, 1.000f, 1.0f}}, 1.0f, 0.0f, 0.0f, {0.950f, 0.970f, 1.000f}, {0.012f, 0.013f, 0.018f}},
    {"Smoked quartz",   3, {{0.020f, 0.015f, 0.010f, 0.0f}, {0.550f, 0.420f, 0.300f, 0.5f}, {0.850f, 0.720f, 0.550f, 1.0f}}, 1.0f, 0.0f, 0.0f, {0.900f, 0.800f, 0.600f}, {0.010f, 0.009f, 0.008f}},
    {"Calcite",         3, {{0.020f, 0.030f, 0.050f, 0.0f}, {0.600f, 0.720f, 0.900f, 0.5f}, {0.920f, 0.960f, 1.000f, 1.0f}}, 1.0f, 0.0f, 0.0f, {0.900f, 0.950f, 1.000f}, {0.012f, 0.013f, 0.018f}},
    {"Nematic",         4, {{0.000f, 0.050f, 0.100f, 0.0f}, {0.000f, 0.800f, 0.900f, 0.33f}, {0.900f, 0.100f, 0.800f, 0.66f}, {1.000f, 0.900f, 0.200f, 1.0f}}, 1.0f, 0.0f, 0.0f, {1.000f, 1.000f, 1.000f}, {0.008f, 0.010f, 0.014f}},
    {"Amber",           3, {{0.050f, 0.020f, 0.000f, 0.0f}, {0.900f, 0.500f, 0.050f, 0.5f}, {1.000f, 0.850f, 0.400f, 1.0f}}, 1.0f, 0.0f, 0.0f, {1.000f, 0.900f, 0.600f}, {0.014f, 0.010f, 0.006f}},
    {"Tourmaline",      3, {{0.020f, 0.050f, 0.020f, 0.0f}, {0.100f, 0.700f, 0.300f, 0.5f}, {0.950f, 0.500f, 0.700f, 1.0f}}, 1.0f, 0.0f, 0.0f, {0.950f, 0.850f, 0.900f}, {0.010f, 0.014f, 0.010f}},
    {"Olivine",         3, {{0.020f, 0.040f, 0.010f, 0.0f}, {0.350f, 0.600f, 0.100f, 0.5f}, {0.850f, 0.950f, 0.400f, 1.0f}}, 1.0f, 0.0f, 0.0f, {0.900f, 1.000f, 0.700f}, {0.010f, 0.014f, 0.008f}},
    {"Labradorite",     3, {{0.000f, 0.020f, 0.060f, 0.0f}, {0.050f, 0.300f, 0.800f, 0.4f}, {0.900f, 0.750f, 0.200f, 1.0f}}, 1.0f, 0.0f, 0.0f, {0.800f, 0.850f, 1.000f}, {0.008f, 0.010f, 0.016f}},
    {"Fluorite",        3, {{0.040f, 0.000f, 0.060f, 0.0f}, {0.600f, 0.200f, 0.900f, 0.5f}, {0.300f, 0.900f, 0.500f, 1.0f}}, 1.0f, 0.0f, 0.0f, {0.850f, 0.800f, 1.000f}, {0.012f, 0.008f, 0.016f}},
    // colour-blind-considerate (the Okabe–Ito pair) and the perceptual ramps, as the other media carry them
    {"Cobalt & amber",  3, {{0.000f, 0.050f, 0.200f, 0.0f}, {0.000f, 0.165f, 0.445f, 0.4f}, {0.800f, 0.350f, 0.000f, 1.0f}}, 1.0f, 0.0f, 0.0f, {1.000f, 0.620f, 0.000f}, {0.010f, 0.010f, 0.014f}},
    {"Viridis",         3, {{0.054f, 0.000f, 0.089f, 0.0f}, {0.016f, 0.280f, 0.260f, 0.5f}, {0.980f, 0.810f, 0.020f, 1.0f}}, 1.0f, 0.0f, 0.0f, {0.800f, 0.900f, 0.500f}, {0.010f, 0.010f, 0.014f}},
    {"Cividis",         3, {{0.000f, 0.015f, 0.075f, 0.0f}, {0.210f, 0.210f, 0.200f, 0.5f}, {0.990f, 0.820f, 0.060f, 1.0f}}, 1.0f, 0.0f, 0.0f, {0.900f, 0.850f, 0.500f}, {0.010f, 0.010f, 0.014f}},
};

extern "C" {

uint32_t sumi_palette_preset_count(uint32_t medium) {
    if (medium == SUMI_MEDIUM_SUMI) return (uint32_t)(sizeof(SUMI_PRESETS) / sizeof(SUMI_PRESETS[0]));
    if (medium == SUMI_MEDIUM_ANOD) return (uint32_t)(sizeof(ANOD_PRESETS) / sizeof(ANOD_PRESETS[0]));
    // 1.7.0 (step 71b, the minerals delta): Kessho carries NO palette library — a mineral is its palette
    // (sumi_mineral_preset); the 1.6.0 gradients are retired, the custom slot reads as a chart through lut_id 1
    return 0u;
}

bool sumi_palette_preset(uint32_t medium, uint32_t index, sumi_palette_t* out, const char** name) {
    const preset_t* table = medium == SUMI_MEDIUM_SUMI ? SUMI_PRESETS : medium == SUMI_MEDIUM_ANOD ? ANOD_PRESETS : NULL;   // 1.7.0: Kessho has minerals, not palettes
    const uint32_t count = sumi_palette_preset_count(medium);
    if (!table || index >= count) { if (name) *name = NULL; return false; }
    const preset_t* p = &table[index];
    if (name) *name = p->name;
    if (out) {
        memset(out, 0, sizeof(*out));
        out->stop_count = p->n;
        for (uint32_t i = 0; i < SUMI_PALETTE_MAX_STOPS; i++) {
            const uint32_t k = i < p->n ? i : p->n - 1u;                  // past the count: the last stop repeated
            out->stops[i].rgb[0] = p->stops[k][0]; out->stops[i].rgb[1] = p->stops[k][1]; out->stops[i].rgb[2] = p->stops[k][2];
            out->stops[i].position = p->stops[k][3];
        }
        out->depth_gamma = p->gamma; out->depth_floor = p->floor_; out->hue_drift = p->drift;
        for (int c = 0; c < 3; c++) { out->accent_rgb[c] = p->accent[c]; out->clear_rgb[c] = p->clear_[c]; }
    }
    return true;
}

void sumi_palette_ring(uint32_t active_id, float morph, uint32_t* id_a, uint32_t* id_b, float* m) {
    float mo = morph;
    if (!(mo >= 0.0f)) mo = 0.0f;          // NaN too
    if (mo > 1.0f) mo = 1.0f;
    if (active_id >= SUMI_PALETTE_CUSTOM) {
        // custom → 0 → 1 → 2
        static const uint32_t ring[4] = {SUMI_PALETTE_CUSTOM, 0u, 1u, 2u};
        const float t = mo * 3.0f;
        int seg = (int)fminf(floorf(t), 2.0f);
        if (id_a) *id_a = ring[seg];
        if (id_b) *id_b = ring[(seg + 1) & 3];
        if (m) *m = t - (float)seg;
        return;
    }
    // the shader's own arithmetic since 0.x, in the same float operations
    const float t = mo * 2.0f;
    const int seg = (int)fminf(floorf(t), 1.0f);
    const uint32_t base = active_id > 2u ? 2u : active_id;
    const uint32_t a = (base + (uint32_t)seg) % 3u;
    if (id_a) *id_a = a;
    if (id_b) *id_b = (a + 1u) % 3u;
    if (m) *m = t - (float)seg;
}

}   // extern "C"
