// composite.glsl — displacement field -> swapchain (PROJECT_SPEC.md §4.5).
// Ink phase -> alternating rings; procedural simplex washi mulberry-fiber
// noise + absorption grain (strength = paper_roughness); three palettes
// (sumi black / indigo / ochre) morphable via palette_morph, with per-drop
// hue offsets from the aux channel. All color math in LINEAR space; the
// final write is sRGB-encoded (§4.5, see DECISIONS.md).

@vs composite_vs
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

vec3 srgb_encode(vec3 c) {
    vec3 lo = c * 12.92;
    vec3 hi = 1.055 * pow(max(c, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055;
    return mix(lo, hi, step(vec3(0.0031308), c));
}

void main() {
    vec4 field = texture(sampler2D(tex_field, smp_field), st);
    float phase = field.z;
    float aux = field.w;

    // Washi paper (§4.5): two directional ridged simplex layers = mulberry
    // fiber strands, plus sizing mottle and fine absorption grain. Isotropic
    // space so fibers do not stretch with the window.
    vec2 p = vec2(st.x * aspect, st.y);
    const mat2 R1 = mat2(0.9659, 0.2588, -0.2588, 0.9659);    //  15 deg
    const mat2 R2 = mat2(0.8192, -0.5736, 0.5736, 0.8192);    // -35 deg
    float strand1 = 1.0 - abs(snoise((R1 * p) * vec2(2.2, 90.0)));
    float strand2 = 1.0 - abs(snoise((R2 * p) * vec2(1.7, 70.0) + 13.7));
    float strands = pow(max(strand1, strand2), 10.0);
    float mottle = snoise(p * 9.0) * 0.5 + 0.5;
    float grain  = snoise(p * 420.0) * 0.5 + 0.5;

    vec3 paper = vec3(0.900, 0.868, 0.790);                   // linear washi cream
    paper *= 1.0 - roughness * (0.10 * mottle + 0.05 * grain);
    paper += vec3(0.060, 0.055, 0.045) * (roughness * strands);

    // Palette morph: base palette blended toward the next (§2.2 Flex).
    int id0 = int(clamp(palette_id, 0.0, 2.0) + 0.5);
    int id1 = (id0 + 1) - 3 * ((id0 + 1) / 3);
    float m = clamp(palette_morph, 0.0, 1.0);
    vec3 ink    = mix(pal_ink(id0),    pal_ink(id1),    m);
    vec3 accent = mix(pal_accent(id0), pal_accent(id1), m);
    vec3 clearw = mix(pal_clear(id0),  pal_clear(id1),  m);

    vec3 col = paper;
    if (phase >= 1.0) {
        float band = mod(floor(phase), 2.0);
        if (band >= 1.0) {
            // Ink band: per-drop hue offset from the continuous aux selector
            // (golden-ratio spread; slide shifts it live, §3.4).
            float hue_t = fract(aux * 0.6180339887);
            vec3 c = mix(ink, accent, 0.45 * hue_t);
            // Absorption: ink soaks into rough paper along grain and fibers.
            float soak = roughness * (0.28 * grain + 0.35 * strands);
            col = mix(c, paper, clamp(soak, 0.0, 0.6));
        } else {
            // Clear water band between inks: wet-paper tone, fibers showing.
            col = mix(clearw, paper, 0.35 + 0.3 * roughness * strands);
        }
    }

    // "Lift the paper" flash right after a dip.
    col = mix(col, vec3(0.92, 0.90, 0.85), clamp(dip_fade, 0.0, 1.0));

    frag_color = vec4(srgb_encode(col), 1.0);   // linear -> sRGB (§4.5)
}
@end

@program composite composite_vs composite_fs
