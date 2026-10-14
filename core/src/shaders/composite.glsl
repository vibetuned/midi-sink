// composite.glsl — displacement field -> swapchain (PROJECT_SPEC.md §4.5).
// Ink phase -> alternating rings; procedural simplex washi mulberry-fiber
// noise + absorption grain (strength = paper_roughness); three palettes
// (sumi black / indigo / ochre) morphable via palette_morph, with per-drop
// hue offsets from the aux channel. All color math in LINEAR space; the
// final write is sRGB-encoded (§4.5, see DECISIONS.md).

// Two vertex shaders, identical except for the GLSL-only flip_vert_y option
// (§4.6 — this is THE backend orientation boundary; MSL/HLSL outputs of both
// are identical, so Metal/D3D11 behavior is unchanged):
//   composite_vs       — final on-screen pass. No flip: GL presents the
//                        default framebuffer bottom-up, which is itself the
//                        §4.6 flip, so the raster must stay bottom-up.
//   composite_print_vs — offscreen print target. Flipped like every other
//                        offscreen pass, so print memory is top-left-origin
//                        and the readback copies rows straight on all
//                        backends.
@vs composite_vs
out vec2 st;   // texture-space coordinate of this fragment's texel (v grows down)
void main() {
    vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
    st = vec2(corner.x, 1.0 - corner.y);
}
@end

@vs composite_print_vs
@glsl_options flip_vert_y
out vec2 st;   // texture-space coordinate of this fragment's texel (v grows down)
void main() {
    vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
    st = vec2(corner.x, 1.0 - corner.y);
}
@end

@fs composite_fs
layout(binding=0) uniform texture2D tex_field;
layout(binding=0) uniform sampler smp_field;
layout(binding=1) uniform texture2D tex_bloom;   // 1.1.0 (step 43): the Anod bloom (bloom.glsl), read when bloom_in > 0
layout(binding=1) uniform sampler smp_bloom;
layout(binding=2) uniform texture2D tex_lut;     // 1.6.0 (step 71): the Kessho LUT — retardation 0..KESSHO_NM -> linear colour (1024x1), read under medium 2
layout(binding=2) uniform sampler smp_lut;
layout(binding=0) uniform composite_params {
    float aspect;         // field W/H: fibers in isotropic space
    float roughness;      // washi fiber/grain strength (0..1)
    float palette_morph;  // 1.1.0 (step 43): the blend between the ring's two slots below (the engine resolves the ring)
    float alpha_out;      // 1.1.0 (step 43, QOL §4): 1 = an Anod EXPORT over alpha — the glow and the grid as straight colour, the glass transparent
    float dip_fade;       // 1 right after a paper dip -> 0 ("lift the paper")
    float texel_y;        // 1 / field height (edge-proximity sampling)
    float ripple_amp;     // §4.5 live ripple (v0.4): 0 = off (bake mode, or
    float ripple_k;       //   the print path — the dip samples UN-rippled)
    float ripple_phase;
    float ripple_ca;      // cos(ripple angle)
    float ripple_sa;      // sin(ripple angle)
    vec4  pa_stops[8];    // 1.1.0 (step 43): palette slot A — linear RGB + position, ascending, the last repeated past the count
    vec4  pa_params;      //   stop count, depth gamma, depth floor, hue drift
    vec4  pa_accent;      //   the drift's target
    vec4  pa_clear;       //   the clear-water band tone
    vec4  pb_stops[8];    // slot B, the next on the ring
    vec4  pb_params;
    vec4  pb_accent;
    vec4  pb_clear;
    float medium;         // 1.1.0: 0 sumi (the path below, bitwise 1.0.0), 1 anod (strain-glow)
    float anod_glow;      //   the strain-glow scale
    float anod_pitch;     //   the water grid's pitch at rest, canvas heights (0 = no grid)
    vec4  paper_tint;     // 1.1.0 (step 43, QOL §2): the washi's base tone, linear RGB (0.x's cream by default)
    float fiber_scale;    //   the strands' frequency multiple (1 = 0.x)
    float anod_dark;      //   the glass's darkness (0.5 = the step-42 glass)
    float anod_grain;     //   the speckle's strength (0.5 = the step-42 speckle)
    float pad_sub;
    float linear_out;     // 1.1.0 (step 43, the glow): 1 = write the Anod composite LINEAR, before the shoulder and the encode (the bloom's source)
    float bloom_in;       //   1 = add tex_bloom (strength below) to the Anod composite, then the shoulder toward white
    float bloom_strength;
    float pad_bloom;
    float dbg_lattice;    // DEV ONLY: the Chladni plate guide's strength (0 = off, the shipped path)
    float dbg_cell_count; //   how many DISPLAY CELLS follow
    vec4  dbg_cells[320]; //   the cells: centre x, centre y (normalized), radius (canvas heights), kind (bit 0 accidental, bit 1 odd)
    float scope_mode;     // 1.3.0 (Phase 8 step 59, SYNTH §2.7): THE SCOPE VIEW — 0 off (the shipped composite), 1 the shell's polylines over the medium, 2 instead of it
    float scope_count;    //   how many SEGMENTS follow (the live path only: the print, the export and the bloom never see them)
    vec4  scope_seg[128]; //   the segments: x0, y0, x1, y1 (normalized canvas coordinates)
    float kessho_retard;  // 1.6.0 (Phase 11 step 71, KESSHO §1): Layer A's retardation at unit strain, nm
    float kessho_backlight; //   the lamp behind the crystal
    float kessho_grain;   //   the frosted facets' speckle
    float kessho_tint;    //   Layer B's regime tint
    float kessho_caustic; //   Layer B's caustic gain
    float caustic_z;      //   the bed's depth z (1.05..8; 1 = the flat plane of the dev gate)
    float caustic_only;   //   1 = an export of Layer B alone over alpha (SUMI_EXPORT_KESSHO_CAUSTIC)
    float pol_count;      //   per-voice polarizers that follow (0 = the global analyzer alone)
    vec4  pol_global;     //   cos 4α, sin 4α of the global analyzer, 0, 0
    vec4  pol_voices[16]; //   x, y (normalized), cos 4α_k, sin 4α_k — the held voices (polarizer_mode 1)
};
in vec2 st;
out vec4 frag_color;

// ---- 2D simplex noise (Ian McEwan / Ashima Arts style) ----
vec3 permute(vec3 x) { return mod(((x * 34.0) + 1.0) * x, 289.0); }
float snoise(vec2 v) {
    const vec4 C = vec4(0.211324865405187, 0.366025403784439,
                        -0.577350269189626, 0.024390243902439);
    vec2 i = floor(v + dot(v, C.yy));
    vec2 x0 = v - i + dot(i, C.xx);
    vec2 i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec4 x12 = x0.xyxy + C.xxzz;
    x12.xy -= i1;
    i = mod(i, 289.0);
    vec3 p = permute(permute(i.y + vec3(0.0, i1.y, 1.0)) + i.x + vec3(0.0, i1.x, 1.0));
    vec3 m = max(0.5 - vec3(dot(x0, x0), dot(x12.xy, x12.xy), dot(x12.zw, x12.zw)), 0.0);
    m = m * m;
    m = m * m;
    vec3 x = 2.0 * fract(p * C.www) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;
    m *= 1.79284291400159 - 0.85373472095314 * (a0 * a0 + h * h);
    vec3 g;
    g.x = a0.x * x0.x + h.x * x0.y;
    g.yz = a0.yz * x12.xz + h.yz * x12.yw;
    return 130.0 * dot(m, g);
}

// Palette table (LINEAR RGB). Paper is shared across palettes.
// 1.1.0 (Phase 6 step 43, QOL §1) — THE ONE PALETTE PATH. Every palette — the
// medium's three built-ins, the curated presets, the user's custom slot — is
// the same model: 2..8 stops of linear RGB along the depth axis (Sumi: the
// ink's thickness; Anod: the charge's glow), a depth curve u = floor +
// (1 − floor)·depth^γ, a per-drop hue drift toward an accent colour by
// drift·hue_t (the aux selector), and the clear-water tone. The composite
// blends TWO slots, A and B, by palette_morph — the engine resolves the ring
// (palettes.cpp) — stop-wise and parameter-wise, the arithmetic the 0.x
// per-id tables used: mix(stopA, stopB, m) is what mix(pal_ink(id0),
// pal_ink(id1), m) was, and a built-in is a two-stop palette of one colour,
// so its gradient is that colour exactly and mix(c, accent, 0.45·hue_t) is
// the same operation in the same order — bitwise (the `--palette-test`
// hashes, the composite gate).
float pal_count() { return max(pa_params.x, pb_params.x); }
vec3  pal_accent_m() { return mix(pa_accent.rgb, pb_accent.rgb, palette_morph); }
vec3  pal_clear_m()  { return mix(pa_clear.rgb,  pb_clear.rgb,  palette_morph); }
float pal_drift_m()  { return mix(pa_params.w, pb_params.w, palette_morph); }
vec3 pal_gradient(float depth) {
    float gamma = mix(pa_params.y, pb_params.y, palette_morph), flo = mix(pa_params.z, pb_params.z, palette_morph);
    float u = clamp(flo + (1.0 - flo) * pow(max(depth, 0.0), gamma), 0.0, 1.0);
    int n = int(pal_count() + 0.5);
    vec3 c = mix(pa_stops[0].rgb, pb_stops[0].rgb, palette_morph);
    for (int i = 0; i < 7; i++) {
        if (i + 1 >= n) break;
        vec4 s0 = mix(pa_stops[i], pb_stops[i], palette_morph), s1 = mix(pa_stops[i + 1], pb_stops[i + 1], palette_morph);
        if (u >= s0.w) c = mix(s0.rgb, s1.rgb, clamp((u - s0.w) / max(s1.w - s0.w, 1e-5), 0.0, 1.0));
    }
    return c;
}
// the drop's colour: the gradient at its depth, drifted toward the accent by the drop's aux
vec3 pal_ink_at(float depth, float hue_t) { return mix(pal_gradient(depth), pal_accent_m(), pal_drift_m() * hue_t); }

// 1.1.0 (Phase 6 step 42, MEDIUM §3) — the ANOD composite: the CHARGE glows
// by its strain, the WATER draws the field's grid. The field already carries
// the accumulated map: finite-differencing the stored displacements between
// neighbouring texels (plus the identity, 1.2.0) gives its Jacobian J, and ‖J‖_F² − 2 =
// (λ − 1/λ)² for an area-preserving map with singular values λ, 1/λ — zero
// for the identity and for pure rotation, positive wherever the sheet has
// been STRAINED. Charged material (phase >= 1, the ink re-read) glows like
// ionized gas at a base of ANOD_BASE plus σ = |λ − 1/λ|'s share, g = 1 −
// exp(−σ/anod_glow); the charge phase bands the filament between the
// palette's core and halo, aux drifts the hue per event. Water never glows
// by strain (the estimator is unreliable on a half-float field at screen
// resolution, and the exact shears' bands run the whole canvas): it draws
// the DEFORMED GRID — iso-lines of position + gain·displacement, one family
// per axis — where the displacement exceeds a texel, at a pitch of
// params.anod_pitch canvas heights (0 = no grid). THE SEAM MASK: a charged
// texel beside fresh water (identity coordinates, no phase — a scroll seam,
// a discontinuity, not strain) reads no strain. The substrate is near-black
// with the same screen-locked simplex grain as the washi (§4.5's invariant:
// sampled at st, never through the field).
// THE FIELD'S PRECISION (1.2.0, Phase 8 step 55b): the field stores each
// texel's DISPLACEMENT (u − x, v − y) as half floats, so its quantum follows
// the displacement — 2^-24 canvas heights at rest, 2^-15 at a sixteenth of
// the canvas — where the absolute coordinates of 1.1.0 carried 2^-11 over the
// outer half of the sheet whatever the motion (a full texel of a 2048-wide
// field; differencing an identity field read that rounding as strain — the
// right half of a 2560-wide window glowed in stripes). The estimator below
// rebuilds J from the displacement (J = I + ∇d, the same finite differences
// plus the identity), the widened stencil stays as the print's design —
// 2·round(H/512) texels each side, 2 at 512, 6 at 1440 — and the rounding
// bias it subtracts (the variance of the differences, 3× for its tail) is now
// taken at the displacement's own magnitude, where it is next to nothing.
// The fresh test is a class, not a tolerance: no phase and a displacement
// under half a texel (the ULP term of 1.1.0 is gone — an identity sheet is
// zero exactly at any size).
float anod_ulp(float x) { return exp2(floor(log2(max(x, 1e-6))) - 10.0); }   // half-float spacing at x
bool anod_fresh(vec4 f) {                        // fresh water / never touched: no displacement, no phase
    return f.z < 0.5 && abs(f.x) < 0.5 * texel_y / aspect && abs(f.y) < 0.5 * texel_y;
}
vec3 anod_col(vec4 field, float grain, out vec4 straight) {   // straight: the lit colour and its coverage, for an export over alpha
    float n = 2.0 * max(1.0, floor(1.0 / (texel_y * 512.0) + 0.5));   // stencil half-width, texels: 2 at 512, 6 at 1440
    vec2 tx = vec2(n * texel_y / aspect, 0.0), ty = vec2(0.0, n * texel_y);
    vec4 fpx = texture(sampler2D(tex_field, smp_field), st + tx);
    vec4 fmx = texture(sampler2D(tex_field, smp_field), st - tx);
    vec4 fpy = texture(sampler2D(tex_field, smp_field), st + ty);
    vec4 fmy = texture(sampler2D(tex_field, smp_field), st - ty);
    bool fc = anod_fresh(field);
    bool seam = (anod_fresh(fpx) != fc) || (anod_fresh(fmx) != fc) ||
                (anod_fresh(fpy) != fc) || (anod_fresh(fmy) != fc);
    // THE ESTIMATOR: each entry of J from its two ONE-SIDED differences,
    // keeping the smaller. A half-float field is a staircase — a smooth,
    // small displacement (a burst's far field) advances the stored
    // coordinate by one quantum every few hundred texels, and a central
    // difference straddling such a step reads a ring of false strain
    // (measured: concentric arcs across a 2560-wide canvas). A step, like a
    // scroll seam, has a side with nothing; strain has both sides. The same
    // choice keeps the spark's kinks lit: a central difference across a
    // triangle's peak cancels its two slopes to zero, the smaller of the two
    // one-sided slopes is the slope itself. (Fresh water is never marked in
    // the field — the §4.6 fixture pins the ingress rule's bytes — so seams
    // and steps are found, not stored.) 1.2.0: the field holds d = u − x, so
    // each entry of J is the displacement's difference plus the identity's.
    vec2 scl = vec2(aspect, 1.0) / (n * texel_y);                 // canvas-height units per canvas height
    vec2 gxp = (fpx.xy - field.xy) * scl + vec2(1.0, 0.0), gxm = (field.xy - fmx.xy) * scl + vec2(1.0, 0.0);   // ∂(u,v)/∂x, either side
    vec2 gyp = (fpy.xy - field.xy) * scl + vec2(0.0, 1.0), gym = (field.xy - fmy.xy) * scl + vec2(0.0, 1.0);   // ∂(u,v)/∂y
    float ux = abs(gxp.x) < abs(gxm.x) ? gxp.x : gxm.x, vx = abs(gxp.y) < abs(gxm.y) ? gxp.y : gxm.y;
    float uy = abs(gyp.x) < abs(gym.x) ? gyp.x : gym.x, vy = abs(gyp.y) < abs(gym.y) ? gyp.y : gym.y;
    float F2 = ux * ux + uy * uy + vx * vx + vy * vy;
    // the rounding bias: a one-sided difference carries two uniform errors of ±ULP/2 (variance ULP²/6),
    // scaled as the derivatives are; ‖J‖_F² is biased up by the sum over its four entries — three
    // times that is subtracted for the tail
    float su = anod_ulp(abs(field.x)) * scl.x, sv = anod_ulp(abs(field.y)) * scl.y;   // 1.2.0: at the displacement's magnitude
    float bias = 2.0 * (su * su + sv * sv) / 6.0;
    float sigma = (seam || fc) ? 0.0 : sqrt(max(F2 - 2.0 - 3.0 * bias, 0.0));
    float g = 1.0 - exp(-sigma / anod_glow);
    // the substrate: vacuum glass with a phosphor speckle, screen-locked
    // step 43 (QOL §2): the glass's darkness and the speckle's own strength — at 0.5 / 0.5 the step-42 substrate exactly
    vec3 col = vec3(0.010, 0.010, 0.014) * (2.0 * (1.0 - anod_dark)) * (1.0 + anod_grain * (0.6 * grain - 0.1));
    // the discharge: charged material bands between core and halo, drifts by aux
    float phase = field.z, aux = field.w;
    bool charged = phase >= 1.0;
    float band = mod(floor(max(phase, 1.0)), 2.0);
    float hue_t = fract(aux * 0.6180339887);
    // the palette read as a glow (step 43, the one path): the gradient sampled by strain is the charge's
    // CORE, the accent its HALO, and the charge phase bands the filament between the two — odd bands core
    // drifting toward halo, even bands halo drifting toward core (the step-42 tables, as presets)
    vec3 core = pal_gradient(g), halo = pal_accent_m();
    vec3 a = band >= 1.0 ? core : halo, b = band >= 1.0 ? halo : core;
    vec3 c = mix(a, b, pal_drift_m() * hue_t);
    // THE CHARGE GLOWS; THE GAS GLOWS AROUND IT; THE GLASS IS DARK (the
    // author's calls, 2026-09-22). Charged material carries a base glow —
    // ANOD_BASE of its colour with no strain at all, so a fresh strike is
    // visible as charge — and its strain adds the rest: how hard it burns is
    // how far it has been stretched. Water never glows by strain: at screen
    // resolution the half-float staircase makes the estimator unreliable on
    // small smooth displacements (measured on a 2560-wide canvas: arcs and
    // stripes), and the exact shears' bands run the whole canvas. Water shows
    // the deformed grid instead (below); a strained-water "gas" within reach
    // of the charge was tried and set aside (2026-09-22, the author's call).
    const float ANOD_BASE = 0.22;                                       // a charged texel's glow with no strain at all
    // THE GRID: the displacement gain per family (signed: a negative gain bends
    // a family's lines INTO a drop, a positive one round it), the line
    // brightness, and the local pitch (texels) under which a family fades; the
    // pitch at rest is params.anod_pitch, in canvas heights
    const float ANOD_GRID_GAIN_X = -4.0, ANOD_GRID_GAIN_Y = 3.4, ANOD_GRID = 0.14, ANOD_GRID_ALIAS = 3.0;
    float lit = 0.0;
    if (charged) {
        lit = ANOD_BASE + (1.0 - ANOD_BASE) * g;
    } else if (anod_pitch > 0.0) {
        // THE FAR FIELD, drawn as the deformed grid (the author's call,
        // 2026-09-22: "the far fields that look like a magnetic field"). What
        // the strain estimator lit on a 2560-wide canvas was this grid by
        // accident: the half-float staircase draws the iso-lines of the
        // source coordinates every quantum, the quantum beats against the
        // texel grid (2048 steps per canvas height against 2560 and 1440
        // texels), and the beat is a grid of pitch ~5 texels whose bending is
        // the displacement amplified ×4 — negative along x, positive along y,
        // which is why one family converged on the drop and the other bulged
        // round it — with straight stripes wherever the water rested. Drawn
        // on purpose: iso-lines of (position + gain·displacement), one family
        // per axis, from the displacement averaged over a window along the
        // axis (the staircase's sawtooth averages out to a few percent of a
        // quantum), shown only where the displacement itself exceeds a texel
        // so rest shows nothing, and faded where the local pitch falls under
        // ANOD_GRID_ALIAS texels (a drop's rim, a spark's core) so the grid
        // never aliases. Charged texels are left out of the windows.
        int hw = int(3.0 * max(1.0, floor(1.0 / (texel_y * 512.0) + 0.5)));   // 3 at 512, 6 at 1080, 9 at 1440
        vec2 t1x = vec2(texel_y / aspect, 0.0), t1y = vec2(0.0, texel_y);
        vec2 sx = vec2(0.0), sy = vec2(0.0); float cx = 0.0, cy = 0.0;
        vec2 ex = vec2(0.0), ey = vec2(0.0);                              // window-end displacements, for ∇
        for (int i = -12; i <= 12; i++) {
            if (abs(i) > hw) continue;
            vec2 ax = st + float(i) * t1x, ay = st + float(i) * t1y;
            vec4 fx = texture(sampler2D(tex_field, smp_field), ax);
            vec4 fy = texture(sampler2D(tex_field, smp_field), ay);
            if (fx.z < 1.0 && ax.x >= 0.0 && ax.x <= 1.0) { sx += fx.xy; cx += 1.0; if (i == hw) ex += fx.xy; if (i == -hw) ex -= fx.xy; }   // 1.2.0: the texel's displacement is the payload
            if (fy.z < 1.0 && ay.y >= 0.0 && ay.y <= 1.0) { sy += fy.xy; cy += 1.0; if (i == hw) ey += fy.xy; if (i == -hw) ey -= fy.xy; }
        }
        vec2 sc = vec2(aspect, 1.0) / texel_y;                            // canvas → texels
        vec2 dx = (cx > 0.0 ? sx / cx : vec2(0.0)) * sc;                  // displacement, averaged along x
        vec2 dy = (cy > 0.0 ? sy / cy : vec2(0.0)) * sc;                  // … along y
        ex *= sc / (2.0 * float(hw)); ey *= sc / (2.0 * float(hw));       // ∂d/∂x, ∂d/∂y (texel per texel)
        float m = length(vec2(dx.x, dy.y));                               // each component along its own window
        vec2 pos = st * sc;
        float gx = ANOD_GRID_GAIN_X, gy = ANOD_GRID_GAIN_Y, P = anod_pitch / texel_y;   // the pitch in texels
        float qx = pos.x + gx * dx.x, qy = pos.y + gy * dy.y;             // the grid coordinates
        float nx = length(vec2(1.0 + gx * ex.x, gx * ey.x));              // |∇qx|: local pitch is P/nx
        float ny = length(vec2(gy * ex.y, 1.0 + gy * ey.y));
        float lx = 1.0 - smoothstep(0.45, 1.2, abs(fract(qx / P + 0.5) - 0.5) * P / max(nx, 1e-3));   // distance to the nearest line, texels
        float ly = 1.0 - smoothstep(0.45, 1.2, abs(fract(qy / P + 0.5) - 0.5) * P / max(ny, 1e-3));
        lx *= smoothstep(ANOD_GRID_ALIAS * 0.6, ANOD_GRID_ALIAS, P / max(nx, 1e-3));
        ly *= smoothstep(ANOD_GRID_ALIAS * 0.6, ANOD_GRID_ALIAS, P / max(ny, 1e-3));
        lit = ANOD_GRID * max(lx, ly) * smoothstep(1.0, 3.0, m);
    }
    straight = vec4(c, clamp(lit, 0.0, 1.0));
    col += lit * c;
    return min(col, vec3(1.0));
}

vec3 srgb_encode(vec3 c) {
    vec3 lo = c * 12.92;
    vec3 hi = 1.055 * pow(max(c, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055;
    return mix(lo, hi, step(vec3(0.0031308), c));
}

// DEV ONLY (the bench's N key) — THE PLATE: the Chladni lattice the operator
// is actually using, drawn over the print. With deform.glsl's own convention,
// u = k_x(P.x − x0), v = k_y(P.y − y0), k = π/pitch, the stream function is
// ψ = cos u · cos v: its separatrices ψ = 0 ARE the cell boundaries — the
// nodal lines, where the sand would settle and where the ink is drawn out —
// and an eddy core sits at every cell centre, neighbours counter-rotating
// with the sign of ψ. Screen-locked and drawn after the dip's flash: a guide
// on the plate, never ink, never through the field. dbg_lattice = 0 is the
// shipped path and leaves the composite bit-identical (the §4.6 fixture).
// DEV ONLY (the bench's N key) — THE PLATE over the print: the display cells
// the Chladni stir turns, drawn as the circles the shells draw for the keys
// (imaginary largest circles where a layout draws none), naturals and
// accidentals in the shells' two colours, the odd cells (the ones B reverses)
// with a fuller face. No lattice: the cells ARE the eddies. Screen-locked,
// drawn after the dip's flash, never through the field. dbg_lattice = 0 is
// the shipped path: the composite is bit-identical (the §4.6 fixture).
vec3 chladni_guide(vec3 col) {
    float a = clamp(dbg_lattice, 0.0, 1.0);
    int n = int(dbg_cell_count + 0.5);
    float best_edge = 1e9, best_fill = 1e9, kindc = 0.0;
    for (int i = 0; i < 320; i++) {
        if (i >= n) break;
        vec4 c = dbg_cells[i];
        float d = length(vec2((st.x - c.x) * aspect, st.y - c.y));          // canvas-height units
        float edge = abs(d - c.z) / texel_y;                                 // texels to the drawn circle
        if (edge < best_edge) { best_edge = edge; kindc = c.w; }
        float fill = (d - c.z) / texel_y;
        if (fill < best_fill) best_fill = fill;
    }
    if (n > 0) {
        float ring = 1.0 - smoothstep(0.7, 1.9, best_edge);
        float inside = 1.0 - smoothstep(-2.0, 0.0, best_fill);
        float acc = mod(kindc, 2.0), odd = kindc >= 2.0 ? 1.0 : 0.0;
        vec3 c_nat = vec3(0.15, 0.95, 1.00), c_acc = vec3(1.00, 0.62, 0.18);     // the shells' two key families
        vec3 cc = mix(c_nat, c_acc, clamp(acc, 0.0, 1.0));
        col = mix(col, cc, (0.08 + 0.10 * odd) * a * inside);                    // the face: fuller on the odd cells
        col = mix(col, cc, 0.85 * a * ring);                                     // the outline
    }
    return col;
}

// 1.3.0 (Phase 8 step 59, SYNTH §2.7): THE SCOPE VIEW — the shell's polylines (the synth voices' orbits, placed at
// their cells by the shell) drawn screen-locked over the composite, or instead of it on the scope's dark glass:
// the distance to the nearest segment in texels, a line a texel and a half wide, amber. Live path only — like the
// live ripple, the print and the export never see it (the dip prints the water, not the scope). scope_mode = 0 is
// the shipped path: the composite is bit-identical (the §4.6 fixture).
vec3 scope_view(vec3 col) {
    int n = int(scope_count + 0.5);
    float best = 1e9;
    vec2 p = vec2(st.x * aspect, st.y);                                     // canvas-height units
    for (int i = 0; i < 128; i++) {
        if (i >= n) break;
        vec4 s = scope_seg[i];
        vec2 a = vec2(s.x * aspect, s.y), b = vec2(s.z * aspect, s.w);
        vec2 ab = b - a; float l2 = dot(ab, ab);
        float t = l2 > 1e-12 ? clamp(dot(p - a, ab) / l2, 0.0, 1.0) : 0.0;
        float d = length(p - (a + ab * t)) / texel_y;                       // texels to the segment
        best = min(best, d);
    }
    if (scope_mode > 1.5) col = vec3(0.055, 0.058, 0.072);                 // the scope's glass: the medium replaced
    if (n > 0) {
        float line = 1.0 - smoothstep(0.6, 1.9, best);
        float halo = 1.0 - smoothstep(1.5, 6.0, best);
        vec3 amber = vec3(0.97, 0.80, 0.42);
        col = mix(col, amber, 0.18 * halo * (scope_mode > 1.5 ? 1.0 : 0.5));
        col = mix(col, amber, 0.92 * line);
    }
    return col;
}

// step 43: the glow's shoulder — linear below a, easing toward 1 above, per channel
vec3 anod_shoulder(vec3 x) {
    const float a = 0.8;
    vec3 over = a + (1.0 - a) * (1.0 - exp(-(x - a) / (1.0 - a)));
    return mix(x, over, step(vec3(a), x));
}

// 1.6.0 (Phase 11 step 71, KESSHO §1; DECISIONS_10 #3) — THE KESSHO COMPOSITE: the crystal. The same field
// read as a birefringent slab between crossed polarizers over a backlight. Polar-decompose the texel's
// Jacobian, J = R_φ·U(ψ, λ): the stretch λ is Anod's σ exactly (σ = λ − 1/λ = sqrt(‖J‖²_F − 2)), the
// principal axis ψ is the eigenvector of C = JᵀJ — the forward map's left Cauchy–Green tensor, what a
// polarizer on the screen sees — and the rotation sits in tr J (conjugation-invariant, the same for the
// forward and the inverse map when det J = 1).
//   LAYER A, photoelastic birefringence: I_A = sin²(2(ψ − α)) · ML(Γσ) — the isoclinic brush (dark where
//   the principal axis is parallel to the analyzer, the Maltese cross of a vortex's shear annulus) times
//   the Michel-Lévy colour at retardation Γσ, a LUT the engine builds (the spectral chart, or the palette
//   read in orders). Rigid rotation has C = I: ψ undefined, σ = 0, dark — exactly where Anod is dark.
//   LAYER B, the trace discriminant and the caustic: Δ = (tr J)² − 4 tints the elliptic field (vortex cores)
//   cool and the hyperbolic (pinches, wakes, chaos) warm; the caustic is the refraction of the stored
//   displacement onto a bed at depth z — with det J = 1 the determinant collapses to one line,
//   det(I + z∇d) = 1 + z(1 − z)(tr J − 2), so z = 1 is FLAT for any exact field (the free det-J gate,
//   --kessho-flat) and for z > 1 the fold is the iso-trace contour tr J = 2 + 1/(z(z − 1)), which the
//   depth control sweeps through the hyperbolic regime toward the parabolic horizon.
// The short stencil (Anod's, one-sided min, the rounding bias subtracted) gives ψ and σ; tr J for Layer B
// is read from the wide window the Anod grid uses (the staircase averaged along each axis), since the
// fold at a large z sits a few hundredths above 2 and a texel-wide difference would read the quantum.
// The substrate is the palette's clear tone (frosted calcite) with the washi's screen-locked grain as
// facets; the caustic's light is the palette's accent. Screen-locked by construction: the §4.5 invariant.
const float KESSHO_NM = 2200.0;   // the LUT's extent — KESSHO_LUT_NM in kessho_lut.h
vec3 kessho_col(vec4 field, float grain, out vec4 straight, out vec4 straight_b) {
    // ---- the short stencil: J for ψ and σ ----
    float n = 2.0 * max(1.0, floor(1.0 / (texel_y * 512.0) + 0.5));
    vec2 tx = vec2(n * texel_y / aspect, 0.0), ty = vec2(0.0, n * texel_y);
    vec4 fpx = texture(sampler2D(tex_field, smp_field), st + tx);
    vec4 fmx = texture(sampler2D(tex_field, smp_field), st - tx);
    vec4 fpy = texture(sampler2D(tex_field, smp_field), st + ty);
    vec4 fmy = texture(sampler2D(tex_field, smp_field), st - ty);
    bool fc = anod_fresh(field);
    bool seam = (anod_fresh(fpx) != fc) || (anod_fresh(fmx) != fc) || (anod_fresh(fpy) != fc) || (anod_fresh(fmy) != fc);
    vec2 scl = vec2(aspect, 1.0) / (n * texel_y);
    vec2 gxp = (fpx.xy - field.xy) * scl + vec2(1.0, 0.0), gxm = (field.xy - fmx.xy) * scl + vec2(1.0, 0.0);
    vec2 gyp = (fpy.xy - field.xy) * scl + vec2(0.0, 1.0), gym = (field.xy - fmy.xy) * scl + vec2(0.0, 1.0);
    float ux = abs(gxp.x) < abs(gxm.x) ? gxp.x : gxm.x, vx = abs(gxp.y) < abs(gxm.y) ? gxp.y : gxm.y;
    float uy = abs(gyp.x) < abs(gym.x) ? gyp.x : gym.x, vy = abs(gyp.y) < abs(gym.y) ? gyp.y : gym.y;
    float F2 = ux * ux + uy * uy + vx * vx + vy * vy;
    float su = anod_ulp(abs(field.x)) * scl.x, sv = anod_ulp(abs(field.y)) * scl.y;
    float bias = 2.0 * (su * su + sv * sv) / 6.0;
    bool masked = seam || fc;
    float sigma = masked ? 0.0 : sqrt(max(F2 - 2.0 - 3.0 * bias, 0.0));
    float C11 = ux * ux + vx * vx, C22 = uy * uy + vy * vy, C12 = ux * uy + vx * vy;
    float psi = 0.5 * atan(2.0 * C12, C11 - C22);                     // the principal axis of C = JᵀJ
    // ---- the wide window: tr J for Layer B ----
    int hw = int(3.0 * max(1.0, floor(1.0 / (texel_y * 512.0) + 0.5)));
    vec2 t1x = vec2(texel_y / aspect, 0.0), t1y = vec2(0.0, texel_y);
    vec2 ex = vec2(0.0), ey = vec2(0.0);
    bool wmask = false;
    for (int i = -12; i <= 12; i++) {
        if (abs(i) != hw) continue;
        vec2 ax = st + float(i) * t1x, ay = st + float(i) * t1y;
        vec4 fx = texture(sampler2D(tex_field, smp_field), ax);
        vec4 fy = texture(sampler2D(tex_field, smp_field), ay);
        if (ax.x < 0.0 || ax.x > 1.0 || ay.y < 0.0 || ay.y > 1.0) wmask = true;   // the window off the sheet: no trace
        if (anod_fresh(fx) != fc || anod_fresh(fy) != fc) wmask = true;           // a seam inside the window: no trace
        float sgn = i > 0 ? 1.0 : -1.0;
        ex += sgn * fx.xy; ey += sgn * fy.xy;
    }
    vec2 sc = vec2(aspect, 1.0) / texel_y;
    ex *= sc / (2.0 * float(hw)); ey *= sc / (2.0 * float(hw));          // ∂d/∂x, ∂d/∂y (texel per texel)
    float trd = (masked || wmask) ? 0.0 : (ex.x + ey.y);                  // tr(∇d) = tr J − 2
    float Delta = (trd + 2.0) * (trd + 2.0) - 4.0;                        // the trace discriminant
    // Layer B: the regime tint and the caustic fold. The fold is the contour det(I + z∇d) = 0 — a LINE in the
    // sheet: drawn screen-space, a fragment and a half wide whatever the local gradient of the trace (fwidth,
    // so it neither aliases where the strain changes fast nor smears where it changes slowly), over the soft
    // glow of the inverse-determinant form where the water is near the fold; the rest value (den = 1) is
    // subtracted so resting water is dark.
    float z = caustic_z;
    float den = 1.0 + z * (1.0 - z) * trd;                                // det(I + z∇d) under det J = 1
    const float EPS = 0.06;
    float c0 = EPS / (1.0 + EPS);
    float soft = clamp((EPS / (abs(den) + EPS) - c0) / (1.0 - c0), 0.0, 1.0);
    float fw = fwidth(den) + 1e-7;
    float line = 1.0 - smoothstep(0.0, 1.0, abs(den) / (1.5 * fw));
    float caus = kessho_caustic * max(soft, line);
    float tint_w = kessho_tint * smoothstep(0.05, 0.8, abs(Delta));       // the staircase's noise sits under 0.05
    vec3 cool = vec3(0.10, 0.22, 0.42), warm = vec3(0.52, 0.22, 0.08);
    vec3 layer_b = (Delta < 0.0 ? cool : warm) * tint_w + caus * pal_accent_m();
    // Layer A: the analyzer — global, or the nearest four held voices' blended by inverse-square distance
    // over the global as the floor (mode B, the A/B item K-1), the vectors (cos 4α, sin 4α) so the π/2
    // period wraps; then the brush × the chart
    vec2 pol = pol_global.xy;
    int np = int(pol_count + 0.5);
    if (np > 0) {
        float bd[4]; vec2 bv[4];
        for (int k = 0; k < 4; k++) { bd[k] = 1e9; bv[k] = vec2(0.0); }
        vec2 p = vec2(st.x * aspect, st.y);
        for (int i = 0; i < 16; i++) {
            if (i >= np) break;
            vec4 v = pol_voices[i];
            vec2 q = vec2(v.x * aspect, v.y);
            float d2 = dot(p - q, p - q);
            for (int k = 0; k < 4; k++) {
                if (d2 < bd[k]) {
                    for (int j = 3; j > k; j--) { bd[j] = bd[j - 1]; bv[j] = bv[j - 1]; }
                    bd[k] = d2; bv[k] = v.zw;
                    break;
                }
            }
        }
        const float R0 = 0.03, RFAR = 0.35;                               // a voice's reach; where the global takes over
        vec2 acc = pol * (1.0 / (RFAR * RFAR));
        for (int k = 0; k < 4; k++) if (bd[k] < 1e8) acc += bv[k] * (1.0 / (bd[k] + R0 * R0));
        float l = length(acc);
        pol = l > 1e-6 ? acc / l : pol_global.xy;
    }
    float brush = 0.5 * (1.0 - (cos(4.0 * psi) * pol.x + sin(4.0 * psi) * pol.y));   // sin²(2(ψ − α))
    float t = clamp(kessho_retard * sigma / KESSHO_NM, 0.0, 1.0);
    vec3 ml = texture(sampler2D(tex_lut, smp_lut), vec2((t * 1023.0 + 0.5) / 1024.0, 0.5)).rgb;
    vec3 layer_a = kessho_backlight * brush * ml;
    // the substrate: frosted calcite between crossed polarizers — a fixed dark tone (as Anod's glass is, so a custom
    // palette's clear tone never lights the sheet), the facets' speckle screen-locked
    vec3 col = vec3(0.012, 0.013, 0.018) * (1.0 + kessho_grain * (0.8 * grain - 0.2));
    vec3 light = layer_b + layer_a;
    straight_b = vec4(layer_b, clamp(2.0 * max(layer_b.r, max(layer_b.g, layer_b.b)), 0.0, 1.0));
    straight = vec4(light, clamp(2.0 * max(light.r, max(light.g, light.b)), 0.0, 1.0));
    return min(col + light, vec3(1.0));
}

void main() {
    // §4.5 live ripple: displace the INK sampling coordinate by the §4.3(6)
    // shear before the field lookup — a non-destructive view displacement; the
    // field itself is untouched, the paper (below) stays screen-locked. The
    // amp == 0 branch keeps the un-rippled path bit-identical to v0.3.
    vec2 st_ink = st;
    if (ripple_amp != 0.0) {
        vec2 Pr = vec2(st.x * aspect, st.y);
        vec2 C0 = vec2(0.5 * aspect, 0.5);
        vec2 relr = Pr - C0;
        float rlx =  ripple_ca * relr.x + ripple_sa * relr.y;
        float rly = -ripple_sa * relr.x + ripple_ca * relr.y;
        rlx -= ripple_amp * sin(ripple_k * rly + ripple_phase);
        vec2 Ps = C0 + vec2(ripple_ca * rlx - ripple_sa * rly,
                            ripple_sa * rlx + ripple_ca * rly);
        st_ink = vec2(Ps.x / aspect, Ps.y);
    }
    vec4 field = texture(sampler2D(tex_field, smp_field), st_ink);
    float phase = field.z;
    float aux = field.w;

    // Washi paper (§4.5): two directional ridged simplex layers = mulberry
    // fiber strands. Per-region ±20° angle drift via low-frequency noise and
    // break-up along the ridge length (short segments) keep any two areas
    // from sharing a coherent crosshatch lattice. Screen-locked: sampled at
    // st, never through the deformed field. Isotropic space (no stretch).
    vec2 p = vec2(st.x * aspect, st.y);
    float drift1 = snoise(p * 0.9 + 3.1) * 0.349;    // ±20 deg
    float drift2 = snoise(p * 0.7 + 27.4) * 0.349;
    float a1 = 0.2618 + drift1;                      //  15 deg base
    float a2 = -0.6109 + drift2;                     // -35 deg base
    mat2 R1 = mat2(cos(a1), sin(a1), -sin(a1), cos(a1));
    mat2 R2 = mat2(cos(a2), sin(a2), -sin(a2), cos(a2));
    vec2 p1 = R1 * p;
    vec2 p2 = R2 * p;
    // step 43 (QOL §2): fiber_scale multiplies the strands' frequencies — at 1 the product is 0.x's exactly
    float strand1 = 1.0 - abs(snoise(p1 * vec2(2.2, 90.0) * fiber_scale));
    float strand2 = 1.0 - abs(snoise(p2 * vec2(1.7, 70.0) * fiber_scale + 13.7));
    // Segment masks: modulate along the ridge direction so strands read as
    // short overlapping fibers, not continuous rules.
    float seg1 = smoothstep(0.25, 0.55, 0.5 + 0.5 * snoise(p1 * vec2(26.0, 4.5) * fiber_scale + 11.0));
    float seg2 = smoothstep(0.25, 0.55, 0.5 + 0.5 * snoise(p2 * vec2(22.0, 4.0) * fiber_scale + 5.0));
    float strands = max(pow(strand1, 10.0) * seg1, pow(strand2, 10.0) * seg2);
    float mottle = snoise(p * 9.0) * 0.5 + 0.5;
    float grain  = snoise(p * 420.0) * 0.5 + 0.5;

    vec3 paper = paper_tint.rgb;                              // step 43: the tint (0.x's linear washi cream by default)
    paper *= 1.0 - roughness * (0.10 * mottle + 0.05 * grain);
    paper += vec3(0.060, 0.055, 0.045) * (roughness * strands);

    vec3 col;
    vec4 anod_straight = vec4(0.0);
    if (medium > 0.5) {
        if (medium > 1.5) {
            // 1.6.0 (step 71): the Kessho medium — the crystal; the bloom and the shoulder are Anod's, shared
            vec4 straight_b;
            col = kessho_col(field, grain, anod_straight, straight_b);
            if (caustic_only > 0.5) anod_straight = straight_b;
        } else {
            col = anod_col(field, grain, anod_straight);   // 1.1.0: the Anod medium reads the same field as strain (MEDIUM §3)
        }
        if (linear_out > 0.5) { frag_color = vec4(col, 1.0); return; }   // step 43: the bloom's source — linear, the discharge alone
        if (bloom_in > 0.5) {
            // THE GLOW (step 43, the author's): the blurred emission added back, then a soft shoulder — linear
            // below 0.8, easing toward 1 above — so the brightest filaments clip toward white: a wide cold
            // halo, a neon mid, a white-hot core, the way a discharge blooms in a lens
            vec3 bl = texture(sampler2D(tex_bloom, smp_bloom), st).rgb * bloom_strength;
            col = anod_shoulder(col + bl);
            anod_straight.rgb = anod_shoulder(anod_straight.rgb + bl);
            anod_straight.a = max(anod_straight.a, clamp(max(bl.r, max(bl.g, bl.b)), 0.0, 1.0));
        }
    } else {
        // Palette morph (§2.2 Flex; #61): the CC travels the whole ring from the
        // active palette so one controller reaches every palette; step 43: the
        // engine resolves the ring into the two slots and the blend above.
        vec3 clearw = pal_clear_m();

        col = paper;
        if (phase >= 1.0) {
            float band = mod(floor(phase), 2.0);
            if (band >= 1.0) {
                // Ink band: per-drop hue offset from the continuous aux selector
                // (golden-ratio spread; slide shifts it live, §3.4).
                float hue_t = fract(aux * 0.6180339887);
                // Ink thickness: thin near the VISIBLE ring boundary. fract(phase)
                // is useless here — feed-grown regions are onion-layered micro-
                // shells, one per emission — so probe the band at four small
                // offsets instead: fewer same-band neighbors = closer to an edge.
                // Edge probes are INK lookups: they ride the same (possibly
                // rippled) sampling coordinate as the center tap.
                float e = texel_y * 5.0;
                float b0 = mod(floor(texture(sampler2D(tex_field, smp_field), st_ink + vec2( e / aspect, 0.0)).z), 2.0);
                float b1 = mod(floor(texture(sampler2D(tex_field, smp_field), st_ink + vec2(-e / aspect, 0.0)).z), 2.0);
                float b2 = mod(floor(texture(sampler2D(tex_field, smp_field), st_ink + vec2(0.0,  e)).z), 2.0);
                float b3 = mod(floor(texture(sampler2D(tex_field, smp_field), st_ink + vec2(0.0, -e)).z), 2.0);
                float same = (step(0.5, b0) == step(0.5, band) ? 0.25 : 0.0) +
                             (step(0.5, b1) == step(0.5, band) ? 0.25 : 0.0) +
                             (step(0.5, b2) == step(0.5, band) ? 0.25 : 0.0) +
                             (step(0.5, b3) == step(0.5, band) ? 0.25 : 0.0);
                float thickness = smoothstep(0.4, 1.0, same);
                vec3 c = pal_ink_at(thickness, hue_t);   // step 43: the one path — built-in, preset or custom
                // Absorption: thin ink lets paper grain through; under dense ink
                // the fiber modulation is capped low so pooled sumi stays
                // near-black (~0.05-0.1 linear at the centers).
                float soak_thin  = roughness * (0.30 * grain + 0.35 * strands);
                float soak_dense = roughness * (0.05 * grain + 0.07 * strands);
                float soak = mix(soak_thin, soak_dense, thickness) + 0.055;
                col = mix(c, paper, clamp(soak, 0.0, 0.65));
            } else {
                // Clear water band between inks: wet-paper tone, fibers showing.
                col = mix(clearw, paper, 0.35 + 0.3 * roughness * strands);
            }
        }

        // "Lift the paper" flash right after a dip.
    }
    col = mix(col, vec3(0.92, 0.90, 0.85), clamp(dip_fade, 0.0, 1.0));
    if (dbg_lattice > 0.0) col = chladni_guide(col);   // DEV ONLY: the plate over the print
    if (scope_mode > 0.5) col = scope_view(col);        // 1.3.0: the scope view (the live path only)

    if (alpha_out > 0.5 && medium > 0.5) {
        frag_color = vec4(srgb_encode(anod_straight.rgb), anod_straight.a);   // step 43: the discharge over alpha (straight)
    } else {
        frag_color = vec4(srgb_encode(col), 1.0);   // linear -> sRGB (§4.5)
    }
}
@end

@program composite       composite_vs       composite_fs
@program composite_print composite_print_vs composite_fs
