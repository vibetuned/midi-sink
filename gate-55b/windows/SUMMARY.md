# Step 55b on the Windows box — the D3D11 tier re-measured under the displacement payload

Box: Windows 11 Pro (26220), MSVC 14.44 (VS 2022), CMake + Ninja through
`build_win.bat`, Release. NVIDIA GeForce RTX 5090, driver 616.64, D3D11.
Tree: `fd7a196` ("Step 55b linux"), `midi-sink 2.0.0-alpha.1-…-gfd7a196
(libsumi 1.2.0)`. Every command from the repo root. The decision entry is
`DECISIONS_7 #7`. The Linux box's evidence sits at `gate-55b/`; this box's
is the `windows/` subfolder so both survive.

| item | result | evidence |
|---|---|---|
| 1. build + ctest | 0 errors (the usual libremidi warnings); **10/10** (`abi_c_compile` and `abi_c_compile_static` at 1.2.0) | — |
| 2. field gate, D3D11, the new fixture | **dx 2.44e-4, dy 3.66e-4, ink 3.91e-3, aux 0; mean 2.26e-6** — PASS at 1e-2 / 1e-4, the negative control red. This box's old tier was max 3.9e-3 (ink) / mean 6.85e-6 (DECISIONS_5 #83): the ink maximum is unchanged (ink is bitwise the old fixture), the mean fell 3×. The maxima sit on the same texels as the Linux box's GL run and the means agree to four digits | `field_gate_d3d11.txt`, `field_gate_d3d11_stdout.txt`, `field_d3d11.bin` |
| 3. composite gate, D3D11 | max channel diff 1 (the D3D11 tier), 25 617 of 1 048 576 channel samples differ (GL: 25 672); the negative control red (65) | `composite_gate_d3d11.txt`, `composite.rgba` |
| 4. palette test | first run **4/5**: the Sumi four `(= recorded)` untouched, the Anod four `(!= recorded)` as predicted; hashes copied into the `d3d11` column (`cases[]` rows 5–8); rebuilt; the rerun **4/5 again with four different hashes** — see below | `palette_test.txt`, `palette_hashes_d3d11.txt`, `palette_test_after.txt`, `diag/` |
| 5. the seventeen self-tests | fifteen at exit 0 with 0 `^FAIL`; **gesture: 2 FAIL** (exit 2) — the Sumi pair differs at 1–18 samples by a payload ULP, the Anod-twist pair at a constant 78 samples; see below | `<test>.txt`, `diag/trace_gesture_*.txt` |
| 6. the soak | **47 of 48**, the crossed pinch's pair the known red (73.10 texels); the exact operators' (b) between 8.11 (ripple-bake) and 21.98 (chladni) texels under the 32-texel tier; the three negative controls red as required. The table equals the Linux box's to the second decimal in every (b) and (c) cell | `soak_all.txt`, `soak_negative.txt`, `soak_table.md` |

## The D3D11 column's Anod four

| case | was (1.1.0 payload, step 46) | now (displacement, 55b — the 5-frame dip) |
|---|---|---|
| medium 1 palette 0 morph 0 | `0d008c5cfd49f9ad` | `9a3d35df09e0c37f` |
| medium 1 palette 1 morph 0 | `532095638db23a4f` | `1b72baa0345056a9` |
| medium 1 palette 2 morph 0 | `5c0893afe0d410bf` | `1024f97d03e48f64` |
| medium 1 palette 1 morph 38 | `b20c62467579a14f` | `493b66a2e532a203` |

## The finding: the bench's dip takes 4 or 5 frames on D3D11

Across 25 palette-test runs each Anod case hashed to its column value about
four times in five and to one of two other values otherwise (p0 21 : 4,
p1 20 : 5, p2 20 : 5, p1-morph 22 : 2 : 1), the cases deviating
independently; the Sumi four never moved. A temporary env-gated trace
(removed from the tree) counted the frames `t19_dip_print` steps until
`sumi_read_print` reports the dip's print: **5 usually, 4 sometimes, 3 on a
fresh instance** (`diag/trace_palette_*.txt`). The D3D11 readback is a
staging `CopyResource` polled with `Map(DO_NOT_WAIT)`, so which frame it
lands on is GPU timing. An Anod scene's state runs on the clock, so a
4-frame dip before an Anod case prints a different picture; a Sumi scene
does not care. The recorded column is the majority (5-frame) outcome; a run
that draws a 4 inside an Anod case says `!= recorded` for that case.

The gesture test shows the same variance: its Sumi check compares a scene
after the FIRST dip (4 frames, always, on a fresh instance) with the same
scene after a later dip (5), and the fields differ at 1–18 samples by a
payload ULP (five traced runs: 1, 1, 18, 1, 1). Its Anod-twist check differs
at a constant 78 samples (row 0 and three texels near (254,33), up to 3e-3)
— not pinned in the timebox. Both are D3D11-only (0 on the Linux box).

Proposed, not done (the author's call, since it moves the Metal and GL Anod
columns if their boxes' constant differs): make `t19_dip_print` cost a fixed
number of frames on every backend — step until ready, then pad to a constant
— so the Anod prints and the gesture pairs are deterministic wherever the
readback is asynchronous. No column was edited to make a run pass.

## Tree

`desktop/src/dev_tools.cpp`: the `d3d11` column's Anod four (rows 5–8 of
`cases[]`) and the comment above them. Nothing else; the diagnostic trace was
built in a side directory and removed.
