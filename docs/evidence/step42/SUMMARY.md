# Evidence — Step 42: The Anod medium

ROADMAP_5 Step 42; MEDIUM §1 (switching), §3 (composite), §4 (binding
tables). Decisions: `_work/DECISIONS_5.md` #50–#58. Machine: the author's Mac
(Apple silicon, Metal); the wasm rebuilt and gated; the iOS shell compiled
against the 1.1.0 header. The composite's Anod branch was settled live
against the author's 2560×1440 window on 2026-09-22 (#56, #57): the charge
glows by strain, the water draws the field's deformed grid. **Author input
pending:** the hour on the ROLI Piano + Airwave in Anod, then the binding
table signed by eye (`binding_table.md`).

## What landed (`sumi_version()` → 1.1.0, additive)

* **The Anod composite** (`composite.glsl anod_col`, #50, #56, #57):
  charged material glows 0.22 + 0.78·(1 − e^(−σ/`anod_glow`)), σ =
  sqrt(‖J‖_F² − 2) from the stored coordinates by a one-sided-min estimator
  that survives the half-float staircase (stencil widening with the field,
  ULP-tolerant fresh test, rounding bias subtracted); the charge phase bands
  the filament, aux drifts the hue. Water never glows by strain: it draws the
  DEFORMED GRID — iso-lines of position + gain·displacement, gains −4 along x
  and +3.4 along y, from window-averaged displacements, shown where the water
  has moved by more than a texel, faded where the local pitch would alias.
  Near-black substrate with the washi's screen-locked grain as speckle; the
  three palettes under the same ids (electric blue / violet, plasma orange,
  phosphor green); the custom palette read as a glow. The Sumi path wrapped
  untouched — **bitwise against the 1.0.0 print fixture**.
* **`anod_pitch`** (#58): the grid's pitch at rest, canvas heights,
  1/256..1/8, default 1/144 (10 texels at 1440), 0 = no grid. Desktop "Grid
  lines" slider (Off..256 per height), INI key, web param 32, the `anod`
  scene's P slider.
* **The seam mask** (#52): a charged texel beside fresh water reads no
  strain; the class test for fresh water (identity coordinates within one
  ULP, no phase) finds the seam, nothing is stored.
* **The binding tables** (#51): `SUMI_MODE_MEDIUM_DEFAULT` (255, the new
  default of the three modes) resolves to the medium's column; new mode
  values `bend_mode` 2/3 (torsion / spark wavenumber), `press_mode` 2 (the
  torsion sweep feed); the strike, the poly-pressure dimension and the
  mod-wheel dimension are the medium's outright — Anod's strike is the spark
  composition with the burst's order from `burst_order_by_class`, its poly
  pressure the Chladni stir (the CC map overrides), its mod wheel the
  Chirikov throw. Sumi's column is the 0.x behaviour, unchanged.
* **Live switching** decided as a feature (#53); prints medium-styled by
  construction (#54); the ITERATE ledger in `binding_table.md`.
* **Set aside** (#56): strained water as a GAS within reach of the charge —
  liked, kept as an idea for a future medium, not this one.
* Desktop: a "Medium" section (radio, glow scale, grid lines), the mode
  radios as combos with "Medium default", INI keys (`medium` already,
  `anod_glow`, `anod_pitch`, the modes' new values), the A key,
  `--anod-test`. Web: `anod_glow`, `anod_pitch` and the mode values, scene
  `anod` (a session laid in Sumi, the medium switched), the gate's sweep.
* Page draft `anod.mdx`; the table and ledger `binding_table.md`; the
  re-read prints `anod_reread_sumi.png` / `anod_reread_anod.png` — the same
  §4.6 script under both media.

## Measurements

`--anod-test` (`anod_test.log`, 9/9; 512² bench, grid pitch 10 texels):

| Check | Result |
|---|---|
| The identity field in Anod | the substrate alone: mean luminance 27.5/255, max 29.5 |
| The identity field at 1920×1080 | mean 27.5, max 29.5, the lower-right quarter 27.5 — the half-float rounding is read neither as strain nor as a grid |
| The medium switch Sumi → Anod → Sumi → Anod | the field bitwise: 0 samples differ |
| The charge glows by its strain | 41 169 charged texels at mean 111.1; luminance correlates 0.99 with the CPU's 0.22 + 0.78(1 − e^(−σ)) |
| The water draws the grid | 42% of 89 711 displaced water texels lit (≥ 4 texels of displacement) |
| The grid off (`anod_pitch` 0) | 0.00% of the water lit; the charge's mean 111.1 both ways |
| The photograph of the discharge | a lone drop's interior 91.0 (the base glow; substrate 27.5), 40% of the 1.5–2.4 R annulus lit |
| The reach | a small drop (R = 0.02): 38% of its 1.5–3 R annulus lit, 0.00% of the 243 600 water texels beyond r = 0.15 |
| The three palettes | blue (79, 88, 130), red (128, 88, 52), green (67, 123, 81) dominant in turn |

`test_medium_binding_tables` (headless, in `ctest`):

| Dimension | Sumi | Anod | Anod, modes overridden to 0 |
|---|---|---|---|
| strike (C♯) | drop 1 | drop 1, burst 4 (order 3), spark 2 | drop 1, burst 4, spark 2 |
| bend | tine 1, TORSION_K unmoved | TORSION_K +0.500, no tine | tine 1 |
| slide | SPARK_K 0.500 | SPARK_K 0.945 | 0.500 |
| pressure (40 frames) | 20 drops, 0 torsion | 0 drops, 39 torsion | 20 drops |
| poly pressure | 40 swirls, stir 0 | 0 swirls, 78 Chladni, stir 0.79 | 0 swirls, 78 Chladni |
| mod wheel | 10 vortex, 0 Chirikov | 0 vortex, 2 Chirikov | 0 vortex, 2 Chirikov |

## Gates

| Gate | Result |
|---|---|
| `ctest` | 4/4 (ABI pin 1.1.0; the binding-table test in the normalizer suite) |
| §4.6 field, Metal | bitwise: max\|d\| 0 |
| composite, Metal (Sumi) | bitwise vs the 1.0.0 fixture: max diff 0; the negative control red |
| §4.6 field, web tier | max 9.8·10⁻⁴ / mean 7.9·10⁻⁹ |
| `web_gate.mjs --scenes` | 17/17 including `anod` |
| iOS shell | compiles against the 1.1.0 header (`xcodebuild … BUILD SUCCEEDED`) |

## DONE

| Criterion | Status |
|---|---|
| the Sumi composite is bitwise vs. step 41 | max diff 0 |
| a recorded session re-reads in Anod | `anod_reread_*.png`; the field bitwise across the switch |
| the binding table is signed | **pending the author's hour** — shipped as the table in `binding_table.md`, resolvable to the 0.x behaviour by "Medium default" in Sumi |
| every `[ITERATE]` in MEDIUM §1/§3/§4 resolved or carried with its question | the ledger in `binding_table.md`; the long-exposure question carried to the Phase-9 beta |
| the Anod look at the author's window | the charge by strain, the water as the field's grid, the gas set aside (#56–#58) |
