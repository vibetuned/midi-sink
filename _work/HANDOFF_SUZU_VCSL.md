# Hand-off — fit Suzu's patches to the Versilian Community Sample Library

**For:** a later session (any machine with the native build; the author
decides when — this is not on the Phase-10 roadmap, which is Publish).
**From:** the Mac session of step 67 (2026-10-06), at the author's ask:
"optimise Suzu based on VCSL samples".
**Goal:** make Suzu's voices sound like the instruments they are named
after, by measuring real instruments and fitting the patches to them —
with the measurement, the fit and the comparison as tools in the tree, and
every fitted voice shipping its profile beside the sample's.

## Orientation — first time in this project

Read, in this order, before planning:

1. **`CLAUDE.md`** at the repo root — the working rules every session
   follows: the user owns the specs and roadmaps (agents do not edit them);
   the core is frozen (Voxo is not, but it is gated); never `git commit`,
   `git add` or push — the author commits from the working-tree diff;
   evidence under `docs/evidence/<step>/` with a `SUMMARY.md`; every
   resolved ambiguity is a numbered entry in the decision part in flight
   (`_work/DECISIONS_9.md` today, `DECISIONS_9 #n`).
2. **The spec of what you are tuning:** `specs/TO_PROJECT_SPEC.md` §12 (the
   sound engine — the Decent Sampler player, the bus, the licensing
   posture) and §13 (Suzu: the thesis, the DSP class table, every voice as
   built, the gates in §13.5). `docs/PROJECT_SPEC.md` is the engine's spec
   (the visuals); you will not need its body, only its vocabulary.
3. **The record:** `docs/DECISIONS.md` Part VI (Phase 7, the sound,
   `DECISIONS_6 #n`) and Part VII (Phase 8, the synth, `DECISIONS_7 #8–#34`
   — one entry per voice, with the measurements and the red controls).
   Where a spec and an entry disagree, the entry is what shipped.
4. **The code:** `voxo/include/voxo.h` (the pure-C contract, the callback
   contract at its top, `voxo_suzu_params_t` — every knob you can fit);
   `voxo/src/suzu.h` (the cells, the class table at its top, the modal
   presets' ratio tables near the comment "kick profile"); `voxo/src/voxo.cpp`
   (the sources, the voices' MPE consumers); `voxo/src/ds_preset.cpp` (the
   `.dspreset` parser) and `presets/SCHEMA.md` (the `suzu` block a preset
   carries). The readable version of all of it is the Suzu book,
   `site/src/content/docs/suzu/` (plain Markdown; `npm run dev` in `site/`
   renders it).

**Build and test** (`docs/BUILD.md`, the Desktop section; the Mac's
Homebrew cmake and ninja live in `/opt/homebrew/bin`):

```
cmake -B build -G Ninja && cmake --build build && ctest --test-dir build
ctest --test-dir build -R voxo_suzu_tests --output-on-failure     # the synth's suite alone
```

The headless suites you answer to: `voxo_tests`, `voxo_preset_tests`,
`voxo_fuzz`, `voxo_suzu_tests` (`tests/voxo_suzu_tests.cpp` — the drift,
glide, ledger, tuning, bow, CFL, passivity and intonation gates; the place
the fitted numbers get pinned) and `voxo_c_compile` (the header's C
purity). `build/` is Debug; the profiles, the charts and the storm want a
Release tree — on the Mac that is `build-universal`
(`cmake -B build-universal -G Ninja -DCMAKE_BUILD_TYPE=Release`), machine
idle for the storm.

**The desktop bench** is `build-universal/desktop/midi-sink --dev …`
(`--help` lists every flag): `--voxo-profile <dir>` with
`--voxo-suzu-voice <kind>` or `--voxo-suzu-preset <n>` (every note 21–108
struck offline), `--voxo-chart <dir>`, `--voxo-preset <dspreset>
--voxo-storm <s>` (the acceptance test: 0 XRuns, 0 dropped). The tools
beside it, each with a row in `tools/README.md`: `sound_profile.py` (draws
a profile — level, harmonics, spectrogram), `chaos_chart.py`,
`mode_splitting_chart.py`, `fetch_dan_tranh.py` (the VCSL fetch and the
SFZ → `.dspreset` conversion to copy from).

**The web lab**, if a fitted default must be proven in the browser:
`emcmake cmake -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake
--build build-web` (on the Mac `EMSDK_PYTHON=/opt/homebrew/bin/python3.14`
first, or emcc picks a stale Python), then `tools/suzu_web_gate.mjs` (the
wasm against the native reference, bit for bit) and
`tools/suzu_lab_gate.mjs` (every lab page's `?gate=` check in headless
Chrome). The lab's pages are `web/suzu/site/`.

**The figures that exist** — every voice's profile from steps 56–58c — are
in `site/src/assets/suzu/` (the book's copies) and `site/drafts/suzu/`;
regenerate them with the bench and the tool above rather than editing
them. `voxo/COMPAT_REPORT.md` is what the sampler says it does not play.

## Why VCSL

The Versilian Community Sample Library (github.com/sgossner/VCSL) is CC0 —
public domain — so its samples can be downloaded, measured, bundled as
references, and even shipped, with no terms to carry. The Dan Tranh demo
already comes from it (`voxo/demo/`, `tools/fetch_dan_tranh.py` walks the
repository and converts an SFZ to a `.dspreset`): the plumbing to fetch any
of its instruments exists. The library covers brass (trumpet, trombone,
horn, tuba), woodwinds (flute, clarinet, oboe, bassoon, recorders),
bowed and plucked strings (violin to bass, arco and pizzicato; guitars,
harp, zithers), keyboards, and a wide struck-percussion shelf (tubular
bells, glockenspiel, vibraphone, marimba, xylophone, crotales, hand bells).
Verify the exact instrument names and articulations against the
repository's SFZ files before planning — the list above is from memory.

## The mapping to start from

| Suzu voice (the patch that sounds it) | VCSL reference | What to fit |
|---|---|---|
| the modal lattice, *bell* preset | tubular bells, hand bells | the partial ratios and their strike weights (hum, prime, tierce, quint, nominal…), per-partial T60 |
| *stiff bar* | glockenspiel, vibraphone, marimba, xylophone | ratios (the free bar's), weights, T60 — and the bar's register-dependent partial count |
| *glass* | crotales (or a wine glass if the library has one) | ratios, the faint inharmonic ring's decay |
| *harmonic string*, *plucked string* | harp, nylon and steel guitar, the Dan Tranh | inharmonicity B, the pluck position from the comb, T60 per partial, the attack |
| the breath bow (the harmonic string under breath) | violin, viola, cello sustains | the sustained spectrum (bow position), the onset time |
| the flute | flute (sustained, soft and loud) | breath reference and range, the jet's noise, the bore's loss and corner, the level across the keyboard |
| the saxophone | saxophone if present, else clarinet as the reed reference | the reed's threshold and closing pressure (level vs breath), the bell cutoff, the bore wall loss |
| the trumpet | trumpet, trombone | the lips' Q and opening, the bell's flare and start, the brass shear's onset with level |

The Verlet chain, the hybrid string, Duffing, the rotor and the modulator
are not instruments; leave them.

## The method

1. **Measure.** A tool (`tools/suzu_fingerprint.py`, new) that takes an
   instrument's samples (the SFZ's zones give note and velocity) and writes
   one JSON fingerprint per instrument: per note, f₀; the partials' ratios
   r_k and their amplitudes at the strike w_k (peak picking on a windowed
   STFT, harmonics tracked by proximity to k·f₀, inharmonicity B fitted
   where it is a string); per-partial decay T60 (the slope of the
   log-envelope); the attack time; the noise floor; and for sustained
   instruments the steady spectrum per velocity layer. Plot it the way
   `tools/sound_profile.py` plots a voice — the same three panels — so a
   sample and a patch are compared on one figure.
2. **Fit.** A second tool (`tools/suzu_fit.py`) from a fingerprint to a
   patch for the voice kind: the modal presets by least squares on ratios,
   weights and decays over the playable range (`modes` up to 16, the
   `decay_s` / brightness-decay law, `stiffness`, `pluck position`); the
   winds by matching the level-across-keyboard and the spectral centroid
   against breath (the breath reference and range, the bore's loss,
   corner and wall, the jet's gain and noise; the reed's and the lips'
   knobs); the bow by the sustained spectrum (bow position) and the onset.
   The output is a `suzu` block in the preset's JSON form
   (`presets/SCHEMA.md`), loadable on the desktop as it is.
3. **Compare.** Render the fitted patch's profile (`midi-sink --dev
   --voxo-profile <dir> --voxo-suzu-voice <kind>` with the patch applied)
   and the sample's profile through the sampler (`--voxo-profile
   --voxo-preset <the VCSL .dspreset>`), and put them on one figure with a
   number: a spectral distance per note over the first eight partials, and
   the T60 error. Iterate on the fit, not by ear alone — then by ear, the
   author's.
4. **Ship.** Fitted patches become NAMED PRESETS beside the five modal ones
   (and the three winds' defaults, if the author prefers the fitted values
   as defaults — a decision entry either way, because the lab's web gate
   compares the wasm against the native reference bit for bit and a changed
   default re-captures the reference). Every fitted voice ships its profile
   (the author's rule, `DECISIONS_7 #9`) and the sample's beside it in the
   evidence and in `site/src/content/docs/suzu/sound-profile.mdx`; the
   Suzu book's preset table gains a "fitted to" column; the tests pin the
   fitted numbers (`tests/voxo_suzu_tests.cpp`: tuning within the usual
   cents, T60 within 0.1 %, the load gates still green).

## What stays fixed, what needs the author

* The modal presets' RATIO TABLES are code (`voxo/src/suzu.h`), not patch
  fields; "user-editable modal ratio tables" is on the deferred list of
  `_work/ROADMAP_5.md`. A fit that needs ratios outside the five tables
  wants either a new named table in code (cheap, a decision entry) or the
  editable tables (the deferred feature — the author's call before the
  work starts).
* The core (`core/`) is not involved; Voxo is open but gated — the three
  Voxo suites, the storm (`--voxo-storm`, 0 XRuns, 0 dropped), the lab's
  page gates (`tools/suzu_lab_gate.mjs`) and the web reference
  (`tools/suzu_web_gate.mjs`) must stay green.
* Licensing: the app bundles no libraries but the demo; VCSL being CC0
  does not change that policy on its own — bundling a second instrument,
  or a fitted-patch pack that names its references, is the author's call
  (`reference/licensing.md` says what the app bundles).
* Questions to settle with the author first: which instruments (the table
  is a proposal); presets in code or editable tables; whether the fitted
  values replace the defaults or sit beside them; whether a VCSL-derived
  demo set ships with them; and where this lands in the roadmap (after
  2.0 unless the author moves it).

## Deliverables

`tools/suzu_fingerprint.py`, `tools/suzu_fit.py`, their rows in
`tools/README.md`; the fingerprints and the figures under
`docs/evidence/<step>/`; the fitted presets with tests; the decision
entries; the Suzu book updated; the sound profiles regenerated. Never a
commit — the author commits.
