// Placeholder shader proving the sokol-shdc GLSL -> MSL/HLSL5/GLSL410/GLES3
// pipeline is wired (roadmap step 1). Not referenced by any pass; replaced by
// deform.glsl / composite.glsl in later steps.
@vs placeholder_vs
in vec2 position;
void main() {
    gl_Position = vec4(position, 0.0, 1.0);
}
@end

@fs placeholder_fs
out vec4 frag_color;
void main() {
    frag_color = vec4(1.0, 1.0, 1.0, 1.0);
}
@end

@program placeholder placeholder_vs placeholder_fs
