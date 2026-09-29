# Step 58b — The bore & the jet: the flute — the Mac's evidence

Decisions `DECISIONS_7 #18–#19` (`_work/DECISIONS_7.md`, continuing Part
VII): the acoustic bore (#18) and the jet (#19). Voxo is 0.11.0 (additive:
`voice_kind` 6, the `bore_*`, `jet_*`, `breath_ref`, `breath_range` and
`bore_wall_s` fields of `voxo_suzu_params_t`).

Machine: the author's Mac (Apple Silicon), headless through `voxo_render`
unless stated. Spec: SYNTH §2.11, §2.13, §5; roadmap step 58b.

## What changed in the tree

- `voxo/src/suzu.h`: `Bore` (Webster's system on the staggered grid: the
  profiles, the ends as ports with the frequency-dependent radiation, the
  staggered energy, μ_max by power iteration, the CFL bound, the end
  correction, the tuning by length, the glide's retune) and `Jet` (the
  displacement chain, the Hermite delay, the receptivity band, the tanh
  partition, the noise); the class table's rows.
- `voxo/src/voxo.cpp`: the flute's strike (the bore built for the note), the
  per-block breath → mouth pressure → speed, delay, band, gain, area; the
  render (the dipole port, the power limiter, the wall loss); the CFL gate;
  the defaults; 0.11.0.
- `voxo/include/voxo.h`: the step-58b ABI.
- `tests/voxo_suzu_tests.cpp`: gates 16–19 (below); `tests/voxo_c_compile.c`
  0.11.0; `tests/preset_tests.c` the new fields.
- `presets/`: the `suzu` object's flute fields (`SCHEMA.md`).
- `desktop/`: the voice combo's "flute (bore + jet)" with its knobs (INI
  `suzu_bore_*`, `suzu_jet_*`, `suzu_breath_ref`, `suzu_breath_range`); the
  bench's `--voxo-chart` writes the breath ramp, the profile takes
  `--voxo-suzu-voice 6 --voxo-suzu-breath <b>`.
- `tools/chaos_chart.py` draws the ramp; `tools/README.md`,
  `docs/CHANGELOG.md`, `CLAUDE.md`, `_work/DECISIONS_7.md` #18–#19,
  `site/drafts/suzu/` (the flute's profile and ramp, the chapter's section).

## The gates — `voxo_suzu_tests.txt`

| gate (SYNTH §5, roadmap 58b) | bound | measured |
|---|---|---|
| the bore's series: closed–open cylinder | the odd harmonics within cents | 0.0 cent on 1, 3, 5, 7 (54 cells) |
| the cone, closed at its apex (5 % of its length) | all integers within cents | +0.7, +2.5, +5.1, +8.7 cent on 1–4 (103 cells): a truncated cone's stretch |
| the open–open cylinder | all integers within cents | 0.0 cent on 1–4 (108 cells) |
| the closed lossless bore, 120 cells, 10 min | the drift bound (0.1 dB) | 0.0003 dB in the staggered energy |
| the CFL gate: a forced Courant number 1.05× the bound | rejected with a message; 0.95× admitted | rejected ("…forced to 1.050× its CFL bound…"); admitted |
| its RED control: 1.05× with the gate bypassed | blows up within a second | non-finite (57 sub-steps on the primitive; through the ABI at C5 within the second); 0.95× rings bounded (peak 0.048) |
| the mouth-power ledger over a scripted phrase (a swell, A4 → C5, a release) | the stored energy ≤ ∫P_mouth·Q_in at all times (1 %) | at worst 0.05 % of the mouth's work; the port's limiter acted on 0.65 % of the samples |
| the overblow: A4, the breath ramped 0 → 127 over 12 s, nothing else changed | on the note mid-ramp; the octave at the top | +9 cent at 5 s; −11 cent of 2f₀ in the last second (−287, −155, −63, −27, −7, +9, +21, +41, +81, +161, +257, +1189 cent second by second) |
| soft blowing flattens | < −15 cent at breath 32/127 against 53/127 | −36 cent |
| every note C2–C7 at breath 56/127 | sounds, bounded | 0 silent, 0 blown (F#4 once blew up on a Courant number of 1.00026 — #18) |
| the press blows: A4 under channel pressure 70/127, no breath CC | sounds; silent with the switch off | sounds (peak in the log); silent |
| the earlier steps' gates | unchanged | all green |

The bore's own arithmetic with centred port products does not close on the
phrase (injected 0.048 against radiated 0.051): the ports' discrete power
at the half step is an `[ITERATE]`; the exact conservation is the closed
bore's gate above.

## The chart — `chart/flute_ramp.png`

A4, the breath ramped 0 → 1 over 16 s through the bench: silence, a flat
whisper, the tone rising through the note, sharp, and the octave by
itself — the spectacle (the CSV carries the breath against time; the WAV
is not kept).

## The sound profile — `profile/flute/`

`--voxo-profile <dir> --voxo-suzu-voice 6 --voxo-suzu-breath 0.44`:

| voice | figure | level across the keyboard (peak, dBFS) | what it shows |
|---|---|---|---|
| the flute at the reference breath | `profile/flute/profile.png` | −24.0 at A4; C2–C7 within 8 dB; the sub-contra octave (A0–C1, below the flute's range) 30 dB under | one breath plays the keyboard under the embouchure laws (#19); the odd harmonics 20 dB ahead of the even at the default labium offset — the jet's asymmetry is the ear's `[ITERATE]` |

## The tablets and the boxes

Voxo compiles into both tablet shells unchanged (the run is in this step's
log); neither exposes Suzu. The boxes' re-run of the suite is the author's.

## For the author

- **By ear (roadmap DONE):** the Brisa plays a flute that breathes. At the
  desk with a controller without breath, the press blows (the switch in the
  Sound section, on by default); a keyboard without pressure has nothing to
  blow with.
- **Spec flags:** the jet's source is the labium's dipole (a pressure port
  ∝ dQ/dt; a flow alone does no work at a pinned open end); the jet's
  displacement feeds the delay (the spec's η ∝ v_ac read as the
  displacement, the phase condition demanded it); the receptivity band is
  an addition (the flat gain locked the sixth mode and the full-period
  delay); the embouchure follows the note in three laws (delay in periods,
  gain as f, area as 1/√f) — the physical jet with one geometry is the
  `[ITERATE]`; the bore's CFL bound is min(1, 2/√μ_max); the wall loss is a
  declared per-node damping the spec's radiation port alone did not give.
- **Carried `[ITERATE]`s:** the ports' discrete power at the half step; the
  labium's asymmetry for the even harmonics; toneholes; the physical jet.
