# Step 59c — The Suzu lab: every voice family — the Mac's evidence

A step the author added on 2026-10-04 with 59b; it carries the phase end
(the author's). Decisions `DECISIONS_7 #31–#34` (`_work/DECISIONS_7.md`):
the host and the six panels (#31), Voxo 0.15.0's recent trace and the winds'
ledger (#32), what the panels measured (#33), the flags (#34).

Machine: the author's Mac (Apple Silicon); Chrome 154 for the browser gate;
node 26 for the node gate. Spec: SYNTH §2.1–§2.13, §5, §6; roadmap step 59c.

## What changed in the tree

- `voxo/include/voxo.h`, `voxo/src/voxo.cpp`, `voxo/src/suzu.h`: Voxo
  0.15.0 (additive): `voxo_set_trace_decimation`, `voxo_trace_recent` (the
  ring's last points at full density, x, y and two aux channels per kind);
  the winds' ledger in the inspection (k[8] the mouth's work, k[9] the energy
  held); the flute's η at the labium kept for the aux.
- `web/suzu/suzu_web.c`, `web/CMakeLists.txt`: the recent trace and the
  density on the lab's flat surface; the copy now carries the lab's pages.
- `web/suzu/site/`: `lab-core.js` (new, the shared core), `index.html` and
  `index.js` (the front page, rewritten), a page per family (`cell`,
  `modal`, `strings`, `chaos`, `winds`, new; `flute.html`/`flute.js`, 59b's
  `index.html`/`lab.js` moved onto the core), `suzu-worklet.js` (the
  safety, the recent points, the scripted runs), `suzu-engine.js`,
  `lab.css`.
- `tests/voxo_suzu_tests.cpp` gate 26; `tests/voxo_c_compile.c` 0.15.0 and
  the recent trace from C; `tests/suzu_web_reference.c` and
  `tests/fixtures/suzu_web_script.txt` (the density, the recent points in
  every snapshot).
- `tools/suzu_web_gate.mjs` (the recent trace compared), `tools/suzu_lab_gate.mjs`
  (every page, `--pages`, `--shots page[:arg][:light][:phone]`);
  `tools/README.md`.
- `site/drafts/suzu/lab.md` (a section and a link per panel), the other
  Suzu drafts linking their panels, `site/drafts/operators/chirikov.mdx`
  (the sound twin, linking the rotor's panel).
- `_work/DECISIONS_7.md` #31–#34; `docs/CHANGELOG.md`; `CLAUDE.md`.

## The browser gate (`suzu_lab_gate.txt`, `suzu_lab_data.json`)

Each page renders its scripted demonstration offline through the same
AudioWorklet and measures it with its own code. GREEN, 6 of 6 pages, in
Chrome 154.

| page | check | measured |
|---|---|---|
| flute | mid-ramp on the note; the overblow by itself; soft blowing flattens | closest 0.4 cents; the jump at breath 116, the octave +4.4; −45.8 against +2.7 cents |
| cell | the leapfrog keeps the orbit's size (A3, undamped, 2.5 s) | x² + y² − εxy drifts 7.3e-6 dB |
| | the shear combs harmonics in | the 3rd at −158.9 dB at shear 0, −30.5 dB at 0.5 |
| | red: the naive update grows, the lab catches it | caught at 0.38 s, grown 26 dB; the engine restarted once |
| modal | the bow settles each mode at its balance E/E_t = 1 − γ_k·τ | within 0.38 dB, mode by mode |
| | no decay, no coupling: the fundamental on its target | within 0.031 dB |
| | no breath: the bow lets go | 42.4 dB down two seconds after |
| | the bell in tune under κ 0.4 | the first four peaks at +0 cents of their ratios |
| | the load gate refuses the bell at κ 1, saying why | refused, the engine's sentence shown |
| strings | the Verlet chain, the modal pluck on their note | −1.3 and +0.1 cents |
| | the hybrid's string on its note (the spectral peak) | +0.0 cents; its body at 341 Hz (−3 dB) and 472 Hz (+2 dB) |
| | the CFL gate refuses k·dt² = 1.05; red: bypassed, it blows up | refused; caught in the first quantum, restarted once |
| chaos | the rotor below K_c confined to its island (2√K ≈ 1.1) | |p| ≤ 1.07 rad over 328 kicks |
| | past K_c the momentum spreads into the sea | spread 1.57 rad (a uniform sea 1.81; confined 0.62) |
| | Duffing's clang settles onto the note | +226 cents in the first 80 ms, +0.0 at 2 s |
| winds | the sax in its first register (A3, breath 70; bound 40 cents) | +19.2 cents (#33: quasi-periodic, an `[ITERATE]`) |
| | the ledger: the energy held never exceeds the mouth's work | at worst 0.013 of the work |
| | the trumpet's CC 74 staircase: the peak below, the note, the octave | −772, +15, +1275 cents |
| | the valve gate refuses the naive junction; red: bypassed, a bend trips it | refused (5.6e7× the mouth's work); caught at 0.30 s |

## The node gate (`suzu_web_gate.txt`)

The script through the wasm against the native no-FMA reference, now with the
recent trace at density 2 in every snapshot.

| voice kind | audio | trace, inspection and recent trace | verdict |
|---|---|---|---|
| 0 the cell, 2 Verlet, 3 the hybrid, 4 Duffing, 5 the rotor, 7 the sax | 100 % bit-identical | bit-identical | bit for bit |
| 1 the lattice | within −112.9 dB | within −106.5 dB | the declared −80 dB |
| 6 the flute | within −91.7 dB | within −106.0 dB | the declared −80 dB |
| 8 the trumpet | within −727.6 dB (one ulp) | bit-identical | the declared −80 dB |
| negative control | — | — | red, as required |

Against the shipping desktop build: −68 to −112 dB for every kind but the
rotor (chaos: the statistics, not the samples).

## The rest

| check | result |
|---|---|
| `voxo_suzu_tests` (`voxo_suzu_tests.txt`) | all gates green; gate 26: the cell's 1023 recent points keep their conserved form within 7e-5, the rotor's kicks 4 of 4 expected, the sax's and the trumpet's ledgers ≤ 0.52 and 0.14 of the work, a render read every block bit-identical to the unread one |
| ctest | 10 of 10 |
| the marble's web gate on the rebuilt web tree | PASS (max 9.8e-4, mean 3.9e-9) |
| iOS `cmake --build build-ios` | builds (Voxo 0.15.0) |
| Android Gradle `:app:assembleDebug` | builds (Voxo 0.15.0; a fresh native directory for the new version string — the first try failed on the network, fetching dr_libs's submodule, the retry built) |
| the release desktop (`build-universal`) | builds |

## The size and the cost

| measure | value |
|---|---|
| `suzu.wasm` | 124 024 bytes, no imports, fixed 32 MB memory |
| two voices in node, per 128-frame quantum | 3.4 µs (the rotor) to 78.4 µs (the sax); the winds and the flute 73–78 µs, 2.7–2.9 % of the quantum |
| one voice in the live worklet, where Chrome's clock resolves it | 71 µs the flute, 73–121 µs the sax, 146 µs the trumpet (2.7–5.5 % of its time); the cell and the rotor are quicker than its millisecond clock |
| the browser gate's flute ramp | 15 s rendered offline in 218 ms (the other pages do not time theirs) |

## The screenshots

Taken through the DevTools protocol after the live page had played four
seconds in headless Chrome (`?demo`):
`lab_index.png` and `lab_index_phone.png` (the front page);
`lab_cell.png` (A3 with the cubic shear at 0.35: the bent circle, the 3rd
harmonic), `lab_cell_light.png`;
`lab_modal.png` (the bell bowed at breath 60: each bar under its target, on
its dashed balance tick; the servo strip settling at −2.8 dB),
`lab_modal_phone.png` (390 px);
`lab_strings.png` (the Verlet chain plucked at A2);
`lab_chaos.png` (the rotor at K ≈ 2: the section filling the sea around the
islands);
`lab_flute_70.png` (on the shared core);
`lab_winds.png` (the sax: the cone's p·r standing wave, the reed's portrait
against the lay, the ledger), `lab_winds_light.png`,
`lab_winds_trumpet.png` (the lips' portrait, the bore's third peak).

Open it locally:

```
cmake --build build-web
python3 tools/web_serve.py --dist build-web/suzu-dist
```

then `http://localhost:8765/`.

## For the author

- **By ear and by eye (roadmap DONE, and the phase end):** the six panels in
  your browser — the lab mode's red controls on each, the bow's balance
  ticks, the rotor's section past K_c, the trumpet's staircase. The taste
  sign-off and which red controls the public page exposes are yours.
- **The sax at A3 (#33):** quasi-periodic, about 18 cents sharp at the
  playing breath, natively and in wasm alike — the fundamental on the note,
  the second harmonic split (440 Hz at −16 dB, +18 cents at 0 dB). An
  engine `[ITERATE]` from 58c; not changed here.
- **The mode splitting (#34):** not live — the shipped voice has no unison
  pair; the draft keeps the 57 chart. A unison preset would make it live.
- **SYNTH §7** still lists the web build as deferred (#34); the line is
  yours to strike.
- **Safari and Firefox:** not run here.
- **The phase end:** the tag `v2.0.0-alpha.3`, the devices played, the fold
  of `DECISIONS_7` #8–#34 into Part VII.
