// deform.glsl — ping-pong displacement-field passes (PROJECT_SPEC.md §4.1–§4.3).
// All deformation math runs in aspect-corrected normalized space: x is scaled
// by aspect (W/H) so distances are isotropic and rings stay circular on
// non-square canvases; lengths/radii are in units of canvas height.
//
// Texel payload (§4.2): (u, v, ink, aux). u/v are continuous pre-image
// coordinates, ink is a continuous scalar phase (never a discrete ID), aux is
// a continuous per-drop selector (reserved; palettes come later).

// Fullscreen triangle via gl_VertexIndex — no vertex buffer.
@vs deform_vs
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

// §4.3.3 — vortex agitation centered at V: angular deflection
// θ(d) = A · exp(−d / R); rotate P around V by −θ(d) for the inverse lookup.
@fs vortex_fs
layout(binding=0) uniform texture2D tex_current;
layout(binding=0) uniform sampler smp_field;
layout(binding=0) uniform vortex_params {
    vec2  center;       // normalized [0,1]
    float strength;     // A, radians
    float vradius;      // R, canvas-height units
    float aspect;
};
in vec2 st;
out vec4 frag_color;
void main() {
    vec2 P = vec2(st.x * aspect, st.y);
    vec2 V = vec2(center.x * aspect, center.y);
    vec2 rel = P - V;
    float d = length(rel);
    float theta = -strength * exp(-d / vradius);
    float s = sin(theta), c = cos(theta);
    vec2 P_src = V + vec2(c * rel.x - s * rel.y, s * rel.x + c * rel.y);
    frag_color = texture(sampler2D(tex_current, smp_field),
                         vec2(P_src.x / aspect, P_src.y));
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
