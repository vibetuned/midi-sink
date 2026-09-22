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
layout(binding=0) uniform composite_params {
    float aspect;         // field W/H: fibers in isotropic space
    float roughness;      // washi fiber/grain strength (0..1)
    float palette_id;     // 0 sumi, 1 indigo, 2 ochre
    float palette_morph;  // 0..1 blend toward the next palette
    float dip_fade;       // 1 right after a paper dip -> 0 ("lift the paper")
    float texel_y;        // 1 / field height (edge-proximity sampling)
    float ripple_amp;     // §4.5 live ripple (v0.4): 0 = off (bake mode, or
    float ripple_k;       //   the print path — the dip samples UN-rippled)
    float ripple_phase;
    float ripple_ca;      // cos(ripple angle)
    float ripple_sa;      // sin(ripple angle)
    vec4  cust_stops[8];  // 1.0.0 custom palette (palette_id 3): linear RGB + position, ascending
    vec4  cust_params;    //   stop count, depth gamma, depth floor, per-drop drift
    vec4  cust_clear;     //   the clear-water band tone
    float medium;         // 1.1.0: 0 sumi (the path below, bitwise 1.0.0), 1 anod (strain-glow)
    float anod_glow;      //   the strain-glow scale
    float anod_pitch;     //   the water grid's pitch at rest, canvas heights (0 = no grid)
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
vec3 pal_ink(int id) {
    if (id == 1) return vec3(0.015, 0.035, 0.170);   // indigo
    if (id == 2) return vec3(0.430, 0.185, 0.022);   // ochre
    return vec3(0.012, 0.011, 0.013);                // sumi black
}
vec3 pal_accent(int id) {                            // per-drop hue drift target
    if (id == 1) return vec3(0.020, 0.110, 0.150);   // indigo -> teal
    if (id == 2) return vec3(0.300, 0.060, 0.015);   // ochre -> burnt sienna
    return vec3(0.055, 0.042, 0.034);                // sumi -> warm soot
}
vec3 pal_clear(int id) {                             // "clear water" band tone
    if (id == 1) return vec3(0.780, 0.800, 0.830);
    if (id == 2) return vec3(0.840, 0.780, 0.660);
    return vec3(0.830, 0.815, 0.760);
}

// 1.0.0 (Phase 6 step 41, QOL §1): the CUSTOM palette — an ink-depth gradient.
// depth = the band's thickness (0 at a visible edge, 1 pooled), curved by
// u = floor + (1 − floor)·depth^γ, shifted per drop by ±drift/2 along the
// gradient (the aux selector, as the built-ins' hue drift), then sampled
// between the ascending stops. The washi and the soak below are the same as
// the built-ins' — the medium keeps its character (the identity guardrail).
vec3 pal_custom(float depth, float hue_t) {
    float u = clamp(cust_params.z + (1.0 - cust_params.z) * pow(max(depth, 0.0), cust_params.y), 0.0, 1.0);
    u = clamp(u + cust_params.w * (hue_t - 0.5), 0.0, 1.0);
    int n = int(cust_params.x + 0.5);
    vec3 c = cust_stops[0].rgb;
    for (int i = 0; i < 7; i++) {
        if (i + 1 >= n) break;
        float p0 = cust_stops[i].w, p1 = cust_stops[i + 1].w;
        if (u >= p0) c = mix(cust_stops[i].rgb, cust_stops[i + 1].rgb, clamp((u - p0) / max(p1 - p0, 1e-5), 0.0, 1.0));
    }
    return c;
}

// 1.1.0 (Phase 6 step 42, MEDIUM §3) — the ANOD composite: the CHARGE glows
// by its strain, the WATER draws the field's grid. The field already carries
// the accumulated map: finite-differencing the stored source coordinates
// between neighbouring texels gives its Jacobian J, and ‖J‖_F² − 2 =
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
vec3 anod_core(int id) {
    if (id == 1) return vec3(1.00, 0.40, 0.08);      // plasma orange
    if (id == 2) return vec3(0.22, 1.00, 0.34);      // phosphor green
    return vec3(0.30, 0.42, 1.00);                   // electric blue / violet
}
vec3 anod_halo(int id) {
    if (id == 1) return vec3(1.00, 0.82, 0.30);
    if (id == 2) return vec3(0.72, 1.00, 0.50);
    return vec3(0.62, 0.30, 1.00);
}
// THE FIELD'S PRECISION: the coordinates are half floats, whose spacing above
// 0.5 is 2^-11 — a full texel of a 2048-wide field. Differencing neighbours
// of an identity field reads that rounding as strain (measured: the right
// half of a 2560-wide window glowed in stripes while the left stayed dark).
// So the stencil widens with the field — 2·round(H/512) texels each side,
// 2 at 512, 6 at 1440 — keeping the rounding a fixed fraction of the step,
// the fresh test tolerates one ULP of the coordinate, and the expected
// rounding bias of ‖J‖_F² (the variance of the differences, 3× for its tail)
// is subtracted before the strain is read. On charged material, whose strain
// is large, that is enough; on water it is not — see the grid below.
float anod_ulp(float x) { return exp2(floor(log2(max(x, 1e-6))) - 10.0); }   // half-float spacing at x
bool anod_fresh(vec4 f, vec2 at) {               // fresh water / never touched: identity coords, no phase
    return f.z < 0.5 && abs(f.x - at.x) < max(0.5 * texel_y / aspect, anod_ulp(at.x))
                     && abs(f.y - at.y) < max(0.5 * texel_y, anod_ulp(at.y));
}
vec3 anod_col(vec4 field, float grain) {
    float n = 2.0 * max(1.0, floor(1.0 / (texel_y * 512.0) + 0.5));   // stencil half-width, texels: 2 at 512, 6 at 1440
    vec2 tx = vec2(n * texel_y / aspect, 0.0), ty = vec2(0.0, n * texel_y);
    vec4 fpx = texture(sampler2D(tex_field, smp_field), st + tx);
    vec4 fmx = texture(sampler2D(tex_field, smp_field), st - tx);
    vec4 fpy = texture(sampler2D(tex_field, smp_field), st + ty);
    vec4 fmy = texture(sampler2D(tex_field, smp_field), st - ty);
    bool fc = anod_fresh(field, st);
    bool seam = (anod_fresh(fpx, st + tx) != fc) || (anod_fresh(fmx, st - tx) != fc) ||
                (anod_fresh(fpy, st + ty) != fc) || (anod_fresh(fmy, st - ty) != fc);
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
    // and steps are found, not stored.)
    vec2 scl = vec2(aspect, 1.0) / (n * texel_y);                 // canvas-height units per canvas height
    vec2 gxp = (fpx.xy - field.xy) * scl, gxm = (field.xy - fmx.xy) * scl;   // ∂(u,v)/∂x, either side
    vec2 gyp = (fpy.xy - field.xy) * scl, gym = (field.xy - fmy.xy) * scl;   // ∂(u,v)/∂y
    float ux = abs(gxp.x) < abs(gxm.x) ? gxp.x : gxm.x, vx = abs(gxp.y) < abs(gxm.y) ? gxp.y : gxm.y;
    float uy = abs(gyp.x) < abs(gym.x) ? gyp.x : gym.x, vy = abs(gyp.y) < abs(gym.y) ? gyp.y : gym.y;
    float F2 = ux * ux + uy * uy + vx * vx + vy * vy;
    // the rounding bias: a one-sided difference carries two uniform errors of ±ULP/2 (variance ULP²/6),
    // scaled as the derivatives are; ‖J‖_F² is biased up by the sum over its four entries — three
    // times that is subtracted for the tail
    float su = anod_ulp(field.x) * scl.x, sv = anod_ulp(field.y) * scl.y;
    float bias = 2.0 * (su * su + sv * sv) / 6.0;
    float sigma = (seam || fc) ? 0.0 : sqrt(max(F2 - 2.0 - 3.0 * bias, 0.0));
    float g = 1.0 - exp(-sigma / anod_glow);
    // the substrate: vacuum glass with a phosphor speckle, screen-locked
    vec3 col = vec3(0.010, 0.010, 0.014) * (1.0 + roughness * (0.6 * grain - 0.1));
    // the discharge: charged material bands between core and halo, drifts by aux
    float phase = field.z, aux = field.w;
    bool charged = phase >= 1.0;
    float band = mod(floor(max(phase, 1.0)), 2.0);
    float hue_t = fract(aux * 0.6180339887);
    vec3 c;
    if (palette_id >= 2.5) {
        c = pal_custom(g, hue_t);                      // the custom palette read as a glow: the gradient by strain
    } else {
        float t = clamp(palette_morph, 0.0, 1.0) * 2.0;
        int seg = int(min(floor(t), 1.0));
        int base = int(clamp(palette_id, 0.0, 2.0) + 0.5);
        int id0 = (base + seg) - 3 * ((base + seg) / 3);
        int id1 = (id0 + 1) - 3 * ((id0 + 1) / 3);
        float m = t - float(seg);
        vec3 core = mix(anod_core(id0), anod_core(id1), m), halo = mix(anod_halo(id0), anod_halo(id1), m);
        vec3 a = band >= 1.0 ? core : halo, b = band >= 1.0 ? halo : core;
        c = mix(a, b, 0.45 * hue_t);
    }
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
            if (fx.z < 1.0 && ax.x >= 0.0 && ax.x <= 1.0) { sx += fx.xy - ax; cx += 1.0; if (i == hw) ex += fx.xy - ax; if (i == -hw) ex -= fx.xy - ax; }
            if (fy.z < 1.0 && ay.y >= 0.0 && ay.y <= 1.0) { sy += fy.xy - ay; cy += 1.0; if (i == hw) ey += fy.xy - ay; if (i == -hw) ey -= fy.xy - ay; }
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
    col += lit * c;
    return min(col, vec3(1.0));
}

vec3 srgb_encode(vec3 c) {
    vec3 lo = c * 12.92;
    vec3 hi = 1.055 * pow(max(c, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055;
    return mix(lo, hi, step(vec3(0.0031308), c));
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
    float strand1 = 1.0 - abs(snoise(p1 * vec2(2.2, 90.0)));
    float strand2 = 1.0 - abs(snoise(p2 * vec2(1.7, 70.0) + 13.7));
    // Segment masks: modulate along the ridge direction so strands read as
    // short overlapping fibers, not continuous rules.
    float seg1 = smoothstep(0.25, 0.55, 0.5 + 0.5 * snoise(p1 * vec2(26.0, 4.5) + 11.0));
    float seg2 = smoothstep(0.25, 0.55, 0.5 + 0.5 * snoise(p2 * vec2(22.0, 4.0) + 5.0));
    float strands = max(pow(strand1, 10.0) * seg1, pow(strand2, 10.0) * seg2);
    float mottle = snoise(p * 9.0) * 0.5 + 0.5;
    float grain  = snoise(p * 420.0) * 0.5 + 0.5;

    vec3 paper = vec3(0.900, 0.868, 0.790);                   // linear washi cream
    paper *= 1.0 - roughness * (0.10 * mottle + 0.05 * grain);
    paper += vec3(0.060, 0.055, 0.045) * (roughness * strands);

    vec3 col;
    if (medium > 0.5) {
        col = anod_col(field, grain);   // 1.1.0: the Anod medium reads the same field as strain (MEDIUM §3)
    } else {
        // Palette morph (§2.2 Flex; #61): the CC travels the whole ring from the
        // active palette — 0 = active, 1/2 = the next, 1 = the third — so one
        // controller reaches every palette (sumi -> indigo -> ochre from Sumi).
        float t = clamp(palette_morph, 0.0, 1.0) * 2.0;
        int seg = int(min(floor(t), 1.0));
        int base = int(clamp(palette_id, 0.0, 2.0) + 0.5);
        int id0 = (base + seg) - 3 * ((base + seg) / 3);
        int id1 = (id0 + 1) - 3 * ((id0 + 1) / 3);
        float m = t - float(seg);
        vec3 ink    = mix(pal_ink(id0),    pal_ink(id1),    m);
        vec3 accent = mix(pal_accent(id0), pal_accent(id1), m);
        vec3 clearw = mix(pal_clear(id0),  pal_clear(id1),  m);

        col = paper;
        if (phase >= 1.0) {
            float band = mod(floor(phase), 2.0);
            if (band >= 1.0) {
                // Ink band: per-drop hue offset from the continuous aux selector
                // (golden-ratio spread; slide shifts it live, §3.4).
                float hue_t = fract(aux * 0.6180339887);
                vec3 c = mix(ink, accent, 0.45 * hue_t);
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
                if (palette_id >= 2.5) c = pal_custom(thickness, hue_t);   // 1.0.0: the custom palette; the built-in path above is untouched
                // Absorption: thin ink lets paper grain through; under dense ink
                // the fiber modulation is capped low so pooled sumi stays
                // near-black (~0.05-0.1 linear at the centers).
                float soak_thin  = roughness * (0.30 * grain + 0.35 * strands);
                float soak_dense = roughness * (0.05 * grain + 0.07 * strands);
                float soak = mix(soak_thin, soak_dense, thickness) + 0.055;
                col = mix(c, paper, clamp(soak, 0.0, 0.65));
            } else {
                // Clear water band between inks: wet-paper tone, fibers showing.
                vec3 cw = clearw;
                if (palette_id >= 2.5) cw = cust_clear.rgb;   // 1.0.0
                col = mix(cw, paper, 0.35 + 0.3 * roughness * strands);
            }
        }

        // "Lift the paper" flash right after a dip.
    }
    col = mix(col, vec3(0.92, 0.90, 0.85), clamp(dip_fade, 0.0, 1.0));

    frag_color = vec4(srgb_encode(col), 1.0);   // linear -> sRGB (§4.5)
}
@end

@program composite       composite_vs       composite_fs
@program composite_print composite_print_vs composite_fs
