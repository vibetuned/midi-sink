// deform.glsl — ping-pong displacement-field passes (PROJECT_SPEC.md §4.1–§4.3).
// Step 2: identity init + passthrough (the minimal read-current/write-next pass
// exercising the ping-pong machinery). Drop/tine/vortex arrive in later steps.
//
// Texel payload (§4.2): (u, v, ink, aux). u/v are continuous pre-image
// coordinates, safe to sample with linear filtering.

// Fullscreen triangle via gl_VertexIndex — no vertex buffer.
@vs deform_vs
out vec2 uv;
void main() {
    uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
@end

// Identity init (§4.1): every texel stores its own normalized coordinate,
// ink and aux start at 0.
@fs identity_fs
in vec2 uv;
out vec4 frag_color;
void main() {
    frag_color = vec4(uv, 0.0, 0.0);
}
@end

// Passthrough: read tex_current at the texel's own coordinate, write to
// tex_next unchanged (linear sampling per §4.2).
@fs passthrough_fs
layout(binding=0) uniform texture2D tex_current;
layout(binding=0) uniform sampler smp_field;
in vec2 uv;
out vec4 frag_color;
void main() {
    frag_color = texture(sampler2D(tex_current, smp_field), uv);
}
@end

@program deform_identity deform_vs identity_fs
@program deform_passthrough deform_vs passthrough_fs
