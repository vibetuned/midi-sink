# Step 58 — Strings & chaos — the Mac's evidence

Decisions `DECISIONS_7 #13–#17` (`_work/DECISIONS_7.md`, continuing Part
VII): the Verlet chain and its CFL gate (#13); the hybrid string's junction,
its phase compensation and the passivity gate (#14); the Duffing cell and the
kicked rotor (#15); the chaotic modulator (#16); the layered source and the
combined stress (#17). Voxo is 0.10.0 (additive: `voice_kind` 2–5, the
string, bridge, Duffing, rotor and modulator fields of `voxo_suzu_params_t`,
`VOXO_SOURCE_LAYERED`, `voxo_suzu_passive_bound`).

Machine: the author's Mac (Apple Silicon), headless through `voxo_render`
unless stated; the storms on the MacBook's speakers. Spec: SYNTH §2.3,
§2.8–§2.10, §5; roadmap step 58.

## What changed in the tree

- `voxo/src/suzu.h`: `VerletString` (the chain, its CFL tuning helpers,
  the pluck, the pickup, the energy), `HybridString` (the ring with its
  history, the Thiran allpass, the one-zero loss, the bridge cells, the
  scattering junction on the synchronized velocity, the closed-form
  junction phase, the load-time probe), `Duffing`, `Rotor`,
  `DoublePendulum` (RK4 with the energy projection); the class table's new
  rows.
- `voxo/src/voxo.cpp`: the strike and release per kind, `render_suzu_other`
  (the four bodies through the common output stage), the layered source in
  the render loop, the mod wheel (CC 1) per zone, the modulator stepped per
  block, the CFL and passivity gates in `voxo_set_suzu_params`,
  `voxo_suzu_passive_bound`; 0.10.0.
- `voxo/include/voxo.h`: the step-58 ABI above.
- `tests/voxo_suzu_tests.cpp`: gates 9–15 (below). `tests/voxo_c_compile.c`:
  0.10.0. `tests/preset_tests.c`: the new fields round-trip.
- `presets/`: the `suzu` object's step-58 fields (`SCHEMA.md`).
- `desktop/`: the Source row's "Both (layered)"; the voice combo's four new
  kinds with their knobs (INI `suzu_string_*`, `suzu_pickup`,
  `suzu_bridge_*`, `suzu_loop_loss`, `suzu_duffing_beta`, `suzu_drive*`,
  `suzu_rotor_k`, `suzu_mod_*`); the bench's `--voxo-suzu-voice <kind>`,
  `--voxo-source layered`, `--voxo-chart <dir>`.
- `tools/chaos_chart.py` (new), `tools/README.md`, `docs/CHANGELOG.md`,
  `CLAUDE.md`, `_work/DECISIONS_7.md` #13–#17, `site/drafts/suzu/` (the
  profiles, the charts, `strings-and-chaos.md`).

## The gates — `voxo_suzu_tests.txt`

| gate (SYNTH §5, roadmap 58) | bound | measured |
|---|---|---|
| the CFL gate: a forced k·dt² of 1.05 | rejected with a message; 0.95 admitted | rejected ("…exceeds the CFL bound 1…"); admitted |
| its RED control: 1.05 with the gate bypassed, C4 | blows up within a second | non-finite (25 samples on the primitive); 0.95 rings bounded, peak 0.055 |
| the chain's tuning sweep, MIDI 21–108, 48 nodes reduced per note | < 2 cent where representable | 0.000 cent |
| the passivity gate: the load-time probe's bound on the reflection's gain; 1.05 | ≈ 1; rejected with a message; 1 admitted | 1.0016; rejected ("…the closed loop's energy rose to 3.706× in 300 ms at C6…"); admitted |
| loop passivity: every declared damping zeroed, 10 min at A2 | within 0.1 dB | RMS second 1 → 600: −0.018 dB; spread 0.037 dB |
| its RED control: 1.05 with the probe bypassed | grows | non-finite at 31 s |
| the hybrid's tuning, MIDI 21–108, the defaults | < 2 cent | 0.144 cent |
| the Duffing clang-and-settle: C4, velocity 127, β 8, a 2 s decay | > 10 cent sharp at first, < 1 cent by 2.5 s | +210.6 cent over the first 100 ms (+258 at the first window of the chart), +0.00 at 2.5 s |
| the driven Duffing (printed) | — | the fundamental's share of the power: 100 % → 99 % → 66 % as the press rises to 127 |
| the rotor's drift: K 0.3, undamped, 10 min at A3 | < 0.1 dB; \|p\| ≤ π | 0.0000 dB; \|p\| ≤ 1.108 |
| the rotor at K = 0 | the pure tone within 2 cent | 0.000 cent; the fundamental's share as the wheel sweeps K: 97 → 2 → 20 → 52 → 36 % (K 0 / 0.47 / 0.98 / 1.5 / 2.5) |
| the modulator: a hard kick (E 6), 10 min at 1 kHz | bounded; the energy held | \|out\| ≤ 1.000; E within 9e-15; the projection within 9e-8 of 1 |
| the layered source: one note | both bodies, one voice; ends when both have | the sine 0.031, the cell 0.043, both 0.052 RMS; source 2; 0 voices after the release |
| headroom: 10 Verlet strings at 80 nodes, SVF on | under the period | 0.56 ms worst of 2.67 (21 %) |
| step 56's and 57's gates | unchanged | all green (16 voices 4.0 %) |

## The charts — `chart/`

`--voxo-chart <dir>` writes the material (three WAVs with the swept
parameter's CSVs) and `tools/chaos_chart.py` draws it; the figures and the
CSVs are the archive, the WAVs (19 MB, regenerated in seconds) are not kept.

| figure | what it shows |
|---|---|
| `rotor_sweep.png` | A3 on the kicked rotor, K swept 0 → 2.5 by the mod wheel over 24 s: the tone, its libration sidebands, the band widening toward the octave about the note past K_c — the visual Chirikov's sibling |
| `duffing_clang.png` | C4 at velocity 127 on the Duffing cell (β 8): +258 cents at the strike, settling onto the note within 0.6 s over the amplitude's straight decibel line (a 2 s T60) |
| `duffing_drive.png` | A3 on the Duffing cell driven at the note, the press swept 0 → 1: one partial and its harmonics to a drive of 0.87, then a comb of new partials — the bifurcation |

## The sound profiles — `profile/` (the standing rule, #9)

`--voxo-profile <dir> --voxo-suzu-voice <kind>`, the defaults; the CSV and
the figure are the record (the WAVs are not kept):

| voice | figure | level across the keyboard (peak, dBFS) | what it shows |
|---|---|---|---|
| the Verlet string (48 nodes) | `profile/kind2/profile.png` | −27.6 … −26.1, spread 1.5 dB (RMS 0.2) | the chain's body: a rich comb (h2 11 dB under), the highs compressing toward the top mode; fewer nodes above C6 |
| the hybrid string | `profile/kind3/profile.png` | spread 11.3 dB (RMS 9.5) | the BODY's response: the output mixes the bridge's velocity, so the notes around its modes (220 and 356 Hz) come out up to 11 dB louder, with the wolves as dips exactly at them (the string's energy leaving for the bridge) and the modes ringing as horizontal lines across every note in the spectrogram; the top octave also falls by Karplus–Strong's law (the loss is per round trip, a high note is short) |
| the Duffing cell (β 8) | `profile/kind4/profile.png` | spread 0.2 dB | at velocity 100 a modest clang in the first cycles; the odd harmonics of the cubic |
| the kicked rotor (K 0.3) | `profile/kind5/profile.png` | spread 0.3 dB | the island's libration: sidebands around every note |

## The combined stress — `storm_layered_*.txt`

`--dev --voxo-storm 8 --voxo-source layered --voxo-preset <Bösendorfer 280VC>
--voxo-suzu-voice <kind>`: the fifteen-channel MPE storm, the author's heavy
library under the string voice, on the MacBook's speakers:

| run | synth voice | messages | callbacks | XRuns | render max | dropped | fps | voices at the end | verdict |
|---|---|---|---|---|---|---|---|---|---|
| `storm_layered_verlet_1.txt` | the Verlet chain | 35 066 | 3 389 | 0 | 0.800 ms | 0 | 96.1 | 12 | PASS |
| `storm_layered_verlet_2.txt` | the Verlet chain | 34 427 | 3 396 | 0 | 0.774 ms | 0 | 94.5 | 11 | PASS |
| `storm_layered_hybrid_1.txt` | the hybrid string | 34 564 | 3 398 | 0 | 0.388 ms | 0 | 94.7 | 11 | PASS |

## The tablets and the boxes

Voxo compiles into both tablet shells unchanged (`build-ios`, the Android
debug APK; the run is in this step's log); neither exposes Suzu. The boxes'
re-run of the suite (Windows, Linux — the sanctioned fan-out) is the
author's, as at 55b.

## For the author

- **Spec flags:** the double pendulum ships under RK4 with its energy
  projected (a leapfrog is not symplectic for a non-separable Hamiltonian
  and drifted to NaN); the hybrid's junction is the scattering form with the
  radiation term and the midpoint rule (the spec's two-gain form has one
  passive point, not a range — 0.95 is not conserving either); the Duffing
  spring is the cubic on the position (the spec's "g(y) = −βy³" is a shear
  of y by itself, not a det-1 map); the rotor's momentum maps to the pitch
  as f₀·(1 + p/2π) on the torus.
- **By ear:** K's musical range (the island already swings three semitones
  at K 0.3); the hybrid's bridge (220 Hz, c 0.002: the wolf sits at the
  bridge's modes); the Duffing's β and drive ratio for a broadband storm
  (at β 8, ratio 1 the drive bifurcates into a comb at 0.87).
- **Carried `[ITERATE]`s:** the chain's SIMD layout (21 % for ten strings
  at 80 nodes); the hybrid's per-note loss scale (KS's short high notes);
  a second pickup as stereo; the fret-buzz collision; the modulator per voice
  and the mod-matrix corner; the wave-digital junction (unneeded: no patch
  failed the probe on its merits).
