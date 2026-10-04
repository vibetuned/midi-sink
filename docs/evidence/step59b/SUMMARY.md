# Step 59b — Suzu on the web: the engine in a worklet, the flute panel — the Mac's evidence

A step the author added on 2026-10-04 (with 59c, which now carries the phase
end). Decisions `DECISIONS_7 #28–#30` (`_work/DECISIONS_7.md`): the engine in
the browser (#28), the gates (#29), the flute panel (#30). Voxo is 0.14.0
(additive: `voxo_suzu_inspect`, `voxo_inspect_t`, `VOXO_INSPECT_MAX`).

Machine: the author's Mac (Apple Silicon); Chrome 154 for the browser gate;
node 26 for the node gate. Spec: SYNTH §2.11, §2.13, §6, §7; roadmap step 59b.

## What changed in the tree

- `_work/ROADMAP_5.md`: steps 59b and 59c added at the author's request, the
  phase end moved to 59c, the docs step's Suzu chapter takes the lab's panels
  as its live scenes, "Suzu on the web" left the deferred list.
- `voxo/src/backend_none.cpp` (new): no device, the host renders.
  `voxo/CMakeLists.txt`: one source list, two backends; the web branch;
  `voxo_nofma` (tests only: no fused multiply-add, -O3). `CMakeLists.txt`:
  the web fetches the sampler's parsers and builds Voxo.
- `voxo/include/voxo.h`, `voxo/src/voxo.cpp`: the inspection (per kind, read
  between renders), the flute's per-block values for it; 0.14.0.
- `web/suzu/suzu_web.c` (new): the lab's flat wasm surface — the parameters
  by name (a table generated from voxo.h), MIDI in, stereo out, the trace and
  the inspection as float records, the log. `web/CMakeLists.txt`: the
  standalone wasm target and the page's copy (`build-web/suzu-dist/`).
- `web/suzu/site/` (new): `suzu-engine.js` (one engine, two hosts),
  `suzu-worklet.js` (the audio thread), `index.html`, `lab.js`, `lab.css`
  (the flute panel and the gate mode).
- `tests/suzu_web_reference.c` (new) and `tests/CMakeLists.txt`: the native
  references; `tests/fixtures/suzu_web_script.txt` (new): the gate's script;
  `tests/voxo_suzu_tests.cpp` gate 25 (the inspection);
  `tests/voxo_c_compile.c` 0.14.0 and the inspection from C.
- `tools/suzu_web_gate.mjs`, `tools/suzu_lab_gate.mjs` (new); `tools/README.md`;
  `docs/CHANGELOG.md`; `CLAUDE.md`; `_work/DECISIONS_7.md` #28–#30;
  `site/drafts/suzu/lab.md` (new).

## The gates

The node gate (`suzu_web_gate.txt`): the script through the wasm against the
native reference built from the same surface over the no-FMA Voxo.

| voice kind | audio | trace and inspection | verdict |
|---|---|---|---|
| 0 the cell, 2 Verlet, 3 the hybrid, 4 Duffing, 5 the rotor, 7 the sax | 100 % bit-identical | bit-identical | bit for bit |
| 1 the lattice (sinf per sample under the script's bend) | within −112.9 dB of the peak | within −144.1 dB | the declared −80 dB |
| 6 the flute (tanhf) | within −91.7 dB | within −119.9 dB | the declared −80 dB |
| 8 the trumpet (powf) | within −727.6 dB (one ulp) | bit-identical | the declared −80 dB |
| negative control: one reference sample moved by 0.25 | — | — | red, as required |

Against the shipping desktop build (fused multiply-add on), the report:
−68 to −111 dB of the peak for every kind but the rotor, which diverges as
chaos does with a last-bit difference.

The browser gate (`suzu_lab_gate.txt`, `overblow_rows.json`): the page's
breath ramp — A4 held, the breath 0 → 127 over 12 s, then 3 s at the top —
rendered offline through the AudioWorklet in Chrome 154.

| check | bound | measured |
|---|---|---|
| the audio | finite | finite |
| mid-ramp, breath 40–90 | the first register within 30 cents of the note | 13 of 20 windows; closest 0.4 cents |
| the last second at full breath | the octave within 60 cents | 4 of 4 windows, +4.3 to +4.4 cents |
| the jump | autonomous | at breath 116/127 (t 11 s) |
| soft blowing flattens | breath 32 below breath 56 by 10 cents | −45.8 against +2.7 cents |

The pitch second by second (breath: register × cents): 11: 1×−230, 21:
1×−112, 32: 1×−45, 42: 1×−16, 53: 1×+0.4, 64: 1×+15, 74: 1×+31, 85: 1×+58,
95: 1×+114, 106: 1×+213, 116: 2×−27, 127: 2×+0.9 … +4.4.

The rest:

| check | result |
|---|---|
| the inspection only reads (`voxo_suzu_tests` gate 25) | the flute inspected after every block renders bit-identically |
| the flute's inspection | 104 pressure nodes at 48 kHz, the bore a cylinder, the jet's 64 points, finite |
| the Suzu suite and ctest | all green, 10 of 10 |
| the marble's web gate on the rebuilt web tree | green (max 9.8e-4, mean 3.9e-9) |
| the tablets | iOS `cmake --build build-ios` and Android Gradle compile Voxo 0.14.0 |
| the release desktop (`build-universal`) | builds |

## The size and the cost

| measure | value |
|---|---|
| `suzu.wasm` | 122 449 bytes, no imports, fixed 32 MB memory |
| one flute voice in the live worklet | 38–61 µs a quantum, 1.3–2.1 % of its time |
| two voices in node | 73–77 µs a quantum for the winds, 3–27 µs for the others |
| the browser gate's 15 s offline | 227–234 ms |

The worklet in Chrome has only a millisecond clock here, so its mean over
many quanta is reported and its worst quantum is not.

## The flute panel — the screenshots

`lab_flute_breath70.png` (the first register: the envelope one half-sine),
`lab_flute_breath127.png` (the octave: two humps, a node in the middle),
`lab_flute_breath127_light.png` (the light theme), `lab_flute_breath70_phone.png`
(390 px wide). Taken through the DevTools protocol after the live page had
played four seconds in headless Chrome.

Open it locally:

```
cmake --build build-web
python3 tools/web_serve.py --dist build-web/suzu-dist
```

then `http://localhost:8765/` (localhost is a secure context; AudioWorklet
needs one).

## For the author

- **By ear (roadmap DONE):** the flute in your browser — hold a note, raise
  the breath, press "Ramp the breath" and hear the octave arrive by itself.
  A breath controller plays it through Web MIDI (the checkbox under "The
  embouchure").
- **Your input on the step:** whether web and desktop should render
  bit-identically (Voxo without fused multiply-add everywhere, Suzu's own
  tanh and pow); not done, the default. The lab's look.
- **Found by the drawing (#30):** the jet's switching drives the bore grid's
  shortest waves — 6 % of the pressure energy at breath 70, 21 % at full
  breath. They are inaudible at the mouth end and averaged out of the
  drawing; a gentle grid-scale damping in the bore is an `[ITERATE]`.
- **Your drafts (`visuals/`, untracked):** superseded by the engine for the
  flute; their A/B switches and the bore beside the reed's portrait are 59c's.
- **Safari and Firefox:** not run here (the browser gate is Chrome's); the
  worklet imports the engine as a module, which both support.
