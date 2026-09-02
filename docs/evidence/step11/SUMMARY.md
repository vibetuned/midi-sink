# Step 11 — Windows backend (D3D11): DONE evidence

All runs: Windows 11, MSVC 19.44 (VS 2022), 1280×720 window @165 Hz display,
D3D11 feature level 11.1, `--exit-after`-scripted. Evidence images are
paper-dip print exports (`--dip-at`/`--print-out` — the composited print the
engine itself reads back), not screenshots. MIDI virtual source: loopMIDI
port fed by `tests/mpe_stress_win.cpp` (WinMM port of osmose_stress.swift).

Per §4.6 the step added **zero flip code**: D3D11 shares Metal's top-left row
origin, and the deformation chain is untouched — the prints below are
correctly oriented straight off the shared shaders (HLSL5 via sokol-shdc).

| DONE criterion | Result | Evidence |
|---|---|---|
| Both static and shared artifacts link on MSVC (DECISIONS #7 resolved → DECISIONS_2 #14: objects compiled twice, no .def) | `sumi_static.lib` + `sumi.dll` both link; `abi_c_compile` (DLL, dllimport via `SUMI_USE_SHARED`) and `abi_c_compile_static` both pass; full ctest 3/3 green, normalizer suite **8,934 checks** unchanged | `build_full.log` (lines 58–63), `ctest.log` |
| Mouse-marbling sharpness (step-3 checks: rings, tine chevron, vortex; same gesture code path as mouse input) | Chevron: 12 concentric rings combed into the classic sharp-edged Jaffer chevron, washi grain screen-locked. Vortex: clean spiral shear, no aliasing/tearing. Resize test recreates swapchain + targets live (900×500 → 1440×900), no artifacts, 163 fps | `chevron_print.png`, `vortex_print.png`, `run_resize.log` |
| MPE stress at 60 fps with 0 drops (step-5 check) | 69,023 messages / 30 s (10 voices × 200 press/s + bends + CC74 after MCM) via loopMIDI/WinMM: **165.9 fps avg, 0 dropped MIDI**; MCM decoded (MPE lower zone, 15 members); 1 Hz port rescan (DECISIONS #25) opened the port as-is | `run_stress.log`, `run_stress_err.log`, `feeder_stress.log` |
| Dip export (step-7 check): async print readback via D3D11 staging texture, double-buffer contract | Dip mid-stress: print saved, dip-window worst frame **6.94 ms** (no hitch). Burst test: dips at t/t+0.2/t+0.25 → both buffers fill, **third dip refused with WARN**, both prints read back intact (newest then oldest) | `run_stress_print.log`, `mpe_stress_print.png`, `run_burst.log`, `run_burst_err.log`, `burst_print_newest.png`, `burst_print_oldest.png` |
| Cross-backend field regression (§4.6): `--field-dump` vs the Metal fixture | **D3D11 side complete, comparison pending the Metal golden.** Dump lands (512×512 RGBA32F, header + rows, row 0 = top); field values verified (drop 1 band 1.x/aux 0, drop 2 band 2.x/aux 1, scroll-shifted identity elsewhere). Two independent D3D11 runs are **bit-identical (max Δ = 0)** — the 1e-2/1e-4 tolerance budget is purely for cross-GPU differences. Once macOS runs `midi-sink --field-dump tests/fixtures/field_512_metal.bin` and commits it: `field_dump_compare field_512_d3d11.bin tests/fixtures/field_512_metal.bin` | `field_512_d3d11.bin`, `field_determinism_d3d11.log`, DECISIONS_2 #18 |

Not re-run here: the step-3 40×(z/40)-tine compose check and step-6 CC-routing
checks are shader/mapper-level and platform-independent (the shared HLSL5 is
generated from the same GLSL; the mapper is covered by the 8,934-check suite,
which passes bit-for-bit on MSVC).

**macOS revalidation needed** (blind refactors, see DECISIONS_2 #15/#16):
the readback seam is now backend-neutral (`readback_begin(sg_image, w, h,
bpp)`), the renderer's skip-frame check keys on swapchain width, and backend
validation moved into the swapchain TUs — re-run the step-7/8 dip checks and
produce the field fixture on the Mac.

Harness additions: `--field-dump <path>` (§4.6 dump, exits after writing).
New tools: `tests/mpe_stress_win.cpp` (WinMM feeder, needs a loopMIDI port),
`tests/field_dump_compare.c` (tolerance comparator), `build_win.bat` (vcvars
+ CMake/Ninja PATH wrapper).
