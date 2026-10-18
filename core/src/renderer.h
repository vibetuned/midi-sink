// Internal renderer interface. Deliberately sokol-free: everything above
// renderer.cpp / swapchain_*.{mm,cpp} must stay ignorant of sokol (working rule 2).
#pragma once

#include "sumi_core.h"
#include "displacement.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sumi_renderer_t sumi_renderer_t;
// 1.7.0 (step 71b): the deformation passes run since creation — the bench's gate 9 (a habit is a profile, not a pass)
uint32_t sumi_renderer_pass_count(const sumi_renderer_t* r);

sumi_renderer_t* sumi_renderer_create (const sumi_config_t* config, float sim_scale);
void             sumi_renderer_destroy(sumi_renderer_t* r);
void             sumi_renderer_resize (sumi_renderer_t* r, uint32_t w, uint32_t h, float pixel_ratio);
// Recreates the simulation targets when the scale actually changed.
void             sumi_renderer_set_sim_scale(sumi_renderer_t* r, float sim_scale);

// step 43: the layout's display cells for the SUMI_DEFORM_CELLS pass — four
// floats each (centre x, centre y normalized, radius in canvas heights, kind:
// bit 0 accidental, bit 1 odd); at most 320. The renderer rasterizes them into
// an index map at the field's resolution (rebuilt on resize) so the pass
// knows which disc a texel lies in. Count 0 clears them (the pass is then a
// passthrough).
void             sumi_renderer_set_cells(sumi_renderer_t* r, const float* cells_xyrk, uint32_t count);



// Live composite parameters (§4.5), passed each frame.
typedef struct {

    float    roughness;       // washi fiber/grain strength 0..1
    float    palette_morph;   // 0..1 blend toward the next palette
    // §4.5 live ripple (v0.4): view-only ink-coordinate displacement. 0 amp
    // = off (bake mode, or no ripple). The print path always renders with
    // amp 0 — the dip samples the un-rippled field.
    float    ripple_amp;      // A, canvas-height units
    float    ripple_k;        // wavenumber, radians per canvas-height unit
    float    ripple_phase;    // φ
    float    ripple_angle;    // ripple frame rotation, radians
    // 1.0.0 (Phase 6 step 41): the custom palette (active when palette_id == 3)
    // 1.1.0 (step 43): the palette ring's two slots, both in the one model (stops past the count repeat the last)
    float    pal_a_stops[8][4];   // linear RGB + position, ascending
    float    pal_a_params[4];     // stop count, depth gamma, depth floor, hue drift
    float    pal_a_accent[4];     // the drift's target
    float    pal_a_clear[4];      // the clear-water band tone
    float    pal_b_stops[8][4];
    float    pal_b_params[4];
    float    pal_b_accent[4];
    float    pal_b_clear[4];
    uint32_t medium;              // 1.1.0: 0 sumi, 1 anod — the composite's branch
    float    anod_glow;           // 1.1.0: the strain-glow scale
    float    anod_pitch;          // 1.1.0: the water grid's pitch at rest, canvas heights (0 = no grid)
    float    paper_tint[4];       // 1.1.0 (step 43, QOL §2): the washi's base tone, linear RGB
    float    fiber_scale;         //   the strands' frequency multiple
    float    anod_dark;           //   the glass's darkness
    float    anod_grain;          //   the speckle's strength
    float    anod_bloom;          // 1.1.0 (step 43): the Anod bloom's strength (0 = off, the composite as it stands)
    uint32_t anod_bloom_levels;   //   its reach in octaves, 1..5
    float    dbg_lattice;         // dev only: the Chladni plate guide's strength (0 = off)
    uint32_t dbg_cell_count;      //   the layout's display cells (0 = none)
    float    dbg_cells[320][4];   //   centre x, centre y (normalized), radius (canvas heights), kind (bit 0 accidental, bit 1 odd)
    uint32_t scope_mode;          // 1.3.0 (Phase 8 step 59): the scope view — 0 off, 1 the polylines over the medium, 2 instead of it (the live path only)
    uint32_t scope_count;         //   segments that follow (≤ 128)
    float    scope_seg[128][4];   //   x0, y0, x1, y1 (normalized canvas coordinates)
    // 1.6.0 (Phase 11 step 71, KESSHO §1): the Kessho medium — its knobs, the bed's depth, the analyzer and the LUT
    float    kessho_retard;       // Layer A's retardation at unit strain, nm
    float    kessho_backlight;    // the lamp behind the crystal
    float    kessho_grain;        // the frosted facets' speckle
    float    kessho_tint;         // Layer B's regime tint
    float    kessho_caustic;      // Layer B's caustic gain
    float    caustic_z;           // the bed's depth z (1.05..8; 1 = the flat plane of --kessho-flat)
    uint32_t polarizer_mode;      // 0 the global analyzer, 1 per voice (the A/B item K-1)
    uint32_t pol_count;           // per-voice polarizers that follow (0 = the global analyzer alone)
    float    pol_global[2];       // cos 4α, sin 4α of the global analyzer
    float    pol_voices[16][4];   // x, y (normalized), cos 4α_k, sin 4α_k
    uint32_t kessho_lut_version;  // the engine's LUT generation — the renderer uploads when it moves (0 = none)
    uint16_t kessho_lut[1024 * 4];// the LUT as RGBA16F texels: retardation 0..KESSHO_LUT_NM -> linear colour (the chart, or the palette read in orders)
    // 1.7.0 (Phase 11 step 71b, KESSHO §4): the mineral's optical half, as the composite reads it
    uint32_t mineral_optic;       // SUMI_OPTIC_*
    float    mineral_retard;      // Γ_eff at σ = 1, nm — the mineral's retardation under the exposure knob
    float    mineral_bias;        // Γ₀, nm (the sensitive-tint plate)
    float    mineral_two_v;       // V, rad (biaxial)
    float    mineral_walkoff;     // δ, texels at σ = 1
    float    mineral_dispersion;  // (z_B / z_R) − 1
    float    mineral_film_n, mineral_film_r;
    float    mineral_conical;     // the ring kernel's radius, texels
    float    mineral_body[4];     // the body tint, linear RGB
    float    mineral_bloom_gain;  // Layer B's bloom gain
    int32_t  tine_override;       // dev only (sumi_debug): −1 = the records' profiles; ≥ 0 forces one (3 = the unwindowed fault, the red control)
    // 1.11.0 (Phase 12 step 76): the spark reading and the strain buffer's clock
    uint32_t anod_glow_mode;      // 0 plasma (bitwise 1.10.0), 1 the spark
    float    spark_afterglow;     // seconds: the buffer's slow time constant (the afterglow; Kessho's exposure)
    float    spark_time;          // seconds: the flicker's clock (the engine's)
    uint32_t spark_nms;           // dev: 1 the ridges (shipped), 0 the suppression off (the red control: the plateau)
} sumi_render_visuals_t;

// Drains the deformation queue as ping-pong passes, then composites the
// current field to the swapchain. A RESET pass first snapshots the composite
// into the print target and schedules the async readback (§5.3 paper dip).
// Does not clear the queue.
void             sumi_renderer_render (sumi_renderer_t* r, const sumi_deform_queue_t* deforms,
                                       double dt, const sumi_render_visuals_t* visuals);

// step 43 (QOL §4): the visuals for the next composite (render() sets them per
// frame; an export sets them first so it sees the params as they stand), and
// the EXPORT — the composite of a field (a snapshot's data, or NULL for the
// field as it stands) into an RGBA8 target of w×h, read back asynchronously
// on the swapchain's one readback slot (refused while a print or field
// readback is in flight; a dip is refused while an export is).
void             sumi_renderer_set_visuals(sumi_renderer_t* r, const sumi_render_visuals_t* visuals);
// 1.11.0 (step 76), test-only: the strain buffer as it stands (RGBA16F: σ, the fast activity, the slow activity, the exposure),
// the field's size; out NULL queries the size. May block (bounded), like the field's read.
bool             sumi_renderer_read_strain(sumi_renderer_t* r, uint8_t* out_rgba16f, size_t capacity, uint32_t* out_w, uint32_t* out_h);
bool             sumi_renderer_export_begin(sumi_renderer_t* r, const uint8_t* field_rgba16f, uint32_t fw, uint32_t fh,
                                            uint32_t w, uint32_t h, uint32_t flags);
int              sumi_renderer_export_poll(sumi_renderer_t* r, uint8_t* out_rgba8, size_t capacity,
                                           uint32_t* out_w, uint32_t* out_h);

// True when a paper dip can be accepted: a free print buffer exists and no
// readback is in flight (spec §5.3 double-buffer contract).
bool             sumi_renderer_dip_ready(const sumi_renderer_t* r);

// Newest completed paper-dip print (RGBA8, tightly packed, sRGB-encoded).
// pixels == NULL: query size only (does not consume). A successful data read
// consumes the buffer (frees it for the next dip). False if no print is ready.
bool             sumi_renderer_read_print(sumi_renderer_t* r, uint8_t* pixels, size_t capacity,
                                          uint32_t* out_w, uint32_t* out_h);

// §4.6 cross-backend regression support: synchronous readback of the current
// field texture as raw RGBA16F (w*h*8 bytes, tightly packed, row 0 = top).
// out_rgba16f == NULL queries the size. Test-only — may block (bounded).
bool             sumi_renderer_read_field_begin(sumi_renderer_t* r);
int              sumi_renderer_read_field_poll(sumi_renderer_t* r, uint8_t* out_rgba16f, size_t capacity,
                                               uint32_t* out_w, uint32_t* out_h);
bool             sumi_renderer_read_field(sumi_renderer_t* r, uint8_t* out_rgba16f, size_t capacity,
                                          uint32_t* out_w, uint32_t* out_h);

#ifdef __cplusplus
}
#endif
