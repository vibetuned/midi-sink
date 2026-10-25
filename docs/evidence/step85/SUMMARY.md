# Step 85 — The port-Hamiltonian Dirac network (`voice_kind` 12) and the billiard (`voice_kind` 13) — evidence

Date 2026-10-21. Voxo 0.25.0 (additive); the core frozen (libsumi 1.12.0 untouched). Decision entry `DECISIONS_13 #3`
(`_work/DECISIONS_13.md`); the spec draft `_work/TO_PROJECT_SPEC.md` §13.13's two paragraphs as shipped; the book
`site/src/content/docs/suzu/dirac-network.mdx` and `suzu/billiard.mdx`.

## What shipped

* `voxo/src/suzu.h`: `suzu::Dirac` — n ≤ 36 states in double, H = ½xᵀQx (Q diagonal), THE CAYLEY STEP by Gauss–Jordan,
  its Q-norm gain by power iteration (`gain_q`); `chain()` the prepared network (the springs' extensions and the masses'
  momenta as the states, J the incidence, the bridge on the last spring with its own loss, the lowest mode on the note by
  inverse iteration with the bridge's loading, the target prewarped for the midpoint rule); `rattle()` the stop under the
  bridge with its declared loss. `suzu::Billiard` — the exact rotation at ×4 (three shears a plane), the stadium wall, the
  collision's fraction by bisection, the reflection, the two-point polyBLAMP of each kink through a one-sample delay;
  `exact` the red.
* `voxo/src/voxo.cpp`: the strikes (12: the build and a kick ×8 × 220/f; 13: the kick along the pan), the block head (the
  billiard's wall from the wheel and CC 74), the sub-steps (12: the Cayley step, the bow's port with its work and gain, the
  rattle, the force on the bridge as the output; 13: two ring steps a sub-step, both channels through their half-band
  pairs), the stereo output generalised to kind 13 (`hbr[2]`), the trace aux, the inspections, the bow's block head for
  kind 12 (`SUZU_DIRAC_KV`, `_KF` 0.18, the reference × 220/f).
* `voxo/include/voxo.h`: `dirac_masses`, `dirac_bridge_hz`, `dirac_bridge_mass`, `dirac_stop`, `dirac_stop_loss`,
  `dirac_bridge_decay`; `bill_drive`, `bill_aspect`, `bill_length`, `bill_length_rest`, `bill_exact`; the docs.
* `voxo/src/suzu_patches.cpp`: patches 19 *Prepared network* and 20 *Billiard (stadium)*; `presets/prepared_network.json`,
  `presets/billiard.json`; the preset schema's ten fields (`presets/`, `SCHEMA.md`, the helper, the tests, the generator).
* The desktop: the two voices with their rows (the network's with the bow's), the INI, `--preset`, `--voxo-chart`'s
  `billiard_sweep`; the web: the engine's fields, the lab's kind names, the gate script's two sections (the network
  plucked: the bow's branch flips under a last bit, as the bowed lattice), `TOLERANT[12, 13]`.
* `tests/voxo_suzu_tests.cpp`: gates 59–62; gate 28 on twenty-one patches; `tools/chaos_chart.py`'s billiard panel;
  `tools/strobe_chart.py`'s two billiard panels.

## Gates (the Release suite, `suite_release.txt`: 251 green; the Debug ctest `ctest_debug.txt`: 14/14)

| gate | reading |
|---|---|
| 59 the passivity probe | the Cayley step's Q-norm gain over twelve random skew J (n 6..16, a step 50× the engine's): 1 within 2.2e-16 lossless, at most 0.99996 damped |
| 59 the chain | plucked, lossless, no bow: the lowest mode −0.20 cent from A3 with the bridge's loading; 19 states; the step's gain 1.000000000; H 1.40599 → 1.40599 over 12 s |
| 60 the ports | the bow (breath 90, press 80), lossless: the tone +4.3 cents; the ledger H + the stop's losses − the bow's gain − H₀ = +9.7e-7 of H; the gain 8.95 under the work 708; the stop hit 632 times taking 8.14; no stop, no hit |
| 61 the billiard | the circle kicked 1.75 past the wall bounces at 2.577× the note ((π/2)/asin(1/v), within 0.01 % over A3..A5); the energy across 66 393 collisions in 60 s lossless within 2.4e-12 dB |
| 61 the alias | the off-grid residual of the circle's orbit in the top octave −80.5 dB over A3..A5 (the gate −60); the red: the bounce 3.78 % off the geometry (the exact path 0.009 %) |
| 62 the strobe | the circle 24 of 576 cells (a line), L and R the same bits; the stadium 153 (dust), L and R apart; the wheel 0.30 → 2.00 — `strobe/strobe_billiard_L0/L2.csv`, `strobe_billiard.png` |
| 28 the table | twenty-one patches; 19 and 20 equal their files field for field (115 keys) |

The web gate (`web_and_lab_gates.txt`): the network plucked 100 % bit-identical (50 µs, 1.9 % of the quantum), the
billiard 100 % bit-identical (16 µs, 0.6 %); the lab gate GREEN (6 pages). The profiles (`profiles/`): the network
plucked 88 notes, the peak −26.8..−12.9 dBFS (14 dB: the 700 Hz bridge's dip between A2 and D4; 39 dB at the first
reading on the bridge's velocity with a flat kick); the billiard −14.6..−14.3 dBFS (0.24 dB: the wall bounds it). The
sweep (`chart/billiard_sweep.{wav,csv,png}`): A2 kicked 1.75 past the wall, the wheel 0 → 2 over 24 s — the circle's
regular bounce to the stadium's grit.

## The storm (`storm.txt`)

From the universal build, fifteen channels for twenty seconds: on `presets/prepared_network.json` 0 XRuns, the render at
most 0.57 ms of the 2.67 ms period with sixteen voices at the end; on `presets/billiard.json` 0 XRuns, 0.22 ms with twelve.

## The tablets (`tablets/`)

* iPad (voxo 0.25.0): the network 0 XRuns, render max 1.27 ms of the 2.67 ms burst (the 19×19 double product under the
  spike's polyphony); the billiard 0 XRuns, 0.13 ms.
* The Tab was UNPLUGGED at the step; the Pixel_10_Pro emulator stood in (the release APK signed with the debug key): the
  network 0 XRuns, 1.66 ms of a 20 ms burst; the billiard 0 XRuns, 0.87 ms.

## Found at the step (the probes `/scratchpad/s85_probe.cpp`, `bow85.cpp`, `bowscan.cpp`, not kept)

* The chain's bow scale threw the network's mass to the bow every sub-step (the tone +200 cents, the work 3·10⁴) → the
  network's own `SUZU_DIRAC_KF` 0.18, measured over a decade.
* The bridge's velocity as the voice tilted 6 dB an octave (39 dB over the keyboard) → the force on the bridge, the kick
  and the bow's reference × 220/f; the bridge above the range flattens the keyboard (9 dB) but the bow stops singing →
  the bridge stays at 700 Hz (14 dB, the body's dip), with its own loss.
* The default kick 1.5 × the velocity gain never reached the wall at velocity 100 → the drive's default 1.4 puts the wall
  at mezzo-forte; the fold raises the bounce (the geometry) — said in the book.
* The alias gate's first form took the maximum of negative decibels from zero; the billiard's red does not alias (a
  sample-periodic orbit) — the red is the period.
* The circle's section is a line (a central kick is radial), not the roadmap's ring — flagged.

## Files

`suite_release.txt`, `ctest_debug.txt`, `web_and_lab_gates.txt`, `storm.txt`, `chart/`, `strobe/` (with the earlier
steps' CSVs the suite re-dumps), `profiles/`, `tablets/`.
