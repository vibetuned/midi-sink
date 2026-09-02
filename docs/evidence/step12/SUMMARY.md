# Step 12 — Linux backend (OpenGL 4.1 core): DONE evidence

All runs: Ubuntu 25.10, gcc 15.2.0, CMake 3.31.6 (Unix Makefiles), 1280×720
window on a ~163 Hz display, GLFW 3.4 (Wayland session; X11/XWayland verified
too), GL 4.1.0 core context on NVIDIA GeForce RTX 5090 (driver 610.43.02),
`--exit-after`-scripted. Evidence images are paper-dip print exports
(`--dip-at`/`--print-out`), not screenshots. MIDI virtual sources: `snd_seq`
virtual ports created by `tests/mpe_stress_alsa.cpp` / `tests/wind_breath_alsa.cpp`
(ALSA ports of osmose_stress.swift / wind_breath.swift, byte-identical
schedules).

Per §4.6 the orientation handling landed at the shader-dialect level
(DECISIONS_2 #20): `@glsl_options flip_vert_y` on every offscreen vertex
shader (GLSL outputs only — MSL/HLSL are byte-identical, Metal/D3D11
untouched), an unflipped VS for the final on-screen composite (GL's bottom-up
default-framebuffer scanout is itself the §4.6 flip), and **zero** flips in
the readback paths — the PBO copy is a straight memcpy, mirroring D3D11's
zero-flip result.

| DONE criterion | Result | Evidence |
|---|---|---|
| Cross-backend field regression passes (proving the deform chain has zero GL branches) | **PASS vs the committed D3D11 dump**: max\|Δ\| **1.34e-3** (≤ 1e-2), mean\|Δ\| **1.35e-8** (≤ 1e-4), aux channel bit-identical, no orientation correction applied. Two GL runs bit-identical (cmp). Control build with `flip_vert_y` removed **FAILs** at max\|Δ\| 1.99 / mean 7.5e-2 with the v channel mirrored — the dialect flip is load-bearing and is the only thing standing between the chain and per-pass mirroring (the GL twin of DECISIONS #17) | `field_regression.log`, `field_512_gl.bin`, `field_determinism_gl.log`, `field_noflip_control.log` |
| Dip PNG matches the reference print of the same script pixel-for-pixel in orientation, within tolerance in tone | Same-input scripted prints (burst: 12 centered drops) vs the committed step-11 D3D11 prints: **max\|Δ\| = 1 LSB, 99.97% of pixels bit-identical, 100% within 2 LSB** — orientation matches pixel-for-pixel, tone delta is 1 LSB (measured max). Chevron/vortex prints match in orientation, chirality and structure (stem top / trunk bottom / swirl direction); their committed step-11 counterparts each contain one stray extra-drop feature (visible in both), so full-image parity is structural there, while ink-free paper regions still agree within 1 LSB. Metal-print comparison pending a Mac run (below) | `print_parity.log`, `burst_print_newest.png`, `burst_print_oldest.png`, `chevron_print.png`, `vortex_print.png` |
| On-screen output visually matches captures of the same scripted performance | The on-screen composite is the same pass the print exports (§4.5 manual sRGB, §5.3 print = composite into RGBA8), and the prints above match the validated D3D11 captures; eyeballed live during every run — right-side up, no mirroring. Composite invariant: washi grain **pixel-locked (max\|Δ\| = 0)** across dips and across runs while ROLL_H ink translated **+33 px in 0.2 s** (expected 32 at 120 BPM × 0.0625) — ink rides the scroll, paper does not, nothing is flipped in the wrong pass | `roll_grain_lock.log`, `run_resize.log` (900×500 → 1440×900 live resize, 161.7 fps, no artifacts) |
| Step 3–7 checks: mouse-marbling sharpness (drop/tine/vortex) | Chevron: 12 concentric rings combed into the sharp-edged Jaffer chevron, feed stem at top; vortex: clean offset spiral, correct chirality; dip-window worst frame 10.04 ms | `chevron_print.png`, `vortex_print.png`, `run_chevron.log`, `run_vortex.log` |
| Step 3–7 checks: MPE stress at 60 fps with 0 dropped MIDI | **69,023 messages / 30 s** (10 voices × 200 press/s + bends + CC74 after MCM) through the `snd_seq` virtual port: **162.5 fps avg, 0 dropped MIDI**; MCM decoded (lower zone, 15 members); the 1 Hz rescan (DECISIONS #25) opened the ALSA port as-is; dip mid-stress: print saved, dip-window worst frame **12.04 ms** (no hitch) | `run_stress.log`, `run_stress_err.log`, `feeder_stress.log`, `mpe_stress_print.png` |
| Step 3–7 checks: wind mode | Mode auto-detected (`input mode -> wind`), 3,085 msgs / 20 s breath stream, calligraphic wandering line with breath-proportional width, 162.7 fps, 0 drops | `wind_print.png`, `run_wind.log`, `run_wind_err.log`, `feeder_wind.log` |
| Step 3–7 checks: paper dip export, double-buffer contract | Burst test: dips at t/t+0.2/t+0.25 → both buffers fill, **third dip refused with WARN**, both prints read back intact (newest then oldest), 162.5 fps | `run_burst.log`, `run_burst_err.log`, `burst_print_newest.png`, `burst_print_oldest.png` |
| `ctest` green, clean build | Full clean rebuild with **zero warnings** in project code; `abi_c_compile` (links `libsumi.so`) + `normalizer_tests` (8,934 checks) both pass unchanged | `build_full.log`, `ctest.log` |

Not re-run here: the step-3 40×(z/40)-tine compose check is subsumed by the
field regression — compose-exactness across 7 mixed passes is precisely what
it verifies cross-backend (and the no-flip control shows what a compose break
looks like).

Measurement note: the very first window after boot/session idle can report
~29 fps — the compositor throttles freshly mapped, occluded windows; every
attended run paces at the display's ~163 Hz. `run_resize.log` was recaptured
attended.

**Still pending for tri-backend closure** (carried over from step 11's
"macOS revalidation needed"): `tests/fixtures/` still has no Metal fixture —
the GL regression above compares against the committed D3D11 dump
(`docs/evidence/step11/field_512_d3d11.bin`). On the Mac, run
`midi-sink --field-dump tests/fixtures/field_512_metal.bin`, commit it, and
run `field_dump_compare` against both the GL and D3D11 dumps; a Mac
chevron/vortex/burst print set would also close the Metal half of the print
parity (the step-11 seam refactor still wants its macOS re-run). Note the
step-11 committed chevron/vortex prints each contain a stray extra drop —
worth recapturing on any backend when convenient.

New tools: `tests/mpe_stress_alsa.cpp`, `tests/wind_breath_alsa.cpp`
(snd_seq virtual-port feeders). Harness: no new flags — the Linux path is
`GLFW_OPENGL_API` 4.1 core hints + `glfwMakeContextCurrent` before
`sumi_create(NULL, SUMI_BACKEND_GL)` + `glfwSwapBuffers` after `sumi_render`.
Core: `core/src/swapchain_gl.cpp` (SOKOL_IMPL host, §5.1 validation, PBO
readback), `composite.glsl` split into screen/print programs, composite
swapchain pipeline format now inherits the environment default
(DECISIONS_2 #20–#23).
