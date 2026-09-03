// deform.glsl — ping-pong displacement-field passes (PROJECT_SPEC.md §4.1–§4.3).
// All deformation math runs in aspect-corrected normalized space: x is scaled
// by aspect (W/H) so distances are isotropic and rings stay circular on
// non-square canvases; lengths/radii are in units of canvas height.
//
// Texel payload (§4.2): (u, v, ink, aux). u/v are continuous pre-image
// coordinates, ink is a continuous scalar phase (never a discrete ID), aux is
// a continuous per-drop selector (reserved; palettes come later).

// Fullscreen triangle via gl_VertexIndex — no vertex buffer.
//
// flip_vert_y (§4.6): emitted into the GLSL dialects ONLY (glsl410/glsl300es;
// MSL and HLSL outputs are untouched). GL rasterizes offscreen targets with a
// bottom-left row origin; negating clip-space y makes every offscreen pass
// land in memory with the same top-left row origin as Metal/D3D11, so the
// ping-pong chain composes in the one y-down space with zero runtime branches
// and the field texture is byte-compatible across backends (the §4.6
// regression test reads it with no orientation correction). Without this, st
// and GL's raster disagree and each pass mirrors the previous one's field —
// the GL twin of the Metal bug in DECISIONS.md #17.
@vs deform_vs
@glsl_options flip_vert_y
out vec2 st;   // texture-space coordinate of this fragment's texel (v grows down)
void main() {
    vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
    st = vec2(corner.x, 1.0 - corner.y);
}
@end

// Identity init (§4.1): every texel stores its own normalized coordinate,
// ink and aux start at 0 (water).
@fs identity_fs
in vec2 st;
out vec4 frag_color;
void main() {
    frag_color = vec4(st, 0.0, 0.0);
}
@end

// Passthrough: read tex_current at the texel's own coordinate, write to
// tex_next unchanged (linear sampling per §4.2).
@fs passthrough_fs
layout(binding=0) uniform texture2D tex_current;
layout(binding=0) uniform sampler smp_field;
in vec2 st;
out vec4 frag_color;
void main() {
    frag_color = texture(sampler2D(tex_current, smp_field), st);
}
@end

// §4.3.1 — circular drop expansion of radius r at center C.
// Outside:  P_src = C + (P − C) · sqrt(1 − r² / ‖P − C‖²)
// Inside:   write the new ink phase (phase_base + local radial coordinate,
//           §4.2) and reset the pre-image to the texel's own coordinate.
@fs drop_fs
layout(binding=0) uniform texture2D tex_current;
layout(binding=0) uniform sampler smp_field;
layout(binding=0) uniform drop_params {
    vec2  center;       // normalized [0,1]
    float radius;       // canvas-height units
    float aspect;       // W/H of the field
    float phase_base;   // parity-derived band base (1 or 2); 0 = clear water drop
    float aux_value;    // raw drop counter (§4.2 aux selector)
};
in vec2 st;
out vec4 frag_color;
void main() {
    vec2 P = vec2(st.x * aspect, st.y);
    vec2 C = vec2(center.x * aspect, center.y);
    vec2 rel = P - C;
    float dist = length(rel);
    if (dist >= radius) {
        vec2 P_src = C + rel * sqrt(1.0 - (radius * radius) / (dist * dist));
        frag_color = texture(sampler2D(tex_current, smp_field),
                             vec2(P_src.x / aspect, P_src.y));
    } else {
        float radial = (dist / radius) * 0.999;   // keep the fraction below 1
        float ink = (phase_base > 0.5) ? (phase_base + radial) : 0.0;
        frag_color = vec4(st, ink, aux_value);
    }
}
@end

// §4.3.2 — tine / comb stroke along unit direction D̂ through point L with
// sharpness α and magnitude z:
//   d     = perpendicular distance from P to the line (L, D̂)
//   P_src = P − z · D̂ · α / (α + d)
@fs tine_fs
layout(binding=0) uniform texture2D tex_current;
layout(binding=0) uniform sampler smp_field;
layout(binding=0) uniform tine_params {
    vec2  p0;           // normalized [0,1]
    vec2  p1;
    float alpha;        // canvas-height units
    float magnitude;    // canvas-height units
    float aspect;
};
in vec2 st;
out vec4 frag_color;
void main() {
    vec2 P = vec2(st.x * aspect, st.y);
    vec2 L = vec2(p0.x * aspect, p0.y);
    vec2 D = normalize(vec2((p1.x - p0.x) * aspect, p1.y - p0.y));
    vec2 rel = P - L;
    float d = abs(rel.x * D.y - rel.y * D.x);
    vec2 P_src = P - magnitude * D * (alpha / (alpha + d));
    frag_color = texture(sampler2D(tex_current, smp_field),
                         vec2(P_src.x / aspect, P_src.y));
}
@end

// §4.3.3 — vortex agitation centered at V, two profiles (v0.4), both pure
// rotations by −θ(d) (exactly area-preserving, exact at any angle):
//   EXPONENTIAL (0): θ(d) = A · exp(−d / R)         — diffuse, breath-like
//   RANKINE     (1): θ(d) = ω  for d < R;           — rigid core, all shear
//                    θ(d) = ω · R²/d²  for d ≥ R      in the crease ring at R
@fs vortex_fs
layout(binding=0) uniform texture2D tex_current;
layout(binding=0) uniform sampler smp_field;
layout(binding=0) uniform vortex_params {
    vec2  center;       // normalized [0,1]
    float strength;     // A (exponential) or ω (rankine), radians
    float vradius;      // decay length (exponential) or core R (rankine)
    float aspect;
    float profile;      // 0 exponential, 1 rankine
};
in vec2 st;
out vec4 frag_color;
void main() {
    vec2 P = vec2(st.x * aspect, st.y);
    vec2 V = vec2(center.x * aspect, center.y);
    vec2 rel = P - V;
    float d = length(rel);
    float theta;
    if (profile > 0.5) {
        float dd = max(d, vradius);
        theta = -strength * (vradius * vradius) / (dd * dd);   // rigid inside
    } else {
        theta = -strength * exp(-d / vradius);
    }
    float s = sin(theta), c = cos(theta);
    vec2 P_src = V + vec2(c * rel.x - s * rel.y, s * rel.x + c * rel.y);
    frag_color = texture(sampler2D(tex_current, smp_field),
                         vec2(P_src.x / aspect, P_src.y));
}
@end

// §4.3.4 — dipolar wake: the potential-flow doublet around the rigid stylus
// tip (radius a) for ONE sub-step of motion d⃗ (‖d⃗‖ ≤ a/2, upstream). Lab-
// frame fluid displacement Δ = d·a²·((x²−y²)/r⁴, 2xy/r⁴) in the stroke frame
// (x along d̂), from φ = −U a² x/r² (the sign satisfying the no-penetration
// boundary — DECISIONS_3 #32 corrects the spec draft's '+', which rendered
// inside-out and broke its own zero-seam claim). Inverse lookup P_src = P − Δ;
// inside the tip body P_src = P − d⃗, matching the outer field with zero seam
// on the motion axis. Front ink bulges forward; flank ink streams backward.
@fs wake_fs
layout(binding=0) uniform texture2D tex_current;
layout(binding=0) uniform sampler smp_field;
layout(binding=0) uniform wake_params {
    vec2  tip;          // tip position AFTER the sub-step, normalized [0,1]
    vec2  dvec;         // sub-step motion, aspect-corrected canvas-height units
    float tip_radius;   // a, canvas-height units
    float aspect;
};
in vec2 st;
out vec4 frag_color;
void main() {
    vec2 P = vec2(st.x * aspect, st.y);
    vec2 T = vec2(tip.x * aspect, tip.y);
    vec2 rel = P - T;
    float r2 = dot(rel, rel);
    float a2 = tip_radius * tip_radius;
    vec2 P_src;
    if (r2 <= a2) {
        P_src = P - dvec;                       // rigid tip body: no fluid inside
    } else {
        float d = length(dvec);
        vec2 dh = dvec / d;                     // stroke frame x
        vec2 nh = vec2(-dh.y, dh.x);            // stroke frame y
        float lx = dot(rel, dh);
        float ly = dot(rel, nh);
        float r4 = r2 * r2;
        vec2 disp = d * a2 * ((lx * lx - ly * ly) * dh + (2.0 * lx * ly) * nh) / r4;
        P_src = P - disp;
    }
    // §3.4 ingress rule (as in the scroll pass): a source beyond the canvas
    // is fresh water — edge-clamp would DUPLICATE boundary content, which
    // fabricates ink under repeated passes (DECISIONS_3 #32).
    vec2 src = vec2(P_src.x / aspect, P_src.y);
    if (src.x < 0.0 || src.x > 1.0 || src.y < 0.0 || src.y > 1.0) {
        frag_color = vec4(st, 0.0, 0.0);
    } else {
        frag_color = texture(sampler2D(tex_current, smp_field), src);
    }
}
@end

// §4.3.5 — Hamiltonian pinch: localized area-preserving saddle. In pinch-local
// coordinates (rotated by the fold angle about the center):
//   x_src = x · e^{+k·w(s)},  y_src = y · e^{−k·w(s)},  s = x·y,
//   w(s) = exp(−|s|/S)
// s is conserved along each hyperbolic trajectory, so the window keeps the map
// closed-form with det = 1 exactly. The arms (s = 0) run outward as fading
// creases — what pinched paper physically does.
@fs pinch_fs
layout(binding=0) uniform texture2D tex_current;
layout(binding=0) uniform sampler smp_field;
layout(binding=0) uniform pinch_params {
    vec2  center;       // normalized [0,1]
    float k;            // per-pass exponent (a smoothed DELTA, never absolute)
    float ca;           // cos(fold angle)
    float sa;           // sin(fold angle)
    float window_s;     // S, aspect-corrected units²
    float aspect;
};
in vec2 st;
out vec4 frag_color;
void main() {
    vec2 P = vec2(st.x * aspect, st.y);
    vec2 C = vec2(center.x * aspect, center.y);
    vec2 rel = P - C;
    float lx =  ca * rel.x + sa * rel.y;        // into the pinch frame
    float ly = -sa * rel.x + ca * rel.y;
    float s = lx * ly;
    float w = exp(-abs(s) / window_s);
    float e = exp(k * w);
    float sx = lx * e;                          // §4.3(5) inverse form, verbatim
    float sy = ly / e;
    vec2 P_src = C + vec2(ca * sx - sa * sy, sa * sx + ca * sy);
    // §3.4 ingress rule: the fold-axis corridors cross the canvas edge at
    // full strength (w does not decay on the axes) — with edge-clamp each
    // compression half-cycle DUPLICATES boundary content inward, fabricating
    // ink over long streams (measured +9.5%/12k passes before this branch;
    // DECISIONS_3 #32). Off-canvas sources are fresh water.
    vec2 src = vec2(P_src.x / aspect, P_src.y);
    if (src.x < 0.0 || src.x > 1.0 || src.y < 0.0 || src.y > 1.0) {
        frag_color = vec4(st, 0.0, 0.0);
    } else {
        frag_color = texture(sampler2D(tex_current, smp_field), src);
    }
}
@end

// §4.3.6 — sine ripple bake pass, a pure shear (Jacobian = 1 at any
// amplitude, never folds): in the ripple frame (rotated by `angle` about the
// canvas center), x_src = x − amp · sin(k·y + phase), y_src = y. amp is the
// per-pass ΔA: at fixed (k, phase, angle) passes compose additively.
@fs ripple_fs
layout(binding=0) uniform texture2D tex_current;
layout(binding=0) uniform sampler smp_field;
layout(binding=0) uniform ripple_params {
    float amp;          // ΔA, canvas-height units
    float rk;           // wavenumber, radians per canvas-height unit
    float phase;
    float rca;          // cos(ripple angle)
    float rsa;          // sin(ripple angle)
    float aspect;
};
in vec2 st;
out vec4 frag_color;
void main() {
    vec2 P = vec2(st.x * aspect, st.y);
    vec2 C0 = vec2(0.5 * aspect, 0.5);
    vec2 rel = P - C0;
    float lx =  rca * rel.x + rsa * rel.y;      // into the ripple frame
    float ly = -rsa * rel.x + rca * rel.y;
    lx -= amp * sin(rk * ly + phase);
    vec2 P_src = C0 + vec2(rca * lx - rsa * ly, rsa * lx + rca * ly);
    // §3.4 ingress rule: every row shears across the side edges — fresh
    // water enters, never a duplicated boundary texel (DECISIONS_3 #32).
    vec2 src = vec2(P_src.x / aspect, P_src.y);
    if (src.x < 0.0 || src.x > 1.0 || src.y < 0.0 || src.y > 1.0) {
        frag_color = vec4(st, 0.0, 0.0);
    } else {
        frag_color = texture(sampler2D(tex_current, smp_field), src);
    }
}
@end

// §3.4 field motion — uniform translation with inverse lookup
// P_src = P − delta. INGRESS IS AN EXPLICIT BRANCH: when the source falls
// outside [0,1] the fragment writes fresh water — ink 0, aux 0, and the
// identity coordinates of its OWN texel. Sampler clamp modes cannot express
// this: edge-clamp would streak the boundary texel's old ink across the
// entering region, and border-clamp cannot produce per-texel identity coords.
@fs scroll_fs
layout(binding=0) uniform texture2D tex_current;
layout(binding=0) uniform sampler smp_field;
layout(binding=0) uniform scroll_params {
    vec2 delta;   // this frame's translation, st space (y down)
};
in vec2 st;
out vec4 frag_color;
void main() {
    vec2 src = st - delta;
    if (src.x < 0.0 || src.x > 1.0 || src.y < 0.0 || src.y > 1.0) {
        frag_color = vec4(st, 0.0, 0.0);   // fresh water enters at the now-line side
    } else {
        frag_color = texture(sampler2D(tex_current, smp_field), src);
    }
}
@end

@program deform_identity    deform_vs identity_fs
@program deform_passthrough deform_vs passthrough_fs
@program deform_drop        deform_vs drop_fs
@program deform_tine        deform_vs tine_fs
@program deform_vortex      deform_vs vortex_fs
@program deform_scroll      deform_vs scroll_fs
@program deform_wake        deform_vs wake_fs
@program deform_pinch       deform_vs pinch_fs
@program deform_ripple      deform_vs ripple_fs
