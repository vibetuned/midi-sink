# Step 84 — The Boris–leapfrog magnetic gyroscope (`voice_kind` 11) — evidence

Date 2026-10-21. Voxo 0.24.0 (additive); the core frozen (libsumi 1.12.0 untouched). Decision entry `DECISIONS_13 #2`
(`_work/DECISIONS_13.md`); the spec draft `_work/TO_PROJECT_SPEC.md` §13.13's Boris paragraph as shipped; the book
`site/src/content/docs/suzu/gyroscope.mdx`.

## What shipped

* `voxo/src/suzu.h`: `suzu::Gyro` — (x₁, x₂, y₁, y₂) in double; the half kick, THE CAYLEY ROTATION AS THREE SHEARS
  (t = tan θ/2, s = sin θ, t), the half kick, the drift; `strike` a displacement by the equal-power pan law;
  `contract` the declared decay; `energy`; `angles` (θ/2 ± ψ, cos ψ = cos(θ/2)(1 − ε²/2)).
* `voxo/src/voxo.cpp`: the strike (kind 11), `suzu_boris_fit` every block (the field from the poly pressure in Hz,
  ε and θ prewarped or raw), the sub-step with the cyclotron wrap for the own-clock strobe, THE FIRST STEREO PATH of
  `render_suzu_other` ((x₁, x₂) → (L, R), `svf2`, the plate fed the mean), the trace's pair (x₁, x₂) with the energy
  and the field as aux, the inspection k[0..8].
* `voxo/include/voxo.h`: `boris_b`, `boris_b_rest` (Hz, 0..50), `boris_prewarp` (the red); the docs.
* `voxo/src/suzu_patches.cpp`: patch 18 *Gyroscope (Zeeman stereo)*; `presets/gyroscope.json`; the preset schema's two
  fields (`presets/`, `SCHEMA.md`, the helper, the tests, the generator).
* The desktop: the *Boris gyroscope (Zeeman stereo)* voice, the two fields, the decay and the pluck as the pan, the INI,
  `--preset`; the bench's `--boris-demo <dir>`; the web: the engine's fields, the lab's kind name, the gate script's
  kind-11 section, `TOLERANT[11]`.
* `tests/voxo_suzu_tests.cpp`: gates 55–58; gate 28 on nineteen patches; `tools/boris_chart.py`; the strobe chart's
  Boris panel.

## Gates (the Release suite, `suite_release.txt`: 239 green; the Debug ctest `ctest_debug.txt`: 14/14)

| gate | reading |
|---|---|
| 55 the doublet | the scheme's lines from the struct's own complex map within 0.0010 cent of ω_B/2 ± sqrt(ω₀² + ω_B²/4) over A2..B6 × f_B 0..50 Hz; the engine's inspection the same within 0.0000 |
| 55 the red | boris_prewarp 0: 1.208 cents sharp at B6 — `chart/boris_doublet.{csv,png}` |
| 56 the stereo | pluck 0.5 with no field: L and R the same bits on 48000 of 48000; pluck 0 the left alone, 1 the right |
| 56 the field | A4 under 20 Hz: the rendered lines −0.000 / −0.000 cents from 450.11 / 430.11 Hz; L/R correlate −0.000; the poly pressure 2 → 24 Hz |
| 57 the energy | A3 under 20 Hz, 10 minutes at 96 kHz on the struct alone: H within [−0.031, +0.031] dB, the minute means drift 0.000000 dB |
| 58 the strobe | the own clock (2) 39 returns in 2 s at 20.00 Hz; the bass 440 of 440; no field → the bass (440) — `strobe/strobe_boris.{csv,png}` |
| 28 the table | nineteen patches; patch 18 equals `gyroscope.json` field for field (106 keys) |

The web gate (`web_and_lab_gates.txt`): the gyroscope 100 % bit-identical to the desktop reference, 6.3 µs a quantum
(0.24 %); the lab gate GREEN (6 pages). The storm (`storm.txt`): on `presets/gyroscope.json` and on
`--voxo-suzu-voice 11`, 0 XRuns, the render at most 0.13 ms, 12–14 voices at the end. The profile
(`profiles/profile_boris.png`): 88 notes, the peak −28.0 dBFS flat to 0.01 dB, h2 at −66 dBFS (a linear voice).

## The spirograph (`spirograph/`, the author's viewing — gate delta c)

`midi-sink --dev --boris-demo <dir>`: four notes (E3 B3 E4 B4) under a poly-pressure sweep 0 → 127 over twelve
seconds (the field 2 → 24 Hz), traced into Kessho (`kessho_NNNNN.png`, 37 stills, a half second apart) and into
Anod's spark reading (`anod_spark_NNNNN.png`): the strike's line opening into the rosette as the field rises.

## The tablets (`tablets/`)

* iPad (`ipad_voxo_spike_patch18.csv`, voxo 0.24.0): 0 XRuns, render max 0.10 ms of the 2.67 ms burst.
* The Tab was UNPLUGGED at the step; the Pixel_10_Pro emulator stood in (`android_emulator_patch18.txt`, the release
  APK signed with the debug key): 0 XRuns, 1.1 ms of a 20 ms burst.

## Found at the step (the probe `/scratchpad/gyro_probe.cpp`, not kept)

* A cos/sin rotation matrix is a systematic energy drift in float (c² + s² ≠ 1 every step) → the three-shear Cayley
  rotation; the state double.
* The two prewarps alone leave (ω_B·dt/2)²/6 — three orders under the gate; no fit of (ε, θ) needed.
* The bench demo's last export refused the next pass's first (an export in flight) → polled out between passes.

## Files

`suite_release.txt`, `ctest_debug.txt`, `web_and_lab_gates.txt`, `storm.txt`, `chart/`, `strobe/` (with the earlier
steps' CSVs the suite re-dumps), `spirograph/`, `profiles/`, `tablets/`.
