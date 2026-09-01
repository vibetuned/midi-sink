# Agent brief — Step 12: Linux backend (OpenGL 4.1 core)

You are working in the `midi-sink` repository on a Linux machine. This file
is your task prompt. The project is a Suminagashi (ink-marbling) MPE
visualizer: a portable C++20 core (`libsumi`, pure C ABI in
`core/include/sumi_core.h`) rendering closed-form Jaffer marbling
deformations via sokol_gfx, plus a GLFW desktop harness. Steps 1–10 are
complete and validated on macOS/Metal; Step 11 (Windows/D3D11) may or may not
have landed before you start — check `git log` and `DECISIONS_2.md`. Your job
is Step 12 only.

**Sequencing note:** Step 11 owns two pieces of shared plumbing you also
need — the backend-neutral print-readback seam (replacing the Metal-specific
`sg_mtl_query_image_info` call at `core/src/renderer.cpp:173`) and the
`--field-dump` cross-backend regression tool (canonical script + dump format,
described in `docs/handoff/STEP11_WINDOWS_AGENT.md`). If Step 11 has landed,
build on its seam and reuse its dump format/fixture unchanged. If it has NOT
landed, implement both yourself per that brief's specification (identical
format and canonical script — the two implementations must converge) and
flag the overlap to the user so they can reconcile the commits.

Read first, in this order:
1. `CLAUDE.md` — the working rules. They are binding. Highlights:
   **never run `git commit` — the user commits everything themselves**; keep
   every sokol call behind `core/src/renderer.cpp` / `core/src/swapchain_*`;
   no exceptions/STL/callbacks-into-C++ across `sumi_core.h`; log every newly
   resolved ambiguity in `DECISIONS_2.md`; do not implement later-step work
   (Android/GLES3 and WebGPU are later steps — target desktop GL 4.1 core
   only).
2. `PROJECT_SPEC_2.md` §5.1 (the GL exception: the HOST owns the context —
   `native_surface_handle = NULL`, `backend = SUMI_BACKEND_GL`, core renders
   into the currently bound default framebuffer, host swaps buffers), §4.6
   (bottom-left row origin — **the flip lands here and ONLY here**: final
   swapchain composite + print readback path), §5.3 (async print readback
   contract), §7 (shader outputs — `glsl410` is already generated, see
   DECISIONS #2).
3. `DECISIONS.md` #1 (SOKOL_IMPL hosting pattern), #2 (glsl410 dialect), #25
   (libremidi hotplug is polled, ports as-is); `DECISIONS_2.md` for the v2
   history and anything Step 11 added.

## The step (verbatim)

> **Step 12 — Linux backend (OpenGL 4.1 core)**
>
> Spec sections: §5.1 (GL exception: host owns the context, handle = NULL),
> §4.6 (bottom-left row origin — the flip lands here, only at the swapchain
> composite + print path), §7 (glsl410 output per DECISIONS #2).
>
> - `core/src/swapchain_gl.cpp`: thin — binds the default framebuffer,
>   applies the §4.6 composite flip, hosts SOKOL_IMPL for GL builds.
> - Harness: GLFW GL 4.1 core context created host-side, made current on the
>   render thread before `sumi_create`; host swaps buffers. libremidi ALSA
>   backend.
> - Print readback via PBO; verify the exported PNG is not vertically
>   flipped (the flip must be applied exactly once — this is the classic GL
>   bug).
>
> DONE when: cross-backend field regression test passes (proving the deform
> chain has zero GL branches); on-screen output visually matches macOS
> captures of the same scripted performance; the dip PNG matches the Metal
> dip PNG of the same script pixel-for-pixel in orientation and within
> tolerance in tone; Steps 3–7 checks pass on Linux.

## Codebase orientation (validated facts, found the hard way on macOS)

- **Build**: `cmake -B build -G Ninja && cmake --build build && ctest
  --test-dir build`. All dependencies (glm, sokol, libremidi, GLFW, stb,
  sokol-shdc prebuilt) are FetchContent/download-pinned in `CMakeLists.txt`
  and `cmake/CompileShaders.cmake` — the shdc helper already knows the linux
  binary names. You'll need distro GL/X11 dev packages for GLFW (and ALSA
  headers for libremidi). `tests/normalizer_tests.cpp` (8,934 checks) and
  `tests/abi_c_compile.c` are GPU-free and must pass unchanged.
- **One y-down texture space (§4.6) — read this twice.** Every pass samples
  at `st = (u, 1 − v_clip)` emitted by the fullscreen-triangle VS
  (`core/src/shaders/deform.glsl`), and the whole deformation chain
  (identity, drop, tine, vortex, scroll) composes in that one space. On
  Metal, sampling the raw NDC interpolant made consecutive deformations
  CANCEL (each offscreen pass flipped the field) — that is why this
  convention exists. Therefore: **zero flips inside the deformation chain,
  zero `#ifdef` / uniform-driven GL branches in `deform.glsl` or the
  ping-pong dispatch.** GL's bottom-left row origin is corrected in exactly
  two places: (1) the final on-screen composite pass in `swapchain_gl.cpp`
  (flip the composite VS/sample y for the default framebuffer), (2) the
  print readback path (row order of the PBO copy). Note sokol_gfx already
  papers over most GL origin differences for offscreen render targets —
  verify empirically which flips are actually needed rather than assuming;
  the regression test below is your oracle. The classic failure mode is a
  double flip that looks correct on screen but exports an upside-down PNG
  (or vice versa).
- **SOKOL_IMPL hosting (DECISIONS #1)**: `swapchain_metal.mm` defines
  `SOKOL_IMPL` + `SOKOL_METAL` on macOS. Mirror it: `swapchain_gl.cpp`
  defines `SOKOL_IMPL` + `SOKOL_GLCORE`, is the only TU touching GL entry
  points, and stays thin — with a host-owned context there is no
  device/swapchain to create; it binds the default framebuffer, tracks
  drawable size on resize, and hosts the readback. GL function loading:
  sokol_gfx loads its own GL functions on desktop GL — do not add a loader
  library.
- **Print readback (PBO)**: same async double-buffer contract as Metal
  (`core/src/swapchain.h`): `readback_begin` must never block the render
  loop (glReadPixels into a PBO + fence/poll with `glClientWaitSync(…, 0)`
  each frame), `readback_poll` returns 0 idle / 1 in flight / 2 completed,
  two print buffers, a third dip while both are unconsumed is refused with a
  WARN. The print target is an offscreen sokol image — get its GL texture
  name via `sg_gl_query_image_info()` inside `swapchain_gl.cpp` through the
  backend-neutral seam (see the sequencing note; renderer.cpp must not name
  any backend API).
- **Clang flags are fine on Linux** (`-fvisibility=hidden -fno-exceptions
  -fno-rtti`), but the top-level CMake enables OBJC/OBJCXX and adds Apple
  frameworks — make sure those are `if(APPLE)`-guarded (Step 11 may have
  already done it; don't duplicate).
- **Harness** (`desktop/src/main.cpp`): create the context host-side —
  `GLFW_CONTEXT_VERSION 4.1, GLFW_OPENGL_CORE_PROFILE,
  GLFW_OPENGL_FORWARD_COMPAT` off (Linux), `glfwMakeContextCurrent` BEFORE
  `sumi_create(native_surface_handle=NULL, backend=SUMI_BACKEND_GL)`, and
  `glfwSwapBuffers` after `sumi_render`. Existing test flags to reuse:
  `--exit-after`, `--resize-test`, `--drop-test`, `--demo-chevron`,
  `--demo-vortex`, `--layout N`, `--dip-at t`, `--print-out path`,
  `--map-cc`. libremidi v5.4.3 selects ALSA automatically; keep
  `track_virtual = true` and the 1 Hz port rescan exactly as-is (DECISIONS
  #25). For MIDI feeding, ALSA virtual ports via `aconnect`/`amidi` or a
  small C feeder using `snd_seq` work well — port the needed evidence
  feeders into `tests/` (the `.swift` ones there are macOS/CoreMIDI).
- **Composite invariants to eyeball**: washi grain is procedural in SCREEN
  space — it must stay pixel-locked while the field scrolls (roll layouts,
  key `L` cycles, `B` nudges BPM). If the grain flips or scrolls, you flipped
  in the wrong pass.

## DONE verification specifics

- **Field regression**: run the canonical `--field-dump` script (Step 11
  brief defines it) and compare against the committed Metal fixture in
  `tests/fixtures/` (if absent, produce your dump and ask the user to
  generate the Metal fixture on the Mac with the same flag). This test
  passing proves the deform chain has zero GL branches — it reads the FIELD
  texture, before any composite flip, so it must match Metal without any
  orientation correction. If your field dump comes out vertically mirrored,
  the bug is in the chain, not the comparison.
- **Dip PNG parity**: run an identical scripted performance
  (`--demo-chevron` / `--drop-test` + `--dip-at`/`--print-out`) on Linux and
  compare with the same run's Metal print (ask the user for the Mac PNG, or
  use fixtures the user provides): orientation must match pixel-for-pixel;
  tone within tolerance (different GPUs — small LSB differences are fine,
  document the measured max delta).
- **Steps 3–7 checks**: mouse marbling (drop/tine/vortex feel + 40-passes ==
  1-pass tine sharpness), MPE stress at 60 fps with 0 dropped MIDI, wind
  mode, paper dip export. Capture evidence as dip-print exports, not
  screenshots.

## Deliverables

- Code as above; `ctest` green.
- Evidence under `docs/evidence/step12/` with a `SUMMARY.md` DONE table
  (mirror `docs/evidence/step10/SUMMARY.md` for format), including the
  field-regression numbers, the orientation check, and fps/drop counts.
- New `DECISIONS_2.md` entries for every ambiguity you resolve (which flips
  proved necessary and where, PBO poll design, tolerances, ALSA specifics).
- Report readiness to the user with the evidence reference for their commit
  message. **Do not commit.**
