# DECISIONS

Spec ambiguities resolved during implementation, per the working rules.
Guiding principle: keep the core identical for iOS/Android.

## Step 1

1. **`SOKOL_IMPL` lives in `swapchain_metal.mm`, not `renderer.cpp`.**
   sokol_gfx's Metal backend must be compiled as Objective-C++ with ARC, which
   a plain `.cpp` cannot provide. `renderer.cpp` includes sokol declarations
   only. The working rule ("every sokol call behind renderer.cpp /
   swapchain_*") still holds, and the pattern ports cleanly: each platform's
   swapchain TU hosts the sokol implementation for its backend (identical on
   iOS via `metal_ios`; `swapchain_gl.cpp` will host it for GLES3 on Android).

2. **sokol-shdc dialect `glsl410` instead of the spec's `glsl330`.**
   Current sokol-shdc (pinned sokol-tools-bin commit) dropped the `glsl330`
   output. `glsl410` matches the spec's own phase-2 Linux target ("OpenGL 4.1
   core", spec header + §5.1). GLES3 output is `glsl300es`.

3. **Log level convention for `sumi_log_fn`'s `int level`** (unspecified in
   §5): sokol's numbering — 0 panic, 1 error, 2 warning, 3 info
   (`core/src/log_levels.h`). Lets sokol's logger bridge through unchanged on
   every platform.

4. **Version encoding**: `sumi_version()` returns `(0<<16)|(1<<8)|0` = 256 for
   0.1.0, per the header comment `(maj<<16)|(min<<8)|patch`.

5. **Swapchain pixel format BGRA8Unorm (non-sRGB) for phase 1.** §4.5's
   "write sRGB-encoded swapchain" is a composite-pass concern (a later step);
   deciding it now would risk baking in a format some mobile swapchains
   handle differently. The clear color is authored directly in swapchain
   space. Revisit in the composite step.

6. **Deep indigo clear** = (0.055, 0.050, 0.220, 1.0).

7. **Static + shared built from one CMake OBJECT library.**
   `SUMI_BUILD_SHARED` only changes behavior on Windows (`__declspec`); on
   macOS/iOS/Android the `visibility("default")` attribute serves both
   artifacts, so building objects once is safe. Must be revisited when the
   Windows/D3D11 build lands in phase 2 (objects will need to split, or the
   define moves to an export-map approach).

8. **Harness test flags** `--exit-after <seconds>` and `--resize-test`
   (programmatic 2-stage window resize) exist only in `desktop/src/main.cpp`
   to automate the step DONE checks. Never part of the core.

9. **`leaks --atExit` used as the "short Instruments pass"** for the DONE
   leak check (same malloc-introspection machinery, scriptable in CI).

10. **Placeholder shader** `core/src/shaders/placeholder.glsl` is compiled at
    build time purely to prove the shdc download + cross-compilation wiring
    required by step 1; nothing includes the generated header yet.

11. **Pinned dependencies** (all FetchContent):
    - glm `1.0.1`
    - sokol `1847290135f95e57e6d220b0a41208306aafc0dd` (master 2026-08-30)
    - libremidi `v5.4.3`
    - GLFW `3.4`
    - sokol-tools-bin `11d0cf678105d614d675e6d9bd2aaf3eeff12f8c` (sokol-shdc)
