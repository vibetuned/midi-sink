# Step 56 — Suzu cells — the Mac's evidence

Decision `DECISIONS_7 #8` (`_work/DECISIONS_7.md`, continuing Part VII): the name, the cell as
the leapfrog with its orbit re-based on retune, the SVF tuned for its damped
ring, the shears, FTZ/DAZ, the class table, the numbers of every gate.
Voxo is 0.8.0 (additive: `voxo_set_source`, `VOXO_SOURCE_*`,
`voxo_suzu_params_t`, `voxo_suzu_default_params`, `voxo_set_suzu_params`,
`voxo_stats_t.source/.ftz`).

Machine: the author's Mac (Apple Silicon), headless through `voxo_render`
unless stated; the storm on the MacBook's speakers.

## What changed in the tree

- `voxo/src/suzu.h` (new): the class table (SYNTH §1's DSP table as the
  header's own), `Cell` (kick / step / step_naive / retune / retune_plain /
  contract), the shears, `Svf`, `eps_for`, `svf_f_for`, `contraction_for`,
  the FTZ helpers. `voxo/src/suzu.cpp` (new): FTZ/DAZ per platform.
- `voxo/src/voxo.cpp`: `SuzuVoice` inside `Voice`; the source atomic and the
  double-buffered patch; the strike's kick and the release's contraction in
  `voice_start` / `voice_release`; `render_suzu` (the 2× section, the pitch
  ramp, the re-base, the SVF, the mix); FTZ at the rendering thread's first
  block; the source switch ends every voice; stats. Version 0.8.0.
- `voxo/include/voxo.h`: the step-56 paragraph and the additive ABI above.
- `tests/voxo_suzu_tests.cpp` (new, ctest `voxo_suzu_tests`): the gates.
  `tests/voxo_c_compile.c`: 0.8.0 and the new calls from C.
- `desktop/`: the Sound section's "Source" row (`AppSettings.sound_source`,
  INI `sound_source`), applied in `sound_apply`; the lab's `--voxo-source`.
- The shears (#9): each shape acts on the orbit normalized by its amplitude
  and is scaled back by it (`shape_cubic`, `shape_tri`, `Cell::step_sheared`),
  and the cell CALIBRATES the shear's detune at create (`shear_period_ratio`,
  a 17-point table per shape) so the voice's ε compensates it.
- The desktop's Sound section, when the source is Suzu: the patch knobs —
  level, release, shear, shear kind, cutoff, resonance (INI `suzu_*`).
- The bench's `--voxo-profile <dir> [--voxo-suzu-shear g]` and
  `tools/sound_profile.py` (#9): the SOUND PROFILE of any source, the graph
  the author asked for every synth, in the evidence (`profile/`) and drafted
  for the docs (`site/drafts/suzu/`).
- `docs/CHANGELOG.md`, `CLAUDE.md`, `_work/DECISIONS_7.md` #8–#9.

## The gates — `voxo_suzu_tests.txt`

| gate (SYNTH §5) | bound | measured |
|---|---|---|
| drift, 10 min undriven undamped at A = 0.5 | < 0.1 dB, < 0.5 cent | +0.0001 dB, +0.000 cent |
| the red control: the naive simultaneous update | must spiral | +690 dB at 10 min, over 0.1 dB within 1 s |
| glide −48 → +48 st over 2 s, re-based | < 0.5 dB | 0.097 dB |
| the plain form under the per-sample ramp (printed) | — | 0.057 dB |
| the plain form stepped per block (printed) | — | 0.062 dB |
| the jump, ±48 in one block, eight phases (printed) | — | re-based 0.082 dB · plain 0.630 dB |
| oscillator tuning, MIDI 21–108, 44.1 k / 48 k | < 2 cent | 0.000 / 0.000 cent |
| SVF ringing frequency, MIDI 21–108, 44.1 k / 48 k, Q = 50 | < 2 cent | 0.007 / 0.008 cent |
| the struck note through the ABI | < 2 cent | 0.001 cent |
| FTZ set on the rendering thread | stats.ftz = 1 | 1 (read back from FPCR) |
| 64 voices released, 2 s T60, 12 s of tail | render/second flat | 48.8 ms first, 48.9 worst |
| the same with FTZ off (printed) | — | no cliff on this CPU (x86's to show) |
| headroom: 16 voices, SVF + shear, 128 @ 48 k | under the period | 0.06 ms mean, 0.09 worst, 3 % |
| the source switch; the sampler after it | ends voices; A4 = 440 | 1 → 0; 440.00 Hz |
| tuning under a full shear, C2 / C4 / C7 (#9) | < 10 cent | cubic 0.19, triangle 0.02 cent |
| confinement: full shear, every note, velocity 127 (#9) | finite, peak < 0.5 | worst peak 0.088 (both kinds) |

## The sound profile — `profile/` (#9, the author's ask)

`--voxo-profile <dir>` strikes every MIDI note 21–108 at velocity 100 (held
0.5 s, released 0.3 s) offline and `tools/sound_profile.py` draws the level
across the keyboard, the first three harmonics per note and a spectrogram:

| source | figure | level across the keyboard (peak, dBFS) | notes |
|---|---|---|---|
| Suzu, the defaults | `profile/suzu/profile.png` | −24.27 → −24.36, spread 0.09 dB | a pure sine per voice: no harmonics (the −56…−100 dB "h2/h3" are the Goertzel's leakage) |
| Suzu, cubic shear 0.3 | `profile/suzu_shear03/profile.png` | spread 0.07 dB | in tune at every note (h1 = the peak); the third harmonic at −57 dBFS, 33 dB under the fundamental — a symplectic cubic is a gentle waveshaper |
| the sampler's sine | `profile/sine/profile.png` | −27.13 flat, spread 0.00 dB | the skeleton's voice, for reference |
| the Dan Tranh demo | `profile/dan_tranh/profile.png` | −23.6 → −48.1, spread 24.5 dB | the library's own: 16 samples stretched across the keyboard, the low zones loud, the top faint |

**The bass.** The author, playing: "the bass notes have a strange
amplitude." The profile says the signal is flat to a tenth of a dB from A0
to C8 (and so is the sampler's sine); what varies is the reproduction and
the ear — a pure sine at 30–60 Hz at −24 dBFS is at the edge of what small
speakers and the equal-loudness curve give back, and its loudness swings note
to note with the speaker's response. The remedy is timbre, not level: the
shear knob (and, in 57, the modal voice's partials) gives a bass note the
harmonics the ear reads the pitch and the loudness from.

**The shears, three versions (#9).** As first written, `x += g·y³` was a
fixed kick per sub-step: at low pitch it dwarfed the tiny per-step rotation
and the orbit escaped — every note below C3 rendered NaN (−180 dBFS) at
g = 0.3. Scaled with ε as a force, in the hardening sense, every orbit is
confined — but the note goes sharp (110 cents at full gain under a
first-order compensation; the fold, not sine-like, 555). So the cell
calibrates itself at create: the sheared cell at unit amplitude runs 80
cycles at a reference ε per gain of a 17-point table and its period is
measured against the bare cell's; the voice scales ε by that ratio and the
note lands within 0.19 cent (cubic) / 0.02 (triangle) at full gain across
C2–C7. The shapes act on the orbit normalized by its amplitude, so a soft
note is sheared as a loud one.

## The storm on the real output — `storm_suzu*.txt`

`--dev --voxo-source suzu --voxo-storm 8`: the fifteen-channel MPE storm on
the MacBook's speakers with Suzu as the source, the pass rule step 55's
(0 XRuns, 0 dropped, the visuals at or above 58 fps):

| run | messages | callbacks | XRuns | render max | dropped | fps | verdict |
|---|---|---|---|---|---|---|---|
| `storm_suzu_1.txt` | 29 913 | 3 394 | 0 | 0.166 ms | 0 | 79.4 | PASS |
| `storm_suzu_2.txt` | 21 934 | 3 398 | 0 | 0.179 ms | 0 | 60.0 | PASS |

A first run (not kept) and the sampler's reference run (`storm_sampler.txt`)
each hit one ~1 s frame at start-up (max frame 1 028 / 1 037 ms) that pulled
their averages to 53 fps with the audio half clean (0 XRuns) — the same stall
on both sources, so the machine's this session (the IDE and a site build were
live), not Suzu's; the two kept runs show none.

## For the author

- The name: Suzu, kept (the author, 2026-09-29).
- The roadmap points the name's confirmation at `DECISIONS_7 #1` (it is #8);
  the spec's companions name `SOUND_SPEC.md` and `MEDIUM_SPEC.md`, which are
  in git history (their shipped content is `specs/TO_PROJECT_SPEC.md` §12 and
  §10).
- Carried `[ITERATE]`s: the decimator (a 2-tap box, cos(θ/2) roll-off,
  −0.08 dB at C8 — a half-band FIR when the budget says), the fold shear's
  ×4, the quadrature y as stereo width.
