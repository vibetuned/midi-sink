# Step 57 — The modal voice & the breath bow — the Mac's evidence

Decisions `DECISIONS_7 #10–#12` (`_work/DECISIONS_7.md`, continuing Part
VII): the lattice and its coupling with the detune compensation and the
two-condition load gate, the presets and the energy ledger (#10); the bow
and its reach (#11); the binding table as built, for the author's ear (#12).
Voxo is 0.9.0 (additive: `voice_kind`, `modal_preset`, `modes`, `coupling`,
`decay_s`, `decay_bright`, `stiffness`, `pluck`, `bow_onset_s`,
`bow_position`, `breath_cc`, `lattice_gate` in `voxo_suzu_params_t`;
`voxo_set_suzu_params` returns bool; `voxo_suzu_coupling_bound`;
`voxo_stats_t.suzu_modes`).

Machine: the author's Mac (Apple Silicon), headless through `voxo_render`
unless stated; the storm on the MacBook's speakers. Spec: SYNTH §2.5–§2.6,
§3, §5; roadmap step 57.

## What changed in the tree

- `voxo/src/suzu.h`: the lattice primitives — `lattice_kick` (the chain's
  shared-potential kick from pre-update positions; muted modes are walls),
  `lattice_lambda_max` (power iteration, walled), `coupling_k` (κ·ε₀²
  capped at C8), `sym_eigen` / `sym_eigenvalues` (cyclic Jacobi with
  vectors), `lattice_compensation` (Newton on the eigen-derived Jacobian,
  J⁻¹ out), `lattice_comp_at_pitch` (the runtime's first-order correction),
  `lattice_lambda_max_over_pitch` (the gate's sweep, MIDI 21–156),
  `bow_factor` / `bow_factor_give_only`, `ModalPreset` / `ModalTable` /
  `modal_table`, `MODE_CEILING`, `LATTICE_BOUND`, the `COMP_*` table
  constants.
- `voxo/src/voxo.cpp`: `SuzuVoice` as a lattice (modes, ε, 1/ε, decays,
  bow targets, the table); the strike kicks every mode by the preset's
  profile with the compensated ε; `render_suzu`'s loop — the retune only
  when the pitch moves, the kick, each cell's step, the declared damping,
  the bow's servo; the breath CCs; the compensation tables (c² and J⁻¹ per
  κ, cached on the ratio fields) beside each patch slot; the load gate and
  `voxo_suzu_coupling_bound`; the source of every number below.
- `voxo/include/voxo.h`: the step-57 ABI above, 0.9.0.
- `tests/voxo_suzu_tests.cpp`: the step-57 gates (below); the step-56 gates
  pinned to the single cell. `tests/voxo_c_compile.c`: 0.9.0.
- `presets/`: the `suzu` object in the QoL preset file (`SCHEMA.md`, the
  reader/writer, `tests/preset_tests.c` round-trips it).
- `desktop/`: the Sound section's modal knobs (voice, preset, modes,
  coupling, decay, brightness decay, stiffness, pluck position, bow onset,
  bow position — INI `suzu_*`), applied with the gate's verdict printed;
  the preset mapping carries the `suzu` block; the bench's
  `--voxo-suzu-preset <n>` and `--voxo-suzu-breath <b>` for the profile,
  whose harmonic measure now searches ±12 cents about each multiple.
- `tools/mode_splitting_chart.py` (new), `tools/README.md`,
  `docs/CHANGELOG.md`, `CLAUDE.md`, `_work/DECISIONS_7.md` #10–#12,
  `site/drafts/suzu/` (the profiles, the chart, `modal-voice.md`).

## The gates — `voxo_suzu_tests.txt`

| gate (SYNTH §5, roadmap 57) | bound | measured |
|---|---|---|
| energy ledger: the bell, 8 modes, κ 0.3, every decay declared zero, 10 min held | < 0.1 dB | RMS second 1 → 600: −0.002 dB; the envelope's spread 0.04 dB |
| the T60s restored (2 s, β 0.3), every mode by Goertzel slope, κ = 0 | within 5 % | within 0.1 % (unchanged at κ 0.05, printed) |
| the bow, on the cell: settles within 5 % from silence (a −20 dB seed) / from 2× | < 12 τ / < 5 τ | 4.9 τ / 1.7 τ; the two orbits agree to 0.00 % |
| zero breath: the declared 2 s decay alone | < −60 dB in 2.2 s, monotone | −66 dB, monotone |
| the servo's ledger over 1 s of steady state | injected = extracted within 1 % | 0.000 % |
| the RED control: the give-only servo from 2× | grows without bound | 1000× the target's energy at 0.43 s, 8e11× at 2 s |
| the bow through the ABI: breath 80/127 at velocity 1; from velocity 127 | sings, stable; the same tone | −39.4 dBFS, stable to 0.05 dB; within 0.3 dB |
| the breath withdrawn, the note held 3 s | < −60 dB | −84 dB |
| the lattice load gate: the harmonic 16-mode patch's bound | 0.95× admitted, 1.05× rejected with a message | κ = 1.035 (the second partial's own spring at κ + 0.5); admitted / rejected, the message captured |
| its RED control: the 1.05× patch, gate bypassed, swirl at full | the partials cannot be placed | C4's fundamental +21 cents |
| the 0.95× patch, swirl at full (κ 1.483, the compensation's far end) | < 2 cent | 0.76 cent |
| the sampling bound on the primitive: two cells at 23.04 kHz / 96 k, κ* = 0.567 | +5 % blows up, −5 % bounded | λ_max 4.106: at once; 3.894: bounded, peak 0.82 |
| tuning under coupling: the lowest three partials of the harmonic, bell and plucked presets, C2/C4/C6/C8, κ 0.05 and with the swirl at full | < 2 cent | 0.08 cent |
| C8's harmonic string, every alive partial (printed) | — | r1–r4 0.00 cent; r5 (20.9 kHz, against the wall) +1.0 |
| the compensation's solve at κ 1.485, the harmonic 16-chain | normal modes on the ratios | residual 5e-8; c₀² 0.60 |
| mode splitting: two cells at A3, κ 0.0125 … 0.5 | beat = f₊ − f₋ within 2 % | 0.01 % (2.7 … 91 Hz); the chart's CSVs |
| the plucked string at p = ½ | even partials unfed, w₃ = 1/9 | 2e-8, 1e-8; 0.1111 |
| patch load: the table at 8 / 16 modes (printed) | — | 2.5 ms / 14.0 ms; a knob that keeps the ratios 0.8 / 1.9 ms |
| step 56's gates on the single cell | unchanged | all green; sixteen voices 3.8 % of the callback; the 64-voice tail 59 ms/s (the first lattice cut, a sine per mode per frame, 513) |

## The sound profiles — `profile/` (the standing rule, #9)

`--voxo-profile <dir> --voxo-suzu-preset <n>` (8 modes, κ 0.05, the
defaults) and `--voxo-suzu-breath 0.63` for the bow; `tools/sound_profile.py`. The
`profile.csv` and the figure are the record; the 27 MB `profile.wav` each run
writes is not kept (step 56's four are in git history; the bench regenerates
one in seconds):

| voice | figure | level across the keyboard (peak, dBFS) | what it shows |
|---|---|---|---|
| harmonic string | `profile/preset0/profile.png` | −29.6 … −28.0, spread 1.6 dB | the partials 1/k: h2 8 dB, h3 14 dB under the fundamental; the top notes shed partials at the 24 kHz ceiling |
| stiff bar | `profile/preset1/profile.png` | spread 4.9 dB | ratios 1, 2.78, 5.44, 9 …: by C4 most are past the ceiling — fewer partials, less held energy, the spread is the instrument's |
| bell | `profile/preset2/profile.png` | spread 2.7 dB (RMS 0.8) | the hum an octave under the prime, the inharmonic cluster above; the peak's sawtooth across the keyboard is the 0.4 s window catching the prime–tierce beat (0.2·f) at a different phase per note — the RMS is flat |
| glass | `profile/preset3/profile.png` | spread 2.3 dB | ratios 1, 2.32, 4.25 …, weights 1/m²: nearly a sine with a faint ring |
| plucked string | `profile/preset4/profile.png` | spread 0.7 dB (RMS 0.3) | Karplus–Strong in modal form at p = 0.28: the comb of sin(kπp)/k² |
| the breath bow (harmonic, breath 0.63) | `profile/bowed/profile.png` | spread 1.2 dB | a flat singing line; the second partial level with the first at bow position 0.3 |

`mode_splitting.png`: two cells at A3, the split against κ (measured beat on
the analytic line) and the pair's spectrum at κ 0.1.

## The storm on the real output — `storm_suzu_*.txt`

`--dev --voxo-source suzu --voxo-storm 8` (the modal voice, the defaults):

| run | messages | callbacks | XRuns | render max | dropped | fps | verdict |
|---|---|---|---|---|---|---|---|
| `storm_suzu_1.txt` | 35 112 | 3 392 | 0 | 0.818 ms | 0 | 96.2 | PASS |
| `storm_suzu_2.txt` | 34 564 | 3 395 | 0 | 0.754 ms | 0 | 94.8 | PASS |

## The tablets

Voxo compiles into both shells unchanged: `build-ios` (libvoxo.a) and the
Android debug APK rebuilt with the final tree, no warnings. Neither shell
exposes Suzu (they stay on the sampler; the source row and the knobs are
the desktop's since step 56), so nothing was installed — the step is the
desktop machine's (roadmap 57).

## For the author

- **By ear (roadmap DONE):** the Brisa's singing tone on the actual
  hardware, and the binding table (#12) — swirl → coupling, breath → the
  bow's target, the press as gain.
- **Patch names** (roadmap "Author input"): the five presets are named by
  their physics (harmonic string, stiff bar, bell, glass, plucked string).
- **Spec flags:** the decays ship as γ_k = α + β·(r_k² − 1) (the spec's
  α + βk²) so the fundamental's T60 is exactly `decay_s` and inharmonic
  highs die by frequency; κ is relative to the LOWEST mode's stiffness (the
  bell's hum keeps a spring); the coupling's detune compensation is not in
  the spec — without it κ was a pitch control (149 cents); with it the
  placed partials stay on their ratios and splitting remains the feature
  for identical or close pairs (charted).
- **Found:** for every shipped preset the load gate's binding condition is
  the compensation's feasibility, not the sampling bound (which only modes
  at the ceiling reach — the primitive's red control); the bow's reach —
  a partial sings only if its decay rate is under 1/τ (the first three at
  the defaults) — and its steady state sits under the target by the
  decay's share (E_t·(1 − γτ)).
- **Carried `[ITERATE]`s:** the bow's target corrected for the decay; μ
  from the breath's attack rate; the bow profile from a CC; the strike
  position from the layout's cell; the press's "sustained small drive"
  (the press stays the gain stage of step 56); the topology (the chain).
