# Step 71b — Crystal habits and cleavage profiles; the minerals (2026-10-14)

libsumi **1.7.0**, additive (`DECISIONS_10 #6`–`#7`; the amended step 71's
gates 8–12 under `../step71/minerals/`). The author's `Mineral.md` and its
two deltas, folded the same day into `_work/ROADMAP_6.md` (step 71 amended,
step 71b) and `_work/TO_PROJECT_SPEC.md` §10.7.8, then built on the Mac.

## What shipped

* **A mineral replaces Kessho's palette.** `sumi_mineral_t` (sized, 60 bytes),
  `sumi_set_mineral` / `sumi_get_mineral` / `sumi_mineral_preset` (six, the
  optical half), `core/src/minerals.cpp`; the 1.6.0 Kessho palette library
  retired (count 0; the custom slot survives as `lut_id` 1, a chart by order).
  The composite's laws (`composite.glsl`): the optic classes, the sensitive-
  tint plate, the walk-off, the thin film, the dispersion, Hamilton's ring,
  the body, the bloom gated to Layer B.
* **The crystal drop** (`drop_fs`): R(θ) in the drop's frame — circle, rhomb,
  hexagon, rosette — exact by the theorem; the interior ink from r/R(θ); the
  strike's frame the pitch axis; a held voice keeps its habit. **The cleavage
  profiles** (`tine_fs`): smooth, twin kink, terrace — bumps, zero beyond 4α;
  profile 3 the dev-only fault. Four params fields; habit 0 and profile 0
  bitwise.
* **The shells:** the desktop's Mineral section under Kessho (`app_mineral_preset`
  pairs the halves), the INI's `mineral_*`, the session's `mineral` block
  (`presets/`, `SCHEMA.md`), six `presets/kessho_<mineral>.json`; the Medium
  radios one per line; the lab-bench keys named for the live keyboard layout
  (`dev_key_name`); the Kessho knob *Exposure*.
* **The book:** the drop and tine pages' new sections, `operators/minerals`
  (Hamilton's line after Jaffer's), the settings reference's Kessho and
  Mineral rows.

## Gates (all green; the reds red)

| # | Gate | Result | File |
|---|---|---|---|
| 1 | the four §10.2 gates: `--soak twin_tine`, `--soak terrace_tine` | green on all four parts — twin kink (b) −1.19 % / 5.26 texel, (c) +0.20 %, (d) ×−0.03; terrace (b) −0.67 % / 5.33, (c) +0.00 %, (d) ×0.15 | `soaks/soak_twin_tine.txt`, `soaks/soak_terrace_tine.txt` |
| 1 | `--soak crystal_drop` | (b) the closed form (no inverse pass): one rhomb pass, pre-image det min 0.972, mean 1.00004, flatness median 1.16e-3; (c) +0.00 %; (d) informational — a drop's expansion carries the ink off the sheet (×14 the control, the spark's drop precedent) | `soaks/soak_crystal_drop.txt` |
| 2 | flatness under the three soaks (`--kessho-flat`) | medians 3.05e-3 / 2.16e-3 / 2.67e-3 at ±3 — the field's floor (`DECISIONS_10 #4`) | `soaks/flat_*.txt` |
| 2 | the analytic witness `tests/habit_det.c` | 22/22: \|det J − 1\| ≤ 7e-9 at 10⁴ random points per habit, det 1 on the corner ray, rays monotone, the bumps zero beyond H | `habit_det.txt` |
| 3 | RED — the unwindowed profile (`--tine-fault`) | the gate goes red at (b): the "pair" is the same shear twice, −99.89 % / 305 texels | `soaks/soak_tine_fault.txt` |
| 4 | bitwise | `field_512_metal.bin` and `composite_512_metal.rgba` bitwise on Metal; ctest 13/13 | `gates/` |
| 5 | the homothety | a rhomb's fract-0.5 ring is the rhomb at f = 0.1503 (r₀ 0.3), residual 0.49 texel; hexagon 0.33, rosette 0.30; RED: the circle under the rhomb model misses by 6.1 | `habit_test.txt` |
| 6 | the growth | the fed rhomb's boundary residual 0.26 / 0.25 / 0.26 texel at 100 / 200 / 300 frames; f 0.1197 → 0.1554 → 0.1909 (the feed's linear law; the area ∝ f²) | `habit_test.txt`, `renders/habit_growth_*.png` |
| 7 | the crease | corner-ray contrast 0.464 at roundness 0, 0.060 at 1 | `renders/habit_crease_roundness*.png` |
| 8 | calcite + twin kink | the lamella's interior 38 against its lit ramps 222 in Kessho (Jaffer's 189 / 198); the Sumi facets | `renders/calcite_*.png` |
| 9 | the pass count | a rhomb strike one pass, a twin-kink stroke one, as the circle's and Jaffer's | `habit_test.txt` |
| 71:8–12 | the mineral gates (`--kessho-test`, 17 checks) | the switch across the six bitwise; the selenite sign 43/255 swing, 2.4 at Γ₀ = 0; fluorite = Layer B bitwise; the dispersion fringe 78 k texels, 0 at d = 0; the walk-off closed on the arms (1.16) and open between (13.3) | `../step71/minerals/` |

`renders/habit_<habit>_<medium>.png`: one strike per habit in Sumi, Anod and
Kessho (the DONE line's set). The web shell compiles (no gate run: the step
is the Mac's).

## Open for the author

* The six at the exposure (the one `[ITERATE]` number) and the bodies, by eye.
* The roundness law's sharpness (k = 2 + 40(1 − roundness)²), by eye at the
  crease.
* Three stylisations recorded in `DECISIONS_10 #6`: the biaxial brush at the
  π/2 period, the plate fixed to the sheet, the walk-off doubling the chart.
* The tablets' Mineral pages and the medium guide (their own steps); step 72.
