// composite.glsl — displacement field -> swapchain (PROJECT_SPEC.md §4.5).
// Step 3: ink phase -> alternating black/white rings on paper (§4.2's
// periodic mapping). Washi fibers / palettes / sRGB decisions come later.

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
in vec2 st;
out vec4 frag_color;
void main() {
    vec4 field = texture(sampler2D(tex_field, smp_field), st);
    float phase = field.z;

    const vec3 paper = vec3(0.914, 0.894, 0.845);   // un-inked water / washi
    const vec3 black = vec3(0.055, 0.050, 0.055);   // sumi ink
    const vec3 white = vec3(0.980, 0.972, 0.950);   // "clear" band between inks

    vec3 col = paper;
    if (phase >= 1.0) {
        // Continuous phase in [1,3) -> band: parity derived from the drop
        // counter (§4.2); first drop (phase base 1) is ink-black.
        float band = mod(floor(phase), 2.0);
        col = (band >= 1.0) ? black : white;
    }
    frag_color = vec4(col, 1.0);
}
@end

@program composite composite_vs composite_fs
