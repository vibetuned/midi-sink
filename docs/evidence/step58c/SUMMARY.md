# Step 58c — The reed & the lips: the saxophone and the trumpet — the Mac's evidence

Decisions `DECISIONS_7 #20–#23` (`_work/DECISIONS_7.md`, continuing Part
VII): the valve and its junction (#20), the saxophone (#21), the trumpet
(#22), the flags for the spec (#23). Voxo is 0.12.0 (additive: `voice_kind`
7 and 8, the `reed_*`, `cone_apex`, `lip_*`, `partial`, `bell_*`, `brass`,
`valve_naive` and `valve_gate` fields of `voxo_suzu_params_t`).

Machine: the author's Mac (Apple Silicon), headless through `voxo_render`
unless stated. Spec: SYNTH §2.12, §2.11, §5; roadmap step 58c.

## What changed in the tree

- `voxo/src/suzu.h`: `Valve` (the kick–drift valve with the pressure force,
  the reed's stop, the swept volume, the implicit Bernoulli flow and the
  naive form, the staggered energy, the low-passed breath noise);
  `Bore::step` with the two ports at node 0 (the closed end fills from the
  valve's flow, the open end's dipole), the bell shear, the ends' pressure
  ahead and one-step impedance for the junction, the flare profile
  (`BORE_TRUMPET`), the cone normalized at the reed with its mouthpiece
  (`cone_mouthpiece`, `cone_extra`), `bore_peaks`; the step's sweeps fused
  with the damping and a stored 1/S.
- `voxo/src/voxo.cpp`: the winds' build (the sax's bell and wall scaled
  with the note), the junction step, the strike and the per-block breath →
  mouth pressure (the released mouth at zero), the lips' embouchure from
  CC 74, the DC block and the level; the valve gate (the probe with its
  message); the intonation calibration at four notes per embouchure (the
  period as the measure, logged); the defaults; 0.12.0.
- `voxo/include/voxo.h`: the step-58c ABI.
- `tests/voxo_suzu_tests.cpp`: gates 20–23 (below) and the period
  estimator; `tests/voxo_c_compile.c` 0.12.0; `tests/preset_tests.c` the
  new fields.
- `presets/`: the `suzu` object's 58c fields (`SCHEMA.md`).
- `desktop/`: the voice combo's "saxophone (reed + cone)" and "trumpet
  (lips + flare)" with their knobs (INI `suzu_reed_*`, `suzu_cone_apex`,
  `suzu_lip_*`, `suzu_partial`, `suzu_bell_*`, `suzu_brass`); the bench's
  `--voxo-chart` writes the sax's breath ramp and the trumpet's embouchure
  sweep, the profile takes `--voxo-source suzu --voxo-suzu-voice 7|8
  --voxo-suzu-breath <b>`.
- `tools/chaos_chart.py` draws the two charts; `tools/README.md`,
  `docs/CHANGELOG.md`, `CLAUDE.md`, `_work/DECISIONS_7.md` #20–#23,
  `site/drafts/suzu/` (the two profiles, the two charts, the chapter's
  section).

## The gates — `voxo_suzu_tests.txt`

| gate (SYNTH §5, roadmap 58c) | bound | measured |
|---|---|---|
| the mouth-power ledger on the primitives, the reed: a 3 s phrase (a swell to 2.5 P_ref, a note change A3 → D4, a release) | E_bore + E_valve ≤ ∫P_mouth·Q at all times (the probe's 5 %) | holds (the worst ratio in the log) |
| the same for the lips | the same | holds |
| the naive junction on the same phrase | the RED: non-finite or over 10 | non-finite (the reed); the lips' naive form noted |
| the valve gate through the ABI: the naive junction at load | rejected with the message; the implicit admitted | rejected ("…the reed's junction makes energy: … held 1e+300× the mouth's work…"); admitted |
| its RED control: the naive junction, the gate bypassed, a lossless bore, A3 bent a fourth | non-finite within a second | non-finite |
| the sax bore's peaks with the radiation port (apex 0.25, the mouthpiece) | ≥ 4 found; the integers within cents (NOTE) | 6 found; 2: +20.3, 3: +14.2, 4: +30.6 cent (the port's end correction shrinks with frequency; the pinned cone is gate 16's 0.1 cent) |
| the sax across C3–C6 (every third semitone) at breath 70/127 | every note sounds within 40 cent of the note | 0 silent; within 5.0 cent (the worst C6 at −5): −0, −1, −2, +1, +2, +3, +2, +1, −1, +3, +0, −2, −5 |
| the trumpet across C3–C5 at breath 70/127: CC 74 at centre | within 30 cent; a register below at 0 (4 of 4); above at 127 (4 of 4) | within 9.9 cent; 4 of 4 below (−785 to −811); 4 of 4 above (+1275 to +1280); the byte log in the test's line |
| the earlier steps' gates | unchanged | all green (the fused bore step re-ran them) |

Beside the suite, through the ABI (the scratch harness, this step's log):
a note held under the breath (CC 11) or the press ramped from zero over
0.3, 2 and 6 s speaks at 4–10 of 127 on the sax, 5–11 on the trumpet (a
first map kept the sax silent to 50 of 127 — the author heard nothing from
a pedal; #21); the sax's level from 0.017 at CC 5 to 0.054 at 100 (the
declared dynamics), the trumpet's 0.038 to 0.186;
the sax's onsets over 104 (C3–C6, breath 60–127) all in the first
register at the defaults (apex 0.25, reed flow 0.14; the island 0.12–0.15 —
0.22 starts 17 in the third, flow 0.18 starts 10); the trumpet at CC 74 = 0
/ 64 / 127 on all thirteen notes C3–C6: −772 / +14…+25 / +1276…+1296 cents.
The calibration the library logs at load: the sax's pull −13, −18, −20,
−22 cents at C3, C4, C5, C6; the trumpet's +89, +87, +86, +92.

## The charts — `chart/`

| chart | what it shows |
|---|---|
| `sax_ramp.png` | A3, the breath ramped 0 → 1 over 16 s: the tone from a light breath, growing 10 dB and brightening as the reed closes further — no register change |
| `trumpet_lips.png` | A3, CC 74 swept 0 → 127 over 16 s: the registers as a staircase — the second peak, the third (the note), the fourth, fifth, sixth — each bending within itself, the pedal showing through where the lips cross between peaks |

## The sound profiles — `profile/sax/`, `profile/trumpet/`

`--voxo-profile <dir> --voxo-source suzu --voxo-suzu-voice 7|8 --voxo-suzu-breath 0.55`:

| voice | figure | level across the keyboard (peak, dBFS) | what it shows |
|---|---|---|---|
| the saxophone at breath 0.55 | `profile/sax/profile.png` | A3 −29.8 (−24 at full breath: the level follows the breath); C2–C7 within 6 dB (−34 to −28); below C2 falling away (the cell cap) | the first register everywhere (the period gate); the fundamental and the second harmonic within a few dB of each other at this breath, the fundamental ahead from C4 up |
| the trumpet at breath 0.55, CC 74 centre | `profile/trumpet/profile.png` | A3 −17.0; C3–C6 within 1 dB; C7 −26; below C3 falling to −46 at C2 | the trumpet's range: the bore's own fundamental is the note's third and the cap runs out under C3 |

## The combined stress — `voxo_storm_sax.txt`, `voxo_storm_trumpet.txt`

`--dev --voxo-storm 8 --voxo-source layered --voxo-preset <Bösendorfer
280VC> --voxo-suzu-voice 7` and `8`, the release binary
(`build-universal`, `CMAKE_BUILD_TYPE=Release`):

| run | synth voice | callbacks | XRuns | render max | dropped | voices at the end | verdict |
|---|---|---|---|---|---|---|---|
| 1 | the saxophone (kind 7) | 3389 | 0 | 1.291 ms (last 0.369) | 0 | 12 | ok — glitch-free, the visuals at 98.5 fps |
| 2 | the trumpet (kind 8) | 3392 | 0 | 1.428 ms (last 0.461) | 0 | 12 | ok — glitch-free, the visuals at 98.4 fps |

FOUND: the `build` tree is a Debug configuration, and sixteen wind voices
there cost 1.8–2.2 ms of a 2.67 ms block before the piano (a 128-cell bore
at the sub-rate, unoptimized), so the storm from `build` XRuns on the
winds alone; the earlier storms (the strings, 0.4–0.8 ms) fit. The gate is
the shipped configuration's. The bore's step was fused with its damping
and multiplies by a stored 1/S in passing (the Debug cost from 2.2–2.5 to
1.8–2.2 ms; the optimized step is 109 ns at 128 cells).

## The tablets and the boxes

Voxo compiles into both tablet shells unchanged (iOS `cmake --build
build-ios`, Android Gradle — the run is in this step's log); neither
exposes Suzu. The boxes' re-run of the suite is the author's.

## For the author

- **By ear (roadmap DONE):** the sax and the trumpet under breath (CC 2
  or 11, on the note's channel or the zone's master) or press (the
  press-blows switch, on by default) — both speak from a light breath and
  grow with it; CC 74 on the trumpet is the register key — 0 the peak
  below, 64 the note, 127 the octave; on the sax the breath alone. The
  embouchure binding is signed by ear.
- **Spec flags (#23):** the reed's stop; the implicit junction with the
  swept volume and the valve's energy; the probe's design; the cone
  normalized at the reed with Benade's mouthpiece; the truncation decides
  the register (a deeply truncated cone's first peak is its weakest); the
  sax scaled with the note; the peaks of a flared bore measured at load;
  the intonation calibration at four notes per embouchure; the period as
  the pitch measure; the released mouth at zero.
- **Carried `[ITERATE]`s:** the reed channel's flow inertia; toneholes and
  a register vent; the two-mass lips; the physical reed at 2–3 kHz; the
  ports' discrete power at the half step; the bass under C2 (sax) and C3
  (trumpet) — the cell cap.
