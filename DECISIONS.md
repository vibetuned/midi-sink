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

## Step 2

12. **The core owns a per-frame autorelease pool on Apple platforms.**
    Every Metal pass encoder/drawable is an autoreleased ObjC object; a plain
    C render loop (this harness, and any non-runloop host) has no draining
    pool, so at 1,000 passes/frame RSS grew ~4 GB/s in the stress test. Fixed
    with `objc_autoreleasePoolPush/Pop` wrapped around each frame (and each
    target recreation) via `sumi_swapchain_frame_pool_push/pop`; the same code
    path serves iOS. Non-ObjC backends will implement these as no-ops. Fix
    verified: flat 147 MB RSS over the same stress run.

13. **`sumi_set_params` clamps `sim_scale` to (0, 2]** (header comment
    "(0,1]..2"), and computed target dimensions are guarded to [8, 8192] so no
    window/scale combination can exceed GPU texture limits. §4.1's "clampable
    to e.g. 2048²" is read as *the user can clamp via sim_scale*, not a hard
    engine cap (a hard cap would silently change visuals between a MacBook and
    a phone).

14. **Stress hook = `SUMI_STRESS_SWAPS` env var**, parsed once in
    `sumi_create`, pushing N passthrough deforms per `sumi_update`. Chosen
    over an ABI addition (the contract must not carry test-only entry points)
    and over harness-side pushes (no public path to enqueue passthrough passes
    exists, by design). Deform queue capacity is 4096/frame — the spec's
    per-frame deform budget (§3.4, later step) is 64, so the headroom exists
    purely for this stress mode.

15. **Resize re-initializes the field to identity.** Until real state-carrying
    deformations exist (later steps), recreating targets at the new simulation
    resolution and re-running identity init is the only correct behavior;
    "without corrupting state" = no stale/garbage texels, which re-init
    guarantees. Revisit (content-preserving rescale?) when the field carries
    performance state worth keeping across resizes.

## Step 3

16. **Naming: the app is `midi-sink`; only the core library is `sumi`.**
    Desktop target/binary and window title renamed accordingly (user
    direction, 2026-08-31). ABI (`sumi_*`), core library output name, and
    header stay `sumi`.

17. **One orientation convention everywhere: texture space, v grows down.**
    The fullscreen-triangle vertex shaders emit `st = (u, 1 − v_clip)` — the
    texture-space coordinate of the fragment's own texel — and every pass
    (identity, deforms, composite) works purely in that space. Found the hard
    way: sampling at the raw interpolant made every offscreen pass vertically
    flip the field (Metal NDC y-up vs texture row 0 = top), so consecutive
    deformations cancelled instead of composing; identity+composite and
    even-count stress runs masked it. With `st`, passthrough is a true no-op,
    deformations compose exactly (verified: 40 × z/40 tine == 1 × z tine),
    and mouse coords/texture rows/screen rows all share one y-down space.
    This convention must be revisited per backend in phase 2 (GL's row order
    differs) — the flip, if any, belongs in the swapchain/composite boundary,
    never in the deform chain.

18. **Ink phase = parity-derived, in [1,3): `1 + (counter % 2) + radial`**
    (water = 0). A raw `counter + radial` phase breaks in RGBA16F: ULP(512) =
    0.5 destroys band parity past ~256 drops, and a wrapped counter speckles
    seams where far-apart counters touch (interpolation sweeps many integers).
    With the parity form, any two field values interpolate across at most one
    band threshold — no speckle at any drop count (verified at 500). Still
    §4.2-conformant: the phase is *derived from* the monotonic counter, stays
    continuous, and the composite bands it with a periodic function. The raw
    counter is stored in `aux` as the per-drop selector (note: aux itself
    degrades above ~2048 in half float — revisit when palettes land).

19. **Drop radius/lengths are in canvas-height units** (aspect-corrected
    space normalizes y to [0,1]); `sumi_add_drop`'s radius, tine alpha /
    magnitude, and vortex radius all share that unit. Deformation math uses
    the actual field texture's aspect (sim_width/sim_height), not the window's.

20. **Vortex demo is deliberately off-center from the rings** — θ(d) rotation
    concentric with circular rings maps circles to circles (rotation-invariant),
    so a centered vortex is invisible on rings; spirals require an offset
    center or angular content. The mouse gesture (vortex at cursor) is
    naturally off-center.

21. **Pinned dependencies** (all FetchContent):
    - glm `1.0.1`
    - sokol `1847290135f95e57e6d220b0a41208306aafc0dd` (master 2026-08-30)
    - libremidi `v5.4.3`
    - GLFW `3.4`
    - sokol-tools-bin `11d0cf678105d614d675e6d9bd2aaf3eeff12f8c` (sokol-shdc)
