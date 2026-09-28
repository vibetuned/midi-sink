# Step 55b — The field as displacement — the Mac's evidence

Decisions `DECISIONS_7 #1` (the payload: (u − x, v − y, ink, aux); the fixture
re-captured once; the Sumi print bitwise; the clamp-to-edge trap; the web tier's
fall to 3.9e-9) and `#2` (the emission floors re-derived from the displacement's
quantum, a quarter of the old step; the #61 experiment repeated: 1.36 rad with no
floor where the old payload gave 0.07), `#3` (the soak's pre-image tier
re-derived to 32: the 8 was the coordinates' freeze), `#4` (the Tab: the
coordinate channels 14× closer to Metal; #88's six strikes alike on the Tab and
the Mac; #71's thin strike survives on the Tab) and `#5` (the thin strike is
the strike again, the author's call). libsumi is 1.2.0.

Machine: the author's Mac (Metal), the desktop bench at 512² on the scripted
clock unless stated. Nothing here needed a device; the Tab's part and the boxes'
are below.

## What changed in the tree

- `core/src/shaders/deform.glsl`: the payload comment; the identity, the
  ingress rule and a drop's interior write `vec4(0.0)`; one `@block field_fetch`
  (`sumi_fetch(src, at)`: sample, re-base by `clamp(src, ½/W…) − at`) included
  in every field-reading pass, all 16 reads through it.
- `core/src/shaders/composite.glsl`: the Anod estimator rebuilds J = I + ∇d,
  `anod_fresh` is "no phase and under half a texel" (the ULP term gone), the
  rounding bias at the displacement's magnitude, the water grid reads the
  payload.
- `core/src/engine.cpp` (1.2.0), `core/include/sumi_core.h` (the payload in
  `sumi_read_field`'s doc), `core/src/renderer.cpp` (comments),
  `tests/abi_c_compile.c` (expects 1.2.0).
- `core/src/voice_mapper.{h,cpp}`: `SUMI_FIELD_QUANTUM` = 2^-13; the burst's
  and the spark's floors one quantum, the stir's two (were 5e-4 / 5e-4 / 1e-3).
- `desktop/src/dev_tools.cpp`: the bench's reader rebuilds pre-images from the
  payload (every check unchanged); the bake test compares displacements to its
  raw baseline; the Anod CPU replica's fresh class and bias; Metal's Anod
  column recaptured; `--spark-shear <f>` beside `--anod-strike-render` for the
  Tab compare.
- `tests/field_dump_compare.c` (channels dx/dy, the doc), `tools/field_gate.py`
  (the corruption's comment).
- `tests/fixtures/field_512_metal.bin`: RE-CAPTURED (DECISIONS_5 #12 restarted).
- `android/` — nothing (the temporary `--es strikes 1 [--ef sparkShear f]`
  hook that drove #88's script on the Tab was removed after the captures;
  `git diff android/` is empty).
- `desktop/src/dev_tools.cpp`, also: `--pair-drift <op>` (the growth law of
  the soak's pairs), `--preset <file.json>` (a shell's session on the bench),
  `--strike-thin` (#71's strike through the lab switch), `SOAK_DEV_EXACT`
  8 → 32 (#3).
- `core/src/voice_mapper.cpp`, `core/src/engine.cpp`, `core/include/sumi_core.h`
  (#5, the author's call): the Anod strike is #71's again — the charge at
  `anod_drop` (default 0.33, was 0.57) and the shear on the full Sumi radius,
  no burst — in the mapper and in `sumi_gesture_tap`; the lab switch that
  ran the experiment is gone. `tests/normalizer_tests.cpp`: the binding-table
  test expects no burst, the band at 2 × the Sumi radius, the charge at 0.33.
- `_work/DECISIONS_7.md` (new, #1–#2), `_work/BOXES_HANDOFF_55B.md`,
  `specs/TO_PROJECT_SPEC.md` (the §4.1–§4.2 transcription pointer),
  `CLAUDE.md` (the invariant restarts from the new fixture).

## The payload — `payload_compare.txt`, `compare_payloads.py`

Old fixture (pre-images) against the new (displacements + the texel's own
coordinate), 512²: ink differs at 0 texels, aux at 0; the pre-images agree to
max 1.6e-3 (seven passes of the old 2^-12–2^-11 rounding) and to 1e-4 over the
bulk; the new payload's mean |d| 0.093, max 0.26 canvas. Before the clamp in the
helper, the corners disagreed by 0.09 — the drop/tine/vortex/swirl passes'
clamp-to-edge semantics (#1).

## The gates — `gates/`

- `field_gate_metal.txt`: green (tier 1e-2 / 1e-4), the negative control red; a
  second dump `cmp`-identical to the fixture (bitwise).
- `composite_gate_metal.txt`: bitwise (0 of 1,048,576 samples differ) against
  the UNCHANGED `composite_512_metal.rgba`.
- `field_gate_webgpu.txt` (headless Chrome, this Mac): dx max 7.6e-5, dy
  1.2e-4, ink 9.8e-4, aux 0, mean 3.9e-9 — PASS at 2.5e-2 / 1e-3 with two
  orders to spare.

## The harness — `harness/`, `palette_test.txt`

Every suite green on the final build (the floors in): wake 2/2, flick 1/1,
rankine 5/5, pressure 6/6, swirl 6/6, ripple-group 3/3, ripple-dip 1/1,
ripple-permanence 1/1, stokeslet 4/4, torsion 8/8, chladni 7/7, burst 9/9,
spark 5/5, chirikov 4/4, anod 9/9, print 10/10, gesture 6/6, palette 5/5 —
91 checks, no threshold touched. The palette test: the Sumi four `= recorded`
without any edit; the Anod four recaptured for Metal
(67fa3753d3c648be, ac238b60aa0ef7a2, b3e652c64f775150, 38ac7ba0c707083d).

## The floors — `floors/` (#2)

`--chladni-test`, the stir on the chromatic grid, 150 frames of 0.0125 rad:

| build | the ring at ρ 0.7 | radial scatter | passes |
|---|---|---|---|
| 1.1.0 coordinates, no floor (#61) | 0.07 rad | — | every frame, rounded back |
| displacement, old floor (1e-3) — `chladni_old_floor.txt` | 1.45 rad | 0.03 | every third frame |
| displacement, NO floor — `chladni_no_floor.txt` | 1.36 rad | 0.05 | every frame |
| displacement, new floor (2 × 2^-13) — `chladni_new_floor.txt` | 1.36 rad | 0.05 | every frame |

The burst, spark, torsion and gesture suites pass with no floor at all
(`*_no_floor.txt`) and with the new floors (`harness/`).

## The soaks and the pre-image drift — `drift/`, `soak_*` (#3)

Two soaks under the old 8-texel tier (`soak_final_floors_old_tier.md`: the
final floors) went red on (b) for every exact operator — the pre-image
deviation after 500 pairs 9–22 texels where Phase 6 had 1.5–6 — with the
ink-mass halves of (b), (c) and (d) bitwise Phase 6's. `--pair-drift <op>`
on both payloads (the old bench built from a worktree at 7ceb111):

| pairs | tine, coordinates | tine, displacement | stir, coord. | stir, displ. | crossed pinch (non-inverse), displ. |
|---|---|---|---|---|---|
| 1 | 0.184 | 0.194 | 2.17 | 2.18 | 1.48 |
| 10 | 0.538 | 0.774 | 2.88 | 3.31 | 8.20 |
| 50 | 1.101 | 2.346 | 4.02 | 6.28 | 23.4 |
| 100 | 1.368 | 3.844 | 4.51 | 8.99 | 36.0 |
| 200 | 1.520 | 6.276 | 4.85 | 13.2 | 51.8 |
| 500 | 1.597 | 11.874 | 5.16 | 22.1 | 72.9 |

Same single-pair residual; the coordinates froze the water's pre-image after
~50 pairs (their 8×8 map: 0.01 texel in the water), the displacement
accumulates the resampler's smoothing as ~N^0.7 everywhere. The tier is
re-derived to 32 (`SOAK_DEV_EXACT`); the final soak under it, on the final
build: `soak_final_tier32.md` — 47 of 48 checks, the one red the crossed pinch
(DECISIONS_5 #17's known non-inverse, 72.9 texels), every exact operator's (b)
between 9.1 and 22.1 texels, every (c) and (d) as Phase 6 had them, fifteen
operators PASS · PASS · PASS; `soak_negative_tier32.txt` — the three negative
controls red as required (the offset pairs at 198 texels > 32, the clamp tines
+14 %, the 15-sub-step wake ×12.6).

## The Tab (GLES, Adreno 730, SM-X906B) — `tab/` (#4)

- `field_gate_gles3.txt`: dx max 1.1e-3, dy 1.2e-3, ink 1.5e-2, aux 4.9e-4,
  mean 7.9e-5 against the new Metal fixture — PASS at 2.5e-2 / 1e-3; under
  the coordinates the same device sat at max 1.51e-2 / mean 6.08e-4 (#88).
- The six-strike compare, both at 1024² under the Tab's own session
  (`tab_last_session.json`: anod_drop 0.33, no grid, the phosphor palette; the
  Mac through the bench's new `--preset`), `strike_compare.py` at 60 levels
  over the glass, ≥ 200 px:

| variant | the Mac (`mac/session*/anod_strikes.png`) | the Tab (`tab/anod_strikes_*.rgba/.png`) |
|---|---|---|
| shear 0.6 (the session) | 6 charges: 6423 6014 5895 5419 5359 5014 px, 3.25 % lit | 6: 6263 5916 5773 5256 5153 4964, 3.18 % |
| shear 2.0 (#88's harsh case) | 6: 8986 8474 7909 6103 5976 5301, 4.08 % | 6: 8684 7946 7556 5529 5337 4802, 3.80 % |
| **#71's thin strike** (`--strike-thin` / `--ei strikeThin 1`), shear 0.6 | 6: 9600 9476 8066 7214 5888 4256, 4.24 % | 6 + a 209-px splinter: 7651 7517 7398 6111 3183 2680, 3.31 % |
| thin strike, shear 2.0 | 6: 5642 3508 3091 1527 1370 409, 1.50 % | 6: 2201 1603 1428 1219 336 302, 0.68 % |

`mac/ipad_session/`: the author's iPad session (shear 2.0, τ 0.5) with the
thin strike — one bright charge of six on the NEW payload (`new_tau05.png`) and
the same one on the OLD (`old_payload_tau05.png`, the pre-#88 bench at 76e2f5e);
τ 0.25: four (new) / three (old); shear 0.6, τ 0.25: six. The tearing at the
harsh preset is the composition's, on either payload (#5).

#88's AFTER under shear 2.0 had the Tab's two thinnest at 1253 and 763 px;
its BEFORE — the thin strike under the coordinates — had the Tab at TWO
fragments of 151 and 127 px (0.02 % lit) against the desktop's six at 1.93 %.
`mac/default/` and `mac/shear2/` are the same script under the bench's
defaults (anod_drop 0.57, the grid, electric blue) for the eye. The
temporary hook is removed; the Tab carries the hook-free build.

## The boxes (GL, D3D11)

`_work/BOXES_HANDOFF_55B.md` — the tiers re-measured, the Anod columns
recaptured, on the Linux and Windows boxes.
