// minerals.cpp — Phase 11 step 71b (KESSHO §4.6; DECISIONS_10 #6): THE SIX
// MINERALS. The ratios between the retardations are physical (Δn: quartz
// 0.009, calcite 0.172, muscovite 0.040, barite 0.012, fluorite 0, selenite
// 0.010); the common EXPOSURE that maps them onto the chart — 2000 nm at
// σ = 1 for quartz, step 71's first capture — is the one `[ITERATE]` number
// (KESSHO §4.6: calcite sits in high-order white by physics, thick spar
// does). The bodies, the walk-off, the film and the ring radius are set by
// eye at the fold and are the author's to sign. The mechanical pairing (the
// calcite rhomb and twin kink, the mica hexagon and terrace, the barite
// rosette) is the shells' and the JSON presets'. Pure data, instance-free.
#include "minerals.h"

#include <math.h>
#include <string.h>

typedef struct {
    const char* name;
    sumi_mineral_t m;
} mineral_preset_t;

#define EXPOSURE (2000.0f / 0.009f)   // nm at σ = 1 per unit Δn: quartz lands at 2000 nm

static const mineral_preset_t PRESETS[SUMI_MINERAL_COUNT] = {
    // id 0 — QUARTZ: the reference, uniaxial, the medium as first captured
    {"Quartz",    {sizeof(sumi_mineral_t), SUMI_OPTIC_UNIAXIAL,  0.009f * EXPOSURE,    0.0f, 0.0f,   0.0f, 0.0f,  0.0f,  0.0f,  0.0f, {1.000f, 1.000f, 1.000f}, 1.0f, 0u}},
    // id 1 — CALCITE: Iceland spar — uniaxial, the walk-off double image, the cleavage rhomb, the twin-kink tine
    {"Calcite",   {sizeof(sumi_mineral_t), SUMI_OPTIC_UNIAXIAL,  0.172f * EXPOSURE,    0.0f, 0.0f,   6.0f, 0.0f,  0.0f,  0.0f,  0.0f, {0.920f, 0.960f, 1.000f}, 1.0f, 0u}},
    // id 2 — MUSCOVITE: mica — biaxial (2V 40°), the thin-film pearlescence, the hexagonal sheet, the terrace slip
    {"Muscovite", {sizeof(sumi_mineral_t), SUMI_OPTIC_BIAXIAL,   0.040f * EXPOSURE,    0.0f, 0.349f, 0.0f, 0.0f,  1.58f, 0.25f, 0.0f, {1.000f, 0.930f, 0.780f}, 1.0f, 0u}},
    // id 3 — BARITE: baryte — biaxial (2V 37°), the dispersion rainbow and Hamilton's ring on the fold, the desert rose
    {"Barite",    {sizeof(sumi_mineral_t), SUMI_OPTIC_BIAXIAL,   0.012f * EXPOSURE,    0.0f, 0.323f, 0.0f, 0.3f,  0.0f,  0.0f,  3.0f, {1.000f, 0.820f, 0.500f}, 1.0f, 0u}},
    // id 4 — FLUORITE: isotropic — Layer A dark, Layer B alone: a diagnostic view as much as a mineral
    {"Fluorite",  {sizeof(sumi_mineral_t), SUMI_OPTIC_ISOTROPIC, 0.0f,                 0.0f, 0.0f,   0.0f, 0.0f,  0.0f,  0.0f,  0.0f, {0.720f, 0.700f, 0.950f}, 1.0f, 0u}},
    // id 5 — SELENITE: the sensitive-tint plate — Γ₀ = 550 nm, the sign of strain made visible
    {"Selenite",  {sizeof(sumi_mineral_t), SUMI_OPTIC_UNIAXIAL,  0.010f * EXPOSURE,  550.0f, 0.0f,   0.0f, 0.0f,  0.0f,  0.0f,  0.0f, {1.000f, 1.000f, 1.000f}, 1.0f, 0u}},
};

static float clampf(float v, float lo, float hi, float dflt) {
    if (!(v == v)) return dflt;   // NaN
    return v < lo ? lo : (v > hi ? hi : v);
}

extern "C" {

bool sumi_mineral_preset(uint32_t id, sumi_mineral_t* out, const char** name) {
    if (id >= SUMI_MINERAL_COUNT) { if (name) *name = NULL; return false; }
    if (name) *name = PRESETS[id].name;
    if (out) { *out = PRESETS[id].m; out->size = (uint32_t)sizeof(sumi_mineral_t); }
    return true;
}

void sumi_mineral_validate(sumi_mineral_t* m) {
    if (!m) return;
    // the size says how much of the struct the host filled: an older, shorter one keeps its prefix, the
    // rest comes from Quartz; a longer one (a newer host) is read up to what this version knows
    sumi_mineral_t q = PRESETS[SUMI_MINERAL_QUARTZ].m;
    const uint32_t have = m->size;
    if (have < sizeof(sumi_mineral_t)) {
        unsigned char tmp[sizeof(sumi_mineral_t)];
        memcpy(tmp, &q, sizeof q);
        const uint32_t keep = have > sizeof(uint32_t) ? have : sizeof(uint32_t);
        memcpy(tmp, m, keep);
        memcpy(m, tmp, sizeof tmp);
    }
    m->size = (uint32_t)sizeof(sumi_mineral_t);
    if (m->optic_class > SUMI_OPTIC_BIAXIAL) m->optic_class = SUMI_OPTIC_UNIAXIAL;
    m->retardation = clampf(m->retardation, 0.0f, 200000.0f, q.retardation);
    m->retardation_bias = clampf(m->retardation_bias, 0.0f, 2200.0f, 0.0f);
    m->two_v = clampf(m->two_v, 0.0f, 1.5707963f, 0.35f);
    m->walkoff = clampf(m->walkoff, 0.0f, 32.0f, 0.0f);
    m->dispersion = clampf(m->dispersion, 0.0f, 1.0f, 0.0f);
    m->film_n = clampf(m->film_n, 1.0f, 3.0f, 1.58f);
    m->film_r = clampf(m->film_r, 0.0f, 0.95f, 0.0f);
    m->conical_ring = clampf(m->conical_ring, 0.0f, 16.0f, 0.0f);
    for (int c = 0; c < 3; c++) m->body_tint[c] = clampf(m->body_tint[c], 0.0f, 1.0f, 1.0f);
    m->bloom_gain = clampf(m->bloom_gain, 0.0f, 4.0f, 1.0f);
    if (m->lut_id > 1u) m->lut_id = 1u;
}

}   // extern "C"
