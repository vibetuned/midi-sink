# Step 83 — The Toda lattice soliton ring (`voice_kind` 10) — evidence

Date 2026-10-19. Voxo 0.23.0 (additive); the core frozen (libsumi 1.12.0 untouched). Decision entry `DECISIONS_13 #1`
(`_work/DECISIONS_13.md`); the spec draft `_work/TO_PROJECT_SPEC.md` §13.13's Toda paragraph as shipped; the book
`site/src/content/docs/suzu/toda-ring.mdx`.

## What shipped

* `voxo/src/suzu.h`: `suzu::TodaRing` — N ≤ 64 nodes in DOUBLE under H = Σp²/2 + (a/b)Σ(e^{−br} + br − 1); the kick–drift
  leapfrog; `k_for` (the first mode on the note exactly at the step), `nodes_for` (the bound); the C¹ soft cap
  (E = e to cap/2, cap − cap²/4e past it; the potential's integral in `energy()`); `toda_exp` (Cody–Waite + a degree-11
  Taylor, 10⁻¹⁴, the same bits everywhere); `strike` (a mean-free Gaussian compression launched with p = −√k·r),
  `kick_all` (the swirl's uniform momentum), `rescale` (the inner-step change), the pickup standing in the lab against
  the mean drift (`pick_xi` runs back at Σp/N a step; a Catmull–Rom cubic over the bonds).
* `voxo/src/voxo.cpp`: the strike (kind 10), `suzu_toda_fit` — THE RESOLUTION RULE √k·ĉ·b ≤ 0.15 by 2..8 inner steps a
  2× sub-step (4 at most under `VOXO_MOBILE`), the nonlinearity CAPPED FOR THE NOTE at the strike where the steps
  cannot reach it (the 1.3× self-focusing allowance; lowered, never raised); the render loop (m steps, two reads at ×4
  through `hb[0]`, `hb[1]` to the output rate); the swirl's impulse on the change; the inspection k[0..8] (H in the ×4
  step's units whatever m); the trace's aux pair; `toda_nodes` ≤ 32 on the mobile tier.
* `voxo/include/voxo.h`: `toda_nodes`, `toda_b`, `toda_cap`, `toda_width`, `toda_gate` (the red control); the docs.
* `voxo/src/suzu_patches.cpp`: patch 17 *Toda ring*; `presets/toda.json` (written by `tools/make_patch_preset.cpp`, the
  step-82 generator generalised); the preset schema's four fields (`presets/`, `SCHEMA.md`, the helper, the tests).
* The desktop: the *Toda ring (solitons)* voice, four rows, the INI, `--preset`; the web: the engine's fields, the
  lab's kind name, the gate script's kind-10 section, `TOLERANT[10]`.
* `tests/voxo_suzu_tests.cpp`: gates 49–54; gate 28 on eighteen patches; `tools/strobe_chart.py`'s Toda panel.

## Gates (the Release suite, `suite_release.txt`: 229 green, 0 failures; the Debug ctest `ctest_debug.txt`: 14/14)

| gate | reading |
|---|---|
| 49 the linear ring | mode 1 −0.00 cents from A3; modes 2–5 within 0.00 % of sin(mπ/16)/sin(π/16) |
| 49 the press | press 0: the note +3.8 cents (b 0.12); press 127 (b 2.40): the train +427 cents, h5 −11.0 dB |
| 49 the ×4 path | the top octave −131.7 dB under the audible band at full press |
| 50 the drift | the ring alone, 10 minutes lossless at b 2.4: the minute-mean H drifts 0.000017 dB (H breathes ±0.059 dB), Σp 3.9e-14 |
| 50 the shape | the biggest soliton 0.9717 (struck 0.756; κ 1.834, width 0.545) × profile width 1.126 within 0.72 % (amplitude 0.26 %) |
| 51 the swirl | Σp 0 → 0.07381 = N·J·√k; the train 281.5 → 336.9 Hz (+19.7 %); the same message again adds nothing |
| 52 the rule | C7: 4 inner steps, b as asked; C8 at level 1: 8 steps, b 0.46 of 2.40, H +0.90 dB a second, output 0.70; the red +19.4 dB |
| 53 the strobe | 440 of 440 returns at A3, finite — `strobe/strobe_toda.csv`, `strobe_toda.png` |
| 54 headroom | 16 rings C3–C7 at full press: worst 0.495 ms of 2.667 (18.6 %; the bar 30 %) |
| 28 the table | eighteen patches; patch 17 equals `toda.json` field for field (104 keys) |

The web gate (`web_and_lab_gates.txt`): the Toda ring −118.1 dB from the desktop, 94 µs a quantum (3.5 %); the lab
gate GREEN (6 pages). The storm (`storm.txt`): on `presets/toda.json` and on `--voxo-suzu-voice 10`, 0 XRuns, the
render at most 0.63 ms, 12–13 voices at the end, the visuals at rate. The profile (`profiles/profile_toda.png`): 88
notes, the peak −30.3..−27.9 dBFS (spread 2.4), the RMS spread 0.7 dB.

## The cost (one voice, the Release library, arm64)

| note | nodes | inner steps | CPU |
|---|---|---|---|
| A2 | 16 | 2 | 1.1 % of a core |
| A2 | 64 | 2 | 3.0 % |
| C7 | 16 | 4 | 1.7 % |
| C7 | 64 | 6 | 9.1 % |

## The tablets (`tablets/`)

* iPad (`ipad_voxo_spike_patch17.csv`, voxo 0.23.0): 0 XRuns, render max 0.42 ms of the 2.67 ms burst.
* Tab (`android_tab_patch17.txt`, the release APK signed with the debug key): 731 XRuns, render max 13.9 ms of a 4 ms
  burst — the device's audio thread (step 82's unison pair reads 1.9 ms there against the Mac's 0.05: some forty times
  the Mac's cost on any voice); the bisect on the Pixel_10_Pro emulator (`android_emulator_patch17.txt`, the same APK,
  arm64 at the Mac's speed): 0 XRuns, 3.6 ms of a 20 ms burst. Carried for the author: the shell's audio-thread
  placement (ADPF / affinity) — see `DECISIONS_13 #1` (10).

## Found at the step (the probes, `/scratchpad/toda_*.cpp`, not kept)

* The float ring drifted 0.009 dB a minute lossless → the state in double.
* The hard cap's kink and the resolution rule (the ring makes energy past √k·ĉ·b ≈ 0.5 uncapped, ≈ 0.22 capped) → the
  soft cap, the inner steps, the ceiling on b at the strike (a limiter following the live peak pumped the pulse).
* A fully Eulerian pickup read a hard soliton's density spike (−43 dB in the top octave) → the pickup stands in the
  lab against the mean drift and reads in lattice coordinates (−132 dB).
* libm's `exp` was the Tab's budget → `toda_exp`.
* The inspection's H compared across inner-step changes needs one unit → k[2] in the ×4 step's units.

## Files

`suite_release.txt`, `ctest_debug.txt`, `web_and_lab_gates.txt`, `storm.txt`, `profiles/profile_toda.{csv,png}`,
`strobe/strobe_toda.{csv,png}` (with the step-75 and step-79 CSVs the suite re-dumps), `tablets/`.
