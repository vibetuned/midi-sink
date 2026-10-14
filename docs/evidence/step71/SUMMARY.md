# Step 71 — Medium 2: the Kessho composite (the Mac, 2026-10-14)

`DECISIONS_10 #3`–`#5`; the roadmap `_work/ROADMAP_6.md` step 71; the spec `_work/TO_PROJECT_SPEC.md` §10.7
(corrected to what shipped) and `_work/specs/medium_specs_v3.md` §1 (`KESSHO §1`). libsumi 1.6.0, additive.

## What shipped
`SUMI_MEDIUM_KESSHO` = 2 — the same field read as a birefringent crystal between crossed polarizers:
Layer A the Michel-Lévy colour by the strain σ under the isoclinic brush by the principal axis ψ; Layer B
the trace discriminant's tints and the caustic fold of a bed at depth z, a screen-space line on the
iso-trace contour. The chart (`tools/kessho_lut.py` → `core/src/kessho_lut.h`) and the twelve palettes
through one engine-built LUT; `sumi_ctl_t` 20–23; the params `polarizer_mode`, `braid_mode`, `kessho_*`;
`slide_mode` 3 (the polarizer, global or per voice); `SUMI_EXPORT_KESSHO_CAUSTIC`; the desktop's settings;
the bench's `--kessho-test`, `--kessho-flat`, `--ab polarizer`.

## The gates
| Gate | Result | File |
|---|---|---|
| 1 — the switch leaves the field bitwise (Sumi → Anod → Kessho → Sumi → Kessho → Anod → Kessho) | 0 samples differ | `kessho_test.txt` |
| 2 — flatness (`--kessho-flat all`) | 18/18 AS RE-PINNED: the identity 0 exactly; the CPU controls (an exact twist 1.4e-3, the windowed kick 50 — red); every operator's typical texel within 0.1 of det 1. The proposal's 1e-4 is not a number the half-float field can hold (medians 2e-3–6.5e-2; the drops' rims own everything above); exact and sub-stepped NOT separable (`#4`) | `kessho_flat.txt` |
| 3 — the analyzer's period: α = 0 and α = π/2 bitwise | 0 bytes differ (the control's end is its start); α = π/4 turns the cross | `kessho_test.txt` |
| 4 — the chart against the integration at 550 / 1100 / 1650 nm | ΔE 0.37 / 0.67 / 0.41 (< 2); black at 0, white at 275 nm; the table regenerates bit-identically | `kessho_test.txt`, `kessho_lut_check.txt`, `michel_levy_chart.png` |
| 5 — the substrate screen-locked under a 120 BPM scroll | 0 bytes differ | `kessho_test.txt` |
| 6 — the export is the print; the caustic alone over alpha; 8K | 0 bytes differ; every lit texel within the light; 8192×8192 | `kessho_test.txt`, `renders/kessho_caustic_alpha.png` |
| 7 — A/B K-1 | rendered both ways, the author's viewing pending (`#5`) | `kessho_ab/` |
| the frozen media | the field fixture bitwise (Metal), the composite fixture bitwise, 30/30 bench checks (Anod, print, palette, gesture), ctest 12/12, the web gate green, the wasm built | `gates/` |

Also measured (`kessho_test.txt`): the identity sheet dark (mean 31.9/255); the moved water lit (105
against 32); the Maltese cross (146 on the axes against 48 on the diagonals — the dark arms on the
diagonals, where a simple shear's principal axis lies parallel to the analyzer); the regimes (the
vortex core cool, the saddle warm); the caustic (the fold's line at 253 against 65 at rest).

## The renders (`renders/`)
`kessho_script.png` the §4.6 script as the crystal; `kessho_swirl.png` / `kessho_swirl_45.png` the
Maltese cross at α = 0 and π/4 (the first-order rings round the core); `kessho_regimes.png` the tint
alone — a vortex (cool) and a saddle (warm); `kessho_caustic.png` the fold's lines at z ≈ 6;
`kessho_caustic_alpha.png` / `kessho_light_alpha.png` the exports over alpha.

## For the author (`#5`)
* **K-1:** `kessho_ab/kessho_ab_A_global*.png` against `kessho_ab_B_pervoice*.png` (the Michel-Lévy
  palette) and `*_calcite*.png` — one phrase: three held notes with slides at 7°, 45° and 84°, a
  vortex, a saddle. In B each note's cross sits at its own angle; in A the sheet at the latest slide's.
  Mode A is the default until the word.
* **The bloom:** the `*_bloom035.png` set is the same phrase at 0.35; at the shared default (0.75) the
  crystal clips toward white. The knob is shared with Anod by the spec; settling it is the author's.
* **The frost** round every swirl (the half-float staircase read as strain on the far field) and
  **the depth's top** (at z ≈ 6 the soft term lights the staircase of strongly strained water) are
  `[ITERATE]`s recorded in `#5`.
* **The chart's hues** sit 20–30 nm early against the textbook chart; `kessho_retard` absorbs the
  scale, and a published chart's sampled colours would replace the integration as gate 4's reference.

## Not in this step
The tablets' medium pickers (two media until their own steps); the web's Kessho scenes (step 92); the
harmony operators (72–74); the strobe (75). The press is the swirl and the bend the glide under Kessho
until 72 and 73.

## Amended by the minerals delta (2026-10-14, step 71b — `DECISIONS_10 #6`)
A mineral replaces the Kessho palette (the twelve gradients retired; the chart is the mineral's `lut_id`),
and the amended step's gates 8–12 run inside `--kessho-test` (17 checks now): the switch across the six
minerals bitwise, the selenite sign with its Γ₀ = 0 red, fluorite = Layer B alone bitwise, the dispersion
fringe, the walk-off collapse — logs and renders under `minerals/` (`kessho_test.txt`,
`mineral_<id>_<name>.png`, `selenite_sign*.png`, `fluorite_light.png`, `barite_caustic_*.png`,
`calcite_walkoff.png`). The crystal drop and the cleavage profiles are `../step71b/`.
