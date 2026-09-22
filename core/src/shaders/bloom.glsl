// bloom.glsl — Phase 6 step 43 (the author's glow): THE ANOD BLOOM. A
// screen-space glow after the composite, the way a photograph of a discharge
// blooms in the lens: the composite's LINEAR colour (composite.glsl with
// linear_out) at half resolution is thresholded with a soft knee to its
// emission, blurred down a chain of octaves with the 13-tap downsample and
// back up with a 3x3 tent, each octave adding the one below (Jimenez, "Next
// Generation Post Processing in Call of Duty", 2014), and the composite adds
// the result back before its shoulder toward white. Screen-space by
// construction (§4.5): it never reads the field. Every target is offscreen,
// so the vertex shader is the print's flipped variant.
@vs bloom_vs
@glsl_options flip_vert_y
out vec2 st;
void main() {
    vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
    st = vec2(corner.x, 1.0 - corner.y);
}
@end

@fs bloom_down_fs
layout(binding=0) uniform texture2D tex_src;
layout(binding=0) uniform sampler smp_bl;
layout(binding=0) uniform bloom_down_params {
    vec2  texel;        // one texel of the SOURCE
    float threshold;    // the emission's floor (the first octave only; 0 after)
    float knee;         // the soft knee's half-width
};
in vec2 st;
out vec4 frag_color;
vec3 tap(vec2 o) { return texture(sampler2D(tex_src, smp_bl), st + o * texel).rgb; }
void main() {
    // the 13-tap downsample: the centre quad at half weight, four outer quads at an eighth each
    vec3 A = tap(vec2(-2.0, -2.0)), B = tap(vec2(0.0, -2.0)), C = tap(vec2(2.0, -2.0));
    vec3 D = tap(vec2(-1.0, -1.0)), E = tap(vec2(1.0, -1.0));
    vec3 F = tap(vec2(-2.0,  0.0)), G = tap(vec2(0.0,  0.0)), H = tap(vec2(2.0,  0.0));
    vec3 I = tap(vec2(-1.0,  1.0)), J = tap(vec2(1.0,  1.0));
    vec3 K = tap(vec2(-2.0,  2.0)), L = tap(vec2(0.0,  2.0)), M = tap(vec2(2.0,  2.0));
    vec3 c = (D + E + I + J) * 0.125
           + (A + B + F + G) * 0.03125 + (B + C + G + H) * 0.03125
           + (F + G + K + L) * 0.03125 + (G + H + L + M) * 0.03125;
    if (threshold > 0.0) {
        // the emission: what rises above the glass, with a soft knee so a faint grid line blooms a little
        float br = max(c.r, max(c.g, c.b));
        float soft = clamp(br - threshold + knee, 0.0, 2.0 * knee);
        soft = soft * soft / (4.0 * knee + 1e-5);
        float contrib = max(soft, br - threshold) / max(br, 1e-4);
        c *= clamp(contrib, 0.0, 1.0);
    }
    frag_color = vec4(c, 1.0);
}
@end

@fs bloom_up_fs
layout(binding=0) uniform texture2D tex_small;   // the octave below, to be spread
layout(binding=1) uniform texture2D tex_add;     // this octave's own downsample, added
layout(binding=0) uniform sampler smp_bl;
layout(binding=0) uniform bloom_up_params {
    vec2  texel;        // one texel of the SMALL texture
    float pad0;
    float pad1;
};
in vec2 st;
out vec4 frag_color;
vec3 tap(vec2 o) { return texture(sampler2D(tex_small, smp_bl), st + o * texel).rgb; }
void main() {
    // a 3x3 tent (1 2 1 / 2 4 2 / 1 2 1) over the octave below, added to this octave
    vec3 c = tap(vec2(-1.0, -1.0)) + 2.0 * tap(vec2(0.0, -1.0)) + tap(vec2(1.0, -1.0))
           + 2.0 * tap(vec2(-1.0, 0.0)) + 4.0 * tap(vec2(0.0, 0.0)) + 2.0 * tap(vec2(1.0, 0.0))
           + tap(vec2(-1.0, 1.0)) + 2.0 * tap(vec2(0.0, 1.0)) + tap(vec2(1.0, 1.0));
    c *= 1.0 / 16.0;
    frag_color = vec4(c + texture(sampler2D(tex_add, smp_bl), st).rgb, 1.0);
}
@end

@program bloom_down bloom_vs bloom_down_fs
@program bloom_up   bloom_vs bloom_up_fs
