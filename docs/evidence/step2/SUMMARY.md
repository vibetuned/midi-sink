# Step 2 DONE evidence — ping-pong targets & identity pass

Date: 2026-08-31. Same rig as step 1 (Apple Silicon, macOS 25.6.0).
New in this step: RGBA16F ping-pong field targets (§4.1/§4.2), identity-init +
passthrough deform shaders, deformation queue in displacement.cpp, temporary
u/v-visualizing composite, `sim_scale` respected end to end.

| DONE check | Result | Evidence |
|---|---|---|
| Clean red/green gradient | **PASS** — red = u (left→right), green = v (top→bottom), smooth, no banding/seams | `window_uv_gradient.png` (captured *after* both test resizes) |
| 1,000 ping-pong swaps/frame for 10 s: no memory growth | **PASS** — `SUMI_STRESS_SWAPS=1000`, 12 s run, RSS flat at ~147 MB (±128 KB) from first to last sample; with Metal validation on, flat at ~338 MB. Baseline `leaks --atExit` unchanged from step 1 (288 OS-XPC cycles, zero sumi/sokol frames) | `stress_novalidation.log`, `stress_1000swaps.log`, `stress_memory_samples.log`, `leaks_atexit.log` |
| …and no GPU pipeline stalls | **PASS** — 51.4 fps average while encoding 1,000 full-screen passes/frame (min frame 5.8 ms), no frame-time runaway, no validation errors, CPU/GPU stay pipelined (no vsync deadlock, steady cadence). 1,000 passes/frame is ~16× the future per-frame deform budget (§3.4: 64) | `stress_novalidation.log` |
| Resize recreates targets at correct sim resolution, no corruption | **PASS** — resizes log `sim targets 1800x1000` / `2880x1800` (sim_scale 1.0) and `640x360` for output 2560×1440 at sim_scale 0.25; gradient clean after resizing; zero Metal validation output during resize | `run_validation_resize.log`, `stress_1000swaps.log`, `window_uv_gradient.png` |

**Bug found & fixed by the stress test:** without a frame-scoped autorelease
pool, 1,000 Metal encoders/frame leaked ~4 GB/s (RSS reached 24 GB). The core
now wraps every frame and target recreation in `objc_autoreleasePoolPush/Pop`
via the swapchain layer — the fix applies to iOS unchanged (DECISIONS.md #12).
Before/after: 24 GB climb → flat 147 MB.

Note on "Instruments": memory growth was measured by 1 Hz RSS sampling plus
`leaks --atExit` (the agreed CLI substitute, DECISIONS.md #9); stall absence
by frame-time stats from the harness (min/avg/max printed at exit).

Regression: step-1 checks still green — `ctest` (C11 ABI test) passes,
`sumi_version` 0.1.0, clean `sumi_destroy`.
