# Agent brief — Step 14: Android shell (Jetpack Compose)

You are working in the `midi-sink` repository on the Linux box (Android SDK +
NDK + a connected mid-range test device). This file is your task prompt. The
project is a Suminagashi (ink-marbling) MPE visualizer: a portable C++20 core
(`libsumi`, pure C ABI in `core/include/sumi_core.h`) rendering closed-form
Jaffer marbling via sokol_gfx. Steps 1–13 are complete: macOS/Metal,
Windows/D3D11, Linux/GL 4.1 (you built that one — the flip discipline and GL
swapchain patterns are yours), and the iOS SwiftUI shell. Your job is Step 14
only.

Read first, in this order:
1. `CLAUDE.md` — binding working rules. Highlights: **never run `git commit`
   — the user commits**; every sokol call stays behind `core/src/renderer.cpp`
   / `core/src/swapchain_*`; no exceptions/STL/callbacks-into-C++ across
   `sumi_core.h`; log every resolved ambiguity in `DECISIONS_2.md`; nothing
   from later steps.
2. `PROJECT_SPEC_2.md` §5.4 (Android bridge AND the **Android teardown
   contract — a hard requirement, read it verbatim**), §5.1 (GL backend:
   host owns the EGL context, `native_surface_handle = NULL`), §5.2
   (threading: all sumi_* calls on ONE render thread; `sumi_push_midi` from
   exactly one other thread), §4.6, §7 (glsl300es), the `sim_scale` params
   comment.
3. `DECISIONS.md` #12, #24; `DECISIONS_2.md` #20–23 (your own GL findings),
   #15 (readback seam), #25–28 (the iOS step: hotplug, sim_scale heuristic,
   resize preservation — all have Android analogs).

## The step (verbatim)

> **Step 14 — Android shell (Jetpack Compose)**
>
> Spec sections: §5.4 (JNI bridge: SurfaceView → ANativeWindow → EGL
> host-owned context), §5.1 (GL backend rules), §4.6 (flip isolation —
> already proven in Step 12), params comment (sim_scale 0.75 default). §7
> (glsl300es output).
>
> - Compose AndroidView wrapping SurfaceView; JNI wrapper (one .cpp)
>   forwards Surface lifecycle: surfaceCreated → ANativeWindow_fromSurface →
>   EGL context on a dedicated render thread → sumi_create(NULL, GL);
>   surfaceChanged → sumi_resize; surfaceDestroyed → blocking teardown per
>   the §5.4 Android teardown contract: the UI thread waits (mutex + condvar
>   or thread join) until the render thread finishes its in-flight frame and
>   unbinds the EGL surface, then releases the ANativeWindow. Never return
>   from surfaceDestroyed while the render thread can still touch the
>   surface.
> - MIDI: AMidi (API 29+) → sumi_push_midi from the MIDI thread; the
>   harness-side producer mutex from DECISIONS #24 ports to the JNI layer.
> - sim_scale default 0.75; drop to 0.6 under THERMAL_STATUS_SEVERE via
>   PowerManager thermal listener (host-side — the core never detects
>   devices).
> - Touch gestures as on iOS.
>
> DONE when: the Osmose stress script (Step 5) holds ≥ 55 fps for 10 minutes
> on a mid-range test device at sim_scale 0.75 with graceful thermal
> degradation logged; GLES3 output passes the cross-backend field regression
> test; surface rotate/destroy/recreate cycles (10× scripted, triggered
> mid-frame during the heavy stress script to exercise the teardown race)
> produce zero EGL errors or native crashes and leak nothing (Android Studio
> memory profiler evidence); Bluetooth MIDI from the ROLI works end-to-end.

## Codebase orientation (validated facts + landmines)

- **Follow the iOS precedent for build wiring** (step 13, committed): the
  root `CMakeLists.txt` already has the `if(CMAKE_SYSTEM_NAME STREQUAL
  "iOS")` pattern — fetch only glm+sokol, build only `core/`, static archive
  only (`SUMI_ARTIFACTS` in `core/CMakeLists.txt`). Add the equivalent
  `ANDROID` guards. Recommended shape: Gradle `externalNativeBuild` points
  CMake at the REPO ROOT (the NDK toolchain sets `ANDROID`), and an
  `if(ANDROID) add_subdirectory(android/cpp)` builds the one JNI shared lib
  (your .cpp + `$<TARGET_OBJECTS>`/link of `sumi_static`, plus `GLESv3 EGL
  android amidi log`). Keep the app in `android/` (Compose, Kotlin) the way
  the iOS shell lives in `ios/`.
- **swapchain_gl.cpp hosts Android too — with `SOKOL_GLES3`.** The TU
  currently defines `SOKOL_GLCORE` for desktop Linux. Switch the define on
  `__ANDROID__` inside that TU (backend selection lives in swapchain TUs —
  DECISIONS_2 #16 — so this is the sanctioned place). Audit the TU for
  desktop-GL-only calls while you're there: GLES3 has PBOs and fence sync
  (your #22 readback design ports), but check every entry point against ES
  3.0. sokol's GLES3 path links GLES3 headers directly — no loader.
- **RGBA16F color attachments are NOT core ES 3.0.** The field targets are
  RGBA16F render targets; on GLES3 that requires `EXT_color_buffer_half_float`
  (or ES 3.2). Nearly every 2019+ device has it, but verify at
  `sumi_create` in the swapchain TU and fail loudly with a clear log if
  missing — a silent incomplete-FBO is misery to debug. Reading the field
  back (regression test) also needs `glReadPixels` with
  RGBA/`GL_HALF_FLOAT` — check `GL_IMPLEMENTATION_COLOR_READ_TYPE` and log.
- **Flip discipline: expect ZERO new flips.** Your `@glsl_options
  flip_vert_y` (#20) applies to the glsl300es output exactly as it did to
  glsl410, and an EGL window surface scans out like the desktop default
  framebuffer: `composite` (unflipped VS) on screen, `composite_print`
  offscreen. The field regression is the oracle — if the GLES3 dump comes
  out mirrored, the bug is real; do not "fix" it in the comparison.
- **Field regression on-device**: the dump hooks (`core/src/sumi_debug.h`)
  are static-link-only and work from the JNI layer. Add a debug entry point
  in the shell (an Intent extra / BuildConfig flag) that runs the canonical
  script (the shared float-literal function from #18 — do NOT re-type the
  numbers) at 512×512 and writes the dump (`w,h uint32 LE + float32 RGBA
  rows, row 0 = top` — decode half→float CPU-side) to app files; `adb pull`
  and compare on the Linux box with `build/tests/field_dump_compare` against
  `tests/fixtures/field_512_metal.bin` (committed) and your step-12 GL dump
  (`docs/evidence/step12/field_512_gl.bin`). Tolerances: max ≤ 1e-2,
  mean ≤ 1e-4 (#18).
- **Threading is the step's real difficulty — unlike iOS, the UI thread is
  NOT the render thread.** All sumi_* calls (create/destroy/resize/update/
  render/set_params/gestures) happen on the dedicated render thread that
  owns the EGL context. Marshal everything from Compose (touch events,
  sim_scale/layout changes, dip) through a small command queue drained at
  the top of each render-thread frame. `sumi_push_midi` is the one
  exception (§5.2): exactly ONE producer thread — AMidi delivers per-device,
  so if more than one MIDI device is open, serialize producers with a mutex
  (DECISIONS #24 ports verbatim to the JNI layer).
- **Teardown contract (§5.4) is a hard requirement and the DONE stress
  targets exactly its race**: `surfaceDestroyed` must BLOCK until the render
  thread finished the in-flight frame and called `eglMakeCurrent(dpy,
  EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)`, and only then
  `ANativeWindow_release`. Keep the EGL context alive across surface
  destroy/recreate (only the window surface is recreated) — the field
  textures live in the context, and with the context preserved, DECISIONS_2
  #28's resize preservation carries the drawing across rotations exactly as
  on the iPad. Put `android:configChanges="orientation|screenSize|
  screenLayout|smallestScreenSize"` on the activity so rotation does NOT
  recreate the activity/instance — only surfaceChanged fires.
- **Resize preservation already exists in core** (#28): `sumi_resize` (and
  sim_scale changes, including your thermal 0.75→0.6 drop) resample the
  field instead of erasing; a pristine field keeps exact identity init so
  the regression dump stays byte-stable. Don't reimplement anything.
- **Stress feeder**: the Step 5 Osmose script exists as
  `tests/mpe_stress_win.cpp` / `tests/mpe_stress_alsa.cpp` (byte-identical
  schedules, 69,023 messages / 30 s, absolute-clock pacing — #19/#23).
  On-device, port the schedule into a debug feeder thread in the JNI layer
  that calls `sumi_push_midi` directly (it IS the single producer while it
  runs; disable AMidi ingestion for the duration), looped for 10 minutes.
  Put the schedule source in `tests/` or share it; log the transport
  decision in DECISIONS_2. fps/thermal evidence: port the iOS shell's 1 Hz
  CSV logger (`ios/Sources/SumiCanvas.swift` — t, fps, worst frame ms,
  thermal status) to the render thread, plus `PowerManager
  .addThermalStatusListener` for the sim_scale drop; `adb pull` the CSV.
- **Touch gestures as on iOS** (`ios/Sources/SumiCanvas.swift` is the
  reference): tap → `sumi_add_drop(x, y, 0.06, 0)`; one-finger drag →
  `sumi_add_tine(x0, y0, x1, y1, 0.035, aspect-corrected segment length)`
  with a ~5 dp threshold; two-finger twist → `sumi_add_vortex(centroid,
  clamp(Δrotation, ±0.5), 0.18)`. Coordinates normalized [0,1], y-down
  top-left — Android view coords already match §4.6, same as UIKit. Route
  them through the render-thread command queue.
- **Scripted rotate/destroy/recreate ×10 mid-stress**: drive it from the
  host — e.g. `adb shell settings put system accelerometer_rotation 0` then
  cycle `settings put system user_rotation 0..3`, or toggle
  multi-window/recents; whatever you choose must destroy and recreate the
  SURFACE while the stress feeder is saturating the render thread. Zero EGL
  errors (`eglGetError` checked and logged at every EGL call), zero native
  crashes (`adb logcat -b crash`), flat memory (Android Studio profiler or
  an `adb shell dumpsys meminfo` series — save the evidence).
- **Bluetooth MIDI (ROLI)**: AMidi only enumerates devices the system knows;
  BLE-MIDI on Android needs `MidiManager.openBluetoothDevice` after a BLE
  scan (BLUETOOTH_SCAN/CONNECT runtime permissions) — Kotlin side, then the
  opened device reaches the JNI AMidi path like any other. The physical
  ROLI is with the user — coordinate the end-to-end test with them.

## Deliverables

- Code as above; desktop Linux build + `ctest` still green (`cmake -B build
  -G Ninja && cmake --build build && ctest --test-dir build`) — the shared
  swapchain TU must not regress your step-12 backend.
- Evidence under `docs/evidence/step14/` with a `SUMMARY.md` DONE table
  (format: `docs/evidence/step13/SUMMARY.md`): 10-min stress CSV (fps ≥ 55,
  thermal timeline + any graceful degradation events), field-regression
  numbers vs the Metal fixture and the GL dump, the ×10 teardown-race log
  (EGL error sweep + crash-log sweep + memory series), BT MIDI end-to-end
  note.
- New `DECISIONS_2.md` entries for every ambiguity resolved (GLES3 define
  split, half-float extension policy, stress transport, thermal step-down,
  teardown implementation choice, anything EGL).
- Report readiness to the user with the evidence reference for their commit
  message. **Do not commit.**
