# Step 80 — The stick-slip bow and the strike position from the cell (2026-10-19)

Voxo **0.20.0**, additive (`DECISIONS_12 #4`; `_work/ROADMAP_6.md` step 80;
`_work/TO_PROJECT_SPEC.md` §13.10 as shipped). The core is frozen: nothing
under `core/` moved.

## What shipped

* **`suzu::StickSlip`** (`voxo/src/suzu.h`): the hyperbolic friction curve
  μ_d + (μ_s − μ_d)·v₀²/(v₀² + v_rel²) with the sign smoothed, solved
  implicitly against the point's mobility (three Newton steps, the slope
  floored on the folding flank), bounded by the sticking limit, with the
  stick/slip branch kept (the hysteresis); `solve_naive` the raw steps, the
  red control.
* **`bow_mode` 1 on three bodies**: the modal lattice's bowing point (the
  force shared to the modes by their profile), the Verlet chain's node
  between `kick` and `drift` (the step split; the fused step bit for bit),
  and the hybrid string's loop (`HybridString::bow`: two taps against Z = 1,
  the textbook waveguide bow — the clean Helmholtz). The mouth is the bow's
  speed; the press and the velocity its force; no mouth lifts it.
  `bow_force`, `bow_speed`, the lab's `bow_naive`; the inspection's force,
  speed, work, gain and branch per kind.
* **The strike position from the cell**: `voxo_set_strike_pluck`; the
  iPad's and the Tab's touch-begins map the touch's distance from the cell's
  centre (the centre the middle, the rim the bridge) and hand it before the
  note-on; it stands in for `pluck` on the chain, the plucked lattice and the
  membrane.
* **The plumbing**: the web table and `sw_strike_pluck`, the preset
  library's three fields, the desktop's *Suzu bow* rows (under the lattice
  and the chain), the suite's gates 39–41, the patch table's sixteen
  (*Bowed string (stick-slip)* = the waveguide, *Bowed chain*) pinned to
  `presets/bowed_stickslip.json`, `bowed_chain.json`, the chart runner's
  mode 5 (`bow_force`, `bow_force_lattice`) and `tools/chaos_chart.py`.

## Findings (the decision entry has them in full)

* The hysteresis is part of the model (without it every body whistles).
* The bow acts between the chain's kick and its drift (after the drift a
  strong bow pinned the node into a static deflection).
* The clean Helmholtz lives on the delay line; the chain's lattice
  dispersion smears the corner.
* The ledger's gain reads at the synchronized velocity (the leapfrog
  invariant); read at the half-step the chain's balance was impossible.
* The red is the raw Newton: a converged root never crosses the bow and the
  explicit force's excess is friction heat.
* The web gate drops the bowed lattice (its sinf flips the branch).

## Gates

| Gate | Result | File |
|---|---|---|
| the Helmholtz signature, the waveguide at A3 | the point sticks 21 / 48 / 68 % at the force 0.25 / 0.5 / 1.0, rising toward 1 − β = 88 %; the pitch +1 / 0 / −4 cents | `suite_step80_blocks.txt` |
| the same on the Verlet chain | 49 / 70 / 85 %; the pitch 0 / −15 / +16 cents | same |
| the ledger (the tight line ∫f·v ≤ ∫f·v_bow and the held line), 3 s at the force 1.0 | the waveguide took 2089 of 13 660 of work, the chain 0.054 of 4.095; the held energy under the work | same |
| RED: the three Newton steps taken raw (bow_naive) | the chain took 0.156 against 1.46 of work — the line crossed; the waveguide 7.0e4 against 6330 (printed) | same |
| the modal string under the bow at A3 | −13 cents, the peak 0.263, stuck 69 % | same |
| the servo bow untouched | bow_mode 0 renders bit for bit under the stick-slip's knobs | same |
| the balance with every loss zeroed (the probe) | dE = dGain per quarter second on the chain and the waveguide | `probes/gain_probe.txt` |
| the Release suite | all 190 green | `suite_release.txt` |
| the web gate | GREEN — the chain and the waveguide under the bow bit for bit; the first twelve sections' audio and traces bit-identical to the step-79 reference | `gates/suzu_web_gate.txt` |
| the lab gate (six pages) | GREEN | `gates/suzu_lab_gate.txt` |
| the storms from the Release universal build | Bowed string (stick-slip) and Bowed chain: 0 XRuns, render max 0.23 / 0.26 ms | `gates/storm_*.txt` |
| the fuzz (Release) | 60 s clean; the strobe fuzz 30 s: 0 blocks differ, 0 non-finite | `gates/voxo_fuzz.txt` |
| ctest (Debug) | 14/14 | `gates/ctest.txt` |
| the chart | the chain and the modal string at A3, the force ramped by the press over 16 s at full breath — the corner sharpens, then the pull | `chart/` |
| the profiles | the waveguide: peak −33 … −22 dBFS over 21..108; the chain: −27 … −25.5 (flat within 1.4 dB) | `profiles/` |

## The tablets

* **Android (the Pixel 10 Pro emulator from the release APK signed with the
  debug key)**: Bowed string (stick-slip) 5 286 callbacks, 0 device XRuns,
  render max 1.9 ms of a 20 ms burst; Bowed chain 5 275 callbacks, 0 / 0,
  render max 1.8 ms. The spike sends no press, so the bow lifts: the runs
  are the load's evidence; bowing under the finger is the author's.
  `tablets/android_emulator_patch1*.txt`.
* **iOS (the iPad Air M4)**: `build-ios` rebuilt, the app built on the
  existing project and installed; the device locked before the launches
  (CoreDevice "Locked"; the copies answered with a stale CSV and were
  discarded). The two runs and the finger's bowing are the author's.

## Carried

Bowing under the finger on both tablets (the author's ear); "a struck bar's"
strike profile (the VCSL fits would move); the lattice's pitch pull under the
bow; the chain's raucous pin above the playable range; the bowed lattice in
the web gate.
