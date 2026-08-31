// composite.glsl — displacement field -> swapchain (PROJECT_SPEC.md §4.5).
// Step 2 TEMPORARY visualization: show the stored pre-image coordinates
// directly (red = u, green = v). Ink rings / washi / palettes come later.

@vs composite_vs
out vec2 uv;
void main() {
    uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
@end

@fs composite_fs
layout(binding=0) uniform texture2D tex_field;
layout(binding=0) uniform sampler smp_field;
in vec2 uv;
out vec4 frag_color;
void main() {
    vec4 field = texture(sampler2D(tex_field, smp_field), uv);
    frag_color = vec4(field.x, field.y, 0.0, 1.0);
}
@end

@program composite composite_vs composite_fs
